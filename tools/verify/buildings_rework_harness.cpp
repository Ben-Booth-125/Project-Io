// ---------------------------------------------------------------------------
// Headless buildings-rework harness (BL-323 first slice; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Two unrelated things, kept in one harness because both are small:
//
//   R1/R2  The extraction roster (BL-323 S1): every resource added to
//          k_extractable is actually placeable wherever its deposit is > 0,
//          with the existing generic can_place logic untouched.
//   R3     The Smelter's second recipe (iron_nickel_ore -> steel) resolves.
//   R4/R5  Site-dependent build time (BL-323 S3): landform, reach, and stack
//          each move ticks_remaining the direction they should, floor at 1,
//          and 0-duration types stay instant.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/logistics.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdio>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

entity_id make_tile(world& w, entity_id body, int c, int r,
                    terrain_landform lf = terrain_landform::plains)
{
    const entity_id t = w.create_entity();
    tile_component tc;
    tc.body        = body;
    tc.grid_x      = c;
    tc.grid_y      = r;
    tc.composition = terrain_composition::rocky;
    tc.landform    = lf;
    w.tiles.emplace(t, tc);
    return t;
}

} // namespace

int main()
{
    // --- R1/R2: every widened k_extractable target is placeable on its deposit
    {
        world w;
        const entity_id body = w.create_entity();
        body_component bc; bc.grid_width = 4; bc.grid_height = 4;
        w.bodies.emplace(body, bc);

        bool all_ok = true;
        int  n      = 0;
        for (resource_type target : placement_rules::k_extractable)
        {
            const entity_id t = make_tile(w, body, n % 4, n / 4);
            w.tiles.at(t).resource_deposit[ri(target)] = 80.0f;
            const bool ok = placement_rules::can_place(w.tiles.at(t),
                building_type::extraction_site, target).ok();
            all_ok = all_ok && ok;
            ++n;
        }
        check(n >= 15, "R1 k_extractable widened to at least 15 targets");
        check(all_ok, "R1 every k_extractable target is placeable on a >0 deposit of itself");

        // A target with no deposit on the tile is refused (the check stays real).
        const entity_id bare = make_tile(w, body, 0, 3);
        check(!placement_rules::can_place(w.tiles.at(bare),
                  building_type::extraction_site, resource_type::coal).ok(),
              "R2 a target with zero deposit is still refused");
    }

    // --- R3: the Smelter's iron-nickel recipe is distinct from the iron recipe
    {
        recipe_registry reg;
        recipe steel_fe; steel_fe.name = "steel";
        steel_fe.inputs[ri(resource_type::iron_ore)] = 2.0f;
        steel_fe.outputs[ri(resource_type::steel)]   = 1.0f;
        reg.add_recipe(steel_fe);

        recipe steel_feni; steel_feni.name = "steel_from_iron_nickel";
        steel_feni.inputs[ri(resource_type::iron_nickel_ore)] = 2.0f;
        steel_feni.outputs[ri(resource_type::steel)]          = 1.0f;
        const uint16_t feni_id = reg.add_recipe(steel_feni);

        const recipe& r = reg.recipe_at(building_type::processing_facility, feni_id);
        check(r.inputs[ri(resource_type::iron_nickel_ore)] > 0.0f
                  && r.inputs[ri(resource_type::iron_ore)] == 0.0f,
              "R3 the iron-nickel recipe consumes iron-nickel ore, not iron ore");
        check(r.outputs[ri(resource_type::steel)] > 0.0f,
              "R3 the iron-nickel recipe still outputs steel");
    }

    // --- R4/R5: site-dependent build time --------------------------------
    {
        recipe_registry reg;
        building_economics ex; ex.build_cost = 100.0f; ex.build_duration_ticks = 4.0f;
        reg.set_economics(building_type::extraction_site, ex);
        construction_params cp;
        cp.max_logistics_reach     = 24.0f;
        cp.site_time_reach_scale   = 1.0f;
        cp.site_time_stack_discount = 0.15f;
        cp.site_time_stack_min      = 0.5f;
        reg.set_construction(cp);

        world w;
        const entity_id body = w.create_entity();
        constexpr int GW = 6, GH = 2;
        body_component bc; bc.grid_width = GW; bc.grid_height = GH;
        w.bodies.emplace(body, bc);

        // A FULL grid (the A* reach path needs every cell present, not just the
        // ones under test — a gap reads as unreachable, not "skip it").
        entity_id grid[GH][GW];
        for (int r = 0; r < GH; ++r)
            for (int c = 0; c < GW; ++c)
                grid[r][c] = make_tile(w, body, c, r);

        // An anchor at (0,0) so reach is measurable; a plains tile right beside
        // it, a mountain tile at the same distance, and a remote mountain tile.
        const entity_id anchor_t = grid[0][0];
        {
            const entity_id hub = w.create_entity();
            building_component hbc; hbc.tile = anchor_t; hbc.type = building_type::inland_logistics_hub;
            w.buildings.emplace(hub, hbc);
        }
        const entity_id near_plains = grid[0][1];
        w.tiles.at(near_plains).resource_deposit[ri(resource_type::iron_ore)] = 100.0f;
        const entity_id near_mountain = grid[1][1];
        w.tiles.at(near_mountain).landform = terrain_landform::mountain;
        w.tiles.at(near_mountain).resource_deposit[ri(resource_type::iron_ore)] = 100.0f;
        const entity_id remote = grid[1][5];
        w.tiles.at(remote).landform = terrain_landform::mountain;
        w.tiles.at(remote).resource_deposit[ri(resource_type::iron_ore)] = 100.0f;

        const entity_id corp = w.create_entity();
        corporation_component cc; cc.balance = 100000.0f;
        w.corporations.emplace(corp, cc);

        entity_id b1 = null_entity;
        const construction_result r1 = construct_building(w, reg, corp, near_plains,
            building_type::extraction_site, resource_type::iron_ore, b1);
        check(r1 == construction_result::placed, "R4 setup: near-plains build placed");
        const int base_ticks = b1 != null_entity ? w.buildings.at(b1).ticks_remaining : -1;
        check(base_ticks == 4, "R4 a first, anchor-adjacent, plains build takes exactly the base duration");

        entity_id b2 = null_entity;
        const construction_result r2 = construct_building(w, reg, corp, near_mountain,
            building_type::extraction_site, resource_type::iron_ore, b2);
        check(r2 == construction_result::placed, "R4 setup: mountain build placed");
        const int mountain_ticks = b2 != null_entity ? w.buildings.at(b2).ticks_remaining : -1;
        check(mountain_ticks > base_ticks, "R4 a mountain site takes longer than the same-reach plains site");

        // A second extraction site on the SAME tile (near_plains) — the stack
        // discount should make it faster than the base duration.
        entity_id b3 = null_entity;
        const construction_result r3 = construct_building(w, reg, corp, near_plains,
            building_type::extraction_site, resource_type::iron_ore, b3);
        const int stacked_ticks = b3 != null_entity ? w.buildings.at(b3).ticks_remaining : -1;
        check(r3 == construction_result::placed && stacked_ticks > 0 && stacked_ticks <= base_ticks,
              "R4 a second site on an already-built tile takes no longer than the first");
        check(stacked_ticks >= 1, "R5 ticks_remaining never floors to zero for a real-duration type");

        // A 0-duration type stays instant regardless of site: re-economics
        // extraction_site to 0 and build on the fresh, unstacked, remote-mountain tile.
        building_economics instant_ex; instant_ex.build_cost = 100.0f; instant_ex.build_duration_ticks = 0.0f;
        reg.set_economics(building_type::extraction_site, instant_ex);
        entity_id b4 = null_entity;
        const construction_result r4 = construct_building(w, reg, corp, remote,
            building_type::extraction_site, resource_type::iron_ore, b4);
        check(r4 == construction_result::placed && b4 != null_entity
                  && w.buildings.at(b4).ticks_remaining == 0,
              "R5 a 0-duration building type stays instant regardless of site");
    }

    // --- R6/R7 (BL-325 S1): the military base --------------------------------
    {
        recipe_registry reg;
        building_economics base; base.build_cost = 300.0f; base.build_duration_ticks = 4.0f;
        reg.set_economics(building_type::military_base, base);
        construction_params cp; cp.max_logistics_reach = 5.0f;
        reg.set_construction(cp);

        world w;
        const entity_id body = w.create_entity();
        constexpr int GW = 12, GH = 2;
        body_component bcmp; bcmp.grid_width = GW; bcmp.grid_height = GH;
        w.bodies.emplace(body, bcmp);
        entity_id grid[GH][GW];
        for (int r = 0; r < GH; ++r)
            for (int c = 0; c < GW; ++c)
                grid[r][c] = make_tile(w, body, c, r);
        w.tiles.at(grid[1][0]).composition = terrain_composition::ocean;

        // A hub anchor at (0,0), so reach is measurable.
        {
            const entity_id hub = w.create_entity();
            building_component hbc; hbc.tile = grid[0][0]; hbc.type = building_type::inland_logistics_hub;
            w.buildings.emplace(hub, hbc);
        }
        body_reach_field(w, body);

        // Placement: land within reach OK (no deposit needed), ocean refused,
        // beyond-reach refused — the base earns NO anchor exemption.
        check(placement_rules::can_place_in_world(w, grid[0][2],
                  building_type::military_base, resource_type::iron_ore, 5.0f).ok(),
              "R6 a military base is placeable on bare land within reach (no deposit needed)");
        check(!placement_rules::can_place_in_world(w, grid[1][0],
                  building_type::military_base, resource_type::iron_ore, 5.0f).ok(),
              "R6 a military base is refused on ocean");
        // Column 6 is 6 steps away (columns wrap at 12, so the short way is 6).
        check(!placement_rules::can_place_in_world(w, grid[0][6],
                  building_type::military_base, resource_type::iron_ore, 5.0f).ok(),
              "R6 a military base beyond the reach budget is refused (no anchor-type exemption)");

        // Built through the real path: staffs at zero, and — ruling 3 — the
        // completed base is NOT a supply anchor, so the reach field is unchanged.
        const entity_id corp = w.create_entity();
        corporation_component cc; cc.balance = 100000.0f;
        w.corporations.emplace(corp, cc);

        const float reach_before = tile_reach_cost(w, grid[0][4]);
        entity_id built = null_entity;
        const construction_result r = construct_building(w, reg, corp, grid[0][2],
            building_type::military_base, resource_type::iron_ore, built);
        check(r == construction_result::placed && built != null_entity,
              "R6 setup: base placed via construct_building");
        check(built != null_entity && w.buildings.at(built).workforce_assigned == 0.0f,
              "R7 a military base staffs at zero (passive infrastructure)");

        for (auto& [bid, bc2] : w.buildings) bc2.ticks_remaining = 0; // complete it
        invalidate_logistics_caches(w);
        body_reach_field(w, body);
        check(!is_supply_anchor(w, grid[0][2]),
              "R7 a COMPLETED military base is not a supply anchor (BL-325 ruling 3)");
        check(tile_reach_cost(w, grid[0][4]) == reach_before,
              "R7 building a base changes nothing in the reach field");
    }

    if (g_fail)
        std::printf("\nFAILURES: %d (of %d)\n", g_fail, g_pass + g_fail);
    else
        std::printf("\nALL PASS (%d)\n", g_pass);
    return g_fail ? 1 : 0;
}
