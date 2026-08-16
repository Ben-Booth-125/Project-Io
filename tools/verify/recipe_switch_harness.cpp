// recipe_switch_harness — BL-430 (alternate production methods).
//
// TWO INDEPENDENT THINGS, one file because they are two facets of the same
// item: alternates are only a real design choice if (a) switching between them
// costs something, and (b) neither sibling quietly dominates the other.
//
//   S1-S5  MECHANISM. try_switch_recipe / corp_command's set_recipe verb over a
//          hand-built world: cost debited, cooldown started and enforced,
//          insufficient funds refused, an unknown/no-op recipe refused, and the
//          BL-079 reflex switch (economy_system.cpp's floored-recipe rescue,
//          which does NOT go through try_switch_recipe) staying free/instant —
//          the one behaviour this item must NOT change.
//   R1     THE NO-DOMINANCE GUARD — MOVED to tools/verify/chain_depth.cpp's R2
//          row (BL-432, 2026-08-16), and deliberately not kept in both places.
//
//          It lived here and failed on four real authored pairs (NR-243): steel,
//          propellant, charcoal and trade_goods. Ben's call, 2026-08-16, was to
//          settle the tier-vs-alternate axis BEFORE retuning anything — and the
//          axis turned out to be already written in scripts/recipes.lua's own
//          comments (ids 22 and 23): distinct raws feeding a shared good is an
//          ordinary multi-producer fact, NOT an alternate METHOD. This guard's
//          grouping — (primary output, era) — could not see that distinction, so
//          it was comparing supply routes on price when which route a corp runs
//          is decided by deposit access. All four pairs were false positives of
//          the grouping, not balance defects.
//
//          The replacement buckets every sibling pair as a supply route, an
//          explicitly-exempted precondition pair, or a genuine interchangeable
//          method, and only the third is price-compared. Keeping a second copy
//          here would mean maintaining two reference-price tables and two
//          answers to the same question.

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

// --- S1-S5: mechanism, over a hand-built world -----------------------------

entity_id make_processor(world& w, entity_id body, uint16_t recipe)
{
    const entity_id tile = w.create_entity();
    tile_component tc{};
    tc.body = body;
    w.tiles[tile] = tc;

    const entity_id bld = w.create_entity();
    building_component b{};
    b.tile = tile;
    b.type = building_type::processing_facility;
    b.recipe = recipe;
    w.buildings[bld] = b;
    return bld;
}

