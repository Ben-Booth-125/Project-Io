#include "nation_generation.hpp"

#include "hard_coded_world.hpp" // generation_progress — the BL-305 carve tap
#include "hex_neighbors.hpp"    // BL-571: garrison border-tile adjacency
#include "logistics.hpp"        // BL-571: body_tile_grid, for the same
#include "nation_ai.hpp"        // BL-571: nation_grudge, the highest-grudge pick
#include "province.hpp"         // BL-571: province_holder_for, province lookup
#include "unit_roster.hpp"      // BL-571: unit_strength, for garrison_strength_in

#include "tongue.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers — all in anonymous namespace
// ---------------------------------------------------------------------------

namespace {

// ---------------------------------------------------------------------------
// Grid utilities
// ---------------------------------------------------------------------------

/// Chebyshev distance between two grid cells with horizontal column wrapping.
/// Row axis does not wrap (polar caps are open edges).
int grid_distance(int c0, int r0, int c1, int r1, int gw)
{
    int dc = std::abs(c0 - c1);
    if (dc > gw / 2)
        dc = gw - dc; // wrap the shorter way around
    const int dr = std::abs(r0 - r1);
    return std::max(dc, dr);
}

/// Raster index for (col, row), with column wrapped into [0, gw).
int raster_idx(int col, int row, int gw)
{
    return ((col % gw) + gw) % gw + row * gw;
}

/// Enumerate the four cardinal grid neighbours of (col, row).
/// Column wraps; rows outside [0, gh) are omitted.
void cardinal_neighbours(int col, int row, int gw, int gh,
                         std::pair<int,int> out[4], int& count)
{
    const int dc[4] = {  0,  0, -1, 1 };
    const int dr[4] = { -1,  1,  0, 0 };
    count = 0;
    for (int i = 0; i < 4; ++i)
    {
        const int nr = row + dr[i];
        if (nr < 0 || nr >= gh)
            continue;
        const int nc = ((col + dc[i]) % gw + gw) % gw;
        out[count++] = { nc, nr };
    }
}

// ---------------------------------------------------------------------------
// Pass 1 — seed placement
// ---------------------------------------------------------------------------

/// Attempt to place `seed_target` seeds on non-ocean tiles, each at
/// least `min_sep` grid distance from every previously placed seed. Fewer are
/// placed when the separation rule leaves no eligible land — the caller treats
/// Ground a nation would actually seed on: a real biotic cover on real SOIL
/// (BL-519). The pre-split test named three compositions — grassland, forest,
/// wetland — to say one thing, and could not exclude a forested crag because
/// "forested" and "on rock" were alternatives rather than a pair. Scrub is out
/// for the same reason it was before, when it was called tundra.
bool habitable_ground(terrain_substrate sub, terrain_cover cov)
{
    return sub == terrain_substrate::sedimentary
        && (cov == terrain_cover::grass || cov == terrain_cover::forest
            || cov == terrain_cover::marsh);
}

/// the returned size as authoritative.
/// Habitable ground — a real biotic cover on real soil — is strongly preferred.
/// Falls back to any land tile if the preferred pool is exhausted.
///
/// Returns the raster indices of the chosen seed tiles.
std::vector<int> place_seeds(const std::vector<bool>& is_water_map,
                             const std::vector<terrain_substrate>& sub,
                             const std::vector<terrain_cover>& cov,
                             int gw, int gh,
                             int seed_target, int min_sep,
                             std::mt19937& rng)
{
    const int total = gw * gh;

    // Partition land tiles into preferred (habitable) and any-land pools.
    std::vector<int> preferred;
    std::vector<int> any_land;
    preferred.reserve(static_cast<std::size_t>(total) / 4);
    any_land.reserve(static_cast<std::size_t>(total));

    for (int idx = 0; idx < total; ++idx)
    {
        if (is_water_map[idx])
            continue;
        any_land.push_back(idx);
        if (habitable_ground(sub[idx], cov[idx]))
            preferred.push_back(idx);
    }

    // Shuffle both pools independently so candidate order is random.
    std::shuffle(preferred.begin(), preferred.end(), rng);
    std::shuffle(any_land.begin(), any_land.end(), rng);

    // Build a combined candidate list: preferred tiles first, then the rest.
    // Duplicates are fine — we check placement eligibility before accepting.
    std::vector<int> candidates;
    candidates.reserve(preferred.size() + any_land.size());
    candidates.insert(candidates.end(), preferred.begin(), preferred.end());
    for (int idx : any_land)
    {
        // Only add tiles not already in preferred.
        if (!habitable_ground(sub[idx], cov[idx]))
            candidates.push_back(idx);
    }

    std::vector<int> seeds;
    seeds.reserve(static_cast<std::size_t>(seed_target));

    for (int idx : candidates)
    {
        if (static_cast<int>(seeds.size()) >= seed_target)
            break;

        const int col = idx % gw;
        const int row = idx / gw;

        // Enforce minimum separation from all already-placed seeds.
        bool too_close = false;
        for (int s : seeds)
        {
            if (grid_distance(col, row, s % gw, s / gw, gw) < min_sep)
            {
                too_close = true;
                break;
            }
        }
        if (!too_close)
            seeds.push_back(idx);
    }

    return seeds;
}

// ---------------------------------------------------------------------------
// Pass 2 — territory expansion (weighted Voronoi BFS)
// ---------------------------------------------------------------------------

/// Terrain-based expansion cost for a tile. Higher cost drains the claiming
/// nation's budget more, so ranges naturally fall near mountain barriers.
float expansion_cost(terrain_landform lf)
{
    switch (lf)
    {
        case terrain_landform::mountain: return 3.0f;  // hard barrier
        case terrain_landform::highland: return 2.0f;  // soft barrier
        case terrain_landform::rift:     return 1.5f;
        case terrain_landform::canyon:   return 1.25f;
        default:                         return 1.0f;
    }
}

/// Entry for the BFS priority queue.
struct bfs_entry
{
    float    cost;        ///< Accumulated weighted distance from the seed.
    int      col;
    int      row;
    int      nation_idx;  ///< Index into the seeds vector.

