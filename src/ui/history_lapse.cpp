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
ImU32 polity_colour(const history_lapse& h, uint16_t owner)
{
    const int32_t slot = (owner < h.polity_slot.size()) ? h.polity_slot[owner]
                                                        : static_cast<int32_t>(owner);
    return palette::lapse_polity_colour(slot);
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
ImU32 owner_colour(const history_lapse& h, uint16_t owner)
{
    if (owner < h.culture_colour.size())
        return static_cast<ImU32>(h.culture_colour[owner]);
    return polity_colour(h, owner);
}

} // namespace

void build_lineage_palette(history_lapse& h, const std::vector<int32_t>& parent)
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
    int roots = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (parent[i] < 0 || static_cast<std::size_t>(parent[i]) >= i) ++roots;
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

    int root_rank = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const int32_t p = parent[i];
        // A root, or a malformed link (a parent at or above its daughter) that is
        // treated as one rather than followed — the tree contract is that ids
        // are handed out in arrival order, and a violation is a root, not a loop.
        if (p < 0 || static_cast<std::size_t>(p) >= i)
        {
            h.culture_family[i] = static_cast<int32_t>(i);
            h.culture_hue[i]    = wedge * static_cast<float>(root_rank++);
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
    std::deque<int> frontier;
    for (std::size_t r = 0; r < h.region_col.size(); ++r)
    {
        const int c = h.region_col[r], w = h.region_row[r];
        if (c < 0 || c >= gw || w < 0 || w >= gh) continue;
        const int idx = w * gw + c;
        if (is_water(preview_substrate(packed[static_cast<std::size_t>(idx)]))) continue;
        if (h.tile_region[static_cast<std::size_t>(idx)] >= 0) continue; // two anchors, one tile
        h.tile_region[static_cast<std::size_t>(idx)] = static_cast<int32_t>(r);
        frontier.push_back(idx);
    }

    while (!frontier.empty())
    {
        const int idx = frontier.front();
        frontier.pop_front();
        const int32_t owner = h.tile_region[static_cast<std::size_t>(idx)];
        const int col = idx % gw, row = idx / gw;

        // Columns WRAP as the equator does; rows do not. The same rule the
        // world's own grid keeps (hard_coded_world.hpp § the homeworld grid).
        const int steps[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
        for (const auto& s : steps)
        {
            int nc = col + s[0], nr = row + s[1];
            if (nr < 0 || nr >= gh) continue;
            if (nc < 0)   nc += gw;
            if (nc >= gw) nc -= gw;
            const std::size_t ni = static_cast<std::size_t>(nr * gw + nc);
            if (h.tile_region[ni] >= 0) continue;
            if (is_water(preview_substrate(packed[ni]))) continue;
            h.tile_region[ni] = owner;
            frontier.push_back(static_cast<int>(ni));
        }
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

    // The Culture round's record carries a lineage palette (BL-919); its hue
    // families seed the slot walk so kin start near one another on the wheel.
    assign_polity_colours(h, h.culture_family.empty() ? nullptr : &h.culture_family);
}

void assign_polity_colours(history_lapse& h, const std::vector<int32_t>* hue_family)
{
    h.polity_slot.clear();
    if (h.tile_region.empty()) return;

    const int gw = h.grid_w, gh = h.grid_h;
    const std::size_t nreg = h.region_col.size();

    // 1. REGION adjacency, off the nearest-region raster: two regions touch if
    //    any two of their tiles do, under the same four-neighbour, column-wrapping
    //    rule the raster itself was walked with. A sorted, deduplicated edge list
    //    rather than an n^2 matrix — a few hundred regions, a few hundred edges.
    std::vector<std::pair<int32_t, int32_t>> region_edges;
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
        {
            const int32_t a = h.tile_region[static_cast<std::size_t>(r * gw + c)];
            if (a < 0) continue;
            const int steps[2][2] = { {1, 0}, {0, 1} }; // east and south: each pair once
            for (const auto& s : steps)
            {
                int nc = c + s[0], nr = r + s[1];
                if (nr >= gh) continue;
                if (nc >= gw) nc -= gw;
                const int32_t b = h.tile_region[static_cast<std::size_t>(nr * gw + nc)];
                if (b < 0 || b == a) continue;
                region_edges.emplace_back(std::min(a, b), std::max(a, b));
            }
        }
    std::sort(region_edges.begin(), region_edges.end());
    region_edges.erase(std::unique(region_edges.begin(), region_edges.end()), region_edges.end());

    std::vector<std::vector<int32_t>> region_nbrs(nreg);
    for (const auto& e : region_edges)
    {
        region_nbrs[static_cast<std::size_t>(e.first)].push_back(e.second);
        region_nbrs[static_cast<std::size_t>(e.second)].push_back(e.first);
    }

    // 2. POLITY adjacency, across the WHOLE record rather than one slice. Two
    //    polities are adjacent if they ever held neighbouring regions at the
    //    same time — and a neighbouring pair can only come into being when one
    //    side changes hands, so folding the change list and looking around each
    //    changed region sees every pair that ever existed. Linear in changes
    //    times region degree.
    std::size_t npol = h.polity_seat.size();
    std::vector<uint16_t> owner(nreg, owner_none);
    std::vector<std::pair<int32_t, int32_t>> polity_edges;
    for (const owner_change& ch : h.lapse.changes)
    {
        if (ch.region >= nreg) continue;
        owner[ch.region] = ch.owner;
        if (ch.owner == owner_none) continue;
        if (ch.owner >= npol) npol = static_cast<std::size_t>(ch.owner) + 1;
        for (const int32_t nb : region_nbrs[ch.region])
        {
            const uint16_t o = owner[static_cast<std::size_t>(nb)];
            if (o == owner_none || o == ch.owner) continue;
            polity_edges.emplace_back(std::min<int32_t>(ch.owner, o),
                                      std::max<int32_t>(ch.owner, o));
        }
    }
    std::sort(polity_edges.begin(), polity_edges.end());
    polity_edges.erase(std::unique(polity_edges.begin(), polity_edges.end()), polity_edges.end());

    std::vector<std::vector<int32_t>> polity_nbrs(npol);
    for (const auto& e : polity_edges)
    {
        polity_nbrs[static_cast<std::size_t>(e.first)].push_back(e.second);
        polity_nbrs[static_cast<std::size_t>(e.second)].push_back(e.first);
    }

    // 3. GREEDY, in a fixed order: highest degree first, then index, which is
    //    the standard heuristic and deterministic. Each polity takes the lowest
    //    slot no already-coloured neighbour holds. With a HUE FAMILY supplied,
    //    the search starts at the family's band of the palette and wraps, so
    //    kin share a neighbourhood of hues where the constraint allows it. If
    //    every slot is taken by a neighbour (a degree above the palette, which
    //    the greedy order makes unlikely), the least-used slot among the
    //    neighbours is taken and that one shared edge is left to the frontier
    //    line — a bound met honestly rather than a palette silently widened.
    std::vector<int32_t> order(npol);
    for (std::size_t i = 0; i < npol; ++i) order[i] = static_cast<int32_t>(i);
    std::sort(order.begin(), order.end(), [&](int32_t a, int32_t b) {
        const std::size_t da = polity_nbrs[static_cast<std::size_t>(a)].size();
        const std::size_t db = polity_nbrs[static_cast<std::size_t>(b)].size();
        if (da != db) return da > db;
        return a < b;
    });

    constexpr int k_slots = palette::lapse_polity_slot_count;
    h.polity_slot.assign(npol, -1);
    for (const int32_t p : order)
    {
        int used[k_slots] = {};
        for (const int32_t nb : polity_nbrs[static_cast<std::size_t>(p)])
        {
            const int32_t s = h.polity_slot[static_cast<std::size_t>(nb)];
            if (s >= 0) ++used[s % k_slots];
        }
        const bool has_family = hue_family != nullptr
                             && static_cast<std::size_t>(p) < hue_family->size()
                             && (*hue_family)[static_cast<std::size_t>(p)] >= 0;
        const int start = has_family
                              ? ((*hue_family)[static_cast<std::size_t>(p)] * 7) % k_slots
                              : 0;
        int chosen = -1;
        for (int k = 0; k < k_slots && chosen < 0; ++k)
            if (used[(start + k) % k_slots] == 0) chosen = (start + k) % k_slots;
        if (chosen < 0)
        {
            chosen = 0;
            for (int k = 1; k < k_slots; ++k)
                if (used[k] < used[chosen]) chosen = k;
        }
        h.polity_slot[static_cast<std::size_t>(p)] = chosen;
    }
}

// ---------------------------------------------------------------------------
// The map
// ---------------------------------------------------------------------------

uint32_t lapse_owner_colour(const history_lapse& h, uint16_t owner)
{
    return static_cast<uint32_t>(owner_colour(h, owner));
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
    const float scale = std::min(avail.x / static_cast<float>(gw),
                                 avail.y / static_cast<float>(gh));
    const float mw = scale * static_cast<float>(gw);
    const float mh = scale * static_cast<float>(gh);

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
    // would only disguise that.
    constexpr float map_nudge_x = 120.0f;
    const ImVec2 tl{origin.x + (avail.x - mw) * 0.5f + map_nudge_x,
                    origin.y + (avail.y - mh) * 0.5f};

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
    // Owner keys: -1 sea (nothing drawn, nothing bordered), -2 wild, else the
    // polity index. The frontier is drawn between any two DIFFERENT non-sea
    // keys, on both axes.
    const float carry_fade = lapse_carry_fade(h, year);
    std::vector<int32_t> row(static_cast<std::size_t>(gw));
    std::vector<int32_t> above(static_cast<std::size_t>(gw), -1);
    std::vector<char>    present; // owner -> holds ground in this slice
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
                                  with_alpha(owner_colour(h, o),
                                             static_cast<int>(tint_alpha * (1.0f - carry_fade))));
                ++prims;
            }
            else if (key == -2 && carry_fade > 0.0f)
            {
                // THE ROUND BEFORE THIS ONE, under ground nobody holds yet.
                // Only under UNCLAIMED land: over a holder it would be a second
                // tint nobody could read, and the moment a city state organises
                // its ground the old colour is gone there — which is the
                // hand-over made visible rather than narrated. The run merged
                // above is one owner key, not one region, so this walks the run
                // and strokes each stretch of one carried colour.
                int k = c;
                while (k < e)
                {
                    const int32_t reg = h.tile_region[static_cast<std::size_t>(r * gw + k)];
                    const uint32_t col = (reg >= 0 && static_cast<std::size_t>(reg) < h.carry_colour.size())
                                             ? h.carry_colour[static_cast<std::size_t>(reg)] : 0u;
                    int k2 = k + 1;
                    while (k2 < e)
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
            // The VERTICAL frontier: between this run and the one to its west.
            if (key >= -2 && c > 0)
            {
                const int32_t west = row[static_cast<std::size_t>(c - 1)];
                if (west >= -2 && west != key)
                {
                    // +0.5: a 1 px line centred ON the pixel column, not
                    // anti-aliased across two.
                    dl->AddLine({px(static_cast<float>(c)) + 0.5f, y0},
                                {px(static_cast<float>(c)) + 0.5f, y1}, col_frontier, 1.0f);
                    ++prims;
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
                dl->AddLine({px(static_cast<float>(s)), y0 + 0.5f},
                            {px(static_cast<float>(e)) + 1.0f, y0 + 0.5f}, col_frontier, 1.0f);
                ++prims;
                s = e;
            }
        }
        std::swap(row, above);
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
    dl->PushClipRect({tl.x, tl.y}, {tl.x + static_cast<float>(gw) * scale,
                                    tl.y + static_cast<float>(gh) * scale}, true);
    for (const lapse_road_seg& s : h.road_segs)
    {
        if (year < s.year_track) continue; // not promoted yet at this playhead
        const bool at_road = year >= s.year_road;
        const float w = at_road ? std::max(1.5f, scale * 0.30f)
                                : std::max(1.0f, scale * 0.16f);
        const ImU32 col = at_road ? col_road : col_road_track;
        const bool  wrapped = s.c1 < 0.0f || s.c1 > static_cast<float>(gw);
        const float shift   = s.c1 < 0.0f ? world_w : -world_w;
        dl->AddLine({px(s.c0), py(s.r0)}, {px(s.c1), py(s.r1)}, col, w);
        ++prims;
        if (wrapped)
        {
            dl->AddLine({px(s.c0) + shift, py(s.r0)}, {px(s.c1) + shift, py(s.r1)}, col, w);
            ++prims;
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
            const int32_t seat = h.polity_seat[o];
            if (seat < 0 || static_cast<std::size_t>(seat) >= h.region_col.size()) continue;
            const ImVec2 at{px(static_cast<float>(h.region_col[static_cast<std::size_t>(seat)]) + 0.5f),
                            py(static_cast<float>(h.region_row[static_cast<std::size_t>(seat)]) + 0.5f)};
            dl->AddCircleFilled(at, rad, owner_colour(h, static_cast<uint16_t>(o)), 10);
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
                const int32_t seat = h.polity_seat[o];
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
    // without looking away from the frontier.
    {
        const std::string label = lapse_year_label(year);
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
    // those are lines on a thing, not pulses over it. A future surface that
    // wants a mark back should ask which kinds earn one rather than restore
    // the blanket (STARTUP.md § Rounds 4 and 5).

    ImGui::Dummy(avail);
}

int lapse_marker_window_years(const history_lapse& h)
{
    // One screen-second of the transport (startup_screens.cpp: span / 30 s).
    return std::max(1, h.lapse.years / 30);
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

/// A region's generated name, or an honest placeholder — never an Earth name.
const char* region_name_of(const history_lapse& h, uint16_t region)
{
    if (region == lapse_event_none || static_cast<std::size_t>(region) >= h.region_name.size()
     || h.region_name[region].empty())
        return "unnamed ground";
    return h.region_name[region].c_str();
}

/// A polity's name is its seat's name — the same rule the board uses.
const char* polity_name_of(const history_lapse& h, uint16_t polity)
{
    if (polity == lapse_event_none || static_cast<std::size_t>(polity) >= h.polity_seat.size()
     || h.polity_seat[polity] < 0)
        return "an unnamed realm";
    return region_name_of(h, static_cast<uint16_t>(h.polity_seat[polity]));
}

} // namespace

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

    ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
    ImGui::Text("%d powers hold ground; the top %d are listed.",
                static_cast<int>(now.size()), k_board_rows);
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
    if (!ImGui::BeginTable("##lapse_board", 6,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        return;

    // Short headers throughout: the header is drawn by ImGui's own
    // TableHeadersRow, which neither elides nor reports, so a header wider than
    // its column simply clips ("Popul...") with nothing recording that it did.
    // "People" is the rank column and reads as a share; "Pop" is the headcount.
    ImGui::TableSetupColumn("#",      ImGuiTableColumnFlags_WidthFixed, 22.0f);
    ImGui::TableSetupColumn("Seat",   ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("People", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Land",   ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Pop",    ImGuiTableColumnFlags_WidthFixed, 46.0f);
    ImGui::TableSetupColumn("Mt",     ImGuiTableColumnFlags_WidthFixed, 24.0f);
    ImGui::TableHeadersRow();

    const int shown = std::min(k_board_rows, static_cast<int>(now.size()));
    for (int i = 0; i < shown; ++i)
    {
        const board_row& b = now[static_cast<std::size_t>(i)];
        ImGui::TableNextRow();

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
                {p.x, p.y + 2.0f}, {p.x + s * 0.55f, p.y + s - 1.0f}, owner_colour(h, b.owner));
            swatch_w = s * 0.55f + 5.0f + ImGui::GetStyle().ItemSpacing.x;
            ImGui::Dummy({s * 0.55f + 5.0f, s});
            ImGui::SameLine();
        }
        {
            const int32_t seat = (b.owner < h.polity_seat.size())
                                     ? h.polity_seat[b.owner] : -1;
            const char* nm = (seat >= 0 && static_cast<std::size_t>(seat) < h.region_name.size()
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

            if (entered) ImGui::PushStyleColor(ImGuiCol_Text, col_bright);
            ui::fit_text(text_box::table_cell, "wizard.round4.seat", cell,
                         std::max(24.0f, name_avail - swatch_w));
            if (entered) ImGui::PopStyleColor();
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
            ImGui::Text("%d.%d%%", q / 10, q % 10);
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
    }
    ImGui::EndTable();

    if (static_cast<int>(now.size()) > shown)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::Text("and %d more below the board.",
                    static_cast<int>(now.size()) - shown);
        ImGui::PopStyleColor();
    }
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
    if (a.eliminated == 0 && a.rose_and_fell == 0 && peak_q < 100)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("A quiet age. %d peoples held ground and none of them "
                           "grew large or was destroyed.", a.polities);
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::Text("%d polities, %d destroyed.", a.polities, a.eliminated);
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
        std::snprintf(buf, sizeof buf, "A realm rises at %s.", R);
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
        std::snprintf(buf, sizeof buf, "Two peoples settle a shared way of life at %s.", R);
        break;
    case lapse_event_kind::creed_preached:
        std::snprintf(buf, sizeof buf, "A creed is preached at %s, and claims every people.", R);
        break;
    case lapse_event_kind::culture_split:
        if (e.region == lapse_event_none)
            std::snprintf(buf, sizeof buf, "A new people parts from its kin on the march.");
        else
            std::snprintf(buf, sizeof buf, "A new people parts from its kin at %s.", R);
        break;
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
    case lapse_event_kind::subject_freed:
        std::snprintf(buf, sizeof buf, "%s refuses renewal and breaks from %s.",
                      polity_name_of(h, e.polity), polity_name_of(h, e.other));
        break;
    default:
        std::snprintf(buf, sizeof buf, "Something happens at %s.", R);
        break;
    }
    return lapse_year_label(e.year) + "  " + buf;
}

void draw_lapse_ticker(const history_lapse& h, int year, int max_rows)
{
    if (h.lapse.events.empty() || max_rows <= 0) return;

    // The last event at or before the playhead: one binary search over the
    // ascending list, then up to `max_rows` NARRATED events back from it.
    int lo = 0, hi = static_cast<int>(h.lapse.events.size()) - 1, last = -1;
    while (lo <= hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (h.lapse.events[static_cast<std::size_t>(mid)].year <= year) { last = mid; lo = mid + 1; }
        else hi = mid - 1;
    }

    // ROADS ARE RINGED, NOT NARRATED. A road promotion is the commonest event
    // by far — on a full-span world they outnumber every other kind together —
    // and a ticker that prints them shows nothing else for the last centuries
    // of the replay (measured on the first capture: eight road lines, no
    // realm). They still pulse on the map; the ticker is for the moments of
    // the ARC, which is what the round exists to show.
    const auto narrated = [](const lapse_event& e) {
        return e.kind != static_cast<uint8_t>(lapse_event_kind::road_promoted);
    };

    ImGui::SeparatorText("As it happened");
    std::vector<int> rows; // Indices, newest last.
    for (int i = last; i >= 0 && static_cast<int>(rows.size()) < max_rows; --i)
        if (narrated(h.lapse.events[static_cast<std::size_t>(i)])) rows.push_back(i);
    if (rows.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextUnformatted("Nothing yet.");
        ImGui::PopStyleColor();
        return;
    }
    std::reverse(rows.begin(), rows.end());

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
