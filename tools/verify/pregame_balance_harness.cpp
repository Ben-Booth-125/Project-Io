// Throwaway headless harness (playtest patch, 2026-07-06): with corporations
// now seeded as "new charters" (base_capital = 0, corporation_generation.hpp),
// a corp's entire opening balance comes from the pre-game warm start —
// app::start_new_game's 12 economy ticks (~3 in-game years) run against the
// generation-time asset placement before turn one. This harness reproduces
// that exact sequence headlessly (loading the real scripts/economy.lua +
// scripts/recipes.lua, same as the game) and prints the player corp's balance
// tick by tick, to check the trajectory is sane — doesn't collapse to a
// crippling negative, doesn't need the safety net of a seeded lump sum.
// No SDL / ImGui; links lua_state + recipe_registry (sol2) plus the world/*
// generation + economy TUs. Kept outside src/ so the CMake glob ignores it.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /O2 /I src ^
//      /I C:\claude\io-deps\src\sol2-3.5.0\include ^
//      /I C:\claude\io-deps\src\lua-5.4.7 ^
//      tools\verify\pregame_balance_harness.cpp ^
//      src\world\world.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
//      src\world\market_clearing.cpp src\world\hard_coded_world.cpp ^
//      src\world\tile_generation.cpp src\world\nation_generation.cpp ^
//      src\world\corporation_generation.cpp src\world\population_generation.cpp ^
//      src\world\orbital_system.cpp src\world\economy_system.cpp ^
//      src\world\budget_system.cpp src\world\supply_system.cpp ^
//      src\world\survey_system.cpp src\world\recipe_registry.cpp ^
//      src\scripting\lua_state.cpp ^
//      C:\claude\io-deps\src\lua-5.4.7\*.c ^
//      /Fe:build\pregame_balance_harness.exe

#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <cstdio>

int main()
{
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    world w = make_hard_coded_world();
    const entity_id default_recipe = reg.recipe_id("steel");
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = static_cast<uint16_t>(default_recipe);

    const entity_id corp = w.player_entity;
    std::printf("New-charter pre-game warm start (base_capital = 0)\n");
    std::printf("Tick  0: balance = %.1f cr (opening — generation only, no ticks run)\n",
                static_cast<double>(w.corporations[corp].balance));

    bool went_negative = false;
    for (int t = 1; t <= 12; ++t)
    {
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));
        advance_convoys(w);
        const economy_report report = run_economy_step(w, reg);
        const auto flows = clear_markets(w, reg, report, {});
        apply_budget(w, reg, flows, report.workforce_contention, nullptr);
        credit_arrived_convoys(w, t);

        const float bal = w.corporations[corp].balance;
        if (bal < 0.0f)
            went_negative = true;
        std::printf("Tick %2d: balance = %.1f cr\n", t, static_cast<double>(bal));
    }

    const float final_balance = w.corporations[corp].balance;
    std::printf("\nFinal (turn-one) balance: %.1f cr\n", static_cast<double>(final_balance));
    std::printf("Went negative at any point: %s\n", went_negative ? "YES" : "no");
    std::printf("Playable (final balance covers cheapest build, 100cr + materials): %s\n",
                final_balance >= 150.0f ? "YES" : "NO — corp opens unable to build anything");
    return 0;
}
