#include "logistics.hpp"

#include <algorithm>
#include <functional>
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

const char* road_tier_name(std::uint8_t road_level)
{
    switch (road_level)
    {
        case 1:  return "Track";
        case 2:  return "Road";
        case 3:  return "Highway";
        default: return "";
    }
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

logistics_path intra_body_path(world& w, entity_id body, entity_id src_tile, entity_id dst_tile)
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
        w.astar_cost_cache.emplace(key, res); // unreachable / unknown endpoints
        return res;
    }

    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (gw <= 0 || gh <= 0 || grid.empty())
    {
        w.astar_cost_cache.emplace(key, res);
        return res;
    }

    if (src_tile == dst_tile)
    {
        res.reachable     = true;
        res.cost          = 0.0f;
        res.crosses_ocean = (sit->second.composition == terrain_composition::ocean);
        res.tiles         = { src_tile };
        w.astar_cost_cache.emplace(key, res);
        return res;
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

            const float edge = 0.5f * (cur_cost + tile_traversal_cost(*n_tc));
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
    w.astar_cost_cache.emplace(key, res);
    return res;
}
