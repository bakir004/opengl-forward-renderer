#pragma once
#include <cstddef>
#include <vector>
#include "core/IBLDebugMode.h"

class Scene;
class Renderer;
class MouseInput;
struct RendererDebugStats;
struct AssetCacheStats;
struct FrameSubmission;

/// Sidebar tab indices — keep in sync with kTabLabels in RendererUI.cpp.
enum class UITab : int {
    Scene = 0,
    Lights = 1,
    Materials = 2,
    Shadow = 3,
    PostFX = 4,
    Stats = 5,
};

class RendererUI {
public:
    RendererUI() = default;

    /// Apply the global ImGui theme. Call once after ImGui::CreateContext(),
    /// or whenever you want to re-apply (e.g. after a style reset).
    /// Must NOT be called inside a NewFrame/Render block.
    static void ApplyTheme();

    /// Draw the full UI for the current frame (call between NewFrame / Render).
    void Draw(int fbWidth, int fbHeight,
              Scene &scene,
              Renderer &renderer,
              MouseInput *mouse,
              const std::vector<Scene *> &scenes,
              std::size_t &activeSceneIndex);

    /// Returns the viewport rectangle [x, y, w, h] in framebuffer pixels.
    /// Forward this to Renderer::Resize() / SetViewport() every frame.
    void GetViewportRect(int &x, int &y, int &w, int &h) const {
        x = m_vpX;
        y = m_vpY;
        w = m_vpW;
        h = m_vpH;
    }

    // Public toggles
    bool wireframeOverride = false;
    bool normalMapOverride = true;
    bool skyboxOverride = true;
    // After: bool skyboxOverride = true;
    bool  tonemapEnabled        = true;
    int   tonemapOperator       = 1;   // 0=Reinhard, 1=ACES, 2=Uncharted2
    float exposure              = 1.0f;
    bool  bloomEnabled          = true;
    float bloomStrength         = 1.0f;
    float bloomThreshold        = 1.0f;
    bool  bloomSoftThreshold    = true;
    float bloomSoftKnee         = 0.15f;
    float bloomRadius           = 1.0f;
    int   bloomBlurIterations   = 10;
    int   postFxDebugView       = 0;   // 0=Final, 1=HDR only, 2=Bright-pass, 3=Blurred bloom, 4=No bloom
    IBLDebugMode iblDebugMode = kDefaultIBLDebugMode;
    float iblDebugPrefilteredMip = 0.0f;
    bool showHelpWindow = false;
    bool showSidebar = true;

private:
    // Viewport rect in framebuffer pixels (updated every Draw call).
    int m_vpX = 0, m_vpY = 0, m_vpW = 0, m_vpH = 0;
    int m_prevVpW = -1, m_prevVpH = -1;

    UITab m_activeTab = UITab::Scene;

    // ── Draw helpers ──────────────────────────────────────────────────────────
    void DrawTopbar(int fbWidth,
                    Scene &scene,
                    const RendererDebugStats &stats,
                    const std::vector<Scene *> &scenes,
                    std::size_t &activeSceneIndex,
                    bool lookMode);

    // `frame` is the submission already built by Draw() — do not rebuild inside.
    void DrawSidebar(int fbHeight,
                     Scene &scene,
                     const RendererDebugStats &stats,
                     const FrameSubmission &frame,
                     bool lookMode);

    /// Viewport overlay: HUD pills, gizmo row, camera-mode badge.
    // `frame` is the submission already built by Draw() — do not rebuild inside.
    void DrawViewport(int fbWidth, int fbHeight,
                      const RendererDebugStats &stats,
                      Scene &scene,
                      const FrameSubmission &frame,
                      bool lookMode);

    // ── Tab content ───────────────────────────────────────────────────────────
    // All tabs receive the pre-built FrameSubmission so they never call
    // scene.BuildSubmission() themselves.
    void DrawTabScene(Scene &scene, const RendererDebugStats &stats,
                      const FrameSubmission &frame);

    void DrawTabLights(Scene &scene, const RendererDebugStats &stats,
                       const FrameSubmission &frame);

    void DrawTabMaterials(Scene &scene, const RendererDebugStats &stats,
                          const FrameSubmission &frame);

    void DrawTabShadow(Scene &scene, const RendererDebugStats &stats);

    void DrawTabPostFX(Scene &scene, const RendererDebugStats &stats);

    void DrawTabStats(Scene &scene, const RendererDebugStats &stats,
                      const AssetCacheStats &cacheStats);

    void DrawHelpWindow(int fbWidth, int fbHeight);
};
