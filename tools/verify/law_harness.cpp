// ---------------------------------------------------------------------------
// Headless law harness (BL-343; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises the laws MVP end to end: src/world/law.{hpp,cpp} (the record and the
// per-tick resolution) and the enforcement seam in budget_system.cpp.
//
// The bar BL-343 set is "one law you can turn on and see", so the assertions are
// about OBSERVABILITY and about the seam holding, not about a taxonomy:
//
//   L1  An un-enacted law changes nothing — the shipped default is inert, and a
//       levy that charged from turn one would be a balance change smuggled in
//       as a feature.
//   L2  An enacted levy reduces net margin by EXACTLY rate x units extracted,
//       and lands on its own `levies` line rather than being folded into an
//       existing flow. `corp_budget::net()` still equals the balance delta.
//   L3  No other flow is perturbed — income, expenditure, maintenance and wages
//       are identical with and without the levy. This is the "modifier over the
//       market, never an override of it" rule, asserted: the market prices the
//       output exactly as before.
//   L4  Repeal restores the baseline EXACTLY (bit-identical), so the law is a
//       reversible modifier and not a one-way ratchet on some stored state.
//   L5  Two identical worlds with the same enacted set produce identical
//       balances — the determinism invariant across the new seam.
//   L6  Scoping works: a levy scoped to one resource charges only extraction of
//       that resource; two enacted levies stack additively.
//   L7  A law with an unsatisfied condition_set does not charge, and starts
//       charging once the condition is met — the BL-342 seam, live.
//   L8  Processing output is NOT levied. The extraction levy is a levy on RAW
//       output; charging the refined good too would tax the same ore twice.
//
// The process exits non-zero if any assertion FAILs.

#include "world/budget_system.hpp"
#include "world/economy_system.hpp"
#include "world/law.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

/// A world with one corp, one body, and two extraction sites whose reports are
/// hand-built. The budget seam reads `economy_report::buildings`, so the harness
/// supplies that directly rather than running a whole economy step — the seam is
/// what is under test, not extraction.
struct fixture
{
    world     w;
    entity_id corp = null_entity;
    entity_id body = null_entity;
    std::vector<building_report> production;
};

entity_id add_extraction(world& w, entity_id corp, entity_id body)
{
    const entity_id tile = w.create_entity();
    tile_component  tc{};
    tc.body = body;
    w.tiles[tile] = tc;

    const entity_id b = w.create_entity();
    building_component bc{};
    bc.type              = building_type::extraction_site;
    bc.tile              = tile;
    bc.workforce_target  = 0.0f; // no opex noise: the levy is the only variable
    bc.workforce_assigned = 0.0f;
    w.buildings[b] = bc;
    w.corporations[corp].assets.push_back(b);
    return b;
}

fixture make_fixture()
{
    fixture f;
    f.body = f.w.create_entity();
    f.w.bodies[f.body] = body_component{};

    f.corp = f.w.create_entity();
    corporation_component cc{};
    cc.name      = "Levy Test Corp";
    cc.balance   = 10000.0f;
    cc.is_player = true;
    f.w.corporations[f.corp] = cc;
    f.w.player_entity        = f.corp;

    const entity_id a = add_extraction(f.w, f.corp, f.body);
    const entity_id b = add_extraction(f.w, f.corp, f.body);

    building_report ra;
    ra.building        = a;
    ra.corp            = f.corp;
    ra.body            = f.body;
    ra.type            = building_type::extraction_site;
    ra.target_resource = resource_type::iron_ore;
    ra.active          = true;
    ra.output_quantity = 30.0f;

    building_report rb = ra;
    rb.building        = b;
    rb.target_resource = resource_type::copper_ore;
    rb.output_quantity = 20.0f;

    f.production = {ra, rb};
    return f;
}

