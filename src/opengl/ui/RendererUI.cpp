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

namespace Pal {
    // Apple-inspired Dark Theme
    static constexpr ImVec4 Bg0 = {0.05f, 0.05f, 0.05f, 0.95f}; // Near black, high alpha
    static constexpr ImVec4 Bg1 = {0.11f, 0.11f, 0.12f, 0.85f}; // Deep gray, translucent
    static constexpr ImVec4 Bg2 = {0.16f, 0.16f, 0.17f, 0.90f}; // Lighter gray for items
    static constexpr ImVec4 Bg3 = {0.20f, 0.20f, 0.22f, 1.00f}; // Headers
    static constexpr ImVec4 Border = {0.25f, 0.25f, 0.26f, 0.50f}; // Subtle borders
    static constexpr ImVec4 Accent = {0.00f, 0.48f, 1.00f, 1.00f}; // Apple Blue (#007aff)
    static constexpr ImVec4 AccentDim = {0.00f, 0.48f, 1.00f, 0.25f}; // Soft blue tint
    static constexpr ImVec4 Green = {0.20f, 0.84f, 0.29f, 1.00f}; // Apple Green
    static constexpr ImVec4 Orange = {1.00f, 0.62f, 0.04f, 1.00f}; // Apple Orange
    static constexpr ImVec4 Red = {1.00f, 0.28f, 0.24f, 1.00f}; // Apple Red
    static constexpr ImVec4 RedBg = {0.25f, 0.10f, 0.10f, 1.00f};
    static constexpr ImVec4 TextHi = {1.00f, 1.00f, 1.00f, 1.00f}; // San Francisco White
    static constexpr ImVec4 TextMid = {0.92f, 0.92f, 0.95f, 0.80f}; // Secondary
    static constexpr ImVec4 TextDim = {0.55f, 0.55f, 0.57f, 1.00f}; // Tertiary
    static constexpr ImVec4 TextFaint = {0.38f, 0.38f, 0.40f, 1.00f}; // Quaternary
}

static constexpr float kPanelPadding = 12.0f;
static constexpr float kRounding = 10.0f;
static constexpr float kTopbarHeight = 44.0f; // Slimmer, Apple-like
static constexpr int kTabCount = 4;
static const char *kTabLabels[] = {"Scene", "Lights", "Shadow", "Stats"};

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

static bool MiniBadgeButton(const char *label, bool danger = false, bool add = false) {
    if (danger) {
        ImGui::PushStyleColor(ImGuiCol_Button, Pal::RedBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f, 0.08f, 0.08f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.15f, 0.04f, 0.04f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::Red);
    } else if (add) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.05f, 0.18f, 0.10f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.08f, 0.26f, 0.14f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.04f, 0.14f, 0.08f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {0.30f, 0.62f, 0.43f, 1.f});
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, Pal::Bg3);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.15f, 0.15f, 0.19f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Pal::Bg2);
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    const bool clicked = ImGui::Button(label);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

static bool SectionHeader(const char *label, bool defaultOpen = true) {
    ImGui::PushStyleColor(ImGuiCol_Header, {1, 1, 1, 0.03f});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {1, 1, 1, 0.08f});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, {1, 1, 1, 0.12f});
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextHi);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;
    if (defaultOpen) f |= ImGuiTreeNodeFlags_DefaultOpen;
    const bool open = ImGui::CollapsingHeader(label, f);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return open;
}

