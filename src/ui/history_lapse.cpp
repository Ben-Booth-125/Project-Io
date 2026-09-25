/// @file history_lapse.cpp
/// Round 4's time-lapse map and its top-16 board (BL-829 / BL-830).
/// See history_lapse.hpp for what this surface is and what it deliberately
/// refuses to draw.

#include "history_lapse.hpp"

#include "generation_preview.hpp" // preview_pack / preview_substrate — the land mask
#include "presentation.hpp"       // identity colours; they live there, never here
#include "text_fit.hpp"           // the board's name cell, elided AND recorded

#include "world/components.hpp"   // is_water, terrain_landform
#include "world/hex_neighbors.hpp" // the odd-r side offsets the river bits are keyed by

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>

namespace ui {

namespace {

// The map's non-identity ground. These are BACKDROP, not identity: sea, and land
// nobody has reached yet. Identity colours are ui::palette's and are not
// duplicated here (presentation.hpp owns them).
constexpr ImU32 col_sea      = IM_COL32( 16,  24,  38, 255);
constexpr ImU32 col_void     = IM_COL32( 10,  11,  15, 255);
constexpr ImU32 col_frontier = IM_COL32(  8,   9,  12, 200); ///< The line between holders.
/// BL-1090: the 2 px dark of a HARD border -- opaque where the soft line is
/// translucent, so the two read as different weights and not as one line
/// drawn twice.
constexpr ImU32 col_frontier_hard = IM_COL32(  8,   9,  12, 255);
constexpr ImU32 col_dim      = IM_COL32(120, 128, 145, 255);
constexpr ImU32 col_bright   = IM_COL32(225, 230, 240, 255);

// THE TERRAIN BASE (BL-915). Dull on purpose: it is the ground the fill is read
// against, never a subject of its own, so every value here sits in a narrow
// grey-olive band and the political tint over it carries the hue. Indexed by
// `lapse_base_run::kind`.
constexpr ImU32 col_base[5] = {
    IM_COL32( 60,  62,  54, 255), // 0 flat land
    IM_COL32( 70,  74,  58, 255), // 1 valley — faintly lighter and greener
    IM_COL32( 88,  86,  76, 255), // 2 highland
    IM_COL32(118, 114, 106, 255), // 3 mountain
    IM_COL32( 40,  38,  42, 255), // 4 canyon / rift — a cut, so darker
};
constexpr ImU32 col_relief_lit  = IM_COL32(210, 206, 196, 120); ///< The lit rim of a range.
constexpr ImU32 col_relief_dark = IM_COL32(  8,   8,  10, 160); ///< Its shadowed rim.
constexpr ImU32 col_river       = IM_COL32( 92, 150, 200, 230);
constexpr ImU32 col_seat_ring   = IM_COL32(  8,   9,  12, 230);

// THE PROMOTED ROAD NETWORK (BL-917). Track is a faint scratch, barely apart
// from the ground it crosses; Road is the same hue brightened and widened, so
// a corridor visibly THICKENS the frame it crosses a tier rather than
// changing colour outright — the skeleton hardens, it does not relabel
// itself. `col_bridge` is deliberately paler than either: a bridge is a point
// fact, not a line, and reads best as a small bright mark ON the road.
constexpr ImU32 col_road_track = IM_COL32(150, 130,  90, 130);
constexpr ImU32 col_road       = IM_COL32(215, 190, 130, 235);
constexpr ImU32 col_bridge     = IM_COL32(240, 240, 235, 255);

/// AMICABLE CROSS-BORDER TRADE (BL-925). A distinct hue from the road network
/// on purpose — this stroke crosses a FRONTIER, which a road corridor (always
/// inside one realm) never does, and the two facts should not read as one
/// idiom repainted. A cool green against the road's warm ochre.
constexpr ImU32 col_trade_link = IM_COL32(110, 205, 150, 220);
/// SEA LANES (BL-1097). Its own water layer, a third hue: a pale sea-blue
/// against the trade link's green and the road's ochre, so a lane earned by
/// crossings never reads as a road repainted onto water or as an open border.
/// Alpha kept below the trade link's so the water's own tint still shows
/// through a long lane.
constexpr ImU32 col_sea_lane = IM_COL32(140, 200, 245, 190);
/// BL-1080: a region that has crossed the furnace — an ember square at its
/// anchor, warm and saturated so it reads against every polity tint.
constexpr ImU32 col_furnace = IM_COL32(255, 138, 48, 245);
/// BL-1080: one spark of the industry heat — the furnace's hue, paler and
/// smaller, so a crossing's ember square still reads as the stronger fact.
constexpr ImU32 col_heat = IM_COL32(255, 196, 96, 230);

/// The fill's opacity over the base. High enough that a colour reads as a
/// colour on the board's swatch too; low enough that a mountain range and a
/// river still show through it, which is the whole point of the base.
constexpr int tint_alpha = 170;

/// The colour one polity index is drawn in, everywhere on this surface — the map
/// swatch and the board row are the same call, so a row and its territory cannot
/// disagree. The SLOT comes from the greedy colouring over the adjacency graph
/// (`assign_polity_colours`), not from the id, so two neighbours never share it.
/// A polity the colouring never saw (a record still deriving) falls back to its
/// own index, which is at least stable.
///
/// BL-1087: the slot is a wedge and an offset (world/polity_identity.hpp), so
/// the colour comes from `polity_slot_colour` with the record's wedge count,
/// and @p year applies the shade ratchet as it stood then (R8). A record with
/// no culture tree (`family_count == 0`) still reads the fallback table.
ImU32 polity_colour(const history_lapse& h, uint16_t owner, int year)
{
    const int32_t slot = (owner < h.polity_slot.size()) ? h.polity_slot[owner]
                                                        : static_cast<int32_t>(owner);
    const int rung = polity_rung_at(h.identity, owner, year);
    return palette::polity_slot_colour(slot, h.family_count, rung);
}

ImU32 with_alpha(ImU32 c, int a)
{
    return (c & 0x00FFFFFFu) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT);
}

/// Do two line segments cross, and where — plain 2D segment intersection in
/// TILE units, used once per candidate (road corridor, river edge) pair at
/// bake time (BL-917). Parallel or collinear segments report no crossing:
/// a road running exactly along a river for a stretch is not a bridge, it is
/// the kind of degenerate case a straight-line corridor abstraction can
/// simply decline to draw a glyph for.
bool lapse_segments_cross(float ax, float ay, float bx, float by,
                          float cx, float cy, float dx, float dy,
                          float& out_x, float& out_y)
{
    const float rx = bx - ax, ry = by - ay;
    const float sx = dx - cx, sy = dy - cy;
    const float denom = rx * sy - ry * sx;
    if (std::fabs(denom) < 1e-6f) return false;
    const float qpx = cx - ax, qpy = cy - ay;
    const float t = (qpx * sy - qpy * sx) / denom;
    const float u = (qpx * ry - qpy * rx) / denom;
    if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f) return false;
    out_x = ax + t * rx;
    out_y = ay + t * ry;
    return true;
}

/// Which base band a landform is drawn in. Crater and plains are flat: a crater
/// is an airless-body form the homeworld barely has, and drawing it as relief
/// would invent a barrier the walk never priced.
uint8_t base_kind(terrain_landform lf)
{
    switch (lf)
    {
    case terrain_landform::valley:   return 1;
    case terrain_landform::highland: return 2;
    case terrain_landform::mountain: return 3;
    case terrain_landform::canyon:
    case terrain_landform::rift:     return 4;
    default:                         return 0;
    }
}

/// BL-943 — the Empire era's own close (`hard_coded_world.cpp`'s
/// `epoch_year`, EXPLORATION.md's "1200 -> 1660"), where the Exploration
/// span's treasury consolidation fires once (BL-932 sec Capital arrives:
/// "material becomes capital"). A plain constant rather than a pull from
/// `world_params`, because this file keeps zero dependency on
/// hard_coded_world.hpp/history_sim.hpp by design (see the header's own "A
/// REPLAY, NEVER A RE-RUN" note) — a drift between the two would show in
/// `exploration_sim_harness`, not here.
constexpr int32_t lapse_exploration_epoch_year = 1200;

/// BL-943 — sample a corridor's straight line (TILE units) against the
/// water/land band `finish_history_lapse` bakes for the terrain base, so a
/// fleet/caravan exemplar reads the same ground fact a bridge glyph does
/// rather than assuming every corridor is land. Majority-water along a
/// handful of even samples, not "any water", because a bridge crossing one
/// river tile is not a sea corridor.
float lapse_unwrap_col(float c0, float c1, int gw)
{
    // THE MAP IS A CYLINDER AND A CORRIDOR IS THE SHORT WAY ROUND (Ben,
    // 2026-09-16, watching the lapse: "we see these roads as wrapping around
    // the whole view, when actually they are very small roads"). A corridor
    // between anchors at column 5 and column 255 on a 261-wide world is ten
    // columns long over the seam and 250 the other way; every reader here took
    // the raw columns, so the drawn line, the water sample and the bridge test
    // all described the long way — the one path the corridor is NOT.
    //
    // The fix is one function and it belongs at the BAKE, not the draw: this
    // returns `c1` moved by a whole world width when that is the shorter run,
    // so the stored segment is the real corridor and everything downstream of
    // it — over_water, bridges, the exemplar's midpoint — reads the short path
    // for free. The result can sit outside [0, gw); the draw handles that (a
    // second, shifted copy, clipped to the map) and the water sample already
    // folds a sampled column back into range.
    if (gw <= 0) return c1;
    const float w = static_cast<float>(gw);
    if (c1 - c0 >  w * 0.5f) return c1 - w;
    if (c1 - c0 < -w * 0.5f) return c1 + w;
    return c1;
}

bool lapse_corridor_over_water(const std::vector<uint8_t>& band, int gw, int gh,
                               float c0, float r0, float c1, float r1)
{
    if (band.empty() || gw <= 0 || gh <= 0) return false;
    constexpr int samples = 7;
    int water = 0, total = 0;
    for (int i = 0; i <= samples; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        int c = static_cast<int>(std::lround(c0 + (c1 - c0) * t));
        int r = static_cast<int>(std::lround(r0 + (r1 - r0) * t));
        if (r < 0 || r >= gh) continue;
        if (c < 0) c += gw;
        if (c >= gw) c -= gw;
        ++total;
        if (band[static_cast<std::size_t>(r * gw + c)] == 0xFFu) ++water;
    }
    return total > 0 && water * 2 > total;
}

/// BL-943 — one fleet or caravan exemplar, at a corridor's midpoint. `tier`
/// (1 Track, 2 Road, 3 Post Road) grows the mark and rings it, so the road
/// ladder's three rungs never draw the same mark; a trade corridor carries no
/// ladder and is always drawn at the base rung with no ring, in the trade
/// network's own cool hue rather than the road network's warm one, so the two
/// kinds of corridor never read as one repainted idiom.
void draw_lapse_exemplar(ImDrawList* dl, ImVec2 at, float scale, bool over_water,
                         int tier, bool is_trade, int alpha)
{
    tier = std::clamp(tier, 1, 3);
    const float base = std::clamp(scale * 0.55f, 1.6f, 3.2f);
    const float r = base * (0.7f + 0.3f * static_cast<float>(tier));
    const ImU32 hue  = is_trade ? col_trade_link : (tier == 1 ? col_road_track : col_road);
    const ImU32 fill = with_alpha(hue, alpha);
    const ImU32 ring = with_alpha(col_bright, alpha);

    if (over_water)
    {
        // A SAIL: one triangle, apex up — the smallest legible mark that
        // reads as a hull rather than a dot.
        dl->AddTriangleFilled({at.x, at.y - r * 1.2f},
                              {at.x - r * 0.85f, at.y + r * 0.7f},
                              {at.x + r * 0.85f, at.y + r * 0.7f}, fill);
    }
    else
    {
        // A CARAVAN: one diamond — distinct from the sail's triangle and from
        // the round seat/event marks drawn elsewhere on this map.
        dl->AddQuadFilled({at.x, at.y - r}, {at.x + r, at.y},
                          {at.x, at.y + r}, {at.x - r, at.y}, fill);
    }

    // THE ROAD RUNG, AS RINGS: Track draws none, Road draws one, Post Road
    // draws two — concentric on the mark rather than a bigger blob alone, so
    // the tier reads even where two similarly-sized marks sit close together.
    if (!is_trade)
        for (int k = 0; k < tier - 1; ++k)
            dl->AddCircle(at, r * (1.6f + 0.5f * static_cast<float>(k)), ring, 12, 1.2f);
}

/// The colour one OWNER index is drawn in on this surface, whichever round it
/// is. On the Culture round the owner is a culture and the record carries a
/// lineage palette (BL-919); on the Empires round it is a polity and the palette
/// is empty, so the identity slot applies. One function for the map row and the
/// board swatch, so a row and its ground cannot disagree on either round.
/// BL-1087: the lineage palette is built on every round now (the polity
/// rounds read it for the wedge and the base), so the ROUND's flag, not the
/// palette's emptiness, says which id space `owner` is in.
ImU32 owner_colour(const history_lapse& h, uint16_t owner, int year = 0x7FFFFFFF)
{
    if (h.owners_are_cultures && owner < h.culture_colour.size())
        return static_cast<ImU32>(h.culture_colour[owner]);
    return polity_colour(h, owner, year);
}

} // namespace

void build_lineage_palette(history_lapse& h, const std::vector<int32_t>& parent,
                           const std::vector<int32_t>* folded_into)
{
    const std::size_t n = parent.size();
    h.culture_family.assign(n, -1);
    h.culture_hue.assign(n, 0.0f);
    h.culture_depth.assign(n, 0);
    h.culture_colour.assign(n, 0u);
    if (n == 0) return;

    // Roots first, spread evenly: the k-th cradle culture in index order owns
    // the k-th wedge. Even spacing rather than a hash is what makes two cradles'
    // ground read as two different families rather than two slots that happened
    // to land near each other.
    // BL-1017: the living tree only. A folded culture's absorber is always a
    // lower index that did not fold, so its colour is final by the time the
    // folded row reads it.
    const auto folded_to = [&](std::size_t i) -> int32_t {
        if (folded_into == nullptr || i >= folded_into->size()) return -1;
        const int32_t f = (*folded_into)[i];
        return (f >= 0 && static_cast<std::size_t>(f) < i) ? f : -1;
    };

    // BL-1087: the wedge walk is the pure `lineage_wedges_of` — the same
    // function the sweep and the load path call — so a family's wedge here
    // and the wedge its realms' slots encode are one number.
    const lineage_wedges lw = lineage_wedges_of(parent, folded_into);
    h.culture_wedge = lw.wedge;
    h.family_count  = lw.count;
    const int   roots = lw.count;
    const float wedge = 1.0f / static_cast<float>(roots > 0 ? roots : 1);
    // The fixed step a daughter takes off its parent, and the furthest any
    // member may drift from the root: inside the wedge with a margin, so the
    // last member of one family and the first of the next never touch.
    const float step  = wedge / 6.0f;
    const float bound = wedge * 0.40f;

    // Deviation from the root hue, tracked separately so the bound is applied
    // to the whole lineage's drift and not to one generation's step.
    std::vector<float> deviation(n, 0.0f);
    // Sibling rank: how many earlier daughters this parent already has.
    std::vector<int32_t> children(n, 0);

    for (std::size_t i = 0; i < n; ++i)
    {
        if (const int32_t f = folded_to(i); f >= 0)
        {
            const std::size_t fi = static_cast<std::size_t>(f);
            h.culture_family[i] = h.culture_family[fi];
            h.culture_hue[i]    = h.culture_hue[fi];
            h.culture_depth[i]  = h.culture_depth[fi];
            h.culture_colour[i] = h.culture_colour[fi];
            deviation[i]        = deviation[fi];
            continue; // No wedge, no sibling step: the name did not outlive the round.
        }
        const int32_t p = parent[i];
        // A root, or a malformed link (a parent at or above its daughter) that is
        // treated as one rather than followed — the tree contract is that ids
        // are handed out in arrival order, and a violation is a root, not a loop.
        if (p < 0 || static_cast<std::size_t>(p) >= i)
        {
            h.culture_family[i] = static_cast<int32_t>(i);
            h.culture_hue[i]    = wedge * static_cast<float>(lw.wedge[i] < 0 ? 0 : lw.wedge[i]);
            h.culture_depth[i]  = 0;
            deviation[i]        = 0.0f;
        }
        else
        {
            const std::size_t pi = static_cast<std::size_t>(p);
            const int32_t rank = children[pi]++;
            // Alternate sides of the parent: +1, -1, +2, -2 ... steps.
            const float signed_steps = static_cast<float>(rank / 2 + 1)
                                     * ((rank % 2 == 0) ? 1.0f : -1.0f);
            float dev = deviation[pi] + signed_steps * step;
            if (dev >  bound) dev =  bound;
            if (dev < -bound) dev = -bound;
            deviation[i]        = dev;
            h.culture_family[i] = h.culture_family[pi];
            h.culture_depth[i]  = h.culture_depth[pi] + 1;
            const std::size_t root = static_cast<std::size_t>(h.culture_family[i]);
            float hue = h.culture_hue[root] + dev;
            hue -= static_cast<float>(static_cast<int>(hue));
            if (hue < 0.0f) hue += 1.0f;
            h.culture_hue[i] = hue;
        }
        h.culture_colour[i] = static_cast<uint32_t>(
            palette::lineage_colour(h.culture_hue[i], h.culture_depth[i]));
    }
}

std::string lapse_year_label(int year)
{
    char buf[32];
    if (year < 0) std::snprintf(buf, sizeof buf, "%d BCE", -year);
    else          std::snprintf(buf, sizeof buf, "%d CE",   year);
    return buf;
}

