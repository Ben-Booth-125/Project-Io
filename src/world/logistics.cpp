#include "logistics.hpp"
#include "river_generation.hpp"
#include "hex_neighbors.hpp" // BL-1186: a port reaches its sea across any hex side
#include "road_generation.hpp" // BL-1230: kMaxCrossingTiles, the strait a road crosses

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

/// Sea-leg traversal cost for an ocean tile — costlier than any land landform, so A*
/// prefers a land route and only crosses water when it must. A calibration constant.
constexpr float sea_leg_cost = 2.5f;

/// The anchor TILE set — every city, plus every built-and-active port / inland
/// logistics hub — exactly `is_supply_anchor`'s predicate, collected once
/// rather than probed per tile. Shared by `body_reach_field` (placement's
/// distance rule) and `active_lp_anchor_pools` (BL-596: active Logistic
/// Points, LOGISTICS.md § Logistic Points) — one anchor set, two consumers,
/// per BL-325 ruling 3 ("no second distance/anchor model").
std::unordered_set<entity_id> collect_anchor_tile_set(const world& w)
{
    std::unordered_set<entity_id> anchor_tiles;
    for (const auto& [centre, ctile] : w.population_centre_tile)
    {
        (void)centre;
        anchor_tiles.insert(ctile);
    }
    for (const auto& [bid, bc] : w.buildings)
    {
        (void)bid;
        if ((bc.type == building_type::port || bc.type == building_type::inland_logistics_hub)
            && bc.ticks_remaining <= 0 && !bc.decommissioned)
            anchor_tiles.insert(bc.tile);
    }
    return anchor_tiles;
}

/// Raster index for (col, row) with the column wrapped into [0, gw). Mirrors
/// nation_generation.cpp's raster_idx so the two share one grid convention.
inline int raster_idx(int col, int row, int gw)
{
    return ((col % gw) + gw) % gw + row * gw;
}


/// Lowest-cost-first priority-queue entry (mirrors nation_generation's bfs_entry).
struct pq_entry
{
    float cost;
    int   col;
    int   row;
    bool operator>(const pq_entry& o) const { return cost > o.cost; }
};

} // namespace

float landform_logistics_cost(terrain_landform lf)
{
    switch (lf)
    {
        case terrain_landform::plains:   return 1.0f;
        case terrain_landform::highland: return 1.25f;
        case terrain_landform::mountain: return 2.0f;
        case terrain_landform::canyon:   return 1.5f;
        case terrain_landform::valley:   return 1.1f;
        case terrain_landform::crater:   return 1.3f;
        case terrain_landform::rift:     return 1.6f;
    }
    return 1.0f;
}

float road_traversal_multiplier(std::uint8_t road_level)
{
    // Each tier cuts the traversal cost; tier 0 = 1.0. 1/(1 + 0.5*tier): Track(1) ~0.67,
    // Road(2) 0.50, Highway(3) 0.40 — diminishing returns up the ladder (BL-172).
    return 1.0f / (1.0f + 0.5f * static_cast<float>(road_level));
}

static float sea_lane_traversal_multiplier(std::uint8_t lane_level)
{
    // THE LANE READS AS THE LAND STAMP READS THE SAME COUNT (LOGISTICS.md § 4b): a leg
    // earns its lane at `sea_lane_tier1_uses` (4), and four uses is the count the ancient
    // stamp turns into a ROAD (`kAncientRoadUses`, § 4a) -- so the lane's one rung is the
    // road ladder's second rung on the water, road_traversal_multiplier(2) = x0.50. The sea
    // leg's 2.5 becomes 1.25 along a lane: still dearer than plains, cheaper than a rift.
    return lane_level > 0 ? road_traversal_multiplier(2) : 1.0f;
}

/// A tile's traversal cost: ocean = sea leg, land = landform cost, both scaled by the
/// road discount. The per-node weight; an edge cost is the average of its two nodes
/// (A* and body_reach_field's Dijkstra both do this — see their own comments).
///
/// PROMOTED OUT OF the anonymous namespace above for BL-470 (unit march seam):
/// run_unit_march (economy_system.cpp) spends a marching unit's march points
/// against this SAME per-tile weight, one hop at a time, rather than inventing
/// a second traversal-cost model. `sea_leg_cost` stays private to this file —
/// the only caller outside it reads land tiles a unit can actually stand on.
float tile_traversal_cost(const tile_component& tc)
{
    const float base = is_water(tc.substrate) // BL-516: water of any kind is the sea-mode leg
                           ? sea_leg_cost
                           : landform_logistics_cost(tc.landform);
    // BL-1098: a sea lane discounts the water it lies on (only ever stamped on sea tiles;
    // a land tile's lane_level is always 0, so this factor is 1 there).
    return base * road_traversal_multiplier(tc.road_level) * sea_lane_traversal_multiplier(tc.lane_level);
}


const std::vector<entity_id>& body_tile_grid(world& w, entity_id body)
{
    const auto it = w.body_tile_index.find(body);
    if (it != w.body_tile_index.end())
        return it->second;

    std::vector<entity_id> grid;
    const auto bit = w.bodies.find(body);
    if (bit != w.bodies.end())
    {
        const int gw = bit->second.grid_width;
        const int gh = bit->second.grid_height;
        if (gw > 0 && gh > 0)
        {
            grid.assign(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), null_entity);
            for (const auto& [tid, tc] : w.tiles)
            {
                if (tc.body != body)
                    continue;
                if (tc.grid_x < 0 || tc.grid_x >= gw || tc.grid_y < 0 || tc.grid_y >= gh)
                    continue;
                grid[static_cast<std::size_t>(tc.grid_y) * static_cast<std::size_t>(gw)
                     + static_cast<std::size_t>(tc.grid_x)] = tid;
            }
        }
    }
    const auto ins = w.body_tile_index.emplace(body, std::move(grid));
    return ins.first->second;
}