void run_mechanism_checks()
{
    std::printf("=== S1-S5: recipe-switch cost/cooldown mechanism ===\n\n");

    recipe_registry reg;
    // Two dummy recipes so a "switch to a different one" is meaningful.
    recipe r0; r0.name = "raw_a"; r0.outputs[static_cast<std::size_t>(resource_type::steel)] = 1.0f;
    recipe r1; r1.name = "raw_b"; r1.outputs[static_cast<std::size_t>(resource_type::steel)] = 1.0f;
    const uint16_t id0 = reg.add_recipe(r0);
    const uint16_t id1 = reg.add_recipe(r1);

    recipe_switch_params sw;
    sw.switch_cost    = 10.0f;
    sw.cooldown_ticks  = 3;
    reg.set_recipe_switch(sw);

    world w;
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};

    const entity_id corp = w.create_entity();
    { corporation_component cc; cc.is_player = true; cc.balance = 25.0f; w.corporations[corp] = cc; }
    const entity_id bld = make_processor(w, body, id0);
    w.corporations[corp].assets.push_back(bld);

    // S1 — a successful switch debits the cost and starts the cooldown.
    {
        building_component& b = w.buildings.at(bld);
        const recipe_switch_result r = try_switch_recipe(w, reg, corp, b, id1);
        check(r == recipe_switch_result::applied, "S1a a fundable switch applies");
        check(b.recipe == id1, "S1b the building's recipe actually changed");
        check(w.corporations[corp].balance == 15.0f, "S1c the switch cost (10) was debited");
        check(b.recipe_switch_cooldown == 3, "S1d the cooldown was set from recipe_switch_params");
    }

    // S2 — while on cooldown, a further switch (even a fundable one) is refused
    // and mutates nothing.
    {
        building_component& b = w.buildings.at(bld);
        const float before = w.corporations[corp].balance;
        const recipe_switch_result r = try_switch_recipe(w, reg, corp, b, id0);
        check(r == recipe_switch_result::on_cooldown, "S2a a switch mid-cooldown is refused");
        check(b.recipe == id1, "S2b ...and the recipe did not change");
        check(w.corporations[corp].balance == before, "S2c ...and nothing was debited");
    }

    // S3 — the cooldown decrements one per economy tick (run_economy_step),
    // and a switch through corp_command succeeds again once it reaches zero.
    {
        for (int t = 0; t < 3; ++t)
            run_economy_step(w, reg);
        check(w.buildings.at(bld).recipe_switch_cooldown == 0,
              "S3a three economy ticks clear a 3-tick cooldown");

        corp_command cmd;
        cmd.corp = corp; cmd.verb = corp_verb::set_recipe; cmd.subject = bld; cmd.recipe = id0;
        const corp_command_result r = apply_corp_command(w, reg, cmd);
        check(r == corp_command_result::applied,
              "S3b corp_command's set_recipe verb succeeds once the cooldown has cleared");
        check(w.buildings.at(bld).recipe == id0, "S3c ...and actually switched the recipe");
    }

    // S4 — insufficient funds refuses the switch (corp_command surfaces
    // rejected_funds; the mutation-free guarantee is checked directly).
    {
        building_component& b = w.buildings.at(bld);
        b.recipe_switch_cooldown = 0; // S3b's own switch left this on cooldown; clear it so this
                                      // check isolates the FUNDS leg, not the cooldown leg.
        w.corporations[corp].balance = 2.0f; // below the 10cr switch cost
        const recipe_switch_result r = try_switch_recipe(w, reg, corp, b, id1);
        check(r == recipe_switch_result::insufficient_funds, "S4a a corp that cannot afford it is refused");
        check(b.recipe == id0, "S4b ...and the recipe did not change");
        check(w.corporations[corp].balance == 2.0f, "S4c ...and the balance is untouched");
    }

    // S5 — the BL-079 reflex switch stays free/instant: it mutates b.recipe
    // directly (economy_system.cpp's floored-output rescue), never touching
    // recipe_switch_cooldown or the corp's balance. Simulated here at the
    // level this harness can reach — the field itself, not the reflex's full
    // decision logic — since that logic needs a live market context this
    // fixture does not build.
    {
        w.corporations[corp].balance = 100.0f;
        building_component& b = w.buildings.at(bld);
        b.recipe_switch_cooldown = 0;
        const float before_balance = w.corporations[corp].balance;
        b.recipe = id1; // the reflex's own idiom: direct assignment, no seam call
        check(b.recipe_switch_cooldown == 0,
              "S5 a direct (reflex-style) recipe assignment leaves the cooldown untouched");
        check(w.corporations[corp].balance == before_balance,
              "...and leaves the corp's balance untouched (free/instant, as designed)");
    }

    // S6 — BL-434 retraction (2026-08-16, same session BL-434 landed): cross-GROUP
    // switching is now REFUSED outright, not priced at a steep multiplier (the
    // mechanism S6 originally checked). Same three dummy recipes (Group A x2,
    // Group B x1), isolating the group lookup from the real authored roster —
    // this checks the MECHANISM; R1 above is the real-roster sanity check
    // (unaffected by this retraction, since it never priced group switching).
    {
        recipe rA1; rA1.name = "groupA_1"; rA1.group = "Group A";
        rA1.outputs[static_cast<std::size_t>(resource_type::steel)] = 1.0f;
        recipe rA2; rA2.name = "groupA_2"; rA2.group = "Group A";
        rA2.outputs[static_cast<std::size_t>(resource_type::steel)] = 1.0f;
        recipe rB1; rB1.name = "groupB_1"; rB1.group = "Group B";
        rB1.outputs[static_cast<std::size_t>(resource_type::steel)] = 1.0f;
        const uint16_t idA1 = reg.add_recipe(rA1);
        const uint16_t idA2 = reg.add_recipe(rA2);
        const uint16_t idB1 = reg.add_recipe(rB1);

        const entity_id bld6 = make_processor(w, body, idA1);
        w.corporations[corp].assets.push_back(bld6);
        w.corporations[corp].balance = 1000.0f;

        building_component& b6 = w.buildings.at(bld6);
        const recipe_switch_result r6a = try_switch_recipe(w, reg, corp, b6, idA2); // intra-group
        const float intra_cost = 1000.0f - w.corporations[corp].balance;
        check(r6a == recipe_switch_result::applied, "S6a intra-group switch applies");
        check(b6.recipe == idA2, "S6b ...and the recipe actually changed");
        check(intra_cost == sw.switch_cost,
              "S6c intra-group switch costs exactly the base switch_cost (" +
                  std::to_string(intra_cost) + ")");

        b6.recipe_switch_cooldown = 0; // clear S6's own cooldown so the next switch is testable
        const float before_cross = w.corporations[corp].balance;
        const recipe_switch_result r6b = try_switch_recipe(w, reg, corp, b6, idB1); // cross-group
        const float cross_cost = before_cross - w.corporations[corp].balance;
        check(r6b == recipe_switch_result::cross_group, "S6d cross-group switch is refused outright");
        check(b6.recipe == idA2, "S6e ...and the recipe did NOT change");
        check(cross_cost == 0.0f, "S6f ...and nothing was debited (a rejection mutates nothing)");
    }
}

} // namespace

int main()
{
    run_mechanism_checks();

    std::printf("\n=== %s (%d failure%s) ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
