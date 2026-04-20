// RendererUI.cpp
// Non-overlapping topbar / sidebar / viewport layout with a draggable
// resize handle between sidebar and viewport.

#include "ui/RendererUI.h"

#include "core/Camera.h"
#include "core/Renderer.h"
#include "core/MouseInput.h"
#include "scene/Scene.h"
#include "scene/LightEnvironment.h"
#include "scene/FrameSubmission.h"
#include "assets/AssetImporter.h"

#include <imgui.h>
#include <imgui_internal.h>     // ImGui::GetCurrentWindow, SetNextWindowBgAlpha, ItemAdd
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cstdint>
#include <cmath>
#include <string>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Design tokens  (match the HTML mock-up colours)
// ─────────────────────────────────────────────────────────────────────────────
namespace Pal {
    static constexpr ImVec4 Bg0        = {0.051f, 0.051f, 0.059f, 1.00f}; // #0d0d0f  viewport bg
    static constexpr ImVec4 Bg1        = {0.078f, 0.078f, 0.086f, 1.00f}; // #141416  sidebar bg
    static constexpr ImVec4 Bg2        = {0.098f, 0.098f, 0.110f, 1.00f}; // #191919
    static constexpr ImVec4 Bg3        = {0.102f, 0.102f, 0.118f, 1.00f}; // #1a1a1e  topbar / section headers
    static constexpr ImVec4 Border     = {0.165f, 0.165f, 0.165f, 1.00f}; // #2a2a2a
    static constexpr ImVec4 Accent     = {0.486f, 0.620f, 0.961f, 1.00f}; // #7c9ef5
    static constexpr ImVec4 AccentDim  = {0.228f, 0.314f, 0.502f, 1.00f}; // #3a5080
    static constexpr ImVec4 Green      = {0.306f, 0.788f, 0.306f, 1.00f}; // #4ec94e
    static constexpr ImVec4 Orange     = {0.941f, 0.627f, 0.188f, 1.00f}; // #f0a030
    static constexpr ImVec4 Red        = {0.753f, 0.439f, 0.439f, 1.00f};
    static constexpr ImVec4 RedBg      = {0.200f, 0.063f, 0.063f, 1.00f};
    static constexpr ImVec4 TextHi     = {0.900f, 0.900f, 0.900f, 1.00f};
    static constexpr ImVec4 TextMid    = {0.800f, 0.800f, 0.800f, 1.00f};
    static constexpr ImVec4 TextDim    = {0.400f, 0.400f, 0.400f, 1.00f};
    static constexpr ImVec4 TextFaint  = {0.267f, 0.267f, 0.267f, 1.00f};
    static constexpr ImVec4 HandleIdle = {0.180f, 0.180f, 0.220f, 1.00f}; // drag handle colour
    static constexpr ImVec4 HandleHot  = {0.486f, 0.620f, 0.961f, 0.60f}; // accent when hovered/dragged
}

static constexpr float kTopbarHeight = 50.0f;
static constexpr int   kTabCount     = 4;
static const char*     kTabLabels[]  = { "Scene", "Lights", "Shadow", "Stats" };

// ─────────────────────────────────────────────────────────────────────────────
// Small widget helpers
// ─────────────────────────────────────────────────────────────────────────────
static std::string FormatCompact(uint64_t v) {
    if (v >= 1'000'000ull)
        return std::to_string(v / 1'000'000ull) + "." +
               std::to_string((v % 1'000'000ull) / 100'000ull) + "M";
    if (v >= 1'000ull)
        return std::to_string(v / 1'000ull) + "." +
               std::to_string((v % 1'000ull) / 100ull) + "k";
    return std::to_string(v);
}

static void ColorDot(ImVec4 col) {
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted("●");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);
}

static bool MiniBadgeButton(const char* label, bool danger = false, bool add = false) {
    if (danger) {
        ImGui::PushStyleColor(ImGuiCol_Button,        Pal::RedBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f,0.08f,0.08f,1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.15f,0.04f,0.04f,1.f});
        ImGui::PushStyleColor(ImGuiCol_Text,          Pal::Red);
    } else if (add) {
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.05f,0.18f,0.10f,1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.08f,0.26f,0.14f,1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.04f,0.14f,0.08f,1.f});
        ImGui::PushStyleColor(ImGuiCol_Text,          {0.30f,0.62f,0.43f,1.f});
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        Pal::Bg3);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.15f,0.15f,0.19f,1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Pal::Bg2);
        ImGui::PushStyleColor(ImGuiCol_Text,          Pal::TextDim);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(6, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    const bool clicked = ImGui::Button(label);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

static bool SectionHeader(const char* label, bool defaultOpen = true) {
    ImGui::PushStyleColor(ImGuiCol_Header,        Pal::Bg3);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.13f,0.13f,0.17f,1.f});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  Pal::Bg2);
    ImGui::PushStyleColor(ImGuiCol_Text,          Pal::TextDim);
    ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (defaultOpen) f |= ImGuiTreeNodeFlags_DefaultOpen;
    const bool open = ImGui::CollapsingHeader(label, f);
    ImGui::PopStyleColor(4);
    return open;
}

