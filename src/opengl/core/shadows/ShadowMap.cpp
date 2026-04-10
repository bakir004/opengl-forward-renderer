#include "core/shadows/ShadowMap.h"

#include <spdlog/spdlog.h>
#include <utility>

ShadowMap::ShadowMap(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height) {
    glGenTextures(1, &m_depthTexture);
    if (m_depthTexture == 0) {
        spdlog::error("[ShadowMap] Failed to create depth texture ({}x{})", m_width, m_height);
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT32F,
        static_cast<GLsizei>(m_width),
        static_cast<GLsizei>(m_height),
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Outside-light-frustum UVs should sample a far depth (1.0) so fragments are treated as lit.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_fbo);
    if (m_fbo == 0) {
        spdlog::error("[ShadowMap] Failed to create framebuffer ({}x{})", m_width, m_height);
        glDeleteTextures(1, &m_depthTexture);
        m_depthTexture = 0;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    // This framebuffer is depth-only: disabling color read/write avoids undefined color attachment behavior.
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        spdlog::error(
            "[ShadowMap] Framebuffer incomplete (status=0x{:X}, {}x{})",
            static_cast<uint32_t>(status),
            m_width,
            m_height);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_depthTexture);
        m_fbo = 0;
        m_depthTexture = 0;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    spdlog::info("[ShadowMap] Created depth-only shadow map ({}x{})", m_width, m_height);
}

ShadowMap::~ShadowMap() {
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_depthTexture != 0) {
        glDeleteTextures(1, &m_depthTexture);
        m_depthTexture = 0;
    }
    if (m_width > 0 && m_height > 0) {
        spdlog::info("[ShadowMap] Destroyed shadow map resources ({}x{})", m_width, m_height);
    }
}

ShadowMap::ShadowMap(ShadowMap&& other) noexcept
    : m_fbo(other.m_fbo)
    , m_depthTexture(other.m_depthTexture)
    , m_width(other.m_width)
    , m_height(other.m_height) {
    other.m_fbo = 0;
    other.m_depthTexture = 0;
    other.m_width = 0;
    other.m_height = 0;
}

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept {
    if (this != &other) {
        if (m_fbo != 0) {
            glDeleteFramebuffers(1, &m_fbo);
        }
        if (m_depthTexture != 0) {
            glDeleteTextures(1, &m_depthTexture);
        }

        m_fbo = other.m_fbo;
        m_depthTexture = other.m_depthTexture;
        m_width = other.m_width;
        m_height = other.m_height;

        other.m_fbo = 0;
        other.m_depthTexture = 0;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void ShadowMap::Bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
}

void ShadowMap::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint ShadowMap::GetDepthTexture() const {
    return m_depthTexture;
}

uint32_t ShadowMap::GetWidth() const {
    return m_width;
}

uint32_t ShadowMap::GetHeight() const {
    return m_height;
}

bool ShadowMap::IsValid() const {
    return m_fbo != 0 && m_depthTexture != 0;
}


