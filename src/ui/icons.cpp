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

// Flat-top hexagon with a small hub dot — the Inland Logistics Hub marker (BL-149). A
// six-sided network-node silhouette, distinct from the launchpad circle and the port
// triangle, evoking a routing waypoint the land network converges on.
void hub_node(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    ImVec2 v[6];
    for (int i = 0; i < 6; ++i)
    {
        const float a = 3.14159265f / 3.0f * static_cast<float>(i); // 0°,60°,… flat-top
        v[i] = { c.x + r * std::cos(a), c.y + r * std::sin(a) };
    }
    dl->AddConvexPolyFilled(v, 6, fill);
    dl->AddPolyline(v, 6, outline, ImDrawFlags_Closed, 1.0f);
    dl->AddCircleFilled(c, r * 0.28f, outline); // hub node at the centre
}

// Shield silhouette — the Military Base marker (BL-325 S1). A flat top with
// shoulders tapering to a bottom point: the martial building, FILLED like every
// other building glyph — so it never reads as the port's upward triangle or the
// hub's hexagon. (The stroke-only unit chevron was deleted uncalled, BL-294; a
// unit marker returns with BL-157.)
void shield(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    const ImVec2 v[5] = {
        { c.x - r,        c.y - r },         // top-left
        { c.x + r,        c.y - r },         // top-right
        { c.x + r,        c.y + r * 0.25f }, // right shoulder
        { c.x,            c.y + r },         // bottom point
        { c.x - r,        c.y + r * 0.25f }, // left shoulder
    };
    dl->AddConvexPolyFilled(v, 5, fill);
    dl->AddPolyline(v, 5, outline, ImDrawFlags_Closed, 1.0f);
}

// --- BL-429: named-building glyphs -------------------------------------------
// One shape per (extraction target / processing primary-output) resource that
// the ancient roster gave a building name to (BL-429 slice 2). Two or more
// named buildings that reach the same resource share one glyph by design —
// the glyph identifies WHAT a site makes or works, not which specific recipe
// — so e.g. the Charcoal Burner and the Peat Kiln (both -> charcoal) read the
// same, exactly as two Iron Mines already share one glyph regardless of tile.
// Anything without a bespoke shape here falls through to the generic
// ore_chunk / square in `building()` below.

void quarry_stone(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // An irregular five-sided boulder — taller and more asymmetric than the
    // flat eight-sided ore_chunk crystal, so raw stone reads apart from a
    // mineral deposit at a glance.
    const ImVec2 v[5] = {
        { c.x - r * 0.55f, c.y - r },         // top-left peak
        { c.x + r * 0.30f, c.y - r * 0.80f }, // top-right shoulder
        { c.x + r,         c.y + r * 0.20f }, // right bulge
        { c.x + r * 0.15f, c.y + r },         // bottom point
        { c.x - r,         c.y + r * 0.35f }, // left bulge
    };
    dl->AddConvexPolyFilled(v, 5, fill);
    dl->AddPolyline(v, 5, outline, ImDrawFlags_Closed, 1.0f);
}

void woodcutter_timber(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // Two crossed logs with round end-caps — a stronger "raw wood" read at
    // icon size than a tree silhouette, and the only X silhouette in the
    // building family.
    const float k = r * 0.75f;
    dl->AddLine({c.x - k, c.y - k}, {c.x + k, c.y + k}, fill, 3.0f);
    dl->AddLine({c.x - k, c.y + k}, {c.x + k, c.y - k}, fill, 3.0f);
    const float capr = r * 0.20f;
    dl->AddCircleFilled({c.x - k, c.y - k}, capr, fill);
    dl->AddCircleFilled({c.x + k, c.y + k}, capr, fill);
    dl->AddCircleFilled({c.x - k, c.y + k}, capr, fill);
    dl->AddCircleFilled({c.x + k, c.y - k}, capr, fill);
}