    // std::priority_queue is a max-heap; we want lowest-cost first.
    bool operator>(const bfs_entry& o) const { return cost > o.cost; }
};

/// Expand each seed outward until every claimable tile is assigned.
/// OPEN OCEAN is never claimed; coastal water and lakes are (BL-776, the
/// water-domain ruling: coastal water is owned by whoever owns the shore, the
/// deep sea is owned by nobody). Mountains and highlands drain the budget of
/// the claiming nation so ranges fall at or near those features.
///
/// @param seeds        Raster indices of the nation seeds (one per nation).
/// @param unclaimable  Per-tile open-ocean flag: ground no nation may hold.
/// @param tiles        Tile components, for landform lookup.
/// @param tile_ids     Raster-order tile entity IDs.
/// @param gw, gh       Grid dimensions.
/// @param weight       Per-seed growth weight; a seed's step cost is divided by
///                     its weight, so high-weight seeds expand cheaper and claim
///                     larger territory (BL-053 — strongly varied nation sizes).
/// @param rng          Seeded RNG for jitter.
/// @param progress     Optional BL-305 tap: each settled tile is published as it
///                     is claimed, so the loading screen shows the borders grow
///                     in the order the pass actually grows them. Write-only —
///                     never read back, never steers the BFS.
/// @return             Per-tile nation index (-1 = unclaimed/open ocean). Indexed as row*gw+col.
std::vector<int> expand_territory(const std::vector<int>& seeds,
                                  const std::vector<bool>& unclaimable,
                                  const world& w,
                                  const std::vector<entity_id>& tile_ids,
                                  int gw, int gh,
                                  const std::vector<float>& weight,
                                  std::mt19937& rng,
                                  generation_progress* progress)
{
    const int total = gw * gh;
    std::vector<int>   owner(static_cast<std::size_t>(total), -1);
    std::vector<float> dist(static_cast<std::size_t>(total), 1e9f);
    std::vector<bool>  settled(static_cast<std::size_t>(total), false);

    std::uniform_real_distribution<float> jitter(0.0f, 0.25f);

    std::priority_queue<bfs_entry,
                        std::vector<bfs_entry>,
                        std::greater<bfs_entry>> pq;

    // Seed the frontier.
    for (int ni = 0; ni < static_cast<int>(seeds.size()); ++ni)
    {
        const int s = seeds[static_cast<std::size_t>(ni)];
        dist[static_cast<std::size_t>(s)]  = 0.0f;
        owner[static_cast<std::size_t>(s)] = ni;
        pq.push({ 0.0f, s % gw, s / gw, ni });
        if (progress) progress->claim_tile(s, ni);
    }
    if (progress) progress->publish_carve(); // the cores, before anything grows

    int since_publish = 0;

    while (!pq.empty())
    {
        const bfs_entry cur = pq.top();
        pq.pop();

        const int idx = raster_idx(cur.col, cur.row, gw);

        if (settled[static_cast<std::size_t>(idx)])
            continue;
        settled[static_cast<std::size_t>(idx)] = true;

        // BL-305 tap. `settled` is where a tile stops being contested, so this
        // is the honest moment to show it claimed, and `cur.nation_idx` is the
        // winner by the same comparison the pass itself trusts. Batched: an
        // epoch bump per tile would be ~45,000 release fences for a screen that
        // redraws 60 times a second.
        if (progress)
        {
            progress->claim_tile(idx, cur.nation_idx);
            if (++since_publish >= gen_carve_publish_interval)
            {
                since_publish = 0;
                progress->publish_carve();
            }
        }

        // Expand into cardinal neighbours.
        std::pair<int,int> nbrs[4];
        int n = 0;
        cardinal_neighbours(cur.col, cur.row, gw, gh, nbrs, n);

        for (int i = 0; i < n; ++i)
        {
            const int nc  = nbrs[i].first;
            const int nr  = nbrs[i].second;
            const int nidx = raster_idx(nc, nr, gw);

            if (settled[static_cast<std::size_t>(nidx)]
             || unclaimable[static_cast<std::size_t>(nidx)])
                continue;

            // Look up the tile's landform for cost weighting.
            const entity_id tid = tile_ids[static_cast<std::size_t>(nidx)];
            float cost_step = 1.0f;
            if (tid != null_entity)
            {
                const auto it = w.tiles.find(tid);
                if (it != w.tiles.end())
                    cost_step = expansion_cost(it->second.landform);
            }

            // BL-053: divide the step by the owning seed's growth weight, so
            // high-weight seeds accumulate cost slower, reach further, and claim
            // more tiles — turning near-uniform Voronoi cells into varied sizes.
            const float w_owner = weight[static_cast<std::size_t>(cur.nation_idx)];
            const float new_cost = cur.cost + (cost_step + jitter(rng)) / w_owner;

            if (new_cost < dist[static_cast<std::size_t>(nidx)])
            {
                dist[static_cast<std::size_t>(nidx)]  = new_cost;
                owner[static_cast<std::size_t>(nidx)] = cur.nation_idx;
                pq.push({ new_cost, nc, nr, cur.nation_idx });
            }
        }
    }

    if (progress) progress->publish_carve(); // the last partial batch
    return owner;
}

// ---------------------------------------------------------------------------
// Pass 2b — orphan-island assignment
// ---------------------------------------------------------------------------

/// Assign every unclaimed claimable tile to a nation, closing the gaps the
/// sea-blocked Voronoi BFS leaves behind on landmasses disconnected from
/// every seed. Orphan tiles (owner == -1 and not open ocean) are grouped into
/// connected components by cardinal adjacency (column-wrapped, rows do not
/// wrap), and each whole component is assigned to the nation owning the nearest
/// already-claimed tile by `grid_distance`.
///
/// The pass is a pure, deterministic function of its inputs: components are
/// discovered in raster order and "nearest" is tie-broken first by lowest
/// distance, then by lowest nation index, then by lowest claimed-tile index. No
/// RNG is used. If there are no claimed tiles at all, `owner_map` is left
/// unchanged.
///
/// @param owner_map    Per-tile nation index (-1 = unclaimed/open ocean), mutated in place.
/// Give coastal water and lakes the owner of the SHORE that claims them
/// (NR-792; `docs/generation/PROVINCES.md` § Who owns water).
///
/// The carve never grows across water (`unclaimable`), so every water tile
/// arrives here unowned. Ownership is then *derived*, which is what the
/// authority doc has always said and what the flood was never doing:
///
///   1. A water tile touching OWNED land takes that land's owner. Where two
///      nations share a strait, the LOWEST OWNER INDEX wins — arbitrary, but
///      total and stable, which is what a tie-break has to be.
///   2. A LAKE then fills: ownership spreads through lake tiles only, so a lake
///      is owned WHOLE by the shore enclosing it. Coastal water does NOT
///      spread — it is the shoreline RING and nothing more.
///   3. Water that touches no owned shore stays UNOWNED — which is the whole
///      point. With most land unowned, most open coastline is now unowned too,
///      and "you may walk your own shore, not someone else's" finally meets
///      shore that belongs to nobody.
///
/// WHY THE SEA IS A RING AND A LAKE IS NOT, and this was found by measuring
/// rather than reasoned out in advance: the first cut spread ownership through
/// ALL non-ocean water, and it changed nothing at all — coastal water came back
/// 100% owned, byte-identical to the flood it replaced. The coastal band is
/// GLOBALLY CONNECTED, so a single owned shore tile anywhere conducts ownership
/// around every landmass it touches. A lake is genuinely enclosed by its own
/// shore, so filling it is bounded and means what it says. The sea is not
/// enclosed by anything, so on the sea only adjacency is meaningful.
///
/// OPEN OCEAN IS NEVER A SOURCE AND NEVER A DESTINATION. It is structurally
/// unowned (§ Who owns water), so it neither carries ownership nor conducts it
/// between two coasts that a deep sea separates.
///
/// DETERMINISM: a level-synchronous BFS seeded in ascending tile index and
/// expanded in ascending index at every level. Equal-distance ties resolve to
/// the lower index on every machine, and nothing reads a container's own order.
void derive_water_ownership(std::vector<int>& owner_map,
                            const std::vector<terrain_substrate>& sub,
                            int gw, int gh)
{
    const int total = gw * gh;

    auto is_conductive = [&](int idx) {
        const terrain_substrate st = sub[static_cast<std::size_t>(idx)];
        return is_water(st) && !is_open_ocean(st);
    };

    // --- Step 1: the shore ring -------------------------------------------
    std::vector<int> frontier;
    for (int idx = 0; idx < total; ++idx)
    {
        if (!is_conductive(idx) || owner_map[static_cast<std::size_t>(idx)] >= 0)
            continue;

        const int col = idx % gw;
        const int row = idx / gw;
        std::pair<int,int> nbrs[4];
        int n = 0;
        cardinal_neighbours(col, row, gw, gh, nbrs, n);

        int best = -1;
        for (int i = 0; i < n; ++i)
        {
            const int nidx = raster_idx(nbrs[i].first, nbrs[i].second, gw);
            if (is_water(sub[static_cast<std::size_t>(nidx)]))
                continue; // the shore is LAND: water never seeds water here
            const int o = owner_map[static_cast<std::size_t>(nidx)];
            if (o >= 0 && (best < 0 || o < best))
                best = o;
        }
        if (best >= 0)
        {
            owner_map[static_cast<std::size_t>(idx)] = best;
            frontier.push_back(idx);
        }
    }

    // --- Step 2: fill LAKES only, level by level ---------------------------
    // Coastal water stops at the ring seeded above. Only lake tiles conduct.
    std::vector<int> next;
    while (!frontier.empty())
    {
        next.clear();
        for (const int idx : frontier)
        {
            if (sub[static_cast<std::size_t>(idx)] != terrain_substrate::lake)
                continue; // sea is a ring: it never passes ownership on

            const int owner = owner_map[static_cast<std::size_t>(idx)];
            const int col   = idx % gw;
            const int row   = idx / gw;
            std::pair<int,int> nbrs[4];
            int n = 0;
            cardinal_neighbours(col, row, gw, gh, nbrs, n);

            for (int i = 0; i < n; ++i)
            {
                const int nidx = raster_idx(nbrs[i].first, nbrs[i].second, gw);
                if (sub[static_cast<std::size_t>(nidx)] != terrain_substrate::lake
                 || owner_map[static_cast<std::size_t>(nidx)] >= 0)
                    continue;
                owner_map[static_cast<std::size_t>(nidx)] = owner;
                next.push_back(nidx);
            }
        }
        frontier.swap(next);
    }
}


/// @param unclaimable  Per-tile open-ocean flag: ground no nation may hold.
/// @param gw, gh       Grid dimensions.
void assign_orphan_islands(std::vector<int>& owner_map,
                           const std::vector<bool>& unclaimable,
                           int gw, int gh)
{
    const int total = gw * gh;

    // Precompute the claimed-tile list once (raster order).
    std::vector<int> claimed;
    claimed.reserve(static_cast<std::size_t>(total));
    for (int idx = 0; idx < total; ++idx)
    {
        if (owner_map[static_cast<std::size_t>(idx)] >= 0)
            claimed.push_back(idx);
    }

    // Degenerate case: nothing claimed, nothing to anchor orphans to.
    if (claimed.empty())
        return;

    std::vector<bool> visited(static_cast<std::size_t>(total), false);

    // Discover orphan components in raster order via a cardinal flood fill.
    for (int start = 0; start < total; ++start)
    {
        if (visited[static_cast<std::size_t>(start)])
            continue;
        if (owner_map[static_cast<std::size_t>(start)] != -1
         || unclaimable[static_cast<std::size_t>(start)])
        {
            visited[static_cast<std::size_t>(start)] = true;
            continue;
        }

        // Flood the component of orphan land tiles reachable from `start`.
        std::vector<int> component;
        std::queue<int> frontier;
        frontier.push(start);
        visited[static_cast<std::size_t>(start)] = true;

        while (!frontier.empty())
        {
            const int idx = frontier.front();
            frontier.pop();
            component.push_back(idx);

            const int col = idx % gw;
            const int row = idx / gw;
            std::pair<int,int> nbrs[4];
            int n = 0;
            cardinal_neighbours(col, row, gw, gh, nbrs, n);
            for (int i = 0; i < n; ++i)
            {
                const int nidx = raster_idx(nbrs[i].first, nbrs[i].second, gw);
                if (visited[static_cast<std::size_t>(nidx)])
                    continue;
                if (owner_map[static_cast<std::size_t>(nidx)] != -1
                 || unclaimable[static_cast<std::size_t>(nidx)])
                {
                    visited[static_cast<std::size_t>(nidx)] = true;
                    continue;
                }
                visited[static_cast<std::size_t>(nidx)] = true;
                frontier.push(nidx);
            }
        }

        // Find the nearest claimed tile to any tile in the component.
        // Tie-break: lowest distance, then lowest nation index, then lowest
        // claimed-tile index.
        int  best_dist   = -1;
        int  best_nation = -1;
        int  best_claim  = -1;
        for (int cidx : component)
        {
            const int cc = cidx % gw;
            const int cr = cidx / gw;
            for (int kidx : claimed)
            {
                const int kc = kidx % gw;
                const int kr = kidx / gw;
                const int d  = grid_distance(cc, cr, kc, kr, gw);
                const int kn = owner_map[static_cast<std::size_t>(kidx)];

                bool better = false;
                if (best_dist < 0 || d < best_dist)
                    better = true;
                else if (d == best_dist)
                {
                    if (kn < best_nation)
                        better = true;
                    else if (kn == best_nation && kidx < best_claim)
                        better = true;
                }

                if (better)
                {
                    best_dist   = d;
                    best_nation = kn;
                    best_claim  = kidx;
                }
            }
        }

        // Assign the whole component to the winning nation.
        for (int cidx : component)
            owner_map[static_cast<std::size_t>(cidx)] = best_nation;
    }
}

// ---------------------------------------------------------------------------
// Pass 2c — light "in history" merges (BL-053)
// ---------------------------------------------------------------------------

/// Absorb every nation whose territory falls below `min_tiles` into its largest
/// cardinally-adjacent neighbour — smallest first — then compact owner indices
/// into [0, final_count). This amplifies the size spread (rich-get-richer) and
/// produces irregular, grown-looking borders — a light stand-in for "generated
/// in history", not a historical simulation.
///
/// The stopping condition is a **size floor, not a target count**: the loop ends
/// when the smallest survivor clears `min_tiles` (or only one nation is left), so
/// how many nations remain is a consequence of the landmass and its coastline
/// shape rather than a pre-set number.
///
/// Fully deterministic (no RNG): the smallest nation is chosen by tile count then
/// lowest index; the absorbing neighbour by tile count then lowest index; an
/// island nation with no land neighbour is absorbed into the globally largest
/// nation so the pass always progresses. `owner_map` is mutated in place; ocean
/// tiles stay -1.
///
/// BL-769 — `exempt` NAMES THE CITY STATES. An exempt nation is never chosen as
/// the smallest (so the loop steps past it and keeps working on the rest rather
/// than stopping at the first survivor under the floor), is never absorbed, and
/// is never an ABSORBER either — a city state that swallowed its neighbours
/// would stop being one, which is the failure the exemption exists to prevent.
/// An empty mask is exactly the pre-BL-769 pass.
///
/// @return the final nation count (distinct nations after compaction).
int merge_undersized_nations(std::vector<int>& owner_map, int seed_count,
                             int min_tiles, int gw, int gh,
                             const std::vector<bool>& exempt)
{
    const auto is_exempt = [&](int ni) {
        return ni >= 0 && ni < static_cast<int>(exempt.size())
            && exempt[static_cast<std::size_t>(ni)];
    };

    const int total = gw * gh;

    std::vector<int> count(static_cast<std::size_t>(seed_count), 0);
    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner_map[static_cast<std::size_t>(idx)];
        if (ni >= 0)
            ++count[static_cast<std::size_t>(ni)];
    }

