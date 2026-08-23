#include "province.hpp"

#include "hex_neighbors.hpp"
#include "logistics.hpp" // body_tile_grid — the raster index every body-grid pass walks

#include <algorithm>
#include <cmath>
#include <istream>
#include <map>
#include <ostream>
#include <queue>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Stateless folds — no RNG stream is consumed anywhere in this file
// ---------------------------------------------------------------------------

/// SplitMix64 finaliser. A pure avalanche of @p x; the whole file's randomness.
uint64_t mix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

uint64_t fold(uint64_t a, uint64_t b) { return mix64(mix64(a) ^ (b * 0x9E3779B97F4A7C15ull)); }

/// Three-input fold — the per-edge jitter draw, (seed, lower tile, upper tile).
uint64_t fold(uint64_t a, uint64_t b, uint64_t c) { return mix64(fold(a, b) ^ mix64(c)); }

// ---------------------------------------------------------------------------
// Grid helpers
// ---------------------------------------------------------------------------

/// The odd-r hex neighbour of (@p gx, @p gy) on side @p side, with the column
/// wrapped into [0, gw) and the row rejected when it leaves the grid. Columns
/// wrap and rows do not — the same cylinder logistics.cpp walks.
/// @return True when the neighbour exists; then @p out_x / @p out_y hold it.
bool wrapped_neighbour(int gx, int gy, int side, int gw, int gh, int& out_x, int& out_y)
{
    const hex_neighbors::coord c = hex_neighbors::neighbour(gx, gy, side);
    if (c.gy < 0 || c.gy >= gh)
        return false;
    int nx = c.gx % gw;
    if (nx < 0)
        nx += gw;
    out_x = nx;
    out_y = c.gy;
    return true;
}

// ---------------------------------------------------------------------------
// The three DOMAINS (BL-516)
// ---------------------------------------------------------------------------
// The partition runs the SAME algorithm three times over three exclusive tile
// sets. Everything that used to be a file-level constant of the growth rule —
// the ceiling, the budget, the seed spacing — is a field here instead, so the
// land pass is the pre-BL-516 pass with its own numbers rather than a special
// case of a new one.

/// Does substrate @p sub belong to domain @p k? The three predicates PARTITION
/// the substrates: every tile lands in exactly one, which is what makes "a
/// province never spans two domains" structural rather than checked.
bool in_domain(terrain_substrate sub, province_kind k)
{
    switch (k)
    {
        case province_kind::land:          return !is_water(sub);
        case province_kind::coastal_water: return is_coastal_water(sub) || is_lake(sub);
        case province_kind::open_ocean:    return is_open_ocean(sub);
    }
    return false;
}

struct domain_spec
{
    province_kind kind        = province_kind::land;
    std::size_t   soft_target = k_province_min_tiles;      ///< Default growth budget.
    std::size_t   max_tiles   = k_province_max_tiles;      ///< The clamp growth obeys.
    std::size_t   hard_min    = k_province_hard_min_tiles; ///< Taken whatever it costs.
    int           spacing     = k_province_seed_spacing;   ///< Minimum seed separation.
    bool          settled     = true;                      ///< Population centres seed it (land only).
};

/// The domains IN THE ORDER THEY ARE PARTITIONED, land first. The order is not
/// load-bearing — the sets are disjoint, so no domain can take a tile another
/// wanted — but it is fixed anyway so a reader never has to prove that.
const domain_spec k_domains[3] = {
    // Land: the pre-BL-516 band and rules, unchanged.
    { province_kind::land, k_province_min_tiles, k_province_max_tiles,
      k_province_hard_min_tiles, k_province_seed_spacing, true },
    // Coastal water: Ben named the land band for it, and nothing settles it.
    { province_kind::coastal_water, k_province_min_tiles, k_province_max_tiles,
      k_province_hard_min_tiles, k_province_seed_spacing, false },
    // Open ocean: much larger, growth capped at 80.
    { province_kind::open_ocean, k_sea_province_soft_target, k_sea_province_max_tiles,
      k_province_hard_min_tiles, k_sea_province_seed_spacing, false },
};

/// Per-body, per-domain working state for the fill. Everything is keyed by tile
/// entity id or by an index into `members` (ascending tile id), never by
/// container order.
struct body_work
{
    int gw = 0;
    int gh = 0;

    std::vector<entity_id> members; ///< This domain's tiles, ascending entity id.
    std::vector<entity_id> grid;    ///< gy*gw + gx -> tile, null_entity if absent.
    std::unordered_map<entity_id, uint32_t> owner; ///< tile -> region index (+1; 0 = unclaimed).
};

