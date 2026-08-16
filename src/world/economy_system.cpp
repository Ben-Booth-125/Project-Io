#include "economy_system.hpp"

#include "budget_system.hpp"   // compute_building_opex, body_mean_habitability (BL-181 solver)
#include "building_profit.hpp" // estimate_building_profit (BL-079 corp agency)
#include "corp_ai.hpp"         // run_corp_strategic_step (BL-202 strategic tier)
#include "logistics.hpp"       // invalidate_logistics_caches (anchor-set changes)
#include "market_clearing.hpp" // market_for_tile (BL-095 construction gate)
#include "placement_rules.hpp" // stack_output_scalar (BL-193 building stacks)
#include "workforce.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

// --- Deposit depletion constants (backlog.json § Environment, settled 2026-06-15) ---
/// BL-428: record that @p corp has now produced @p r, raising its reached chain
/// depth if this good sits deeper than anything it had made before. Called from
/// the two places a good is actually created (run_extraction, run_processing)
/// rather than from a per-tick sweep, so the bit is set by the *event* of making
/// something — a corp cannot inherit depth from stock it merely bought.
///
/// Set-only. See corporation_component::produced_ever for why it never clears.
void mark_produced(world& w, entity_id corp, resource_type r)
{
    const auto it = w.corporations.find(corp);
    if (it == w.corporations.end())
        return;
    it->second.produced_ever[static_cast<std::size_t>(r)] = true;
}

/// Body that a building sits on (via its tile). null_entity if the tile is gone.
entity_id building_body(const world& w, const building_component& b)
{
    const auto it = w.tiles.find(b.tile);
    return (it != w.tiles.end()) ? it->second.body : null_entity;
}

/// World history log (BL-208): push an additional, non-evicting `agency`-topic
/// entry for a BL-079 reflex-tier action. Self-contained — history_topic and
/// world::history_log are already visible via world.hpp (this file already
/// includes it through economy_system.hpp), so this adds no new translation-
/// unit dependency for the existing tools/verify/README.md hand-written recipes
/// that link economy_system.cpp.
void log_reflex_agency(world& w, entity_id corp, entity_id body, const char* what)
{
    world_history_entry e;
    e.timestamp = w.current_day_tick;
    e.topic     = history_topic::agency;
    e.body      = body;
    e.corp      = corp;
    e.event     = std::string("Corp ") + std::to_string(corp) + " " + what;
    w.history_log.push_back(std::move(e));
}

/// BL-193 building stacks: the per-tick state one extraction site shares with the
/// rest of its tile's stack, handed down by the pre-pass in run_economy_step.
///
/// `rank` is the site's 1-based place in stored order, which sets how far down the
/// diminishing-returns curve it sits. `taper` is computed ONCE per stack, against
/// the stack's COMBINED nominal draw rather than each site's own — that is what
/// makes a stack exhaust *together* instead of dribbling out of a spent deposit
/// one site at a time, and what makes a five-site stack burn its taper band five
/// sites' worth faster.
///
/// Only a stack of MORE THAN ONE site ever gets one of these (BL-347). A lone site's
/// combined nominal is its own nominal, so its shared taper is arithmetically the
/// self taper `run_extraction` computes anyway — pre-computing it bought nothing and
/// cost an entry per site in the world.
struct stack_draw
{
    int   rank             = 1;    ///< 1-based position in the tile's stack.
    float combined_nominal = 0.0f; ///< What the whole stack draws this tick, decay included.
    float taper            = 1.0f; ///< Shared depletion scalar (1 = reserve is ample).
};

} // namespace

// Exported (BL-346): declared in economy_system.hpp so the prospective estimator
// (building_profit.cpp) sizes a stack's combined draw from the same figure the tick
// draws, rather than re-deriving it. The anonymous namespace re-opens below.
float extraction_nominal(const world& w, const recipe_registry& reg,
                         const building_component& b, float contention)
{
    const auto tile_it = w.tiles.find(b.tile);
    if (tile_it == w.tiles.end())
        return 0.0f;
    const tile_component& tc = tile_it->second;
    const std::size_t     ri = static_cast<std::size_t>(b.target_resource);
    // Apply the player's workforce target (0–200 % of nominal capacity).
    const float wt_scalar = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
    const building_economics& e = reg.economics(building_type::extraction_site);
    return e.base_rate
         * richness_rate_scalar(e, tc.resource_deposit[ri])
         * (b.workforce_assigned * contention)
         * wt_scalar
         * (1.0f - tc.hazard_level);
}

namespace {

/// Extraction: credit the (corp, body) pool from EVERY deposit on the site's tile
/// and draw each from its own finite reserve. Nominal output is
/// base_rate × richness × workforce × (1 − hazard), diminished by this site's rank
/// in its tile's stack (BL-193); it tapers as the *stack's* shared reserve nears
/// empty and the building reports `exhausted` once it can draw nothing. Deposits
/// never refill.
///
/// BL-437 (2026-08-16): a site works the whole tile, not just `target_resource`.
/// That field survives as the PRIMARY — it still sets the rate, the stack key and
/// the taper, and it is what the UI and the AI scorer name the site by — but the
/// site's capacity is now SHARED across the tile's deposits in proportion to
/// richness rather than spent entirely on one. Total output per site is
/// unchanged; what changes is that it arrives as a basket. See the co-extraction
/// block below for why sharing rather than multiplying is the load-bearing call.
building_report run_extraction(world& w, const recipe_registry& reg,
                               entity_id corp, entity_id building_id,
                               const building_component& b, float contention,
                               const stack_draw* stack)
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

