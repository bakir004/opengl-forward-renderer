#include "assets/AssetImporter.h"
#include "core/ShaderProgram.h"
#include "core/Texture2D.h"
#include "core/Material.h"
#include "core/MeshBuffer.h"
#include "core/Primitives.h"   // fallback cube geometry

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
//  Static cache storage
// ---------------------------------------------------------------------------

std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> AssetImporter::s_shaders;
std::unordered_map<std::string, std::shared_ptr<Texture2D>>     AssetImporter::s_textures;
std::unordered_map<std::string, std::shared_ptr<MeshBuffer>>    AssetImporter::s_meshes;
std::unordered_map<std::string, std::shared_ptr<Material>>      AssetImporter::s_materials;

// ---------------------------------------------------------------------------
//  Path helpers
// ---------------------------------------------------------------------------

std::string AssetImporter::ResolvePath(const std::string& path) {
    const fs::path requested(path);
    if (fs::exists(requested))
        return fs::canonical(requested).string();

    // Walk upward from cwd until we find the file (mirrors Options strategy).
    for (fs::path cur = fs::current_path(); ; cur = cur.parent_path()) {
        const fs::path candidate = cur / requested;
        if (fs::exists(candidate))
            return fs::canonical(candidate).string();
        if (!cur.has_parent_path() || cur == cur.parent_path())
            break;
    }
    // Return as-is so callers get a meaningful error message.
    return path;
}

std::string AssetImporter::Extension(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    // Strip leading dot.
    if (!ext.empty() && ext[0] == '.')
        ext = ext.substr(1);
    return ext;
}

std::string AssetImporter::Stem(const std::string& path) {
    return fs::path(path).stem().string();
}

std::string AssetImporter::Directory(const std::string& path) {
    return fs::path(path).parent_path().string();
}

// ---------------------------------------------------------------------------
//  Extension sets
// ---------------------------------------------------------------------------

static const std::unordered_set<std::string> kShaderExts   = {"vert","frag","glsl","vs","fs"};
static const std::unordered_set<std::string> kTextureExts  = {"png","jpg","jpeg","tga","bmp","hdr","exr"};
static const std::unordered_set<std::string> kMeshExts     = {"obj","gltf","glb","fbx","dae","3ds","ply"};
static const std::unordered_set<std::string> kMaterialExts = {"mat"};

// ---------------------------------------------------------------------------
//  Cache management
// ---------------------------------------------------------------------------

void AssetImporter::CollectUnused() {
    auto evict = [](auto& map) {
        for (auto it = map.begin(); it != map.end(); ) {
            if (it->second.use_count() == 1)
                it = map.erase(it);
            else
                ++it;
        }
    };
    evict(s_shaders);
    evict(s_textures);
    evict(s_meshes);
    evict(s_materials);
    spdlog::debug("[AssetImporter] CollectUnused — {} shaders, {} textures, {} meshes, {} materials remain",
                  s_shaders.size(), s_textures.size(), s_meshes.size(), s_materials.size());
}

void AssetImporter::Clear() {
    s_shaders.clear();
    s_textures.clear();
    s_meshes.clear();
    s_materials.clear();
    spdlog::info("[AssetImporter] All caches cleared");
}

std::size_t AssetImporter::CachedCount() {
    return s_shaders.size() + s_textures.size() + s_meshes.size() + s_materials.size();
}

// ---------------------------------------------------------------------------
//  Internal: ImportShader
//
//  Given a .vert or .frag path, automatically finds the partner file:
//    "assets/shaders/basic.vert"  →  looks for "assets/shaders/basic.frag"
//    "assets/shaders/basic.frag"  →  looks for "assets/shaders/basic.vert"
//    "assets/shaders/basic.glsl"  →  looks for basic.vert + basic.frag siblings
//
//  Cache key: "vertPath|fragPath" (sorted so vert always comes first).
// ---------------------------------------------------------------------------

