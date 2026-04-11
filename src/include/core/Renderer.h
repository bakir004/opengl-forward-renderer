#pragma once
#include <cstdint>
#include <memory>
#include "core/InitContext.h"
#include "core/SubmissionContext.h"
#include "core/RenderQueue.h"
#include "core/UniformBuffer.h"
#include "core/ShaderProgram.h"

struct RenderItem;
struct FrameSubmission;

/// Small per-frame debug snapshot used by the runtime stats UI.
struct RendererDebugStats {
    uint32_t submittedRenderItemCount = 0;
    uint32_t queuedRenderItemCount    = 0;
    uint32_t processedRenderItemCount = 0;
    uint32_t drawCallCount            = 0;
    uint64_t approxTriangleCount      = 0;
    uint32_t directionalLightCount    = 0;
    uint32_t pointLightCount          = 0;
    uint32_t shadowCasterCount        = 0;
    float    frameTimeMs              = 0.0f;
    float    fps                      = 0.0f;

    // The count currently comes from queued RenderItem flags, not from a dedicated shadow pass.
    bool shadowCasterCountApproximate = true;
    bool shadowPassDataAvailable      = false;  // Placeholder until the shadow pass is integrated.
    bool approxTriangleCountApproximate = true; // Triangle estimates are pragmatic and treat non-triangle topologies as 0.
};

/// Manages the OpenGL rendering pipeline.
///
/// Renderer is a thin orchestrator — it owns InitContext, SubmissionContext, and RenderQueue,
/// but contains no direct GL calls itself.  All GL state changes happen inside those classes.
///
/// Per-frame usage:
///   BeginFrame(submission)          — clear, upload camera UBO, apply pipeline state
///   SubmitDraw(item) × N            — enqueue draw calls
///   EndFrame()                      — sort queue, flush all draws, reset queue
class Renderer {
    InitContext       m_initCtx;
    SubmissionContext m_currentContext;
    RenderQueue                    m_queue;
    std::unique_ptr<UniformBuffer> m_cameraUBO;
    std::unique_ptr<UniformBuffer> m_lightUBO;
    std::unique_ptr<ShaderProgram> m_errorShader;
    RendererDebugStats             m_debugStats;
    bool m_reportedInvalidPackedLights = false;
    bool m_inFrame = false;

public:
    /// Loads GL function pointers, enables debug output, and applies the default pipeline state.
    /// Must be called after a valid GL context is current.
    bool Initialize();

    /// Logs shutdown.  The GL context is destroyed by the caller (Application).
    void Shutdown();

    /// Begins a frame: applies FrameClearInfo (viewport + clear), uploads the camera UBO,
    /// and applies the submission's SubmissionContext.
    void BeginFrame(const FrameSubmission& submission);

    /// Ends a frame: sorts the render queue by shader, flushes all draws, clears the queue.
    void EndFrame();

    /// Enqueues a draw item. Must be called between BeginFrame and EndFrame.
    void SubmitDraw(const RenderItem& item);

    /// Called by the GLFW framebuffer-resize callback. Viewport is driven each frame
    /// through FrameSubmission::clearInfo; this method only logs the new dimensions.
    void Resize(int width, int height);

    /// Returns the latest per-frame debug snapshot for runtime UI.
    [[nodiscard]] const RendererDebugStats& GetDebugStats() const { return m_debugStats; }
};