    std::vector<bool> active(static_cast<std::size_t>(seed_count), false);
    int distinct = 0;
    for (int ni = 0; ni < seed_count; ++ni)
        if (count[static_cast<std::size_t>(ni)] > 0) { active[static_cast<std::size_t>(ni)] = true; ++distinct; }

    while (distinct > 1)
    {
        // Smallest active NON-EXEMPT nation (tie: lowest index). Exempt ones are
        // skipped rather than breaking the loop: the stopping condition below is
        // about the realms the floor still governs, and a city state sitting
        // permanently under it would otherwise halt the pass on its first turn.
        int small = -1;
        for (int ni = 0; ni < seed_count; ++ni)
        {
            if (!active[static_cast<std::size_t>(ni)]) continue;
            if (is_exempt(ni)) continue;
            if (small < 0 || count[static_cast<std::size_t>(ni)] < count[static_cast<std::size_t>(small)])
                small = ni;
        }
        if (small < 0) break;

        // Size floor, not a target count: once the smallest survivor is viable,
        // every survivor is, and the pass is done.
        if (count[static_cast<std::size_t>(small)] >= min_tiles) break;

        // Largest active nation cardinally adjacent to `small` (tie: lowest index).
        int best = -1;
        std::vector<bool> seen(static_cast<std::size_t>(seed_count), false);
        for (int idx = 0; idx < total; ++idx)
        {
            if (owner_map[static_cast<std::size_t>(idx)] != small) continue;
            const int col = idx % gw, row = idx / gw;
            std::pair<int,int> nbrs[4]; int n = 0;
            cardinal_neighbours(col, row, gw, gh, nbrs, n);
            for (int i = 0; i < n; ++i)
            {
                const int no = owner_map[static_cast<std::size_t>(
                    raster_idx(nbrs[i].first, nbrs[i].second, gw))];
                if (no < 0 || no == small || !active[static_cast<std::size_t>(no)]) continue;
                if (is_exempt(no)) continue; // a city state does not annex
                if (seen[static_cast<std::size_t>(no)]) continue;
                seen[static_cast<std::size_t>(no)] = true;
                if (best < 0
                 || count[static_cast<std::size_t>(no)] > count[static_cast<std::size_t>(best)]
                 || (count[static_cast<std::size_t>(no)] == count[static_cast<std::size_t>(best)] && no < best))
                    best = no;
            }
        }

        // Island with no land neighbour: absorb into the globally largest nation.
        if (best < 0)
            for (int ni = 0; ni < seed_count; ++ni)
            {
                if (!active[static_cast<std::size_t>(ni)] || ni == small) continue;
                if (is_exempt(ni)) continue;
                if (best < 0 || count[static_cast<std::size_t>(ni)] > count[static_cast<std::size_t>(best)])
                    best = ni;
            }
        if (best < 0) break; // only one nation remains

        for (int idx = 0; idx < total; ++idx)
            if (owner_map[static_cast<std::size_t>(idx)] == small)
                owner_map[static_cast<std::size_t>(idx)] = best;
        count[static_cast<std::size_t>(best)]  += count[static_cast<std::size_t>(small)];
        count[static_cast<std::size_t>(small)]  = 0;
        active[static_cast<std::size_t>(small)] = false;
        --distinct;
    }

