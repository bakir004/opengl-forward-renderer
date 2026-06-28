#pragma once
#include <imgui.h>

// Shared palette and lightweight styling helpers used by both RendererUI
// and any scene that draws into a sidebar tab (e.g. TerrainScene).
// All helpers are inline so they can live in a header without ODR issues.

namespace Pal {
    constexpr ImVec4 Bg1      = {0.11f, 0.11f, 0.12f, 0.85f};
    constexpr ImVec4 Bg2      = {0.16f, 0.16f, 0.17f, 0.90f};
    constexpr ImVec4 Bg3      = {0.20f, 0.20f, 0.22f, 1.00f};
    constexpr ImVec4 Border   = {0.25f, 0.25f, 0.26f, 0.50f};
    constexpr ImVec4 Accent   = {0.00f, 0.48f, 1.00f, 1.00f};
    constexpr ImVec4 AccentDim= {0.00f, 0.48f, 1.00f, 0.25f};
    constexpr ImVec4 Green    = {0.20f, 0.84f, 0.29f, 1.00f};
    constexpr ImVec4 Orange   = {1.00f, 0.62f, 0.04f, 1.00f};
    constexpr ImVec4 Red      = {1.00f, 0.28f, 0.24f, 1.00f};
    constexpr ImVec4 RedBg    = {0.25f, 0.10f, 0.10f, 1.00f};
    constexpr ImVec4 TextHi   = {1.00f, 1.00f, 1.00f, 1.00f};
    constexpr ImVec4 TextMid  = {0.92f, 0.92f, 0.95f, 0.80f};
    constexpr ImVec4 TextDim  = {0.55f, 0.55f, 0.57f, 1.00f};
    constexpr ImVec4 TextFaint= {0.38f, 0.38f, 0.40f, 1.00f};
    constexpr ImVec4 Amber    = {1.00f, 0.78f, 0.20f, 1.00f};
}

/// Styled collapsing section header — matches the framed CollapsingHeader
/// used throughout the sidebar tabs.
inline bool SectionHeader(const char* label, bool defaultOpen = true)
{
    ImGui::PushStyleColor(ImGuiCol_Header,        {1, 1, 1, 0.03f});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {1, 1, 1, 0.08f});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  {1, 1, 1, 0.12f});
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextHi);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;
    if (defaultOpen) f |= ImGuiTreeNodeFlags_DefaultOpen;
    const bool open = ImGui::CollapsingHeader(label, f);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return open;
}
