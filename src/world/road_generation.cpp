#include "road_generation.hpp"

#include "components.hpp"
#include "logistics.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace {

// Road tiers (BL-172 three-tier ladder; BL-146 shipped local/trunk). road_traversal_multiplier
// = 1/(1+0.5*tier): Track(1) x0.67, Road(2) x0.50, Highway(3) x0.40. Generation assigns a tier
// per edge from the two centres' scales; the player may then upgrade any tile (BL-172 place_road).
constexpr std::uint8_t kTrack   = 1;
constexpr std::uint8_t kRoad    = 2;
constexpr std::uint8_t kHighway = 3;
// Scale thresholds: a Highway joins two major centres (City=3 .. Metropolis=5); a Road needs at
// least one Town+ (scale 2); everything smaller (and every cross-nation border link) is a Track.
constexpr int          kMajorScale = 3;
constexpr int          kMidScale   = 2;

// Tier for an edge between two centres of the given scales (BL-172).
std::uint8_t edge_tier(int scale_a, int scale_b)
{
    if (scale_a >= kMajorScale && scale_b >= kMajorScale)
        return kHighway;
    if (scale_a >= kMidScale || scale_b >= kMidScale)
        return kRoad;
    return kTrack;
}

constexpr float kUnreachable = std::numeric_limits<float>::max();

/// A road node: one population centre on the body, tagged with its nation and scale.
struct road_node
{
    entity_id centre;
    entity_id tile;
    entity_id nation;
    int       scale;
};

entity_id nation_of(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return (it != w.tile_to_nation.end()) ? it->second : null_entity;
}

/// Stamp a road of @p level along the A* path between two tiles, taking the max on
/// overlap and skipping ocean (roads are a land feature). No-op if unreachable.
void stamp_edge(world& w, entity_id body, entity_id ta, entity_id tb, std::uint8_t level)
{
    const logistics_path p = intra_body_path(w, body, ta, tb);
    if (!p.reachable)
        return;
    for (const entity_id t : p.tiles)
    {
        const auto it = w.tiles.find(t);
        if (it == w.tiles.end() || it->second.composition == terrain_composition::ocean)
            continue;
        it->second.road_level = std::max(it->second.road_level, level);
    }
}

} // namespace

