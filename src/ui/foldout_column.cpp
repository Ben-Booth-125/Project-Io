#include "foldout_column.hpp"

#include "nav_pane.hpp"      // nav_pane_width
#include "profile_panel.hpp" // profile_panel_height

#include <imgui.h>

namespace ui {

float shell_column_width(float disp_x)
{
    // ~1.6x the original 0.17/[300,360] band (widened for the ledger + Selection
    // content that now shares this column); still resolution-scaled. ~480 @1720,
    // ~522 @1920.
    float w = 0.272f * disp_x;
    if (w < 480.0f) w = 480.0f;
    if (w > 576.0f) w = 576.0f;
    // Round to a whole pixel so the column edge (and everything anchored to it) lands
    // on a pixel boundary rather than blurring across two.
    return static_cast<float>(static_cast<int>(w + 0.5f));
}

foldout_rect foldout_column_rect()
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float  W    = shell_column_width(disp.x);
    return {
        nav_pane_width,                       // x: right of the icon rail
        profile_panel_height,                 // y: below the identity tile
        W - nav_pane_width,                   // w: rail edge -> column edge
        disp.y - profile_panel_height,        // h: to the bottom edge — flush with the
                                              //    nav rail (same top and bottom, so the
                                              //    menu and its fold-out items are equal
                                              //    height, no gap below the ledger)
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

void nav_button(const char* label, int id, int& view, bool* close)
{
    const bool active = (view == id);
    if (active)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(label))
    {
        // BL-126 toggle rule: re-clicking the active tab closes the hosting ledger (clears
        // its show_* flag); clicking a different tab is an ordinary view change. When no
        // close target is supplied, the active re-click stays a no-op.
        if (active && close)
            *close = false;
        else
            view = id;
    }
    if (active)
        ImGui::PopStyleColor();
}

} // namespace ui
