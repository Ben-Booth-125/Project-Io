// ---------------------------------------------------------------------------
// Shared harness world_params helper
// ---------------------------------------------------------------------------
// Wiring the Era -1 year-tick sim into generation (2026-08-12) added ~23 s to
// EVERY world any caller builds, and most harnesses build two or more. Harnesses
// that audit tiles, roads, corporations, economy arithmetic or determinism do
// not test the pre-epoch era at all, and were paying its whole cost — several
// past their ctest timeouts, which is what stopped the gate being trustworthy.
//
// `no_prehistory()` says, in one greppable word, "this harness does not test the
// era". It is a SCOPE declaration, not a speed hack: the harnesses that DO test
// the era (era_world_harness, history_sim_harness, history_sweep,
// stepped_clock_harness) must not use it.
//
// Determinism is untouched — prehistory_years is part of world_params, so the
// same params still produce the same world.
#pragma once

#include "scripting/lua_state.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world_gen_config.hpp"

/// The caller's params with the pre-epoch year-tick sim switched off.
inline world_params no_prehistory(world_params p = {})
{
    p.prehistory_years = 0;
    return p;
}

// ---------------------------------------------------------------------------
// The two arcs (BL-1044)
// ---------------------------------------------------------------------------
// BL-1044 turned the Industrialisation span and BL-1037's corridor tier ON by
// default, so `world_params{}` is the SHIPPED world: the 1960 close, and the
// charter web the world's own stockpile buys. The world before it — span off,
// tier off — is the LEGACY arc: the world BL-1031's digest pins were taken on,
// kept buildable so those pins stay a live check rather than a record (Ben,
// 2026-09-18: "the BL-1031 pins stay as legacy rows and are never overwritten").
// A harness names its arc here instead of flipping the two switches itself, so
// "legacy" means one world everywhere.
enum class world_arc
{
    shipped, ///< `world_params{}`'s own: the span and the tier on (BL-1044)
    legacy,  ///< the pre-BL-1044 world: the span off, the tier off
};

inline const char* world_arc_name(world_arc a)
{
    return a == world_arc::legacy ? "legacy" : "shipped";
}

/// @p p (default `world_params{}`) on @p a's arc. The shipped arc changes
/// nothing; the legacy arc switches the span and the tier off.
inline world_params arc_params(world_arc a, world_params p = {})
{
    if (a == world_arc::legacy)
    {
        p.industrialisation_span_enabled  = false;
        p.resume_seeds_corridor_tier = false;
    }
    return p;
}

// ---------------------------------------------------------------------------
// Shared generation-config helper (2026-08-26, NR-686's sibling defect)
// ---------------------------------------------------------------------------
// `make_hard_coded_world`'s `gen_cfg` parameter defaults to the C++ fallback,
// which prices **10 of 47** resources where `scripts/world_gen.lua` authors
// **42**. Markets are seeded from that table, so a harness that omits the
// argument measures a world in which stone, timber, clay, fibre, planks and
// tools are UNPRICED — not cheap, unquoted — and every site working them is
// unsellable at any workforce.
//
// That is not a small divergence and it is invisible: it invalidated a whole
// sprint's spawn-viability numbers, and was found in THREE harnesses on one day
// (spawn_solvency, material_floor's counterfactual, player_seed_sweep). Loading
// world_gen.lua into the Lua state is NOT sufficient — the table must be PARSED
// into a config object and PASSED. One harness made exactly that half-fix.
//
// Use this from any harness whose subject touches PRICES, MARKETS, INCOME or
// PROFITABILITY. A Lua-free logic harness (tile generation, partitioning,
// determinism) legitimately keeps the fallback — prices are not its subject —
// and `world_gen_config::is_fallback` lets it SAY so rather than leave a reader
// guessing which of the two worlds a number came from.
inline world_gen_config parsed_gen_config(lua_state& lua)
{
    world_gen_config cfg{};
    cfg.load_from_lua(lua);
    return cfg;
}

// ---------------------------------------------------------------------------
// The shipped start's economic substrate (BL-979)
// ---------------------------------------------------------------------------
// app::start_new_game_prelude does not spawn the background economy with a bare
// generate_background_firms call any more. Since BL-770 phase 6 it SEARCHES:
// setup_world + load_economy, then `search_landscape` over all three axes —
// roster, placement and road tier, every candidate REPLACING the world-gen
// specialists (BL-977: `remove_specialist_roster` then `generate_corporations`
// from `world::gen_settlement`) — then `apply_landscape_candidate(winner)`,
// then a second assign_default_recipes.
//
// Nine instruments kept the bare call after the search landed, so every one of
// them measured the SEED CANDIDATE — the walk's starting point — rather than
// the world the player gets. That is BL-714's "instruments that cannot see
// their subject" recurring, and it is why this helper exists: ONE place that
// mirrors app.cpp's sequence, so a harness cannot quietly drift from the game
// again. Use it from any harness whose subject is the shipped economy.
//
// It MIRRORS; it does not re-implement. The search params are the ones app.cpp
// passes (read them there before changing these) and the search itself is
// src/world/landscape_search.cpp. A single-candidate walk (`search = false`)
// applies the seed candidate unsearched — the specialist roster regenerated at
// the seed's count and placement, plus generate_background_firms(seed) — so an
// instrument that deliberately wants the seed candidate says so with the flag
// rather than by calling generate_background_firms itself.
//
// COST. The search is ~19 evaluations at ~1 s each on a live world (app.cpp's
// 2026-09-07 measurement), so a harness that builds N worlds pays ~20 s x N
// more than it did. That is the price of measuring the subject.

#include "world/corporation_generation.hpp" // assign_default_recipes
#include "world/landscape_search.hpp"
#include "world/stockpile_budget.hpp"     // BL-1042: the shipped path's charter budget
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <vector>