std::shared_ptr<ShaderProgram> AssetImporter::ImportShader(const std::string& path) {
    const std::string resolved = ResolvePath(path);
    const std::string ext      = Extension(resolved);
    const std::string dir      = Directory(resolved);
    const std::string stem     = Stem(resolved);

    // Determine vert/frag paths.
    std::string vertPath, fragPath;

    if (ext == "vert" || ext == "vs") {
        vertPath = resolved;
        fragPath = ResolvePath((fs::path(dir) / (stem + ".frag")).string());
        // Also try .fs extension.
        if (!fs::exists(fragPath))
            fragPath = ResolvePath((fs::path(dir) / (stem + ".fs")).string());
    } else if (ext == "frag" || ext == "fs") {
        fragPath = resolved;
        vertPath = ResolvePath((fs::path(dir) / (stem + ".vert")).string());
        if (!fs::exists(vertPath))
            vertPath = ResolvePath((fs::path(dir) / (stem + ".vs")).string());
    } else {
        // Generic .glsl — look for stem.vert + stem.frag siblings.
        vertPath = ResolvePath((fs::path(dir) / (stem + ".vert")).string());
        fragPath = ResolvePath((fs::path(dir) / (stem + ".frag")).string());
    }

    return LoadShader(vertPath, fragPath);
}

std::shared_ptr<ShaderProgram> AssetImporter::LoadShader(const std::string& vertPath,
                                                          const std::string& fragPath) {
    const std::string resolvedVert = ResolvePath(vertPath);
    const std::string resolvedFrag = ResolvePath(fragPath);
    const std::string key          = resolvedVert + "|" + resolvedFrag;

    auto it = s_shaders.find(key);
    if (it != s_shaders.end()) {
        spdlog::debug("[AssetImporter] Shader cache hit: '{}'", key);
        return it->second;
    }

    auto prog = std::make_shared<ShaderProgram>(resolvedVert, resolvedFrag);
    if (!prog->IsValid()) {
        spdlog::error("[AssetImporter] Shader load failed: '{}' + '{}'", resolvedVert, resolvedFrag);
        return nullptr;
    }

    s_shaders[key] = prog;
    spdlog::info("[AssetImporter] Shader cached: '{}'", key);
    return prog;
}

// ---------------------------------------------------------------------------
//  Internal: ImportTexture / LoadTexture
// ---------------------------------------------------------------------------

std::shared_ptr<Texture2D> AssetImporter::ImportTexture(const std::string& path) {
    // Default: treat unknown textures as sRGB color data.
    // Callers who need non-color (normal maps, etc.) should use LoadTexture explicitly.
    return LoadTexture(path, TextureColorSpace::sRGB);
}

std::shared_ptr<Texture2D> AssetImporter::LoadTexture(const std::string& path,
                                                        TextureColorSpace  colorSpace,
                                                        SamplerDesc        sampler,
                                                        bool               flipY) {
    const std::string resolved = ResolvePath(path);

    auto it = s_textures.find(resolved);
    if (it != s_textures.end()) {
        spdlog::debug("[AssetImporter] Texture cache hit: '{}'", resolved);
        return it->second;
    }

    if (!fs::exists(resolved)) {
        spdlog::warn("[AssetImporter] Texture not found: '{}' — using magenta fallback", path);
        auto fallback = std::make_shared<Texture2D>(
            Texture2D::CreateFallback(255, 0, 255));
        // Cache under the original (unresolved) path so repeated misses are fast.
        s_textures[path] = fallback;
        return fallback;
    }

    auto tex = std::make_shared<Texture2D>(resolved, colorSpace, sampler, flipY);
    if (!tex->IsValid()) {
        spdlog::error("[AssetImporter] Texture upload failed: '{}'", resolved);
        auto fallback = std::make_shared<Texture2D>(Texture2D::CreateFallback(255, 0, 255));
        s_textures[resolved] = fallback;
        return fallback;
    }

    s_textures[resolved] = tex;
    return tex;
}

// ---------------------------------------------------------------------------
//  Internal: ImportMesh / LoadMesh
//
//  Wraps Assimp import. For now returns the first submesh as a MeshBuffer.
//  Full multi-submesh support lives in Mesh.h (Task 3).
// ---------------------------------------------------------------------------

std::shared_ptr<MeshBuffer> AssetImporter::ImportMesh(const std::string& path) {
    return LoadMesh(path);
}

std::shared_ptr<MeshBuffer> AssetImporter::LoadMesh(const std::string& path) {
    const std::string resolved = ResolvePath(path);

    auto it = s_meshes.find(resolved);
    if (it != s_meshes.end()) {
        spdlog::debug("[AssetImporter] Mesh cache hit: '{}'", resolved);
        return it->second;
    }

    if (!fs::exists(resolved)) {
        spdlog::warn("[AssetImporter] Mesh not found: '{}' — using fallback cube", path);
        // Fallback: return a unit cube so the scene is never completely empty.
        auto fallback = std::make_shared<MeshBuffer>(GenerateCube().CreateMeshBuffer());
        s_meshes[path] = fallback;
        return fallback;
    }

    // Assimp import is implemented in MeshImporter.cpp (Task 3).
    // For now, log and return a fallback.  Task 3 will replace this stub.
    spdlog::warn("[AssetImporter] Mesh import not yet wired (Task 3): '{}'", path);
    auto fallback = std::make_shared<MeshBuffer>(GenerateCube().CreateMeshBuffer());
    s_meshes[resolved] = fallback;
    return fallback;
}

