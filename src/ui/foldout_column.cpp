#include "foldout_column.hpp"

#include "nav_pane.hpp"      // nav_pane_width
#include "profile_panel.hpp" // profile_panel_height

#include <imgui.h>

namespace ui {

namespace {
/// Shared shell margin — matches the 8px margin app.cpp lays the chrome out on.
constexpr float shell_margin = 8.0f;
} // namespace

float shell_column_width(float disp_x)
{
    float w = 0.17f * disp_x;
    if (w < 300.0f) w = 300.0f;
    if (w > 360.0f) w = 360.0f;
    // Round to a whole pixel so the column edge (and everything anchored to it) lands
    // on a pixel boundary rather than blurring across two.
    return static_cast<float>(static_cast<int>(w + 0.5f));
}

foldout_rect foldout_column_rect()
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float  W    = shell_column_width(disp.x);
    return {
        nav_pane_width,                                 // x: right of the icon rail
        profile_panel_height,                           // y: below the identity tile
        W - nav_pane_width,                             // w: rail edge -> column edge
        disp.y - profile_panel_height - shell_margin,   // h: down to the bottom margin
    };
}

bool foldout_begin(const char* name)
{
    const foldout_rect r = foldout_column_rect();
    ImGui::SetNextWindowPos({r.x, r.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({r.w, r.h}, ImGuiCond_Always);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    return ImGui::Begin(name, nullptr, flags);
}

void foldout_end()
{
    ImGui::End();
}

} // namespace ui
