#include "logistics.hpp"
#include "river_generation.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <tuple>
#include <vector>

namespace {

/// Sea-leg traversal cost for an ocean tile — costlier than any land landform, so A*
/// prefers a land route and only crosses water when it must. A calibration constant.
constexpr float sea_leg_cost = 2.5f;

/// Raster index for (col, row) with the column wrapped into [0, gw). Mirrors
/// nation_generation.cpp's raster_idx so the two share one grid convention.
inline int raster_idx(int col, int row, int gw)
{
    return ((col % gw) + gw) % gw + row * gw;
}

/// A tile's traversal cost: ocean = sea leg, land = landform cost, both scaled by the
/// road discount. The per-node weight; an edge cost is the average of its two nodes.
float tile_traversal_cost(const tile_component& tc)
{
    const float base = (tc.composition == terrain_composition::ocean)
                           ? sea_leg_cost
                           : landform_logistics_cost(tc.landform);
    return base * road_traversal_multiplier(tc.road_level);
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
        res.crosses_ocean = (sit->second.composition == terrain_composition::ocean);
        res.tiles         = { src_tile };
        return w.astar_cost_cache.emplace(key, std::move(res)).first->second;
    }

    const int total = gw * gh;
    const int sc = sit->second.grid_x, sr = sit->second.grid_y;
    const int dc = dit->second.grid_x, dr = dit->second.grid_y;

    std::vector<float> dist(static_cast<std::size_t>(total), 1e30f);
    std::vector<char>  settled(static_cast<std::size_t>(total), 0);
    std::vector<char>  crossed(static_cast<std::size_t>(total), 0); // best path touches ocean?
    std::vector<int>   came_from(static_cast<std::size_t>(total), -1); // parent idx for path reconstruction (BL-152)

    const auto tile_at = [&](int idx) -> const tile_component* {
        const entity_id tid = grid[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            return nullptr;
        const auto tit = w.tiles.find(tid);
        return (tit != w.tiles.end()) ? &tit->second : nullptr;
    };

    const int start   = raster_idx(sc, sr, gw);
    const int destIdx = raster_idx(dc, dr, gw);
    dist[static_cast<std::size_t>(start)] = 0.0f;
    crossed[static_cast<std::size_t>(start)] =
        (sit->second.composition == terrain_composition::ocean) ? 1 : 0;

    std::priority_queue<pq_entry, std::vector<pq_entry>, std::greater<pq_entry>> pq;
    pq.push({ 0.0f, sc, sr });

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
        if (idx == destIdx)
            break;

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

            // River discount (BL-170): a river-adjacent edge is cheaper, stacking
            // MULTIPLICATIVELY with the road-tier discount already folded into
            // tile_traversal_cost above. The A* neighbour walk is 4-cardinal on the
            // raster grid; hex_side_for_offset maps that cardinal move to the hex
            // side (0-5) river tracing recorded on `cur`'s tile.
            const int hex_side = hex_side_for_offset(off_dc[i], off_dr[i], (cur.row & 1) != 0);
            const float river_mult = (hex_side >= 0) ? river_edge_discount(*cur_tc, hex_side) : 1.0f;

            const float edge = 0.5f * (cur_cost + tile_traversal_cost(*n_tc)) * river_mult;
            const float nd   = dist[static_cast<std::size_t>(idx)] + edge;
            if (nd < dist[static_cast<std::size_t>(nidx)])
            {
                dist[static_cast<std::size_t>(nidx)] = nd;
                crossed[static_cast<std::size_t>(nidx)] =
                    (crossed[static_cast<std::size_t>(idx)]
                     || n_tc->composition == terrain_composition::ocean) ? 1 : 0;
                came_from[static_cast<std::size_t>(nidx)] = idx;
                pq.push({ nd, nc, nr });
            }
        }
    }

    if (dist[static_cast<std::size_t>(destIdx)] < 1e30f)
    {
        res.reachable     = true;
        res.cost          = dist[static_cast<std::size_t>(destIdx)];
        res.crosses_ocean = crossed[static_cast<std::size_t>(destIdx)] != 0;

        // Reconstruct the tile sequence (BL-152). Walk parents dest→start, mapping
        // each raster index to its tile id, then reverse to src→dst. Canonicalise to
        // lo→hi (the cache is keyed on the unordered pair) so every reader sees one
        // stable order regardless of which direction first populated the entry.
        std::vector<entity_id> seq;
        for (int i = destIdx; i != -1; i = came_from[static_cast<std::size_t>(i)])
        {
            const entity_id tid = grid[static_cast<std::size_t>(i)];
            if (tid != null_entity) seq.push_back(tid);
            if (i == start) break;
        }
        std::reverse(seq.begin(), seq.end()); // now src→dst
        if (src_tile != lo)                   // canonicalise to lo→hi
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

    // Seed every anchor at zero. RASTER ORDER, never tiles-map order — this runs
    // inside a deterministic simulation and the seed order must not depend on
    // hash-map iteration.
    std::priority_queue<pq_entry, std::vector<pq_entry>, std::greater<pq_entry>> open;
    for (int i = 0; i < total; ++i)
    {
        const entity_id tid = grid[static_cast<std::size_t>(i)];
        if (tid == null_entity || !is_supply_anchor(w, tid))
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
