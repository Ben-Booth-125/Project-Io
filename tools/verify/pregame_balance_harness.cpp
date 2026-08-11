// Headless economy gate (playtest patch 2026-07-06 → BL-078/BL-095/BL-096/BL-079/
// BL-112, 2026-07-07). Two jobs, both against the REAL scripts/economy.lua +
// scripts/recipes.lua and the real generation pipeline (make_hard_coded_world):
//
//  1. WARM-START TRAJECTORY (original): corporations are "new charters"
//     (base_capital = 0), so the opening balance is earned entirely by
//     app::start_new_game's 12 economy ticks. Print the player corp's balance tick
//     by tick to check the trajectory is sane.
//
//  2. ECONOMY-DYNAMISM + FILLABILITY ASSERTIONS (the batch gate): under the BL-078
//     elastic substrate model the market must present *differentiated*, *elastic*
//     demand and a *live margin* (BL-078), that margin must be a *fillable* path to
//     profit (BL-112), and the whole thing must stay *deterministic*. Hard PASS/FAIL
//     assertions; process exits non-zero on any failure.
//
// No SDL / ImGui; links lua_state + recipe_registry (sol2) plus the world/*
// generation + economy TUs. Kept outside src/ so the CMake glob ignores it (it has a
// dedicated Lua-linked target in CMakeLists.txt).
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
//      src\world\survey_system.cpp src\world\building_profit.cpp ^
//      src\world\recipe_registry.cpp src\scripting\lua_state.cpp ^
//      C:\claude\io-deps\src\lua-5.4.7\*.c ^
//      /Fo:build_gen\verify\pregame_balance_harness\ ^
//      /Fe:build_gen\verify\pregame_balance_harness.exe
// Run: .\build_gen\verify\pregame_balance_harness.exe

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

#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok)
        ++g_failures;
}

// The tradeable prototype subset (the resources markets actually price).
constexpr resource_type tradeables[] = {
    resource_type::iron_ore, resource_type::petroleum, resource_type::water,
    resource_type::agricultural_produce, resource_type::steel,
    resource_type::refined_fuel, resource_type::food_rations };

// Run the full per-tick economy step, matching app::step_economy.
void tick(world& w, const recipe_registry& reg, int t)
{
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space));
    advance_convoys(w);
    const economy_report report = run_economy_step(w, reg);
    const auto flows = clear_markets(w, reg, report);
    apply_budget(w, reg, flows, report.workforce_contention, nullptr);
    credit_arrived_convoys(w, t);
}

void seed_default_recipes(world& w, const recipe_registry& reg)
{
    const entity_id default_recipe = reg.recipe_id("steel");
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = static_cast<uint16_t>(default_recipe);
}

// Aggregate supply/demand/price across every market on a body (post-clear state).
struct market_agg
{
    std::array<float, resource_count> supply = {};
    std::array<float, resource_count> demand = {};
    std::array<float, resource_count> price  = {};
    std::array<float, resource_count> base   = {};
    int markets = 0;
};

market_agg aggregate_body(const world& w, entity_id body)
{
    market_agg a;
    for (const auto& [mid, mc] : w.markets)
    {
        if (mc.body != body)
            continue;
        ++a.markets;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            a.supply[r] += mc.supply[r];
            a.demand[r] += mc.demand[r];
            a.price[r]   = mc.price[r];       // representative (markets share base)
            a.base[r]    = mc.base_price[r];
        }
    }
    return a;
}

} // namespace

/// Warm-start length in quarterly economy ticks. Defaults to the shipped
/// `app::start_new_game` figure so the gate's assertions keep measuring what the
/// app actually does; override from argv to measure a proposed length before
/// changing the app (Ben, 2026-08-10: "20 years of economy ticks" = 80).
/// A tick is one quarter (k_ticks_per_year = 4, budget_system.hpp), so
/// years = ticks / 4.
constexpr int default_warm_start_ticks = 12;