/// Run one budget step and return the player's breakdown.
corp_budget run(fixture& f, const recipe_registry& reg, bool with_production)
{
    const std::unordered_map<entity_id, corp_cash_flow>    flows;      // no market flows
    const std::map<std::pair<entity_id, entity_id>, float> contention; // uncontended
    std::map<entity_id, corp_budget>                       breakdown;
    apply_budget(f.w, reg, flows, contention, &breakdown,
                 with_production ? &f.production : nullptr);
    return breakdown[f.corp];
}

/// Append the prototype levy, enacted, at `rate` over `scope` (-1 = all).
void enact_levy(world& w, float rate, int scope = law::all_resources)
{
    law l;
    l.id      = "LAW-TEST-LEVY";
    l.name    = "Test Levy";
    l.effect  = law_effect_kind::extraction_levy;
    l.rate    = rate;
    l.scope_resource = scope;
    l.enacted = true;
    w.laws.push_back(l);
}

bool same(float a, float b) { return a == b; } // bit-identical, deliberately not epsilon

} // namespace

int main()
{
    std::printf("=== law harness (BL-343) ===\n\n");

    recipe_registry reg; // default-constructed: the economics constants the seam reads

    // Total raw output in the fixture: 30 iron ore + 20 copper ore = 50 units.
    constexpr float k_total_output = 50.0f;

    // --- baseline: no laws at all ------------------------------------------
    fixture base = make_fixture();
    const corp_budget b0 = run(base, reg, /*with_production=*/true);
    const float base_balance = base.w.corporations[base.corp].balance;

    // --- L1: an un-enacted law is inert ------------------------------------
    std::printf("-- L1  un-enacted law --\n");
    {
        fixture f = make_fixture();
        seed_prototype_laws(f.w, 2.0f); // the shipped seed: UN-enacted
        check(!f.w.laws.empty() && !f.w.laws.front().enacted,
              "L1a seed_prototype_laws ships the levy un-enacted");
        const corp_budget b = run(f, reg, true);
        check(b.levies == 0.0f, "L1b an un-enacted law charges nothing");
        check(same(f.w.corporations[f.corp].balance, base_balance),
              "L1c the balance is bit-identical to a world with no laws at all");
    }

    // --- L2: an enacted levy charges exactly rate x units -------------------
    std::printf("\n-- L2  enacted levy --\n");
    {
        fixture f = make_fixture();
        enact_levy(f.w, 2.0f);
        const corp_budget b = run(f, reg, true);
        check(same(b.levies, 2.0f * k_total_output),
              "L2a levies == rate x units extracted (2 x 50 = 100)");
        check(same(f.w.corporations[f.corp].balance, base_balance - b.levies),
              "L2b the balance fell by exactly the levy");
        // net() must still describe the delta it claims to describe.
        check(same(b.net(), b0.net() - b.levies),
              "L2c corp_budget::net() accounts the new line");
    }

    // --- L3: no other flow is perturbed ------------------------------------
    std::printf("\n-- L3  the market is untouched --\n");
    {
        fixture f = make_fixture();
        enact_levy(f.w, 2.0f);
        const corp_budget b = run(f, reg, true);
        check(same(b.income, b0.income),           "L3a income unchanged");
        check(same(b.expenditure, b0.expenditure), "L3b expenditure unchanged");
        check(same(b.maintenance, b0.maintenance), "L3c maintenance unchanged");
        check(same(b.wages, b0.wages),             "L3d wages unchanged");
        check(b.levies > 0.0f,                     "L3e ...and the levy is its own line");
    }

    // --- L4: repeal restores the baseline exactly --------------------------
    std::printf("\n-- L4  repeal --\n");
    {
        fixture f = make_fixture();
        enact_levy(f.w, 2.0f);
        run(f, reg, true);                    // charged
        f.w.laws.front().enacted = false;     // repealed
        f.w.corporations[f.corp].balance = 10000.0f; // reset to the opening balance
        const corp_budget b = run(f, reg, true);
        check(b.levies == 0.0f, "L4a a repealed law charges nothing");
        check(same(f.w.corporations[f.corp].balance, base_balance),
              "L4b the balance returns to the bit-identical baseline");
    }

    // --- L5: determinism ----------------------------------------------------
    std::printf("\n-- L5  determinism --\n");
    {
        fixture g = make_fixture(); enact_levy(g.w, 1.5f);
        fixture h = make_fixture(); enact_levy(h.w, 1.5f);
        const corp_budget bg = run(g, reg, true);
        const corp_budget bh = run(h, reg, true);
        check(same(bg.levies, bh.levies), "L5a two identical worlds charge identically");
        check(same(g.w.corporations[g.corp].balance, h.w.corporations[h.corp].balance),
              "L5b ...and their balances are bit-identical");
    }

    // --- L6: scope and stacking --------------------------------------------
    std::printf("\n-- L6  scope and stacking --\n");
    {
        fixture f = make_fixture();
        enact_levy(f.w, 3.0f, static_cast<int>(resource_type::iron_ore));
        const corp_budget b = run(f, reg, true);
        check(same(b.levies, 3.0f * 30.0f),
              "L6a a resource-scoped levy charges only that resource's output (3 x 30)");

        fixture g = make_fixture();
        enact_levy(g.w, 3.0f, static_cast<int>(resource_type::iron_ore));
        enact_levy(g.w, 1.0f); // all resources
        const corp_budget bg = run(g, reg, true);
        check(same(bg.levies, 3.0f * 30.0f + 1.0f * k_total_output),
              "L6b two enacted levies stack additively (90 + 50 = 140)");
    }

    // --- L7: the condition_set seam, live ----------------------------------
    std::printf("\n-- L7  conditional law (BL-342 seam) --\n");
    {
        fixture f = make_fixture();
        law l;
        l.id      = "LAW-COND";
        l.name    = "Conditional Levy";
        l.rate    = 2.0f;
        l.enacted = true;
        condition c;
        c.subject    = condition_subject::structure;
        c.structure  = building_type::port;
        c.comparator = condition_comparator::at_least;
        c.operand    = 1.0f;            // the fixture corp owns no port
        l.conditions.all.push_back(c);
        f.w.laws.push_back(l);

        check(run(f, reg, true).levies == 0.0f,
              "L7a an enacted law with an unmet condition charges nothing");

        // Give the corp a port, and the same law starts charging.
        const entity_id tile = f.w.create_entity();
        tile_component  tc{}; tc.body = f.body; f.w.tiles[tile] = tc;
        const entity_id port = f.w.create_entity();
        building_component bc{}; bc.type = building_type::port; bc.tile = tile;
        bc.workforce_target = 0.0f; bc.workforce_assigned = 0.0f;
        f.w.buildings[port] = bc;
        f.w.corporations[f.corp].assets.push_back(port);

        check(same(run(f, reg, true).levies, 2.0f * k_total_output),
              "L7b ...and charges once the condition is met");
    }

    // --- L8: processing output is not levied -------------------------------
    std::printf("\n-- L8  raw output only --\n");
    {
        fixture f = make_fixture();
        building_report proc;
        proc.building        = f.w.create_entity();
        proc.corp            = f.corp;
        proc.body            = f.body;
        proc.type            = building_type::processing_facility;
        proc.target_resource = resource_type::iron_ore;
        proc.active          = true;
        proc.output_quantity = 1000.0f; // would dominate if it were levied
        f.production.push_back(proc);

        enact_levy(f.w, 2.0f);
        check(same(run(f, reg, true).levies, 2.0f * k_total_output),
              "L8a a processor's output is not levied (the ore was already levied once)");
    }

    // --- the seam's default: no production handed in ------------------------
    std::printf("\n-- default caller --\n");
    {
        fixture f = make_fixture();
        enact_levy(f.w, 2.0f);
        const corp_budget b = run(f, reg, /*with_production=*/false);
        check(b.levies == 0.0f,
              "D1 a caller that omits `production` charges nothing (pre-BL-343 arithmetic)");
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
