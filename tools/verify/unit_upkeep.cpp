// ---------------------------------------------------------------------------
// Headless unit-upkeep + derived-strength harness (BL-454 / BL-459)
// No SDL / Lua / ImGui — builds over the world/* logic alone.
// ---------------------------------------------------------------------------
// Two paired items. BL-454 made a standing force cost something to KEEP: until
// it, `w.units` appeared in economy_system.cpp, budget_system.cpp and
// construction.cpp exactly zero times, so hire_unit debited once and a regiment
// was free forever while every building beside it paid every tick. BL-459 then
// stopped `unit_component::strength` being a stored duplicate of `count` and
// made it derived — count x roster quality x the supply factor BL-454 writes.
// They are one check because one writes the field the other reads.
//
//   U1  The credit charge is exactly N x the type's rate, and NOTHING else in
//       the budget moves. The goods debit is exactly N x each authored good.
//   U2  An EMPTY pool produces decay at the authored scalar, and stock never
//       goes negative.
//   U3  Demolishing the muster base leaves no orphan unit.
//   U4  Two runs of one seed produce identical balances, pools and strengths.
//   U5  AT THE SHIPPED (ZERO) RATES nothing moves at all — no balance, no pool,
//       no supply factor, no pool even created. This is the load-bearing row:
//       it is what lets the economy goldens be re-blessed honestly, because it
//       says the item is inert until someone authors a rate.
//   U6  Strength is derived correctly: two units of equal count and different
//       roster type differ in the right order; an unsupplied unit is strictly
//       weaker than a supplied one of identical composition; and at quality 1.0
//       with full supply, strength == count * 100 exactly.
//   U7  The combat adapter round-trips a live unit into an army_stack_entry the
//       resolver accepts, identically across two runs of one seed.
//
// The process exits non-zero if any assertion FAILs.

#include "world/budget_system.hpp"
#include "world/combat.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <unordered_map>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

constexpr std::size_t ORD = static_cast<std::size_t>(resource_type::ordnance);
constexpr std::size_t RAT = static_cast<std::size_t>(resource_type::food_rations);
constexpr std::size_t IRO = static_cast<std::size_t>(resource_type::iron_ore);

/// Roster indices used throughout. Row 0 is Levy Spear (power_mod 0, so quality
/// is exactly 1.000 — the scale anchor); row 15 is Rifle Regiment (power_mod
/// 380). Asserted below rather than assumed, so a roster edit fails loudly here
/// instead of quietly changing what every other row in this file means.
constexpr uint16_t ROW_LEVY  = 0;
constexpr uint16_t ROW_RIFLE = 15;

/// A minimal world: one body, one corp, one tile carrying a completed
/// military_base, and whatever units the caller adds.
struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id corp = null_entity;
    entity_id tile = null_entity;
    entity_id base = null_entity;
};

entity_id add_tile(world& w, entity_id body)
{
    const entity_id t = w.create_entity();
    tile_component tc{};
    tc.body = body;
    w.tiles[t] = tc;
    return t;
}

fixture make_fixture()
{
    fixture f;
    f.body = f.w.create_entity();
    f.w.bodies[f.body] = body_component{};

    f.corp = f.w.create_entity();
    corporation_component cc{};
    cc.name    = "Test Corp";
    cc.balance = 10000.0f;
    f.w.corporations[f.corp] = cc;
    f.w.player_entity        = f.corp;

    f.tile = add_tile(f.w, f.body);

    f.base = f.w.create_entity();
    building_component bc{};
    bc.type            = building_type::military_base;
    bc.tile            = f.tile;
    bc.ticks_remaining = 0;
    f.w.buildings[f.base] = bc;
    f.w.corporations[f.corp].assets.push_back(f.base);

    return f;
}

entity_id add_unit(fixture& f, uint16_t type, int count, entity_id muster, int supply = 1000)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.owner                  = f.corp;
    uc.position               = f.tile;
    uc.count                  = count;
    uc.type                   = type;
    uc.supply_factor_permille = supply;
    uc.muster_base            = muster;
    f.w.units[u] = uc;
    return u;
}

