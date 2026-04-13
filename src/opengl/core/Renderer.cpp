#include "core/Renderer.h"
#include "core/Camera.h"
#include "core/Mesh.h"
#include "core/MeshBuffer.h"
#include "core/shadows/CascadedShadowMap.h"
#include "scene/FrameSubmission.h"
#include "scene/LightBlock.h"
#include "scene/LightUtils.h"
#include "scene/RenderItem.h"
#include <glad/glad.h>
#include <algorithm>
#include <array>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <cassert>
#include <limits>

namespace
{

    constexpr float kMinShadowSceneRadius = 5.0f;
    // How far from the camera the cascades cover. Anything beyond receives no shadow.
    // Deliberately smaller than the world far plane so near-camera cascades stay tight.
    constexpr float kCascadeShadowMaxDistance = 150.0f;
    // Practical split lambda: 0 = uniform splits, 1 = logarithmic splits.
    // Values near 1.0 put most shadow-map resolution into near cascades.
    constexpr float kCascadeSplitLambda = 0.9f;
    // World-space distance the light-space near plane is pulled back so casters
    // between the light and the view slice (e.g. tree canopies above the camera)
    // still write into the depth map. Must be a fixed absolute value — a ratio
    // collapses to near-zero for tight near cascades.
    constexpr float kCascadeCasterPullback = 80.0f;
    // Small padding on the far side (away from the light) to cover numerical slop.
    constexpr float kCascadeFarPadding = 5.0f;

