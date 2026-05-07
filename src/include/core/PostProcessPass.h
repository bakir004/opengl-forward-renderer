#pragma once

#include <cstdint>

/// Base class for post-process effects.
/// Renders fullscreen quads with various shaders applied to texture inputs.
/// This is the infrastructure/plumbing that all concrete post-process passes inherit from.
class PostProcessPass
{
public:
    virtual ~PostProcessPass() = default;

    /// Executes this post-process pass.
    /// @param inputTexture  GL texture ID to read from
    /// @param outputFbo     GL framebuffer ID to render into (0 = default framebuffer)
    /// @param width, height Viewport dimensions
    virtual void Execute(uint32_t inputTexture, uint32_t outputFbo, uint32_t width, uint32_t height) = 0;

protected:
    PostProcessPass() = default;

    /// Sets up viewport for rendering and saves current state
    void SetupViewport(uint32_t width, uint32_t height);

    /// Restores previously saved viewport state
    void RestoreViewport();

private:
    uint32_t m_savedViewport[4] = {0, 0, 0, 0};
};