    // Compact surviving indices into [0, final_count) in ascending order.
    std::vector<int> remap(static_cast<std::size_t>(seed_count), -1);
    int next = 0;
    for (int ni = 0; ni < seed_count; ++ni)
        if (active[static_cast<std::size_t>(ni)])
            remap[static_cast<std::size_t>(ni)] = next++;
    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner_map[static_cast<std::size_t>(idx)];
        if (ni >= 0)
            owner_map[static_cast<std::size_t>(idx)] = remap[static_cast<std::size_t>(ni)];
    }
    return next;
}

// ---------------------------------------------------------------------------
// Pass 2c' — the city states (BL-769)
// ---------------------------------------------------------------------------

/// Which surviving candidate nations are CITY STATES, and therefore not the
/// size floor's business.
///
/// THREE CONDITIONS, ALL FROM THE HISTORY, none of them a size:
///   1. the nation came out of the polity fold at all (its representative seed
///      carries a real polity id) — a Voronoi cell nobody ever governed is not
///      a city state, it is unclaimed ground;
///   2. the polity held exactly ONE region at the epoch. That is what a city
///      state IS at sim grain, and it is the condition that keeps the exemption
///      narrow: an empire below the floor is a failed empire, not a city;
///   3. a population centre stands on its ground. Ben's point 5 is "it is fine
///      to consider city states as population centres", so a single-region
///      polity with no city is a holdout, not a city state, and merges as any
///      other undersized realm does.
///
/// DETERMINISTIC OVER AN UNORDERED CONTAINER, explicitly rather than by luck:
/// `w.population_centres` is hashed, so the ids are SORTED before the walk. The
/// result would be the same either way (the walk only sets flags, and set-union
/// does not care about order) but a later edit that made it care would not be
/// visible, and this file's invariant is that no iteration order can be read.
std::vector<bool> mark_city_states(const world& w,
                                   const std::vector<entity_id>& tile_ids,
                                   const std::vector<int>& owner_map,
                                   const std::vector<int>& fold,
                                   const std::vector<int>& seed_polity,
                                   int seed_count, int total)
{
    std::vector<bool> exempt(static_cast<std::size_t>(seed_count), false);
    if (seed_polity.empty()) return exempt;

    // (1) + (2): how many seeds folded into each representative, and did that
    // representative carry a polity at all.
    std::vector<int> folded_seeds(static_cast<std::size_t>(seed_count), 0);
    for (int si = 0; si < seed_count && si < static_cast<int>(seed_polity.size()); ++si)
    {
        if (seed_polity[static_cast<std::size_t>(si)] < 0) continue;
        const int rep = fold[static_cast<std::size_t>(si)];
        if (rep >= 0 && rep < seed_count) ++folded_seeds[static_cast<std::size_t>(rep)];
    }

    // (3): the cities. Reverse index tile -> raster idx, then one sorted walk.
    std::unordered_map<entity_id, int> tile_slot;
    tile_slot.reserve(tile_ids.size() * 2);
    for (int idx = 0; idx < total && idx < static_cast<int>(tile_ids.size()); ++idx)
        if (tile_ids[static_cast<std::size_t>(idx)] != null_entity)
            tile_slot[tile_ids[static_cast<std::size_t>(idx)]] = idx;

    std::vector<entity_id> centre_ids;
    centre_ids.reserve(w.population_centres.size());
    for (const auto& [cid, cc] : w.population_centres) { (void)cc; centre_ids.push_back(cid); }
    std::sort(centre_ids.begin(), centre_ids.end());

    std::vector<bool> has_centre(static_cast<std::size_t>(seed_count), false);
    for (const entity_id cid : centre_ids)
    {
        const auto ct = w.population_centre_tile.find(cid);
        if (ct == w.population_centre_tile.end()) continue;
        const auto slot = tile_slot.find(ct->second);
        if (slot == tile_slot.end()) continue;
        const int ni = owner_map[static_cast<std::size_t>(slot->second)];
        if (ni >= 0 && ni < seed_count) has_centre[static_cast<std::size_t>(ni)] = true;
    }

    for (int ni = 0; ni < seed_count; ++ni)
        if (folded_seeds[static_cast<std::size_t>(ni)] == 1
            && has_centre[static_cast<std::size_t>(ni)])
            exempt[static_cast<std::size_t>(ni)] = true;

    return exempt;
}

// ---------------------------------------------------------------------------
// Pass 3 — resource profile derivation
// ---------------------------------------------------------------------------