void finish_history_lapse(history_lapse& h, const uint8_t* packed, std::size_t packed_len,
                          const uint16_t* terrain, std::size_t terrain_len)
{
    if (h.derived() || h.empty()) return;
    if (h.grid_w <= 0 || h.grid_h <= 0) return;

    const std::size_t n = static_cast<std::size_t>(h.grid_w)
                        * static_cast<std::size_t>(h.grid_h);
    if (packed == nullptr || packed_len != n) return; // the surface is not built yet

    const int gw = h.grid_w, gh = h.grid_h;

    h.tile_region.assign(n, -1);
    h.region_area.assign(h.region_col.size(), 0);

    // A multi-source breadth-first walk from every region anchor at once, over
    // LAND only. Every tile ends on the region whose anchor is nearest by walking
    // distance rather than by straight line — which is the honest measure here,
    // because a region across a strait is not next door however close it looks.
    // BL-1087: the walk itself is `nearest_region_raster` (world/
    // polity_identity.hpp) — the same function the Begin/load colour table
    // and the sweep's clash column call — so the raster the slots were
    // assigned over is the raster they are re-derived over. Columns WRAP as
    // the equator does; rows do not (hard_coded_world.hpp § the homeworld
    // grid); two anchors on one tile resolve to the lower index.
    {
        std::vector<uint8_t> water(n, 0);
        for (std::size_t i = 0; i < n; ++i)
            water[i] = is_water(preview_substrate(packed[i])) ? 1 : 0;
        h.tile_region = nearest_region_raster(water, gw, gh, h.region_col, h.region_row);
        if (h.tile_region.size() != n) h.tile_region.assign(n, -1);
    }

    for (std::size_t i = 0; i < n; ++i)
    {
        const int32_t r = h.tile_region[i];
        if (r < 0) continue;
        ++h.region_area[static_cast<std::size_t>(r)];
        ++h.claimed_land;
    }

    // Every polity's seat: the first region the record ever shows it holding.
    // `changes` is in year order, so the first appearance IS the founding claim.
    for (const owner_change& c : h.lapse.changes)
    {
        if (c.owner == owner_none) continue;
        if (h.polity_seat.size() <= c.owner)
            h.polity_seat.resize(static_cast<std::size_t>(c.owner) + 1, -1);
        if (h.polity_seat[c.owner] < 0)
            h.polity_seat[c.owner] = static_cast<int32_t>(c.region);
    }

    // --- The terrain base, baked once (BL-915) -------------------------------
    //
    // Run-merged along each row by base KIND, land only, in tile units. This is
    // the "cached draw list" the item asks for in the form the map can actually
    // use: the pane can change size between frames, so what is cached is the
    // merge, and the scale is applied at draw time. A missing terrain raster
    // (the async build not landed, or an older caller) bakes flat land, so the
    // map is never blocked on the ground.
    h.base_runs.clear();
    h.river_segs.clear();
    h.relief_segs.clear();
    const bool have_terrain = (terrain != nullptr && terrain_len == n);

    // The base band per tile, 0xFF for water. Computed once so the run merge
    // and the relief edges below read the same thing.
    std::vector<uint8_t> band(n, 0xFF);
    for (std::size_t i = 0; i < n; ++i)
    {
        if (is_water(preview_substrate(packed[i]))) continue;
        band[i] = have_terrain ? base_kind(lapse_landform(terrain[i])) : 0;
    }

    for (int r = 0; r < gh; ++r)
    {
        int c = 0;
        while (c < gw)
        {
            const uint8_t kind = band[static_cast<std::size_t>(r * gw + c)];
            if (kind == 0xFF) { ++c; continue; }
            int e = c + 1;
            while (e < gw && band[static_cast<std::size_t>(r * gw + e)] == kind) ++e;
            h.base_runs.push_back({static_cast<uint16_t>(r), static_cast<uint16_t>(c),
                                   static_cast<uint16_t>(e), kind});
            c = e;
        }
    }

    // RELIEF EDGES. Only MOUNTAIN (band 3) gets a rim, and only where its north
    // or south neighbour is LOW ground — flat, valley, a cut, or water; a step
    // down to highland is left to the band colours. The landform field is noisy
    // at tile grain, so rimming every highland step drew scanlines across whole
    // continents (measured: 978 rims for 1,966 base runs) — a rim is worth
    // drawing only where the drop is a real one. The north rim is lit and the
    // south rim shadowed, the same top-left light every canvas here assumes.
    // Merged along the row.
    {
        const auto elevated = [](uint8_t b) { return b == 3; };
        const auto height_of = [](uint8_t b) { return (b == 0xFF || b == 4) ? 0 : (b >= 2 ? 2 : 0); };
        for (int r = 0; r < gh; ++r)
        {
            for (int lit = 1; lit >= 0; --lit)
            {
                const int nr = lit ? r - 1 : r + 1;
                int c = 0;
                while (c < gw)
                {
                    const uint8_t b = band[static_cast<std::size_t>(r * gw + c)];
                    const bool rim = elevated(b)
                        && (nr < 0 || nr >= gh
                            || height_of(band[static_cast<std::size_t>(nr * gw + c)]) < height_of(b));
                    if (!rim) { ++c; continue; }
                    int e = c + 1;
                    while (e < gw)
                    {
                        const uint8_t b2 = band[static_cast<std::size_t>(r * gw + e)];
                        const bool rim2 = elevated(b2)
                            && (nr < 0 || nr >= gh
                                || height_of(band[static_cast<std::size_t>(nr * gw + e)]) < height_of(b2));
                        if (!rim2) break;
                        ++e;
                    }
                    h.relief_segs.push_back({static_cast<uint16_t>(r), static_cast<uint16_t>(c),
                                             static_cast<uint16_t>(e), static_cast<uint8_t>(lit)});
                    c = e;
                }
            }
        }
    }

    // Rivers: one segment per DOWNSTREAM edge, from the tile's centre to its
    // downstream neighbour's — so every river edge is drawn exactly once, by the
    // tile upstream of it, and consecutive edges chain into a polyline. The
    // river bits are keyed by odd-r hex side (river_generation.hpp), so the
    // neighbour is read off the shared side table rather than a square-grid
    // guess. A segment that would cross the wrap seam is dropped: a line across
    // the whole map is not a river.
    if (have_terrain)
    {
        for (int r = 0; r < gh; ++r)
            for (int c = 0; c < gw; ++c)
            {
                const uint16_t t = terrain[static_cast<std::size_t>(r * gw + c)];
                const uint8_t down = lapse_river_downstream(t) & lapse_river_edges(t);
                if (down == 0) continue;
                for (int side = 0; side < 6; ++side)
                {
                    if ((down & (1u << side)) == 0) continue;
                    const hex_neighbors::coord nb = hex_neighbors::neighbour(c, r, side);
                    if (nb.gy < 0 || nb.gy >= gh) continue;
                    if (nb.gx < 0 || nb.gx >= gw) continue; // the wrap seam
                    h.river_segs.push_back({static_cast<int16_t>(c),     static_cast<int16_t>(r),
                                            static_cast<int16_t>(nb.gx), static_cast<int16_t>(nb.gy)});
                }
            }
    }

    // --- Promoted road corridors (BL-917), baked once -------------------------
    //
    // Built from `road_promoted` events alone, one segment per DISTINCT (a, b)
    // pair the events ever name, anchor centre to anchor centre. A corridor the
    // era only walked never gets an event and never gets a segment here — see
    // `lapse_road_seg`. The Culture round's record carries no such events, so
    // this loop leaves `road_segs` empty there without a round flag to check.
    h.road_segs.clear();
    for (const lapse_event& e : h.lapse.events)
    {
        if (e.kind != static_cast<uint8_t>(lapse_event_kind::road_promoted)) continue;
        if (e.region == lapse_event_none || e.other == lapse_event_none) continue;
        const uint16_t a = e.region, b = e.other; // note_corridor: region = lo, other = hi
        if (static_cast<std::size_t>(a) >= h.region_col.size()
         || static_cast<std::size_t>(b) >= h.region_col.size()) continue;

        auto it = std::find_if(h.road_segs.begin(), h.road_segs.end(),
                               [&](const lapse_road_seg& s) { return s.region_a == a && s.region_b == b; });
        if (it == h.road_segs.end())
        {
            lapse_road_seg seg;
            seg.region_a = a;
            seg.region_b = b;
            seg.c0 = static_cast<float>(h.region_col[a]) + 0.5f;
            seg.r0 = static_cast<float>(h.region_row[a]) + 0.5f;
            seg.c1 = lapse_unwrap_col(seg.c0, static_cast<float>(h.region_col[b]) + 0.5f, gw);
            seg.r1 = static_cast<float>(h.region_row[b]) + 0.5f;
            seg.year_track = e.year;
            if (e.polity >= 2) seg.year_road      = e.year; // robust to a corridor's first event already being Road
            if (e.polity >= 3) seg.year_post_road = e.year; // ...or already Post Road (BL-940/BL-943)
            seg.over_water = lapse_corridor_over_water(band, gw, gh, seg.c0, seg.r0, seg.c1, seg.r1);
            // Bridges: where this corridor's straight line crosses a river
            // edge — a pure geometric fact, tested once against every river
            // segment already baked above.
            for (const lapse_river_seg& r : h.river_segs)
            {
                float ix = 0.0f, iy = 0.0f;
                if (lapse_segments_cross(seg.c0, seg.r0, seg.c1, seg.r1,
                                         static_cast<float>(r.c0), static_cast<float>(r.r0),
                                         static_cast<float>(r.c1), static_cast<float>(r.r1),
                                         ix, iy))
                    seg.bridges.push_back({ix, iy});
            }
            h.road_segs.push_back(std::move(seg));
        }
        else if (e.polity >= 3)
        {
            it->year_post_road = e.year; // BL-940/BL-943: a later crossing to Post Road
        }
        else if (e.polity >= 2)
        {
            it->year_road = e.year; // the second crossing this pair can ever get
        }
    }

    // --- Amicable cross-border trade corridors (BL-925), baked once --------
    //
    // Built from `trade_link_opened` / `trade_link_closed` events, the same
    // "one segment per distinct (a, b) pair" idiom as the road bake just
    // above, ADDED BESIDE IT rather than folded in: a trade link is not a
    // road (it can close and reopen, so it carries a list of spans rather
    // than a single promotion year) and the two are unrelated facts about a
    // corridor. The Culture round's record carries no such events, so this
    // loop leaves `trade_segs` empty there without a round flag to check.
    h.trade_segs.clear();
    for (const lapse_event& e : h.lapse.events)
    {
        const bool opened = e.kind == static_cast<uint8_t>(lapse_event_kind::trade_link_opened);
        const bool closed = e.kind == static_cast<uint8_t>(lapse_event_kind::trade_link_closed);
        if (!opened && !closed) continue;
        if (e.region == lapse_event_none || e.other == lapse_event_none) continue;
        const uint16_t a = e.region, b = e.other; // note_event: region/other = the corridor's ends
        if (static_cast<std::size_t>(a) >= h.region_col.size()
         || static_cast<std::size_t>(b) >= h.region_col.size()) continue;

        auto it = std::find_if(h.trade_segs.begin(), h.trade_segs.end(),
                               [&](const lapse_trade_seg& s) { return s.region_a == a && s.region_b == b; });
        if (it == h.trade_segs.end())
        {
            lapse_trade_seg seg;
            seg.region_a = a;
            seg.region_b = b;
            seg.c0 = static_cast<float>(h.region_col[a]) + 0.5f;
            seg.r0 = static_cast<float>(h.region_row[a]) + 0.5f;
            seg.c1 = lapse_unwrap_col(seg.c0, static_cast<float>(h.region_col[b]) + 0.5f, gw);
            seg.r1 = static_cast<float>(h.region_row[b]) + 0.5f;
            seg.over_water = lapse_corridor_over_water(band, gw, gh, seg.c0, seg.r0, seg.c1, seg.r1);
            h.trade_segs.push_back(std::move(seg));
            it = std::prev(h.trade_segs.end());
        }
        if (opened)
        {
            // A well-formed record never opens twice without a close between
            // (the sim's own "note only on change" idiom), but a fixture or a
            // future caller doing something odd is handled by simply not
            // opening a second span on top of one already open.
            if (it->spans.empty() || it->spans.back().year_close != 0x7FFFFFFF)
                it->spans.push_back({e.year, 0x7FFFFFFF});
        }
        else // closed
        {
            if (!it->spans.empty() && it->spans.back().year_close == 0x7FFFFFFF)
                it->spans.back().year_close = e.year;
        }
    }

    // --- Sea lanes (BL-1097), baked once -------------------------------------
    //
    // Built from `sea_lane_opened` events alone, one segment per DISTINCT
    // (a, b) pair, anchor centre to anchor centre — the road bake's idiom on
    // the water. A leg the era only crossed once or twice never gets an event
    // and never gets a segment. The lane tier has one rung, so the first event
    // for a pair is the whole story and a second (a resumed span re-noting
    // would be a sim bug, not a fixture case) is ignored.
    h.lane_segs.clear();
    for (const lapse_event& e : h.lapse.events)
    {
        if (e.kind != static_cast<uint8_t>(lapse_event_kind::sea_lane_opened)) continue;
        if (e.region == lapse_event_none || e.other == lapse_event_none) continue;
        const uint16_t a = e.region, b = e.other; // note_sea_leg: region = lo, other = hi
        if (static_cast<std::size_t>(a) >= h.region_col.size()
         || static_cast<std::size_t>(b) >= h.region_col.size()) continue;
        const bool seen = std::any_of(h.lane_segs.begin(), h.lane_segs.end(),
                                      [&](const lapse_lane_seg& s) { return s.region_a == a && s.region_b == b; });
        if (seen) continue;
        lapse_lane_seg seg;
        seg.region_a  = a;
        seg.region_b  = b;
        seg.c0 = static_cast<float>(h.region_col[a]) + 0.5f;
        seg.r0 = static_cast<float>(h.region_row[a]) + 0.5f;
        seg.c1 = lapse_unwrap_col(seg.c0, static_cast<float>(h.region_col[b]) + 0.5f, gw);
        seg.r1 = static_cast<float>(h.region_row[b]) + 0.5f;
        seg.year_open = e.year;
        h.lane_segs.push_back(seg);
    }

    // --- BL-1092: the kin arrows, baked once ---------------------------------
    //
    // ONE ARROW PER FOUNDING WITH SOMEWHERE TO COME FROM (Ben, 2026-09-24,
    // rulings R11; COLONISATION.md sec The route record, region grain). The
    // change list is walked in order; a region's FIRST change is its founding,
    // and the arrow runs from the founding people's previous region -- the
    // one it most recently founded before this -- or, for a people's first
    // founding, from its PARENT's most recent region, read off the
    // `culture_split` events (`polity` = daughter, `other` = parent). A later
    // change on a region already founded is a reculture (the split-year
    // hue change BL-1092 emits) and draws no arrow: nobody moved. A cradle's
    // first region has no parent and no previous region, so it draws none.
    // Dashed where the straight line between the anchors CROSSES water --
    // not the road bake's majority rule, which is built for a caravan that
    // walks a mostly-dry corridor and reads a one-tile strait as land. The
    // line is sampled tile by tile and dashes from TWO interior water tiles
    // up: a crude hop is a bounded crossing of up to three
    // (`colonisation_max_hop_tiles`), while a coast-hugging people's
    // consecutive foundings graze a bay by one tile almost every time --
    // measured 2026-09-25 on library seed 13, where any-water dashed 193 of
    // 249 arrows and the hops were lost in them. The doc's words are "dashed
    // where the line between the two anchors crosses water" (COLONISATION.md
    // sec The route record), honest about its grain either way; this reads
    // "crosses" as more than a graze. Nothing on a polity round: its record
    // carries no `culture_split` and its owners are realms, whose foundings
    // are seats, not routes.
    h.kin_segs.clear();
    if (h.owners_are_cultures)
    {
        const auto crosses_water = [&](float c0, float r0, float c1, float r1) {
            const float dc = c1 - c0, dr = r1 - r0;
            const int   steps = std::max(2, static_cast<int>(std::ceil(std::max(std::fabs(dc), std::fabs(dr)))));
            int water = 0;
            for (int i = 1; i < steps; ++i) // interior samples only: the ends are anchors on land
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                int c = static_cast<int>(std::lround(c0 + dc * t - 0.5f));
                int r = static_cast<int>(std::lround(r0 + dr * t - 0.5f));
                if (r < 0 || r >= gh) continue;
                if (c < 0) c += gw;
                if (c >= gw) c -= gw;
                if (band[static_cast<std::size_t>(r * gw + c)] == 0xFFu && ++water >= 2) return true;
            }
            return false;
        };
        std::vector<int32_t> parent_of;   // culture -> parent, from the splits
        for (const lapse_event& e : h.lapse.events)
        {
            if (e.kind != static_cast<uint8_t>(lapse_event_kind::culture_split)) continue;
            if (e.polity == lapse_event_none || e.other == lapse_event_none) continue;
            if (parent_of.size() <= e.polity) parent_of.resize(static_cast<std::size_t>(e.polity) + 1, -1);
            parent_of[e.polity] = static_cast<int32_t>(e.other);
        }
        std::vector<int32_t> last_region; // culture -> the region it most recently founded
        std::vector<uint8_t> founded(h.region_col.size(), 0u);
        const auto last_of = [&](uint16_t c) -> int32_t {
            return (static_cast<std::size_t>(c) < last_region.size()) ? last_region[c] : -1;
        };
        for (const owner_change& c : h.lapse.changes)
        {
            if (c.owner == owner_none) continue;
            if (static_cast<std::size_t>(c.region) >= h.region_col.size()) continue;
            if (last_region.size() <= c.owner)
                last_region.resize(static_cast<std::size_t>(c.owner) + 1, -1);
            if (founded[c.region])
            {
                // A reculture: the daughter's most recent ground is this
                // region from here on, but nobody walked anywhere.
                last_region[c.owner] = static_cast<int32_t>(c.region);
                continue;
            }
            founded[c.region] = 1u;
            int32_t from = last_of(c.owner);
            if (from < 0 && static_cast<std::size_t>(c.owner) < parent_of.size())
            {
                // The people's first founding: its kin are its parent's, so
                // the arrow leaves the parent's most recent region. A
                // grandparent is not walked -- a parent with no region yet
                // is a fixture case, not a world.
                const int32_t p = parent_of[c.owner];
                if (p >= 0) from = last_of(static_cast<uint16_t>(p));
            }
            last_region[c.owner] = static_cast<int32_t>(c.region);
            if (from < 0 || from == static_cast<int32_t>(c.region)) continue;
            lapse_kin_seg seg;
            seg.region_from = static_cast<uint16_t>(from);
            seg.region_to   = c.region;
            seg.culture     = c.owner;
            seg.year        = c.year;
            seg.c0 = static_cast<float>(h.region_col[static_cast<std::size_t>(from)]) + 0.5f;
            seg.r0 = static_cast<float>(h.region_row[static_cast<std::size_t>(from)]) + 0.5f;
            seg.c1 = lapse_unwrap_col(seg.c0, static_cast<float>(h.region_col[c.region]) + 0.5f, gw);
            seg.r1 = static_cast<float>(h.region_row[c.region]) + 0.5f;
            seg.over_water = crosses_water(seg.c0, seg.r0, seg.c1, seg.r1);
            h.kin_segs.push_back(seg);
        }
    }

    // --- The industry layer (BL-1080), baked once ------------------------------
    //
    // A region's first `furnace_lit` is its crossing; the list is ascending, so
    // the first seen is the earliest. Industry points are read off the samples
    // at draw time (they are a series, not a bake); here only whether any exist.
    //
    // BL-1100: a REALM's crossing (`rung_crossed`, at its capital) marks the
    // capital the same way from the same year (STARTUP.md § Round 6, "The rung
    // crossing is narrated") -- a generated world lights no region furnace,
    // so on the shipped world this is the only mark the layer carries. On a
    // seed where no realm crosses the layer stays honestly empty.
    h.region_lit_year.assign(h.region_col.size(), INT32_MAX);
    for (const lapse_event& e : h.lapse.events)
    {
        if (e.kind != static_cast<uint8_t>(lapse_event_kind::furnace_lit)
         && e.kind != static_cast<uint8_t>(lapse_event_kind::rung_crossed)) continue;
        if (e.region == lapse_event_none || e.region >= h.region_lit_year.size()) continue;
        int32_t& y = h.region_lit_year[e.region];
        if (e.year < y) y = e.year;
    }
    h.industry_recorded     = false;
    h.industry_density_peak = 0.0;
    for (const polity_sample& s : h.lapse.samples)
    {
        if (s.industry_points <= 0) continue;
        h.industry_recorded = true;
        const double d = static_cast<double>(s.industry_points)
                       / static_cast<double>(std::max<int>(1, s.regions));
        h.industry_density_peak = std::max(h.industry_density_peak, d);
    }

    // The hard-border bitmap (BL-1090): the hysteresis walk over the steps,
    // from the carried state. Empty on the Culture round (no samples).
    lapse_hard_walk(h);

    // The identity: pinned slots, family wedges, the ratchet's moments and the
    // capital fold (BL-1087/BL-1088; world/polity_identity.hpp holds the rule).
    assign_polity_colours(h);
}

