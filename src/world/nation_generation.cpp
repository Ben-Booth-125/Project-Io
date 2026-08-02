#include "nation_generation.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <queue>
#include <random>
#include <string>
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
/// the returned size as authoritative.
/// Habitable compositions (grassland, forest, wetland) are strongly preferred.
/// Falls back to any land tile if the preferred pool is exhausted.
///
/// Returns the raster indices of the chosen seed tiles.
std::vector<int> place_seeds(const std::vector<bool>& is_ocean,
                             const std::vector<terrain_composition>& comp,
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
        if (is_ocean[idx])
            continue;
        any_land.push_back(idx);
        const terrain_composition c = comp[idx];
        if (c == terrain_composition::grassland
         || c == terrain_composition::forest
         || c == terrain_composition::wetland)
        {
            preferred.push_back(idx);
        }
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
        const terrain_composition c = comp[idx];
        if (c != terrain_composition::grassland
         && c != terrain_composition::forest
         && c != terrain_composition::wetland)
        {
            candidates.push_back(idx);
        }
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

/// Expand each seed outward until every claimable land tile is assigned.
/// Ocean tiles are never claimed. Mountains and highlands drain the budget
/// of the claiming nation so ranges fall at or near those features.
///
/// @param seeds        Raster indices of the nation seeds (one per nation).
/// @param is_ocean     Per-tile ocean flag.
/// @param tiles        Tile components, for landform lookup.
/// @param tile_ids     Raster-order tile entity IDs.
/// @param gw, gh       Grid dimensions.
/// @param weight       Per-seed growth weight; a seed's step cost is divided by
///                     its weight, so high-weight seeds expand cheaper and claim
///                     larger territory (BL-053 — strongly varied nation sizes).
/// @param rng          Seeded RNG for jitter.
/// @return             Per-tile nation index (-1 = unclaimed/ocean). Indexed as row*gw+col.
std::vector<int> expand_territory(const std::vector<int>& seeds,
                                  const std::vector<bool>& is_ocean,
                                  const world& w,
                                  const std::vector<entity_id>& tile_ids,
                                  int gw, int gh,
                                  const std::vector<float>& weight,
                                  std::mt19937& rng)
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
    }

    while (!pq.empty())
    {
        const bfs_entry cur = pq.top();
        pq.pop();

        const int idx = raster_idx(cur.col, cur.row, gw);

        if (settled[static_cast<std::size_t>(idx)])
            continue;
        settled[static_cast<std::size_t>(idx)] = true;

        // Expand into cardinal neighbours.
        std::pair<int,int> nbrs[4];
        int n = 0;
        cardinal_neighbours(cur.col, cur.row, gw, gh, nbrs, n);

        for (int i = 0; i < n; ++i)
        {
            const int nc  = nbrs[i].first;
            const int nr  = nbrs[i].second;
            const int nidx = raster_idx(nc, nr, gw);

            if (settled[static_cast<std::size_t>(nidx)] || is_ocean[static_cast<std::size_t>(nidx)])
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

    return owner;
}

// ---------------------------------------------------------------------------
// Pass 2b — orphan-island assignment
// ---------------------------------------------------------------------------

/// Assign every unclaimed land tile to a nation, closing the gaps the
/// water-blocked Voronoi BFS leaves behind on landmasses disconnected from
/// every seed. Orphan land tiles (owner == -1 and not ocean) are grouped into
/// connected components by cardinal adjacency (column-wrapped, rows do not
/// wrap), and each whole component is assigned to the nation owning the nearest
/// already-claimed land tile by `grid_distance`.
///
/// The pass is a pure, deterministic function of its inputs: components are
/// discovered in raster order and "nearest" is tie-broken first by lowest
/// distance, then by lowest nation index, then by lowest claimed-tile index. No
/// RNG is used. If there are no claimed tiles at all, `owner_map` is left
/// unchanged.
///
/// @param owner_map  Per-tile nation index (-1 = unclaimed/ocean), mutated in place.
/// @param is_ocean   Per-tile ocean flag.
/// @param gw, gh     Grid dimensions.
void assign_orphan_islands(std::vector<int>& owner_map,
                           const std::vector<bool>& is_ocean,
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
         || is_ocean[static_cast<std::size_t>(start)])
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
                 || is_ocean[static_cast<std::size_t>(nidx)])
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
/// @return the final nation count (distinct nations after compaction).
int merge_undersized_nations(std::vector<int>& owner_map, int seed_count,
                             int min_tiles, int gw, int gh)
{
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
        // Smallest active nation (tie: lowest index).
        int small = -1;
        for (int ni = 0; ni < seed_count; ++ni)
        {
            if (!active[static_cast<std::size_t>(ni)]) continue;
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

// The name bank is entirely self-contained. Names are constructed from a
// randomly chosen structural template filled with syllables drawn from three
// independent pools (onset, nucleus, coda). This produces names that feel
// vaguely alien without requiring a fixed human-authored list.

static const char* const k_onsets[] = {
    "Ar", "Bel", "Cal", "Dor", "El", "Far", "Gar", "Hel", "Ir", "Jal",
    "Kor", "Lun", "Mel", "Nor", "Or", "Par", "Qel", "Ros", "Sel", "Tar",
    "Ul", "Vel", "Wor", "Xen", "Yar", "Zan", "Ceth", "Drath", "Eln", "Fyr",
};
static constexpr int k_onset_count = 30;

static const char* const k_nuclei[] = {
    "a", "e", "i", "o", "u", "an", "en", "on", "al", "el",
    "ir", "ur", "ath", "eth", "ith",
};
static constexpr int k_nucleus_count = 15;

static const char* const k_codas[] = {
    "ia", "ar", "on", "us", "is", "an", "or", "ath", "el", "yn",
    "era", "ix", "ael", "oran", "ast",
};
static constexpr int k_coda_count = 15;

// Structural adjective pool for compound forms ("Free Lands of ...")
static const char* const k_adjectives[] = {
    "Free", "United", "Grand", "High", "Old", "New", "Sacred", "Iron",
    "Silver", "Northern", "Southern", "Eastern", "Western", "Central",
};
static constexpr int k_adj_count = 14;

// Structural noun pool for compound forms
static const char* const k_nouns[] = {
    "Reach", "March", "Expanse", "Dominion", "Federation", "Commonwealth",
    "Compact", "Alliance", "Protectorate", "Principality", "Republic", "Conclave",
};
static constexpr int k_noun_count = 12;

/// Build a single phoneme-derived word (two to three syllable clusters).
std::string make_phoneme_word(std::mt19937& rng)
{
    std::uniform_int_distribution<int> pick_onset(0, k_onset_count - 1);
    std::uniform_int_distribution<int> pick_nuc(0, k_nucleus_count - 1);
    std::uniform_int_distribution<int> pick_coda(0, k_coda_count - 1);
    std::uniform_int_distribution<int> pick_syl(2, 3); // syllable count

    std::string word;
    const int syllables = pick_syl(rng);
    for (int s = 0; s < syllables; ++s)
    {
        if (s == 0)
            word += k_onsets[static_cast<std::size_t>(pick_onset(rng))];
        else
            word += k_onsets[static_cast<std::size_t>(pick_onset(rng))];

        word += k_nuclei[static_cast<std::size_t>(pick_nuc(rng))];
    }
    word += k_codas[static_cast<std::size_t>(pick_coda(rng))];
    return word;
}

/// Generate a procedural nation name using one of three structural templates,
/// chosen uniformly. Templates:
///   0 — single phoneme word                  e.g. "Arenarath"
///   1 — adjective + phoneme word             e.g. "Free Belonor"
///   2 — phoneme word + noun                  e.g. "Kalorath Dominion"
std::string make_nation_name(std::mt19937& rng)
{
    std::uniform_int_distribution<int> tmpl(0, 2);
    std::uniform_int_distribution<int> pick_adj(0, k_adj_count - 1);
    std::uniform_int_distribution<int> pick_noun(0, k_noun_count - 1);

    const int form = tmpl(rng);
    switch (form)
    {
        case 0:
            return make_phoneme_word(rng);
        case 1:
        {
            std::string name = k_adjectives[static_cast<std::size_t>(pick_adj(rng))];
            name += ' ';
            name += make_phoneme_word(rng);
            return name;
        }
        default: // 2
        {
            std::string name = make_phoneme_word(rng);
            name += ' ';
            name += k_nouns[static_cast<std::size_t>(pick_noun(rng))];
            return name;
        }
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
    uint32_t seed)
{
    const int total = gw * gh;

    // Distinct seed offsets keep each pass's RNG stream independent.
    const uint32_t seed_seeds  = seed ^ 0xA3B4C5D6u;
    const uint32_t seed_expand = seed ^ 0x1F2E3D4Cu;
    const uint32_t seed_weight = seed ^ 0x6D2B79F5u;
    const uint32_t seed_pol    = seed ^ 0x8C7B6A59u;
    const uint32_t seed_name   = seed ^ 0x4E5F607Au;

    // --- Build flat composition and is_ocean maps from the existing tile data ---
    std::vector<terrain_composition> comp(static_cast<std::size_t>(total),
                                          terrain_composition::barren);
    std::vector<bool> is_ocean(static_cast<std::size_t>(total), false);

    for (int idx = 0; idx < total; ++idx)
    {
        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            continue;
        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end())
            continue;
        comp[static_cast<std::size_t>(idx)] = it->second.composition;
        if (it->second.composition == terrain_composition::ocean)
            is_ocean[static_cast<std::size_t>(idx)] = true;
    }

    // --- Seed budget: scaled to the habitable landmass, not taken as a target ---
    // One seed per `land_tiles_per_seed` non-ocean tiles. A wetter or smaller body
    // therefore starts with fewer nations than a dry, continental one — the count
    // is downstream of the geography.
    int land_tiles = 0;
    for (int idx = 0; idx < total; ++idx)
        if (!is_ocean[static_cast<std::size_t>(idx)])
            ++land_tiles;

    const int tiles_per_seed = params.land_tiles_per_seed > 0 ? params.land_tiles_per_seed : 1;
    const int seed_target    = land_tiles > 0
                             ? std::max(1, land_tiles / tiles_per_seed)
                             : 0;
    if (seed_target == 0)
        return {};

    // --- Pass 1: seed placement ---
    // Historical seeds win when the caller supplies them (BL-218): the cores are
    // the settlement pass's province anchors, so the political map grows out of
    // places people settled rather than out of a random draw. The RNG stream is
    // still constructed and left unconsumed on this path, which keeps every
    // *later* pass's stream identical to the random-placement path.
    std::mt19937 seed_rng(seed_seeds);
    std::vector<int> seeds;
    if (!params.seed_tiles.empty())
    {
        std::vector<bool> taken(static_cast<std::size_t>(total), false);
        for (int idx : params.seed_tiles)
        {
            if (idx < 0 || idx >= total) continue;
            if (is_ocean[static_cast<std::size_t>(idx)]) continue;
            if (taken[static_cast<std::size_t>(idx)]) continue;
            taken[static_cast<std::size_t>(idx)] = true;
            seeds.push_back(idx);
        }
    }
    if (seeds.empty())
    {
        seeds = place_seeds(
            is_ocean, comp, gw, gh,
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

    // --- Pass 2: territory expansion (weighted) ---
    std::mt19937 expand_rng(seed_expand);
    std::vector<int> owner_map = expand_territory(
        seeds, is_ocean, w, tile_ids, gw, gh, weights, expand_rng);

    // --- Pass 2b: orphan-island assignment (claim water-disconnected land) ---
    assign_orphan_islands(owner_map, is_ocean, gw, gh);

    // --- Pass 2c: light "in history" merges — absorb anything below the size floor ---
    // No target count: the loop stops when every survivor holds a viable territory,
    // so the final nation count is whatever the landmass and coastline produce.
    int nation_count = seed_count;
    if (params.min_nation_tiles > 0)
        nation_count = merge_undersized_nations(owner_map, seed_count, params.min_nation_tiles, gw, gh);

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

    // --- Pass 3: resource profile derivation ---
    derive_resource_profiles(owner_map, tile_ids, w, gw, gh, nation_data);

    // --- Pass 4: political character ---
    std::mt19937 pol_rng(seed_pol);
    for (int ni = 0; ni < nation_count; ++ni)
        assign_political_character(nation_data[static_cast<std::size_t>(ni)], pol_rng);

    // --- Pass 5: naming ---
    std::mt19937 name_rng(seed_name);
    for (int ni = 0; ni < nation_count; ++ni)
        nation_data[static_cast<std::size_t>(ni)].name = make_nation_name(name_rng);

    // --- Register nations in the world and write ownership maps ---
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(static_cast<std::size_t>(nation_count));

    for (int ni = 0; ni < nation_count; ++ni)
    {
        const entity_id nid = w.create_entity();
        nation_ids.push_back(nid);
        w.nations[nid] = std::move(nation_data[static_cast<std::size_t>(ni)]);
    }

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

    // --- Pass 6: substrate density + nation_substrate accumulation ---
    // For each nation-owned tile, find the nearest population centre on the same
    // body and compute a density ripple. Tiles on bodies with no centres are left
    // at substrate_density = 0.
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

        if (best_density <= 0.0f)
            continue;

        const entity_id nation_id = nation_ids[static_cast<std::size_t>(ni)];
        nation_substrate& sub = w.nation_substrates[std::make_pair(nation_id, body_id)];

        // BL-078: store only generation baselines here — raw deposit-weighted
        // capacity and the population-proximity weight. The economic scalars
        // (capacity_scale, basket, elasticity, clearing_fraction) are applied at
        // tick time by inject_substrate_demand, so the demand/supply model is
        // fully retunable from economy.lua without regenerating the world.
        sub.population_weight += best_density;
        for (std::size_t r = 0; r < resource_count; ++r)
            sub.capacity[r] += best_density * tc.resource_deposit[r];
    }

    return nation_ids;
}
