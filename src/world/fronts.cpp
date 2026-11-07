#include "fronts.hpp"

#include "battle_system.hpp" // unit_in_battle
#include "corp_command.hpp"
#include "hex_neighbors.hpp"
#include "logistics.hpp"     // body_tile_grid (warming the raster)
#include "province.hpp"
#include "recipe_registry.hpp" // BL-1455: the class terrain rows a front's slot and target read
#include "stance.hpp"        // is_hostile
#include "unit_roster.hpp"   // unit_strength
#include "war.hpp"           // at_war (BL-1502: occupation)
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

/// BL-1502: the nation a front reads as holding a tile, owner OR occupier. Every front
/// walk in this file reads this; legal ownership is `tile_to_nation`'s, read by no front.
entity_id owner_of(const world& w, entity_id tile)
{
    return w.tile_controller(tile);
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
    // BL-1441 (front edge cases): A SPAN IS NEVER LEFT UNNAMED. A span whose every
    // tile an earlier span already claimed (a one-tile salient touching two
    // neighbours, say) takes instead the lowest unclaimed tile of its NEIGHBOUR'S
    // ground across it. Such a tile is never in any of this nation's spans, so it
    // cannot collide with a span key, and tiles across toward different neighbours
    // are different nations' tiles, so they cannot collide with each other. Claimed
    // in the same order as above. Only a span the first rule left bare is touched,
    // so every other key is exactly what it was.
    for (const std::size_t i : claim)
    {
        if (out[i].key != null_entity)
            continue;
        std::set<entity_id> across;
        for (const entity_id t : out[i].span)
        {
            const auto tit = w.tiles.find(t);
            if (tit == w.tiles.end())
                continue;
            for (int side = 0; side < 6; ++side)
            {
                const entity_id n = neighbour_of(w, rs, tit->second, side);
                if (n != null_entity && owner_of(w, n) == out[i].neighbour)
                    across.insert(n);
            }
        }
        for (const entity_id t : across) // ascending
            if (used.insert(t).second)
            {
                out[i].key = t;
                break;
            }
    }
    // Not known to occur, not proven impossible (it would take two spans
    // toward one neighbour, each wholly claimed, sharing their only tile across);
    // the one case that still drops a front; erased so a key is never null.
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

/// BL-1503 (MILITARY.md § Fronts, Slots; Ben, 2026-11-05): a land unit never takes a
/// WATER slot, bridges excepted. `march_wall` lets a land class onto its own shore
/// (the owned-coast ruling, which stands for a march), and owned shore water is in
/// `tile_to_nation`, so a span can hold it: this is the slot's own stricter test, read
/// by hold and by `front_reachable`. A naval row is untouched, and a roaded water tile
/// is land to a land class (`tile_domain_blocks`: a bridge is a road).
bool water_slot_for(uint16_t type, const tile_component& t)
{
    if (!is_water(t.substrate) || tile_has_road(t))
        return false;
    const auto& rows = unit_roster_table();
    return type < rows.size()
        && (rows[type].domains & domain_bit(traversal_domain::land)) != 0;
}

/// A push's targets as each unit ranks them: weakest stack, then nearest its slot, then the
/// lower tile id. One ranking, for the step and for the scorer that prices it (BL-1540).
struct push_rank { int64_t s; int d; entity_id t; };
std::vector<push_rank> push_order(const world& w, const std::map<entity_id, int64_t>& held, entity_id slot)
{
    std::vector<push_rank> order;
    order.reserve(held.size());
    for (const auto& [t, st] : held)
        order.push_back({ st, hex_distance(w, slot, t), t });
    std::sort(order.begin(), order.end(), [](const push_rank& a, const push_rank& b) {
        if (a.s != b.s) return a.s < b.s;
        if (a.d != b.d) return a.d < b.d;
        return a.t < b.t;
    });
    return order;
}

/// The push's candidate tiles (Ben, 2026-10-27: "fronts and push use tiles too"):
/// the LAND tiles of `f.neighbour` with a hex side onto `f`'s span, ascending id.
std::set<entity_id> push_candidates(const world& w, raster_source& rs, const front& f)
{
    std::set<entity_id> out;
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
            const auto nit = w.tiles.find(n);
            if (nit != w.tiles.end() && !is_water(nit->second.substrate))
                out.insert(n);
        }
    }
    return out;
}

