#pragma once

#include "assets/ModelData.h"
#include "core/Texture2D.h"
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

class ShaderProgram;
class Texture2D;
class Material;
struct MeshData;
class MeshBuffer;

enum class TextureColorSpace;
struct SamplerDesc;

struct AssetCacheStats {
    std::size_t shaderCount   = 0;
    std::size_t textureCount  = 0;
    std::size_t meshCount     = 0;
    std::size_t materialCount = 0;

    [[nodiscard]] std::size_t TotalCount() const {
        return shaderCount + textureCount + meshCount + materialCount;
    }
};

/// Centralised asset loading and caching hub.
///
/// Provides two usage styles:
///
///   1. Generic wildcard import — type is inferred from the file extension:
///      @code
///      auto shader  = AssetImporter::Import<ShaderProgram>("assets/shaders/basic.vert");
///      auto texture = AssetImporter::Import<Texture2D>("assets/textures/albedo.png");
///      @endcode
///
///   2. Explicit typed loaders — useful when the extension is ambiguous or the caller
///      needs to supply extra parameters (color-space, sampler, etc.):
///      @code
///      auto tex = AssetImporter::LoadTexture("assets/textures/normal.png",
///                                             TextureColorSpace::Linear);
///      @endcode
///
/// All returned resources are std::shared_ptr so ownership is shared.
/// Requests for the same canonical path reuse the cached instance.
/// Relative paths are resolved upward from the current working directory
/// (same strategy as Options).
///
/// Thread safety: none — call from the main/render thread only.
class AssetImporter {
public:
    // -----------------------------------------------------------------------
    //  Generic wildcard import
    // -----------------------------------------------------------------------

    /// Import any supported asset.  The concrete loader is chosen by extension:
    ///   .vert / .frag / .glsl           → ShaderProgram  (pair: vert+frag, see note)
    ///   .png / .jpg / .jpeg / .tga / .bmp / .hdr → Texture2D
    ///   .obj / .gltf / .glb / .fbx / .dae        → MeshBuffer
    ///   .mat                                      → Material (JSON-described)
    ///
    /// @note  Shaders: for .vert paths the importer automatically looks for a
    ///        matching .frag next to it (same stem), and vice-versa.
    ///
    /// @param path   Asset path (relative to working dir or absolute).
    /// @return       Shared pointer to the loaded resource, or nullptr on failure.
    template <typename T>
    static std::shared_ptr<T> Import(const std::string& path);

    // -----------------------------------------------------------------------
    //  Explicit typed loaders
    // -----------------------------------------------------------------------

    /// Loads (or returns cached) a ShaderProgram from a vertex + fragment pair.
    static std::shared_ptr<ShaderProgram> LoadShader(const std::string& vertPath,
                                                      const std::string& fragPath);

    /// Loads (or returns cached) a Texture2D from an image file.
    /// @param colorSpace  sRGB for color/albedo, Linear for non-color maps.
    /// @param sampler     Filtering/wrapping override; defaults apply if omitted.
    /// @param flipY       Flip rows on load (true = OpenGL convention).
    static std::shared_ptr<Texture2D> LoadTexture(const std::string& path,
                                                   TextureColorSpace  colorSpace,
                                                   SamplerDesc        sampler = {},
                                                   bool               flipY   = true);

    /// Loads (or returns cached) a MeshBuffer from a model file via Assimp.
    /// Returns the first mesh in the file. For multi-mesh files use LoadModel().
    static std::shared_ptr<MeshBuffer> LoadMesh(const std::string& path);

    /// Loads (or returns cached) a multi-material model.
    /// Returns a ModelData with one SubMesh per Assimp aiMesh and one
    /// ModelMaterialInfo per unique material (with the diffuse texture path).
    /// Use this instead of LoadMesh when a model has multiple materials.
    static ModelData LoadModel(const std::string& path);

    /// Loads a Material from a JSON .mat descriptor file.
    /// The .mat file references shader paths and texture paths.
    static std::shared_ptr<Material> LoadMaterial(const std::string& path);

    // -----------------------------------------------------------------------
    //  Cache management
    // -----------------------------------------------------------------------

    /// Evicts all cached resources whose use-count has dropped to 1 (only the
    /// cache holds a reference).  Call between scenes to free GPU memory.
    static void CollectUnused();

    /// Clears all caches unconditionally.  All outstanding shared_ptrs remain
    /// valid; the cache simply stops keeping them alive.
    static void Clear();

    /// Returns the number of currently cached entries across all resource types.
    [[nodiscard]] static std::size_t CachedCount();

    /// Returns current per-cache entry counts for debug UI.
    [[nodiscard]] static AssetCacheStats GetCacheStats();

private:
    // Caches keyed by canonical path.
    static std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> s_shaders;
    static std::unordered_map<std::string, std::shared_ptr<Texture2D>>     s_textures;
    static std::unordered_map<std::string, std::shared_ptr<MeshBuffer>>    s_meshes;
    static std::unordered_map<std::string, std::shared_ptr<Material>>      s_materials;
    static std::unordered_map<std::string, ModelData>                      s_models;

    // Internal helpers
    static std::string        ResolvePath(const std::string& path);
    static std::string        Extension(const std::string& path);   // lowercase, no dot
    static std::string        Stem(const std::string& path);        // filename without extension
    static std::string        Directory(const std::string& path);   // parent directory

    static std::shared_ptr<ShaderProgram> ImportShader(const std::string& path);
    static std::shared_ptr<Texture2D>     ImportTexture(const std::string& path);
    static std::shared_ptr<MeshBuffer>    ImportMesh(const std::string& path);
    static std::shared_ptr<Material>      ImportMaterial(const std::string& path);
};

// ---------------------------------------------------------------------------
//  Template specialisations — defined in the header so TU can instantiate them
// ---------------------------------------------------------------------------

template <>
inline std::shared_ptr<ShaderProgram> AssetImporter::Import<ShaderProgram>(const std::string& path) {
    return ImportShader(path);
}

template <>
inline std::shared_ptr<Texture2D> AssetImporter::Import<Texture2D>(const std::string& path) {
    return ImportTexture(path);
}

template <>
inline std::shared_ptr<MeshBuffer> AssetImporter::Import<MeshBuffer>(const std::string& path) {
    return ImportMesh(path);
}

template <>
inline std::shared_ptr<Material> AssetImporter::Import<Material>(const std::string& path) {
    return ImportMaterial(path);
}
