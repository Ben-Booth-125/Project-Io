#include "economy_system.hpp"

#include "budget_system.hpp"   // compute_building_opex, body_mean_habitability (BL-181 solver)
#include "building_profit.hpp" // estimate_building_profit (BL-079 corp agency)
#include "corp_ai.hpp"         // run_corp_strategic_step (BL-202 strategic tier)
#include "market_clearing.hpp" // market_for_tile (BL-095 construction gate)
#include "workforce.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

namespace {

// --- Deposit depletion constants (backlog.json § Environment, settled 2026-06-15) ---
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

// --- Player workforce auto-solver (BL-181) --------------------------------------
// Chooses the workforce target that maximises a player building's estimated net profit
// this tick. The underlying model is otherwise LINEAR in the workforce target (output,
// wages, and the labour part of maintenance all scale with it), so at fixed prices the
// optimum would be degenerate bang-bang (0 or max). The interior optimum comes from the
// local market PRICE RESPONSE: more output → more supply → a lower clearing price
// (market_clearing's base·sqrt(demand/supply)). The search reprices each candidate
// against that response and picks the best net.
//
// First-pass heuristic: contention is held at its current value; input-price response is
// ignored (inputs valued at the current price); the search is over coarse tiers, so the
// result can hunt by ±one tier. A finer model + hysteresis is future work (BL-181).
// Deterministic — reads last tick's market state only. Player corp only, opt-out via the
// building's `workforce_auto` flag (io-standing-rules.md § the player-corp exception).
constexpr float wf_price_floor_mult = 0.25f; // mirror market_clearing.cpp price band
constexpr float wf_price_ceil_mult  = 4.0f;

// The market's target (pre-smoothing) clearing price for one resource at a hypothetical
// supply — the same formula resolve_price aims at, used here as a forward estimate.
float wf_target_price(float base, float supply, float demand)
{
    if (base <= 0.0f)
        return 0.0f;
    float target;
    if (supply <= 0.0f && demand <= 0.0f) target = base;
    else if (supply <= 0.0f)              target = base * wf_price_ceil_mult;
    else                                  target = base * std::sqrt(demand / supply);
    return std::clamp(target, base * wf_price_floor_mult, base * wf_price_ceil_mult);
}

} // namespace

// Exported (BL-202): declared in economy_system.hpp so the strategic scorer
// (corp_ai.cpp) reuses the one solver. The anonymous namespace re-opens below.
int solve_workforce_target(const world& w, const recipe_registry& reg,
                           const building_component& b, float contention)
{
    const entity_id body = building_body(w, b);
    if (body == null_entity)
        return b.workforce_target;

    const market_component* mkt = nullptr;
    if (const entity_id mid = market_for_tile(w, b.tile); mid != null_entity)
        if (const auto it = w.markets.find(mid); it != w.markets.end())
            mkt = &it->second;

    const building_economics& e   = reg.economics(b.type);
    const float               hab = body_mean_habitability(w, body);
    const float               eff = b.workforce_assigned * contention;
    const float               now = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);

    // Clearing price for resource r if this building's supply of it shifts by delta.
    const auto price_of = [&](std::size_t r, float supply_delta) -> float {
        if (mkt == nullptr)
            return 0.0f;
        const float supply = std::max(0.0f, mkt->supply[r] + supply_delta);
        return wf_target_price(mkt->base_price[r], supply, mkt->demand[r]);
    };

    const auto tit = w.tiles.find(b.tile);

    int   best_wt  = 0;
    float best_net = -std::numeric_limits<float>::infinity();
    for (int wt = 0; wt <= 200; wt += 10)
    {
        const float wts = wt / 100.0f;

        building_component probe = b;
        probe.workforce_target   = wt;
        const building_opex opex = compute_building_opex(probe, e, contention, hab);

        float revenue = 0.0f, input_cost = 0.0f;
        if (b.type == building_type::extraction_site && tit != w.tiles.end())
        {
            const std::size_t ri = static_cast<std::size_t>(b.target_resource);
            const float rich = tit->second.resource_deposit[ri];
            const float k    = e.base_rate * rich * eff * (1.0f - tit->second.hazard_level);
            const float q    = k * wts;
            revenue = q * price_of(ri, q - k * now); // supply delta vs this building's current output
        }
        else if (b.type == building_type::processing_facility)
        {
            if (const recipe* rcp = reg.get_recipe(b.recipe))
            {
                const float runs     = e.base_rate * eff * wts;
                const float runs_now = e.base_rate * eff * now;
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    if (rcp->outputs[r] > 0.0f)
                        revenue += rcp->outputs[r] * runs *
                                   price_of(r, rcp->outputs[r] * (runs - runs_now));
                    if (rcp->inputs[r] > 0.0f)
                        input_cost += rcp->inputs[r] * runs * price_of(r, 0.0f);
                }
            }
        }

        const float net = revenue - input_cost - opex.maintenance - opex.wages;
        if (net > best_net) { best_net = net; best_wt = wt; }
    }
    return best_wt;
}

