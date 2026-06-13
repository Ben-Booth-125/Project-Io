#include "highlight.hpp"

#include "presentation.hpp"

namespace ui {

namespace {

// Ring offset outside the marker and stroke widths per state. Fixed pixels (not
// zoom-scaled) so highlights keep a constant footprint, matching the existing
// body/tile outline treatment.
constexpr float ring_gap           = 3.0f;
constexpr float selected_thickness = 1.5f;
constexpr float pinned_thickness   = 2.0f;
constexpr float hover_thickness    = 1.5f;

struct ring_style
{
    ImU32 colour;
    float thickness;
};

ring_style style_for(highlight h)
{
    switch (h)
    {
        case highlight::selected: return { palette::selection, selected_thickness };
        case highlight::pinned:   return { palette::pinned,    pinned_thickness };
        case highlight::hovered:  return { palette::hover,     hover_thickness };
        default:                  return { 0u, 0.0f };
    }
}

} // namespace

highlight resolve_highlight(bool selected, bool hovered, bool pinned)
{
    if (selected) return highlight::selected;
    if (pinned)   return highlight::pinned;
    if (hovered)  return highlight::hovered;
    return highlight::none;
}

void draw_body_highlight(ImDrawList* dl, ImVec2 centre, float body_radius, highlight h)
{
    if (h == highlight::none)
        return;
    const ring_style s = style_for(h);
    dl->AddCircle(centre, body_radius + ring_gap, s.colour, 0, s.thickness);
}

void draw_hex_highlight(ImDrawList* dl, const ImVec2 verts[6], highlight h)
{
    if (h == highlight::none)
        return;
    const ring_style s = style_for(h);
    dl->AddPolyline(verts, 6, s.colour, ImDrawFlags_Closed, s.thickness);
}

} // namespace ui
