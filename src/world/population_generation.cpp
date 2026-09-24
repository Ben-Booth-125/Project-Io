#include "population_generation.hpp"

#include "world/city_names.hpp"     // world::generate_city_name
#include "world/hex_neighbors.hpp"  // the canonical odd-r sides (BL-612 footprints)
#include "world/placement_rules.hpp"
#include "world/settlement.hpp"     // settlement_state — the Era -1 record (BL-610)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// k_population_for_scale moved to population_generation.hpp (BL-616): the
// promotion/decline pass in economy_system.cpp reads the same rungs, and two
// copies of the scale->headcount table is how they drift apart.

/// Weighted scale distribution: indices 0–4 correspond to scale 1–5.
/// Weights: 40%, 30%, 20%, 8%, 2%.
///
/// RETIRED ON THE CAMPAIGN PATH (BL-610, centres from demography): a body with
/// an Era -1 settlement record carves its scales from the simulated region
/// populations instead (`carve_region_scales` below). The draw survives for
/// the no-settlement fallback and for the coverage pass's small-seat draw.
constexpr int k_scale_weight[5] = { 40, 30, 20, 8, 2 };
constexpr int k_scale_weight_total = 100;

/// Draw a scale (1–5) from the weighted distribution.
int draw_scale(std::mt19937& rng)
{
    std::uniform_int_distribution<int> dist(0, k_scale_weight_total - 1);
    int roll = dist(rng);
    int cumulative = 0;
    for (int i = 0; i < 5; ++i)
    {
        cumulative += k_scale_weight[i];
        if (roll < cumulative)
            return i + 1; // scale is 1-based
    }
    return 1; // fallback (shouldn't be reached)
}

// ---------------------------------------------------------------------------
// BL-610 (centres from demography) — the campaign path's count and scales.
// ---------------------------------------------------------------------------

/// The urban share of a region's simulated population, in thousandths. A
/// pre-industrial settlement system towns roughly a tenth of its people; the
/// rest are the countryside the region itself represents. Mechanism from real
/// history, never a name (the standing rule). The centre COUNT and the scale
/// carve both read the urban headcount, so this is the one knob between the
/// demography and the density.
constexpr int64_t k_demography_urban_share_q = 100; // 10%

// The sim-grain rung and the campaign-era rung are the SAME rung (BL-766): a
// region that stood up three centres during the era must materialise three at
// the epoch. Two copies of a constant is how they drift apart, so bind them at
// compile time rather than in a comment.
static_assert(k_demography_heads_per_centre == region_centre_heads,
              "BL-766: the sim-grain centre rung and the campaign-era carve rung "
              "must be the same headcount");

/// Scale banding thresholds in RAW HEADS: the geometric midpoints between the
/// `k_population_for_scale` rungs (10k/50k/200k/1M/5M heads), so a carved share
/// lands on the NEAREST rung in log space rather than always rounding down.
/// sqrt(10k*50k)=22,360; sqrt(50k*200k)=100,000; sqrt(200k*1M)=447,213;
/// sqrt(1M*5M)=2,236,067. Constants, so no float sqrt runs in a gate path.
constexpr int64_t k_scale_band_heads[4] = { 22360, 100000, 447213, 2236067 };

int scale_for_share(int64_t share_heads)
{
    int s = 1;
    for (int i = 0; i < 4; ++i)
        if (share_heads >= k_scale_band_heads[i])
            s = i + 2;
    return s;
}

/// One carved centre: WHICH REGION grew it, and how large it stands.
///
/// BL-783 (a centre stands where its region stood) added the `region` half. The
/// carve used to return a bare scale list, so the count and the scale were the
/// region record's consequence but the PLACE was not — a region sacked twice
/// and a region never touched handed their different counts to the same
/// undifferentiated body-wide placement pass, and the causal story died at the
/// last step.
///
/// BL-1042 (stockpile to budget) adds the SLOT: the key it sorted on and its
/// rank inside its region. The carve already knew both and threw them away; the
/// charter budget needs them, because a region's industry points reach its
/// campaign centres by those slots (INDUSTRIALISATION.md Part III) and nothing after
/// the carve can recover which centre was which region's k-th.
struct carved_centre
{
    int     region = -1; ///< Index into `settlement_state::regions`.
    int     scale  = 1;  ///< 1-5, on `k_population_for_scale`'s own rungs.
    int64_t key    = 0;  ///< The slot key the carve sorted on: `urban_population / rank`.
    int     rank   = 0;  ///< 1-based rank of this centre inside its own region.
};

