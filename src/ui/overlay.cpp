#include "overlay.hpp"

#include "nav_pane.hpp"

namespace ui {

namespace {

// Lens accent colour, used for the legend swatch and (later) the lens drawing
// itself. Distinct from the resource/faction palettes — these key the overlay
// modes, not entities.
ImU32 overlay_accent(overlay_mode m)
{
    switch (m)
    {
        case overlay_mode::supply:  return IM_COL32(110, 190, 255, 255); // convoy blue
        case overlay_mode::market:  return IM_COL32(120, 205, 140, 255); // market green
        case overlay_mode::faction: return IM_COL32(190, 140, 225, 255); // faction violet
        default:                    return IM_COL32(170, 175, 185, 255);
    }
}

} // namespace

const char* overlay_mode_name(overlay_mode m)
{
    switch (m)
    {
        case overlay_mode::supply:  return "Supply routes";
        case overlay_mode::market:  return "Market";
        case overlay_mode::faction: return "Faction presence";
        default:                    return "None";
    }
}

void toggle_overlay(ui_state& ui, overlay_mode m)
{
    ui.overlay = (ui.overlay == m) ? overlay_mode::none : m;
}

void draw_canvas_overlay(const world& w, const ui_state& ui, canvas_level level,
                         ImVec2 origin, ImVec2 size, ImDrawList* dl)
{
    (void)w;     // overlay data source — used once lenses draw real geometry
    (void)level; // lenses differ per rung — switched on here by later layers

    if (ui.overlay == overlay_mode::none)
        return;

    // Legend chip — the only overlay element in Layer 2. Anchored bottom-left of
    // the canvas, clear of the nav pane (left), the solar scale bar (bottom
    // centre), and the minimap (bottom right). Later layers draw the lens itself
    // over the canvas above this chip.
    const ImU32  accent = overlay_accent(ui.overlay);
    const char*  label  = overlay_mode_name(ui.overlay);
    const float  pad    = 6.0f;
    const float  sw     = ImGui::GetTextLineHeight();
    const ImVec2 tsz    = ImGui::CalcTextSize(label);
    const float  chip_h = sw + pad * 2.0f;
    const float  chip_w = sw + pad * 3.0f + tsz.x;

    const ImVec2 p0 = { origin.x + nav_pane_width + 12.0f,
                        origin.y + size.y - chip_h - 12.0f };
    const ImVec2 p1 = { p0.x + chip_w, p0.y + chip_h };

    dl->AddRectFilled(p0, p1, IM_COL32(18, 20, 28, 220), 4.0f);
    dl->AddRect(p0, p1, IM_COL32(70, 75, 90, 255), 4.0f);

    const ImVec2 swp = { p0.x + pad, p0.y + pad };
    dl->AddRectFilled(swp, { swp.x + sw, swp.y + sw }, accent, 2.0f);
    dl->AddText({ swp.x + sw + pad, p0.y + (chip_h - tsz.y) * 0.5f },
                IM_COL32(225, 228, 235, 255), label);
}

} // namespace ui
