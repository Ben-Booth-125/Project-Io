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
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <vector>

/// BL-1032 — a charter budget handed to the search, for an instrument that
/// wants one. THE SHIPPED PATH PASSES NONE: app.cpp's new-game prelude has no
/// budget source (the Digitisation stockpile does not exist yet), so its search
/// params carry `landscape_search_params::budget`'s null default, and every
/// helper below defaults this to the same none. A null `budget` is the app's
/// call, verbatim; a non-null one (even an empty one) reaches the seam.
struct harness_charter_input
{
    const charter_budget* budget = nullptr;
    charter_spend_params  spend{};
    /// Receives the WINNER'S spend report (the search's own evaluations report
    /// nothing). Untouched when the budget is null or empty.
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
    // BL-1032: app.cpp sets neither field (its budget is none); only an
    // instrument that asked for a budget writes them.
    if (charter.budget != nullptr)
    {
        sp.budget = charter.budget;
        sp.spend  = charter.spend;
    }
    if (search)
    {
        out.search = search_landscape(w, reg, sp);
    }
    else
    {
        out.search.seed_candidate = sp.start;
        out.search.winner         = sp.start;
    }
    if (charter.budget == nullptr)
    {
        // app.cpp:1071, verbatim — the shipped path.
        apply_landscape_candidate(w, reg, out.search.winner, /*regenerate_specialists=*/true);
    }
    else
    {
        // BL-1032's overload. An EMPTY budget forwards to the call above inside
        // world/*, which is what `--charter-budget empty|zero` exists to prove.
        apply_landscape_candidate(w, reg, out.search.winner, /*regenerate_specialists=*/true,
                                  charter.budget, charter.spend, charter.report);
    }

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
// presentation calls step_economy makes (standings cache, comms, counsel,
// history recorders, strategy readout) take `const world&` and are not
// mirrored — they cannot move a byte of the world.
//
// Written for player_seed_sweep; it exists so every instrument measuring the
// shipped start can stop restating the sequence. Other harnesses are NOT
// migrated by the item that added it, and their numbers are unchanged.

#include "world/budget_system.hpp"     // apply_budget
#include "world/corp_command.hpp"      // run_firm_exits
#include "world/economy_system.hpp"    // run_economy_step, economy_report
#include "world/history_log.hpp"       // seed_genesis_history
#include "world/market_clearing.hpp"   // clear_markets
#include "world/nation_step.hpp"       // run_nation_step
#include "world/recipe_registry.hpp"   // recipe_registry, era_band_for_epoch
#include "world/supply_system.hpp"     // dispatch/advance/credit convoys
#include "world/survey_system.hpp"     // init_survey_states
#include "world/tech_gate.hpp"         // advance_tech_gates
#include "world/works_roster.hpp"      // works_registry

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

/// Build @p out as the app builds a campaign start from @p params, stopping
/// where the app arms the validation run (app.cpp:653-655). @p lua plays
/// app::m_lua: one long-lived state the scripts are (re)loaded into.
inline void build_app_start_world(lua_state& lua, const world_params& params,
                                  app_start_world& out,
                                  const harness_charter_input& charter = {})
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

    // app.cpp:1052-1081 — the search, the winner applied — and app.cpp:1092, the
    // second recipe pass: apply_shipped_landscape is that block, given the
    // PARSED config's roster count (app.cpp:1057).
    // BL-1032: the app passes NO charter budget here (its default is none), and
    // neither does this mirror unless an instrument hands one in.
    out.land = apply_shipped_landscape(out.w, out.reg, params.seed, /*search=*/true,
                                       out.cfg.corporation_count, charter);
}

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
inline void run_app_validation_settle(world& w, const recipe_registry& reg,
                                      int ticks = k_app_validation_ticks)
{
    constexpr int k_validation_day_tick = 0; // m_sim_loop.day_tick(), see above
    for (int econ_step = 0; econ_step < ticks; ++econ_step)  // app.cpp:655, 579-583
    {
        w.current_econ_tick = econ_step;                                  // app.cpp:1258
        lp_pool_map tick_lp_pools;                                        // app.cpp:1265
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),   // app.cpp:1266-1268
                         reg.logistics_cost(convoy_mode::space), &tick_lp_pools);
        advance_convoys(w);                                               // app.cpp:1269
        // app.cpp:1281-1283: `m_ui.spectating || m_validation_run`, and
        // m_validation_run is true for every tick of this run (app.cpp:653).
        economy_report report = run_economy_step(w, reg, /*spectating=*/true,
                                                 &tick_lp_pools);
        const auto flows = clear_markets(w, reg, report);                 // app.cpp:1285
        apply_budget(w, reg, flows, report.workforce_contention,          // app.cpp:1287-1290
                     &report.budgets, &report.buildings, &report.building_labour);
        run_nation_step(w, reg, report, w.current_econ_tick);             // app.cpp:1297
        advance_tech_gates(w);                                            // app.cpp:1303
        // app.cpp:1308 compute_corp_standings reads a const world into a UI cache.
        credit_arrived_convoys(w, k_validation_day_tick);                 // app.cpp:1309
        run_firm_exits(w, reg.firm_exit(), &report.firm_exits);           // app.cpp:1314
        // app.cpp:1322-1353: presentation over a const world; nothing to mirror.
    }
}

// ---------------------------------------------------------------------------
// SYNTHETIC TEST INPUT — a charter budget for the BL-1032 seam (tools/verify only)
// ---------------------------------------------------------------------------
// NOT A BUDGET SOURCE, AND NOTHING SHIPPED MAY READ IT. The budget has one source
// by design — a centre's unspent industry-point stockpile at 1960 (DIGITISATION.md
// § 1) — and "no stand-in derived from urban population fills it" (Ben,
// 2026-09-17). This builder exists only so the seam can be shown to DO something
// (BL-1032 R4): its weights are SEEDED DRAWS, never population, so a reading taken
// on it proves the plumbing and cannot be mistaken for a density result.
//
// 1x is the number of corporations the LEGACY landscape charters on the same seed
// (specialists + background firms), which the caller measures on a legacy build in
// the same process and passes in. Prices: firm 1 point, specialist 4 firm charters
// (so 4 points).

#include "world/planetology.hpp"  // checkpoint_rng

#include <cmath>
#include <map>

/// Salt for the synthetic weights. Harness-only, and collides with no generation salt.
inline constexpr std::uint32_t k_synthetic_charter_salt = 0x5C0FFA7Bu;

/// The synthetic spend: firm 1 point, specialist 4 firm charters, window 4,
/// province cap on.
inline charter_spend_params synthetic_charter_spend()
{
    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 4;
    s.window_radius            = 4;
    s.province_cap             = true;
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