    // Rate the deposit would yield at full reserve (richness sets the rate),
    // diminished by this site's rank in the tile's stack (BL-193): the second site
    // yields 0.8 of the first, the third 0.64, and so on. Stacking therefore pays
    // sub-linearly — and there is no clamp under the curve, because the curve is
    // the economics.
    const float nominal = extraction_nominal(w, reg, b, contention)
                        * placement_rules::stack_output_scalar(stack != nullptr ? stack->rank : 1);
    if (nominal <= 0.0f)
    {
        rep.idle = true; // unstaffed, no deposit of the target, or fully hazardous
        return rep;
    }

    float& remaining = tc.resource_remaining[ri];

    // Taper output as the reserve approaches empty: full until the last
    // (taper_ticks × nominal) of reserve, then linearly down toward the floor.
    // The nominal in question is the whole STACK's (BL-193) — every site on this
    // deposit rides the same curve and reports exhausted on the same tick, and a
    // full stack reaches that tick sooner than a lone site would.
    //
    // No pre-pass entry means this site is the only one working this (tile, target),
    // so the stack's combined nominal IS its own and the shared taper collapses to
    // the self taper computed here (BL-347) — the same expression, the same floats,
    // and nobody else can have moved this reserve earlier in the tick.
    const float taper = (stack != nullptr)
        ? stack->taper
        : std::clamp(remaining / (deposit_taper_ticks * nominal), 0.0f, 1.0f);
    if (taper < deposit_min_taper)
    {
        rep.exhausted = true; // out of resources — the reserve is spent
        return rep;
    }

    // The site's total capacity this tick, before it is shared out below. NOT
    // capped by the primary's reserve any more: each deposit is capped by its
    // own below, and capping the whole basket by one member's reserve would let
    // a nearly-spent primary throttle deposits that are still full.
    const float capacity = nominal * taper;

    // --- BL-437: co-extraction — a site works EVERY deposit on its tile ------
    //
    // Before this, a site mined only `target_resource`, chosen by
    // placement_rules::richest_extractable as the single richest deposit on the
    // tile. Measured 2026-08-16 (tier_margin R4/R5), that left EIGHT raws with
    // deposits and zero extraction sites — coal among them, which is the reagent
    // of `steel`, which is the era-aware DEFAULT recipe. The default recipe could
    // not run in a shipped campaign because nothing anywhere mined its reagent.
    //
    // The measurement also chose this fix over the alternatives: the affected
    // resources are richest on 0-12% of the tiles carrying them (clay 0.0%, sand
    // 0.1%, coal 1.1%, peat 1.3%), i.e. they COEXIST with a richer deposit on the
    // same tile. Reaching the whole tile therefore reaches all of them at once,
    // where re-biasing the winner would only ever swap which single resource is
    // starved.
    //
    // CAPACITY IS SHARED, NOT MULTIPLIED, and that is the load-bearing decision.
    // Yielding every deposit at the full rate would multiply extraction output
    // several-fold — widening the very extraction-vs-processing gap BL-436 exists
    // to close, and inflating an economy that already measures 1947 cr/tick per
    // mine. Instead the site's total stays exactly what it was and is apportioned
    // by richness, so a mine now yields a BASKET rather than one good. Richness
    // still decides what it mostly produces.
    //
    // Determinism: k_extractable is a fixed-order constexpr array and the shares
    // are pure functions of the tile's own richness — no container iteration
    // order, no RNG (the BL-406 lesson).
    float richness_total = 0.0f;
    for (const resource_type r : placement_rules::k_extractable)
        richness_total += tc.resource_deposit[static_cast<std::size_t>(r)];

    float produced_total = 0.0f;
    if (capacity > 0.0f && richness_total > 0.0f)
    {
        stockpile_component& pool = w.pool_for(corp, body);
        for (const resource_type r : placement_rules::k_extractable)
        {
            const std::size_t rr    = static_cast<std::size_t>(r);
            const float       rich  = tc.resource_deposit[rr];
            if (rich <= 0.0f)
                continue;
            // Each deposit draws only its own share, and only what its own
            // reserve still holds — a spent secondary simply stops contributing
            // rather than blocking the site.
            const float share = capacity * (rich / richness_total);
            const float got   = std::min(share, tc.resource_remaining[rr]);
            if (got <= 0.0f)
                continue;
            tc.resource_remaining[rr] -= got;
            pool.quantities[rr]       += got;
            mark_produced(w, corp, r); // BL-428 growth spine, now for every good worked
            produced_total += got;
        }
    }

    if (produced_total > 0.0f)
    {
        rep.active          = true;
        rep.output_quantity = produced_total;
    }
    else
    {
        // Unchanged semantics: a site reports exhausted when it can draw nothing.
        // The taper check above still keys on the PRIMARY reserve, so a site is
        // retired on the same tick it always was.
        rep.exhausted = true;
    }
    return rep;
}