// ─────────────────────────────────────────────────────────────────────────────
// Light editors  (unchanged logic, restyled)
// ─────────────────────────────────────────────────────────────────────────────
static void DrawDirectionalLight(DirectionalLight &light) {
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
    ImGui::DragFloat("Intensity##dir", &light.intensity, 0.05f, 0.0f, 1000.0f, "%.3f");
    ImGui::ColorEdit3("Color##dir", &light.color.x);

    const glm::vec3 &d = light.direction;
    float elev = glm::degrees(std::asin(glm::clamp(d.y, -1.0f, 1.0f)));
    float azim = glm::degrees(std::atan2(d.z, d.x));
    bool changed = false;
    changed |= ImGui::DragFloat("Azimuth (°)##dir", &azim, 0.5f, -180.f, 180.f, "%.1f");
    changed |= ImGui::DragFloat("Elevation (°)##dir", &elev, 0.5f, -90.f, 90.f, "%.1f");
    if (changed) {
        const float pr = glm::radians(elev), yr = glm::radians(azim);
        light.direction = {std::cos(pr) * std::cos(yr), std::sin(pr), std::cos(pr) * std::sin(yr)};
    }
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
    ImGui::Text("XYZ: %.3f, %.3f, %.3f", d.x, d.y, d.z);
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
}