    GLenum ToGLPrimitive(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::Triangles:
            return GL_TRIANGLES;
        case PrimitiveTopology::Lines:
            return GL_LINES;
        case PrimitiveTopology::Points:
            return GL_POINTS;
        case PrimitiveTopology::TriangleStrip:
            return GL_TRIANGLE_STRIP;
        case PrimitiveTopology::LineStrip:
            return GL_LINE_STRIP;
        }
        return GL_TRIANGLES;
    }

    bool HasDrawableGeometry(const RenderItem &item)
    {
        return item.mesh != nullptr || item.meshMulti != nullptr;
    }

    bool CanRenderInDirectionalShadowPass(const RenderItem &item)
    {
        return item.flags.visible && item.flags.castShadow && HasDrawableGeometry(item);
    }

    glm::mat4 BuildItemModelMatrix(const RenderItem &item)
    {
        glm::mat4 model = item.transform.GetModelMatrix();

        const glm::vec3 &tOff = item.translationOffset;
        if (tOff != glm::vec3(0.0f))
            model = model * glm::translate(glm::mat4(1.0f), tOff);

        // Apply the mesh-local orientation fix as a single quaternion->matrix conversion.
        // One glm::mat4_cast replaces three separate glm::rotate matrix multiplications,
        // and the quaternion stores the intent (axis + angle) without any order ambiguity.
        static const glm::quat kIdentity(1.0f, 0.0f, 0.0f, 0.0f);
        if (item.rotationOffset != kIdentity)
            model = model * glm::mat4_cast(item.rotationOffset);

        return model;
    }

    struct ShadowSceneBounds
    {
        glm::vec3 center = glm::vec3(0.0f);
        float radius = kMinShadowSceneRadius;
    };

    ShadowSceneBounds ComputeDirectionalShadowBounds(const std::vector<RenderItem> &items)
    {
        glm::vec3 minPoint(std::numeric_limits<float>::max());
        glm::vec3 maxPoint(-std::numeric_limits<float>::max());
        bool hasAnyCaster = false;

        // Dev9 keeps this intentionally coarse for runtime inspection. Person 7/8 can
        // swap in tighter caster bounds or cascades once the production shadow path grows.
        for (const RenderItem &item : items)
        {
            if (!CanRenderInDirectionalShadowPass(item))
                continue;

            const glm::vec3 anchor = item.transform.GetTranslation() + item.translationOffset;
            const glm::vec3 scale = glm::abs(item.transform.GetScale());
            const float extent = std::max({0.5f, scale.x, scale.y, scale.z});
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

    void PopulateDebugStatsFromSubmission(const FrameSubmission &submission, RendererDebugStats &stats)
    {
        stats.submittedRenderItemCount = static_cast<uint32_t>(submission.objects.size());
        stats.queuedRenderItemCount = 0;
        stats.processedRenderItemCount = 0;
        stats.drawCallCount = 0;
        stats.approxTriangleCount = 0;
        stats.directionalLightCount = submission.lights.HasDirectionalLight() ? 1u : 0u;
        stats.pointLightCount = static_cast<uint32_t>(submission.lights.GetPointLights().size());
        stats.shadowCasterCount = 0;
        stats.shadowReceiverCount = 0;
        stats.shadowPassObjectCount = 0;
        stats.shadowPassExcludedObjectCount = 0;
        stats.frameTimeMs = submission.deltaTime * 1000.0f;
        stats.fps = submission.deltaTime > 0.0f ? (1.0f / submission.deltaTime) : 0.0f;
        stats.shadowCasterCountApproximate = true;
        stats.shadowPassDataAvailable = false;
        stats.approxTriangleCountApproximate = true;
        stats.shadowMapTextureId = 0;
        stats.shadowMapWidth = 0;
        stats.shadowMapHeight = 0;
        stats.shadowMapPreviewAvailable = false;
        stats.cascadePreviewTextureIds.fill(0);
        stats.cascadeSplitDistances.fill(0.0f);
        stats.directionalShadowFrustum = {};

        for (const RenderItem &item : submission.objects)
        {
            if (!item.flags.visible || !HasDrawableGeometry(item))
                continue;

            if (item.flags.receiveShadow)
                ++stats.shadowReceiverCount;
            if (!item.flags.castShadow)
                ++stats.shadowPassExcludedObjectCount;
        }
    }

} // namespace

bool Renderer::Initialize()
{
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

void Renderer::Shutdown()
{
    m_initCtx.Shutdown();
}

void Renderer::BeginFrame(const FrameSubmission &submission)
{
    assert(!m_inFrame && "BeginFrame() called without a matching EndFrame()");
    m_inFrame = true;
    PopulateDebugStatsFromSubmission(submission, m_debugStats);
    RenderDirectionalShadowPass(submission);

    if (!m_lightUBO)
        m_lightUBO = std::make_unique<UniformBuffer>(sizeof(LightBlock), GL_DYNAMIC_DRAW);

    LightBlock packedLights = LightBlock::Pack(submission.lights);
    const bool packedValid = LightUtils::ValidatePackedBlock(packedLights);
    if (!packedValid && !m_reportedInvalidPackedLights)
    {
        spdlog::warn("[Renderer] BeginFrame: packed light block has validation warnings");
        m_reportedInvalidPackedLights = true;
    }
    else if (packedValid && m_reportedInvalidPackedLights)
    {
        m_reportedInvalidPackedLights = false;
    }
    m_lightUBO->Upload(&packedLights, sizeof(LightBlock));
    m_lightUBO->BindToSlot(1);

    if (submission.camera)
    {
        if (!m_cameraUBO)
            m_cameraUBO = std::make_unique<UniformBuffer>(sizeof(CameraData), GL_DYNAMIC_DRAW);

        CameraData camData = submission.camera->BuildCameraData(submission.time, submission.deltaTime);
        m_cameraUBO->Upload(&camData, sizeof(CameraData));
        m_cameraUBO->BindToSlot(0);
    }

    submission.clearInfo.Apply();
    submission.context.Apply(m_currentContext);
}

void Renderer::EndFrame()
{
    assert(m_inFrame && "EndFrame() called without a matching BeginFrame()");

    // Pass cascade matrices + split distances + depth array + PCF radius to the queue.
    if (m_directionalShadowMap && m_directionalShadowMap->IsValid())
    {
        int pcfRadius = 1;
        if (m_debugStats.shadowPassDataAvailable)
            pcfRadius = m_shadowPcfRadius;
        m_queue.SetDirectionalShadowData(m_cascadeViewProj,
                                         m_cascadeSplits,
                                         m_directionalShadowMap->GetDepthTextureArray(),
                                         pcfRadius);
    }

    m_queue.Sort();
    const RenderQueueFrameStats queueStats = m_queue.Flush(m_currentContext);
    m_debugStats.processedRenderItemCount = queueStats.processedItemCount;
    m_debugStats.drawCallCount = queueStats.drawCallCount;
    m_debugStats.approxTriangleCount = queueStats.approxTriangleCount;
    m_queue.Clear();
    m_inFrame = false;
}

void Renderer::SubmitDraw(const RenderItem &item)
{
    assert(m_inFrame && "SubmitDraw() must be called between BeginFrame() and EndFrame()");
    if (m_queue.Add(item))
    {
        ++m_debugStats.queuedRenderItemCount;
        if (item.flags.castShadow)
            ++m_debugStats.shadowCasterCount;
    }
}

void Renderer::Resize(int width, int height)
{
    spdlog::debug("[Renderer] Resize: {}x{} (viewport applied via FrameSubmission each frame)",
                  width, height);
}

namespace
{
    // Computes view-space cascade split distances using the practical scheme:
    //   blend of uniform and logarithmic splits controlled by `lambda`.
    // Splits are stored as positive view-space distances from the camera.
    std::array<float, CascadedShadowMap::kNumCascades>
    ComputeCascadeSplits(float nearClip, float farClip, float lambda)
    {
        std::array<float, CascadedShadowMap::kNumCascades> splits{};
        const float range = farClip - nearClip;
        const float ratio = farClip / std::max(nearClip, 0.0001f);
        for (uint32_t i = 0; i < CascadedShadowMap::kNumCascades; ++i) {
            const float p = static_cast<float>(i + 1) / static_cast<float>(CascadedShadowMap::kNumCascades);
            const float logSplit = nearClip * std::pow(ratio, p);
            const float uniSplit = nearClip + range * p;
            splits[i] = lambda * logSplit + (1.0f - lambda) * uniSplit;
        }
        return splits;
    }

    // Builds a *stable* directional-light view-projection matrix for one cascade.
    //
    // Stabilization strategy (standard CSM technique):
    //  1. Compute 8 world-space corners of the view-frustum slice.
    //  2. Fit a bounding *sphere* to the 8 corners. Sphere center and radius
    //     are rotation-invariant, so they don't change when the camera rotates.
    //  3. Use the sphere radius as half the ortho side — width = height always,
    //     so the projected image fits the square shadow-map texture without
    //     stretching and the ortho size is camera-orientation-independent.
    //  4. Snap the sphere center to whole-texel increments in light space so
    //     shadow edges stay pinned to texels instead of crawling as the camera
    //     translates.
    glm::mat4 BuildCascadeViewProjection(const Camera& camera,
                                         const glm::vec3& lightDirection,
                                         float sliceNear,
                                         float sliceFar,
                                         uint32_t shadowMapSize)
    {
        const glm::mat4 sliceProj = glm::perspective(
            glm::radians(camera.GetFOV()),
            camera.GetAspect(),
            sliceNear,
            sliceFar);
        const glm::mat4 invViewProj = glm::inverse(sliceProj * camera.GetView());

        std::array<glm::vec3, 8> corners{};
        int idx = 0;
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                for (int z = 0; z < 2; ++z) {
                    const glm::vec4 ndc(
                        2.0f * static_cast<float>(x) - 1.0f,
                        2.0f * static_cast<float>(y) - 1.0f,
                        2.0f * static_cast<float>(z) - 1.0f,
                        1.0f);
                    const glm::vec4 world = invViewProj * ndc;
                    corners[idx++] = glm::vec3(world) / world.w;
                }
            }
        }

        // Bounding sphere: centroid + max distance from centroid.
        glm::vec3 centroid(0.0f);
        for (const glm::vec3& c : corners) centroid += c;
        centroid /= 8.0f;

        float radius = 0.0f;
        for (const glm::vec3& c : corners)
            radius = std::max(radius, glm::length(c - centroid));
        // Round up slightly so the sphere size is quantized and doesn't breathe
        // with sub-pixel camera translation.
        radius = std::ceil(radius * 16.0f) / 16.0f;

        const glm::vec3 up = (std::abs(glm::dot(lightDirection, glm::vec3(0, 1, 0))) < 0.99f)
                                 ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        const glm::mat4 lightView = glm::lookAt(centroid - lightDirection * radius,
                                                centroid,
                                                up);

        // Snap the light-space centroid to whole-texel increments. The ortho
        // covers [-radius, +radius], so one texel is (2*radius)/shadowMapSize
        // world units wide.
        const float texelSize = (2.0f * radius) / static_cast<float>(shadowMapSize);
        glm::vec3 centroidLightSpace = glm::vec3(lightView * glm::vec4(centroid, 1.0f));
        centroidLightSpace.x = std::floor(centroidLightSpace.x / texelSize) * texelSize;
        centroidLightSpace.y = std::floor(centroidLightSpace.y / texelSize) * texelSize;
        // Rebuild the view matrix so its origin lands on the snapped centroid.
        const glm::vec3 snappedCentroidWorld = glm::vec3(glm::inverse(lightView) * glm::vec4(centroidLightSpace, 1.0f));
        const glm::mat4 stableLightView = glm::lookAt(snappedCentroidWorld - lightDirection * radius,
                                                      snappedCentroidWorld,
                                                      up);

        // Square, camera-rotation-invariant ortho. Z extends on the light-facing
        // side to catch casters above the view slice (kCascadeCasterPullback).
        const float nearZ = -(radius + kCascadeCasterPullback);
        const float farZ  = +(radius + kCascadeFarPadding);
        const glm::mat4 lightProj = glm::ortho(
            -radius, +radius,
            -radius, +radius,
            nearZ, farZ);

        return lightProj * stableLightView;
    }
} // namespace