/// Processing: run the recipe pool-first, then against the local market's real
/// inventory (BL-130) under the two-threshold model, recording the drawn
/// quantity into `purchases`.
building_report run_processing(world& w, const recipe_registry& reg,
                               entity_id corp, entity_id building_id,
                               const building_component& b,
                               entity_id market_id,
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
    // BL-130: a processor's real available stock is its own pool PLUS the local
    // market's real inventory — a genuine finite draw, not an unconditional
    // auto-buy. Mutable: this function actually consumes what it draws, below.
    market_component* mc = (market_id != null_entity) ? &w.markets.at(market_id) : nullptr;

    // Coverage of a full run = the scarcest input's (pool + market inventory) fraction.
    float coverage = std::numeric_limits<float>::infinity();
    bool  has_input = false;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float in = rcp->inputs[r];
        if (in <= 0.0f)
            continue;
        has_input = true;
        const float need   = in * batches_full;
        const float avail  = pool.quantities[r] + (mc ? std::max(0.0f, mc->inventory[r]) : 0.0f);
        const float cov    = (need > 0.0f) ? avail / need : std::numeric_limits<float>::infinity();
        if (cov < coverage)
        {
            coverage              = cov;
            rep.limiting_input    = static_cast<resource_type>(r);
            rep.has_limiting      = true;
        }
    }
    if (!has_input)
        coverage = 1.0f; // degenerate no-input recipe runs full

    // Run fraction — the two-threshold model, uniformly, whether or not a
    // market backs this body. BL-130 retires the old "a market body always
    // runs full batch" special case: a market's inventory is now real and
    // finite, so it earns its place in `coverage` above rather than bypassing
    // the threshold model outright (docs/economy/PRODUCTION.md, corrected).
    float run = 0.0f;
    if (coverage >= reg.t_full())
    {
        run = 1.0f;
    }
    else if (coverage >= reg.t_idle())
    {
        run = coverage; // scale output to the limiting input
    }
    else
    {
        rep.idle = true; // too little between pool and market to bootstrap
        return rep;
    }

    const float batches = batches_full * run;

    // Consume inputs pool-first; the remainder draws on the market's real
    // inventory (BL-130) — guaranteed sufficient at this `run` level by the
    // coverage-min construction above (every input's avail/need_full >= run).
    // Recorded into `bought` for clear_markets' pricing pass, same as before —
    // now billing a real draw rather than an unconditional one.
    auto& bought = out.purchases[std::make_pair(corp, body)];
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float in = rcp->inputs[r];
        if (in <= 0.0f)
            continue;
        const float need      = in * batches;
        const float from_pool = std::min(pool.quantities[r], need);
        pool.quantities[r] -= from_pool;
        const float remainder = need - from_pool;
        if (remainder <= 0.0f)
            continue;
        const float from_market = mc ? std::min(std::max(0.0f, mc->inventory[r]), remainder) : 0.0f;
        if (mc)
            mc->inventory[r] = std::max(0.0f, mc->inventory[r] - from_market);
        bought[r] += from_market;
    }

    float produced = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float outq = rcp->outputs[r] * batches;
        if (outq <= 0.0f)
            continue;
        pool.quantities[r] += outq;
        produced           += outq;
        mark_produced(w, corp, static_cast<resource_type>(r)); // BL-428 growth spine
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
                           const building_component& b, float contention,
                           int stack_rank, float* out_gain)
{
    if (out_gain)
        *out_gain = 0.0f; // every early return below is "no move, so no gain"

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

    // The modelled net at one candidate target. Hoisted out of the search loop so
    // the building's CURRENT target can be priced by the identical model — the
    // caller needs the difference, and re-deriving it outside got the sign wrong
    // (see the out_gain note in the header).
    const auto net_at = [&](int wt) -> float {
        const float wts = wt / 100.0f;

        building_component probe = b;
        probe.workforce_target   = wt;
        const building_opex opex = compute_building_opex(probe, e, contention, hab);

        float revenue = 0.0f, input_cost = 0.0f;
        if (b.type == building_type::extraction_site && tit != w.tiles.end())
        {
            const std::size_t ri = static_cast<std::size_t>(b.target_resource);
            // BL-436: the same richness->rate conversion the live tick uses.
            // The workforce solver optimises against this curve, so a raw
            // richness here would solve for a rate the building cannot reach.
            const float rich = richness_rate_scalar(
                reg.economics(building_type::extraction_site),
                tit->second.resource_deposit[ri]);
            // Stack decay (BL-193), applied here for the same reason run_extraction
            // applies it: a rank-3 site never yields the lone-site rate, so a dial
            // solved against the lone-site rate is solved against a curve this
            // building cannot reach (BL-346).
            const float decay = placement_rules::stack_output_scalar(stack_rank);
            const float k    = e.base_rate * rich * eff * (1.0f - tit->second.hazard_level) * decay;
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

        return revenue - input_cost - opex.maintenance - opex.wages;
    };

    int   best_wt  = 0;
    float best_net = -std::numeric_limits<float>::infinity();
    for (int wt = 0; wt <= 200; wt += 10)
    {
        const float net = net_at(wt);
        if (net > best_net) { best_net = net; best_wt = wt; }
    }

    if (out_gain)
    {
        // Price the incumbent through the same model. b.workforce_target is not
        // necessarily on the step-10 grid (the player's set_workforce verb accepts
        // any 0-200), so it is evaluated directly rather than looked up.
        const float gain = best_net - net_at(b.workforce_target);
        // Floored, not clamped-for-noise: an off-grid incumbent can legitimately
        // beat every grid point, and a negative gain and a zero gain say the same
        // thing to the only caller — the move is not worth making.
        *out_gain = (gain > 0.0f) ? gain : 0.0f;
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

        // BL-130: read the market's REAL persistent inventory — what is actually
        // on hand from prior ticks' sales — rather than last tick's cleared
        // throughput. Mutable: a build that draws on it actually consumes it
        // (below), same as any other consumer competing for the same stock.
        const entity_id mid = market_for_tile(w, b.tile);
        market_component* m = (mid != null_entity) ? &w.markets.at(mid) : nullptr;

        // Rate = the fraction of this tick's material need the market can supply,
        // set by the scarcest required material; below 1/max_stretch it pauses.
        float rate = 1.0f;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float need = econ.resource_build_cost[r] / duration;
            if (need <= 0.0f)
                continue;
            const float avail = m ? std::max(0.0f, m->inventory[r]) : 0.0f;
            rate = std::min(rate, avail / need);
        }
        rate = std::clamp(rate, 0.0f, 1.0f);
        if (rate < pause_below)
            rate = 0.0f; // paused: market can't supply even the max-stretched rate
        if (rate <= 0.0f)
            continue;

        // Draw this tick's materials from the market's real inventory (BL-130) and
        // charge the flat build_cost portion incrementally to the owning corp.
        // `bought` still records the drawn quantity as market demand for pricing
        // (clear_markets), same as before — the transaction is now real, not just
        // its billing.
        const entity_id corp = owner_corp_of(w, bid);
        const entity_id body = building_body(w, b);
        if (corp != null_entity && body != null_entity)
        {
            auto& bought = report.purchases[std::make_pair(corp, body)];
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                const float need = econ.resource_build_cost[r] / duration;
                if (need <= 0.0f)
                    continue;
                const float drawn = need * rate;
                bought[r] += drawn;
                if (m)
                    m->inventory[r] = std::max(0.0f, m->inventory[r] - drawn);
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
        {
            b.construction_progress = 0.0f;
            // COMPLETION is when a port/hub starts anchoring supply
            // (is_supply_anchor's ticks_remaining contract, 2026-08-08), so the
            // reach field goes stale at this moment too, not only at placement.
            // Port/hub ONLY (2026-08-12): a mine or farm completing changes no
            // cached answer, and completions are per-tick events through the
            // warm start — see invalidate_logistics_caches' narrowing note.
            if (building_affects_logistics(b.type))
                invalidate_logistics_caches(w);
            // BL-263: COMPLETION is also the spontaneous-market-emergence
            // trigger. No-op if the body already has a market (including an
            // instant build that already spawned one at placement, construction.cpp).
            if (body != null_entity)
                maybe_spawn_market(w, reg, body, b.tile);
        }
    }
}

} // namespace

