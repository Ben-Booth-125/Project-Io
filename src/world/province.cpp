#include "province.hpp"

#include "hex_neighbors.hpp"
#include "logistics.hpp" // body_tile_grid — the raster index every body-grid pass walks

#include <algorithm>
#include <cmath>
#include <istream>
#include <limits>
#include <map>
#include <ostream>
#include <queue>
#include <unordered_map>
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
    // No settlement lock (Ben, 2026-09-27, NR-954 B; BL-1150, a centre's fill
    // crosses the settled line). BL-849 locked land growth to the seed's
    // colonisation verdict the way it is locked to the seed's nation; the lock
    // retired, so a centre's region grows from settled ground into the
    // never-settled ground of its own nation, and the nation lock is the one
    // lock left. `w.tile_settled` is no longer an input to this file.
};

/// The domains IN THE ORDER THEY ARE PARTITIONED, land first. The order is not
/// load-bearing — the sets are disjoint, so no domain can take a tile another
/// wanted — but it is fixed anyway so a reader never has to prove that.
const domain_spec k_domains[3] = {
    // Land: the pre-BL-516 band and rules (covered land re-ruled by BL-1133).
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

    /// The nation of the seed tile (null when the ground is nobody's — an
    /// unsettled body, or water). On LAND the fill is nation-locked (BL-611,
    /// province centre anchor; partition ruling 5 — a national border is a
    /// hard edge): growth, leftover seeding and singleton absorption all stay
    /// inside this nation, so a land province is single-nation by
    /// construction and its anchor centre's nation IS its tile-derived
    /// nation. Water is never locked — nations own no water.
    entity_id nation = null_entity;

    // The nation is the region's ONLY lock key. The seed's colonisation verdict
    // was a second one (BL-849) and retired with BL-1150: a centre's region
    // crosses the settled line, so never-settled ground of its nation joins the
    // province of the centre that reaches it.

    /// Sum of the edge costs of the steps that claimed `tiles`, and how many
    /// there were (the seed itself was not stepped to). Their mean is what the
    /// SOFT brake reads — see grow_regions.
    long long   step_cost_sum = 0;
    std::size_t step_count    = 0;

    /// THE WEIGHT ON REACH (Ben, 2026-09-26; BL-1133, a province is its
    /// centre's ground — delegated reading NR-953). What one step of edge cost
    /// adds to this region's path cost. On COVERED land (a body whose centres
    /// seed its land, see build_province_partition) a centre's scale DIVIDES
    /// its step cost, so a metropolis reaches five times as far for the same
    /// ground as a village and still draws the larger province once the budget
    /// no longer stops anyone. Kept integral and exact by scaling every step by
    /// `k_reach_scale_lcm` (60 = lcm(1..5)): the multiplier is 60 / scale, so
    /// 60 / 30 / 20 / 15 / 12 for scales 1-5 and no division ever rounds.
    /// 1 everywhere else — the water domains, an unsettled body's hinterland
    /// and a covered body's leftover ground grow exactly as before.
    long long   reach_mult = 1;
};

/// lcm(1..5): the common scale that keeps the reach weight's division exact.
constexpr long long k_reach_scale_lcm = 60;

