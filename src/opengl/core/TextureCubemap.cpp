#include "core/TextureCubemap.h"
#include "core/Texture2D.h"
#include <stb_image.h>
#include <spdlog/spdlog.h>
#include <algorithm>

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
    m_internalFormat = internalFmt;

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
    texture.m_internalFormat = internalFormat;

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
    for (GLuint& previewTexture : m_facePreviewTextures) {
        if (previewTexture != 0)
            glDeleteTextures(1, &previewTexture);
        previewTexture = 0;
    }
}

TextureCubemap::TextureCubemap(TextureCubemap&& other) noexcept
    : m_id(other.m_id)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_mipLevels(other.m_mipLevels)
    , m_internalFormat(other.m_internalFormat)
    , m_facePreviewTextures(other.m_facePreviewTextures)
    , m_facePreviewMipLevels(other.m_facePreviewMipLevels)
    , m_facePreviewWidths(other.m_facePreviewWidths)
    , m_facePreviewHeights(other.m_facePreviewHeights)
{
    other.m_id = 0;
    other.m_facePreviewTextures.fill(0);
}

TextureCubemap& TextureCubemap::operator=(TextureCubemap&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) glDeleteTextures(1, &m_id);
        for (GLuint& previewTexture : m_facePreviewTextures) {
            if (previewTexture != 0)
                glDeleteTextures(1, &previewTexture);
            previewTexture = 0;
        }
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        m_mipLevels = other.m_mipLevels;
        m_internalFormat = other.m_internalFormat;
        m_facePreviewTextures = other.m_facePreviewTextures;
        m_facePreviewMipLevels = other.m_facePreviewMipLevels;
        m_facePreviewWidths = other.m_facePreviewWidths;
        m_facePreviewHeights = other.m_facePreviewHeights;
        other.m_id = 0;
        other.m_facePreviewTextures.fill(0);
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

GLuint TextureCubemap::GetFacePreviewTexture(int faceIndex, int mipLevel) const {
    if (!IsValid() || faceIndex < 0 || faceIndex >= 6)
        return 0;

    const int clampedMip = std::clamp(mipLevel, 0, std::max(0, m_mipLevels - 1));
    const int previewWidth = std::max(1, m_width >> clampedMip);
    const int previewHeight = std::max(1, m_height >> clampedMip);

    GLuint& previewTexture = m_facePreviewTextures[static_cast<std::size_t>(faceIndex)];
    int& previewMip = m_facePreviewMipLevels[static_cast<std::size_t>(faceIndex)];
    int& storedWidth = m_facePreviewWidths[static_cast<std::size_t>(faceIndex)];
    int& storedHeight = m_facePreviewHeights[static_cast<std::size_t>(faceIndex)];

    if (previewTexture != 0 &&
        previewMip == clampedMip &&
        storedWidth == previewWidth &&
        storedHeight == previewHeight) {
        return previewTexture;
    }

    if (previewTexture == 0)
        glGenTextures(1, &previewTexture);

    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousReadFramebuffer = 0;
    GLint previousDrawFramebuffer = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, previewTexture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 static_cast<GLint>(m_internalFormat),
                 previewWidth,
                 previewHeight,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint readFramebuffer = 0;
    glGenFramebuffers(1, &readFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
                           m_id,
                           clampedMip);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    const GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE) {
        glCopyTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            0,
                            0,
                            previewWidth,
                            previewHeight);
        previewMip = clampedMip;
        storedWidth = previewWidth;
        storedHeight = previewHeight;
    } else {
        spdlog::warn("[TextureCubemap] Cubemap face preview FBO incomplete (status=0x{:X})",
                     static_cast<unsigned int>(status));
    }

    glDeleteFramebuffers(1, &readFramebuffer);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDrawFramebuffer);
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));

    return status == GL_FRAMEBUFFER_COMPLETE ? previewTexture : 0;
}
