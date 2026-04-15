// ModelImporter.cpp
// Assimp-based multi-material model loader.
//
// Unlike MeshImporter.cpp (which merges everything into one draw call),
// this loader keeps each aiMesh as its own SubMesh so the caller can bind
// a different MaterialInstance per submesh. Material diffuse paths are
// extracted from Assimp and returned as ModelMaterialInfo entries.
//
// All submeshes share one VBO + EBO (classic base-vertex technique).
// glDrawElementsBaseVertex lets each submesh's index array start from 0
// while still indexing into the correct region of the shared VBO.

#include "assets/ModelData.h"
#include "core/MeshData.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// Forward declaration — called by AssetImporter::LoadModel.
ModelData ImportModelFromFile(const std::string& path);

// ---------------------------------------------------------------------------

static std::string ResolveDiffusePath(const aiMaterial* mat,
                                      const std::string& modelDir)
{
    // GLTF2 PBR base color comes through as BASE_COLOR in newer Assimp builds,
    // but older builds and other formats use DIFFUSE. Try both.
    aiString aiPath;
    if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &aiPath) == AI_SUCCESS
        || mat->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath) == AI_SUCCESS)
    {
        const std::string raw = aiPath.C_Str();
        if (raw.empty())
            return {};

        // Assimp returns paths relative to the model file's directory.
        // Prepend the model dir to get a working-dir-relative path.
        fs::path full = fs::path(modelDir) / raw;
        // Normalise (resolves any ".." segments that Assimp sometimes emits).
        return full.lexically_normal().string();
    }
    return {};
}

ModelData ImportModelFromFile(const std::string& path)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate           |
        aiProcess_GenSmoothNormals      |
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices  |  // bake node transforms → static city
        aiProcess_FixInfacingNormals);     // fix normals flipped by negative-scale nodes

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        spdlog::error("[ModelImporter] Assimp error loading '{}': {}",
                      path, importer.GetErrorString());
        return {};
    }

    if (scene->mNumMeshes == 0)
    {
        spdlog::error("[ModelImporter] No meshes found in '{}'", path);
        return {};
    }

    const std::string modelDir = fs::path(path).parent_path().string();

    // -----------------------------------------------------------------------
    //  Build a compact material table.
    //  Multiple aiMeshes may share the same aiMaterial index; we deduplicate
    //  so ModelMaterialInfo has one entry per unique material.
    // -----------------------------------------------------------------------

    // aiMaterialIndex → our compact material index
    std::unordered_map<unsigned int, uint32_t> materialRemap;
    std::vector<ModelMaterialInfo> materials;

    auto GetOrAddMaterial = [&](unsigned int aiMatIdx) -> uint32_t {
        auto it = materialRemap.find(aiMatIdx);
        if (it != materialRemap.end())
            return it->second;

        const uint32_t idx = static_cast<uint32_t>(materials.size());
        materialRemap[aiMatIdx] = idx;

        ModelMaterialInfo info;
        if (aiMatIdx < scene->mNumMaterials)
        {
            const aiMaterial* aiMat = scene->mMaterials[aiMatIdx];
            aiString matName;
            aiMat->Get(AI_MATKEY_NAME, matName);
            info.name        = matName.C_Str();
            info.diffusePath = ResolveDiffusePath(aiMat, modelDir);
        }
        materials.push_back(std::move(info));
        return idx;
    };

    // -----------------------------------------------------------------------
    //  Build shared vertex / index buffers.
    //  Each aiMesh → one SubMesh with its own [indexByteOffset, indexCount]
    //  slice of the shared EBO, and baseVertex pointing at its slice of the VBO.
    // -----------------------------------------------------------------------

    std::vector<VertexPNT> vertices;
    std::vector<uint32_t>  indices;
    std::vector<SubMesh>   submeshes;

    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];

        // Skip degenerate meshes (lines, points, no faces).
        if (mesh->mNumFaces == 0 || mesh->mNumVertices == 0)
            continue;

        SubMesh sm;
        sm.name            = mesh->mName.C_Str();
        sm.baseVertex      = static_cast<int32_t>(vertices.size());
        sm.indexByteOffset = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
        sm.materialIndex   = GetOrAddMaterial(mesh->mMaterialIndex);

        // --- vertices ---
        vertices.reserve(vertices.size() + mesh->mNumVertices);
        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            VertexPNT vtx;
            vtx.position = { mesh->mVertices[v].x,
                             mesh->mVertices[v].y,
                             mesh->mVertices[v].z };
            vtx.normal   = { mesh->mNormals[v].x,
                             mesh->mNormals[v].y,
                             mesh->mNormals[v].z };
            if (mesh->mTextureCoords[0])
                vtx.uv = { mesh->mTextureCoords[0][v].x,
                           mesh->mTextureCoords[0][v].y };
            vertices.push_back(vtx);
        }

        // --- indices (0-based relative to this submesh's baseVertex) ---
        sm.indexCount = mesh->mNumFaces * 3;
        indices.reserve(indices.size() + sm.indexCount);
        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            indices.push_back(face.mIndices[0]);
            indices.push_back(face.mIndices[1]);
            indices.push_back(face.mIndices[2]);
        }

        submeshes.push_back(sm);
    }

    if (submeshes.empty())
    {
        spdlog::error("[ModelImporter] No drawable submeshes in '{}'", path);
        return {};
    }

    // -----------------------------------------------------------------------
    //  Upload to GPU via MeshData → Mesh.
    // -----------------------------------------------------------------------

    MeshData data;
    data.vertices  = std::move(vertices);
    data.indices   = std::move(indices);
    data.submeshes = std::move(submeshes);
    data.name      = fs::path(path).filename().string();

    spdlog::info("[ModelImporter] Loaded '{}': {} vertices, {} indices, "
                 "{} submesh(es), {} material(s)",
                 path,
                 data.vertices.size(),
                 data.indices.size(),
                 data.submeshes.size(),
                 materials.size());

    for (size_t i = 0; i < materials.size(); ++i)
    {
        spdlog::debug("[ModelImporter]   material[{}] name='{}' diffuse='{}'",
                      i, materials[i].name,
                      materials[i].diffusePath.empty() ? "(none)" : materials[i].diffusePath);
    }

    ModelData result;
    result.mesh      = std::make_shared<Mesh>(data);
    result.materials = std::move(materials);
    return result;
}
