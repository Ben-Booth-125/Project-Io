#include "shell_metrics.hpp"

#include "nav_pane.hpp"      // nav_pane_width
#include "profile_panel.hpp" // profile_panel_height

#include <algorithm>
#include <cmath>

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
    //
    // ROUNDED to a whole pixel (Ben, 2026-11-22: the band "should expand so it covers the
    // edges of the canvas, and the edge of the mini map"). The minimap width is fractional
    // at most sizes and ImGui truncates a window's position and size, so the band ended
    // one pixel short of the minimap and the map showed through the seam. Every occupant
    // of the column, and the band's right edge, reads this one rounded figure.
    return std::round(disp.x - right_chrome_width(disp));
}

shell_rect minimap_rect(ImVec2 disp)
{
    const float x = right_chrome_left(disp);
    const float w = disp.x - x; // to the screen edge exactly, from the rounded left edge
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
    return { x, disp.y - h, w, h };
}

shell_rect time_panel_rect(ImVec2 disp, float time_h)
{
    // Flush to the TOP screen edge (BL-600). The profile tile and the header have
    // always started at y = 0; the time panel started at y = 8, so the top band was
    // ragged along an edge two of its three tiles already shared.
    const float x = right_chrome_left(disp);
    return { x, 0.0f, disp.x - x, time_h };
}

shell_rect selection_band_rect(ImVec2 disp)
{
    // From the shell column's right edge (Ben, 2026-11-19): the comms dock that sat
    // left of the band retired, and the ledger column runs to the screen's bottom.
    const float        left  = shell_column_width(disp.x); // flush against the column, no gutter
    const float        right = right_chrome_left(disp);
    const float        h     = selection_band_height(disp.x, disp.y);
    return { left, disp.y - h, std::max(0.0f, right - left), h };
}

shell_rect lens_chrome_rect(ImVec2 disp, float time_h, float want_h)
{
    const shell_rect mini = minimap_rect(disp);
    // The FLOOR is the minimap's own top edge, with no gutter: the region is the
    // minimap's header, not a separate panel hovering above it. The shell carries no
    // internal gutter anywhere else in this column either (see selection_band_rect).
    const float floor_y   = mini.y;
    const float ceiling_y = (time_h > 0.0f) ? (time_h + shell_margin) : shell_margin;
    const float avail     = std::max(0.0f, floor_y - ceiling_y);
    const float h         = std::min(std::max(0.0f, want_h), avail);
    return { mini.x, floor_y - h, mini.w, h };
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

namespace {
shell_rect g_lens_chrome{};
int        g_lens_chrome_frame = -1;
} // namespace

void note_lens_chrome(const shell_rect& r)
{
    g_lens_chrome       = r;
    g_lens_chrome_frame = ImGui::GetFrameCount();
}

bool lens_chrome_this_frame(shell_rect& out)
{
    if (g_lens_chrome_frame != ImGui::GetFrameCount())
        return false;
    out = g_lens_chrome;
    return true;
}

} // namespace ui