/// BL-1032 — a charter budget handed to the search, for an instrument that
/// wants one. NULL IS THE SHIPPED PATH (BL-1042): app::start_new_game_prelude
/// builds the world's own stockpile budget (`build_stockpile_budget`, charged at
/// `stockpile_charter_spend`) and passes it to BOTH the search and the winner's
/// apply, and `apply_shipped_landscape` does exactly that when this is null.
/// The Industrialisation span runs by default (BL-1044), so that budget is the
/// world's own; on a world the span did not run on (the legacy arc) it is EMPTY,
/// and an empty budget is the pre-budget world byte for byte. A non-null budget
/// (even an empty one) is an instrument's own, and REPLACES the stockpile.
struct harness_charter_input
{
    const charter_budget* budget = nullptr;
    /// Read only with a non-null `budget`; the shipped path charges the
    /// stockpile at `stockpile_charter_spend(stockpile)`.
    charter_spend_params  spend{};
    /// BL-1043 — THE SHIPPED BUDGET AT THE INSTRUMENT'S PRICES. Read ONLY when
    /// `budget` is null, i.e. on the shipped path, where the budget is still the
    /// one `build_stockpile_budget` built from the world's own stockpile: this
    /// replaces `stockpile_charter_spend(stockpile)` and NOTHING ELSE, so a row
    /// that sets it is measuring the real budget at a price off the matrix, never
    /// a synthetic budget. Null is the shipped spend. With a non-zero
    /// `stockpile_price_divisor` its firm price is REPLACED by the one the
    /// budget derived (a divisor row); at 0 it is taken whole (a FIXED-price row,
    /// BL-1043 stage 1's).
    const charter_spend_params* stockpile_spend = nullptr;
    /// BL-1064 — THE DIVISOR the shipped path's budget derives its firm price by
    /// (`build_stockpile_budget`'s argument). 0 is the shipped constant,
    /// `k_stockpile_price_divisor`. Read ONLY when `budget` is null. A value < 0
    /// is passed through, and the builder rejects it (the budget is then empty:
    /// today's world, and the rejection is on `shipped_landscape::stockpile`).
    std::int64_t stockpile_price_divisor = 0;
    /// Receives the WINNER'S spend report (the search's own evaluations report
    /// nothing) — the instrument's budget's, or on the shipped path the
    /// stockpile's. Untouched when the budget in force is empty.
    charter_spend_report* report = nullptr;
};

struct shipped_landscape
{
    landscape_search_result search;   ///< the walk; `winner` is what was applied
    /// BACKGROUND firms the applied candidate laid, ascending id. Specialists
    /// are in `specialists`: since BL-977 the candidate replaces that roster
    /// too, and an instrument asking "which are the companies" must not be
    /// handed the corporations as well.
    std::vector<entity_id>  firms;
    std::vector<entity_id>  specialists;  ///< the candidate's specialist roster, ascending id
    bool                    searched = true;  ///< false: the seed candidate was applied unsearched
    /// BL-1042: the shipped path's stockpile budget and the account of every
    /// point, as app.cpp builds it. Built only when the instrument passed no
    /// budget of its own (`stockpile_path`); default (empty) otherwise.
    stockpile_budget        stockpile;
    bool                    stockpile_path = false;
    /// BL-1064: the spend the stockpile was ACTUALLY charged at — its derived
    /// firm price, or the instrument's (`harness_charter_input`). What a row
    /// reports it charged is read from here, never restated. Default (empty)
    /// off the stockpile path.
    charter_spend_params    stockpile_spend{};
};

/// The search params app.cpp passes, keyed from the world seed exactly as it
/// keys them. `regenerate_specialists = true` since BL-977; the start's
/// corporation count is `world_gen_config::corporation_count` (app.cpp:1057
/// reads the PARSED config). The default is that field's own default rather
/// than a restated literal (BL-1030), so a caller with no parsed config gets
/// the same number the config struct does; `build_app_start_world` below
/// passes the parsed value.
inline landscape_search_params shipped_search_params(
    std::uint32_t world_seed,
    int corporation_count = world_gen_config{}.corporation_count)
{
    landscape_search_params sp;
    sp.regenerate_specialists  = true;
    sp.seed                    = world_seed ^ 0x8A21F00Du;
    sp.start.placement_seed    = sp.seed;
    sp.start.corporation_count = corporation_count;
    return sp;
}

