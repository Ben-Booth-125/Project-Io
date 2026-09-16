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
/// corporation count is `world_gen_config::corporation_count`, which every
/// shipped config leaves at its default — pass the parsed value where an
/// instrument has one.
inline landscape_search_params shipped_search_params(std::uint32_t world_seed,
                                                     int corporation_count = 8)
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
inline shipped_landscape apply_shipped_landscape(world& w, const recipe_registry& reg,
                                                 std::uint32_t world_seed,
                                                 bool search = true)
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

    const landscape_search_params sp = shipped_search_params(world_seed);
    if (search)
    {
        out.search = search_landscape(w, reg, sp);
    }
    else
    {
        out.search.seed_candidate = sp.start;
        out.search.winner         = sp.start;
    }
    apply_landscape_candidate(w, reg, out.search.winner, /*regenerate_specialists=*/true);

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
