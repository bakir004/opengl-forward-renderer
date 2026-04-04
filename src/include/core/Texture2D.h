#pragma once

#include <glad/glad.h>
#include <string>

/// Color-space interpretation for texture uploads.
enum class TextureColorSpace {
    sRGB,   ///< Color/albedo data — driver converts to linear during sampling
    Linear  ///< Non-color data: normals, roughness, metallic, AO
};

/// Filtering and wrapping policy for a sampler.
struct SamplerDesc {
    GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR;
    GLenum magFilter = GL_LINEAR;
    GLenum wrapS     = GL_REPEAT;
    GLenum wrapT     = GL_REPEAT;
};

/// RAII owner of a single OpenGL 2-D texture.
///
/// Loads image data through stb_image, uploads to GPU, and generates mipmaps.
/// Copy is disabled; move transfers ownership.
///
/// Color-space policy:
///   - Color / albedo textures → TextureColorSpace::sRGB   (internal: GL_SRGB8_ALPHA8)
///   - Normal / roughness / metallic / AO → TextureColorSpace::Linear (internal: GL_RGBA8)
class Texture2D {
public:
    /// Loads an image from disk and uploads it to the GPU.
    /// @param path       File path relative to the working directory.
    /// @param colorSpace sRGB for color data, Linear for non-color data.
    /// @param sampler    Filtering and wrapping policy.
    /// @param flipY      Flip rows on load (stb default = top-left origin; OpenGL expects bottom-left).
    explicit Texture2D(const std::string& path,
                       TextureColorSpace  colorSpace = TextureColorSpace::sRGB,
                       SamplerDesc        sampler    = {},
                       bool               flipY      = true);

    /// Creates a 1×1 solid-color fallback texture (RGBA).
    /// Useful when a requested asset is missing.
    static Texture2D CreateFallback(unsigned char r, unsigned char g,
                                    unsigned char b, unsigned char a = 255);

    /// Creates a checkerboard fallback texture (magenta + black pattern).
    /// Used as a highly visible error indicator when an asset fails to load.
    /// @param size  Width and height of the texture in pixels (must be power of 2).
    static Texture2D CreateCheckerboard(int size = 8);

    ~Texture2D();

    Texture2D(const Texture2D&)            = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    /// Binds this texture to the given texture unit (GL_TEXTURE0 + unit).
    void Bind(GLuint unit = 0) const;

    /// Unbinds the 2D texture target on the given unit.
    static void Unbind(GLuint unit = 0);

    [[nodiscard]] GLuint      GetID()     const { return m_id; }
    [[nodiscard]] int         GetWidth()  const { return m_width; }
    [[nodiscard]] int         GetHeight() const { return m_height; }
    [[nodiscard]] bool        IsValid()   const { return m_id != 0; }
    [[nodiscard]] const std::string& GetPath() const { return m_path; }

private:
    Texture2D() = default; // used by CreateFallback

    GLuint      m_id     = 0;
    int         m_width  = 0;
    int         m_height = 0;
    std::string m_path;
};
