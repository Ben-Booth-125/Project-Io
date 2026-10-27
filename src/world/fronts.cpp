#include "fronts.hpp"

#include "battle_system.hpp" // unit_in_battle
#include "corp_command.hpp"
#include "hex_neighbors.hpp"
#include "logistics.hpp"     // body_tile_grid (warming the raster)
#include "province.hpp"
#include "stance.hpp"        // is_hostile
#include "unit_roster.hpp"   // unit_strength
#include "world.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <limits>
#include <set>
#include <utility>

// BL-1427 (fronts; MILITARY.md § Fronts). See fronts.hpp for the rules; this file
// is their arithmetic. Every walk below is over sorted ids or a std::map/std::set:
// the unordered stores (`tile_to_nation`, `units`) are only ever looked up, or
// gathered and sorted before anything is decided from them.

namespace {

// ---------------------------------------------------------------------------
// The grid
// ---------------------------------------------------------------------------

/// Per-body raster lookup for a CONST world: the warm `world::body_tile_index`
/// when it is built (reanchor_fronts warms it), else a local copy built once per
/// derivation. Either way the same tile per cell — a body holds one tile a cell.
struct raster_source
{
    const world& w;
    std::map<entity_id, std::vector<entity_id>> local;

    const std::vector<entity_id>* get(entity_id body)
    {
        const auto bit = w.bodies.find(body);
        if (bit == w.bodies.end() || bit->second.grid_width <= 0 || bit->second.grid_height <= 0)
            return nullptr;
        const std::size_t cells = static_cast<std::size_t>(bit->second.grid_width)
                                * static_cast<std::size_t>(bit->second.grid_height);
        if (const auto it = w.body_tile_index.find(body);
            it != w.body_tile_index.end() && it->second.size() == cells)
            return &it->second;
        if (const auto it = local.find(body); it != local.end())
            return &it->second;
        std::vector<entity_id> grid(cells, null_entity);
        const int gw = bit->second.grid_width, gh = bit->second.grid_height;
        // One writer per cell, so the walk order of `tiles` cannot reach the result.
        for (const auto& [tid, tc] : w.tiles)
            if (tc.body == body && tc.grid_x >= 0 && tc.grid_x < gw && tc.grid_y >= 0 && tc.grid_y < gh)
                grid[static_cast<std::size_t>(tc.grid_y) * static_cast<std::size_t>(gw)
                     + static_cast<std::size_t>(tc.grid_x)] = tid;
        return &local.emplace(body, std::move(grid)).first->second;
    }
};

/// The hex neighbour of @p tc on @p side, or null_entity. Columns wrap (the body
/// is a cylinder), rows do not — the rule placement, province and logistics walk.
entity_id neighbour_of(const world& w, raster_source& rs, const tile_component& tc, int side)
{
    const auto bit = w.bodies.find(tc.body);
    if (bit == w.bodies.end())
        return null_entity;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>* r = rs.get(tc.body);
    if (r == nullptr)
        return null_entity;
    hex_neighbors::coord c = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, side);
    if (c.gy < 0 || c.gy >= gh)
        return null_entity;
    if (c.gx < 0) c.gx += gw;
    else if (c.gx >= gw) c.gx -= gw;
    return (*r)[static_cast<std::size_t>(c.gy) * static_cast<std::size_t>(gw)
                + static_cast<std::size_t>(c.gx)];
}

entity_id owner_of(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return it != w.tile_to_nation.end() ? it->second : null_entity;
}

