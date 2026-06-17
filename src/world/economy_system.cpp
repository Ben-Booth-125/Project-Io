#include "economy_system.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace {

// --- Deposit depletion constants (TODO § Environment, settled 2026-06-15) ---
// Each deposit carries a finite reserve (tile_component.resource_remaining, seeded
// at generation to richness × a reserve factor). Extraction draws the reserve down;
// as it nears empty the output tapers, then the building reports "out of resources".
// Hard-coded sensible estimates, iterated by playtest — not derived.
constexpr float deposit_taper_ticks = 8.0f;  ///< Output tapers over the last ~8 ticks of nominal yield.
constexpr float deposit_min_taper   = 0.05f; ///< Below 5% of nominal the reserve reads as exhausted.

/// Body that a building sits on (via its tile). null_entity if the tile is gone.
entity_id building_body(const world& w, const building_component& b)
{
    const auto it = w.tiles.find(b.tile);
    return (it != w.tiles.end()) ? it->second.body : null_entity;
}

/// Extraction: credit the (corp, body) pool with the target resource and draw the
/// same amount from the tile's finite reserve. Nominal output is
/// base_rate × richness × workforce × (1 − hazard); it tapers as the reserve nears
/// empty and the building reports `exhausted` once the reserve falls below the
/// minimum taper of nominal. Deposits never refill.
building_report run_extraction(world& w, const recipe_registry& reg,
                               entity_id corp, entity_id building_id,
                               const building_component& b, float contention)
{
    building_report rep;
    rep.building        = building_id;
    rep.corp            = corp;
    rep.type            = b.type;
    rep.target_resource = b.target_resource;

    // Effective labour: the requested target throttled by the (corp, body) pool's
    // contention scalar (POPULATION.md § Workforce model, step 1).
    const float effective_workforce = b.workforce_assigned * contention;
    rep.effective_workforce = effective_workforce;

    const entity_id body = building_body(w, b);
    rep.body = body;

    const auto tile_it = w.tiles.find(b.tile);
    if (body == null_entity || tile_it == w.tiles.end())
    {
        rep.idle = true;
        return rep;
    }

    tile_component& tc = tile_it->second;
    const std::size_t ri = static_cast<std::size_t>(b.target_resource);
    const float richness = tc.resource_deposit[ri];
    const float base_rate = reg.economics(building_type::extraction_site).base_rate;

    // Apply the player's workforce target (0–200 % of nominal capacity).
    const float wt_scalar = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);

    // Rate the deposit would yield at full reserve (richness sets the rate).
    const float nominal = base_rate * richness * effective_workforce * wt_scalar * (1.0f - tc.hazard_level);
    if (nominal <= 0.0f)
    {
        rep.idle = true; // unstaffed, no deposit of the target, or fully hazardous
        return rep;
    }

    float& remaining = tc.resource_remaining[ri];

    // Taper output as the reserve approaches empty: full until the last
    // (taper_ticks × nominal) of reserve, then linearly down toward the floor.
    const float taper_band = deposit_taper_ticks * nominal;
    const float taper = std::clamp(remaining / taper_band, 0.0f, 1.0f);
    if (taper < deposit_min_taper)
    {
        rep.exhausted = true; // out of resources — the reserve is spent
        return rep;
    }

    // Never extract more than the reserve still holds.
    const float output = std::min(nominal * taper, remaining);
    remaining -= output;

    if (output > 0.0f)
    {
        w.pool_for(corp, body).quantities[ri] += output;
        rep.active          = true;
        rep.output_quantity = output;
    }
    else
    {
        rep.exhausted = true;
    }
    return rep;
}