// ─────────────────────────────────────────────────────────────────────────────
// Light editors  (unchanged logic, restyled)
// ─────────────────────────────────────────────────────────────────────────────
static void DrawDirectionalLight(DirectionalLight& light) {
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
    ImGui::DragFloat("Intensity##dir", &light.intensity, 0.05f, 0.0f, 1000.0f, "%.3f");
    ImGui::ColorEdit3("Color##dir",    &light.color.x);

    const glm::vec3& d = light.direction;
    float elev = glm::degrees(std::asin(glm::clamp(d.y, -1.0f, 1.0f)));
    float azim = glm::degrees(std::atan2(d.z, d.x));
    bool changed = false;
    changed |= ImGui::DragFloat("Azimuth (°)##dir",   &azim, 0.5f, -180.f, 180.f, "%.1f");
    changed |= ImGui::DragFloat("Elevation (°)##dir", &elev, 0.5f,  -90.f,  90.f, "%.1f");
    if (changed) {
        const float pr = glm::radians(elev), yr = glm::radians(azim);
        light.direction = { std::cos(pr)*std::cos(yr), std::sin(pr), std::cos(pr)*std::sin(yr) };
    }
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
    ImGui::Text("XYZ: %.3f, %.3f, %.3f", d.x, d.y, d.z);
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
}

