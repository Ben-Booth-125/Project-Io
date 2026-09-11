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

} // namespace

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

    assign_polity_colours(h, nullptr);
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
                dl->AddRectFilled({px(static_cast<float>(c)), y0},
                                  {px(static_cast<float>(e)), y1},
                                  with_alpha(polity_colour(h, o), tint_alpha));
                ++prims;
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
            dl->AddCircleFilled(at, rad, polity_colour(h, static_cast<uint16_t>(o)), 10);
            dl->AddCircle(at, rad, col_seat_ring, 10, 1.0f);
            prims += 2;
        }
    }

    // The draw cost, reported once per record under the capture harness so the
    // bound is a measured number rather than an assumption. Never in play.
    if (!h.prim_report_done)
    {
        h.prim_report_done = true;
        std::fprintf(stderr, "history_lapse: %zu base runs, %zu relief rims, %zu river segments, "
                             "%d primitives this frame\n",
                     h.base_runs.size(), h.relief_segs.size(), h.river_segs.size(), prims);
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

    ImGui::Dummy(avail);
}

// ---------------------------------------------------------------------------
// The board
// ---------------------------------------------------------------------------

namespace {

struct board_row
{
    uint16_t owner   = owner_none;
    int32_t  land    = 0; ///< Tiles held.
    int      regions = 0;
};

/// Total the land each polity holds in one slice, ordered by it.
///
/// SHARE OF LAND is the ordering metric, and it is the honest default for a round
/// about borders (BL-830): it is the one quantity the ownership record alone can
/// answer, and share of LAND rather than of surface — an ocean nobody can hold
/// would otherwise compress every polity into the bottom of the axis.
std::vector<board_row> rank_slice(const history_lapse& h,
                                  const std::vector<uint16_t>& slice)
{
    std::vector<board_row> rows;
    for (std::size_t r = 0; r < slice.size() && r < h.region_area.size(); ++r)
    {
        const uint16_t o = slice[r];
        if (o == owner_none) continue;
        auto it = std::find_if(rows.begin(), rows.end(),
                               [&](const board_row& b) { return b.owner == o; });
        if (it == rows.end()) { rows.push_back({o, 0, 0}); it = rows.end() - 1; }
        it->land += h.region_area[r];
        ++it->regions;
    }
    // Deterministic: land descending, then polity index, so two equal holders do
    // not swap places frame to frame.
    std::sort(rows.begin(), rows.end(), [](const board_row& a, const board_row& b) {
        if (a.land != b.land) return a.land > b.land;
        return a.owner < b.owner;
    });
    return rows;
}

constexpr int k_board_rows = 16;

} // namespace

void draw_lapse_scoreboard(const history_lapse& h,
                           const std::vector<uint16_t>& slice,
                           const std::vector<uint16_t>& lagged)
{
    const std::vector<board_row> now  = rank_slice(h, slice);
    const std::vector<board_row> then = rank_slice(h, lagged);

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

    // POPULATION and MILITARY COLUMNS GO HERE (BL-817, in flight). They need a
    // per-polity sample series the ownership record does not carry; the record is
    // a change list over regions and nothing else. Add them as two columns beside
    // Land, ordered still by Land unless Ben rules otherwise.
    //
    // A RESEARCH COLUMN DOES NOT GO HERE, and that is a standing refusal rather
    // than a deferral: research points accrue from population (BL-822), so the
    // column would restate population under a second name and show a correlation
    // it never measured — on the board Ben is judging the research levers with.
    if (!ImGui::BeginTable("##lapse_board", 4,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        return;

    // "Rgn", not "Regions": the header is drawn by ImGui's own TableHeadersRow,
    // which neither elides nor reports, so a header wider than its column simply
    // clips ("Re...") with nothing recording that it did.
    ImGui::TableSetupColumn("#",    ImGuiTableColumnFlags_WidthFixed, 22.0f);
    ImGui::TableSetupColumn("Seat", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Land", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn("Rgn",  ImGuiTableColumnFlags_WidthFixed, 34.0f);
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
                {p.x, p.y + 2.0f}, {p.x + s * 0.55f, p.y + s - 1.0f}, polity_colour(h, b.owner));
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

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.1f%%", 100.0f * static_cast<float>(b.land)
                                     / static_cast<float>(held));

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%d", b.regions);
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

    // Per polity: first holding, peak holding, last holding. `polity` ids are
    // sparse, so a map keyed by id would iterate in an order the record does not
    // define — a vector indexed by id keeps this deterministic.
    std::vector<int> first_r, peak_r, last_r;
    for (const polity_sample& s : h.lapse.samples)
    {
        const std::size_t id = s.polity;
        if (id >= first_r.size())
        {
            first_r.resize(id + 1, -1);
            peak_r.resize(id + 1, 0);
            last_r.resize(id + 1, 0);
        }
        const int r = static_cast<int>(s.regions);
        if (first_r[id] < 0) first_r[id] = r;
        if (r > peak_r[id])  peak_r[id]  = r;
        last_r[id] = r;
    }

    int biggest_end = 0, smallest_end = 0;
    for (std::size_t id = 0; id < first_r.size(); ++id)
    {
        if (first_r[id] < 0) continue;       // never seen
        if (peak_r[id] <= 0) continue;       // never held ground
        ++out.polities;

        const int pq = (peak_r[id] * 1000) / stride;
        if (pq > out.peak_share_q) out.peak_share_q = pq;

        if (last_r[id] <= 0) { ++out.eliminated; continue; }

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
    if (a.eliminated == 0 && a.rose_and_fell == 0 && a.peak_share_q < 100)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("A quiet age. %d peoples held ground and none of them "
                           "grew large or was destroyed.", a.polities);
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::Text("%d polities, %d destroyed.", a.polities, a.eliminated);
        ImGui::Text("The largest empire held %d%% of the world; %d rose and fell back.",
                    a.peak_share_q / 10, a.rose_and_fell);
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("At the end the greatest holds %d%%, the least %d region%s.",
                           a.biggest_end_q / 10, a.smallest_end,
                           a.smallest_end == 1 ? "" : "s");
        ImGui::PopStyleColor();
    }
}

} // namespace ui
