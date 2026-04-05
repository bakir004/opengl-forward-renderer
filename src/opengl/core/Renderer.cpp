#include "core/Renderer.h"
#include "core/Camera.h"
#include "scene/FrameSubmission.h"
#include "scene/RenderItem.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <cassert>

bool Renderer::Initialize() {
    if (!m_initCtx.Initialize(m_currentContext))
        return false;

    m_errorShader = std::make_unique<ShaderProgram>(
        "assets/shaders/basic.vert", "assets/shaders/basic.frag");
    if (m_errorShader->IsValid())
        m_queue.SetErrorShader(m_errorShader.get());
    else
        spdlog::warn("[Renderer] Error shader failed to load — missing-material objects will be silently dropped");

    return true;
}

void Renderer::Shutdown() {
    m_initCtx.Shutdown();
}

void Renderer::BeginFrame(const FrameSubmission& submission) {
    assert(!m_inFrame && "BeginFrame() called without a matching EndFrame()");
    m_inFrame = true;

    if (submission.camera) {
        if (!m_cameraUBO)
            m_cameraUBO = std::make_unique<UniformBuffer>(sizeof(CameraData), GL_DYNAMIC_DRAW);

        CameraData camData = submission.camera->BuildCameraData(submission.time, submission.deltaTime);
        m_cameraUBO->Upload(&camData, sizeof(CameraData));
        m_cameraUBO->BindToSlot(0);
    }

    submission.clearInfo.Apply();
    submission.context.Apply(m_currentContext);
}

void Renderer::EndFrame() {
    assert(m_inFrame && "EndFrame() called without a matching BeginFrame()");
    m_queue.Sort();
    m_queue.Flush(m_currentContext);
    m_queue.Clear();
    m_inFrame = false;
}

void Renderer::SubmitDraw(const RenderItem& item) {
    assert(m_inFrame && "SubmitDraw() must be called between BeginFrame() and EndFrame()");
    m_queue.Add(item);
}

void Renderer::Resize(int width, int height) {
    spdlog::debug("[Renderer] Resize: {}x{} (viewport applied via FrameSubmission each frame)",
                  width, height);
}