// BL-430: see the declaration in economy_system.hpp for the full rationale —
// the single implementation shared by corp_command's set_recipe verb (also the
// AI's dial_recipe margin-chase path) and construction_panel's method dropdown.
recipe_switch_result try_switch_recipe(world& w, const recipe_registry& reg,
                                       entity_id corp, building_component& b,
                                       uint16_t new_recipe_id)
{
    if (reg.get_recipe(new_recipe_id) == nullptr)
        return recipe_switch_result::invalid;
    if (b.recipe == new_recipe_id)
        return recipe_switch_result::invalid; // no-op
    if (b.recipe_switch_cooldown > 0)
        return recipe_switch_result::on_cooldown;

    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return recipe_switch_result::invalid;

    const recipe_switch_params& sw = reg.recipe_switch();

    // BL-434 retraction (2026-08-16, minutes after BL-434 itself landed in this
    // same session): cross-GROUP switching used to cost more (a tiered
    // multiplier); Ben reconsidered and it is now REFUSED outright instead —
    // "switching methods can mean changing to a different building type — we
    // should retire that completely. So the only way to access a different
    // building type is fully dismantling the building, then using the tile
    // selector to reselect and build a new building." Looked up by group name
    // rather than id/enum, since `group` is the whole point of the field
    // (recipe_registry.hpp). A missing old recipe (e.g. a fresh
    // processing_facility mid-construction, whose b.recipe may be `no_recipe`)
    // or an unrecognised group falls back to PERMITTING the switch at the
    // intra-group cost rather than refusing on a data edge case — this seam
    // must never crash or silently misbehave on missing data.
    const recipe* old_r = reg.get_recipe(b.recipe);
    const recipe* new_r = reg.get_recipe(new_recipe_id);
    if (old_r != nullptr && new_r != nullptr && old_r->group != new_r->group)
        return recipe_switch_result::cross_group;

    // BL-428 chain-depth gate, mirroring construct_building's. Without it the gate
    // has a trivial bypass: place the shallowest ancient method the corp can
    // reach, then immediately retool onto the deepest one in the same group, and
    // the ladder never has to be climbed at all. A gate that only guards the front
    // door is not a gate. Ancient-band recipes only, same first-cut scope.
    if (new_r != nullptr && new_r->era == era_band::ancient)
    {
        const int need = reg.recipe_required_depth(new_recipe_id);
        if (need < 0 || need > corp_reached_depth(cit->second, reg))
            return recipe_switch_result::depth_locked;
    }

    const float cost = sw.switch_cost;
    if (cit->second.balance < cost)
        return recipe_switch_result::insufficient_funds;

    cit->second.balance -= cost;
    b.recipe                 = new_recipe_id;
    b.active_recipe_index    = static_cast<int>(new_recipe_id); // registry index == id (recipe_at)
    b.loss_streak            = 0;                                // give the new recipe a chance
    b.recipe_switch_cooldown = sw.cooldown_ticks;
    return recipe_switch_result::applied;
}