/// Carve a body's Era -1 demography into centre scales (BL-610), each BOUND to
/// the region that grew it (BL-783).
///
/// COUNT is per region: a living region's urban headcount over
/// `k_demography_heads_per_centre`, floored at one — a region history kept
/// alive has at least a village; a razed region (population 0) contributes
/// nothing. Summing per region rather than carving the body total is what
/// makes the count the DISTRIBUTION's consequence: a world of many thin
/// regions towns differently from one of few fat ones.
///
/// SCALES are rank-size over the whole body's urban headcount: rank i of n
/// receives U/(i*H_n), H_n the harmonic number — one hierarchy of a few
/// cities over many towns over a train of villages, the concentration real
/// settlement systems show (a MECHANISM, never a name — the standing rule).
///
/// WHICH REGION TAKES WHICH RANK (BL-783). The body-wide rank-size share-out is
/// unchanged — same n, same harmonic, same scale multiset — but the ranks are
/// no longer handed out in an arbitrary order. Every region enters its own
/// centres as SLOTS keyed `urban_population / k` for k = 1..centres: the
/// region's internal rank-size read, so a region's first city competes on its
/// whole urban headcount and its fifth on a fifth of it. Sorting those slots
/// descending IS the body-wide rank order, and it is causal in both directions:
/// a heavily sacked region carries a small `urban_population`, so its slots
/// sort late and it materialises fewer AND smaller cities than an untouched
/// neighbour of the same farming ground. That is the whole of R2.
///
/// All integer (harmonic sum in millionths), no RNG: a pure function of the
/// region populations, so count, scale and binding are the demography's
/// consequence and nothing else's.
std::vector<carved_centre> carve_demography_centres(const settlement_state& settlement,
                                                    int heads_per_centre)
{
    std::vector<carved_centre> out;
    if (heads_per_centre <= 0)
        return out;

    /// A candidate rank: the k-th centre of one region, competing on that
    /// region's urban headcount divided by its own internal rank.
    struct slot
    {
        int64_t key = 0;
        int     region = 0;
        int     rank_in_region = 0;
    };
    std::vector<slot> slots;

    // BL-766: the two quantities are now READ, not re-derived. `region::centres`
    // and `region::urban_population` were drawn before the Era -1 sim and moved
    // by it, so the count carries every founding, every sack and every ruin the
    // history produced — which the flat urban share below could not see.
    //
    // `urban_map_drawn` is the discriminator and not the headcount: a world
    // whose cities history razed to the last one and a world where no map was
    // ever drawn both sum to zero, and they want opposite answers.
    int64_t urban_total = 0;
    int64_t count       = 0;
    if (settlement.urban_map_drawn)
    {
        for (std::size_t ri = 0; ri < settlement.regions.size(); ++ri)
        {
            const region& p = settlement.regions[ri];
            if (p.population <= 0 || p.centres <= 0)
                continue; // A razed or emptied region towns nobody.
            urban_total += p.urban_population;
            count       += p.centres;
            for (int k = 1; k <= p.centres; ++k)
                slots.push_back({ p.urban_population / k, static_cast<int>(ri), k });
        }
    }
    else
    {
        // THE PRE-BL-766 CARVE, and it is reachable ONLY from a hand-built
        // settlement record — which in practice means a harness fixture.
        //
        // The first version of this comment also offered "a body whose urban
        // draw did not run", and that state does not exist: `draw_urban_map` is
        // called unconditionally in `make_hard_coded_world`, OUTSIDE the
        // `era_minus_one_enabled` gate, so every generated body has
        // `urban_map_drawn == true` whether or not the era sim ran. Naming an
        // unreachable state as a live one invites the next reader to preserve a
        // branch for a case that cannot occur.
        //
        // Kept unchanged all the same: it is a fallback for fixtures, not a
        // second model to keep in step with the first.
        for (std::size_t ri = 0; ri < settlement.regions.size(); ++ri)
        {
            const region& p = settlement.regions[ri];
            if (p.population <= 0)
                continue;
            const int64_t urban = p.population * k_demography_urban_share_q / 1000;
            const int64_t here  = std::max<int64_t>(1, urban / heads_per_centre);
            urban_total += urban;
            count       += here;
            const int capped = static_cast<int>(std::min<int64_t>(here, 65536));
            for (int k = 1; k <= capped; ++k)
                slots.push_back({ urban / k, static_cast<int>(ri), k });
        }
    }
    if (count <= 0 || slots.empty())
        return out;

    // Descending by key IS the body-wide rank order. The tie-break is TOTAL and
    // layout-free — region index then internal rank, both plain integers — so
    // two runs order identically whatever any container did on the way here
    // (src/world/CLAUDE.md: no pointer- or hash-layout-dependent iteration
    // order, and no "same process" carve-out on that rule).
    std::stable_sort(slots.begin(), slots.end(),
                     [](const slot& a, const slot& b) {
                         if (a.key != b.key) return a.key > b.key;
                         if (a.region != b.region) return a.region < b.region;
                         return a.rank_in_region < b.rank_in_region;
                     });

    const int n = static_cast<int>(
        std::min<int64_t>(std::min<int64_t>(count, static_cast<int64_t>(slots.size())),
                          65536));

    int64_t harmonic_millionths = 0;
    for (int i = 1; i <= n; ++i)
        harmonic_millionths += 1000000 / i;

    // Rank-size share-out of the urban total; already descending by rank. The
    // scale MULTISET is exactly what it was before BL-783 — same n, same
    // harmonic, same c — so the body's scale histogram does not move; only
    // WHICH region receives which rank is new.
    const int64_t c = urban_total * 1000000 / harmonic_millionths;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 1; i <= n; ++i)
    {
        const slot& sl = slots[static_cast<std::size_t>(i - 1)];
        out.push_back({ sl.region, scale_for_share(c / i), sl.key, sl.rank_in_region });
    }
    return out;
}