/// A registry with upkeep authored ON. Everything else stays at its struct
/// default, so nothing but the upkeep rates differs from a bare registry.
recipe_registry registry_with_upkeep(float credits_per_head,
                                     float ordnance_per_head,
                                     int   decay_permille,
                                     int   recovery_permille = 0,
                                     float credits_per_power = 0.0f)
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    mp.upkeep.credits_per_head           = credits_per_head;
    mp.upkeep.credits_per_head_per_power = credits_per_power;
    mp.upkeep.goods_per_head[ORD]        = ordnance_per_head;
    mp.upkeep.supply_decay_permille      = decay_permille;
    mp.upkeep.supply_recovery_permille   = recovery_permille;
    reg.set_military(mp);
    return reg;
}

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// ---------------------------------------------------------------------------

void roster_anchors()
{
    std::printf("\n-- roster anchors (fail loudly if the table moved) --\n");
    const std::vector<roster_row>& t = unit_roster_table();
    check(t.size() > ROW_RIFLE, "roster table still holds row 15");
    if (t.size() <= ROW_RIFLE)
        return;
    check(t[ROW_LEVY].power_mod == 0,
          "row 0 (Levy Spear) still has power_mod 0 — the quality 1.0 anchor");
    check(t[ROW_RIFLE].power_mod == 380,
          "row 15 (Rifle Regiment) still has power_mod 380");
    check(roster_type_quality_permille(ROW_LEVY) == 1000, "quality(row 0) == 1.000");
    check(roster_type_quality_permille(ROW_RIFLE) == 1380, "quality(row 15) == 1.380");
}

// --- U1: the charge is exactly N x the rate, and nothing else moves ---------

void u1_exact_charge()
{
    std::printf("\n-- U1  N units pay exactly N x the rate; nothing else moves --\n");

    fixture f = make_fixture();
    add_unit(f, ROW_LEVY, 10, f.base);
    add_unit(f, ROW_LEVY, 10, f.base);
    add_unit(f, ROW_LEVY, 30, f.base);   // 50 heads in total across three groups

    // 2 credits/head, 0.5 ordnance/head. No decay: the pool is stocked.
    recipe_registry reg = registry_with_upkeep(2.0f, 0.5f, 100);
    f.w.pool_for(f.corp, f.body).quantities[ORD] = 1000.0f;
    f.w.pool_for(f.corp, f.body).quantities[IRO] = 77.0f; // an untouched neighbour

    const float balance_before = f.w.corporations.at(f.corp).balance;

    const unit_upkeep_tick t = run_unit_upkeep(f.w, reg);
    check(t.units == 3 && t.disbanded == 0 && t.unmet == 0,
          "U1a the pass saw three units, disbanded none, met every draw");

    // The goods half: 50 heads x 0.5 = 25 ordnance, and NOTHING else drawn.
    const stockpile_component& pool = f.w.pool_for(f.corp, f.body);
    check(near(pool.quantities[ORD], 975.0f),
          "U1b ordnance debited by exactly N x the authored rate (1000 - 50*0.5)");
    check(near(pool.quantities[IRO], 77.0f),
          "U1c a neighbouring resource in the same pool is untouched");
    check(near(pool.quantities[RAT], 0.0f),
          "U1d an unauthored good is not drawn");

    // The credit half, through apply_budget's own term.
    std::unordered_map<entity_id, corp_cash_flow>          flows;
    std::map<std::pair<entity_id, entity_id>, float>       contention;
    std::map<entity_id, corp_budget>                       breakdown;
    apply_budget(f.w, reg, flows, contention, &breakdown);

    const corp_budget& b = breakdown.at(f.corp);
    check(near(b.upkeep, 100.0f), "U1e upkeep term == 50 heads x 2 credits");
    check(b.maintenance == 0.0f && b.wages == 0.0f && b.income == 0.0f
              && b.expenditure == 0.0f && b.levies == 0.0f,
          "U1f upkeep is its OWN term — no other budget flow moved");
    check(b.force_units == 3 && b.force_unsupplied == 0,
          "U1g the force line counts the units and reports none unsupplied");
    check(near(f.w.corporations.at(f.corp).balance, balance_before - 100.0f),
          "U1h the balance fell by exactly the upkeep and nothing else");
    check(near(b.net(), -100.0f), "U1i net() accounts for the new term");

    // The power-scaled leg: a Rifle Regiment costs more to keep than a levy.
    {
        fixture g = make_fixture();
        add_unit(g, ROW_RIFLE, 10, g.base);
        recipe_registry r2 = registry_with_upkeep(1.0f, 0.0f, 0, 0, 0.01f);
        std::unordered_map<entity_id, corp_cash_flow>    fl;
        std::map<std::pair<entity_id, entity_id>, float> ct;
        std::map<entity_id, corp_budget>                 bd;
        apply_budget(g.w, r2, fl, ct, &bd);
        // 10 heads x (1.0 + 0.01 * 380) = 10 x 4.8 = 48
        check(near(bd.at(g.corp).upkeep, 48.0f),
              "U1j the power-scaled leg charges base + per_power x power_mod");
    }
}

