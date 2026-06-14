// Throwaway headless harness for the Layer 3 economy logic (no SDL / Lua / ImGui).
// Builds a tiny world + a hand-built recipe registry, runs one economy tick
// (production -> market clearing -> budget), and asserts the documented outcomes.
// Kept outside src/ so the CMake glob does not pull it into the real build.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src econ_harness.cpp ^
//      src\world\world.cpp src\world\economy_system.cpp ^
//      src\world\market_clearing.cpp src\world\budget_system.cpp /Fe:econ_harness.exe

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;

static void check(bool cond, const char* what, float got = 0.0f, float want = 0.0f)
{
    if (cond)
    {
        std::printf("  PASS  %s\n", what);
    }
    else
    {
        std::printf("  FAIL  %s   (got %.3f, want %.3f)\n", what, got, want);
        ++g_failures;
    }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
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
    mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
    mc.base_price[ri(resource_type::steel)]    = 8.0f;
    mc.price = mc.base_price;
    w.markets[market] = mc;

    // --- extraction corp E: iron_ore richness 2.0, workforce 0.5, no hazard ---
    const entity_id tile_e = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.composition = terrain_composition::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)] = 2.0f;
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

    // --- processing corp P: steel recipe, workforce 0.5, seeded pool of iron ---
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
    // Seed P's pool with 4 iron ore -> coverage 4/8 = 0.5 (between t_idle and t_full).
    w.pool_for(corp_p, body).quantities[ri(resource_type::iron_ore)] = 4.0f;

    // --- run one tick ---
    economy_report rep = run_economy_step(w, reg);
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows);

    std::printf("Layer 3 economy harness\n");

    // Extraction: output = 20 * 2.0 * 0.5 * 1.0 = 20 (then all sold -> pool 0).
    // Find E's report.
    float e_out = 0.0f, p_out = 0.0f; bool p_idle = true; bool p_has_lim = false;
    for (const auto& br : rep.buildings)
    {
        if (br.building == bld_e) e_out = br.output_quantity;
        if (br.building == bld_p) { p_out = br.output_quantity; p_idle = br.idle; p_has_lim = br.has_limiting; }
    }
    check(near(e_out, 20.0f), "R3.1 extraction output = base*richness*workforce*(1-hazard)", e_out, 20.0f);

    // Processing: batches_full = 8*0.5 = 4 -> steel produced = 4; iron bought = 8-4 = 4.
    check(near(p_out, 4.0f), "R3.2 processing produces full batch outputs", p_out, 4.0f);
    check(!p_idle && p_has_lim, "R3.2 processor active with a limiting input", p_idle ? 1.0f : 0.0f, 0.0f);
    check(near(rep.purchases[{corp_p, body}][ri(resource_type::iron_ore)], 4.0f),
          "R4.2 auto-bought shortfall = need - pool = 4", rep.purchases[{corp_p, body}][ri(resource_type::iron_ore)], 4.0f);

    // resource_remaining untouched (R3.5)
    check(near(w.tiles[tile_e].resource_remaining[ri(resource_type::iron_ore)], 0.0f),
          "R3.5 deposits do not deplete (resource_remaining untouched)");

    // Market supply/demand (R4.1/R4.2): steel supply 4, iron demand 4.
    const market_component& m = w.markets[market];
    check(near(m.supply[ri(resource_type::steel)], 4.0f), "R4.1 market supply = listed surplus (steel 4)", m.supply[ri(resource_type::steel)], 4.0f);
    check(near(m.demand[ri(resource_type::iron_ore)], 4.0f), "R4.2 market demand = auto-bought (iron 4)", m.demand[ri(resource_type::iron_ore)], 4.0f);
    check(near(m.price[ri(resource_type::steel)], m.base_price[ri(resource_type::steel)]), "R4.3 price stays at base_price");

    // Budget (R5):
    //   E: income 20*2.5=50, maint 5, wage 0.5*8=4 -> +41 -> 1041
    //   P: income 4*8=32, expend 4*2.5=10, maint 10, wage 0.5*12=6 -> +6 -> 1006
    check(near(w.corporations[corp_e].balance, 1041.0f), "R5 extraction corp balance = +41", w.corporations[corp_e].balance, 1041.0f);
    check(near(w.corporations[corp_p].balance, 1006.0f), "R5 processing corp balance = +6", w.corporations[corp_p].balance, 1006.0f);

    // R3.3 idle below t_idle: zero P's workforce-pool scenario -> empty pool, run again.
    {
        world w2;
        recipe_registry r2 = reg;
        const entity_id b2 = w2.create_entity(); w2.bodies[b2] = body_component{};
        const entity_id t2 = w2.create_entity();
        tile_component tc{}; tc.body = b2; w2.tiles[t2] = tc;
        const entity_id pb = w2.create_entity();
        building_component bc{}; bc.tile = t2; bc.type = building_type::processing_facility;
        bc.workforce_assigned = 0.5f; bc.recipe = steel_id; w2.buildings[pb] = bc;
        const entity_id pc = w2.create_entity();
        corporation_component cc; cc.balance = 0.0f; cc.assets.push_back(pb); w2.corporations[pc] = cc;
        // empty pool -> coverage 0 < t_idle -> idle
        economy_report r = run_economy_step(w2, r2);
        bool idle = false;
        for (const auto& br : r.buildings) if (br.building == pb) idle = br.idle;
        check(idle, "R3.3 processor idles below t_idle (empty pool)");
    }

    std::printf("\n%s  (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
