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
                demand_by_body[body] += b.workforce_assigned;
        }

        std::map<entity_id, float> contention_by_body;
        for (const auto& [body, demand] : demand_by_body)
        {
            const float supply = w.workforce_supply(corp, body);
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

    return report;
}
