#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>
#include "core/InitContext.h"
#include "core/SubmissionContext.h"
#include "core/RenderQueue.h"
#include "core/UniformBuffer.h"
#include "core/ShaderProgram.h"
#include "core/HdrFramebuffer.h"
#include "core/EnvironmentLightingPipeline.h"
#include "core/IBLDebugMode.h"
#include "core/shadows/CascadedShadowMap.h"
#include "core/RenderConfig.h"
#include "core/Frustum.h"

struct RenderItem;
struct FrameSubmission;
struct ReflectionProbe;
class Skybox;
class Camera;

enum class RendererPassTimingId : uint32_t
{
    DirectionalShadow = 0,
    MainScene,
    Skybox,
    PostProcess,
    Count
};

inline constexpr std::size_t kRendererPassTimingCount =
    static_cast<std::size_t>(RendererPassTimingId::Count);

struct RendererPassTiming
{
    const char *name = "";
    float milliseconds = 0.0f;
    bool available = false;
};

/// Small per-frame debug snapshot used by the runtime stats UI.
struct ShadowFrustumDebugInfo
{
    float focusCenterX = 0.0f;
    float focusCenterY = 0.0f;
    float focusCenterZ = 0.0f;
    float lightDirectionX = 0.0f;
    float lightDirectionY = 0.0f;
    float lightDirectionZ = 0.0f;
    float orthoRadius = 0.0f;
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    bool available = false;
};

struct RendererDebugStats
{
    uint32_t submittedRenderItemCount = 0;
    uint32_t visibleRenderItemCount = 0;
    uint32_t frustumCulledRenderItemCount = 0;
    uint32_t queuedRenderItemCount = 0;
    uint32_t processedRenderItemCount = 0;
    uint32_t drawCallCount = 0;
    uint32_t shaderProgramChangeCount = 0;
    uint32_t materialChangeCount = 0;
    uint32_t textureBindingCount = 0;
    uint64_t approxTriangleCount = 0;
    uint32_t directionalLightCount = 0;
    uint32_t pointLightCount = 0;
    uint32_t spotLightCount = 0;
    uint32_t totalLightCount = 0;
    uint32_t shadowCasterCount = 0;
    uint32_t shadowReceiverCount = 0;
    uint32_t shadowPassObjectCount = 0;
    uint32_t shadowPassExcludedObjectCount = 0;
    float frameTimeMs = 0.0f;
    float fps = 0.0f;
    std::array<RendererPassTiming, kRendererPassTimingCount> passTimings = {{
        {"Directional Shadow", 0.0f, false},
        {"Main Scene",         0.0f, false},
        {"Skybox",             0.0f, false},
        {"Post Process",       0.0f, false},
    }};
    uint32_t meshCount = 0;
    uint32_t materialCount = 0;
    uint32_t textureCount = 0;
    uint32_t shaderProgramCount = 0;
    uint32_t cachedMeshCount = 0;
    uint32_t cachedMaterialCount = 0;
    uint32_t cachedTexture2DCount = 0;
    uint32_t cachedCubemapCount = 0;
    uint32_t cachedShaderProgramCount = 0;
    uint32_t activeCameraCount = 0;
    float cameraPositionX = 0.0f;
    float cameraPositionY = 0.0f;
    float cameraPositionZ = 0.0f;
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;
    float cameraFov = 0.0f;
    bool cullingEnabled = false;
    bool cullingDataAvailable = false;
    bool currentCameraAvailable = false;
    uint32_t shadowMapTextureId = 0;
    uint32_t shadowMapWidth = 0;
    uint32_t shadowMapHeight = 0;
    uint32_t hdrColorTextureId = 0;
    uint32_t hdrWidth = 0;
    uint32_t hdrHeight = 0;
    uint32_t iblSourceTextureId = 0;
    uint32_t iblSourceWidth = 0;
    uint32_t iblSourceHeight = 0;
    uint32_t iblIrradianceTextureId = 0;
    uint32_t iblIrradianceWidth = 0;
    uint32_t iblIrradianceHeight = 0;
    uint32_t iblPrefilteredTextureId = 0;
    uint32_t iblPrefilteredWidth = 0;
    uint32_t iblPrefilteredHeight = 0;
    uint32_t iblBrdfLutTextureId = 0;
    uint32_t iblBrdfLutWidth = 0;
    uint32_t iblBrdfLutHeight = 0;
    uint32_t iblPrefilteredMipCount = 0;
    std::array<uint32_t, 6> iblSourcePreviewTextureIds{};
    std::array<uint32_t, 6> iblIrradiancePreviewTextureIds{};
    std::array<uint32_t, 6> iblPrefilteredPreviewTextureIds{};
    float iblIntensity = 0.0f;
    float iblDebugPrefilteredMip = 0.0f;
    IBLDebugMode iblDebugMode = kDefaultIBLDebugMode;
    bool iblAvailable = false;
    // Per-cascade 2D texture views into the depth array, suitable for ImGui.
    std::array<uint32_t, CascadedShadowMap::kNumCascades> cascadePreviewTextureIds{};
    // View-space distance covered by each cascade (positive, far edge).
    std::array<float,    CascadedShadowMap::kNumCascades> cascadeSplitDistances{};
    ShadowFrustumDebugInfo directionalShadowFrustum;

