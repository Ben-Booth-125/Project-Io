#include "population_generation.hpp"

#include "world/city_names.hpp"     // world::generate_city_name
#include "world/hex_neighbors.hpp"  // the canonical odd-r sides (BL-612 footprints)
#include "world/placement_rules.hpp"
#include "world/settlement.hpp"     // settlement_state — the Era -1 record (BL-610)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

/// Headcount in thousands for each scale level 1–5.
constexpr int k_population_for_scale[5] = { 10, 50, 200, 1000, 5000 };

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

/// Carve a body's Era -1 demography into centre scales (BL-610).
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
/// Carved body-wide rather than per region because placement is body-wide
/// too: the region record decides HOW MANY and HOW LARGE, the placement pass
/// decides where.
///
/// All integer (harmonic sum in millionths), no RNG: a pure function of the
/// region populations, so count and scale are the demography's consequence
/// and nothing else's.
std::vector<int> carve_demography_scales(const settlement_state& settlement,
                                         int heads_per_centre)
{
    std::vector<int> out;
    if (heads_per_centre <= 0)
        return out;

    int64_t urban_total = 0;
    int64_t count       = 0;
    for (const region& p : settlement.regions)
    {
        if (p.population <= 0)
            continue;
        const int64_t urban = p.population * k_demography_urban_share_q / 1000;
        urban_total += urban;
        count       += std::max<int64_t>(1, urban / heads_per_centre);
    }
    if (count <= 0)
        return out;

    const int n = static_cast<int>(std::min<int64_t>(count, 65536));

    int64_t harmonic_millionths = 0;
    for (int i = 1; i <= n; ++i)
        harmonic_millionths += 1000000 / i;

    // Rank-size share-out of the urban total; already descending by rank.
    const int64_t c = urban_total * 1000000 / harmonic_millionths;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 1; i <= n; ++i)
        out.push_back(scale_for_share(c / i));
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

    // Target centre count and scales — BL-610 (centres from demography): on a
    // body with an Era -1 settlement record, BOTH derive from the regions'
    // simulated populations. Density is history's consequence — a world whose
    // history fed more people carries more and larger towns — replacing the
    // land-area divisor and the authored 40/30/20/8/2 weighted draw at once.
    //
    // The carve comes back descending by rank, so the largest cities are
    // placed first and the adjacency weighting below clusters the train of
    // villages around them.
    std::vector<int> demography_scales;
    if (settlement != nullptr)
        demography_scales = carve_demography_scales(*settlement,
                                                    k_demography_heads_per_centre);
    const bool from_demography = !demography_scales.empty();

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
        ? std::min(static_cast<int>(demography_scales.size()),
                   static_cast<int>(candidates.size()))
        : std::clamp(land_tiles / divisor,
                     1, static_cast<int>(candidates.size()));

    // Seeded RNG — deterministic, never draws from random_device.
    std::mt19937 rng(seed);

    // Track which tiles already host a centre (for adjacency weighting).
    std::unordered_set<int> occupied_indices;
    occupied_indices.reserve(static_cast<std::size_t>(centre_count));

    // Track which raster indices are adjacent to an existing centre.
    std::unordered_set<int> adjacent_indices;
    adjacent_indices.reserve(static_cast<std::size_t>(centre_count * 8));

    // Place centres one at a time, rebuilding the weighted candidate pool each
    // time so agglomeration is progressive (each placed centre attracts the next).
    for (int placed = 0; placed < centre_count; ++placed)
    {
        if (candidates.empty())
            break;

        // Build weighted candidate list: tiles adjacent to an existing centre
        // get 3× weight; others get 1×. BL-132 change (1): multiplied by a
        // 1..5 richness weight so a rich, unclaimed deposit competes with (and
        // can outweigh) a merely-adjacent tile, rather than richness only ever
        // acting as a tie-breaker within the adjacency tier.
        std::vector<int> pool;
        pool.reserve(candidates.size() * 3);
        for (const int idx : candidates)
        {
            if (occupied_indices.count(idx))
                continue; // already occupied
            const int weight = (adjacent_indices.count(idx) ? 3 : 1) * richness_weight(idx);
            for (int w2 = 0; w2 < weight; ++w2)
                pool.push_back(idx);
        }

        if (pool.empty())
            break;

        std::uniform_int_distribution<std::size_t> pick(0, pool.size() - 1);
        const int chosen_idx = pool[pick(rng)];
        const entity_id chosen_tile = tile_ids[static_cast<std::size_t>(chosen_idx)];
        if (chosen_tile == null_entity)
            continue;

        // Read habitability from the tile.
        const auto tc_it = w.tiles.find(chosen_tile);
        const float hab = (tc_it != w.tiles.end()) ? tc_it->second.habitability : 1.0f;

        // The scale: carved from the demography on the campaign path (BL-610,
        // largest first — the sort above), drawn from the weighted table on the
        // fallback. Either way `k_population_for_scale` stays the one
        // scale -> headcount mapping.
        const int scale = from_demography
            ? demography_scales[static_cast<std::size_t>(placed)]
            : draw_scale(rng);
        const int pop   = k_population_for_scale[scale - 1];

        // Create the population centre entity.
        const entity_id centre_id = w.create_entity();
        population_centre_component pcc;
        pcc.scale        = scale;
        pcc.population   = pop;
        pcc.habitability = hab;
        w.population_centres[centre_id] = pcc;
        w.population_centre_tile[centre_id] = chosen_tile;

        // Mark the tile occupied and update adjacency set.
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
            // Find the raster index for this neighbour.
            const auto nit = w.tiles.find(nbrs[n]);
            if (nit == w.tiles.end())
                continue;
            const int nidx = nit->second.grid_y * gw + nit->second.grid_x;
            adjacent_indices.insert(nidx);
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
