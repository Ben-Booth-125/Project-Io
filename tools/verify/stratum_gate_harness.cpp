// ---------------------------------------------------------------------------
// Headless stratum-placement-gate harness (BL-615; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises POPULATION.md § Strata gate buildings: the placement-rule axis
// through placement_rules::can_place_in_world that ties certain buildings to
// the population-centre scale ladder. Driven by authored DATA on the building
// definition (placement_gate on building_economics; a per-recipe radius for
// the heavy processor class), never a building-name switch.
//
//   S1  A university (requires_centre, min_centre_scale 4) is refused on open
//       land with the needs_centre reason.
//   S2  The same university on a Town (scale 3) centre is refused with the
//       DISTINCT centre_too_small reason — the two refusals are told apart.
//   S3  On a City (scale 4) centre it places.
//   S4  A schooling building (requires_centre, no minimum) is refused off any
//       centre and placed on the smallest centre (scale 1).
//   S5  A heavy processor (centre_proximity_radius 6) is refused beyond its
//       radius with the far_from_centre reason, and placed within it.
//   S6  The radius wraps the east-west cylinder: a tile 2 columns the short
//       way round from a centre passes even though the unwrapped distance
//       is far beyond the radius.
//   S7  The default (empty) gate gates nothing — every pre-BL-615 call site
//       keeps its old meaning.
//   S8  recipe_registry::placement_gate_for resolves the gate for a SPECIFIC
//       named building: the type's authored gate, with a processing recipe's
//       own non-zero radius overriding the type's.
//   S9  The real seam enforces it: construct_building refuses a university
//       below a City and places it on one — no caller has to remember to
//       pass the gate, the authoritative path resolves it itself.
//   S10 Deterministic: two identically-built worlds answer identically for
//       every row above (the checks are pure and order-independent).
//
// The process exits non-zero if any assertion FAILs.

#include "world/construction.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

constexpr int GW = 20, GH = 8;

/// A flat plains body, GW x GH — same fixture shape as logistics_reach_harness,
/// so terrain is never the reason a placement is refused.
struct fixture
{
    world                  w;
    entity_id              body = null_entity;
    std::vector<entity_id> grid; // raster: row * GW + col
};

fixture make_body()
{
    fixture f;
    f.body = f.w.create_entity();
    body_component bc;
    bc.grid_width  = GW;
    bc.grid_height = GH;
    f.w.bodies.emplace(f.body, bc);

    f.grid.assign(static_cast<std::size_t>(GW) * GH, null_entity);
    for (int r = 0; r < GH; ++r)
        for (int c = 0; c < GW; ++c)
        {
            const entity_id t = f.w.create_entity();
            tile_component tc;
            tc.body      = f.body;
            tc.grid_x    = c;
            tc.grid_y    = r;
            tc.substrate = terrain_substrate::sedimentary;
            tc.cover     = terrain_cover::grass;
            tc.cover_density = 150;
            tc.landform  = terrain_landform::plains;
            tc.habitability = 0.8f;
            tc.resource_deposit[static_cast<std::size_t>(resource_type::iron_ore)]   = 100.0f;
            tc.resource_remaining[static_cast<std::size_t>(resource_type::iron_ore)] = 40000.0f;
            f.w.tiles.emplace(t, tc);
            f.grid[static_cast<std::size_t>(r) * GW + c] = t;
        }
    return f;
}

entity_id at(const fixture& f, int c, int r)
{
    return f.grid[static_cast<std::size_t>(r) * GW + c];
}

/// Anchor a population centre of @p scale on (c, r).
entity_id add_centre(fixture& f, int c, int r, int scale)
{
    const entity_id pc = f.w.create_entity();
    population_centre_component pcc;
    pcc.scale      = scale;
    pcc.population = scale * 10;
    f.w.population_centres.emplace(pc, pcc);
    f.w.population_centre_tile.emplace(pc, at(f, c, r));
    return pc;
}

using placement_rules::can_place_in_world;
using placement_rules::placement_reason;

// The authored gates under test — the same shapes scripts/economy.lua and
// scripts/recipes.lua author for the real game.
placement_gate university_gate()
{
    placement_gate g;
    g.requires_centre  = true;
    g.min_centre_scale = 4; // City on the Outpost(1)..Metropolis(5) ladder
    return g;
}

