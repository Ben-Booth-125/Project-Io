#include "foldout_column.hpp"

#include "header_panel.hpp"  // header_panel_height
#include "nav_pane.hpp"      // nav_pane_width
#include "profile_panel.hpp" // profile_panel_height
#include "selection_card.hpp" // selection_band_height (the comms dock shares it)

#include <imgui.h>

namespace ui {

float shell_column_width(float disp_x)
{
    // Narrowed back down (BL-213, 2026-07-28): the BL-124 widening to
    // ~1.6x/[480,576] was explicitly so this column could host the Selection
    // element as a sidebar. Selection moved out to its own fixed bottom band
    // (BL-213) and no longer lives here at all, so that justification is gone —
    // Ben asked to widen the band, and the column no longer needs the extra
    // width the ledgers alone were re-tuned for at a narrower size (LAYOUT.md's
    // BL-081/111/117 legibility passes were themselves tuned against a ~244px
    // effective content width, well inside this new range). The floor binds across
    // the whole common range: 380 @1280..1900, 384 @1920, ~410 @2048 (BL-216 — the
    // "~410 @1920" this comment used to claim was arithmetically wrong; 0.20*1920
    // is 384).
    float w = 0.20f * disp_x;
    if (w < 380.0f) w = 380.0f;
    if (w > 460.0f) w = 460.0f;
    // Round to a whole pixel so the column edge (and everything anchored to it) lands
    // on a pixel boundary rather than blurring across two.
    return static_cast<float>(static_cast<int>(w + 0.5f));
}

float minimap_width(float disp_x, float disp_y)
{
    const float by_display = 0.28f * ((disp_x < disp_y) ? disp_x : disp_y);
    return (by_display > 336.0f) ? by_display : 336.0f;
}

float minimap_height(float disp_x, float disp_y)
{
    return minimap_width(disp_x, disp_y) * 0.75f;
}

float selection_band_height(float disp_x, float disp_y)
{
    // Top edge flush with the minimap's: the minimap floats `chrome_margin` off
    // the bottom, the strip does not, so the strip is exactly that much taller.
    return minimap_height(disp_x, disp_y) + chrome_margin;
}

foldout_rect foldout_column_rect()
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float  W    = shell_column_width(disp.x);
    return {
        nav_pane_width,                       // x: right of the icon rail
        profile_panel_height,                 // y: below the identity tile
        W - nav_pane_width,                   // w: rail edge -> column edge
        // h: down to the TOP OF THE COMMS DOCK, not the bottom margin (BL-227).
        //    The dock owns the bottom-left tile of the screen's bottom strip, so
        //    every menu and ledger in this column is now permanently shorter by
        //    exactly the Selection band's height. Ben's call, 2026-07-30.
        std::max(0.0f, disp.y - profile_panel_height - selection_band_height(disp.x, disp.y)),
    };
}

foldout_rect comms_dock_rect()
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float  W    = shell_column_width(disp.x);
    const float  h    = selection_band_height(disp.x, disp.y);
    // Three quarters of the fold-out column's width (Ben, 2026-07-30). Comms is
    // ambient chatter, not a decision surface, so it does not need to hold the
    // full column width down here — the quarter it gives back goes to the
    // Selection band, which starts at this dock's right edge rather than at the
    // shell column edge, so the bottom strip stays SOLID with no canvas sliver
    // punched between the two.
    return {
        nav_pane_width,                            // x: right of the icon rail
        disp.y - h,                                // y: shares the Selection band's top edge
        (W - nav_pane_width) * 0.75f,              // w: three quarters of the column
        h,                                         // h: identical to the band, by design
    };
}

foldout_rect canvas_rect()
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float  x    = shell_column_width(disp.x);
    return {
        x,                                                                        // x: right of the shell column
        header_panel_height,                                                     // y: below the header strip
        std::max(0.0f, disp.x - x),                                              // w: to the screen edge (minimap floats inside)
        std::max(0.0f, disp.y - header_panel_height - selection_band_height(disp.x, disp.y)), // h: above the bottom strip
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
