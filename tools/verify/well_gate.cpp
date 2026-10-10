// Headless Well harness (BL-1198, water where people live; no SDL / Lua / ImGui).
//
// PRODUCTION.md § Extraction buildings, the Well paragraph (Ben, 2026-10-05,
// NR-971): an extraction_site targeting `water`, placement gated on FRESH-WATER
// adjacency — a land tile a river runs along or a lake borders — deposit-
// agnostic, the Fishing Wharf's rule with fresh water in place of ocean. The Ice
// Extractor route (an icy deposit) is unchanged.
//
//   P1  river tile (river_edges set, no deposit)        -> valid Well
//   P2  lake-shore tile (a lake neighbour, no river)     -> valid Well
//   P3  sea-coast-only tile (ocean neighbour, no fresh)  -> invalid (not_fresh_water)
//   P4  dry inland tile                                  -> invalid (not_fresh_water)
//   P5  the lake tile itself                             -> invalid (ocean: nothing on water)
//   P6  icy deposit on a river -> an Ice Extractor, NOT a Well (is_well_site false)
//   P7  a lakeshore is still not a coast (is_coastal unchanged by the refactor)
//   P8  construct_building places a Well on the river tile; refuses dry ground
//
//   E1  MULTI-TICK: a staffed Well produces water every tick for 6 ticks with
//       no deposit, at a constant rate base_rate x k_well_rate_scalar x labour,
//       never exhausts, and draws no reserve (resource_remaining stays 0).
//   E2  the same building forced onto dry ground (bypassing placement) yields
//       nothing — the fresh-water gate is the only thing that makes water here.
//   E3  the Build door's estimate prices a Well at a positive revenue.
//
// BL-1199 (fishing wharf yields) — the Fishing Wharf takes the Well's path:
//   W1  sea-coast tile, no produce deposit       -> valid Wharf (is_wharf_site)
//   W2  dry inland tile                          -> refused (not_coastal), not a Wharf
//   W3  coastal tile WITH a produce deposit      -> a Farm, not a Wharf
//   W4  a lakeshore is not a coast               -> not a Wharf
//   W5  MULTI-TICK: a staffed Wharf produces produce every tick for 6 ticks at
//       base_rate x k_wharf_rate_scalar x labour, constant, never exhausts,
//       draws no reserve; extraction_nominal (input_reach's building_output)
//       agrees with the tick.
//   W6  the same site forced onto dry inland ground yields nothing.
//   W7  the coastal Farm draws its reserve down (the deposit route, unchanged).
//   W8  the Build door prices a Wharf (has_data).
//
// Build:  node tools/verify/build_harness.js well_gate
// Run:    build_gen/verify/well_gate.exe

#include "world/building_profit.hpp"
#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

constexpr int gw = 5, gh = 5;
entity_id g_tile[gh][gw];

} // namespace

