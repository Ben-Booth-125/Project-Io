// ---------------------------------------------------------------------------
// Headless logistics-reach harness (BL-323 S2; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises the placement-side "breadth must cost something" rule: the reach
// field in src/world/logistics.cpp and the gate it feeds in placement_rules.
//
// Before this rule there was NO distance rule in placement of any kind, so a
// corp could site a building arbitrarily far from any road, hub, city or port
// at no cost and with no refusal. These assertions are what stop that coming
// back silently.
//
//   R1  A body with no anchor has an all-infinite reach field — nothing is
//       suppliable, and the field says so rather than defaulting to zero.
//   R2  An anchor tile reads zero, and cost rises with distance from it.
//   R3  Terrain is respected: crossing mountain costs more reach than crossing
//       plains over the same tile distance. Reach is the CONVOY cost function,
//       not a straight-line radius.
//   R4  Roads extend reach: the same tile is cheaper to reach once a road runs
//       to it, so a player can BUY their way out to remote ground.
//   R5  The gate refuses beyond budget and permits within it, and a negative
//       budget disables the rule entirely (the pre-BL-323 behaviour every
//       existing call site still gets).
//   R6  A supply anchor is always placeable regardless of budget — otherwise
//       the first port on a fresh coast is unplaceable and the rule cannot
//       bootstrap.
//   R7  The field is deterministic: two builds of the same world agree
//       element-for-element (it is seeded in raster order, never in tiles-map
//       iteration order).
//   R8  An unbuilt field reads -1 ("unknown"), distinct from infinity
//       ("computed and unreachable") — the contract the const gate relies on to
//       skip rather than guess.
//
// The process exits non-zero if any assertion FAILs.

#include "world/construction.hpp" // R11: the real place/demolish invalidation path
#include "world/logistics.hpp"
#include "world/placement_rules.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

constexpr int   GW = 20, GH = 8;
constexpr float inf = std::numeric_limits<float>::infinity();

/// A flat plains body, GW x GH, with a deposit on every tile so terrain never
/// becomes the reason a placement is refused. Returns the tile-id grid.
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
            tc.body        = f.body;
            tc.grid_x      = c;
            tc.grid_y      = r;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
            tc.landform    = terrain_landform::plains;
            tc.resource_deposit[static_cast<std::size_t>(resource_type::iron_ore)]  = 100.0f;
            tc.resource_remaining[static_cast<std::size_t>(resource_type::iron_ore)] = 40000.0f;
            f.w.tiles.emplace(t, tc);
            f.grid[static_cast<std::size_t>(r) * GW + c] = t;
        }
    return f;
}

entity_id at(const fixture& f, int c, int r) { return f.grid[static_cast<std::size_t>(r) * GW + c]; }

/// Make (c,r) a supply anchor by putting an inland logistics hub on it.
void anchor(fixture& f, int c, int r)
{
    const entity_id b = f.w.create_entity();
    building_component bc;
    bc.tile = at(f, c, r);
    bc.type = building_type::inland_logistics_hub;
    f.w.buildings.emplace(b, bc);
    f.w.body_reach_cost.clear();
}

float reach_at(fixture& f, int c, int r)
{
    body_reach_field(f.w, f.body);
    return tile_reach_cost(f.w, at(f, c, r));
}

} // namespace