/// Grid neighbours (cardinal + diagonal = 8 neighbours) with horizontal column
/// wrapping and row clamping. Returns tile entity IDs for valid neighbours.
void eight_neighbours(const std::vector<entity_id>& tile_ids,
                      int col, int row, int gw, int gh,
                      entity_id out[8], int& count)
{
    count = 0;
    for (int dr = -1; dr <= 1; ++dr)
    for (int dc = -1; dc <= 1; ++dc)
    {
        if (dr == 0 && dc == 0)
            continue;
        const int nr = row + dr;
        if (nr < 0 || nr >= gh)
            continue;
        const int nc = ((col + dc) % gw + gw) % gw;
        out[count++] = tile_ids[static_cast<std::size_t>(nr * gw + nc)];
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public implementation
// ---------------------------------------------------------------------------

void generate_population_centres(world& w, entity_id body_id, unsigned seed,
                                 const settlement_state* settlement,
                                 int land_tiles_per_centre)
{
    // Locate the body to get grid dimensions.
    const auto body_it = w.bodies.find(body_id);
    if (body_it == w.bodies.end())
        return;

    const body_component& bc = body_it->second;
    const int gw = bc.grid_width;
    const int gh = bc.grid_height;

    // Collect tile IDs belonging to this body in raster order.
    // world::tiles is an unordered_map keyed by tile entity id; reconstruct
    // raster order via the grid_x / grid_y fields stored in tile_component.
    const int total = gw * gh;
    std::vector<entity_id> tile_ids(static_cast<std::size_t>(total), null_entity);

    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body_id)
            continue;
        const int idx = tc.grid_y * gw + tc.grid_x;
        if (idx >= 0 && idx < total)
            tile_ids[static_cast<std::size_t>(idx)] = tid;
    }

    // Build a list of candidate tiles (non-ocean, habitability > 0).
    std::vector<int> candidates; // raster indices
    candidates.reserve(static_cast<std::size_t>(total));
    for (int i = 0; i < total; ++i)
    {
        const entity_id tid = tile_ids[static_cast<std::size_t>(i)];
        if (tid == null_entity)
            continue;
        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end())
            continue;
        if (placement_rules::can_place_population_centre(it->second))
            candidates.push_back(i);
    }

    if (candidates.empty())
        return;

    // BL-132 change (1): population spawns near rich resources. Deposit
    // richness (the extraction rate multiplier, tile_component::resource_deposit)
    // summed over the extractable set, per candidate — a pure function of
    // generation-time tile data, no tick dependency. Normalised against the
    // richest candidate ON THIS BODY (not a fixed absolute scale, since richness
    // is body-relative and its raw magnitude varies with generation params) into
    // a 1..5 weight bucket, same shape as the existing 1×/3× adjacency weight
    // below so the two compose by multiplication rather than fighting over which
    // wins.
    std::unordered_map<int, float> idx_to_richness;
    idx_to_richness.reserve(candidates.size());
    float max_richness = 0.0f;
    for (const int idx : candidates)
    {
        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        const auto tc_it = w.tiles.find(tid);
        if (tc_it == w.tiles.end())
            continue;
        float sum = 0.0f;
        for (const resource_type er : placement_rules::k_extractable)
            sum += tc_it->second.resource_deposit[static_cast<std::size_t>(er)];
        idx_to_richness[idx] = sum;
        max_richness = std::max(max_richness, sum);
    }
    auto richness_weight = [&](int idx) -> int {
        const auto rit = idx_to_richness.find(idx);
        const float r = (rit != idx_to_richness.end()) ? rit->second : 0.0f;
        return (max_richness > 0.0f)
            ? 1 + static_cast<int>(std::round(4.0f * r / max_richness))
            : 1;
    };

    // BL-766, the tile-grain half of "extra attention to areas where farming
    // would be easy" (Ben, the eight-phase reorder). The region-grain half is
    // the urban map drawn before the sim, which decides HOW MANY centres and
    // HOW LARGE; this decides WHERE on the body they land, and the two compose
    // by multiplication like every other term in this pool.
    //
    // Separate from `richness_weight` on purpose even though that sum already
    // includes agricultural_produce: there, food is one extractable among
    // seven and a rich ore tile drowns it out. Cities stand on ground that
    // feeds them, so the food deposit gets a term of its own — a 1..3 bucket,
    // deliberately narrower than richness's 1..5, so it tilts placement toward
    // farmland without overturning the deposit pull BL-132 established.
    std::unordered_map<int, float> idx_to_farm;
    idx_to_farm.reserve(candidates.size());
    float max_farm = 0.0f;
    for (const int idx : candidates)
    {
        const auto tc_it = w.tiles.find(tile_ids[static_cast<std::size_t>(idx)]);
        if (tc_it == w.tiles.end())
            continue;
        const float f = tc_it->second.resource_deposit[
            static_cast<std::size_t>(resource_type::agricultural_produce)];
        idx_to_farm[idx] = f;
        max_farm = std::max(max_farm, f);
    }
    auto farm_weight = [&](int idx) -> int {
        const auto fit = idx_to_farm.find(idx);
        const float f = (fit != idx_to_farm.end()) ? fit->second : 0.0f;
        return (max_farm > 0.0f)
            ? 1 + static_cast<int>(std::round(2.0f * f / max_farm))
            : 1;
    };

    // Target centre count and scales — BL-610 (centres from demography): on a
    // body with an Era -1 settlement record, BOTH derive from the regions'
    // simulated populations. Density is history's consequence — a world whose
    // history fed more people carries more and larger towns — replacing the
    // land-area divisor and the authored 40/30/20/8/2 weighted draw at once.
    //
    // BL-783 adds the third quantity: each carved centre also carries the
    // REGION that grew it. The carve comes back descending by rank, so the
    // largest cities are placed first and the adjacency weighting below
    // clusters the train of villages around them.
    std::vector<carved_centre> demography_centres;
    if (settlement != nullptr)
        demography_centres = carve_demography_centres(*settlement,
                                                      k_demography_heads_per_centre);
    const bool from_demography = !demography_centres.empty();

    // The FALLBACK: land area over a divisor (BL-463: land area, not grid
    // area). Taken only when there is no settlement record to read — a body
    // without an Era -1 sim, or a harness probing placement alone.
    //
    // The divisor is MEASURED, not chosen (BL-463 § Direction, not a chosen
    // number; BL-224's report-then-tune discipline). Over the eight-seed
    // baseline sweep run by tools/verify/substrate_census.cpp the pre-BL-610
    // generator placed 248 centres over 101,629 land tiles — 409.8 land tiles
    // per centre, rounded into `k_land_tiles_per_centre`.
    //
    // The bounds are structural, not tuning: at least one centre, and never more
    // centres than there are tiles able to host one.
    const int divisor = (land_tiles_per_centre > 0) ? land_tiles_per_centre
                                                    : k_land_tiles_per_centre;
    int land_tiles = 0;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == body_id && !is_water(tc.substrate)) // BL-516
            ++land_tiles;

    const int centre_count = from_demography
        ? std::min(static_cast<int>(demography_centres.size()),
                   static_cast<int>(candidates.size()))
        : std::clamp(land_tiles / divisor,
                     1, static_cast<int>(candidates.size()));

    // Seeded RNG — deterministic, never draws from random_device. Consumed by
    // the FALLBACK path only since BL-783: the campaign path's placement is a
    // pure argmax, seedless like the urban draw it materialises.
    std::mt19937 rng(seed);

    // Track which tiles already host a centre (for adjacency weighting).
    std::unordered_set<int> occupied_indices;
    occupied_indices.reserve(static_cast<std::size_t>(centre_count));

    // Track which raster indices are adjacent to an existing centre.
    std::unordered_set<int> adjacent_indices;
    adjacent_indices.reserve(static_cast<std::size_t>(centre_count * 8));

    // Founding one centre: create the entity, mark the ground taken, and widen
    // the adjacency set. Shared by both placement paths so the two cannot drift
    // in what a founding actually writes.
    //
    // BL-1042: @p slot, when non-null, is the carved centre this founding
    // materialises, and the founding records it in `world::gen_carve_centres`
    // (centre -> region, rank, key). Only the demography path passes one; the
    // fallback's draw, the coverage foundings and the province anchors carry no
    // slot, so they hold no share of any region's industry points. Returns the
    // new centre, or null when nothing was founded.
    auto found_centre = [&](int chosen_idx, int scale,
                            const carved_centre* slot) -> entity_id {
        const entity_id chosen_tile = tile_ids[static_cast<std::size_t>(chosen_idx)];
        if (chosen_tile == null_entity)
            return null_entity;

        const auto tc_it = w.tiles.find(chosen_tile);
        const float hab = (tc_it != w.tiles.end()) ? tc_it->second.habitability : 1.0f;

        const entity_id centre_id = w.create_entity();
        population_centre_component pcc;
        pcc.scale        = scale;
        pcc.population   = k_population_for_scale[scale - 1];
        pcc.habitability = hab;
        w.population_centres[centre_id] = pcc;
        w.population_centre_tile[centre_id] = chosen_tile;
        if (slot != nullptr)
            w.gen_carve_centres[centre_id] = { slot->region, slot->rank, slot->key };

        occupied_indices.insert(chosen_idx);

        const int col = chosen_idx % gw;
        const int row = chosen_idx / gw;
        entity_id nbrs[8];
        int nbr_count = 0;
        eight_neighbours(tile_ids, col, row, gw, gh, nbrs, nbr_count);
        for (int n = 0; n < nbr_count; ++n)
        {
            if (nbrs[n] == null_entity)
                continue;
            const auto nit = w.tiles.find(nbrs[n]);
            if (nit == w.tiles.end())
                continue;
            adjacent_indices.insert(nit->second.grid_y * gw + nit->second.grid_x);
        }
        return centre_id;
    };

    // The ground weight a candidate tile carries, unchanged in its three terms
    // from BL-132 / BL-766: adjacency 1x or 3x, times a 1..5 richness bucket,
    // times a 1..3 food bucket.
    auto ground_weight = [&](int idx) {
        return (adjacent_indices.count(idx) ? 3 : 1)
             * richness_weight(idx)
             * farm_weight(idx); // BL-766: cities stand where the food is.
    };

    if (from_demography)
    {
        // -------------------------------------------------------------------
        // BL-783 — A CENTRE STANDS WHERE ITS REGION STOOD.
        //
        // Before this, the carve handed the count and the scale to a body-wide
        // weighted draw, so nothing bound a placed city back to the farmland
        // that grew it: a region sacked twice and a region never touched fed
        // the same undifferentiated pass, and a player reading the map could
        // not find the war behind a ruin. Now every carved centre carries its
        // source region and is founded on THAT REGION'S OWN GROUND.
        //
        // WHAT "WITHIN THE REGION" MEANS. A region is an anchor, not a stored
        // tile set — the Voronoi over anchors IS its extent, and `nearest_region`
        // is already the canonical read of it (city_names.cpp names a centre in
        // its nearest region's tongue). So the same function partitions the
        // candidates here, which also makes those names correct rather than
        // accidental: the city is now named by the region that actually grew it.
        //
        // SEEDLESS (R3). Placement is a pure argmax over the region's own
        // candidates — the same three ground terms as before, tie-broken on
        // habitability and then on the lowest raster index, both total orders
        // over plain integers. No RNG on this path, matching the urban draw it
        // materialises, and no container walk whose order could vary: the
        // buckets are filled in ascending raster order from the ascending
        // `candidates` list (src/world/CLAUDE.md — no hash-layout-dependent
        // iteration, and no "same process" carve-out on that rule).
        const std::size_t region_count = settlement->regions.size();
        std::vector<std::vector<int>> region_candidates(region_count);
        for (const int idx : candidates)
        {
            const int ri = nearest_region(*settlement, idx % gw, idx / gw, gw);
            if (ri >= 0 && ri < static_cast<int>(region_count))
                region_candidates[static_cast<std::size_t>(ri)].push_back(idx);
        }

        // Best unoccupied candidate in one region, or -1 when it has none left.
        auto best_in_region = [&](int ri) -> int {
            if (ri < 0 || ri >= static_cast<int>(region_count))
                return -1;
            int best_idx = -1;
            long long best_score = -1;
            for (const int idx : region_candidates[static_cast<std::size_t>(ri)])
            {
                if (occupied_indices.count(idx))
                    continue;
                const auto tit = w.tiles.find(tile_ids[static_cast<std::size_t>(idx)]);
                const float hab = (tit != w.tiles.end()) ? tit->second.habitability : 0.0f;
                const int hab_q = std::clamp(static_cast<int>(hab * 1000.0f + 0.5f), 0, 1000);
                const long long score =
                    static_cast<long long>(ground_weight(idx)) * 1001LL + hab_q;
                // Strictly greater: the ascending walk makes the lowest raster
                // index the tie-break, which is a total order and stable.
                if (score > best_score)
                {
                    best_score = score;
                    best_idx   = idx;
                }
            }
            return best_idx;
        };

        // Regions ordered by anchor distance from a given region — the spill
        // order for the rare region that grew more cities than its own ground
        // can host. Built lazily and cached, since most regions never spill.
        std::unordered_map<int, std::vector<int>> spill_order;
        auto spill_for = [&](int ri) -> const std::vector<int>& {
            auto it = spill_order.find(ri);
            if (it != spill_order.end())
                return it->second;
            const region& home = settlement->regions[static_cast<std::size_t>(ri)];
            std::vector<std::pair<int, int>> keyed; // (distance, region index)
            keyed.reserve(region_count);
            for (std::size_t j = 0; j < region_count; ++j)
            {
                if (static_cast<int>(j) == ri) continue;
                const region& o = settlement->regions[j];
                const int dc = std::abs(o.col - home.col);
                const int d  = std::max(std::min(dc, gw - dc), std::abs(o.row - home.row));
                keyed.push_back({ d, static_cast<int>(j) });
            }
            std::sort(keyed.begin(), keyed.end()); // distance, then region index
            std::vector<int> order;
            order.reserve(keyed.size());
            for (const auto& kv : keyed) order.push_back(kv.second);
            return spill_order.emplace(ri, std::move(order)).first->second;
        };

        // BL-1042: where the placement stopped. Every carved centre from here
        // to the end of the carve is DROPPED — the whole body was built out, or
        // the carve handed back more centres than the body has candidate tiles
        // (`centre_count` never attempts those) — and is recorded as such, so
        // the industry points its slot would have held are counted under their
        // own unspent reason rather than silently spread over its siblings.
        int stopped_at = centre_count;
        for (int placed = 0; placed < centre_count; ++placed)
        {
            const carved_centre& cc = demography_centres[static_cast<std::size_t>(placed)];
            int chosen_idx = best_in_region(cc.region);

            // SPILL, counted by construction rather than hidden: a region whose
            // whole extent is already built out still materialises its city, on
            // the nearest region that has ground left. The alternative is to
            // drop it, which would silently lose a settlement history grew.
            if (chosen_idx < 0 && cc.region >= 0
                && cc.region < static_cast<int>(region_count))
            {
                for (const int alt : spill_for(cc.region))
                {
                    chosen_idx = best_in_region(alt);
                    if (chosen_idx >= 0) break;
                }
            }

            if (chosen_idx < 0)
            {
                stopped_at = placed;
                break; // The whole body is built out.
            }

            if (found_centre(chosen_idx, cc.scale, &cc) == null_entity)
                w.gen_carve_dropped.push_back(
                    { cc.region, cc.rank, cc.key, carve_drop_reason::no_tile });
        }
        for (int d = stopped_at; d < static_cast<int>(demography_centres.size()); ++d)
        {
            const carved_centre& dc = demography_centres[static_cast<std::size_t>(d)];
            w.gen_carve_dropped.push_back(
                { dc.region, dc.rank, dc.key, carve_drop_reason::body_built_out });
        }
    }
    else
    {
        // THE FALLBACK, unchanged: no settlement record to bind to, so the
        // weighted seeded draw stands. Reached by a harness probing placement
        // alone, or a body with no Era -1 sim.
        for (int placed = 0; placed < centre_count; ++placed)
        {
            if (candidates.empty())
                break;

            std::vector<int> pool;
            pool.reserve(candidates.size() * 3);
            for (const int idx : candidates)
            {
                if (occupied_indices.count(idx))
                    continue; // already occupied
                const int weight = ground_weight(idx);
                for (int w2 = 0; w2 < weight; ++w2)
                    pool.push_back(idx);
            }

            if (pool.empty())
                break;

            std::uniform_int_distribution<std::size_t> pick(0, pool.size() - 1);
            const int chosen_idx = pool[pick(rng)];
            if (tile_ids[static_cast<std::size_t>(chosen_idx)] == null_entity)
                continue;

            found_centre(chosen_idx, draw_scale(rng), nullptr);
        }
    }

    // Name each population centre on this body. Drawn from an INDEPENDENT seeded stream
    // in sorted-id order — after generation — so assigning names does not consume from
    // the main `rng` and the generated world stays byte-identical (determinism rule).
    {
        std::vector<entity_id> ids;
        for (const auto& [cid, tid] : w.population_centre_tile)
        {
            const auto tit = w.tiles.find(tid);
            if (tit != w.tiles.end() && tit->second.body == body_id)
                ids.push_back(cid);
        }
        std::sort(ids.begin(), ids.end());
        std::mt19937 name_rng(seed ^ 0x9E3779B9u);
        // This pass runs BEFORE the creeds, so there is no culture to name
        // from yet: roll one tongue for the body and name every centre from
        // it, so the placeholder names are at least internally consistent.
        // `name_population_centres` overwrites them per-region once the
        // settlement record exists (BL-290).
        mt_picker picker(name_rng);
        const tongue body_speech = roll_tongue(picker);
        for (entity_id cid : ids)
            w.population_centre_name[cid] = generate_city_name(name_rng, body_speech);
    }
}