void sand_pit(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // A low, wide dune — much flatter than the port/production triangles, so
    // it reads as a heap rather than a peak — with wind-blown grain dots
    // above the crest.
    const ImVec2 v[3] = {
        { c.x - r, c.y + r * 0.60f },
        { c.x + r, c.y + r * 0.60f },
        { c.x,     c.y - r * 0.10f },
    };
    dl->AddConvexPolyFilled(v, 3, fill);
    dl->AddPolyline(v, 3, outline, ImDrawFlags_Closed, 1.0f);
    dl->AddCircleFilled({c.x - r * 0.35f, c.y - r * 0.55f}, r * 0.08f, fill);
    dl->AddCircleFilled({c.x,             c.y - r * 0.75f}, r * 0.08f, fill);
    dl->AddCircleFilled({c.x + r * 0.35f, c.y - r * 0.55f}, r * 0.08f, fill);
}

void clay_pit(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // A narrow-necked raw-clay vessel — the unfired counterpart of the
    // Potter & Weaver's tied goods bundle, with a plain rim rather than a
    // cinched neck.
    const ImVec2 v[6] = {
        { c.x - r * 0.28f, c.y - r },         // neck top-left
        { c.x + r * 0.28f, c.y - r },         // neck top-right
        { c.x + r * 0.55f, c.y - r * 0.15f }, // shoulder right
        { c.x + r * 0.40f, c.y + r },         // base right
        { c.x - r * 0.40f, c.y + r },         // base left
        { c.x - r * 0.55f, c.y - r * 0.15f }, // shoulder left
    };
    dl->AddConvexPolyFilled(v, 6, fill);
    dl->AddPolyline(v, 6, outline, ImDrawFlags_Closed, 1.0f);
}

void peat_cutting(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // Three stacked, alternately-offset turf bricks — cut peat, distinct from
    // both the stone boulder and the ore/mineral chunk.
    const float bw   = r * 0.85f;
    const float bh   = r * 0.40f;
    const float gap  = r * 0.14f;
    const float offs = r * 0.20f;
    const float step = bh + gap;
    const float y0   = c.y - step * 1.5f + bh * 0.5f;
    for (int i = 0; i < 3; ++i)
    {
        const float  yy   = y0 + static_cast<float>(i) * step;
        const float  xoff = (i % 2 == 0) ? -offs : offs;
        const ImVec2 lo   = {c.x - bw * 0.5f + xoff, yy};
        const ImVec2 hi   = {c.x + bw * 0.5f + xoff, yy + bh};
        dl->AddRectFilled(lo, hi, fill);
        dl->AddRect(lo, hi, outline, 0.0f, 0, 1.0f);
    }
}

void iron_mine(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // Crossed pick handles with small wedge heads — the universal "mine"
    // mark, distinct from the raw ore_chunk crystal it sits beside in the
    // Build door.
    const float  k  = r * 0.80f;
    const ImVec2 a1 = {c.x - k, c.y - k}, a2 = {c.x + k, c.y + k};
    const ImVec2 b1 = {c.x - k, c.y + k}, b2 = {c.x + k, c.y - k};
    dl->AddLine(a1, a2, fill, 2.5f);
    dl->AddLine(b1, b2, fill, 2.5f);
    const float  hw = r * 0.22f;
    const ImVec2 h1[3] = { {a1.x - hw, a1.y}, {a1.x + hw, a1.y}, {a1.x, a1.y - hw} };
    const ImVec2 h2[3] = { {b2.x - hw, b2.y}, {b2.x + hw, b2.y}, {b2.x, b2.y - hw} };
    dl->AddConvexPolyFilled(h1, 3, fill);
    dl->AddConvexPolyFilled(h2, 3, fill);
}

void copper_mine(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // A pointy-top hexagon nugget — hub_node's hexagon rotated 30 degrees and
    // stripped of its centre dot, so a copper claim never reads as a
    // logistics hub.
    ImVec2 v[6];
    for (int i = 0; i < 6; ++i)
    {
        const float a = 3.14159265f / 3.0f * static_cast<float>(i) + 3.14159265f / 6.0f;
        v[i] = { c.x + r * 0.85f * std::cos(a), c.y + r * 0.85f * std::sin(a) };
    }
    dl->AddConvexPolyFilled(v, 6, fill);
    dl->AddPolyline(v, 6, outline, ImDrawFlags_Closed, 1.0f);
}

