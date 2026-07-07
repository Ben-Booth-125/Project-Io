// Headless harness for the Layer 4 player-construction logic (no SDL / Lua / ImGui).
// Builds a tiny world + a hand-built registry and asserts construct_building's
// validation, cost spend, and component authoring. Kept outside src/ so the CMake
// glob does not pull it into the real build.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src tools\verify\construction_harness.cpp ^
//      src\world\world.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
//      /Fe:construction_harness.exe

#include "world/components.hpp"
#include "world/construction.hpp"
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
    // Registry: extraction build_cost 100, processing 150, port 80; a steel recipe.
    recipe_registry reg;
    { building_economics ex; ex.build_cost = 100.0f; reg.set_economics(building_type::extraction_site, ex);
      building_economics pr; pr.build_cost = 150.0f; reg.set_economics(building_type::processing_facility, pr);
      building_economics po; po.build_cost =  80.0f; reg.set_economics(building_type::port, po); }
    recipe steel; steel.name = "steel"; const uint16_t steel_id = reg.add_recipe(steel);

    world w;
    const entity_id body = w.create_entity(); w.bodies[body] = body_component{};

    // A land tile with an iron deposit, and an ocean tile.
    const entity_id land = w.create_entity();
    { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::rocky;
      tc.resource_deposit[ri(resource_type::iron_ore)] = 2.0f; w.tiles[land] = tc; }
    const entity_id sea = w.create_entity();
    { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::ocean; w.tiles[sea] = tc; }

    // Player corp with a modest balance.
    const entity_id corp = w.create_entity();
    { corporation_component cc; cc.balance = 250.0f; w.corporations[corp] = cc; }
    w.player_entity = corp;

    std::printf("Layer 4 construction harness\n");

    entity_id built = null_entity;

    // R1: ocean tile is rejected, no mutation.
    auto r_ocean = construct_building(w, reg, corp, sea, building_type::extraction_site,
                                      resource_type::iron_ore, built);
    check(r_ocean == construction_result::invalid_tile && built == null_entity,
          "C.R1 ocean tile rejected (invalid_tile, no build)");

    // R2: valid extraction site placed. BL-095: placement no longer debits up-front
    // (pay-as-you-build — run_construction charges build_cost + materials as the build
    // progresses), so the balance is UNCHANGED at placement; asset added; target set.
    auto r_ext = construct_building(w, reg, corp, land, building_type::extraction_site,
                                    resource_type::iron_ore, built);
    check(r_ext == construction_result::placed && built != null_entity,
          "C.R2 valid extraction placed");
    check(near(w.corporations[corp].balance, 250.0f),
          "C.R2 placement does NOT debit up-front (pay-as-you-build)", w.corporations[corp].balance, 250.0f);
    check(w.corporations[corp].assets.size() == 1 && w.corporations[corp].assets[0] == built,
          "C.R2 building appended to corp assets");
    check(w.buildings.count(built) == 1 &&
          w.buildings[built].target_resource == resource_type::iron_ore &&
          near(w.buildings[built].workforce_assigned, 0.5f),
          "C.R2 building authored (target + staffed)");
    check(w.stockpiles.count(built) == 1, "C.R2 building gets a stockpile component");

    // R3: processing facility seeded with the default steel recipe; balance still
    // unchanged at placement (BL-095 pay-as-you-build).
    entity_id proc = null_entity;
    auto r_proc = construct_building(w, reg, corp, land, building_type::processing_facility,
                                     resource_type::iron_ore, proc);
    check(r_proc == construction_result::placed && w.buildings[proc].recipe == steel_id,
          "C.R3 processing facility seeded with default recipe");
    check(near(w.corporations[corp].balance, 250.0f),
          "C.R3 processing placement also un-charged at placement", w.corporations[corp].balance, 250.0f);

    // R4: the affordability GATE is retained (BL-095) — a build a corp cannot afford
    // the whole of is refused up front with no mutation, even though payment is now
    // spread across construction. Drop the balance below the processing build_cost
    // (150; no market here so material_cost is 0 -> total_cost == build_cost), then
    // attempt a VALID processing tile (Any terrain, so the refusal is the funds check,
    // not terrain — a Port would short-circuit to invalid_tile and mask it).
    w.corporations[corp].balance = 100.0f; // < 150 processing build_cost
    const entity_id land2 = w.create_entity();
    { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::rocky;
      tc.resource_deposit[ri(resource_type::iron_ore)] = 2.0f; w.tiles[land2] = tc; }
    const std::size_t n_before = w.corporations[corp].assets.size();
    entity_id none_built = null_entity;
    auto r_broke = construct_building(w, reg, corp, land2, building_type::processing_facility,
                                      resource_type::iron_ore, none_built);
    check(r_broke == construction_result::insufficient_funds &&
          none_built == null_entity &&
          near(w.corporations[corp].balance, 100.0f) &&
          w.corporations[corp].assets.size() == n_before,
          "C.R4 unaffordable build refused by the gate (no build, no spend)");

    // R5: unknown corp / tile are refused.
    entity_id x = null_entity;
    check(construct_building(w, reg, 99999u, land, building_type::port,
                             resource_type::iron_ore, x) == construction_result::no_corp,
          "C.R5 unknown corp rejected");
    check(construct_building(w, reg, corp, 99999u, building_type::port,
                             resource_type::iron_ore, x) == construction_result::no_tile,
          "C.R5 unknown tile rejected");

    std::printf("\n%s  (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
