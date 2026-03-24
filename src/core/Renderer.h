#pragma once
#include <nlohmann/json_fwd.hpp>

/// Manages the OpenGL rendering pipeline.
/// Responsible for loading GL function pointers, issuing draw calls, and handling viewport changes.
class Renderer {
    public:
        /// Loads GL function pointers via GLAD and sets up the debug callback in debug builds.
        /// Must be called after a GL context is current.
        /// @return true on success, false if GLAD initialization fails
        bool Initialize();

        /// Issues draw calls for the current frame (clear + scene rendering).
        void RenderFrame();

        /// Updates the GL viewport to the new framebuffer dimensions.
        /// Called automatically by the framebuffer resize callback.
        /// @param width  New framebuffer width in pixels
        /// @param height New framebuffer height in pixels
        void Resize(int width, int height);

        /// Performs any cleanup before the GL context is destroyed.
        void Shutdown();
};
