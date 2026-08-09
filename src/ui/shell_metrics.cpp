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
    return disp.x - shell_margin - right_chrome_width(disp);
}

shell_rect minimap_rect(ImVec2 disp)
{
    const float w = right_chrome_width(disp);
    const float h = minimap_height(disp.x, disp.y);
    // Right edge flush to the screen edge (BL-312) — an 8px gap with no neighbour
    // to its right to justify it. The bottom keeps `chrome_margin`, which is what
    // makes `selection_band_height` (= minimap_height + chrome_margin) land the
    // bottom strip's top edge exactly here.
    return { disp.x - w, disp.y - chrome_margin - h, w, h };
}

shell_rect time_panel_rect(ImVec2 disp, float time_h)
{
    return { right_chrome_left(disp), shell_margin, right_chrome_width(disp), time_h };
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
    const float y = shell_margin + time_h + shell_margin;
    const float h = minimap_rect(disp).y - shell_margin - y;
    return { right_chrome_left(disp), y, right_chrome_width(disp), std::max(0.0f, h) };
}

} // namespace ui
