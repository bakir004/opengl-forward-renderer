#pragma once
#include <glm/glm.hpp>
#include <memory>
#include "UniformBuffer.h"

class ShaderProgram;
class MeshBuffer;
struct RenderItem;
struct FrameSubmission;

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

struct Viewport {
    int x = 0; ///< Bottom-left X origin (usually 0)
    int y = 0; ///< Bottom-left Y origin (usually 0)
    int width = 0; ///< Framebuffer width
    int height = 0; ///< Framebuffer height

    bool operator==(const Viewport& o) const {
        return x == o.x && y == o.y && width == o.width && height == o.height;
    }
    bool operator!=(const Viewport& o) const { return !(*this == o); }
};
/// Bitmask that controls which GL buffers are cleared.
///
/// Combine flags with bitwise OR:
/// @code
///     renderer.Clear(ClearFlags::Color | ClearFlags::Depth);
/// @endcode
enum class ClearFlags : unsigned int {
    None = 0u,           ///< Clear nothing
    Color = 1u << 0u,    ///< Clear the colour buffer
    Depth = 1u << 1u,    ///< Clear the depth buffer
    Stencil = 1u << 2u,    ///< Clear the stencil buffer
    // Convenience aliases ─────────────────────────────────────────────────
    ColorDepth = (1u << 0u) | (1u << 1u),                    ///< Colour + depth (most common)
    All = (1u << 0u) | (1u << 1u) | (1u << 2u)        ///< All three buffers
};

/// Bitwise OR so callers can write ClearFlags::Color | ClearFlags::Depth.
inline ClearFlags operator|(ClearFlags a, ClearFlags b) {
    return static_cast<ClearFlags>(
        static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}
/// Bitwise AND used internally to test whether a flag is set.
inline ClearFlags operator&(ClearFlags a, ClearFlags b) {
    return static_cast<ClearFlags>(
        static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}
/// Returns true when no flags are set (needed for the != None check below).
inline bool operator!(ClearFlags f) {
    return static_cast<unsigned int>(f) == 0u;
}
struct FrameParams {
    /// RGBA colour written to the colour buffer during Clear().
    glm::vec4  clearColor = { 0.1f, 0.1f, 0.1f, 1.0f };

    /// Which buffers to clear at the start of the frame.
    ClearFlags clearFlags = ClearFlags::ColorDepth;
};

/// Manages the OpenGL rendering pipeline.
/// Responsible for loading GL function pointers, issuing draw calls, and handling viewport changes.
class Renderer {
	//cached viewport and clear colour to avoid redundant GL calls when the same state is set multiple times
        Viewport   m_viewport = {};
        glm::vec4  m_clearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
        // Cached pipeline state to avoid redundant GL calls
        bool m_depthTestEnabled = false;
        DepthFunc m_depthFunc = DepthFunc::Less;
        BlendMode m_blendMode = BlendMode::Disabled;
        CullMode m_cullMode = CullMode::Disabled;

        // Set to true between BeginFrame() and EndFrame() so we can assert on
        // mismatched calls in debug builds.
        bool m_inFrame = false;

        std::unique_ptr<UniformBuffer> m_cameraUBO;

    public:
        /// Loads GL function pointers via GLAD and sets up the debug callback in debug builds.
        /// Must be called after a GL context is current.
        /// @return true on success, false if GLAD initialization fails
        bool Initialize();
        /// Performs any cleanup before the GL context is destroyed.
        void Shutdown();
        void BeginFrame(const FrameParams& params = {});
        void BeginFrame(const class FrameSubmission& submission);
        void EndFrame();
        void SetViewport(int x, int y, int width, int height);
        /// Viewport overload that accepts a pre-filled Viewport struct.
        void SetViewport(const Viewport& vp);

        /// Sets the colour written when clearing the colour buffer.  Cached.
        void SetClearColor(const glm::vec4& color);

        /// Reads back the currently cached clear colour (no GL round-trip needed).
        [[nodiscard]] const glm::vec4& GetClearColor() const { return m_clearColor; }

        /// Reads back the currently cached viewport.
        [[nodiscard]] const Viewport& GetViewport() const { return m_viewport; }

        void Clear(ClearFlags flags = ClearFlags::ColorDepth);
        /// Updates the viewport to the new framebuffer dimensions.
        /// Called automatically by the GLFW framebuffer-resize callback in
        /// Application — you should not need to call this manually.
        void Resize(int width, int height);

       
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

        void SubmitDraw(const ShaderProgram& shader, const MeshBuffer& mesh);
        void SubmitDraw(const ShaderProgram& shader, const MeshBuffer& mesh, const glm::mat4& modelMatrix);
        void SubmitDraw(const RenderItem& item);
        void UnbindShader();
};