/// Lay the shipped start's economic substrate onto @p w — a fresh
/// make_hard_coded_world, with @p reg loaded — in app.cpp's order: load_economy's
/// recipe pass, the search, the winner applied, the second recipe pass. Returns
/// the walk and the firms it added, so an instrument can print what it measured.
///
/// @p corporation_count is the search's starting roster (app.cpp:1057,
/// `m_worldgen_cfg.corporation_count`). It defaults to `world_gen_config`'s own
/// default — 8, the value scripts/world_gen.lua also authors — so every caller
/// that omits it gets exactly what it got when this was a hard-coded 8 (BL-1030).
inline shipped_landscape apply_shipped_landscape(
    world& w, const recipe_registry& reg, std::uint32_t world_seed, bool search = true,
    int corporation_count = world_gen_config{}.corporation_count,
    const harness_charter_input& charter = {})
{
    shipped_landscape out;
    out.searched = search;

    // load_economy's pass. Idempotent, so a caller that already ran it pays one
    // map walk and nothing else.
    assign_default_recipes(w, reg);

    std::vector<entity_id> before;
    before.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        before.push_back(kv.first);
    std::sort(before.begin(), before.end());

    landscape_search_params sp = shipped_search_params(world_seed, corporation_count);
    // BL-1042 — THE BUDGET, as app::start_new_game_prelude builds it: the
    // world's own stockpile (`build_stockpile_budget`) at the stockpile spend,
    // unless an instrument handed in a budget of its own. ONE budget and ONE
    // spend reach BOTH the search and the winner's apply below, and the budget
    // lives in `out` so it outlives the search. With the span off (the legacy
    // arc) the stockpile is empty, the search is the pre-budget one, and the
    // apply's legacy branch runs first; a budget that opens no specialist falls
    // back the same way inside world/* (NR-910).
    const charter_budget* budget = charter.budget;
    charter_spend_params  spend  = charter.spend;
    if (budget == nullptr)
    {
        // BL-1064: the price is derived as the budget is built, by the shipped
        // divisor unless the instrument named another for this row.
        out.stockpile      = build_stockpile_budget(w, charter.stockpile_price_divisor != 0
                                                           ? charter.stockpile_price_divisor
                                                           : k_stockpile_price_divisor);
        out.stockpile_path = true;
        budget             = &out.stockpile.budget;
        // BL-1043: the shipped spend unless the instrument named its own prices
        // for this row. The BUDGET is the shipped builder's either way.
        spend              = charter.stockpile_spend != nullptr ? *charter.stockpile_spend
                                                                : stockpile_charter_spend(out.stockpile);
        if (charter.stockpile_spend != nullptr && charter.stockpile_price_divisor != 0)
            spend.firm_price_points = out.stockpile.firm_price_points;   // a divisor row
        out.stockpile_spend = spend;
    }
    sp.budget = budget;
    sp.spend  = spend;
    if (search)
    {
        out.search = search_landscape(w, reg, sp);
    }
    else
    {
        out.search.seed_candidate = sp.start;
        out.search.winner         = sp.start;
    }
    // app.cpp's winner apply, verbatim: the 7-argument overload with the same
    // budget, spend and a report. An EMPTY budget forwards to the legacy
    // 4-argument call inside world/* before anything else is read.
    apply_landscape_candidate(w, reg, out.search.winner, /*regenerate_specialists=*/true,
                              budget, spend, charter.report);

    // app.cpp's second pass, and not belt-and-braces: without it every processor
    // a background firm authored keeps `no_recipe` for the whole campaign.
    assign_default_recipes(w, reg);

    std::vector<entity_id> after;
    after.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        after.push_back(kv.first);
    std::sort(after.begin(), after.end());
    std::vector<entity_id> added;
    std::set_difference(after.begin(), after.end(), before.begin(), before.end(),
                        std::back_inserter(added));
    for (const entity_id cid : added)
        (w.corporations.at(cid).is_background ? out.firms : out.specialists).push_back(cid);
    return out;
}

/// One manifest line per world, so a reading names the landscape it was taken on.
inline void print_shipped_landscape(const shipped_landscape& s)
{
    const landscape_search_result& r = s.search;
    if (!s.searched)
    {
        std::printf("landscape: SEED CANDIDATE, unsearched (corps=%d placement=%08X "
                    "road_tier=%u); %zu background firms\n",
                    r.winner.corporation_count, r.winner.placement_seed,
                    static_cast<unsigned>(r.winner.road_tier), s.firms.size());
        return;
    }
    std::printf("landscape: search WINNER corps=%d placement=%08X road_tier=%u "
                "(seed candidate placement=%08X); composite %.6f -> %.6f; "
                "%d evaluations, %d accepted; %zu background firms\n",
                r.winner.corporation_count, r.winner.placement_seed,
                static_cast<unsigned>(r.winner.road_tier),
                r.seed_candidate.placement_seed,
                r.seed_score.composite, r.winner_score.composite,
                r.evaluations, r.accepted, s.firms.size());
}

// ---------------------------------------------------------------------------
// The campaign start, in app order (BL-1030)
// ---------------------------------------------------------------------------
// `apply_shipped_landscape` mirrors ONE phase of the start. An instrument whose
// subject is the world the player is handed needs the whole sequence, and the
// pieces around the search were each dropped somewhere: player_seed_sweep built
// with no scripts/world_gen.lua config (C++ fallback prices), no scripts/works.lua
// registry (the Era -1 sim ran with works disabled), no `set_era` (the registry
// stayed on the `any` band, so every era's recipes were live), and settled with
// ticks numbered from 1, no firm exits, and a non-zero day tick where the app
// passes 0. Each is a different world wearing the same seed, and nothing failed.
//
// TWO HELPERS, one per app phase, so an instrument can read the world at the
// seam between them (the landscape applied; the validation run closed):
//
//   * `build_app_start_world` — app::begin_new_game (the config and works,
//     then generation) and app::start_new_game_prelude (setup_world's world
//     mutations, load_economy, the search and its winner, the second recipe
//     pass). Everything up to the moment the validation run is armed.
//   * `run_app_validation_settle` — app::poll_worldgen's validation run: the
//     `validation_ticks` calls to app::step_economy, with the tick state the app
//     actually has at that point rather than the state a harness would assume.
//
// Both MIRROR app.cpp and cite it line by line; neither re-implements a system.
// If a cited line moves or changes, this is the other half of that edit. The
// presentation calls step_economy makes after its phase 5 lap (agency comms,
// battle dispatches, persona counsel, history recorders, strategy readout) take
// `const world&` and are not mirrored — they cannot move a byte of the world,
// and they live in src/core or src/ui, outside this build. The standings cache
// (app.cpp:1308) sits INSIDE phase 5 and IS mirrored (BL-1033): it is in
// src/world, reads a const world and the tick's flows, and writes only its
// return value, so a timed phase 5 lines up with the app's lap.
//
// Written for player_seed_sweep; it exists so every instrument measuring the
// shipped start can stop restating the sequence. Other harnesses are NOT
// migrated by the item that added it, and their numbers are unchanged.