/// A frontier entry: reaching @p tile from @p seed's region at total @p cost,
/// where the LAST edge crossed cost @p step. `step` is what the soft-target
/// rule reads — past its budget a region crosses only a binding edge, and that
/// is a property of the edge itself, not of the path that got there.
struct frontier_entry
{
    long long   cost   = 0; ///< Path cost, the step costs times the region's `reach_mult`.
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
/// bit, the height term is an absolute difference, and the jitter fold is over
/// the sorted id pair. Roads are NOT read (BL-623, provinces before roads):
/// the partition runs before generate_roads, so road_level is unstamped when
/// the borders are drawn, and reading it would make a later recompute depend
/// on state the original fill never saw.
int edge_cost_impl(uint32_t seed, entity_id a_id, const tile_component& a, entity_id b_id,
                   const tile_component& b, int side)
{
    const int opposite = (side + 3) % 6;

    int c = k_province_edge_base_cost;

    // A RIVER DIVIDES ITS BANKS (Ben, 2026-10-01, NR-962 B; BL-1156). A river
    // is a run of course tiles (`river_edges != 0`) joined by the steps its
    // bits mark, from each course tile to the next downstream. The fill pays
    // the river's cost to step ONTO or OFF a course sideways — one end a
    // course tile, the step not one the river flows through — so crossing
    // from bank to bank costs it twice, and stepping along the course pays
    // nothing extra. Symmetric: both ends' course-ness and either side's bit.
    // LAND COURSE TILES ONLY: the river's last step sets an inflow bit on the
    // water tile it reaches (its mouth), and the ruling is about banks on land,
    // so a mouth tile charges water-to-water steps nothing and the water
    // domains partition exactly as before (BL-1156 review).
    const bool along = ((a.river_edges >> side) & 1u) != 0u
                       || ((b.river_edges >> opposite) & 1u) != 0u;
    const bool a_course = a.river_edges != 0u && !is_water(a.substrate);
    const bool b_course = b.river_edges != 0u && !is_water(b.substrate);
    if (!along && (a_course || b_course))
        c += k_province_river_edge_cost;

    const float dh = std::fabs(a.height - b.height);
    c += static_cast<int>(dh * static_cast<float>(k_province_height_cost) + 0.5f);

    const uint64_t lo = std::min(a_id, b_id);
    const uint64_t hi = std::max(a_id, b_id);
    c += static_cast<int>(fold(seed, lo, hi) % static_cast<uint64_t>(k_province_edge_jitter));

    return c < 1 ? 1 : c;
}

/// The nation holding @p tile, or null_entity for nobody (water, or an
/// unsettled body). One lookup, shared by seeding, growth and absorption so
/// the lock cannot disagree with itself.
entity_id nation_of_tile(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return (it == w.tile_to_nation.end()) ? null_entity : it->second;
}

/// Grow every region named in @p active SIMULTANEOUSLY as one cost-weighted
/// multi-source fill, claiming into @p bw.owner. Neighbouring seeds therefore
/// meet on the terrain between them rather than in the order they were listed.
///
/// On LAND the fill is NATION-LOCKED (BL-611; ruling 5 — a national border is
/// a hard edge): a region claims only tiles of its seed's nation, so the
/// terrain cost function operates only within a nation's territory and a
/// region's frontier is the border wherever it reaches one. The settled line
/// is NOT a border (BL-1150): a region crosses from settled into never-settled
/// ground of its nation like any other edge. Water domains are never locked.
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
        // and in the shallows, 80 on the open ocean — and NONE on covered land
        // (BL-1133: a province is its centre's ground, so the caller hands a
        // covered body's land an unbounded ceiling and unbounded targets).
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
            // BL-611: the nation lock. Only land is locked, and there only
            // against ground of ANOTHER nation — on an unsettled body every
            // tile's nation is null and the lock never bites.
            if (dom.kind == province_kind::land && nation_of_tile(w, n) != r.nation)
                continue;
            // No settlement lock here (BL-1150; it was BL-849's): the settled
            // / unsettled line is priced like any other edge, by the cost
            // model alone, so never-settled ground joins the centre that
            // reaches it first.