economy_report run_economy_step(world& w, const recipe_registry& reg, bool spectating)
{
    economy_report report;

    // Build-time pacing (playtest 2026-07-06 → BL-095): advance every building's
    // construction before anything else runs, so a building finishing this tick is
    // already eligible for production/workforce demand below. BL-095 replaces the
    // flat one-tick-per-tick decrement with a market-gated rate: each build draws +
    // pays for its materials as real market demand and progresses only as fast as
    // the local market can supply them (full / stretched / paused).
    run_construction(w, reg, report);

    // BL-430: tick down every building's player-recipe-switch cooldown. Order-
    // independent (each building's counter is decremented against itself only),
    // so iterating w.buildings directly is deterministic without a sort.
    for (auto& [bid, b] : w.buildings)
        if (b.recipe_switch_cooldown > 0)
            --b.recipe_switch_cooldown;

    // BL-332 capability points: a flat per-tick credit to the owning corp's
    // military_points / science for every COMPLETED military_base /
    // research_institute it holds — passive, no workforce dependency (both
    // types staff at zero), symmetric across every corp. Runs after
    // run_construction so a base finishing this tick already counts, mirroring
    // the rest of this function's build-then-produce ordering. Iterates each
    // corp's own `assets` vector (not w.buildings), so per-corp determinism
    // needs no extra sort — only that corp's own building list, in its own
    // stored order, decides its own accumulator.
    {
        const auto& mp = reg.military();
        for (auto& [corp, cc] : w.corporations)
        {
            for (const entity_id bid : cc.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end())
                    continue;
                const building_component& b = bit->second;
                if (b.ticks_remaining > 0 || b.decommissioned)
                    continue; // still under construction, or torn down
                if (b.type == building_type::military_base)
                    cc.military_points += mp.military_points_per_base_tick;
                else if (b.type == building_type::research_institute)
                    cc.science += mp.science_per_research_institute_tick;
            }
        }
    }

    // BL-350 procurement contracts: paced like a BL-095 build — the deposit
    // was already debited at accept_quote, so each tick draws an even slice of
    // the remainder from the BUYER's balance (a simplification of BL-095's own
    // market-gated stretch/pause rate: a contract paces on a fixed schedule
    // rather than the supplier's live throughput, a known first-cut cut — see
    // BL-350's item resolution). On completion the full quantity is credited
    // to the buyer's (buyer, body) pool and reputation moves up. Ascending
    // contract id (insertion order — the vector is appended-to, never
    // reordered) keeps this deterministic without an extra sort.
    {
        const auto& pp = reg.procurement();
        std::vector<std::size_t> completed;
        for (std::size_t i = 0; i < w.procurement_contracts.size(); ++i)
        {
            procurement_contract& c = w.procurement_contracts[i];
            const auto bit = w.corporations.find(c.buyer);
            if (bit == w.corporations.end())
                continue; // buyer no longer exists — leave the contract inert
            const float remaining_total = c.quantity * c.unit_price * (1.0f - pp.deposit_fraction);
            const float per_tick = (c.lead_time_ticks > 0) ? remaining_total / static_cast<float>(c.lead_time_ticks) : remaining_total;
            bit->second.balance -= per_tick;
            ++c.ticks_elapsed;
            if (c.ticks_elapsed >= c.lead_time_ticks)
            {
                w.pool_for(c.buyer, c.body).quantities[static_cast<std::size_t>(c.resource)] += c.quantity;
                w.corp_reputation[{c.buyer, c.supplier}] += pp.reputation_on_complete;
                completed.push_back(i);
            }
        }
        // Erase back-to-front so earlier indices stay valid.
        for (auto it = completed.rbegin(); it != completed.rend(); ++it)
            w.procurement_contracts.erase(w.procurement_contracts.begin() + static_cast<long>(*it));
    }

    // BL-042: Derive per-body workforce supply from population centres.
    // Scale → labour-force table (units available to industry on this body).
    static constexpr float labour_by_scale[6] = { 0.0f, 1.0f, 3.0f, 10.0f, 30.0f, 100.0f };
    std::map<entity_id, float> pop_supply_by_body;
    // BL-041: Habitability cap — weighted mean habitability of population centres per body.
    // weight = centre scale; cap = min(1, mean_hab / 0.6); default 1.0 (uncapped) when no centres.
    std::map<entity_id, float> hab_weighted_sum;
    std::map<entity_id, float> hab_weight_total;
    // Ascending centre id: these are float accumulations, so the summation order
    // must not be read off population_centres' unordered layout.
    std::vector<entity_id> centre_ids;
    centre_ids.reserve(w.population_centres.size());
    for (const auto& kv : w.population_centres)
        centre_ids.push_back(kv.first);
    std::sort(centre_ids.begin(), centre_ids.end());
    for (const entity_id cid : centre_ids)
    {
        const population_centre_component& pcc = w.population_centres.at(cid);
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
    // Building counts per (corp, body) for apportionment. Integer counts commute
    // exactly, so the unordered corporations walk is safe here.
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

    // ── Pass 1: labour contention for every (corp, body).
    //
    // Hoisted out of the production loop (BL-193). The stack pre-pass below has to
    // size a stack's combined draw across ALL its members, and a member may belong
    // to a corp the production loop has not reached yet — so every corp's scalar
    // has to exist before any building produces.
    //
    // `report.workforce_contention` IS the store — the private duplicate this pass
    // used to keep alongside it held the same keys and the same values (BL-347), and
    // the report's copy is not rescaled by habitability until after production.

    // Reused across corporations rather than allocated per corporation (BL-347): a
    // corp's labour lands on one or two bodies, and a linear scan over that handful
    // beats a tree whose nodes are allocated and freed once per corp per tick. The
    // walk becomes first-touch order over `cc.assets` (stored order) instead of
    // ascending body id — still deterministic, and the result is keyed either way.
    std::vector<std::pair<entity_id, float>> demand_by_body;
    for (const entity_id corp : corp_ids)
    {
        const corporation_component& cc = w.corporations.at(corp);

        // Workforce pool, step 1: sum this corp's labour demand on each body
        // (the requested workforce_assigned of every producing building there),
        // then derive a per-body contention scalar min(1, supply/demand). Below 1
        // it throttles every building on that (corp, body) uniformly.
        // BL-041: demand is capped by the body's habitability scalar before summing.
        demand_by_body.clear();
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
            if (body == null_entity)
                continue;
            const float add = b.workforce_assigned * hab_cap_for(body);
            const auto  it  = std::find_if(demand_by_body.begin(), demand_by_body.end(),
                                           [body](const std::pair<entity_id, float>& e) {
                                               return e.first == body;
                                           });
            if (it != demand_by_body.end()) it->second += add;
            else                            demand_by_body.emplace_back(body, add);
        }

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
            report.workforce_contention[std::make_pair(corp, body)] = scalar;
        }
    }

    auto contention_for = [&](entity_id corp, entity_id body) -> float {
        const auto it = report.workforce_contention.find({corp, body});
        return (it != report.workforce_contention.end()) ? it->second : 1.0f;
    };

    // ── Pass 2: BL-193 stack grouping — who shares a deposit, and at what rank.
    //
    // Split out ahead of the auto-solve (BL-346): a site's RANK is its place in its
    // tile's build order and depends on nothing the solver touches, while the shared
    // taper below depends on the very workforce targets the solver sets. Ranks first,
    // then the solve, then the taper — no circle.
    //
    // Only tiles carrying MORE THAN ONE site of the same target appear here (BL-347).
    // A lone site has nobody to share a taper with, its combined nominal is its own,
    // and run_extraction reaches the same answer unaided — so the common tile costs
    // this pass one slot in a flat vector and nothing else. The pass used to spend a
    // map node per site plus a heap vector per tile, every tick, in a world that might
    // contain no stack at all.
    //
    // Everything below walks in stored order (ascending entity id, which is creation
    // order): that order picks who is rank 1 and takes the undiminished yield, so it
    // can never be read off world::buildings' own iteration.

    /// One extraction site, keyed so that sorting groups a tile's stack together with
    /// its members already in stored order.
    struct site_slot
    {
        entity_id   tile   = null_entity;
        std::size_t target = 0;
        entity_id   id     = null_entity;

        bool same_stack_as(const site_slot& o) const
        {
            return tile == o.tile && target == o.target;
        }
        bool operator<(const site_slot& o) const
        {
            if (tile   != o.tile)   return tile   < o.tile;
            if (target != o.target) return target < o.target;
            return id < o.id; // stored order inside the stack
        }
    };

    std::vector<site_slot> sites;
    sites.reserve(w.buildings.size());
    for (const auto& [bid, bc] : w.buildings)
        if (bc.type == building_type::extraction_site)
            sites.push_back({bc.tile, static_cast<std::size_t>(bc.target_resource), bid});
    std::sort(sites.begin(), sites.end());

    // Half-open [first, last) ranges into `sites`, one per genuine stack. Empty in a
    // world where nothing is stacked, which is the world the tick is usually in.
    std::vector<std::pair<std::size_t, std::size_t>> stack_runs;
    std::map<entity_id, stack_draw>                  stack_by_building;
    for (std::size_t first = 0; first < sites.size(); )
    {
        std::size_t last = first + 1;
        while (last < sites.size() && sites[last].same_stack_as(sites[first]))
            ++last;
        if (last - first > 1)
        {
            stack_runs.emplace_back(first, last);
            for (std::size_t i = first; i < last; ++i)
            {
                stack_draw sd;
                sd.rank = static_cast<int>(i - first) + 1; // taper filled in Pass 4
                stack_by_building[sites[i].id] = sd;
            }
        }
        first = last;
    }

    auto rank_of = [&](entity_id bid) -> int {
        const auto it = stack_by_building.find(bid);
        return (it != stack_by_building.end()) ? it->second.rank : 1;
    };

    // ── Pass 3: BL-181 workforce auto-solve, player corp only (opt-out via
    //    workforce_auto; io-standing-rules.md § player-corp exception).
    //
    // Runs after contention (which reads workforce_assigned, not the target) and
    // ahead of the stack taper, rather than inline in the production loop where it
    // used to sit: the targets the taper sizes a stack's combined draw from must be
    // the same targets the sites then produce at. The solver reads only market and
    // tile state, neither of which production moves, so hoisting it changes nothing
    // else. It is handed the site's rank so it maximises against the curve the site
    // actually rides (BL-346).
    if (const auto pit = w.corporations.find(w.player_entity); pit != w.corporations.end())
    {
        for (const entity_id building_id : pit->second.assets)
        {
            const auto bit = w.buildings.find(building_id);
            if (bit == w.buildings.end())
                continue;
            building_component& b = bit->second; // mutable: the auto-solver sets the target
            if (b.decommissioned || b.ticks_remaining > 0)
                continue;
            if (!b.workforce_auto)
                continue;
            if (b.type != building_type::extraction_site &&
                b.type != building_type::processing_facility)
                continue;
            b.workforce_target = solve_workforce_target(
                w, reg, b, contention_for(w.player_entity, building_body(w, b)),
                rank_of(building_id));
        }
    }

    // ── Pass 4: BL-193 shared depletion taper, one figure per stack.
    //
    // A tile's stack works one shared reserve, so the taper is sized ONCE against the
    // stack's COMBINED nominal draw and handed to every member. Taper each site
    // against its own nominal instead and the stack desynchronises: the members drop
    // off one at a time and the deposit dribbles out. Sizing it here, before any site
    // draws, also stops the sites that run first from tapering the sites that run
    // after them within the same tick.
    if (!stack_runs.empty())
    {
        // Owner of a stacked site. Built once, and only because some tile IS stacked:
        // the taper has to price a stack's members whoever owns them (a rival may
        // stack onto the same deposit) and ownership is only readable the other way
        // round, corp → assets.
        std::map<entity_id, entity_id> corp_of_building;
        for (const auto& [c, cc] : w.corporations)
            for (const entity_id asset : cc.assets)
                corp_of_building[asset] = c;
        const auto owner_of = [&](entity_id bid) -> entity_id {
            const auto it = corp_of_building.find(bid);
            return (it != corp_of_building.end()) ? it->second : null_entity;
        };

        for (const auto& [first, last] : stack_runs)
        {
            // Combined draw: what the whole stack takes off this deposit this tick,
            // decay included. A mothballed or half-built site keeps its RANK — rank is
            // simply place in the tile's build order, the same population the placement
            // ceiling counts — but contributes nothing to the draw.
            float combined = 0.0f;
            for (std::size_t i = first; i < last; ++i)
            {
                const building_component& b = w.buildings.at(sites[i].id);
                if (b.decommissioned || b.ticks_remaining > 0)
                    continue;
                const entity_id body = building_body(w, b);
                if (body == null_entity)
                    continue;
                const float n =
                    extraction_nominal(w, reg, b, contention_for(owner_of(sites[i].id), body))
                    * placement_rules::stack_output_scalar(static_cast<int>(i - first) + 1);
                if (n > 0.0f)
                    combined += n;
            }

            float taper = 1.0f;
            if (const auto tit = w.tiles.find(sites[first].tile);
                tit != w.tiles.end() && combined > 0.0f)
            {
                const float remaining = tit->second.resource_remaining[sites[first].target];
                taper = std::clamp(remaining / (deposit_taper_ticks * combined), 0.0f, 1.0f);
            }

            for (std::size_t i = first; i < last; ++i)
            {
                stack_draw& sd      = stack_by_building.at(sites[i].id);
                sd.combined_nominal = combined;
                sd.taper            = taper;
            }
        }
    }

    // ── Pass 5: production.
    for (const entity_id corp : corp_ids)
    {
        const corporation_component& cc = w.corporations.at(corp);

        for (const entity_id building_id : cc.assets)
        {
            const auto bit = w.buildings.find(building_id);
            if (bit == w.buildings.end())
                continue;
            building_component& b = bit->second;
            const entity_id body = building_body(w, b);

            // Decommissioned buildings produce nothing — skip production entirely.
            if (b.decommissioned)
                continue;
            // Still under construction — no production yet (playtest patch, 2026-07-06).
            if (b.ticks_remaining > 0)
                continue;

            switch (b.type)
            {
                case building_type::extraction_site:
                {
                    // No entry means this site is not stacked with anything — the
                    // common case — and run_extraction tapers it against its own
                    // nominal, which is what a stack of one shares anyway (BL-347).
                    const auto sit = stack_by_building.find(building_id);
                    const stack_draw* stack =
                        (sit != stack_by_building.end()) ? &sit->second : nullptr;
                    report.buildings.push_back(
                        run_extraction(w, reg, corp, building_id, b,
                                       contention_for(corp, body), stack));
                    break;
                }
                case building_type::processing_facility:
                {
                    // BL-130: the building's own catchment market (not just "any
                    // market on the body" — a multi-market body, BL-096/BL-263,
                    // must draw its own centre's stock, not a sibling market's).
                    const entity_id market_id = market_for_tile(w, b.tile);
                    report.buildings.push_back(
                        run_processing(w, reg, corp, building_id, b, market_id,
                                       contention_for(corp, body), report));
                    break;
                }
                default:
                    break; // ports and none take no production action in L3
            }
        }
    }

    // Index the report rows once (BL-360): estimate_building_profit resolves its
    // row through building_row rather than scanning `buildings` per call.
    report.building_row.reserve(report.buildings.size());
    for (std::size_t i = 0; i < report.buildings.size(); ++i)
        report.building_row.emplace(report.buildings[i].building, i);

    // Population food demand (BL-190) is injected by inject_population_demand,
    // called from clear_markets AFTER its per-tick demand reset — injected here
    // it was erased by that reset the same tick and never reached price
    // resolution (2026-07-31 ordering fix; see market_clearing.cpp).

    // BL-048A: Body habitability aggregate — weighted mean of population-centre
    // tile habitability, where weight = population centre scale.
    {
        std::map<entity_id, std::pair<float, float>> hab_sum; // body → (weighted_sum, weight)
        // centre_ids (sorted above): float accumulation, fixed summation order.
        for (const entity_id cid : centre_ids)
        {
            const population_centre_component& pcc = w.population_centres.at(cid);
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
    const growth_params& growth_sp = reg.growth();

    // Per-body market basket, summed over ALL the body's markets (BL-357). A body
    // hosts several resource-carved markets (BL-096), and reading whichever one an
    // unordered walk found first let hash layout pick which market gated a centre's
    // growth — and ignored the rest of the body's supply. Market ids ascending so
    // the float accumulation order is fixed.
    std::map<entity_id, std::array<float, resource_count>> basket_supply, basket_demand;
    {
        std::vector<entity_id> market_ids;
        market_ids.reserve(w.markets.size());
        for (const auto& kv : w.markets)
            market_ids.push_back(kv.first);
        std::sort(market_ids.begin(), market_ids.end());
        for (const entity_id mid : market_ids)
        {
            const market_component& mc = w.markets.at(mid);
            auto& sup = basket_supply[mc.body];
            auto& dem = basket_demand[mc.body];
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                sup[r] += mc.supply[r];
                dem[r] += mc.demand[r];
            }
        }
    }

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
        // mean of supply/demand over the body's aggregated markets (BL-357), so a
        // centre grows only when its population's consumption is broadly met and
        // plateaus when it is not.
        float met_ratio = 1.0f;
        if (const auto dit = basket_demand.find(body); dit != basket_demand.end())
        {
            const auto& sup = basket_supply.at(body);
            float met_acc = 0.0f, met_weight = 0.0f;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                const float bw = growth_sp.demand_basket[r];
                if (bw <= 0.0f || dit->second[r] <= 0.0f)
                    continue;
                met_acc    += bw * std::min(1.0f, sup[r] / dit->second[r]);
                met_weight += bw;
            }
            if (met_weight > 0.0f)
                met_ratio = met_acc / met_weight;
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
                            log_reflex_agency(w, corp, building_body(w, b), "switched recipe (floored output)");
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
                        // Hold the strategic tier off this building for the same
                        // span its own state changes hold for (AI_OPPONENT.md
                        // § "Hysteresis & action budget": a building that changed
                        // state holds for C evals). This tier changed state and
                        // used not to set the cooldown, so BL-202's resume
                        // candidate could reverse an 8-tick-loss idling on the
                        // very next evaluation — the two tiers owned the same flag
                        // and neither knew the other existed. That was the larger
                        // half of the measured idle/resume oscillation.
                        // Default-constructed params, matching the default
                        // argument run_corp_strategic_step is called with at the
                        // bottom of this same function. If that call ever passes
                        // tuned params, this needs the same object — the two
                        // writers of this field must agree or the hold is
                        // asymmetric.
                        b.ai_cooldown = corp_ai_params{}.cooldown_evals;
                        // An idled port/hub stops anchoring supply — reach stale.
                        // Any other type idling changes nothing cached (2026-08-12).
                        if (building_affects_logistics(b.type))
                            invalidate_logistics_caches(w);
                        report.agency_events.push_back(
                            {corp, bid, agency_event::kind::idled, 0});
                        log_reflex_agency(w, corp, building_body(w, b), "idled a loss-making building");
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
    // evaluated or commanded (io-standing-rules.md) — EXCEPT under `spectating`
    // (BL-409), where the session has no human seat at all, so the prohibition
    // has no subject and every corp evaluates on the same cadence.
    {
        corp_ai_params p;
        p.spectating = spectating;
        run_corp_strategic_step(w, reg, report, w.current_day_tick, p);
    }

    return report;
}

// inject_substrate_demand (BL-078) was removed by BL-365: the abstract
// nation-substrate demand/supply injection is replaced entirely by real
// background corporations (corporation_generation.cpp's
// generate_background_firms) trading through the normal recipe/workforce/
// market pipeline. See docs/economy/MARKETS.md.