/// One growing province. `seed` doubles as the region's stable tie-break key in
/// the frontier ordering — it is unique across regions by construction, because
/// a tile is claimed the instant it is seeded.
struct region
{
    entity_id              seed   = null_entity;
    std::size_t            target = k_province_min_tiles;
    std::vector<entity_id> tiles;

    /// Sum of the edge costs of the steps that claimed `tiles`, and how many
    /// there were (the seed itself was not stepped to). Their mean is what the
    /// SOFT brake reads — see grow_regions.
    long long   step_cost_sum = 0;
    std::size_t step_count    = 0;
};

/// A frontier entry: reaching @p tile from @p seed's region at total @p cost,
/// where the LAST edge crossed cost @p step. `step` is what the soft-target
/// rule reads — past its budget a region crosses only a binding edge, and that
/// is a property of the edge itself, not of the path that got there.
struct frontier_entry
{
    int         cost   = 0;
    int         step   = 0;
    entity_id   seed   = null_entity;
    entity_id   tile   = null_entity;
    std::size_t region_index = 0;
};

/// Min-heap ordering: cheapest first, ties broken on (seed tile, tile). A total
/// order over unique keys — no two entries compare equal, so no container
/// accident can reach a claim.
struct frontier_worse
{
    bool operator()(const frontier_entry& a, const frontier_entry& b) const
    {
        if (a.cost != b.cost)
            return a.cost > b.cost;
        if (a.seed != b.seed)
            return a.seed > b.seed;
        return a.tile > b.tile;
    }
};

/// The cost model, on already-fetched tiles. See province.hpp for what pins
/// each coefficient. Symmetric in (a, b): the river term takes EITHER side's
/// bit, the height term is an absolute difference, the jitter fold is over the
/// sorted id pair, and the road term needs both tiles roaded.
int edge_cost_impl(uint32_t seed, entity_id a_id, const tile_component& a, entity_id b_id,
                   const tile_component& b, int side)
{
    const int opposite = (side + 3) % 6;

    int c = k_province_edge_base_cost;

    if (((a.river_edges >> side) & 1u) != 0u || ((b.river_edges >> opposite) & 1u) != 0u)
        c += k_province_river_edge_cost;

    const float dh = std::fabs(a.height - b.height);
    c += static_cast<int>(dh * static_cast<float>(k_province_height_cost) + 0.5f);

    const uint64_t lo = std::min(a_id, b_id);
    const uint64_t hi = std::max(a_id, b_id);
    c += static_cast<int>(fold(seed, lo, hi) % static_cast<uint64_t>(k_province_edge_jitter));

    // A ROAD BINDS. Divided, not zeroed: a bridge over a gorge is still a gorge.
    if (a.road_level > 0 && b.road_level > 0)
        c = (c + k_province_road_bind_divisor - 1) / k_province_road_bind_divisor;

    return c < 1 ? 1 : c;
}

