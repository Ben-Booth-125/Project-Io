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
        a[static_cast<std::size_t>(resource_type::grain)]              = 2.0f;
        a[static_cast<std::size_t>(resource_type::fodder)]             = 1.2f;
        a[static_cast<std::size_t>(resource_type::salt)]               = 2.0f;
        a[static_cast<std::size_t>(resource_type::transport_capacity)] = 5.0f;
        a[static_cast<std::size_t>(resource_type::charcoal)]           = 4.0f;
        a[static_cast<std::size_t>(resource_type::iron_blooms)]        = 6.0f;
        a[static_cast<std::size_t>(resource_type::bullion)]            = 50.0f;
        a[static_cast<std::size_t>(resource_type::trade_goods_misc)]   = 15.0f;
        return a;
    }();

    market_carving_params  market_carving{};
    endemic_pricing_params endemic{};

    /// Non-player corporations generated alongside the player's own, authored
    /// under `world_gen.corporations.count`.
    int corporation_count = 8;

    /// Populate from scripts/world_gen.lua via the embedded Lua state (protected
    /// calls only). Any table/key missing in the script leaves the matching field
    /// at its default above, so a partial script is safe to author against.
    ///
    /// @param lua A loaded lua_state.
    /// @throws std::runtime_error on a Lua error or malformed data.
    void load_from_lua(lua_state& lua);
};
