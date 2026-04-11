#pragma once

#include <cstdint>
#include <glad/glad.h>

/// RAII owner of a depth-only framebuffer and depth texture used for directional shadow mapping.
/// The shadow map is fully isolated from the main scene framebuffer.
class ShadowMap {
public:
    /// Creates the depth texture and framebuffer at the requested resolution.
    /// @param width  Shadow map texture width in pixels.
    /// @param height Shadow map texture height in pixels.
    ShadowMap(uint32_t width, uint32_t height);

    /// Deletes the underlying framebuffer and depth texture objects.
    ~ShadowMap();

    /// Copy is disabled to enforce unique ownership of GL resources.
    ShadowMap(const ShadowMap&) = delete;
    /// Copy is disabled to enforce unique ownership of GL resources.
    ShadowMap& operator=(const ShadowMap&) = delete;

    /// Transfers ownership of GL resources; leaves other in a null (non-owning) state.
    ShadowMap(ShadowMap&& other) noexcept;
    /// Transfers ownership of GL resources; releases any resources currently owned by this object.
    ShadowMap& operator=(ShadowMap&& other) noexcept;

    /// Binds the depth-only framebuffer and sets viewport to shadow map resolution.
    void Bind();

    /// Rebinds the default framebuffer.
    void Unbind();

    /// Returns the OpenGL texture ID for the shadow depth map.
    [[nodiscard]] GLuint GetDepthTexture() const;

    /// Returns the configured shadow map width.
    [[nodiscard]] uint32_t GetWidth() const;

    /// Returns the configured shadow map height.
    [[nodiscard]] uint32_t GetHeight() const;

    /// Returns true if both framebuffer and depth texture were created successfully.
    [[nodiscard]] bool IsValid() const;

private:
    GLuint m_fbo = 0;           // Depth-only framebuffer object for shadow rendering.
    GLuint m_depthTexture = 0;  // Depth texture attached to m_fbo at GL_DEPTH_ATTACHMENT.
    uint32_t m_width = 0;       // Shadow map width in pixels.
    uint32_t m_height = 0;      // Shadow map height in pixels.
};