/// Grow every region named in @p active SIMULTANEOUSLY as one cost-weighted
/// multi-source fill, claiming into @p bw.owner. Neighbouring seeds therefore
/// meet on the terrain between them rather than in the order they were listed.
void grow_regions(body_work& bw, const world& w, uint32_t seed, const domain_spec& dom,
                  std::vector<region>& regions, const std::vector<std::size_t>& active)
{
    std::priority_queue<frontier_entry, std::vector<frontier_entry>, frontier_worse> pq;

    for (const std::size_t ri : active)
    {
        frontier_entry e;
        e.cost         = 0;
        e.step         = 0;
        e.seed         = regions[ri].seed;
        e.tile         = regions[ri].seed;
        e.region_index = ri;
        pq.push(e);
    }

    while (!pq.empty())
    {
        const frontier_entry f = pq.top();
        pq.pop();

        if (bw.owner.find(f.tile) != bw.owner.end())
            continue;

        region& r = regions[f.region_index];

        // The one hard clamp in the file. Per DOMAIN since BL-516: 12 on land
        // and in the shallows, 80 on the open ocean.
        if (r.tiles.size() >= dom.max_tiles)
            continue;

        // THE HARD FLOOR (3-12 hard target). A region takes its first three
        // tiles whatever they cost. This is not a repair pass — nothing is ever
        // merged — it is the growth rule refusing to stop early. A region can
        // still ship below it when the land genuinely runs out, which is the
        // island case the ruling protects.
        //
        // THE SOFT BRAKE, past the region's budget: it annexes only ground NO
        // HARDER TO REACH than the ground it already holds. Self-referential on
        // purpose — it needs no threshold constant, it is scale-free across
        // gentle and broken country alike, and it is exactly "boundaries win
        // ties": a region on a plain keeps spreading, a region ringed by rivers
        // and slopes stops the moment its budget is met.
        if (r.tiles.size() >= dom.hard_min && r.tiles.size() >= r.target
            && r.step_count > 0
            && static_cast<long long>(f.step) * static_cast<long long>(r.step_count)
                   > r.step_cost_sum)
            continue;

        bw.owner[f.tile] = static_cast<uint32_t>(f.region_index) + 1u;
        r.tiles.push_back(f.tile);
        if (f.tile != f.seed)
        {
            r.step_cost_sum += f.step;
            ++r.step_count;
        }
        if (r.tiles.size() >= dom.max_tiles)
            continue; // full — do not extend its frontier any further

        const tile_component& tc = w.tiles.at(f.tile);
        for (int s = 0; s < 6; ++s)
        {
            int nx = 0, ny = 0;
            if (!wrapped_neighbour(tc.grid_x, tc.grid_y, s, bw.gw, bw.gh, nx, ny))
                continue;
            const entity_id n = bw.grid[static_cast<std::size_t>(ny) * bw.gw + nx];
            if (n == null_entity)
                continue;
            const auto nit = w.tiles.find(n);
            // BL-516: growth never leaves its domain. On land this is the old
            // "not ocean" test verbatim; on water it is what stops a sea
            // province reaching ashore or a lake joining the sea.
            if (nit == w.tiles.end() || !in_domain(nit->second.substrate, dom.kind))
                continue;
            if (bw.owner.find(n) != bw.owner.end())
                continue;

            frontier_entry e;
            e.step         = edge_cost_impl(seed, f.tile, tc, n, nit->second, s);
            e.cost         = f.cost + e.step;
            e.seed         = f.seed;
            e.tile         = n;
            e.region_index = f.region_index;
            pq.push(e);
        }
    }
}

} // namespace

province_kind province_kind_of(const world& w, const province& pr)
{
    if (pr.tiles.empty())
        return province_kind::land;
    const auto it = w.tiles.find(pr.tiles.front());
    if (it == w.tiles.end())
        return province_kind::land;
    const terrain_substrate sub = it->second.substrate;
    if (is_open_ocean(sub))
        return province_kind::open_ocean;
    if (is_water(sub))
        return province_kind::coastal_water;
    return province_kind::land;
}

province_kind province_kind_of(const world& w, uint32_t id)
{
    const province* pr = w.provinces.find(id);
    return (pr == nullptr) ? province_kind::land : province_kind_of(w, *pr);
}

// ---------------------------------------------------------------------------
// The province holder (BL-569, province holder)
// ---------------------------------------------------------------------------

void seed_province_holders(world& w)
{
    w.province_holder.assign(w.provinces.provinces.size(), null_entity);

    for (std::size_t i = 0; i < w.provinces.provinces.size(); ++i)
    {
        const province& pr = w.provinces.provinces[i];
        if (province_kind_of(w, pr) != province_kind::land)
            continue; // no_entity for a non-land province (already the default)

        // Tally votes in an ORDERED map keyed on nation id, walking
        // `pr.tiles` in its own ascending order (the partition's contract) —
        // so the tally itself needs no sort. The winner is picked by a single
        // strictly-greater scan over the map's own ascending-key order, which
        // is what makes the tie-break "ascending nation id" fall out for free:
        // the first candidate to reach a given count is the lowest id that
        // ever held it, and a later equal count cannot displace it.
        std::map<entity_id, int> tally;
        for (const entity_id tile : pr.tiles)
        {
            const auto it = w.tile_to_nation.find(tile);
            if (it == w.tile_to_nation.end())
                continue; // unclaimed land tile — no vote
            ++tally[it->second];
        }

        entity_id best       = null_entity;
        int       best_count = 0;
        for (const auto& [nation_id, count] : tally)
        {
            if (count > best_count)
            {
                best_count = count;
                best       = nation_id;
            }
        }
        w.province_holder[i] = best;
    }
}

entity_id province_holder_for(const world& w, uint32_t province_id)
{
    const province* pr = w.provinces.find(province_id);
    if (pr == nullptr)
        return null_entity;
    const auto idx = static_cast<std::size_t>(pr - w.provinces.provinces.data());
    if (idx >= w.province_holder.size())
        return null_entity; // partition built but province_holder not yet seeded
    return w.province_holder[idx];
}

int province_edge_cost(const world& w, uint32_t seed, entity_id a, entity_id b, int side)
{
    const auto ait = w.tiles.find(a);
    const auto bit = w.tiles.find(b);
    if (ait == w.tiles.end() || bit == w.tiles.end())
        return k_province_edge_base_cost;
    return edge_cost_impl(seed, a, ait->second, b, bit->second, side);
}

