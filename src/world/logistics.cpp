#include "logistics.hpp"
#include "river_generation.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
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
    return base * road_traversal_multiplier(tc.road_level);
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

/// Fetch or build the COMPLETED flood field anchored at @p anchor_tile — the
/// full Dijkstra the pair search used to run per endpoint pair, run once and
/// kept on `world.logistics_flood_fields` (2026-08-25 warm-start stall).
/// dispatch_convoys prices hundreds of origins against the same few
/// destination centres, and rival place_road (BL-599) clears the pair cache at
/// tick rate, so the per-pair searches re-flooded the grid hundreds of times
/// every econ tick — the 2026-08-12 AppHangB1 disease with a new carrier.
///
/// Relaxation order, edge costs and tie-breaks are IDENTICAL to the retired
/// per-pair search (it was already Dijkstra — the "A*" name carried no
/// heuristic); the only difference is no early exit, and a settled node's
/// parent is final, so a pair reconstructed from this field matches what the
/// early-exit search anchored at the same tile returned.
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

    // 4-cardinal offsets, matching nation_generation::cardinal_neighbours (N, S, W, E).
    const int off_dc[4] = {  0,  0, -1, 1 };
    const int off_dr[4] = { -1,  1,  0, 0 };

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
            const int nr = cur.row + off_dr[i];
            if (nr < 0 || nr >= gh)
                continue;
            const int nc = ((cur.col + off_dc[i]) % gw + gw) % gw;
            const int nidx = raster_idx(nc, nr, gw);
            if (settled[static_cast<std::size_t>(nidx)])
                continue;
            const tile_component* n_tc = tile_at(nidx);
            if (!n_tc)
                continue; // absent grid cell — impassable

            // River discount (BL-170): see the pair search this loop was lifted
            // from — a river-adjacent edge is cheaper, stacking multiplicatively
            // with the road-tier discount inside tile_traversal_cost.
            const int hex_side = hex_side_for_offset(off_dc[i], off_dr[i], (cur.row & 1) != 0);
            const float river_mult = (hex_side >= 0) ? river_edge_discount(*cur_tc, hex_side) : 1.0f;

            const float edge = 0.5f * (cur_cost + tile_traversal_cost(*n_tc)) * river_mult;
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

