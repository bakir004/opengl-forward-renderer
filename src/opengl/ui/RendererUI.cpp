#include "ui/RendererUI.h"

#include "core/Camera.h"
#include "core/Renderer.h"
#include "core/MouseInput.h"
#include "scene/Scene.h"
#include "scene/LightEnvironment.h"
#include "scene/FrameSubmission.h"
#include "core/Material.h"
#include "assets/AssetImporter.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Palette
// ─────────────────────────────────────────────────────────────────────────────
namespace Pal {
    static constexpr ImVec4 Bg1 = {0.11f, 0.11f, 0.12f, 0.85f};
    static constexpr ImVec4 Bg2 = {0.16f, 0.16f, 0.17f, 0.90f};
    static constexpr ImVec4 Bg3 = {0.20f, 0.20f, 0.22f, 1.00f};
    static constexpr ImVec4 Border = {0.25f, 0.25f, 0.26f, 0.50f};
    static constexpr ImVec4 Accent = {0.00f, 0.48f, 1.00f, 1.00f};
    static constexpr ImVec4 AccentDim = {0.00f, 0.48f, 1.00f, 0.25f};
    static constexpr ImVec4 Green = {0.20f, 0.84f, 0.29f, 1.00f};
    static constexpr ImVec4 Orange = {1.00f, 0.62f, 0.04f, 1.00f};
    static constexpr ImVec4 Red = {1.00f, 0.28f, 0.24f, 1.00f};
    static constexpr ImVec4 RedBg = {0.25f, 0.10f, 0.10f, 1.00f};
    static constexpr ImVec4 TextHi = {1.00f, 1.00f, 1.00f, 1.00f};
    static constexpr ImVec4 TextMid = {0.92f, 0.92f, 0.95f, 0.80f};
    static constexpr ImVec4 TextDim = {0.55f, 0.55f, 0.57f, 1.00f};
    static constexpr ImVec4 TextFaint = {0.38f, 0.38f, 0.40f, 1.00f};
}

static constexpr float kPanelPadding = 12.0f;
static constexpr float kRounding = 10.0f;
static constexpr float kTopbarHeight = 44.0f;
static constexpr float kSidebarWidth = 360.0f;
static constexpr int kTabCount = 5;
static const char *kTabLabels[] = {"Scene", "Lights", "Materials", "Shadow", "Stats"};

// ─────────────────────────────────────────────────────────────────────────────
// ApplyTheme — call once after ImGui::CreateContext(), NOT inside a frame.
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::ApplyTheme() {
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
    style.DisplaySafeAreaPadding = ImVec2(8, 8);
    style.ItemSpacing = ImVec2(8, 8);
    style.Colors[ImGuiCol_WindowBg] = Pal::Bg1;
    style.Colors[ImGuiCol_Border] = Pal::Border;
}

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

static bool ProjectWorldToViewport(const Camera &camera,
                                   const glm::vec3 &world,
                                   float viewportWidth,
                                   float viewportHeight,
                                   ImVec2 &outScreen) {
    const glm::vec4 clip = camera.GetViewProjection() * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0001f) return false;

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.0f || ndc.z > 1.0f) return false;

    outScreen.x = (ndc.x * 0.5f + 0.5f) * viewportWidth;
    outScreen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportHeight;
    return true;
}

static void DrawCenteredOverlayText(ImDrawList *drawList,
                                    const ImVec2 &pos,
                                    ImU32 color,
                                    const char *text) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 min(pos.x - size.x * 0.5f - 8.0f, pos.y - size.y * 0.5f - 4.0f);
    const ImVec2 max(pos.x + size.x * 0.5f + 8.0f, pos.y + size.y * 0.5f + 4.0f);
    drawList->AddRectFilled(min, max, IM_COL32(12, 14, 18, 190), 6.0f);
    drawList->AddRect(min, max, IM_COL32(255, 255, 255, 28), 6.0f);
    drawList->AddText(ImVec2(pos.x - size.x * 0.5f, pos.y - size.y * 0.5f), color, text);
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
// Light editors
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
        light.direction = {
            std::cos(pr) * std::cos(yr),
            std::sin(pr),
            std::cos(pr) * std::sin(yr)
        };
    }
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
    ImGui::Text("XYZ: %.3f, %.3f, %.3f", d.x, d.y, d.z);
    ImGui::PopStyleColor(2);
}

