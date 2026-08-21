#include "population_generation.hpp"

#include "world/city_names.hpp"     // world::generate_city_name
#include "world/placement_rules.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

    // Target centre count — derived from LAND AREA, not grid area (BL-463).
    //
    // This read `clamp(gw * gh / 1000, 20, 40)` until 2026-08-20. Grid area is a
    // compile-time constant (261×121 = 31,581), so the expression evaluated to
    // exactly 31 on every world ever generated, while the land under it ranged
    // 8,952–16,074 tiles across an eight-seed sweep. No world could be more or
    // less settled than any other.
    //
    // The divisor is MEASURED, not chosen (BL-463 § Direction, not a chosen
    // number; BL-224's report-then-tune discipline). Over the eight-seed
    // baseline sweep run by tools/verify/substrate_census.cpp the shipped
    // generator placed 248 centres over 101,629 land tiles — 409.8 land tiles
    // per centre. `k_land_tiles_per_centre` is that figure rounded, so the
    // CENTRAL density is unchanged and the only thing this line changes is that
    // the count now VARIES with the land the seed actually produced.
    //
    // The bounds are structural, not tuning: at least one centre, and never more
    // centres than there are tiles able to host one.
    // The divisor now lives in the header as `k_land_tiles_per_centre` and
    // arrives as a defaulted parameter, so a harness can measure an alternative
    // without a recompile. Every production caller passes nothing and gets the
    // shipped constant, so the generated world is unchanged.
    const int divisor = (land_tiles_per_centre > 0) ? land_tiles_per_centre
                                                    : k_land_tiles_per_centre;
    int land_tiles = 0;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == body_id && !is_water(tc.substrate)) // BL-516
            ++land_tiles;

    const int centre_count = std::clamp(land_tiles / divisor,
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

        // Draw scale and derive population.
        const int scale = draw_scale(rng);
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
