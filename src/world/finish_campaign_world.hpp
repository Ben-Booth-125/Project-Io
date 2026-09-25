#pragma once
// ---------------------------------------------------------------------------
// finish_campaign_world -- everything that mutates a generated world before
// play, in one call (BL-1085, Begin retired into round six; Ben, 2026-09-24:
// "we basically want the Industrialisation round to do all of the work in
// Begin; we should completely obsolete that block of code").
// ---------------------------------------------------------------------------
//
// STARTUP.md § Handoff. Round 6's worker calls this after its own
// `make_hard_coded_world`, and Begin's cold worker (menu -> Begin with no wizard
// behind it, `--autostart`) calls it after ITS own -- the same call in the same
// order, so an adopted world and a cold build open the campaign on one state
// hash. `--serve` makes the same call. `--verify` does NOT: a golden reads
// generation alone, so it is pinned to the unsearched seed-candidate world.
//
// THE ORDER, and it is the order app::start_new_game_prelude ran on the main
// thread until this existed (the harness mirror in tools/verify/
// harness_params.hpp cites the same sequence):
//
//   1. the world-only halves of setup: the genesis history bridged from the
//      report (once, not idempotent), the survey states;
//   2. the recipe band, from `campaign_band_from_world` (below);
//   3. `assign_default_recipes` -- the first pass, before the search reads
//      recipe outputs;
//   4. the stockpile budget, then THE LANDSCAPE SEARCH (phase 6, GENERATION_
//      STRATEGY.md § The eight phases) and the winner's apply, ONE budget and
//      ONE spend reaching both;
//   5. `assign_default_recipes` again -- not belt-and-braces: every processor
//      the winner's background firms authored keeps `no_recipe` without it;
//   6. THE SETTLE: twelve quarterly ticks in spectate (world/campaign_settle).
//
// What it does NOT do is presentation: the epoch formatter, the chat line, the
// camera, the tech tree, the persona bench, the balance series and the seat
// all stay at Begin (STARTUP.md § Handoff, items 2-4).
//
// PROGRESS. Steps 4 and 6 are captioned on the caller's sink as "Searching
// the landscape" and "Proving the field" at their measured weights
// (`generation_step_cost_ms` 16 and 17). The caller publishes those two
// weights into `generation_progress::weight_after` BEFORE its
// make_hard_coded_world, so generation's plan already holds them and the bar
// never reaches its end early; this function then enters the steps as
// generation enters its own.

#include "world/charter_budget.hpp"    // charter_spend_report, charter_spend_params
#include "world/era_band.hpp"
#include "world/landscape_search.hpp"  // landscape_search_result, landscape_search_params
#include "world/stockpile_budget.hpp"  // stockpile_budget

#include <cstdint>
#include <vector>

struct generation_progress;
struct generation_report;
class  recipe_registry;
struct world;
struct world_gen_config;
struct world_params;

/// THE CAMPAIGN'S RECIPE BAND for a built world: the world's own
/// (`world::campaign_band`, BL-1101, Ben 2026-09-24), derived once at the
/// Industrialisation fold from the history's industry state -- `industrial`
/// iff a living polity's materials capacity sits at the Industrial rung at the
/// 1960 close, `ancient` otherwise -- never from the epoch year, which names the
/// calendar alone. A world whose fold never ran (a harness fixture without the
/// prehistory) carries `any` and bands here as the shipped default,
/// `industrial`, so no fixture admits both rosters at once.
///
/// ONE FUNCTION, ON PURPOSE: the round-6 worker bands its registry copy here,
/// the app's `load_economy` and `load_game_from` band here, and the harness
/// mirror bands its registry here -- so a save opens on the band its world
/// earned and a harness never bands differently from the app.
era_band campaign_band_from_world(const world& w);

/// The search params the campaign passes, keyed from the world seed exactly
/// as Begin always keyed them: `regenerate_specialists` (every candidate
/// replaces the world-gen roster, BL-977), placement seed `world_seed ^
/// 0x8A21F00D`, the parsed config's starting roster count. The budget and
/// spend are set by the caller from the stockpile.
landscape_search_params campaign_search_params(std::uint32_t world_seed,
                                               int corporation_count);

/// What the finish leaves for Begin and for the log.
struct finish_campaign_result
{
    /// The walk. `search.winner_score` is phase 6's static score of the
    /// winner, which the seat's shortlist gates and ranks on (BL-1020).
    landscape_search_result search;
    /// The world's own charter budget, as built, and the spend it was charged
    /// at (BL-1042 / BL-1064).
    stockpile_budget        stockpile;
    charter_spend_params    spend{};
    /// The winner's spend report (untouched when the budget was empty).
    charter_spend_report    charter;
    /// Wall clock of the search and of the settle, milliseconds -- REPORTED
    /// ONLY, on the BL-754 footing; the loading bar's weights are re-measured
    /// from these, never read from them at run time.
    std::int64_t            ms_search = 0;
    std::int64_t            ms_settle = 0;
    /// Each settle tick's wall clock, milliseconds, in order -- REPORTED ONLY.
    /// Measured 2026-09-24 (Release, seeds 0 and 28): one tick of the twelve
    /// runs 50-90 s and the others ~1.5 s, so the slowest is named on the
    /// `[finish_campaign_world]` line for whoever takes the tick tail up.
    std::vector<std::int64_t> settle_tick_ms;
};

/// The measured weight of the two steps this function adds to a wait, for the
/// caller to publish into `generation_progress::weight_after` before its
/// build: `generation_step_cost_ms(16) + generation_step_cost_ms(17)`.
std::int64_t finish_campaign_weight_ms(const world_params& params);

/// Finish @p w for play (the order above). @p reg is the recipe registry the
/// settle runs on: loaded from Lua by the caller on the main thread, BANDED
/// HERE, and what Begin then moves into play -- so play runs on the registry
/// the settle ran on. @p report is the build's own (the genesis history reads
/// it). @p progress may be null (a harness, `--serve`).
///
/// Prints the `[stockpile_budget]` and `[landscape_search]` lines Begin always
/// printed, and one `[finish_campaign_world]` line at the end -- the log proof
/// tools/verify/begin_adopts_check.js reads.
finish_campaign_result finish_campaign_world(world& w, const generation_report& report,
                                             recipe_registry& reg,
                                             const world_params& params,
                                             const world_gen_config& cfg,
                                             generation_progress* progress);