/// The standing stack on each of @p cand, as the push reads it: the summed
/// `unit_strength` of the units of any nation but @p nation on it (an empty tile
/// is 0, the weakest of all). Integer strengths, so the unordered walk cannot
/// reach the sums. Tiles @p nation's own units already stand on are dropped —
/// they are taken, not targets.
std::map<entity_id, int64_t> push_strengths(const world& w, entity_id nation,
                                            const std::set<entity_id>& cand)
{
    std::map<entity_id, int64_t> held;
    for (const entity_id t : cand)
        held[t] = 0;
    std::set<entity_id> ours;
    for (const auto& [uid, u] : w.units)
    {
        if (u.count <= 0)
            continue;
        const auto it = held.find(u.position);
        if (it == held.end())
            continue;
        if (u.owner == nation)
            ours.insert(u.position);
        else
            it->second += unit_strength(w, u);
    }
    for (const entity_id t : ours)
        held.erase(t);
    return held;
}

} // namespace

// ---------------------------------------------------------------------------

std::vector<front> derive_fronts(const world& w, entity_id nation)
{
    std::vector<entity_id> owned;
    for (const auto& [tile, n] : w.tile_to_nation)
        if (w.tile_controller(tile) == nation) // BL-1502: owner or occupier
            owned.push_back(tile);
    std::sort(owned.begin(), owned.end());
    raster_source rs{ w, {} };
    return fronts_of(w, rs, nation, owned);
}