// ---------------------------------------------------------------------------
//  Internal: ImportMaterial / LoadMaterial
//
//  .mat JSON schema:
//  {
//    "shader": { "vert": "assets/shaders/pbr.vert", "frag": "assets/shaders/pbr.frag" },
//    "textures": {
//      "u_AlbedoMap":    { "path": "assets/textures/albedo.png",    "colorSpace": "sRGB"   },
//      "u_NormalMap":    { "path": "assets/textures/normal.png",    "colorSpace": "Linear" },
//      "u_MetallicMap":  { "path": "assets/textures/metallic.png",  "colorSpace": "Linear" },
//      "u_RoughnessMap": { "path": "assets/textures/roughness.png", "colorSpace": "Linear" }
//    },
//    "floats": { "u_Metallic": 0.0, "u_Roughness": 0.5 },
//    "vec3s":  { "u_AlbedoColor": [1.0, 1.0, 1.0] }
//  }
// ---------------------------------------------------------------------------

std::shared_ptr<Material> AssetImporter::ImportMaterial(const std::string& path) {
    return LoadMaterial(path);
}

std::shared_ptr<Material> AssetImporter::LoadMaterial(const std::string& path) {
    const std::string resolved = ResolvePath(path);

    auto it = s_materials.find(resolved);
    if (it != s_materials.end()) {
        spdlog::debug("[AssetImporter] Material cache hit: '{}'", resolved);
        return it->second;
    }

    if (!fs::exists(resolved)) {
        spdlog::warn("[AssetImporter] Material not found: '{}'", path);
        return nullptr;
    }

    std::ifstream f(resolved);
    if (!f.is_open()) {
        spdlog::error("[AssetImporter] Cannot open material file: '{}'", resolved);
        return nullptr;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        spdlog::error("[AssetImporter] Malformed .mat '{}': {}", resolved, e.what());
        return nullptr;
    }

    // --- Shader ---
    if (!j.contains("shader")) {
        spdlog::error("[AssetImporter] .mat '{}' missing 'shader' key", resolved);
        return nullptr;
    }
    const std::string vertPath = j["shader"].value("vert", "");
    const std::string fragPath = j["shader"].value("frag", "");
    auto shader = LoadShader(vertPath, fragPath);
    if (!shader) return nullptr;

    auto mat = std::make_shared<Material>(shader);

    // --- Textures ---
    if (j.contains("textures")) {
        for (auto& [slotName, texDesc] : j["textures"].items()) {
            const std::string texPath = texDesc.value("path", "");
            const std::string csStr   = texDesc.value("colorSpace", "sRGB");
            const TextureColorSpace cs = (csStr == "Linear")
                                         ? TextureColorSpace::Linear
                                         : TextureColorSpace::sRGB;
            auto tex = LoadTexture(texPath, cs);
            if (tex)
                mat->SetTexture(slotName, tex);
        }
    }

    // --- Scalar floats ---
    if (j.contains("floats")) {
        for (auto& [name, val] : j["floats"].items())
            mat->SetFloat(name, val.get<float>());
    }

    // --- vec3 parameters ---
    if (j.contains("vec3s")) {
        for (auto& [name, arr] : j["vec3s"].items()) {
            if (arr.is_array() && arr.size() == 3)
                mat->SetVec3(name, glm::vec3(arr[0], arr[1], arr[2]));
        }
    }

    // --- vec4 parameters ---
    if (j.contains("vec4s")) {
        for (auto& [name, arr] : j["vec4s"].items()) {
            if (arr.is_array() && arr.size() == 4)
                mat->SetVec4(name, glm::vec4(arr[0], arr[1], arr[2], arr[3]));
        }
    }

    s_materials[resolved] = mat;
    spdlog::info("[AssetImporter] Material loaded: '{}'", resolved);
    return mat;
}

// ---------------------------------------------------------------------------
//  Dispatch helper used by the non-specialised Import<T> overload.
//  (All specialisations are defined inline in the header; this is the catch-all
//  that fires a compile-time error for unsupported types.)
// ---------------------------------------------------------------------------
// No runtime dispatch needed — handled entirely by the header specialisations.