void Renderer::RenderDirectionalShadowPass(const FrameSubmission &submission)
{
    m_debugStats.directionalShadowFrustum = {};

    if (!submission.lights.HasDirectionalLight() || !submission.camera)
        return;

    const DirectionalLight &light = submission.lights.GetDirectionalLight();
    if (!light.enabled || !light.shadow.castShadow)
        return;

    glm::vec3 lightDirection = light.direction;
    if (glm::length(lightDirection) <= 0.0001f)
        lightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    else
        lightDirection = glm::normalize(lightDirection);

    if (!m_shadowDepthShader || !m_shadowDepthShader->IsValid())
    {
        spdlog::warn("[Renderer] Directional shadow pass unavailable: depth shader failed to initialize");
        return;
    }

    const uint32_t shadowWidth = light.shadow.shadowMapWidth > 0 ? static_cast<uint32_t>(light.shadow.shadowMapWidth) : 1u;
    const uint32_t shadowHeight = light.shadow.shadowMapHeight > 0 ? static_cast<uint32_t>(light.shadow.shadowMapHeight) : 1u;

    if (!m_directionalShadowMap ||
        m_directionalShadowMap->GetWidth() != shadowWidth ||
        m_directionalShadowMap->GetHeight() != shadowHeight)
    {
        m_directionalShadowMap = std::make_unique<CascadedShadowMap>(shadowWidth, shadowHeight);
    }

    if (!m_directionalShadowMap || !m_directionalShadowMap->IsValid())
        return;

    // Cascade splits are expressed as positive view-space distances from the
    // camera. The first cascade starts at camera near; the last ends at
    // min(camera.far, kCascadeShadowMaxDistance).
    const float camNear = submission.camera->GetNear();
    const float camFar  = std::min(submission.camera->GetFar(), kCascadeShadowMaxDistance);
    m_cascadeSplits = ComputeCascadeSplits(camNear, camFar, kCascadeSplitLambda);
    m_shadowPcfRadius = std::max(0, light.shadow.pcfRadius);

    // Populate debug frustum with broad scene bounds (still useful for UI).
    const ShadowSceneBounds bounds = ComputeDirectionalShadowBounds(submission.objects);
    m_debugStats.directionalShadowFrustum.focusCenterX = bounds.center.x;
    m_debugStats.directionalShadowFrustum.focusCenterY = bounds.center.y;
    m_debugStats.directionalShadowFrustum.focusCenterZ = bounds.center.z;
    m_debugStats.directionalShadowFrustum.lightDirectionX = lightDirection.x;
    m_debugStats.directionalShadowFrustum.lightDirectionY = lightDirection.y;
    m_debugStats.directionalShadowFrustum.lightDirectionZ = lightDirection.z;
    m_debugStats.directionalShadowFrustum.orthoRadius = bounds.radius;
    m_debugStats.directionalShadowFrustum.nearPlane = camNear;
    m_debugStats.directionalShadowFrustum.farPlane = camFar;
    m_debugStats.directionalShadowFrustum.available = true;

    SubmissionContext shadowContext{};
    shadowContext.depthTestEnabled = true;
    shadowContext.depthFunc = DepthFunc::Less;
    shadowContext.depthWrite = true;
    shadowContext.blendMode = BlendMode::Disabled;
    shadowContext.cullMode = CullMode::Back;
    shadowContext.Apply(m_currentContext);

    m_shadowDepthShader->Bind();

    // Polygon offset pushes depth written into the shadow map slightly away
    // from the light, eliminating self-shadow acne on lit surfaces without
    // the heavy cost of a large constant bias.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    uint32_t shadowPassObjectCount = 0;
    for (uint32_t cascade = 0; cascade < CascadedShadowMap::kNumCascades; ++cascade)
    {
        const float sliceNear = (cascade == 0) ? camNear : m_cascadeSplits[cascade - 1];
        const float sliceFar  = m_cascadeSplits[cascade];

        const glm::mat4 cascadeVP = BuildCascadeViewProjection(
            *submission.camera, lightDirection, sliceNear, sliceFar, shadowWidth);
        m_cascadeViewProj[cascade] = cascadeVP;

        m_directionalShadowMap->BindLayer(cascade);
        glClear(GL_DEPTH_BUFFER_BIT);

        m_shadowDepthShader->SetUniform("u_LightViewProj", cascadeVP);

        for (const RenderItem &item : submission.objects)
        {
            if (!CanRenderInDirectionalShadowPass(item))
                continue;

            m_shadowDepthShader->SetUniform("u_Model", BuildItemModelMatrix(item));

            if (item.meshMulti)
            {
                if (item.subMeshIndex >= item.meshMulti->SubMeshCount())
                    continue;
                item.meshMulti->DrawSubMesh(item.subMeshIndex);
                ++shadowPassObjectCount;
            }
            else if (item.mesh)
            {
                item.mesh->Draw(ToGLPrimitive(item.topology));
                ++shadowPassObjectCount;
            }
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);

    m_directionalShadowMap->Unbind();
    ShaderProgram::Unbind();

    m_debugStats.shadowPassDataAvailable = true;
    // Per-cascade 2D texture views (aliased via glTextureView) make the depth
    // array's individual layers previewable in ImGui::Image.
    m_debugStats.shadowMapPreviewAvailable = true;
    m_debugStats.shadowPassObjectCount = shadowPassObjectCount;
    m_debugStats.shadowMapTextureId = m_directionalShadowMap->GetLayerPreviewTexture(0);
    m_debugStats.shadowMapWidth = m_directionalShadowMap->GetWidth();
    m_debugStats.shadowMapHeight = m_directionalShadowMap->GetHeight();
    for (uint32_t i = 0; i < CascadedShadowMap::kNumCascades; ++i) {
        m_debugStats.cascadePreviewTextureIds[i] = m_directionalShadowMap->GetLayerPreviewTexture(i);
        m_debugStats.cascadeSplitDistances[i] = m_cascadeSplits[i];
    }
}
