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
#include <map>
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
                                  const std::vector<float>& step_cost,
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

            // The tile's landform cost (`nation_carve_ground::step_cost`,
            // BL-1278: read once per tile up front, the same value).
            const float cost_step = step_cost[static_cast<std::size_t>(nidx)];

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
/// BL-1089 — WHAT WAS ABSORBED INTO WHAT. @p absorbed_into, when given, is
/// sized to `seed_count` and receives per seed index the index it was folded
/// into (the absorber at that step; an absorber absorbed later is followed by
/// the caller), or -1 for a survivor. @p remap_out receives the compaction:
/// seed index -> nation index, -1 for an absorbed seed. Both are pure READS of
/// the walk — writing them changes no branch.
///
/// @return the final nation count (distinct nations after compaction).
int merge_undersized_nations(std::vector<int>& owner_map, int seed_count,
                             int min_tiles, int gw, int gh,
                             const std::vector<bool>& exempt,
                             std::vector<int>* absorbed_into = nullptr,
                             std::vector<int>* remap_out = nullptr,
                             const std::vector<char>* water = nullptr)
{
    if (absorbed_into) absorbed_into->assign(static_cast<std::size_t>(seed_count), -1);
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

        // Island with no land neighbour: absorb into the NEAREST nation across
        // the water (BL-1310, NATION_GENERATION.md § Pass 2c / § Pass 2e:
        // "never the globally largest"), by water tiles, ties to the lower
        // index then the lower tile. With no water mask (a world the strait
        // rule does not reach) the pre-BL-1310 globally-largest rule stands.
        if (best < 0 && water != nullptr)
        {
            std::vector<int>  land_owner(owner_map);
            std::vector<char> eligible(static_cast<std::size_t>(seed_count), 0);
            std::vector<int>  rank(static_cast<std::size_t>(seed_count));
            for (int idx = 0; idx < total; ++idx)
                if ((*water)[static_cast<std::size_t>(idx)]) land_owner[static_cast<std::size_t>(idx)] = -1;
            for (int ni = 0; ni < seed_count; ++ni)
            {
                rank[static_cast<std::size_t>(ni)] = ni;
                eligible[static_cast<std::size_t>(ni)] =
                    (active[static_cast<std::size_t>(ni)] && ni != small && !is_exempt(ni)) ? 1 : 0;
            }
            best = nearest_key_across_water(land_owner, *water, gw, gh, small, eligible, rank);
        }
        // Island with no land neighbour and no water mask (or no nation the
        // water reaches): absorb into the globally largest nation.
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
        if (absorbed_into) (*absorbed_into)[static_cast<std::size_t>(small)] = best; // BL-1089
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
    if (remap_out) *remap_out = remap; // BL-1089
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
        // BL-1318: capped as a realm's name is (tongue.hpp `cap_realm_name`).
        // The epithet / realm-word draw is made before the cap is read, so
        // `name_rng` advances exactly as it did uncapped.
        case 1:
            if (lex.qualifier.empty()) return name;
            return cap_realm_name(lex.qualifier[static_cast<std::size_t>(pick.pick(
                                      static_cast<int>(lex.qualifier.size())))] + " " + name,
                                  name);
        default: // 2
            if (lex.polity.empty()) return name;
            return cap_realm_name(name + " " + lex.polity[static_cast<std::size_t>(pick.pick(
                                      static_cast<int>(lex.polity.size())))],
                                  name);
    }
}

