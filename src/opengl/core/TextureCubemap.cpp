#include "core/TextureCubemap.h"
#include "core/Texture2D.h"
#include <stb_image.h>
#include <spdlog/spdlog.h>

TextureCubemap::TextureCubemap(const std::array<std::string, 6>& facePaths,
                                TextureColorSpace colorSpace,
                                bool flipY)
{
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);

    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    const GLenum internalFmt = (colorSpace == TextureColorSpace::sRGB)
                               ? GL_SRGB8_ALPHA8
                               : GL_RGBA8;

    for (unsigned int i = 0; i < 6; i++) {
        int channels = 0;
        unsigned char* data = stbi_load(facePaths[i].c_str(), &m_width, &m_height, &channels, 4);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, static_cast<GLint>(internalFmt), m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            spdlog::error("[TextureCubemap] Failed to load cubemap face at {}: {}", facePaths[i], stbi_failure_reason());
            stbi_image_free(data);
            glDeleteTextures(1, &m_id);
            m_id = 0;
            return;
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    spdlog::info("[TextureCubemap] Loaded cubemap from 6 faces ({}x{}, {})",
                 m_width, m_height, (colorSpace == TextureColorSpace::sRGB) ? "sRGB" : "Linear");
}

TextureCubemap TextureCubemap::CreateRenderTarget(int size,
                                                  int mipLevels,
                                                  GLenum internalFormat,
                                                  GLenum minFilter,
                                                  GLenum magFilter)
{
    TextureCubemap texture;
    texture.m_width = size;
    texture.m_height = size;
    texture.m_mipLevels = std::max(1, mipLevels);

    if (texture.m_width <= 0 || texture.m_height <= 0)
    {
        spdlog::error("[TextureCubemap] Invalid render-target size: {}x{}", texture.m_width, texture.m_height);
        return texture;
    }

    glGenTextures(1, &texture.m_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture.m_id);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP,
                   texture.m_mipLevels,
                   internalFormat,
                   static_cast<GLsizei>(texture.m_width),
                   static_cast<GLsizei>(texture.m_height));

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, texture.m_mipLevels - 1);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    spdlog::info("[TextureCubemap] Created runtime cubemap target ({}x{}, {} mips)",
                 texture.m_width, texture.m_height, texture.m_mipLevels);
    return texture;
}

TextureCubemap::~TextureCubemap() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
}

TextureCubemap::TextureCubemap(TextureCubemap&& other) noexcept
    : m_id(other.m_id)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_mipLevels(other.m_mipLevels)
{
    other.m_id = 0;
}

TextureCubemap& TextureCubemap::operator=(TextureCubemap&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) glDeleteTextures(1, &m_id);
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        m_mipLevels = other.m_mipLevels;
        other.m_id = 0;
    }
    return *this;
}

void TextureCubemap::Bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
}

void TextureCubemap::Unbind(GLuint unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}