/// Processing: run the recipe pool-first under the two-threshold model, recording
/// the auto-bought shortfall into `purchases`.
building_report run_processing(world& w, const recipe_registry& reg,
                               entity_id corp, entity_id building_id,
                               const building_component& b,
                               bool body_has_market,
                               float contention,
                               economy_report& out)
{
    building_report rep;
    rep.building = building_id;
    rep.corp     = corp;
    rep.type     = b.type;
    rep.recipe   = b.recipe;

    // Effective labour: the requested target throttled by the (corp, body) pool's
    // contention scalar (POPULATION.md § Workforce model, step 1).
    const float effective_workforce = b.workforce_assigned * contention;
    rep.effective_workforce = effective_workforce;

    const entity_id body = building_body(w, b);
    rep.body = body;

    const recipe* rcp = reg.get_recipe(b.recipe);
    // Apply the player's workforce target (0–200 % of nominal capacity).
    const float wt_scalar    = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
    const float batches_full =
        reg.economics(building_type::processing_facility).base_rate * effective_workforce * wt_scalar;

    if (body == null_entity || rcp == nullptr || batches_full <= 0.0f)
    {
        rep.idle = true; // misconfigured, unstaffed, or detached
        return rep;
    }

    stockpile_component& pool = w.pool_for(corp, body);

    // Pool coverage of a full run = the scarcest input's pool fraction.
    float coverage = std::numeric_limits<float>::infinity();
    bool  has_input = false;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float in = rcp->inputs[r];
        if (in <= 0.0f)
            continue;
        has_input = true;
        const float need = in * batches_full;
        const float cov  = (need > 0.0f) ? pool.quantities[r] / need : std::numeric_limits<float>::infinity();
        if (cov < coverage)
        {
            coverage              = cov;
            rep.limiting_input    = static_cast<resource_type>(r);
            rep.has_limiting      = true;
        }
    }
    if (!has_input)
        coverage = 1.0f; // degenerate no-input recipe runs full

    // Run fraction. With a market on the body the building runs a full batch,
    // drawing inputs pool-first and auto-buying any shortfall (the Layer 3 path).
    // Without a market it falls back to the two-threshold partial run from its
    // own pool: full at/above t_full, scaled between t_idle and t_full, idle below.
    float run = 0.0f;
    if (body_has_market)
    {
        run = 1.0f;
    }
    else if (coverage >= reg.t_full())
    {
        run = 1.0f;
    }
    else if (coverage >= reg.t_idle())
    {
        run = coverage; // scale output to the limiting input
    }
    else
    {
        rep.idle = true; // too little to bootstrap and nowhere to buy
        return rep;
    }

    const float batches = batches_full * run;

    // Consume inputs pool-first; the remainder (only possible with a market) is
    // auto-bought and recorded as this tick's purchases / market demand.
    auto& bought = out.purchases[std::make_pair(corp, body)];
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float in = rcp->inputs[r];
        if (in <= 0.0f)
            continue;
        const float need      = in * batches;
        const float from_pool = std::min(pool.quantities[r], need);
        pool.quantities[r] -= from_pool;
        bought[r]          += (need - from_pool); // 0 when the pool covered it
    }

    float produced = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float outq = rcp->outputs[r] * batches;
        if (outq <= 0.0f)
            continue;
        pool.quantities[r] += outq;
        produced           += outq;
    }

    rep.active          = true;
    rep.output_quantity = produced;
    return rep;
}

} // namespace

