#include "core/Texture2D.h"

#define TINYDDSLOADER_IMPLEMENTATION
#include <tinyddsloader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glad/glad.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <cctype>
#include <vector>

namespace
{
    constexpr GLenum kGlCompressedRgbaS3tcDxt1Ext = 0x83F1;
    constexpr GLenum kGlCompressedRgbaS3tcDxt3Ext = 0x83F2;
    constexpr GLenum kGlCompressedRgbaS3tcDxt5Ext = 0x83F3;
    constexpr GLenum kGlCompressedSrgbAlphaS3tcDxt1Ext = 0x8C4D;
    constexpr GLenum kGlCompressedSrgbAlphaS3tcDxt3Ext = 0x8C4E;
    constexpr GLenum kGlCompressedSrgbAlphaS3tcDxt5Ext = 0x8C4F;

    struct DdsGlFormat
    {
        GLenum internalFormat = 0;
        GLenum format = 0;
        GLenum type = 0;
        bool compressed = false;
    };

    std::string ToLower(std::string value)
    {
        for (char& ch : value)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return value;
    }

    bool IsDdsPath(const std::string& path)
    {
        return ToLower(std::filesystem::path(path).extension().string()) == ".dds";
    }

    DdsGlFormat TranslateDdsFormat(tinyddsloader::DDSFile::DXGIFormat format,
                                   TextureColorSpace colorSpace)
    {
        using DXGIFormat = tinyddsloader::DDSFile::DXGIFormat;

        switch (format)
        {
        case DXGIFormat::BC1_UNorm:
            // Honor the caller's colorSpace when the DDS file has no sRGB tag.
            return {colorSpace == TextureColorSpace::sRGB
                        ? kGlCompressedSrgbAlphaS3tcDxt1Ext
                        : kGlCompressedRgbaS3tcDxt1Ext, 0, 0, true};
        case DXGIFormat::BC1_UNorm_SRGB:
            return {kGlCompressedSrgbAlphaS3tcDxt1Ext, 0, 0, true};
        case DXGIFormat::BC2_UNorm:
            return {colorSpace == TextureColorSpace::sRGB
                        ? kGlCompressedSrgbAlphaS3tcDxt3Ext
                        : kGlCompressedRgbaS3tcDxt3Ext, 0, 0, true};
        case DXGIFormat::BC2_UNorm_SRGB:
            return {kGlCompressedSrgbAlphaS3tcDxt3Ext, 0, 0, true};
        case DXGIFormat::BC3_UNorm:
            return {colorSpace == TextureColorSpace::sRGB
                        ? kGlCompressedSrgbAlphaS3tcDxt5Ext
                        : kGlCompressedRgbaS3tcDxt5Ext, 0, 0, true};
        case DXGIFormat::BC3_UNorm_SRGB:
            return {kGlCompressedSrgbAlphaS3tcDxt5Ext, 0, 0, true};
        case DXGIFormat::BC4_UNorm:
            return {GL_COMPRESSED_RED_RGTC1, 0, 0, true};
        case DXGIFormat::BC4_SNorm:
            return {GL_COMPRESSED_SIGNED_RED_RGTC1, 0, 0, true};
        case DXGIFormat::BC5_UNorm:
            return {GL_COMPRESSED_RG_RGTC2, 0, 0, true};
        case DXGIFormat::BC5_SNorm:
            return {GL_COMPRESSED_SIGNED_RG_RGTC2, 0, 0, true};
        case DXGIFormat::BC6H_UF16:
            return {GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, 0, 0, true};
        case DXGIFormat::BC6H_SF16:
            return {GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT, 0, 0, true};
        case DXGIFormat::BC7_UNorm:
            return {colorSpace == TextureColorSpace::sRGB
                        ? GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
                        : GL_COMPRESSED_RGBA_BPTC_UNORM, 0, 0, true};
        case DXGIFormat::BC7_UNorm_SRGB:
            return {GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, 0, 0, true};

        case DXGIFormat::R8G8B8A8_UNorm:
            return {static_cast<GLenum>(colorSpace == TextureColorSpace::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8), GL_RGBA, GL_UNSIGNED_BYTE, false};
        case DXGIFormat::B8G8R8A8_UNorm:
            return {static_cast<GLenum>(colorSpace == TextureColorSpace::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8), GL_BGRA, GL_UNSIGNED_BYTE, false};
        case DXGIFormat::B8G8R8X8_UNorm:
            return {static_cast<GLenum>(colorSpace == TextureColorSpace::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8), GL_BGRA, GL_UNSIGNED_BYTE, false};
        case DXGIFormat::R8_UNorm:
            return {GL_R8, GL_RED, GL_UNSIGNED_BYTE, false};
        case DXGIFormat::R8G8_UNorm:
            return {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, false};
        case DXGIFormat::R16_UNorm:
            return {GL_R16, GL_RED, GL_UNSIGNED_SHORT, false};
        case DXGIFormat::R16G16_UNorm:
            return {GL_RG16, GL_RG, GL_UNSIGNED_SHORT, false};
        default:
            break;
        }

        return {};
    }

