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
