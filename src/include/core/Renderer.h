#pragma once
#include <memory>
#include "core/InitContext.h"
#include "core/SubmissionContext.h"
#include "core/RenderQueue.h"
#include "core/UniformBuffer.h"
#include "core/ShaderProgram.h"

struct RenderItem;
struct FrameSubmission;

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
};