const province* province_partition::find(uint32_t id) const
{
    const auto it = std::lower_bound(provinces.begin(), provinces.end(), id,
                                     [](const province& p, uint32_t v) { return p.id < v; });
    if (it == provinces.end() || it->id != id)
        return nullptr;
    return &*it;
}

void build_province_partition(world& w, uint32_t seed, province_absorption_stats* stats)
{
    province_partition out;
    out.seed = seed;

    province_absorption_stats tally;

    // `world::bodies` is an UNORDERED map: its iteration order is a container-layout
    // accident, and is not even stable across a copy of the same world. The walk is
    // sorted here explicitly. Province ids no longer encode body rank, so a wrong
    // order can no longer MISNUMBER anything — but it could still change which
    // region reached a contested tile first, so the sort stays load-bearing.
    std::vector<entity_id> body_ids;
    body_ids.reserve(w.bodies.size());
    for (const auto& entry : w.bodies)
        body_ids.push_back(entry.first);
    std::sort(body_ids.begin(), body_ids.end());

    // Population centres are stored centre-keyed in an unordered_map, so the
    // reverse tile -> scale index is gathered into an ORDERED map first. Scale
    // is ACCUMULATED, never first-wins, so the content is independent of the
    // source's iteration order as well as the read order.
    std::map<entity_id, int> centre_scale_by_tile;
    for (const auto& [centre_id, tile_id] : w.population_centre_tile)
    {
        const auto pit = w.population_centres.find(centre_id);
        if (pit == w.population_centres.end())
            continue;
        centre_scale_by_tile[tile_id] += pit->second.scale;
    }

    for (const entity_id body_id : body_ids)
    {
        const body_component& bc = w.bodies.at(body_id);
        const int             gw = bc.grid_width;
        const int             gh = bc.grid_height;
        if (gw <= 0 || gh <= 0)
            continue;

        // The raster is built ONCE and shared by all three domain passes below —
        // they read the same grid and disagree only about which of its tiles they
        // are allowed to claim.
        const std::vector<entity_id> grid = body_tile_grid(w, body_id); // cache stays intact

        // BL-516: LAND, then COASTAL WATER, then OPEN OCEAN. The three passes are
        // the same algorithm with different numbers, over disjoint tile sets, so
        // no pass can take a tile another wanted and the order is not
        // load-bearing. Everything from here to the emit is per-domain.
        for (const domain_spec& dom : k_domains)
        {
        body_work bw;
        bw.gw   = gw;
        bw.gh   = gh;
        bw.grid = grid;

        // Domain mask, ascending tile id. Every tile not of this domain is
        // excluded outright — on land that is the pre-BL-516 "ocean is excluded"
        // rule, spelled generally.
        for (const entity_id t : bw.grid)
        {
            if (t == null_entity)
                continue;
            const auto tit = w.tiles.find(t);
            if (tit == w.tiles.end())
                continue;
            if (!in_domain(tit->second.substrate, dom.kind))
                continue;
            bw.members.push_back(t);
        }
        if (bw.members.empty())
            continue;
        std::sort(bw.members.begin(), bw.members.end());

        std::vector<region>    regions;
        std::vector<entity_id> centre_seeds; ///< Ascending; pass 2 spaces itself off these.

        // --- Pass 1: SETTLEMENT GROWTH.
        //
        // Every population centre on this body is a seed, in ascending tile id,
        // with a growth budget scaled by its centre scale (1 = village ..
        // 5 = metropolis): a metropolis draws a larger province than a village.
        // The budget spans the soft band over the scale's own defined domain,
        // so nothing here is a tuned number — scale 1 gets k_province_min_tiles
        // and scale 5 gets k_province_max_tiles.
        //
        // All seeds grow SIMULTANEOUSLY, as one multi-source fill, so two
        // centres meet on the terrain between them rather than in list order.
        if (dom.settled) // BL-516: only land is settled; water is all hinterland.
        {
            std::vector<std::size_t> active;
            for (const auto& [tile_id, scale] : centre_scale_by_tile) // ascending tile id
            {
                const auto tit = w.tiles.find(tile_id);
                if (tit == w.tiles.end() || tit->second.body != body_id)
                    continue;
                if (!in_domain(tit->second.substrate, dom.kind))
                    continue;

                const int clamped = scale < 1 ? 1 : (scale > 5 ? 5 : scale);
                region    r;
                r.seed   = tile_id;
                r.target = k_province_min_tiles
                           + static_cast<std::size_t>((clamped - 1))
                                 * (k_province_max_tiles - k_province_min_tiles) / 4u;
                centre_seeds.push_back(tile_id);
                active.push_back(regions.size());
                regions.push_back(std::move(r));
            }
            grow_regions(bw, w, seed, dom, regions, active);
        }

        // --- Pass 2: HINTERLAND.
        //
        // Land no centre reached is partitioned under the SAME cost rules,
        // seeded one region at a time from the LEAST-ACCESSIBLE unclaimed tile.
        // Inaccessibility is the sum of the six sides' edge costs, with a side
        // that has no tile OF THIS DOMAIN across it counted as a border at least
        // as strong as a river — a tile ringed by ocean is the most walled-in
        // thing there is, and so is a patch of sea ringed by shore.
        //
        // The order is computed ONCE over the whole land mask, before any
        // hinterland claim, so it is a property of the terrain rather than of
        // the fill's own progress. Walking it most-walled-in first is what makes
        // a hinterland a shape the terrain chose rather than a leftover.
        {
            constexpr int k_no_land_side_cost =
                k_province_edge_base_cost + k_province_river_edge_cost;

            std::vector<std::pair<int, entity_id>> ranked; // (-inaccessibility, tile)
            ranked.reserve(bw.members.size());
            for (const entity_id t : bw.members)
            {
                const tile_component& tc = w.tiles.at(t);
                int                   walls = 0;
                for (int s = 0; s < 6; ++s)
                {
                    int nx = 0, ny = 0;
                    if (!wrapped_neighbour(tc.grid_x, tc.grid_y, s, gw, gh, nx, ny))
                    {
                        walls += k_no_land_side_cost;
                        continue;
                    }
                    const entity_id n = bw.grid[static_cast<std::size_t>(ny) * gw + nx];
                    const auto      nit = (n == null_entity) ? w.tiles.end() : w.tiles.find(n);
                    if (nit == w.tiles.end()
                        || !in_domain(nit->second.substrate, dom.kind))
                    {
                        walls += k_no_land_side_cost;
                        continue;
                    }
                    walls += edge_cost_impl(seed, t, tc, n, nit->second, s);
                }
                ranked.emplace_back(-walls, t);
            }
            // Most walled-in first (negated score ascending), ties by lowest
            // tile id. A total order over unique tile ids.
            std::sort(ranked.begin(), ranked.end());

            // SEED SPACING. The hinterland seeds are chosen BEFORE any of them
            // grows, and grow simultaneously afterwards — so a hinterland
            // province's size comes from how far apart the seeds are and where
            // the terrain lets each one reach, not from the order they were
            // visited in. Seeding one at a time and growing it to its budget
            // was measured to produce a de-facto clamp (a spike at exactly the
            // budget) and a flood of one-tile slivers wedged between finished
            // regions; spacing is what removes both.
            //
            // A chosen seed blocks every LAND tile within
            // k_province_seed_spacing - 1 hex steps of it — a geodesic radius
            // over land, so two seeds either side of a strait are correctly
            // treated as far apart even where the grid says otherwise.
            std::unordered_map<entity_id, bool> seed_blocked;
            const auto block_around = [&](entity_id from) {
                std::vector<entity_id> open{ from };
                std::vector<entity_id> next;
                seed_blocked[from] = true;
                for (int depth = 1; depth < dom.spacing; ++depth)
                {
                    next.clear();
                    for (const entity_id cur : open)
                    {
                        const auto cit = w.tiles.find(cur);
                        if (cit == w.tiles.end())
                            continue;
                        for (int s = 0; s < 6; ++s)
                        {
                            int nx = 0, ny = 0;
                            if (!wrapped_neighbour(cit->second.grid_x, cit->second.grid_y, s, gw,
                                                   gh, nx, ny))
                                continue;
                            const entity_id n = bw.grid[static_cast<std::size_t>(ny) * gw + nx];
                            if (n == null_entity)
                                continue;
                            const auto nit = w.tiles.find(n);
                            if (nit == w.tiles.end()
                                || !in_domain(nit->second.substrate, dom.kind))
                                continue;
                            if (seed_blocked.find(n) != seed_blocked.end())
                                continue;
                            seed_blocked[n] = true;
                            next.push_back(n);
                        }
                    }
                    open = next;
                }
            };

            for (const entity_id c : centre_seeds) // ascending; a centre spaces the
                block_around(c);                   // hinterland off itself too

            std::vector<std::size_t> active;
            for (const auto& [neg_walls, t] : ranked)
            {
                (void)neg_walls;
                if (bw.owner.find(t) != bw.owner.end())
                    continue;
                if (seed_blocked.find(t) != seed_blocked.end())
                    continue;
                region r;
                r.seed   = t;
                r.target = dom.soft_target;
                active.push_back(regions.size());
                regions.push_back(std::move(r));
                block_around(t);
            }
            grow_regions(bw, w, seed, dom, regions, active);

            // --- Leftovers. Land the spaced fill could not reach — enclosed by
            // regions that hit the hard ceiling, or cut off behind a border too
            // expensive for any of them to cross. Seeded in the same fixed
            // least-accessible-first order and grown one at a time, because by
            // now they are pockets rather than open country. This is where a
            // genuinely tiny province comes from, and it is KEPT.
            for (const auto& [neg_walls, t] : ranked)
            {
                (void)neg_walls;
                if (bw.owner.find(t) != bw.owner.end())
                    continue;
                region r;
                r.seed   = t;
                r.target = dom.soft_target;
                const std::size_t ri = regions.size();
                regions.push_back(std::move(r));
                grow_regions(bw, w, seed, dom, regions, { ri });
            }
        }

        // --- Pass 3: SINGLETON ABSORPTION.
        //
        // Ben, 2026-08-21, on seeing the organic borders rendered: "We can add a
        // pass to capture all the 1 tile provinces. I suppose I was wrong when I
        // said 'don't reject'." A one-tile province is a hex with a border drawn
        // round it, and enough of them read as the hex lattice the organic
        // partition exists to hide.
        //
        // THE SCOPE IS EXACTLY ONE TILE. This is NOT the merge-to-floor pass
        // BL-515 removed: that one produced a de-facto clamp, and a two-tile
        // province is still kept here.
        //
        // The target is the neighbour across the CHEAPEST edge under the same
        // cost model that drew the borders, so absorption is the growth logic
        // run once more rather than a second, disagreeing rule. Ties break on
        // the candidate's province id — its lowest member tile — unique across
        // regions by construction, so no container order is ever read.
        //
        // Fixpoint, in ascending singleton-tile order: absorbing one singleton
        // can expose another (its target may itself have been a singleton).
        //
        // A singleton with NO LAND NEIGHBOUR is a TRUE ISLAND and is KEPT.
        // Nothing reaches across water to place it.
        {
            const auto region_id = [&](std::size_t ri) -> entity_id {
                // A region's province id IS its lowest member tile (BL-515).
                entity_id lo  = null_entity;
                bool      any = false;
                for (const entity_id t : regions[ri].tiles)
                    if (!any || t < lo)
                    {
                        lo  = t;
                        any = true;
                    }
                return lo;
            };

            int body_passes = 0;
            for (;;)
            {
                std::vector<std::pair<entity_id, std::size_t>> singles;
                for (std::size_t ri = 0; ri < regions.size(); ++ri)
                    if (regions[ri].tiles.size() == 1)
                        singles.emplace_back(regions[ri].tiles.front(), ri);
                if (singles.empty())
                    break;
                // Ascending by the singleton's tile id: a total order, since a
                // tile belongs to exactly one region.
                std::sort(singles.begin(), singles.end());

                if (body_passes == 0)
                    tally.singletons_before += static_cast<int>(singles.size());
                ++body_passes;

                int absorbed_here = 0;
                for (const auto& [t, ri] : singles)
                {
                    if (regions[ri].tiles.size() != 1)
                        continue; // grew when an earlier singleton chose it

                    const tile_component& tc = w.tiles.at(t);

                    bool        found      = false;
                    int         best_cost  = 0;
                    entity_id   best_key   = null_entity; // candidate's province id
                    std::size_t best_index = 0;

                    for (int s = 0; s < 6; ++s)
                    {
                        int nx = 0, ny = 0;
                        if (!wrapped_neighbour(tc.grid_x, tc.grid_y, s, gw, gh, nx, ny))
                            continue;
                        const entity_id n = bw.grid[static_cast<std::size_t>(ny) * gw + nx];
                        if (n == null_entity)
                            continue;
                        const auto nit = w.tiles.find(n);
                        if (nit == w.tiles.end()
                            || !in_domain(nit->second.substrate, dom.kind))
                            continue;
                        const auto oit = bw.owner.find(n);
                        if (oit == bw.owner.end())
                            continue; // unreachable: every land tile is owned by now
                        const std::size_t nri = static_cast<std::size_t>(oit->second) - 1u;
                        if (nri == ri)
                            continue; // unreachable: ri holds one tile, and it is t

                        const int       cost = edge_cost_impl(seed, t, tc, n, nit->second, s);
                        const entity_id key  = region_id(nri);
                        // CHEAPEST EDGE WINS, FULL OR NOT. The neighbour's size
                        // is deliberately not consulted: a prefer-a-neighbour-
                        // with-room variant was built and measured against this
                        // one (241 over the preferred ceiling vs 1,099, max 14 vs
                        // 16), and DELETED under Ben's 2026-08-21 ruling on
                        // NR-438 — 12 became a preference and 20 the hard cap, so
                        // the breach that was its only justification is now
                        // permitted. Consulting size here would mean choosing a
                        // COSTLIER neighbour, contradicting the cheapest-edge
                        // rule the whole growth model is expressed in.
                        if (!found || cost < best_cost || (cost == best_cost && key < best_key))
                        {
                            found      = true;
                            best_cost  = cost;
                            best_key   = key;
                            best_index = nri;
                        }
                    }

                    if (!found)
                        continue; // TRUE ISLAND — no land neighbour. Kept.

                    regions[best_index].tiles.push_back(t);
                    regions[ri].tiles.clear();
                    bw.owner[t] = static_cast<uint32_t>(best_index) + 1u;
                    ++absorbed_here;
                    ++tally.absorbed;
                    if (regions[best_index].tiles.size() > dom.max_tiles)
                    {
                        ++tally.over_ceiling_created;
                        ++tally.over_ceiling_by_domain[static_cast<std::size_t>(dom.kind)];
                    }
                }

                if (absorbed_here == 0)
                    break; // every remaining singleton is a true island
            }
            if (body_passes > tally.passes)
                tally.passes = body_passes;

            for (const region& r : regions)
                if (r.tiles.size() == 1)
                    ++tally.true_islands;
        }

        // --- Emit. Identity is DERIVED: the lowest member tile id. No id is
        // allocated anywhere in this function.
        for (region& r : regions)
        {
            if (r.tiles.empty())
                continue;
            std::sort(r.tiles.begin(), r.tiles.end());
            province p;
            p.id    = r.tiles.front();
            p.body  = body_id;
            p.tiles = std::move(r.tiles);
            out.provinces.push_back(std::move(p));
        }
        } // domain — BL-516; ids stay unique across domains for the same reason
          // they are unique within one: a tile belongs to exactly one province.
    }

    // Ascending id — the iteration contract. Ids are lowest-member-tile ids and
    // are unique by construction (a tile belongs to exactly one province), so
    // this sort is strict.
    std::sort(out.provinces.begin(), out.provinces.end(),
              [](const province& a, const province& b) { return a.id < b.id; });

    for (const province& p : out.provinces)
        for (const entity_id t : p.tiles)
            out.tile_province[t] = p.id;

    w.provinces = std::move(out);

    if (stats)
        *stats = tally;
}

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------

