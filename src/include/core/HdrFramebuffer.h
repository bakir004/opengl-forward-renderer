#pragma once

#include <cstdint>

/// RAII owner of an HDR-capable offscreen framebuffer.
///
/// Holds a GL_RGBA16F floating-point color attachment and a depth+stencil
/// renderbuffer.  The renderer renders the full scene into this FBO instead
/// of the default framebuffer, allowing HDR color values > 1.0 to survive
/// until tone mapping is applied in a post-process pass.
///
/// Usage per frame:
///   Bind()    — redirect draws into the HDR FBO
///   Unbind()  — return to the default framebuffer for post-processing
///
/// The color texture is accessible via GetColorTexture() for the post-process
/// pass (Person 4) to sample during tone mapping and bloom composition.
class HdrFramebuffer
{
public:
    /// Creates the FBO, a GL_RGBA16F color texture, and a depth+stencil
    /// renderbuffer at the given resolution.
    HdrFramebuffer(uint32_t width, uint32_t height);
    ~HdrFramebuffer();

    HdrFramebuffer(const HdrFramebuffer&) = delete;
    HdrFramebuffer& operator=(const HdrFramebuffer&) = delete;

    HdrFramebuffer(HdrFramebuffer&& other) noexcept;
    HdrFramebuffer& operator=(HdrFramebuffer&& other) noexcept;

    /// Binds the HDR FBO and sets the viewport to the framebuffer resolution.
    void Bind() const;

    /// Rebinds the default framebuffer (FBO 0).
    void Unbind() const;

    /// Recreates the FBO and attachments at a new resolution.
    /// Safe to call every frame — skips the recreate when dimensions match.
    void Resize(uint32_t width, uint32_t height);

    /// Returns the GL_RGBA16F color texture for the post-process pass.
    [[nodiscard]] uint32_t GetColorTexture() const { return m_colorTexture; }

    [[nodiscard]] uint32_t GetWidth()  const { return m_width;  }
    [[nodiscard]] uint32_t GetHeight() const { return m_height; }

    [[nodiscard]] bool IsValid() const { return m_fbo != 0 && m_colorTexture != 0; }

private:
    uint32_t m_fbo          = 0;
    uint32_t m_colorTexture = 0;
    uint32_t m_depthRbo     = 0;
    uint32_t m_width        = 0;
    uint32_t m_height       = 0;

    void Create(uint32_t width, uint32_t height);
    void Destroy();
};