placement_gate schooling_gate()
{
    placement_gate g;
    g.requires_centre = true;
    return g;
}

placement_gate mill_gate()
{
    placement_gate g;
    g.centre_proximity_radius = 6;
    return g;
}

/// One full pass of the behavioural rows against a fresh fixture, returning
/// every reason observed — S10 compares two passes for determinism.
std::vector<placement_reason> behavioural_pass()
{
    std::vector<placement_reason> seen;
    fixture f = make_body();
    add_centre(f, 3, 3, 3);  // a Town
    add_centre(f, 10, 3, 4); // a City

    const auto probe = [&](int c, int r, building_type bt, placement_gate g) {
        seen.push_back(can_place_in_world(f.w, at(f, c, r), bt,
                                          resource_type::iron_ore,
                                          -1.0f, null_entity, g).reason);
    };
    probe(0, 0,  building_type::university, university_gate()); // open land
    probe(3, 3,  building_type::university, university_gate()); // the Town
    probe(10, 3, building_type::university, university_gate()); // the City
    probe(0, 0,  building_type::schooling,  schooling_gate());  // open land
    probe(3, 3,  building_type::schooling,  schooling_gate());  // the Town
    probe(3, 0,  building_type::processing_facility, mill_gate());  // 3 rows off the Town
    probe(10, 7, building_type::processing_facility, mill_gate());  // 4 rows off the City
    return seen;
}

} // namespace

