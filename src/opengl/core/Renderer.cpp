#include "core/Renderer.h"
#include "core/Camera.h"
#include "core/Mesh.h"
#include "core/MeshBuffer.h"
#include "core/shadows/ShadowMap.h"
#include "scene/FrameSubmission.h"
#include "scene/LightBlock.h"
#include "scene/LightUtils.h"
#include "scene/RenderItem.h"
#include <glad/glad.h>
#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <cassert>
#include <limits>

namespace {

constexpr float kMinShadowSceneRadius = 5.0f;

GLenum ToGLPrimitive(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::Triangles:     return GL_TRIANGLES;
        case PrimitiveTopology::Lines:         return GL_LINES;
        case PrimitiveTopology::Points:        return GL_POINTS;
        case PrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
        case PrimitiveTopology::LineStrip:     return GL_LINE_STRIP;
    }
    return GL_TRIANGLES;
}

bool HasDrawableGeometry(const RenderItem& item) {
    return item.mesh != nullptr || item.meshMulti != nullptr;
}

bool CanRenderInDirectionalShadowPass(const RenderItem& item) {
    return item.flags.visible && item.flags.castShadow && HasDrawableGeometry(item);
}

glm::mat4 BuildItemModelMatrix(const RenderItem& item) {
    glm::mat4 model = item.transform.GetModelMatrix();

    const glm::vec3& tOff = item.translationOffset;
    if (tOff != glm::vec3(0.0f))
        model = model * glm::translate(glm::mat4(1.0f), tOff);

    const glm::vec3& rOff = item.rotationOffsetDeg;
    if (rOff.x != 0.0f) model = model * glm::rotate(glm::mat4(1.0f), glm::radians(rOff.x), {1,0,0});
    if (rOff.y != 0.0f) model = model * glm::rotate(glm::mat4(1.0f), glm::radians(rOff.y), {0,1,0});
    if (rOff.z != 0.0f) model = model * glm::rotate(glm::mat4(1.0f), glm::radians(rOff.z), {0,0,1});

    return model;
}

struct ShadowSceneBounds {
    glm::vec3 center = glm::vec3(0.0f);
    float     radius = kMinShadowSceneRadius;
};

ShadowSceneBounds ComputeDirectionalShadowBounds(const std::vector<RenderItem>& items) {
    glm::vec3 minPoint( std::numeric_limits<float>::max());
    glm::vec3 maxPoint(-std::numeric_limits<float>::max());
    bool hasAnyCaster = false;

    for (const RenderItem& item : items) {
        if (!CanRenderInDirectionalShadowPass(item))
            continue;

        const glm::vec3 anchor = item.transform.GetTranslation() + item.translationOffset;
        const glm::vec3 scale  = glm::abs(item.transform.GetScale());
        const float extent     = std::max({0.5f, scale.x, scale.y, scale.z});
        const glm::vec3 pad(extent);

        minPoint = glm::min(minPoint, anchor - pad);
        maxPoint = glm::max(maxPoint, anchor + pad);
        hasAnyCaster = true;
    }

    ShadowSceneBounds bounds{};
    if (!hasAnyCaster)
        return bounds;

    bounds.center = (minPoint + maxPoint) * 0.5f;
    bounds.radius = std::max(kMinShadowSceneRadius, glm::length(maxPoint - bounds.center));
    return bounds;
}

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
    stats.shadowMapTextureId = 0;
    stats.shadowMapWidth = 0;
    stats.shadowMapHeight = 0;
    stats.shadowMapPreviewAvailable = false;
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

    m_shadowDepthShader = std::make_unique<ShaderProgram>(
        "assets/shaders/shadow_depth.vert", "assets/shaders/shadow_depth.frag");

    return true;
}

void Renderer::Shutdown() {
    m_initCtx.Shutdown();
}

void Renderer::BeginFrame(const FrameSubmission& submission) {
    assert(!m_inFrame && "BeginFrame() called without a matching EndFrame()");
    m_inFrame = true;
    PopulateDebugStatsFromSubmission(submission, m_debugStats);
    RenderDirectionalShadowPass(submission);

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

void Renderer::RenderDirectionalShadowPass(const FrameSubmission& submission) {
    if (!submission.lights.HasDirectionalLight())
        return;

    const DirectionalLight& light = submission.lights.GetDirectionalLight();
    if (!light.enabled || !light.shadow.castShadow)
        return;

    if (!m_shadowDepthShader || !m_shadowDepthShader->IsValid()) {
        spdlog::warn("[Renderer] Directional shadow pass unavailable: depth shader failed to initialize");
        return;
    }

    const uint32_t shadowWidth  = light.shadow.shadowMapWidth  > 0 ? static_cast<uint32_t>(light.shadow.shadowMapWidth)  : 1u;
    const uint32_t shadowHeight = light.shadow.shadowMapHeight > 0 ? static_cast<uint32_t>(light.shadow.shadowMapHeight) : 1u;

    if (!m_directionalShadowMap ||
        m_directionalShadowMap->GetWidth() != shadowWidth ||
        m_directionalShadowMap->GetHeight() != shadowHeight) {
        m_directionalShadowMap = std::make_unique<ShadowMap>(shadowWidth, shadowHeight);
    }

    if (!m_directionalShadowMap || !m_directionalShadowMap->IsValid())
        return;

    const ShadowSceneBounds bounds = ComputeDirectionalShadowBounds(submission.objects);
    const glm::mat4 lightViewProj = LightUtils::DirectionalLightViewProjection(
        light, bounds.center, bounds.radius);

    SubmissionContext shadowContext{};
    shadowContext.depthTestEnabled = true;
    shadowContext.depthFunc        = DepthFunc::Less;
    shadowContext.depthWrite       = true;
    shadowContext.blendMode        = BlendMode::Disabled;
    shadowContext.cullMode         = CullMode::Back;
    shadowContext.Apply(m_currentContext);

    m_directionalShadowMap->Bind();
    glClear(GL_DEPTH_BUFFER_BIT);

    m_shadowDepthShader->Bind();
    m_shadowDepthShader->SetUniform("u_LightViewProj", lightViewProj);

    for (const RenderItem& item : submission.objects) {
        if (!CanRenderInDirectionalShadowPass(item))
            continue;

        m_shadowDepthShader->SetUniform("u_Model", BuildItemModelMatrix(item));

        if (item.meshMulti) {
            if (item.subMeshIndex >= item.meshMulti->SubMeshCount())
                continue;
            item.meshMulti->DrawSubMesh(item.subMeshIndex);
        } else if (item.mesh) {
            item.mesh->Draw(ToGLPrimitive(item.topology));
        }
    }

    m_directionalShadowMap->Unbind();
    ShaderProgram::Unbind();

    m_debugStats.shadowPassDataAvailable   = true;
    m_debugStats.shadowMapPreviewAvailable = true;
    m_debugStats.shadowMapTextureId        = m_directionalShadowMap->GetDepthTexture();
    m_debugStats.shadowMapWidth            = m_directionalShadowMap->GetWidth();
    m_debugStats.shadowMapHeight           = m_directionalShadowMap->GetHeight();
}