/// Hex steps between two tiles on one body (odd-r offset -> axial, the cylinder
/// wrap taken whichever way is shorter). INT_MAX across bodies or for a missing tile.
int hex_distance(const world& w, entity_id a, entity_id b)
{
    const auto ai = w.tiles.find(a), bi = w.tiles.find(b);
    if (ai == w.tiles.end() || bi == w.tiles.end() || ai->second.body != bi->second.body)
        return std::numeric_limits<int>::max();
    const auto bit = w.bodies.find(ai->second.body);
    const int gw = bit != w.bodies.end() ? bit->second.grid_width : 0;
    const auto axial_q = [](int x, int y) { return x - (y - (y & 1)) / 2; };
    const int ay = ai->second.grid_y, by = bi->second.grid_y;
    int best = std::numeric_limits<int>::max();
    for (int k = -1; k <= 1; ++k)
    {
        if (gw <= 0 && k != 0)
            continue;
        const int dq = axial_q(bi->second.grid_x + k * gw, by) - axial_q(ai->second.grid_x, ay);
        const int dr = by - ay;
        best = std::min(best, (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2);
    }
    return best;
}

// ---------------------------------------------------------------------------
// One span's walk order
// ---------------------------------------------------------------------------

/// Breadth-first distances (and first-discovery parents) from @p src over the
/// component graph @p adj, skipping @p blocked cells. Neighbours are visited in
/// ascending index order, so the parents are a pure function of the graph.
void bfs(const std::vector<std::vector<int>>& adj, int src, const std::vector<char>* blocked,
         std::vector<int>& dist, std::vector<int>& parent)
{
    dist.assign(adj.size(), -1);
    parent.assign(adj.size(), -1);
    std::deque<int> q;
    dist[static_cast<std::size_t>(src)] = 0;
    q.push_back(src);
    while (!q.empty())
    {
        const int u = q.front();
        q.pop_front();
        for (const int v : adj[static_cast<std::size_t>(u)])
        {
            if (dist[static_cast<std::size_t>(v)] != -1)
                continue;
            if (blocked != nullptr && (*blocked)[static_cast<std::size_t>(v)])
                continue;
            dist[static_cast<std::size_t>(v)] = dist[static_cast<std::size_t>(u)] + 1;
            parent[static_cast<std::size_t>(v)] = u;
            q.push_back(v);
        }
    }
}

/// The farthest reached cell from a distance vector, ties to the lower index
/// (which is the lower tile id: component cells are indexed in ascending id).
int farthest(const std::vector<int>& dist)
{
    int best = 0;
    for (std::size_t i = 0; i < dist.size(); ++i)
        if (dist[i] > dist[static_cast<std::size_t>(best)])
            best = static_cast<int>(i);
    return best;
}

std::vector<int> path_to(const std::vector<int>& parent, int dst)
{
    std::vector<int> p;
    for (int v = dst; v != -1; v = parent[static_cast<std::size_t>(v)])
        p.push_back(v);
    std::reverse(p.begin(), p.end());
    return p;
}

/// Order one span (cells `tiles`, ascending id, graph `adj`) end to end. See
/// fronts.hpp § WALK ORDER for the rule.
std::vector<entity_id> walk_order(const std::vector<entity_id>& tiles,
                                  const std::vector<std::vector<int>>& adj)
{
    const int m = static_cast<int>(tiles.size());
    if (m <= 1)
        return tiles;

    std::vector<int> dist, parent;
    bfs(adj, 0, nullptr, dist, parent); // cell 0 is the lowest tile id
    const int a = farthest(dist);
    bfs(adj, a, nullptr, dist, parent);
    const int b = farthest(dist);
    std::vector<int> walk = path_to(parent, b); // a .. b

    // A CLOSED RING: is there a second way from a to b that keeps clear of the
    // first line — off its interior and every cell touching it, save the cells
    // touching the two ends? A span two tiles thick offers none (its second lane
    // touches the line's interior all along); a ring round an enclave does.
    if (walk.size() >= 4)
    {
        std::vector<char> blocked(static_cast<std::size_t>(m), 0);
        std::vector<char> near_end(static_cast<std::size_t>(m), 0);
        near_end[static_cast<std::size_t>(a)] = near_end[static_cast<std::size_t>(b)] = 1;
        for (const int v : adj[static_cast<std::size_t>(a)]) near_end[static_cast<std::size_t>(v)] = 1;
        for (const int v : adj[static_cast<std::size_t>(b)]) near_end[static_cast<std::size_t>(v)] = 1;
        for (std::size_t i = 1; i + 1 < walk.size(); ++i)
        {
            const int u = walk[i];
            blocked[static_cast<std::size_t>(u)] = 1;
            for (const int v : adj[static_cast<std::size_t>(u)])
                if (!near_end[static_cast<std::size_t>(v)])
                    blocked[static_cast<std::size_t>(v)] = 1;
        }
        std::vector<int> d2, p2;
        bfs(adj, a, &blocked, d2, p2);
        if (d2[static_cast<std::size_t>(b)] >= 2)
        {
            const std::vector<int> back = path_to(p2, b); // a .. b the other way
            for (std::size_t i = back.size() - 2; i >= 1; --i) // b's side back toward a
                walk.push_back(back[i]);
        }
    }

    // Cells off the line sit beside their nearest line cell: a multi-source
    // breadth-first pass in which each cell takes the LOWEST walk position among
    // its nearest line cells (every layer is complete before the next is read).
    std::vector<int> owner(static_cast<std::size_t>(m), -1), d(static_cast<std::size_t>(m), -1);
    std::deque<int> q;
    for (std::size_t i = 0; i < walk.size(); ++i)
    {
        owner[static_cast<std::size_t>(walk[i])] = static_cast<int>(i);
        d[static_cast<std::size_t>(walk[i])] = 0;
        q.push_back(walk[i]);
    }
    while (!q.empty())
    {
        const int u = q.front();
        q.pop_front();
        for (const int v : adj[static_cast<std::size_t>(u)])
        {
            const std::size_t vi = static_cast<std::size_t>(v), ui = static_cast<std::size_t>(u);
            if (d[vi] == -1)
            {
                d[vi] = d[ui] + 1;
                owner[vi] = owner[ui];
                q.push_back(v);
            }
            else if (d[vi] == d[ui] + 1 && owner[ui] < owner[vi])
                owner[vi] = owner[ui];
        }
    }
    std::vector<int> cells(static_cast<std::size_t>(m));
    for (int i = 0; i < m; ++i) cells[static_cast<std::size_t>(i)] = i;
    std::sort(cells.begin(), cells.end(), [&](int x, int y) {
        const std::size_t xi = static_cast<std::size_t>(x), yi = static_cast<std::size_t>(y);
        if (owner[xi] != owner[yi]) return owner[xi] < owner[yi];
        if (d[xi] != d[yi]) return d[xi] < d[yi];
        return x < y;
    });
    std::vector<entity_id> out;
    out.reserve(static_cast<std::size_t>(m));
    for (const int c : cells) out.push_back(tiles[static_cast<std::size_t>(c)]);
    return out;
}

// ---------------------------------------------------------------------------
// One nation's fronts
// ---------------------------------------------------------------------------

std::vector<front> fronts_of(const world& w, raster_source& rs, entity_id nation,
                             const std::vector<entity_id>& owned) // ascending
{
    // Border tiles per neighbour, each list ascending (owned is).
    std::map<entity_id, std::vector<entity_id>> border;
    for (const entity_id t : owned)
    {
        const auto tit = w.tiles.find(t);
        if (tit == w.tiles.end())
            continue;
        for (int side = 0; side < 6; ++side)
        {
            const entity_id n = neighbour_of(w, rs, tit->second, side);
            if (n == null_entity)
                continue;
            const entity_id o = owner_of(w, n);
            if (o == null_entity || o == nation)
                continue;
            std::vector<entity_id>& v = border[o];
            if (v.empty() || v.back() != t)
                v.push_back(t);
        }
    }

    std::vector<front> out;
    for (const auto& [nb, tiles] : border)
    {
        const std::size_t n = tiles.size();
        const auto index_of = [&](entity_id t) -> int {
            const auto it = std::lower_bound(tiles.begin(), tiles.end(), t);
            return (it != tiles.end() && *it == t) ? static_cast<int>(it - tiles.begin()) : -1;
        };
        std::vector<std::vector<int>> adj(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const tile_component& tc = w.tiles.at(tiles[i]);
            for (int side = 0; side < 6; ++side)
            {
                const int j = index_of(neighbour_of(w, rs, tc, side));
                if (j >= 0 && static_cast<std::size_t>(j) != i)
                    adj[i].push_back(j);
            }
            std::sort(adj[i].begin(), adj[i].end());
            adj[i].erase(std::unique(adj[i].begin(), adj[i].end()), adj[i].end());
        }

        // Components, in ascending order of their lowest tile.
        std::vector<int> comp_of(n, -1);
        for (std::size_t s = 0; s < n; ++s)
        {
            if (comp_of[s] != -1)
                continue;
            std::vector<int> members;
            std::deque<int> q{ static_cast<int>(s) };
            comp_of[s] = static_cast<int>(s);
            while (!q.empty())
            {
                const int u = q.front();
                q.pop_front();
                members.push_back(u);
                for (const int v : adj[static_cast<std::size_t>(u)])
                    if (comp_of[static_cast<std::size_t>(v)] == -1)
                    {
                        comp_of[static_cast<std::size_t>(v)] = static_cast<int>(s);
                        q.push_back(v);
                    }
            }
            std::sort(members.begin(), members.end());
            // Re-index the component's own graph, ascending id.
            std::vector<entity_id> ctiles;
            std::vector<int> local(n, -1);
            for (std::size_t k = 0; k < members.size(); ++k)
            {
                local[static_cast<std::size_t>(members[k])] = static_cast<int>(k);
                ctiles.push_back(tiles[static_cast<std::size_t>(members[k])]);
            }
            std::vector<std::vector<int>> cadj(members.size());
            for (std::size_t k = 0; k < members.size(); ++k)
            {
                for (const int v : adj[static_cast<std::size_t>(members[k])])
                    cadj[k].push_back(local[static_cast<std::size_t>(v)]);
                std::sort(cadj[k].begin(), cadj[k].end());
            }
            front f;
            f.nation    = nation;
            f.neighbour = nb;
            f.span      = walk_order(ctiles, cadj);
            out.push_back(std::move(f));
        }
    }

    // Keys: unique per nation (fronts.hpp § KEYS ARE UNIQUE PER NATION). Claimed
    // smallest span first, ties by neighbour then lowest tile.
    std::vector<std::size_t> claim(out.size());
    std::vector<entity_id> lowest(out.size());
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        claim[i]  = i;
        lowest[i] = *std::min_element(out[i].span.begin(), out[i].span.end());
    }
    std::sort(claim.begin(), claim.end(), [&](std::size_t x, std::size_t y) {
        if (out[x].span.size() != out[y].span.size()) return out[x].span.size() < out[y].span.size();
        if (out[x].neighbour != out[y].neighbour) return out[x].neighbour < out[y].neighbour;
        return lowest[x] < lowest[y];
    });
    std::set<entity_id> used;
    for (const std::size_t i : claim)
    {
        std::vector<entity_id> sorted = out[i].span;
        std::sort(sorted.begin(), sorted.end());
        for (const entity_id t : sorted)
            if (used.insert(t).second)
            {
                out[i].key = t;
                break;
            }
    }
    out.erase(std::remove_if(out.begin(), out.end(),
                             [](const front& f) { return f.key == null_entity; }),
              out.end());
    std::sort(out.begin(), out.end(), [](const front& x, const front& y) {
        if (x.neighbour != y.neighbour) return x.neighbour < y.neighbour;
        return x.key < y.key;
    });
    return out;
}