const logistics_path& intra_body_path(world& w, entity_id body, entity_id src_tile,
                                      entity_id dst_tile)
{
    logistics_path res;

    // Canonicalise the endpoint pair (the weighted path is symmetric) for the cache key.
    const entity_id lo = std::min(src_tile, dst_tile);
    const entity_id hi = std::max(src_tile, dst_tile);
    const auto key = std::make_tuple(body, lo, hi);
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

    // Answer from a completed flood field rather than a per-pair search
    // (2026-08-25 — see flood_field_for above). Prefer a field either endpoint
    // already anchors; on a double miss, flood from the DESTINATION, because
    // the hot callers (dispatch's shortfall scan, nearest_lp_anchor, the march
    // pass) ask many origins about the same few destination tiles.
    const logistics_flood_field* field = nullptr;
    {
        const auto dfit = w.logistics_flood_fields.find(std::make_pair(body, dst_tile));
        if (dfit != w.logistics_flood_fields.end())
            field = &dfit->second;
        else
        {
            const auto sfit = w.logistics_flood_fields.find(std::make_pair(body, src_tile));
            if (sfit != w.logistics_flood_fields.end())
                field = &sfit->second;
        }
    }
    if (!field)
        field = &flood_field_for(w, body, dst_tile, gw, gh, grid, dit->second);

    // The endpoint the field is NOT anchored at — the walk start. The weighted
    // path is symmetric (the cache key is the unordered pair for exactly that
    // reason), so either endpoint's field prices the pair.
    const bool anchored_at_dst =
        field->anchor_idx == raster_idx(dit->second.grid_x, dit->second.grid_y, gw);
    const tile_component& other_tc  = anchored_at_dst ? sit->second : dit->second;
    const entity_id       other_tile = anchored_at_dst ? src_tile : dst_tile;
    const int otherIdx = raster_idx(other_tc.grid_x, other_tc.grid_y, gw);

    if (field->dist[static_cast<std::size_t>(otherIdx)] < 1e30f)
    {
        res.reachable     = true;
        res.cost          = field->dist[static_cast<std::size_t>(otherIdx)];
        res.crosses_ocean = field->crossed[static_cast<std::size_t>(otherIdx)] != 0;

        // Reconstruct the tile sequence (BL-152): walk parents other→anchor,
        // then canonicalise to lo→hi (the cache is keyed on the unordered pair)
        // so every reader sees one stable order regardless of which endpoint
        // anchored the field.
        std::vector<entity_id> seq;
        for (int i = otherIdx; i != -1; i = field->came_from[static_cast<std::size_t>(i)])
        {
            const entity_id tid = grid[static_cast<std::size_t>(i)];
            if (tid != null_entity) seq.push_back(tid);
            if (i == field->anchor_idx) break;
        }
        if (other_tile != lo) // seq runs other→anchor; flip unless other IS lo
            std::reverse(seq.begin(), seq.end());
        res.tiles = std::move(seq);
    }
    return w.astar_cost_cache.emplace(key, std::move(res)).first->second;
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

entity_id nearest_lp_anchor(world& w, entity_id body, entity_id from_tile,
                            const std::unordered_map<entity_id, float>& pool)
{
    entity_id nearest_anchor = null_entity;
    float best_cost = std::numeric_limits<float>::infinity();
    for (const auto& kv : pool)
    {
        const entity_id anchor_tile = kv.first;
        const logistics_path& p = intra_body_path(w, body, from_tile, anchor_tile);
        if (!p.reachable)
            continue;
        if (nearest_anchor == null_entity || p.cost < best_cost
            || (p.cost == best_cost && anchor_tile < nearest_anchor))
        {
            best_cost      = p.cost;
            nearest_anchor = anchor_tile;
        }
    }
    return nearest_anchor;
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

convoy_route convoy_route_tiles(world& w, const convoy_component& cv)
{
    convoy_route route;

    const auto sm = w.markets.find(cv.source_market);
    const auto dm = w.markets.find(cv.dest_market);
    if (sm == w.markets.end() || dm == w.markets.end())
        return route; // unresolved endpoint — no lane to stand on
    if (sm->second.body == null_entity || sm->second.body != dm->second.body)
        return route; // inter-body leg: in transit between bodies, on no tile

    const entity_id body = sm->second.body;
    const entity_id st   = sm->second.centre_tile;
    const entity_id dt   = dm->second.centre_tile;
    if (st == null_entity || dt == null_entity)
        return route; // an unanchored market has no centre to route from/to

    const logistics_path& lp = intra_body_path(w, body, st, dt);
    if (!lp.reachable || lp.tiles.empty())
        return route;

    route.body  = body;
    route.tiles = lp.tiles; // copied: the cache entry stays canonical lo->hi

    // THE ORIENTATION RULE (BL-458). intra_body_path canonicalises its stored
    // sequence to lo->hi to match its canonicalised (lo, hi) cache key, so the
    // cached order is source->destination only when the source tile is the
    // lower id. Flip it when it is not. Skipping this puts a convoy's head at
    // the far end of its own lane about half the time, and the vision beam
    // renders identically either way, so nothing on screen would report it.
    if (st != std::min(st, dt))
        std::reverse(route.tiles.begin(), route.tiles.end());

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

entity_id convoy_tile_at(world& w, const convoy_component& cv)
{
    const convoy_route route = convoy_route_tiles(w, cv);
    const int head = convoy_head_index(route.tiles.size(), cv.progress);
    if (head < 0)
        return null_entity;
    return route.tiles[static_cast<std::size_t>(head)];
}