// ---------------------------------------------------------------------------
// Coverage pass (BL-463) — every nation holds at least one population centre
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Province anchors (BL-611) — every land province holds a centre
// ---------------------------------------------------------------------------

int ensure_province_anchor_centres(world& w, entity_id body_id, int* gate_relaxed)
{
    if (gate_relaxed != nullptr)
        *gate_relaxed = 0;

    // Which tiles already host a centre. Content is order-independent (a set).
    std::unordered_set<entity_id> host_tiles;
    for (const auto& [cid, tid] : w.population_centre_tile)
        host_tiles.insert(tid);

    int founded = 0;
    for (const province& pr : w.provinces.provinces) // ascending id — the contract
    {
        if (pr.body != body_id)
            continue;
        if (province_kind_of(w, pr) != province_kind::land)
            continue;

        bool      anchored    = false;
        entity_id best_gated  = null_entity; // best tile PASSING the placement gate
        double    gated_score = -1.0;
        entity_id best_any    = null_entity; // least-bad tile, gate or no gate
        double    any_score   = -1.0;

        for (const entity_id tid : pr.tiles) // ascending — strictly-greater argmax
        {
            if (host_tiles.count(tid)) { anchored = true; break; }
            const auto tit = w.tiles.find(tid);
            if (tit == w.tiles.end())
                continue;
            const tile_component& tc = tit->second;

            double richness = 0.0;
            for (const resource_type er : placement_rules::k_extractable)
                richness += static_cast<double>(
                    tc.resource_deposit[static_cast<std::size_t>(er)]);
            const double score = static_cast<double>(tc.habitability) * (1.0 + richness);

            if (score > any_score) { any_score = score; best_any = tid; }
            if (placement_rules::can_place_population_centre(tc) && score > gated_score)
            {
                gated_score = score;
                best_gated  = tid;
            }
        }
        if (anchored)
            continue;

        const entity_id site = (best_gated != null_entity) ? best_gated : best_any;
        if (site == null_entity)
            continue; // no readable tile at all — cannot happen for a real province
        if (best_gated == null_entity && gate_relaxed != nullptr)
            ++*gate_relaxed; // pure ice / bare rock: the anchor stands anyway

        const auto tit = w.tiles.find(site);
        const float hab = (tit != w.tiles.end()) ? tit->second.habitability : 0.0f;

        population_centre_component pcc;
        pcc.scale           = 1; // an anchor founding is a village, always
        pcc.population      = k_population_for_scale[0];
        pcc.habitability    = hab;
        pcc.province_anchor = true; // never a partition seed on a rebuild
        const entity_id centre_id = w.create_entity();
        w.population_centres[centre_id] = pcc;
        w.population_centre_tile[centre_id] = site;

        host_tiles.insert(site);
        ++founded;
    }
    return founded;
}