std::map<entity_id, std::vector<front>> derive_all_fronts(const world& w)
{
    std::map<entity_id, std::vector<entity_id>> owned;
    for (const auto& [tile, n] : w.tile_to_nation)
        if (const entity_id holder = w.tile_controller(tile); holder != null_entity) // BL-1502
            owned[holder].push_back(tile);
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
        // Which neighbour did it face? Only the fronts it was set against know.
        entity_id faced = null_entity;
        if (const auto oit = old.find(nation); oit != old.end())
            if (const front* of = front_keyed(oit->second, key))
                faced = of->neighbour;
        // Rule 1 holds only while the key still faces the SAME neighbour: keys are
        // claimed across neighbours, so a span changing size elsewhere can hand this
        // tile to a front toward another nation (cold review, 2026-10-27). Following
        // it there would choose a front, and an order, the actor never chose.
        if (const front* kf = front_keyed(now, key);
            kf != nullptr && (faced == null_entity || kf->neighbour == faced))
            return key;
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

bool occupy_on_entry(world& w, entity_id nation, uint16_t type, entity_id tile, bool& batch_open)
{
    if (w.wars.empty())
        return false; // at peace nothing is taken, and the layer is empty
    const auto tit = w.tiles.find(tile);
    const auto& rows = unit_roster_table();
    if (tit == w.tiles.end() || type >= rows.size()
        || (rows[type].domains & domain_bit(traversal_domain::land)) == 0)
        return false; // a naval row takes no ground
    if (is_water(tit->second.substrate) && !tile_has_road(tit->second))
        return false; // water is not ground; a bridge is a road
    const auto own = w.tile_to_nation.find(tile);
    if (own == w.tile_to_nation.end() || own->second == null_entity)
        return false; // no one's tile
    const entity_id holder = w.tile_controller(tile);
    if (holder == nation || !at_war(w, nation, holder))
        return false;
    if (!batch_open)
    {
        reanchor_fronts(w); // the old fronts, before the first write (fronts.hpp)
        batch_open = true;
    }
    // The layer's one invariant: an occupier is at war with the tile's OWNER. A nation
    // driving out a third party it has no war with the owner of hands the tile home.
    if (nation != own->second && at_war(w, nation, own->second))
        w.tile_occupier[tile] = nation;
    else
        w.tile_occupier.erase(tile);
    ++w.territory_generation; // the front cache reads it; ownership and catchment did not move
    return true;
}

void clear_occupation_at_peace(world& w)
{
    std::vector<entity_id> ending; // ascending: the map is
    for (const auto& [tile, occupier] : w.tile_occupier)
    {
        const auto own = w.tile_to_nation.find(tile);
        if (own == w.tile_to_nation.end() || !at_war(w, occupier, own->second))
            ending.push_back(tile);
    }
    if (ending.empty())
        return;
    reanchor_fronts(w); // the old fronts, before the first erase (fronts.hpp)
    for (const entity_id tile : ending)
        w.tile_occupier.erase(tile);
    ++w.territory_generation;
    reanchor_fronts(w);
}

bool front_may_push(const world& w, const front& f)
{
    return is_hostile(w, f.nation, f.neighbour);
}

entity_id front_push_target(const world& w, const front& f, int64_t* defence)
{
    if (defence != nullptr)
        *defence = 0;
    raster_source rs{ w, {} };
    const std::map<entity_id, int64_t> held = push_strengths(w, f.nation, push_candidates(w, rs, f));
    entity_id best = null_entity;
    int64_t best_s = std::numeric_limits<int64_t>::max();
    for (const auto& [t, s] : held) // ascending: the first at the lowest strength wins
        if (s < best_s)
        {
            best_s = s;
            best   = t;
        }
    if (defence != nullptr && best != null_entity)
        *defence = best_s;
    return best;
}

rival_march_tally*& rival_march_tally_sink()
{
    static rival_march_tally* sink = nullptr;
    return sink;
}

int tile_hex_steps(const world& w, entity_id a, entity_id b)
{
    return hex_distance(w, a, b);
}

std::vector<entity_id> front_push_tiles(world& w, const recipe_registry& reg, const front& f,
                                        const std::vector<entity_id>& members)
{
    std::vector<entity_id> out(members.size(), null_entity);
    if (f.span.empty() || members.empty())
        return out;
    raster_source rs{ w, {} };
    const std::map<entity_id, int64_t> held = push_strengths(w, f.nation, push_candidates(w, rs, f));
    const std::size_t n = members.size(), len = f.span.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const unit_component& u = w.units.at(members[i]);
        if (u.count <= 0 || u.position == null_entity || unit_refitting(u) || unit_in_battle(w, members[i]))
            continue;
        if (owner_of(w, u.position) == f.neighbour && !at_war(w, f.nation, f.neighbour))
            continue;
        const std::vector<push_rank> order = push_order(w, held, f.span[((2 * i + 1) * len) / (2 * n)]);
        const std::function<bool(const tile_component&)> wall =
            march_wall(w, u.owner, u.type, class_wall(reg.military(), u.type));
        for (const push_rank& r : order)
        {
            if (const auto tit = w.tiles.find(r.t); wall && tit != w.tiles.end() && wall(tit->second))
                continue;
            if (u.order.dest == r.t)
            {
                out[i] = r.t;
                break;
            }
            const corp_command_result res = march_check(w, f.nation, members[i], r.t, null_entity, nullptr, &reg.military());
            if (res == corp_command_result::applied)
            {
                out[i] = r.t;
                break;
            }
            if (res != corp_command_result::rejected_terrain)
                break;
        }
    }
    return out;
}

entity_id front_hold_slot(world& w, const recipe_registry& reg, const unit_component& unit, const front& f,
                          std::size_t i, std::size_t n)
{
    const std::size_t len = f.span.size();
    if (len == 0 || n == 0 || i >= n)
        return null_entity;
    const std::function<bool(const tile_component&)> wall =
        march_wall(w, unit.owner, unit.type, class_wall(reg.military(), unit.type));
    const auto open = [&](entity_id t) {
        const auto tit = w.tiles.find(t);
        return tit != w.tiles.end() && !(wall && wall(tit->second)) && !water_slot_for(unit.type, tit->second);
    };
    const std::size_t s = ((2 * i + 1) * len) / (2 * n); // the slot, as advance_fronts spaces them
    for (std::size_t d = 0; d < len; ++d) // nearest first, ties to the lower index, as hold walks
    {
        if (d > s && s + d >= len)
            break;
        if (d <= s && open(f.span[s - d]))
            return f.span[s - d];
        if (d > 0 && s + d < len && open(f.span[s + d]))
            return f.span[s + d];
    }
    return null_entity;
}