int main()
{
    recipe_registry reg;
    {
        building_economics ex;
        ex.base_rate = 40.0f; ex.maintenance = 2.0f; ex.base_wage = 4.0f; ex.build_cost = 100.0f;
        ex.richness_reference = 24.9f; // the shipped BL-436 calibration
        reg.set_economics(building_type::extraction_site, ex);
    }

    world w;
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    w.bodies[body].grid_width  = gw;
    w.bodies[body].grid_height = gh;

    // A 5x5 grid of dry rocky land, then: row 4 ocean, (3,1) a lake.
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = body; tc.grid_x = x; tc.grid_y = y;
            tc.substrate = (y == 4) ? terrain_substrate::ocean : terrain_substrate::rocky;
            tc.habitability = 0.8f;
            w.tiles[t] = tc;
            g_tile[y][x] = t;
        }
    const entity_id lake      = g_tile[1][3];
    w.tiles[lake].substrate   = terrain_substrate::lake;
    const entity_id river     = g_tile[1][1];
    w.tiles[river].river_edges = 1u; // a river along its east side
    const entity_id lakeshore = g_tile[1][2]; // west of the lake, no river
    const entity_id coast     = g_tile[3][0]; // ocean below, nothing fresh
    const entity_id dry       = g_tile[0][0];
    const entity_id ice       = g_tile[2][2];
    w.tiles[ice].river_edges = 2u;
    w.tiles[ice].resource_deposit[ri(resource_type::water)]   = 30.0f;
    w.tiles[ice].resource_remaining[ri(resource_type::water)] = 3000.0f;
    // BL-1199: a second coast tile carrying a produce deposit — a Farm's ground.
    const entity_id farm_coast = g_tile[3][3];
    w.tiles[farm_coast].resource_deposit[ri(resource_type::agricultural_produce)]   = 30.0f;
    w.tiles[farm_coast].resource_remaining[ri(resource_type::agricultural_produce)] = 3000.0f;
    const entity_id coast2 = g_tile[3][1]; // a second bare coast tile, for the estimate

    using placement_rules::can_place_in_world;
    using placement_rules::placement_reason;
    const auto reason = [&](entity_id t) {
        return can_place_in_world(w, t, building_type::extraction_site, resource_type::water).reason;
    };

    std::printf("well_gate — BL-1198 / BL-1199\n");
    check(reason(river) == placement_reason::ok && placement_rules::is_well_site(w, river, resource_type::water),
          "P1 river tile: Well valid");
    check(reason(lakeshore) == placement_reason::ok && placement_rules::is_well_site(w, lakeshore, resource_type::water),
          "P2 lake-shore tile: Well valid");
    check(reason(coast) == placement_reason::not_fresh_water && placement_rules::is_coastal(w, coast),
          "P3 sea-coast-only tile: Well refused (not_fresh_water)");
    check(reason(dry) == placement_reason::not_fresh_water,
          "P4 dry inland tile: Well refused (not_fresh_water)");
    check(reason(lake) == placement_reason::ocean && !placement_rules::is_fresh_water_adjacent(w, lake),
          "P5 the lake tile itself: refused (nothing is built on water)");
    check(reason(ice) == placement_reason::ok && !placement_rules::is_well_site(w, ice, resource_type::water),
          "P6 icy deposit on a river: an Ice Extractor, not a Well");
    check(!placement_rules::is_coastal(w, lakeshore),
          "P7 a lakeshore is not a coast (is_coastal unchanged)");
    check(!placement_rules::is_well_site(w, river, resource_type::agricultural_produce)
              && reason(river) == placement_reason::ok,
          "P1b only a water target is a Well");

    // W1-W4 — the Fishing Wharf's predicate agrees with its placement gate.
    const auto food_reason = [&](entity_id t) {
        return can_place_in_world(w, t, building_type::extraction_site,
                                  resource_type::agricultural_produce).reason;
    };
    constexpr resource_type food = resource_type::agricultural_produce;
    check(food_reason(coast) == placement_reason::ok && placement_rules::is_wharf_site(w, coast, food)
              && placement_rules::is_depositless_site(w, coast, food),
          "W1 sea-coast tile, no produce deposit: Wharf valid");
    check(food_reason(dry) == placement_reason::not_coastal && !placement_rules::is_wharf_site(w, dry, food),
          "W2 dry inland tile: Wharf refused (not_coastal)");
    check(!placement_rules::is_wharf_site(w, farm_coast, food)
              && !placement_rules::is_depositless_site(w, farm_coast, food),
          "W3 coastal tile with a produce deposit: a Farm, not a Wharf");
    check(!placement_rules::is_wharf_site(w, lakeshore, food),
          "W4 a lakeshore is not a coast: not a Wharf");
    check(!placement_rules::is_wharf_site(w, coast, resource_type::water)
              && !placement_rules::is_well_site(w, coast, food),
          "W1b only a produce target is a Wharf; a Wharf is not a Well");

    // P8 through the real construction seam.
    const entity_id corp = w.create_entity();
    {
        corporation_component cc;
        cc.balance = 1.0e6f; cc.starting_capital = 1.0e6f;
        cc.is_player = true; // not AI-driven: this harness pins arithmetic
        w.corporations[corp] = cc;
    }
    w.player_entity = corp;
    entity_id well = null_entity, refused = null_entity;
    const construction_result r_ok  = construct_building(w, reg, corp, river, building_type::extraction_site,
                                                         resource_type::water, well);
    const construction_result r_bad = construct_building(w, reg, corp, dry, building_type::extraction_site,
                                                         resource_type::water, refused);
    check(r_ok == construction_result::placed && w.buildings.count(well) == 1,
          "P8 construct_building places a Well on the river tile");
    check(r_bad == construction_result::invalid_tile && refused == null_entity,
          "P8 construct_building refuses a Well on dry ground");

    // E3 — the Build door's estimate (before any tick moves the world).
    const building_profit est = estimate_prospective_profit(w, reg, lakeshore,
                                                            building_type::extraction_site,
                                                            resource_type::water);
    std::printf("    E3 estimate on the lakeshore: revenue %.3f (no market: price may read 0)\n",
                est.revenue);

    const building_profit west = estimate_prospective_profit(w, reg, coast2,
                                                             building_type::extraction_site, food);
    std::printf("    W8 estimate on a bare coast: revenue %.3f\n", west.revenue);

    // W5-W7 — a Wharf through the seam, a forced inland control, a coastal Farm.
    entity_id wharf = null_entity;
    const construction_result r_wharf = construct_building(w, reg, corp, coast,
                                                           building_type::extraction_site, food, wharf);
    check(r_wharf == construction_result::placed && w.buildings.count(wharf) == 1,
          "W1 construct_building places a Wharf on the coast");
    w.buildings[wharf].ticks_remaining    = 0;
    w.buildings[wharf].workforce_assigned = 0.5f;
    w.buildings[wharf].workforce_auto     = false;
    w.buildings[wharf].workforce_target   = 100;
    const auto force_site = [&](entity_id tile) {
        const entity_id id = w.create_entity();
        building_component b{};
        b.tile = tile; b.type = building_type::extraction_site;
        b.target_resource = food; b.workforce_assigned = 0.5f;
        b.workforce_auto = false;
        w.buildings[id] = b;
        w.corporations[corp].assets.push_back(id);
        return id;
    };
    const entity_id inland_wharf = force_site(dry);
    const entity_id farm         = force_site(farm_coast);
    const std::size_t rf = ri(food);
    float wharf_first = -1.0f;
    bool  wharf_steady = true, wharf_active = true, wharf_live = true, inland_idle = true;
    bool  nominal_agrees = true;

    // E1/E2 — finish the build, force a control site onto dry ground, then tick.
    w.buildings[well].ticks_remaining = 0;
    w.buildings[well].workforce_assigned = 0.5f;
    w.buildings[well].workforce_auto = false; // no market here: the solver would price water at 0
    w.buildings[well].workforce_target = 100;
    const entity_id control = w.create_entity();
    {
        building_component b{};
        b.tile = dry; b.type = building_type::extraction_site;
        b.target_resource = resource_type::water; b.workforce_assigned = 0.5f;
        b.workforce_auto = false;
        w.buildings[control] = b;
    }
    w.corporations[corp].assets.push_back(control);

    const std::size_t rw = ri(resource_type::water);
    float last = 0.0f, first_delta = -1.0f;
    bool  steady = true, all_active = true, never_exhausted = true, control_idle = true;
    for (int t = 1; t <= 6; ++t)
    {
        w.current_day_tick = t;
        const economy_report rep = run_economy_step(w, reg);
        float held = 0.0f;
        for (const auto& [key, pool] : w.landed_this_tick) // BL-1265: landings (no clear runs here)
            if (key.first == corp) held += pool.quantities[rw];
        const float delta = held - last;
        last = held;
        float out_well = 0.0f, out_ctrl = 0.0f;
        for (const building_report& br : rep.buildings)
        {
            if (br.building == well)
            {
                out_well = br.output_quantity;
                all_active = all_active && br.active;
                never_exhausted = never_exhausted && !br.exhausted;
            }
            if (br.building == control)
            {
                out_ctrl = br.output_quantity;
                control_idle = control_idle && !br.active && br.output_quantity <= 0.0f;
            }
        }
        float out_wharf = 0.0f, out_inland = 0.0f, out_farm = 0.0f;
        for (const building_report& br : rep.buildings)
        {
            if (br.building == wharf)
            {
                out_wharf    = br.output_quantity;
                wharf_active = wharf_active && br.active;
                wharf_live   = wharf_live && !br.exhausted;
            }
            if (br.building == inland_wharf)
            {
                out_inland  = br.output_quantity;
                inland_idle = inland_idle && !br.active && br.output_quantity <= 0.0f;
            }
            if (br.building == farm) out_farm = br.output_quantity;
        }
        if (wharf_first < 0.0f) wharf_first = out_wharf;
        else if (std::fabs(out_wharf - wharf_first) > 1e-4f) wharf_steady = false;
        // input_reach's building_output reads the planned figure off
        // extraction_nominal; it must be the figure the tick produced.
        if (std::fabs(extraction_nominal(w, reg, w.buildings[wharf], 1.0f, corp) - out_wharf) > 1e-4f)
            nominal_agrees = false;
        std::printf("    tick %d  wharf produce %.3f  inland %.3f  farm %.3f  wharf reserve %.3f"
                    "  farm reserve %.3f\n",
                    t, out_wharf, out_inland, out_farm, w.tiles[coast].resource_remaining[rf],
                    w.tiles[farm_coast].resource_remaining[rf]);
        if (first_delta < 0.0f) first_delta = out_well;
        else if (std::fabs(out_well - first_delta) > 1e-4f) steady = false;
        std::printf("    tick %d  well output %.3f  pool water %.3f (+%.3f)  control %.3f  reserve %.3f\n",
                    t, out_well, held, delta, out_ctrl, w.tiles[river].resource_remaining[rw]);
    }
    check(first_delta > 0.0f && all_active, "E1 the Well produces water every tick with no deposit");
    check(steady, "E1 at a constant rate (no taper)");
    check(never_exhausted && w.tiles[river].resource_remaining[rw] == 0.0f,
          "E1 never exhausts and draws no reserve");
    check(std::fabs(first_delta - 40.0f * placement_rules::k_well_rate_scalar * 0.5f) < 1e-3f,
          "E1 rate = base_rate x k_well_rate_scalar x labour (40 x 1 x 0.5 = 20)");
    check(control_idle, "E2 the same site on dry ground yields nothing");
    check(est.has_data, "E3 the Build door prices a Well (has_data)");

    check(wharf_first > 0.0f && wharf_active, "W5 the Wharf produces produce every tick with no deposit");
    check(wharf_steady, "W5 at a constant rate (no taper)");
    check(wharf_live && w.tiles[coast].resource_remaining[rf] == 0.0f,
          "W5 never exhausts and draws no reserve");
    check(std::fabs(wharf_first - 40.0f * placement_rules::k_wharf_rate_scalar * 0.5f) < 1e-3f,
          "W5 rate = base_rate x k_wharf_rate_scalar x labour (40 x 1 x 0.5 = 20)");
    check(nominal_agrees, "W5 extraction_nominal (input_reach's plan) agrees with the tick");
    check(inland_idle, "W6 the same site on dry inland ground yields nothing");
    check(w.tiles[farm_coast].resource_remaining[rf] < 3000.0f,
          "W7 the coastal Farm draws its deposit's reserve (the deposit route)");
    check(west.has_data, "W8 the Build door prices a Wharf (has_data)");

    std::printf("well_gate: %s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
