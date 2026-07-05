#include "icons.hpp"

#include "presentation.hpp"

#include <algorithm>
#include <cmath>

namespace ui::icons {

namespace {

// Dark outline drawn around filled glyphs so they read on light terrain.
constexpr ImU32 outline = IM_COL32(20, 22, 28, 255);

void diamond(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    const ImVec2 v[4] = { {c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y} };
    dl->AddConvexPolyFilled(v, 4, fill);
}

// Faceted ore/mineral silhouette for extraction-site — an angular eight-sided
// crystal chunk, wider than tall, with cuts at all four corners. Distinct from
// the gem-diamond pip (a regular 4-point diamond) and the port triangle.
void ore_chunk(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    const float rx = r;           // horizontal half-extent
    const float ry = r * 0.72f;  // vertical half-extent (flatter than a diamond)
    const float cx = rx * 0.45f; // corner cut size (horizontal)
    const float cy = ry * 0.45f; // corner cut size (vertical)
    const ImVec2 v[8] = {
        { c.x - rx + cx, c.y - ry },   // top-left
        { c.x + rx - cx, c.y - ry },   // top-right
        { c.x + rx,      c.y - ry + cy }, // right-top facet
        { c.x + rx,      c.y + ry - cy }, // right-bottom facet
        { c.x + rx - cx, c.y + ry },   // bottom-right
        { c.x - rx + cx, c.y + ry },   // bottom-left
        { c.x - rx,      c.y + ry - cy }, // left-bottom facet
        { c.x - rx,      c.y - ry + cy }, // left-top facet
    };
    dl->AddConvexPolyFilled(v, 8, fill);
    dl->AddPolyline(v, 8, outline, ImDrawFlags_Closed, 1.0f);
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
        case building_type::extraction_site:     ore_chunk(dl, centre, r, fill); break;
        case building_type::processing_facility: square(dl, centre, r, fill);    break;
        case building_type::port:                triangle(dl, centre, r, fill);  break;
        default:
            dl->AddCircleFilled(centre, r, fill);
            dl->AddCircle(centre, r, outline, 0, 1.0f);
            break;
    }
}

void resource(ImDrawList* dl, ImVec2 centre, float r, resource_type res)
{
    diamond(dl, centre, r, presentation_of(res).colour);
}

void unit(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Open upward chevron (V): two lines meeting at a bottom point, open at top.
    // Stroke-only so it is unambiguously distinct from the filled port triangle.
    const ImVec2 v[3] = {
        { centre.x - r, centre.y - r * 0.5f },  // top-left arm
        { centre.x,     centre.y + r },          // bottom point
        { centre.x + r, centre.y - r * 0.5f },  // top-right arm
    };
    dl->AddPolyline(v, 3, outline, ImDrawFlags_None, 2.0f);
    dl->AddPolyline(v, 3, colour,  ImDrawFlags_None, 1.5f);
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

void convoy(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    const float hw = r * 0.55f;
    const float hh = r * 0.70f;
    dl->AddLine({centre.x - hw, centre.y - hh}, {centre.x + hw, centre.y}, colour, 2.0f);
    dl->AddLine({centre.x + hw, centre.y},       {centre.x - hw, centre.y + hh}, colour, 2.0f);
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

void country(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
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

void resource(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Three stacked horizontal strata, widening top-to-bottom and deepening in
    // opacity — a gradient / deposit-density motif. Distinct from the supply
    // pair of thin full-width rules, the market vertical bars, and the resource
    // pip diamond. The fill alpha is scaled per stratum to read as a gradient.
    const float gap = r * 0.18f;          // vertical gap between strata
    const float th  = r * 0.42f;          // stratum thickness
    const float widths[3]  = { r * 0.45f, r * 0.72f, r };          // top → bottom
    const float alphas[3]  = { 0.40f, 0.70f, 1.00f };              // top → bottom
    const float top = centre.y - r;
    for (int i = 0; i < 3; ++i)
    {
        const float y0 = top + static_cast<float>(i) * (th + gap);
        const float w  = widths[i];
        const ImU32 c  = (colour & 0x00FFFFFFu)
                       | (static_cast<ImU32>(((colour >> 24) & 0xFFu) * alphas[i]) << 24);
        dl->AddRectFilled({ centre.x - w, y0 }, { centre.x + w, y0 + th }, c, th * 0.4f);
    }
}

void population(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // A small figure: round head over a tapered torso (shoulders wider than the
    // head, narrowing toward a flat base) — a "people / habitability" motif.
    const float head_r = r * 0.42f;
    const ImVec2 head_c = { centre.x, centre.y - r * 0.45f };
    dl->AddCircleFilled(head_c, head_r, colour);
    const ImVec2 torso[4] = {
        { centre.x - r * 0.30f, centre.y - r * 0.05f }, // left shoulder
        { centre.x + r * 0.30f, centre.y - r * 0.05f }, // right shoulder
        { centre.x + r * 0.55f, centre.y + r },         // right base
        { centre.x - r * 0.55f, centre.y + r },         // left base
    };
    dl->AddConvexPolyFilled(torso, 4, colour);
}

void scarcity(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // A hollow downward-pointing triangle — an "empty / depleted" motif, the
    // inverse of the filled resource pip. Stroke only so it reads as absence.
    const ImVec2 v[3] = {
        { centre.x - r, centre.y - r },   // top-left
        { centre.x + r, centre.y - r },   // top-right
        { centre.x,     centre.y + r },   // bottom point
    };
    dl->AddPolyline(v, 3, colour, ImDrawFlags_Closed, 1.5f);
}

void opportunity(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // An open circle with an inner "+" — a "potential gain / margin" motif: where
    // value *could* be made. Stroke only, distinct from the filled corporation
    // seal-square and the resource strata.
    dl->AddCircle(centre, r, colour, 0, 1.5f);
    const float a = r * 0.5f;
    dl->AddLine({ centre.x - a, centre.y }, { centre.x + a, centre.y }, colour, 1.5f);
    dl->AddLine({ centre.x, centre.y - a }, { centre.x, centre.y + a }, colour, 1.5f);
}

void production(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // A filled upward-pointing triangle over a short baseline — an "output /
    // throughput rising" motif. Distinct from the market vertical bars and the
    // scarcity hollow down-triangle (this one is filled and points up).
    const ImVec2 v[3] = {
        { centre.x,         centre.y - r },        // apex
        { centre.x + r,     centre.y + r * 0.5f }, // bottom-right
        { centre.x - r,     centre.y + r * 0.5f }, // bottom-left
    };
    dl->AddConvexPolyFilled(v, 3, colour);
    dl->AddLine({ centre.x - r, centre.y + r }, { centre.x + r, centre.y + r }, colour, 1.5f);
}

void industry(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Factory: a body block with a two-tooth sawtooth roof and a chimney on the
    // left. Reads as "industry", distinct from the production up-triangle.
    const float bw = r;               // body half-width
    const float top = centre.y - r * 0.15f;
    const float bot = centre.y + r * 0.75f;
    // Main body.
    dl->AddRectFilled({ centre.x - bw, top }, { centre.x + bw, bot }, colour);
    dl->AddRect({ centre.x - bw, top }, { centre.x + bw, bot }, outline, 0.0f, 0, 1.0f);
    // Two sawtooth roof teeth over the right two-thirds.
    const float rt = r * 0.55f;       // tooth height
    const ImVec2 t1[3] = { { centre.x - bw * 0.2f, top }, { centre.x + bw * 0.3f, top }, { centre.x - bw * 0.2f, top - rt } };
    const ImVec2 t2[3] = { { centre.x + bw * 0.3f, top }, { centre.x + bw * 0.8f, top }, { centre.x + bw * 0.3f, top - rt } };
    dl->AddTriangleFilled(t1[0], t1[1], t1[2], colour);
    dl->AddTriangleFilled(t2[0], t2[1], t2[2], colour);
    // Chimney on the left, rising above the roofline.
    dl->AddRectFilled({ centre.x - bw * 0.85f, centre.y - r }, { centre.x - bw * 0.5f, top }, colour);
}

void market_centre(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Circle outline.
    dl->AddCircle(centre, r, colour, 0, 1.5f);
    // Centred cross, arms reaching to 60 % of radius.
    const float arm = r * 0.6f;
    dl->AddLine({ centre.x - arm, centre.y }, { centre.x + arm, centre.y }, colour, 1.5f);
    dl->AddLine({ centre.x, centre.y - arm }, { centre.x, centre.y + arm }, colour, 1.5f);
}

void settlement(ImDrawList* dl, ImVec2 centre, float r, int tier, ImU32 colour)
{
    // A small skyline: `bars` towers sitting on a baseline, the middle tallest, the
    // count rising with tier so an outpost reads as a lone tower and a metropolis as
    // a dense cluster. Tier drives complexity; colour stays civic-neutral.
    const int bars = std::max(1, std::min(tier, 5));
    const float baseline = centre.y + r * 0.85f;
    const float bw       = (2.0f * r) / (bars * 1.55f);   // tower width incl. a gap allowance
    const float gap      = bw * 0.55f;
    const float span     = bars * bw + (bars - 1) * gap;
    float x = centre.x - span * 0.5f;
    const float mid = (bars - 1) * 0.5f;
    for (int i = 0; i < bars; ++i)
    {
        // Height peaks at the middle tower and tapers to the edges.
        const float t = 1.0f - (mid > 0.0f ? std::abs(i - mid) / (mid + 0.6f) : 0.0f);
        const float h = r * (0.7f + 1.05f * t);
        const ImVec2 a{ x, baseline - h };
        const ImVec2 b{ x + bw, baseline };
        dl->AddRectFilled(a, b, colour);
        dl->AddRect(a, b, outline, 0.0f, 0, 1.0f);
        x += bw + gap;
    }
}

void hq(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Eight-point star = a diamond (N/E/S/W points) overlaid with an axis-aligned
    // square (the NE/SE/SW/NW points), ringed. Reads as a "seat of power" distinct
    // from the building/market/settlement glyphs.
    const ImVec2 d[4] = {
        { centre.x, centre.y - r }, { centre.x + r, centre.y },
        { centre.x, centre.y + r }, { centre.x - r, centre.y },
    };
    dl->AddConvexPolyFilled(d, 4, colour);
    const float s = r * 0.62f;
    const ImVec2 sq[4] = {
        { centre.x - s, centre.y - s }, { centre.x + s, centre.y - s },
        { centre.x + s, centre.y + s }, { centre.x - s, centre.y + s },
    };
    dl->AddConvexPolyFilled(sq, 4, colour);
    dl->AddCircle(centre, r * 1.35f, colour, 0, 1.6f);
    // Dark centre dot so the star reads against a same-colour ownership fill.
    dl->AddCircleFilled(centre, std::max(1.0f, r * 0.18f), outline, 8);
}

void corp_emblem(ImDrawList* dl, ImVec2 centre, float r, int shape, ImU32 fill)
{
    // Simple, legible geometric primitives so the emblem reads at portrait size on
    // the identity card, at header size in the Selection panel, and shrunk to a
    // small on-canvas identity tag. Shape is chosen deterministically upstream by
    // palette::corp_emblem_shape; here we just wrap the index into range.
    const ImVec2 c = centre;
    switch (((shape % palette::corp_emblem_shape_count) + palette::corp_emblem_shape_count)
            % palette::corp_emblem_shape_count)
    {
        case 0: // circle
            dl->AddCircleFilled(c, r, fill, 24);
            break;
        case 1: // square
            dl->AddRectFilled({ c.x - r * 0.85f, c.y - r * 0.85f },
                              { c.x + r * 0.85f, c.y + r * 0.85f }, fill);
            break;
        case 2: // upward triangle
            dl->AddTriangleFilled({ c.x, c.y - r },
                                  { c.x + r * 0.92f, c.y + r * 0.7f },
                                  { c.x - r * 0.92f, c.y + r * 0.7f }, fill);
            break;
        case 3: // diamond
            dl->AddQuadFilled({ c.x, c.y - r }, { c.x + r, c.y },
                              { c.x, c.y + r }, { c.x - r, c.y }, fill);
            break;
        case 4: // hexagon
            dl->AddNgonFilled(c, r, fill, 6);
            break;
        default: // 5 — pentagon
            dl->AddNgonFilled(c, r, fill, 5);
            break;
    }
}

void unknown(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Question-mark hook: an arc across the top, opening at the lower-left.
    const ImVec2 hook_centre{ centre.x, centre.y - r * 0.25f };
    dl->PathArcTo(hook_centre, r * 0.45f, -2.7f, 0.9f, 16);
    dl->PathStroke(colour, 0, 1.6f);
    // Short stem from the hook down toward the centre.
    dl->AddLine({ centre.x, centre.y + r * 0.05f }, { centre.x, centre.y + r * 0.30f }, colour, 1.6f);
    // Dot below.
    dl->AddCircleFilled({ centre.x, centre.y + r * 0.62f }, std::max(1.0f, r * 0.10f), colour, 8);
}

void activity(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Concentric "pulse": a filled core ringed by a signal ring — reads as a live
    // commercial beacon, distinct from the survey magnifier and the unknown hook.
    dl->AddCircleFilled(centre, std::max(1.0f, r * 0.40f), colour, 12);
    dl->AddCircle(centre, r * 0.85f, colour, 12, 1.4f);
}

void survey_badge(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Magnifying glass: a lens circle with a diagonal handle (a "scan" connotation).
    const float    lr   = r * 0.55f;
    const ImVec2   lens{ centre.x - r * 0.18f, centre.y - r * 0.18f };
    dl->AddCircle(lens, lr, colour, 0, 1.6f);
    const ImVec2 h0{ lens.x + lr * 0.7f, lens.y + lr * 0.7f };
    const ImVec2 h1{ centre.x + r * 0.7f, centre.y + r * 0.7f };
    dl->AddLine(h0, h1, colour, 2.0f);
}

} // namespace ui::icons
