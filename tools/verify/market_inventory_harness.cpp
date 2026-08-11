// Headless harness for BL-130 (real persistent per-resource market inventory):
// markets hold real stock rather than a derived-from-recent-supply figure.
// Fills from real corp sales only (not the abstract substrate); drains during
// production (run_processing) and construction (run_construction), both of
// which run before clear_markets in the same tick, against whatever stock
// survived from PRIOR ticks. A processor or a build can only draw what the
// market genuinely has on hand — no more unconditional auto-buy. Kept outside
// src/ so the CMake glob does not pull it into the real build.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else      { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    recipe_registry reg;
    reg.set_thresholds(1.0f, 0.2f);
    { building_economics pr; pr.base_rate = 20.0f; reg.set_economics(building_type::processing_facility, pr); }
    recipe steel; steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
    steel.outputs[ri(resource_type::steel)]   = 1.0f;
    const uint16_t steel_id = reg.add_recipe(steel);

    std::printf("Real market inventory (BL-130) harness\n");

    // --- R1: a processor with an EMPTY pool and EMPTY market inventory idles
    // (no unconditional auto-buy — the old "market body always full batch" case). ---
    {
        world w;
        const entity_id body = w.create_entity(); w.bodies[body] = body_component{};
        const entity_id tile = w.create_entity();
        { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::grassland; w.tiles[tile] = tc; }
        const entity_id market = w.create_entity();
        { market_component mc; mc.body = body; mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
          mc.base_price[ri(resource_type::steel)] = 8.0f; mc.price = mc.base_price;
          w.markets[market] = mc; } // inventory left at 0 — no stock at all.
        const entity_id bld = w.create_entity();
        { building_component b{}; b.tile = tile; b.type = building_type::processing_facility;
          b.workforce_assigned = 0.5f; b.recipe = steel_id; w.buildings[bld] = b; }
        const entity_id corp = w.create_entity();
        { corporation_component cc; cc.balance = 10000.0f; cc.is_player = true;
          cc.assets.push_back(bld); w.corporations[corp] = cc; }
        // No pool seeded either — zero available from any source.

        const economy_report rep = run_economy_step(w, reg);
        bool idle = false;
        for (const auto& br : rep.buildings)
            if (br.building == bld) idle = br.idle;
        check(idle, "R1 empty pool + empty market inventory -> processor idles (no free auto-buy)");
    }

    // --- R2: the SAME scenario, but with ample market inventory -> processor
    // runs a full batch, drawing the whole need from market inventory, which
    // is then decremented by exactly that amount. ---
    {
        world w;
        const entity_id body = w.create_entity(); w.bodies[body] = body_component{};
        const entity_id tile = w.create_entity();
        { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::grassland; w.tiles[tile] = tc; }
        const entity_id market = w.create_entity();
        { market_component mc; mc.body = body; mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
          mc.base_price[ri(resource_type::steel)] = 8.0f; mc.price = mc.base_price;
          mc.inventory[ri(resource_type::iron_ore)] = 1000.0f;
          w.markets[market] = mc; }
        const entity_id bld = w.create_entity();
        { building_component b{}; b.tile = tile; b.type = building_type::processing_facility;
          b.workforce_assigned = 0.5f; b.recipe = steel_id; w.buildings[bld] = b; }
        const entity_id corp = w.create_entity();
        { corporation_component cc; cc.balance = 10000.0f; cc.is_player = true;
          cc.assets.push_back(bld); w.corporations[corp] = cc; }

        const economy_report rep = run_economy_step(w, reg);
        float out = 0.0f; bool idle = true;
        for (const auto& br : rep.buildings)
            if (br.building == bld) { out = br.output_quantity; idle = br.idle; }
        // batches_full = 20*0.5 = 10; need iron = 2*10 = 20; steel out = 10.
        check(!idle, "R2 ample market inventory -> processor is active");
        check(near(out, 10.0f), "R2 full batch produced (base_rate x workforce = 10 steel)");
        check(near(w.markets.at(market).inventory[ri(resource_type::iron_ore)], 1000.0f - 20.0f),
              "R2 market inventory drained by exactly the drawn quantity (20 iron)");
    }

    // --- R3: pool PARTIALLY covers, market inventory covers the rest -> full
    // batch, pool exhausted, market inventory drained only by the remainder. ---
    {
        world w;
        const entity_id body = w.create_entity(); w.bodies[body] = body_component{};
        const entity_id tile = w.create_entity();
        { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::grassland; w.tiles[tile] = tc; }
        const entity_id market = w.create_entity();
        { market_component mc; mc.body = body; mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
          mc.base_price[ri(resource_type::steel)] = 8.0f; mc.price = mc.base_price;
          mc.inventory[ri(resource_type::iron_ore)] = 100.0f;
          w.markets[market] = mc; }
        const entity_id bld = w.create_entity();
        { building_component b{}; b.tile = tile; b.type = building_type::processing_facility;
          b.workforce_assigned = 0.5f; b.recipe = steel_id; w.buildings[bld] = b; }
        const entity_id corp = w.create_entity();
        { corporation_component cc; cc.balance = 10000.0f; cc.is_player = true;
          cc.assets.push_back(bld); w.corporations[corp] = cc; }
        w.pool_for(corp, body).quantities[ri(resource_type::iron_ore)] = 12.0f; // covers 12 of the 20 needed

        const economy_report rep = run_economy_step(w, reg);
        float out = 0.0f;
        for (const auto& br : rep.buildings) if (br.building == bld) out = br.output_quantity;
        check(near(out, 10.0f), "R3 pool + market inventory together still yield a full batch");
        check(near(w.pool_for(corp, body).quantities[ri(resource_type::iron_ore)], 0.0f),
              "R3 pool drawn down to zero (pool-first)");
        check(near(w.markets.at(market).inventory[ri(resource_type::iron_ore)], 100.0f - 8.0f),
              "R3 market inventory drained only by the remainder (20 - 12 = 8)");
    }

    // --- R4: coverage BETWEEN t_idle and t_full -> the two-threshold model now
    // applies uniformly, even on a market body (BL-130 retires the old
    // "market bodies always run full batch" special case). ---
    {
        world w;
        const entity_id body = w.create_entity(); w.bodies[body] = body_component{};
        const entity_id tile = w.create_entity();
        { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::grassland; w.tiles[tile] = tc; }
        const entity_id market = w.create_entity();
        { market_component mc; mc.body = body; mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
          mc.base_price[ri(resource_type::steel)] = 8.0f; mc.price = mc.base_price;
          mc.inventory[ri(resource_type::iron_ore)] = 8.0f; // covers 8 of the 20 needed -> coverage 0.4
          w.markets[market] = mc; }
        const entity_id bld = w.create_entity();
        { building_component b{}; b.tile = tile; b.type = building_type::processing_facility;
          b.workforce_assigned = 0.5f; b.recipe = steel_id; w.buildings[bld] = b; }
        const entity_id corp = w.create_entity();
        { corporation_component cc; cc.balance = 10000.0f; cc.is_player = true;
          cc.assets.push_back(bld); w.corporations[corp] = cc; }

        const economy_report rep = run_economy_step(w, reg);
        float out = 0.0f; bool idle = true;
        for (const auto& br : rep.buildings) if (br.building == bld) { out = br.output_quantity; idle = br.idle; }
        // coverage = 8/20 = 0.4 (between t_idle=0.2, t_full=1.0) -> run=0.4 -> steel = 10*0.4 = 4.
        check(!idle, "R4 partial coverage between thresholds is active, not idle");
        check(near(out, 4.0f), "R4 output scales with coverage (0.4 x full batch of 10 = 4)");
        check(near(w.markets.at(market).inventory[ri(resource_type::iron_ore)], 0.0f),
              "R4 market inventory fully drawn down (8 of 8 available, at the scaled need)");
    }

    // --- R5: inventory fills from REAL corp sales, not from the abstract
    // substrate. A corp with surplus above its processor reservation sells it;
    // the sold quantity lands in inventory. ---
    {
        world w;
        const entity_id body = w.create_entity(); w.bodies[body] = body_component{};
        const entity_id tile = w.create_entity();
        { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::grassland; w.tiles[tile] = tc; }
        const entity_id market = w.create_entity();
        { market_component mc; mc.body = body; mc.base_price[ri(resource_type::steel)] = 8.0f;
          mc.price = mc.base_price; w.markets[market] = mc; }
        const entity_id corp = w.create_entity();
        { corporation_component cc; cc.balance = 1000.0f; cc.is_player = true; w.corporations[corp] = cc; }
        w.pool_for(corp, body).quantities[ri(resource_type::steel)] = 50.0f; // pure surplus, no processors reserving it.

        const economy_report rep = run_economy_step(w, reg);
        auto flows = clear_markets(w, reg, rep);
        (void)flows;
        check(near(w.markets.at(market).inventory[ri(resource_type::steel)], 50.0f),
              "R5 a corp's real surplus sale lands in market inventory");
        check(near(w.pool_for(corp, body).quantities[ri(resource_type::steel)], 0.0f),
              "R5 the corp's pool is debited by the sold quantity");
    }

    // --- R6: construction draws real inventory too, and drains it —
    // construction_gate_harness (BL-095) already covers the pacing curve in
    // detail; this just confirms the inventory field itself is what gates it
    // now, end to end via the real recipe_registry path. ---
    {
        world w;
        recipe_registry reg2;
        { building_economics pr; pr.build_cost = 0.0f; pr.build_duration_ticks = 1.0f;
          pr.resource_build_cost[ri(resource_type::steel)] = 10.0f;
          reg2.set_economics(building_type::processing_facility, pr); }
        const entity_id body = w.create_entity(); w.bodies[body] = body_component{};
        const entity_id tile = w.create_entity();
        { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::grassland; w.tiles[tile] = tc; }
        const entity_id market = w.create_entity();
        { market_component mc; mc.body = body; mc.centre_tile = tile;
          mc.base_price[ri(resource_type::steel)] = 8.0f; mc.price = mc.base_price;
          mc.inventory[ri(resource_type::steel)] = 5.0f; // half of what the build needs this tick.
          w.markets[market] = mc; }
        const entity_id corp = w.create_entity();
        { corporation_component cc; cc.balance = 10000.0f; w.corporations[corp] = cc; }
        const entity_id bld = w.create_entity();
        { building_component b{}; b.tile = tile; b.type = building_type::processing_facility;
          b.ticks_remaining = 1; b.construction_progress = 0.0f; w.buildings[bld] = b;
          w.corporations[corp].assets.push_back(bld); }

        // run_construction is file-internal to economy_system.cpp; reached here
        // through the public run_economy_step entry point, whose first pass it is.
        run_economy_step(w, reg2);
        check(near(w.markets.at(market).inventory[ri(resource_type::steel)], 0.0f),
              "R6 construction draws real inventory (5 of 5 available, rate capped at 0.5)");
        check(near(w.buildings.at(bld).construction_progress, 0.5f),
              "R6 construction rate is capped by real inventory, not an unconditional draw");
    }

    if (g_failures == 0) std::printf("\nALL PASS\n");
    else                 std::printf("\n%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
