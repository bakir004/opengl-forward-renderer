#include "core/Texture2D.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <vector>

Texture2D::Texture2D(const std::string& path,
                     TextureColorSpace  colorSpace,
                     SamplerDesc        sampler,
                     bool               flipY)
    : m_path(path)
{
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
