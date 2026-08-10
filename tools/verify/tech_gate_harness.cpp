// ---------------------------------------------------------------------------
// Headless tech-gate harness (BL-344; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises the loop BL-344 closed: src/world/tech_gate.{hpp,cpp} — a real
// predicate (BL-342's condition_set), per-corp earned state, and ONE content
// gate that changes what a corporation may do.
//
// Until 2026-08-10 `tech_tree.hpp:49` stored the gate as a descriptive STRING,
// so no tech could be earned and nothing could be unlocked. These assertions are
// what stop that coming back:
//
//   T1  The one authored gate exists, unlocks the MILITARY BASE (BL-094's
//       design test — a technology that can only unlock a building is being
//       designed for the corporate player the pivot is moving away from), and
//       carries a NON-EMPTY predicate. An empty condition_set is TRUE by
//       definition, so an empty gate would earn itself on the first tick.
//   T2  A tech with an unmet gate is not earned, and the gated content is
//       refused at the placement check with the RIGHT reason (`tech_locked`,
//       not a generic invalid_tile).
//   T3  Satisfying the condition earns it — through ordinary play quantities
//       (structures owned + cash balance), not a debug poke.
//   T4  The unlock then PERMITS placement at the same call site that refused
//       it, and `construct_building` agrees with the placement check.
//   T5  Earned state is PER-CORP, not global: a rival earning the tech does not
//       unlock the player's military base.
//   T6  Two identical worlds earn identically (determinism), and earning is
//       monotonic and idempotent — a tech is never un-earned by a later dip
//       below its threshold, and re-running the pass earns nothing twice.
//   T7  An UNGATED type is unaffected, and a caller that names no corp gets the
//       pre-BL-344 behaviour (the default every existing call site still has).
//
// The process exits non-zero if any assertion FAILs.

#include "world/construction.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <string>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

constexpr int GW = 8, GH = 6;

struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id corp = null_entity;
    entity_id free_tile = null_entity; ///< A plains tile with no building on it.
};

entity_id make_tile(world& w, entity_id body, int gx, int gy)
{
    const entity_id t = w.create_entity();
    tile_component  tc{};
    tc.body        = body;
    tc.grid_x      = gx;
    tc.grid_y      = gy;
    tc.composition = terrain_composition::grassland;
    tc.landform    = terrain_landform::plains;
    w.tiles[t] = tc;
    return t;
}

/// A corp with `sites` extraction sites and `balance` credits on a small plains
/// body, plus one free tile a military base could go on.
fixture make_fixture(int sites, float balance)
{
    fixture f;
    f.body = f.w.create_entity();
    body_component bc{};
    bc.grid_width  = GW;
    bc.grid_height = GH;
    f.w.bodies[f.body] = bc;

    f.corp = f.w.create_entity();
    corporation_component cc{};
    cc.name      = "Gate Test Corp";
    cc.balance   = balance;
    cc.is_player = true;
    f.w.corporations[f.corp] = cc;
    f.w.player_entity        = f.corp;

    for (int i = 0; i < sites; ++i)
    {
        const entity_id t = make_tile(f.w, f.body, i, 0);
        const entity_id b = f.w.create_entity();
        building_component bcp{};
        bcp.type = building_type::extraction_site;
        bcp.tile = t;
        f.w.buildings[b] = bcp;
        f.w.corporations[f.corp].assets.push_back(b);
    }

    f.free_tile = make_tile(f.w, f.body, 0, 3);
    return f;
}

/// The placement check as the build front door asks it — reach disabled, so the
/// only thing that can refuse a plains tile here is the tech gate.
placement_rules::placement_result gate_check(const fixture& f, building_type type)
{
    return placement_rules::can_place_in_world(f.w, f.free_tile, type,
                                               resource_type::iron_ore,
                                               /*max_reach=*/-1.0f, f.corp);
}

} // namespace

