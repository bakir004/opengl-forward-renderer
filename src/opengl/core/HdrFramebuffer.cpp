#include "core/HdrFramebuffer.h"

#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <utility>

HdrFramebuffer::HdrFramebuffer(uint32_t width, uint32_t height)
{
    Create(width, height);
}

HdrFramebuffer::~HdrFramebuffer()
{
    Destroy();
}

HdrFramebuffer::HdrFramebuffer(HdrFramebuffer&& other) noexcept
    : m_fbo(other.m_fbo)
    , m_colorTexture(other.m_colorTexture)
    , m_depthRbo(other.m_depthRbo)
    , m_width(other.m_width)
    , m_height(other.m_height)
{
    other.m_fbo          = 0;
    other.m_colorTexture = 0;
    other.m_depthRbo     = 0;
    other.m_width        = 0;
    other.m_height       = 0;
}

HdrFramebuffer& HdrFramebuffer::operator=(HdrFramebuffer&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_fbo          = other.m_fbo;
        m_colorTexture = other.m_colorTexture;
        m_depthRbo     = other.m_depthRbo;
        m_width        = other.m_width;
        m_height       = other.m_height;
        other.m_fbo          = 0;
        other.m_colorTexture = 0;
        other.m_depthRbo     = 0;
        other.m_width        = 0;
        other.m_height       = 0;
    }
    return *this;
}

void HdrFramebuffer::Bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
}

void HdrFramebuffer::Unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void HdrFramebuffer::Resize(uint32_t width, uint32_t height)
{
    if (width == m_width && height == m_height)
        return;
    Destroy();
    Create(width, height);
}

void HdrFramebuffer::Create(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        spdlog::warn("[HdrFramebuffer] Cannot create framebuffer with zero dimension ({}x{})", width, height);
        return;
    }

    m_width  = width;
    m_height = height;

    // --- Floating-point color texture (GL_RGBA16F) ---
    // Stores HDR values > 1.0 produced by PBR lighting. Bilinear filter is fine
    // since the post-process quad samples it at full resolution.
    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 static_cast<GLsizei>(m_width),
                 static_cast<GLsizei>(m_height),
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // --- Depth + stencil renderbuffer ---
    // A renderbuffer is sufficient because we never need to sample depth in the
    // post-process pass; only the color attachment needs to be a texture.
    glGenRenderbuffers(1, &m_depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          static_cast<GLsizei>(m_width),
                          static_cast<GLsizei>(m_height));
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // --- Assemble the FBO ---
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        spdlog::error("[HdrFramebuffer] Framebuffer incomplete (status=0x{:X}, {}x{})",
                      static_cast<uint32_t>(status), m_width, m_height);
        Destroy();
        return;
    }

    spdlog::info("[HdrFramebuffer] Created GL_RGBA16F offscreen target ({}x{})", m_width, m_height);
}

void HdrFramebuffer::Destroy()
{
    if (m_fbo != 0)
    {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_colorTexture != 0)
    {
        glDeleteTextures(1, &m_colorTexture);
        m_colorTexture = 0;
    }
    if (m_depthRbo != 0)
    {
        glDeleteRenderbuffers(1, &m_depthRbo);
        m_depthRbo = 0;
    }
    if (m_width > 0 && m_height > 0)
    {
        spdlog::info("[HdrFramebuffer] Destroyed HDR framebuffer ({}x{})", m_width, m_height);
        m_width  = 0;
        m_height = 0;
    }
}