#include "world/budget_system.hpp"     // apply_budget
#include "world/corp_ai.hpp"           // corp_strategic_eval_due (live-window due COUNT)
#include "world/corp_command.hpp"      // run_firm_exits
#include "world/orbital_system.hpp"    // advance_orbits (live window)
#include "world/economy_system.hpp"    // run_economy_step, economy_report
#include "world/history_log.hpp"       // seed_genesis_history
#include "world/market_clearing.hpp"   // clear_markets
#include "world/nation_step.hpp"       // run_nation_step
#include "world/recipe_registry.hpp"   // recipe_registry, era_band_for_epoch
#include "world/standing.hpp"          // compute_corp_standings (phase 5, as app.cpp:1308)
#include "world/supply_system.hpp"     // dispatch/advance/credit convoys
#include "world/survey_system.hpp"     // init_survey_states
#include "world/tech_gate.hpp"         // advance_tech_gates
#include "world/works_roster.hpp"      // works_registry

#include <chrono>                      // app_tick_timing (diagnostic wall time only)

/// `app::validation_ticks` (app.hpp:746), restated because app.hpp brings SDL.
/// If the app's number moves, this one moves with it.
inline constexpr int k_app_validation_ticks = 12;

/// Everything the app holds for one campaign start that the world was built
/// from. Owned by the caller, so two starts (a reproduction check) never share
/// a registry or a config.
struct app_start_world
{
    world_params      params{};   ///< app::m_active_world_params
    world_gen_config  cfg{};      ///< app::m_worldgen_cfg
    works_registry    works;      ///< app::m_works
    recipe_registry   reg;        ///< app::m_registry
    generation_report report{};   ///< app::m_generation_report
    world             w;          ///< app::m_world
    /// The search. `land.search.winner_score` is app::m_landscape_winner_score
    /// (app.cpp:1074), the score the seat reads.
    shipped_landscape land;
};

/// The first half of `build_app_start_world`: everything up to the landscape
/// search — app::begin_new_game and app::start_new_game_prelude as far as
/// load_economy's recipe pass (app.cpp:1193). Split out (BL-1033) so an
/// instrument can time the landscape phase on its own; `build_app_start_world`
/// is exactly this followed by `apply_app_start_landscape` ON THE SAME OBJECT,
/// so the two paths are one sequence and the BL-1031 digest pins check both.
///
/// COPYING THE RESULT WAS UNSAFE UNTIL BL-1034. MEASURED 2026-09-17 on seed 28:
/// a copied base matched the pins at D_search and D_land and DIFFERED at
/// D_settle and D_seat — a COPIED `world` settled to D_settle 18F78EB9B2B20F29
/// against the pin's 265C48A23E313B1A whether it was copied before the landscape
/// or after it, while the original settled with a COPIED `recipe_registry`
/// matched. The cause was the copy's iteration order (MSVC reverses every shared
/// bucket of an unordered_map it copies) meeting an order-dependent float sum in
/// the tick; world's stores now copy in order (faithful_unordered_map.hpp), and
/// tools/verify/world_copy_determinism.cpp is the check that a copy settles as
/// its original. A SAVE ROUND TRIP is not a copy and still reorders them.
inline void build_app_base_world(lua_state& lua, const world_params& params,
                                 app_start_world& out)
{
    out.params = params;

    // --- app::begin_new_game (app.cpp:502-557) -----------------------------
    // app.cpp:504-506 — the world-gen config is PARSED and passed, not merely loaded.
    lua.load("scripts/world_gen.lua");
    out.cfg = world_gen_config{};
    out.cfg.load_from_lua(lua);
    // app.cpp:546 -> app::ensure_works_loaded (app.cpp:1140-1148), loaded once.
    if (out.works.size() == 0)
    {
        lua.load("scripts/works.lua");
        out.works.load_from_lua(lua);
    }
    // app.cpp:553-554 — the worker: report, config and works all passed. The
    // progress sink is atomics the loading screen reads, never an input.
    out.report = generation_report{};
    out.w = make_hard_coded_world(params, &out.report, out.cfg, /*progress=*/nullptr,
                                  &out.works);

    // --- app::start_new_game_prelude (app.cpp:980-1108) --------------------
    // app.cpp:1014 -> app::setup_world (app.cpp:1364-1519). Its world is already
    // built (the app.cpp:1390 branch is not taken on this path); of the rest,
    // only two calls write the world — the others frame the camera and the chat.
    seed_genesis_history(out.w, out.report);   // app.cpp:1405
    init_survey_states(out.w);                 // app.cpp:1517

    // app.cpp:1016 -> app::load_economy (app.cpp:1150-1237).
    lua.load("scripts/recipes.lua");           // app.cpp:1153
    lua.load("scripts/economy.lua");           // app.cpp:1154
    out.reg.load_from_lua(lua);                // app.cpp:1155
    out.reg.set_era(era_band_for_epoch(params.epoch_year)); // app.cpp:1165
    // app.cpp:1176 ensure_works_loaded: already loaded above. app.cpp:1180-1181
    // (tech_tree.lua) and 1220-1228 (persona bench) write no world state, and
    // app.cpp:1209-1221 only reads it.
    assign_default_recipes(out.w, out.reg);    // app.cpp:1193
}

/// The second half of `build_app_start_world`: the landscape phase, on the world
/// `build_app_base_world` left in the same @p out (the app's own sequence; see above
/// for what copying it cost before BL-1034).
inline void apply_app_start_landscape(app_start_world& out,
                                      const harness_charter_input& charter = {})
{
    // app.cpp:1052-1081 — the search, the winner applied — and app.cpp:1092, the
    // second recipe pass: apply_shipped_landscape is that block, given the
    // PARSED config's roster count (app.cpp:1057).
    // BL-1042: with no budget handed in, the mirror builds the world's own
    // stockpile budget and passes it to the search and the apply, as the app
    // does; an instrument's budget replaces it.
    out.land = apply_shipped_landscape(out.w, out.reg, out.params.seed, /*search=*/true,
                                       out.cfg.corporation_count, charter);
}

