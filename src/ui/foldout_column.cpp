#include "foldout_column.hpp"

#include "nav_pane.hpp"      // nav_pane_width
#include "profile_panel.hpp" // profile_panel_height

#include <imgui.h>

#include <initializer_list>

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

// The tabbed-ledger header switch renders at this font scale — ~2x the body text so the
// tabs read as the ledger's primary control, not a footnote (this session's steer).
constexpr float k_header_tab_scale = 2.0f;

void nav_button_strip(std::initializer_list<const char*> labels, int& view, bool* close)
{
    const int count = static_cast<int>(labels.size());
    if (count <= 0)
        return;

    const ImGuiStyle& style   = ImGui::GetStyle();
    const float       avail   = ImGui::GetContentRegionAvail().x;
    const float       spacing = style.ItemSpacing.x;
    // Divide the full header width evenly between the tabs so the strip spans the header.
    // Floor to a whole pixel so rounding can only leave the row a hair short — never
    // overflow into a wrap (SameLine buttons would spill to a second line).
    float btn_w = (avail - spacing * static_cast<float>(count - 1)) / static_cast<float>(count);
    btn_w = static_cast<float>(static_cast<int>(btn_w));
    if (btn_w < 1.0f)
        btn_w = 1.0f;

    ImGui::SetWindowFontScale(k_header_tab_scale);
    int id = 0;
    for (const char* label : labels)
    {
        if (id > 0)
            ImGui::SameLine();
        const bool active = (view == id);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label, {btn_w, 0.0f}))
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
        ++id;
    }
    ImGui::SetWindowFontScale(1.0f);
}

} // namespace ui
