#include "icons.hpp"

#include "presentation.hpp"

#include <algorithm>

namespace ui::icons {

namespace {

// Dark outline drawn around filled glyphs so they read on light terrain.
constexpr ImU32 outline = IM_COL32(20, 22, 28, 255);

void diamond(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    const ImVec2 v[4] = { {c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y} };
    dl->AddConvexPolyFilled(v, 4, fill);
    dl->AddPolyline(v, 4, outline, ImDrawFlags_Closed, 1.0f);
}

void square(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    dl->AddRectFilled({c.x - r, c.y - r}, {c.x + r, c.y + r}, fill);
    dl->AddRect({c.x - r, c.y - r}, {c.x + r, c.y + r}, outline, 0.0f, 0, 1.0f);
}

void triangle(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    const ImVec2 v[3] = { {c.x, c.y - r}, {c.x + r, c.y + r}, {c.x - r, c.y + r} };
    dl->AddConvexPolyFilled(v, 3, fill);
    dl->AddPolyline(v, 3, outline, ImDrawFlags_Closed, 1.0f);
}

} // namespace

void building(ImDrawList* dl, ImVec2 centre, float r, building_type type, ImU32 fill)
{
    switch (type)
    {
        case building_type::extraction_site:     diamond(dl, centre, r, fill); break;
        case building_type::processing_facility: square(dl, centre, r, fill);  break;
        case building_type::port:                triangle(dl, centre, r, fill); break;
        default:                                 dl->AddCircleFilled(centre, r, fill); break;
    }
}

void resource(ImDrawList* dl, ImVec2 centre, float r, resource_type res)
{
    diamond(dl, centre, r, presentation_of(res).colour);
}

void unit(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    const ImVec2 v[3] = { {centre.x, centre.y - r}, {centre.x + r, centre.y + r}, {centre.x - r, centre.y + r} };
    dl->AddConvexPolyFilled(v, 3, colour);
}

void ledger(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    const ImVec2 a = { centre.x - r, centre.y - r };
    const ImVec2 b = { centre.x + r, centre.y + r };
    dl->AddRect(a, b, colour, 2.0f, 0, 1.5f);
    // Two ruled rows so it reads as a table rather than a plain box.
    dl->AddLine({ a.x, centre.y - r * 0.33f }, { b.x, centre.y - r * 0.33f }, colour, 1.0f);
    dl->AddLine({ a.x, centre.y + r * 0.33f }, { b.x, centre.y + r * 0.33f }, colour, 1.0f);
}

void placeholder(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    dl->AddRect({ centre.x - r, centre.y - r }, { centre.x + r, centre.y + r }, colour, 2.0f, 0, 1.5f);
}

void supply(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Two parallel horizontal lines — a route / convoy shorthand.
    const float off = r * 0.45f;
    dl->AddLine({ centre.x - r, centre.y - off }, { centre.x + r, centre.y - off }, colour, 1.5f);
    dl->AddLine({ centre.x - r, centre.y + off }, { centre.x + r, centre.y + off }, colour, 1.5f);
}

void market(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Three ascending bars, outlined, sharing a common baseline — a price chart.
    const float base = centre.y + r;
    const float bw   = r * 0.5f;          // bar width
    const float gap  = r * 0.15f;         // gap between bars
    const float x0   = centre.x - r;
    const float heights[3] = { r * 0.7f, r * 1.2f, r * 1.7f };
    for (int i = 0; i < 3; ++i)
    {
        const float x = x0 + static_cast<float>(i) * (bw + gap);
        dl->AddRect({ x, base - heights[i] }, { x + bw, base }, colour, 0.0f, 0, 1.5f);
    }
}

void faction(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Downward-pointing shield silhouette: flat top, square shoulders, point at
    // the bottom. Filled with a thin dark outline for contrast on any background.
    const ImVec2 v[5] = {
        { centre.x - r, centre.y - r },          // top-left
        { centre.x + r, centre.y - r },          // top-right
        { centre.x + r, centre.y + r * 0.2f },   // right shoulder
        { centre.x,     centre.y + r },          // bottom point
        { centre.x - r, centre.y + r * 0.2f },   // left shoulder
    };
    dl->AddConvexPolyFilled(v, 5, colour);
    dl->AddPolyline(v, 5, outline, ImDrawFlags_Closed, 1.0f);
}

void corporation(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Filled square (reuses the building/processing square + outline), with a
    // centred dark inner dot punched into it so it reads as a "seal" — distinct
    // from the plain processing-facility square that has no dot.
    square(dl, centre, r, colour);
    dl->AddCircleFilled(centre, std::max(1.0f, r * 0.35f), outline);
}

} // namespace ui::icons
