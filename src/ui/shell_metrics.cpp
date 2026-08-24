#include "shell_metrics.hpp"

#include "nav_pane.hpp"      // nav_pane_width
#include "profile_panel.hpp" // profile_panel_height

#include <algorithm>

namespace ui {

float right_chrome_width(ImVec2 disp)
{
    return minimap_width(disp.x, disp.y);
}

float right_chrome_left(ImVec2 disp)
{
    // FLUSH to the right screen edge (BL-599/BL-600, Ben 2026-08-24). It used to
    // subtract `shell_margin` as well, which left an 8px strip of live canvas
    // between the Selection band's right edge and the minimap's left edge -- the
    // minimap alone was already flush, so the column and its own foot disagreed by
    // exactly one margin and the map showed through the seam. Measured on the
    // 2026-08-24 shell captures: ocean hexes at x = 935..943, y = 600.
    //
    // Every other edge of the shell was ALREADY flush -- profile tile and header at
    // y = 0, nav rail and comms dock at x = 0, comms dock at y = 719 -- so this is
    // the right column joining a rule the rest of the shell already followed, not a
    // new rule being invented for it.
    return disp.x - right_chrome_width(disp);
}

shell_rect minimap_rect(ImVec2 disp)
{
    const float w = right_chrome_width(disp);
    // Flush right (BL-312) and now flush BOTTOM too (BL-599). The bottom used to
    // keep `chrome_margin`, which floated the box four pixels clear of the screen
    // foot with canvas visible underneath it (measured: row 716 is background, rows
    // 717-719 are hexes).
    //
    // The margin is ABSORBED rather than deleted: the box grows by it instead of
    // sitting above it, so the top edge does not move. That matters because
    // `selection_band_height` is `minimap_height + chrome_margin` for the express
    // purpose of landing the bottom strip's top edge on this rect's top edge, and
    // that alignment is what makes the screen's foot read as one band. Height here
    // is therefore `selection_band_height`, which is the same number said in the
    // terms that explain why.
    const float h = selection_band_height(disp.x, disp.y);
    return { right_chrome_left(disp), disp.y - h, w, h };
}

shell_rect time_panel_rect(ImVec2 disp, float time_h)
{
    // Flush to the TOP screen edge (BL-600). The profile tile and the header have
    // always started at y = 0; the time panel started at y = 8, so the top band was
    // ragged along an edge two of its three tiles already shared.
    return { right_chrome_left(disp), 0.0f, right_chrome_width(disp), time_h };
}

float bottom_band_budget(ImVec2 disp)
{
    return std::max(0.0f, right_chrome_left(disp) - nav_pane_width);
}

shell_rect selection_band_rect(ImVec2 disp)
{
    const foldout_rect dock  = comms_dock_rect();
    const float        left  = dock.x + dock.w;      // flush against the dock, no gutter
    const float        right = right_chrome_left(disp);
    const float        h     = selection_band_height(disp.x, disp.y);
    return { left, disp.y - h, std::max(0.0f, right - left), h };
}

shell_rect pinned_panel_rect(ImVec2 disp, float time_h)
{
    // Below the time panel, above the minimap — one margin clear of each. The
    // minimap's TOP edge is the floor, not its rect origin plus anything: the lens
    // bar lives inside the minimap box.
    // The time panel now starts at y = 0 (BL-600), so this is one margin below its
    // foot rather than two below the screen's.
    const float y = time_h + shell_margin;
    const float h = minimap_rect(disp).y - shell_margin - y;
    return { right_chrome_left(disp), y, right_chrome_width(disp), std::max(0.0f, h) };
}

} // namespace ui