// ---------------------------------------------------------------------------
// Urban footprints (BL-612) — generation stamps the ground cities stand on
// ---------------------------------------------------------------------------

int stamp_urban_land_use(world& w, entity_id body_id)
{
    const auto body_it = w.bodies.find(body_id);
    if (body_it == w.bodies.end())
        return 0;
    const int gw = body_it->second.grid_width;
    const int gh = body_it->second.grid_height;
    if (gw <= 0 || gh <= 0)
        return 0;

    // Raster lookup, as in generate_population_centres above.
    std::vector<entity_id> grid(static_cast<std::size_t>(gw) * gh, null_entity);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body_id)
            continue;
        const int idx = tc.grid_y * gw + tc.grid_x;
        if (idx >= 0 && idx < gw * gh)
            grid[static_cast<std::size_t>(idx)] = tid;
    }

    // Centres on this body, in sorted centre-id order — the walk is
    // deterministic and, because stamping is idempotent, the RESULT is
    // order-independent besides.
    std::vector<std::pair<entity_id, entity_id>> centres; // (centre, tile)
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit != w.tiles.end() && tit->second.body == body_id)
            centres.emplace_back(cid, tid);
    }
    std::sort(centres.begin(), centres.end());

    int stamped = 0;
    const auto stamp = [&](entity_id tid) {
        auto& lu = w.land_use[tid]; // absent entry default-constructs undeveloped
        if (lu.use != land_use_component::type::urban)
        {
            lu.use = land_use_component::type::urban;
            ++stamped;
        }
    };

    for (const auto& [cid, tid] : centres)
    {
        const auto pit = w.population_centres.find(cid);
        if (pit == w.population_centres.end())
            continue;
        const int scale = std::clamp(pit->second.scale, 1, 5);
        const int want  = k_urban_footprint_tiles[scale - 1];

        stamp(tid); // a settlement always paves its own tile

        if (want <= 1)
            continue;

        // Rank the six hex neighbours (habitability desc, tile id asc) and
        // pave the best `want - 1` land tiles among them. The coast can cut a
        // footprint short, and that is kept rather than compensated.
        const auto tit = w.tiles.find(tid);
        if (tit == w.tiles.end())
            continue;
        struct cand { float hab; entity_id tile; };
        std::vector<cand> ring;
        ring.reserve(6);
        for (int s = 0; s < 6; ++s)
        {
            const auto [nx_raw, ny] =
                hex_neighbors::neighbour(tit->second.grid_x, tit->second.grid_y, s);
            if (ny < 0 || ny >= gh)
                continue; // rows clamp; columns wrap (the east-west cylinder)
            const int nx = ((nx_raw % gw) + gw) % gw;
            const entity_id n = grid[static_cast<std::size_t>(ny) * gw + nx];
            if (n == null_entity)
                continue;
            const auto nit = w.tiles.find(n);
            if (nit == w.tiles.end() || is_water(nit->second.substrate))
                continue; // urban ground is a land feature
            ring.push_back({ nit->second.habitability, n });
        }
        std::sort(ring.begin(), ring.end(), [](const cand& a, const cand& b) {
            if (a.hab != b.hab)
                return a.hab > b.hab; // the city grows onto its most livable side
            return a.tile < b.tile;
        });
        const int extra = std::min<int>(want - 1, static_cast<int>(ring.size()));
        for (int i = 0; i < extra; ++i)
            stamp(ring[static_cast<std::size_t>(i)].tile);
    }

    return stamped;
}