// Returns true → remove this light.
static bool DrawPointLight(PointLight &light, int idx, const glm::vec3 &camPos) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Pal::Bg3);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

    const std::string cid = "plc" + std::to_string(idx);
    ImGui::BeginChild(cid.c_str(), ImVec2(0, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);

    // Header row
    const std::string name = light.name.empty() ? ("Light " + std::to_string(idx + 1)) : light.name;
    ImGui::TextUnformatted(name.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
    const bool remove = MiniBadgeButton(("X##rm" + std::to_string(idx)).c_str(), true);

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
    const std::string p = "##pl" + std::to_string(idx);
    ImGui::DragFloat(("Intensity" + p).c_str(), &light.intensity, 0.05f, 0.f, 1000.f, "%.3f");
    ImGui::ColorEdit3(("Color" + p).c_str(), &light.color.x);
    ImGui::DragFloat3(("Position" + p).c_str(), &light.position.x, 0.05f);
    ImGui::DragFloat(("Radius" + p).c_str(), &light.radius, 0.5f, 0.1f, 500.f, "%.1f");
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

static void DrawShadowParams(LightShadowParams &s, const char *farLabel,
                             const char *note, bool dirNote) {
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
    ImGui::Checkbox("Cast shadow", &s.castShadow);
    ImGui::DragFloat("Depth bias", &s.depthBias, 0.0001f, 0.f, 1.f, "%.5f");
    ImGui::DragFloat("Normal bias", &s.normalBias, 0.0005f, 0.f, 10.f, "%.4f");
    ImGui::DragFloat("Slope bias", &s.slopeBias, 0.050f, 0.f, 10.f, "%.3f");
    int res[2] = {s.shadowMapWidth, s.shadowMapHeight};
    if (ImGui::DragInt2("Resolution", res, 4.f, 16, 8192)) {
        s.shadowMapWidth = std::max(16, res[0]);
        s.shadowMapHeight = std::max(16, res[1]);
    }
    ImGui::DragFloat("Near plane", &s.nearPlane, 0.01f, 0.001f, 1000.f, "%.3f");
    ImGui::DragFloat(farLabel, &s.farPlane, 0.05f, 0.01f, 10000.f, "%.3f");
    if (s.nearPlane < 0.001f) s.nearPlane = 0.001f;
    if (s.farPlane <= s.nearPlane) s.farPlane = s.nearPlane + 0.01f;
    ImGui::SliderInt("PCF radius", &s.pcfRadius, 0, 4);
    if (s.pcfRadius < 0) s.pcfRadius = 0;
    const int k = s.pcfRadius * 2 + 1;
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
    ImGui::Text("PCF kernel: %dx%d (%d taps)", k, k, k * k);
    ImGui::TextWrapped("%s", note);
    if (dirNote) ImGui::TextWrapped("Directional cast/bias packed in light UBO.");
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level Draw — computes the layout and calls sub-draw methods
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::Draw(int fbW, int fbH,
                      Scene &scene, Renderer &renderer, MouseInput *mouse,
                      const std::vector<Scene *> &scenes,
                      std::size_t &activeSceneIndex) {
    const bool lookMode = mouse && mouse->IsCaptured();
    const RendererDebugStats &stats = renderer.GetDebugStats();

    // ── Layout logic ──────────────────────────────────────────────────────────
    // In the modern concept, the GL viewport fills the entire window.
    // Floating panels sit on top of it.
    m_vpX = 0;
    m_vpY = 0;
    m_vpW = fbW;
    m_vpH = fbH;

    if (m_vpW != m_prevVpW || m_vpH != m_prevVpH) {
        renderer.Resize(m_vpW, m_vpH);
        m_prevVpW = m_vpW;
        m_prevVpH = m_vpH;
    }

    // ── Global style overrides ────────────────────────────────────────────────
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = kRounding;
    style.ChildRounding = kRounding * 0.5f;
    style.PopupRounding = kRounding * 0.5f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.ItemSpacing = ImVec2(8, 8);
    style.Colors[ImGuiCol_WindowBg] = Pal::Bg1;
    style.Colors[ImGuiCol_Border] = Pal::Border;

    // ── Draw Regions ──────────────────────────────────────────────────────────
    // The "Topbar" is now a floating Dynamic Island centered at the top.
    DrawTopbar(fbW, scene, stats, scenes, activeSceneIndex, lookMode);

    // The "Sidebar" is now a floating Inspector panel on the left.
    if (showSidebar) {
        DrawSidebar(fbH, kPanelPadding, scene, stats, lookMode);
    }

    // Resize handle is removed as the panel is now floating/overlayed.
    // DrawResizeHandle(fbH, kTopbarHeight);

    // HUD overlays (camera badge, FPS etc)
    DrawViewport(fbW, fbH, 0.0f, stats, scene, lookMode);

    if (showHelpWindow)
        DrawHelpWindow(fbW, fbH, lookMode);
}

// ─────────────────────────────────────────────────────────────────────────────
// Topbar  (full width, fixed height)
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTopbar(int fbW,
                            Scene &scene,
                            const RendererDebugStats &stats,
                            const std::vector<Scene *> &scenes,
                            std::size_t &activeSceneIndex,
                            bool lookMode) {
    // Dynamic Island Style: Centered floating pill
    const float pillWidth = 400.0f;
    const float pillHeight = kTopbarHeight;
    const float pillX = (static_cast<float>(fbW) - pillWidth) * 0.5f;
    const float pillY = kPanelPadding;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings;
    if (lookMode) flags |= ImGuiWindowFlags_NoMouseInputs;

    ImGui::SetNextWindowPos({pillX, pillY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({pillWidth, pillHeight}, ImGuiCond_Always);

    // Glass effect
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Pal::Bg1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, pillHeight * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 0));

    if (ImGui::Begin("##DynamicIsland", nullptr, flags)) {
        // ── Dropdown menu style helpers ───────────────────────────────────────
        auto BeginMenuButton = [&](const char *label, bool isActive, const char *hotkey = nullptr) -> bool {
            ImGui::PushStyleColor(ImGuiCol_Button, isActive ? Pal::AccentDim : ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1.0f, 1.0f, 1.0f, 0.05f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Pal::AccentDim);
            ImGui::PushStyleColor(ImGuiCol_Text, isActive ? Pal::Accent : Pal::TextMid);

            ImGui::SetCursorPosY((pillHeight - ImGui::GetFrameHeight()) * 0.5f);
            const bool clicked = ImGui::Button(label);
            if (ImGui::IsItemHovered() && hotkey) {
                ImGui::SetTooltip("Hotkey: %s", hotkey);
            }
            ImGui::PopStyleColor(4);
            return clicked;
        };

        ImGui::SameLine(0, 20);
        ImGui::SetCursorPosY(0);

        // --- Scenes ---
        BeginMenuButton("Scenes", false);
        if (ImGui::BeginPopupContextItem("##scenesPopup", ImGuiPopupFlags_MouseButtonLeft)) {
            for (std::size_t i = 0; i < scenes.size(); ++i) {
                if (!scenes[i]) continue;
                const bool sel = (i == activeSceneIndex);
                if (ImGui::MenuItem(scenes[i]->GetName().c_str(), nullptr, sel))
                    activeSceneIndex = i;
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine(0, 8);

        // --- View ---
        if (BeginMenuButton("View", wireframeOverride || showSidebar, "V")) {
            ImGui::OpenPopup("##viewPopup");
        }
        if (ImGui::BeginPopup("##viewPopup")) {
            if (ImGui::MenuItem("Wireframe", "W", wireframeOverride))
                wireframeOverride = !wireframeOverride;
            ImGui::Separator();
            if (ImGui::MenuItem("Shaded", nullptr, !wireframeOverride))
                wireframeOverride = false;
            if (ImGui::MenuItem("Inspector", "S", showSidebar))
                showSidebar = !showSidebar;
            ImGui::EndPopup();
        }

        ImGui::SameLine(0, 8);

        // --- Help ---
        if (BeginMenuButton("Help", showHelpWindow, "H")) {
            showHelpWindow = !showHelpWindow;
        }

        // --- Stats Pill (integrated mini) ---
        float statsX = pillWidth - 80.0f;
        ImGui::SetCursorPos({statsX, (pillHeight - ImGui::GetTextLineHeight()) * 0.5f});
        ImGui::TextColored(Pal::Green, "%.0f", stats.fps);
        ImGui::SameLine(0, 4);
        ImGui::TextColored(Pal::TextDim, "FPS");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawSidebar(int fbH, float topY,
                             Scene &scene,
                             const RendererDebugStats &stats,
                             bool lookMode) {
    // Modern Apple-style floating Inspector
    const float sbW = 320.0f;
    const float sbH = static_cast<float>(fbH) - (kPanelPadding * 2.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings;
    if (lookMode) flags |= ImGuiWindowFlags_NoMouseInputs;

    ImGui::SetNextWindowPos({kPanelPadding, kPanelPadding}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({sbW, sbH}, ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    if (ImGui::Begin("##Inspector", nullptr, flags)) {
        // --- Tab Selection (Pill style) ---
        ImGui::SetCursorPos({16, 16});
        ImGui::BeginChild("##tabBar", ImVec2(sbW - 32, 36), false, ImGuiWindowFlags_NoScrollbar);

        const float tabW = (sbW - 32) / kTabCount;
        for (int i = 0; i < kTabCount; ++i) {
            const bool active = (static_cast<int>(m_activeTab) == i);
            ImGui::PushStyleColor(ImGuiCol_Button, active ? Pal::Accent : ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? Pal::Accent : ImVec4(1, 1, 1, 0.05f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Pal::Accent);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? Pal::TextHi : Pal::TextDim);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18.0f);

            if (ImGui::Button(kTabLabels[i], ImVec2(tabW, 30))) {
                m_activeTab = static_cast<UITab>(i);
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            if (i < kTabCount - 1) ImGui::SameLine(0, 0);
        }
        ImGui::EndChild();

        ImGui::Separator();

        // --- Scrollable content area ---
        ImGui::SetCursorPosX(0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
        ImGui::BeginChild("##content", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 12));

        const AssetCacheStats cs = AssetImporter::GetCacheStats();
        switch (m_activeTab) {
            case UITab::Scene: DrawTabScene(scene, stats);
                break;
            case UITab::Lights: DrawTabLights(scene, stats);
                break;
            case UITab::Shadow: DrawTabShadow(scene, stats);
                break;
            case UITab::Stats: DrawTabStats(scene, stats, cs);
                break;
        }

        if (lookMode) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::Separator();
            ImGui::TextUnformatted("TAB - release mouse for UI");
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar(2);
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Viewport overlay  (HUD; sits on top of the GL content, no BG)
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawViewport(int fbW, int fbH, float topY,
                              const RendererDebugStats &stats,
                              Scene &scene,
                              bool lookMode) {
    // Minimalist overlays sitting on the viewport
    const float vpW = static_cast<float>(fbW);
    const float vpH = static_cast<float>(fbH);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (lookMode) flags |= ImGuiWindowFlags_NoMouseInputs;

    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({vpW, vpH}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f); // transparent

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPanelPadding, kPanelPadding));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));

    if (ImGui::Begin("##VP_Overlay", nullptr, flags)) {
        // ── Bottom-right HUD pills ─────────────────────────────────────────
        const char *modeStr = "FreeFly";
        {
            FrameSubmission tmp;
            scene.BuildSubmission(tmp);
            if (tmp.camera) {
                switch (tmp.camera->GetMode()) {
                    case CameraMode::FirstPerson: modeStr = "First Person";
                        break;
                    case CameraMode::ThirdPerson: modeStr = "Third Person";
                        break;
                    default: break;
                }
            }
        }

        char resBuf[32], dcBuf[32];
        snprintf(resBuf, sizeof(resBuf), "%d × %d", fbW, fbH);
        snprintf(dcBuf, sizeof(dcBuf), "%u draw calls", stats.drawCallCount);

        const float pillH = 26.0f;
        const float pillGap = 6.0f;
        float py = vpH - pillH - kPanelPadding;

        auto HudPill = [&](const char *txt, bool accent, const char *hotkey = nullptr) {
            const float tw = ImGui::CalcTextSize(txt).x;
            const float pw = tw + 20.0f;
            ImGui::SetCursorPos({vpW - pw - kPanelPadding, py});

            ImGui::PushStyleColor(ImGuiCol_Button, {0.07f, 0.07f, 0.08f, 0.7f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.1f, 0.1f, 0.12f, 0.8f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.05f, 0.05f, 0.06f, 0.9f});
            ImGui::PushStyleColor(ImGuiCol_Text, accent ? Pal::Accent : Pal::TextDim);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, pillH * 0.5f);

            if (ImGui::Button(txt, ImVec2(pw, pillH)) && hotkey) {
                // Potential action trigger
            }
            if (ImGui::IsItemHovered() && hotkey) {
                ImGui::SetTooltip("Hotkey: %s", hotkey);
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            py -= (pillH + pillGap);
        };

        HudPill(dcBuf, false);
        HudPill(resBuf, false);
        HudPill(modeStr, true, "C (switch)");

        // ── Bottom-left gizmo row ──────────────────────────────────────────
        {
            const float bsz = 32.0f;
            const float gx = kPanelPadding + (showSidebar ? 320.0f + 8.0f : 0.0f);
            const float gy = vpH - bsz - kPanelPadding;

            ImGui::SetCursorPos({gx, gy});

            auto ToolBtn = [&](const char *lbl, bool active, const char *hotkey, const char *tooltip) {
                ImGui::PushStyleColor(ImGuiCol_Button, active ? Pal::Accent : ImVec4(0.07f, 0.07f, 0.08f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? Pal::Accent : ImVec4(1, 1, 1, 0.1f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, Pal::Accent);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

                if (ImGui::Button(lbl, ImVec2(bsz, bsz))) {
                    if (std::string(lbl) == "W") wireframeOverride = !wireframeOverride;
                    if (std::string(lbl) == "«" or std::string(lbl) == "»") showSidebar = !showSidebar;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s (Hotkey: %s)", tooltip, hotkey);
                }

                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                ImGui::SameLine(0, 8);
            };

            ToolBtn(showSidebar ? "«" : "»", showSidebar, "S", "Toggle Inspector");
            ToolBtn("W", wireframeOverride, "W", "Toggle Wireframe");
            // Placeholder for other tools (G for grid, etc)
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Scene
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabScene(Scene &scene, const RendererDebugStats &stats) {
    if (SectionHeader("  Camera")) {
        FrameSubmission tmp;
        scene.BuildSubmission(tmp);
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        if (tmp.camera) {
            const glm::vec3 p = tmp.camera->GetPosition();
            auto Row = [](const char *lbl, const char *valFmt, ...) {
                ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
                ImGui::TextUnformatted(lbl);
                ImGui::PopStyleColor();
                ImGui::SameLine(90);
                va_list ap;
                va_start(ap, valFmt);
                ImGui::TextV(valFmt, ap);
                va_end(ap);
            };
            Row("Position", "%.2f, %.2f, %.2f", p.x, p.y, p.z);
            Row("Yaw/Pitch", "%.1f / %.1f",
                tmp.camera->GetYaw(), tmp.camera->GetPitch());

            const char *mode = "FreeFly";
            switch (tmp.camera->GetMode()) {
                case CameraMode::FirstPerson: mode = "FirstPerson";
                    break;
                case CameraMode::ThirdPerson: mode = "ThirdPerson";
                    break;
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
        auto SR = [](const char *l, const char *v) {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
            ImGui::TextUnformatted(l);
            ImGui::PopStyleColor();
            ImGui::SameLine(110);
            ImGui::TextUnformatted(v);
        };
        char buf[64];
        snprintf(buf, sizeof(buf), "%u", stats.drawCallCount);
        SR("Draw calls", buf);
        snprintf(buf, sizeof(buf), "%s", FormatCompact(stats.approxTriangleCount).c_str());
        SR("Approx tris", buf);
        snprintf(buf, sizeof(buf), "%u", stats.submittedRenderItemCount);
        SR("Submitted", buf);
        snprintf(buf, sizeof(buf), "%u", stats.processedRenderItemCount);
        SR("Processed", buf);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Lights
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabLights(Scene &scene, const RendererDebugStats & /*stats*/) {
    LightEnvironment &lights = scene.GetLights();

    if (SectionHeader("  Directional Light")) {
        if (lights.HasDirectionalLight())
            DrawDirectionalLight(lights.GetDirectionalLight());
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::TextUnformatted("No directional light in scene.");
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }

    if (SectionHeader("  Point Lights")) {
        FrameSubmission tmp;
        scene.BuildSubmission(tmp);
        const glm::vec3 cam = tmp.camera ? tmp.camera->GetPosition() : glm::vec3(0.f);
        auto &pls = lights.GetPointLights();

        const bool atMax = (static_cast<int>(pls.size()) >= kMaxPointLights);
        if (atMax) ImGui::BeginDisabled();
        if (ImGui::Button("+ Add Point Light", ImVec2(-1, 0))) {
            PointLight nl;
            nl.position = cam;
            nl.color = {1, 1, 1};
            nl.intensity = 3.f;
            nl.radius = 20.f;
            nl.name = "Light " + std::to_string(pls.size() + 1);
            lights.AddPointLight(nl);
        }
        if (atMax) {
            ImGui::EndDisabled();
            ImGui::TextColored(Pal::Orange, "Max lights reached.");
        }

        ImGui::Spacing();

        if (pls.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::TextUnformatted("No point lights in scene.");
            ImGui::PopStyleColor();
        } else {
            int rm = -1;
            for (std::size_t i = 0; i < pls.size(); ++i) {
                if (DrawPointLight(pls[i], static_cast<int>(i), cam)) {
                    rm = static_cast<int>(i);
                }
            }
            if (rm >= 0) pls.erase(pls.begin() + rm);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Shadow
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabShadow(Scene &scene, const RendererDebugStats &stats) {
    LightEnvironment &lights = scene.GetLights();

    int dirSh = (lights.HasDirectionalLight() &&
                 lights.GetDirectionalLight().shadow.castShadow)
                    ? 1
                    : 0;
    int ptSh = 0;
    for (const auto &pl: lights.GetPointLights())
        if (pl.shadow.castShadow) ++ptSh;

    if (SectionHeader("  Summary")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        ImGui::Text("Dir shadow lights : %d", dirSh);
        ImGui::Text("Point shadow lights: %d", ptSh);
        ImGui::Text("Shadow draws      : %u", stats.shadowPassObjectCount);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (lights.HasDirectionalLight()) {
        if (SectionHeader("  Directional Shadow", false)) {
            DrawShadowParams(lights.GetDirectionalLight().shadow,
                             "Far / extent",
                             "Resolution is live for directional shadow-map generation.", true);
            ImGui::Spacing();
        }
    }

    auto &pls = lights.GetPointLights();
    for (std::size_t i = 0; i < pls.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const std::string hdr = "  Point Shadow " + std::to_string(i + 1) + "##psh";
        if (SectionHeader(hdr.c_str(), false)) {
            DrawShadowParams(pls[i].shadow, "Far plane",
                             "Point-light shadow params.", false);
            ImGui::Spacing();
        }
        ImGui::PopID();
    }

    if (SectionHeader("  Cascade Preview", false)) {
        if (stats.shadowMapPreviewAvailable) {
            ImGui::Text("Res/cascade: %u x %u", stats.shadowMapWidth, stats.shadowMapHeight);

            // Adapt to floating panel width (roughly 320 - padding)
            const float pw = (320.0f - 40.0f) * 0.5f;
            for (std::size_t i = 0; i < stats.cascadePreviewTextureIds.size(); ++i) {
                const uint32_t tex = stats.cascadePreviewTextureIds[i];
                const float sn = (i == 0) ? 0.f : stats.cascadeSplitDistances[i - 1];
                const float sf = stats.cascadeSplitDistances[i];
                ImGui::BeginGroup();
                ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
                ImGui::Text("C%zu [%.0f–%.0f]", i, sn, sf);
                ImGui::PopStyleColor();
                if (tex)
                    ImGui::Image(reinterpret_cast<ImTextureID>(
                                     static_cast<intptr_t>(tex)),
                                 ImVec2(pw, pw), {0, 1}, {1, 0});
                else {
                    ImGui::Dummy({pw, pw});
                    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
                    ImGui::TextUnformatted("no view");
                    ImGui::PopStyleColor();
                }
                ImGui::EndGroup();
                if ((i % 2) == 0) ImGui::SameLine(0, 4);
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::TextUnformatted("Cascade preview not available.");
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Stats
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabStats(Scene & /*scene*/, const RendererDebugStats &stats,
                              const AssetCacheStats &cs) {
    if (SectionHeader("  Performance")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        ImGui::Text("FPS           : %.1f", stats.fps);
        ImGui::Text("Frame time    : %.2f ms", stats.frameTimeMs);
        ImGui::Text("Draw calls    : %u", stats.drawCallCount);
        ImGui::Text("Submitted     : %u", stats.submittedRenderItemCount);
        ImGui::Text("Queued        : %u", stats.queuedRenderItemCount);
        ImGui::Text("Processed     : %u", stats.processedRenderItemCount);
        ImGui::Text("Approx tris   : %s", FormatCompact(stats.approxTriangleCount).c_str());
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
void RendererUI::DrawHelpWindow(int fbW, int fbH, bool /*lookMode*/) {
    const float w = 380.0f;
    const float h = 420.0f;
    ImGui::SetNextWindowPos({(fbW - w) * 0.5f, (fbH - h) * 0.5f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, Pal::Accent);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));

    if (ImGui::Begin("Keyboard Shortcuts", &showHelpWindow, flags)) {
        auto KeyRow = [](const char *key, const char *desc) {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::Accent);
            ImGui::Text("%-12s", key);
            ImGui::PopStyleColor();
            ImGui::SameLine(100);
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
            ImGui::TextWrapped("%s", desc);
            ImGui::PopStyleColor();
            ImGui::Spacing();
        };

        ImGui::TextColored(Pal::TextDim, "NAVIGATION");
        ImGui::Separator();
        KeyRow("W, A, S, D", "Move camera (FreeFly)");
        KeyRow("CTRL, SPACE", "Move down / up");
        KeyRow("TAB", "Toggle camera angle movement");
        KeyRow("SHIFT", "Raise movement speed");

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextColored(Pal::TextDim, "CONTROLS");
        ImGui::Separator();
        KeyRow("X", "Unimplemented hotkey");
        KeyRow("X", "Unimplemented hotkey");
        KeyRow("X", "Unimplemented hotkey");
        KeyRow("X", "Unimplemented hotkey");

        ImGui::SetCursorPosY(h - 50.0f);
        if (ImGui::Button("Close", ImVec2(-1, 30))) {
            showHelpWindow = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