    // The count currently comes from accepted RenderItem flags in SubmitDraw().
    bool shadowCasterCountApproximate = true;
    bool shadowPassDataAvailable = false;
    bool approxTriangleCountApproximate = true; // Triangle estimates are pragmatic and treat non-triangle topologies as 0.
    bool shadowMapPreviewAvailable = false;
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
class Renderer
{
    InitContext m_initCtx;
    SubmissionContext m_currentContext;
    RenderQueue m_queue;
    std::unique_ptr<UniformBuffer> m_cameraUBO;
    std::unique_ptr<UniformBuffer> m_lightUBO;
    std::unique_ptr<ShaderProgram> m_errorShader;
    std::unique_ptr<ShaderProgram> m_shadowDepthShader;
    std::unique_ptr<CascadedShadowMap> m_directionalShadowMap;
    std::unique_ptr<HdrFramebuffer> m_hdrFramebuffer;
    std::unique_ptr<EnvironmentLightingPipeline> m_environmentLightingPipeline;
    const Skybox* m_currentSkybox = nullptr;
    const Camera* m_currentCamera = nullptr;
    std::shared_ptr<ReflectionProbe> m_defaultReflectionProbe;
    std::array<glm::mat4, CascadedShadowMap::kNumCascades> m_cascadeViewProj{};
    std::array<float, CascadedShadowMap::kNumCascades> m_cascadeSplits{};
    int m_shadowPcfRadius = 1;
    IBLDebugMode m_iblDebugMode = kDefaultIBLDebugMode;
    float m_iblDebugPrefilteredMip = 0.0f;
    float m_ambientFloorStrength = 0.18f;
    float m_maxShadowOcclusion = 0.75f;
    RenderConfig m_renderConfig{};
    Frustum m_cameraFrustum{};
    bool m_cameraFrustumValid = false;
    RendererDebugStats m_debugStats;
    bool m_reportedInvalidPackedLights = false;
    bool m_inFrame = false;

    void RenderDirectionalShadowPass(const FrameSubmission &submission);

public:
    ~Renderer();

    /// Loads GL function pointers, enables debug output, and applies the default pipeline state.
    /// Must be called after a valid GL context is current.
    bool Initialize();

    /// Logs shutdown.  The GL context is destroyed by the caller (Application).
    void Shutdown();

    /// Begins a frame: applies FrameClearInfo (viewport + clear), uploads the camera UBO,
    /// and applies the submission's SubmissionContext.
    void BeginFrame(const FrameSubmission &submission);

    /// Ends a frame: sorts the render queue by shader, flushes all draws, clears the queue.
    /// After this call the default framebuffer (0) is bound.
    void EndFrame();

    /// Rebinds the HDR offscreen framebuffer so that additional draws (e.g. instanced
    /// vegetation in Scene::OnPostRender) land in the same buffer that RenderPostProcess reads.
    /// No-op if no HDR framebuffer is active.
    void RebindHdrFramebuffer();

    /// Enqueues a draw item. Must be called between BeginFrame and EndFrame.
    void SubmitDraw(const RenderItem &item);

    /// Called by the GLFW framebuffer-resize callback. Viewport is driven each frame
    /// through FrameSubmission::clearInfo; this method only logs the new dimensions.
    void Resize(int width, int height);

    /// Returns the latest per-frame debug snapshot for runtime UI.
    [[nodiscard]] const RendererDebugStats &GetDebugStats() const { return m_debugStats; }

    /// Records CPU-side timings for render work owned outside Renderer::EndFrame.
    void RecordDebugPassTiming(RendererPassTimingId pass, float milliseconds);

    /// Selects the PBR IBL inspection mode used while flushing scene materials.
    void SetIBLDebugState(IBLDebugMode mode, float prefilteredMipLevel);

    /// Sets global lighting readability controls used by PBR shaders.
    void SetLightingDebugControls(float ambientFloorStrength, float maxShadowOcclusion);

    /// Sets runtime renderer options such as frustum culling.
    void SetRenderConfig(const RenderConfig& config);

    [[nodiscard]] const RenderConfig& GetRenderConfig() const { return m_renderConfig; }

    [[nodiscard]] IBLDebugMode GetIBLDebugMode() const { return m_iblDebugMode; }
    [[nodiscard]] float GetIBLDebugPrefilteredMipLevel() const { return m_iblDebugPrefilteredMip; }
    [[nodiscard]] float GetAmbientFloorStrength() const { return m_ambientFloorStrength; }
    [[nodiscard]] float GetMaxShadowOcclusion() const { return m_maxShadowOcclusion; }
};