void water_extractor(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // A teardrop — a pointed cap over a rounded base — the only rounded,
    // pointed silhouette in the set, reading as "water" against every
    // angular glyph beside it.
    const float  br = r * 0.62f;
    const ImVec2 bc = {c.x, c.y + r * 0.25f};
    dl->AddCircleFilled(bc, br, fill);
    const ImVec2 v[3] = {
        { c.x,               c.y - r },
        { bc.x + br * 0.85f, bc.y - br * 0.15f },
        { bc.x - br * 0.85f, bc.y - br * 0.15f },
    };
    dl->AddConvexPolyFilled(v, 3, fill);
    dl->AddCircle(bc, br, outline, 0, 1.0f);
}

void farm(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // Three stalks fanning from a base point, each capped with a grain head —
    // "cultivated ground", distinct from the tied goods bundle the harvest
    // eventually becomes. Shared by the Farm and the Fishing Wharf (BL-168):
    // both work the same target resource, distinguished by placement rather
    // than by what they visibly produce.
    const ImVec2 base  = {c.x, c.y + r};
    const float  headr = r * 0.16f;
    for (int i = -1; i <= 1; ++i)
    {
        const ImVec2 tip = {c.x + static_cast<float>(i) * r * 0.55f, c.y - r * 0.85f};
        dl->AddLine(base, tip, fill, 2.0f);
        dl->AddCircleFilled(tip, headr, fill);
    }
}

void charcoal_kiln(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // A squat earthen mound with a smoke vent — the only dome silhouette in
    // the set. Shared by the Charcoal Burner and the Peat Kiln (both -> charcoal).
    const ImVec2 v[5] = {
        { c.x - r,         c.y + r },
        { c.x - r * 0.75f, c.y - r * 0.30f },
        { c.x,             c.y - r * 0.90f },
        { c.x + r * 0.75f, c.y - r * 0.30f },
        { c.x + r,         c.y + r },
    };
    dl->AddConvexPolyFilled(v, 5, fill);
    dl->AddPolyline(v, 5, outline, ImDrawFlags_Closed, 1.0f);
    dl->AddLine({c.x, c.y - r * 0.90f}, {c.x, c.y - r * 1.15f}, outline, 1.5f);
}

void bloomery(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // A cluster of rough lumps — the raw bloom, distinct from the smooth
    // teardrop and the angular ore_chunk crystal.
    dl->AddCircleFilled({c.x,             c.y - r * 0.15f}, r * 0.55f, fill);
    dl->AddCircleFilled({c.x - r * 0.55f, c.y + r * 0.30f}, r * 0.38f, fill);
    dl->AddCircleFilled({c.x + r * 0.55f, c.y + r * 0.25f}, r * 0.38f, fill);
    dl->AddCircle({c.x, c.y - r * 0.15f}, r * 0.55f, outline, 0, 1.0f);
}

void smithy_ingot(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // A flat trapezoid ingot bar — the worked-metal silhouette, distinct from
    // the copper hexagon nugget and the iron ore-chunk crystal it is smelted
    // from. Shared with the industrial Smelter's steel — it IS the same good.
    const ImVec2 v[4] = {
        { c.x - r,         c.y + r * 0.35f },
        { c.x - r * 0.62f, c.y - r * 0.35f },
        { c.x + r * 0.62f, c.y - r * 0.35f },
        { c.x + r,         c.y + r * 0.35f },
    };
    dl->AddConvexPolyFilled(v, 4, fill);
    dl->AddPolyline(v, 4, outline, ImDrawFlags_Closed, 1.0f);
}

void goods_bundle(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // A cinched sack — round body, tied neck — the finished-goods bundle.
    // Distinct from the water teardrop (no point at the top; a flat tie-line
    // instead) and the plain processing square. Shared by the Potter & Weaver
    // and the Glassworks (both -> trade_goods_misc).
    const ImVec2 bc = {c.x, c.y + r * 0.15f};
    dl->AddCircleFilled(bc, r * 0.80f, fill);
    dl->AddCircle(bc, r * 0.80f, outline, 0, 1.0f);
    dl->AddLine({c.x - r * 0.45f, c.y - r * 0.55f}, {c.x + r * 0.45f, c.y - r * 0.55f}, outline, 2.0f);
}

