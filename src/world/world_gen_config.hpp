#pragma once

#include "components.hpp"

#include <array>
#include <cstdint>

// Forward-declared so this header carries no sol2/Lua dependency — hard_coded_world.cpp
// (world/*, SDL- and Lua-free) includes it and stays headlessly buildable. Only
// world_gen_config.cpp pulls in the Lua state to populate it. Mirrors the
// recipe_registry.hpp split (see its header comment).
class lua_state;

/// Kepler market-carving tunables, authored in scripts/world_gen.lua under
/// `world_gen.kepler_market.carving`. Classifies a nation's tradeable-resource
/// concentration against the cross-nation mean to fracture or fold its market
/// count (BL-096). See hard_coded_world.cpp's market-seeding block.
struct market_carving_params
{
    float rich_factor   = 1.30f; ///< concentration >= mean * this -> fracture (more markets).
    float barren_factor = 0.70f; ///< concentration <  mean * this -> fold (fewer markets).
    /// BL-132 change (3): each distinct corporation holding an asset in a
    /// nation's territory multiplies its concentration by (1 + this), nudging
    /// a commercially-contested territory toward fracture on top of raw
    /// geology. 0 = corp presence has no effect (the pre-BL-132 formula).
    float corp_presence_gain = 0.15f;
};

/// Endemic-good distance pricing tunables (BL-191), authored in scripts/world_gen.lua
/// under `world_gen.kepler_market.endemic`. A good is cheap where it grows and dearer
/// with distance from its source tiles.
struct endemic_pricing_params
{
    float source_price  = 1.5f; ///< price at the source tile.
    float distance_gain = 7.0f; ///< multiplier applied across the globe (half-diagonal = 1.0 norm).
};

/// World-generation balance values, authored in scripts/world_gen.lua. Pure data
/// once built; constructed either from Lua (load_from_lua) in the real build or
/// left at these defaults in a headless test harness — the defaults reproduce
/// the pre-BL-236 hard-coded generation exactly.
struct world_gen_config
{
    /// STOP GENERATION ONCE THE ANCIENT ERA HAS RUN, leaving the world
    /// half-built and the REPORT complete (Ben, 2026-09-09).
    ///
    /// WHAT IT IS FOR. The wizard's history round wants one thing from
    /// generation — the recorded Era -1 time-lapse — and the only entry point
    /// that produces it is `make_hard_coded_world`. Calling that ran all
    /// thirteen stages: the era is stage 8, and stages 9-12 (borders, roads,
    /// companies, finishing) were computed and thrown away. Measured, that is
    /// 10,805 ms of 11,316 — about 95% of the round's wait spent on passes it
    /// discards, and it is why the round visibly hung on "Laying roads".
    ///
    /// WHY A KNOB RATHER THAN A SECOND ENTRY POINT. A second function that
    /// "just runs the early passes" is a second construction of the
    /// invocation, which is precisely the drift `era_minus_one.hpp` exists to
    /// stop — six divergent axes, found the hard way by BL-462, and a seventh
    /// caller found again by NR-733. There is still exactly ONE path through
    /// generation; this only says where to stop walking it.
    ///
    /// THE WORLD IS NOT USABLE WHEN THIS IS SET, and that is the whole contract.
    /// No nations, no roads, no corporations, no markets. A caller that sets it
    /// wants `generation_report` and must discard the `world`. Nothing in the
    /// campaign path may ever set it.
    ///
    /// Default false: every existing caller builds a whole world, exactly as
    /// before. Authored nowhere in Lua — this is a call-site scope knob, not a
    /// balance value, and it is the one field here that is not.
    bool stop_after_ancient_era = false;

    /// STOP GENERATION ONCE THE EXPLORATION SPAN HAS RUN, before borders,
    /// roads and companies are built (BL-946) -- the Exploration round's own
    /// sibling to `stop_after_ancient_era` above, same contract, one round
    /// later. Also gates the Exploration span ITSELF off whenever
    /// `stop_after_ancient_era` is set, so the Empires round's own launch
    /// (which stops right after the ancient era) never pays for a span it
    /// will discard.
    ///
    /// THE WORLD IS NOT USABLE WHEN THIS IS SET, for the same reason
    /// `stop_after_ancient_era` is not.
    ///
    /// Default false: every existing caller is unaffected.
    bool stop_after_exploration = false;