namespace {

void write_u32(std::ostream& out, uint32_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

bool read_u32(std::istream& in, uint32_t& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

} // namespace

void write_province_section(const province_partition& p, std::ostream& out)
{
    write_u32(out, province_section_magic);
    write_u32(out, province_section_version);
    write_u32(out, p.seed);
    write_u32(out, static_cast<uint32_t>(p.provinces.size()));

    for (const province& pr : p.provinces)
    {
        write_u32(out, pr.id);
        write_u32(out, pr.body);
        write_u32(out, static_cast<uint32_t>(pr.tiles.size()));
        for (const entity_id t : pr.tiles)
            write_u32(out, t);
    }
}

bool read_province_section(province_partition& out, std::istream& in)
{
    out = province_partition{};

    uint32_t magic = 0;
    if (!read_u32(in, magic))
        return true; // clean end of stream — a pre-BL-466 save, which must still load
    if (magic != province_section_magic)
        return false;

    uint32_t version = 0;
    if (!read_u32(in, version) || version != province_section_version)
        return false;

    if (!read_u32(in, out.seed))
        return false;

    uint32_t count = 0;
    if (!read_u32(in, count))
        return false;
    if (count > province_section_max_provinces)
        return false;

    out.provinces.reserve(count);
    uint32_t prev_id  = 0;
    bool     have_prev = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        province pr;
        if (!read_u32(in, pr.id))
            return false;
        if (have_prev && pr.id <= prev_id)
            return false; // ids must be strictly ascending — the iteration contract
        prev_id   = pr.id;
        have_prev = true;

        if (!read_u32(in, pr.body))
            return false;

        uint32_t tiles = 0;
        if (!read_u32(in, tiles))
            return false;
        if (tiles == 0 || tiles > province_section_max_tiles)
            return false;

        pr.tiles.resize(tiles);
        for (uint32_t t = 0; t < tiles; ++t)
            if (!read_u32(in, pr.tiles[t]))
                return false;

        out.provinces.push_back(std::move(pr));
    }

    for (const province& pr : out.provinces)
        for (const entity_id t : pr.tiles)
            out.tile_province[t] = pr.id;

    return true;
}

// ---------------------------------------------------------------------------
// Province building ceiling (BL-513) — the four-input sustain heuristic
// ---------------------------------------------------------------------------
// The shape, the role each of Ben's four inputs plays, and the pinning
// discipline behind the single free coefficient all live in province.hpp beside
// the constants. This file is only the arithmetic.
//
// DETERMINISM. Two ordering hazards exist here and both are closed:
//   * the tile sum walks `province::tiles`, ascending entity id by the partition
//     contract, so the float addition order is fixed;
//   * population centres are stored centre-keyed in an unordered_map, so the
//     reverse tile -> centre lookup is gathered into an ORDERED std::map first
//     and read back in tile order. No unordered iteration order reaches the sum.
// ---------------------------------------------------------------------------

namespace {

/// Tile -> summed population scale standing on it. The map is ordered so the
/// caller reads it in ascending tile order; the CONTENT is independent of the
/// source unordered_map's iteration order because every entry is an
/// accumulation, never a first-wins pick.
std::map<entity_id, int> population_scale_by_tile(const world& w)
{
    std::map<entity_id, int> by_tile;
    for (const auto& [centre_id, tile_id] : w.population_centre_tile)
    {
        const auto pit = w.population_centres.find(centre_id);
        if (pit == w.population_centres.end())
            continue;
        by_tile[tile_id] += pit->second.scale;
    }
    return by_tile;
}

} // namespace

province_sustain measure_province_sustain(const world& w, const province& pr)
{
    province_sustain s;

    // BL-516: A SEA PROVINCE SUSTAINS NOTHING, and says so with a zero rather
    // than by arithmetic accident. Left to the terms below it would report a
    // healthy ceiling — open water carries habitability 0.50 in
    // derive_environment — which would be a lie about a place no building can
    // stand. The ONE-BUILDING FLOOR further down is land's floor specifically,
    // and this is the case that made it need saying: it exists because the
    // partition's own claim is that a land province is habitable ground, and a
    // sea province makes no such claim.
    if (province_kind_of(w, pr) != province_kind::land)
        return s; // every term zero, ceiling 0

    const std::map<entity_id, int> pop_by_tile = population_scale_by_tile(w);

    int pop_scale_total = 0;

    for (const entity_id tile_id : pr.tiles) // ascending, by the partition contract
    {
        const auto tit = w.tiles.find(tile_id);
        if (tit == w.tiles.end())
            continue;
        const tile_component& tc = tit->second;

        ++s.land_tiles;                         // AREA
        s.habitability_area += tc.habitability;  // HABITABILITY
        s.infrastructure_gain +=                 // INFRASTRUCTURE
            tc.habitability * (static_cast<float>(tc.road_level) / k_road_ladder_max);

        const auto pit = pop_by_tile.find(tile_id);
        if (pit != pop_by_tile.end())
            pop_scale_total += pit->second;      // POPULATION
    }

    s.population_factor =
        1.0f + static_cast<float>(pop_scale_total) / k_population_scale_max;
    s.units = (s.habitability_area + s.infrastructure_gain) * s.population_factor;

    // A LAND province that exists at all sustains at least one building. That
    // floor is the partition's own claim that this is habitable land, not a
    // tuning clamp: "some land, room for nothing" would be a contradiction
    // rather than a constraint. Since BL-516 the partition DOES emit sea
    // provinces, and they never reach here — the guard at the top returns a zero
    // sustain for them, because the claim this floor rests on is one only a land
    // province makes.
    const int scaled =
        static_cast<int>(s.units * k_province_buildings_per_sustain_unit + 0.5f);
    s.ceiling = scaled < 1 ? 1 : scaled;
    return s;
}

int province_building_ceiling(const world& w, uint32_t province_id)
{
    const province* pr = w.provinces.find(province_id);
    if (pr == nullptr)
        return -1; // UNKNOWN, never "no room" — see the header's contract.
    return measure_province_sustain(w, *pr).ceiling;
}

int province_buildings_standing(const world& w, uint32_t province_id)
{
    if (province_id == 0)
        return 0;
    // Order-independent: a count, not a fold, so the unordered walk is safe.
    int n = 0;
    for (const auto& [bid, bc] : w.buildings)
        if (w.provinces.province_of(bc.tile) == province_id)
            ++n;
    return n;
}
