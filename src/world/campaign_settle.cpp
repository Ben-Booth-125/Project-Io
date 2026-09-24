#include "world/campaign_settle.hpp"

#include "world/budget_system.hpp"     // apply_budget
#include "world/corp_command.hpp"      // run_firm_exits
#include "world/economy_system.hpp"    // run_economy_step
#include "world/hard_coded_world.hpp"  // generation_progress
#include "world/logistics.hpp"         // lp_pool_map
#include "world/market_clearing.hpp"   // clear_markets
#include "world/nation_step.hpp"       // run_nation_step
#include "world/recipe_registry.hpp"   // logistics_cost, firm_exit
#include "world/standing.hpp"          // compute_corp_standings
#include "world/supply_system.hpp"     // advance / credit / dispatch convoys
#include "world/tech_gate.hpp"         // advance_tech_gates

#include <atomic>

settle_tick_result run_settle_tick(world& w, const recipe_registry& reg, int econ_step,
                                   int day_tick, bool spectating,
                                   const settle_tick_hooks* hooks)
{
    using clk = std::chrono::steady_clock;
    const auto stamp = [&](int i) {
        if (hooks != nullptr && hooks->lap_clock != nullptr)
            (*hooks->lap_clock)[static_cast<std::size_t>(i)] = clk::now(); // reported only
    };
    const auto lap_done = [&](int lap) {
        stamp(lap + 1);
        if (hooks != nullptr && hooks->after_lap != nullptr)
            hooks->after_lap(w, lap, hooks->ctx);
    };

    settle_tick_result out;
    stamp(0);

    // BL-568: the cadence key. The day tick is 90n at a quarter boundary and
    // rotates only half the corp_ai slots; this counter advances by exactly one
    // per step across the settle, live play and --verify, which is the schedule
    // every harness certifies.
    w.current_econ_tick = econ_step;

    // BL-597: one shared passive/active Logistic Point pool for this tick --
    // handed to both dispatch_convoys (passive) and run_economy_step's march
    // pass (active) so the two genuinely contend for one anchor's capacity,
    // per LOGISTICS.md's bifold table. Local to this tick; never persisted
    // (LP is a per-tick RATE, ruling on NR-343).
    lp_pool_map tick_lp_pools;
    // BL-1066 / BL-995 (Ben, 2026-09-23): the convoy ORDER within the tick is
    // advance -> credit arrivals -> economy -> DISPATCH -> clearing -> budget
    // (SUPPLY.md § Dispatch trigger, "One beat per haul"). A delivery lands
    // BEFORE its destination clears, so it lists and sells there first; the
    // dispatch sits before the clear because auto-surplus sells every unit a
    // pool holds above its reservation, so a seller that has not chosen to
    // haul by the clear has sold at home. Dispatch reads LAST tick's resolved
    // prices and supply/demand (clear_markets rewrites them only below).
    advance_convoys(w);
    credit_arrived_convoys(w, day_tick);
    lap_done(0); // convoys: advance + arrivals

    // BL-409 / BL-630: under spectate -- a spectate session, or the settle,
    // through which nobody is seated yet -- the strategic tier evaluates every
    // corp, the player's included: the prohibition the flag lifts has no
    // subject to protect, and every corp files real returns.
    out.report = run_economy_step(w, reg, spectating, &tick_lp_pools);
    // BL-995: dispatch BEFORE the clear -- the seller hauls before it sells. It
    // draws the same tick's LP pool the march (inside run_economy_step) already
    // drew from: armies claim first (the goods-vs-force priority LOGISTICS.md
    // says must be chosen, not inherited).
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space), &tick_lp_pools);
    lap_done(1); // economy step (production + corp AI) + dispatch

    out.flows = clear_markets(w, reg, out.report);
    lap_done(2); // market clearing

    apply_budget(w, reg, out.flows, out.report.workforce_contention,
                 &out.report.budgets,
                 &out.report.buildings,        // BL-343: law enforcement seam
                 &out.report.building_labour); // BL-614: wages on the per-building grant
    // Sprint N3: the nation step -- score the due nations' weights, spend the
    // treasury against this tick's claims, dispatch the earmarked surveys.
    // After apply_budget so the treasury holds this quarter's levy and tariff;
    // before the tech gates so a `surplus` gate reads the moved balance.
    run_nation_step(w, reg, out.report, w.current_econ_tick);
    lap_done(3); // budget + nation step

    // BL-344: the tech gates once per tick, after the money loop has moved
    // balances. Monotonic and deterministic; a no-op once everything earnable
    // is earned.
    advance_tech_gates(w);
    lap_done(4); // tech gates

    // BL-262: this tick's standing profile, for the Corporations panel (a
    // transient cache the app keeps; a harness discards it). Then BL-743: the
    // insolvency wind-up, LAST -- it reads the returns this tick's apply_budget
    // just filed, and everything above already ran against the field as it
    // stood. Inert at unauthored params; the player's corp is exempt inside
    // the pass itself.
    out.standings = compute_corp_standings(w, out.flows);
    run_firm_exits(w, reg.firm_exit(), &out.report.firm_exits);
    lap_done(5); // standings + exits

    return out;
}

void run_settle(world& w, const recipe_registry& reg, int ticks,
                const settle_tick_hooks* hooks, generation_progress* progress)
{
    // Every settle tick is spectating (nobody is seated: the seat is drawn from
    // what these ticks produce) at day tick 0 (the sim loop is rebuilt at Begin
    // and never advanced on the building screen). Econ steps 0..ticks-1: live
    // play continues the counter from `ticks`.
    if (progress != nullptr) progress->report_sub(0, ticks);
    for (int econ_step = 0; econ_step < ticks; ++econ_step)
    {
        (void)run_settle_tick(w, reg, econ_step, /*day_tick=*/0, /*spectating=*/true, hooks);
        if (progress != nullptr) progress->report_sub(econ_step + 1, ticks);
    }
    if (progress != nullptr) progress->report_sub(0, 0);
}