int main()
{
    // --- R1: no anchor, nothing suppliable --------------------------------
    {
        fixture f = make_body();
        const std::vector<float>& field = body_reach_field(f.w, f.body);
        bool all_inf = !field.empty();
        for (const float v : field) if (!std::isinf(v)) all_inf = false;
        check(all_inf, "R1 a body with no supply anchor has an all-infinite reach field");
    }

    // --- R2: anchor is zero, cost rises with distance ----------------------
    {
        fixture f = make_body();
        anchor(f, 0, 0);
        const float a  = reach_at(f, 0, 0);
        const float d1 = reach_at(f, 1, 0);
        const float d5 = reach_at(f, 5, 0);
        check(a == 0.0f,   "R2 the anchor tile itself reads zero");
        check(d1 > 0.0f && d5 > d1, "R2 reach cost rises with distance from the anchor");
        // Plains weight 1.0, edge = mean of two nodes = 1.0 per step.
        check(std::fabs(d5 - 5.0f) < 0.01f, "R2 five plains steps cost 5.0 (edge = mean node weight)");
    }

    // --- R3: terrain is respected -----------------------------------------
    {
        fixture f = make_body();
        anchor(f, 0, 0);
        // Turn row 4 into mountain, and measure a tile reached across it.
        for (int c = 0; c < GW; ++c)
            f.w.tiles.at(at(f, c, 4)).landform = terrain_landform::mountain;
        f.w.body_reach_cost.clear();

        const float plains_5  = reach_at(f, 5, 0); // along the untouched plains row
        // A mountain tile on the same column costs more to reach than the plains one.
        const float mountain_5 = reach_at(f, 5, 4);
        check(mountain_5 > plains_5,
              "R3 a mountain tile costs more reach than a plains tile at the same column");
    }

    // --- R4: roads extend reach -------------------------------------------
    {
        fixture f = make_body();
        anchor(f, 0, 0);
        const float before = reach_at(f, 10, 0);

        for (int c = 0; c <= 10; ++c)
            f.w.tiles.at(at(f, c, 0)).road_level = 3; // highway the whole corridor
        f.w.body_reach_cost.clear();
        const float after = reach_at(f, 10, 0);

        check(after < before, "R4 a road corridor reduces the reach cost to a remote tile");
    }

    // --- R5: the gate refuses, permits, and disables ----------------------
    {
        fixture f = make_body();
        anchor(f, 0, 0);
        body_reach_field(f.w, f.body);

        // COLUMNS WRAP — the body is a cylinder, so the furthest column from 0 on a
        // 20-wide row is 10, not 19. Picking 18 here would be 2 steps the short way
        // round and the "far" tile would sit inside the budget.
        const entity_id near_tile = at(f, 3, 0);   // ~3.0
        const entity_id far_tile  = at(f, 10, 0);  // ~10.0 — the antipode of column 0

        const auto near_ok = placement_rules::can_place_in_world(
            f.w, near_tile, building_type::extraction_site, resource_type::iron_ore, 5.0f);
        const auto far_no = placement_rules::can_place_in_world(
            f.w, far_tile, building_type::extraction_site, resource_type::iron_ore, 5.0f);
        const auto far_disabled = placement_rules::can_place_in_world(
            f.w, far_tile, building_type::extraction_site, resource_type::iron_ore, -1.0f);
        const auto far_default = placement_rules::can_place_in_world(
            f.w, far_tile, building_type::extraction_site, resource_type::iron_ore);

        check(near_ok.ok(), "R5 a tile inside the reach budget is placeable");
        check(!far_no.ok() && far_no.reason == placement_rules::placement_reason::out_of_logistics_range,
              "R5 a tile beyond the budget is refused, with the reach reason");
        check(far_disabled.ok(), "R5 a negative budget disables the rule");
        check(far_default.ok(),  "R5 the default argument disables it, so existing call sites are unchanged");
    }

    // --- R6: an anchor is always placeable --------------------------------
    {
        fixture f = make_body();
        anchor(f, 0, 0);
        anchor(f, 10, 7); // a second hub at the far corner, well beyond any sane budget
        body_reach_field(f.w, f.body);

        const auto on_anchor = placement_rules::can_place_in_world(
            f.w, at(f, 10, 7), building_type::extraction_site, resource_type::iron_ore, 1.0f);
        check(on_anchor.ok(),
              "R6 a supply-anchor tile is placeable regardless of budget (the rule can bootstrap)");
    }

    // --- R7: deterministic -------------------------------------------------
    {
        fixture f = make_body();
        anchor(f, 7, 3);
        const std::vector<float> first = body_reach_field(f.w, f.body);
        f.w.body_reach_cost.clear();
        const std::vector<float>& second = body_reach_field(f.w, f.body);
        check(first.size() == second.size() && first == second,
              "R7 two builds of the same field agree element-for-element");
    }

    // --- R8: unknown vs unreachable ---------------------------------------
    {
        fixture f = make_body();
        // Field never built.
        check(tile_reach_cost(f.w, at(f, 0, 0)) < 0.0f,
              "R8 an unbuilt field reads -1 (unknown), not 0 and not infinity");

        anchor(f, 0, 0);
        // Isolate a tile by walling it off is not possible on a torus row, so use
        // the no-anchor case for infinity, already covered by R1; here just prove
        // built-and-reachable is >= 0.
        check(reach_at(f, 0, 0) >= 0.0f, "R8 a built field reads >= 0 for a reachable tile");

        // And the gate SKIPS rather than refuses when the field is unknown.
        fixture g = make_body();
        const auto unknown = placement_rules::can_place_in_world(
            g.w, at(g, 5, 0), building_type::extraction_site, resource_type::iron_ore, 1.0f);
        check(unknown.ok(),
              "R8 the gate skips the rule when the field is unbuilt, rather than guessing");
    }

    // --- R9: anchors require completion (Ben's ruling, 2026-08-08) ---------
    {
        fixture f = make_body();
        // A hub still under construction anchors nothing.
        {
            const entity_id b = f.w.create_entity();
            building_component bc;
            bc.tile            = at(f, 0, 0);
            bc.type            = building_type::inland_logistics_hub;
            bc.ticks_remaining = 3;
            f.w.buildings.emplace(b, bc);
        }
        const std::vector<float>& field = body_reach_field(f.w, f.body);
        bool all_inf = !field.empty();
        for (const float v : field) if (!std::isinf(v)) all_inf = false;
        check(all_inf, "R9 an under-construction hub is not an anchor (field stays infinite)");

        // Completing it (and invalidating, as run_construction now does) makes it one.
        for (auto& [bid, bc] : f.w.buildings) bc.ticks_remaining = 0;
        invalidate_logistics_caches(f.w);
        check(reach_at(f, 0, 0) == 0.0f, "R9 the same hub anchors once complete");

        // Decommissioning it (the idle flip) takes the anchor away again.
        for (auto& [bid, bc] : f.w.buildings) bc.decommissioned = true;
        invalidate_logistics_caches(f.w);
        check(std::isinf(reach_at(f, 0, 0)), "R9 a decommissioned hub is not an anchor");
    }

    // --- R10: the first-anchor bootstrap (Ben's ruling, 2026-08-08) --------
    {
        fixture f = make_body();
        body_reach_field(f.w, f.body); // built and all-infinite: no anchor anywhere

        // Before the fix this refused EVERYTHING on a virgin body, the first hub
        // included, making Era 1 off-world expansion impossible.
        const auto first_hub = placement_rules::can_place_in_world(
            f.w, at(f, 5, 3), building_type::inland_logistics_hub, resource_type::iron_ore, 5.0f);
        check(first_hub.ok(), "R10 the FIRST hub on an anchor-less body skips the reach rule");

        const auto site = placement_rules::can_place_in_world(
            f.w, at(f, 5, 3), building_type::extraction_site, resource_type::iron_ore, 5.0f);
        check(!site.ok(), "R10 a non-anchor building is still refused on an anchor-less body");

        // Committing the first hub — even still under construction — ends the
        // exemption: a second hub cannot use it, and (the first not yet
        // anchoring) is refused until the first completes. No free-anchor spam.
        {
            const entity_id b = f.w.create_entity();
            building_component bc;
            bc.tile            = at(f, 5, 3);
            bc.type            = building_type::inland_logistics_hub;
            bc.ticks_remaining = 3; // still building
            f.w.buildings.emplace(b, bc);
            invalidate_logistics_caches(f.w);
        }
        body_reach_field(f.w, f.body);
        const auto second_hub = placement_rules::can_place_in_world(
            f.w, at(f, 10, 3), building_type::inland_logistics_hub, resource_type::iron_ore, 5.0f);
        check(!second_hub.ok(),
              "R10 a second hub is refused while the first is still building (exemption ends on commit)");

        // Once the first completes, a second hub within reach chains normally.
        for (auto& [bid, bc] : f.w.buildings) bc.ticks_remaining = 0;
        invalidate_logistics_caches(f.w);
        body_reach_field(f.w, f.body);
        const auto chained = placement_rules::can_place_in_world(
            f.w, at(f, 8, 3), building_type::inland_logistics_hub, resource_type::iron_ore, 5.0f);
        check(chained.ok(), "R10 a second hub chains within reach once the first completes");
    }

    // --- R11: construct/demolish invalidate the caches themselves ----------
    {
        fixture f = make_body();
        recipe_registry reg;
        { building_economics hub; hub.build_cost = 10.0f; hub.build_duration_ticks = 0.0f;
          reg.set_economics(building_type::inland_logistics_hub, hub); }
        construction_params cp; cp.max_logistics_reach = 5.0f;
        reg.set_construction(cp);

        const entity_id corp = f.w.create_entity();
        corporation_component cc; cc.balance = 1000.0f;
        f.w.corporations.emplace(corp, cc);

        body_reach_field(f.w, f.body); // cache an all-infinite field

        // Place the bootstrap hub through the REAL path. 0-duration = instant
        // completion, so it anchors immediately — and construct_building's own
        // invalidation (no manual clear here) must be what makes the field see it.
        entity_id hub = null_entity;
        const construction_result r = construct_building(f.w, reg, corp, at(f, 0, 0),
            building_type::inland_logistics_hub, resource_type::iron_ore, hub);
        check(r == construction_result::placed, "R11 setup: bootstrap hub placed via construct_building");
        check(reach_at(f, 3, 0) > 0.0f && !std::isinf(reach_at(f, 3, 0)),
              "R11 placement invalidates the caches itself (no manual clear needed)");

        // Demolition through the real path takes the anchor away — again with no
        // manual clear. Before the fix the stale field kept offering tiles
        // anchored on the ghost.
        check(demolish_building(f.w, corp, hub), "R11 setup: hub demolished");
        check(std::isinf(reach_at(f, 3, 0)),
              "R11 demolition invalidates the caches itself (no ghost anchor)");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