// Returns true → caller should remove this light.
static bool DrawPointLight(PointLight &light, int idx, const glm::vec3 &camPos) {
    // Use ImGui's ID stack for uniqueness — no per-frame string allocation needed.
    ImGui::PushID(idx);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Pal::Bg3);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

    ImGui::BeginChild("##plc", ImVec2(0, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);

    // Header row — display name, then remove button flush-right.
    char nameBuf[64];
    if (light.name.empty())
        std::snprintf(nameBuf, sizeof(nameBuf), "Light %d", idx + 1);
    else
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", light.name.c_str());

    ImGui::TextUnformatted(nameBuf);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
    const bool remove = MiniBadgeButton("X##rm", /*danger=*/true);

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);

    // All widgets share the same ID scope (PushID above), so plain labels are fine.
    ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.f, 1000.f, "%.3f");
    ImGui::ColorEdit3("Color", &light.color.x);
    ImGui::DragFloat3("Position", &light.position.x, 0.05f);
    ImGui::DragFloat("Radius", &light.radius, 0.5f, 0.1f, 500.f, "%.1f");

    if (MiniBadgeButton("Move to camera"))
        light.position = camPos;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
    ImGui::Text("(%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PopID();
    return remove;
}

static void DrawShadowParams(LightShadowParams &s, const char *farLabel,
                             const char *note, bool dirNote) {
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
    ImGui::Checkbox("Cast shadow", &s.castShadow);

    // Clamp before presenting so downstream consumers never see a bad value.
    s.nearPlane = std::max(s.nearPlane, 0.001f);
    if (s.farPlane <= s.nearPlane) s.farPlane = s.nearPlane + 0.01f;

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

    // Re-clamp after drag in case user pulled near past far.
    s.nearPlane = std::max(s.nearPlane, 0.001f);
    if (s.farPlane <= s.nearPlane) s.farPlane = s.nearPlane + 0.01f;

    ImGui::SliderInt("PCF radius", &s.pcfRadius, 0, 4);
    s.pcfRadius = std::max(0, s.pcfRadius);

    const int k = s.pcfRadius * 2 + 1;
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
    ImGui::Text("PCF kernel: %dx%d (%d taps)", k, k, k * k);
    ImGui::TextWrapped("%s", note);
    if (dirNote) ImGui::TextWrapped("Directional cast/bias packed in light UBO.");
    ImGui::PopStyleColor(2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level Draw — builds the shared FrameSubmission once, then dispatches.
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::Draw(int fbW, int fbH,
                      Scene &scene, Renderer &renderer, MouseInput *mouse,
                      const std::vector<Scene *> &scenes,
                      std::size_t &activeSceneIndex) {
    const bool lookMode = mouse && mouse->IsCaptured();
    const RendererDebugStats &stats = renderer.GetDebugStats();

    // ── Viewport fills the entire window ─────────────────────────────────────
    m_vpX = 0;
    m_vpY = 0;
    m_vpW = fbW;
    m_vpH = fbH;
    if (m_vpW != m_prevVpW || m_vpH != m_prevVpH) {
        renderer.Resize(m_vpW, m_vpH);
        m_prevVpW = m_vpW;
        m_prevVpH = m_vpH;
    }

    // ── Build scene submission exactly once for the whole frame ───────────────
    FrameSubmission frame;
    scene.BuildSubmission(frame);

    // ── Draw regions ──────────────────────────────────────────────────────────
    DrawTopbar(fbW, scene, stats, scenes, activeSceneIndex, lookMode);

    if (showSidebar)
        DrawSidebar(fbH, scene, stats, frame, lookMode);

    DrawViewport(fbW, fbH, stats, scene, frame, lookMode);

    if (showHelpWindow)
        DrawHelpWindow(fbW, fbH);
}

// ─────────────────────────────────────────────────────────────────────────────
// Topbar — centered Dynamic Island pill
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTopbar(int fbW,
                            Scene &scene,
                            const RendererDebugStats &stats,
                            const std::vector<Scene *> &scenes,
                            std::size_t &activeSceneIndex,
                            bool lookMode) {
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

    ImGui::PushStyleColor(ImGuiCol_WindowBg, Pal::Bg1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, pillHeight * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 0));

    if (ImGui::Begin("##DynamicIsland", nullptr, flags)) {
        // Helper: transparent menu-bar button that highlights when active.
        auto MenuButton = [&](const char *label, bool isActive,
                              const char *hotkey = nullptr) -> bool {
            ImGui::PushStyleColor(ImGuiCol_Button, isActive
                                                       ? Pal::AccentDim
                                                       : ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1.0f, 1.0f, 1.0f, 0.05f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Pal::AccentDim);
            ImGui::PushStyleColor(ImGuiCol_Text, isActive
                                                     ? Pal::Accent
                                                     : Pal::TextMid);
            ImGui::SetCursorPosY((pillHeight - ImGui::GetFrameHeight()) * 0.5f);
            const bool clicked = ImGui::Button(label);
            if (ImGui::IsItemHovered() && hotkey) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                ImGui::SetTooltip("Hotkey: %s", hotkey);
                ImGui::PopStyleVar();
            }
            ImGui::PopStyleColor(4);
            return clicked;
        };

        ImGui::SameLine(0, 20);
        ImGui::SetCursorPosY(0);

        // --- Scenes ---
        MenuButton("Scenes", false);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
        if (ImGui::BeginPopupContextItem("##scenesPopup", ImGuiPopupFlags_MouseButtonLeft)) {
            for (std::size_t i = 0; i < scenes.size(); ++i) {
                if (!scenes[i]) continue;
                if (ImGui::MenuItem(scenes[i]->GetName().c_str(), nullptr,
                                    i == activeSceneIndex))
                    activeSceneIndex = i;
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        ImGui::SameLine(0, 8);

        // --- View ---
        if (MenuButton("View", wireframeOverride || showSidebar || !skyboxOverride, "V"))
            ImGui::OpenPopup("##viewPopup");

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
        if (ImGui::BeginPopup("##viewPopup")) {
            if (ImGui::MenuItem("Inspector", "S", showSidebar))
                showSidebar = !showSidebar;
            ImGui::Separator();
            if (ImGui::MenuItem("Skybox", nullptr, skyboxOverride)) {
                skyboxOverride = !skyboxOverride;
                scene.SetSkyboxVisible(skyboxOverride);
            }
            if (ImGui::MenuItem("Wireframe", "Z", wireframeOverride))
                wireframeOverride = !wireframeOverride;
            if (ImGui::MenuItem("Shaded", nullptr, !wireframeOverride))
                wireframeOverride = false;
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        ImGui::SameLine(0, 8);

        // --- Help ---
        if (MenuButton("Help", showHelpWindow, "H"))
            showHelpWindow = !showHelpWindow;

        // --- Mini FPS pill ---
        ImGui::SetCursorPos({
            pillWidth - 80.0f,
            (pillHeight - ImGui::GetTextLineHeight()) * 0.5f
        });
        ImGui::TextColored(Pal::Green, "%.0f", stats.fps);
        ImGui::SameLine(0, 4);
        ImGui::TextColored(Pal::TextDim, "FPS");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar — floating Inspector panel
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawSidebar(int fbH,
                             Scene &scene,
                             const RendererDebugStats &stats,
                             const FrameSubmission &frame,
                             bool lookMode) {
    const float sbH = static_cast<float>(fbH) - (kPanelPadding * 2.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings;
    if (lookMode) flags |= ImGuiWindowFlags_NoMouseInputs;

    ImGui::SetNextWindowPos({kPanelPadding, kPanelPadding}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({kSidebarWidth, sbH}, ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Pal::Bg1);

    if (ImGui::Begin("##Inspector", nullptr, flags)) {
        // --- Pill-style tab bar ---
        ImGui::SetCursorPos({16, 16});
        ImGui::BeginChild("##tabBar", ImVec2(kSidebarWidth - 32, 36),
                          false, ImGuiWindowFlags_NoScrollbar);

        const float tabW = (kSidebarWidth - 32) / static_cast<float>(kTabCount);
        for (int i = 0; i < kTabCount; ++i) {
            const bool active = (static_cast<int>(m_activeTab) == i);
            ImGui::PushStyleColor(ImGuiCol_Button, active
                                                       ? Pal::Accent
                                                       : ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active
                                                              ? Pal::Accent
                                                              : ImVec4(1, 1, 1, 0.05f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Pal::Accent);
            ImGui::PushStyleColor(ImGuiCol_Text, active
                                                     ? Pal::TextHi
                                                     : Pal::TextDim);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18.0f);

            // Add some horizontal padding for the text inside the button
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

            if (ImGui::Button(kTabLabels[i], ImVec2(tabW, 30)))
                m_activeTab = static_cast<UITab>(i);

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
            if (i < kTabCount - 1) ImGui::SameLine(0, 0);
        }
        ImGui::EndChild();

        ImGui::Separator();

        // --- Scrollable content area ---
        ImGui::SetCursorPosX(0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
        ImGui::BeginChild("##content", ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 12));

        const AssetCacheStats cs = AssetImporter::GetCacheStats();
        switch (m_activeTab) {
            case UITab::Scene: DrawTabScene(scene, stats, frame);
                break;
            case UITab::Lights: DrawTabLights(scene, stats, frame);
                break;
            case UITab::Materials: DrawTabMaterials(scene, stats, frame);
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
            ImGui::TextUnformatted("TAB — release mouse for UI");
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar(2);
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// ─────────────────────────────────────────────────────────────────────────────
// Viewport overlay — HUD; transparent, sits on top of GL content
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawViewport(int fbW, int fbH,
                              const RendererDebugStats &stats,
                              Scene &scene,
                              const FrameSubmission &frame,
                              bool lookMode) {
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
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPanelPadding, kPanelPadding));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));

    if (ImGui::Begin("##VP_Overlay", nullptr, flags)) {
        // ── Camera mode string — use the pre-built submission ─────────────────
        const char *modeStr = "FreeFly";
        if (frame.camera) {
            switch (frame.camera->GetMode()) {
                case CameraMode::FirstPerson: modeStr = "First Person";
                    break;
                case CameraMode::ThirdPerson: modeStr = "Third Person";
                    break;
                default: break;
            }
        }

        // ── Bottom-right HUD pills ─────────────────────────────────────────────
        char resBuf[32], dcBuf[32];
        std::snprintf(resBuf, sizeof(resBuf), "%d × %d", fbW, fbH);
        std::snprintf(dcBuf, sizeof(dcBuf), "%u draw calls", stats.drawCallCount);

        const float pillH = 26.0f;
        const float pillGap = 6.0f;
        float py = vpH - pillH - kPanelPadding;

        // Returns true if the pill was clicked.
        auto HudPill = [&](const char *txt, bool accent,
                           const char *hotkey = nullptr) -> bool {
            const float tw = ImGui::CalcTextSize(txt).x;
            const float pw = tw + 20.0f;
            ImGui::SetCursorPos({vpW - pw - kPanelPadding, py});

            ImGui::PushStyleColor(ImGuiCol_Button, {0.07f, 0.07f, 0.08f, 0.7f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.10f, 0.10f, 0.12f, 0.8f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.05f, 0.05f, 0.06f, 0.9f});
            ImGui::PushStyleColor(ImGuiCol_Text, accent ? Pal::Accent : Pal::TextDim);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, pillH * 0.5f);

            const bool clicked = ImGui::Button(txt, ImVec2(pw, pillH));

            if (ImGui::IsItemHovered() && hotkey) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                ImGui::SetTooltip("Hotkey: %s", hotkey);
                ImGui::PopStyleVar();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            py -= (pillH + pillGap);
            return clicked;
        };

        HudPill(dcBuf, /*accent=*/false);
        HudPill(resBuf, /*accent=*/false);
        if (HudPill(modeStr, /*accent=*/true, "C (switch)")) {
            Camera &cam = scene.GetCamera();
            CameraMode m = static_cast<CameraMode>(
                (static_cast<int>(cam.GetMode()) + 1) % 3);
            cam.SetMode(m);
        }

        if (scene.GetName() == "PBR Validation" && frame.camera) {
            ImDrawList *drawList = ImGui::GetWindowDrawList();

            ImVec2 roughLeft, roughRight, roughLabel;
            ImVec2 metalBottom, metalTop, metalLabel;

            const bool haveRoughLeft = ProjectWorldToViewport(*frame.camera, {-3.04f, -1.15f, 0.95f}, vpW, vpH, roughLeft);
            const bool haveRoughRight = ProjectWorldToViewport(*frame.camera, {3.04f, -1.15f, 0.95f}, vpW, vpH, roughRight);
            const bool haveRoughLabel = ProjectWorldToViewport(*frame.camera, {0.0f, -1.65f, 1.10f}, vpW, vpH, roughLabel);
            const bool haveMetalBottom = ProjectWorldToViewport(*frame.camera, {-4.35f, -1.09f, 0.25f}, vpW, vpH, metalBottom);
            const bool haveMetalTop = ProjectWorldToViewport(*frame.camera, {-4.35f, 4.99f, 0.25f}, vpW, vpH, metalTop);
            const bool haveMetalLabel = ProjectWorldToViewport(*frame.camera, {-5.55f, 1.95f, 0.60f}, vpW, vpH, metalLabel);

            if (haveRoughLeft && haveRoughRight) {
                drawList->AddLine(roughLeft, roughRight, IM_COL32(155, 178, 255, 180), 2.0f);
                drawList->AddCircleFilled(roughLeft, 4.0f, IM_COL32(180, 200, 255, 235));
                drawList->AddCircleFilled(roughRight, 4.0f, IM_COL32(255, 255, 255, 235));
                DrawCenteredOverlayText(drawList, ImVec2(roughLeft.x, roughLeft.y + 18.0f), IM_COL32(225, 232, 255, 255), "0.0");
                DrawCenteredOverlayText(drawList, ImVec2(roughRight.x, roughRight.y + 18.0f), IM_COL32(225, 232, 255, 255), "1.0");
            }

            if (haveMetalBottom && haveMetalTop) {
                drawList->AddLine(metalBottom, metalTop, IM_COL32(255, 198, 132, 180), 2.0f);
                drawList->AddCircleFilled(metalBottom, 4.0f, IM_COL32(255, 224, 190, 235));
                drawList->AddCircleFilled(metalTop, 4.0f, IM_COL32(255, 244, 224, 235));
                DrawCenteredOverlayText(drawList, ImVec2(metalBottom.x - 28.0f, metalBottom.y), IM_COL32(255, 238, 214, 255), "0.0");
                DrawCenteredOverlayText(drawList, ImVec2(metalTop.x - 28.0f, metalTop.y), IM_COL32(255, 238, 214, 255), "1.0");
            }

            if (haveRoughLabel)
                DrawCenteredOverlayText(drawList, roughLabel, IM_COL32(225, 232, 255, 255), "Roughness");

            if (haveMetalLabel)
                DrawCenteredOverlayText(drawList, metalLabel, IM_COL32(255, 238, 214, 255), "Metallic");
        }

        // ── Bottom-left gizmo row ──────────────────────────────────────────────
        {
            const float bsz = 32.0f;
            const float gx = kPanelPadding + (showSidebar ? kSidebarWidth + 8.0f : 0.0f);
            const float gy = vpH - bsz - kPanelPadding;
            ImGui::SetCursorPos({gx, gy});

            // Action-driven tool button — no string comparison, no allocation.
            auto ToolBtn = [&](const char *lbl, bool active,
                               const char *hotkey, const char *tooltip,
                               auto action) {
                ImGui::PushStyleColor(ImGuiCol_Button, active
                                                           ? Pal::Accent
                                                           : ImVec4(0.07f, 0.07f, 0.08f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active
                                                                  ? Pal::Accent
                                                                  : ImVec4(1, 1, 1, 0.1f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, Pal::Accent);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

                if (ImGui::Button(lbl, ImVec2(bsz, bsz)))
                    action();

                if (ImGui::IsItemHovered()) {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                    ImGui::SetTooltip("%s (Hotkey: %s)", tooltip, hotkey);
                    ImGui::PopStyleVar();
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                ImGui::SameLine(0, 8);
            };

            ToolBtn(showSidebar ? "«" : "»", showSidebar, "X", "Toggle Inspector",
                    [&] { showSidebar = !showSidebar; });
            ToolBtn("W", wireframeOverride, "Z", "Toggle Wireframe",
                    [&] { wireframeOverride = !wireframeOverride; });
            ToolBtn("N", normalMapOverride, "N", "Toggle Normal Maps",
                    [&] { normalMapOverride = !normalMapOverride; });
            ToolBtn("S", skyboxOverride, "K", "Toggle Skybox",
                    [&] {
                        skyboxOverride = !skyboxOverride;
                        scene.SetSkyboxVisible(skyboxOverride);
                    });
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Scene
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabScene(Scene &scene, const RendererDebugStats &stats,
                              const FrameSubmission &frame) {
    if (SectionHeader("Camera")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);

        // Label/value row helper — aligns values at column 90.
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

        if (frame.camera) {
            const glm::vec3 p = frame.camera->GetPosition();
            Row("Position", "%.2f, %.2f, %.2f", p.x, p.y, p.z);
            Row("Yaw/Pitch", "%.1f / %.1f",
                frame.camera->GetYaw(), frame.camera->GetPitch());

            const char *modeName = "FreeFly";
            switch (frame.camera->GetMode()) {
                case CameraMode::FirstPerson: modeName = "FirstPerson";
                    break;
                case CameraMode::ThirdPerson: modeName = "ThirdPerson";
                    break;
                default: break;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
            ImGui::TextUnformatted("Mode");
            ImGui::PopStyleColor();
            ImGui::SameLine(90);
            ImGui::TextColored(Pal::Green, "%s", modeName);

            Row("Speed", "%.1f m/s", scene.GetCurrentCameraSpeed());
        } else {
            ImGui::TextColored(Pal::TextFaint, "No camera");
        }

        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (SectionHeader("Render Stats")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);

        auto SR = [](const char *l, const char *v) {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
            ImGui::TextUnformatted(l);
            ImGui::PopStyleColor();
            ImGui::SameLine(110);
            ImGui::TextUnformatted(v);
        };
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%u", stats.drawCallCount);
        SR("Draw calls", buf);
        std::snprintf(buf, sizeof(buf), "%s", FormatCompact(stats.approxTriangleCount).c_str());
        SR("Approx tris", buf);
        std::snprintf(buf, sizeof(buf), "%u", stats.submittedRenderItemCount);
        SR("Submitted", buf);
        std::snprintf(buf, sizeof(buf), "%u", stats.processedRenderItemCount);
        SR("Processed", buf);

        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Lights
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabLights(Scene &scene, const RendererDebugStats & /*stats*/,
                               const FrameSubmission &frame) {
    LightEnvironment &lights = scene.GetLights();

    if (SectionHeader("Global Ambient")) {
        ImGui::ColorEdit3("Ambient Color", &lights.ambientColor.x);
        ImGui::SliderFloat("Ambient Intensity", &lights.ambientIntensity, 0.0f, 5.0f);
        ImGui::Spacing();
    }

    if (SectionHeader("Directional Light")) {
        if (lights.HasDirectionalLight())
            DrawDirectionalLight(lights.GetDirectionalLight());
        else
            ImGui::TextColored(Pal::TextFaint, "No directional light in scene.");
        ImGui::Spacing();
    }

    if (SectionHeader("Point Lights")) {
        // Camera position from the pre-built submission — no extra BuildSubmission.
        const glm::vec3 cam = frame.camera ? frame.camera->GetPosition() : glm::vec3(0.f);
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
            ImGui::TextColored(Pal::TextFaint, "No point lights in scene.");
        } else {
            int rm = -1;
            for (std::size_t i = 0; i < pls.size(); ++i)
                if (DrawPointLight(pls[i], static_cast<int>(i), cam))
                    rm = static_cast<int>(i);

            if (rm >= 0) pls.erase(pls.begin() + rm);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Materials
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabMaterials(Scene &scene, const RendererDebugStats &stats,
                                  const FrameSubmission &frame) {
    if (frame.objects.empty()) {
        ImGui::TextColored(Pal::TextDim, "No renderable objects in current frame.");
        return;
    }

    // Identify unique MaterialInstances in the submission
    std::vector<const MaterialInstance *> uniqueMats;
    for (const auto &item : frame.objects) {
        if (item.material) {
            bool found = false;
            for (auto m : uniqueMats) {
                if (m == item.material) {
                    found = true;
                    break;
                }
            }
            if (!found) uniqueMats.push_back(item.material);
        }
    }

    if (uniqueMats.empty()) {
        ImGui::TextColored(Pal::TextDim, "No material instances found in objects.");
        return;
    }

    ImGui::Text("   %zu Material Instances found", uniqueMats.size());
    ImGui::Separator();
    ImGui::Spacing();

    for (size_t i = 0; i < uniqueMats.size(); ++i) {
        MaterialInstance *inst = const_cast<MaterialInstance *>(uniqueMats[i]);

        char label[256];
        snprintf(label, sizeof(label), "%s##%p", inst->GetName().c_str(), (void*)inst);

        if (SectionHeader(label, /*defaultOpen=*/false)) {
            ImGui::PushID(static_cast<int>(i));

            ImGui::Indent();

            ImGui::Separator();

            // Albedo
            glm::vec3 albedo = inst->GetVec3("u_AlbedoColor", glm::vec3(1.0f));
            if (ImGui::ColorEdit3("Albedo Color", &albedo.x, ImGuiColorEditFlags_NoInputs))
                inst->SetVec3("u_AlbedoColor", albedo);

            // Metallic
            float metallic = inst->GetFloat("u_MetallicValue", 0.0f);
            if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f))
                inst->SetFloat("u_MetallicValue", metallic);

            // Roughness
            float roughness = inst->GetFloat("u_RoughnessValue", 0.5f);
            if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f))
                inst->SetFloat("u_RoughnessValue", roughness);

            // AO
            float aoStrength = inst->GetFloat("u_AoStrength", 1.0f);
            if (ImGui::SliderFloat("AO Strength", &aoStrength, 0.0f, 1.0f))
                inst->SetFloat("u_AoStrength", aoStrength);

            // Emissive
            glm::vec3 emissive = inst->GetVec3("u_EmissiveColor", glm::vec3(0.0f));
            if (ImGui::ColorEdit3("Emissive Color", &emissive.x, ImGuiColorEditFlags_NoInputs))
                inst->SetVec3("u_EmissiveColor", emissive);
            
            float emissiveStr = inst->GetFloat("u_EmissiveStrength", 1.0f);
            if (ImGui::SliderFloat("Emissive Strength", &emissiveStr, 0.0f, 20.0f))
                inst->SetFloat("u_EmissiveStrength", emissiveStr);

            ImGui::Separator();

            // Normal Mapping
            bool useNormalMap = inst->GetUseNormalMap();
            if (ImGui::Checkbox("Use Normal Map", &useNormalMap))
                inst->SetUseNormalMap(useNormalMap);

            if (useNormalMap) {
                float normalScale = inst->GetFloat("u_NormalScale", 1.0f);
                if (ImGui::SliderFloat("Normal Scale", &normalScale, 0.0f, 2.0f))
                    inst->SetFloat("u_NormalScale", normalScale);
            }

            ImGui::Spacing();
            if (ImGui::Button("Reset to Defaults")) {
                // Clear overrides in the instance by setting them to parent's values or nullopt
                // Since we don't have a 'Clear' API yet, we can just set them to standard defaults
                inst->SetVec3("u_AlbedoColor", {1, 1, 1});
                inst->SetFloat("u_MetallicValue", 0.0f);
                inst->SetFloat("u_RoughnessValue", 0.5f);
                inst->SetVec3("u_EmissiveColor", {0, 0, 0});
                inst->SetFloat("u_EmissiveStrength", 1.0f);
                inst->SetFloat("u_AoStrength", 1.0f);
                inst->SetFloat("u_NormalScale", 1.0f);
                inst->SetUseNormalMap(true);
            }

            ImGui::Unindent();
            ImGui::PopID();
            ImGui::Spacing();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Shadow
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabShadow(Scene &scene, const RendererDebugStats &stats) {
    LightEnvironment &lights = scene.GetLights();

    const int dirSh = (lights.HasDirectionalLight() &&
                       lights.GetDirectionalLight().shadow.castShadow)
                          ? 1
                          : 0;
    int ptSh = 0;
    for (const auto &pl: lights.GetPointLights())
        if (pl.shadow.castShadow) ++ptSh;

    if (SectionHeader("Summary")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        ImGui::Text("Dir shadow lights : %d", dirSh);
        ImGui::Text("Point shadow lights: %d", ptSh);
        ImGui::Text("Shadow draws      : %u", stats.shadowPassObjectCount);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (lights.HasDirectionalLight()) {
        if (SectionHeader("Directional Shadow", /*defaultOpen=*/false)) {
            DrawShadowParams(lights.GetDirectionalLight().shadow,
                             "Far / extent",
                             "Resolution is live for directional shadow-map generation.",
                             /*dirNote=*/true);
            ImGui::Spacing();
        }
    }

    auto &pls = lights.GetPointLights();
    for (std::size_t i = 0; i < pls.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        char hdr[48];
        std::snprintf(hdr, sizeof(hdr), "Point Shadow %zu##psh", i + 1);
        if (SectionHeader(hdr, /*defaultOpen=*/false)) {
            DrawShadowParams(pls[i].shadow, "Far plane",
                             "Point-light shadow params.", /*dirNote=*/false);
            ImGui::Spacing();
        }
        ImGui::PopID();
    }

    if (SectionHeader("Cascade Preview", /*defaultOpen=*/false)) {
        if (stats.shadowMapPreviewAvailable) {
            ImGui::Text("Res/cascade: %u x %u",
                        stats.shadowMapWidth, stats.shadowMapHeight);

            const float pw = (kSidebarWidth - 40.0f) * 0.5f;
            for (std::size_t i = 0; i < stats.cascadePreviewTextureIds.size(); ++i) {
                const uint32_t tex = stats.cascadePreviewTextureIds[i];
                const float sn = (i == 0) ? 0.f : stats.cascadeSplitDistances[i - 1];
                const float sf = stats.cascadeSplitDistances[i];

                ImGui::BeginGroup();
                ImGui::TextColored(Pal::TextDim, "C%zu [%.0f–%.0f]", i, sn, sf);
                if (tex)
                    ImGui::Image(reinterpret_cast<ImTextureID>(
                                     static_cast<intptr_t>(tex)),
                                 ImVec2(pw, pw), {0, 1}, {1, 0});
                else {
                    ImGui::Dummy({pw, pw});
                    ImGui::TextColored(Pal::TextFaint, "no view");
                }
                ImGui::EndGroup();
                if ((i % 2) == 0) ImGui::SameLine(0, 4);
            }
        } else {
            ImGui::TextColored(Pal::TextFaint, "Cascade preview not available.");
        }
        ImGui::Spacing();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Stats
// ─────────────────────────────────────────────────────────────────────────────
void RendererUI::DrawTabStats(Scene & /*scene*/, const RendererDebugStats &stats,
                              const AssetCacheStats &cs) {
    if (SectionHeader("Performance")) {
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

    if (SectionHeader("Light Counts")) {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        ImGui::Text("Directional   : %u", stats.directionalLightCount);
        ImGui::Text("Point lights  : %u", stats.pointLightCount);
        ImGui::Text("Shadow casters: %u", stats.shadowCasterCount);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (SectionHeader("Resource Cache")) {
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
void RendererUI::DrawHelpWindow(int fbW, int fbH) {
    constexpr float w = 380.0f;
    constexpr float h = 500.0f;
    ImGui::SetNextWindowPos({(fbW - w) * 0.5f, (fbH - h) * 0.5f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_Always);

    // NoScrollbar removed — let ImGui show the bar only when content overflows,
    // matching the sidebar content child behaviour.
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse; // scroll via keyboard/drag only

    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, Pal::Accent);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Pal::Bg1); // ← translucent, matches sidebar
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding); // ← rounded corners


    if (ImGui::Begin("Keyboard Shortcuts", &showHelpWindow, flags)) {
        auto KeyRow = [](const char *key, const char *desc) {
            ImGui::TextColored(Pal::Accent, "%-12s", key);
            ImGui::SameLine(100);
            ImGui::TextColored(Pal::TextMid, "%s", desc);
            ImGui::Spacing();
        };

        ImGui::TextColored(Pal::TextDim, "MOVEMENT");
        ImGui::Separator();
        KeyRow("W/A/S/D", "Move");
        KeyRow("SPACE/CTRL", "Move up/down");
        KeyRow("SHIFT", "Increase speed");

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextColored(Pal::TextDim, "CAMERA");
        ImGui::Separator();
        KeyRow("TAB", "Toggle rotation mode");
        KeyRow("C", "Cycle camera modes");
        KeyRow("F1", "Free-fly");
        KeyRow("F2", "First-person");
        KeyRow("F3", "Third-person");

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextColored(Pal::TextDim, "INTERFACE");
        ImGui::Separator();
        KeyRow("X", "Toggle inspector");
        KeyRow("Z", "Wireframe mode");
        KeyRow("N", "Toggle normal map");
        KeyRow("K", "Toggle skybox");
        KeyRow("H", "Help window");

        ImGui::Dummy(ImVec2(0.0f, ImGui::GetContentRegionAvail().y - 30.0f));
        if (ImGui::Button("Close", ImVec2(-1, 30)))
            showHelpWindow = false;
    }
    ImGui::End();
    ImGui::PopStyleVar(2); // ← WindowRounding + WindowPadding
    ImGui::PopStyleColor(2); // ← WindowBg + TitleBgActive
}
