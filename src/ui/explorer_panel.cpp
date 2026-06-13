#include "explorer_panel.hpp"

#include <imgui.h>

namespace ui {

void draw_explorer_panel(float x, float y, float width, float height)
{
    if (height <= 0.0f)
        return; // no vertical room between the time column and the minimap

    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({width, height});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##explorer_panel", nullptr, flags);

    ImGui::TextDisabled("EXPLORER");
    ImGui::Separator();
    ImGui::Spacing();

    // Pinned items live here once pinning is wired. Empty placeholder for now.
    ImGui::TextDisabled("No pinned items.");
    ImGui::TextDisabled("Pin units, bodies, and");
    ImGui::TextDisabled("markets to jump to them.");

    ImGui::End();
}

} // namespace ui