            frontier_entry e;
            e.step         = edge_cost_impl(seed, f.tile, tc, n, nit->second, s);
            e.cost         = f.cost + static_cast<long long>(e.step) * r.reach_mult; // BL-1133
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

std::map<uint32_t, province_anchor> province_anchors(const world& w)
{
    // Gathered into an ORDERED map so no unordered iteration order reaches the
    // pick: the scales of every centre standing on a tile, summed.
    std::map<entity_id, int> centre_scale_by_tile;
    for (const auto& [centre_id, tile_id] : w.population_centre_tile)
    {
        const auto pit = w.population_centres.find(centre_id);
        if (pit == w.population_centres.end())
            continue;
        centre_scale_by_tile[tile_id] += pit->second.scale;
    }

    std::map<uint32_t, province_anchor> out;
    for (const province& pr : w.provinces.provinces)
    {
        // The anchor pick: walk `pr.tiles` in its own ascending order (the
        // partition's contract) with a strictly-greater scan, so the lowest
        // tile id to reach a given scale wins ties for free.
        entity_id anchor       = null_entity;
        int       anchor_scale = 0;
        for (const entity_id tile : pr.tiles)
        {
            const auto cit = centre_scale_by_tile.find(tile);
            if (cit == centre_scale_by_tile.end())
                continue;
            if (cit->second > anchor_scale)
            {
                anchor_scale = cit->second;
                anchor       = tile;
            }
        }
        if (anchor != null_entity)
            out[pr.id] = province_anchor{ anchor, anchor_scale };
    }
    return out;
}

entity_id province_anchor_tile(const world& w, const province& pr)
{
    if (pr.tiles.empty())
        return null_entity;

    // The same per-tile sum `province_anchors` builds, narrowed to this
    // province's tiles, in an ORDERED map so the scan below walks ascending
    // tile id exactly as that function walks `pr.tiles`.
    std::map<entity_id, int> scale_on_tile;
    for (const auto& [centre_id, tile_id] : w.population_centre_tile)
    {
        if (w.provinces.province_of(tile_id) != pr.id)
            continue;
        if (!std::binary_search(pr.tiles.begin(), pr.tiles.end(), tile_id))
            continue; // guards province id 0, which province_of also returns for "none"
        const auto pit = w.population_centres.find(centre_id);
        if (pit == w.population_centres.end())
            continue;
        scale_on_tile[tile_id] += pit->second.scale;
    }

    entity_id anchor       = null_entity;
    int       anchor_scale = 0;
    for (const auto& [tile, scale] : scale_on_tile) // ascending: strict > keeps the lowest id
    {
        if (scale > anchor_scale)
        {
            anchor_scale = scale;
            anchor       = tile;
        }
    }
    return (anchor != null_entity) ? anchor : pr.tiles.front(); // centreless: the lowest-id tile
}

entity_id province_anchor_tile(const std::map<uint32_t, province_anchor>& anchors,
                               const province& pr)
{
    if (pr.tiles.empty())
        return null_entity;
    const auto it = anchors.find(pr.id);
    return (it != anchors.end()) ? it->second.tile : pr.tiles.front(); // centreless: the lowest-id tile
}

void seed_province_holders(world& w)
{
    w.province_holder.assign(w.provinces.provinces.size(), null_entity);

    // BL-611 (province centre anchor): the ANCHOR is the political decider —
    // the province's nation is its anchor centre's nation, and taking the
    // centre takes the province (BL-567's mechanism). The anchor is DERIVED,
    // never stored: the highest summed centre scale standing in the province,
    // ties to the lowest tile id — so it cannot desynchronise from the
    // centres it describes. ONE DERIVATION (`province_anchors`, shared since
    // the BL-1146 review with the charter budget's per-province cap).
    const std::map<uint32_t, province_anchor> anchors = province_anchors(w);

    for (std::size_t i = 0; i < w.provinces.provinces.size(); ++i)
    {
        const province& pr = w.provinces.provinces[i];
        if (province_kind_of(w, pr) != province_kind::land)
            continue; // no_entity for a non-land province (already the default)

        const auto ait = anchors.find(pr.id);
        if (ait != anchors.end())
        {
            const auto nit = w.tile_to_nation.find(ait->second.tile);
            w.province_holder[i] =
                (nit == w.tile_to_nation.end()) ? null_entity : nit->second;
            continue;
        }

        // No centre stands here (an unsettled body's land, or ground the
        // anchor-founding pass has not yet reached): fall back to the
        // pre-BL-611 plurality vote. Ordered tally, strictly-greater scan —
        // the tie-break "ascending nation id" falls out of the map's own
        // ascending-key order.
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
    //
    // BL-611: ANCHOR FOUNDINGS ARE NOT SEEDS. A centre flagged
    // `province_anchor` was founded AFTER this partition shipped, to anchor a
    // province this fill left without one — seeding from it on a rebuild
    // would make the partition a function of its own output. Skipping them
    // keeps the partition a pure function of the pre-anchor centre set and
    // the seed, which is what P6/P7's recompute rows assert.
    std::map<entity_id, int> centre_scale_by_tile;
    for (const auto& [centre_id, tile_id] : w.population_centre_tile)
    {
        const auto pit = w.population_centres.find(centre_id);
        if (pit == w.population_centres.end())
            continue;
        if (pit->second.province_anchor)
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

        // THE EFFECTIVE DOMAIN. Identical to `dom` everywhere but COVERED LAND
        // (BL-1133, below): the land of a body whose centres seed it, where no
        // ceiling stops the fill. Every later pass reads `eff`, so the water
        // domains and an unsettled body's land see exactly the band they did.
        domain_spec eff = dom;
        bool covered = false;

        // --- Pass 1: SETTLEMENT GROWTH — A PROVINCE IS ITS CENTRE'S GROUND.
        //
        // Every population centre on this body is a seed, in ascending tile id,
        // and all of them grow SIMULTANEOUSLY, as one multi-source fill, so two
        // centres meet on the terrain between them rather than in list order.
        //
        // Ben, 2026-09-26 (BL-1133; PROVINCES.md § The partition): the fill no
        // longer stops at a growth budget. Every centre's province grows until
        // its nation's land is covered — across the settled line as well as
        // within it (Ben, 2026-09-27, NR-954 B; BL-1150) — the nation lock
        // still bounds it, nothing else does — so a body has as many provinces
        // as seeded centres and none is left without one. The 20-tile cap and
        // the preferred 12 retire here (they still bind the water domains and
        // an unsettled body's hinterland).
        //
        // THE BUDGET BECOMES A WEIGHT ON REACH (delegated reading NR-953): a
        // centre's scale divides its step cost (`region::reach_mult`), so a
        // metropolis claims ground five times as far off as a village does for
        // the same terrain and still draws the larger province (ruling 1),
        // while competition still decides every border.
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
                r.nation = nation_of_tile(w, tile_id); // BL-611: the lock's key
                r.target     = std::numeric_limits<std::size_t>::max(); // no budget
                r.reach_mult = k_reach_scale_lcm / clamped;             // 60 / scale, exact
                centre_seeds.push_back(tile_id);
                active.push_back(regions.size());
                regions.push_back(std::move(r));
            }
            if (!centre_seeds.empty())
            {
                covered       = true;
                eff.max_tiles = std::numeric_limits<std::size_t>::max(); // no ceiling
                tally.covered_bodies += 1;
            }
            grow_regions(bw, w, seed, eff, regions, active);
        }

        // --- Pass 2: HINTERLAND — water's primary mechanism, land's retired one.
        //
        // BL-611 (province centre anchor) RETIRED the spaced hinterland
        // seeding on settled land: a body that has population centres seeds
        // its land provinces from them ALONE, and only the leftovers mop-up
        // below runs after the centre growth. The spaced pass survives for
        // the two water domains, and for the land of an UNSETTLED body (no
        // centres anywhere — Selene, Cinder, Pallas), where there is nothing
        // else to seed from.
        //
        // Ground the spaced fill (or the centre fill) could not reach is
        // partitioned under the SAME cost rules, seeded one region at a time
        // from the LEAST-ACCESSIBLE unclaimed tile. Inaccessibility is the sum
        // of the six sides' edge costs, with a side that has no tile OF THIS
        // DOMAIN across it counted as a border at least as strong as a river —
        // a tile ringed by ocean is the most walled-in thing there is, and so
        // is a patch of sea ringed by shore.
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

            // BL-611: the spaced seeding runs only where centres did NOT seed
            // — water always, land only on an unsettled body. Settled land
            // goes straight to the leftovers mop-up below.
            if (centre_seeds.empty())
            {
                std::vector<std::size_t> active;
                for (const auto& [neg_walls, t] : ranked)
                {
                    (void)neg_walls;
                    if (bw.owner.find(t) != bw.owner.end())
                        continue;
                    if (seed_blocked.find(t) != seed_blocked.end())
                        continue;
                    region r;
                    r.seed    = t;
                    r.nation  = nation_of_tile(w, t);
                    r.target  = dom.soft_target;
                    active.push_back(regions.size());
                    regions.push_back(std::move(r));
                    block_around(t);
                }
                grow_regions(bw, w, seed, dom, regions, active);
            }

            // --- Leftovers. Ground the primary fill could not reach —
            // enclosed by regions that hit the hard ceiling, or cut off behind
            // a border too expensive to cross. Seeded in the same fixed
            // least-accessible-first order and grown one at a time, because by
            // now they are pockets rather than open country. This is where a
            // genuinely tiny province comes from, and it is KEPT.
            //
            // ON COVERED LAND (BL-1133) nothing stops the centre fill, so the
            // only ground left is ground NO CENTRE OF ITS NATION CAN REACH AT
            // ALL under the nation lock — an UNCENTRED ISLAND: a nation's
            // island or enclave with no centre on it, or land no nation holds.
            // (Never-settled ground a centre of its nation can reach is no
            // longer left here: the settled line is not a lock, BL-1150.) Each
            // is grown unbounded, so one leftover province covers one such
            // island whole, and it is the province the anchor-founding pass
            // (ensure_province_anchor_centres, population_generation.cpp) gives
            // its centre AFTER the partition ships — counted in
            // `uncentred_regions`, never hidden.
            for (const auto& [neg_walls, t] : ranked)
            {
                (void)neg_walls;
                if (bw.owner.find(t) != bw.owner.end())
                    continue;
                region r;
                r.seed    = t;
                r.nation  = nation_of_tile(w, t);
                r.target  = covered ? std::numeric_limits<std::size_t>::max() : dom.soft_target;
                const std::size_t ri = regions.size();
                regions.push_back(std::move(r));
                grow_regions(bw, w, seed, eff, regions, { ri });
                if (covered)
                    ++tally.uncentred_regions;
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
                        // BL-611: absorption honours the nation lock too — a
                        // land singleton joins only a province of its own
                        // nation, or ruling 5's hard edge would dissolve at
                        // exactly the border tiles it exists to draw.
                        if (dom.kind == province_kind::land
                            && regions[nri].nation != regions[ri].nation)
                            continue;
                        // No settlement lock (BL-1150): the settled line is not
                        // a border in growth, so it is not one here either.

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
                        continue; // TRUE ISLAND — no same-domain neighbour it MAY
                                  // join (BL-611: on land, none of its nation). Kept.

                    regions[best_index].tiles.push_back(t);
                    regions[ri].tiles.clear();
                    bw.owner[t] = static_cast<uint32_t>(best_index) + 1u;
                    ++absorbed_here;
                    ++tally.absorbed;
                    // BL-1133: on covered land the only singleton with a
                    // neighbour it may join is a CENTRE's — a leftover covers
                    // its whole uncentred island, so it has none — and the
                    // absorption is the one route by which covered land ends
                    // with fewer provinces than seeded centres. Counted.
                    if (covered)
                        ++tally.covered_centre_singletons_absorbed;
                    // No ceiling on covered land (`eff`), so no breach to count.
                    if (regions[best_index].tiles.size() > eff.max_tiles)
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

/// BL-1079: the state behind a live `province_ceiling_scope` (province.hpp).
/// The two unordered maps are read ONLY by key lookup, never iterated, so their
/// layout cannot reach any result.
struct ceiling_memo
{
    const world*                           w = nullptr;
    bool                                   pop_built = false;
    std::map<entity_id, int>               pop_by_tile;
    std::unordered_map<uint32_t, int>      ceiling;     // province id -> ceiling
    bool                                   standing_built = false;
    std::unordered_map<uint32_t, int>      standing;    // province id -> buildings
};

thread_local ceiling_memo* t_ceiling_memo = nullptr;

/// The live memo for @p w, or null when no scope is open on this world.
ceiling_memo* memo_for(const world& w)
{
    return (t_ceiling_memo != nullptr && t_ceiling_memo->w == &w) ? t_ceiling_memo : nullptr;
}

} // namespace

province_ceiling_scope::province_ceiling_scope(const world& w)
    : m_prev(t_ceiling_memo), m_self(new ceiling_memo)
{
    static_cast<ceiling_memo*>(m_self)->w = &w;
    t_ceiling_memo = static_cast<ceiling_memo*>(m_self);
}

province_ceiling_scope::~province_ceiling_scope()
{
    t_ceiling_memo = static_cast<ceiling_memo*>(m_prev);
    delete static_cast<ceiling_memo*>(m_self);
}

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

    // BL-1079: inside a province_ceiling_scope the map is built once per scope;
    // outside one, per call exactly as before. Same map either way.
    std::map<entity_id, int>        local_pop;
    const std::map<entity_id, int>* pop_src = nullptr;
    if (ceiling_memo* m = memo_for(w))
    {
        if (!m->pop_built)
        {
            m->pop_by_tile = population_scale_by_tile(w);
            m->pop_built   = true;
        }
        pop_src = &m->pop_by_tile;
    }
    else
    {
        local_pop = population_scale_by_tile(w);
        pop_src   = &local_pop;
    }
    const std::map<entity_id, int>& pop_by_tile = *pop_src;

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
    ceiling_memo* m = memo_for(w);
    if (m != nullptr)
        if (const auto it = m->ceiling.find(province_id); it != m->ceiling.end())
            return it->second;
    const int c = measure_province_sustain(w, *pr).ceiling;
    if (m != nullptr)
        m->ceiling.emplace(province_id, c);
    return c;
}

int province_buildings_standing(const world& w, uint32_t province_id)
{
    if (province_id == 0)
        return 0;
    // BL-1079: inside a province_ceiling_scope, one building walk counts every
    // province at once. A count per key, so the walk order still cannot matter.
    if (ceiling_memo* m = memo_for(w))
    {
        if (!m->standing_built)
        {
            for (const auto& [bid, bc] : w.buildings)
                if (const uint32_t pid = w.provinces.province_of(bc.tile); pid != 0)
                    ++m->standing[pid];
            m->standing_built = true;
        }
        const auto it = m->standing.find(province_id);
        return it != m->standing.end() ? it->second : 0;
    }
    // Order-independent: a count, not a fold, so the unordered walk is safe.
    int n = 0;
    for (const auto& [bid, bc] : w.buildings)
        if (w.provinces.province_of(bc.tile) == province_id)
            ++n;
    return n;
}