// --- U2: an empty pool decays at the authored scalar, stock never negative --

void u2_shortfall_decay()
{
    std::printf("\n-- U2  empty pool -> decay at the authored scalar, never negative --\n");

    fixture f = make_fixture();
    const entity_id u = add_unit(f, ROW_LEVY, 20, f.base);

    // 1 ordnance/head = 20 needed per tick; the pool holds 5.
    recipe_registry reg = registry_with_upkeep(0.0f, 1.0f, 150);
    f.w.pool_for(f.corp, f.body).quantities[ORD] = 5.0f;

    unit_upkeep_tick t = run_unit_upkeep(f.w, reg);
    check(t.unmet == 1, "U2a a short draw is reported unmet");
    check(f.w.pool_for(f.corp, f.body).quantities[ORD] == 0.0f,
          "U2b the pool took what was there and floored at zero (never negative)");
    check(f.w.units.at(u).supply_factor_permille == 850,
          "U2c supply fell by exactly the authored decay scalar (1000 - 150)");

    // Repeat on a now-empty pool: the decay is linear and the stock stays at 0.
    for (int i = 0; i < 3; ++i)
        run_unit_upkeep(f.w, reg);
    check(f.w.units.at(u).supply_factor_permille == 400,
          "U2d four ticks of unmet draw = four subtractions (1000 - 4*150)");
    check(f.w.pool_for(f.corp, f.body).quantities[ORD] == 0.0f,
          "U2e stock is still exactly zero, never negative");

    // It floors at zero rather than running negative.
    for (int i = 0; i < 10; ++i)
        run_unit_upkeep(f.w, reg);
    check(f.w.units.at(u).supply_factor_permille == 0,
          "U2f the supply factor floors at zero");
    check(unit_strength(f.w, f.w.units.at(u)) == 0,
          "U2g a wholly unsupplied unit has zero derived strength");

    // Restock and the SAME rule recovers it — one rule, symmetric.
    recipe_registry rec = registry_with_upkeep(0.0f, 1.0f, 150, 200);
    f.w.pool_for(f.corp, f.body).quantities[ORD] = 1000.0f;
    run_unit_upkeep(f.w, rec);
    check(f.w.units.at(u).supply_factor_permille == 200,
          "U2h a met draw recovers at the authored rate");
    check(near(f.w.pool_for(f.corp, f.body).quantities[ORD], 980.0f),
          "U2i and the met draw debited the full 20 heads x 1.0");
}

// --- U3: demolishing the muster base leaves no orphan -----------------------