namespace {

// BL-095: pace each under-construction building against the local market's recent
// supply of its materials, drawing + paying for them as it builds (pay-as-you-build).
// Runs at the very top of the economy step, before production, so a building that
// completes this tick is immediately eligible below. Visits buildings in ascending
// id order (deterministic) because it mutates shared market demand (report.purchases)
// and corp balances.
void run_construction(world& w, const recipe_registry& reg, economy_report& report)
{
    const float max_stretch = reg.construction().max_stretch;
    const float pause_below  = (max_stretch > 1.0f) ? (1.0f / max_stretch) : 0.0f;

    std::vector<entity_id> ids;
    for (const auto& [bid, b] : w.buildings)
        if (b.ticks_remaining > 0)
            ids.push_back(bid);
    std::sort(ids.begin(), ids.end());

    for (const entity_id bid : ids)
    {
        building_component& b          = w.buildings.at(bid);
        const building_economics& econ = reg.economics(b.type);
        const float duration           = econ.build_duration_ticks;
        if (duration <= 0.0f) { b.ticks_remaining = 0; continue; } // instant safety

        // Local market — read its RECENT supply (last tick's cleared throughput) as
        // the derived "stock" the build competes for. No stored inventory, no new
        // serialised field (BL-095).
        const entity_id mid       = market_for_tile(w, b.tile);
        const market_component* m = (mid != null_entity) ? &w.markets.at(mid) : nullptr;

        // Rate = the fraction of this tick's material need the market can supply,
        // set by the scarcest required material; below 1/max_stretch it pauses.
        float rate = 1.0f;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float need = econ.resource_build_cost[r] / duration;
            if (need <= 0.0f)
                continue;
            const float avail = m ? m->supply[r] : 0.0f;
            rate = std::min(rate, avail / need);
        }
        rate = std::clamp(rate, 0.0f, 1.0f);
        if (rate < pause_below)
            rate = 0.0f; // paused: market can't supply even the max-stretched rate
        if (rate <= 0.0f)
            continue;

        // Draw this tick's materials as real market demand (bids price up, competes
        // with population and other builds) via the shared auto-buy path, and charge
        // the flat build_cost portion incrementally to the owning corp.
        const entity_id corp = owner_corp_of(w, bid);
        const entity_id body = building_body(w, b);
        if (corp != null_entity && body != null_entity)
        {
            auto& bought = report.purchases[std::make_pair(corp, body)];
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                const float need = econ.resource_build_cost[r] / duration;
                if (need > 0.0f)
                    bought[r] += need * rate;
            }
            const auto cit = w.corporations.find(corp);
            if (cit != w.corporations.end())
                cit->second.balance -= (econ.build_cost / duration) * rate;
        }

