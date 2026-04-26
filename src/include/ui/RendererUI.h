#pragma once
#include <cstddef>
#include <string>
#include <vector>

class Scene;
class Renderer;
class MouseInput;
struct RendererDebugStats;
struct AssetCacheStats;

/// Sidebar tab indices — keep in sync with kTabLabels in RendererUI.cpp.
enum class UITab : int {
    Scene  = 0,
    Lights = 1,
    Shadow = 2,
    Stats  = 3,
};

class RendererUI {
public:
    RendererUI() = default;

    /// Draw the full UI for the current frame (call between NewFrame / Render).
    void Draw(int fbWidth, int fbHeight,
              Scene& scene,
              Renderer& renderer,
              MouseInput* mouse,
              const std::vector<Scene*>& scenes,
              std::size_t& activeSceneIndex);

    /// Returns the viewport rectangle [x, y, w, h] in framebuffer pixels.
    /// Forward this to Renderer::Resize() / SetViewport() every frame.
    void GetViewportRect(int& x, int& y, int& w, int& h) const {
        x = m_vpX; y = m_vpY; w = m_vpW; h = m_vpH;
    }

    // ── Public toggles ────────────────────────────────────────────────────────
    bool wireframeOverride = false;
    bool showHelpWindow    = false;
    bool showSidebar       = true;

    static constexpr float kSidebarMinWidth = 320.0f;
    static constexpr float kSidebarMaxWidth = 520.0f;
    static constexpr float kHandleWidth     = 12.0f;

private:
    // ── Persistent layout state ───────────────────────────────────────────────
    float m_sidebarWidth = 280.0f;
    bool  m_dragging     = false;

    // Viewport rect in framebuffer pixels (updated every Draw call).
    int m_vpX = 0, m_vpY = 0, m_vpW = 0, m_vpH = 0;
    int m_prevVpW = -1,  m_prevVpH = -1;

    UITab m_activeTab = UITab::Scene;

    // ── Draw helpers ──────────────────────────────────────────────────────────
    void DrawTopbar(int fbWidth,
                    Scene& scene,
                    const RendererDebugStats& stats,
                    const std::vector<Scene*>& scenes,
                    std::size_t& activeSceneIndex,
                    bool lookMode);

    void DrawSidebar(int fbHeight, float topY,
                     Scene& scene,
                     const RendererDebugStats& stats,
                     bool lookMode);

    /// Invisible drag handle between sidebar and viewport.
    /// Updates m_sidebarWidth while the user drags.
    void DrawResizeHandle(int fbHeight, float topY);

    /// Viewport overlay: HUD pills, gizmo row, camera-mode badge.
    void DrawViewport(int fbWidth, int fbHeight, float topY,
                      const RendererDebugStats& stats,
                      Scene& scene,
                      bool lookMode);

    // ── Tab content ───────────────────────────────────────────────────────────
    static void DrawTabScene (Scene& scene, const RendererDebugStats& stats);
    void DrawTabLights(Scene& scene, const RendererDebugStats& stats);
    void DrawTabShadow(Scene& scene, const RendererDebugStats& stats);
    void DrawTabStats (Scene& scene, const RendererDebugStats& stats,
                       const AssetCacheStats& cacheStats);

    void DrawHelpWindow(int fbWidth, int fbHeight, bool lookMode);
};