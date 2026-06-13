#include "icons.hpp"

#include "presentation.hpp"

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

} // namespace ui::icons
