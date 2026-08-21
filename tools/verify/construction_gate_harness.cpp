// Headless harness (BL-095): the market-gated construction rate. A building under
// construction progresses each tick at a rate set by how much of its per-tick
// material need the local market can supply — full when ample, stretched when tight,
// paused when starved — and pays for its materials as it builds (pay-as-you-build).
// Hand-builds a minimal world (no Lua / SDL / ImGui); kept outside src/ so the CMake
// glob ignores it.

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
void check(bool ok, const char* label, double got = 0.0, double want = 0.0)
{
    std::printf("  [%s] %s", ok ? "PASS" : "FAIL", label);
    if (!ok) std::printf("  (got %.4f want %.4f)", got, want);
    std::printf("\n");
    if (!ok) ++g_failures;
}
std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// Build a world with one corp owning one processing building under construction
// (build_duration_ticks = 3, needs 24 steel), anchored to a market on its body.
// Returns the ids via out-params. `steel_supply` seeds the market's real
// inventory (BL-130) — what it can actually supply the build.
struct scene { world w; entity_id bld; entity_id market; entity_id corp; };

scene make_scene(const recipe_registry& reg, float steel_supply)
{
    (void)reg;
    scene s;
    world& w = s.w;

    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};

    const entity_id tile = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
        w.tiles[tile] = tc;
    }

    s.market = w.create_entity();
    {
        market_component mc;
        mc.body        = body;
        mc.centre_tile = tile; // so market_for_tile(tile) routes here
        mc.base_price[ri(resource_type::steel)] = 8.0f;
        mc.price = mc.base_price;
        // BL-130: run_construction now reads real inventory, not "recent
        // supply" — seed both so this fixture's name (`steel_supply`) still
        // means "what the market can actually supply the build".
        mc.supply[ri(resource_type::steel)]    = steel_supply;
        mc.inventory[ri(resource_type::steel)] = steel_supply;
        w.markets[s.market] = mc;
    }

    s.bld = w.create_entity();
    {
        building_component b{};
        b.tile = tile;
        b.type = building_type::processing_facility;
        b.ticks_remaining = 3;      // under construction
        b.construction_progress = 0.0f;
        w.buildings[s.bld] = b;
    }

    s.corp = w.create_entity();
    {
        corporation_component cc;
        cc.name = "Builder Co";
        cc.is_player = true; // exempt from the BL-079 agency so it can't interfere
        cc.starting_capital = 10000.0f;
        cc.balance = 10000.0f;
        cc.assets.push_back(s.bld);
        w.corporations[s.corp] = cc;
    }
    return s;
}

} // namespace

int main()
{
    // Registry: processing build costs 24 steel over 3 ticks (8/tick) + 300 credits;
    // max_stretch 10 (pause below 1/10 of the per-tick need). Hand-built (no Lua).
    recipe_registry reg;
    {
        building_economics pr;
        pr.base_rate = 8.0f; pr.maintenance = 10.0f; pr.base_wage = 12.0f;
        pr.build_cost = 300.0f;
        pr.build_duration_ticks = 3.0f;
        pr.resource_build_cost[ri(resource_type::steel)] = 24.0f; // 8 per tick
        reg.set_economics(building_type::processing_facility, pr);
    }
    construction_params cp; cp.max_stretch = 10.0f; reg.set_construction(cp);

    const float per_tick_need = 24.0f / 3.0f; // = 8

    std::printf("BL-095 construction-gate harness (per-tick steel need = %.1f)\n", per_tick_need);

    // --- Case A: AMPLE supply -> full rate (one whole tick of progress). ---
    {
        scene s = make_scene(reg, /*steel_supply=*/1000.0f);
        const float bal0 = s.w.corporations[s.corp].balance;
        const economy_report rep = run_economy_step(s.w, reg);
        const building_component& b = s.w.buildings[s.bld];
        check(b.ticks_remaining == 2, "A: ample supply advances a whole tick (3 -> 2)",
              b.ticks_remaining, 2);
        check(std::fabs(b.construction_progress) < 1e-4f,
              "A: no fractional remainder at full rate", b.construction_progress, 0.0f);
        const float drawn = rep.purchases.count({s.corp, s.w.markets[s.market].body})
            ? rep.purchases.at({s.corp, s.w.markets[s.market].body})[ri(resource_type::steel)] : 0.0f;
        check(std::fabs(drawn - per_tick_need) < 1e-3f,
              "A: draws one tick of materials as market demand (pay-as-you-build)", drawn, per_tick_need);
        const float spent = bal0 - s.w.corporations[s.corp].balance;
        check(std::fabs(spent - 300.0f / 3.0f) < 1e-3f,
              "A: charges one tick of build_cost incrementally", spent, 100.0f);
    }

    // --- Case B: TIGHT supply (60% of need) -> stretched (partial progress, no tick). ---
    {
        scene s = make_scene(reg, /*steel_supply=*/per_tick_need * 0.6f);
        run_economy_step(s.w, reg);
        const building_component& b = s.w.buildings[s.bld];
        check(b.ticks_remaining == 3, "B: tight supply does NOT complete a tick (stays 3)",
              b.ticks_remaining, 3);
        check(std::fabs(b.construction_progress - 0.6f) < 1e-3f,
              "B: progress advances at the material-limited rate (~0.6)", b.construction_progress, 0.6f);
    }

    // --- Case C: STARVED supply (below 1/max_stretch of need) -> paused. ---
    {
        scene s = make_scene(reg, /*steel_supply=*/per_tick_need * 0.05f); // < 1/10
        const float bal0 = s.w.corporations[s.corp].balance;
        run_economy_step(s.w, reg);
        const building_component& b = s.w.buildings[s.bld];
        check(b.ticks_remaining == 3 && std::fabs(b.construction_progress) < 1e-4f,
              "C: starved build is paused (no progress)", b.construction_progress, 0.0f);
        check(std::fabs(bal0 - s.w.corporations[s.corp].balance) < 1e-4f,
              "C: a paused build spends nothing", 0.0, 0.0);
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