/// Sum each nation's tiles' resource_deposit arrays into resource_abundance.
void derive_resource_profiles(
    const std::vector<int>& owner_map,
    const std::vector<entity_id>& tile_ids,
    const world& w,
    int gw, int gh,
    std::vector<nation_component>& nations)
{
    const int total = gw * gh;
    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner_map[static_cast<std::size_t>(idx)];
        if (ni < 0)
            continue;

        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            continue;

        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end())
            continue;

        const auto& dep = it->second.resource_deposit;
        auto& abund     = nations[static_cast<std::size_t>(ni)].resource_abundance;
        for (std::size_t r = 0; r < resource_count; ++r)
            abund[r] += dep[r];
    }
}

// ---------------------------------------------------------------------------
// Pass 4 — political character
// ---------------------------------------------------------------------------

/// Draw ideology, expansionism, and economic_focus uniformly from the seeded RNG.
void assign_political_character(nation_component& nc, std::mt19937& rng)
{
    {
        std::uniform_int_distribution<int> d(0, 3);
        nc.politics = static_cast<::ideology>(d(rng));
    }
    {
        std::uniform_int_distribution<int> d(0, 2);
        nc.posture = static_cast<::expansionism>(d(rng));
    }
    {
        std::uniform_int_distribution<int> d(0, 2);
        nc.focus = static_cast<::economic_focus>(d(rng));
    }
}

// ---------------------------------------------------------------------------
// Pass 5 — procedural naming
// ---------------------------------------------------------------------------

// There is NO name bank (BL-290). A nation is named in the tongue of the
// culture that settled the region its seed grew from — the same phoneme
// inventory the creeds pass coined that culture's own name and its gods from
// (world/tongue.hpp). The structural words go with it: the "realm" noun and
// the standing epithet are morphemes the culture coined for itself, so the
// name carries no English or Latin morpheme at all, and two nations of one
// culture read as kin because they share both the sounds AND the scaffolding.

