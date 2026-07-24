// Headless harness for BL-166 (Hydroponics Bay) and BL-168 (Fishing Wharf) — the two
// controlled-environment / coastal agriculture buildings that share the terrestrial
// Farm's raw-tier agricultural_produce good via their own processing-style recipes.
// No SDL / Lua / ImGui. Kept outside src/ so the CMake glob does not pull it into the
// real build; picked up automatically by the generic tools/verify/*.cpp CTest loop
// (CMakeLists.txt § CTest wiring), which links the SDL/Lua-free world superset.
//
// Covers:
//   - placement_rules::has_terrestrial_farm_affinity and the Hydroponics Bay
//     placement predicate (its logical inverse of Farm's own deposit check).
//   - placement_rules::is_coastal reused (unchanged) as the Fishing Wharf predicate,
//     via can_place_in_world (no stored per-tile coastal flag).
//   - construct_building seeds each type with its own fixed recipe.
//   - run_economy_step actually produces agricultural_produce through run_processing
//     for both building types, using their OWN building_economics (not
//     processing_facility's hardcoded rate — the BL-166/BL-168 economy_system.cpp fix).

#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what, float got = 0.0f, float want = 0.0f)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else { std::printf("  FAIL  %s   (got %.3f, want %.3f)\n", what, got, want); ++g_failures; }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    std::printf("BL-166/BL-168 agriculture-buildings harness\n");

    // --- registry: Hydroponics Bay + Fishing Wharf economics and recipes ---
    recipe_registry reg;
    {
        building_economics hb; hb.base_rate = 6.0f; hb.maintenance = 8.0f; hb.base_wage = 10.0f;
        hb.build_cost = 180.0f;
        reg.set_economics(building_type::hydroponics_bay, hb);

        building_economics fw; fw.base_rate = 8.0f; fw.maintenance = 6.0f; fw.base_wage = 8.0f;
        fw.build_cost = 140.0f;
        reg.set_economics(building_type::fishing_wharf, fw);
    }
    recipe hydroponics_recipe;
    hydroponics_recipe.name = "hydroponics_produce";
    hydroponics_recipe.inputs[ri(resource_type::water)] = 1.0f;
    hydroponics_recipe.inputs[ri(resource_type::steel)] = 0.5f;
    hydroponics_recipe.outputs[ri(resource_type::agricultural_produce)] = 1.0f;
    const uint16_t hydroponics_id = reg.add_recipe(hydroponics_recipe);

    recipe fishing_recipe;
    fishing_recipe.name = "fishing_produce";
    fishing_recipe.outputs[ri(resource_type::agricultural_produce)] = 1.0f;
    const uint16_t fishing_id = reg.add_recipe(fishing_recipe);

    // --- world: one body with four tiles ---
    //   arable    — grassland, agricultural_produce deposit (Farm would succeed here)
    //   barren    — barren, no agricultural_produce deposit (Hydroponics Bay's gap)
    //   coastal   — barren land tile adjacent to an ocean tile (Fishing Wharf valid)
    //   inland    — barren land tile with no ocean neighbour (Fishing Wharf invalid)
    //   ocean     — the coastal tile's ocean neighbour
    world w;
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    w.bodies[body].grid_width  = 4;
    w.bodies[body].grid_height = 2;

    const entity_id arable = w.create_entity();
    {
        tile_component tc{};
        tc.body = body; tc.grid_x = 0; tc.grid_y = 0;
        tc.composition = terrain_composition::grassland;
        tc.resource_deposit[ri(resource_type::agricultural_produce)] = 1.0f;
        w.tiles[arable] = tc;
    }
    const entity_id barren = w.create_entity();
    {
        tile_component tc{};
        tc.body = body; tc.grid_x = 1; tc.grid_y = 0;
        tc.composition = terrain_composition::barren;
        w.tiles[barren] = tc;
    }
    const entity_id coastal = w.create_entity();
    {
        tile_component tc{};
        tc.body = body; tc.grid_x = 2; tc.grid_y = 0;
        tc.composition = terrain_composition::barren;
        w.tiles[coastal] = tc;
    }
    const entity_id inland = w.create_entity();
    {
        tile_component tc{};
        tc.body = body; tc.grid_x = 3; tc.grid_y = 0;
        tc.composition = terrain_composition::barren;
        w.tiles[inland] = tc;
    }
    const entity_id ocean = w.create_entity();
    {
        // Even-row (grid_y=0) hex neighbours use even_off = {(+1,0),(0,-1),(-1,-1),(-1,0),
        // (-1,+1),(0,+1)} (placement_rules.cpp is_coastal). `coastal` is (2,0); its (-1,+1)
        // offset lands on (1,1) — a neighbour of `coastal` alone, NOT of `inland` (3,0),
        // whose own neighbour set is {(0,0),(2,0),(2,1),(3,1)} (mod column wrap).
        tile_component tc{};
        tc.body = body; tc.grid_x = 1; tc.grid_y = 1;
        tc.composition = terrain_composition::ocean;
        w.tiles[ocean] = tc;
    }

    // --- Tile-level predicate: has_terrestrial_farm_affinity ---
    check(placement_rules::has_terrestrial_farm_affinity(w.tiles[arable]) == true,
          "P.1 arable tile (agricultural_produce deposit) carries farm affinity");
    check(placement_rules::has_terrestrial_farm_affinity(w.tiles[barren]) == false,
          "P.2 barren tile (no deposit) lacks farm affinity");

    // --- BL-166: Hydroponics Bay placement — the inverse of Farm's predicate ---
    check(!placement_rules::can_place(w.tiles[arable], building_type::hydroponics_bay, resource_type::iron_ore),
          "P.3 Hydroponics Bay REJECTED where Farm would succeed (arable tile)");
    check(placement_rules::can_place(w.tiles[arable], building_type::hydroponics_bay, resource_type::iron_ore).reason
              == placement_rules::placement_reason::has_farm_affinity,
          "P.4 rejection reason is has_farm_affinity");
    check(static_cast<bool>(placement_rules::can_place(w.tiles[barren], building_type::hydroponics_bay, resource_type::iron_ore)),
          "P.5 Hydroponics Bay OK on a barren tile lacking farm affinity");
    // Farm itself (extraction_site targeting agricultural_produce) is the mirror check.
    check(static_cast<bool>(placement_rules::can_place(w.tiles[arable], building_type::extraction_site, resource_type::agricultural_produce)),
          "P.6 Farm (extraction_site) OK on the arable tile (sanity: the tile it targets)");
    check(!placement_rules::can_place(w.tiles[barren], building_type::extraction_site, resource_type::agricultural_produce),
          "P.7 Farm rejected on the barren tile (no deposit) — the gap Hydroponics Bay fills");

    // --- BL-168: Fishing Wharf placement — reuses is_coastal via can_place_in_world ---
    check(placement_rules::is_coastal(w, coastal) == true,
          "P.8 coastal tile has an ocean neighbour");
    check(placement_rules::is_coastal(w, inland) == false,
          "P.9 inland tile has no ocean neighbour");
    check(static_cast<bool>(placement_rules::can_place_in_world(w, coastal, building_type::fishing_wharf, resource_type::iron_ore)),
          "P.10 Fishing Wharf OK on a coastal tile");
    check(!placement_rules::can_place_in_world(w, inland, building_type::fishing_wharf, resource_type::iron_ore),
          "P.11 Fishing Wharf rejected on a non-coastal tile");
    check(placement_rules::can_place_in_world(w, inland, building_type::fishing_wharf, resource_type::iron_ore).reason
              == placement_rules::placement_reason::not_coastal,
          "P.12 rejection reason is not_coastal");
    // Also on the ocean tile itself — never buildable on water.
    check(!placement_rules::can_place_in_world(w, ocean, building_type::fishing_wharf, resource_type::iron_ore),
          "P.13 Fishing Wharf rejected on the ocean tile itself");

    // --- Construction: each type seeded with its OWN fixed recipe ---
    const entity_id corp = w.create_entity();
    {
        corporation_component cc;
        cc.balance = 10000.0f;
        w.corporations[corp] = cc;
    }
    w.player_entity = corp;

    entity_id hb_bld = null_entity;
    auto r_hb = construct_building(w, reg, corp, barren, building_type::hydroponics_bay,
                                   resource_type::iron_ore, hb_bld);
    check(r_hb == construction_result::placed && hb_bld != null_entity,
          "C.1 Hydroponics Bay placed on the barren tile");
    check(w.buildings.count(hb_bld) == 1 && w.buildings[hb_bld].recipe == hydroponics_id,
          "C.2 Hydroponics Bay seeded with the hydroponics_produce recipe");

    entity_id fw_bld = null_entity;
    auto r_fw = construct_building(w, reg, corp, coastal, building_type::fishing_wharf,
                                   resource_type::iron_ore, fw_bld);
    check(r_fw == construction_result::placed && fw_bld != null_entity,
          "C.3 Fishing Wharf placed on the coastal tile");
    check(w.buildings.count(fw_bld) == 1 && w.buildings[fw_bld].recipe == fishing_id,
          "C.4 Fishing Wharf seeded with the fishing_produce recipe");

    auto r_fw_bad = construct_building(w, reg, corp, inland, building_type::fishing_wharf,
                                       resource_type::iron_ore, fw_bld);
    check(r_fw_bad == construction_result::invalid_tile,
          "C.5 Fishing Wharf construction refused on a non-coastal tile");

    // --- Production: run one economy tick and confirm both buildings produce
    //     agricultural_produce through run_processing, at THEIR OWN base_rate
    //     (not processing_facility's hardcoded one — the BL-166/BL-168 fix). ---
    w.buildings[hb_bld].ticks_remaining = 0;
    w.buildings[fw_bld].ticks_remaining = 0;
    w.buildings[hb_bld].workforce_assigned = 1.0f;
    w.buildings[fw_bld].workforce_assigned = 1.0f;
    w.pool_for(corp, body).quantities[ri(resource_type::water)] = 1.0e6f;
    w.pool_for(corp, body).quantities[ri(resource_type::steel)] = 1.0e6f;

    const economy_report rep = run_economy_step(w, reg);

    const float hb_out = w.pool_for(corp, body).quantities[ri(resource_type::agricultural_produce)];
    check(hb_out > 0.0f,
          "E.1 Hydroponics Bay + Fishing Wharf together produced agricultural_produce this tick",
          hb_out, 0.0f);

    // Hydroponics Bay alone: base_rate 6.0 * workforce 1.0 * wt_scalar 1.0 = 6 batches,
    // each consuming 1 water + 0.5 steel and outputting 1 agricultural_produce -> 6 units.
    // Fishing Wharf alone: base_rate 8.0, zero-input recipe -> full 8 batches -> 8 units.
    // Combined pool credit: 14 units (no market on this body, so both draw pool-first /
    // run at full batches_full since the zero/ample-input coverage is 1.0).
    check(near(hb_out, 14.0f),
          "E.2 combined output matches each building's OWN base_rate (6 + 8 = 14)", hb_out, 14.0f);

    bool found_hb_report = false, found_fw_report = false;
    for (const building_report& br : rep.buildings)
    {
        if (br.building == hb_bld) { found_hb_report = true; check(near(br.output_quantity, 6.0f), "E.3 Hydroponics Bay report output_quantity == 6", br.output_quantity, 6.0f); }
        if (br.building == fw_bld) { found_fw_report = true; check(near(br.output_quantity, 8.0f), "E.4 Fishing Wharf report output_quantity == 8", br.output_quantity, 8.0f); }
    }
    check(found_hb_report, "E.5 Hydroponics Bay building_report present");
    check(found_fw_report, "E.6 Fishing Wharf building_report present");

    std::printf("\n%s  (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
