#pragma once
// ---------------------------------------------------------------------------
// The campaign's economy tick, and the settle that runs it twelve times
// (BL-1085, Begin retired into round six; ERAS.md § The opening position,
// STARTUP.md § Handoff).
// ---------------------------------------------------------------------------
//
// ONE FUNCTION FOR THE APP, THE WORKER AND EVERY HARNESS. Until this file the
// tick body lived twice: app::step_economy (the live loop and, until 2026-09-24,
// the winner's validation run) and tools/verify/harness_params.hpp's
// run_app_validation_tick, which restated it lap for lap with app.cpp line
// citations so a sweep's D_settle pin could be read against the app. A body
// that is copied is a body that drifts; this is the copy's retirement.
//
// The world half only. What app::step_economy does AFTER the tick -- the
// agency comms, the persona counsel, the history recorders, the strategy
// readout -- reads a const world and lives in src/core and src/ui; a harness
// never runs it and a settle inside a generation worker never needs it.
//
// DETERMINISM. Every write here is a world/* call that was already in the
// tick; the clock reads behind `settle_tick_hooks::lap_clock` are reported
// only (the BL-754 footing: nothing in the world reads a wall clock back).

#include "world/economy_system.hpp"  // economy_report
#include "world/market_clearing.hpp" // corp_cash_flow
#include "world/standing.hpp"        // corp_standing
#include "world/world.hpp"

#include <array>
#include <chrono>
#include <unordered_map>
#include <vector>

class recipe_registry;
struct generation_progress;

/// THE SETTLE IS TWELVE QUARTERLY TICKS (ERAS.md § The opening position):
/// measured, not round -- the first tick at which both the 4-tick and 8-tick
/// trailing means of the convoy dispatch count sit within 5% of the 80-tick
/// level (`haulage_measure --per-tick`, pooled over five seeds). `app` resumes
/// a loaded campaign's cadence counter at this plus the envelope's econ tick,
/// and the seat card's trailing net reads the returns these ticks file.
inline constexpr int k_campaign_settle_ticks = 12;

/// The tick's six laps, in order -- the app's own `step_economy` laps 0-5 and
/// the phase names a timed harness row prints.
inline constexpr int k_campaign_settle_lap_count = 6;
inline constexpr const char* k_campaign_settle_lap_names[k_campaign_settle_lap_count] = {
    "convoys", "run_economy_step", "clear_markets", "apply_budget+nation_step",
    "tech_gates", "standings+credit+firm_exits" };

/// What one tick hands back to a caller that keeps presentation state: the
/// app stores all three (the ledgers read the report, the Corporations panel
/// the standings, the recorders the flows); a harness discards them.
struct settle_tick_result
{
    economy_report                                 report;
    std::unordered_map<entity_id, corp_cash_flow>  flows;
    std::vector<corp_standing>                     standings;
};

/// READ hooks into a tick. Neither may write the world.
struct settle_tick_hooks
{
    /// Called after each lap with its index into `k_campaign_settle_lap_names`
    /// (world_copy_determinism digests the world there).
    void (*after_lap)(const world&, int lap, void* ctx) = nullptr;
    void* ctx = nullptr;
    /// When set, receives the steady clock at the tick's start and after each
    /// lap: seven stamps, so `lap_clock[i+1] - lap_clock[i]` is lap i. Reported
    /// only. A hooked tick's clock counts the hook inside the next lap, so an
    /// instrument times OR hooks, not both.
    std::array<std::chrono::steady_clock::time_point, k_campaign_settle_lap_count + 1>*
        lap_clock = nullptr;
};

/// ONE ECONOMY TICK on @p w -- the app's `step_economy` world half, exactly:
///
///   0. `current_econ_tick = econ_step`; advance convoys, credit the ones that
///      arrived at @p day_tick                                   (lap 0)
///   1. `run_economy_step` under @p spectating, then DISPATCH before the clear
///      (BL-995: the seller hauls before it sells), both on one Logistic
///      Point pool for the tick                                  (lap 1)
///   2. `clear_markets`                                          (lap 2)
///   3. `apply_budget`, then the nation step on the moved treasury (lap 3)
///   4. `advance_tech_gates`                                     (lap 4)
///   5. `compute_corp_standings`, then `run_firm_exits` LAST -- it reads the
///      returns this tick's budget just filed                    (lap 5)
///
/// @p econ_step is the cadence key (BL-568): the settle stamps 0..11, live
/// play continues from twelve. @p day_tick is the sim loop's day (0 for every
/// settle tick: the loop is rebuilt at Begin and only `sim_loop::tick`
/// advances it, which the building screen never calls). @p spectating lifts
/// the player-corp exemption from the scorer: true through the settle (nobody
/// is seated yet, BL-630) and under a spectate session; false in play.
/// `world::current_day_tick` is NOT written here -- the app mirrors it each
/// in-game frame; through the settle it keeps what generation left.
settle_tick_result run_settle_tick(world& w, const recipe_registry& reg, int econ_step,
                                   int day_tick, bool spectating,
                                   const settle_tick_hooks* hooks = nullptr);

/// THE SETTLE: econ steps 0..@p ticks-1 of `run_settle_tick`, day tick 0,
/// spectating, on the searched landscape -- phase 6's "validate dynamically,
/// once" (GENERATION_STRATEGY.md § Three passes). The balances, pools, filed
/// returns and prices it leaves ARE the opening position (ERAS.md § The opening
/// position); the calendar is rebased at Begin, so it has no calendar meaning.
///
/// @p progress, when non-null, has its inner bar counted per tick
/// (`report_sub(done, ticks)`) and cleared at the end; nothing is read back.
/// @p hooks is handed to every tick.
void run_settle(world& w, const recipe_registry& reg, int ticks = k_campaign_settle_ticks,
                const settle_tick_hooks* hooks = nullptr,
                generation_progress* progress = nullptr);