/// Generate a procedural nation name in @p t, using one of three structural
/// templates chosen uniformly. @p lex is @p t's own coined vocabulary.
/// Templates:
///   0 — bare name                          e.g. "Kanureth"
///   1 — coined epithet + name              e.g. "Sela Kanureth"
///   2 — name + coined realm word           e.g. "Kanureth Valin"
std::string make_nation_name(std::mt19937& rng, const tongue& t, const tongue_lexicon& lex)
{
    mt_picker pick(rng);

    const int form = std::uniform_int_distribution<int>(0, 2)(rng);
    // Two to three syllables — the same length the old bank produced, so the
    // political map's labels keep their weight.
    std::string name = tongue_word(pick, t, 2 + pick.pick(2));

    switch (form)
    {
        case 0:
            return name;
        case 1:
            if (lex.qualifier.empty()) return name;
            return lex.qualifier[static_cast<std::size_t>(pick.pick(
                       static_cast<int>(lex.qualifier.size())))] + " " + name;
        default: // 2
            if (lex.polity.empty()) return name;
            return name + " " + lex.polity[static_cast<std::size_t>(pick.pick(
                       static_cast<int>(lex.polity.size())))];
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

std::vector<entity_id> generate_nations(
    world& w,
    entity_id /* body_id */,
    const std::vector<entity_id>& tile_ids,
    int gw, int gh,
    const nation_params& params,
    uint32_t seed,
    generation_progress* progress)
{
    const int total = gw * gh;

    // Distinct seed offsets keep each pass's RNG stream independent.
    const uint32_t seed_seeds  = seed ^ 0xA3B4C5D6u;
    const uint32_t seed_expand = seed ^ 0x1F2E3D4Cu;
    const uint32_t seed_weight = seed ^ 0x6D2B79F5u;
    const uint32_t seed_pol    = seed ^ 0x8C7B6A59u;
    const uint32_t seed_name   = seed ^ 0x4E5F607Au;

    // --- Build flat terrain-axis and water maps from the existing tile data ---
    //
    // TWO masks, because the layer asks two different questions (BL-776, the
    // water-domain ruling, Ben 2026-09-06: "give coastal to owners, and sea
    // provinces are unowned"):
    //
    //   `is_water_map` — ground a nation cannot be SEEDED on. A core is a place
    //                    people settled, so it is land, and the seed budget is
    //                    scaled to the landmass. Every kind of water fails this.
    //   `unclaimable`  — ground a nation cannot GROW ACROSS. EVERY kind of
    //                    water. The deep sea is crossed, not held; and coastal
    //                    water and lakes are held, but they are held BY THE
    //                    SHORE rather than by the flood (see `derive_water_
    //                    ownership` below).
    //
    // They coincided until the ruling, which is why one flag used to answer both.
    //
    // THE SUBTLETY THAT WAS WRONG HERE UNTIL 2026-09-07, and it is worth stating
    // because the comment itself carried the error: this flag used to mark only
    // open ocean, reasoning that coastal water "falls to whoever owns the shore,
    // so the carve claims them like any other tile". Those are not the same
    // thing. Letting the flood claim water made ownership a question of which
    // seed's growth arrived first, not of who owns the adjacent land — and it
    // measured out at 100% of coastal water owned against 39% of LAND, which
    // cannot be derived from a shore that is itself mostly unowned (NR-792).
    // Water is now excluded from growth entirely and derived afterwards.
    std::vector<terrain_substrate> sub(static_cast<std::size_t>(total),
                                       terrain_substrate::barren);
    std::vector<terrain_cover> cov(static_cast<std::size_t>(total), terrain_cover::none);
    std::vector<bool> is_water_map(static_cast<std::size_t>(total), false);
    std::vector<bool> unclaimable(static_cast<std::size_t>(total), false);

    for (int idx = 0; idx < total; ++idx)
    {
        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            continue;
        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end())
            continue;
        sub[static_cast<std::size_t>(idx)] = it->second.substrate;
        cov[static_cast<std::size_t>(idx)] = it->second.cover;
        if (is_water(it->second.substrate)) // BL-516
            is_water_map[static_cast<std::size_t>(idx)] = true;
        // NR-792: EVERY water kind is excluded from the growth. Coastal water
        // and lakes get their owner from the shore in `derive_water_ownership`.
        if (is_water(it->second.substrate))
            unclaimable[static_cast<std::size_t>(idx)] = true;
    }

    // --- Seed budget: scaled to the habitable landmass, not taken as a target ---
    // One seed per `land_tiles_per_seed` non-ocean tiles. A wetter or smaller body
    // therefore starts with fewer nations than a dry, continental one — the count
    // is downstream of the geography.
    int land_tiles = 0;
    for (int idx = 0; idx < total; ++idx)
        if (!is_water_map[static_cast<std::size_t>(idx)])
            ++land_tiles;

    const int tiles_per_seed = params.land_tiles_per_seed > 0 ? params.land_tiles_per_seed : 1;
    const int seed_target    = land_tiles > 0
                             ? std::max(1, land_tiles / tiles_per_seed)
                             : 0;
    if (seed_target == 0)
        return {};

    // --- Pass 1: seed placement ---
    // Historical seeds win when the caller supplies them (BL-218): the cores are
    // the settlement pass's region anchors, so the political map grows out of
    // places people settled rather than out of a random draw. The RNG stream is
    // still constructed and left unconsumed on this path, which keeps every
    // *later* pass's stream identical to the random-placement path.
    std::mt19937 seed_rng(seed_seeds);
    std::vector<int> seeds;
    // Parallel to `seeds`: the tongue each surviving seed brought with it, so
    // Pass 5 can name a nation in the speech of the people who settled its
    // core. Empty entries (or the random-placement path) fall back to a rolled
    // tongue there.
    std::vector<tongue> seed_speech;
    // Parallel to `seeds` again (BL-769): which polity held the region this seed
    // is the anchor of. -1 = ground no polity ended up holding, and also every
    // entry on a body with no settlement pass.
    std::vector<int> seed_polity;
    if (!params.seed_tiles.empty())
    {
        std::vector<bool> taken(static_cast<std::size_t>(total), false);
        for (std::size_t si = 0; si < params.seed_tiles.size(); ++si)
        {
            const int idx = params.seed_tiles[si];
            if (idx < 0 || idx >= total) continue;
            if (is_water_map[static_cast<std::size_t>(idx)]) continue;
            if (taken[static_cast<std::size_t>(idx)]) continue;
            taken[static_cast<std::size_t>(idx)] = true;
            seeds.push_back(idx);
            seed_speech.push_back(si < params.seed_tongues.size()
                                      ? params.seed_tongues[si]
                                      : tongue{});
            seed_polity.push_back(si < params.seed_polities.size()
                                      ? params.seed_polities[si]
                                      : -1);
        }
    }
    if (seeds.empty())
    {
        seeds = place_seeds(
            is_water_map, sub, cov, gw, gh,
            seed_target, params.min_seed_separation,
            seed_rng);
    }

    const int seed_count = static_cast<int>(seeds.size());
    if (seed_count == 0)
        return {};

    // --- Pass 1b: per-seed growth weights (BL-053) ---
    // Skewed so most seeds stay small and a few become "great powers": the cube of
    // a uniform draw maps to w in [0.4, 3.4], heavily massed at the low end.
    std::mt19937 weight_rng(seed_weight);
    std::vector<float> weights(static_cast<std::size_t>(seed_count));
    {
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        for (int i = 0; i < seed_count; ++i)
        {
            const float t = u(weight_rng);
            weights[static_cast<std::size_t>(i)] = 0.4f + 3.0f * t * t * t;
        }
    }

    // BL-305: open the carve now the seed count is known, so the loading screen
    // has a blank map of the right shape before the first tile lands.
    if (progress) progress->begin_carve(gw, gh, seed_count);

    // --- Pass 2: territory expansion (weighted) ---
    std::mt19937 expand_rng(seed_expand);
    std::vector<int> owner_map = expand_territory(
        seeds, unclaimable, w, tile_ids, gw, gh, weights, expand_rng, progress);

    // --- Pass 2b: orphan-island assignment (claim sea-disconnected ground) ---
    assign_orphan_islands(owner_map, unclaimable, gw, gh);

    // --- Pass 2c: water takes its owner from the shore (NR-792) -------------
    // Must run AFTER the land carve and orphan islands (its sources are owned
    // land tiles) and BEFORE the polity fold, so a water tile is remapped by
    // the fold exactly as the shore it was derived from is.
    derive_water_ownership(owner_map, sub, gw, gh);

    // --- Pass 2d: THE POLITY FOLD (BL-769) ---------------------------------
    //
    // Phase 5 finalises what the history produced rather than inventing it. The
    // carve above answered a geometric question — where the boundary between
    // two anchors falls — and this answers the political one the sim already
    // answered: whose flag flies over both. Seeds anchored in regions the same
    // polity held collapse to ONE nation, so an empire that took nine regions
    // arrives as one realm holding nine regions' ground instead of nine
    // separate realms that happen to sit next to each other.
    //
    // The representative is the LOWEST-INDEXED seed of the polity. Region ids
    // ascend in founding order (`run_settlement` places, `Settle` appends), so
    // the lowest-indexed region of a polity is its founding core — which is
    // also what Pass 5 already means by "the region it grew from" when it picks
    // the realm's tongue and its capital tile. One rule, three consumers.
    //
    // Territory can end up NON-CONTIGUOUS, and that is the point rather than a
    // defect: a polity that conquered across a neighbour holds ground on both
    // sides of it, which is a fact of the history and was previously erased.
    //
    // Empty `seed_polities` (or all -1) is the identity, so every body without a
    // settlement pass carves exactly as it did before this item.
    std::vector<int> fold(static_cast<std::size_t>(seed_count));
    for (int si = 0; si < seed_count; ++si) fold[static_cast<std::size_t>(si)] = si;
    // BL-975 — THE CHEST CROSSES WITH THE FLAG. Parallel to `seeds`: the 1660
    // treasury (sim material currency, unconverted) each seed carries into
    // nationhood. Only a polity's REPRESENTATIVE seed carries anything, and it
    // carries the polity's whole sum once — the other seeds of the same polity
    // fold under it, so crediting each of them would count the chest per
    // region. Integer until the one conversion below, so the sum is exact and
    // order-free. Zero everywhere on a body with no settlement pass.
    std::vector<int64_t> seed_credit(static_cast<std::size_t>(seed_count), 0);
    {
        int max_polity = -1;
        for (std::size_t si = 0; si < seed_polity.size(); ++si)
            if (seed_polity[si] > max_polity) max_polity = seed_polity[si];

        if (max_polity >= 0)
        {
            // Indexed by polity id, not hashed: the walk below is ascending over
            // a vector, so the representative a polity gets cannot depend on any
            // container's layout.
            std::vector<int> first_seed(static_cast<std::size_t>(max_polity + 1), -1);
            for (int si = 0; si < seed_count && si < static_cast<int>(seed_polity.size()); ++si)
            {
                const int pol = seed_polity[static_cast<std::size_t>(si)];
                if (pol < 0) continue;
                int& rep = first_seed[static_cast<std::size_t>(pol)];
                if (rep < 0) rep = si;
                fold[static_cast<std::size_t>(si)] = rep;
            }

            for (int idx = 0; idx < total; ++idx)
            {
                const int ni = owner_map[static_cast<std::size_t>(idx)];
                if (ni >= 0 && ni < seed_count)
                    owner_map[static_cast<std::size_t>(idx)] = fold[static_cast<std::size_t>(ni)];
            }

            // BL-975: each polity's 1660 treasury lands on its representative
            // seed. A polity with an entry but no anchored seed (all its
            // ground unanchored) has no nation to credit and its chest is
            // lost with it — the history's own outcome, not a leak to patch.
            for (int pol = 0; pol <= max_polity; ++pol)
            {
                const int rep = first_seed[static_cast<std::size_t>(pol)];
                if (rep < 0) continue;
                if (static_cast<std::size_t>(pol) >= params.polity_treasuries.size()) continue;
                const int64_t t = params.polity_treasuries[static_cast<std::size_t>(pol)];
                if (t > 0) seed_credit[static_cast<std::size_t>(rep)] += t;
            }
        }
    }

    // --- Pass 2c: light "in history" merges — absorb anything below the size floor ---
    // No target count: the loop stops when every survivor holds a viable territory,
    // so the final nation count is whatever the landmass and coastline produce.
    //
    // BL-769: the floor now judges POLITY-GRAIN territories, because the fold
    // ran first — so a realm is absorbed for being a small realm, not for being
    // one province of a large one.
    const std::vector<bool> city_states =
        params.keep_city_states
            ? mark_city_states(w, tile_ids, owner_map, fold, seed_polity, seed_count, total)
            : std::vector<bool>{};

    int nation_count = seed_count;
    if (params.min_nation_tiles > 0)
        nation_count = merge_undersized_nations(owner_map, seed_count, params.min_nation_tiles,
                                                gw, gh, city_states);

    // BL-305: 2b and 2c both rewrite owner indices wholesale (the merge also
    // COMPACTS them), so this is the one place the published map has to be
    // restated rather than extended. One whole-map pass, and the recolour it
    // causes is the point — the islands join, the small realms are absorbed,
    // and the count the screen then reports is the final one.
    if (progress)
    {
        for (int idx = 0; idx < total; ++idx)
            progress->claim_tile(idx, owner_map[static_cast<std::size_t>(idx)]);
        progress->nation_count.store(nation_count, std::memory_order_relaxed);
        progress->publish_carve();
    }

    // --- Allocate nation_component stubs (populated in Passes 3–5) ---
    std::vector<nation_component> nation_data(static_cast<std::size_t>(nation_count));

    // Fill tiles lists and write tile_to_nation ownership map.
    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner_map[static_cast<std::size_t>(idx)];
        if (ni < 0)
            continue;

        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            continue;

        nation_data[static_cast<std::size_t>(ni)].tiles.push_back(tid);
        // tile_to_nation is written below once we have the entity IDs.
    }

    // --- Pass 7: starting treasury (BL-975) ----------------------------------
    // The per-seed credits Pass 2d placed, gathered onto the nation each seed
    // ENDED UP in — `owner_map[seeds[si]]`, the same read Pass 5 uses for a
    // nation's tongue and capital — so a seed the size-floor merge absorbed
    // hands its chest to its absorber rather than to nobody. Summed as
    // integers in ascending seed order, converted ONCE per nation through the
    // one stated per-mille (NATION_GENERATION.md § Pass 7 owns the figure),
    // and floored: a nation no polity's chest reached starts on
    // `treasury_floor`. This is what `seed_nation_garrisons` reads.
    {
        std::vector<int64_t> nation_credit(static_cast<std::size_t>(nation_count), 0);
        for (std::size_t si = 0; si < seeds.size(); ++si)
        {
            const int ni = owner_map[static_cast<std::size_t>(seeds[si])];
            if (ni < 0 || ni >= nation_count) continue;
            nation_credit[static_cast<std::size_t>(ni)] += seed_credit[si];
        }
        for (int ni = 0; ni < nation_count; ++ni)
        {
            const double credited =
                static_cast<double>(nation_credit[static_cast<std::size_t>(ni)])
                * static_cast<double>(params.treasury_credit_per_mille) / 1000.0;
            nation_data[static_cast<std::size_t>(ni)].treasury =
                std::max(params.treasury_floor, static_cast<float>(credited));
        }
    }

    // --- Pass 3: resource profile derivation ---
    derive_resource_profiles(owner_map, tile_ids, w, gw, gh, nation_data);

    // --- Pass 4: political character ---
    std::mt19937 pol_rng(seed_pol);
    for (int ni = 0; ni < nation_count; ++ni)
        assign_political_character(nation_data[static_cast<std::size_t>(ni)], pol_rng);

    // --- Pass 5: naming ---
    // A nation speaks the tongue of the region its seed grew from: walk the
    // seeds in order and give each surviving nation the speech of the LOWEST-
    // indexed seed still inside it (a seed absorbed by Pass 2c contributes
    // nothing — the surviving core names the realm). Ties broken on the lowest
    // index, the same rule every other selection in the layer uses.
    //
    // BL-571 (nation garrisons) piggybacks on the SAME walk for `capital_tile`:
    // the lowest-indexed surviving seed for a nation IS "the region it grew
    // from" that the comment above already names, restated as a tile rather
    // than a tongue. Unlike the speech pick this one is unconditional (no
    // "usable()" gate — every surviving seed has a real tile), so it is set on
    // the FIRST si whose owner is this nation, full stop.
    std::mt19937 name_rng(seed_name);
    std::vector<tongue> nation_speech(static_cast<std::size_t>(nation_count));
    for (std::size_t si = 0; si < seeds.size(); ++si)
    {
        const int ni = owner_map[static_cast<std::size_t>(seeds[si])];
        if (ni < 0 || ni >= nation_count) continue;
        if (nation_data[static_cast<std::size_t>(ni)].capital_tile == null_entity)
            nation_data[static_cast<std::size_t>(ni)].capital_tile =
                tile_ids[static_cast<std::size_t>(seeds[si])];
        if (si >= seed_speech.size() || !seed_speech[si].usable()) continue;
        if (!nation_speech[static_cast<std::size_t>(ni)].usable())
            nation_speech[static_cast<std::size_t>(ni)] = seed_speech[si];
    }

    // A body with no culture layer (no settlement pass, or a caller that
    // supplied bare seed tiles) still gets ONE coherent tongue rather than a
    // per-nation re-roll — an unwritten history is still a shared one.
    tongue fallback;
    {
        mt_picker pick(name_rng);
        fallback = roll_tongue(pick);
    }

    for (int ni = 0; ni < nation_count; ++ni)
    {
        const tongue& t = nation_speech[static_cast<std::size_t>(ni)].usable()
                              ? nation_speech[static_cast<std::size_t>(ni)]
                              : fallback;
        nation_data[static_cast<std::size_t>(ni)].name =
            make_nation_name(name_rng, t, coin_lexicon(t));
    }

    // --- Register nations in the world and write ownership maps ---
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(static_cast<std::size_t>(nation_count));

    for (int ni = 0; ni < nation_count; ++ni)
    {
        const entity_id nid = w.create_entity();
        nation_ids.push_back(nid);
        w.nations[nid] = std::move(nation_data[static_cast<std::size_t>(ni)]);
    }

    // BL-305: the ids exist now, so the loading screen can stop colouring the
    // carve by index and colour it by the same key the Country lens uses. Ids
    // come out of one tight create_entity loop, so index i is base + i.
    if (progress && !nation_ids.empty())
        progress->nation_id_base.store(static_cast<uint32_t>(nation_ids.front()),
                                       std::memory_order_relaxed);

    // Write tile_to_nation entries now that entity IDs are known.
    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner_map[static_cast<std::size_t>(idx)];
        if (ni < 0)
            continue;
        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            continue;
        w.tile_to_nation[tid] = nation_ids[static_cast<std::size_t>(ni)];
    }

    // --- Pass 6: substrate density (BL-050) ---
    // For each nation-owned tile, find the nearest population centre on the same
    // body and compute a density ripple. Tiles on bodies with no centres are left
    // at substrate_density = 0. Purely a rendering field now (the Industry lens,
    // body_surface_canvas.cpp) — BL-365 replaced the nation_substrate aggregate
    // this used to also feed with real background firms (corporation_generation.cpp
    // generate_background_firms), so nothing downstream reads density for market
    // injection any more.
    constexpr float ripple_radius = 8.0f;

    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner_map[static_cast<std::size_t>(idx)];
        if (ni < 0)
            continue;

        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            continue;

        auto tile_it = w.tiles.find(tid);
        if (tile_it == w.tiles.end())
            continue;

        tile_component& tc      = tile_it->second;
        const entity_id body_id = tc.body;
        const int       tx      = tc.grid_x;
        const int       ty      = tc.grid_y;

        // Find nearest population centre on the same body.
        float     best_density  = 0.0f;
        for (const auto& [cid, cc] : w.population_centres)
        {
            const auto ct_it = w.population_centre_tile.find(cid);
            if (ct_it == w.population_centre_tile.end())
                continue;

            const auto ct2 = w.tiles.find(ct_it->second);
            if (ct2 == w.tiles.end() || ct2->second.body != body_id)
                continue;

            const int dx   = std::abs(tx - ct2->second.grid_x);
            const int dy   = std::abs(ty - ct2->second.grid_y);
            const int dist = std::max(dx, dy); // Chebyshev
            const float density = std::max(0.0f, 1.0f - static_cast<float>(dist) / ripple_radius)
                                  * cc.scale;
            if (density > best_density)
                best_density = density;
        }

        tc.substrate_density = best_density;
    }

    return nation_ids;
}