void assign_polity_colours(history_lapse& h)
{
    h.polity_slot.clear();
    h.identity = polity_identity{};
    h.polity_wedge.clear();
    h.capital_moves.clear();
    if (h.tile_region.empty()) return;

    // 1. REGION adjacency off the nearest-region raster, and the polity
    //    adjacency the module folds from it across the WHOLE record (two
    //    polities are adjacent if they ever held neighbouring regions at the
    //    same time) -- the same graph BL-915's greedy walk always coloured.
    const std::vector<std::vector<int32_t>> region_nbrs =
        region_adjacency(h.tile_region, h.grid_w, h.grid_h, h.region_col.size());

    // 2. THE FOUNDING FAMILY per polity, read UI-side (BL-1087 R2): the
    //    plurality people of the realm's seat at its first recorded year,
    //    through the lineage tree's wedges. Empty when the record carries no
    //    culture tree (a bare harness record): every realm then reads the
    //    fallback table, as before this item.
    const std::vector<int32_t> first_region = polity_first_region(h.lapse);
    const std::vector<int32_t> seat_region  = polity_seat_region(h.lapse, first_region);
    if (h.family_count > 0)
    {
        const std::vector<int32_t> culture = polity_founding_culture(h.lapse, seat_region, first_region);
        h.polity_wedge.assign(culture.size(), -1);
        for (std::size_t p = 0; p < culture.size(); ++p)
            if (culture[p] >= 0 && static_cast<std::size_t>(culture[p]) < h.culture_wedge.size())
                h.polity_wedge[p] = h.culture_wedge[static_cast<std::size_t>(culture[p])];
    }

    // 3. THE ASSIGNMENT: pins kept, the clash rule, the dead-ground rule, then
    //    the greedy walk in founding order (world/polity_identity.hpp).
    polity_identity_input in;
    in.rec          = &h.lapse;
    in.region_nbrs  = &region_nbrs;
    in.family       = h.family_count > 0 ? &h.polity_wedge : nullptr;
    in.family_count = h.family_count;
    in.pins         = h.has_pins ? &h.pins : nullptr;
    h.identity    = assign_polity_identity(in);
    h.polity_slot = h.identity.slot;

    // 4. THE CAPITAL FOLD (BL-1088): baked once; `lapse_polity_capital` reads it.
    h.capital_moves = polity_capital_moves(h.lapse);
    // The instrument (what the pins had to do at this round's opening) stays
    // on `h.identity` for the verify API's `history_identity()`; the sweep's
    // column is the reading across the library. Not printed here: the live
    // tap re-derives every 0.2 s while a round runs.
}

polity_pins lapse_pins_for_successor(const history_lapse& prev)
{
    if (prev.owners_are_cultures || prev.polity_slot.empty()) return polity_pins{};
    return pins_from(prev.identity, prev.lapse, prev.has_pins ? &prev.pins : nullptr);
}

int lapse_polity_rung(const history_lapse& h, uint16_t polity, int year)
{
    return polity_rung_at(h.identity, polity, year);
}

std::string lapse_polity_name(const history_lapse& h, uint16_t polity)
{
    if (polity == lapse_event_none) return "an unnamed realm";
    if (static_cast<std::size_t>(polity) < h.lapse.polity_name.size()
        && !h.lapse.polity_name[polity].empty())
        return h.lapse.polity_name[polity];
    if (static_cast<std::size_t>(polity) < h.polity_seat.size() && h.polity_seat[polity] >= 0
        && static_cast<std::size_t>(h.polity_seat[polity]) < h.region_name.size()
        && !h.region_name[static_cast<std::size_t>(h.polity_seat[polity])].empty())
        return h.region_name[static_cast<std::size_t>(h.polity_seat[polity])];
    return "an unnamed realm";
}

int32_t lapse_polity_capital(const history_lapse& h, uint16_t polity, int year)
{
    const int32_t first = (static_cast<std::size_t>(polity) < h.polity_seat.size())
                              ? h.polity_seat[polity] : -1;
    return polity_capital_at(h.capital_moves, polity, year, first);
}

// ---------------------------------------------------------------------------
// The map
// ---------------------------------------------------------------------------


uint32_t lapse_owner_colour(const history_lapse& h, uint16_t owner, int year)
{
    return static_cast<uint32_t>(owner_colour(h, owner, year));
}

float lapse_carry_fade(const history_lapse& h, int year)
{
    // A TENTH OF THE ROUND'S OWN SPAN, so the hand-over reads the same however
    // long the span is and however fast the viewer is playing it — tying it to
    // seconds would make the 270-second pace show the old ground for a minute
    // and the 90-second pace for twenty seconds, which is the sort of accident
    // a reader would take for meaning.
    if (h.carry_colour.empty() || h.lapse.years <= 0) return 0.0f;
    const float over = static_cast<float>(h.lapse.years) * 0.10f;
    const float gone = static_cast<float>(year - h.lapse.start_year);
    if (gone <= 0.0f)  return 1.0f;
    if (gone >= over)  return 0.0f;
    return 1.0f - gone / over;
}