/// Build @p out as the app builds a campaign start from @p params, stopping
/// where the app arms the validation run (app.cpp:653-655). @p lua plays
/// app::m_lua: one long-lived state the scripts are (re)loaded into.
inline void build_app_start_world(lua_state& lua, const world_params& params,
                                  app_start_world& out,
                                  const harness_charter_input& charter = {})
{
    build_app_base_world(lua, params, out);
    apply_app_start_landscape(out, charter);
}

/// Wall-clock cost of a run of economy ticks — BL-1033. A DIAGNOSTIC read from
/// the steady clock around calls that are made anyway: nothing in the world
/// reads it, so a timed run and an untimed one step the same bytes.
///
/// The phases follow app::step_economy's own laps 0-5 (app.cpp:1239-1315), so a
/// reading here lines up with the app's `[validation phases]` print. Phase 5 is
/// the app's "standings + convoy credit + exits" lap (app.cpp:1315):
/// compute_corp_standings, credit_arrived_convoys and run_firm_exits.
///
/// A LOWER BOUND, NOT THE APP'S TICK. The app's laps 6-8 (app.cpp:1317-1354) —
/// agency comms, battle dispatches, persona counsel, the history recorders and
/// the strategy readout — are NOT run and NOT timed here: they live in src/core
/// and src/ui, outside this build. So a tick's figure is a lower bound on the
/// app's step_economy, and the gap WIDENS WITH DENSITY: the comms and the
/// recorders walk corporations, buildings and markets, which a denser web grows.
inline constexpr int k_app_tick_phase_count = 6;
inline constexpr const char* k_app_tick_phase_names[k_app_tick_phase_count] = {
    "convoys", "run_economy_step", "clear_markets", "apply_budget+nation_step",
    "tech_gates", "standings+credit+firm_exits" };

struct app_tick_timing
{
    /// Per tick, the economy step's laps 0-5 only, milliseconds — a LOWER BOUND
    /// on the app's step_economy (see above).
    std::vector<double> tick_ms;
    /// Each phase summed over every tick, milliseconds.
    double phase_ms[k_app_tick_phase_count] = {};
    /// LIVE WINDOW ONLY: the frame loop's world writes between boundaries
    /// (orbits, surveys), summed. Not part of any tick's figure.
    double between_ms = 0.0;
    /// LIVE WINDOW ONLY, per tick: how many corporations `corp_strategic_eval_due`
    /// names at the day tick app::step_economy hands post_persona_counsel
    /// (app.cpp:1323,1338). A COUNT, NOT A COST DRIVER: post_persona_counsel
    /// evaluates at most the one open-channel corporation per tick (BL-398,
    /// session_history.cpp:206-220), so the blackboard export and the pack
    /// evaluation — counsel's expensive half — are bounded whatever the density.
    /// The id sort and one channel per due corp still walk the corporation set,
    /// so a small O(N log N) residue does follow density. Counsel is not
    /// mirrored and not timed here, residue included.
    std::vector<int> strategic_evals_due;
};

namespace harness_timing_detail {
using clk = std::chrono::steady_clock;
inline double ms_between(clk::time_point a, clk::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}
} // namespace harness_timing_detail

