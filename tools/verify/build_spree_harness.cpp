// Throwaway headless harness (playtest patch, 2026-07-06): simulates the player
// hammering "Build" on every eligible tile on their home body, back to back, with
// no in-game time passing (construction is instant — no build-duration model yet).
// Reports how many buildings a single starting balance can buy, to check the
// build costs (100-500 cr + market-priced materials from BL-095-lite) against
// Ben's benchmark: ~20 buildings in "6 months" (2 econ ticks) would be ~1500%
// over the intended build rate for a lean, small chartered-company layer sitting
// on top of the automated population/substrate economy (GENERATION_STRATEGY.md).
// No SDL / Lua / ImGui. Kept outside src/ so the CMake glob does not pull it in.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src tools\verify\build_spree_harness.cpp ^
//      src\world\world.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
//      src\world\market_clearing.cpp src\world\hard_coded_world.cpp ^
//      src\world\tile_generation.cpp src\world\nation_generation.cpp ^
//      src\world\corporation_generation.cpp src\world\population_generation.cpp ^
//      src\world\orbital_system.cpp ^
//      /Fo:build_gen\verify\build_spree_harness\ ^
//      /Fe:build_gen\verify\build_spree_harness.exe
// Run: .\build_gen\verify\build_spree_harness.exe

#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <vector>

static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    // Mirrors scripts/economy.lua as of the 2026-07-06 playtest patch.
    recipe_registry reg;
    {
        building_economics ex;
        ex.build_cost = 100.0f;
        ex.resource_build_cost[ri(resource_type::steel)] = 20.0f;
        reg.set_economics(building_type::extraction_site, ex);

        building_economics pr;
        pr.build_cost = 200.0f;
        pr.resource_build_cost[ri(resource_type::steel)] = 25.0f;
        reg.set_economics(building_type::processing_facility, pr);

        building_economics po;
        po.build_cost = 150.0f;
        po.resource_build_cost[ri(resource_type::steel)] = 20.0f;
        reg.set_economics(building_type::port, po);
    }

    world w = make_hard_coded_world(no_prehistory());
    const entity_id corp = w.player_entity;
    const entity_id home = w.home_body;

    const float starting_balance = w.corporations[corp].balance;
    std::printf("Player starting balance: %.1f cr\n", static_cast<double>(starting_balance));

    // Every home-body tile, in stable entity-id order (deterministic spree order).
    std::vector<entity_id> tiles;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == home)
            tiles.push_back(tid);

    int placed = 0;
    float spent = 0.0f;
    for (const entity_id t : tiles)
    {
        const float before = w.corporations[corp].balance;
        entity_id built = null_entity;

        // Prefer extraction on a tile with an iron deposit; otherwise try a
        // processing facility — mirrors a player clicking "build here" on
        // whatever the tile offers, in one uninterrupted session.
        construction_result r;
        const tile_component& tc = w.tiles[t];
        if (tc.resource_deposit[ri(resource_type::iron_ore)] > 0.0f)
            r = construct_building(w, reg, corp, t, building_type::extraction_site,
                                   resource_type::iron_ore, built);
        else
            r = construct_building(w, reg, corp, t, building_type::processing_facility,
                                   resource_type::iron_ore, built);

        if (r == construction_result::placed)
        {
            ++placed;
            spent += before - w.corporations[corp].balance;
        }
        if (w.corporations[corp].balance < 100.0f) // below the cheapest build cost — spree is over
            break;
    }

    std::printf("Buildings placed in one uninterrupted spree: %d\n", placed);
    std::printf("Credits spent: %.1f  (balance remaining: %.1f)\n",
                static_cast<double>(spent), static_cast<double>(w.corporations[corp].balance));

    const double pct_of_20_in_2_ticks = placed / 20.0 * 100.0;
    std::printf("As %% of the 20-buildings-per-2-econ-ticks benchmark: %.0f%%\n", pct_of_20_in_2_ticks);
    return 0;
}