void ration_pack(ImDrawList* dl, ImVec2 c, float r, ImU32 fill)
{
    // A rounded, strapped ration pack — two binding lines across a loaf
    // shape. Distinct from the goods sack (no cinched neck; a flat
    // rectangular block instead). Shared with the industrial Food
    // Processor's rations — it IS the same good.
    const ImVec2 lo = {c.x - r * 0.80f, c.y - r * 0.55f};
    const ImVec2 hi = {c.x + r * 0.80f, c.y + r * 0.55f};
    dl->AddRectFilled(lo, hi, fill, r * 0.25f);
    dl->AddRect(lo, hi, outline, r * 0.25f, 0, 1.0f);
    dl->AddLine({c.x - r * 0.30f, lo.y}, {c.x - r * 0.30f, hi.y}, outline, 1.5f);
    dl->AddLine({c.x + r * 0.30f, lo.y}, {c.x + r * 0.30f, hi.y}, outline, 1.5f);
}

} // namespace

void building(ImDrawList* dl, ImVec2 centre, float r, building_type type,
             resource_type identity, ImU32 fill)
{
    switch (type)
    {
        case building_type::extraction_site:
            switch (identity)
            {
                case resource_type::stone:                quarry_stone(dl, centre, r, fill);      break;
                case resource_type::timber:                woodcutter_timber(dl, centre, r, fill); break;
                case resource_type::sand:                  sand_pit(dl, centre, r, fill);           break;
                case resource_type::clay:                  clay_pit(dl, centre, r, fill);           break;
                case resource_type::peat:                  peat_cutting(dl, centre, r, fill);       break;
                case resource_type::iron_ore:               iron_mine(dl, centre, r, fill);          break;
                case resource_type::copper_ore:             copper_mine(dl, centre, r, fill);        break;
                case resource_type::water:                  water_extractor(dl, centre, r, fill);    break;
                case resource_type::agricultural_produce:   farm(dl, centre, r, fill);               break;
                default:                                     ore_chunk(dl, centre, r, fill);          break;
            }
            break;
        case building_type::processing_facility:
            switch (identity)
            {
                case resource_type::charcoal:         charcoal_kiln(dl, centre, r, fill); break;
                case resource_type::iron_blooms:      bloomery(dl, centre, r, fill);      break;
                case resource_type::steel:            smithy_ingot(dl, centre, r, fill);  break;
                case resource_type::trade_goods_misc: goods_bundle(dl, centre, r, fill);  break;
                case resource_type::food_rations:     ration_pack(dl, centre, r, fill);   break;
                default:                                square(dl, centre, r, fill);        break;
            }
            break;
        case building_type::port:                 triangle(dl, centre, r, fill);  break;
        case building_type::inland_logistics_hub: hub_node(dl, centre, r, fill);  break; // BL-149
        case building_type::military_base:        shield(dl, centre, r, fill);    break; // BL-325
        case building_type::research_institute:   research(dl, centre, r, fill);  break; // BL-332
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

void under_construction(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Crane silhouette: a mast, an angled boom reaching toward the load, a
    // back-stay bracing it, and a short hook line hanging from the boom tip.
    // Four strokes, stroke-only (see the header contract) — distinct from the
    // convoy chevron (points right) and every filled building glyph. Same
    // shadow-then-colour pass as convoy() so it reads against any terrain or
    // lens fill.
    const ImVec2 base    { centre.x - r * 0.15f, centre.y + r         };
    const ImVec2 top     { centre.x - r * 0.15f, centre.y - r         };
    const ImVec2 boom_tip{ centre.x + r,         centre.y - r * 0.60f };
    const ImVec2 stay    { centre.x - r * 0.85f, centre.y - r * 0.15f };
    const ImVec2 hook    { boom_tip.x,            boom_tip.y + r * 0.35f };

    const auto stroke = [&](ImVec2 a, ImVec2 b) {
        dl->AddLine(a, b, outline, 2.5f);
        dl->AddLine(a, b, colour,  1.5f);
    };
    stroke(base, top);       // mast
    stroke(top, boom_tip);   // boom
    stroke(top, stay);       // back-stay
    stroke(boom_tip, hook);  // hook line
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

void battle(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Two crossed blades. Each is a line with a short cross-guard near the hilt,
    // so at marker size it still reads as a weapon rather than as an X — an X is
    // the "closed" affordance elsewhere in this vocabulary and must not collide
    // with it.
    const float a = r * 0.80f;
    const float g = r * 0.30f;   // cross-guard half-length
    const float t = 2.0f;

    // Blade one: top-left to bottom-right.
    dl->AddLine({centre.x - a, centre.y - a}, {centre.x + a, centre.y + a}, colour, t);
    dl->AddLine({centre.x + a - g, centre.y + a - g * 1.6f},
                {centre.x + a - g * 1.6f, centre.y + a - g}, colour, t);
    // Blade two: top-right to bottom-left.
    dl->AddLine({centre.x + a, centre.y - a}, {centre.x - a, centre.y + a}, colour, t);
    dl->AddLine({centre.x - a + g, centre.y + a - g * 1.6f},
                {centre.x - a + g * 1.6f, centre.y + a - g}, colour, t);
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

void continent(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Two plates split by a diagonal seam. Each plate is its own convex quad —
    // the seam is the GAP between them, not a drawn line, so the "crust in
    // pieces" reading survives at strip size where a hairline would vanish. The
    // quads are deliberately not mirror images: an irregular pair reads as
    // tectonic, a symmetric one reads as a button.
    const ImVec2 left[4] = {
        { centre.x - r,         centre.y - r * 0.90f },
        { centre.x + r * 0.05f, centre.y - r * 0.70f },
        { centre.x - r * 0.15f, centre.y + r * 0.90f },
        { centre.x - r,         centre.y + r * 0.70f },
    };
    const ImVec2 right[4] = {
        { centre.x + r * 0.30f, centre.y - r * 0.70f },
        { centre.x + r,         centre.y - r * 0.90f },
        { centre.x + r,         centre.y + r * 0.70f },
        { centre.x + r * 0.10f, centre.y + r * 0.90f },
    };
    dl->AddConvexPolyFilled(left, 4, colour);
    dl->AddPolyline(left, 4, outline, ImDrawFlags_Closed, 1.0f);
    dl->AddConvexPolyFilled(right, 4, colour);
    dl->AddPolyline(right, 4, outline, ImDrawFlags_Closed, 1.0f);
}

void landform(ImDrawList* dl, ImVec2 centre, float r, terrain_landform lf, ImU32 colour)
{
    // Terrain shape, not an entity — so every glyph here is STROKE-ONLY and none
    // carries the filled family's dark `outline`. A filled silhouette would read as
    // "something is installed on this tile", which is the one thing landform is not.
    //
    // The stroke scales with r but never below ~1px, so a glyph stays visible when
    // the canvas is zoomed out far enough that hexes are small.
    const float t = std::max(1.0f, r * 0.16f);

    switch (lf)
    {
        case terrain_landform::mountain:
        {
            // Twin peaks sharing a saddle, open at the bottom. No baseline: the
            // open feet are what separate this from the filled port triangle and
            // the production up-triangle, both of which sit ON a line.
            const ImVec2 range[5] = {
                { centre.x - r,         centre.y + r * 0.62f },
                { centre.x - r * 0.34f, centre.y - r * 0.72f },
                { centre.x + r * 0.04f, centre.y + r * 0.06f },
                { centre.x + r * 0.46f, centre.y - r * 0.46f },
                { centre.x + r,         centre.y + r * 0.62f },
            };
            dl->AddPolyline(range, 5, colour, ImDrawFlags_None, t);
            break;
        }
        case terrain_landform::canyon:
        {
            // Two rim shoulders split by a narrow incision that cuts BELOW them —
            // the gorge is the gap, and the rims are level, so it reads as a cleft
            // in flat ground rather than as the continent glyph's diagonal seam.
            const ImVec2 left[3] = {
                { centre.x - r,         centre.y - r * 0.42f },
                { centre.x - r * 0.30f, centre.y - r * 0.42f },
                { centre.x - r * 0.10f, centre.y + r * 0.78f },
            };
            const ImVec2 right[3] = {
                { centre.x + r,         centre.y - r * 0.42f },
                { centre.x + r * 0.30f, centre.y - r * 0.42f },
                { centre.x + r * 0.10f, centre.y + r * 0.78f },
            };
            dl->AddPolyline(left,  3, colour, ImDrawFlags_None, t);
            dl->AddPolyline(right, 3, colour, ImDrawFlags_None, t);
            break;
        }
        case terrain_landform::crater:
        {
            // A flattened bowl: a wide, low ellipse with a raised near rim drawn as
            // a second arc inside its lower half. Deliberately NOT concentric circles
            // (the activity pulse) and not a circle-plus-cross (the market centre) —
            // the squashed aspect ratio is what says "impact basin seen from above".
            const int   kSeg = 20;
            ImVec2      rim[kSeg];
            for (int i = 0; i < kSeg; ++i)
            {
                const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(kSeg);
                rim[i] = { centre.x + r * 0.98f * std::cos(a),
                           centre.y + r * 0.56f * std::sin(a) };
            }
            dl->AddPolyline(rim, kSeg, colour, ImDrawFlags_Closed, t);

            const int kInner = 9; // lower half only — the near rim of the bowl
            ImVec2    floor_arc[kInner];
            for (int i = 0; i < kInner; ++i)
            {
                const float a = 3.1415927f * static_cast<float>(i) / static_cast<float>(kInner - 1);
                floor_arc[i] = { centre.x + r * 0.52f * std::cos(a),
                                 centre.y + r * 0.30f * std::sin(a) };
            }
            dl->AddPolyline(floor_arc, kInner, colour, ImDrawFlags_None, t * 0.8f);
            break;
        }
        case terrain_landform::rift:
        {
            // A single jagged fissure running top to bottom — the only zigzag in the
            // vocabulary, so it cannot be confused with any chevron (which meet at one
            // point) or with the canyon's paired rims.
            const ImVec2 fissure[6] = {
                { centre.x + r * 0.30f, centre.y - r },
                { centre.x - r * 0.16f, centre.y - r * 0.44f },
                { centre.x + r * 0.24f, centre.y - r * 0.06f },
                { centre.x - r * 0.26f, centre.y + r * 0.34f },
                { centre.x + r * 0.14f, centre.y + r * 0.66f },
                { centre.x - r * 0.20f, centre.y + r },
            };
            dl->AddPolyline(fissure, 6, colour, ImDrawFlags_None, t);
            break;
        }
        case terrain_landform::plains:
        case terrain_landform::highland:
        case terrain_landform::valley:
            break; // common ground — carried by the relief tint, not a glyph
    }
}

bool landform_spans(terrain_landform lf)
{
    return lf == terrain_landform::mountain
        || lf == terrain_landform::rift
        || lf == terrain_landform::canyon;
}

void landform_span(ImDrawList* dl, ImVec2 from, ImVec2 mid, float amp, float thick,
                   terrain_landform lf, ImU32 colour)
{
    if (!landform_spans(lf))
        return;

    const float dx  = mid.x - from.x;
    const float dy  = mid.y - from.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.5f)
        return;

    // Canonical perpendicular. Deriving it from the direction of travel alone would
    // point it opposite ways for the two halves of one span — they would deflect to
    // opposite sides and meet in a kink instead of forming a continuous wave. Forcing
    // a deterministic side (up on screen; leftward for a vertical run) makes both
    // halves agree without either tile knowing about the other.
    float px = -dy / len;
    float py =  dx / len;
    if (py > 0.0f || (py == 0.0f && px > 0.0f)) { px = -px; py = -py; }

    // Sample the half-span at fixed parameters, offset perpendicular by the profile.
    // Both ends sit at zero offset so the halves join flush at `mid` and at the centre.
    auto stroke = [&](const float* offs, int n, float t_thick) {
        ImVec2 pts[6];
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(n - 1);
            const float o = offs[i] * amp;
            pts[i] = { from.x + dx * t + px * o, from.y + dy * t + py * o };
        }
        dl->AddPolyline(pts, n, colour, ImDrawFlags_None, t_thick);
    };

    switch (lf)
    {
        case terrain_landform::mountain:
        {
            // ONE peak per half-edge, so a tile-to-tile span reads as two summits and a
            // three-tile run as four — a row of peaks, which is how a range is drawn on
            // a map. An earlier profile put two spikes in each half; at four per span the
            // teeth were fine enough that a cluster read as a jagged OUTLINE rather than
            // a ridge, which is the one thing bridging was meant to fix.
            static const float teeth[3] = { 0.0f, 1.0f, 0.0f };
            stroke(teeth, 3, thick);
            break;
        }
        case terrain_landform::rift:
        {
            // A jagged crack: alternating deflection at higher frequency and a thinner
            // stroke, echoing the lone tile's fissure. The only span that crosses its
            // own axis, so it never reads as a ridge.
            static const float crack[4] = { 0.0f, 0.85f, -0.85f, 0.0f };
            stroke(crack, 4, thick * 0.8f);
            break;
        }
        case terrain_landform::canyon:
        {
            // Two straight parallel rims with the gorge between them — the lone tile's
            // paired-rim glyph, extended along the run.
            static const float rim_hi[2] = {  1.0f,  1.0f };
            static const float rim_lo[2] = { -1.0f, -1.0f };
            stroke(rim_hi, 2, thick * 0.8f);
            stroke(rim_lo, 2, thick * 0.8f);
            break;
        }
        default:
            break;
    }
}

void history(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Hourglass: a down-triangle over an up-triangle meeting at a centre waist,
    // with capped ends. Read as "time"; the meeting point is what distinguishes
    // it from the scarcity and production single triangles.
    const float hw = r * 0.72f;   // half-width at the capped ends
    dl->AddLine({ centre.x - hw, centre.y - r }, { centre.x + hw, centre.y - r }, colour, 1.5f);
    dl->AddLine({ centre.x - hw, centre.y + r }, { centre.x + hw, centre.y + r }, colour, 1.5f);
    // The two tapers crossing at the waist.
    dl->AddLine({ centre.x - hw, centre.y - r }, { centre.x, centre.y }, colour, 1.5f);
    dl->AddLine({ centre.x + hw, centre.y - r }, { centre.x, centre.y }, colour, 1.5f);
    dl->AddLine({ centre.x - hw, centre.y + r }, { centre.x, centre.y }, colour, 1.5f);
    dl->AddLine({ centre.x + hw, centre.y + r }, { centre.x, centre.y }, colour, 1.5f);
}

void research(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Branching tree: a stem rising from the bottom to a fork, then two
    // diagonals out to capped terminal nodes. The only branching glyph in the
    // vocabulary, so it reads as "tech tree" rather than an arrow or triangle.
    const float fork_y = centre.y - r * 0.15f;
    const float node_r = std::max(1.0f, r * 0.22f);
    // Stem.
    dl->AddLine({ centre.x, centre.y + r }, { centre.x, fork_y }, colour, 1.5f);
    // Two branches out to their nodes.
    const ImVec2 left  = { centre.x - r * 0.70f, centre.y - r * 0.75f };
    const ImVec2 right = { centre.x + r * 0.70f, centre.y - r * 0.75f };
    dl->AddLine({ centre.x, fork_y }, left,  colour, 1.5f);
    dl->AddLine({ centre.x, fork_y }, right, colour, 1.5f);
    dl->AddCircleFilled(left,  node_r, colour);
    dl->AddCircleFilled(right, node_r, colour);
}

void strategy(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Pennant on a pole: a vertical staff with a filled right-triangle flag at
    // its head, flying to the right. The flag hangs off the staff top rather
    // than resting on a baseline, keeping it clear of the production triangle.
    const float x = centre.x - r * 0.55f;
    dl->AddLine({ x, centre.y - r }, { x, centre.y + r }, colour, 1.5f);
    const ImVec2 v[3] = {
        { x,                 centre.y - r },          // staff head
        { centre.x + r,      centre.y - r * 0.45f },  // flag point
        { x,                 centre.y + r * 0.10f },  // flag foot on the staff
    };
    dl->AddConvexPolyFilled(v, 3, colour);
}

void readout(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Tally chart: a left axis with three left-anchored horizontal bars of
    // descending length — "counts compared", the readout motif. The axis is
    // what keeps it distinct from the supply route pair and the resource
    // strata; the horizontal bars keep it clear of the market lens's vertical
    // ones.
    const float x0 = centre.x - r * 0.8f;
    dl->AddLine({ x0, centre.y - r }, { x0, centre.y + r }, colour, 1.5f);

    const float bar_h   = r * 0.36f;
    const float ys[3]   = { centre.y - r * 0.60f, centre.y, centre.y + r * 0.60f };
    const float lens[3] = { 1.60f, 1.05f, 0.55f }; // descending, in units of r
    for (int i = 0; i < 3; ++i)
        dl->AddRectFilled({ x0 + 2.0f, ys[i] - bar_h * 0.5f },
                          { x0 + 2.0f + r * lens[i], ys[i] + bar_h * 0.5f }, colour);
}

void diplomacy(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // Two overlapping circle outlines — two parties meeting. The overlap is the
    // motif, so it never reads as the single market-centre circle.
    const float cr  = r * 0.62f;
    const float off = r * 0.38f;
    dl->AddCircle({ centre.x - off, centre.y }, cr, colour, 0, 1.5f);
    dl->AddCircle({ centre.x + off, centre.y }, cr, colour, 0, 1.5f);
}

void contract(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // A page with the top-right corner cut off for a dog-ear fold, plus a
    // short check mark near the bottom — "a signed document". The fold is
    // what keeps this distinct from `ledger` (a plain ruled box, no fold);
    // the rectangle baseline keeps it distinct from `history`'s hourglass.
    const float hw   = r * 0.62f;
    const float hh   = r * 0.85f;
    const float fold = r * 0.34f;
    const ImVec2 tl{ centre.x - hw, centre.y - hh };
    const ImVec2 tr{ centre.x + hw, centre.y - hh };
    const ImVec2 br{ centre.x + hw, centre.y + hh };
    const ImVec2 bl{ centre.x - hw, centre.y + hh };
    const ImVec2 pts[5] = {
        tl,
        { tr.x - fold, tr.y },
        { tr.x, tr.y + fold },
        br,
        bl,
    };
    dl->AddPolyline(pts, 5, colour, ImDrawFlags_Closed, 1.5f);
    // Check mark: a short down-stroke then a longer up-stroke, "accepted".
    dl->AddLine({ centre.x - hw * 0.45f, centre.y + hh * 0.30f },
               { centre.x - hw * 0.05f, centre.y + hh * 0.62f }, colour, 1.5f);
    dl->AddLine({ centre.x - hw * 0.05f, centre.y + hh * 0.62f },
               { centre.x + hw * 0.55f, centre.y - hh * 0.05f }, colour, 1.5f);
}

void unit_marker(ImDrawList* dl, ImVec2 centre, float r, ImU32 fill, bool committed)
{
    // Humanoid silhouette: filled circle head over a filled triangle body,
    // the same two-primitive shape as glyph_soldier (selection_panel.cpp) so
    // the canvas marker and the unit card's placeholder read as one
    // vocabulary. Owner-tinted like every other entity marker's fill, with
    // the standard dark outline stroke for contrast on any terrain.
    const ImVec2 head{ centre.x, centre.y - r * 0.42f };
    const float  head_r = r * 0.30f;
    dl->AddCircleFilled(head, head_r, fill, 14);
    dl->AddCircle(head, head_r, outline, 14, 1.0f);

    const ImVec2 top{ centre.x, centre.y - r * 0.05f };
    const ImVec2 bl { centre.x - r * 0.55f, centre.y + r * 0.80f };
    const ImVec2 br { centre.x + r * 0.55f, centre.y + r * 0.80f };
    dl->AddTriangleFilled(top, bl, br, fill);
    dl->AddTriangle(top, bl, br, outline, 1.0f);

    // BL-575 stub: nothing sets `committed` true yet — BL-573 (wave 4 of the
    // same batch) adds the real per-unit contract flag this reads. Amber ring
    // is a local convention (no palette entry exists for "under contract"
    // yet); revisit if/when BL-573/BL-576 settle a shared contract colour.
    if (committed)
    {
        constexpr ImU32 kCommittedRing = IM_COL32(255, 195, 60, 230);
        dl->AddCircle(centre, r * 1.20f, kCommittedRing, 20, 2.0f);
    }
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

void value_mark(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour)
{
    // A simple filled dot with a thin dark outline for contrast on any terrain —
    // deliberately plain so the red→green colour itself carries the reading,
    // rather than a shape (BL-135, Workforce + Opportunity lenses).
    dl->AddCircleFilled(centre, r, colour, 16);
    dl->AddCircle(centre, r, IM_COL32(20, 20, 24, 200), 16, 1.2f);
}

} // namespace ui::icons
