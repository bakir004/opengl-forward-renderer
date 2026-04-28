#include "core/shadows/CascadedShadowMap.h"

#include <spdlog/spdlog.h>
#include <utility>

CascadedShadowMap::CascadedShadowMap(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height) {
    glGenTextures(1, &m_depthTextureArray);
    if (m_depthTextureArray == 0) {
        spdlog::error("[CascadedShadowMap] Failed to create depth texture array ({}x{} x{} layers)",
                      m_width, m_height, kNumCascades);
        return;
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthTextureArray);
    // Immutable storage is required for glTextureView, which we use below to
    // alias each array layer as a 2D texture for ImGui previews.
    glTexStorage3D(
        GL_TEXTURE_2D_ARRAY,
        1,
        GL_DEPTH_COMPONENT32F,
        static_cast<GLsizei>(m_width),
        static_cast<GLsizei>(m_height),
        static_cast<GLsizei>(kNumCascades));

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Outside-light-frustum UVs return "far" (1.0) so unshadowed fragments stay lit.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
    // Raw depth sampling (no hardware compare) — the mesh shader does the
    // compare itself, and previews need raw depth values.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // Per-layer 2D views for ImGui preview. glTextureView aliases a range of
    // layers from a source texture as a new target — here one layer each.
    // The views share storage; no per-frame blits or copies are needed.
    glGenTextures(kNumCascades, m_layerPreviewTextures.data());
    for (uint32_t i = 0; i < kNumCascades; ++i) {
        glTextureView(
            m_layerPreviewTextures[i],
            GL_TEXTURE_2D,
            m_depthTextureArray,
            GL_DEPTH_COMPONENT32F,
            0, 1,   // minlevel, numlevels
            i, 1);  // minlayer, numlayers
        // ImGui's default shader reads .rgba — swizzle R into RGB so depth
        // shows as grayscale, and force A to 1.0 so the preview is opaque.
        glBindTexture(GL_TEXTURE_2D, m_layerPreviewTextures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_fbo);
    if (m_fbo == 0) {
        spdlog::error("[CascadedShadowMap] Failed to create framebuffer ({}x{})", m_width, m_height);
        glDeleteTextures(1, &m_depthTextureArray);
        m_depthTextureArray = 0;
        return;
    }

    // Attach layer 0 initially and validate completeness. BindLayer() re-binds
    // a different layer before each cascade depth pass via glFramebufferTextureLayer.
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthTextureArray, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        spdlog::error(
            "[CascadedShadowMap] Framebuffer incomplete (status=0x{:X}, {}x{})",
            static_cast<uint32_t>(status), m_width, m_height);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_depthTextureArray);
        m_fbo = 0;
        m_depthTextureArray = 0;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    spdlog::info("[CascadedShadowMap] Created {}x{} x{} layers", m_width, m_height, kNumCascades);
}

CascadedShadowMap::~CascadedShadowMap() {
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    for (GLuint& view : m_layerPreviewTextures) {
        if (view != 0) {
            glDeleteTextures(1, &view);
            view = 0;
        }
    }
    if (m_depthTextureArray != 0) {
        glDeleteTextures(1, &m_depthTextureArray);
        m_depthTextureArray = 0;
    }
    if (m_width > 0 && m_height > 0) {
        spdlog::info("[CascadedShadowMap] Destroyed cascaded shadow resources ({}x{})", m_width, m_height);
    }
}

CascadedShadowMap::CascadedShadowMap(CascadedShadowMap&& other) noexcept
    : m_fbo(other.m_fbo)
    , m_depthTextureArray(other.m_depthTextureArray)
    , m_layerPreviewTextures(other.m_layerPreviewTextures)
    , m_width(other.m_width)
    , m_height(other.m_height) {
    other.m_fbo = 0;
    other.m_depthTextureArray = 0;
    other.m_layerPreviewTextures.fill(0);
    other.m_width = 0;
    other.m_height = 0;
}

CascadedShadowMap& CascadedShadowMap::operator=(CascadedShadowMap&& other) noexcept {
    if (this != &other) {
        if (m_fbo != 0)               glDeleteFramebuffers(1, &m_fbo);
        for (GLuint view : m_layerPreviewTextures)
            if (view != 0) glDeleteTextures(1, &view);
        if (m_depthTextureArray != 0) glDeleteTextures(1, &m_depthTextureArray);

        m_fbo = other.m_fbo;
        m_depthTextureArray = other.m_depthTextureArray;
        m_layerPreviewTextures = other.m_layerPreviewTextures;
        m_width = other.m_width;
        m_height = other.m_height;

        other.m_fbo = 0;
        other.m_depthTextureArray = 0;
        other.m_layerPreviewTextures.fill(0);
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void CascadedShadowMap::BindLayer(uint32_t layer) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthTextureArray,
                              0, static_cast<GLint>(layer));
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
}

void CascadedShadowMap::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
