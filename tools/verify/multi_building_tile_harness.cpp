// Headless harness for BL-366 (multi-building tile stack cap + urban transform):
// the composition-keyed non-extraction stack cap, the aggregate (cross-type)
// occupancy count it is checked against, the one-way urban transform that fires
// when the cap fills, and the consequences of being urban (no new extraction,
// raised habitability floor, grandfathered pre-transform sites). Hand-builds a
// minimal world (no Lua / SDL / ImGui); kept outside src/ so the CMake glob does
// not pull it into the real build.
//
// Build: cmake --build build --target multi_building_tile_harness
// Run:   .\build\multi_building_tile_harness.exe

#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    recipe_registry reg;
    {
        building_economics ex; ex.build_cost = 10.0f; reg.set_economics(building_type::extraction_site, ex);
        building_economics pr; pr.build_cost = 10.0f; reg.set_economics(building_type::processing_facility, pr);
        building_economics hb; hb.build_cost = 10.0f; reg.set_economics(building_type::inland_logistics_hub, hb);
        building_economics mb; mb.build_cost = 10.0f; reg.set_economics(building_type::military_base, mb);
        building_economics ri_; ri_.build_cost = 10.0f; reg.set_economics(building_type::research_institute, ri_);
    }
    recipe steel; steel.name = "steel"; reg.add_recipe(steel);

    world w;
    const entity_id body = w.create_entity(); w.bodies[body] = body_component{};

    const entity_id corp = w.create_entity();
    { corporation_component cc; cc.balance = 100000.0f; w.corporations[corp] = cc; }

    std::printf("Multi-building tile (BL-366) harness\n");

    // --- R1: non_extraction_stack_cap table -----------------------------------
    // BL-519: the cap is now a function of (substrate, cover), so this table is
    // written as the PAIR each pre-split composition became. That makes it a
    // regression check on the split itself — every one of these numbers is the
    // number BL-366 authored, and if the decomposition drifts, these fail.
    using su = terrain_substrate;
    using cv = terrain_cover;
    auto cap = [](su s, cv c) { return placement_rules::non_extraction_stack_cap(s, c); };
    check(cap(su::sedimentary, cv::grass)  == 6,  "R1 grassland (grass on soil) cap == 6");
    check(cap(su::sedimentary, cv::forest) == 6,  "R1 forest (forest on soil) cap == 6");
    check(cap(su::sedimentary, cv::marsh)  == 6,  "R1 wetland (marsh on soil) cap == 6");
    check(cap(su::sedimentary, cv::scrub)  == 3,  "R1 tundra (scrub on soil) cap == 3");
    check(cap(su::barren,   cv::none)      == 4,  "R1 barren cap == 4");
    check(cap(su::rocky,    cv::none)      == 4,  "R1 rocky cap == 4");
    check(cap(su::regolith, cv::none)      == 4,  "R1 regolith cap == 4");
    check(cap(su::metallic, cv::none)      == 4,  "R1 metallic cap == 4");
    check(cap(su::volcanic, cv::none)      == 2,  "R1 volcanic cap == 2");
    check(cap(su::icy,      cv::none)      == 2,  "R1 icy cap == 2");
    check(cap(su::sedimentary, cv::urban)  == 12, "R1 urban cap == 12");
    check(cap(su::ocean,    cv::none)      == 0,  "R1 ocean cap == 0");

    // R1b — THE PAIRS THE PRE-SPLIT MODEL COULD NOT NAME. Urban keeps its cap
    // whatever it is built on, which is what makes it a cover rather than a
    // replacement for the ground; and a forested crag is harder to build on than
    // the bare rock beside it, which is the whole point of the axis.
    check(cap(su::metallic, cv::urban) == 12, "R1b urban on metallic keeps the urban cap");
    check(cap(su::rocky,    cv::forest) == 3, "R1b a forested crag is harder than bare rock (4)");
    check(cap(su::rocky,    cv::none)   == 4, "R1b ...and the bare rock beside it is unchanged");

    // --- R7: a tile that never reaches its cap never transforms ---------------
    const entity_id under_cap_tile = w.create_entity();
    { tile_component tc{}; tc.body = body; tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
      tc.grid_x = 0; tc.grid_y = 0; w.tiles[under_cap_tile] = tc; }
    entity_id built = null_entity;
    construct_building(w, reg, corp, under_cap_tile, building_type::processing_facility,
                       resource_type::iron_ore, built);
    construct_building(w, reg, corp, under_cap_tile, building_type::inland_logistics_hub,
                       resource_type::iron_ore, built);
    check(w.tiles[under_cap_tile].cover == terrain_cover::grass,
          "R7 two buildings on grassland (cap 6) does not transform");

    // --- R2/R3: 6 MIXED non-extraction buildings fire the transform on the 6th,
    // proving the cap counts in aggregate across types, not per type. ----------
    const entity_id tile = w.create_entity();
    {
        tile_component t{};
        t.body = body;
        t.substrate = terrain_substrate::sedimentary; t.cover = terrain_cover::grass; t.cover_density = 150;
        t.grid_x = 1; t.grid_y = 0;
        t.resource_deposit[ri(resource_type::iron_ore)] = 100.0f; // for R4/R5 below
        w.tiles[tile] = t;
    }

    // military_base is excluded — it is tech-gated (BL-344's `garrison` gate),
    // and this harness's corp earns no tech, so it would spuriously refuse.
    const building_type mix[6] = {
        building_type::processing_facility, building_type::processing_facility,
        building_type::inland_logistics_hub, building_type::inland_logistics_hub,
        building_type::research_institute, building_type::processing_facility,
    };

    // Pre-transform extraction site (R5 grandfathering): placed before the stack
    // fills, on the deposit seeded above.
    entity_id pre_extraction = null_entity;
    auto r_pre = construct_building(w, reg, corp, tile, building_type::extraction_site,
                                    resource_type::iron_ore, pre_extraction);
    check(r_pre == construction_result::placed && pre_extraction != null_entity,
          "R5 setup: pre-transform extraction site placed");

    construction_result last_mix_result = construction_result::placed;
    for (int i = 0; i < 6; ++i)
    {
        entity_id b = null_entity;
        last_mix_result = construct_building(w, reg, corp, tile, mix[i],
                                             resource_type::iron_ore, b);
        check(last_mix_result == construction_result::placed, "R2 mixed-type placement succeeds pre-cap");
    }
    check(w.tiles[tile].cover == terrain_cover::urban,
          "R2 6th mixed non-extraction building (aggregate cap 6) fires urban transform");

    // R3: urban's own cap (12) is higher — a 7th non-extraction placement still succeeds.
    entity_id seventh = null_entity;
    auto r7 = construct_building(w, reg, corp, tile, building_type::processing_facility,
                                 resource_type::iron_ore, seventh);
    check(r7 == construction_result::placed, "R3 7th non-extraction placement succeeds on urban (cap 12)");

    // R4: a NEW extraction site is refused on the now-urban tile, even though the
    // tile carries a real seeded deposit.
    entity_id new_extraction = null_entity;
    auto r_new_ext = construct_building(w, reg, corp, tile, building_type::extraction_site,
                                        resource_type::iron_ore, new_extraction);
    check(r_new_ext == construction_result::invalid_tile && new_extraction == null_entity,
          "R4 new extraction refused on urban tile despite real deposit");

    // R5: the PRE-transform extraction site is grandfathered — still standing.
    check(w.buildings.count(pre_extraction) == 1,
          "R5 pre-transform extraction site survives the urban transform (grandfathered)");

    // R6: habitability raised to >= 0.80 by the transform.
    check(w.tiles[tile].habitability >= 0.80f - 1e-4f,
          "R6 habitability raised to the urban ceiling (>= 0.80)");

    // R6b: habitability is never LOWERED by the transform if it started above 0.80.
    const entity_id high_hab_tile = w.create_entity();
    {
        tile_component t{};
        t.body = body;
        t.substrate = terrain_substrate::sedimentary; t.cover = terrain_cover::grass; t.cover_density = 150;
        t.grid_x = 2; t.grid_y = 0;
        t.habitability = 0.95f;
        w.tiles[high_hab_tile] = t;
    }
    for (int i = 0; i < 6; ++i)
    {
        entity_id b = null_entity;
        construct_building(w, reg, corp, high_hab_tile, building_type::processing_facility,
                           resource_type::iron_ore, b);
    }
    check(w.tiles[high_hab_tile].cover == terrain_cover::urban,
          "R6b setup: high-habitability tile also transforms at its cap");
    check(near(w.tiles[high_hab_tile].habitability, 0.95f),
          "R6b transform does not LOWER a habitability already above the urban floor");

    // R8: extraction stacking itself is unaffected — richness/50 rule still governs.
    {
        tile_component richness_tc{};
        richness_tc.resource_deposit[ri(resource_type::iron_ore)] = 125.0f; // -> cap 2 (125/50 = 2.5 -> 2)
        const int cap = placement_rules::stack_capacity(richness_tc, building_type::extraction_site,
                                                         resource_type::iron_ore);
        check(cap == 2, "R8 extraction stack_capacity still richness/50-bound (unchanged by BL-366)");
    }

    std::printf(g_failures == 0 ? "\nALL PASS\n" : "\n%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