void draw_lapse_map(const history_lapse& h, const std::vector<uint16_t>& slice,
                    int year)
{
    const ImVec2 avail  = ImGui::GetContentRegionAvail();
    ImDrawList*  dl     = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    if (avail.x <= 1.0f || avail.y <= 1.0f) return;

    if (!h.derived())
    {
        // Nothing painted before the record lands — the shell's own background
        // is the right empty state, and flooding the pane with `col_void` was
        // the same black frame the drawn map has now dropped.
        ImGui::Dummy(avail);
        return;
    }

    const int gw = h.grid_w, gh = h.grid_h;

    // Fit the raster into the pane, aspect preserved: a political map stretched
    // to a pane is a map of a different world's shape.
    //
    // NO LETTERBOX (Ben, 2026-09-09: "so that our timelapse doesn't contain
    // black bars"). The pane used to be flooded with `col_void` and the map laid
    // on top of it, so a 261x121 raster in a much taller pane wore a black band
    // above and below — a frame around the subject that said nothing. Only the
    // map's own rect is painted now, and the shell's background carries the
    // rest, so the map reads as a map rather than as a picture of one.
    //
    // NUDGED RIGHT (Ben, same): centred in the pane it sat visually left of the
    // space it was given, because the round's left column ends well before the
    // pane begins. The offset is authored rather than derived — it is a framing
    // judgement about this screen, and deriving it from some other quantity
    // would only disguise that. THE RULE LIVES IN `lapse_map_frame` (BL-1091):
    // the wizard's globe dissolve stamps its year over this same corner, and
    // one framing shared beats two that agree by luck.
    float fx = 0.0f, fy = 0.0f, scale = 0.0f;
    lapse_map_frame(gw, gh, avail.x, avail.y, fx, fy, scale);
    const float mw = scale * static_cast<float>(gw);
    const float mh = scale * static_cast<float>(gh);
    const ImVec2 tl{origin.x + fx, origin.y + fy};

    dl->AddRectFilled(tl, {tl.x + mw, tl.y + mh}, col_sea);

    // TILE EDGES SNAP TO WHOLE PIXELS. At ~4.6 px per tile a rect edge lands on
    // a fraction, and the rasteriser's anti-aliasing then either leaves a
    // hairline seam between rows (edge-exact rects) or doubles the tint's alpha
    // along a hairline (rects that bleed) — both drew stripes across the whole
    // map. Snapped, neighbouring rows and runs share one integer edge exactly:
    // no seam, no overlap, and no bleed needed anywhere below.
    const auto px = [&](float c) { return std::floor(tl.x + c * scale); };
    const auto py = [&](float r) { return std::floor(tl.y + r * scale); };

    // ── 1. THE GROUND (BL-915). The baked base runs, scaled and emitted: land
    //    by landform band, then the lit north rim and shadowed south rim of
    //    every range so it reads as relief rather than as a paler patch.
    //    Nothing here depends on the slice, and nothing here is merged per
    //    frame — see finish_history_lapse. ──
    int prims = 0;
    for (const lapse_base_run& b : h.base_runs)
    {
        const float x0 = px(static_cast<float>(b.c0));
        const float x1 = px(static_cast<float>(b.c1));
        const float y0 = py(static_cast<float>(b.row));
        const float y1 = py(static_cast<float>(b.row + 1));
        dl->AddRectFilled({x0, y0}, {x1, y1}, col_base[b.kind]);
        ++prims;
    }
    for (const lapse_relief_seg& s : h.relief_segs)
    {
        const float x0 = px(static_cast<float>(s.c0));
        const float x1 = px(static_cast<float>(s.c1));
        const float y  = s.lit ? py(static_cast<float>(s.row)) + 0.5f
                               : py(static_cast<float>(s.row + 1)) - 0.5f;
        dl->AddLine({x0, y}, {x1, y}, s.lit ? col_relief_lit : col_relief_dark, 1.0f);
        ++prims;
    }

    // ── 2. THE FILL, as a translucent TINT over the ground. One owner per tile,
    //    RUN-MERGED along the row: a political map is long runs of one owner,
    //    so this collapses ~31,500 cells to on the order of a thousand rects.
    //    (The 16-bit draw-index bound this was written against is met by the
    //    SDL3 backend's VtxOffset support; the merge is still what keeps a
    //    frame cheap.) Unclaimed land gets no tint: the bare ground IS the
    //    "nobody here yet" colour. ──
    //
    // Owner keys: -1 sea (nothing drawn), -2 wild, else the polity index. The
    // frontier is drawn between any two DIFFERENT keys on both axes -- the
    // `>= -2` tests below admit sea, so a coast is outlined too (which is what
    // every capture has shown); a hard realm's coast draws heavy like the
    // rest of its outline (BL-1090), since only the land side can be hard.
    const float carry_fade = lapse_carry_fade(h, year);

    // THE CULTURE BASE (BL-1087 R3; Ben, 2026-09-24, R7 "both"): on the
    // polity rounds a dull lineage-hue tint of each region's plurality people
    // goes down under everything political — the carry lands on it as it
    // fades, the realm fill sits over it — so unorganised peopled ground reads
    // as somebody's ground at 800 CE and the base is still there at 1900. One
    // fold of the record's culture changes per frame (the cost the ownership
    // slice already pays), then a colour per region. Nothing on the Culture
    // round, whose fill IS the culture, and nothing without a lineage tree.
    std::vector<uint32_t> base_colour;
    if (!h.owners_are_cultures && h.family_count > 0 && !h.lapse.culture_changes.empty())
    {
        const std::vector<int32_t> plural = culture_plurality_at(h.lapse, year);
        base_colour.assign(plural.size(), 0u);
        for (std::size_t r = 0; r < plural.size(); ++r)
        {
            const int32_t c = plural[r];
            if (c < 0 || static_cast<std::size_t>(c) >= h.culture_hue.size()) continue;
            base_colour[r] = static_cast<uint32_t>(
                palette::culture_base_colour(h.culture_hue[static_cast<std::size_t>(c)],
                                             h.culture_depth[static_cast<std::size_t>(c)]));
        }
    }
    constexpr int base_alpha = 110;

    std::vector<int32_t> row(static_cast<std::size_t>(gw));
    std::vector<int32_t> above(static_cast<std::size_t>(gw), -1);
    std::vector<char>    present; // owner -> holds ground in this slice

    // BL-1090: owner -> hard at this frame, read once per owner. The bitmap
    // lookup is one binary search over the steps; cached because a frontier
    // asks it per edge and a political map has thousands of edges.
    std::vector<int8_t> hard_cache; // -1 unread, 0 soft, 1 hard
    const auto hard_of = [&](int32_t key) -> bool {
        if (key < 0) return false; // sea and wild ground carry no share
        const std::size_t o = static_cast<std::size_t>(key);
        if (hard_cache.size() <= o) hard_cache.resize(o + 1, -1);
        if (hard_cache[o] < 0)
            hard_cache[o] = lapse_polity_hard(h, static_cast<uint16_t>(key), year) ? 1 : 0;
        return hard_cache[o] == 1;
    };

    for (int r = 0; r < gh; ++r)
    {
        for (int c = 0; c < gw; ++c)
        {
            const std::size_t i = static_cast<std::size_t>(r * gw + c);
            const int32_t reg = h.tile_region[i];
            if (reg < 0) { row[static_cast<std::size_t>(c)] = -1; continue; }
            const uint16_t o = (static_cast<std::size_t>(reg) < slice.size())
                                   ? slice[static_cast<std::size_t>(reg)] : owner_none;
            row[static_cast<std::size_t>(c)] = (o == owner_none) ? -2 : static_cast<int32_t>(o);
        }

        // Edge-exact on the snapped grid (see px/py): a TRANSLUCENT rect that
        // overlapped its neighbour would double its alpha along the overlap.
        const float y0 = py(static_cast<float>(r));
        const float y1 = py(static_cast<float>(r + 1));

        // THE ROUND BEFORE THIS ONE, UNDER EVERYTHING (Ben, 2026-09-16). The
        // first cut painted the carried frame only under ground nobody held,
        // which works for the Culture -> Empires hand-over (400 BCE is almost
        // all unorganised culture ground) and is INVISIBLE for Empires ->
        // Exploration, where every realm already holds its land at 1200 CE: the
        // carry had nowhere to show, and the new round fading in over nothing
        // just made the map dim. So the carried frame goes down first across the
        // whole row and this round's own fill goes over it — two translucent
        // tints over the same ground, one leaving as the other arrives.
        // THE BASE, run-merged by colour along the row exactly as the carry
        // is below: peopled ground in its people's dull lineage hue.
        if (!base_colour.empty())
        {
            int k = 0;
            while (k < gw)
            {
                const int32_t reg = h.tile_region[static_cast<std::size_t>(r * gw + k)];
                const uint32_t col = (reg >= 0 && static_cast<std::size_t>(reg) < base_colour.size())
                                         ? base_colour[static_cast<std::size_t>(reg)] : 0u;
                int k2 = k + 1;
                while (k2 < gw)
                {
                    const int32_t r2 = h.tile_region[static_cast<std::size_t>(r * gw + k2)];
                    const uint32_t c2 = (r2 >= 0 && static_cast<std::size_t>(r2) < base_colour.size())
                                            ? base_colour[static_cast<std::size_t>(r2)] : 0u;
                    if (c2 != col) break;
                    ++k2;
                }
                if (col != 0u)
                {
                    dl->AddRectFilled({px(static_cast<float>(k)), y0},
                                      {px(static_cast<float>(k2)), y1},
                                      with_alpha(static_cast<ImU32>(col), base_alpha));
                    ++prims;
                }
                k = k2;
            }
        }
        if (carry_fade > 0.0f)
        {
            int k = 0;
            while (k < gw)
            {
                const int32_t reg = h.tile_region[static_cast<std::size_t>(r * gw + k)];
                const uint32_t col = (reg >= 0 && static_cast<std::size_t>(reg) < h.carry_colour.size())
                                         ? h.carry_colour[static_cast<std::size_t>(reg)] : 0u;
                int k2 = k + 1;
                while (k2 < gw)
                {
                    const int32_t r2 = h.tile_region[static_cast<std::size_t>(r * gw + k2)];
                    const uint32_t c2 = (r2 >= 0 && static_cast<std::size_t>(r2) < h.carry_colour.size())
                                            ? h.carry_colour[static_cast<std::size_t>(r2)] : 0u;
                    if (c2 != col) break;
                    ++k2;
                }
                if (col != 0u)
                {
                    dl->AddRectFilled({px(static_cast<float>(k)), y0},
                                      {px(static_cast<float>(k2)), y1},
                                      with_alpha(static_cast<ImU32>(col),
                                                 static_cast<int>(tint_alpha * carry_fade)));
                    ++prims;
                }
                k = k2;
            }
        }
        int c = 0;
        while (c < gw)
        {
            const int32_t key = row[static_cast<std::size_t>(c)];
            int e = c + 1;
            while (e < gw && row[static_cast<std::size_t>(e)] == key) ++e;
            if (key >= 0)
            {
                const uint16_t o = static_cast<uint16_t>(key);
                if (present.size() <= o) present.resize(static_cast<std::size_t>(o) + 1, 0);
                present[o] = 1;
                // AND THE NEW ROUND FADES IN AS THE OLD ONE FADES OUT (Ben,
                // 2026-09-16). One cross-fade, not a cut with an underlay: the
                // carried ground is leaving at exactly the rate this round's
                // own holders are arriving, so the opening reads as the same
                // world changing hands rather than two surfaces swapping. A
                // round with nothing behind it (the migration, and any round
                // whose predecessor was never run) reads a fade of 0 from the
                // first frame, so it draws at full strength as it always did.
                dl->AddRectFilled({px(static_cast<float>(c)), y0},
                                  {px(static_cast<float>(e)), y1},
                                  with_alpha(owner_colour(h, o, year), // BL-1087: the ratchet at this year
                                             static_cast<int>(tint_alpha * (1.0f - carry_fade))));
                ++prims;
            }
            // The VERTICAL frontier: between this run and the one to its west.
            if (key >= -2 && c > 0)
            {
                const int32_t west = row[static_cast<std::size_t>(c - 1)];
                if (west >= -2 && west != key)
                {
                    const bool hk = hard_of(key), hw = hard_of(west);
                    if (hk || hw)
                    {
                        // BL-1090: A HARD BORDER. 2 px of dark centred on the
                        // tile edge (pixel columns c-1 and c), then a 1 px
                        // stroke one pixel INSIDE each hard realm's ground in
                        // its own colour -- so the weight reads as the realm's,
                        // not the edge's, and two hard neighbours each keep
                        // their own stroke on their own side.
                        const float x = px(static_cast<float>(c));
                        dl->AddLine({x, y0}, {x, y1}, col_frontier_hard, 2.0f);
                        ++prims;
                        if (hk)
                        {
                            dl->AddLine({x + 1.5f, y0}, {x + 1.5f, y1},
                                        owner_colour(h, static_cast<uint16_t>(key)), 1.0f);
                            ++prims;
                        }
                        if (hw)
                        {
                            dl->AddLine({x - 1.5f, y0}, {x - 1.5f, y1},
                                        owner_colour(h, static_cast<uint16_t>(west)), 1.0f);
                            ++prims;
                        }
                    }
                    else
                    {
                        // +0.5: a 1 px line centred ON the pixel column, not
                        // anti-aliased across two.
                        dl->AddLine({px(static_cast<float>(c)) + 0.5f, y0},
                                    {px(static_cast<float>(c)) + 0.5f, y1}, col_frontier, 1.0f);
                        ++prims;
                    }
                }
            }
            c = e;
        }

        // The HORIZONTAL frontier: between this row and the one above, merged
        // into segments along the row. Until BL-915 only the vertical line was
        // drawn, so a north-south border between two holders did not exist on
        // screen — and the border is this round's whole subject.
        if (r > 0)
        {
            int s = 0;
            while (s < gw)
            {
                const bool edge = row[static_cast<std::size_t>(s)] >= -2
                               && above[static_cast<std::size_t>(s)] >= -2
                               && row[static_cast<std::size_t>(s)] != above[static_cast<std::size_t>(s)];
                if (!edge) { ++s; continue; }
                int e = s + 1;
                while (e < gw && row[static_cast<std::size_t>(e)] >= -2
                              && above[static_cast<std::size_t>(e)] >= -2
                              && row[static_cast<std::size_t>(e)] != above[static_cast<std::size_t>(e)]
                              && row[static_cast<std::size_t>(e)] == row[static_cast<std::size_t>(s)]
                              && above[static_cast<std::size_t>(e)] == above[static_cast<std::size_t>(s)])
                    ++e;
                {
                    // BL-1090: the same hard rule on the north-south edge --
                    // 2 px of dark on pixel rows r-1 and r, the inner stroke
                    // one pixel into whichever side is hard.
                    const int32_t south = row[static_cast<std::size_t>(s)];
                    const int32_t north = above[static_cast<std::size_t>(s)];
                    const bool hs = hard_of(south), hn = hard_of(north);
                    const float x0 = px(static_cast<float>(s));
                    const float x1 = px(static_cast<float>(e)) + 1.0f;
                    if (hs || hn)
                    {
                        dl->AddLine({x0, y0}, {x1, y0}, col_frontier_hard, 2.0f);
                        ++prims;
                        if (hs)
                        {
                            dl->AddLine({x0, y0 + 1.5f}, {x1, y0 + 1.5f},
                                        owner_colour(h, static_cast<uint16_t>(south)), 1.0f);
                            ++prims;
                        }
                        if (hn)
                        {
                            dl->AddLine({x0, y0 - 1.5f}, {x1, y0 - 1.5f},
                                        owner_colour(h, static_cast<uint16_t>(north)), 1.0f);
                            ++prims;
                        }
                    }
                    else
                    {
                        dl->AddLine({x0, y0 + 0.5f}, {x1, y0 + 0.5f}, col_frontier, 1.0f);
                        ++prims;
                    }
                }
                s = e;
            }
        }
        std::swap(row, above);
    }

    // ── 2b. THE INDUSTRY HEAT (BL-1080; map layer ruled by Ben, 2026-09-24).
    //    Round 6's story is industry, and the span on an antiquity-epoch world
    //    draws no furnace crossing, so the ember marks below stay empty while
    //    the board's Ind column climbs. What the span DOES compute is industry
    //    points, sampled per polity per step, so the heat is BY POLITY
    //    TERRITORY: each realm's points per region held, over the record's
    //    peak density (`lapse_industry_heat`).
    //
    //    DRAWN AS A STIPPLE, NOT A WASH. An ember wash over a 170-alpha polity
    //    tint either buries the political colour or reads as one more polity
    //    hue; sparks scattered over the ground leave the colour and the
    //    frontier readable underneath and say "works here" in a different
    //    idiom. One candidate spark per 3x3 tile cell (staggered), lit when a
    //    fixed per-tile hash falls under the realm's heat — so the fraction of
    //    a realm's ground that sparks IS its heat, and a spark lit at one heat
    //    stays lit at every higher one: the heat grows across the span rather
    //    than flickering. Empty on every record with no points. ──
    if (h.industry_recorded && h.industry_density_peak > 0.0)
    {
        std::vector<float> heat_of; // owner -> heat this frame; -1 = not yet read
        const float fade_in = 1.0f - carry_fade;
        const float half    = std::clamp(scale * 0.40f, 1.0f, 2.4f);
        constexpr int cell  = 3;
        for (int r = 0; r < gh; r += cell)
        {
            const int stagger = ((r / cell) & 1) ? cell / 2 + 1 : 0;
            for (int c = stagger; c < gw; c += cell)
            {
                const std::size_t i = static_cast<std::size_t>(r * gw + c);
                const int32_t reg = h.tile_region[i];
                if (reg < 0 || static_cast<std::size_t>(reg) >= slice.size()) continue;
                const uint16_t o = slice[static_cast<std::size_t>(reg)];
                if (o == owner_none) continue;
                if (heat_of.size() <= o) heat_of.resize(static_cast<std::size_t>(o) + 1, -1.0f);
                if (heat_of[o] < 0.0f) heat_of[o] = lapse_industry_heat(h, o, year) * fade_in;
                const float heat = heat_of[o];
                if (heat <= 0.0f) continue;
                // A fixed hash of the tile, in [0, 1): presentation only, a
                // function of the raster position and nothing else.
                uint32_t k = static_cast<uint32_t>(i) * 2654435761u;
                k ^= k >> 15; k *= 2246822519u; k ^= k >> 13;
                const float t = static_cast<float>(k & 0xFFFFu) / 65536.0f;
                if (t >= heat) continue;
                const ImVec2 at{px(static_cast<float>(c)) + scale * 0.5f,
                                py(static_cast<float>(r)) + scale * 0.5f};
                // A dark ring under a bright core, as the ember marks and the
                // seats wear: a spark must read on a yellow realm as well as a
                // blue one, and a bare pale square vanished on the warm hues.
                dl->AddRectFilled({at.x - half - 1.0f, at.y - half - 1.0f},
                                  {at.x + half + 1.0f, at.y + half + 1.0f}, col_seat_ring);
                dl->AddRectFilled({at.x - half, at.y - half}, {at.x + half, at.y + half},
                                  col_heat);
                prims += 2;
            }
        }
    }

    // ── 3. RIVERS, over the fill: a river is what a frontier stops at, so it
    //    reads best on top of the tint rather than dimmed under it. Tile centre
    //    to downstream centre, baked once. ──
    {
        const float half = scale * 0.5f;
        const float w    = std::max(1.0f, scale * 0.28f);
        for (const lapse_river_seg& s : h.river_segs)
        {
            dl->AddLine({px(static_cast<float>(s.c0)) + half, py(static_cast<float>(s.r0)) + half},
                        {px(static_cast<float>(s.c1)) + half, py(static_cast<float>(s.r1)) + half},
                        col_river, w);
        }
        prims += static_cast<int>(h.river_segs.size());
    }

    // ── 3b. THE PROMOTED ROAD NETWORK (BL-917), over the rivers: a corridor
    //    not yet promoted at the playhead's year is not drawn at all — the
    //    walked-once settle tree stays invisible by construction, since it
    //    never got a `lapse_road_seg` in the first place (finish_history_lapse).
    //    A promoted one draws faint at Track and thickens at Road; a bridge
    //    glyph marks every point its line crosses a river, but only once the
    //    corridor carrying it is actually drawn. ──
    // A SEAM-CROSSING CORRIDOR IS DRAWN TWICE, ONCE OFF EACH EDGE. The bake
    // stored the short way round (`lapse_unwrap_col`), so one end may sit at a
    // negative column or past `gw`: drawing it once leaves the far half
    // missing, and drawing it unclipped would paint over the panel beside the
    // map. So each corridor is stroked at its own columns and again a world
    // width away, inside the map's own clip rect — the half that belongs on
    // each edge survives, and nothing escapes the map.
    const float world_w = static_cast<float>(gw) * scale;
    // One screen-second of playback, in years: every mark below fades over it.
    const int marker_window = lapse_marker_window_years(h);
    dl->PushClipRect({tl.x, tl.y}, {tl.x + static_cast<float>(gw) * scale,
                                    tl.y + static_cast<float>(gh) * scale}, true);
    for (const lapse_road_seg& s : h.road_segs)
    {
        if (year < s.year_track) continue; // not promoted yet at this playhead
        const bool at_road = year >= s.year_road;
        float w = at_road ? std::max(1.5f, scale * 0.30f)
                          : std::max(1.0f, scale * 0.16f);
        const ImU32 col = at_road ? col_road : col_road_track;
        const bool  wrapped = s.c1 < 0.0f || s.c1 > static_cast<float>(gw);
        const float shift   = s.c1 < 0.0f ? world_w : -world_w;

        // BL-1094: THE POST ROAD PULSE (Ben, 2026-09-24, R10). A layer's
        // transition is a mark ON THE THING, not a ping over it (STARTUP.md
        // § Identity across the rounds): for one marker window after a
        // corridor reaches Post Road it draws heavier and with a bright core,
        // decaying to its steady stroke -- one pulse along its length, and
        // then the road it now is. Track and Road promotions draw nothing
        // extra; the tier they reach is the stroke itself.
        float pulse = 0.0f;
        if (s.year_post_road != 0x7FFFFFFF && year >= s.year_post_road
         && year - s.year_post_road < marker_window)
            pulse = 1.0f - static_cast<float>(year - s.year_post_road)
                         / static_cast<float>(marker_window);
        w += w * 1.5f * pulse;

        dl->AddLine({px(s.c0), py(s.r0)}, {px(s.c1), py(s.r1)}, col, w);
        ++prims;
        if (wrapped)
        {
            dl->AddLine({px(s.c0) + shift, py(s.r0)}, {px(s.c1) + shift, py(s.r1)}, col, w);
            ++prims;
        }
        if (pulse > 0.0f)
        {
            const ImU32 core = with_alpha(col_bright, static_cast<int>(230.0f * pulse));
            const float cw   = std::max(1.0f, w * 0.45f);
            dl->AddLine({px(s.c0), py(s.r0)}, {px(s.c1), py(s.r1)}, core, cw);
            ++prims;
            if (wrapped)
            {
                dl->AddLine({px(s.c0) + shift, py(s.r0)}, {px(s.c1) + shift, py(s.r1)}, core, cw);
                ++prims;
            }
        }
        for (const lapse_bridge& br : s.bridges)
        {
            const ImVec2 at{px(br.col), py(br.row)};
            dl->AddCircleFilled(at, std::clamp(scale * 0.35f, 1.5f, 3.5f), col_bridge, 8);
            ++prims;
        }
    }

    // ── 3c. AMICABLE CROSS-BORDER TRADE (BL-925), beside the road network
    //    rather than folded into its loop above: a trade link is OPEN or
    //    CLOSED at the playhead year, read off whichever span (if any)
    //    brackets it, where a road corridor only ever ratchets forward. A
    //    grudge closing a border stops drawing the stroke; it does not erase
    //    the corridor, which can reopen once the grudge decays. ──
    for (const lapse_trade_seg& s : h.trade_segs)
    {
        bool open_here = false;
        for (const lapse_trade_span& sp : s.spans)
        {
            if (year >= sp.year_open && year < sp.year_close) { open_here = true; break; }
        }
        if (!open_here) continue;
        const float w = std::max(1.25f, scale * 0.22f);
        dl->AddLine({px(s.c0), py(s.r0)}, {px(s.c1), py(s.r1)}, col_trade_link, w);
        ++prims;
        if (s.c1 < 0.0f || s.c1 > static_cast<float>(gw)) // the seam, drawn off the other edge
        {
            const float shift = s.c1 < 0.0f ? world_w : -world_w;
            dl->AddLine({px(s.c0) + shift, py(s.r0)}, {px(s.c1) + shift, py(s.r1)},
                        col_trade_link, w);
            ++prims;
        }
    }

    // ── 3c'. SEA LANES (BL-1097), its own water layer beside the two above:
    //    a lane is drawn from the frame its leg earned the tier to the
    //    round's end, and never before — the crossings that fell short stay
    //    invisible, as the walked-once settle tree does on land. Drawn as a
    //    DASHED stroke, so a lane reads as traffic over water rather than a
    //    road painted onto it or a trade link's solid green; the dash length
    //    scales with the map so the rhythm holds at every zoom. Seam-crossing
    //    lanes are stroked twice like the corridors above. ──
    for (const lapse_lane_seg& s : h.lane_segs)
    {
        if (year < s.year_open) continue; // not yet a lane at this playhead
        const float w = std::max(1.25f, scale * 0.20f);
        const float dash = std::max(4.0f, scale * 1.5f);
        const auto dashed = [&](float x0, float y0, float x1, float y1) {
            const float dx = x1 - x0, dy = y1 - y0;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.0f) return;
            const float ux = dx / len, uy = dy / len;
            for (float t = 0.0f; t < len; t += dash * 2.0f)
            {
                const float e = std::min(t + dash, len);
                dl->AddLine({x0 + ux * t, y0 + uy * t}, {x0 + ux * e, y0 + uy * e},
                            col_sea_lane, w);
                ++prims;
            }
        };
        dashed(px(s.c0), py(s.r0), px(s.c1), py(s.r1));
        if (s.c1 < 0.0f || s.c1 > static_cast<float>(gw)) // the seam, drawn off the other edge
        {
            const float shift = s.c1 < 0.0f ? world_w : -world_w;
            dashed(px(s.c0) + shift, py(s.r0), px(s.c1) + shift, py(s.r1));
        }
    }

    // ── 3c''. THE KIN ARROWS (BL-1092; Ben, 2026-09-24, rulings R11), the
    //    Culture round's own layer, drawn from each founding's year and FADING
    //    over a few marker windows after it -- a route is a moment on this
    //    map, not a standing corridor, and a migration of hundreds of foundings
    //    drawn as permanent lines would be plaid over the very colours the
    //    round exists to show. In the founding people's lineage hue, so a
    //    family's roads read as that family's; DASHED where the line crosses
    //    water (the crude hop, at region grain), solid over land; a small
    //    arrowhead at the new region says which way the people went. Seam
    //    crossers are stroked twice like every corridor above. Empty on the
    //    polity rounds by construction (finish_history_lapse). ──
    if (!h.kin_segs.empty())
    {
        const int   kin_window = std::max(1, marker_window * 3);
        const float w    = std::max(1.25f, scale * 0.22f);
        const float dash = std::max(3.0f, scale * 1.2f);
        for (const lapse_kin_seg& s : h.kin_segs)
        {
            if (year < s.year || year - s.year >= kin_window) continue;
            const float fade = 1.0f - static_cast<float>(year - s.year)
                                    / static_cast<float>(kin_window);
            const int   alpha = static_cast<int>(40.0f + 200.0f * fade);
            const ImU32 col   = with_alpha(owner_colour(h, s.culture, year), alpha);
            const ImU32 head  = with_alpha(col_bright, alpha);
            const auto stroke = [&](float x0, float y0, float x1, float y1) {
                const float dx = x1 - x0, dy = y1 - y0;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len <= 0.0f) return;
                const float ux = dx / len, uy = dy / len;
                if (s.over_water)
                {
                    for (float t = 0.0f; t < len; t += dash * 2.0f)
                    {
                        const float e = std::min(t + dash, len);
                        dl->AddLine({x0 + ux * t, y0 + uy * t}, {x0 + ux * e, y0 + uy * e}, col, w);
                        ++prims;
                    }
                }
                else
                {
                    dl->AddLine({x0, y0}, {x1, y1}, col, w);
                    ++prims;
                }
                // The head: a small filled triangle pointing along the line
                // at the new region, sized to the tile so it survives a
                // narrow pane without swallowing the seat dot beneath it.
                const float hl = std::clamp(scale * 0.9f, 3.0f, 6.0f);
                const ImVec2 tip{x1, y1};
                const ImVec2 base{x1 - ux * hl, y1 - uy * hl};
                dl->AddTriangleFilled(tip, {base.x - uy * hl * 0.5f, base.y + ux * hl * 0.5f},
                                      {base.x + uy * hl * 0.5f, base.y - ux * hl * 0.5f}, head);
                ++prims;
            };
            stroke(px(s.c0), py(s.r0), px(s.c1), py(s.r1));
            if (s.c1 < 0.0f || s.c1 > static_cast<float>(gw)) // the seam, drawn off the other edge
            {
                const float shift = s.c1 < 0.0f ? world_w : -world_w;
                stroke(px(s.c0) + shift, py(s.r0), px(s.c1) + shift, py(s.r1));
            }
        }
    }
    dl->PopClipRect();

    // ── 3d. FLEET AND CARAVAN EXEMPLARS (BL-943, EXPLORATION.md sec Goods move
    //    as throughput, never as cargo: "the visual is a filter on that
    //    number, not a second simulation"). A `road_promoted`
    //    event already IS a corridor's throughput crossing a threshold — the
    //    road ladder promotes exactly on usage clearing one — and a
    //    `trade_link_opened` event is the same fact for a cross-border link, so
    //    reading those two event kinds inside the marker window is the whole
    //    filter; nothing here re-derives a number. One exemplar per crossing,
    //    fading exactly like the event ring below, never a mark per cargo unit
    //    and never a continuous animation. ──
    {
        const int window = lapse_marker_window_years(h);
        for (const lapse_event& e : h.lapse.events)
        {
            if (e.year > year) break; // ascending by year
            if (year - e.year >= window) continue;
            const bool is_road  = e.kind == static_cast<uint8_t>(lapse_event_kind::road_promoted);
            const bool is_trade = e.kind == static_cast<uint8_t>(lapse_event_kind::trade_link_opened);
            if (!is_road && !is_trade) continue;
            if (e.region == lapse_event_none || e.other == lapse_event_none) continue;

            float mx, my;
            bool  over_water;
            int   tier = 1; // a trade corridor has no ladder; drawn at the base rung
            if (is_road)
            {
                auto it = std::find_if(h.road_segs.begin(), h.road_segs.end(),
                    [&](const lapse_road_seg& s) { return s.region_a == e.region && s.region_b == e.other; });
                if (it == h.road_segs.end()) continue;
                mx = (it->c0 + it->c1) * 0.5f;
                my = (it->r0 + it->r1) * 0.5f;
                over_water = it->over_water;
                tier = static_cast<int>(e.polity); // note_event's own tier: 1/2/3
            }
            else
            {
                auto it = std::find_if(h.trade_segs.begin(), h.trade_segs.end(),
                    [&](const lapse_trade_seg& s) { return s.region_a == e.region && s.region_b == e.other; });
                if (it == h.trade_segs.end()) continue;
                mx = (it->c0 + it->c1) * 0.5f;
                my = (it->r0 + it->r1) * 0.5f;
                over_water = it->over_water;
            }

            // The midpoint of a seam-crossing corridor sits off the map by
            // construction (the bake stored the short way round), so fold it
            // back into the raster before drawing: the exemplar belongs where
            // the corridor actually runs, not past the edge.
            if (mx < 0.0f)                            mx += static_cast<float>(gw);
            else if (mx > static_cast<float>(gw))     mx -= static_cast<float>(gw);

            const float t = static_cast<float>(year - e.year) / static_cast<float>(window);
            const int   a = static_cast<int>(255.0f * (1.0f - t));
            draw_lapse_exemplar(dl, {px(mx), py(my)}, scale, over_water, tier, is_trade, a);
            ++prims;
        }
    }

    // ── 3e. THE FURNACES (BL-1080): every region that has crossed the furnace
    //    by the playhead year carries an ember mark at its anchor, from its
    //    crossing year on — so the Industrialisation round visibly
    //    industrialises, region by region, instead of animating borders over
    //    ground whose story is industry. A STATE mark, not an event ping (the
    //    2026-09-16 ruling below retired the pings): it appears at the
    //    crossing and stays. Drawn under the seats, so a seat reads over it.
    //    Empty on the three earlier rounds, whose records carry no crossing. ──
    if (!h.region_lit_year.empty())
    {
        const float r_out = std::clamp(scale * 0.55f, 1.6f, 3.4f);
        for (std::size_t ri = 0; ri < h.region_lit_year.size() && ri < h.region_col.size(); ++ri)
        {
            if (h.region_lit_year[ri] > year) continue;
            if (ri < slice.size() && slice[ri] == owner_none) continue; // nobody's ground: no works
            const ImVec2 at{px(static_cast<float>(h.region_col[ri]) + 0.5f),
                            py(static_cast<float>(h.region_row[ri]) + 0.5f)};
            dl->AddRectFilled({at.x - r_out - 1.0f, at.y - r_out - 1.0f},
                              {at.x + r_out + 1.0f, at.y + r_out + 1.0f}, col_seat_ring);
            dl->AddRectFilled({at.x - r_out, at.y - r_out}, {at.x + r_out, at.y + r_out},
                              col_furnace);
            prims += 2;
        }
    }

    // ── 3f. THE MARKS THAT EARN THEIR PLACE (BL-1094; Ben, 2026-09-24, R10;
    //    STARTUP.md § Identity across the rounds). There is NO blanket ring
    //    (Ben, 2026-09-16, watching round 4 run -- see the note at the end of
    //    this function): a mark is earned BY KIND, each with its own glyph, so
    //    the eye is not pulled off the borders by fifteen kinds wearing one
    //    white ring. Four kinds earn one:
    //      seat_captured        a ring in the WINNER's colour at the fallen
    //                           seat, fading over the marker window;
    //      broke_away / schism  a CRACK from the parent's seat to the
    //                           successor's, fading the same way;
    //      civilisation_formed  a two-tone DIAMOND at the coining region that
    //                           STAYS -- a state, like the furnace marks;
    //      capital_moved        the seat dot SLIDES old -> new over the window
    //                           (a slide table filled here, read by the seats
    //                           pass below).
    //    And a layer's transition is a mark on the thing, not a ping over it:
    //    the Post Road pulse in 3b. `realm_ended` and `creed_preached` draw
    //    NOTHING, and neither does any other kind -- the ticker names them.
    //
    //    SEATS ON THIS RECORD sit at `polity_seat` (the first region held), so
    //    a crack's parent end and a slide's endpoints are read there and off
    //    the event's own regions; when the UI-side capital fold lands
    //    (BL-1088, the name lane) the seat dot follows the capital and the
    //    parent end reads the fold at the event's year instead -- one lookup
    //    to change, `seat_region_of` below. ──
    struct seat_slide { uint16_t polity; float c0, r0, c1, r1, t; };
    std::vector<seat_slide> slides;
    // THE WORKS (BL-1099): the one glyph here that is a building -- a low
    // BRIGHT block with a furnace-toned stack rising from its right shoulder,
    // over a dark underline. The stack ties it to the ember squares as the
    // same family of fact (industry); the bright block is what tells it from
    // them and from the heat sparks, which are plain furnace-hued squares (the
    // first capture drew it all in the furnace tone and it vanished into the
    // stipple: a flash must read apart from the ground it lands on). A
    // specialist's stands a little taller. Shared by the in-span note in 3f
    // and the close's real charters in 3g, so the two read as one kind of
    // thing.
    const auto draw_works = [&](ImVec2 at, int alpha, bool specialist) {
        const float s   = std::clamp(scale * (specialist ? 0.9f : 0.75f), 2.5f, 5.5f);
        const float bw  = s * 1.4f, bh = s * 0.8f;          // the block
        const float sw  = s * 0.45f, sh = s * (specialist ? 2.4f : 2.0f); // the stack
        const ImU32 under = with_alpha(col_seat_ring, alpha);
        const ImU32 block = with_alpha(col_bright, alpha);
        const ImU32 stack = with_alpha(col_furnace, alpha);
        // the underline first, one pixel proud all round
        dl->AddRectFilled({at.x - bw - 1.0f, at.y - bh - 1.0f}, {at.x + bw + 1.0f, at.y + bh + 1.0f}, under);
        dl->AddRectFilled({at.x + bw - sw - 1.0f, at.y - sh - 1.0f}, {at.x + bw + 1.0f, at.y + 1.0f}, under);
        dl->AddRectFilled({at.x - bw, at.y - bh}, {at.x + bw, at.y + bh}, block);
        dl->AddRectFilled({at.x + bw - sw, at.y - sh}, {at.x + bw, at.y}, stack);
        prims += 4;
    };
    // THE REST SEAT (BL-1094, the review's fix round): where a polity's dot
    // SITS between moves -- the region of its last `capital_moved` at or
    // before the playhead, else `polity_seat`. Read off the record itself,
    // so a moved capital stays moved after the slide's window and a second
    // move starts where the first one ended; BL-1088's capital fold reads the
    // same events and supersedes this table when it lands.
    std::vector<int32_t> rest_seat;
    {
        const auto region_ok = [&](uint16_t r) {
            return r != lapse_event_none && static_cast<std::size_t>(r) < h.region_col.size();
        };
        const auto anchor = [&](uint16_t r) {
            return ImVec2{static_cast<float>(h.region_col[r]) + 0.5f,
                          static_cast<float>(h.region_row[r]) + 0.5f};
        };
        rest_seat.assign(h.polity_seat.size(), -1);
        // BL-1088: THE CAPITAL FOLD. A `founded` states the seat a realm rose
        // at and an `inherited` (a resumed span's restatement) where it sat at
        // the resume -- which on a resumed record is its CAPITAL, not the
        // lowest-indexed region the change list happens to show first -- and a
        // `capital_moved` moves it. The last at or before the playhead wins.
        // This is the one fold the seat dot, the 1200 burst and every mark on
        // a seat read; the NAME never follows it (CIVILISATION.md sec A
        // realm's name).
        for (const lapse_event& e : h.lapse.events)
        {
            if (e.year > year) break; // ascending by year
            const lapse_event_kind k = static_cast<lapse_event_kind>(e.kind);
            if (k != lapse_event_kind::capital_moved && k != lapse_event_kind::founded
             && k != lapse_event_kind::inherited) continue;
            if (e.polity == lapse_event_none || static_cast<std::size_t>(e.polity) >= rest_seat.size()) continue;
            if (!region_ok(e.region)) continue;
            rest_seat[e.polity] = e.region; // the last statement at or before the playhead wins
        }
        // A polity's seat region on this record at the playhead (see the note above).
        const auto seat_region_of = [&](uint16_t polity) -> int32_t {
            if (polity == lapse_event_none || static_cast<std::size_t>(polity) >= h.polity_seat.size())
                return -1;
            const int32_t moved = rest_seat[polity];
            return moved >= 0 ? moved : h.polity_seat[polity];
        };
        const float d_half   = std::clamp(scale * 1.1f, 4.0f, 7.5f);  // the diamond
        const float ring_r   = std::clamp(scale * 1.7f, 5.0f, 11.0f); // the fallen seat
        const float crack_amp = std::clamp(scale * 0.6f, 2.0f, 5.0f); // the crack's jag

        // THE DIAMOND: two tones -- two peoples settling one way of life: the
        // coining realm's colour on the west half, a pale tone on the east,
        // under one dark outline. One drawing for the carried marks and for
        // this record's own.
        const auto draw_diamond = [&](uint16_t region, uint16_t polity) {
            if (!region_ok(region)) return;
            const ImVec2 a  = anchor(region);
            const ImVec2 at{px(a.x), py(a.y)};
            const ImVec2 top{at.x, at.y - d_half}, bot{at.x, at.y + d_half};
            const ImVec2 lft{at.x - d_half, at.y}, rgt{at.x + d_half, at.y};
            const ImU32 west = polity == lapse_event_none ? col_bright : owner_colour(h, polity);
            constexpr ImU32 east = IM_COL32(238, 226, 196, 255);
            dl->AddTriangleFilled(top, lft, bot, west);
            dl->AddTriangleFilled(top, bot, rgt, east);
            dl->AddQuad(top, rgt, bot, lft, col_seat_ring, 1.5f);
            prims += 3;
        };
        dl->PushClipRect({tl.x, tl.y}, {tl.x + static_cast<float>(gw) * scale,
                                        tl.y + static_cast<float>(gh) * scale}, true);
        // THE DIAMONDS THAT CAME BEFORE (BL-1094): coined on an earlier round
        // and carried by id, drawn from this record's first frame.
        for (const history_lapse::civ_mark& m : h.civ_carry) draw_diamond(m.region, m.polity);
        for (const lapse_event& e : h.lapse.events)
        {
            if (e.year > year) break; // ascending by year
            const auto kind = static_cast<lapse_event_kind>(e.kind);

            // THE DIAMOND STAYS: every civilisation coined at or before the
            // playhead is marked, from its year on (and across the hand-over,
            // through `civ_carry` above).
            if (kind == lapse_event_kind::civilisation_formed)
            {
                draw_diamond(e.region, e.polity);
                continue;
            }

            // Everything else marks for one marker window, then is gone.
            if (year - e.year >= marker_window) continue;
            const float t = static_cast<float>(year - e.year) / static_cast<float>(marker_window);
            const int   a = static_cast<int>(255.0f * (1.0f - t));

            switch (kind)
            {
            case lapse_event_kind::seat_captured:
            {
                // A ring in the winner's colour at the fallen seat, widening a
                // little as it fades -- the one glyph here that is a ring, so
                // "a seat fell" reads as itself.
                if (!region_ok(e.region) || e.polity == lapse_event_none) continue;
                const ImVec2 an = anchor(e.region);
                const ImVec2 at{px(an.x), py(an.y)};
                const float  r  = ring_r * (1.0f + 0.35f * t);
                dl->AddCircle(at, r + 1.0f, with_alpha(col_seat_ring, a), 16, 3.5f);
                dl->AddCircle(at, r, with_alpha(owner_colour(h, e.polity), a), 16, 2.0f);
                prims += 2;
                break;
            }
            case lapse_event_kind::broke_away:
            case lapse_event_kind::schism:
            {
                // A crack from the parent's seat to the successor's: a jagged
                // pale line over a dark underline, its jag a fixed hash of the
                // event so it never shimmers frame to frame. Drawn the short
                // way round the seam and again a world width away, inside the
                // map's clip, as the corridors are.
                const int32_t ps = seat_region_of(e.other);
                if (ps < 0 || !region_ok(e.region)) continue;
                const ImVec2 p0 = anchor(static_cast<uint16_t>(ps));
                ImVec2 p1 = anchor(e.region);
                p1.x = lapse_unwrap_col(p0.x, p1.x, gw);
                const bool  wrapped = p1.x < 0.0f || p1.x > static_cast<float>(gw);
                const float shift   = p1.x < 0.0f ? world_w : -world_w;
                constexpr int segs = 6;
                ImVec2 pts[segs + 1];
                const float dx = px(p1.x) - px(p0.x), dy = py(p1.y) - py(p0.y);
                const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
                const float nx = -dy / len, ny = dx / len; // the perpendicular
                uint32_t k = static_cast<uint32_t>(e.year) * 2654435761u
                           ^ (static_cast<uint32_t>(e.region) << 16) ^ e.other;
                for (int i = 0; i <= segs; ++i)
                {
                    const float u = static_cast<float>(i) / static_cast<float>(segs);
                    k ^= k >> 13; k *= 2246822519u; k ^= k >> 16;
                    const float off = (i == 0 || i == segs) ? 0.0f
                                    : (static_cast<float>(k & 0xFFFFu) / 32768.0f - 1.0f) * crack_amp;
                    pts[i] = {px(p0.x) + dx * u + nx * off, py(p0.y) + dy * u + ny * off};
                }
                const ImU32 under = with_alpha(col_seat_ring, a);
                const ImU32 over  = with_alpha(col_bright, a);
                for (int pass = 0; pass < (wrapped ? 2 : 1); ++pass)
                {
                    const float sx = pass == 0 ? 0.0f : shift;
                    for (int i = 0; i < segs; ++i)
                        dl->AddLine({pts[i].x + sx, pts[i].y}, {pts[i + 1].x + sx, pts[i + 1].y}, under, 3.0f);
                    for (int i = 0; i < segs; ++i)
                        dl->AddLine({pts[i].x + sx, pts[i].y}, {pts[i + 1].x + sx, pts[i + 1].y}, over, 1.25f);
                    prims += 2 * segs;
                }
                break;
            }
            case lapse_event_kind::capital_moved:
            {
                // The seat dot slides from the OLD capital (`other`) to the new
                // (`region`) over the window; the seats pass reads this table.
                if (!region_ok(e.region) || !region_ok(e.other) || e.polity == lapse_event_none) continue;
                const ImVec2 from = anchor(e.other);
                ImVec2 to = anchor(e.region);
                to.x = lapse_unwrap_col(from.x, to.x, gw);
                slides.push_back({e.polity, from.x, from.y, to.x, to.y, t});
                break;
            }
            case lapse_event_kind::works_chartered:
            {
                // BL-1099: a works at the region's anchor, from its note's year,
                // fading over the window (STARTUP.md § Round 6, "Company
                // creation flashes"). A ping in the sense the 2026-09-16 ruling
                // allows: its own glyph for its own kind, never a ring.
                if (!region_ok(e.region)) continue;
                const ImVec2 an = anchor(e.region);
                draw_works({px(an.x), py(an.y)}, a, /*specialist=*/false);
                break;
            }
            default:
                break; // realm_ended, creed_preached and every other kind: nothing
            }
        }
        dl->PopClipRect();
    }

    // ── 3g. THE REAL CHARTERS AT THE CLOSE (BL-1099; Ben, 2026-09-24, R15).
    //    On the record's LAST frame, and only once the finish has landed
    //    (`works_close` is filled then), the firms the search actually
    //    chartered flash in at their anchor tiles, richest centre first,
    //    over a short wall-clock stagger -- the one place this map animates
    //    against the clock rather than the year, because the year has
    //    stopped. Each carries the year the pairing dated it to; nothing is
    //    read off the world-gen roster. Frozen under --verify. ──
    if (!h.works_close.empty() && year < h.lapse.start_year + h.lapse.years)
        h.works_close_t0 = -1.0; // off the close: the next arrival flashes again (cold review)
    if (!h.works_close.empty() && year >= h.lapse.start_year + h.lapse.years)
    {
        constexpr double stagger_s = 0.04; // per charter, in report order
        constexpr double ramp_s    = 0.5;  // to full strength
        const double now = ImGui::GetTime();
        if (h.works_close_t0 < 0.0) h.works_close_t0 = now;
        const double since = now - h.works_close_t0;
        dl->PushClipRect({tl.x, tl.y}, {tl.x + static_cast<float>(gw) * scale,
                                        tl.y + static_cast<float>(gh) * scale}, true);
        for (std::size_t i = 0; i < h.works_close.size(); ++i)
        {
            const history_lapse::works_mark& m = h.works_close[i];
            int a = 255;
            if (!h.works_close_frozen)
            {
                const double t = (since - stagger_s * static_cast<double>(i)) / ramp_s;
                if (t <= 0.0) break; // in order: none after this one is due yet
                a = static_cast<int>(255.0 * std::min(1.0, t));
            }
            draw_works({px(m.col + 0.5f), py(m.row + 0.5f)}, a, m.specialist);
        }
        dl->PopClipRect();
    }

    // ── 4. SEATS: one dot per polity HOLDING GROUND in this slice, at the
    //    region it first held. Seats only, not every region — the in-game Ages
    //    view draws a dot per region, and at blob granularity that is a rash;
    //    a seat per power is what makes a fragmentation into successors legible
    //    when two of them happen to sit in neighbouring hues. ──
    {
        const float rad = std::clamp(scale * 0.7f, 2.0f, 4.5f);
        for (std::size_t o = 0; o < present.size() && o < h.polity_seat.size(); ++o)
        {
            if (!present[o]) continue;
            // The dot rests at the last moved capital (BL-1094, `rest_seat`),
            // else at the first region held.
            const int32_t seat = (o < rest_seat.size() && rest_seat[o] >= 0) ? rest_seat[o]
                                                                            : h.polity_seat[o];
            if (seat < 0 || static_cast<std::size_t>(seat) >= h.region_col.size()) continue;
            ImVec2 at{px(static_cast<float>(h.region_col[static_cast<std::size_t>(seat)]) + 0.5f),
                      py(static_cast<float>(h.region_row[static_cast<std::size_t>(seat)]) + 0.5f)};
            // BL-1094: A CAPITAL MOVED slides the dot from the old seat to the
            // new over the marker window (3f fills the table; the LAST move in
            // the window wins). Eased, and folded back into the raster where
            // the short way round crosses the seam.
            for (std::size_t k = slides.size(); k-- > 0;)
            {
                if (slides[k].polity != static_cast<uint16_t>(o)) continue;
                const float u  = slides[k].t;
                const float ez = u * u * (3.0f - 2.0f * u); // smoothstep
                float cx = slides[k].c0 + (slides[k].c1 - slides[k].c0) * ez;
                const float cy = slides[k].r0 + (slides[k].r1 - slides[k].r0) * ez;
                if (cx < 0.0f)                        cx += static_cast<float>(gw);
                else if (cx >= static_cast<float>(gw)) cx -= static_cast<float>(gw);
                at = {px(cx), py(cy)};
                break;
            }
            dl->AddCircleFilled(at, rad, owner_colour(h, static_cast<uint16_t>(o), year), 10);
            dl->AddCircle(at, rad, col_seat_ring, 10, 1.0f);
            prims += 2;
        }
    }

    // ── 5. THE CONSOLIDATION (BL-932/BL-943): a ONE-TIME beat at 1200 CE,
    //    distinct from every ongoing corridor exemplar above — EXPLORATION.md
    //    sec Capital arrives names the one-time flow of every seat's stores
    //    into the capital as the phase's visible opening act, and Ben called
    //    it out by name as something the lapse should show. Gated on the
    //    record actually reaching past the Empire era's own 1200 CE close: a
    //    world that opted OUT of the Exploration span (`exploration_sim_enabled`
    //    default true since BL-946, but still a caller's field to clear) reads
    //    no ground past that year, so this block draws nothing on it rather
    //    than firing on the Empire close it shares the date with. The
    //    wizard's own Exploration round (BL-946) is exactly the record this
    //    was written for: its `h.lapse` IS the Exploration span (1200 ->
    //    1660), so this fires on every arrival at that round. ──
    if (h.lapse.start_year + h.lapse.years > lapse_exploration_epoch_year)
    {
        const int window = lapse_marker_window_years(h);
        if (year >= lapse_exploration_epoch_year
         && year - lapse_exploration_epoch_year < window)
        {
            const float t = static_cast<float>(year - lapse_exploration_epoch_year)
                          / static_cast<float>(window);
            const int   a = static_cast<int>(255.0f * (1.0f - t));
            const float r = std::clamp(scale * 1.1f, 3.5f, 7.0f);
            const ImU32 gold = with_alpha(IM_COL32(230, 190, 90, 255), a);
            for (std::size_t o = 0; o < present.size() && o < h.polity_seat.size(); ++o)
            {
                if (!present[o]) continue;
                // BL-1088: the burst is at the CAPITAL the fold gives, as the dot is.
                const int32_t seat = (o < rest_seat.size() && rest_seat[o] >= 0) ? rest_seat[o]
                                                                                : h.polity_seat[o];
                if (seat < 0 || static_cast<std::size_t>(seat) >= h.region_col.size()) continue;
                const ImVec2 at{px(static_cast<float>(h.region_col[static_cast<std::size_t>(seat)]) + 0.5f),
                                py(static_cast<float>(h.region_row[static_cast<std::size_t>(seat)]) + 0.5f)};
                // A BURST, not a dot: eight short rays around the seat, so a
                // one-time inflow reads differently from an ongoing corridor
                // exemplar or the seat's own steady ring.
                for (int k = 0; k < 8; ++k)
                {
                    const float ang = static_cast<float>(k) * (3.14159265f / 4.0f);
                    const ImVec2 p0{at.x + std::cos(ang) * r * 0.6f, at.y + std::sin(ang) * r * 0.6f};
                    const ImVec2 p1{at.x + std::cos(ang) * r,        at.y + std::sin(ang) * r};
                    dl->AddLine(p0, p1, gold, 1.5f);
                }
                ++prims;
            }
        }
    }

    // The draw cost, reported once per record under the capture harness so the
    // bound is a measured number rather than an assumption. Never in play.
    if (!h.prim_report_done)
    {
        h.prim_report_done = true;
        std::fprintf(stderr, "history_lapse: %zu base runs, %zu relief rims, %zu river segments, "
                             "%zu road corridors, %zu trade corridors, %d primitives this frame\n",
                     h.base_runs.size(), h.relief_segs.size(), h.river_segs.size(),
                     h.road_segs.size(), h.trade_segs.size(), prims);
    }

    // The year, over the map's own corner. It is the one thing a watcher needs
    // without looking away from the frontier. THE BODY'S NAME LEADS IT
    // (BL-1091; STARTUP.md § Rounds: "every lapse header names the body"):
    // the round is a history of one world, and the stamp says which.
    {
        const std::string label = h.body_name.empty()
            ? lapse_year_label(year)
            : h.body_name + "  " + lapse_year_label(year);
        const ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        const ImVec2 at{tl.x + 8.0f, tl.y + 6.0f};
        dl->AddRectFilled({at.x - 5.0f, at.y - 3.0f},
                          {at.x + ts.x + 5.0f, at.y + ts.y + 3.0f},
                          IM_COL32(8, 9, 12, 190));
        dl->AddText(at, col_bright, label.c_str()); // fit-exempt: a year stamp sized by CalcTextSize
    }

    // NO EVENT PINGS ON THE MAP (Ben, 2026-09-16, watching round 4 run).
    // BL-916 drew a white ring at every event inside the marker window, and it
    // drew the SAME ring for all of them: a realm dying, a treaty taken and a
    // trade route opening were one mark. Fifteen event kinds carry a region and
    // the trade links alone fire several a year, so the map read as a snowstorm
    // and — Ben's own words — pulled the eye off the border changes, which are
    // the thing a time-lapse of who-held-what is for.
    //
    // THE BORDERS ARE THE STORY. What is gone is the RING, not the record: the
    // events still cross in `lapse.events`, the ticker still names them with
    // their year and place, the arc readout still counts them, and the road and
    // trade-link overlays above still draw on the corridors they belong to --
    // those are lines on a thing, not pulses over it.
    //
    // FOUR KINDS EARN A MARK, EACH ITS OWN GLYPH (BL-1094; Ben, 2026-09-24,
    // R10; STARTUP.md § Identity across the rounds) -- pass 3f above:
    // `seat_captured`, `broke_away` / `schism`, `civilisation_formed` and
    // `capital_moved`; a fifth, `works_chartered` (BL-1099), earns the works
    // glyph -- a building, not a ring -- and the close's real charters wear
    // the same glyph in 3g; and the Post Road pulse in 3b is a layer
    // transition marked on the thing. `realm_ended`, `creed_preached` and
    // every other kind draw nothing. Adding a kind here means asking whether
    // it earns a glyph of its own, never restoring the blanket.

    ImGui::Dummy(avail);
}