int main(int argc, char** argv)
{
    int warm_start_ticks = default_warm_start_ticks;
    if (argc > 1)
    {
        const int n = std::atoi(argv[1]);
        if (n <= 0)
        {
            std::printf("usage: %s [warm_start_ticks]   (positive integer)\n", argv[0]);
            return 2;
        }
        warm_start_ticks = n;
    }
    std::printf("Warm start: %d ticks (%.2f in-game years)\n\n",
                warm_start_ticks, warm_start_ticks / 4.0);

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    world w = make_hard_coded_world();
    seed_default_recipes(w, reg);

    const entity_id corp = w.player_entity;
    const entity_id home = w.home_body;

    // --- 1. Warm-start trajectory -------------------------------------------
    std::printf("New-charter pre-game warm start (base_capital = 0)\n");
    std::printf("Tick  0: balance = %.1f cr (opening — generation only)\n",
                static_cast<double>(w.corporations[corp].balance));
    bool went_negative = false;
    for (int t = 1; t <= warm_start_ticks; ++t)
    {
        tick(w, reg, t);
        const float bal = w.corporations[corp].balance;
        if (bal < 0.0f)
            went_negative = true;
        std::printf("Tick %2d: balance = %.1f cr\n", t, static_cast<double>(bal));
    }
    const float final_balance = w.corporations[corp].balance;
    std::printf("\nFinal (turn-one) balance: %.1f cr | went negative: %s\n\n",
                static_cast<double>(final_balance), went_negative ? "YES" : "no");

    // --- 2. Economy-dynamism + fillability assertions -----------------------
    const market_agg agg = aggregate_body(w, home);
    std::printf("Home-body markets: %d\n", agg.markets);

    // BL-078 R1 — DIFFERENTIATED demand: the per-capita basket makes demand vary
    // across resources (the old flat model gave every resource identical demand).
    {
        float dmax = 0.0f, dmin = 1e30f;
        for (const resource_type rt : tradeables)
        {
            const float d = agg.demand[static_cast<std::size_t>(rt)];
            if (d <= 0.0f) continue;
            dmax = (d > dmax) ? d : dmax;
            dmin = (d < dmin) ? d : dmin;
        }
        check(dmax > dmin * 1.2f,
              "BL-078 R1: home-market demand is differentiated across resources (basket, not flat)");
    }

    // BL-078 R2 / BL-112 R1 — a LIVE MARGIN (fillable gap): at least one tradeable
    // has demand exceeding supply and a price bid above base — the opportunity the
    // player fills.
    {
        int gaps = 0;
        for (const resource_type rt : tradeables)
        {
            const std::size_t r = static_cast<std::size_t>(rt);
            if (agg.demand[r] > agg.supply[r] * 1.05f && agg.price[r] > agg.base[r] * 1.10f)
                ++gaps;
        }
        check(gaps >= 1,
              "BL-078 R2 / BL-112 R1: a live unmet-demand margin exists (>=1 resource bid above base)");
    }

    // BL-112 R1 — the gap is LUCRATIVE (a real path to profit): the best price/base
    // ratio on the home market is meaningfully above 1 (producing into it pays).
    {
        float best_ratio = 0.0f;
        resource_type best = resource_type::iron_ore;
        for (const resource_type rt : tradeables)
        {
            const std::size_t r = static_cast<std::size_t>(rt);
            if (agg.base[r] <= 0.0f) continue;
            const float ratio = agg.price[r] / agg.base[r];
            if (ratio > best_ratio) { best_ratio = ratio; best = rt; }
        }
        std::printf("  (best price/base ratio %.2f on resource %d)\n",
                    static_cast<double>(best_ratio), static_cast<int>(best));
        check(best_ratio >= 1.3f,
              "BL-112 R1: the fillable gap is lucrative (best price >= 1.3x base)");
    }

    // BL-078 R1 — ELASTIC demand: on a home market, lowering a resource's price
    // raises the substrate demand injected for it. Manipulate one market's price and
    // re-inject, comparing demand at base price vs half price for the same resource.
    {
        entity_id mid = null_entity;
        for (const auto& [id, mc] : w.markets)
            if (mc.body == home) { mid = id; break; }
        bool elastic_ok = false;
        if (mid != null_entity)
        {
            const std::size_t r = static_cast<std::size_t>(resource_type::food_rations);
            market_component& mc = w.markets.at(mid);
            const float base = mc.base_price[r];

            mc.supply.fill(0.0f); mc.demand.fill(0.0f);
            mc.price[r] = base;                 // dear-ish (== base)
            inject_substrate_demand(w, reg);
            const float demand_at_base = mc.demand[r];

            mc.supply.fill(0.0f); mc.demand.fill(0.0f);
            mc.price[r] = base * 0.5f;           // cheaper
            inject_substrate_demand(w, reg);
            const float demand_cheaper = mc.demand[r];

            std::printf("  (food_rations demand at base=%.2f, at 0.5x base=%.2f)\n",
                        static_cast<double>(demand_at_base), static_cast<double>(demand_cheaper));
            elastic_ok = demand_cheaper > demand_at_base * 1.05f;
        }
        check(elastic_ok, "BL-078 R1: demand is price-elastic (cheaper price -> more demanded)");
    }

    // Determinism — same seed -> identical home-market supply/demand after an equal
    // warm start (no RNG entered the tick path).
    {
        world w2 = make_hard_coded_world();
        seed_default_recipes(w2, reg);
        for (int t = 1; t <= warm_start_ticks; ++t)
            tick(w2, reg, t);
        const market_agg a2 = aggregate_body(w2, home);
        bool identical = (a2.markets == agg.markets);
        for (const resource_type rt : tradeables)
        {
            const std::size_t r = static_cast<std::size_t>(rt);
            if (a2.supply[r] != agg.supply[r] || a2.demand[r] != agg.demand[r])
                identical = false;
        }
        check(identical, "Determinism: same seed reproduces identical home-market supply/demand");
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