int main()
{
    std::printf("=== tech gate harness (BL-344) ===\n\n");

    // --- T1: the authored gate ---------------------------------------------
    std::printf("-- T1  the authored gate --\n");
    {
        const tech_gate* g = find_tech_gate("E0-ML-01");
        check(g != nullptr, "T1a E0-ML-01 is in the gate table");
        if (g)
        {
            check(g->unlocks_structure == building_type::military_base,
                  "T1b it unlocks the MILITARY BASE, not another factory (BL-094's test)");
            check(!g->condition.all.empty(),
                  "T1c its predicate is NON-EMPTY (an empty set is true, and would self-earn)");
        }
        check(gating_tech_for(building_type::military_base) == "E0-ML-01",
              "T1d the reverse lookup names the gating tech, so a refusal can say which");
        check(gating_tech_for(building_type::processing_facility).empty(),
              "T1e an ungated type reports no gating tech");
    }

    // --- T2: unmet gate ------------------------------------------------------
    std::printf("\n-- T2  unmet gate --\n");
    {
        fixture f = make_fixture(/*sites=*/1, /*balance=*/100.0f); // both conditions unmet
        check(advance_tech_gates(f.w) == 0, "T2a nothing is earned with the gate unmet");
        check(!f.w.has_tech(f.corp, "E0-ML-01"), "T2b ...and the corp has not earned it");
        check(!structure_unlocked(f.w, f.corp, building_type::military_base),
              "T2c the military base reads as locked");

        const placement_rules::placement_result pr = gate_check(f, building_type::military_base);
        check(!pr, "T2d placement is refused");
        check(pr.reason == placement_rules::placement_reason::tech_locked,
              "T2e ...with the RIGHT reason (tech_locked, not a generic invalid_tile)");
    }

    // --- T3: earned through ordinary play ------------------------------------
    std::printf("\n-- T3  earning it --\n");
    {
        // Two sites but no money: still unmet — the AND-list is doing real work.
        fixture poor = make_fixture(/*sites=*/2, /*balance=*/100.0f);
        advance_tech_gates(poor.w);
        check(!poor.w.has_tech(poor.corp, "E0-ML-01"),
              "T3a two sites alone do not earn it (the balance condition still binds)");

        // Money but one site: also unmet.
        fixture small = make_fixture(/*sites=*/1, /*balance=*/50000.0f);
        advance_tech_gates(small.w);
        check(!small.w.has_tech(small.corp, "E0-ML-01"),
              "T3b money alone does not earn it (the structure condition still binds)");

        fixture f = make_fixture(/*sites=*/2, /*balance=*/5000.0f);
        check(advance_tech_gates(f.w) == 1, "T3c both conditions met earns exactly one tech");
        check(f.w.has_tech(f.corp, "E0-ML-01"), "T3d ...and it is E0-ML-01");
    }

    // --- T4: the unlock permits what it refused ------------------------------
    std::printf("\n-- T4  the unlock --\n");
    {
        fixture f = make_fixture(/*sites=*/2, /*balance=*/50000.0f);

        // The SAME call site, before and after.
        check(gate_check(f, building_type::military_base).reason
                  == placement_rules::placement_reason::tech_locked,
              "T4a before: the placement check refuses");
        advance_tech_gates(f.w);
        check(gate_check(f, building_type::military_base).ok(),
              "T4b after:  the same placement check permits");

        // And the construction front door agrees with the check.
        recipe_registry reg;
        entity_id built = null_entity;
        const construction_result r = construct_building(
            f.w, reg, f.corp, f.free_tile, building_type::military_base,
            resource_type::iron_ore, built);
        check(r == construction_result::placed,
              "T4c construct_building places it once the tech is earned");
        check(built != null_entity && f.w.buildings.count(built) == 1,
              "T4d ...and the building really exists");

        // The mirror: an unearned corp is refused by the front door with the
        // tech reason, not with a generic one.
        fixture g = make_fixture(/*sites=*/1, /*balance=*/50000.0f);
        entity_id b2 = null_entity;
        const construction_result r2 = construct_building(
            g.w, reg, g.corp, g.free_tile, building_type::military_base,
            resource_type::iron_ore, b2);
        check(r2 == construction_result::tech_locked,
              "T4e construct_building refuses with tech_locked when it is not earned");
        check(b2 == null_entity, "T4f ...and places nothing");
    }

    // --- T5: per-corp, not global -------------------------------------------
    std::printf("\n-- T5  per-corp state --\n");
    {
        fixture f = make_fixture(/*sites=*/1, /*balance=*/100.0f); // player: unmet

        // A rival that DOES meet the gate, in the same world.
        const entity_id rival = f.w.create_entity();
        corporation_component rc{};
        rc.name    = "Rival Co";
        rc.balance = 9000.0f;
        f.w.corporations[rival] = rc;
        for (int i = 0; i < 2; ++i)
        {
            const entity_id t = make_tile(f.w, f.body, i, 5);
            const entity_id b = f.w.create_entity();
            building_component bcp{};
            bcp.type = building_type::extraction_site;
            bcp.tile = t;
            f.w.buildings[b] = bcp;
            f.w.corporations[rival].assets.push_back(b);
        }

        advance_tech_gates(f.w);
        check(f.w.has_tech(rival, "E0-ML-01"), "T5a the rival earns it");
        check(!f.w.has_tech(f.corp, "E0-ML-01"), "T5b the player does NOT");
        check(structure_unlocked(f.w, rival, building_type::military_base),
              "T5c the rival may build a military base");
        check(!structure_unlocked(f.w, f.corp, building_type::military_base),
              "T5d ...and the player still may not");
    }

    // --- T6: determinism, monotonicity, idempotence --------------------------
    std::printf("\n-- T6  determinism and monotonicity --\n");
    {
        fixture g = make_fixture(/*sites=*/2, /*balance=*/5000.0f);
        fixture h = make_fixture(/*sites=*/2, /*balance=*/5000.0f);
        const int eg = advance_tech_gates(g.w);
        const int eh = advance_tech_gates(h.w);
        check(eg == eh && eg == 1, "T6a two identical worlds earn identically");
        check(g.w.earned_techs[g.corp] == h.w.earned_techs[h.corp],
              "T6b ...and their earned sets are equal");

        check(advance_tech_gates(g.w) == 0, "T6c re-running the pass earns nothing twice");

        // Monotonic: falling below the threshold does not revoke it.
        g.w.corporations[g.corp].balance = -1000.0f;
        advance_tech_gates(g.w);
        check(g.w.has_tech(g.corp, "E0-ML-01"),
              "T6d a tech is not un-earned by a later dip below its threshold");
        check(structure_unlocked(g.w, g.corp, building_type::military_base),
              "T6e ...so the unlock survives too");
    }

    // --- T7: ungated types and the null-corp default -------------------------
    std::printf("\n-- T7  scope of the gate --\n");
    {
        fixture f = make_fixture(/*sites=*/1, /*balance=*/100.0f); // nothing earned
        check(structure_unlocked(f.w, f.corp, building_type::processing_facility),
              "T7a an ungated type is unaffected by the gate");
        check(gate_check(f, building_type::processing_facility).ok(),
              "T7b ...and its placement is permitted");
        check(structure_unlocked(f.w, null_entity, building_type::military_base),
              "T7c a caller that names no corp gets the pre-BL-344 behaviour");
        const placement_rules::placement_result pr =
            placement_rules::can_place_in_world(f.w, f.free_tile, building_type::military_base,
                                                resource_type::iron_ore, -1.0f);
        check(pr.ok(), "T7d ...including can_place_in_world's default corp argument");
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
