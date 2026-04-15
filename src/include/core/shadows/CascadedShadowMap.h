#pragma once

#include <array>
#include <cstdint>
#include <glad/glad.h>

/// RAII owner of a depth-only framebuffer whose attachment is a GL_TEXTURE_2D_ARRAY.
/// Each array layer stores one cascade of a cascaded shadow map (CSM).
///
/// The renderer renders the scene once per cascade, calling BindLayer(i) to
/// reattach the FBO's depth target to the i-th layer before each depth pass.
class CascadedShadowMap {
public:
    /// Number of cascades used by the renderer.  Fixed at compile time so the
    /// mesh shader can declare its uniform arrays with a known size.
    static constexpr uint32_t kNumCascades = 4;

    /// Creates a depth GL_TEXTURE_2D_ARRAY with kNumCascades layers and a
    /// single depth-only framebuffer whose attachment is re-bound per layer.
    CascadedShadowMap(uint32_t width, uint32_t height);
    ~CascadedShadowMap();

    CascadedShadowMap(const CascadedShadowMap&) = delete;
    CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;

    CascadedShadowMap(CascadedShadowMap&& other) noexcept;
    CascadedShadowMap& operator=(CascadedShadowMap&& other) noexcept;

    /// Binds the FBO, reattaches the depth target to array layer `layer`, and
    /// sets the viewport to the shadow map resolution.  Subsequent depth draws
    /// write into that cascade only.
    void BindLayer(uint32_t layer);

    /// Rebinds the default framebuffer.
    void Unbind();

    /// Returns the GL_TEXTURE_2D_ARRAY holding all cascade depth layers.
    [[nodiscard]] GLuint GetDepthTextureArray() const { return m_depthTextureArray; }

    /// Returns a GL_TEXTURE_2D view that aliases a single cascade layer as a
    /// regular 2D texture — suitable for ImGui::Image preview. The underlying
    /// storage is shared with the depth array via glTextureView, so no copy
    /// happens at render time. Returns 0 if texture views are unsupported or
    /// creation failed.
    [[nodiscard]] GLuint GetLayerPreviewTexture(uint32_t layer) const {
        return layer < kNumCascades ? m_layerPreviewTextures[layer] : 0;
    }

    [[nodiscard]] uint32_t GetWidth()  const { return m_width;  }
    [[nodiscard]] uint32_t GetHeight() const { return m_height; }

    [[nodiscard]] bool IsValid() const { return m_fbo != 0 && m_depthTextureArray != 0; }

private:
    GLuint   m_fbo               = 0;
    GLuint   m_depthTextureArray = 0;
    std::array<GLuint, kNumCascades> m_layerPreviewTextures{};
    uint32_t m_width             = 0;
    uint32_t m_height            = 0;
};
