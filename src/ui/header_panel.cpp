#include "header_panel.hpp"

#include "format.hpp"

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

    // Budget on the left. Placeholder zeroes until the treasury is wired, but
    // routed through the shared formatters so the live numbers drop straight in.
    ImGui::TextDisabled("BUDGET");
    ImGui::SameLine();
    ImGui::Text("%s", fmt::credits(0.0).c_str());     // treasury balance
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", fmt::rate(0.0, "qtr").c_str()); // net per economy tick

    // Resource overview — single aggregate until resources are wired.
    ImGui::SameLine();
    ImGui::TextDisabled("   |   ");
    ImGui::SameLine();
    ImGui::TextDisabled("STOCKPILE");
    ImGui::SameLine();
    ImGui::Text("%s", fmt::abbreviate(0.0).c_str()); // total stockpile placeholder

    ImGui::End();
}

} // namespace ui