int main()
{
    // --- S1-S3: the university ladder --------------------------------------
    {
        fixture f = make_body();
        add_centre(f, 3, 3, 3);  // Town
        add_centre(f, 10, 3, 4); // City

        const auto open_land = can_place_in_world(f.w, at(f, 0, 0),
            building_type::university, resource_type::iron_ore, -1.0f, null_entity,
            university_gate());
        check(!open_land.ok() && open_land.reason == placement_reason::needs_centre,
              "S1 a university on open land is refused: needs_centre");

        const auto on_town = can_place_in_world(f.w, at(f, 3, 3),
            building_type::university, resource_type::iron_ore, -1.0f, null_entity,
            university_gate());
        check(!on_town.ok() && on_town.reason == placement_reason::centre_too_small,
              "S2 a university on a Town (scale 3) is refused: centre_too_small (distinct code)");

        const auto on_city = can_place_in_world(f.w, at(f, 10, 3),
            building_type::university, resource_type::iron_ore, -1.0f, null_entity,
            university_gate());
        check(on_city.ok(), "S3 a university on a City (scale 4) centre places");
    }

    // --- S4: schooling needs any centre ------------------------------------
    {
        fixture f = make_body();
        add_centre(f, 5, 5, 1); // an Outpost — the smallest stratum

        const auto off = can_place_in_world(f.w, at(f, 0, 0),
            building_type::schooling, resource_type::iron_ore, -1.0f, null_entity,
            schooling_gate());
        check(!off.ok() && off.reason == placement_reason::needs_centre,
              "S4 a schooling building outside any centre is refused: needs_centre");

        const auto on = can_place_in_world(f.w, at(f, 5, 5),
            building_type::schooling, resource_type::iron_ore, -1.0f, null_entity,
            schooling_gate());
        check(on.ok(), "S4 a schooling building on the smallest centre (scale 1) places");
    }

    // --- S5: the heavy processor radius ------------------------------------
    {
        fixture f = make_body();
        add_centre(f, 5, 3, 2); // a Settlement; any centre counts for proximity

        // (13, 3): dx 8 the short way (min(8, 12)) -> 64 > 36, beyond radius 6.
        const auto far_off = can_place_in_world(f.w, at(f, 13, 3),
            building_type::processing_facility, resource_type::iron_ore, -1.0f,
            null_entity, mill_gate());
        check(!far_off.ok() && far_off.reason == placement_reason::far_from_centre,
              "S5 a heavy processor beyond its radius is refused: far_from_centre");

        // (10, 3): dx 5 -> 25 <= 36, within radius 6.
        const auto near_ok = can_place_in_world(f.w, at(f, 10, 3),
            building_type::processing_facility, resource_type::iron_ore, -1.0f,
            null_entity, mill_gate());
        check(near_ok.ok(), "S5 a heavy processor within its radius places");

        // Exactly AT the radius: (11, 3), dx 6 -> 36 <= 36.
        const auto edge = can_place_in_world(f.w, at(f, 11, 3),
            building_type::processing_facility, resource_type::iron_ore, -1.0f,
            null_entity, mill_gate());
        check(edge.ok(), "S5 exactly at the radius places (the bound is inclusive)");
    }

    // --- S6: the radius wraps the cylinder ----------------------------------
    {
        fixture f = make_body();
        add_centre(f, 0, 3, 2);

        // (18, 3): unwrapped dx 18, wrapped dx 2 — the short way round passes.
        const auto wrapped = can_place_in_world(f.w, at(f, 18, 3),
            building_type::processing_facility, resource_type::iron_ore, -1.0f,
            null_entity, mill_gate());
        check(wrapped.ok(), "S6 the proximity radius wraps the east-west cylinder");
    }

    // --- S7: the default gate gates nothing ---------------------------------
    {
        fixture f = make_body(); // no centres anywhere

        const auto uni = can_place_in_world(f.w, at(f, 0, 0),
            building_type::university, resource_type::iron_ore);
        const auto mill = can_place_in_world(f.w, at(f, 13, 3),
            building_type::processing_facility, resource_type::iron_ore);
        check(uni.ok() && mill.ok(),
              "S7 the default (empty) gate gates nothing - existing call sites unchanged");
    }

    // --- S8: the registry resolves the gate per named building --------------
    {
        recipe_registry reg;
        { building_economics uni; uni.gate = university_gate();
          reg.set_economics(building_type::university, uni); }

        recipe mill;
        mill.name = "steel";
        mill.inputs[static_cast<std::size_t>(resource_type::iron_ore)] = 2.0f;
        mill.outputs[static_cast<std::size_t>(resource_type::steel)]   = 1.0f;
        mill.centre_proximity_radius = 6;
        const uint16_t mill_id = reg.add_recipe(mill);

        recipe light;
        light.name = "food_rations";
        light.inputs[static_cast<std::size_t>(resource_type::agricultural_produce)] = 2.0f;
        light.outputs[static_cast<std::size_t>(resource_type::food_rations)]        = 1.0f;
        const uint16_t light_id = reg.add_recipe(light);

        const placement_gate gu = reg.placement_gate_for(building_type::university, no_recipe);
        check(gu.requires_centre && gu.min_centre_scale == 4,
              "S8 placement_gate_for returns the university type's authored gate");

        const placement_gate gm = reg.placement_gate_for(building_type::processing_facility, mill_id);
        check(gm.centre_proximity_radius == 6,
              "S8 a processing recipe's own radius overrides the type's (the heavy class)");

        const placement_gate gl = reg.placement_gate_for(building_type::processing_facility, light_id);
        check(!gl.requires_centre && gl.min_centre_scale == 0 && gl.centre_proximity_radius == 0,
              "S8 a light recipe on the same type stays ungated");
    }

    // --- S9: the authoritative seam enforces it ------------------------------
    {
        fixture f = make_body();
        add_centre(f, 3, 3, 3);  // Town
        add_centre(f, 10, 3, 4); // City

        recipe_registry reg;
        { building_economics uni; uni.build_cost = 10.0f; uni.build_duration_ticks = 0.0f;
          uni.gate = university_gate();
          reg.set_economics(building_type::university, uni); }

        const entity_id corp = f.w.create_entity();
        corporation_component cc; cc.balance = 1000.0f;
        f.w.corporations.emplace(corp, cc);

        entity_id built = null_entity;
        const construction_result on_town = construct_building(f.w, reg, corp, at(f, 3, 3),
            building_type::university, resource_type::iron_ore, built);
        check(on_town == construction_result::invalid_tile && built == null_entity,
              "S9 construct_building refuses a university below a City (no caller passes the gate by hand)");

        const construction_result on_city = construct_building(f.w, reg, corp, at(f, 10, 3),
            building_type::university, resource_type::iron_ore, built);
        check(on_city == construction_result::placed && built != null_entity,
              "S9 construct_building places a university on a City centre");
    }

    // --- S10: deterministic ---------------------------------------------------
    {
        const std::vector<placement_reason> first  = behavioural_pass();
        const std::vector<placement_reason> second = behavioural_pass();
        check(!first.empty() && first == second,
              "S10 two identically-built worlds answer every row identically");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