    bool LoadDdsTexture(const std::string& path,
                        TextureColorSpace colorSpace,
                        SamplerDesc sampler,
                        bool flipY,
                        GLuint& outId,
                        int& outWidth,
                        int& outHeight)
    {
        tinyddsloader::DDSFile dds;
        const auto result = dds.Load(path.c_str());
        if (result != tinyddsloader::Success)
        {
            spdlog::error("[Texture2D] Failed to load DDS '{}': {}", path, static_cast<int>(result));
            return false;
        }

        if (flipY && !dds.Flip())
        {
            spdlog::warn("[Texture2D] DDS flip failed for '{}' — continuing without flip", path);
        }

        const auto translated = TranslateDdsFormat(dds.GetFormat(), colorSpace);
        if (translated.internalFormat == 0)
        {
            spdlog::error("[Texture2D] Unsupported DDS format in '{}': {}", path, static_cast<uint32_t>(dds.GetFormat()));
            return false;
        }

        outWidth = static_cast<int>(dds.GetWidth());
        outHeight = static_cast<int>(dds.GetHeight());

        glGenTextures(1, &outId);
        glBindTexture(GL_TEXTURE_2D, outId);

        const uint32_t mipCount = dds.GetMipCount();
        for (uint32_t mip = 0; mip < mipCount; ++mip)
        {
            const tinyddsloader::DDSFile::ImageData* image = dds.GetImageData(mip, 0);
            if (!image)
            {
                spdlog::error("[Texture2D] Missing DDS mip {} in '{}'", mip, path);
                glBindTexture(GL_TEXTURE_2D, 0);
                glDeleteTextures(1, &outId);
                outId = 0;
                return false;
            }

            const GLsizei width = static_cast<GLsizei>(image->m_width);
            const GLsizei height = static_cast<GLsizei>(image->m_height);

            if (translated.compressed)
            {
                glCompressedTexImage2D(GL_TEXTURE_2D,
                                       static_cast<GLint>(mip),
                                       static_cast<GLenum>(translated.internalFormat),
                                       width,
                                       height,
                                       0,
                                       static_cast<GLsizei>(image->m_memSlicePitch),
                                       image->m_mem);
            }
            else
            {
                glTexImage2D(GL_TEXTURE_2D,
                             static_cast<GLint>(mip),
                             static_cast<GLint>(translated.internalFormat),
                             width,
                             height,
                             0,
                             translated.format,
                             translated.type,
                             image->m_mem);
            }
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(sampler.minFilter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(sampler.magFilter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     static_cast<GLint>(sampler.wrapS));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     static_cast<GLint>(sampler.wrapT));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(mipCount > 0 ? mipCount - 1 : 0));
        glBindTexture(GL_TEXTURE_2D, 0);

        return true;
    }
}
#include <vector>