namespace {

/// 4-cardinal offsets, matching nation_generation::cardinal_neighbours (N, S, W, E).
constexpr int k_flood_off_dc[4] = {  0,  0, -1, 1 };
constexpr int k_flood_off_dr[4] = { -1,  1,  0, 0 };

/// THE DIRECTED EDGE both intra-body floods relax (flood_field_for and, BL-1117,
/// build_lp_anchor_field). A flood grows outward from its anchor, but what it
/// prices is TRAVEL TOWARD the anchor (BL-1126, path cost reads the cache;
/// LOGISTICS.md § 1: a path is directed, and its cost is always origin ->
/// destination). So reaching neighbour @p n_tc (row @p n_row) from the settled
/// cell (node weight @p cur_cost) along offset @p i prices the hop n -> settled:
/// the mean of the two node weights, times the river discount of the side of
/// @p n_tc the hop LEAVES by (river_edge_discount reads the tile being left;
/// downstream is cheaper than upstream). A field anchored at D then holds, at
/// every cell O, the cost of travelling O -> D, which is what intra_body_path(O,
/// D) reads. Both floods share this one expression so they cannot disagree.
inline float flood_edge_cost(float cur_cost, const tile_component& n_tc, int n_row, int i)
{
    // River discount (BL-170): a river-adjacent edge is cheaper, stacking
    // multiplicatively with the road-tier discount inside tile_traversal_cost.
    // The hop runs n -> settled: the reverse of offset i, from n's row parity.
    const int hex_side = hex_side_for_offset(-k_flood_off_dc[i], -k_flood_off_dr[i], (n_row & 1) != 0);
    const float river_mult = (hex_side >= 0) ? river_edge_discount(n_tc, hex_side) : 1.0f;
    return 0.5f * (cur_cost + tile_traversal_cost(n_tc)) * river_mult;
}

/// Fetch or build the COMPLETED flood field anchored at @p anchor_tile — the
/// full Dijkstra the pair search used to run per endpoint pair, run once and
/// kept on `world.logistics_flood_fields` (2026-08-25 warm-start stall).
/// dispatch_convoys prices hundreds of origins against the same few
/// destination centres, and rival place_road (BL-599) clears the pair cache at
/// tick rate, so the per-pair searches re-flooded the grid hundreds of times
/// every econ tick — the 2026-08-12 AppHangB1 disease with a new carrier.
///
/// A Dijkstra (the "A*" name carried no heuristic) with no early exit; a settled
/// node's parent is final. THE FIELD IS A DESTINATION'S (BL-1126): every edge is
/// priced as the hop TOWARD the anchor (`flood_edge_cost`), so `dist[i]` is the
/// cost of travelling cell i -> anchor and `came_from[i]` is cell i's next hop
/// on that route. `intra_body_path(o, d)` reads d's field at o, never o's field
/// at d — the two differ wherever a river discounts an edge one way only.
const logistics_flood_field& flood_field_for(world& w, entity_id body, entity_id anchor_tile,
                                             int gw, int gh,
                                             const std::vector<entity_id>& grid,
                                             const tile_component& anchor_tc)
{
    const auto key = std::make_pair(body, anchor_tile);
    const auto fit = w.logistics_flood_fields.find(key);
    if (fit != w.logistics_flood_fields.end())
        return fit->second;

    logistics_flood_field f;
    const int total = gw * gh;
    f.dist.assign(static_cast<std::size_t>(total), 1e30f);
    f.came_from.assign(static_cast<std::size_t>(total), -1);
    f.crossed.assign(static_cast<std::size_t>(total), 0);
    std::vector<char> settled(static_cast<std::size_t>(total), 0);

    const auto tile_at = [&](int idx) -> const tile_component* {
        const entity_id tid = grid[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            return nullptr;
        const auto tit = w.tiles.find(tid);
        return (tit != w.tiles.end()) ? &tit->second : nullptr;
    };

    const int ac = anchor_tc.grid_x, ar = anchor_tc.grid_y;
    f.anchor_idx = raster_idx(ac, ar, gw);
    f.dist[static_cast<std::size_t>(f.anchor_idx)]    = 0.0f;
    f.crossed[static_cast<std::size_t>(f.anchor_idx)] =
        is_water(anchor_tc.substrate) ? 1 : 0; // BL-516

    std::priority_queue<pq_entry, std::vector<pq_entry>, std::greater<pq_entry>> pq;
    pq.push({ 0.0f, ac, ar });

    while (!pq.empty())
    {
        const pq_entry cur = pq.top();
        pq.pop();
        const int idx = raster_idx(cur.col, cur.row, gw);
        if (settled[static_cast<std::size_t>(idx)])
            continue;
        settled[static_cast<std::size_t>(idx)] = 1;

        const tile_component* cur_tc = tile_at(idx);
        if (!cur_tc)
            continue;
        const float cur_cost = tile_traversal_cost(*cur_tc);

        for (int i = 0; i < 4; ++i)
        {
            const int nr = cur.row + k_flood_off_dr[i];
            if (nr < 0 || nr >= gh)
                continue;
            const int nc = ((cur.col + k_flood_off_dc[i]) % gw + gw) % gw;
            const int nidx = raster_idx(nc, nr, gw);
            if (settled[static_cast<std::size_t>(nidx)])
                continue;
            const tile_component* n_tc = tile_at(nidx);
            if (!n_tc)
                continue; // absent grid cell — impassable

            const float edge = flood_edge_cost(cur_cost, *n_tc, nr, i);
            const float nd   = f.dist[static_cast<std::size_t>(idx)] + edge;
            if (nd < f.dist[static_cast<std::size_t>(nidx)])
            {
                f.dist[static_cast<std::size_t>(nidx)] = nd;
                f.crossed[static_cast<std::size_t>(nidx)] =
                    (f.crossed[static_cast<std::size_t>(idx)]
                     || is_water(n_tc->substrate)) ? 1 : 0; // BL-516
                f.came_from[static_cast<std::size_t>(nidx)] = idx;
                pq.push({ nd, nc, nr });
            }
        }
    }

    return w.logistics_flood_fields.emplace(key, std::move(f)).first->second;
}

} // namespace

std::vector<std::pair<int, float>> bounded_cost_to_tile(world& w, entity_id body,
                                                        entity_id anchor_tile, float max_cost,
                                                        bool land_only)
{
    std::vector<std::pair<int, float>> out;
    const auto bit = w.bodies.find(body);
    const auto ait = w.tiles.find(anchor_tile);
    if (bit == w.bodies.end() || ait == w.tiles.end() || ait->second.body != body)
        return out;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0)
        return out;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (static_cast<int>(grid.size()) < gw * gh)
        return out;