/// Run the validation run on @p w exactly as app::poll_worldgen does
/// (app.cpp:575-606): `ticks` calls to app::step_economy (app.cpp:1239-1355),
/// under the tick state the app has at that moment. Does NOT seat — the app
/// seats after the run closes (app.cpp:603), and the caller does the same with
/// `seat_player_corporation`.
///
/// THE TICK STATE, and why each value is what it is:
///   * econ tick 0..ticks-1. app.cpp:655 sets m_econ_steps = 0 when the run is
///     armed, and step_economy stamps `current_econ_tick = m_econ_steps++`
///     (app.cpp:1258) — so the first validation tick is 0, not 1.
///   * day tick 0, for every tick. The sim loop is rebuilt at app.cpp:1010 and
///     only `sim_loop::tick()` advances its day counter; the frame loop skips
///     that call on every screen but `in_game` (app.cpp:320-324), and the
///     validation run happens on the `building` screen. So
///     `m_sim_loop.day_tick()` (app.cpp:1309) is 0 throughout.
///   * `world::current_day_tick` is NOT written. The app mirrors it only inside
///     the in_game frame loop (app.cpp:401); through the validation run it keeps
///     the value generation left, and this helper leaves it alone likewise.
///
///   * phase 5 runs compute_corp_standings (app.cpp:1308) and discards the
///     result, as the app's UI cache is nothing a world reads. Its signature is
///     `const world&` plus the tick's flows by const reference, so it writes
///     nothing any digest reads; it is here so the timed phase is the app's lap.
///
/// @p timing, when non-null, receives each tick's wall time and the per-phase
/// sums (BL-1033). The clock reads sit between calls the loop makes anyway.
/// NOT TIMED, AND NOT RUN: step_economy's laps 6-8 (app.cpp:1317-1354). Through
/// the validation run the app suppresses battle dispatches and persona counsel
/// itself (app.cpp:1329,1337), but it still runs the agency comms, the history
/// recorders and the strategy readout, so a val ms per tick here is a LOWER
/// BOUND on the app's validation tick that widens with density.
/// ONE validation tick, econ step @p econ_step — the body `run_app_validation_settle`
/// loops, split out (BL-1034) so an instrument can step two worlds in lockstep and
/// read each between laps. Everything above about the tick state holds here.
///
/// @p after_lap, when set, is called after each lap with its index into
/// `k_app_tick_phase_names`. It is a READ hook (world_copy_determinism digests the
/// world there) and must not write the world. A hooked tick's @p timing would
/// count the hook inside the next lap, so an instrument times OR hooks, not both.
inline void run_app_validation_tick(world& w, const recipe_registry& reg, int econ_step,
                                    app_tick_timing* timing = nullptr,
                                    void (*after_lap)(const world&, int lap, void* ctx) = nullptr,
                                    void* after_lap_ctx = nullptr)
{
    using harness_timing_detail::clk;
    using harness_timing_detail::ms_between;
    constexpr int k_validation_day_tick = 0; // m_sim_loop.day_tick(), see above
    const auto lap_done = [&](int lap) {
        if (after_lap != nullptr)
            after_lap(w, lap, after_lap_ctx);
    };

    const clk::time_point t0 = clk::now();
    w.current_econ_tick = econ_step;                                  // app.cpp:1258
    lp_pool_map tick_lp_pools;                                        // app.cpp step_economy
    advance_convoys(w);                  // BL-995 order: advance -> arrivals -> economy -> dispatch -> clear
    credit_arrived_convoys(w, k_validation_day_tick);
    const clk::time_point t1 = clk::now();
    lap_done(0);
    // app.cpp:1281-1283: `m_ui.spectating || m_validation_run`, and
    // m_validation_run is true for every tick of this run (app.cpp:653).
    economy_report report = run_economy_step(w, reg, /*spectating=*/true,
                                             &tick_lp_pools);
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),   // BL-995: before the clear
                     reg.logistics_cost(convoy_mode::space), &tick_lp_pools);
    const clk::time_point t2 = clk::now();
    lap_done(1);
    const auto flows = clear_markets(w, reg, report);                 // app.cpp:1285
    const clk::time_point t3 = clk::now();
    lap_done(2);
    apply_budget(w, reg, flows, report.workforce_contention,          // app.cpp:1287-1290
                 &report.budgets, &report.buildings, &report.building_labour);
    run_nation_step(w, reg, report, w.current_econ_tick);             // app.cpp:1297
    const clk::time_point t4 = clk::now();
    lap_done(3);
    advance_tech_gates(w);                                            // app.cpp:1303
    const clk::time_point t5 = clk::now();
    lap_done(4);
    // Phase 5, the app's lap(5) (app.cpp:1315): standings + convoy credit + exits.
    // compute_corp_standings reads a const world into the app's UI cache; the
    // harness discards it (nothing in a world reads that cache).
    (void)compute_corp_standings(w, flows);
    run_firm_exits(w, reg.firm_exit(), &report.firm_exits);
    const clk::time_point t6 = clk::now();
    lap_done(5);
    // app.cpp:1317-1354, laps 6-8: agency comms, the history recorders and the
    // strategy readout (counsel and battle dispatches are suppressed through
    // this run by the app). Presentation over a const world, NOT mirrored and
    // NOT TIMED — so tick_ms is a lower bound on the app's tick.
    if (timing != nullptr)
    {
        timing->tick_ms.push_back(ms_between(t0, t6));
        timing->phase_ms[0] += ms_between(t0, t1);
        timing->phase_ms[1] += ms_between(t1, t2);
        timing->phase_ms[2] += ms_between(t2, t3);
        timing->phase_ms[3] += ms_between(t3, t4);
        timing->phase_ms[4] += ms_between(t4, t5);
        timing->phase_ms[5] += ms_between(t5, t6);
    }
}

/// The validation run itself (documented above): econ steps 0..ticks-1, each one
/// `run_app_validation_tick`.
inline void run_app_validation_settle(world& w, const recipe_registry& reg,
                                      int ticks = k_app_validation_ticks,
                                      app_tick_timing* timing = nullptr)
{
    for (int econ_step = 0; econ_step < ticks; ++econ_step)  // app.cpp:655, 579-583
        run_app_validation_tick(w, reg, econ_step, timing);
}