void generate_roads(world& w, entity_id body)
{
    // 1. Collect this body's centres, ordered by tile id so the whole pass is
    //    independent of the unordered_map iteration order (determinism).
    std::vector<road_node> nodes;
    for (const auto& [centre, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        int scale = 1;
        if (const auto pit = w.population_centres.find(centre); pit != w.population_centres.end())
            scale = pit->second.scale;
        nodes.push_back({ centre, tile, nation_of(w, tile), scale });
    }
    std::sort(nodes.begin(), nodes.end(),
              [](const road_node& a, const road_node& b) { return a.tile < b.tile; });
    if (nodes.size() < 2)
        return;

    // Group node indices by nation (std::map → nation ids ascending, deterministic).
    std::map<entity_id, std::vector<int>> by_nation;
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
        by_nation[nodes[i].nation].push_back(i);

    // 2. Per-nation backbone: MST + relative-neighbour redundancy over the centres.
    for (const auto& [nation, members] : by_nation)
    {
        (void)nation;
        const int n = static_cast<int>(members.size());
        if (n < 2)
            continue;

        // Pairwise terrain-weighted A* costs (symmetric matrix); collect the
        // reachable pairs as candidate edges (a,b index into `members`).
        struct edge { float cost; int a; int b; };
        std::vector<edge> edges;
        std::vector<std::vector<float>> d(n, std::vector<float>(n, kUnreachable));
        for (int a = 0; a < n; ++a)
            for (int b = a + 1; b < n; ++b)
            {
                const logistics_path p =
                    intra_body_path(w, body, nodes[members[a]].tile, nodes[members[b]].tile);
                const float c = p.reachable ? p.cost : kUnreachable;
                d[a][b] = d[b][a] = c;
                if (p.reachable)
                    edges.push_back({ c, a, b });
            }

        // Deterministic edge order: cost, then lo-tile-id, then hi-tile-id.
        auto lo_tile = [&](const edge& e) { return std::min(nodes[members[e.a]].tile, nodes[members[e.b]].tile); };
        auto hi_tile = [&](const edge& e) { return std::max(nodes[members[e.a]].tile, nodes[members[e.b]].tile); };
        std::sort(edges.begin(), edges.end(), [&](const edge& e, const edge& f) {
            if (e.cost != f.cost) return e.cost < f.cost;
            if (lo_tile(e) != lo_tile(f)) return lo_tile(e) < lo_tile(f);
            return hi_tile(e) < hi_tile(f);
        });

        // Kruskal MST with union-find.
        std::vector<int> parent(n);
        for (int i = 0; i < n; ++i) parent[i] = i;
        std::function<int(int)> find = [&](int x) {
            while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
            return x;
        };
        std::vector<std::pair<int, int>> chosen;
        std::vector<std::vector<bool>> in_mst(n, std::vector<bool>(n, false));
        for (const edge& e : edges)
        {
            const int ra = find(e.a), rb = find(e.b);
            if (ra != rb)
            {
                parent[ra] = rb;
                chosen.emplace_back(e.a, e.b);
                in_mst[e.a][e.b] = in_mst[e.b][e.a] = true;
            }
        }

        // Relative-neighbour redundancy: keep a non-MST edge (a,b) only if no third
        // centre c is closer to BOTH endpoints than they are to each other — i.e.
        // max(d[a][c], d[b][c]) < d[a][b] for some c disqualifies it. Adds the short
        // loops a bare MST misses without cluttering the lattice.
        for (const edge& e : edges)
        {
            if (in_mst[e.a][e.b])
                continue;
            bool keep = true;
            for (int c = 0; c < n; ++c)
            {
                if (c == e.a || c == e.b)
                    continue;
                if (std::max(d[e.a][c], d[e.b][c]) < d[e.a][e.b])
                {
                    keep = false;
                    break;
                }
            }
            if (keep)
                chosen.emplace_back(e.a, e.b);
        }

        // 3+4. Rasterise: tier by the two centres' scales (Highway / Road / Track).
        for (const auto& [a, b] : chosen)
        {
            const std::uint8_t tier = edge_tier(nodes[members[a]].scale, nodes[members[b]].scale);
            stamp_edge(w, body, nodes[members[a]].tile, nodes[members[b]].tile, tier);
        }
    }

    // 5. Border links: one local road between the nearest centre pair of each
    //    territorially-adjacent nation pair, connecting the per-nation lattices.
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return;
    const int gw = std::max(1, bit->second.grid_width);
    const int gh = std::max(1, bit->second.grid_height);
    const std::vector<entity_id>& grid = body_tile_grid(w, body); // grid_y*gw + grid_x

    // Territorial adjacency (sorted nation pair → adjacent), from a 4-cardinal
    // neighbour scan with east-west column wrap (matching the logistics topology).
    std::set<std::pair<entity_id, entity_id>> adjacency;
    auto note_adjacent = [&](entity_id na, entity_id nb) {
        if (na == null_entity || nb == null_entity || na == nb)
            return;
        adjacency.insert({ std::min(na, nb), std::max(na, nb) });
    };
    for (int r = 0; r < gh; ++r)
        for (int cc = 0; cc < gw; ++cc)
        {
            const entity_id t = grid[static_cast<std::size_t>(r) * gw + cc];
            if (t == null_entity)
                continue;
            const entity_id na = nation_of(w, t);
            const int rc = (cc + 1) % gw; // east neighbour, wrapped
            note_adjacent(na, nation_of(w, grid[static_cast<std::size_t>(r) * gw + rc]));
            if (r + 1 < gh) // south neighbour (no vertical wrap)
                note_adjacent(na, nation_of(w, grid[static_cast<std::size_t>(r + 1) * gw + cc]));
        }

    for (const auto& [na, nb] : adjacency)
    {
        const auto ia = by_nation.find(na);
        const auto ib = by_nation.find(nb);
        if (ia == by_nation.end() || ib == by_nation.end())
            continue;

        // Nearest reachable centre pair across the boundary; tie-break by tile ids
        // (members are already tile-id ordered, so the first minimum is canonical).
        float best = kUnreachable;
        entity_id best_a = null_entity, best_b = null_entity;
        for (const int a : ia->second)
            for (const int b : ib->second)
            {
                const logistics_path p =
                    intra_body_path(w, body, nodes[a].tile, nodes[b].tile);
                if (p.reachable && p.cost < best)
                {
                    best = p.cost;
                    best_a = nodes[a].tile;
                    best_b = nodes[b].tile;
                }
            }
        if (best_a != null_entity)
            stamp_edge(w, body, best_a, best_b, kTrack);
    }

    // The A* cost cache (world.astar_cost_cache) was populated road-free while this
    // pass measured centre-pair costs to lay the network out — correct for the MST
    // decision, but now stale: the stamped roads lower those same lanes' costs. Drop
    // it so the gameplay dispatch loop recomputes against the final road_level field
    // (the "invalidated when road_level changes" contract, world.hpp). The raster
    // index is road-independent and stays. Generation-time, so clearing all is cheap.
    w.astar_cost_cache.clear();
    // Roads change traversal cost, so they change reach too (BL-323 S2).
    w.body_reach_cost.clear();
}