bool front_reachable(world& w, const recipe_registry& reg, entity_id unit, const front& f)
{
    const auto uit = w.units.find(unit);
    if (uit == w.units.end() || f.span.empty())
        return false;
    // A lone unit's slot is span[len / 2]; walk outward from it as `hold` does,
    // nearest first, ties to the lower index.
    const std::size_t len = f.span.size(), s = len / 2;
    const std::function<bool(const tile_component&)> wall =
        march_wall(w, uit->second.owner, uit->second.type, class_wall(reg.military(), uit->second.type));
    std::vector<entity_id> order;
    order.reserve(len);
    for (std::size_t d = 0; d < len; ++d)
    {
        if (d > s && s + d >= len)
            break;
        if (d <= s)
            order.push_back(f.span[s - d]);
        if (d > 0 && s + d < len)
            order.push_back(f.span[s + d]);
    }
    for (const entity_id t : order)
    {
        if (const auto tit = w.tiles.find(t); tit != w.tiles.end()
            && ((wall && wall(tit->second)) || water_slot_for(uit->second.type, tit->second)))
            continue;
        if (uit->second.position == t)
            return true;
        return march_check(w, f.nation, unit, t, null_entity, nullptr, &reg.military())
            == corp_command_result::applied;
    }
    return false;
}

namespace {

/// (province -> tile -> nations standing there), from every living unit on land. Ordered
/// containers filled from an unordered walk: only their contents are ever read.
using province_stacks = std::map<uint32_t, std::map<entity_id, std::set<entity_id>>>;

province_stacks stacks_by_province(const world& w)
{
    province_stacks out;
    for (const auto& [uid, u] : w.units)
        if (u.count > 0 && u.position != null_entity)
            out[w.provinces.province_of(u.position)][u.position].insert(u.owner);
    return out;
}

bool stack_hostile_to(const world& w, entity_id nation, const std::set<entity_id>& owners)
{
    for (const entity_id o : owners)
        if (o != nation && is_hostile(w, nation, o))
            return true;
    return false;
}

} // namespace

entity_id defend_post_for(world& w, const recipe_registry& reg, entity_id unit, uint32_t province)
{
    const auto uit = w.units.find(unit);
    const auto* pr = w.provinces.find(province);
    if (uit == w.units.end() || pr == nullptr)
        return null_entity;
    const entity_id nation = uit->second.owner;
    const auto mine = [&](entity_id t) {
        const auto tit = w.tiles.find(t);
        const auto own = w.tile_to_nation.find(t);
        return tit != w.tiles.end() && own != w.tile_to_nation.end() && own->second == nation
            && (!is_water(tit->second.substrate) || tile_has_road(tit->second));
    };
    if (uit->second.position != null_entity && w.provinces.province_of(uit->second.position) == province
        && mine(uit->second.position))
        return uit->second.position;
    std::vector<std::pair<int, entity_id>> order;
    for (const entity_id t : pr->tiles)
        if (mine(t))
            order.emplace_back(hex_distance(w, uit->second.position, t), t);
    std::sort(order.begin(), order.end());
    for (const auto& [d, t] : order)
        if (march_check(w, nation, unit, t, null_entity, nullptr, &reg.military())
            == corp_command_result::applied)
            return t;
    return null_entity;
}