/// A LIVE WINDOW after the seat — BL-1033. @p ticks economy ticks as the app
/// runs them once play has begun (app::run's in_game frame loop, app.cpp:380-423,
/// into app::step_economy, app.cpp:1239-1320), on a world that has been settled
/// by `run_app_validation_settle` and SEATED by `seat_player_corporation`.
///
/// THE TICK STATE, and what differs from the validation run:
///   * NOT SPECTATING. app.cpp:1282 passes `m_ui.spectating || m_validation_run`;
///     app.cpp:590 cleared m_validation_run before the seat, and a played
///     session's `ui_state::spectating` is false (ui_state.hpp:877). So the
///     seated corp is excluded from the scorer, as a human's is.
///   * econ tick continues from the validation run: m_econ_steps was reset to 0
///     when the run was armed (app.cpp:655) and the run stepped it
///     `validation_ticks` times, so live tick k (1-based) stamps
///     `validation_ticks + k - 1` (app.cpp:1258). @p first_econ_step is that
///     first value.
///   * day tick 90k. finish_new_game rebuilds the sim loop at day 0
///     (app.cpp:1131); econ tick k fires as the day counter reaches
///     k x `econ_tick_days` (sim_loop.cpp on_sim_step), the frame mirrors that
///     day onto the world first (app.cpp:401), and credit_arrived_convoys reads
///     it (app.cpp:1309).
///   * BETWEEN BOUNDARIES the frame loop writes the world twice: advance_orbits
///     (app.cpp:385) and advance_surveys (app.cpp:395). The app spreads both over
///     the quarter's frames; here each takes the quarter's 90 days in one call
///     before the step. Surveys advance on whole days, so they land the same;
///     the orbit angle is a float sum and may differ in its last bits from any
///     given frame split — which the app's own frame rate already varies.
///   * PHASE 5 IS THE APP'S LAP 5 (app.cpp:1315): compute_corp_standings
///     (app.cpp:1308, a const world and the tick's flows in, the UI cache out —
///     discarded here, as no world reads it), credit_arrived_convoys and
///     run_firm_exits.
///   * NOT MIRRORED AND NOT TIMED: the agent seam's drain (app.cpp:417-419; no
///     agent attached) and step_economy's laps 6-8 (app.cpp:1317-1354) — agency
///     comms, battle dispatches, persona counsel, the history recorders and the
///     strategy readout. They read a const world and live in src/core and src/ui,
///     outside this harness's build. So a live ms per tick here is a LOWER BOUND
///     on the app's step_economy, and the gap widens with density (the comms and
///     recorders walk corporations, buildings and markets).
///   * PERSONA COUNSEL'S EXPENSIVE HALF IS BOUNDED, ITS WALK IS NOT.
///     post_persona_counsel evaluates at most the one open-channel corporation
///     per tick (BL-398, session_history.cpp:206-220), so the blackboard export
///     and the pack evaluation do not scale with corporation count; the id sort
///     and one channel per due corp still walk the set, a small O(N log N)
///     residue that does follow density. @p timing's `strategic_evals_due` is a
///     COUNT of corporations due at the day tick, for reference only, and none
///     of counsel is timed here.
inline void run_app_live_window(world& w, const recipe_registry& reg, int first_econ_step,
                                int ticks, app_tick_timing* timing = nullptr)
{
    using harness_timing_detail::clk;
    using harness_timing_detail::ms_between;
    constexpr int k_econ_tick_days = 90;   // sim_loop::econ_tick_days (sim_loop.hpp:28)
    for (int k = 1; k <= ticks; ++k)
    {
        const int day = k * k_econ_tick_days;
        const clk::time_point b0 = clk::now();
        advance_orbits(w, static_cast<double>(k_econ_tick_days));        // app.cpp:385
        advance_surveys(w, k_econ_tick_days);                             // app.cpp:395
        w.current_day_tick = day;                                         // app.cpp:401
        const clk::time_point t0 = clk::now();

        // app.cpp:410-420 -> app::step_economy
        w.current_econ_tick = first_econ_step + (k - 1);                  // app.cpp:1258
        lp_pool_map tick_lp_pools;                                        // app.cpp step_economy
        advance_convoys(w);              // BL-995 order: advance -> arrivals -> economy -> dispatch -> clear
        credit_arrived_convoys(w, day);
        const clk::time_point t1 = clk::now();
        economy_report report = run_economy_step(w, reg, /*spectating=*/false, // app.cpp:1281-1283
                                                 &tick_lp_pools);
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),   // BL-995: before the clear
                         reg.logistics_cost(convoy_mode::space), &tick_lp_pools);
        const clk::time_point t2 = clk::now();
        const auto flows = clear_markets(w, reg, report);                 // app.cpp:1285
        const clk::time_point t3 = clk::now();
        apply_budget(w, reg, flows, report.workforce_contention,          // app.cpp:1287-1290
                     &report.budgets, &report.buildings, &report.building_labour);
        run_nation_step(w, reg, report, w.current_econ_tick);             // app.cpp:1297
        const clk::time_point t4 = clk::now();
        advance_tech_gates(w);                                            // app.cpp:1303
        const clk::time_point t5 = clk::now();
        // Phase 5, the app's lap(5) (app.cpp:1315): standings + convoy credit + exits.
        (void)compute_corp_standings(w, flows);
        run_firm_exits(w, reg.firm_exit(), &report.firm_exits);
        const clk::time_point t6 = clk::now();
        // app.cpp:1317-1354, laps 6-8: NOT mirrored, NOT timed (see above).

        if (timing != nullptr)
        {
            timing->between_ms += ms_between(b0, t0);
            timing->tick_ms.push_back(ms_between(t0, t6));
            timing->phase_ms[0] += ms_between(t0, t1);
            timing->phase_ms[1] += ms_between(t1, t2);
            timing->phase_ms[2] += ms_between(t2, t3);
            timing->phase_ms[3] += ms_between(t3, t4);
            timing->phase_ms[4] += ms_between(t4, t5);
            timing->phase_ms[5] += ms_between(t5, t6);
            // app.cpp:1323,1338 — counsel is handed the DAY tick. A COUNT, not a
            // cost: BL-398 bounds the export and the evaluation to the one
            // open-channel corp; only the sort and the channels walk the set.
            int due = 0;
            for (const auto& kv : w.corporations)
                if (corp_strategic_eval_due(w, kv.first, day))
                    ++due;
            timing->strategic_evals_due.push_back(due);
        }
    }
}

// ---------------------------------------------------------------------------
// SYNTHETIC TEST INPUT — a charter budget for the BL-1032 seam (tools/verify only)
// ---------------------------------------------------------------------------
// NOT A BUDGET SOURCE, AND NOTHING SHIPPED MAY READ IT. The budget has one source
// by design — a centre's unspent industry-point stockpile at 1960 (INDUSTRIALISATION.md
// § 1), built by `build_stockpile_budget` (BL-1042) — and "no stand-in derived
// from urban population fills it" (Ben, 2026-09-17). This builder exists only so the seam can be shown to DO something
// (BL-1032 R4): its weights are SEEDED DRAWS, never population, so a reading taken
// on it proves the plumbing and cannot be mistaken for a density result.
//
// 1x is the number of corporations the LEGACY landscape charters on the same seed
// (specialists + background firms), which the caller measures on a legacy build in
// the same process and passes in. Prices: firm 1 point, specialist 4 firm charters
// (so 4 points) — the digest modes' spend. player_seed_sweep --charter-cost sets
// the specialist's firm charters per row from its --specialist-prices ladder and
// prints the price it ran on every row.

#include "world/planetology.hpp"  // checkpoint_rng

#include <cmath>
#include <limits>
#include <map>

/// Salt for the synthetic weights. Harness-only, and collides with no generation salt.
inline constexpr std::uint32_t k_synthetic_charter_salt = 0x5C0FFA7Bu;

