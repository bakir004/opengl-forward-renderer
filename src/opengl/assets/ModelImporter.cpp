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
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// Forward declaration — called by AssetImporter::LoadModel.
ModelData ImportModelFromFile(const std::string& path);

// ---------------------------------------------------------------------------
//  ExtractEmbeddedTextures
//
//  GLB files store textures as binary blobs inside the file itself. Assimp
//  exposes them in scene->mTextures[] and signals their use in materials by
//  setting the diffuse path to "*<index>" (e.g. "*0", "*2").
//
//  This function walks every embedded texture and writes it to a temp file
//  so the rest of the pipeline (AssetImporter::LoadTexture) can load it
//  from disk as normal, with no changes required upstream.
//
//  Returns a map from the Assimp sentinel string ("*0", "*1", …) to the
//  real temp-file path that was written.
//
//  Temp files are placed next to the source GLB:
//    japanese_cherrybake_embedded_0.png
//    japanese_cherrybake_embedded_1.png
//    …
//  They are re-used on subsequent loads (checked via fs::exists).
// ---------------------------------------------------------------------------

static std::unordered_map<std::string, std::string>
ExtractEmbeddedTextures(const aiScene* scene, const std::string& modelPath)
{
    std::unordered_map<std::string, std::string> result;

    if (!scene->mTextures || scene->mNumTextures == 0)
        return result;

    const std::string modelDir  = fs::path(modelPath).parent_path().string();
    const std::string modelStem = fs::path(modelPath).stem().string();

    for (unsigned int i = 0; i < scene->mNumTextures; ++i)
    {
        const aiTexture* tex = scene->mTextures[i];

        // Build the sentinel key Assimp uses in material paths.
        const std::string sentinel = "*" + std::to_string(i);

        // Determine file extension.
        // tex->achFormatHint is a 4-char hint like "png\0", "jpg\0", etc.
        // If empty (raw uncompressed ARGB8888), we'd need to encode it ourselves;
        // in practice GLBs always store compressed (PNG/JPG) blobs.
        std::string ext = tex->achFormatHint;   // e.g. "png", "jpg"
        if (ext.empty())
            ext = "png";

        const std::string outPath =
            (fs::path(modelDir) / (modelStem + "_embedded_" + std::to_string(i) + "." + ext))
            .string();

        // Skip writing if the file already exists (avoids redundant disk I/O
        // on repeated calls for the same model).
        if (!fs::exists(outPath))
        {
            // mWidth holds the byte size when mHeight == 0 (compressed data).
            // When mHeight > 0 the texture is raw ARGB8888 and mWidth is the
            // pixel width — we don't handle that case here as GLBs never use it.
            if (tex->mHeight != 0)
            {
                spdlog::warn("[ModelImporter] Embedded texture {} is uncompressed "
                             "(raw ARGB); skipping extraction", i);
                continue;
            }

            const std::size_t byteSize = static_cast<std::size_t>(tex->mWidth);

            std::ofstream ofs(outPath, std::ios::binary);
            if (!ofs)
            {
                spdlog::error("[ModelImporter] Cannot write embedded texture to '{}'", outPath);
                continue;
            }

            ofs.write(reinterpret_cast<const char*>(tex->pcData), byteSize);
            spdlog::debug("[ModelImporter] Extracted embedded texture {} → '{}'", i, outPath);
        }
        else
        {
            spdlog::debug("[ModelImporter] Embedded texture {} already extracted → '{}'", i, outPath);
        }

        result[sentinel] = outPath;
    }

    return result;
}

// ---------------------------------------------------------------------------
//  ResolveDiffusePath
//
//  Resolves a material's diffuse texture path.
//  - Embedded GLB textures  ("*0", "*1", …): looked up in embeddedMap.
//  - External file references: joined with modelDir as before.
// ---------------------------------------------------------------------------

static std::string ResolveDiffusePath(
    const aiMaterial*                                      mat,
    const std::string&                                     modelDir,
    const std::unordered_map<std::string, std::string>&    embeddedMap)
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

        // Embedded GLB texture — return the pre-extracted temp file path.
        if (!raw.empty() && raw[0] == '*')
        {
            auto it = embeddedMap.find(raw);
            if (it != embeddedMap.end())
                return it->second;

            spdlog::warn("[ModelImporter] Embedded texture sentinel '{}' has no "
                         "extracted file — texture will be missing", raw);
            return {};
        }

        // External file reference — prepend the model directory.
        fs::path full = fs::path(modelDir) / raw;
        return full.lexically_normal().string();
    }
    return {};
}

// ---------------------------------------------------------------------------
//  ImportModelFromFile
// ---------------------------------------------------------------------------

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

    // Extract embedded textures first so ResolveDiffusePath can map
    // "*0" → real file path for every material below.
    const auto embeddedMap = ExtractEmbeddedTextures(scene, path);

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
            info.diffusePath = ResolveDiffusePath(aiMat, modelDir, embeddedMap);
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