Texture2D::Texture2D(const std::string& path,
                     TextureColorSpace  colorSpace,
                     SamplerDesc        sampler,
                     bool               flipY)
    : m_path(path)
{
    if (IsDdsPath(path))
    {
        if (!LoadDdsTexture(path, colorSpace, sampler, flipY, m_id, m_width, m_height))
            return;

        spdlog::info("[Texture2D] Loaded DDS '{}' ({}x{})", path, m_width, m_height);
        return;
    }

    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &m_width, &m_height, &channels, 4);
    if (!data) {
        spdlog::error("[Texture2D] Failed to load '{}': {}", path, stbi_failure_reason());
        return;
    }

    // Pick internal format based on color-space policy.
    // We always load 4 channels (RGBA) to keep the upload path uniform.
    const GLenum internalFmt = (colorSpace == TextureColorSpace::sRGB)
                               ? GL_SRGB8_ALPHA8
                               : GL_RGBA8;

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFmt),
                 m_width, m_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(sampler.minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(sampler.magFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     static_cast<GLint>(sampler.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     static_cast<GLint>(sampler.wrapT));

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    spdlog::info("[Texture2D] Loaded '{}' ({}x{}, {} ch, {})",
                 path, m_width, m_height, channels,
                 (colorSpace == TextureColorSpace::sRGB) ? "sRGB" : "Linear");

    if (m_width > 4096 || m_height > 4096) {
        spdlog::warn("[Texture2D] Large texture detected: {}x{}. This may impact performance and memory.", 
                     m_width, m_height);
    }
}

Texture2D Texture2D::CreateFallback(unsigned char r, unsigned char g,
                                     unsigned char b, unsigned char a)
{
    Texture2D t;
    t.m_width  = 1;
    t.m_height = 1;
    t.m_path   = "<fallback>";

    const unsigned char pixel[4] = {r, g, b, a};

    glGenTextures(1, &t.m_id);
    glBindTexture(GL_TEXTURE_2D, t.m_id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);

    spdlog::debug("[Texture2D] Created fallback ({},{},{},{})", r, g, b, a);
    return t;
}

Texture2D Texture2D::CreateCheckerboard(int size)
{
    Texture2D t;
    t.m_width  = size;
    t.m_height = size;
    t.m_path   = "<checkerboard>";

    // Build a magenta/black checkerboard pattern in CPU memory.
    // Each pixel is 4 bytes (RGBA).
    std::vector<unsigned char> pixels(size * size * 4);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            // If (x + y) is even -> magenta, odd -> black
            const bool magenta = ((x + y) % 2 == 0);
            const int idx = (y * size + x) * 4;
            pixels[idx + 0] = magenta ? 255 : 0;   // R
            pixels[idx + 1] = 0;                    // G
            pixels[idx + 2] = magenta ? 255 : 0;   // B
            pixels[idx + 3] = 255;                  // A
        }
    }

    glGenTextures(1, &t.m_id);
    glBindTexture(GL_TEXTURE_2D, t.m_id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 size, size, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // No mipmaps — nearest filtering so the pattern stays sharp and obvious.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);

    spdlog::debug("[Texture2D] Created checkerboard fallback ({}x{})", size, size);
    return t;
}

Texture2D Texture2D::CreateRenderTarget(int width,
                                        int height,
                                        GLenum internalFormat,
                                        GLenum format,
                                        GLenum type,
                                        SamplerDesc sampler)
{
    Texture2D texture;
    texture.m_width = width;
    texture.m_height = height;
    texture.m_path = "<runtime>";

    if (texture.m_width <= 0 || texture.m_height <= 0)
    {
        spdlog::error("[Texture2D] Invalid render-target size: {}x{}", texture.m_width, texture.m_height);
        return texture;
    }

    glGenTextures(1, &texture.m_id);
    glBindTexture(GL_TEXTURE_2D, texture.m_id);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 static_cast<GLint>(internalFormat),
                 texture.m_width,
                 texture.m_height,
                 0,
                 format,
                 type,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(sampler.minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(sampler.magFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(sampler.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(sampler.wrapT));
    glBindTexture(GL_TEXTURE_2D, 0);

    spdlog::info("[Texture2D] Created runtime render target ({}x{})", texture.m_width, texture.m_height);
    return texture;
}

Texture2D Texture2D::CreateFromRGBAData(int width,
                                        int height,
                                        const unsigned char* rgbaData,
                                        TextureColorSpace colorSpace,
                                        SamplerDesc sampler,
                                        bool generateMipmaps)
{
    Texture2D texture;
    texture.m_width = width;
    texture.m_height = height;
    texture.m_path = "<runtime>";

    if (texture.m_width <= 0 || texture.m_height <= 0 || rgbaData == nullptr)
    {
        spdlog::error("[Texture2D] Invalid pixel data texture: {}x{}", texture.m_width, texture.m_height);
        return texture;
    }

    const GLenum internalFmt = (colorSpace == TextureColorSpace::sRGB)
                               ? GL_SRGB8_ALPHA8
                               : GL_RGBA8;

    glGenTextures(1, &texture.m_id);
    glBindTexture(GL_TEXTURE_2D, texture.m_id);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 static_cast<GLint>(internalFmt),
                 texture.m_width,
                 texture.m_height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 rgbaData);

    if (generateMipmaps)
        glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(sampler.minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(sampler.magFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(sampler.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(sampler.wrapT));
    glBindTexture(GL_TEXTURE_2D, 0);

    spdlog::info("[Texture2D] Created runtime texture from pixels ({}x{}, {})",
                 texture.m_width,
                 texture.m_height,
                 (colorSpace == TextureColorSpace::sRGB) ? "sRGB" : "Linear");
    return texture;
}

Texture2D::~Texture2D() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
}

Texture2D::Texture2D(Texture2D&& other) noexcept
    : m_id(other.m_id)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_path(std::move(other.m_path))
{
    other.m_id = 0;
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) glDeleteTextures(1, &m_id);
        m_id    = other.m_id;
        m_width = other.m_width;
        m_height= other.m_height;
        m_path  = std::move(other.m_path);
        other.m_id = 0;
    }
    return *this;
}

void Texture2D::Bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture2D::Unbind(GLuint unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, 0);
}