/// BL-1278 — PASSES 1b, 2, 2b AND THE WATER, shared by `generate_nations`
/// and `preview_nation_carve` so the closing rounds' carve and the real one
/// are one code path, not two kept in step. @p progress is the BL-305 tap
/// (null for a preview); `begin_carve` sits where it always sat, after the
/// weights are drawn.
std::vector<int> carve_from_seeds(const std::vector<int>& seeds,
                                  const std::vector<bool>& unclaimable,
                                  const nation_carve_ground& ground,
                                  uint32_t seed,
                                  generation_progress* progress)
{
    const int gw = ground.gw, gh = ground.gh;
    const std::vector<terrain_substrate>& sub = ground.sub;
    const uint32_t seed_expand = seed ^ 0x1F2E3D4Cu;
    const uint32_t seed_weight = seed ^ 0x6D2B79F5u;
    const int seed_count = static_cast<int>(seeds.size());

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
        seeds, unclaimable, ground.step_cost, gw, gh, weights, expand_rng, progress);

    // --- Pass 2b: orphan-island assignment (claim sea-disconnected ground) ---
    assign_orphan_islands(owner_map, unclaimable, gw, gh);

    // --- Water takes its owner from the shore (NR-792; between 2b and 2d) --
    // Not a numbered pass of NATION_GENERATION.md (the "Pass 2c" this label
    // once wore is the size floor below, and one name for two passes was the
    // stale-comment fix sprint 47's rulings named). Must run AFTER the land
    // carve and orphan islands (its sources are owned land tiles) and BEFORE
    // the polity fold, so a water tile is remapped by the fold exactly as the
    // shore it was derived from is.
    derive_water_ownership(owner_map, sub, gw, gh);
    return owner_map;
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
    generation_progress* progress,
    nation_fold_record* fold_out)
{
    const int total = gw * gh;

    // Distinct seed offsets keep each pass's RNG stream independent.
    const uint32_t seed_seeds  = seed ^ 0xA3B4C5D6u;
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

    // --- Passes 1b, 2, 2b and the water (BL-1278: one shared path) ---
    const nation_carve_ground ground = nation_carve_ground_of(w, tile_ids, gw, gh);
    std::vector<int> owner_map = carve_from_seeds(seeds, unclaimable, ground, seed, progress);

    // BL-1310 — the ground a cession moved (NATION_GENERATION.md § Pass 2e).
    // `ceded` marks every land tile whose holder the strait rule changed, here
    // (the closing rounds' relabels, by the receiving region's anchor) or in
    // the tile-level pass after the fold: a seed standing on ceded ground is
    // no longer its nation's capital or tongue (§ Pass 2e: a cession moves
    // ground, never a name).
    std::vector<char> water(static_cast<std::size_t>(total), 0);
    for (int idx = 0; idx < total; ++idx)
        water[static_cast<std::size_t>(idx)] = unclaimable[static_cast<std::size_t>(idx)] ? 1 : 0;
    std::vector<char> ceded(static_cast<std::size_t>(total), 0);
    // The provinces the closing rounds ceded AS REGIONS (the main kind: their
    // polity changed, so the fold already puts them with the receiver) are
    // ceded ground too (BL-1310 review): their seeds seat and name nobody.
    for (const int t : params.ceded_seed_tiles)
        if (t >= 0 && t < total) ceded[static_cast<std::size_t>(t)] = 1;
    if (!params.cell_overrides.empty())
    {
        std::vector<int> seed_at(static_cast<std::size_t>(total), -1);
        for (int si = 0; si < seed_count; ++si) seed_at[static_cast<std::size_t>(seeds[static_cast<std::size_t>(si)])] = si;
        bool any = false;
        for (const auto& [tile, anchor] : params.cell_overrides)
        {
            if (tile < 0 || tile >= total || anchor < 0 || anchor >= total) continue;
            if (water[static_cast<std::size_t>(tile)]) continue;
            const int si = seed_at[static_cast<std::size_t>(anchor)];
            if (si < 0 || owner_map[static_cast<std::size_t>(tile)] == si) continue;
            owner_map[static_cast<std::size_t>(tile)] = si;
            ceded[static_cast<std::size_t>(tile)] = 1;
            any = true;
        }
        if (any) rederive_carve_water(owner_map, ground.sub, gw, gh);
    }
    // The carve's cells (seed per tile) as they stand before the fold folds
    // them into nations: a tail cession names the seed its ground grew from.
    const std::vector<int> cell_seed = owner_map;

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
    // BL-975 — THE CHEST CROSSES WITH THE FLAG. Parallel to `seeds`: the
    // last close's (1660, or 1960 when the Industrialisation span ran: BL-1053)
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

            // BL-975: each polity's closing treasury lands on its representative
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

    // --- Pass 2e: ONE TERRITORY, the strait rule (BL-1310) -------------------
    // NATION_GENERATION.md § Pass 2e (Ben, 2026-10-16): every nation's ground
    // is one territory -- land groups joined by land or by straits of at most
    // `strait_tiles` water tiles to the group holding its capital -- and any
    // other group is ceded to the nation around it, save the remote island.
    // After the fold and BEFORE the floor, so the floor judges territories as
    // the player will see them. Where the history's closing rounds ran, they
    // ceded at the history's grain and this pass finds nothing (a cession here
    // is drift, which the caller guards); where no history ran, this IS the
    // rule. EVERY LAUNCH OBEYS IT (Ben, 2026-10-17): a launch that closed
    // before Industrialisation (no closing round ran) runs it here too, and
    // the caller records each cession as an event of the last span that ran
    // (`nation_fold_record::cession_log`).
    //
    // THE CORE IS THE REALM'S SEAT (§ Pass 2d, Ben 2026-10-17), fixed here,
    // before the first cession, so ground a nation receives never becomes
    // its core: the closing rounds' own frozen core where they ran
    // (`polity_core_tiles`), else the seat tile the last close left
    // (`polity_seat_tiles`) -- or, where that is water or not the realm's
    // own after the fold, the realm's nearest land to it. A nation with no
    // realm: its representative seed. Pass 5 seats a realm's nation on this
    // very tile.
    const bool strait_rule_here = true;
    std::vector<int> seed_core(static_cast<std::size_t>(seed_count), -1); // per representative seed
    std::vector<char> core_off_speck(static_cast<std::size_t>(seed_count), 0); // NR-1080: the core moved off a speck
    std::vector<char> seat_fell_back(static_cast<std::size_t>(seed_count), 0); // NR-1080: seated on its nearest province, not its seat
    const auto polity_of_seed_early = [&](int si) {
        return (si >= 0 && si < static_cast<int>(seed_polity.size())) ? seed_polity[static_cast<std::size_t>(si)] : -1;
    };
    {
        std::vector<int> land_owner(owner_map);
        for (int idx = 0; idx < total; ++idx)
            if (water[static_cast<std::size_t>(idx)]) land_owner[static_cast<std::size_t>(idx)] = -1;
        for (int si = 0; si < seed_count; ++si)
        {
            seed_core[static_cast<std::size_t>(si)] = seeds[static_cast<std::size_t>(si)];
            const int p = si < static_cast<int>(seed_polity.size()) ? seed_polity[static_cast<std::size_t>(si)] : -1;
            if (p < 0) continue;
            if (static_cast<std::size_t>(p) < params.polity_core_tiles.size())
            {
                const int ct = params.polity_core_tiles[static_cast<std::size_t>(p)];
                if (ct >= 0 && ct < total && land_owner[static_cast<std::size_t>(ct)] == si)
                {
                    seed_core[static_cast<std::size_t>(si)] = ct;
                    continue;
                }
            }
            if (static_cast<std::size_t>(p) < params.polity_seat_tiles.size())
            {
                // The seat's own seed where it seeded and the nation holds it;
                // else the nation's nearest province -- the nearest of its own
                // seed tiles (`nearest_tile_among`, ties to the lower tile).
                const int st = params.polity_seat_tiles[static_cast<std::size_t>(p)];
                if (st < 0 || st >= total) continue;
                std::vector<int> provinces;
                for (int sj = 0; sj < seed_count; ++sj)
                {
                    const int t = seeds[static_cast<std::size_t>(sj)];
                    if (land_owner[static_cast<std::size_t>(t)] == si && !ceded[static_cast<std::size_t>(t)])
                        provinces.push_back(t);
                }
                const int near = nearest_tile_among(provinces, st, gw);
                if (near >= 0) seed_core[static_cast<std::size_t>(si)] = near;
                if (near >= 0 && near != st) seat_fell_back[static_cast<std::size_t>(si)] = 1;
            }
        }
        // A REALM UNDER THE SIZE FLOOR KEEPS THE LARGER PIECE (§ Pass 2e, Ben
        // 2026-10-18), on the map as it stands here -- before the first
        // cession. Where a nation's land (water excluded) is under
        // `min_nation_tiles`, its core is the larger of its seat's group and
        // its largest group holding one of its seeds, ties to the seat's group
        // then the lower seed; the core tile is that group's lowest seed, and
        // Pass 5 seats the nation on it. The closing rounds already read it
        // where they ran (`polity_core_tiles` wins above), so it is read here
        // only for a core the rounds did not fix.
        if (params.min_nation_tiles > 0)
            for (int si = 0; si < seed_count; ++si)
            {
                const int p = polity_of_seed_early(si);
                if (p >= 0 && static_cast<std::size_t>(p) < params.polity_core_tiles.size()
                    && params.polity_core_tiles[static_cast<std::size_t>(p)] == seed_core[static_cast<std::size_t>(si)])
                    continue;
                const int seat = seed_core[static_cast<std::size_t>(si)];
                if (seat < 0 || seat >= total || land_owner[static_cast<std::size_t>(seat)] != si) continue;
                int land = 0;
                for (int t = 0; t < total; ++t) land += land_owner[static_cast<std::size_t>(t)] == si ? 1 : 0;
                if (land >= params.min_nation_tiles) continue;
                std::vector<int> comp(static_cast<std::size_t>(total), -1), size;
                for (int s0 = 0; s0 < total; ++s0)
                {
                    if (land_owner[static_cast<std::size_t>(s0)] != si || comp[static_cast<std::size_t>(s0)] >= 0) continue;
                    const int id = static_cast<int>(size.size());
                    size.push_back(0);
                    std::vector<int> stack{ s0 };
                    comp[static_cast<std::size_t>(s0)] = id;
                    while (!stack.empty())
                    {
                        const int t = stack.back();
                        stack.pop_back();
                        ++size[static_cast<std::size_t>(id)];
                        std::pair<int, int> nbrs[4];
                        int n = 0;
                        cardinal_neighbours(t % gw, t / gw, gw, gh, nbrs, n);
                        for (int i = 0; i < n; ++i)
                        {
                            const int u = raster_idx(nbrs[i].first, nbrs[i].second, gw);
                            if (land_owner[static_cast<std::size_t>(u)] == si && comp[static_cast<std::size_t>(u)] < 0)
                            {
                                comp[static_cast<std::size_t>(u)] = id;
                                stack.push_back(u);
                            }
                        }
                    }
                }
                const int seat_comp = comp[static_cast<std::size_t>(seat)];
                int best_size = size[static_cast<std::size_t>(seat_comp)];
                int best_tile = seat;
                for (int sj = 0; sj < seed_count; ++sj) // ascending: a group's first seed is its lowest
                {
                    const int t = seeds[static_cast<std::size_t>(sj)];
                    if (land_owner[static_cast<std::size_t>(t)] != si || ceded[static_cast<std::size_t>(t)]) continue;
                    const int c = comp[static_cast<std::size_t>(t)];
                    if (c < 0 || c == seat_comp) continue;
                    if (size[static_cast<std::size_t>(c)] > best_size) // strictly: ties keep the seat's group
                    {
                        best_size = size[static_cast<std::size_t>(c)];
                        best_tile = t;
                    }
                }
                if (best_tile != seat)
                {
                    seed_core[static_cast<std::size_t>(si)] = best_tile;
                    if (fold_out) ++fold_out->cores_floor_moved;
                }
            }
        // NO CAPITAL ON A SPECK (NR-1080; § Pass 2e, Ben 2026-10-25), on the
        // same map, before the first cession: a core group under
        // `capital_speck_tiles` with a larger group of the nation's own
        // holding one of its seeds seats on its LARGEST such group, at the
        // seed nearest the old seat (ties to the lower tile). Every nation,
        // realm or not; the closing rounds already read it where they ran.
        for (int si = 0; si < seed_count; ++si)
        {
            const int p = polity_of_seed_early(si);
            if (p >= 0 && static_cast<std::size_t>(p) < params.polity_core_tiles.size()
                && params.polity_core_tiles[static_cast<std::size_t>(p)] == seed_core[static_cast<std::size_t>(si)])
                continue;
            const int seat = seed_core[static_cast<std::size_t>(si)];
            if (seat < 0 || seat >= total || land_owner[static_cast<std::size_t>(seat)] != si) continue;
            std::vector<int> provinces;
            for (int sj = 0; sj < seed_count; ++sj)
            {
                const int t = seeds[static_cast<std::size_t>(sj)];
                if (land_owner[static_cast<std::size_t>(t)] == si && !ceded[static_cast<std::size_t>(t)]) provinces.push_back(t);
            }
            int target = seat;
            if (p >= 0 && static_cast<std::size_t>(p) < params.polity_seat_tiles.size()
                && params.polity_seat_tiles[static_cast<std::size_t>(p)] >= 0)
                target = params.polity_seat_tiles[static_cast<std::size_t>(p)];
            // A capital moved to its nearest province may seat on any larger
            // group; a seat on its own ground only on one in its territory.
            const int moved = seat_off_speck(land_owner, gw, gh, si, seat, provinces, target,
                                             seat_fell_back[static_cast<std::size_t>(si)] ? nullptr : &water);
            if (moved != seat)
            {
                seed_core[static_cast<std::size_t>(si)]     = moved;
                core_off_speck[static_cast<std::size_t>(si)] = 1;
                if (fold_out) ++fold_out->cores_speck_moved;
            }
        }
    }
    const auto polity_of_seed = [&](int si) {
        return (si >= 0 && si < static_cast<int>(seed_polity.size())) ? seed_polity[static_cast<std::size_t>(si)] : -1;
    };
    // The seed a ceded group grew from: the lowest seed standing in it, else
    // the carve cell under its lowest tile.
    // A part NAMES ONLY A PROVINCE THAT MOVED (the review): the lowest seed
    // standing in it, or none -- never the cell under its lowest tile, which
    // could name a province that stayed put or went to another receiver.
    const auto ceded_seed_tile = [&](const std::vector<int>& tiles) -> int32_t {
        for (int si = 0; si < seed_count; ++si)
            if (std::binary_search(tiles.begin(), tiles.end(), seeds[static_cast<std::size_t>(si)]))
                return seeds[static_cast<std::size_t>(si)];
        return -1;
    };
    // UNITS FOR THE SPLIT (§ Pass 2e: "province by province on a settled
    // world, where a province never splits, and tile by tile where no history
    // ran"). A history ran -- an Empires- or Exploration-round launch, or the
    // guarantee after closing rounds -- when any seed carries a polity: then a
    // province is a seed's carve cell (`cell_seed`), anchored at its seed tile.
    // Where no history ran, every tile is its own unit.
    bool history_ran = false;
    for (const int p : seed_polity) history_ran = history_ran || p >= 0;
    const std::vector<int>* split_unit        = history_ran ? &cell_seed : nullptr;
    const std::vector<int>* split_unit_anchor = history_ran ? &seeds : nullptr;
    // A tail cession, applied: each tile to the key its part went to (the
    // split, § Pass 2e), and ONE logged record per receiving part, naming the
    // seed its ground grew from (the caller records each as an event).
    const auto apply_cession = [&](const exclave_cession& ex, std::vector<int>& land_owner,
                                   const std::vector<int>& cores, auto polity_of_key) {
        std::map<int, std::vector<int>> parts; // receiver -> its tiles, ascending
        for (std::size_t i = 0; i < ex.tiles.size(); ++i)
            parts[i < ex.part_key.size() ? ex.part_key[i] : ex.to].push_back(ex.tiles[i]);
        // THE ISLETS GO WITH THEIR PROVINCE (NR-1077; § Pass 2e, Ben
        // 2026-10-25): a moved seed's carve cell ground the cedent still holds
        // outside the exclave -- an islet Pass 2b hung on it -- goes to the
        // receiver of the part of that cell it hangs from. Read before the
        // checks below, which judge the exclave's own parts only.
        const int from_core = (ex.from >= 0 && static_cast<std::size_t>(ex.from) < cores.size())
                                  ? cores[static_cast<std::size_t>(ex.from)] : -1;
        const std::vector<carried_islet> islets =
            islets_carried_by_cession(land_owner, gw, gh, ex, cell_seed, seeds, from_core);
        // THE CHECKS (world_audit reads them through the fixture): every part
        // touches its receiver's land as it stood BEFORE the cession (so it
        // joins it), and on a settled world no province spans two parts.
        if (fold_out)
        {
            for (const auto& [to, tiles] : parts)
            {
                bool joined = false;
                for (const int t : tiles)
                {
                    std::pair<int, int> nbrs[4];
                    int n = 0;
                    cardinal_neighbours(t % gw, t / gw, gw, gh, nbrs, n);
                    for (int i = 0; i < n && !joined; ++i)
                        joined = land_owner[static_cast<std::size_t>(raster_idx(nbrs[i].first, nbrs[i].second, gw))] == to;
                    if (joined) break;
                }
                if (!joined && ex.kind == 0) ++fold_out->parts_detached; // the split; an island joins across a strait (kind 1) or is remote (kind 2)
            }
            if (split_unit != nullptr)
                for (int si = 0; si < seed_count; ++si)
                {
                    if (!std::binary_search(ex.tiles.begin(), ex.tiles.end(), seeds[static_cast<std::size_t>(si)])) continue;
                    int key = -2;
                    for (std::size_t i = 0; i < ex.tiles.size(); ++i)
                    {
                        if (cell_seed[static_cast<std::size_t>(ex.tiles[i])] != si) continue;
                        if (key == -2) key = ex.part_key[i];
                        else if (ex.part_key[i] != key) { ++fold_out->provinces_split; break; }
                    }
                }
            if (parts.size() > 1) ++fold_out->cessions_split;
        }
        for (const carried_islet& isl : islets)
        {
            std::vector<int>& pt = parts[isl.to];
            pt.insert(pt.end(), isl.tiles.begin(), isl.tiles.end());
            std::sort(pt.begin(), pt.end());
            if (fold_out) fold_out->islet_tiles_carried += static_cast<int32_t>(isl.tiles.size());
        }
        for (const auto& [to, tiles] : parts)
            for (const int t : tiles)
            {
                land_owner[static_cast<std::size_t>(t)] = to;
                owner_map[static_cast<std::size_t>(t)]  = to;
                ceded[static_cast<std::size_t>(t)]      = 1;
            }
        if (fold_out)
            for (const auto& [to, tiles] : parts)
                fold_out->cession_log.push_back({ polity_of_key(ex.from), polity_of_key(to), ceded_seed_tile(tiles) });
    };
    // The map AT THE CLOSE, before any cession: the city states are judged on
    // it (Ben, 2026-10-18), not on what the strait rule left a realm.
    const std::vector<int> owner_at_close = owner_map;
    int tail_cessions = 0;
    {
        std::vector<int> land_owner(owner_map);
        for (int idx = 0; idx < total; ++idx)
            if (water[static_cast<std::size_t>(idx)]) land_owner[static_cast<std::size_t>(idx)] = -1;
        std::vector<int> rank(static_cast<std::size_t>(seed_count));
        for (int si = 0; si < seed_count; ++si) rank[static_cast<std::size_t>(si)] = si;
        exclave_cession ex;
        for (int guard = 0; guard < total; ++guard)
        {
            if (!next_exclave_cession(land_owner, water, gw, gh, seed_core, rank, ex, split_unit, split_unit_anchor)) break;
            apply_cession(ex, land_owner, seed_core, [&](int key) { return polity_of_seed(key); });
            ++tail_cessions;
        }
        if (tail_cessions > 0) rederive_carve_water(owner_map, sub, gw, gh);
    }
    if (fold_out) fold_out->cessions = tail_cessions;

    // --- Pass 2c: light "in history" merges — absorb anything below the size floor ---
    // No target count: the loop stops when every survivor holds a viable territory,
    // so the final nation count is whatever the landmass and coastline produce.
    //
    // BL-769: the floor now judges POLITY-GRAIN territories, because the fold
    // ran first — so a realm is absorbed for being a small realm, not for being
    // one province of a large one.
    // BL-1278: after a history whose closing rounds ran the floor, nothing is
    // left for it to do here, and running it again would move the count the
    // closing rounds planned (the drift guard in `run_tail` holds them equal).
    const bool floor_here = params.min_nation_tiles > 0 && !params.floor_in_history;
    const std::vector<bool> city_states =
        (params.keep_city_states && floor_here)
            ? mark_city_states(w, tile_ids, owner_at_close, fold, seed_polity, seed_count, total)
            : std::vector<bool>{};

    // BL-1089: the merge's own record of who folded into whom, and the
    // compaction, so Pass 5 can follow a nation back to the SURVIVING seed —
    // the fold representative that kept its identity through the floor.
    std::vector<int> absorbed_into(static_cast<std::size_t>(seed_count), -1);
    std::vector<int> seed_to_nation(static_cast<std::size_t>(seed_count));
    for (int si = 0; si < seed_count; ++si) seed_to_nation[static_cast<std::size_t>(si)] = si;

    int nation_count = seed_count;
    // BL-1278: with the floor run in the history the merge is called at a
    // floor of 0 — it absorbs nothing and only COMPACTS the fold's
    // representative indices, which the nations below are numbered by.
    if (floor_here || params.floor_in_history)
        nation_count = merge_undersized_nations(owner_map, seed_count,
                                                floor_here ? params.min_nation_tiles : 0,
                                                gw, gh, city_states, &absorbed_into, &seed_to_nation,
                                                strait_rule_here ? &water : nullptr);

    // BL-1310 review — THE REPAIR AFTER THE FLOOR, where no history ran (the
    // closing rounds' late repair, the same rule on the same footing). The
    // merge reads a neighbour through owned water, and a lake is owned whole
    // by one shore, so an absorption can join a realm farther than a strait:
    // whatever that leaves outside the rule is ceded here. Ground only; the
    // count the floor left stands. Cores are the absorbers' (merge rule A).
    if (floor_here && strait_rule_here)
    {
        std::vector<int> land_owner(owner_map);
        for (int idx = 0; idx < total; ++idx)
            if (water[static_cast<std::size_t>(idx)]) land_owner[static_cast<std::size_t>(idx)] = -1;
        std::vector<int> core_tile(static_cast<std::size_t>(nation_count), -1), rank(static_cast<std::size_t>(nation_count));
        for (int ni = 0; ni < nation_count; ++ni) rank[static_cast<std::size_t>(ni)] = ni;
        // The survivor seed of each nation (merge rule A: the absorber), and
        // its seat core fixed before the first cession above.
        std::vector<int> survivor(static_cast<std::size_t>(nation_count), -1);
        for (int si = 0; si < seed_count; ++si)
        {
            const int ni = seed_to_nation[static_cast<std::size_t>(si)];
            if (ni >= 0 && ni < nation_count && survivor[static_cast<std::size_t>(ni)] < 0)
            {
                survivor[static_cast<std::size_t>(ni)]  = si;
                core_tile[static_cast<std::size_t>(ni)] = seed_core[static_cast<std::size_t>(si)];
            }
        }
        // NO CAPITAL ON A SPECK, after the floor too (NR-1080; § Pass 2e, Ben
        // 2026-10-25): an absorber seated on a speck that took a larger
        // province group with the realm it absorbed seats on its largest
        // group, at the seed nearest its old seat (ties to the lower tile).
        for (int ni = 0; ni < nation_count; ++ni)
        {
            const int si = survivor[static_cast<std::size_t>(ni)];
            const int ct = core_tile[static_cast<std::size_t>(ni)];
            if (si < 0 || ct < 0 || ct >= total || land_owner[static_cast<std::size_t>(ct)] != ni) continue;
            std::vector<int> provinces;
            for (int sj = 0; sj < seed_count; ++sj)
            {
                const int t = seeds[static_cast<std::size_t>(sj)];
                if (land_owner[static_cast<std::size_t>(t)] == ni && !ceded[static_cast<std::size_t>(t)]) provinces.push_back(t);
            }
            const int moved = seat_off_speck(land_owner, gw, gh, ni, ct, provinces, ct, &water);
            if (moved == ct) continue;
            core_tile[static_cast<std::size_t>(ni)]      = moved;
            seed_core[static_cast<std::size_t>(si)]      = moved;
            core_off_speck[static_cast<std::size_t>(si)] = 1;
            if (fold_out) ++fold_out->cores_speck_moved;
        }
        exclave_cession ex;
        int repaired = 0;
        for (int guard = 0; guard < total; ++guard)
        {
            if (!next_exclave_cession(land_owner, water, gw, gh, core_tile, rank, ex, split_unit, split_unit_anchor)) break;
            apply_cession(ex, land_owner, core_tile,
                          [&](int key) { return polity_of_seed(survivor[static_cast<std::size_t>(key)]); });
            ++repaired;
        }
        if (repaired > 0) rederive_carve_water(owner_map, sub, gw, gh);
        tail_cessions += repaired;
        if (fold_out) fold_out->cessions = tail_cessions;
    }

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

    // BL-1089 — EACH NATION'S REPRESENTATIVE SEED: the SURVIVING seed of its
    // index. After the fold that seed is a polity's founding core (or
    // ownerless ground); after the size floor it is the ABSORBER's core,
    // never an absorbed seed's, whatever index the absorbed one carried
    // (merge rule A). One read, three consumers: the carve's colours below,
    // Pass 5's name, and the fold record.
    std::vector<int> nation_rep_seed(static_cast<std::size_t>(nation_count), -1);
    for (int si = 0; si < seed_count; ++si)
    {
        const int ni = seed_to_nation[static_cast<std::size_t>(si)];
        if (ni < 0 || ni >= nation_count) continue;
        if (nation_rep_seed[static_cast<std::size_t>(ni)] < 0) nation_rep_seed[static_cast<std::size_t>(ni)] = si;
    }
    const auto seed_polity_of = [&](int si) -> int {
        return (si >= 0 && static_cast<std::size_t>(si) < seed_polity.size())
                   ? seed_polity[static_cast<std::size_t>(si)] : -1;
    };
    if (progress)
    {
        std::vector<int32_t> by_nation(static_cast<std::size_t>(nation_count), -1);
        for (int ni = 0; ni < nation_count; ++ni)
            by_nation[static_cast<std::size_t>(ni)] = seed_polity_of(nation_rep_seed[static_cast<std::size_t>(ni)]);
        progress->publish_nation_polities(by_nation);
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
    // BL-1310: THE REALM'S SEAT IS THE CAPITAL (§ Pass 2d, Ben 2026-10-17).
    // A nation of a realm is seated on its core -- the realm's seat, fixed
    // before the first cession (`seed_core`; the absorber's after the floor,
    // merge rule A). A nation with no realm is seated on its LOWEST SEED
    // INSIDE ITS TERRITORY (the walk below), so a lower seed on a remote
    // island, or on ground an island absorption left beyond the strait,
    // seats nobody.
    std::vector<char> in_territory;
    if (strait_rule_here)
    {
        std::vector<int> land_owner(owner_map);
        for (int idx = 0; idx < total; ++idx)
            if (water[static_cast<std::size_t>(idx)]) land_owner[static_cast<std::size_t>(idx)] = -1;
        std::vector<int> core_tile(static_cast<std::size_t>(nation_count), -1);
        for (int ni = 0; ni < nation_count; ++ni)
        {
            const int rep = nation_rep_seed[static_cast<std::size_t>(ni)];
            if (rep < 0) continue;
            core_tile[static_cast<std::size_t>(ni)] = seeds[static_cast<std::size_t>(rep)];
            const int ct = seed_core[static_cast<std::size_t>(rep)];
            if (ct >= 0 && ct < total && land_owner[static_cast<std::size_t>(ct)] == ni)
                core_tile[static_cast<std::size_t>(ni)] = ct;
            // A realm's nation sits on its core; so does a nation with no
            // realm whose core moved off a speck (NR-1080), else the walk
            // below would seat it back on its lowest seed.
            if (seed_polity_of(rep) >= 0 || core_off_speck[static_cast<std::size_t>(rep)])
            {
                const entity_id tid = tile_ids[static_cast<std::size_t>(core_tile[static_cast<std::size_t>(ni)])];
                if (tid != null_entity) nation_data[static_cast<std::size_t>(ni)].capital_tile = tid;
            }
        }
        in_territory = territory_tiles(land_owner, water, gw, gh, core_tile);
    }

    std::mt19937 name_rng(seed_name);
    std::vector<tongue> nation_speech(static_cast<std::size_t>(nation_count));
    for (std::size_t si = 0; si < seeds.size(); ++si)
    {
        const int ni = owner_map[static_cast<std::size_t>(seeds[si])];
        if (ni < 0 || ni >= nation_count) continue;
        // BL-1310: a seed on ceded ground left its realm with the ground; it
        // names and seats nobody (§ Pass 2e: a cession moves ground, never a name).
        if (ceded[static_cast<std::size_t>(seeds[si])]) continue;
        if (!in_territory.empty() && !in_territory[static_cast<std::size_t>(seeds[si])]) continue;
        if (nation_data[static_cast<std::size_t>(ni)].capital_tile == null_entity)
            nation_data[static_cast<std::size_t>(ni)].capital_tile =
                tile_ids[static_cast<std::size_t>(seeds[si])];
        if (si >= seed_speech.size() || !seed_speech[si].usable()) continue;
        if (!nation_speech[static_cast<std::size_t>(ni)].usable())
            nation_speech[static_cast<std::size_t>(ni)] = seed_speech[si];
    }
    // EVERY NATION IS SEATED (BL-1310 review). A nation all of whose seeds
    // stand on ground a cession once moved -- a successor that seceded with
    // provinces its parent had received -- has no seed the walk above
    // admits; ceded ground is then its own founding ground, so it is seated
    // on its lowest seed inside its territory (else its lowest seed), and
    // named in that seed's speech where it has none.
    for (int pass = 0; pass < 2; ++pass)
        for (std::size_t si = 0; si < seeds.size(); ++si)
        {
            const int ni = owner_map[static_cast<std::size_t>(seeds[si])];
            if (ni < 0 || ni >= nation_count) continue;
            if (nation_data[static_cast<std::size_t>(ni)].capital_tile != null_entity) continue;
            if (pass == 0 && !in_territory.empty() && !in_territory[static_cast<std::size_t>(seeds[si])]) continue;
            nation_data[static_cast<std::size_t>(ni)].capital_tile = tile_ids[static_cast<std::size_t>(seeds[si])];
            if (si < seed_speech.size() && seed_speech[si].usable() && !nation_speech[static_cast<std::size_t>(ni)].usable())
                nation_speech[static_cast<std::size_t>(ni)] = seed_speech[si];
        }
    // NO CAPITAL ON A SPECK, on the finished map (NR-1080; § Pass 2e, Ben
    // 2026-10-25). The cores above were read before the cessions and the
    // floor; ground received since may be the nation's largest province
    // group, joined to the capital's speck by a strait. The capital TILE moves
    // to the province nearest it in the largest such group within its own
    // territory (ties to the lower tile) -- territory, name and tongue are
    // unchanged, so no cession is reopened.
    if (strait_rule_here)
    {
        std::vector<int> land_owner(owner_map);
        for (int idx = 0; idx < total; ++idx)
            if (water[static_cast<std::size_t>(idx)]) land_owner[static_cast<std::size_t>(idx)] = -1;
        std::unordered_map<entity_id, int> raster_of;
        for (int idx = 0; idx < total; ++idx)
            if (tile_ids[static_cast<std::size_t>(idx)] != null_entity) raster_of[tile_ids[static_cast<std::size_t>(idx)]] = idx;
        for (int ni = 0; ni < nation_count; ++ni)
        {
            const auto it = raster_of.find(nation_data[static_cast<std::size_t>(ni)].capital_tile);
            if (it == raster_of.end()) continue;
            const int cap = it->second;
            if (land_owner[static_cast<std::size_t>(cap)] != ni) continue;
            std::vector<int> provinces;
            for (int sj = 0; sj < seed_count; ++sj)
            {
                const int t = seeds[static_cast<std::size_t>(sj)];
                if (land_owner[static_cast<std::size_t>(t)] == ni) provinces.push_back(t);
            }
            const int moved = seat_off_speck(land_owner, gw, gh, ni, cap, provinces, cap, &water);
            if (moved == cap || tile_ids[static_cast<std::size_t>(moved)] == null_entity) continue;
            nation_data[static_cast<std::size_t>(ni)].capital_tile = tile_ids[static_cast<std::size_t>(moved)];
            if (fold_out) ++fold_out->cores_speck_moved;
        }
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
        // Coined for EVERY nation, inherited or not, so `name_rng` is drawn
        // exactly as before BL-1089 and a nation of ownerless ground — the one
        // kind Pass 5 still names — keeps the name it always had. The draw is
        // discarded below for a nation with a founding realm.
        nation_data[static_cast<std::size_t>(ni)].name =
            make_nation_name(name_rng, t, coin_lexicon(t));
    }

    // BL-1089 — A NATION INHERITS ITS FOUNDING REALM'S NAME, VERBATIM
    // (NATION_GENERATION.md § Pass 5; Ben, 2026-09-24). The nation's
    // representative is the SURVIVING seed of its index — after the fold that
    // seed is a polity's founding core (or ownerless ground), and after the
    // size floor it is the ABSORBER's core, never an absorbed seed's, whatever
    // index the absorbed one carried (merge rule A). So the polity it names is
    // read off that one seed, and the realm's coined name is copied across.
    // The fold record (`fold_out`) is filled from the same reads
    // (`nation_rep_seed` / `seed_polity_of`, computed after the floor above).
    for (int ni = 0; ni < nation_count; ++ni)
    {
        const int rep = nation_rep_seed[static_cast<std::size_t>(ni)];
        const int pol = seed_polity_of(rep);
        if (pol < 0 || static_cast<std::size_t>(pol) >= params.polity_names.size()) continue;
        const std::string& realm = params.polity_names[static_cast<std::size_t>(pol)];
        if (!realm.empty()) nation_data[static_cast<std::size_t>(ni)].name = realm;
    }
    if (fold_out != nullptr)
    {
        fold_out->polity.assign(static_cast<std::size_t>(nation_count), -1);
        fold_out->absorbed_first.assign(static_cast<std::size_t>(nation_count), 0);
        fold_out->absorbed_count.assign(static_cast<std::size_t>(nation_count), 0);
        fold_out->absorbed.clear();
        for (int ni = 0; ni < nation_count; ++ni)
            fold_out->polity[static_cast<std::size_t>(ni)] = seed_polity_of(nation_rep_seed[static_cast<std::size_t>(ni)]);
        // Per absorbed REALM (a fold representative that the floor folded
        // away), the nation it ended up in: follow the absorber chain to the
        // survivor. Ascending seed order, which is absorption order for the
        // floor's own smallest-first walk only approximately — the order the
        // card lists them in is a presentation fact, and ascending index is a
        // deterministic one.
        for (int ni = 0; ni < nation_count; ++ni)
        {
            fold_out->absorbed_first[static_cast<std::size_t>(ni)] = static_cast<int32_t>(fold_out->absorbed.size());
            for (int si = 0; si < seed_count; ++si)
            {
                if (absorbed_into[static_cast<std::size_t>(si)] < 0) continue;   // a survivor
                if (fold[static_cast<std::size_t>(si)] != si) continue;           // not a representative
                const int pol = seed_polity_of(si);
                if (pol < 0) continue;                                            // ownerless ground names nothing
                int at = si;
                for (int guard = 0; guard < seed_count && absorbed_into[static_cast<std::size_t>(at)] >= 0; ++guard)
                    at = absorbed_into[static_cast<std::size_t>(at)];
                if (seed_to_nation[static_cast<std::size_t>(at)] != ni) continue;
                fold_out->absorbed.push_back(pol);
                ++fold_out->absorbed_count[static_cast<std::size_t>(ni)];
            }
        }
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
        // BL-1346: through the setter, which bumps the catchment stamp - a tile's owner
        // decides which centre pulls a fifth harder on it, and the home body's markets
        // are created BEFORE this pass runs (hard_coded_world.cpp), so a catchment
        // cached in that window would otherwise survive as a no-nation field.
        w.set_tile_nation(tid, nation_ids[static_cast<std::size_t>(ni)]);
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

    // BL-1072: this walk (owned tiles x every centre) is most of the pass --
    // 2-4 s Release on a 1960 world -- so the loading bar counts its rows.
    // Write-only; the walk never reads it back.
    for (int idx = 0; idx < total; ++idx)
    {
        if (progress != nullptr && gw > 0 && idx % gw == 0)
            progress->report_sub(idx / gw, gh);
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
    if (progress != nullptr && gh > 0)
        progress->report_sub(gh, gh);

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
/// @p post — the capital province's anchor centre, or a border province's
/// tile facing the neighbour it guards (seed_nation_garrisons says which,
/// Ben 2026-10-03). The lowest-id tile was the first post, and since BL-1150
/// a province's fill crosses the settled line, so that tile could be
/// never-settled wilderness far from anything the garrison holds. No-op on a
/// province id the partition does not resolve (defensive; provinces are
/// never empty by the partition's own contract, so this should not fire).
void place_garrison(world& w, entity_id nation, uint32_t province_id, entity_id post,
                    int count, uint16_t roster_row)
{
    const province* pr = w.provinces.find(province_id);
    if (pr == nullptr || pr->tiles.empty() || post == null_entity)
        return;

    const entity_id u = w.create_entity();
    unit_component uc{};
    uc.owner                  = nation;               // a nation, not a corp — the fourth writer
    uc.position                = post;
    uc.count                   = count;
    uc.establishment           = count; // BL-1421: the HP bar's full mark
    uc.type                    = roster_row;
    uc.supply_factor_permille  = 1000;
    // uc.order stays default (dest == null_entity): a garrison stands where it is
    // posted until it is assigned to a front (MILITARY.md § Nation garrisons and
    // § Fronts); only a front, through march_unit, ever moves it.
    // uc.muster_base stays null_entity HERE: no base stands yet. BL-1439 (Ben,
    // 2026-10-27): finish_campaign_world lays every capital's military base
    // (`seed_capital_military_bases`, corporation_generation.cpp) and the
    // CAPITAL garrison then records it as its muster base; a border garrison
    // keeps none. A garrison adopts its base rather than being raised there,
    // which `unit_is_raised` reads off the ids (the garrison is the older).
    w.units[u] = uc;
}

} // namespace

entity_id garrison_border_post(world& w, const province& pr, entity_id guarded)
{
    if (guarded == null_entity)
        return null_entity;
    // Ascending tile order (the partition's contract) with a strictly-greater
    // scan: the most sides facing `guarded` wins, the lowest tile id a tie.
    entity_id best       = null_entity;
    int       best_sides = 0;
    for (const entity_id tile : pr.tiles)
    {
        const auto own = w.tile_to_nation.find(tile);
        if (own != w.tile_to_nation.end() && own->second == guarded)
            continue; // a tile of the guarded nation is not a post against it
        int sides = 0;
        for (int side = 0; side < 6; ++side)
        {
            const entity_id nb = garrison_neighbour_tile(w, tile, side);
            if (nb == null_entity)
                continue;
            const auto tnit = w.tile_to_nation.find(nb);
            if (tnit != w.tile_to_nation.end() && tnit->second == guarded)
                ++sides;
        }
        if (sides > best_sides)
        {
            best_sides = sides;
            best       = tile;
        }
    }
    return best;
}

int seat_capitals_on_centres(world& w, int* unseated)
{
    if (unseated) *unseated = 0;
    if (w.nations.empty() || w.provinces.provinces.empty())
        return 0;
    // Summed centre scale per tile (the anchor's own measure; an integer sum, so the
    // unordered walk cannot reach the result).
    std::map<entity_id, int> scale_on;
    for (const auto& [cid, tid] : w.population_centre_tile)
        if (const auto pc = w.population_centres.find(cid); pc != w.population_centres.end()
            && pc->second.scale > 0)
            scale_on[tid] += pc->second.scale;
    std::vector<entity_id> ids;
    ids.reserve(w.nations.size());
    for (const auto& kv : w.nations) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    int moved = 0;
    for (const entity_id nid : ids)
    {
        nation_component& nc = w.nations.at(nid);
        const auto ct = w.tiles.find(nc.capital_tile);
        if (ct == w.tiles.end())
            continue;
        const auto bit = w.bodies.find(ct->second.body);
        if (bit == w.bodies.end())
            continue;
        const int gw = bit->second.grid_width, gh = bit->second.grid_height;
        const std::vector<entity_id>& grid = body_tile_grid(w, ct->second.body);
        const uint32_t prov = w.provinces.province_of(nc.capital_tile);
        // THE CAPITAL'S OWN LAND GROUP: the nation's dry tiles a 4-neighbour walk reaches
        // from the capital (columns wrap) - the strait rule's own group measure - so the
        // move never carries a capital onto an islet the seat province holds across water
        // (No capital on a speck, above). Ascending, for the pick below.
        std::set<entity_id> group{nc.capital_tile};
        std::vector<entity_id> q{nc.capital_tile};
        for (std::size_t head = 0; head < q.size(); ++head)
        {
            const tile_component& tc = w.tiles.at(q[head]);
            const int x = tc.grid_x, y = tc.grid_y;
            const int cand[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                                 y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
            for (const int c : cand)
            {
                if (c < 0 || static_cast<std::size_t>(c) >= grid.size()) continue;
                const entity_id nt = grid[static_cast<std::size_t>(c)];
                if (nt == null_entity || group.count(nt) != 0) continue;
                const auto on = w.tile_to_nation.find(nt);
                if (on == w.tile_to_nation.end() || on->second != nid || is_water(w.tiles.at(nt).substrate))
                    continue;
                group.insert(nt);
                q.push_back(nt);
            }
        }
        // The seat province's largest centre on that group; failing one, the group's.
        // Most summed scale, ties to the lower tile id (`province_anchors`' rule).
        const auto pick = [&](bool in_province) {
            entity_id best = null_entity;
            int best_scale = 0;
            for (const entity_id t : group)
            {
                const auto so = scale_on.find(t);
                if (so == scale_on.end() || is_water(w.tiles.at(t).substrate)) continue;
                if (in_province && w.provinces.province_of(t) != prov) continue;
                if (so->second > best_scale) { best = t; best_scale = so->second; }
            }
            return best;
        };
        entity_id city = pick(true);
        if (city == null_entity)
            city = pick(false);
        if (city == null_entity)
        {
            if (unseated) ++*unseated;
            continue;
        }
        if (city != nc.capital_tile)
        {
            nc.capital_tile = city;
            ++moved;
        }
    }
    return moved;
}

void seed_nation_garrisons(world& w, const nation_garrison_params& params)
{
    if (w.nations.empty() || w.provinces.provinces.empty())
        return; // no nations, or called before the partition exists

    // The anchors, derived once for every province (BL-1145): the post a
    // garrison takes when its province does not hold the capital.
    const std::map<uint32_t, province_anchor> anchors = province_anchors(w);

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
        // nation with its folded polities' closing chest (NATION_GENERATION.md
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

        // --- The post (Ben, 2026-10-03; BL-1145) ------------------------------
        // The CAPITAL garrison stands on the capital province's anchor centre
        // — its chief city — not on `nc.capital_tile`, Pass 5's seed tile,
        // which on the shipped worlds mostly holds no centre. A BORDER
        // garrison stands on its province's tile facing the neighbour it
        // guards (`garrison_border_post`), because the nation scorer reads a
        // garrison as border force only through its own tile's hex neighbours
        // (nation_ai.cpp build_force): on the anchor, deep inside the
        // province, it faces no one. A capital province that is ALSO a border
        // province takes the capital rule — the one set holds it once.
        //
        // A centreless province (it can outlive its centre in play, NR-952 —
        // never at generation, which centre_census C7 gates) falls back to its
        // lowest-id tile inside `province_anchor_tile`, stated there.
        const uint32_t cap_prov = (nc.capital_tile != null_entity)
                                ? w.provinces.province_of(nc.capital_tile) : 0u;
        for (const uint32_t prov : target_provinces) // ascending: deterministic creation order
        {
            const province* pr = w.provinces.find(prov);
            if (pr == nullptr)
                continue;
            entity_id post = null_entity;
            if (prov != cap_prov && best_neighbour != null_entity)
                post = garrison_border_post(w, *pr, best_neighbour);
            if (post == null_entity) // the capital province, or (defensively) no facing tile
                post = province_anchor_tile(anchors, *pr);
            place_garrison(w, nid, prov, post, count, campaign_opening_unit_row());
        }
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

// ---------------------------------------------------------------------------
// BL-1278 — the carve, previewed for the closing rounds
// ---------------------------------------------------------------------------
nation_carve_ground nation_carve_ground_of(const world& w,
                                          const std::vector<entity_id>& tile_ids,
                                          int gw, int gh)
{
    nation_carve_ground g;
    g.gw = gw;
    g.gh = gh;
    const int total = gw * gh > 0 ? gw * gh : 0;
    g.sub.assign(static_cast<std::size_t>(total), terrain_substrate::barren);
    g.step_cost.assign(static_cast<std::size_t>(total), 1.0f);
    for (int idx = 0; idx < total && idx < static_cast<int>(tile_ids.size()); ++idx)
    {
        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity) continue;
        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end()) continue;
        g.sub[static_cast<std::size_t>(idx)]       = it->second.substrate;
        g.step_cost[static_cast<std::size_t>(idx)] = expansion_cost(it->second.landform);
    }
    return g;
}

std::vector<int> preview_nation_carve(const nation_carve_ground& ground,
                                      const std::vector<int>& seed_tiles,
                                      uint32_t seed)
{
    const int total = ground.gw * ground.gh;
    std::vector<int> out(static_cast<std::size_t>(total > 0 ? total : 0), -1);
    if (total <= 0 || static_cast<int>(ground.sub.size()) < total
        || static_cast<int>(ground.step_cost.size()) < total)
        return out;

    // The water mask `generate_nations` builds, read the same way.
    std::vector<bool> is_water_map(static_cast<std::size_t>(total), false);
    int land_tiles = 0;
    for (int idx = 0; idx < total; ++idx)
    {
        if (is_water(ground.sub[static_cast<std::size_t>(idx)])) is_water_map[static_cast<std::size_t>(idx)] = true;
        else ++land_tiles;
    }
    if (land_tiles <= 0) return out; // `generate_nations`' seed_target == 0 return

    // Pass 1's historical-seed filter: off-grid, water and duplicate anchors dropped.
    std::vector<bool> taken(static_cast<std::size_t>(total), false);
    std::vector<int>  seeds, source;
    for (std::size_t si = 0; si < seed_tiles.size(); ++si)
    {
        const int idx = seed_tiles[si];
        if (idx < 0 || idx >= total) continue;
        if (is_water_map[static_cast<std::size_t>(idx)]) continue;
        if (taken[static_cast<std::size_t>(idx)]) continue;
        taken[static_cast<std::size_t>(idx)] = true;
        seeds.push_back(idx);
        source.push_back(static_cast<int>(si));
    }
    if (seeds.empty()) return out;

    // Every water kind is unclaimable (NR-792), exactly as the real carve.
    const std::vector<int> owner_map = carve_from_seeds(seeds, is_water_map, ground, seed, nullptr);
    for (int idx = 0; idx < total; ++idx)
    {
        const int k = owner_map[static_cast<std::size_t>(idx)];
        if (k >= 0 && k < static_cast<int>(source.size()))
            out[static_cast<std::size_t>(idx)] = source[static_cast<std::size_t>(k)];
    }
    return out;
}

// ---------------------------------------------------------------------------
// BL-1310 — one territory: the strait rule (NATION_GENERATION.md § Pass 2e)
// ---------------------------------------------------------------------------
// Shared by the closing rounds (which cede at the history's grain, planning
// on the carve oracle) and by `generate_nations`' tile-level pass after the
// fold (the guarantee, and the whole of the rule where no history ran), so
// the plan and the carve read ONE rule. Integer reads, ascending walks, fixed
// tie-breaks, no draw.
namespace {

constexpr int k_far = 1 << 20;

int strait_nb4(int idx, int gw, int gh, int out[4])
{
    const int col = idx % gw, row = idx / gw;
    int n = 0;
    out[n++] = row * gw + (col + 1) % gw;
    out[n++] = row * gw + (col - 1 + gw) % gw;
    if (row > 0) out[n++] = idx - gw;
    if (row + 1 < gh) out[n++] = idx + gw;
    return n;
}

/// Land groups: per-key connected land, cardinal, column-wrapped. Groups are
/// numbered in raster order of their lowest tile, which is `min_tile`.
struct land_groups
{
    std::vector<int>              comp;
    std::vector<int>              owner;
    std::vector<int>              min_tile;
    std::vector<std::vector<int>> tiles;
};

land_groups find_land_groups(const std::vector<int>& owner, int gw, int gh, int only_key = -1)
{
    const int total = gw * gh;
    land_groups g;
    g.comp.assign(static_cast<std::size_t>(total), -1);
    std::vector<int> stack;
    for (int s = 0; s < total; ++s)
    {
        const int k = owner[static_cast<std::size_t>(s)];
        if (k < 0 || g.comp[static_cast<std::size_t>(s)] >= 0) continue;
        if (only_key >= 0 && k != only_key) continue;
        const int id = static_cast<int>(g.tiles.size());
        g.owner.push_back(k);
        g.min_tile.push_back(s);
        g.tiles.emplace_back();
        g.comp[static_cast<std::size_t>(s)] = id;
        stack.assign(1, s);
        while (!stack.empty())
        {
            const int t = stack.back();
            stack.pop_back();
            g.tiles[static_cast<std::size_t>(id)].push_back(t);
            int nb[4];
            const int n = strait_nb4(t, gw, gh, nb);
            for (int i = 0; i < n; ++i)
                if (owner[static_cast<std::size_t>(nb[i])] == k && g.comp[static_cast<std::size_t>(nb[i])] < 0)
                {
                    g.comp[static_cast<std::size_t>(nb[i])] = id;
                    stack.push_back(nb[i]);
                }
        }
    }
    return g;
}

/// Per group: 1 when it is its key's core or joined to it by land or chained
/// straits. @p wdist is scratch (all `k_far` on entry and on exit).
std::vector<char> find_territories(const land_groups& g, const std::vector<int>& owner,
                                   const std::vector<char>& water, int gw, int gh,
                                   const std::vector<int>& core_tile, std::vector<int>& wdist)
{
    const int total = gw * gh;
    const int K = static_cast<int>(core_tile.size());
    const auto core_held = [&](int k) {
        const int ct = core_tile[static_cast<std::size_t>(k)];
        return ct >= 0 && ct < total && owner[static_cast<std::size_t>(ct)] == k && g.comp[static_cast<std::size_t>(ct)] >= 0;
    };
    std::vector<int> core(static_cast<std::size_t>(K), -1);
    for (int k = 0; k < K; ++k)
        if (core_held(k)) core[static_cast<std::size_t>(k)] = g.comp[static_cast<std::size_t>(core_tile[static_cast<std::size_t>(k)])];
    // A key with no core tile on its own land: its largest group (ties: lowest).
    for (int c = 0; c < static_cast<int>(g.tiles.size()); ++c)
    {
        const int k = g.owner[static_cast<std::size_t>(c)];
        if (k < 0 || k >= K || core_held(k)) continue;
        int& cur = core[static_cast<std::size_t>(k)];
        if (cur < 0 || g.tiles[static_cast<std::size_t>(c)].size() > g.tiles[static_cast<std::size_t>(cur)].size())
            cur = c;
    }

    std::vector<char> in_terr(g.tiles.size(), 0);
    std::vector<int>  q, touched;
    for (int k = 0; k < K; ++k)
    {
        const int c0 = core[static_cast<std::size_t>(k)];
        if (c0 < 0) continue;
        q.clear();
        touched.clear();
        const auto join = [&](int c) {
            in_terr[static_cast<std::size_t>(c)] = 1;
            for (const int t : g.tiles[static_cast<std::size_t>(c)]) q.push_back(t);
        };
        join(c0);
        std::size_t head = 0;
        while (head < q.size())
        {
            const int t = q[head++];
            const int d = water[static_cast<std::size_t>(t)] ? wdist[static_cast<std::size_t>(t)] : 0;
            int nb[4];
            const int n = strait_nb4(t, gw, gh, nb);
            for (int i = 0; i < n; ++i)
            {
                const int u = nb[i];
                if (water[static_cast<std::size_t>(u)])
                {
                    if (d + 1 > strait_tiles || wdist[static_cast<std::size_t>(u)] <= d + 1) continue;
                    if (wdist[static_cast<std::size_t>(u)] == k_far) touched.push_back(u);
                    wdist[static_cast<std::size_t>(u)] = d + 1;
                    q.push_back(u);
                }
                else if (owner[static_cast<std::size_t>(u)] == k)
                {
                    const int c = g.comp[static_cast<std::size_t>(u)];
                    if (c >= 0 && !in_terr[static_cast<std::size_t>(c)]) join(c);
                }
            }
        }
        for (const int t : touched) wdist[static_cast<std::size_t>(t)] = k_far;
    }
    return in_terr;
}

/// The land within strait reach of group @p c: per reached land tile, its
/// water distance (0 = a land neighbour), level by level so the list ascends
/// in distance. Reports ANOTHER key's land, or with @p include_own every land
/// tile outside @p c. A leg is water only: land is reported, never crossed.
/// @p wdist scratch as above.
struct reach_hit { int dist; int tile; };
std::vector<reach_hit> reach_of(const land_groups& g, int c, const std::vector<int>& owner,
                                const std::vector<char>& water, int gw, int gh,
                                std::vector<int>& wdist, bool include_own = false)
{
    const int k = g.owner[static_cast<std::size_t>(c)];
    std::vector<reach_hit> hits;
    std::vector<int> frontier = g.tiles[static_cast<std::size_t>(c)], next, touched;
    for (int d = 0; d <= strait_tiles && !frontier.empty(); ++d)
    {
        next.clear();
        for (const int t : frontier)
        {
            int nb[4];
            const int n = strait_nb4(t, gw, gh, nb);
            for (int i = 0; i < n; ++i)
            {
                const int u = nb[i];
                if (water[static_cast<std::size_t>(u)])
                {
                    if (d + 1 > strait_tiles || wdist[static_cast<std::size_t>(u)] != k_far) continue;
                    wdist[static_cast<std::size_t>(u)] = d + 1;
                    touched.push_back(u);
                    next.push_back(u);
                }
                else
                {
                    const int o = owner[static_cast<std::size_t>(u)];
                    if (o < 0) continue;
                    if (o != k || (include_own && g.comp[static_cast<std::size_t>(u)] != c)) hits.push_back({ d, u });
                }
            }
        }
        frontier.swap(next);
    }
    for (const int t : touched) wdist[static_cast<std::size_t>(t)] = k_far;
    return hits;
}

} // namespace

std::vector<carried_islet> islets_carried_by_cession(const std::vector<int>& owner, int gw, int gh,
                                                     const exclave_cession& ex,
                                                     const std::vector<int>& cell,
                                                     const std::vector<int>& cell_anchor,
                                                     int core_tile)
{
    std::vector<carried_islet> out;
    const int total = gw * gh;
    if (total <= 0 || static_cast<int>(owner.size()) < total || static_cast<int>(cell.size()) < total || ex.tiles.empty())
        return out;
    const auto in_x = [&](int t) { return std::binary_search(ex.tiles.begin(), ex.tiles.end(), t); };
    const auto part_of = [&](int t) -> int {
        const std::size_t i = static_cast<std::size_t>(std::lower_bound(ex.tiles.begin(), ex.tiles.end(), t) - ex.tiles.begin());
        return i < ex.part_key.size() ? ex.part_key[i] : ex.to;
    };
    const int U = static_cast<int>(cell_anchor.size());
    // The provinces the cession moves: a unit whose anchor stands in the
    // exclave and whose cell still holds it (the split's own reading).
    std::vector<char> moving(static_cast<std::size_t>(std::max(U, 0)), 0);
    bool any = false;
    for (int u = 0; u < U; ++u)
    {
        const int a = cell_anchor[static_cast<std::size_t>(u)];
        if (a >= 0 && a < total && cell[static_cast<std::size_t>(a)] == u && in_x(a))
        {
            moving[static_cast<std::size_t>(u)] = 1;
            any = true;
        }
    }
    if (!any) return out;
    // The cedent's core group stays put whatever its cells say.
    std::vector<char> core(static_cast<std::size_t>(total), 0);
    if (core_tile >= 0 && core_tile < total && owner[static_cast<std::size_t>(core_tile)] == ex.from)
    {
        std::vector<int> st{ core_tile };
        core[static_cast<std::size_t>(core_tile)] = 1;
        while (!st.empty())
        {
            const int t = st.back();
            st.pop_back();
            std::pair<int, int> nbrs[4];
            int n = 0;
            cardinal_neighbours(t % gw, t / gw, gw, gh, nbrs, n);
            for (int i = 0; i < n; ++i)
            {
                const int v = raster_idx(nbrs[i].first, nbrs[i].second, gw);
                if (!core[static_cast<std::size_t>(v)] && owner[static_cast<std::size_t>(v)] == ex.from)
                {
                    core[static_cast<std::size_t>(v)] = 1;
                    st.push_back(v);
                }
            }
        }
    }
    // The islets: the moved province's cell ground the cedent still holds
    // outside the exclave and outside its core, flooded per cell (ascending).
    const auto hung = [&](int t) {
        const int u = cell[static_cast<std::size_t>(t)];
        return u >= 0 && u < U && moving[static_cast<std::size_t>(u)] && owner[static_cast<std::size_t>(t)] == ex.from
            && !core[static_cast<std::size_t>(t)] && !in_x(t);
    };
    std::vector<char> seen(static_cast<std::size_t>(total), 0);
    for (int s = 0; s < total; ++s)
    {
        if (seen[static_cast<std::size_t>(s)] || !hung(s)) continue;
        carried_islet isl;
        isl.cell = cell[static_cast<std::size_t>(s)];
        std::vector<int> st{ s };
        seen[static_cast<std::size_t>(s)] = 1;
        while (!st.empty())
        {
            const int t = st.back();
            st.pop_back();
            isl.tiles.push_back(t);
            std::pair<int, int> nbrs[4];
            int n = 0;
            cardinal_neighbours(t % gw, t / gw, gw, gh, nbrs, n);
            for (int i = 0; i < n; ++i)
            {
                const int v = raster_idx(nbrs[i].first, nbrs[i].second, gw);
                if (!seen[static_cast<std::size_t>(v)] && hung(v) && cell[static_cast<std::size_t>(v)] == isl.cell)
                {
                    seen[static_cast<std::size_t>(v)] = 1;
                    st.push_back(v);
                }
            }
        }
        std::sort(isl.tiles.begin(), isl.tiles.end());
        // THE RECEIVER: the part of the province's cell the islet hangs from
        // -- the exclave tile of that cell nearest the islet (Chebyshev,
        // column-wrapped, Pass 2b's own measure), ties to the lower tile. A
        // province never splits on a settled world, so there it is simply the
        // province's receiver; tile by tile, it is the part nearest the islet.
        int best_d = INT32_MAX, best_t = -1;
        for (const int x : ex.tiles) // ascending: ties keep the lower tile
        {
            if (cell[static_cast<std::size_t>(x)] != isl.cell) continue;
            for (const int t : isl.tiles)
            {
                const int d = grid_distance(t % gw, t / gw, x % gw, x / gw, gw);
                if (d < best_d) { best_d = d; best_t = x; }
            }
        }
        if (best_t < 0) continue; // seatbelt: a moving cell always has its anchor in the exclave
        isl.to = part_of(best_t);
        out.push_back(std::move(isl));
    }
    return out;
}

bool next_exclave_cession(const std::vector<int>& owner, const std::vector<char>& water, int gw, int gh,
                          const std::vector<int>& core_tile, const std::vector<int>& order,
                          exclave_cession& out,
                          const std::vector<int>* unit,
                          const std::vector<int>* unit_anchor)
{
    const int total = gw * gh;
    if (total <= 0 || static_cast<int>(owner.size()) < total || static_cast<int>(water.size()) < total) return false;
    const land_groups g = find_land_groups(owner, gw, gh);
    std::vector<int> wdist(static_cast<std::size_t>(total), k_far);
    const std::vector<char> in_terr = find_territories(g, owner, water, gw, gh, core_tile, wdist);
    const int K = static_cast<int>(order.size());
    const auto rank = [&](int k) { return (k >= 0 && k < K) ? order[static_cast<std::size_t>(k)] : INT32_MAX; };
    const auto emit = [&](int c, int to, int contact, int kind) {
        out.from    = g.owner[static_cast<std::size_t>(c)];
        out.to      = to;
        out.contact = contact;
        out.kind    = kind;
        out.tiles   = g.tiles[static_cast<std::size_t>(c)];
        std::sort(out.tiles.begin(), out.tiles.end());
        out.part_key.assign(out.tiles.size(), to);
        out.part_contact.assign(out.tiles.size(), contact);
        return true;
    };
    const int C = static_cast<int>(g.tiles.size());

    // 1 — land-bordered exclaves, lowest tile first, SPLIT among the keys
    // that border them (§ Pass 2e, Ben 2026-10-18): every bordering key's
    // edge grows into the exclave at once, one level at a time; a tile
    // reached claims its whole UNIT (its province where the caller names
    // them -- a province never splits -- else itself) for the key that
    // reached it, ties to the lower rank, then the lower contact tile.
    // "NEAREST" IS NEAREST BY THE GROWTH ACROSS WHOLE PROVINCES: a claim jumps
    // a province at a time, so a province's distance is that of its first
    // tile the growth reaches, not of its anchor or its far side (on a
    // settled world; tile by tile where no history ran). Each part is
    // connected to its receiver's edge, so it joins a group the receiver
    // already holds: the exclave's group goes and none is made, which keeps
    // the walk finite.
    for (int c = 0; c < C; ++c)
    {
        if (in_terr[static_cast<std::size_t>(c)]) continue;
        const int k = g.owner[static_cast<std::size_t>(c)];
        const std::vector<int>& xt = g.tiles[static_cast<std::size_t>(c)];
        bool bordered = false;
        for (const int t : xt)
        {
            int nb[4];
            const int n = strait_nb4(t, gw, gh, nb);
            for (int i = 0; i < n && !bordered; ++i)
            {
                const int o = owner[static_cast<std::size_t>(nb[i])];
                bordered = o >= 0 && o != k;
            }
            if (bordered) break;
        }
        if (!bordered) continue;

        const auto unit_of = [&](int t) -> int {
            if (unit == nullptr || unit_anchor == nullptr) return -1;
            const int u = (*unit)[static_cast<std::size_t>(t)];
            if (u < 0 || u >= static_cast<int>(unit_anchor->size())) return -1;
            const int a = (*unit_anchor)[static_cast<std::size_t>(u)];
            if (a < 0 || a >= total || g.comp[static_cast<std::size_t>(a)] != c) return -1;
            return u;
        };
        // The exclave's tiles of each named unit (ascending: `xt` is sorted below).
        std::vector<int> xs = xt;
        std::sort(xs.begin(), xs.end());
        std::map<int, std::vector<int>> unit_tiles;
        for (const int t : xs)
        {
            const int u = unit_of(t);
            if (u >= 0) unit_tiles[u].push_back(t);
        }
        std::vector<int> claim_key(static_cast<std::size_t>(total), -1), claim_contact(static_cast<std::size_t>(total), -1);
        std::vector<char> unit_done; // indexed by unit id, sized lazily
        struct cand { int r, key, contact; };
        const auto better = [](const cand& a, const cand& b) {
            return a.r != b.r ? a.r < b.r : a.contact < b.contact;
        };
        std::vector<int> frontier, next;
        // Candidates at one level: per unreached tile in the exclave, the best
        // (rank, contact) among its reached-or-foreign land neighbours.
        const auto claim_level = [&](const std::vector<std::pair<int, cand>>& tile_cands) {
            // best per unit (named) or per tile (unnamed), applied in ascending order
            std::map<int, std::pair<cand, int>> best; // unit key -> (cand, any tile)
            for (const auto& [t, cd] : tile_cands)
            {
                const int u = unit_of(t);
                const int uk = u >= 0 ? u : -(t + 1);
                auto it = best.find(uk);
                if (it == best.end() || better(cd, it->second.first)) best[uk] = { cd, t };
            }
            next.clear();
            for (const auto& [uk, bc] : best)
            {
                const cand& cd = bc.first;
                if (uk >= 0)
                {
                    for (const int t : unit_tiles[uk])
                        if (claim_key[static_cast<std::size_t>(t)] < 0)
                        {
                            claim_key[static_cast<std::size_t>(t)]     = cd.key;
                            claim_contact[static_cast<std::size_t>(t)] = cd.contact;
                            next.push_back(t);
                        }
                }
                else
                {
                    const int t = -uk - 1;
                    if (claim_key[static_cast<std::size_t>(t)] >= 0) continue;
                    claim_key[static_cast<std::size_t>(t)]     = cd.key;
                    claim_contact[static_cast<std::size_t>(t)] = cd.contact;
                    next.push_back(t);
                }
            }
            std::sort(next.begin(), next.end());
            frontier.swap(next);
        };
        // Level 0: the exclave's own edge against each bordering key.
        {
            std::vector<std::pair<int, cand>> tc;
            for (const int t : xs)
            {
                int nb[4];
                const int n = strait_nb4(t, gw, gh, nb);
                bool have = false;
                cand bc{};
                for (int i = 0; i < n; ++i)
                {
                    const int o = owner[static_cast<std::size_t>(nb[i])];
                    if (o < 0 || o == k) continue;
                    const cand cd{ rank(o), o, nb[i] };
                    if (!have || better(cd, bc)) { bc = cd; have = true; }
                }
                if (have) tc.emplace_back(t, bc);
            }
            claim_level(tc);
        }
        // Later levels: grow from the tiles claimed last.
        while (!frontier.empty())
        {
            std::map<int, cand> per_tile;
            for (const int f : frontier)
            {
                int nb[4];
                const int n = strait_nb4(f, gw, gh, nb);
                const cand cd{ rank(claim_key[static_cast<std::size_t>(f)]), claim_key[static_cast<std::size_t>(f)],
                               claim_contact[static_cast<std::size_t>(f)] };
                for (int i = 0; i < n; ++i)
                {
                    const int u = nb[i];
                    if (g.comp[static_cast<std::size_t>(u)] != c || claim_key[static_cast<std::size_t>(u)] >= 0) continue;
                    auto it = per_tile.find(u);
                    if (it == per_tile.end() || better(cd, it->second)) per_tile[u] = cd;
                }
            }
            std::vector<std::pair<int, cand>> tc(per_tile.begin(), per_tile.end());
            if (tc.empty()) break;
            claim_level(tc);
        }
        out.from  = k;
        out.kind  = 0;
        out.tiles = xs;
        out.part_key.resize(xs.size());
        out.part_contact.resize(xs.size());
        for (std::size_t i = 0; i < xs.size(); ++i)
        {
            out.part_key[i]     = claim_key[static_cast<std::size_t>(xs[i])];
            out.part_contact[i] = claim_contact[static_cast<std::size_t>(xs[i])];
        }
        out.to      = out.part_key.front();
        out.contact = out.part_contact.front();
        return true;
    }

    // 2 — islands within strait reach of another key's TERRITORY: to the
    // nearest by water tiles (ties: lower rank, then lower tile). Landing in a
    // territory makes the cession final.
    for (int c = 0; c < C; ++c)
    {
        if (in_terr[static_cast<std::size_t>(c)]) continue;
        int best = -1, best_d = 0, best_tile = -1;
        for (const reach_hit& h : reach_of(g, c, owner, water, gw, gh, wdist))
        {
            const int hc = g.comp[static_cast<std::size_t>(h.tile)];
            if (hc < 0 || !in_terr[static_cast<std::size_t>(hc)]) continue;
            const int key = owner[static_cast<std::size_t>(h.tile)];
            if (best < 0 || h.dist < best_d
                || (h.dist == best_d && (rank(key) < rank(best) || (rank(key) == rank(best) && h.tile < best_tile))))
            {
                best      = key;
                best_d    = h.dist;
                best_tile = h.tile;
            }
        }
        if (best >= 0) return emit(c, best, best_tile, 1);
    }

    // 3 — what is left outside every territory is islands, none in reach of
    // another key's territory. One that still reaches another key's ground
    // sits in an archipelago several keys hold, remote from every territory:
    // the archipelago goes whole to the owner of its largest island (ties:
    // lower rank, then lower tile), islet by islet, after which it is the
    // remote island of one nation.
    for (int c = 0; c < C; ++c)
    {
        if (in_terr[static_cast<std::size_t>(c)]) continue;
        if (reach_of(g, c, owner, water, gw, gh, wdist).empty()) continue; // the remote island
        std::vector<int>  cluster{ c };
        std::vector<char> in_cluster(static_cast<std::size_t>(C), 0);
        in_cluster[static_cast<std::size_t>(c)] = 1;
        for (std::size_t h = 0; h < cluster.size(); ++h)
            for (const reach_hit& rh : reach_of(g, cluster[h], owner, water, gw, gh, wdist, /*include_own=*/true))
            {
                const int o = g.comp[static_cast<std::size_t>(rh.tile)];
                if (o < 0 || in_terr[static_cast<std::size_t>(o)] || in_cluster[static_cast<std::size_t>(o)]) continue;
                in_cluster[static_cast<std::size_t>(o)] = 1;
                cluster.push_back(o);
            }
        int big = -1;
        for (const int m : cluster)
        {
            if (big < 0) { big = m; continue; }
            const std::size_t sm = g.tiles[static_cast<std::size_t>(m)].size();
            const std::size_t sb = g.tiles[static_cast<std::size_t>(big)].size();
            const int rm = rank(g.owner[static_cast<std::size_t>(m)]), rb = rank(g.owner[static_cast<std::size_t>(big)]);
            if (sm > sb || (sm == sb && (rm < rb || (rm == rb && g.min_tile[static_cast<std::size_t>(m)] < g.min_tile[static_cast<std::size_t>(big)]))))
                big = m;
        }
        const int to = g.owner[static_cast<std::size_t>(big)];
        if (to != g.owner[static_cast<std::size_t>(c)])
            return emit(c, to, g.min_tile[static_cast<std::size_t>(big)], 2);
    }
    return false;
}

int nearest_key_across_water(const std::vector<int>& owner, const std::vector<char>& water, int gw, int gh,
                             int from, const std::vector<char>& eligible, const std::vector<int>& order)
{
    const int total = gw * gh;
    if (total <= 0) return -1;
    const int K = static_cast<int>(order.size());
    const auto ok = [&](int k) {
        return k >= 0 && k != from && k < static_cast<int>(eligible.size()) && eligible[static_cast<std::size_t>(k)];
    };
    const auto rank = [&](int k) { return (k >= 0 && k < K) ? order[static_cast<std::size_t>(k)] : INT32_MAX; };
    std::vector<char> seen(static_cast<std::size_t>(total), 0);
    std::vector<int>  frontier, next;
    for (int t = 0; t < total; ++t)
        if (owner[static_cast<std::size_t>(t)] == from) frontier.push_back(t);
    while (!frontier.empty())
    {
        int best = -1, best_tile = -1;
        next.clear();
        for (const int t : frontier)
        {
            int nb[4];
            const int n = strait_nb4(t, gw, gh, nb);
            for (int i = 0; i < n; ++i)
            {
                const int u = nb[i];
                if (water[static_cast<std::size_t>(u)])
                {
                    if (seen[static_cast<std::size_t>(u)]) continue;
                    seen[static_cast<std::size_t>(u)] = 1;
                    next.push_back(u);
                }
                else
                {
                    const int o = owner[static_cast<std::size_t>(u)];
                    if (!ok(o)) continue;
                    if (best < 0 || rank(o) < rank(best) || (rank(o) == rank(best) && u < best_tile))
                    {
                        best      = o;
                        best_tile = u;
                    }
                }
            }
        }
        if (best >= 0) return best;
        frontier.swap(next);
    }
    return -1;
}

bool key_is_one_territory(const std::vector<int>& owner, const std::vector<char>& water, int gw, int gh,
                          int key, int core_tile)
{
    const int total = gw * gh;
    if (key < 0 || total <= 0) return true;
    const land_groups g = find_land_groups(owner, gw, gh, key);
    if (g.tiles.size() <= 1) return true;
    std::vector<int> wdist(static_cast<std::size_t>(total), k_far);
    std::vector<int> cores(static_cast<std::size_t>(key + 1), -1);
    cores[static_cast<std::size_t>(key)] = core_tile;
    const std::vector<char> in_terr = find_territories(g, owner, water, gw, gh, cores, wdist);
    for (int c = 0; c < static_cast<int>(g.tiles.size()); ++c)
        if (!in_terr[static_cast<std::size_t>(c)] && !reach_of(g, c, owner, water, gw, gh, wdist).empty())
            return false;
    return true;
}

int nearest_tile_among(const std::vector<int>& tiles, int target, int gw)
{
    if (gw <= 0 || target < 0) return -1;
    const int tc = target % gw, tr = target / gw;
    int best = -1, best_d = 0;
    for (const int t : tiles)
    {
        if (t < 0) continue;
        const int d = grid_distance(tc, tr, t % gw, t / gw, gw);
        if (best < 0 || d < best_d || (d == best_d && t < best)) { best = t; best_d = d; }
    }
    return best;
}

int seat_off_speck(const std::vector<int>& owner, int gw, int gh, int key, int core,
                   const std::vector<int>& provinces, int target, const std::vector<char>* water)
{
    const int total = gw * gh;
    if (total <= 0 || core < 0 || core >= total || static_cast<int>(owner.size()) < total
        || owner[static_cast<std::size_t>(core)] != key)
        return core;
    const land_groups g = find_land_groups(owner, gw, gh, key);
    const int cc = g.comp[static_cast<std::size_t>(core)];
    if (cc < 0) return core;
    const int core_size = static_cast<int>(g.tiles[static_cast<std::size_t>(cc)].size());
    if (core_size >= capital_speck_tiles) return core;
    // Within the core's own territory only, where @p water is given: the
    // groups joined to the core by land or chained straits.
    std::vector<char> in_terr;
    if (water != nullptr)
    {
        std::vector<int> cores(static_cast<std::size_t>(key + 1), -1);
        cores[static_cast<std::size_t>(key)] = core;
        in_terr = territory_tiles(owner, *water, gw, gh, cores);
    }
    // The largest group holding one of the key's provinces.
    int best_size = core_size;
    for (const int t : provinces)
    {
        if (t < 0 || t >= total || owner[static_cast<std::size_t>(t)] != key) continue;
        if (!in_terr.empty() && !in_terr[static_cast<std::size_t>(t)]) continue;
        const int c = g.comp[static_cast<std::size_t>(t)];
        if (c >= 0) best_size = std::max(best_size, static_cast<int>(g.tiles[static_cast<std::size_t>(c)].size()));
    }
    if (best_size <= core_size) return core; // no larger group to seat on
    std::vector<int> in_best;
    for (const int t : provinces)
    {
        if (t < 0 || t >= total || owner[static_cast<std::size_t>(t)] != key) continue;
        if (!in_terr.empty() && !in_terr[static_cast<std::size_t>(t)]) continue;
        const int c = g.comp[static_cast<std::size_t>(t)];
        if (c >= 0 && static_cast<int>(g.tiles[static_cast<std::size_t>(c)].size()) == best_size) in_best.push_back(t);
    }
    const int near = nearest_tile_among(in_best, target >= 0 ? target : core, gw);
    return near >= 0 ? near : core;
}

std::vector<char> territory_tiles(const std::vector<int>& owner, const std::vector<char>& water, int gw, int gh,
                                  const std::vector<int>& core_tile)
{
    const int total = gw * gh;
    std::vector<char> out(static_cast<std::size_t>(total > 0 ? total : 0), 0);
    if (total <= 0) return out;
    const land_groups g = find_land_groups(owner, gw, gh);
    std::vector<int> wdist(static_cast<std::size_t>(total), k_far);
    const std::vector<char> in_terr = find_territories(g, owner, water, gw, gh, core_tile, wdist);
    for (int c = 0; c < static_cast<int>(g.tiles.size()); ++c)
        if (in_terr[static_cast<std::size_t>(c)])
            for (const int t : g.tiles[static_cast<std::size_t>(c)]) out[static_cast<std::size_t>(t)] = 1;
    return out;
}

void rederive_carve_water(std::vector<int>& owner_map, const std::vector<terrain_substrate>& sub, int gw, int gh)
{
    const int total = gw * gh;
    if (total <= 0 || static_cast<int>(owner_map.size()) < total || static_cast<int>(sub.size()) < total) return;
    for (int t = 0; t < total; ++t)
        if (is_water(sub[static_cast<std::size_t>(t)])) owner_map[static_cast<std::size_t>(t)] = -1;
    derive_water_ownership(owner_map, sub, gw, gh);
}