    const auto tile_at = [&](int idx) -> const tile_component* {
        const entity_id tid = grid[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            return nullptr;
        const auto tit = w.tiles.find(tid);
        return (tit != w.tiles.end()) ? &tit->second : nullptr;
    };

    // The same Dijkstra as flood_field_for, over the same directed edge, cut
    // off at max_cost: a node is pushed only when its cost is within reach, so
    // the walk visits the reach and nothing beyond it. Not cached.
    const int total = gw * gh;
    std::vector<float> dist(static_cast<std::size_t>(total), 1e30f);
    std::vector<char>  settled(static_cast<std::size_t>(total), 0);
    const int ac = ait->second.grid_x, ar = ait->second.grid_y;
    dist[static_cast<std::size_t>(raster_idx(ac, ar, gw))] = 0.0f;
    std::priority_queue<pq_entry, std::vector<pq_entry>, std::greater<pq_entry>> pq;
    pq.push({ 0.0f, ac, ar });
    while (!pq.empty())
    {
        const pq_entry cur = pq.top();
        pq.pop();
        const int idx = raster_idx(cur.col, cur.row, gw);
        if (settled[static_cast<std::size_t>(idx)])
            continue;
        settled[static_cast<std::size_t>(idx)] = 1;
        out.emplace_back(idx, dist[static_cast<std::size_t>(idx)]);

        const tile_component* cur_tc = tile_at(idx);
        if (!cur_tc)
            continue;
        const float cur_cost = tile_traversal_cost(*cur_tc);
        for (int i = 0; i < 4; ++i)
        {
            const int nr = cur.row + k_flood_off_dr[i];
            if (nr < 0 || nr >= gh)
                continue;
            const int nc = ((cur.col + k_flood_off_dc[i]) % gw + gw) % gw;
            const int nidx = raster_idx(nc, nr, gw);
            if (settled[static_cast<std::size_t>(nidx)])
                continue;
            const tile_component* n_tc = tile_at(nidx);
            if (!n_tc)
                continue;
            if (land_only && is_water(n_tc->substrate))
                continue; // BL-1125: no water hop without a port at both ends
            const float nd = dist[static_cast<std::size_t>(idx)]
                           + flood_edge_cost(cur_cost, *n_tc, nr, i);
            if (nd <= max_cost && nd < dist[static_cast<std::size_t>(nidx)])
            {
                dist[static_cast<std::size_t>(nidx)] = nd;
                pq.push({ nd, nc, nr });
            }
        }
    }
    return out;
}

const logistics_path& intra_body_path(world& w, entity_id body, entity_id src_tile,
                                      entity_id dst_tile)
{
    logistics_path res;

    // THE ORDERED PAIR (BL-1126, path cost reads the cache). A path is directed
    // (LOGISTICS.md § 1): a river discounts an edge one way only, so src -> dst
    // and dst -> src are two answers, and the key names which one this is.
    const auto key = std::make_tuple(body, src_tile, dst_tile);
    const auto cit = w.astar_cost_cache.find(key);
    if (cit != w.astar_cost_cache.end())
        return cit->second;

    const auto bit = w.bodies.find(body);
    const auto sit = w.tiles.find(src_tile);
    const auto dit = w.tiles.find(dst_tile);
    if (bit == w.bodies.end() || sit == w.tiles.end() || dit == w.tiles.end()
        || sit->second.body != body || dit->second.body != body)
    {
        // unreachable / unknown endpoints
        return w.astar_cost_cache.emplace(key, std::move(res)).first->second;
    }

    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (gw <= 0 || gh <= 0 || grid.empty())
        return w.astar_cost_cache.emplace(key, std::move(res)).first->second;

    if (src_tile == dst_tile)
    {
        res.reachable     = true;
        res.cost          = 0.0f;
        res.crosses_ocean = is_water(sit->second.substrate); // BL-516
        res.tiles         = { src_tile };
        return w.astar_cost_cache.emplace(key, std::move(res)).first->second;
    }

    // ONE WAY ONLY: the DESTINATION's completed flood field (2026-08-25 — see
    // flood_field_for), built if absent, read at the origin. Never the origin's
    // field, even when it exists: that field prices the reverse route, and
    // answering from whichever field happened to be cached made the cost a
    // function of cache state, so a loaded game (caches empty) re-derived some
    // pairs the other way round and continued differently (lp_anchor_field_check
    // L1). The hot callers ask many origins about the same few destinations
    // (dispatch's net-price rule toward market centres, the march toward its
    // order's tile), so one field per destination is also the cheap shape.
    const logistics_flood_field& field = flood_field_for(w, body, dst_tile, gw, gh, grid, dit->second);
    const int src_idx = raster_idx(sit->second.grid_x, sit->second.grid_y, gw);

    if (field.dist[static_cast<std::size_t>(src_idx)] < 1e30f)
    {
        res.reachable     = true;
        res.cost          = field.dist[static_cast<std::size_t>(src_idx)];
        res.crosses_ocean = field.crossed[static_cast<std::size_t>(src_idx)] != 0;

        // Reconstruct the tile sequence (BL-152): walk src's next hops to the
        // anchor (the travel order), then store it lo -> hi. THE STORED ORDER
        // STAYS CANONICAL even though the key is ordered: every reader that
        // wants travel order flips it when src is not the lower id
        // (convoy_route_tiles, body_surface_canvas; LOGISTICS.md § 2's trap).
        std::vector<entity_id> seq;
        for (int i = src_idx; i != -1; i = field.came_from[static_cast<std::size_t>(i)])
        {
            const entity_id tid = grid[static_cast<std::size_t>(i)];
            if (tid != null_entity) seq.push_back(tid);
            if (i == field.anchor_idx) break;
        }
        if (src_tile > dst_tile) // seq runs src -> dst; lo -> hi needs the flip
            std::reverse(seq.begin(), seq.end());
        res.tiles = std::move(seq);
    }
    return w.astar_cost_cache.emplace(key, std::move(res)).first->second;
}

// ---------------------------------------------------------------------------
// BL-1186 (goods cross markets): a route's LEGS
// ---------------------------------------------------------------------------

namespace {

/// Fetch or build @p domain's flood field anchored at @p anchor_tile — the
/// leg-confined sibling of flood_field_for, over the SAME directed edge
/// (`flood_edge_cost`), so a leg's cost is priced exactly as intra_body_path
/// prices the same tiles. Kept on world.leg_flood_fields, apart from the
/// unconfined fields, so the counts the warm-start probes read stay theirs.
///
///   land: never enters a water cell.
///   sea:  the anchor (the destination PORT) expands into water only; a water
///         cell expands anywhere; a land cell other than the anchor is settled
///         (a sea leg may END there — the origin port) but never expanded. So
///         every reached land cell is reached across water from the anchor and
///         the leg is port -> water -> port, nothing else.
const logistics_flood_field& leg_flood_field_for(world& w, entity_id body, entity_id anchor_tile,
                                                 leg_domain domain, int gw, int gh,
                                                 const std::vector<entity_id>& grid,
                                                 const tile_component& anchor_tc)
{
    const auto key = std::make_tuple(body, anchor_tile, static_cast<std::uint8_t>(domain));
    const auto fit = w.leg_flood_fields.find(key);
    if (fit != w.leg_flood_fields.end())
        return fit->second;

    logistics_flood_field f;
    const int total = gw * gh;
    f.dist.assign(static_cast<std::size_t>(total), 1e30f);
    f.came_from.assign(static_cast<std::size_t>(total), -1);
    f.crossed.assign(static_cast<std::size_t>(total), 0);
    std::vector<char> settled(static_cast<std::size_t>(total), 0);

    const auto tile_at = [&](int idx) -> const tile_component* {
        const entity_id tid = grid[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            return nullptr;
        const auto tit = w.tiles.find(tid);
        return (tit != w.tiles.end()) ? &tit->second : nullptr;
    };
    // What counts as WATER for this domain. A land leg never enters water of any
    // kind; a sea leg sails the SEA only — ports gate on `is_coastal`, which reads
    // is_sea, so a lake is no sea lane (a lake cell is to a sea leg what land is:
    // somewhere it may end, never a cell it crosses).
    const auto wet = [domain](const tile_component& t) {
        return domain == leg_domain::sea ? is_sea(t.substrate) : is_water(t.substrate);
    };

    const int ac = anchor_tc.grid_x, ar = anchor_tc.grid_y;
    f.anchor_idx = raster_idx(ac, ar, gw);
    f.dist[static_cast<std::size_t>(f.anchor_idx)]    = 0.0f;
    f.crossed[static_cast<std::size_t>(f.anchor_idx)] = is_water(anchor_tc.substrate) ? 1 : 0;

    std::priority_queue<pq_entry, std::vector<pq_entry>, std::greater<pq_entry>> pq;
    pq.push({ 0.0f, ac, ar });

    while (!pq.empty())
    {
        const pq_entry cur = pq.top();
        pq.pop();
        const int idx = raster_idx(cur.col, cur.row, gw);
        if (settled[static_cast<std::size_t>(idx)])
            continue;
        settled[static_cast<std::size_t>(idx)] = 1;

        const tile_component* cur_tc = tile_at(idx);
        if (!cur_tc)
            continue;
        const bool cur_water = wet(*cur_tc);
        // A sea leg crosses land only at its two ends: a land cell that is not
        // the anchor is where a leg ENDS, never a cell it passes through.
        if (domain == leg_domain::sea && !cur_water && idx != f.anchor_idx)
            continue;
        const float cur_cost = tile_traversal_cost(*cur_tc);

        for (int i = 0; i < 4; ++i)
        {
            const int nr = cur.row + k_flood_off_dr[i];
            if (nr < 0 || nr >= gh)
                continue;
            const int nc = ((cur.col + k_flood_off_dc[i]) % gw + gw) % gw;
            const int nidx = raster_idx(nc, nr, gw);
            if (settled[static_cast<std::size_t>(nidx)])
                continue;
            const tile_component* n_tc = tile_at(nidx);
            if (!n_tc)
                continue;
            const bool n_water = wet(*n_tc);
            if (domain == leg_domain::land && n_water)
                continue; // a land leg never enters water
            if (domain == leg_domain::sea && !cur_water && !n_water)
                continue; // the anchor port leaves by water, never overland

            const float edge = flood_edge_cost(cur_cost, *n_tc, nr, i);
            const float nd   = f.dist[static_cast<std::size_t>(idx)] + edge;
            if (nd < f.dist[static_cast<std::size_t>(nidx)])
            {
                f.dist[static_cast<std::size_t>(nidx)] = nd;
                f.crossed[static_cast<std::size_t>(nidx)] =
                    (f.crossed[static_cast<std::size_t>(idx)] || n_water) ? 1 : 0;
                f.came_from[static_cast<std::size_t>(nidx)] = idx;
                pq.push({ nd, nc, nr });
            }
        }

        // A PORT TOUCHES THE WATER IT IS COASTAL TO. Placement's `is_coastal` reads
        // all six hex sides, the flood only four (the cardinal pair of each hex row
        // misses two diagonals), so a Port whose sea lies only on a diagonal would be
        // placeable yet never reachable by sea. The sea leg therefore also makes the
        // port <-> water hop across the two diagonals the cardinal walk skips —
        // land-to-water out of the anchor, water-to-land into the origin port — and
        // nothing else: water-to-water stays cardinal, so the crossing itself is
        // priced exactly as the unconfined flood prices it. Same node-mean edge and
        // river side rule as flood_edge_cost (the hop leaves the neighbour by the
        // side facing back, (side + 3) % 6).
        if (domain == leg_domain::sea)
        {
            const auto& off = hex_neighbors::offsets(cur.row);
            for (int side = 0; side < 6; ++side)
            {
                const int nr = cur.row + off[side][1];
                if (nr < 0 || nr >= gh)
                    continue;
                const int nc   = ((cur.col + off[side][0]) % gw + gw) % gw;
                const int nidx = raster_idx(nc, nr, gw);
                if (settled[static_cast<std::size_t>(nidx)])
                    continue;
                const tile_component* n_tc = tile_at(nidx);
                if (!n_tc)
                    continue;
                const bool n_water = wet(*n_tc);
                if (cur_water == n_water)
                    continue; // only the port <-> water hop
                const float edge = 0.5f * (cur_cost + tile_traversal_cost(*n_tc))
                                 * river_edge_discount(*n_tc, (side + 3) % 6);
                const float nd = f.dist[static_cast<std::size_t>(idx)] + edge;
                if (nd < f.dist[static_cast<std::size_t>(nidx)])
                {
                    f.dist[static_cast<std::size_t>(nidx)] = nd;
                    f.crossed[static_cast<std::size_t>(nidx)] = 1;
                    f.came_from[static_cast<std::size_t>(nidx)] = idx;
                    pq.push({ nd, nc, nr });
                }
            }
        }
    }

    return w.leg_flood_fields.emplace(key, std::move(f)).first->second;
}

} // namespace

const logistics_path& intra_body_leg_path(world& w, entity_id body, entity_id src_tile,
                                          entity_id dst_tile, leg_domain domain)
{
    logistics_path res;
    const auto key = std::make_tuple(body, src_tile, dst_tile, static_cast<std::uint8_t>(domain));
    const auto cit = w.leg_path_cache.find(key);
    if (cit != w.leg_path_cache.end())
        return cit->second;

    const auto bit = w.bodies.find(body);
    const auto sit = w.tiles.find(src_tile);
    const auto dit = w.tiles.find(dst_tile);
    if (bit == w.bodies.end() || sit == w.tiles.end() || dit == w.tiles.end()
        || sit->second.body != body || dit->second.body != body)
        return w.leg_path_cache.emplace(key, std::move(res)).first->second;

    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (gw <= 0 || gh <= 0 || grid.empty())
        return w.leg_path_cache.emplace(key, std::move(res)).first->second;

    if (src_tile == dst_tile)
    {
        // A zero-length leg: the cargo is already where the leg would take it
        // (an origin ON its port). A sea leg is never zero-length: one port is
        // no crossing.
        if (domain == leg_domain::land && !is_water(sit->second.substrate))
        {
            res.reachable = true;
            res.cost      = 0.0f;
            res.tiles     = { src_tile };
        }
        return w.leg_path_cache.emplace(key, std::move(res)).first->second;
    }
    if (domain == leg_domain::land
        && (is_water(sit->second.substrate) || is_water(dit->second.substrate)))
        return w.leg_path_cache.emplace(key, std::move(res)).first->second;

    const logistics_flood_field& field =
        leg_flood_field_for(w, body, dst_tile, domain, gw, gh, grid, dit->second);
    const int src_idx = raster_idx(sit->second.grid_x, sit->second.grid_y, gw);
    if (field.dist[static_cast<std::size_t>(src_idx)] < 1e30f)
    {
        res.reachable     = true;
        res.cost          = field.dist[static_cast<std::size_t>(src_idx)];
        res.crosses_ocean = field.crossed[static_cast<std::size_t>(src_idx)] != 0;
        std::vector<entity_id> seq;
        for (int i = src_idx; i != -1; i = field.came_from[static_cast<std::size_t>(i)])
        {
            const entity_id tid = grid[static_cast<std::size_t>(i)];
            if (tid != null_entity) seq.push_back(tid);
            if (i == field.anchor_idx) break;
        }
        if (src_tile > dst_tile) // seq runs src -> dst; stored lo -> hi like intra_body_path
            std::reverse(seq.begin(), seq.end());
        res.tiles = std::move(seq);
    }
    return w.leg_path_cache.emplace(key, std::move(res)).first->second;
}

const std::vector<entity_id>& body_active_port_tiles(world& w, entity_id body)
{
    const auto it = w.body_port_tiles.find(body);
    if (it != w.body_port_tiles.end())
        return it->second;
    std::vector<entity_id> ports;
    for (const auto& [bid, bc] : w.buildings)
    {
        (void)bid;
        if (bc.type != building_type::port || bc.ticks_remaining > 0 || bc.decommissioned)
            continue;
        const auto tit = w.tiles.find(bc.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            ports.push_back(bc.tile);
    }
    // Ascending and unique: a fixed walk order however w.buildings hashes, and
    // two ports on one tile are one port.
    std::sort(ports.begin(), ports.end());
    ports.erase(std::unique(ports.begin(), ports.end()), ports.end());
    return w.body_port_tiles.emplace(body, std::move(ports)).first->second;
}

float leg_travel_days(const world& w, entity_id body, float path_cost, convoy_mode mode)
{
    const float km_per_tile = body_km_per_tile(w, body);
    if (km_per_tile <= 0.0f || !(path_cost > 0.0f))
        return 0.0f;
    const float km_per_day = (mode == convoy_mode::sea) ? coastal_km_per_day : caravan_km_per_day;
    return path_cost * km_per_tile / km_per_day;
}

int travel_ticks_for_days(float days)
{
    const int ticks = static_cast<int>(days / static_cast<float>(econ_tick_days_world) + 0.999f);
    return ticks < 1 ? 1 : ticks;
}

// ---------------------------------------------------------------------------
// Logistics reach (BL-323 S2)
// ---------------------------------------------------------------------------

bool is_supply_anchor(const world& w, entity_id tile)
{
    if (tile == null_entity)
        return false;

    // A city anchors supply for free — the same free-hub discount BL-149 gives it.
    for (const auto& [centre, ctile] : w.population_centre_tile)
    {
        (void)centre;
        if (ctile == tile)
            return true;
    }

    // So does a port or an inland logistics hub: the two buildings whose whole
    // purpose is to be a node. A launchpad is deliberately NOT an anchor — it
    // dispatches off-world and supplies nothing on the surface.
    //
    // Built AND active only (Ben's ruling, 2026-08-08): a hub that is still a
    // construction site anchors nothing — the same completion contract the
    // convoy discount already applies (supply_system.cpp § collect_logistics_nodes).
    // Before this the two paths disagreed: an unbuilt shell extended placement
    // reach while conferring no discount.
    for (const auto& [bid, bc] : w.buildings)
    {
        (void)bid;
        if (bc.tile != tile)
            continue;
        if ((bc.type == building_type::port || bc.type == building_type::inland_logistics_hub)
            && bc.ticks_remaining <= 0 && !bc.decommissioned)
            return true;
    }
    return false;
}

bool body_has_supply_anchor(const world& w, entity_id body)
{
    // Cities count, wherever they are on the body.
    for (const auto& [centre, ctile] : w.population_centre_tile)
    {
        (void)centre;
        const auto tit = w.tiles.find(ctile);
        if (tit != w.tiles.end() && tit->second.body == body)
            return true;
    }
    // Anchor-TYPE buildings count by EXISTENCE, deliberately ignoring the
    // completion/decommission state is_supply_anchor requires. This is the
    // first-anchor bootstrap's guard (Ben's ruling, 2026-08-08): the exemption
    // ends the moment the first port/hub is COMMITTED, so a player cannot spam
    // free anchors across a virgin body while the first one is still building.
    for (const auto& [bid, bc] : w.buildings)
    {
        (void)bid;
        if (bc.type != building_type::port && bc.type != building_type::inland_logistics_hub)
            continue;
        const auto tit = w.tiles.find(bc.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            return true;
    }
    return false;
}

const std::vector<float>& body_reach_field(world& w, entity_id body)
{
    if (const auto it = w.body_reach_cost.find(body); it != w.body_reach_cost.end())
        return it->second;

    constexpr float inf = std::numeric_limits<float>::infinity();
    std::vector<float> cost;

    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return w.body_reach_cost.emplace(body, std::move(cost)).first->second;

    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (gw <= 0 || gh <= 0 || grid.empty())
        return w.body_reach_cost.emplace(body, std::move(cost)).first->second;

    const int total = gw * gh;
    cost.assign(static_cast<std::size_t>(total), inf);

    // Collect the anchor tile set ONCE, then seed by lookup. The seed loop used
    // to call is_supply_anchor per grid cell, and that predicate walks every
    // population centre and every building — O(tiles x (centres + buildings)),
    // which the 0 CE world turned pathological: 45,240 cells x ~1,100 ancient-era
    // centres was ~50M map probes per rebuild, and the rebuild fired every warm-
    // start tick (2026-08-12, the AppHangB1 stall). Same predicate, same
    // conditions, one linear pass — the seeded set is identical.
    const std::unordered_set<entity_id> anchor_tiles = collect_anchor_tile_set(w);

    // Seed every anchor at zero. RASTER ORDER, never tiles-map order — this runs
    // inside a deterministic simulation and the seed order must not depend on
    // hash-map iteration.
    std::priority_queue<pq_entry, std::vector<pq_entry>, std::greater<pq_entry>> open;
    for (int i = 0; i < total; ++i)
    {
        const entity_id tid = grid[static_cast<std::size_t>(i)];
        if (tid == null_entity || anchor_tiles.find(tid) == anchor_tiles.end())
            continue;
        cost[static_cast<std::size_t>(i)] = 0.0f;
        open.push(pq_entry{ 0.0f, i % gw, i / gw });
    }

    // Multi-source Dijkstra over the same 4-cardinal, column-wrapping grid the
    // A* uses, with the same edge cost (mean of the two node weights). Sharing
    // the cost function is the point: reach means "suppliable", not a second
    // distance metric invented for placement.
    while (!open.empty())
    {
        const pq_entry cur = open.top();
        open.pop();
        const int cur_idx = raster_idx(cur.col, cur.row, gw);
        if (cur.cost > cost[static_cast<std::size_t>(cur_idx)])
            continue;

        const int row = cur.row;
        const int col = cur.col;
        const auto cur_tc = w.tiles.find(grid[static_cast<std::size_t>(cur_idx)]);
        if (cur_tc == w.tiles.end())
            continue;
        const float cur_weight = tile_traversal_cost(cur_tc->second);

        const int dr[4] = { -1, 1, 0, 0 };
        const int dc[4] = { 0, 0, -1, 1 };
        for (int d = 0; d < 4; ++d)
        {
            const int nrow = row + dr[d];
            if (nrow < 0 || nrow >= gh) // rows do not wrap; columns do
                continue;
            const int nidx = raster_idx(col + dc[d], nrow, gw);
            const entity_id ntid = grid[static_cast<std::size_t>(nidx)];
            if (ntid == null_entity)
                continue;
            const auto n_tc = w.tiles.find(ntid);
            if (n_tc == w.tiles.end())
                continue;

            const float edge = 0.5f * (cur_weight + tile_traversal_cost(n_tc->second));
            const float next = cur.cost + edge;
            if (next < cost[static_cast<std::size_t>(nidx)])
            {
                cost[static_cast<std::size_t>(nidx)] = next;
                open.push(pq_entry{ next, nidx % gw, nidx / gw });
            }
        }
    }

    return w.body_reach_cost.emplace(body, std::move(cost)).first->second;
}

float tile_reach_cost(const world& w, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return -1.0f;

    const auto fit = w.body_reach_cost.find(tit->second.body);
    if (fit == w.body_reach_cost.end() || fit->second.empty())
        return -1.0f; // not computed — distinct from computed-and-unreachable

    const auto bit = w.bodies.find(tit->second.body);
    if (bit == w.bodies.end())
        return -1.0f;

    const int gw = bit->second.grid_width;
    if (gw <= 0)
        return -1.0f;
    const std::size_t idx = static_cast<std::size_t>(tit->second.grid_y) * static_cast<std::size_t>(gw)
                          + static_cast<std::size_t>(tit->second.grid_x);
    if (idx >= fit->second.size())
        return -1.0f;
    return fit->second[idx];
}

// ---------------------------------------------------------------------------
// BL-1230 (power crosses markets) — the power grid at province grain
// ---------------------------------------------------------------------------
// LOGISTICS.md § 3a, "The province is the grid's cell (Ben, 2026-10-07)". A
// union-find over the partition's provinces: every province holding a roaded
// land tile is wired, and two wired provinces join wherever roads join — two
// roaded tiles 4-cardinal adjacent (east-west wrapped), or two roaded tiles
// either side of a strait (at most kMaxCrossingTiles of non-ocean water, the
// crossing the road-only walk of road_generation.cpp admits). The grid id is
// the lowest province id in the component, read only after every union, so no
// visit order can reach the answer.
namespace {

struct province_dsu
{
    std::vector<std::size_t> parent;
    explicit province_dsu(std::size_t n) : parent(n)
    {
        for (std::size_t i = 0; i < n; ++i)
            parent[i] = i;
    }
    std::size_t find(std::size_t a)
    {
        while (parent[a] != a)
        {
            parent[a] = parent[parent[a]];
            a = parent[a];
        }
        return a;
    }
    void unite(std::size_t a, std::size_t b)
    {
        a = find(a);
        b = find(b);
        if (a == b)
            return;
        if (b < a)
            std::swap(a, b);
        parent[b] = a; // cosmetic: the grid id is re-derived as a minimum below
    }
};

} // namespace

const std::map<std::uint32_t, std::uint32_t>& province_power_grid(world& w)
{
    const std::vector<province>& provs = w.provinces.provinces;
    if (w.power_grid_built && w.power_grid_stamp == provs.size())
        return w.power_grid_of_province;

    w.power_grid_of_province.clear();
    w.power_grid_built = true;
    w.power_grid_stamp = provs.size();
    if (provs.empty())
        return w.power_grid_of_province;

    const std::size_t np = provs.size();
    auto index_of = [&](std::uint32_t pid) -> std::size_t {
        const auto it = std::lower_bound(provs.begin(), provs.end(), pid,
                                         [](const province& p, std::uint32_t id) { return p.id < id; });
        return (it != provs.end() && it->id == pid) ? static_cast<std::size_t>(it - provs.begin()) : np;
    };

    // Wired provinces, and the bodies they sit on (ascending: std::set).
    std::vector<char> wired(np, 0);
    std::set<entity_id> bodies;
    for (std::size_t i = 0; i < np; ++i)
    {
        for (const entity_id t : provs[i].tiles)
        {
            const auto tit = w.tiles.find(t);
            if (tit == w.tiles.end() || is_water(tit->second.substrate))
                continue;
            if (tit->second.road_level > 0)
            {
                wired[i] = 1;
                bodies.insert(provs[i].body);
                break;
            }
        }
    }

    province_dsu dsu(np);
    for (const entity_id body : bodies)
    {
        const auto bit = w.bodies.find(body);
        if (bit == w.bodies.end())
            continue;
        const int gw = bit->second.grid_width;
        const int gh = bit->second.grid_height;
        if (gw <= 0 || gh <= 0)
            continue;
        const std::vector<entity_id> raster = body_tile_grid(w, body); // a copy: owned here
        const std::size_t n = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
        if (raster.size() < n)
            continue;

        // Per cell: the province index of a ROADED land cell (np otherwise), and
        // whether the cell is crossable water (non-ocean water).
        std::vector<std::size_t> road_prov(n, np);
        std::vector<char>        strait(n, 0);
        for (std::size_t c = 0; c < n; ++c)
        {
            const auto tit = w.tiles.find(raster[c]);
            if (tit == w.tiles.end())
                continue;
            const tile_component& tc = tit->second;
            if (is_water(tc.substrate))
            {
                strait[c] = is_open_ocean(tc.substrate) ? 0 : 1;
                continue;
            }
            if (tc.road_level > 0)
                road_prov[c] = index_of(w.provinces.province_of(raster[c]));
        }

        auto cell_at = [&](int x, int y) -> int {
            if (y < 0 || y >= gh)
                return -1;
            const int wx = ((x % gw) + gw) % gw; // the east-west wrap
            return y * gw + wx;
        };

        std::vector<int> frontier, next;
        std::vector<int> seen_stamp(n, -1);
        for (int c = 0; c < static_cast<int>(n); ++c)
        {
            const std::size_t pc = road_prov[static_cast<std::size_t>(c)];
            if (pc >= np)
                continue;
            const int cx = c % gw, cy = c / gw;

            // Land: right and down suffice for an undirected adjacency.
            const int right = cell_at(cx + 1, cy);
            const int down  = cell_at(cx, cy + 1);
            if (right >= 0 && road_prov[static_cast<std::size_t>(right)] < np)
                dsu.unite(pc, road_prov[static_cast<std::size_t>(right)]);
            if (down >= 0 && road_prov[static_cast<std::size_t>(down)] < np)
                dsu.unite(pc, road_prov[static_cast<std::size_t>(down)]);

            // A strait: a breadth-first run of at most kMaxCrossingTiles water
            // cells from this road, joining any road on the far shore.
            frontier.clear();
            seen_stamp[static_cast<std::size_t>(c)] = c;
            frontier.push_back(c);
            for (int depth = 0; depth < kMaxCrossingTiles && !frontier.empty(); ++depth)
            {
                next.clear();
                for (const int f : frontier)
                {
                    const int fx = f % gw, fy = f / gw;
                    const int nb[4] = { cell_at(fx - 1, fy), cell_at(fx + 1, fy),
                                        cell_at(fx, fy - 1), cell_at(fx, fy + 1) };
                    for (const int v : nb)
                    {
                        if (v < 0 || seen_stamp[static_cast<std::size_t>(v)] == c)
                            continue;
                        seen_stamp[static_cast<std::size_t>(v)] = c;
                        if (strait[static_cast<std::size_t>(v)])
                            next.push_back(v);
                    }
                }
                // The far shore: roads adjacent to this depth's water cells.
                for (const int f : next)
                {
                    const int fx = f % gw, fy = f / gw;
                    const int nb[4] = { cell_at(fx - 1, fy), cell_at(fx + 1, fy),
                                        cell_at(fx, fy - 1), cell_at(fx, fy + 1) };
                    for (const int v : nb)
                        if (v >= 0 && road_prov[static_cast<std::size_t>(v)] < np)
                            dsu.unite(pc, road_prov[static_cast<std::size_t>(v)]);
                }
                frontier.swap(next);
            }
        }
    }

    // Grid id = the lowest wired province id in the component. Ascending walk:
    // the first member met of each root is its lowest id.
    std::vector<std::uint32_t> grid_of_root(np, 0);
    for (std::size_t i = 0; i < np; ++i)
    {
        if (!wired[i])
            continue;
        const std::size_t root = dsu.find(i);
        if (grid_of_root[root] == 0)
            grid_of_root[root] = provs[i].id;
        w.power_grid_of_province.emplace(provs[i].id, grid_of_root[root]);
    }
    return w.power_grid_of_province;
}

std::uint32_t tile_power_grid(world& w, entity_id tile)
{
    const auto& grid = province_power_grid(w);
    const std::uint32_t pid = w.provinces.province_of(tile);
    if (pid == 0)
        return 0;
    const auto it = grid.find(pid);
    return (it != grid.end()) ? it->second : 0;
}

// ---------------------------------------------------------------------------
// Active Logistic Points (BL-596 — LOGISTICS.md § Logistic Points)
// ---------------------------------------------------------------------------
// LP is a per-tick RATE, never a stock (Ben, ruling on NR-343, 2026-08-20):
// regenerated, spent or wasted each tick, never banked, and carrying it on
// `world` (a field, a cache, anything that outlives one call) would be the
// `military_points` write-only-accumulator defect renamed. So this function
// is PURE and UNCACHED — it recomputes the anchor set and hands back a fresh
// map every call, and the caller owns decrementing it for exactly one tick's
// worth of draws before discarding it.
//
// Constraint 2 (LOGISTICS.md): cities, and built-and-active ports/inland
// hubs, are the locus — the same anchor set `body_reach_field` seeds from
// (`collect_anchor_tile_set`, above), never a per-corp pool (the
// abstraction `military_points` was deleted for).
//
// EXTENSION POINT for BL-597 (passive convoy LP draw, a later item): that
// item draws against the SAME per-anchor pool this function computes, just
// at its own rate and from its own call site — call this again rather than
// re-deriving the anchor set a second time.

std::unordered_map<entity_id, float> active_lp_anchor_pools(world& w, entity_id body,
                                                             float lp_per_anchor_tick)
{
    std::unordered_map<entity_id, float> pools;
    if (lp_per_anchor_tick <= 0.0f)
        return pools;

    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (grid.empty())
        return pools;

    const std::unordered_set<entity_id> anchor_tiles = collect_anchor_tile_set(w);

    // Walk the RASTER grid, not the anchor_tiles set — the grid's order is a
    // pure function of the body (raster index), so this stays independent of
    // population_centre_tile's / w.buildings' hash-map iteration order even
    // though the RESULT is an unordered_map (its content, not its iteration
    // order, is what every caller depends on).
    for (const entity_id tid : grid)
    {
        if (tid == null_entity)
            continue;
        if (anchor_tiles.find(tid) == anchor_tiles.end())
            continue;
        pools.emplace(tid, lp_per_anchor_tick);
    }
    return pools;
}

// ---------------------------------------------------------------------------
// Passive Logistic Points scaffolding (BL-597 — LOGISTICS.md § Logistic Points)
// ---------------------------------------------------------------------------

std::unordered_map<entity_id, float>& lp_pool_for_body(lp_pool_map& pools_by_body, world& w,
                                                        entity_id body, float lp_per_anchor_tick)
{
    auto it = pools_by_body.find(body);
    if (it == pools_by_body.end())
        it = pools_by_body.emplace(body, active_lp_anchor_pools(w, body, lp_per_anchor_tick)).first;
    return it->second;
}

namespace {

/// BL-1117 (settle tick one): ONE multi-source Dijkstra from every anchor in
/// @p anchors (ascending), carrying which anchor each cell's cost came from.
///
/// WHAT IT REPLACES. `nearest_lp_anchor` used to call `intra_body_path(from,
/// anchor)` for every anchor in the pool. On a cold cache each pair double-
/// missed and flooded the WHOLE BODY from the anchor, so the first convoy commit
/// of a settle built one flood field per city: 9,038 on seed 0 (~90 s, ~2.5 GB of
/// fields), 6,079 on seed 28.
///
/// WHY IT IS THE SAME QUANTITY. The edges are `flood_edge_cost` — each hop priced
/// TOWARD the anchor, exactly as an anchor's own flood field prices it — so a
/// cell's cost is the cost of travelling cell -> anchor, which is what
/// `intra_body_path(from, anchor)` reads (BL-1126: the destination's field, read
/// at the origin). Float addition is monotone and a non-negative edge never
/// lowers a sum, so Dijkstra settles each cell at the minimum over paths of the
/// float sum built from its source; seeding every anchor at 0 is one super-
/// source with zero edges (exact in float), so `cost[i]` is EXACTLY min over
/// anchors a of a's own flood distance at cell i.
///
/// THE CONTRACT: an anchor at least cost, and a fixed choice among exact ties.
/// Each label is the pair (cost, anchor) compared in that order and the queue
/// pops in that order, so a settled label is final and the choice is a pure
/// function of the inputs. It is USUALLY the lowest-id anchor among the equal-
/// cost ones, but not always: round-half-to-even can fold a one-ulp gap between
/// two partial sums into an equal total (inside one binade, not only at a
/// boundary), and then the cell inherits the anchor the smaller partial sum
/// carried, which may be the higher id. The cost is exact either way.
/// lp_anchor_field_check's F2 row counts those cells against a brute-force flood
/// per anchor and prints the count on its verdict line.
lp_anchor_field build_lp_anchor_field(world& w, entity_id body, std::vector<entity_id> anchors)
{
    lp_anchor_field f;
    f.anchors = std::move(anchors);

    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return f;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (gw <= 0 || gh <= 0 || grid.empty())
        return f;

    const int total = gw * gh;
    f.cost.assign(static_cast<std::size_t>(total), 1e30f);
    f.nearest.assign(static_cast<std::size_t>(total), null_entity);
    std::vector<char> settled(static_cast<std::size_t>(total), 0);

    const auto tile_at = [&](int idx) -> const tile_component* {
        const entity_id tid = grid[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            return nullptr;
        const auto tit = w.tiles.find(tid);
        return (tit != w.tiles.end()) ? &tit->second : nullptr;
    };

    /// (cost, anchor) ascending, then the cell: a total order, so the pop order
    /// (and with it every label) is a function of the inputs alone.
    struct entry
    {
        float     cost;
        entity_id anchor;
        int       idx;
        bool operator>(const entry& o) const
        {
            if (cost != o.cost) return cost > o.cost;
            if (anchor != o.anchor) return anchor > o.anchor;
            return idx > o.idx;
        }
    };
    std::priority_queue<entry, std::vector<entry>, std::greater<entry>> pq;

    // Seeds. An anchor the per-pair loop could never have reached is skipped
    // the same way intra_body_path refuses it: unknown, or not on this body.
    for (const entity_id a : f.anchors)
    {
        const auto tit = w.tiles.find(a);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        const tile_component& tc = tit->second;
        if (tc.grid_y < 0 || tc.grid_y >= gh)
            continue;
        const int idx = raster_idx(tc.grid_x, tc.grid_y, gw);
        const std::size_t si = static_cast<std::size_t>(idx);
        if (f.nearest[si] == null_entity || a < f.nearest[si])
        {
            f.cost[si]    = 0.0f;
            f.nearest[si] = a;
            pq.push({ 0.0f, a, idx });
        }
    }

    while (!pq.empty())
    {
        const entry cur = pq.top();
        pq.pop();
        const std::size_t ci = static_cast<std::size_t>(cur.idx);
        if (settled[ci])
            continue;
        settled[ci] = 1;

        const tile_component* cur_tc = tile_at(cur.idx);
        if (!cur_tc)
            continue;
        const float cur_cost = tile_traversal_cost(*cur_tc);
        const int   col      = cur.idx % gw;
        const int   row      = cur.idx / gw;

        for (int i = 0; i < 4; ++i)
        {
            const int nr = row + k_flood_off_dr[i];
            if (nr < 0 || nr >= gh)
                continue;
            const int nc = ((col + k_flood_off_dc[i]) % gw + gw) % gw;
            const int nidx = raster_idx(nc, nr, gw);
            const std::size_t ni = static_cast<std::size_t>(nidx);
            if (settled[ni])
                continue;
            const tile_component* n_tc = tile_at(nidx);
            if (!n_tc)
                continue; // absent grid cell — impassable

            const float edge = flood_edge_cost(cur_cost, *n_tc, nr, i);
            const float nd   = f.cost[ci] + edge;
            if (nd < f.cost[ni] || (nd == f.cost[ni] && f.nearest[ci] < f.nearest[ni]))
            {
                f.cost[ni]    = nd;
                f.nearest[ni] = f.nearest[ci];
                pq.push({ nd, f.nearest[ci], nidx });
            }
        }
    }
    return f;
}

/// True when @p f was built over exactly @p pool's key set. EXACT, never a
/// fingerprint: a false match would hand back another anchor set's answer.
bool built_over(const lp_anchor_field& f, const std::unordered_map<entity_id, float>& pool)
{
    if (f.anchors.size() != pool.size())
        return false;
    for (const auto& kv : pool)
        if (!std::binary_search(f.anchors.begin(), f.anchors.end(), kv.first))
            return false;
    return true;
}

} // namespace

const lp_anchor_field& body_lp_anchor_field(world& w, entity_id body,
                                            const std::unordered_map<entity_id, float>& pool)
{
    const auto it = w.lp_anchor_fields.find(body);
    if (it != w.lp_anchor_fields.end() && built_over(it->second, pool))
        return it->second;

    // Built over the POOL's key set, not re-derived from the world: the pool is
    // the tick's snapshot of which anchors hold Logistic Points, and the caller
    // indexes it with the answer (`pools.at(nearest)`), so the answer must be one
    // of its keys even when an anchor changed state since the pool was made.
    std::vector<entity_id> anchors;
    anchors.reserve(pool.size());
    for (const auto& kv : pool)
        anchors.push_back(kv.first);
    std::sort(anchors.begin(), anchors.end());
    lp_anchor_field built = build_lp_anchor_field(w, body, std::move(anchors));
    return w.lp_anchor_fields.insert_or_assign(body, std::move(built)).first->second;
}

entity_id nearest_lp_anchor(world& w, entity_id body, entity_id from_tile,
                            const std::unordered_map<entity_id, float>& pool)
{
    if (pool.empty())
        return null_entity;
    // intra_body_path refuses an unknown or foreign endpoint as unreachable, so
    // every anchor did: nothing is nearest.
    const auto sit = w.tiles.find(from_tile);
    if (sit == w.tiles.end() || sit->second.body != body)
        return null_entity;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return null_entity;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    if (gw <= 0 || sit->second.grid_y < 0 || sit->second.grid_y >= gh)
        return null_entity;

    const lp_anchor_field& f = body_lp_anchor_field(w, body, pool);
    const std::size_t idx =
        static_cast<std::size_t>(raster_idx(sit->second.grid_x, sit->second.grid_y, gw));
    if (idx >= f.nearest.size() || !(f.cost[idx] < 1e30f))
        return null_entity; // no anchor of the pool reaches this tile
    return f.nearest[idx];
}

// ---------------------------------------------------------------------------
// Physical scale and travel time (Ben, 2026-08-12)
// ---------------------------------------------------------------------------

float body_km_per_tile(const world& w, entity_id body)
{
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return 0.0f;

    const int gw = bit->second.grid_width;
    if (gw <= 0)
        return 0.0f;

    // Rocky-planet mass-radius relation, R proportional to M^0.27. A generation
    // -time constant, computed per call rather than cached because the callers
    // are per-dispatch, not per-frame.
    const float mass = (bit->second.mass_earths > 0.0f) ? bit->second.mass_earths : 1.0f;
    const float radius_km = earth_radius_km * std::pow(mass, 0.27f);

    // Columns wrap, so the grid width spans the full circumference.
    constexpr float two_pi = 6.283185307f;
    return (two_pi * radius_km) / static_cast<float>(gw);
}

int convoy_travel_ticks(const world& w, entity_id body, const logistics_path& path)
{
    if (!path.reachable)
        return 1;

    const float km_per_tile = body_km_per_tile(w, body);
    if (km_per_tile <= 0.0f)
        return 1; // No scale for this body — fall back to the old one-tick haul.

    // `path.cost` is already terrain-weighted, so it is a count of EFFECTIVE
    // tiles rather than raw ones: a mountain crossing costs two plains'
    // traversal and therefore two plains' worth of days.
    const float effective_tiles = (path.cost > 0.0f)
                                      ? path.cost
                                      : static_cast<float>(path.tiles.size());
    const float km = effective_tiles * km_per_tile;

    const float km_per_day = path.crosses_ocean ? coastal_km_per_day : caravan_km_per_day;
    const float days       = km / km_per_day;

    // Quantise up to whole econ ticks: the economy clears quarterly, so a haul
    // lands on a clearing boundary or it does not land at all.
    const int ticks = static_cast<int>(days / static_cast<float>(econ_tick_days_world) + 0.999f);
    return ticks < 1 ? 1 : ticks;
}

// ---------------------------------------------------------------------------
// Convoy position (BL-458)
// ---------------------------------------------------------------------------

namespace {

/// One leg of a convoy's lane while it is being laid: where its tiles start in the
/// lane (the join tile it shares with the leg before) and the days it takes.
struct lane_leg
{
    std::size_t first = 0;
    float       days  = 0.0f;
};

/// Append one leg's tiles to @p out, oriented `from` -> `to`. The leg cache stores
/// its sequence lo->hi like intra_body_path (the ORIENTATION RULE below), so the
/// flip is owed here too. A tile shared with the previous leg's last (the port the
/// two legs meet at) is written once. Records the leg's start and its travel days
/// (leg_travel_days on the leg's own path cost, at @p mode's speed — the figure the
/// haul was priced and timed on). False when the leg has no path.
bool append_leg(world& w, entity_id body, entity_id from, entity_id to, leg_domain domain,
                convoy_mode mode, std::vector<entity_id>& out, std::vector<lane_leg>& legs)
{
    const logistics_path& lp = intra_body_leg_path(w, body, from, to, domain);
    if (!lp.reachable || lp.tiles.empty())
        return false;
    std::vector<entity_id> seq = lp.tiles; // copied: the cache entry stays canonical lo->hi
    if (from != std::min(from, to))
        std::reverse(seq.begin(), seq.end());
    std::size_t k = 0;
    if (!out.empty() && out.back() == seq.front())
        k = 1;
    lane_leg leg;
    leg.first = (k == 1) ? out.size() - 1 : out.size();
    leg.days  = leg_travel_days(w, body, lp.cost, mode);
    out.insert(out.end(), seq.begin() + static_cast<std::ptrdiff_t>(k), seq.end());
    legs.push_back(leg);
    return true;
}

/// THE CLOCK ALONG THE LANE (BL-1195). `at[i]` is the fraction of the whole journey's
/// time spent when the cargo reaches tile i: 0 at the origin, 1 at the destination.
/// Each leg takes its own days (caravan overland, coastal by sea — roughly five times
/// faster), and within a leg the days split over its hops in proportion to each hop's
/// node-mean traversal weight, the same weight the leg's path cost sums. So a convoy's
/// progress (the fraction of its travel ticks elapsed) maps to the tile the cargo is
/// actually on. With no physical scale on the body (no days at all) the tiles are
/// spaced evenly, which is what every reader did before.
std::vector<float> lane_clock(const world& w, const std::vector<entity_id>& tiles,
                              const std::vector<lane_leg>& legs)
{
    const std::size_t n = tiles.size();
    std::vector<float> at(n, 0.0f);
    if (n < 2)
        return at;

    const auto weight = [&](entity_id t) {
        const auto it = w.tiles.find(t);
        return it != w.tiles.end() ? tile_traversal_cost(it->second) : 1.0f;
    };

    std::vector<float> hop(n - 1, 0.0f); // days spent on hop i -> i+1
    float total = 0.0f;
    for (std::size_t l = 0; l < legs.size(); ++l)
    {
        const std::size_t a = legs[l].first;
        const std::size_t b = (l + 1 < legs.size()) ? legs[l + 1].first : n - 1;
        if (b <= a || !(legs[l].days > 0.0f) || !std::isfinite(legs[l].days))
            continue;
        float sum = 0.0f;
        for (std::size_t i = a; i < b; ++i)
            sum += 0.5f * (weight(tiles[i]) + weight(tiles[i + 1]));
        for (std::size_t i = a; i < b; ++i)
        {
            const float share = (sum > 0.0f)
                                    ? 0.5f * (weight(tiles[i]) + weight(tiles[i + 1])) / sum
                                    : 1.0f / static_cast<float>(b - a);
            hop[i] = legs[l].days * share;
        }
        total += legs[l].days;
    }

    if (!(total > 0.0f) || !std::isfinite(total))
    {
        for (std::size_t i = 0; i < n; ++i)
            at[i] = static_cast<float>(i) / static_cast<float>(n - 1);
        return at;
    }
    float run = 0.0f;
    for (std::size_t i = 1; i < n; ++i)
    {
        run += hop[i - 1];
        at[i] = std::min(run / total, 1.0f);
    }
    at[n - 1] = 1.0f;
    return at;
}

} // namespace

convoy_route convoy_route_tiles(world& w, const convoy_component& cv)
{
    convoy_route route;

    const auto dm = w.markets.find(cv.dest_market);
    if (dm == w.markets.end())
        return route; // unresolved destination — no lane to stand on

    // The source's body. A market source carries its own; a convoy out of a
    // MARKET-LESS body's pool (BL-1003: `source_market` is then the body, not a
    // market) takes it from the origin tile recorded at dispatch (BL-1195), so it
    // has a lane like any other.
    const auto sm = w.markets.find(cv.source_market);
    entity_id src_body = null_entity;
    if (sm != w.markets.end())
        src_body = sm->second.body;
    else if (cv.origin_tile != null_entity)
    {
        const auto ot = w.tiles.find(cv.origin_tile);
        if (ot != w.tiles.end())
            src_body = ot->second.body;
    }
    if (src_body == null_entity || src_body != dm->second.body)
        return route; // inter-body leg: in transit between bodies, on no tile

    const entity_id body = src_body;
    // BL-1195: the lane starts where the haul was priced from — the dispatch's
    // origin tile — and falls back to the source centre for a convoy that never
    // passed the dispatch seam (one built by hand, which records no route).
    const entity_id st = (cv.origin_tile != null_entity)
                             ? cv.origin_tile
                             : (sm != w.markets.end() ? sm->second.centre_tile : null_entity);
    const entity_id dt = dm->second.centre_tile;
    if (st == null_entity || dt == null_entity)
        return route; // an unanchored market has no centre to route from/to

    std::vector<entity_id> tiles;
    std::vector<lane_leg>  legs;

    // BL-1195 (SUPPLY.md § Logistical cost): THE LANE IS THE LEGS. A route that
    // crossed water was priced land -> port -> sea -> port -> land (BL-1186), so its
    // lane is those three legs end to end, through the two Ports recorded at
    // dispatch. Reading the direct centre-to-centre path instead put the head, the
    // vision beam, interdiction and capture on ground the cargo never crosses. The
    // Ports are fixed at dispatch; each leg's path is read from the network as it
    // stands now.
    if (cv.port_a != null_entity && cv.port_b != null_entity)
    {
        if (!append_leg(w, body, st, cv.port_a, leg_domain::land, convoy_mode::land, tiles, legs)
            || !append_leg(w, body, cv.port_a, cv.port_b, leg_domain::sea, convoy_mode::sea,
                           tiles, legs)
            || !append_leg(w, body, cv.port_b, dt, leg_domain::land, convoy_mode::land, tiles,
                           legs))
            return route; // a leg the dispatch walked no longer exists: no lane
    }
    else
    {
        // One overland leg, chosen exactly as route_intra_body chose it: the
        // unconfined cheapest path while it stays on land, else the cheapest
        // LAND-ONLY path (a pair whose cheapest path crosses water but which has a
        // road round).
        const logistics_path& lp = intra_body_path(w, body, st, dt);
        if (!lp.reachable || lp.tiles.empty())
            return route;
        bool laid = false;
        if (lp.crosses_ocean)
            laid = append_leg(w, body, st, dt, leg_domain::land, convoy_mode::land, tiles, legs);
        // No overland road: only a convoy built outside the dispatch seam reaches
        // here (the router refuses such a pair a land route). It keeps the direct
        // path, the one lane it can be given.
        if (!laid)
        {
            tiles = lp.tiles; // copied: the cache entry stays canonical lo->hi
            // THE ORIENTATION RULE (BL-458). intra_body_path stores its sequence
            // lo->hi under its ordered (src, dst) key (BL-1126), so the cached order
            // is source->destination only when the source tile is the lower id. Flip
            // it when it is not. Skipping this puts a convoy's head at the far end of
            // its own lane about half the time, and the vision beam renders
            // identically either way, so nothing on screen would report it.
            if (st != std::min(st, dt))
                std::reverse(tiles.begin(), tiles.end());
            legs.clear();
            legs.push_back({0, leg_travel_days(w, body, lp.cost, cv.mode == convoy_mode::sea
                                                                     ? convoy_mode::sea
                                                                     : convoy_mode::land)});
        }
    }

    route.body  = body;
    route.at    = lane_clock(w, tiles, legs);
    route.tiles = std::move(tiles);
    return route;
}

int convoy_head_index(std::size_t tile_count, float progress)
{
    if (tile_count == 0)
        return -1;
    const int n = static_cast<int>(tile_count);
    const float p = std::isfinite(progress) ? std::clamp(progress, 0.0f, 1.0f) : 0.0f;
    return std::clamp(static_cast<int>(std::lround(p * static_cast<float>(n - 1))), 0, n - 1);
}

int convoy_lane_index(const std::vector<float>& at, float progress)
{
    if (at.empty())
        return -1;
    const float p = std::isfinite(progress) ? std::clamp(progress, 0.0f, 1.0f) : 0.0f;
    // The tile whose clock reading is nearest p; a tie goes to the later tile (the
    // half-up rounding convoy_head_index applies on an even clock).
    const auto it = std::lower_bound(at.begin(), at.end(), p);
    if (it == at.end())
        return static_cast<int>(at.size()) - 1;
    const int i = static_cast<int>(it - at.begin());
    if (i == 0)
        return 0;
    const float up   = at[static_cast<std::size_t>(i)] - p;
    const float down = p - at[static_cast<std::size_t>(i - 1)];
    return (up <= down) ? i : i - 1;
}

entity_id convoy_tile_at(world& w, const convoy_component& cv)
{
    const convoy_route route = convoy_route_tiles(w, cv);
    const int head = convoy_lane_index(route.at, cv.progress);
    if (head < 0)
        return null_entity;
    return route.tiles[static_cast<std::size_t>(head)];
}
