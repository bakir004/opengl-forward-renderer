// MeshImporter.cpp
// Assimp-based mesh loading.  Produces a single MeshBuffer from any format
// Assimp understands (.gltf, .glb, .obj, .fbx, …).
//
// All meshes in the scene are merged into one flat vertex/index buffer so the
// result maps cleanly onto a single VAO + VBO + EBO (MeshBuffer).  The caller
// (AssetImporter::LoadMesh) owns caching and fallback logic.

#include "core/MeshBuffer.h"
#include "core/MeshData.h"   // VertexPNT
#include "core/VertexLayout.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <spdlog/spdlog.h>
#include <glad/glad.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
//  Forward-declared here; called from AssetImporter::LoadMesh.
// ---------------------------------------------------------------------------
std::shared_ptr<MeshBuffer> ImportMeshFromFile(const std::string& path);

// ---------------------------------------------------------------------------
//  TangentSource — same classification helper as in ModelImporter.cpp.
//
//  Kept as a file-scope static so both importers share the same diagnostic
//  logic without introducing a shared header for a purely internal helper.
// ---------------------------------------------------------------------------

enum class MeshTangentSource { File, Generated, Unavailable };

static MeshTangentSource ClassifyMeshTangentSource(const aiMesh* mesh,
                                                    const std::string& modelPath)
{
    if (!mesh->mTextureCoords[0])
        return MeshTangentSource::Unavailable;

    if (!mesh->mTangents || !mesh->mBitangents)
        return MeshTangentSource::Unavailable;

    // Guess origin by file extension (informational only).
    const std::string ext = [&]() {
        std::string e = fs::path(modelPath).extension().string();
        for (char& c : e) c = static_cast<char>(::tolower(c));
        return e;
    }();

    const bool likelyHasNativeTangents =
        ext == ".gltf" || ext == ".glb" || ext == ".fbx";

    return likelyHasNativeTangents
        ? MeshTangentSource::File
        : MeshTangentSource::Generated;
}

// ---------------------------------------------------------------------------