        // Advance sub-tick progress; a full-rate tick consumes exactly one whole
        // ticks_remaining unit.
        b.construction_progress += rate;
        while (b.construction_progress >= 1.0f && b.ticks_remaining > 0)
        {
            b.construction_progress -= 1.0f;
            --b.ticks_remaining;
        }
        if (b.ticks_remaining <= 0)
            b.construction_progress = 0.0f;
    }
}

} // namespace

economy_report run_economy_step(world& w, const recipe_registry& reg)
{
    economy_report report;

    // Build-time pacing (playtest 2026-07-06 → BL-095): advance every building's
    // construction before anything else runs, so a building finishing this tick is
    // already eligible for production/workforce demand below. BL-095 replaces the
    // flat one-tick-per-tick decrement with a market-gated rate: each build draws +
    // pays for its materials as real market demand and progresses only as fast as
    // the local market can supply them (full / stretched / paused).
    run_construction(w, reg, report);

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
            if (b.ticks_remaining > 0)
                continue; // still under construction — no labour demand yet
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
            building_component& b = bit->second; // mutable: the auto-solver may set the target
            const entity_id body = building_body(w, b);

            // Decommissioned buildings produce nothing — skip production entirely.
            if (b.decommissioned)
                continue;
            // Still under construction — no production yet (playtest patch, 2026-07-06).
            if (b.ticks_remaining > 0)
                continue;

            // BL-181: auto-solve the player's workforce target to maximise this
            // building's profit before it produces. Player corp only, opt-out via
            // workforce_auto (io-standing-rules.md § player-corp exception). Placed
            // here — after contention is computed (which reads workforce_assigned, not
            // the target) — so overwriting the target now is safe for this tick.
            if (corp == w.player_entity && b.workforce_auto &&
                (b.type == building_type::extraction_site ||
                 b.type == building_type::processing_facility))
                b.workforce_target = solve_workforce_target(w, reg, b, contention_for(body));

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

    // Population food demand (BL-190) is injected by inject_population_demand,
    // called from clear_markets AFTER its per-tick demand reset — injected here
    // it was erased by that reset the same tick and never reached price
    // resolution (2026-07-31 ordering fix; see market_clearing.cpp).

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
        // economy-report contention entries.  The curve lives in workforce.hpp
        // (BL-069) so the Population lens / Selection panel show exactly what the
        // simulation applies — full labour at/above 0.6, ramping to 0.5× at 0.
        for (auto& [key, contention] : report.workforce_contention)
        {
            const entity_id body = key.second;
            const auto hit = report.body_habitability.find(body);
            const float hab = (hit != report.body_habitability.end()) ? hit->second : 1.0f;
            contention *= workforce_efficiency(hab);
        }
    }

    // BL-048B / BL-078: Population growth step — accumulate growth per centre each
    // tick; level up when the accumulator crosses the tier threshold. Growth ticks
    // only when body habitability >= 0.5 AND the population's whole consumption
    // basket is met at >= growth_met_threshold (BL-078 keys growth off met-supply,
    // not food alone — minimal bounded growth, no habitability feedback loop). The
    // met-supply is read from last tick's cleared market (this step precedes
    // clear_markets in the tick), so it is deterministic and price-consistent.
    // Tier thresholds (ticks to grow): scale 1→2: 200, 2→3: 500, 3→4: 1500, 4→5: 5000.
    static constexpr int growth_threshold[6] = { 0, 200, 500, 1500, 5000, 0 };
    const substrate_params& growth_sp = reg.substrate();
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

        // Met-supply ratio across the whole demand basket (BL-078): a basket-weighted
        // mean of supply/demand over the body's market, so a centre grows only when
        // its population's consumption is broadly met and plateaus when it is not.
        float met_ratio = 1.0f;
        for (const auto& [mid, mc] : w.markets)
        {
            if (mc.body != body)
                continue;
            float met_acc = 0.0f, met_weight = 0.0f;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                const float b = growth_sp.demand_basket[r];
                if (b <= 0.0f || mc.demand[r] <= 0.0f)
                    continue;
                met_acc    += b * std::min(1.0f, mc.supply[r] / mc.demand[r]);
                met_weight += b;
            }
            if (met_weight > 0.0f)
                met_ratio = met_acc / met_weight;
            break;
        }
        if (met_ratio < growth_sp.growth_met_threshold)
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

    // BL-079: scoped background-corp agency (SCOPED RULE EXCEPTION — narrow, local,
    // deterministic; recorded in .claude/rules/io-standing-rules.md on landing). After
    // production, each NON-player corp may make one small mechanical choice per
    // building: switch a processor stuck on a floored (near-worthless) output to a
    // recipe with a healthier one, else idle a building that has run at a loss for a
    // sustained streak. No relocation, planning, or global optimisation. Visits corps
    // (corp_ids, sorted above) and their assets in stored order for determinism;
    // composes with the deposit-depletion throttle already live in run_extraction.
    {
        constexpr int   loss_streak_to_idle = 8;     // consecutive loss ticks before idling.
        constexpr float floored_frac        = 0.30f; // output within ~30% of the price floor reads as "floored".
        for (const entity_id corp : corp_ids)
        {
            const corporation_component& acc = w.corporations.at(corp);
            if (acc.is_player)
                continue; // never auto-act on the player's own corp

            for (const entity_id bid : acc.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end())
                    continue;
                building_component& b = bit->second;
                if (b.ticks_remaining > 0 || b.decommissioned)
                    continue; // not operating

                // (1) Recipe rescue: a processor whose current output is floored
                // switches to the recipe with the healthiest output, if better.
                if (b.type == building_type::processing_facility)
                {
                    const entity_id mid       = market_for_tile(w, b.tile);
                    const market_component* m = (mid != null_entity) ? &w.markets.at(mid) : nullptr;
                    auto output_ratio = [&](uint16_t recipe_id) -> float {
                        const recipe* rc = reg.get_recipe(recipe_id);
                        if (!rc || !m)
                            return 1.0f;
                        float worst = 1.0f; // lowest price/base over the recipe's outputs
                        for (std::size_t r = 0; r < resource_count; ++r)
                        {
                            if (rc->outputs[r] <= 0.0f)
                                continue;
                            const float base = m->base_price[r];
                            if (base <= 0.0f)
                                continue;
                            worst = std::min(worst, m->price[r] / base);
                        }
                        return worst;
                    };
                    if (output_ratio(b.recipe) <= floored_frac)
                    {
                        const int n      = reg.recipe_count(building_type::processing_facility);
                        int   best_i     = -1;
                        float best_ratio = output_ratio(b.recipe);
                        for (int i = 0; i < n; ++i)
                        {
                            const recipe& cand = reg.recipe_at(building_type::processing_facility, i);
                            const float ratio  = output_ratio(reg.recipe_id(cand.name));
                            if (ratio > best_ratio)
                            {
                                best_ratio = ratio;
                                best_i     = i;
                            }
                        }
                        if (best_i >= 0)
                        {
                            const recipe& chosen  = reg.recipe_at(building_type::processing_facility, best_i);
                            b.recipe              = reg.recipe_id(chosen.name);
                            b.active_recipe_index = best_i;
                            b.loss_streak         = 0; // give the new recipe a chance before idling
                            report.agency_events.push_back(
                                {corp, bid, agency_event::kind::recipe_switch, b.recipe});
                            continue;
                        }
                    }
                }

                // (2) Idle a persistent loser (composes with the depletion throttle:
                // an exhausted deposit drives extraction losses that end in an idle).
                const building_profit bp = estimate_building_profit(w, reg, report, bid);
                if (bp.has_data && bp.net() < 0.0f)
                {
                    if (++b.loss_streak >= loss_streak_to_idle)
                    {
                        b.decommissioned = true;
                        report.agency_events.push_back(
                            {corp, bid, agency_event::kind::idled, 0});
                    }
                }
                else
                {
                    b.loss_streak = 0;
                }
            }
        }
    }

    // BL-202: strategic tier — the scored utility layer. Runs AFTER the BL-079
    // reflex tier (tier 0, unchanged above): due non-player corps evaluate a
    // bounded candidate set, emit corp_commands, and apply them through the
    // player-grade seams (apply_corp_command). The tick source is
    // w.current_day_tick, which drives the staggered cadence; callers (app's
    // sim loop, the harnesses) maintain it per tick. The player corp is never
    // evaluated or commanded (io-standing-rules.md).
    run_corp_strategic_step(w, reg, report, w.current_day_tick);

    return report;
}

