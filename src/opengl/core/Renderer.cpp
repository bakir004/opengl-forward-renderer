#include "core/Renderer.h"
#include "core/Camera.h"
#include "core/Mesh.h"
#include "core/MeshBuffer.h"
#include "scene/FrameSubmission.h"
#include "scene/LightBlock.h"
#include "scene/LightUtils.h"
#include "scene/RenderItem.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <cassert>

namespace {

void PopulateDebugStatsFromSubmission(const FrameSubmission& submission, RendererDebugStats& stats) {
    stats.submittedRenderItemCount = static_cast<uint32_t>(submission.objects.size());
    stats.queuedRenderItemCount = 0;
    stats.processedRenderItemCount = 0;
    stats.drawCallCount = 0;
    stats.approxTriangleCount = 0;
    stats.directionalLightCount = submission.lights.HasDirectionalLight() ? 1u : 0u;
    stats.pointLightCount = static_cast<uint32_t>(submission.lights.GetPointLights().size());
    stats.shadowCasterCount = 0;
    stats.frameTimeMs = submission.deltaTime * 1000.0f;
    stats.fps = submission.deltaTime > 0.0f ? (1.0f / submission.deltaTime) : 0.0f;
    stats.shadowCasterCountApproximate = true;
    stats.shadowPassDataAvailable = false;
    stats.approxTriangleCountApproximate = true;
}

} // namespace

bool Renderer::Initialize() {
    if (!m_initCtx.Initialize(m_currentContext))
        return false;

    m_errorShader = std::make_unique<ShaderProgram>(
        "assets/shaders/error.vert", "assets/shaders/error.frag");
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
    PopulateDebugStatsFromSubmission(submission, m_debugStats);

    if (!m_lightUBO)
        m_lightUBO = std::make_unique<UniformBuffer>(sizeof(LightBlock), GL_DYNAMIC_DRAW);

    LightBlock packedLights = LightBlock::Pack(submission.lights);
    const bool packedValid = LightUtils::ValidatePackedBlock(packedLights);
    if (!packedValid && !m_reportedInvalidPackedLights) {
        spdlog::warn("[Renderer] BeginFrame: packed light block has validation warnings");
        m_reportedInvalidPackedLights = true;
    } else if (packedValid && m_reportedInvalidPackedLights) {
        m_reportedInvalidPackedLights = false;
    }
    m_lightUBO->Upload(&packedLights, sizeof(LightBlock));
    m_lightUBO->BindToSlot(1);

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
    const RenderQueueFrameStats queueStats = m_queue.Flush(m_currentContext);
    m_debugStats.processedRenderItemCount = queueStats.processedItemCount;
    m_debugStats.drawCallCount = queueStats.drawCallCount;
    m_debugStats.approxTriangleCount = queueStats.approxTriangleCount;
    m_queue.Clear();
    m_inFrame = false;
}

void Renderer::SubmitDraw(const RenderItem& item) {
    assert(m_inFrame && "SubmitDraw() must be called between BeginFrame() and EndFrame()");
    if (m_queue.Add(item)) {
        ++m_debugStats.queuedRenderItemCount;
        if (item.flags.castShadow)
            ++m_debugStats.shadowCasterCount;
    }
}

void Renderer::Resize(int width, int height) {
    spdlog::debug("[Renderer] Resize: {}x{} (viewport applied via FrameSubmission each frame)",
                  width, height);
}