std::shared_ptr<MeshBuffer> ImportMeshFromFile(const std::string& path)
{
    Assimp::Importer importer;

    // aiProcess_CalcTangentSpace — compute per-vertex tangent and bitangent
    // vectors required for normal mapping.
    //
    // Behaviour:
    //   • If the source asset already contains tangent data (e.g. a well-authored
    //     glTF/GLB), Assimp preserves the existing data and this flag is a no-op
    //     for those meshes.
    //   • If the asset has normals and UVs but no tangents (common with OBJ),
    //     Assimp generates per-vertex tangents using a MikkTSpace-compatible
    //     algorithm (differentiating UV mapping across triangle edges).
    //   • If a mesh has no UV channel, tangent generation is impossible. Assimp
    //     leaves mTangents null for it. The shader must fall back to the
    //     interpolated vertex normal for that mesh (the u_HasNormalMap uniform
    //     flag disables normal map sampling when the map is absent).
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate          |   // all faces → triangles
        aiProcess_GenSmoothNormals     |   // generate normals if absent
        aiProcess_CalcTangentSpace     |   // ← tangent/bitangent generation
        aiProcess_JoinIdenticalVertices |  // deduplicate vertices → smaller EBO
        aiProcess_PreTransformVertices     // bake node transforms into vertex data
    );

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        spdlog::error("[MeshImporter] Assimp error loading '{}': {}",
                      path, importer.GetErrorString());
        return nullptr;
    }

    if (scene->mNumMeshes == 0)
    {
        spdlog::error("[MeshImporter] No meshes found in '{}'", path);
        return nullptr;
    }

    // -----------------------------------------------------------------------
    //  Tally tangent availability and emit a single summary log line so the
    //  startup log stays readable for models with many submeshes.
    // -----------------------------------------------------------------------
    uint32_t meshesWithTangents = 0;
    uint32_t meshesGenerated    = 0;
    uint32_t meshesNoUV         = 0;

    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        if (mesh->mNumFaces == 0 || mesh->mNumVertices == 0)
            continue;

        switch (ClassifyMeshTangentSource(mesh, path))
        {
        case MeshTangentSource::File:
            ++meshesWithTangents;
            spdlog::debug("[MeshImporter]   mesh[{}] '{}': tangents from file",
                          m, mesh->mName.C_Str());
            break;

        case MeshTangentSource::Generated:
            ++meshesGenerated;
            spdlog::debug("[MeshImporter]   mesh[{}] '{}': tangents generated by Assimp",
                          m, mesh->mName.C_Str());
            break;

        case MeshTangentSource::Unavailable:
            ++meshesNoUV;
            spdlog::warn("[MeshImporter]   mesh[{}] '{}': no UV channel — "
                         "tangent generation skipped; normal mapping unavailable for this mesh",
                         m, mesh->mName.C_Str());
            break;
        }
    }

    spdlog::info("[MeshImporter] Tangent summary for '{}': "
                 "{} from file, {} generated, {} unavailable (no UV)",
                 fs::path(path).filename().string(),
                 meshesWithTangents, meshesGenerated, meshesNoUV);

    // -----------------------------------------------------------------------
    //  Merge every aiMesh in the scene into one flat vertex/index buffer.
    //  Each aiMesh's indices are rebased by the running vertex offset so they
    //  all point into the shared VBO correctly.
    // -----------------------------------------------------------------------

    std::vector<VertexPNT> vertices;
    std::vector<uint32_t>  indices;

    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh       = scene->mMeshes[m];
        const uint32_t baseVtx   = static_cast<uint32_t>(vertices.size());

        // --- vertices -------------------------------------------------------
        vertices.reserve(vertices.size() + mesh->mNumVertices);

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            VertexPNT vtx;

            vtx.position = {
                mesh->mVertices[v].x,
                mesh->mVertices[v].y,
                mesh->mVertices[v].z
            };

            // Assimp guarantees normals exist after aiProcess_GenSmoothNormals.
            vtx.normal = {
                mesh->mNormals[v].x,
                mesh->mNormals[v].y,
                mesh->mNormals[v].z
            };

            // UV channel 0 only; leave {0,0} if not present.
            if (mesh->mTextureCoords[0])
            {
                vtx.uv = {
                    mesh->mTextureCoords[0][v].x,
                    mesh->mTextureCoords[0][v].y
                };
            }
            // After filling uv:
            if (mesh->mTangents && mesh->mTextureCoords[0])
            {
                // Compute handedness via triple product
                const aiVector3D& t = mesh->mTangents[v];
                const aiVector3D& b = mesh->mBitangents[v];
                const aiVector3D& n = mesh->mNormals[v];
                const float w = ((n.x*(b.y*t.z - b.z*t.y)
                                - n.y*(b.x*t.z - b.z*t.x)
                                + n.z*(b.x*t.y - b.y*t.x)) < 0.0f) ? -1.0f : 1.0f;
                vtx.tangent = { t.x, t.y, t.z, w };
            }
        // else tangent stays at default {1,0,0,1}
            vertices.push_back(vtx);
        }

        // --- indices --------------------------------------------------------
        indices.reserve(indices.size() + mesh->mNumFaces * 3);

        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            // aiProcess_Triangulate guarantees exactly 3 indices per face.
            indices.push_back(baseVtx + face.mIndices[0]);
            indices.push_back(baseVtx + face.mIndices[1]);
            indices.push_back(baseVtx + face.mIndices[2]);
        }
    }

    // -----------------------------------------------------------------------
    //  Upload to GPU.
    //  Layout matches VertexPNT: location 0 = position, 1 = normal, 2 = uv, 3 = tangent
    // -----------------------------------------------------------------------

    const VertexLayout layout({
        { 0, 3, GL_FLOAT, GL_FALSE },  // position
        { 1, 3, GL_FLOAT, GL_FALSE },  // normal
        { 2, 2, GL_FLOAT, GL_FALSE },  // uv
        { 3, 4, GL_FLOAT, GL_FALSE },  // tangent
    });

    auto meshBuffer = std::make_shared<MeshBuffer>(
        vertices.data(),
        static_cast<GLsizeiptr>(vertices.size() * sizeof(VertexPNT)),
        static_cast<GLsizei>(vertices.size()),
        indices.data(),
        static_cast<GLsizei>(indices.size()),
        layout
    );

    spdlog::info("[MeshImporter] Loaded '{}': {} vertices, {} indices ({} Assimp mesh(es) merged)",
                 path, vertices.size(), indices.size(), scene->mNumMeshes);

    return meshBuffer;
}