    /// STOP GENERATION ONCE THE MIGRATION HAS RUN, before the Empires round's
    /// history sim ever starts (BL-871).
    ///
    /// WHAT IT IS FOR. The wizard's round 3 (Culture) wants the migration's own
    /// record — colonisation's founding walk, 2400 BCE to wherever it
    /// terminates — and NOT the Empires round's conquest history that used to
    /// be fused into the same run. Mutually exclusive with
    /// `stop_after_ancient_era` in practice: a caller wants one stop point or
    /// the other, never both, though nothing here enforces that (the earlier
    /// check wins if both are set).
    ///
    /// THE WORLD IS NOT USABLE WHEN THIS IS SET, for the same reason
    /// `stop_after_ancient_era` is not: `run_history_sim` has not run at all,
    /// so no region founded after `sim_start_year` exists yet, there are no
    /// polities, and nothing downstream may be built from it.
    ///
    /// Default false: every existing caller is unaffected.
    bool stop_after_migration = false;

    /// Deposit-density multiplier per abundance_level (sparse/lean/standard),
    /// authored under `world_gen.deposit_scalar`. Indexed by abundance_level.
    std::array<float, 3> deposit_scalar = { 0.40f, 0.65f, 1.00f };

    /// Kepler's starting market base prices, authored under
    /// `world_gen.kepler_market.base_price`, indexed by resource_type.
    std::array<float, resource_count> kepler_base_price = [] {
        std::array<float, resource_count> a{};
        a[static_cast<std::size_t>(resource_type::iron_ore)]             = 2.5f;
        a[static_cast<std::size_t>(resource_type::petroleum)]            = 3.5f;
        a[static_cast<std::size_t>(resource_type::water)]                = 1.5f;
        a[static_cast<std::size_t>(resource_type::agricultural_produce)] = 3.0f;
        a[static_cast<std::size_t>(resource_type::steel)]                = 8.0f;
        a[static_cast<std::size_t>(resource_type::refined_fuel)]         = 10.0f;
        a[static_cast<std::size_t>(resource_type::food_rations)]         = 6.0f;
        // --- Logistics goods (BL-286, 2026-08-04) — mid-tier authored placeholders;
        // no consumption/gate mechanic reads these yet (BL-287-290). ---
        a[static_cast<std::size_t>(resource_type::charcoal)]           = 4.0f;
        a[static_cast<std::size_t>(resource_type::iron_blooms)]        = 6.0f;
        a[static_cast<std::size_t>(resource_type::trade_goods_misc)]   = 15.0f;
        return a;
    }();

    // ONE base-price table for both era bands (Ben, 2026-09-02, overturning
    // NR-778). A per-band override was tried when the ancient bloom chain priced
    // its own steel at 113; the ruling was to shorten that chain to depth 1
    // instead (recipes.lua's Bloomery) so one table clears the recipe margin
    // anchor in both bands. A good's price is the larger of the two bands'
    // anchor-route needs; recipe_margin checks both against this one table.

    market_carving_params  market_carving{};
    endemic_pricing_params endemic{};

    /// Non-player corporations generated alongside the player's own, authored
    /// under `world_gen.corporations.count`.
    int corporation_count = 8;

    /// Whether generation hands every specialist corporation an opening military
    /// base and a 50-head unit (`seed_starting_military`, BL-324). **False from
    /// 2026-08-26 (Ben): a new charter does not open with a standing army.**
    /// Forwarded to `corporation_params::seed_starting_force`, which carries the
    /// full reasoning. Kept as a knob rather than deleted so the seeding stays
    /// exercisable - `rival_military_seeding_harness` sets it true.
    bool seed_starting_force = false;

    /// Populate from scripts/world_gen.lua via the embedded Lua state (protected
    /// calls only). Any table/key missing in the script leaves the matching field
    /// at its default above, so a partial script is safe to author against.
    ///
    /// @param lua A loaded lua_state.
    /// @throws std::runtime_error on a Lua error or malformed data.
    void load_from_lua(lua_state& lua);

    /// True while this object is the C++ FALLBACK rather than the parsed
    /// `scripts/world_gen.lua`. `load_from_lua` clears it.
    ///
    /// It exists because the difference is invisible and expensive (2026-08-26):
    /// the fallback prices **10 of 47** resources where the script authors **42**,
    /// markets are seeded from that table, and a harness that omits the argument
    /// silently measures a world in which a third of the ancient roster cannot be
    /// sold at any price. That defect invalidated a sprint of numbers and was
    /// found in THREE separate harnesses in one day. A caller that legitimately
    /// wants the fallback (the Lua-free logic harnesses, where prices are not the
    /// subject) is unaffected — this flag only lets a caller SAY which world it
    /// measured instead of leaving the reader to guess.
    bool is_fallback = true;
};