const std::vector<front>& no_fronts()
{
    static const std::vector<front> empty;
    return empty;
}

const front* front_keyed(const std::vector<front>& fronts, entity_id key)
{
    for (const front& f : fronts)
        if (f.key == key)
            return &f;
    return nullptr;
}

/// Warm the raster index of every body, so the const readers below look a
/// neighbour up in O(1). A derived cache (logistics.hpp), never saved.
void warm_rasters(world& w)
{
    std::vector<entity_id> bodies;
    for (const auto& [bid, bc] : w.bodies)
        if (bc.grid_width > 0 && bc.grid_height > 0)
            bodies.push_back(bid);
    std::sort(bodies.begin(), bodies.end());
    for (const entity_id b : bodies)
        (void)body_tile_grid(w, b);
}

/// The LAND provinces holding a tile of `f.neighbour` with a hex side onto `f`'s
/// span, ascending id.
std::set<uint32_t> push_candidates(const world& w, raster_source& rs, const front& f)
{
    std::set<uint32_t> out;
    for (const entity_id t : f.span)
    {
        const auto tit = w.tiles.find(t);
        if (tit == w.tiles.end())
            continue;
        for (int side = 0; side < 6; ++side)
        {
            const entity_id n = neighbour_of(w, rs, tit->second, side);
            if (n == null_entity || owner_of(w, n) != f.neighbour)
                continue;
            const uint32_t p = w.provinces.province_of(n);
            if (p != 0 && province_kind_of(w, p) == province_kind::land)
                out.insert(p);
        }
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------

std::vector<front> derive_fronts(const world& w, entity_id nation)
{
    std::vector<entity_id> owned;
    for (const auto& [tile, n] : w.tile_to_nation)
        if (n == nation)
            owned.push_back(tile);
    std::sort(owned.begin(), owned.end());
    raster_source rs{ w, {} };
    return fronts_of(w, rs, nation, owned);
}

std::map<entity_id, std::vector<front>> derive_all_fronts(const world& w)
{
    std::map<entity_id, std::vector<entity_id>> owned;
    for (const auto& [tile, n] : w.tile_to_nation)
        if (n != null_entity)
            owned[n].push_back(tile);
    raster_source rs{ w, {} };
    std::map<entity_id, std::vector<front>> out;
    for (auto& [nation, tiles] : owned)
    {
        std::sort(tiles.begin(), tiles.end());
        std::vector<front> f = fronts_of(w, rs, nation, tiles);
        if (!f.empty())
            out.emplace(nation, std::move(f));
    }
    return out;
}

const front* front_at(const std::vector<front>& fronts, entity_id tile)
{
    for (const front& f : fronts)
        for (entity_id t : f.span)
            if (t == tile)
                return &f;
    return nullptr;
}

const front* front_named(const std::vector<front>& fronts, entity_id tile)
{
    if (const front* f = front_keyed(fronts, tile))
        return f;
    return front_at(fronts, tile);
}

front_order front_order_of(const world& w, entity_id nation, entity_id key)
{
    for (const front_order_entry& e : w.front_orders)
        if (e.nation == nation && e.key == key)
            return e.order;
    return front_order::hold;
}

std::vector<front> current_fronts(const world& w, entity_id nation)
{
    const front_cache& c = w.fronts_cache;
    if (c.built && c.territory_generation == w.territory_generation)
    {
        const auto it = c.by_nation.find(nation);
        return it != c.by_nation.end() ? it->second : std::vector<front>{};
    }
    return derive_fronts(w, nation);
}

const std::vector<front>& nation_fronts(world& w, entity_id nation)
{
    reanchor_fronts(w);
    const auto it = w.fronts_cache.by_nation.find(nation);
    return it != w.fronts_cache.by_nation.end() ? it->second : no_fronts();
}

void reanchor_fronts(world& w)
{
    front_cache& c = w.fronts_cache;
    const bool fresh = c.built && c.territory_generation == w.territory_generation;
    std::map<entity_id, std::vector<front>> old;
    if (!fresh)
    {
        warm_rasters(w);
        if (c.built)
            old = std::move(c.by_nation);
        c.by_nation = derive_all_fronts(w);
        c.built = true;
        c.territory_generation = w.territory_generation;
    }

    // Where does a stored key land now? (fronts.hpp § reanchor_fronts.)
    const auto resolve = [&](entity_id nation, entity_id key) -> entity_id {
        const auto nit = c.by_nation.find(nation);
        const std::vector<front>& now = nit != c.by_nation.end() ? nit->second : no_fronts();
        if (front_keyed(now, key) != nullptr)
            return key;
        // Which neighbour did it face? Only the fronts it was set against know.
        entity_id faced = null_entity;
        if (const auto oit = old.find(nation); oit != old.end())
            if (const front* of = front_keyed(oit->second, key))
                faced = of->neighbour;
        if (faced == null_entity)
            return null_entity;
        for (const front& f : now)
            if (f.neighbour == faced && std::find(f.span.begin(), f.span.end(), key) != f.span.end())
                return f.key;
        entity_id best = null_entity;
        int best_d = std::numeric_limits<int>::max();
        for (const front& f : now) // (neighbour, key) order: the first at a distance wins
        {
            if (f.neighbour != faced)
                continue;
            int d = std::numeric_limits<int>::max();
            for (const entity_id t : f.span)
                d = std::min(d, hex_distance(w, key, t));
            if (d < best_d)
            {
                best_d = d;
                best   = f.key;
            }
        }
        return best_d == std::numeric_limits<int>::max() ? null_entity : best;
    };

    // Units, ascending id.
    std::vector<entity_id> assigned;
    for (const auto& [uid, u] : w.units)
        if (u.front != null_entity)
            assigned.push_back(uid);
    std::sort(assigned.begin(), assigned.end());
    for (const entity_id uid : assigned)
    {
        unit_component& u = w.units.at(uid);
        u.front = resolve(u.owner, u.front);
    }

    // Orders, in stored (nation, key) order; a merged front keeps the first.
    if (!w.front_orders.empty())
    {
        std::vector<front_order_entry> next;
        for (const front_order_entry& e : w.front_orders)
        {
            if (e.order == front_order::hold)
                continue;
            const entity_id k = resolve(e.nation, e.key);
            if (k == null_entity)
                continue;
            bool dup = false;
            for (const front_order_entry& x : next)
                dup = dup || (x.nation == e.nation && x.key == k);
            if (!dup)
                next.push_back({ e.nation, k, e.order });
        }
        std::sort(next.begin(), next.end(), [](const front_order_entry& x, const front_order_entry& y) {
            return x.nation != y.nation ? x.nation < y.nation : x.key < y.key;
        });
        w.front_orders = std::move(next);
    }
}

bool front_may_push(const world& w, const front& f)
{
    return is_hostile(w, f.nation, f.neighbour);
}

uint32_t front_push_target(const world& w, const front& f, int64_t* defence)
{
    if (defence != nullptr)
        *defence = 0;
    raster_source rs{ w, {} };
    const std::set<uint32_t> cand = push_candidates(w, rs, f);
    if (cand.empty())
        return 0;
    std::map<uint32_t, int64_t> held;
    for (const uint32_t p : cand)
        held[p] = 0;
    // Integer strengths: the sum cannot depend on the unordered walk.
    for (const auto& [uid, u] : w.units)
    {
        if (u.owner != f.neighbour || u.count <= 0)
            continue;
        const auto it = held.find(w.provinces.province_of(u.position));
        if (it != held.end())
            it->second += unit_strength(w, u);
    }
    uint32_t best = 0;
    int64_t best_s = std::numeric_limits<int64_t>::max();
    for (const auto& [p, s] : held) // ascending: the first at the lowest strength wins
        if (s < best_s)
        {
            best_s = s;
            best   = p;
        }
    if (defence != nullptr)
        *defence = best_s;
    return best;
}

void advance_fronts(world& w, const recipe_registry& reg)
{
    reanchor_fronts(w);

    // (nation, key) -> its assigned units, ascending id.
    std::map<std::pair<entity_id, entity_id>, std::vector<entity_id>> groups;
    {
        std::vector<entity_id> ids;
        for (const auto& [uid, u] : w.units)
            if (u.front != null_entity)
                ids.push_back(uid);
        if (ids.empty())
            return;
        std::sort(ids.begin(), ids.end());
        for (const entity_id uid : ids)
            groups[{ w.units.at(uid).owner, w.units.at(uid).front }].push_back(uid);
    }

    const auto issue = [&](entity_id uid, entity_id owner, corp_verb verb, uint32_t province) {
        corp_command cmd;
        cmd.tick      = w.current_econ_tick;
        cmd.corp      = owner;
        cmd.as_nation = owner;
        cmd.verb      = verb;
        cmd.subject   = uid;
        cmd.province  = province;
        (void)apply_corp_command(w, reg, cmd); // a refusal mutates nothing; next tick asks again
    };
    const auto busy = [&](entity_id uid, const unit_component& u) {
        return u.count <= 0 || u.position == null_entity || unit_refitting(u) || unit_in_battle(w, uid);
    };

    for (const auto& [nk, uids] : groups)
    {
        const auto nit = w.fronts_cache.by_nation.find(nk.first);
        if (nit == w.fronts_cache.by_nation.end())
            continue;
        const front* f = front_keyed(nit->second, nk.second);
        if (f == nullptr || f->span.empty())
            continue;

        const bool push = front_order_of(w, nk.first, nk.second) == front_order::push
                       && front_may_push(w, *f);
        const uint32_t target = push ? front_push_target(w, *f) : 0;

        if (target != 0)
        {
            // PUSH: into the neighbour's weakest adjacent province. A unit already
            // standing in any province the push may enter holds what it took.
            raster_source rs{ w, {} };
            const std::set<uint32_t> cand = push_candidates(w, rs, *f);
            for (const entity_id uid : uids)
            {
                const unit_component& u = w.units.at(uid);
                if (busy(uid, u))
                    continue;
                if (cand.count(w.provinces.province_of(u.position)) != 0)
                    continue;
                if (u.order.dest != null_entity && u.order.dest_province == target)
                    continue;
                issue(uid, nk.first, corp_verb::march_unit, target);
            }
            continue;
        }

        // HOLD: n slots spaced evenly along the span, centred in n equal stretches;
        // unit i (ascending id) takes slot i. A unit is on its slot when it stands in
        // the slot tile's province — a march ends on entering its province.
        const std::size_t n = uids.size(), len = f->span.size();
        for (std::size_t i = 0; i < n; ++i)
        {
            const entity_id uid = uids[i];
            const unit_component& u = w.units.at(uid);
            if (busy(uid, u))
                continue;
            const entity_id slot = f->span[((2 * i + 1) * len) / (2 * n)];
            const uint32_t want = w.provinces.province_of(slot);
            if (want == 0)
                continue;
            if (w.provinces.province_of(u.position) == want)
            {
                if (u.order.dest != null_entity && u.order.dest_province != want)
                    issue(uid, nk.first, corp_verb::halt_unit, no_province);
                continue;
            }
            if (u.order.dest != null_entity && u.order.dest_province == want)
                continue;
            issue(uid, nk.first, corp_verb::march_unit, want);
        }
    }
}
