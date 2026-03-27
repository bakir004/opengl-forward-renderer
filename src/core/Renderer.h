#pragma once
#include <nlohmann/json_fwd.hpp>

/// Depth test comparison functions
enum class DepthFunc {
    Less,           // Pass if incoming < stored (default, standard 3D)
    LessEqual,      // Pass if incoming <= stored
    Greater,        // Pass if incoming > stored (reverse Z)
    GreaterEqual,   // Pass if incoming >= stored
    Always,         // Always pass (effectively disables depth test logic)
    Never,          // Never pass
    Equal,          // Pass if incoming == stored
    NotEqual        // Pass if incoming != stored
};

/// Blending modes for transparency and effects
enum class BlendMode {
    Disabled,       // No blending
    Alpha,          // Standard transparency: src_alpha, 1-src_alpha
    Additive,       // Light/particle effects: src + dst
    Multiply        // Darken/multiply: src * dst
};

/// Face culling modes
enum class CullMode {
    Disabled,       // Render both front and back faces
    Back,           // Cull back faces (default for solid objects)
    Front,          // Cull front faces (useful for inside-out geometry)
    FrontAndBack    // Cull everything (rarely used, except for depth-only passes)
};



/// Manages the OpenGL rendering pipeline.
/// Responsible for loading GL function pointers, issuing draw calls, and handling viewport changes.
class Renderer {

        // Cached pipeline state to avoid redundant GL calls
        bool m_depthTestEnabled = true;
        DepthFunc m_depthFunc = DepthFunc::Less;
        BlendMode m_blendMode = BlendMode::Disabled;
        CullMode m_cullMode = CullMode::Back;

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

        /// Enables or disables depth testing with a specified comparison function.
        /// @param enable Whether to enable depth testing
        /// @param func   Depth comparison function (default: Less)
        void SetDepthTest(bool enable, DepthFunc func = DepthFunc::Less);

        /// Configures blending mode for transparency and effects.
        /// @param mode The desired blend mode
        void SetBlendMode(BlendMode mode);

        /// Configures face culling to skip rendering front/back faces.
        /// @param mode The desired cull mode
        void SetCullMode(CullMode mode);
};