front_step advance_defenders(world& w, const recipe_registry& reg)
{
    front_step out;
    std::vector<entity_id> ids;
    for (const auto& [uid, u] : w.units)
        if (u.defend_province != no_province)
            ids.push_back(uid);
    if (ids.empty())
        return out;
    std::sort(ids.begin(), ids.end());
    const province_stacks stacks = stacks_by_province(w);

    const auto issue = [&](entity_id uid, entity_id owner, corp_verb verb, entity_id tile) {
        corp_command cmd;
        cmd.tick      = w.current_econ_tick;
        cmd.corp      = owner;
        cmd.as_nation = owner;
        cmd.verb      = verb;
        cmd.subject   = uid;
        cmd.tile      = tile;
        // The order's own step: the verbs end a defend order for a hand order, not for this.
        const uint32_t province = w.units.at(uid).defend_province;
        const entity_id post    = w.units.at(uid).defend_post;
        const corp_command_result r = apply_corp_command(w, reg, cmd);
        w.units.at(uid).defend_province = province;
        w.units.at(uid).defend_post     = post;
        if (verb == corp_verb::march_unit)
            ++(r == corp_command_result::applied ? out.marches_applied : out.marches_refused);
    };

    for (const entity_id uid : ids)
    {
        unit_component& u = w.units.at(uid);
        if (u.count <= 0 || u.position == null_entity || unit_refitting(u) || unit_in_battle(w, uid))
            continue;
        const entity_id nation = u.owner;

        // A province with no ground of the nation's left ends the order (a cession).
        const province* pr = w.provinces.find(u.defend_province);
        bool any_own = false;
        if (pr != nullptr)
            for (const entity_id t : pr->tiles)
                if (const auto own = w.tile_to_nation.find(t); own != w.tile_to_nation.end() && own->second == nation)
                {
                    any_own = true;
                    break;
                }
        if (!any_own)
        {
            u.defend_province = no_province;
            u.defend_post     = null_entity;
            continue;
        }

        // The hostile stacks in the province, nearest first, ties to the lower tile id.
        std::vector<std::pair<int, entity_id>> targets;
        if (const auto sit = stacks.find(u.defend_province); sit != stacks.end())
            for (const auto& [t, owners] : sit->second)
                if (stack_hostile_to(w, nation, owners))
                    targets.emplace_back(hex_distance(w, u.position, t), t);
        std::sort(targets.begin(), targets.end());

        bool reacting = false;
        for (const auto& [d, t] : targets)
        {
            if (u.order.dest == t)
            {
                reacting = true; // already on its way
                break;
            }
            if (march_check(w, nation, uid, t, null_entity, nullptr, &reg.military())
                == corp_command_result::applied)
            {
                issue(uid, nation, corp_verb::march_unit, t);
                reacting = true;
                break;
            }
        }
        if (reacting)
            continue;

        // Nothing to march on: back to its post, or stand where it is.
        const entity_id post = u.defend_post;
        if (u.position == post)
        {
            if (u.order.dest != null_entity)
                issue(uid, nation, corp_verb::halt_unit, null_entity);
            continue;
        }
        if (u.order.dest == post)
            continue;
        if (march_check(w, nation, uid, post, null_entity, nullptr, &reg.military())
            == corp_command_result::applied)
            issue(uid, nation, corp_verb::march_unit, post);
        else if (u.order.dest != null_entity)
            issue(uid, nation, corp_verb::halt_unit, null_entity);
    }
    return out;
}

bool defenders_awake(const world& w)
{
    bool any = false;
    for (const auto& [uid, u] : w.units) // a pure any-of: walk order cannot reach it
        if (u.defend_province != no_province && u.count > 0)
        {
            if (u.position != u.defend_post)
                return true;
            any = true;
        }
    if (!any || w.corp_hostile_pairs.empty())
        return false;
    const province_stacks stacks = stacks_by_province(w);
    for (const auto& [uid, u] : w.units)
        if (u.defend_province != no_province && u.count > 0)
            if (const auto sit = stacks.find(u.defend_province); sit != stacks.end())
                for (const auto& [t, owners] : sit->second)
                    if (stack_hostile_to(w, u.owner, owners))
                        return true;
    return false;
}

int open_capital_defence(world& w)
{
    std::vector<entity_id> ids;
    for (const auto& [uid, u] : w.units)
        ids.push_back(uid);
    std::sort(ids.begin(), ids.end());
    int given = 0;
    const auto& rows = unit_roster_table();
    for (const entity_id uid : ids)
    {
        unit_component& u = w.units.at(uid);
        const auto nit = w.nations.find(u.owner);
        if (nit == w.nations.end() || !nit->second.is_major || nit->second.capital_tile == null_entity)
            continue;
        if (u.count <= 0 || u.position == null_entity || u.front != null_entity
            || u.defend_province != no_province || u.type >= rows.size()
            || (rows[u.type].domains & domain_bit(traversal_domain::land)) == 0)
            continue;
        const uint32_t cap = w.provinces.province_of(nit->second.capital_tile);
        if (cap == 0 || w.provinces.province_of(u.position) != cap) // province_of: 0 is absent
            continue;
        u.defend_province = cap;
        u.defend_post     = u.position;
        ++given;
    }
    return given;
}