int ensure_national_population_centres(world& w, entity_id body_id, unsigned seed)
{
    // Why this is a SECOND pass rather than a bigger number in the first one.
    //
    // generate_population_centres runs BEFORE generate_nations (Pass 6's
    // substrate density reads the centres while territory is assigned), so it
    // cannot know how many nations there will be, nor where their borders fall.
    // Raising its target until coverage happened to come out right would be
    // exactly the clamp-and-move-on this item exists to remove: the count would
    // still be a guess, and a nation could still draw an empty hand.
    //
    // So the nation term in "derive the target from land area AND nation count"
    // is applied HERE, after the borders exist, as a structural guarantee: one
    // founding for each nation that holds none. The world's centre count is then
    // land-derived plus nation-derived, and F1 (`every nation holds at least one
    // population centre`, tools/verify/substrate_census.cpp) is true by
    // construction rather than by luck of the draw.
    //
    // Deterministic: nations are visited in sorted-id order, the founding tile is
    // a pure argmax over that nation's own tiles with a first-best tie-break over
    // a sorted list, and the only RNG draw is the scale — from a stream seeded
    // here and touched by nothing else.
    const auto body_it = w.bodies.find(body_id);
    if (body_it == w.bodies.end())
        return 0;

    // Which nations already hold a centre, and which tiles are already hosts.
    std::unordered_set<entity_id> covered;
    std::unordered_set<entity_id> host_tiles;
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        host_tiles.insert(tid);
        const auto nit = w.tile_to_nation.find(tid);
        if (nit != w.tile_to_nation.end())
            covered.insert(nit->second);
    }

    // Nations with territory on this body, in sorted-id order.
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& [nid, nc] : w.nations)
    {
        bool on_body = false;
        for (const entity_id tid : nc.tiles)
        {
            const auto tit = w.tiles.find(tid);
            if (tit != w.tiles.end() && tit->second.body == body_id) { on_body = true; break; }
        }
        if (on_body)
            nation_ids.push_back(nid);
    }
    std::sort(nation_ids.begin(), nation_ids.end());

    std::mt19937 rng(seed);
    int founded = 0;

    for (const entity_id nid : nation_ids)
    {
        if (covered.count(nid))
            continue;

        // Best founding site in this nation: habitable, placeable, unoccupied,
        // scored on habitability weighted by extractable deposit richness — the
        // same two quantities the primary pass weights by, so a coverage founding
        // lands where the primary pass would have wanted to put one.
        entity_id best_tile  = null_entity;
        double    best_score = -1.0;

        std::vector<entity_id> tiles = w.nations.at(nid).tiles;
        std::sort(tiles.begin(), tiles.end());

        for (const entity_id tid : tiles)
        {
            if (host_tiles.count(tid))
                continue;
            const auto tit = w.tiles.find(tid);
            if (tit == w.tiles.end() || tit->second.body != body_id)
                continue;
            const tile_component& tc = tit->second;
            if (!placement_rules::can_place_population_centre(tc))
                continue;

            double richness = 0.0;
            for (const resource_type er : placement_rules::k_extractable)
                richness += static_cast<double>(tc.resource_deposit[static_cast<std::size_t>(er)]);

            const double score = static_cast<double>(tc.habitability) * (1.0 + richness);
            if (score > best_score) { best_score = score; best_tile = tid; }
        }

        if (best_tile == null_entity)
            continue; // a nation of pure ice or bare rock genuinely supports nobody

        const auto tit = w.tiles.find(best_tile);
        const float hab = (tit != w.tiles.end()) ? tit->second.habitability : 1.0f;

        // A coverage founding is a SMALL place. It is the seat a nation was always
        // implied to have, not a metropolis conjured to hit a number, so it draws
        // from the bottom of the same weighted distribution (scales 1-3) rather
        // than the full 1-5 range.
        const int scale = std::min(draw_scale(rng), 3);

        const entity_id centre_id = w.create_entity();
        population_centre_component pcc;
        pcc.scale        = scale;
        pcc.population   = k_population_for_scale[scale - 1];
        pcc.habitability = hab;
        w.population_centres[centre_id] = pcc;
        w.population_centre_tile[centre_id] = best_tile;

        host_tiles.insert(best_tile);
        covered.insert(nid);
        ++founded;
    }

    return founded;
}