int lapse_marker_window_years(const history_lapse& h)
{
    // One screen-second of the transport (startup_screens.cpp: span / 30 s).
    return std::max(1, h.lapse.years / 30);
}

// ---------------------------------------------------------------------------
// BL-1091 / BL-1092 -- the Life -> people bridge and the migration's roads
// ---------------------------------------------------------------------------

float lapse_globe_fade(const history_lapse& h, int year)
{
    // The carry-fade rule (`lapse_carry_fade`) for the round with nothing
    // behind it: a tenth of the span, so the dissolve reads the same at every
    // pace and a scripted capture reaches the fade by parking the playhead.
    if (h.empty() || h.lapse.years <= 0) return 0.0f;
    const float over = static_cast<float>(h.lapse.years) * 0.10f;
    const float gone = static_cast<float>(year - h.lapse.start_year);
    if (gone <= 0.0f)  return 1.0f;
    if (gone >= over)  return 0.0f;
    return 1.0f - gone / over;
}

int lapse_kin_arrows_at(const history_lapse& h, int year)
{
    // The bake is in change-list order, which is ascending by year, so the
    // count is a prefix; the arrows are few enough that a walk is cheaper
    // than keeping a second index in step with the bake.
    int n = 0;
    for (const lapse_kin_seg& s : h.kin_segs)
    {
        if (s.year > year) break;
        ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// The board
// ---------------------------------------------------------------------------

namespace {

struct board_row
{
    uint16_t owner   = owner_none;
    int32_t  land    = 0;  ///< Tiles held.
    int      regions = 0;
    int64_t  people  = -1; ///< Population held at the sampled step; -1 = no sample.
};

/// The recorded step at or before @p year, or -1 before the first. `steps` is
/// ascending by year, so this is one binary search.
int step_at_or_before(const era_timelapse& t, int year)
{
    int lo = 0, hi = static_cast<int>(t.steps.size()) - 1, best = -1;
    while (lo <= hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (t.steps[static_cast<std::size_t>(mid)].year <= year) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    return best;
}

/// The sample one polity has at a recorded step, or nullptr when it has none.
/// Linear in the step's samples: a few dozen living polities, called per row.
const polity_sample* sample_at_step(const history_lapse& h, int step, uint16_t polity)
{
    if (step < 0 || static_cast<std::size_t>(step) >= h.lapse.steps.size()) return nullptr;
    const timelapse_step& st = h.lapse.steps[static_cast<std::size_t>(step)];
    for (int i = 0; i < st.sample_count; ++i)
    {
        const std::size_t k = static_cast<std::size_t>(st.first_sample + i);
        if (k < h.lapse.samples.size() && h.lapse.samples[k].polity == polity)
            return &h.lapse.samples[k];
    }
    return nullptr;
}

/// Every living polity's population summed at a recorded step — the "people"
/// share's denominator, taken over the STEP'S samples exactly as
/// `history_sweep`'s `pop_slice_at` takes it, so the board and the sweep divide
/// by the same number. 0 before the first step.
int64_t people_held_at_step(const history_lapse& h, int step)
{
    if (step < 0 || static_cast<std::size_t>(step) >= h.lapse.steps.size()) return 0;
    const timelapse_step& st = h.lapse.steps[static_cast<std::size_t>(step)];
    int64_t total = 0;
    for (int i = 0; i < st.sample_count; ++i)
    {
        const std::size_t k = static_cast<std::size_t>(st.first_sample + i);
        if (k < h.lapse.samples.size()) total += h.lapse.samples[k].population;
    }
    return total;
}

/// Total the land and the people each polity holds in one slice, ordered by it.
///
/// SHARE OF PEOPLE is the ordering metric (Ben, 2026-09-15, NR-876 / BL-1000).
/// Share of LAND was the honest default while the ownership record was all
/// there was (BL-830), but it measures founding as much as conquest: read by
/// population instead, the largest realms are two to five points MORE
/// concentrated and most region-count "risers" were founding empty ground.
/// The population is `polity_sample::population` at the recorded step at or
/// before @p year — the sum of held regions' headcounts (history_sim.cpp,
/// `record_step`) — and a polity with no sample at that step (the opening
/// years, before the first recorded step; every polity on the Culture round,
/// whose record carries no samples) ranks as holding no one, so the board
/// falls back to land order exactly where people are unmeasured.
std::vector<board_row> rank_slice(const history_lapse& h,
                                  const std::vector<uint16_t>& slice, int year)
{
    std::vector<board_row> rows;
    for (std::size_t r = 0; r < slice.size() && r < h.region_area.size(); ++r)
    {
        const uint16_t o = slice[r];
        if (o == owner_none) continue;
        auto it = std::find_if(rows.begin(), rows.end(),
                               [&](const board_row& b) { return b.owner == o; });
        if (it == rows.end()) { rows.push_back({o, 0, 0, -1}); it = rows.end() - 1; }
        it->land += h.region_area[r];
        ++it->regions;
    }
    const int step = step_at_or_before(h.lapse, year);
    for (board_row& b : rows)
        if (const polity_sample* s = sample_at_step(h, step, b.owner)) b.people = s->population;

    // Deterministic: people descending, then land descending, then polity index,
    // so two equal holders do not swap places frame to frame.
    std::sort(rows.begin(), rows.end(), [](const board_row& a, const board_row& b) {
        const int64_t pa = std::max<int64_t>(0, a.people), pb = std::max<int64_t>(0, b.people);
        if (pa != pb) return pa > pb;
        if (a.land != b.land) return a.land > b.land;
        return a.owner < b.owner;
    });
    return rows;
}

constexpr int k_board_rows = 16;

/// A headcount in the width a board column can afford.
void fmt_population(char* buf, std::size_t n, int64_t pop)
{
    if (pop >= 1000000)     std::snprintf(buf, n, "%.2fM", static_cast<double>(pop) / 1e6);
    else if (pop >= 10000)  std::snprintf(buf, n, "%.0fk", static_cast<double>(pop) / 1e3);
    else if (pop >= 1000)   std::snprintf(buf, n, "%.1fk", static_cast<double>(pop) / 1e3);
    else                    std::snprintf(buf, n, "%lld", static_cast<long long>(pop));
}

/// BL-1080: industry points, compact for a 44 px column (a realm's stock runs
/// from a handful to billions): at most five glyphs and a unit.
void fmt_points(char* buf, std::size_t n, int64_t v)
{
    if (v >= 1000000000)    std::snprintf(buf, n, "%.1fG", static_cast<double>(v) / 1e9);
    else if (v >= 1000000)  std::snprintf(buf, n, "%.1fM", static_cast<double>(v) / 1e6);
    else if (v >= 1000)     std::snprintf(buf, n, "%.0fk", static_cast<double>(v) / 1e3);
    else                    std::snprintf(buf, n, "%lld", static_cast<long long>(v));
}

/// A region's generated name, or an honest placeholder — never an Earth name.
const char* region_name_of(const history_lapse& h, uint16_t region)
{
    if (region == lapse_event_none || static_cast<std::size_t>(region) >= h.region_name.size()
     || h.region_name[region].empty())
        return "unnamed ground";
    return h.region_name[region].c_str();
}

/// A polity's name (BL-1088): the record's coined name — coined once at its
/// founding in its founding culture's tongue, carried by id — and only where
/// the tongue could not coin, its seat's name (the pre-BL-1088 rule). The same
/// rule the board uses, through `lapse_polity_name`.
const char* polity_name_of(const history_lapse& h, uint16_t polity)
{
    if (polity != lapse_event_none && static_cast<std::size_t>(polity) < h.lapse.polity_name.size()
     && !h.lapse.polity_name[polity].empty())
        return h.lapse.polity_name[polity].c_str();
    if (polity == lapse_event_none || static_cast<std::size_t>(polity) >= h.polity_seat.size()
     || h.polity_seat[polity] < 0)
        return "an unnamed realm";
    return region_name_of(h, static_cast<uint16_t>(h.polity_seat[polity]));
}

// --- BL-1090: the hard-border walk ------------------------------------------

/// The forward walk with hysteresis over one record's steps, from a carried
/// state. Shared by the finish (which stores the bitmap on the record) and by
/// the hand-over (which needs only the closing row of a record that may never
/// have been finished), so the two cannot compute the flag differently.
void hard_walk_record(const era_timelapse& t, const std::vector<uint8_t>& carry,
                      std::vector<uint8_t>& out, int32_t& stride)
{
    out.clear();
    stride = 0;
    if (t.steps.empty() || t.samples.empty()) return;

    // The bitmap's width: one past the highest polity id a sample or the
    // carry names. Ids are one table across the spans (the slot inheritance
    // relies on the same fact), so a carried id addresses the same realm.
    int32_t n_pol = static_cast<int32_t>(carry.size());
    for (const polity_sample& s : t.samples)
        n_pol = std::max<int32_t>(n_pol, static_cast<int32_t>(s.polity) + 1);
    stride = n_pol;
    out.assign(t.steps.size() * static_cast<std::size_t>(n_pol), 0);

    std::vector<uint8_t> on(static_cast<std::size_t>(n_pol), 0);
    for (std::size_t p = 0; p < carry.size(); ++p) on[p] = carry[p] ? 1 : 0;
    std::vector<uint8_t> living(static_cast<std::size_t>(n_pol), 0);
    for (std::size_t s = 0; s < t.steps.size(); ++s)
    {
        const timelapse_step& st = t.steps[s];
        int64_t total = 0;
        for (int i = 0; i < st.sample_count; ++i)
        {
            const std::size_t k = static_cast<std::size_t>(st.first_sample + i);
            if (k < t.samples.size()) total += t.samples[k].population;
        }
        std::fill(living.begin(), living.end(), uint8_t{0});
        for (int i = 0; i < st.sample_count; ++i)
        {
            const std::size_t k = static_cast<std::size_t>(st.first_sample + i);
            if (k >= t.samples.size()) continue;
            const polity_sample& smp = t.samples[k];
            // The board's own per-mille arithmetic (`share_q_of` in the sweep).
            const int q = total > 0 ? static_cast<int>((smp.population * 1000) / total) : 0;
            uint8_t& flag = on[smp.polity];
            if (!flag && q >= lapse_hard_on_q)       flag = 1;
            else if (flag && q < lapse_hard_off_q)   flag = 0;
            living[smp.polity] = 1;
        }
        for (int32_t p = 0; p < n_pol; ++p)
        {
            const std::size_t up = static_cast<std::size_t>(p);
            if (!living[up]) on[up] = 0; // absent -- dead or unborn -- is never hard
            out[s * static_cast<std::size_t>(n_pol) + up] = on[up];
        }
    }
}

/// BL-1106: a civilisation's coined name off the record's name table
/// (era_timelapse.hpp § The name table), or an honest placeholder.
const char* civilisation_name_of(const history_lapse& h, uint16_t idx)
{
    if (idx == lapse_event_none || static_cast<std::size_t>(idx) >= h.lapse.civilisation_name.size()
     || h.lapse.civilisation_name[idx].empty())
        return "an unnamed civilisation";
    return h.lapse.civilisation_name[idx].c_str();
}

/// BL-1106: a creed's coined name off the same table, or a placeholder.
const char* creed_name_of(const history_lapse& h, int32_t idx)
{
    if (idx < 0 || static_cast<std::size_t>(idx) >= h.lapse.creed_name.size()
     || h.lapse.creed_name[static_cast<std::size_t>(idx)].empty())
        return "an unnamed creed";
    return h.lapse.creed_name[static_cast<std::size_t>(idx)].c_str();
}

/// BL-1106: the creed a polity adopted, off the record's `polity_creed`
/// table; -1 where the table does not say.
int32_t creed_of_polity(const history_lapse& h, uint16_t polity)
{
    if (polity == lapse_event_none || static_cast<std::size_t>(polity) >= h.lapse.polity_creed.size())
        return -1;
    return h.lapse.polity_creed[polity];
}

} // namespace

void lapse_hard_walk(history_lapse& h)
{
    hard_walk_record(h.lapse, h.hard_carry, h.polity_hard, h.hard_stride);
}

bool lapse_polity_hard(const history_lapse& h, uint16_t polity, int year)
{
    const int step = step_at_or_before(h.lapse, year);
    if (step < 0 || h.hard_stride <= 0 || polity >= h.hard_stride)
        return polity < h.hard_carry.size() && h.hard_carry[polity] != 0;
    const std::size_t k = static_cast<std::size_t>(step) * static_cast<std::size_t>(h.hard_stride)
                        + static_cast<std::size_t>(polity);
    return k < h.polity_hard.size() && h.polity_hard[k] != 0;
}

std::vector<uint8_t> lapse_hard_at_close(const history_lapse& h)
{
    std::vector<uint8_t> bits;
    int32_t stride = 0;
    if (h.hard_stride > 0 && !h.polity_hard.empty())
    {
        bits   = h.polity_hard;
        stride = h.hard_stride;
    }
    else
    {
        hard_walk_record(h.lapse, h.hard_carry, bits, stride);
    }
    if (stride <= 0 || bits.size() < static_cast<std::size_t>(stride)) return {};
    return std::vector<uint8_t>(bits.end() - stride, bits.end());
}

std::vector<history_lapse::civ_mark> lapse_civ_marks_at_close(const history_lapse& h)
{
    std::vector<history_lapse::civ_mark> marks = h.civ_carry;
    const auto have = [&](uint16_t p, uint16_t r) {
        for (const auto& m : marks) if (m.polity == p && m.region == r) return true;
        return false;
    };
    for (const lapse_event& e : h.lapse.events)
    {
        if (static_cast<lapse_event_kind>(e.kind) != lapse_event_kind::civilisation_formed) continue;
        if (e.region == lapse_event_none || static_cast<std::size_t>(e.region) >= h.region_col.size()) continue;
        if (!have(e.polity, e.region)) marks.push_back({e.polity, e.region});
    }
    return marks;
}

float lapse_industry_heat(const history_lapse& h, uint16_t polity, int year)
{
    if (!h.industry_recorded || h.industry_density_peak <= 0.0) return 0.0f;
    const polity_sample* s = sample_at_step(h, step_at_or_before(h.lapse, year), polity);
    if (s == nullptr || s->industry_points <= 0) return 0.0f;
    const double d = static_cast<double>(s->industry_points)
                   / static_cast<double>(std::max<int>(1, s->regions));
    // SQUARE-ROOTED: the density is heavily skewed (on the verify seed one
    // realm stands near the peak and the mean realm near a tenth of it), so a
    // linear heat lit one realm and left the rest of the industrial world
    // dark. The root lifts the low end and keeps zero at zero and the order
    // of any two realms, so growth across the span still reads as growth.
    return static_cast<float>(std::sqrt(std::clamp(d / h.industry_density_peak, 0.0, 1.0)));
}

void draw_lapse_scoreboard(const history_lapse& h,
                           const std::vector<uint16_t>& slice,
                           const std::vector<uint16_t>& lagged,
                           int year, int lagged_year)
{
    const std::vector<board_row> now  = rank_slice(h, slice, year);
    const std::vector<board_row> then = rank_slice(h, lagged, lagged_year);

    // BL-916 on BL-817: the sample this instant's People, Pop and Might columns
    // read. The step AT OR BEFORE the playhead, so the board never shows a
    // number from a future the map has not reached. `people_total` is the
    // sweep's own denominator — every living polity's sample summed at the step.
    const int     step         = step_at_or_before(h.lapse, year);
    const int64_t people_total = people_held_at_step(h, step);

    int32_t held = 0;
    for (const board_row& b : now) held += b.land;
    if (held <= 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("No ground is held yet. The first claims appear as the "
                           "time-lapse reaches the founding centuries.");
        ImGui::PopStyleColor();
        return;
    }

    // BL-1106: THE BOARD'S NOUN FOLLOWS THE RECORD. On the Culture round the
    // owners are peoples and the column is each one's homeland (STARTUP.md
    // § Round 3 — Culture); on the three pass rounds they are realms.
    ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
    ImGui::Text("%d %s hold ground; the top %d are listed.",
                static_cast<int>(now.size()), h.peoples ? "peoples" : "realms", k_board_rows);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // PEOPLE, POPULATION and MIGHT are read off BL-817's per-polity sample series
    // at the step this instant falls in (BL-916). ORDERED BY PEOPLE (BL-1000):
    // the share of everyone held that this polity holds, which is the rank; the
    // share of LAND stays as the second column because a realm that is large and
    // empty is a different thing from one that is small and full, and the pair
    // says which. The region COUNT that used to sit here is gone — it measured
    // the placement pass more than the ground anybody held, and its width is
    // what pays for the second share. Might is the military capacity band, 1-6,
    // exactly as the sim holds it.
    //
    // A RESEARCH COLUMN DOES NOT GO HERE, and that is a standing refusal rather
    // than a deferral: research points accrue from population (BL-822), so the
    // column would restate population under a second name and show a correlation
    // it never measured — on the board Ben is judging the research levers with.
    //
    // AN INDUSTRY COLUMN, ON THE ROUND THAT HAS INDUSTRY (BL-1080). The
    // Industrialisation span credits industry points (INDUSTRIALISATION.md
    // § Beat 1) and the record samples them per polity per step, so the board
    // shows who is building works as the centuries pass. Only when the record
    // carries any: the three earlier rounds keep their six columns. It is a
    // column, not the rank — the board still orders by people.
    const bool ind_col = h.industry_recorded;
    if (!ImGui::BeginTable("##lapse_board", ind_col ? 7 : 6,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        return;

    // Short headers throughout: the header is drawn by ImGui's own
    // TableHeadersRow, which neither elides nor reports, so a header wider than
    // its column simply clips ("Popul...") with nothing recording that it did.
    // "People" is the rank column and reads as a share; "Pop" is the headcount.
    ImGui::TableSetupColumn("#",      ImGuiTableColumnFlags_WidthFixed, 22.0f);
    ImGui::TableSetupColumn(h.peoples ? "Homeland" : "Seat", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("People", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Land",   ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Pop",    ImGuiTableColumnFlags_WidthFixed, 46.0f);
    ImGui::TableSetupColumn("Mt",     ImGuiTableColumnFlags_WidthFixed, 24.0f);
    if (ind_col)
        ImGui::TableSetupColumn("Ind", ImGuiTableColumnFlags_WidthFixed, 44.0f);
    ImGui::TableHeadersRow();

    const int shown = std::min(k_board_rows, static_cast<int>(now.size()));
    for (int i = 0; i < shown; ++i)
    {
        const board_row& b = now[static_cast<std::size_t>(i)];
        ImGui::TableNextRow();
        const bool hard = lapse_polity_hard(h, b.owner, year); // BL-1090: the row is bold

        // Rank, and the movement against the lagged board. A polity CLIMBING INTO
        // the board and DROPPING OUT of it is the whole story of the round, so the
        // entry marker is not decoration — without it a re-ranked list reads as a
        // list that merely reordered.
        int was = -1;
        for (std::size_t j = 0; j < then.size(); ++j)
            if (then[j].owner == b.owner) { was = static_cast<int>(j); break; }
        const bool entered = (was < 0 || was >= k_board_rows);

        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, entered ? col_bright : col_dim);
        ImGui::Text("%d", i + 1);
        ImGui::PopStyleColor();

        ImGui::TableSetColumnIndex(1);
        // MEASURED BEFORE THE SWATCH, not after. `GetContentRegionAvail` inside a
        // table cell is taken from the CURSOR, and the cursor is past the column's
        // right edge once a Dummy + SameLine has run — which reported a NEGATIVE
        // width and failed every row on a column that was fitting fine.
        const float name_avail = ImGui::GetContentRegionAvail().x;
        float swatch_w = 0.0f;
        {
            // The identity swatch: the same colour this polity is painted in on
            // the map, so a row and its territory are matched by eye.
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float  s = ImGui::GetTextLineHeight();
            ImGui::GetWindowDrawList()->AddRectFilled(
                {p.x, p.y + 2.0f}, {p.x + s * 0.55f, p.y + s - 1.0f}, owner_colour(h, b.owner, year));
            swatch_w = s * 0.55f + 5.0f + ImGui::GetStyle().ItemSpacing.x;
            ImGui::Dummy({s * 0.55f + 5.0f, s});
            ImGui::SameLine();
        }
        {
            // BL-1088: the realm's coined name, never its seat's (the seat's
            // name is the fallback for a tongue that could not coin). On the
            // Culture round the owner is a culture and its row keeps the
            // region-name read it always had.
            const int32_t seat = (b.owner < h.polity_seat.size())
                                     ? h.polity_seat[b.owner] : -1;
            const char* nm = (!h.owners_are_cultures && b.owner < h.lapse.polity_name.size()
                              && !h.lapse.polity_name[b.owner].empty())
                                 ? h.lapse.polity_name[b.owner].c_str()
                             : (seat >= 0 && static_cast<std::size_t>(seat) < h.region_name.size()
                                && !h.region_name[static_cast<std::size_t>(seat)].empty())
                                 ? h.region_name[static_cast<std::size_t>(seat)].c_str()
                                 : "unnamed seat";
            // A generated seat name in a third-width column is exactly the shape
            // that overruns, so it is drawn through `ui::fit_text`: it elides with
            // the full name a hover away, and — the half that matters here — the
            // draw is RECORDED, so `verify.expect_no_clipping` over this surface
            // measures something instead of passing vacuously (BL-714).
            char cell[192];
            const int delta = entered ? 0 : (was - i);
            if (entered)        std::snprintf(cell, sizeof cell, "%s  *", nm);
            else if (delta > 0) std::snprintf(cell, sizeof cell, "%s  +%d", nm, delta);
            else if (delta < 0) std::snprintf(cell, sizeof cell, "%s  %d",  nm, delta);
            else                std::snprintf(cell, sizeof cell, "%s", nm);

            // BL-1090: A HARD-BORDERED REALM'S ROW IS BOLD -- the same flag the
            // map draws its heavy border from, so the row and the ground agree.
            // The UI carries ONE face (fonts.cpp), so bold is the fitted
            // string drawn a second time one pixel to the right, the faux-bold
            // every single-face renderer uses: the ledger measures the one
            // `fit_text` draw, and the overlay is the same fitted text at the
            // same width, so nothing is drawn that was not measured.
            const float  name_w  = std::max(24.0f, name_avail - swatch_w);
            const ImVec2 name_at = ImGui::GetCursorScreenPos();
            if (entered || hard) ImGui::PushStyleColor(ImGuiCol_Text, col_bright);
            ui::fit_text(text_box::table_cell, "wizard.round4.seat", cell, name_w);
            if (entered || hard) ImGui::PopStyleColor();
            if (hard)
                ImGui::GetWindowDrawList()->AddText({name_at.x + 1.0f, name_at.y}, col_bright,
                                                    ui::fitted(cell, name_w).c_str());
        }

        // The rank column: this polity's share of everyone held at the step.
        // Per-mille integer arithmetic, the sweep's `share_q_of`, so a figure
        // read off the board is the figure the sweep would print. A polity with
        // no sample at this step — the record starts on the first decision
        // round, so the opening years have none — draws a dim dash rather than
        // a zero it never measured.
        const polity_sample* smp = sample_at_step(h, step, b.owner);
        ImGui::TableSetColumnIndex(2);
        if (smp != nullptr && people_total > 0)
        {
            const int q = static_cast<int>((smp->population * 1000) / people_total);
            char share[16];
            std::snprintf(share, sizeof share, "%d.%d%%", q / 10, q % 10);
            // The rank figure is bold on a hard row too (BL-1090), the same way.
            const ImVec2 at = ImGui::GetCursorScreenPos();
            if (hard) ImGui::PushStyleColor(ImGuiCol_Text, col_bright);
            ImGui::TextUnformatted(share);
            if (hard)
            {
                ImGui::PopStyleColor();
                ImGui::GetWindowDrawList()->AddText({at.x + 1.0f, at.y}, col_bright, share);
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
            ImGui::TextUnformatted("-");
            ImGui::PopStyleColor();
        }

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.1f%%", 100.0f * static_cast<float>(b.land)
                                     / static_cast<float>(held));

        // The two remaining sampled columns, dashed on the same rule.
        ImGui::TableSetColumnIndex(4);
        if (smp != nullptr)
        {
            char pop[24];
            fmt_population(pop, sizeof pop, smp->population);
            ImGui::TextUnformatted(pop);
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
            ImGui::TextUnformatted("-");
            ImGui::PopStyleColor();
        }
        ImGui::TableSetColumnIndex(5);
        if (smp != nullptr) ImGui::Text("%d", static_cast<int>(smp->cap_military));
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
            ImGui::TextUnformatted("-");
            ImGui::PopStyleColor();
        }
        if (ind_col)
        {
            // Industry points standing on this polity's ground at the step,
            // compact; a dash where it holds none yet.
            ImGui::TableSetColumnIndex(6);
            if (smp != nullptr && smp->industry_points > 0)
            {
                char ind[24];
                fmt_points(ind, sizeof ind, smp->industry_points);
                ImGui::TextUnformatted(ind);
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
                ImGui::TextUnformatted("-");
                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::EndTable();

    if (static_cast<int>(now.size()) > shown)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::Text("and %d more below the board.",
                    static_cast<int>(now.size()) - shown);
        ImGui::PopStyleColor();
    }

    // BL-1092: THE PEOPLES CENSUS AND THE MIGRATION COUNTER, on the Culture
    // round alone (Ben, 2026-09-24, rulings R11). The daughters the boundary
    // fold merges back into an ancestor leave the ticker (lapse_ticker_rows);
    // this is where they are counted instead, beside how many peoples the
    // migration coined on the march and how many foundings have moved a
    // people from one region to another by this year (the kin arrows drawn
    // so far, `lapse_kin_arrows_at`). The counts are the settlement's own
    // census, read at the record's construction, never re-derived here.
    if (h.peoples && (h.peoples_cradles > 0 || h.peoples_coined > 0))
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("%d cradles; %d peoples coined on the march, %d of them folded "
                           "back into an ancestor; %d migrations so far.",
                           h.peoples_cradles, h.peoples_coined, h.peoples_folded,
                           lapse_kin_arrows_at(h, year));
        ImGui::PopStyleColor();
    }
}

int lapse_lagged_year(const history_lapse& h, int year)
{
    // A twelfth of the span back: far enough that a rank move means something,
    // near enough that the marks are not permanently lit. CLAMPED TO THE
    // RECORD'S FIRST YEAR (BL-1106): a resumed span's inherited realms are
    // written at its start year, and a lagged slice taken before it holds
    // nobody — which marked every realm on the board as a newcomer for the
    // first twelfth of rounds 5 and 6.
    const int first = h.lapse.start_year;
    const int lag   = std::max(1, h.lapse.years / 12);
    return std::max(first, year - lag);
}

int lapse_board_entered_count(const history_lapse& h, int year)
{
    // THE SAME PREDICATE draw_lapse_scoreboard MARKS BY, over the same two
    // ranked slices, so the verify read and the drawn '*' cannot disagree.
    const int lagged_year = lapse_lagged_year(h, year);
    const std::vector<board_row> now  = rank_slice(h, owner_slice_at(h.lapse, year), year);
    const std::vector<board_row> then = rank_slice(h, owner_slice_at(h.lapse, lagged_year), lagged_year);
    const int shown = std::min(k_board_rows, static_cast<int>(now.size()));
    int entered = 0;
    for (int i = 0; i < shown; ++i)
    {
        int was = -1;
        for (std::size_t j = 0; j < then.size(); ++j)
            if (then[j].owner == now[static_cast<std::size_t>(i)].owner) { was = static_cast<int>(j); break; }
        if (was < 0 || was >= k_board_rows) ++entered;
    }
    return entered;
}


// ---------------------------------------------------------------------------
// BL-891 -- the arc, read off the record
// ---------------------------------------------------------------------------
//
// A ROLLED WORLD CANNOT BE JUDGED BY A SNAPSHOT. BL-890 makes every arrival at
// the wizard a fresh world; without a readout a reroll is a blind redraw,
// because the scoreboard beside this shows who leads RIGHT NOW and says nothing
// about whether an empire ever formed or fell. Ben, 2026-09-10: rolling should
// give "the player the ability to see if their world will contain these types
// of structures".
//
// EVERY DEFINITION HERE IS history_sweep's, deliberately, so the panel and the
// harness cannot drift into disagreeing about the same world.

lapse_arc summarise_lapse_arc(const history_lapse& h)
{
    lapse_arc out;
    const int stride = h.lapse.region_stride > 0 ? h.lapse.region_stride : 1;

    // THE LIVE STRIDE PER STEP (BL-916). Regions are founded all through the
    // run, so the denominator of a share is how many EXISTED at that step, not
    // how many the run ended with. Each region's first appearance is its first
    // ownership change; `changes` is ascending by year, so one forward pointer
    // over it, advanced step by step, counts the regions live at each step.
    std::vector<int> live_at_step(h.lapse.steps.size(), stride);
    {
        std::vector<char> seen(static_cast<std::size_t>(stride), 0);
        int live = 0;
        std::size_t ci = 0;
        for (std::size_t si = 0; si < h.lapse.steps.size(); ++si)
        {
            const int32_t sy = h.lapse.steps[si].year;
            while (ci < h.lapse.changes.size() && h.lapse.changes[ci].year <= sy)
            {
                const uint16_t r = h.lapse.changes[ci].region;
                if (r < seen.size() && !seen[r]) { seen[r] = 1; ++live; }
                ++ci;
            }
            live_at_step[si] = std::max(1, live);
        }
    }

    // Per polity: first holding, peak holding, last holding, and the peak's
    // share against the stride live at ITS step. `polity` ids are sparse, so a
    // map keyed by id would iterate in an order the record does not define — a
    // vector indexed by id keeps this deterministic.
    std::vector<int> first_r, peak_r, last_r, peak_q;
    for (std::size_t si = 0; si < h.lapse.steps.size(); ++si)
    {
        const timelapse_step& st = h.lapse.steps[si];
        for (int k = 0; k < st.sample_count; ++k)
        {
            const std::size_t idx = static_cast<std::size_t>(st.first_sample + k);
            if (idx >= h.lapse.samples.size()) break;
            const polity_sample& s = h.lapse.samples[idx];
            const std::size_t id = s.polity;
            if (id >= first_r.size())
            {
                first_r.resize(id + 1, -1);
                peak_r.resize(id + 1, 0);
                last_r.resize(id + 1, 0);
                peak_q.resize(id + 1, 0);
            }
            const int r = static_cast<int>(s.regions);
            if (first_r[id] < 0) first_r[id] = r;
            if (r > peak_r[id])  peak_r[id]  = r;
            const int q = (r * 1000) / live_at_step[si];
            if (q > peak_q[id]) peak_q[id] = q;
            last_r[id] = r;
        }
    }

    // A REALM THAT ENDED is one the sim recorded ending — never one merely
    // missing from the closing step, which is what every dead realm is.
    for (const lapse_event& e : h.lapse.events)
        if (e.kind == static_cast<uint8_t>(lapse_event_kind::realm_ended)) ++out.eliminated;

    int biggest_end = 0, smallest_end = 0;
    for (std::size_t id = 0; id < first_r.size(); ++id)
    {
        if (first_r[id] < 0) continue;       // never seen
        if (peak_r[id] <= 0) continue;       // never held ground
        ++out.polities;
        if (peak_q[id] > out.peak_share_q) out.peak_share_q = peak_q[id];

        // A polity absent from the closing step ended before it; its end is
        // already counted above and it has no closing standing.
        const timelapse_step& last = h.lapse.steps.back();
        bool at_end = false;
        for (int k = 0; k < last.sample_count; ++k)
        {
            const std::size_t idx = static_cast<std::size_t>(last.first_sample + k);
            if (idx < h.lapse.samples.size() && h.lapse.samples[idx].polity == id)
            { at_end = true; break; }
        }
        if (!at_end || last_r[id] <= 0) continue;

        // THE SWEEP'S OWN SHAPE TEST: doubled AND gained at least three
        // regions, then ended at or under 60% of the peak.
        const bool rose = peak_r[id] >= first_r[id] * 2 && peak_r[id] >= first_r[id] + 3;
        const bool fell = last_r[id] * 100 <= peak_r[id] * 60;
        if (rose && fell) ++out.rose_and_fell;

        if (last_r[id] > biggest_end) biggest_end = last_r[id];
        if (smallest_end == 0 || last_r[id] < smallest_end) smallest_end = last_r[id];
    }
    out.biggest_end_q = (biggest_end * 1000) / stride;
    out.smallest_end  = smallest_end;

    // THE SAME PEAK BY PEOPLE (BL-1000), and it is history_sweep's
    // `peak_share_pop_q` ARITHMETIC EXACTLY, not a per-step maximum: the sweep
    // walks the span a century at a time from its first year, reads the step
    // at or before each mark, and takes the largest polity's population over
    // every living polity's at that step (`pop_slice_at` / `share_q_of`). A
    // per-step walk would find a higher peak between two marks and the panel
    // and the sweep would then disagree about the same world — which is the
    // one thing this readout must never do. The end share is the sweep's
    // `top_share_pop_q`: the closing year's own step.
    if (!h.lapse.steps.empty())
    {
        const int first = h.lapse.start_year;
        const int last  = h.lapse.start_year + h.lapse.years;
        const auto share_at = [&](int y) -> int {
            const int si = step_at_or_before(h.lapse, y);
            if (si < 0) return 0;
            const timelapse_step& st = h.lapse.steps[static_cast<std::size_t>(si)];
            int64_t total = 0, top = 0;
            for (int k = 0; k < st.sample_count; ++k)
            {
                const std::size_t idx = static_cast<std::size_t>(st.first_sample + k);
                if (idx >= h.lapse.samples.size()) break;
                const int64_t p = h.lapse.samples[idx].population;
                total += p;
                if (p > top) top = p;
            }
            return total > 0 ? static_cast<int>((top * 1000) / total) : 0;
        };
        for (int y = first; y <= last; y += 100)
            out.peak_share_pop_q = std::max(out.peak_share_pop_q, share_at(y));
        out.end_share_pop_q = share_at(last);
    }
    return out;
}

void draw_lapse_arc(const history_lapse& h)
{
    const lapse_arc a = summarise_lapse_arc(h);
    if (a.polities <= 0) return;

    const ImU32 col_dim = IM_COL32(150, 158, 175, 255);
    ImGui::SeparatorText("What happened here");

    // STATED AS PROSE, NOT A TABLE. The player is deciding whether to keep this
    // world, which is a judgement about SHAPE; a table of six numbers makes them
    // do the reading. The numbers are still all present.
    //
    // THE SHARES ARE OF PEOPLE, the board's rank column (BL-1000), so the
    // sentence and the board agree about who was largest. The region figures
    // stay as the fallback for a record that carries no samples, so the
    // readout never goes silent on a world it can still describe.
    const bool by_people = a.peak_share_pop_q > 0;
    const int  peak_q    = by_people ? a.peak_share_pop_q : a.peak_share_q;
    const int  end_q     = by_people ? a.end_share_pop_q  : a.biggest_end_q;
    // BL-1106: THE NOUN FOLLOWS THE RECORD, as the board's does. A polity is
    // a realm; only the Culture round's owners are peoples, and the old
    // sentence called every round's polities peoples.
    const char* noun = h.peoples ? "peoples" : "realms";
    if (a.eliminated == 0 && a.rose_and_fell == 0 && peak_q < 100)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("A quiet age. %d %s held ground and none of them "
                           "grew large or was destroyed.", a.polities, noun);
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::Text("%d %s, %d destroyed.", a.polities, noun, a.eliminated);
        // Wrapped, not Text: "people" made this line the column's longest, and
        // an unwrapped line clips at the column edge with nothing recording it.
        ImGui::TextWrapped("The largest empire held %d%% of the world's %s; %d rose and fell back.",
                           peak_q / 10, by_people ? "people" : "land", a.rose_and_fell);
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("At the end the greatest holds %d%%, the least %d region%s.",
                           end_q / 10, a.smallest_end,
                           a.smallest_end == 1 ? "" : "s");
        ImGui::PopStyleColor();
    }
}

// ---------------------------------------------------------------------------
// BL-916 -- the ticker, and the prose each event is phrased as
// ---------------------------------------------------------------------------
//
// THE MOMENTS ARE PART OF THE TIME-LAPSE (Ben, 2026-09-11). A colour flipping
// on the map is a fact with no cause; these lines are the cause, dated and
// placed, in the order they happened. Every noun is a generated region name off
// the record's own table — the seat a polity rose from names the polity, exactly
// as the board names it — so nothing here can be an Earth name.

std::string lapse_event_prose(const history_lapse& h, const lapse_event& e)
{
    char buf[256];
    const char* R = region_name_of(h, e.region);
    switch (static_cast<lapse_event_kind>(e.kind))
    {
    case lapse_event_kind::founded:
        // BL-1088: a realm rises under its own name; ground that organises with
        // no polity to own it is a people settling, not a realm rising
        // (CIVILISATION.md sec A realm's name).
        if (e.polity == lapse_event_none)
            std::snprintf(buf, sizeof buf, "A people settle at %s.", R);
        else
            std::snprintf(buf, sizeof buf, "%s rises at %s.", polity_name_of(h, e.polity), R);
        break;
    case lapse_event_kind::inherited:
        // Ticker-silent by design: a resumed span restating a living realm's
        // seat is a fact for the capital fold, not a moment. Never narrated
        // (the ticker filters the kind); this line is only what a caller
        // reading the prose directly would see.
        std::snprintf(buf, sizeof buf, "%s holds %s.", polity_name_of(h, e.polity), R);
        break;
    case lapse_event_kind::seat_captured:
        std::snprintf(buf, sizeof buf, "%s, seat of %s, falls to %s.",
                      R, polity_name_of(h, e.other), polity_name_of(h, e.polity));
        break;
    case lapse_event_kind::realm_ended:
        if (e.other == lapse_event_none)
            std::snprintf(buf, sizeof buf, "The realm of %s ends.", polity_name_of(h, e.polity));
        else
            std::snprintf(buf, sizeof buf, "The realm of %s ends; %s takes its last ground at %s.",
                          polity_name_of(h, e.polity), polity_name_of(h, e.other), R);
        break;
    case lapse_event_kind::broke_away:
        std::snprintf(buf, sizeof buf, "%s breaks away from %s.",
                      R, polity_name_of(h, e.other));
        break;
    case lapse_event_kind::capital_moved:
        std::snprintf(buf, sizeof buf, "%s re-seats itself at %s.",
                      polity_name_of(h, e.polity), R);
        break;
    case lapse_event_kind::road_promoted:
        std::snprintf(buf, sizeof buf, "A %s is laid between %s and %s.",
                      e.polity >= 2 ? "road" : "track", R, region_name_of(h, e.other));
        break;
    case lapse_event_kind::civilisation_formed:
        // BL-1106: `other` is the civilisation index; its coined name is on
        // the record's name table. The wording is the history log's own.
        std::snprintf(buf, sizeof buf, "%s is settled as a shared way of life at %s.",
                      civilisation_name_of(h, e.other), R);
        break;
    case lapse_event_kind::creed_preached:
        // BL-1106: `other` is the creed index, named off the same table.
        std::snprintf(buf, sizeof buf, "%s is preached at %s, and claims every people.",
                      creed_name_of(h, static_cast<int32_t>(e.other)), R);
        break;
    case lapse_event_kind::schism:
    {
        // BL-1106: both parties and the creed. `polity` is the realm that
        // walked out, seated at `region` (its name today, as broke_away's
        // idiom names the successor by its seat); `other` is the parent, and
        // the creed is the parent's — the institution the seceding people
        // left, read off `polity_creed` since no event carries it.
        const int32_t creed = creed_of_polity(h, e.other);
        if (creed >= 0)
            std::snprintf(buf, sizeof buf, "%s breaks with %s and its creed, %s.",
                          R, polity_name_of(h, e.other), creed_name_of(h, creed));
        else
            std::snprintf(buf, sizeof buf, "%s breaks with %s over its creed.",
                          R, polity_name_of(h, e.other));
        break;
    }
    case lapse_event_kind::culture_split:
    {
        // BL-1091: NAMED WHERE THE RECORD CAN (`people_name`, read off the
        // settlement's roster at the record's construction); the unnamed
        // line stays for a record with no roster behind it.
        const char* D = (static_cast<std::size_t>(e.polity) < h.people_name.size()
                         && !h.people_name[e.polity].empty())
                            ? h.people_name[e.polity].c_str() : nullptr;
        const char* P = (e.other != lapse_event_none
                         && static_cast<std::size_t>(e.other) < h.people_name.size()
                         && !h.people_name[e.other].empty())
                            ? h.people_name[e.other].c_str() : nullptr;
        if (D != nullptr && P != nullptr && e.region != lapse_event_none)
            std::snprintf(buf, sizeof buf, "The %s part from the %s at %s.", D, P, R);
        else if (D != nullptr && P != nullptr)
            std::snprintf(buf, sizeof buf, "The %s part from the %s on the march.", D, P);
        else if (e.region == lapse_event_none)
            std::snprintf(buf, sizeof buf, "A new people parts from its kin on the march.");
        else
            std::snprintf(buf, sizeof buf, "A new people parts from its kin at %s.", R);
        break;
    }
    case lapse_event_kind::cradle:
    {
        // BL-1091 (Ben, 2026-09-24, rulings R12; COLONISATION.md sec The
        // domestication package, "the cradle is announced"): one line per
        // people at the span's start, naming where it began and the package
        // it raised there. `polity` is the culture; the name and the package
        // prose are the record's own tables, resolved read-side from the
        // settlement's pure-output records -- nothing here is re-derived.
        const char* N = (static_cast<std::size_t>(e.polity) < h.people_name.size()
                         && !h.people_name[e.polity].empty())
                            ? h.people_name[e.polity].c_str() : nullptr;
        const char* K = (static_cast<std::size_t>(e.polity) < h.people_package.size()
                         && !h.people_package[e.polity].empty())
                            ? h.people_package[e.polity].c_str() : nullptr;
        // SHORT, because the ticker's column elides at about sixty characters
        // (BL-714) and two generated names already spend half of them: the
        // package follows a colon as a bare list, never a clause.
        if (N != nullptr && K != nullptr)
            std::snprintf(buf, sizeof buf, "The %s begin at %s: %s.", N, R, K);
        else if (N != nullptr)
            std::snprintf(buf, sizeof buf, "The %s begin at %s.", N, R);
        else if (K != nullptr)
            std::snprintf(buf, sizeof buf, "A people begin at %s: %s.", R, K);
        else
            std::snprintf(buf, sizeof buf, "A people begin at %s.", R);
        break;
    }
    case lapse_event_kind::supply_site_upgraded:
        std::snprintf(buf, sizeof buf, "%s buys a waystation, widening its own reach.", R);
        break;
    case lapse_event_kind::trade_link_opened:
        std::snprintf(buf, sizeof buf, "%s opens trade across the border with %s.",
                      R, region_name_of(h, e.other));
        break;
    case lapse_event_kind::trade_link_closed:
        std::snprintf(buf, sizeof buf, "A grudge closes the trade route between %s and %s.",
                      R, region_name_of(h, e.other));
        break;
    case lapse_event_kind::treaty_formed:
        std::snprintf(buf, sizeof buf, "%s binds a treaty with %s.",
                      polity_name_of(h, e.polity), polity_name_of(h, e.other));
        break;
    case lapse_event_kind::treaty_broken:
        std::snprintf(buf, sizeof buf, "%s breaks its treaty with %s.",
                      polity_name_of(h, e.polity), polity_name_of(h, e.other));
        break;
    case lapse_event_kind::subject_bound:
        std::snprintf(buf, sizeof buf, "%s falls under the overlordship of %s.",
                      polity_name_of(h, e.polity), polity_name_of(h, e.other));
        break;
    case lapse_event_kind::furnace_lit:
        // BL-1080: the crossing, named by region and by the realm that holds it.
        if (e.polity == lapse_event_none)
            std::snprintf(buf, sizeof buf, "%s lights its furnaces.", R);
        else
            std::snprintf(buf, sizeof buf, "%s, in the realm of %s, lights its furnaces.",
                          R, polity_name_of(h, e.polity));
        break;
    case lapse_event_kind::rung_crossed:
        // BL-1100: the realm's crossing, named by the realm and by the capital
        // it crossed at (STARTUP.md § Round 6: "the realm of Y lights its
        // furnaces at X").
        if (e.polity == lapse_event_none)
            std::snprintf(buf, sizeof buf, "A realm lights its furnaces at %s.", R);
        else
            std::snprintf(buf, sizeof buf, "The realm of %s lights its furnaces at %s.",
                          polity_name_of(h, e.polity), R);
        break;
    case lapse_event_kind::subject_freed:
        std::snprintf(buf, sizeof buf, "%s refuses renewal and breaks from %s.",
                      polity_name_of(h, e.polity), polity_name_of(h, e.other));
        break;
    case lapse_event_kind::province_bought:
        // BL-1096: the purchase, distinct from "falls under" -- `other` is the
        // buyer, `polity` the native whose province it now is by purchase.
        std::snprintf(buf, sizeof buf, "%s buys the province of %s, and keeps its customs.",
                      polity_name_of(h, e.other), polity_name_of(h, e.polity));
        break;
    case lapse_event_kind::sea_lane_opened:
        // BL-1097: the crossing that made a leg a lane; region/other = the ends.
        std::snprintf(buf, sizeof buf, "A sea lane opens between %s and %s.",
                      R, region_name_of(h, e.other));
        break;
    case lapse_event_kind::works_chartered:
    {
        // BL-1099: the note, named by region and by the focus a firm chartered
        // there takes (`other` = industrial_focus: 0 extraction, 1 processing,
        // 2 trade); a holder, when the ground has one.
        const char* kind = e.other == 0 ? "An extraction works" : e.other == 1 ? "A processing works"
                         : e.other == 2 ? "A trading house"     : "A works";
        if (e.polity == lapse_event_none)
            std::snprintf(buf, sizeof buf, "%s is chartered at %s.", kind, R);
        else
            std::snprintf(buf, sizeof buf, "%s is chartered at %s, in the realm of %s.",
                          kind, R, polity_name_of(h, e.polity));
        break;
    }
    default:
        // Unreachable on a record this build wrote: every kind above `count`
        // has its own line, and a newer writer's kind is refused with the
        // save (save_game.cpp's range check). Named honestly if it ever is.
        std::snprintf(buf, sizeof buf, "An unrecorded kind of moment at %s.", R);
        break;
    }
    return lapse_year_label(e.year) + "  " + buf;
}

namespace {

/// BL-1106: which kinds the ticker narrates, and at what priority, by how
/// RARE the kind is — the ticker has six rows and a sim round dates every
/// event in it to one year, so on a busy year something must drop, and it
/// should never be the moment that happens twice a world. 0 = the RAREST
/// moments (a civilisation settled, a creed preached, a schism, a furnace
/// lit: a handful per world); 1 = the arc's common kinds (foundings, seats
/// falling, realms ending: hundreds per world); 2 = CHURN, the trade and
/// treaty kinds that fire several a year and were crowding the arc off the
/// ticker; -1 = never narrated. Measured on the reference world: with two
/// tiers, the civilisation settled in 1156 was crowded off by six same-year
/// foundings and captures.
int ticker_priority(const lapse_event& e)
{
    switch (static_cast<lapse_event_kind>(e.kind))
    {
    // ROADS ARE RINGED, NOT NARRATED. A road promotion is the commonest event
    // by far — on a full-span world they outnumber every other kind together —
    // and a ticker that prints them shows nothing else for the last centuries
    // of the replay (measured on the first capture: eight road lines, no
    // realm). They still pulse on the map; the ticker is for the moments of
    // the ARC, which is what the round exists to show.
    case lapse_event_kind::road_promoted:        return -1;
    // BL-1088: AN INHERITED SEAT IS NOT A MOMENT. A resumed span restates
    // where every living realm sits (the capital fold reads it); the ticker
    // stays silent on it, because nothing rose — a "rises" at 1200 for a
    // realm on round 4's final board told the player it was born that year.
    case lapse_event_kind::inherited:            return -1;
    // BL-1091: A CRADLE OPENS THE ROUND AND THEN GIVES WAY. It takes the
    // COMMON tier, not the rarest: at 2400 BCE nothing else has happened, so
    // the twelve announcements are the ticker's first lines by themselves,
    // and each living split that follows displaces one, newest first. At the
    // rarest tier they held the whole ticker until twenty-four later moments
    // had passed -- most of the round, measured on the reference world.
    case lapse_event_kind::civilisation_formed:
    case lapse_event_kind::creed_preached:
    case lapse_event_kind::schism:
    case lapse_event_kind::furnace_lit:
    case lapse_event_kind::rung_crossed:         return 0; // BL-1100: a realm's crossing, a handful per world
    // BL-1099: a works note is CHURN on a busy world (up to four per region),
    // so it takes the ticker only when the arc is quiet; its glyph on the map
    // is the primary telling.
    case lapse_event_kind::works_chartered:      return 2;
    case lapse_event_kind::supply_site_upgraded:
    case lapse_event_kind::trade_link_opened:
    case lapse_event_kind::trade_link_closed:
    case lapse_event_kind::treaty_formed:
    case lapse_event_kind::treaty_broken:
    case lapse_event_kind::sea_lane_opened:      return 2;
    default:                                     return 1;
    }
}

constexpr int k_ticker_tiers = 3;

} // namespace

std::vector<int> lapse_ticker_rows(const history_lapse& h, int year, int max_rows)
{
    std::vector<int> rows;
    if (h.lapse.events.empty() || max_rows <= 0) return rows;

    // The last event at or before the playhead: one binary search over the
    // ascending list.
    int lo = 0, hi = static_cast<int>(h.lapse.events.size()) - 1, last = -1;
    while (lo <= hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (h.lapse.events[static_cast<std::size_t>(mid)].year <= year) { last = mid; lo = mid + 1; }
        else hi = mid - 1;
    }

    // PRIORITY BY KIND INSIDE A RECENT WINDOW (BL-1106). The window is the
    // last `4 * max_rows` narrated events back from the playhead — an event
    // count rather than a span of years, so it scales with how busy the world
    // is and never reaches back to a founding a millennium old. Inside it each
    // tier is taken in turn, newest first — the rarest moments, then the arc's
    // common kinds, then churn — until the rows are full; on a quiet world
    // with one kind of event this is exactly the old "last max_rows narrated
    // events".
    std::vector<int> tier[k_ticker_tiers]; // Each newest first.
    const int window = max_rows * 4;
    int seen = 0;
    for (int i = last; i >= 0 && seen < window; --i)
    {
        const lapse_event& e = h.lapse.events[static_cast<std::size_t>(i)];
        const int pr = ticker_priority(e);
        if (pr < 0) continue;
        // BL-1092: A FOLDED DAUGHTER LEAVES THE TICKER (Ben, 2026-09-24,
        // rulings R11). About 85% of the splits the migration coins fold back
        // into an ancestor at the round's close (BL-1017) and never hold a
        // name on the map; their lines crowded off the splits that took
        // ground. The board's census counts them instead. The record still
        // carries every split -- this is the ticker's filter, not the fold's.
        if (e.kind == static_cast<uint8_t>(lapse_event_kind::culture_split)
         && static_cast<std::size_t>(e.polity) < h.culture_folded.size()
         && h.culture_folded[e.polity] != 0)
            continue;
        tier[pr].push_back(i);
        ++seen;
    }
    for (int t = 0; t < k_ticker_tiers; ++t)
        for (std::size_t k = 0; k < tier[t].size() && static_cast<int>(rows.size()) < max_rows; ++k)
            rows.push_back(tier[t][k]);
    std::sort(rows.begin(), rows.end()); // Oldest first; the newest shown is last.
    return rows;
}

void draw_lapse_ticker(const history_lapse& h, int year, int max_rows)
{
    if (h.lapse.events.empty() || max_rows <= 0) return;

    ImGui::SeparatorText("As it happened");
    const std::vector<int> rows = lapse_ticker_rows(h, year, max_rows);
    if (rows.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextUnformatted("Nothing yet.");
        ImGui::PopStyleColor();
        return;
    }

    const float avail = ImGui::GetContentRegionAvail().x;
    for (std::size_t k = 0; k < rows.size(); ++k)
    {
        const int i = rows[k];
        const std::string line = lapse_event_prose(h, h.lapse.events[static_cast<std::size_t>(i)]);
        // The newest line is the one the marker on the map belongs to.
        ImGui::PushStyleColor(ImGuiCol_Text, k + 1 == rows.size() ? col_bright : col_dim);
        // Elided and RECORDED (BL-714): a long generated name in a narrow
        // column is the shape that overruns, and a wrapped ticker would push
        // the board off the fold.
        ui::fit_text(text_box::table_cell, "wizard.round4.ticker", line.c_str(),
                     std::max(24.0f, avail));
        ImGui::PopStyleColor();
    }
}

} // namespace ui