front_step advance_fronts(world& w, const recipe_registry& reg)
{
    front_step out;
    reanchor_fronts(w);

    // (nation, key) -> its assigned units, ascending id.
    std::map<std::pair<entity_id, entity_id>, std::vector<entity_id>> groups;
    {
        std::vector<entity_id> ids;
        for (const auto& [uid, u] : w.units)
            if (u.front != null_entity)
                ids.push_back(uid);
        if (ids.empty())
            return out;
        std::sort(ids.begin(), ids.end());
        for (const entity_id uid : ids)
            groups[{ w.units.at(uid).owner, w.units.at(uid).front }].push_back(uid);
    }

    const auto issue = [&](entity_id uid, entity_id owner, corp_verb verb, entity_id tile) {
        corp_command cmd;
        cmd.tick      = w.current_econ_tick;
        cmd.corp      = owner;
        cmd.as_nation = owner;
        cmd.verb      = verb;
        cmd.subject   = uid;
        cmd.tile      = tile; // every march goes to a tile (2026-10-27)
        const corp_command_result r = apply_corp_command(w, reg, cmd); // a refusal mutates nothing
        if (verb == corp_verb::march_unit)
            ++(r == corp_command_result::applied ? out.marches_applied : out.marches_refused);
        return r;
    };
    // BL-1441 (front edge cases): A MARCH THE GAME WOULD REFUSE IS NEVER ISSUED. Every
    // march a front orders is asked of `march_check` first - the verb's own read-only
    // test - and only an accepted one is issued. A refused slot (unreachable round
    // foreign stacks, a domain the unit cannot enter) leaves the unit standing where it
    // is (a standing order is halted), rather than re-issuing a refused march every
    // tick. Stateless: the test runs again on the next tick, and the slot is taken the
    // day it opens. An accepted march is solved twice, once here and once by the verb,
    // and only on the tick it is issued (the next tick finds `order.dest` on it).
    const auto would_march = [&](entity_id uid, entity_id owner, entity_id tile) {
        return march_check(w, owner, uid, tile, null_entity, nullptr, &reg.military());
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

        // HOLD: n slots spaced evenly along the span, centred in n equal stretches;
        // unit i (ascending id) takes slot i. A unit is on its slot when it stands
        // ON the slot tile — every march ends on its tile (2026-10-27).
        const std::size_t n = uids.size(), len = f->span.size();
        const auto slot_index = [&](std::size_t i) { return ((2 * i + 1) * len) / (2 * n); };
        const auto slot_of = [&](std::size_t i) { return f->span[slot_index(i)]; };

        // TERRAIN AND THE CLASSES (BL-1455; MILITARY.md § Terrain and the classes): a
        // front routes as a march does, so a tile the unit's class cannot enter (blocked
        // unless roaded, no road) is never its slot or its target. `walled_for` is the
        // class's wall test; an empty test (a class the ground never walls) walls nothing.
        // BL-1466: the wall is `march_wall` - the class's DOMAIN too, so a slot on water
        // the unit may not stand on (open ocean, another polity's shore) is skipped for
        // the next candidate, not asked of march_check and refused every day.
        using wall_fn = std::function<bool(const tile_component&)>;
        const auto walled_for = [&](const wall_fn& wall, entity_id t) {
            if (!wall)
                return false;
            const auto it = w.tiles.find(t);
            return it != w.tiles.end() && wall(it->second);
        };

        // HOLD: the slot is the span tile at the unit's index; on a walled slot, the
        // nearest UNWALLED span tile by index (ties to the lower index). A unit with no
        // unwalled slot on the span holds where it stands - halted once, never
        // re-issued daily. BL-1441: a slot `march_check` refuses (unreachable, a domain
        // the unit cannot enter) is never issued - the unit stands where it is, halted,
        // and the slot is asked of `march_check` again next day.
        const auto hold = [&](std::size_t i) {
            const entity_id uid = uids[i];
            const unit_component& u = w.units.at(uid);
            if (busy(uid, u))
                return;
            const wall_fn wall = march_wall(w, u.owner, u.type, class_wall(reg.military(), u.type));
            const std::size_t s = slot_index(i);
            std::vector<entity_id> slots; // nearest first, ties to the lower index
            slots.reserve(len);
            for (std::size_t d = 0; d < len; ++d)
            {
                if (d > s && s + d >= len)
                    break;
                if (d <= s)
                    slots.push_back(f->span[s - d]);
                if (d > 0 && s + d < len)
                    slots.push_back(f->span[s + d]);
            }
            for (const entity_id slot : slots)
            {
                if (walled_for(wall, slot))
                    continue;
                // BL-1503: nor water, bridges excepted. An all-water span runs off the
                // end of this list, and the unit holds where it stands, below.
                if (const auto st = w.tiles.find(slot);
                    st != w.tiles.end() && water_slot_for(u.type, st->second))
                    continue;
                if (u.position == slot)
                {
                    if (u.order.dest != null_entity)
                        issue(uid, nk.first, corp_verb::halt_unit, null_entity);
                    return;
                }
                if (u.order.dest == slot)
                    return;
                if (would_march(uid, nk.first, slot) == corp_command_result::applied)
                    issue(uid, nk.first, corp_verb::march_unit, slot);
                else if (u.order.dest != null_entity)
                    issue(uid, nk.first, corp_verb::halt_unit, null_entity); // stand where it is
                return;
            }
            // No legal slot: hold where it stands.
            if (u.order.dest != null_entity)
                issue(uid, nk.first, corp_verb::halt_unit, null_entity);
        };

        if (push)
        {
            // PUSH: each unit marches onto the neighbour's land tile with a hex side
            // onto the span whose standing stack is weakest (an empty tile weakest of
            // all), nearest its own slot, ties to the lower tile id. Marching onto a
            // held tile is an attack (run_unit_march). At war a tile entered is occupied
            // and the front moves past it, so the push goes on from the new span
            // (BL-1502; MILITARY.md § Occupation). Under a bare hostility nothing is
            // occupied: a unit on the neighbour's ground stands on it, and holds there.
            raster_source rs{ w, {} };
            const std::map<entity_id, int64_t> held =
                push_strengths(w, nk.first, push_candidates(w, rs, *f));
            for (std::size_t i = 0; i < n; ++i)
            {
                const entity_id uid = uids[i];
                const unit_component& u = w.units.at(uid);
                if (busy(uid, u))
                    continue;
                // Still the neighbour's ground under it with no war: no occupation, so it
                // stands. At war it goes on from there, whenever it got there: a unit on
                // the neighbour's ground the day a war opens advances like any other.
                if (owner_of(w, u.position) == f->neighbour && !at_war(w, nk.first, f->neighbour))
                    continue;
                const entity_id slot = slot_of(i);
                // The targets in the push's order: weakest stack, then nearest the slot,
                // then the lower tile id. BL-1455: a target the unit's class cannot
                // enter is skipped, and a target whose every route crosses its walls
                // (rejected_terrain) gives way to the next; the first legal one wins.
                const std::vector<push_rank> order = push_order(w, held, slot);
                const wall_fn wall = march_wall(w, u.owner, u.type, class_wall(reg.military(), u.type));
                bool done = false;
                for (const push_rank& r : order)
                {
                    if (walled_for(wall, r.t))
                        continue;
                    if (u.order.dest == r.t)
                    {
                        done = true; // already on its way
                        break;
                    }
                    // BL-1441: asked of march_check first; only an accepted march is issued.
                    const corp_command_result res = would_march(uid, nk.first, r.t);
                    if (res == corp_command_result::applied)
                    {
                        issue(uid, nk.first, corp_verb::march_unit, r.t);
                        done = true;
                        break;
                    }
                    if (res != corp_command_result::rejected_terrain)
                        break; // unreachable and the like: hold (which asks the same of its slot)
                }
                // Nothing left to push onto, or no target reachable: hold the slot
                // rather than stand idle (cold review, 2026-10-27).
                if (!done)
                    hold(i);
            }
            continue;
        }

        for (std::size_t i = 0; i < n; ++i)
            hold(i);
    }
    return out;
}