void inject_substrate_demand(world& w, const recipe_registry& reg)
{
    const substrate_params& sp = reg.substrate();

    for (auto& [key, sub] : w.nation_substrates)
    {
        const entity_id nation_id = key.first;
        const entity_id body_id   = key.second;

        // BL-096: distribute the nation's substrate across the markets on this body
        // that sit in its OWN territory (a market whose centre_tile the nation owns);
        // if it owns none (it folded into a neighbour under the resource carve), fall
        // back to the body's lowest-id market. Split equally so the per-nation totals
        // are conserved however many markets the carve produced. Markets are gathered
        // in ascending id order for determinism.
        std::vector<entity_id> targets;
        entity_id fallback = null_entity;
        {
            std::vector<entity_id> body_markets;
            for (const auto& [mid, mc] : w.markets)
                if (mc.body == body_id)
                    body_markets.push_back(mid);
            std::sort(body_markets.begin(), body_markets.end());
            for (const entity_id mid : body_markets)
            {
                if (fallback == null_entity)
                    fallback = mid; // lowest-id market on the body
                const entity_id ct = w.markets.at(mid).centre_tile;
                if (ct == null_entity)
                    continue;
                const auto nit = w.tile_to_nation.find(ct);
                if (nit != w.tile_to_nation.end() && nit->second == nation_id)
                    targets.push_back(mid);
            }
        }
        if (targets.empty() && fallback != null_entity)
            targets.push_back(fallback);
        if (targets.empty())
            continue; // no markets on this body

        const float share = 1.0f / static_cast<float>(targets.size());
        const float pop_w = sub.population_weight * share;

        for (const entity_id mid : targets)
        {
            market_component& mc = w.markets.at(mid);
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                // Only tradeable resources carry a substrate; untradeables have no
                // base price to anchor the elasticity curve.
                const float base = mc.base_price[r];
                if (base <= 0.0f)
                    continue;
                const float weighted = pop_w * sp.demand_basket[r] * sp.demand_scale;
                if (weighted <= 0.0f)
                    continue;

                // DEMAND — price-elastic: cheaper than base → consume more, dearer
                // → less. Reads last tick's cleared price (0 before the first clear
                // falls back to base). Deterministic; a curve, not RNG.
                const float price   = (mc.price[r] > 0.0f) ? mc.price[r] : base;
                const float elastic = std::clamp(std::pow(base / price, sp.demand_elasticity),
                                                 sp.elasticity_min, sp.elasticity_max);
                const float demand  = weighted * elastic;

                // SUPPLY — abstract nation capacity that tracks demand and clears it
                // only to `clearing_fraction`, capped by deposit-derived capacity.
                // Ample capacity → a thin saturation margin (price just above base);
                // a resource the nation lacks → supply pegs short, price rises (the
                // fillable opportunity gap BL-112/BL-079 read).
                const float capacity = sub.capacity[r] * share * sp.capacity_scale;
                const float supply   = std::min(capacity, demand * sp.clearing_fraction);

                mc.demand[r] += demand;
                mc.supply[r] += supply;
            }
        }
    }
}