void u3_orphan_cleanup()
{
    std::printf("\n-- U3  demolishing the muster base leaves no orphan unit --\n");

    fixture f = make_fixture();

    // A second base on its own tile, with its own unit, so we can prove the
    // cleanup is targeted rather than a blanket disband.
    const entity_id tile2 = add_tile(f.w, f.body);
    const entity_id base2 = f.w.create_entity();
    {
        building_component bc{};
        bc.type = building_type::military_base;
        bc.tile = tile2;
        f.w.buildings[base2] = bc;
        f.w.corporations[f.corp].assets.push_back(base2);
    }

    const entity_id ua = add_unit(f, ROW_LEVY, 10, f.base);
    const entity_id ub = add_unit(f, ROW_LEVY, 10, base2);
    f.w.units.at(ub).position = tile2;

    // A legacy unit with NO recorded muster base (the hard-coded world's stub):
    // it must survive, or the rule would disband units it was never about.
    const entity_id legacy = add_unit(f, ROW_LEVY, 10, null_entity);

    recipe_registry reg; // shipped (zero) rates — orphan cleanup is unconditional

    run_unit_upkeep(f.w, reg);
    check(f.w.units.size() == 3, "U3a nothing is disbanded while both bases stand");

    const bool demolished = demolish_building(f.w, f.corp, f.base);
    check(demolished, "U3b the muster base was demolished");
    check(f.w.units.count(ua) == 1,
          "U3c demolish_building itself still does not touch w.units (the defect)");

    const unit_upkeep_tick t = run_unit_upkeep(f.w, reg);
    check(t.disbanded == 1, "U3d the unit pass disbanded exactly one orphan");
    check(f.w.units.count(ua) == 0, "U3e the orphaned unit is gone");
    check(f.w.units.count(ub) == 1, "U3f the OTHER base's unit is untouched");
    check(f.w.units.count(legacy) == 1,
          "U3g a unit with no recorded muster base is never orphaned");

    // A unit whose owning corp is gone goes too.
    f.w.corporations.erase(f.corp);
    run_unit_upkeep(f.w, reg);
    check(f.w.units.empty(), "U3h units of an erased corp are disbanded");
}

// --- U4: two runs of one seed are identical ---------------------------------

/// One deterministic scenario, run end to end. Returns a comparable digest.
struct rollout
{
    std::vector<float>   balances;
    std::vector<float>   pool_ordnance;
    std::vector<int64_t> strengths;
    std::vector<int>     stack_counts;
    std::vector<int>     stack_power;
};

rollout run_rollout(int ticks)
{
    fixture f = make_fixture();
    // A deliberately awkward mix: different rows, different counts, one group
    // that will run its pool dry and one that will not.
    add_unit(f, ROW_LEVY,  40, f.base);
    add_unit(f, ROW_RIFLE, 25, f.base);
    add_unit(f, ROW_LEVY,   7, f.base);

    recipe_registry reg = registry_with_upkeep(1.5f, 0.4f, 90, 30, 0.002f);
    f.w.pool_for(f.corp, f.body).quantities[ORD] = 100.0f;

    std::unordered_map<entity_id, corp_cash_flow>    flows;
    std::map<std::pair<entity_id, entity_id>, float> contention;

    rollout r;
    for (int i = 0; i < ticks; ++i)
    {
        run_unit_upkeep(f.w, reg);
        apply_budget(f.w, reg, flows, contention);
        r.balances.push_back(f.w.corporations.at(f.corp).balance);
        r.pool_ordnance.push_back(f.w.pool_for(f.corp, f.body).quantities[ORD]);
    }

    // Strengths and stack entries, in ascending unit id so the digest itself is
    // order-stable regardless of the container's iteration order.
    std::vector<entity_id> ids;
    for (const auto& kv : f.w.units) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    for (const entity_id id : ids)
    {
        const unit_component& u = f.w.units.at(id);
        r.strengths.push_back(unit_strength(f.w, u));
        const army_stack_entry e = unit_to_stack_entry(f.w, u);
        r.stack_counts.push_back(e.count);
        r.stack_power.push_back(e.type_power_mod);
    }
    return r;
}

