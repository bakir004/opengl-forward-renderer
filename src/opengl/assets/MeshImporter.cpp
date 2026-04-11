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
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  Forward-declared here; called from AssetImporter::LoadMesh.
// ---------------------------------------------------------------------------
std::shared_ptr<MeshBuffer> ImportMeshFromFile(const std::string& path);

// ---------------------------------------------------------------------------

std::shared_ptr<MeshBuffer> ImportMeshFromFile(const std::string& path)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate          |   // all faces → triangles
        aiProcess_GenSmoothNormals     |   // generate normals if absent
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
    //  Layout matches VertexPNT: location 0 = position, 1 = normal, 2 = uv.
    // -----------------------------------------------------------------------

    const VertexLayout layout({
        { 0, 3, GL_FLOAT, GL_FALSE },  // position
        { 1, 3, GL_FLOAT, GL_FALSE },  // normal
        { 2, 2, GL_FLOAT, GL_FALSE },  // uv
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
