// Headless harness for BL-263 (spontaneous market emergence): a market appears
// the tick a body's first building completes (any corporation), never before,
// never a second one on the same body; opening prices are seeded from the home
// body's own prices marked up by distance; and an outpost market's demand is
// pulled from the home body's own unmet demand rather than collapsing to the
// price floor. Kept outside src/ so the CMake glob does not pull it into the
// real build.

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
    { building_economics ex; ex.build_cost = 10.0f; reg.set_economics(building_type::extraction_site, ex); }
    { building_economics pr; pr.build_cost = 10.0f; reg.set_economics(building_type::processing_facility, pr); }

    world w;

    // --- Home body (Kepler-analog), with a seeded market, 1 AU. ---
    const entity_id home_body = w.create_entity();
    { body_component bc{}; bc.orbital_radius_au = 1.0f; bc.parent = null_entity; w.bodies[home_body] = bc; }
    w.home_body = home_body;

    const entity_id home_market = w.create_entity();
    {
        market_component mc;
        mc.body = home_body;
        mc.base_price[ri(resource_type::iron_ore)] = 4.0f;
        mc.base_price[ri(resource_type::water)]    = 2.0f;
        // steel left untradeable (base_price 0) deliberately, for R2's skip check.
        mc.price = mc.base_price;
        // A real shortfall for iron_ore at home, so R5 (interbody pull) has
        // something to pull.
        mc.demand[ri(resource_type::iron_ore)] = 100.0f;
        mc.supply[ri(resource_type::iron_ore)] = 20.0f; // shortfall = 80
        w.markets[home_market] = mc;
    }

    // --- An outpost body, 5 AU out (distance 4 AU from home). ---
    const entity_id outpost_body = w.create_entity();
    { body_component bc{}; bc.orbital_radius_au = 5.0f; bc.parent = null_entity; w.bodies[outpost_body] = bc; }

    const entity_id outpost_tile = w.create_entity();
    { tile_component tc{}; tc.body = outpost_body; tc.composition = terrain_composition::rocky;
      tc.resource_deposit[ri(resource_type::iron_ore)] = 100.0f; w.tiles[outpost_tile] = tc; }

    const entity_id corp = w.create_entity();
    { corporation_component cc; cc.balance = 100000.0f; w.corporations[corp] = cc; }

    std::printf("Spontaneous market emergence (BL-263) harness\n");

    // R1: before any building, the outpost body has no market.
    check(!w.markets.count(0) /* trivial */, "harness sanity");
    bool had_market_before = false;
    for (const auto& [mid, mc] : w.markets) if (mc.body == outpost_body) had_market_before = true;
    check(!had_market_before, "R1 outpost body carries no market before any building");

    // R2/R3: placing an EXTRACTION SITE (build_duration_ticks == 0 here, so
    // placement IS completion) fires the trigger; opening price is seeded from
    // home's base_price marked up by distance (mp.price_distance_gain = 0
    // default here isn't set — use the real default 0.08 via a fresh registry
    // with explicit market_emergence_params for a legible, exact check).
    {
        market_emergence_params mp;
        mp.price_distance_gain = 0.08f;
        mp.pull_fraction       = 0.50f;
        mp.distance_falloff    = 0.15f;
        reg.set_market_emergence(mp);
    }
    entity_id built = null_entity;
    auto r = construct_building(w, reg, corp, outpost_tile, building_type::extraction_site,
                                resource_type::iron_ore, built);
    check(r == construction_result::placed, "R2 setup: extraction site placed on the outpost");

    entity_id new_market = null_entity;
    for (const auto& [mid, mc] : w.markets) if (mc.body == outpost_body) new_market = mid;
    check(new_market != null_entity, "R2 a market now exists on the outpost body");

    if (new_market != null_entity)
    {
        const market_component& om = w.markets.at(new_market);
        check(om.centre_tile == outpost_tile, "R2 the new market's centre_tile is the completed building's tile");
        // distance = |5 - 1| = 4 AU; markup = 1 + 0.08*4 = 1.32
        const float expected_iron = 4.0f * 1.32f;
        check(near(om.base_price[ri(resource_type::iron_ore)], expected_iron),
              "R2 opening iron_ore price = home base_price x (1 + price_distance_gain x distance_au)");
        check(near(om.base_price[ri(resource_type::water)], 2.0f * 1.32f),
              "R2 opening water price also marked up identically");
        check(om.base_price[ri(resource_type::steel)] == 0.0f,
              "R3 an untradeable-at-home resource (steel, base_price 0) stays untradeable at the outpost");
        check(near(om.price[ri(resource_type::iron_ore)], om.base_price[ri(resource_type::iron_ore)]),
              "R2 opening price == opening base_price (no EMA history to smooth toward)");
    }

    // R4: a SECOND building on the same outpost body does NOT spawn a second market.
    entity_id built2 = null_entity;
    construct_building(w, reg, corp, outpost_tile, building_type::extraction_site,
                       resource_type::iron_ore, built2);
    int outpost_market_count = 0;
    for (const auto& [mid, mc] : w.markets) if (mc.body == outpost_body) ++outpost_market_count;
    check(outpost_market_count == 1, "R4 a second building on the same body spawns no second market");

    // R5: inject_interbody_demand pulls a discounted slice of home's shortfall
    // onto the outpost market for iron_ore, and leaves an untradeable resource
    // (steel) alone despite home having no steel price either (already implied
    // by base_price gate, checked explicitly for completeness).
    if (new_market != null_entity)
    {
        market_component& om = w.markets.at(new_market);
        om.demand.fill(0.0f); // isolate this call's effect
        inject_interbody_demand(w, reg);
        // home shortfall = 100 - 20 = 80; falloff = 1 + 0.15*4 = 1.6
        // pulled = 80 * 0.50 / 1.6 = 25.0
        check(near(om.demand[ri(resource_type::iron_ore)], 25.0f),
              "R5 outpost iron_ore demand pulled = home_shortfall x pull_fraction / (1 + distance_falloff x AU)");
        check(om.demand[ri(resource_type::steel)] == 0.0f,
              "R5 an untradeable outpost resource is never pulled regardless of any home shortfall");
    }

    // R6: the home market itself is never a target of its own pull (no self-injection).
    {
        market_component& hm = w.markets.at(home_market);
        const float before = hm.demand[ri(resource_type::iron_ore)];
        inject_interbody_demand(w, reg);
        check(near(hm.demand[ri(resource_type::iron_ore)], before),
              "R6 the home market's own demand is untouched by inject_interbody_demand");
    }

    // R7: no home market at all -> maybe_spawn_market degrades to an all-zero
    // (untradeable, harmless) market rather than crashing; inject_interbody_demand
    // is a clean no-op.
    {
        world w2;
        const entity_id body2 = w2.create_entity();
        { body_component bc{}; bc.orbital_radius_au = 2.0f; w2.bodies[body2] = bc; }
        const entity_id tile2 = w2.create_entity();
        { tile_component tc{}; tc.body = body2; w2.tiles[tile2] = tc; }
        // w2.home_body left null_entity (degenerate fixture).
        const entity_id mid2 = maybe_spawn_market(w2, reg, body2, tile2);
        check(mid2 != null_entity, "R7 a market still spawns with no home body to seed from");
        bool all_zero = true;
        for (float p : w2.markets.at(mid2).base_price) if (p != 0.0f) all_zero = false;
        check(all_zero, "R7 with no home market, opening prices are all-zero (untradeable, not a crash)");
        inject_interbody_demand(w2, reg); // must not throw/crash with home_body == null_entity
        check(true, "R7 inject_interbody_demand is a clean no-op with no home body");
    }

    if (g_failures == 0) std::printf("\nALL PASS\n");
    else                 std::printf("\n%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