void u4_determinism()
{
    std::printf("\n-- U4  two runs of one seed: identical balances, pools, strengths --\n");
    const rollout a = run_rollout(12);
    const rollout b = run_rollout(12);
    check(a.balances == b.balances,        "U4a balances identical across two runs");
    check(a.pool_ordnance == b.pool_ordnance, "U4b pools identical across two runs");
    check(a.strengths == b.strengths,      "U4c strengths identical across two runs");
    check(!a.strengths.empty() && a.strengths.size() == 3,
          "U4d the rollout actually fielded three units (the check has teeth)");
    // The scenario has to MOVE, or U4 would pass on a world where nothing ran.
    check(a.balances.front() != a.balances.back(),
          "U4e the balance actually moved over the rollout");
    check(a.pool_ordnance.front() != a.pool_ordnance.back(),
          "U4f the pool actually moved over the rollout");
}

// --- U5: at the shipped (zero) rates, nothing moves at all ------------------

void u5_inert_at_zero()
{
    std::printf("\n-- U5  at the shipped ZERO rates the item is inert --\n");

    recipe_registry reg; // untouched: every upkeep rate at its zero default
    check(reg.military().upkeep.credits_per_head == 0.0f
              && reg.military().upkeep.supply_decay_permille == 0
              && reg.military().upkeep.out_of_supply_reach == 0.0f,
          "U5a the shipped defaults really are zero");

    fixture f = make_fixture();
    const entity_id u1 = add_unit(f, ROW_LEVY,  40, f.base);
    const entity_id u2 = add_unit(f, ROW_RIFLE, 25, f.base);

    const float balance_before = f.w.corporations.at(f.corp).balance;
    const std::size_t pools_before = f.w.corp_body_pools.size();

    std::unordered_map<entity_id, corp_cash_flow>    flows;
    std::map<std::pair<entity_id, entity_id>, float> contention;
    std::map<entity_id, corp_budget>                 breakdown;

    for (int i = 0; i < 25; ++i)
    {
        run_unit_upkeep(f.w, reg);
        apply_budget(f.w, reg, flows, contention, &breakdown);
    }

    check(f.w.corporations.at(f.corp).balance == balance_before,
          "U5b the balance is bit-identical after 25 ticks");
    check(breakdown.at(f.corp).upkeep == 0.0f, "U5c the upkeep term is exactly zero");
    check(breakdown.at(f.corp).net() == 0.0f,  "U5d net() is exactly zero");
    check(f.w.corp_body_pools.size() == pools_before,
          "U5e no pool was even CREATED (an all-zero goods table touches nothing)");
    check(f.w.units.at(u1).supply_factor_permille == 1000
              && f.w.units.at(u2).supply_factor_permille == 1000,
          "U5f no supply factor moved — neither trigger can fire at zero rates");
    check(f.w.units.size() == 2, "U5g nothing was disbanded");
    check(breakdown.at(f.corp).force_units == 2
              && breakdown.at(f.corp).force_unsupplied == 0,
          "U5h the force line still reports the force (observability is not gated)");
}

// --- U6: the derived strength --------------------------------------------

