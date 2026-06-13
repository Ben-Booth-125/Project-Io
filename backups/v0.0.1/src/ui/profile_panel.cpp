#include "profile_panel.hpp"

#include <imgui.h>

#include "nav_pane.hpp" // nav_pane_width — the profile aligns to the pane

namespace ui {

void draw_profile_panel()
{
    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({nav_pane_width, profile_panel_height});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##profile_panel", nullptr, flags);

    // Portrait placeholder: a filled square to the left, with the corporation
    // name and basic standing stacked beside it. A real emblem/picture replaces
    // the square later.
    constexpr float portrait = 56.0f;
    const ImVec2    p0       = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        p0, {p0.x + portrait, p0.y + portrait}, IM_COL32(60, 70, 90, 255));
    ImGui::GetWindowDrawList()->AddRect(
        p0, {p0.x + portrait, p0.y + portrait}, IM_COL32(110, 120, 140, 255));

    ImGui::SameLine(portrait + ImGui::GetStyle().ItemSpacing.x * 2.0f);
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Unnamed Corp"); // placeholder corporation name
    ImGui::TextDisabled("Parent: —");  // parent nation, TBD
    ImGui::TextDisabled("Standing: —"); // headline standing, TBD
    ImGui::EndGroup();

    ImGui::End();
}

} // namespace ui
