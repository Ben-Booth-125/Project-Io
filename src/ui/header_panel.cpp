#include "header_panel.hpp"

#include <imgui.h>

namespace ui {

void draw_header_panel(float left, float right)
{
    constexpr float margin = 8.0f;
    const float     width  = right - left;
    if (width <= 0.0f)
        return; // window too narrow to show the strip

    ImGui::SetNextWindowPos({left, margin});
    ImGui::SetNextWindowSize({width, header_panel_height});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##header_panel", nullptr, flags);

    // Budget on the left. Placeholder zeroes until the treasury is wired.
    ImGui::TextDisabled("BUDGET");
    ImGui::SameLine();
    ImGui::Text("Cr 0");     // treasury balance (Cr = credits placeholder)
    ImGui::SameLine();
    ImGui::TextDisabled("(±0 / econ tick)"); // net per economy tick

    // Resource overview — single aggregate until resources are wired.
    ImGui::SameLine();
    ImGui::TextDisabled("   |   ");
    ImGui::SameLine();
    ImGui::TextDisabled("STOCKPILE");
    ImGui::SameLine();
    ImGui::Text("0"); // total stockpile placeholder

    ImGui::End();
}

} // namespace ui