// Returns true → remove this light.
static bool DrawPointLight(PointLight& light, int idx, const glm::vec3& camPos) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Pal::Bg3);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

    const std::string cid = "plc" + std::to_string(idx);
    ImGui::BeginChild(cid.c_str(), ImVec2(0,0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);

    // Header row
    ColorDot({0.50f, 0.75f, 1.0f, 1.0f});
    const std::string name = light.name.empty() ? ("Light " + std::to_string(idx+1)) : light.name;
    ImGui::TextUnformatted(name.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
    const bool remove = MiniBadgeButton(("X##rm" + std::to_string(idx)).c_str(), true);

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
    const std::string p = "##pl" + std::to_string(idx);
    ImGui::DragFloat(("Intensity" + p).c_str(), &light.intensity, 0.05f, 0.f, 1000.f, "%.3f");
    ImGui::ColorEdit3(("Color"    + p).c_str(), &light.color.x);
    ImGui::DragFloat3(("Position" + p).c_str(), &light.position.x, 0.05f);
    ImGui::DragFloat(("Radius"   + p).c_str(), &light.radius, 0.5f, 0.1f, 500.f, "%.1f");
    if (MiniBadgeButton(("Move to camera" + p).c_str()))
        light.position = camPos;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
    ImGui::Text("(%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::Spacing();
    return remove;
}

static void DrawShadowParams(LightShadowParams& s, const char* farLabel,
                             const char* note, bool dirNote) {
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
    ImGui::Checkbox("Cast shadow",  &s.castShadow);
    ImGui::DragFloat("Depth bias",  &s.depthBias,  0.0001f, 0.f,  1.f,      "%.5f");
    ImGui::DragFloat("Normal bias", &s.normalBias, 0.0005f, 0.f, 10.f,      "%.4f");
    ImGui::DragFloat("Slope bias",  &s.slopeBias,  0.050f,  0.f, 10.f,      "%.3f");
    int res[2] = { s.shadowMapWidth, s.shadowMapHeight };
    if (ImGui::DragInt2("Resolution", res, 4.f, 16, 8192)) {
        s.shadowMapWidth  = std::max(16, res[0]);
        s.shadowMapHeight = std::max(16, res[1]);
    }
    ImGui::DragFloat("Near plane", &s.nearPlane, 0.01f, 0.001f,  1000.f, "%.3f");
    ImGui::DragFloat(farLabel,     &s.farPlane,  0.05f, 0.01f,  10000.f, "%.3f");
    if (s.nearPlane < 0.001f)        s.nearPlane = 0.001f;
    if (s.farPlane <= s.nearPlane)   s.farPlane  = s.nearPlane + 0.01f;
    ImGui::SliderInt("PCF radius", &s.pcfRadius, 0, 4);
    if (s.pcfRadius < 0) s.pcfRadius = 0;
    const int k = s.pcfRadius * 2 + 1;
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
    ImGui::Text("PCF kernel: %dx%d (%d taps)", k, k, k*k);
    ImGui::TextWrapped("%s", note);
    if (dirNote) ImGui::TextWrapped("Directional cast/bias packed in light UBO.");
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level Draw — computes the layout and calls sub-draw methods
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::Draw(int fbW, int fbH,
                      Scene& scene, Renderer& renderer, MouseInput* mouse,
                      const std::vector<Scene*>& scenes,
                      std::size_t& activeSceneIndex)
{
    const bool lookMode = mouse && mouse->IsCaptured();
    const RendererDebugStats& stats = renderer.GetDebugStats();

    // ── Compute & cache viewport rect ─────────────────────────────────────────
    // Layout: [sidebar | handle | viewport], all below the topbar.
    const float clampedSW = std::clamp(m_sidebarWidth,
                                       kSidebarMinWidth, kSidebarMaxWidth);
    m_sidebarWidth = clampedSW;

    m_vpX = static_cast<int>(clampedSW + kHandleWidth);
    m_vpY = static_cast<int>(kTopbarHeight);
    m_vpW = std::max(1, fbW - m_vpX);
    m_vpH = std::max(1, fbH - m_vpY);

    // Notify the renderer of the new dimensions so it can update the projection
    // matrix aspect ratio. The actual glViewport call with the correct X offset
    // (to exclude the sidebar) must be set in Renderer::BeginFrame via the
    // sub.clearInfo.viewport rect {vpX, 0, vpW, vpH} passed by Application.
    // Do NOT call glViewport(0,0,vpW,vpH) inside Resize — that strips the offset
    // and is the root cause of content stretching when the sidebar is resized.
    renderer.Resize(m_vpW, m_vpH);

    // ── Draw each region ──────────────────────────────────────────────────────
    DrawTopbar(fbW, scene, stats, scenes, activeSceneIndex, lookMode);
    DrawSidebar(fbH, kTopbarHeight, scene, stats, lookMode);
    DrawResizeHandle(fbH, kTopbarHeight);
    DrawViewport(fbW, fbH, kTopbarHeight, stats, scene, lookMode);

    if (showHelpWindow)
        DrawHelpWindow(fbW, fbH, lookMode);
}

// ─────────────────────────────────────────────────────────────────────────────
// Topbar  (full width, fixed height)
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTopbar(int fbW,
                            Scene& scene,
                            const RendererDebugStats& stats,
                            const std::vector<Scene*>& scenes,
                            std::size_t& activeSceneIndex,
                            bool lookMode)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove       |
                             ImGuiWindowFlags_NoScrollbar  |
                             ImGuiWindowFlags_NoSavedSettings;
    if (lookMode) flags |= ImGuiWindowFlags_NoMouseInputs;

    ImGui::SetNextWindowPos ({0.f, 0.f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({static_cast<float>(fbW), kTopbarHeight}, ImGuiCond_Always);

    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar  (ImGuiStyleVar_ItemSpacing,   ImVec2(0, 0));
    ImGui::PushStyleVar  (ImGuiStyleVar_FramePadding,  ImVec2(12, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,           Pal::Bg3);
    ImGui::PushStyleColor(ImGuiCol_Border,             Pal::Border);

    if (ImGui::Begin("##TB", nullptr, flags)) {

        // Logo
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
        ImGui::SetCursorPos({12.f, (kTopbarHeight - ImGui::GetTextLineHeight()) * 0.5f});
        ImGui::TextUnformatted("RENDERER");
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 10);
        ImGui::SetCursorPosY(0);
        ImGui::PushStyleColor(ImGuiCol_Separator, Pal::Border);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 10);

        // ── Dropdown menu style helpers ───────────────────────────────────────
        auto BeginMenuButton = [&](const char* label, bool isActive) -> bool {
            ImGui::PushStyleColor(ImGuiCol_Button,        isActive ? Pal::AccentDim : ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.16f,0.16f,0.19f,1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Pal::AccentDim);
            ImGui::PushStyleColor(ImGuiCol_Text,          isActive ? Pal::Accent : Pal::TextMid);
            ImGui::PushStyleVar  (ImGuiStyleVar_FrameRounding, 3.f);
            ImGui::SetCursorPosY((kTopbarHeight - ImGui::GetFrameHeight()) * 0.5f);
            const bool clicked = ImGui::Button(label);
            ImGui::PopStyleVar(); ImGui::PopStyleColor(4);
            return clicked;
        };

        ImGui::PushStyleColor(ImGuiCol_PopupBg,      Pal::Bg2);
        ImGui::PushStyleColor(ImGuiCol_Border,        Pal::Border);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.16f,0.16f,0.22f,1.f});
        ImGui::PushStyleColor(ImGuiCol_Header,        Pal::AccentDim);
        ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding,  ImVec2(8, 6));
        ImGui::PushStyleVar  (ImGuiStyleVar_ItemSpacing,    ImVec2(8, 6));
        ImGui::PushStyleVar  (ImGuiStyleVar_PopupRounding,  4.f);

        // ── "Scenes" dropdown ─────────────────────────────────────────────────
        BeginMenuButton("Scenes", false);
        if (ImGui::BeginPopupContextItem("##scenesPopup",
                ImGuiPopupFlags_MouseButtonLeft | ImGuiPopupFlags_NoOpenOverExistingPopup))
        {
            const std::size_t maxS = std::min<std::size_t>(9, scenes.size());
            for (std::size_t i = 0; i < maxS; ++i) {
                if (!scenes[i]) continue;
                const bool sel = (i == activeSceneIndex);
                const std::string& sn = scenes[i]->GetName();
                const std::string  lb = sn.empty() ? ("Scene " + std::to_string(i+1)) : sn;
                ImGui::PushStyleColor(ImGuiCol_Text, sel ? Pal::Accent : Pal::TextMid);
                if (ImGui::MenuItem(lb.c_str(), nullptr, sel))
                    activeSceneIndex = i;
                ImGui::PopStyleColor();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine(0, 2);

        // ── "View" dropdown ───────────────────────────────────────────────────
        BeginMenuButton("View", wireframeOverride);
        if (ImGui::BeginPopupContextItem("##viewPopup",
                ImGuiPopupFlags_MouseButtonLeft | ImGuiPopupFlags_NoOpenOverExistingPopup))
        {
            // Wireframe
            ImGui::PushStyleColor(ImGuiCol_Text, !wireframeOverride ? Pal::Accent : Pal::TextMid);
            if (ImGui::MenuItem("Wireframe", nullptr, wireframeOverride))
                wireframeOverride = !wireframeOverride;
            ImGui::PopStyleColor();

            // Solid (unavailable)
            ImGui::BeginDisabled();
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::MenuItem("Solid", nullptr, false);
            ImGui::PopStyleColor();
            ImGui::EndDisabled();

            // Lights and Shadows (current / default)
            ImGui::PushStyleColor(ImGuiCol_Text, !wireframeOverride ? Pal::Accent : Pal::TextMid);
            if (ImGui::MenuItem("Lights and Shadows", nullptr, !wireframeOverride))
                wireframeOverride = false;
            ImGui::PopStyleColor();

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar  (3);
        ImGui::PopStyleColor(4);

        {
            char fps[32], ms[32], tris[32];
            snprintf(fps,  sizeof(fps),  "%.1f fps", stats.fps);
            snprintf(ms,   sizeof(ms),   "%.2f ms",  stats.frameTimeMs);
            snprintf(tris, sizeof(tris), "%s tris",  FormatCompact(stats.approxTriangleCount).c_str());

            const float gap = 10.f;
            const float pad = 10.f;

            ImGui::PushFont(ImGui::GetFont()); // keep consistent if you use custom fonts

            ImVec2 fpsSz  = ImGui::CalcTextSize(fps);
            ImVec2 msSz   = ImGui::CalcTextSize(ms);
            ImVec2 trisSz = ImGui::CalcTextSize(tris);

            float totalW =
                fpsSz.x +
                msSz.x +
                trisSz.x +
                gap * 2.f;

            float x = (float)m_vpX + (float)m_vpW - totalW - pad;
            float y = (float)m_vpY + pad;

            ImDrawList* dl = ImGui::GetForegroundDrawList();

            float cursorX = x;
            float baselineY = y;

            // FPS (green)
            dl->AddText(ImVec2(cursorX, baselineY), ImColor(Pal::Green), fps);
            cursorX += fpsSz.x + gap;

            // MS
            dl->AddText(ImVec2(cursorX, baselineY), ImColor(Pal::TextDim), ms);
            cursorX += msSz.x + gap;

            // TRIS
            dl->AddText(ImVec2(cursorX, baselineY), ImColor(Pal::TextDim), tris);

            ImGui::PopFont();
        }

    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawSidebar(int fbH, float topY,
                             Scene& scene,
                             const RendererDebugStats& stats,
                             bool lookMode)
{
    const float sbH = static_cast<float>(fbH) - topY;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove       |
                             ImGuiWindowFlags_NoSavedSettings;
    if (lookMode) flags |= ImGuiWindowFlags_NoMouseInputs;

    ImGui::SetNextWindowPos ({0.f, topY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({m_sidebarWidth, sbH}, ImGuiCond_Always);

    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar  (ImGuiStyleVar_ItemSpacing,   ImVec2(8, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,           Pal::Bg1);
    ImGui::PushStyleColor(ImGuiCol_Border,             Pal::Border);

    if (ImGui::Begin("##SB", nullptr, flags)) {

        // Tab bar
        ImGui::PushStyleVar  (ImGuiStyleVar_ItemSpacing,  ImVec2(2, 0));
        ImGui::PushStyleVar  (ImGuiStyleVar_FramePadding, ImVec2(14, 8));
        ImGui::PushStyleColor(ImGuiCol_TabActive,          Pal::Bg3);
        ImGui::PushStyleColor(ImGuiCol_Tab,               Pal::Bg1);
        ImGui::PushStyleColor(ImGuiCol_TabHovered,        Pal::Bg3);
        ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, Pal::Bg3);

        if (ImGui::BeginTabBar("##tabs")) {
            for (int i = 0; i < kTabCount; ++i) {
                const bool isActive = (static_cast<int>(m_activeTab) == i);
                ImGui::PushStyleColor(ImGuiCol_Text, isActive ? Pal::Accent : Pal::TextDim);
                if (ImGui::BeginTabItem(kTabLabels[i])) {
                    m_activeTab = static_cast<UITab>(i);
                    ImGui::EndTabItem();
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndTabBar();
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);

        ImGui::Separator();

        // Scrollable content area
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
        ImGui::BeginChild("##panel", ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 5));

        const AssetCacheStats cs = AssetImporter::GetCacheStats();
        switch (m_activeTab) {
            case UITab::Scene:  DrawTabScene (scene, stats);     break;
            case UITab::Lights: DrawTabLights(scene, stats);     break;
            case UITab::Shadow: DrawTabShadow(scene, stats);     break;
            case UITab::Stats:  DrawTabStats (scene, stats, cs); break;
        }

        if (lookMode) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::Separator();
            ImGui::TextUnformatted("TAB — release mouse for UI");
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Resize handle  — a thin draggable strip between sidebar and viewport
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawResizeHandle(int fbH, float topY)
{
    const float handleH = static_cast<float>(fbH) - topY;
    const float handleX = m_sidebarWidth;

    // Invisible, always-on-top window that contains only the handle logic.
    ImGui::SetNextWindowPos ({handleX, topY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({kHandleWidth, handleH}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);

    constexpr ImGuiWindowFlags kHandleFlags =
        ImGuiWindowFlags_NoDecoration   |
        ImGuiWindowFlags_NoMove         |
        ImGuiWindowFlags_NoScrollbar    |
        ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoNav          |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,  ImVec2(1, 1));
    ImGui::PushStyleColor(ImGuiCol_Border,            ImVec4(0, 0, 0, 0));

    if (ImGui::Begin("##Handle", nullptr, kHandleFlags)) {
        // Invisible button that covers the whole handle strip.
        ImGui::SetCursorPos({0, 0});
        ImGui::InvisibleButton("##hbtn", ImVec2(kHandleWidth, handleH),
                               ImGuiButtonFlags_MouseButtonLeft);

        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemActive()) m_dragging = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_dragging = false;

        if (m_dragging) {
            m_sidebarWidth = ImGui::GetMousePos().x;
            m_sidebarWidth = std::clamp(m_sidebarWidth, kSidebarMinWidth, kSidebarMaxWidth);
        }

        // Visual: draw a thin line with accent colour when hot.
        const ImVec4 lineCol = (hovered || m_dragging) ? Pal::HandleHot : Pal::HandleIdle;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float cx = handleX + kHandleWidth * 0.5f;
        dl->AddLine({cx, topY}, {cx, topY + handleH},
                    ImGui::ColorConvertFloat4ToU32(lineCol), 2.0f);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Change cursor to resize arrow while hovering or dragging.
    // (Works on platforms where GLFW/SDL expose cursor shapes to ImGui.)
    if (m_dragging)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
}

// ─────────────────────────────────────────────────────────────────────────────
// Viewport overlay  (HUD; sits on top of the GL content, no BG)
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawViewport(int fbW, int fbH, float topY,
                              const RendererDebugStats& stats,
                              Scene& scene,
                              bool lookMode)
{
    const float vpX = m_sidebarWidth + kHandleWidth;
    const float vpY = topY;
    const float vpW = static_cast<float>(fbW) - vpX;
    const float vpH = static_cast<float>(fbH) - vpY;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration   |
                             ImGuiWindowFlags_NoMove         |
                             ImGuiWindowFlags_NoSavedSettings|
                             ImGuiWindowFlags_NoNav          |
                             ImGuiWindowFlags_NoScrollbar    |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (lookMode) flags |= ImGuiWindowFlags_NoMouseInputs;

    ImGui::SetNextWindowPos ({vpX, vpY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({vpW, vpH}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);   // fully transparent — GL renders behind it

    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleColor(ImGuiCol_Border,             ImVec4(0, 0, 0, 0));

    if (ImGui::Begin("##VP", nullptr, flags)) {

        // ── Bottom-right HUD pills ─────────────────────────────────────────
        const char* modeStr = "FreeFly";
        {
            FrameSubmission tmp;
            scene.BuildSubmission(tmp);
            if (tmp.camera) {
                switch (tmp.camera->GetMode()) {
                    case CameraMode::FirstPerson: modeStr = "FirstPerson"; break;
                    case CameraMode::ThirdPerson: modeStr = "ThirdPerson"; break;
                    default: break;
                }
            }
        }

        char resBuf[32], dcBuf[32];
        snprintf(resBuf, sizeof(resBuf), "%d × %d", m_vpW, m_vpH);
        snprintf(dcBuf,  sizeof(dcBuf),  "%u draw calls", stats.drawCallCount);

        // Measure so we can right/bottom-align.
        const float pillH    = ImGui::GetTextLineHeight() + 8.f;
        const float pillGap  = 5.f;
        const float totalPillH = pillH * 3.f + pillGap * 2.f;
        float py = vpH - totalPillH - 10.f;

        auto HudPill = [&](const char* txt, bool accent) {
            ImGui::SetCursorPos({vpW - ImGui::CalcTextSize(txt).x - 22.f, py});
            ImGui::PushStyleColor(ImGuiCol_Button,        {0.f,0.f,0.f,0.55f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.f,0.f,0.f,0.55f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.f,0.f,0.f,0.55f});
            ImGui::PushStyleColor(ImGuiCol_Text, accent ? Pal::Accent : Pal::TextDim);
            ImGui::PushStyleVar  (ImGuiStyleVar_FrameRounding, 3.f);
            ImGui::PushStyleVar  (ImGuiStyleVar_FramePadding,  ImVec2(8, 3));
            ImGui::Button(txt);
            ImGui::PopStyleVar(2); ImGui::PopStyleColor(4);
            py += pillH + pillGap;
        };

        HudPill(modeStr, true);
        HudPill(resBuf,  false);
        HudPill(dcBuf,   false);

        // ── Bottom-left gizmo: W wireframe toggle ─────────────────────────
        {
            const float bsz = 26.f;
            const float gx  = 10.f;
            const float gy  = vpH - 30.f;
            ImGui::SetCursorPos({gx, gy});
            const bool wf = wireframeOverride;
            ImGui::PushStyleColor(ImGuiCol_Button,        wf ? Pal::AccentDim : ImVec4(0.f,0.f,0.f,0.50f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.12f,0.12f,0.18f,0.85f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Pal::AccentDim);
            ImGui::PushStyleColor(ImGuiCol_Text,          wf ? Pal::Accent : Pal::TextDim);
            ImGui::PushStyleVar  (ImGuiStyleVar_FrameRounding, 3.f);
            ImGui::PushStyleVar  (ImGuiStyleVar_FramePadding,  ImVec2(0, 0));
            if (ImGui::Button("W##gz", ImVec2(bsz, bsz)))
                wireframeOverride = !wireframeOverride;
            ImGui::PopStyleVar(2); ImGui::PopStyleColor(4);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Scene
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabScene(Scene& scene, const RendererDebugStats& stats) {
    if (SectionHeader("  Camera")) {
        FrameSubmission tmp;
        scene.BuildSubmission(tmp);
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        if (tmp.camera) {
            const glm::vec3 p = tmp.camera->GetPosition();
            auto Row = [](const char* lbl, const char* valFmt, ...) {
                ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
                ImGui::TextUnformatted(lbl);
                ImGui::PopStyleColor();
                ImGui::SameLine(90);
                va_list ap; va_start(ap, valFmt);
                ImGui::TextV(valFmt, ap);
                va_end(ap);
            };
            Row("Position", "%.2f, %.2f, %.2f", p.x, p.y, p.z);
            Row("Yaw/Pitch", "%.1f / %.1f",
                tmp.camera->GetYaw(), tmp.camera->GetPitch());

            const char* mode = "FreeFly";
            switch (tmp.camera->GetMode()) {
                case CameraMode::FirstPerson: mode = "FirstPerson"; break;
                case CameraMode::ThirdPerson: mode = "ThirdPerson"; break;
                default: break;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
            ImGui::TextUnformatted("Mode");
            ImGui::PopStyleColor();
            ImGui::SameLine(90);
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::Green);
            ImGui::TextUnformatted(mode);
            ImGui::PopStyleColor();

            Row("Speed", "%.1f m/s", scene.GetCurrentCameraSpeed());
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::TextUnformatted("No camera");
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (SectionHeader("  Render Stats")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        auto SR = [](const char* l, const char* v){
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
            ImGui::TextUnformatted(l); ImGui::PopStyleColor();
            ImGui::SameLine(110); ImGui::TextUnformatted(v);
        };
        char buf[64];
        snprintf(buf,sizeof(buf),"%u",stats.drawCallCount);            SR("Draw calls",  buf);
        snprintf(buf,sizeof(buf),"%s",FormatCompact(stats.approxTriangleCount).c_str()); SR("Approx tris", buf);
        snprintf(buf,sizeof(buf),"%u",stats.submittedRenderItemCount); SR("Submitted",   buf);
        snprintf(buf,sizeof(buf),"%u",stats.processedRenderItemCount); SR("Processed",   buf);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Lights
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabLights(Scene& scene, const RendererDebugStats& /*stats*/) {
    LightEnvironment& lights = scene.GetLights();

    if (SectionHeader("  Directional Light")) {
        if (lights.HasDirectionalLight())
            DrawDirectionalLight(lights.GetDirectionalLight());
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::TextUnformatted("No directional light in the active scene.");
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }

    // Point lights header with inline "+ add" button
    ImGui::PushStyleColor(ImGuiCol_Header,        Pal::Bg3);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.13f,0.13f,0.17f,1.f});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  Pal::Bg2);
    ImGui::PushStyleColor(ImGuiCol_Text,          Pal::TextDim);
    const bool plOpen = ImGui::CollapsingHeader("  Point Lights##hdr",
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    ImGui::PopStyleColor(4);

    if (plOpen) {
        FrameSubmission tmp; scene.BuildSubmission(tmp);
        const glm::vec3 cam = tmp.camera ? tmp.camera->GetPosition() : glm::vec3(0.f);
        auto& pls = lights.GetPointLights();

        const bool atMax = (static_cast<int>(pls.size()) >= kMaxPointLights);
        if (atMax) ImGui::BeginDisabled();
        if (MiniBadgeButton("+ add", false, true)) {
            PointLight nl;
            nl.position  = cam; nl.color = {1,1,1};
            nl.intensity = 3.f; nl.radius = 20.f;
            nl.name = "Light " + std::to_string(pls.size() + 1);
            lights.AddPointLight(nl);
        }
        if (atMax) {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::Text("(max %d reached)", kMaxPointLights);
            ImGui::PopStyleColor();
        }

        if (pls.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::TextUnformatted("No point lights in the active scene.");
            ImGui::PopStyleColor();
        } else {
            int rm = -1;
            for (std::size_t i = 0; i < pls.size(); ++i)
                if (DrawPointLight(pls[i], static_cast<int>(i), cam))
                    rm = static_cast<int>(i);
            if (rm >= 0) pls.erase(pls.begin() + rm);
        }
        ImGui::Spacing();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Shadow
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabShadow(Scene& scene, const RendererDebugStats& stats) {
    LightEnvironment& lights = scene.GetLights();

    int dirSh = (lights.HasDirectionalLight() &&
                 lights.GetDirectionalLight().shadow.castShadow) ? 1 : 0;
    int ptSh  = 0;
    for (const auto& pl : lights.GetPointLights())
        if (pl.shadow.castShadow) ++ptSh;

    if (SectionHeader("  Summary")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        ImGui::Text("Directional shadow lights : %d", dirSh);
        ImGui::Text("Point shadow lights       : %d", ptSh);
        ImGui::Text("Shadow pass draws         : %u", stats.shadowPassObjectCount);
        ImGui::Text("Receivers                 : %u", stats.shadowReceiverCount);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (lights.HasDirectionalLight())
        if (SectionHeader("  Directional Shadow", false)) {
            DrawShadowParams(lights.GetDirectionalLight().shadow,
                "Far / extent",
                "Resolution is live for directional shadow-map generation.", true);
            ImGui::Spacing();
        }

    auto& pls = lights.GetPointLights();
    for (std::size_t i = 0; i < pls.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const std::string hdr = "  Point Shadow " + std::to_string(i+1) + "##psh";
        if (SectionHeader(hdr.c_str(), false)) {
            DrawShadowParams(pls[i].shadow, "Far plane",
                "Point-light shadow params.", false);
            ImGui::Spacing();
        }
        ImGui::PopID();
    }

    if (SectionHeader("  Cascade Preview", false)) {
        if (stats.shadowMapPreviewAvailable) {
            ImGui::Text("Resolution per cascade: %u x %u",
                        stats.shadowMapWidth, stats.shadowMapHeight);
            const float pw = (m_sidebarWidth - 40.f) * 0.5f;
            for (std::size_t i = 0; i < stats.cascadePreviewTextureIds.size(); ++i) {
                const uint32_t tex = stats.cascadePreviewTextureIds[i];
                const float sn = (i==0) ? 0.f : stats.cascadeSplitDistances[i-1];
                const float sf = stats.cascadeSplitDistances[i];
                ImGui::BeginGroup();
                ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
                ImGui::Text("C%zu [%.0f–%.0f]", i, sn, sf);
                ImGui::PopStyleColor();
                if (tex) ImGui::Image(reinterpret_cast<ImTextureID>(
                             static_cast<intptr_t>(tex)),
                             ImVec2(pw,pw),{0,1},{1,0});
                else { ImGui::Dummy({pw,pw});
                    ImGui::PushStyleColor(ImGuiCol_Text,Pal::TextFaint);
                    ImGui::TextUnformatted("no view"); ImGui::PopStyleColor(); }
                ImGui::EndGroup();
                if ((i%2)==0) ImGui::SameLine(0,4);
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::TextUnformatted("Cascade preview not available for this frame.");
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Stats
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabStats(Scene& /*scene*/, const RendererDebugStats& stats,
                              const AssetCacheStats& cs)
{
    if (SectionHeader("  Performance")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        ImGui::Text("FPS           : %.1f",  stats.fps);
        ImGui::Text("Frame time    : %.2f ms", stats.frameTimeMs);
        ImGui::Text("Draw calls    : %u",    stats.drawCallCount);
        ImGui::Text("Submitted     : %u",    stats.submittedRenderItemCount);
        ImGui::Text("Queued        : %u",    stats.queuedRenderItemCount);
        ImGui::Text("Processed     : %u",    stats.processedRenderItemCount);
        ImGui::Text("Approx tris   : %s",    FormatCompact(stats.approxTriangleCount).c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    if (SectionHeader("  Light Counts")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        ImGui::Text("Directional   : %u", stats.directionalLightCount);
        ImGui::Text("Point lights  : %u", stats.pointLightCount);
        ImGui::Text("Shadow casters: %u", stats.shadowCasterCount);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    if (SectionHeader("  Resource Cache")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        ImGui::Text("Total         : %zu", cs.TotalCount());
        ImGui::Text("Shaders       : %zu", cs.shaderCount);
        ImGui::Text("Textures      : %zu", cs.textureCount);
        ImGui::Text("Meshes        : %zu", cs.meshCount);
        ImGui::Text("Materials     : %zu", cs.materialCount);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Help window
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawHelpWindow(int fbW, int fbH, bool lookMode) {
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos ({fbW * 0.5f, fbH * 0.5f},
                             ImGuiCond_FirstUseEver, {0.5f, 0.5f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      Pal::Bg3);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, Pal::AccentDim);

    const ImGuiWindowFlags f = lookMode ? ImGuiWindowFlags_NoMouseInputs : 0;
    if (ImGui::Begin("Application Guide", &showHelpWindow, f)) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        ImGui::TextWrapped("Use this guide to navigate the application.");
        ImGui::Spacing();
        ImGui::SeparatorText("Keyboard");
        ImGui::BulletText("W/A/S/D       — Move camera");
        ImGui::BulletText("Space/LCtrl   — Up / Down");
        ImGui::BulletText("Shift         — Sprint");
        ImGui::BulletText("TAB           — Toggle mouse capture");
        ImGui::BulletText("1 – 4         — Switch scene");
        ImGui::Spacing();
        ImGui::SeparatorText("Mouse");
        ImGui::BulletText("Move          — Look (when captured)");
        ImGui::BulletText("RMB hold      — Temporary capture");
        ImGui::BulletText("Scroll        — Zoom / FOV");
        ImGui::Spacing();
        ImGui::SeparatorText("Tips");
        ImGui::TextWrapped("Drag the thin vertical line between the sidebar and "
                           "viewport to resize or collapse the panel. "
                           "Light edits apply in real-time.");
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}