void u6_derived_strength()
{
    std::printf("\n-- U6  strength = count x quality x supply (x100 fixed point) --\n");

    fixture f = make_fixture();
    const entity_id levy  = add_unit(f, ROW_LEVY,  100, f.base);
    const entity_id rifle = add_unit(f, ROW_RIFLE, 100, f.base);
    const entity_id starved = add_unit(f, ROW_RIFLE, 100, f.base, /*supply=*/500);

    const int64_t s_levy    = unit_strength(f.w, f.w.units.at(levy));
    const int64_t s_rifle   = unit_strength(f.w, f.w.units.at(rifle));
    const int64_t s_starved = unit_strength(f.w, f.w.units.at(starved));

    check(s_levy == 10000,
          "U6a scale anchor: quality 1.0, full supply -> strength == count * 100");
    check(s_rifle == 13800, "U6b a Rifle Regiment at quality 1.380 -> 13800");
    check(s_rifle > s_levy,
          "U6c equal headcount, different roster type -> different strength, heavy higher");
    check(s_starved == 6900, "U6d half supply halves it exactly");
    check(s_starved < s_rifle,
          "U6e an unsupplied unit is STRICTLY weaker than a supplied one of identical composition");

    // The duplicate BL-459 exists to kill: strength must not equal count.
    check(s_levy != f.w.units.at(levy).count,
          "U6f strength is no longer a duplicate of count");

    // Zero and negative headcounts are safe.
    unit_component empty{};
    empty.count = 0;
    check(unit_strength(f.w, empty) == 0, "U6g a zero-count unit has zero strength");
    empty.count = -5;
    check(unit_strength(f.w, empty) == 0, "U6h a negative count cannot make strength negative");

    // An out-of-range roster index degrades to the neutral row rather than
    // reading past the table.
    unit_component odd{};
    odd.count = 10;
    odd.type  = 60000;
    check(unit_strength(f.w, odd) == 1000,
          "U6i an unknown roster index resolves at neutral quality, not a crash");
}

// --- U7: the combat adapter -------------------------------------------------

void u7_adapter()
{
    std::printf("\n-- U7  unit_to_stack_entry: the roster meets combat --\n");

    fixture f = make_fixture();
    const entity_id rifle   = add_unit(f, ROW_RIFLE, 100, f.base);
    const entity_id starved = add_unit(f, ROW_RIFLE, 100, f.base, /*supply=*/400);
    const entity_id levy    = add_unit(f, ROW_LEVY,  100, f.base);

    const army_stack_entry er = unit_to_stack_entry(f.w, f.w.units.at(rifle));
    const army_stack_entry es = unit_to_stack_entry(f.w, f.w.units.at(starved));
    const army_stack_entry el = unit_to_stack_entry(f.w, f.w.units.at(levy));

    check(er.count == 100 && er.type_power_mod == 380 && er.type_id == ROW_RIFLE,
          "U7a a fully supplied unit converts to its full headcount and its row's power");
    check(es.count == 40,
          "U7b supply lands in the COUNT (400 permille of 100 heads)");
    check(es.type_power_mod == er.type_power_mod,
          "U7c ... and NOT in the power modifier, which would square it against unit_strength");
    check(el.type_power_mod == 0 && el.cls == unit_class::infantry,
          "U7d a levy converts at neutral power and its own class");

    // The resolver accepts them — the whole point of the adapter existing.
    const std::vector<army_stack_entry> atk = {er, el};
    const std::vector<army_stack_entry> def = {es};
    const doctrine_row                  d{};
    const battle_outcome r1 = resolve_battle(atk, d, def, d,
                                             terrain_composition::rocky,
                                             terrain_landform::plains,
                                             season::summer, 1000, 400);
    const battle_outcome r2 = resolve_battle(atk, d, def, d,
                                             terrain_composition::rocky,
                                             terrain_landform::plains,
                                             season::summer, 1000, 400);
    check(r1.attacker_power > 0 && r1.defender_power > 0,
          "U7e resolve_battle scored both sides from adapter-built stacks");
    check(r1.attacker_power == r2.attacker_power
              && r1.defender_power == r2.defender_power
              && r1.result == r2.result,
          "U7f the resolver is identical across two runs of the same stacks");

    // ... and identically across two independent runs of the same scenario.
    const rollout a = run_rollout(9);
    const rollout b = run_rollout(9);
    check(a.stack_counts == b.stack_counts && a.stack_power == b.stack_power,
          "U7g adapter output is identical across two runs of one seed");
}

} // namespace

int main()
{
    std::printf("=== unit upkeep + derived strength (BL-454 / BL-459) ===\n");

    roster_anchors();
    u1_exact_charge();
    u2_shortfall_decay();
    u3_orphan_cleanup();
    u4_determinism();
    u5_inert_at_zero();
    u6_derived_strength();
    u7_adapter();

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