// ---------------------------------------------------------------------------
// BL-571 — nation garrisons
// ---------------------------------------------------------------------------

namespace {

/// The odd-r hex neighbour of @p tile on @p side (0..5), or `null_entity` when
/// the step leaves the grid. Columns WRAP (the body is a cylinder); rows do
/// not — the same rule every other hex-grid consumer in this layer follows
/// (province.cpp, nation_ai.cpp).
///
/// DUPLICATED rather than reused: `nation_ai.cpp` carries the same ~15 lines
/// in its own anonymous namespace, unreachable from here, and that file is a
/// concurrent sibling item's territory this session (Sprint 16 Wave 2). Both
/// copies read off the one shared table in hex_neighbors.hpp, so they cannot
/// diverge on the geometry itself.
entity_id garrison_neighbour_tile(world& w, entity_id tile, int side)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return null_entity;
    const auto bit = w.bodies.find(tit->second.body);
    if (bit == w.bodies.end())
        return null_entity;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0)
        return null_entity;

    const hex_neighbors::coord c =
        hex_neighbors::neighbour(tit->second.grid_x, tit->second.grid_y, side);
    if (c.gy < 0 || c.gy >= gh)
        return null_entity;
    int nx = c.gx % gw;
    if (nx < 0)
        nx += gw;

    const std::vector<entity_id>& grid = body_tile_grid(w, tit->second.body);
    const std::size_t idx = static_cast<std::size_t>(c.gy) * static_cast<std::size_t>(gw)
                           + static_cast<std::size_t>(nx);
    return idx < grid.size() ? grid[idx] : null_entity;
}