/// BL-1039: the budget path's caps have no shipped default any more (they were
/// restated from Pass 6 inside corporation_generation.cpp until then). These are
/// the values every BL-1032/BL-1033 reading was taken at — Pass 6's own numbers,
/// the per-good cap 8 and the 200-per-body guard — so the legacy synthetic rows
/// reproduce. Harness input, like every number in this section.
inline constexpr std::int32_t k_synthetic_per_resource_firm_cap = 8;
inline constexpr std::int32_t k_synthetic_max_firms_per_body    = 200;

/// The synthetic spend: firm 1 point, specialist 4 firm charters, window 4,
/// province cap on, the per-good cap FIXED at 8 under the 200-per-body guard
/// (BL-1033's "cap kept"), no density ceiling. A specialist's capital is always
/// today's draw (Ben, 2026-09-18); the spend carries no capital parameter.
inline charter_spend_params synthetic_charter_spend()
{
    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 4;
    s.window_radius            = 4;
    s.province_cap             = true;
    s.resource_cap_rule        = charter_cap_rule::fixed;
    s.per_resource_firm_cap    = k_synthetic_per_resource_firm_cap;
    s.max_firms_per_body       = k_synthetic_max_firms_per_body;
    s.density_ceiling          = 0;
    return s;
}

/// SYNTHETIC TEST INPUT. round(@p scale x @p legacy_corporations) points spread
/// over @p w's NON-RAZED population centres by seeded integer weights,
/// apportioned by LARGEST REMAINDER — integer throughout, ties to the lower
/// centre id — so the total is exact.
///
/// THE WEIGHTS ARE A SEEDED RANK LAW, and still pure draws: every centre gets a
/// keyed 64-bit draw, the draws order the centres (ties to the lower id), and the
/// centre at rank r weighs 1e9 / r. MEASURED 2026-09-17 on seed 0: 1x is 87
/// points against a centre count far larger, so a per-centre uniform weight left
/// 87 centres at one point each, and a per-centre 1/k tail left the richest at
/// two — no centre reached the specialist price (4), and the seam's specialist
/// half went unexercised. A rank law gives the richest centre about 1x / ln(N)
/// whatever N is. It is NOT a claim about where capital sits: the order is a
/// shuffle, and no centre's size, scale or population is read.
inline charter_budget synthetic_charter_budget(const world& w, std::uint32_t world_seed,
                                               std::int64_t legacy_corporations, double scale)
{
    std::vector<entity_id> centres;
    for (const auto& [cid, pc] : w.population_centres)
        if (!pc.razed)
            centres.push_back(cid);
    std::sort(centres.begin(), centres.end());
    if (centres.empty())
        return charter_budget{};

    const std::int64_t total = std::llround(static_cast<double>(legacy_corporations) * scale);
    if (total <= 0)
        return charter_budget{};

    // The seeded shuffle: a keyed draw per centre, one splitmix64 step past its key.
    std::vector<std::pair<std::uint64_t, entity_id>> keyed(centres.size());
    for (std::size_t i = 0; i < centres.size(); ++i)
    {
        checkpoint_rng key(world_seed ^ k_synthetic_charter_salt, centres[i]);
        key.unit();
        keyed[i] = { key.s, centres[i] };
    }
    std::sort(keyed.begin(), keyed.end());
    centres.clear();
    for (const auto& kv : keyed)
        centres.push_back(kv.second);   // now in rank order, rank 1 first

    std::vector<std::int64_t> weight(centres.size());
    std::int64_t weight_sum = 0;
    for (std::size_t i = 0; i < centres.size(); ++i)
    {
        weight[i] = 1000000000LL / static_cast<std::int64_t>(i + 1);
        weight_sum += weight[i];
    }

    std::vector<std::int64_t> share(centres.size());
    std::vector<std::int64_t> rem(centres.size());
    std::int64_t assigned = 0;
    for (std::size_t i = 0; i < centres.size(); ++i)
    {
        share[i] = total * weight[i] / weight_sum;
        rem[i]   = total * weight[i] % weight_sum;
        assigned += share[i];
    }
    std::vector<std::size_t> by_rem(centres.size());
    for (std::size_t i = 0; i < by_rem.size(); ++i)
        by_rem[i] = i;
    std::sort(by_rem.begin(), by_rem.end(), [&](std::size_t a, std::size_t b) {
        if (rem[a] != rem[b]) return rem[a] > rem[b];
        return centres[a] < centres[b];
    });
    for (std::int64_t k = 0; k < total - assigned; ++k)
        ++share[by_rem[static_cast<std::size_t>(k)]];

    std::map<entity_id, std::int32_t> points;
    for (std::size_t i = 0; i < centres.size(); ++i)
        points[centres[i]] = static_cast<std::int32_t>(share[i]);
    return charter_budget(points);   // a zero share is dropped here, as any budget's is
}

/// SYNTHETIC TEST INPUT, BL-1033's forced province-cap case under
/// `--forced-pick richest`: @p b's WHOLE total on its richest centre (most
/// points; ties to the lower centre id) and nowhere else. MEASURED 2026-09-17 on
/// seed 28, it does NOT make the province cap bind: the provinces around that
/// centre are smaller than the firms' footprint, so the window's tiles run out
/// first (window_exhausted 80 at radius 1, 62 at radius 4). The default pick
/// (player_seed_sweep.cpp `pick_sparse_province_centre`) reads the ground
/// instead. Not a density reading, for the same reason the source is not.
inline charter_budget concentrated_charter_budget(const charter_budget& b)
{
    entity_id    richest = null_entity;
    std::int32_t most    = 0;
    for (const auto& [centre, pts] : b.points())   // ascending id: a tie keeps the lower
        if (pts > most)
        {
            most    = pts;
            richest = centre;
        }
    if (richest == null_entity)
        return charter_budget{};
    const std::int64_t total = b.total();
    std::map<entity_id, std::int32_t> points;
    points[richest] = static_cast<std::int32_t>(
        std::min<std::int64_t>(total, std::numeric_limits<std::int32_t>::max()));
    return charter_budget(points);
}
