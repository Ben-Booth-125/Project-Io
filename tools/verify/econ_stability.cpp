// Headless multi-tick stability harness for the Layer 3 economy loop (no SDL /
// Lua / ImGui). Builds a small fixed world (extraction + processing + market)
// and runs run_economy_step -> clear_markets -> apply_budget over many ticks,
// asserting the loop stays sane: prices stay inside the [0.25x, 4x] clamp band,
// nothing goes NaN/Inf, deposit reserves decrease monotonically toward
// exhaustion, and balances do not diverge unboundedly. Cheap insurance before
// Layer 4 piles UI on the loop (the single-tick case lives in econ_harness).
//
// Build (from repo root, after sourcing vcvars64): see tools/verify/README.md.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond)
        ++g_failures;
}

static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

static bool finite_ok(float v) { return std::isfinite(v); }

int main()
{
    constexpr int   k_ticks      = 100;
    constexpr float k_iron_base  = 2.5f;
    constexpr float k_steel_base = 8.0f;

    world w;

    // --- registry (hand-built; mirrors scripts/economy.lua + recipes.lua) ---
    recipe_registry reg;
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    {
        building_economics ex; ex.base_rate = 20.0f; ex.maintenance = 5.0f;  ex.base_wage = 8.0f;
        reg.set_economics(building_type::extraction_site, ex);
        building_economics pr; pr.base_rate = 8.0f;  pr.maintenance = 10.0f; pr.base_wage = 12.0f;
        reg.set_economics(building_type::processing_facility, pr);
    }
    recipe steel;
    steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
    steel.outputs[ri(resource_type::steel)]   = 1.0f;
    const uint16_t steel_id = reg.add_recipe(steel);

    // --- a body + market ---
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    w.bodies[body].name = "TestWorld";

    const entity_id market = w.create_entity();
    market_component mc;
    mc.body = body;
    mc.base_price[ri(resource_type::iron_ore)] = k_iron_base;
    mc.base_price[ri(resource_type::steel)]    = k_steel_base;
    mc.price = mc.base_price;
    w.markets[market] = mc;

    // --- extraction corp E: iron, finite reserve so it depletes within the run ---
    const entity_id tile_e = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.composition = terrain_composition::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = 2.0f;
        // ~20/tick at full rate => exhausts partway through 100 ticks, exercising
        // the taper + exhaustion path under the monotonic-reserve assertion.
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1200.0f;
        tc.hazard_level = 0.0f;
        w.tiles[tile_e] = tc;
    }
    const entity_id bld_e = w.create_entity();
    {
        building_component b{};
        b.tile = tile_e;
        b.type = building_type::extraction_site;
        b.workforce_assigned = 0.5f;
        b.target_resource = resource_type::iron_ore;
        w.buildings[bld_e] = b;
    }
    const entity_id corp_e = w.create_entity();
    {
        corporation_component cc;
        cc.name = "Extractor Co";
        cc.starting_capital = 1000.0f;
        cc.balance = 1000.0f;
        cc.assets.push_back(bld_e);
        w.corporations[corp_e] = cc;
    }

    // --- processing corp P: steel recipe ---
    const entity_id tile_p = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.composition = terrain_composition::grassland;
        w.tiles[tile_p] = tc;
    }
    const entity_id bld_p = w.create_entity();
    {
        building_component b{};
        b.tile = tile_p;
        b.type = building_type::processing_facility;
        b.workforce_assigned = 0.5f;
        b.recipe = steel_id;
        w.buildings[bld_p] = b;
    }
    const entity_id corp_p = w.create_entity();
    {
        corporation_component cc;
        cc.name = "Smelter Co";
        cc.starting_capital = 1000.0f;
        cc.balance = 1000.0f;
        cc.assets.push_back(bld_p);
        w.corporations[corp_p] = cc;
    }

    std::printf("Layer 3 economy multi-tick stability harness (%d ticks)\n", k_ticks);

    // Clamp band per docs/economy price resolution: [0.25x, 4x] base_price.
    const float iron_lo  = 0.25f * k_iron_base,  iron_hi  = 4.0f * k_iron_base;
    const float steel_lo = 0.25f * k_steel_base, steel_hi = 4.0f * k_steel_base;

    bool  price_in_band  = true;
    bool  all_finite     = true;
    bool  reserve_mono   = true;
    bool  balance_bound  = true;
    float prev_reserve   = w.tiles[tile_e].resource_remaining[ri(resource_type::iron_ore)];
    constexpr float k_balance_bound = 1.0e9f; // generous; a divergent loop blows past this

    for (int t = 0; t < k_ticks; ++t)
    {
        economy_report rep = run_economy_step(w, reg);
        auto flows = clear_markets(w, reg, rep);
        apply_budget(w, reg, flows);

        const market_component& m = w.markets[market];
        const float pi = m.price[ri(resource_type::iron_ore)];
        const float ps = m.price[ri(resource_type::steel)];

        if (!finite_ok(pi) || !finite_ok(ps)) all_finite = false;
        // Allow a tiny epsilon for the clamp boundary.
        if (pi < iron_lo - 1e-3f || pi > iron_hi + 1e-3f)  price_in_band = false;
        if (ps < steel_lo - 1e-3f || ps > steel_hi + 1e-3f) price_in_band = false;

        const float reserve = w.tiles[tile_e].resource_remaining[ri(resource_type::iron_ore)];
        if (!finite_ok(reserve)) all_finite = false;
        if (reserve > prev_reserve + 1e-3f) reserve_mono = false; // never refills
        prev_reserve = reserve;

        for (const auto& [cid, corp] : w.corporations)
        {
            if (!finite_ok(corp.balance)) all_finite = false;
            if (std::fabs(corp.balance) > k_balance_bound) balance_bound = false;
        }
        // Pool quantities finite.
        for (const auto& [key, pool] : w.corp_body_pools)
        {
            for (std::size_t r = 0; r < resource_count; ++r)
                if (!finite_ok(pool.quantities[r])) all_finite = false;
        }
    }

    const float final_reserve = w.tiles[tile_e].resource_remaining[ri(resource_type::iron_ore)];
    std::printf("  final iron reserve = %.2f (seeded 1200)\n", final_reserve);
    std::printf("  final balances: E=%.2f  P=%.2f\n",
                w.corporations[corp_e].balance, w.corporations[corp_p].balance);

    check(price_in_band, "R2 all market prices stay within [0.25x, 4x] base_price over the run");
    check(all_finite,    "R3 no NaN/Inf in any price, pool quantity, or balance");
    check(reserve_mono,  "R4 deposit reserve decreases monotonically (never refills)");
    check(final_reserve < 600.0f,
          "R4 reserve depletes substantially toward exhaustion over the run");
    check(balance_bound, "R4 balances stay bounded (no unbounded divergence)");
    check(g_failures == 0, "R1 the loop ran 100 ticks without tripping a stability assertion");

    std::printf("\n%s  (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