economy_report run_economy_step(world& w, const recipe_registry& reg)
{
    economy_report report;

    // Bodies that host a market — a processor on one auto-buys input shortfalls
    // rather than idling for want of pool stock (see run_processing).
    std::unordered_set<entity_id> bodies_with_market;
    bodies_with_market.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
        bodies_with_market.insert(mc.body);

    // BL-042: Derive per-body workforce supply from population centres.
    // Scale → labour-force table (units available to industry on this body).
    static constexpr float labour_by_scale[6] = { 0.0f, 1.0f, 3.0f, 10.0f, 30.0f, 100.0f };
    std::map<entity_id, float> pop_supply_by_body;
    // BL-041: Habitability cap — weighted mean habitability of population centres per body.
    // weight = centre scale; cap = min(1, mean_hab / 0.6); default 1.0 (uncapped) when no centres.
    std::map<entity_id, float> hab_weighted_sum;
    std::map<entity_id, float> hab_weight_total;
    for (const auto& [cid, pcc] : w.population_centres)
    {
        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto tc_it = w.tiles.find(tile_it->second);
        if (tc_it == w.tiles.end())
            continue;
        const int sc = std::clamp(pcc.scale, 1, 5);
        pop_supply_by_body[tc_it->second.body] += labour_by_scale[sc];
        const entity_id body = tc_it->second.body;
        const float weight = static_cast<float>(sc);
        hab_weighted_sum[body]  += pcc.habitability * weight;
        hab_weight_total[body]  += weight;
    }
    // Derive hab_cap_by_body: min(1, mean_hab / 0.6). Bodies with no centres get cap 1.0.
    auto hab_cap_for = [&](entity_id body) -> float {
        const auto wit = hab_weight_total.find(body);
        if (wit == hab_weight_total.end() || wit->second <= 0.0f)
            return 1.0f;
        const float mean_hab = hab_weighted_sum.at(body) / wit->second;
        return std::min(1.0f, mean_hab / 0.6f);
    };
    // Building counts per (corp, body) for apportionment.
    std::map<std::pair<entity_id, entity_id>, int> bldg_count_by_corp_body;
    std::map<entity_id, int>                        bldg_count_by_body;
    for (const auto& [corp, cc] : w.corporations)
    {
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const entity_id body = building_body(w, bit->second);
            if (body == null_entity)
                continue;
            ++bldg_count_by_corp_body[{corp, body}];
            ++bldg_count_by_body[body];
        }
    }

    // Visit corporations in ascending id order for deterministic output.
    std::vector<entity_id> corp_ids;
    corp_ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());

    for (const entity_id corp : corp_ids)
    {
        const corporation_component& cc = w.corporations.at(corp);

        // Workforce pool, step 1: sum this corp's labour demand on each body
        // (the requested workforce_assigned of every producing building there),
        // then derive a per-body contention scalar min(1, supply/demand). Below 1
        // it throttles every building on that (corp, body) uniformly.
        // BL-041: demand is capped by the body's habitability scalar before summing.
        std::map<entity_id, float> demand_by_body;
        for (const entity_id building_id : cc.assets)
        {
            const auto bit = w.buildings.find(building_id);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            if (b.type != building_type::extraction_site &&
                b.type != building_type::processing_facility)
                continue; // ports and none demand no labour in L3
            const entity_id body = building_body(w, b);
            if (body != null_entity)
                demand_by_body[body] += b.workforce_assigned * hab_cap_for(body);
        }

        std::map<entity_id, float> contention_by_body;
        for (const auto& [body, demand] : demand_by_body)
        {
            const float pop_total = [&]() -> float {
                const auto it = pop_supply_by_body.find(body);
                return (it != pop_supply_by_body.end()) ? it->second : 0.0f;
            }();
            const int corp_bldgs  = [&]() -> int {
                const auto it = bldg_count_by_corp_body.find({corp, body});
                return (it != bldg_count_by_corp_body.end()) ? it->second : 0;
            }();
            const int total_bldgs = [&]() -> int {
                const auto it = bldg_count_by_body.find(body);
                return (it != bldg_count_by_body.end()) ? it->second : 0;
            }();
            const float share  = (total_bldgs > 0) ? static_cast<float>(corp_bldgs) / static_cast<float>(total_bldgs) : 1.0f;
            const float supply = (pop_total > 0.0f) ? pop_total * share : w.workforce_supply(corp, body);
            const float scalar = (demand > supply && demand > 0.0f) ? supply / demand : 1.0f;
            contention_by_body[body] = scalar;
            report.workforce_contention[std::make_pair(corp, body)] = scalar;
        }

        auto contention_for = [&](entity_id body) {
            const auto it = contention_by_body.find(body);
            return (it != contention_by_body.end()) ? it->second : 1.0f;
        };

        for (const entity_id building_id : cc.assets)
        {
            const auto bit = w.buildings.find(building_id);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            const entity_id body = building_body(w, b);

            // Decommissioned buildings produce nothing — skip production entirely.
            if (b.decommissioned)
                continue;

            switch (b.type)
            {
                case building_type::extraction_site:
                    report.buildings.push_back(
                        run_extraction(w, reg, corp, building_id, b, contention_for(body)));
                    break;
                case building_type::processing_facility:
                {
                    const bool has_market = bodies_with_market.count(body) != 0;
                    report.buildings.push_back(
                        run_processing(w, reg, corp, building_id, b, has_market,
                                       contention_for(body), report));
                    break;
                }
                default:
                    break; // ports and none take no production action in L3
            }
        }
    }

    // Population demand pass: each population centre adds a stub demand of
    // 1 unit of agricultural_produce per scale level into the body's market.
    // stub: replace with food_rations demand when the processing pipeline
    // connects food production end-to-end (food_rations requires agricultural
    // produce → processing_facility recipe, deferred).
    for (const auto& [centre_id, pcc] : w.population_centres)
    {
        const auto tile_it = w.population_centre_tile.find(centre_id);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto tc_it = w.tiles.find(tile_it->second);
        if (tc_it == w.tiles.end())
            continue;
        const entity_id body = tc_it->second.body;

        // Find the market for this body and add demand.
        for (auto& [mid, mc] : w.markets)
        {
            if (mc.body != body)
                continue;
            const std::size_t ri = static_cast<std::size_t>(resource_type::agricultural_produce);
            mc.demand[ri] += static_cast<float>(pcc.scale);
            break; // one market per body in the prototype
        }
    }

    // BL-048A: Body habitability aggregate — weighted mean of population-centre
    // tile habitability, where weight = population centre scale.
    {
        std::map<entity_id, std::pair<float, float>> hab_sum; // body → (weighted_sum, weight)
        for (const auto& [cid, pcc] : w.population_centres)
        {
            const auto tile_it = w.population_centre_tile.find(cid);
            if (tile_it == w.population_centre_tile.end())
                continue;
            const auto tc_it = w.tiles.find(tile_it->second);
            if (tc_it == w.tiles.end())
                continue;
            const float w_scale = static_cast<float>(std::max(1, pcc.scale));
            hab_sum[tc_it->second.body].first  += pcc.habitability * w_scale;
            hab_sum[tc_it->second.body].second += w_scale;
        }
        for (const auto& [body, acc] : hab_sum)
            report.body_habitability[body] = (acc.second > 0.0f) ? acc.first / acc.second : 1.0f;

        // Apply habitability efficiency multiplier to effective_workforce via the
        // economy-report contention entries.  Habitability > 0.6 → 1.0×; below 0.6 →
        // linear from 1.0 down to 0.5× at habitability 0.
        for (auto& [key, contention] : report.workforce_contention)
        {
            const entity_id body = key.second;
            const auto hit = report.body_habitability.find(body);
            const float hab = (hit != report.body_habitability.end()) ? hit->second : 1.0f;
            const float hab_scalar = (hab >= 0.6f) ? 1.0f : (0.5f + (hab / 0.6f) * 0.5f);
            contention *= hab_scalar;
        }
    }

    // BL-048B: Population growth step — accumulate growth per centre each tick;
    // level up when the accumulator crosses the tier threshold.
    // Growth ticks only when body habitability >= 0.5 AND food demand is >= 50% met.
    // Tier thresholds (ticks to grow): scale 1→2: 200, 2→3: 500, 3→4: 1500, 4→5: 5000.
    static constexpr int growth_threshold[6] = { 0, 200, 500, 1500, 5000, 0 };
    for (auto& [cid, pcc] : w.population_centres)
    {
        if (pcc.scale >= 5)
            continue; // already at max

        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto tc_it = w.tiles.find(tile_it->second);
        if (tc_it == w.tiles.end())
            continue;
        const entity_id body = tc_it->second.body;

        const auto hit = report.body_habitability.find(body);
        const float hab = (hit != report.body_habitability.end()) ? hit->second : 1.0f;
        if (hab < 0.5f)
            continue;

        // Check food supply ratio (agricultural_produce demand vs. supply in body's market).
        float food_ratio = 1.0f;
        for (const auto& [mid, mc] : w.markets)
        {
            if (mc.body != body)
                continue;
            const std::size_t ri = static_cast<std::size_t>(resource_type::agricultural_produce);
            const float demand = mc.demand[ri];
            const float supply = mc.supply[ri];
            if (demand > 0.0f)
                food_ratio = std::min(1.0f, supply / demand);
            break;
        }
        if (food_ratio < 0.5f)
            continue;

        ++pcc.growth_accumulator;
        const int threshold = growth_threshold[std::clamp(pcc.scale, 1, 5)];
        if (threshold > 0 && pcc.growth_accumulator >= threshold)
        {
            ++pcc.scale;
            pcc.growth_accumulator = 0;
            // Population headcount: scale × base (10k per scale level as a rough proxy).
            pcc.population = pcc.scale * 10;
        }
    }

    return report;
}

void inject_substrate_demand(world& w)
{
    for (auto& [key, sub] : w.nation_substrates)
    {
        const entity_id body_id = key.second;
        for (auto& [mid, mc] : w.markets)
        {
            if (mc.body != body_id)
                continue;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                mc.supply[r] += sub.background_supply[r];
                mc.demand[r] += sub.background_demand[r];
            }
            break; // one market per body for now
        }
    }
}