/// Create the one static garrison unit for @p nation in @p province_id, on
/// the province's own anchor tile (`province::tiles.front()`, which IS
/// `province::id` by that struct's own contract — province.hpp). No-op on a
/// province id the partition does not resolve (defensive; provinces are
/// never empty by the partition's own contract, so this should not fire).
void place_garrison(world& w, entity_id nation, uint32_t province_id, int count,
                    uint16_t roster_row)
{
    const province* pr = w.provinces.find(province_id);
    if (pr == nullptr || pr->tiles.empty())
        return;

    const entity_id u = w.create_entity();
    unit_component uc{};
    uc.owner                  = nation;               // a nation, not a corp — the fourth writer
    uc.position                = pr->tiles.front();
    uc.count                   = count;
    uc.type                    = roster_row;
    uc.supply_factor_permille  = 1000;
    // uc.order stays default (dest == null_entity): STATIC by ruling — "a
    // nation that wants ground HIRES; a garrison that marched would make the
    // nation its own mercenary" (MILITARY.md § Nation garrisons). No code
    // path may ever hand this unit a march order.
    // uc.muster_base stays null_entity: "a nation raises no base" (same
    // section) — this is the orphan key run_unit_upkeep reads for a CORP
    // unit, and a garrison is deliberately never checked against it (see
    // run_unit_upkeep's own nation-owner branch, economy_system.cpp).
    w.units[u] = uc;
}

} // namespace

void seed_nation_garrisons(world& w, const nation_garrison_params& params)
{
    if (w.nations.empty() || w.provinces.provinces.empty())
        return; // no nations, or called before the partition exists

    // Ascending nation id — `w.nations` is an unordered_map.
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& kv : w.nations)
        nation_ids.push_back(kv.first);
    std::sort(nation_ids.begin(), nation_ids.end());

    const nation_ai_params grudge_params{}; // the scorer's own defaults — this
                                             // item authors no tuning of its own

    for (const entity_id nid : nation_ids)
    {
        const nation_component& nc = w.nations.at(nid);

        // --- Sizing: treasury-scaled against the value anchor (BL-543) ------
        // See nation_garrison_params' own comments for why `count_per_credit`
        // is a placeholder rather than a measured figure. The treasury read
        // here is REAL since BL-975: `generate_nations` Pass 7 credits each
        // nation with its folded polities' 1660 chest (NATION_GENERATION.md
        // § Pass 7), so a realm that arrived rich garrisons more than one
        // that arrived on the floor.
        const int count = std::clamp(
            params.min_count
                + static_cast<int>(std::lround(nc.treasury * params.count_per_credit)),
            params.min_count, params.max_count);

        // --- The highest-grudge neighbour -------------------------------------
        // Candidate neighbours: nations whose territory touches this one's on
        // at least one tile edge, gathered into a std::set so the argmax below
        // walks ascending id and needs no separate tie-break sort — the first
        // strict `>` to reach a value keeps it, exactly the idiom
        // seed_province_holders uses for its own plurality tie-break.
        std::set<entity_id> candidates;
        for (const entity_id tile : nc.tiles)
            for (int side = 0; side < 6; ++side)
            {
                const entity_id nb = garrison_neighbour_tile(w, tile, side);
                if (nb == null_entity)
                    continue;
                const auto tnit = w.tile_to_nation.find(nb);
                if (tnit == w.tile_to_nation.end() || tnit->second == nid)
                    continue;
                candidates.insert(tnit->second);
            }

        entity_id best_neighbour = null_entity;
        float      best_grudge   = -1.0f;
        for (const entity_id other : candidates) // ascending: strict '>' keeps the lowest tie
        {
            const float g = nation_grudge(w, nid, other, grudge_params);
            if (g > best_grudge)
            {
                best_grudge   = g;
                best_neighbour = other;
            }
        }

        // --- Target provinces: capital, plus border with the neighbour above -
        // ONE set, so a capital that also happens to sit on that border gets
        // exactly one garrison rather than two — the doc names two SOURCES of
        // a garrisoned province, not a guarantee they are disjoint.
        std::set<uint32_t> target_provinces;

        if (nc.capital_tile != null_entity)
        {
            const uint32_t cap_prov = w.provinces.province_of(nc.capital_tile);
            if (cap_prov != 0)
                target_provinces.insert(cap_prov);
        }

        if (best_neighbour != null_entity)
        {
            // A province of THIS nation (by `province_holder_for`, BL-569 —
            // consumed here exactly as the item's brief names) is a border
            // province with `best_neighbour` iff one of its tiles is
            // hex-adjacent to a tile `best_neighbour` holds.
            for (const province& pr : w.provinces.provinces)
            {
                if (province_holder_for(w, pr.id) != nid)
                    continue;
                bool touches = false;
                for (const entity_id tile : pr.tiles)
                {
                    for (int side = 0; side < 6 && !touches; ++side)
                    {
                        const entity_id nb = garrison_neighbour_tile(w, tile, side);
                        if (nb == null_entity)
                            continue;
                        const auto tnit = w.tile_to_nation.find(nb);
                        if (tnit != w.tile_to_nation.end() && tnit->second == best_neighbour)
                            touches = true;
                    }
                    if (touches)
                        break;
                }
                if (touches)
                    target_provinces.insert(pr.id);
            }
        }

        for (const uint32_t prov : target_provinces) // ascending: deterministic creation order
            place_garrison(w, nid, prov, count, params.roster_row);
    }
}

int64_t garrison_strength_in(const world& w, uint32_t province_id)
{
    if (province_id == 0)
        return 0;
    int64_t total = 0;
    for (const auto& kv : w.units)
    {
        const unit_component& u = kv.second;
        if (u.count <= 0 || u.position == null_entity)
            continue;
        if (w.nations.find(u.owner) == w.nations.end())
            continue; // a corp-owned unit
        if (w.provinces.province_of(u.position) != province_id)
            continue;
        total += unit_strength(w, u);
    }
    return total;
}
