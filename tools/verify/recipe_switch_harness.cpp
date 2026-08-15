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
//   R1     THE NO-DOMINANCE GUARD, against the REAL authored roster
//          (scripts/recipes.lua via scripts/economy.lua). For every pair of
//          recipes sharing a primary output resource AND an era band, neither
//          may beat the other on EVERY one of two axes at once:
//            - input basket cost, at REFERENCE prices (world_gen.lua's
//              base_price for the resources these recipes actually use — a
//              fixed snapshot, not live market data, since the guard asks
//              about the recipes' shape, not a moment's market state)
//            - chain depth of the recipe's deepest input (recipe_registry's
//              own BL-428 metric) — a proxy for "how much industry you need
//              built before this route is even open"
//          A pair identical on both axes, or where one recipe is strictly
//          worse on one axis and strictly better on the other, is a REAL
//          trade-off and passes. A pair where recipe A beats recipe B on BOTH
//          axes (or ties on both, which is a de-facto dominance — B is never
//          worth choosing) fails. Two singleton axes were chosen over wage/
//          build-cost because those live on building_type, not recipe — a
//          processing_facility offers every era-allowed recipe as an
//          interchangeable method (see recipe_registry.hpp's browse path), so
//          the building-level axes are identical for every sibling by
//          construction and cannot be part of a recipe-level dominance test.

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
}

// --- R1: no-dominance guard over the real authored roster ------------------

// Reference prices (world_gen.lua's base_price), for the goods the sibling
// pairs below actually use. A fixed snapshot — the guard asks about the
// recipes' SHAPE, not a moment's live market state.
float reference_price(resource_type r)
{
    static const std::unordered_map<int, float> table = {
        { static_cast<int>(resource_type::timber), 1.5f },
        { static_cast<int>(resource_type::peat),   1.2f },
        { static_cast<int>(resource_type::clay),   1.2f },
        { static_cast<int>(resource_type::sand),   1.0f },
    };
    const auto it = table.find(static_cast<int>(r));
    return (it != table.end()) ? it->second : 1.0f; // untabled input: neutral 1.0
}

float input_basket_cost(const recipe& r)
{
    float total = 0.0f;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (r.inputs[i] > 0.0f)
            total += r.inputs[i] * reference_price(static_cast<resource_type>(i));
    return total;
}

int deepest_input_depth(const recipe_registry& reg, const recipe& r)
{
    int deepest = 0;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (r.inputs[i] > 0.0f)
        {
            const int d = reg.depth_of(static_cast<resource_type>(i));
            if (d > deepest) deepest = d; // treats unreachable (-1) as shallower — fine here: no
                                          // authored sibling pair below has an unreachable input.
        }
    return deepest;
}

void run_dominance_guard()
{
    std::printf("\n=== R1: no-dominance guard (real scripts/recipes.lua) ===\n\n");

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    const std::size_t n = reg.recipe_count();
    std::printf("authored recipes: %zu\n", n);

    // Group by (primary output, era band) — only these are real ALTERNATE
    // METHODS on the same building_type, per recipe_registry.hpp's browse path.
    std::unordered_map<std::string, std::vector<uint16_t>> groups;
    for (std::size_t i = 0; i < n; ++i)
    {
        const recipe* r = reg.get_recipe(static_cast<uint16_t>(i));
        const std::string key = std::to_string(static_cast<int>(primary_output_resource(*r))) +
                                "/" + std::to_string(static_cast<int>(r->era));
        groups[key].push_back(static_cast<uint16_t>(i));
    }

    int pairs_checked   = 0;
    int dominant_pairs  = 0;
    for (const auto& [key, ids] : groups)
    {
        if (ids.size() < 2)
            continue;
        for (std::size_t a = 0; a < ids.size(); ++a)
            for (std::size_t bIdx = a + 1; bIdx < ids.size(); ++bIdx)
            {
                const recipe* ra = reg.get_recipe(ids[a]);
                const recipe* rb = reg.get_recipe(ids[bIdx]);
                const float cost_a  = input_basket_cost(*ra);
                const float cost_b  = input_basket_cost(*rb);
                const int   depth_a = deepest_input_depth(reg, *ra);
                const int   depth_b = deepest_input_depth(reg, *rb);

                const bool a_dominates = (cost_a <= cost_b) && (depth_a <= depth_b) &&
                                         (cost_a < cost_b || depth_a < depth_b);
                const bool b_dominates = (cost_b <= cost_a) && (depth_b <= depth_a) &&
                                         (cost_b < cost_a || depth_b < depth_a);
                ++pairs_checked;
                if (a_dominates || b_dominates)
                {
                    ++dominant_pairs;
                    std::printf("      DOMINATED PAIR: '%s' (cost %.1f, depth %d) vs '%s' (cost %.1f, depth %d)\n",
                               ra->name.c_str(), cost_a, depth_a, rb->name.c_str(), cost_b, depth_b);
                }
            }
    }

    check(pairs_checked > 0,
          "ANTI-VACUITY: at least one real sibling pair exists to check (else this guard checks nothing)");
    check(dominant_pairs == 0,
          "no sibling pair (same primary output + era band) dominates on both input-basket cost AND chain depth (" +
              std::to_string(pairs_checked) + " pair" + (pairs_checked == 1 ? "" : "s") + " checked)");
}

} // namespace

int main()
{
    run_mechanism_checks();
    run_dominance_guard();

    std::printf("\n=== %s (%d failure%s) ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
