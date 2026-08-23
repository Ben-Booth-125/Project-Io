// Headless harness (BL-479): the tech effect union + the modifier vocabulary.
//
// BL-344 closed the tech loop with ONE effect: `unlocks_structure`. This item
// widens the effect side into a closed tagged union {unlock_structure,
// modify_scalar} and authors the modifier-subject vocabulary shared with law
// (BL-480). The requirement rows this harness answers
// (docs/development/req/requirements.json § tech-effect-union):
//
//   R1  A tech carrying modify_scalar, once earned by corp X, moves exactly the
//       named scalar for X and no other corp; a world where no tech carries an
//       effect beyond unlock_structure is bit-identical (state_hash) to the
//       pre-change build.
//   R2  The vocabulary lives in one shared header (modifier_set.hpp), is a
//       closed enum, and names at least one strain/cohesion-adjacent subject.
//   R3  E0-ML-01 behaves identically through the union (tech_gate_harness runs
//       unmodified — that harness is R3's proof; section U2 here covers the
//       union-side mirror it cannot see).
//
// SECTION H is R1's hash half and is deliberately self-contained: it runs the
// REAL generated world (no tech in the shipped table carries modify_scalar)
// through 120 full ticks and prints state_hash at three marks. The cross-BUILD
// comparison — this exact section compiled against the pre-change tree vs this
// tree — is performed in-session when the item lands and recorded in the
// commit; the hashes are NOT baked in here as goldens, because a world-hash
// golden breaks with every world-gen change (the BL-459 lesson) and would
// re-litigate this item forever. What the section asserts standalone is
// two-runs-same-seed bit-identity, the determinism primitive.
//
// SECTIONS U1-U5 are the union/vocabulary half, over hand-built fixtures:
//   U1  op arithmetic + the closed subject roster (append-only positions).
//   U2  the union's unlock arm mirrors `unlocks_structure` on E0-ML-01.
//   U3  a fixture modify_scalar tech lands modifiers for the earning corp
//       only, folds in earn order, and is monotonic (never applied twice).
//   U4  END-TO-END: through the real economy tick, the earning corp's
//       extraction moves by exactly the modifier; the rival's is bit-identical.
//       U4b pins the STACK TAPER pre-pass to the modified rate (the second
//       read site — a taper sized on the unmodified rate fails it).
//   U5  the null-corp / no-modifier reads hand back the same bits.
//
// The process exits non-zero if any assertion FAILs.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp" // assign_default_recipes
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/modifier_set.hpp"
#include "world/recipe_registry.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// ---------------------------------------------------------------------------
// Section H — the no-effect world (R1's hash half)
// ---------------------------------------------------------------------------
// Registry mirrors ai_skill_harness's make_registry (itself the maintained
// hand-built mirror of scripts/economy.lua + recipes.lua), so the rollout has
// real economics. KEEP THIS SECTION BYTE-STABLE: its whole value is that the
// same code compiled against the pre-change tree prints the same hashes.

recipe_registry make_world_registry()
{
    recipe_registry reg;

    building_economics extraction;
    extraction.base_rate            = 20.0f;
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    extraction.resource_build_cost[ri(resource_type::steel)] = 20.0f;
    extraction.richness_reference = 24.9f; // mirrors economy.lua (BL-436)
    extraction.richness_min       = 0.25f;
    extraction.richness_max       = 2.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    building_economics processing;
    processing.base_rate            = 8.0f;
    processing.maintenance          = 10.0f;
    processing.base_wage            = 12.0f;
    processing.build_cost           = 200.0f;
    processing.build_duration_ticks = 3.0f;
    processing.resource_build_cost[ri(resource_type::steel)] = 25.0f;
    reg.set_economics(building_type::processing_facility, processing);

    building_economics port;
    port.maintenance          = 8.0f;
    port.base_wage            = 6.0f;
    port.build_cost           = 150.0f;
    port.build_duration_ticks = 2.0f;
    port.resource_build_cost[ri(resource_type::steel)] = 20.0f;
    reg.set_economics(building_type::port, port);

    building_economics launchpad;
    launchpad.maintenance          = 20.0f;
    launchpad.base_wage            = 15.0f;
    launchpad.build_cost           = 500.0f;
    launchpad.build_duration_ticks = 6.0f;
    launchpad.resource_build_cost[ri(resource_type::steel)]        = 50.0f;
    launchpad.resource_build_cost[ri(resource_type::refined_fuel)] = 20.0f;
    reg.set_economics(building_type::launchpad, launchpad);

    building_economics hub;
    hub.maintenance          = 12.0f;
    hub.base_wage            = 8.0f;
    hub.build_cost           = 250.0f;
    hub.build_duration_ticks = 3.0f;
    hub.resource_build_cost[ri(resource_type::steel)] = 30.0f;
    reg.set_economics(building_type::inland_logistics_hub, hub);

    building_economics base;
    base.maintenance          = 15.0f;
    base.base_wage            = 10.0f;
    base.build_cost           = 300.0f;
    base.build_duration_ticks = 4.0f;
    base.resource_build_cost[ri(resource_type::steel)] = 35.0f;
    reg.set_economics(building_type::military_base, base);

    recipe steel;
    steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
    steel.outputs[ri(resource_type::steel)]   = 1.0f;
    reg.add_recipe(steel);

    recipe fuel;
    fuel.name = "refined_fuel";
    fuel.inputs[ri(resource_type::petroleum)]     = 2.0f;
    fuel.outputs[ri(resource_type::refined_fuel)] = 1.0f;
    reg.add_recipe(fuel);

    recipe food;
    food.name = "food_rations";
    food.inputs[ri(resource_type::agricultural_produce)] = 2.0f;
    food.outputs[ri(resource_type::food_rations)]        = 1.0f;
    reg.add_recipe(food);

    return reg;
}

/// One full sim tick, in the app's order: economy step (strategic tier
/// included) -> market clearing -> budget -> tech gates. The gate advance is
/// in the loop ON PURPOSE — the sharp form of R1's claim is that a world in
/// which techs ARE earned, but every earned tech carries only
/// unlock_structure, is still bit-identical.
void run_world_tick(world& w, const recipe_registry& reg, int tick)
{
    w.current_day_tick  = tick;
    w.current_econ_tick = tick;
    const economy_report rep = run_economy_step(w, reg);
    const auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, nullptr);
    advance_tech_gates(w);
}

/// state_hash marks at ticks 40 / 80 / 120 for one seed.
std::vector<uint64_t> hash_rollout(uint32_t seed)
{
    world_params params;
    params.seed = seed;
    world w = make_hard_coded_world(no_prehistory(params));
    const recipe_registry reg = make_world_registry();
    assign_default_recipes(w, reg);

    std::vector<uint64_t> marks;
    for (int t = 1; t <= 120; ++t)
    {
        run_world_tick(w, reg, t);
        if (t % 40 == 0)
            marks.push_back(w.state_hash(t));
    }
    return marks;
}

void section_h()
{
    std::printf("\n-- H  the no-effect world (R1 hash half) --\n");
    for (const uint32_t seed : { 0u, 2u })
    {
        const std::vector<uint64_t> a = hash_rollout(seed);
        const std::vector<uint64_t> b = hash_rollout(seed);
        check(a.size() == 3 && b.size() == 3, "H1 rollout produced all three hash marks");
        check(a == b, "H2 two same-seed runs are bit-identical (state_hash)");
        for (std::size_t i = 0; i < a.size(); ++i)
            std::printf("      seed %u tick %3d state_hash %016llX\n",
                        seed, static_cast<int>((i + 1) * 40),
                        static_cast<unsigned long long>(a[i]));
    }
}

// ---------------------------------------------------------------------------
// Fixture world for the union sections — two corps, identical iron tiles
// ---------------------------------------------------------------------------
// Registry deliberately leaves richness_reference unset (conversion disabled,
// scalar = raw richness) so the expected nominal is the legible
// base 20 × richness 2.0 × workforce 0.5 = 20 units/tick per lone site.

recipe_registry make_fixture_registry()
{
    recipe_registry reg;
    building_economics ex;
    ex.base_rate   = 20.0f;
    ex.maintenance = 5.0f;
    ex.base_wage   = 8.0f;
    reg.set_economics(building_type::extraction_site, ex);
    return reg;
}

struct fixture
{
    world     w;
    entity_id body   = null_entity;
    entity_id corp_a = null_entity; ///< The earning corp (rich: passes the fixture gate).
    entity_id corp_b = null_entity; ///< The rival (poor: never passes it).
    entity_id bld_a  = null_entity;
    entity_id bld_b  = null_entity;
};

entity_id add_iron_tile(world& w, entity_id body, int gx, float remaining)
{
    const entity_id t = w.create_entity();
    tile_component  tc{};
    tc.body        = body;
    tc.grid_x      = gx;
    tc.grid_y      = 0;
    tc.substrate = terrain_substrate::rocky;
    tc.landform    = terrain_landform::plains;
    tc.resource_deposit[ri(resource_type::iron_ore)]   = 2.0f;
    tc.resource_remaining[ri(resource_type::iron_ore)] = remaining;
    tc.hazard_level = 0.0f;
    w.tiles[t] = tc;
    return t;
}

entity_id add_site(world& w, entity_id corp, entity_id tile)
{
    const entity_id b = w.create_entity();
    building_component bc{};
    bc.type               = building_type::extraction_site;
    bc.tile               = tile;
    bc.target_resource    = resource_type::iron_ore;
    bc.workforce_assigned = 0.5f;
    bc.workforce_target   = 100;
    bc.workforce_auto     = false; // pin the dial — this fixture measures the rate
    w.buildings[b] = bc;
    w.corporations[corp].assets.push_back(b);
    return b;
}

entity_id add_corp(world& w, const char* name, float balance)
{
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name    = name;
    cc.balance = balance;
    // is_player on BOTH corps keeps the BL-202 strategic tier out of the tick,
    // the econ_harness isolation pattern — this fixture pins arithmetic, and
    // the per-corp claim under test is earned-vs-not, not player-vs-rival.
    cc.is_player = true;
    w.corporations[c] = cc;
    return c;
}

/// Two corps, one identical lone iron site each. A is rich (passes the
/// fixture gate's surplus condition), B is poor (never does).
fixture make_fixture()
{
    fixture f;
    f.body = f.w.create_entity();
    body_component bc{};
    bc.grid_width  = 8;
    bc.grid_height = 4;
    f.w.bodies[f.body] = bc;

    f.corp_a = add_corp(f.w, "Earner Co", 9000.0f);
    f.corp_b = add_corp(f.w, "Rival Co", 100.0f);
    f.w.player_entity = f.corp_a;

    f.bld_a = add_site(f.w, f.corp_a, add_iron_tile(f.w, f.body, 0, 1.0e6f));
    f.bld_b = add_site(f.w, f.corp_b, add_iron_tile(f.w, f.body, 4, 1.0e6f));
    return f;
}

/// The fixture gate table: one tech, earnable by a corp with >= 5000 cr,
/// carrying a single ×1.25 extraction_rate modifier.
std::vector<tech_gate> fixture_gates_x125()
{
    std::vector<tech_gate> gates;
    tech_gate g;
    g.id = "FIX-EX-01";
    {
        condition c;
        c.subject    = condition_subject::surplus;
        c.comparator = condition_comparator::at_least;
        c.operand    = 5000.0f;
        g.condition.all.push_back(c);
    }
    g.add_effect(tech_effect::modify(modifier_subject::extraction_rate,
                                     modifier_op::multiply, 1.25f));
    gates.push_back(g);
    return gates;
}

float output_of(const economy_report& rep, entity_id building)
{
    for (const building_report& br : rep.buildings)
        if (br.building == building)
            return br.output_quantity;
    return -1.0f;
}

// ---------------------------------------------------------------------------
// U1 — op arithmetic and the closed roster
// ---------------------------------------------------------------------------
void section_u1()
{
    std::printf("\n-- U1  the modifier vocabulary (R2) --\n");

    scalar_modifier m;
    m.subject   = modifier_subject::extraction_rate;
    m.magnitude = 2.5f;

    m.op = modifier_op::add;
    check(apply_scalar_modifier(10.0f, m) == 12.5f, "U1a add:      10 + 2.5 = 12.5");
    m.op = modifier_op::subtract;
    check(apply_scalar_modifier(10.0f, m) == 7.5f,  "U1b subtract: 10 - 2.5 = 7.5");
    m.op = modifier_op::multiply;
    check(apply_scalar_modifier(10.0f, m) == 25.0f, "U1c multiply: 10 * 2.5 = 25");

    // The roster's positions are serialisation contract (append-only, one
    // appender per batch). A reordering or an insertion FAILS here by design.
    check(static_cast<uint8_t>(modifier_subject::extraction_rate)  == 0 &&
          static_cast<uint8_t>(modifier_subject::processing_yield) == 1 &&
          static_cast<uint8_t>(modifier_subject::unit_upkeep)      == 2 &&
          static_cast<uint8_t>(modifier_subject::logistics_cost)   == 3 &&
          static_cast<uint8_t>(modifier_subject::wage_floor)       == 4 &&
          static_cast<uint8_t>(modifier_subject::collapse_strain)  == 5,
          "U1d the closed roster holds its authored positions (append-only)");
    check(true, "U1e collapse_strain names the era-collapse subject (BL-477) — compiles, so it exists");
}

// ---------------------------------------------------------------------------
// U2 — the union's unlock arm on the shipped table (R3's union side)
// ---------------------------------------------------------------------------
void section_u2()
{
    std::printf("\n-- U2  E0-ML-01 through the union (R3 mirror) --\n");
    const tech_gate* g = find_tech_gate("E0-ML-01");
    check(g != nullptr, "U2a E0-ML-01 is still in the gate table");
    if (g)
    {
        check(g->effects.size() == 1, "U2b it carries exactly ONE effect");
        check(!g->effects.empty() &&
              g->effects.front().kind == tech_effect_kind::unlock_structure,
              "U2c ...of kind unlock_structure");
        check(!g->effects.empty() &&
              g->effects.front().structure == building_type::military_base,
              "U2d ...unlocking the military base");
        check(g->unlocks_structure == building_type::military_base,
              "U2e the pre-union mirror field agrees with the union");
    }
    check(gating_tech_for(building_type::military_base) == "E0-ML-01",
          "U2f the reverse lookup is unchanged through the union");

    bool any_modifier = false;
    for (const tech_gate& gate : prototype_tech_gates())
        for (const tech_effect& e : gate.effects)
            if (e.kind == tech_effect_kind::modify_scalar)
                any_modifier = true;
    check(!any_modifier,
          "U2g the SHIPPED table carries no modify_scalar tech — section H's premise");
}

// ---------------------------------------------------------------------------
// U3 — the earn moment lands modifiers for the earning corp only
// ---------------------------------------------------------------------------
void section_u3()
{
    std::printf("\n-- U3  earning a modify_scalar tech (R1 fixture half) --\n");
    fixture f = make_fixture();

    // Two gates on the same condition, authored [×1.25, +5]: the fold order
    // check needs two modifiers on ONE subject whose ops do not commute.
    std::vector<tech_gate> gates = fixture_gates_x125();
    {
        tech_gate g2;
        g2.id = "FIX-EX-02";
        {
            condition c;
            c.subject    = condition_subject::surplus;
            c.comparator = condition_comparator::at_least;
            c.operand    = 5000.0f;
            g2.condition.all.push_back(c);
        }
        g2.add_effect(tech_effect::modify(modifier_subject::extraction_rate,
                                          modifier_op::add, 5.0f));
        gates.push_back(g2);
    }

    check(advance_tech_gates(f.w, gates) == 2, "U3a the rich corp earns both fixture techs");
    check(f.w.has_tech(f.corp_a, "FIX-EX-01") && f.w.has_tech(f.corp_a, "FIX-EX-02"),
          "U3b ...recorded in its earned set");
    check(!f.w.has_tech(f.corp_b, "FIX-EX-01"),
          "U3c the poor corp earns neither (the condition binds)");

    check(f.w.corp_modifiers.count(f.corp_a) == 1 &&
          f.w.corp_modifiers.at(f.corp_a).size() == 2,
          "U3d the earner holds exactly the two granted modifiers");
    check(f.w.corp_modifiers.count(f.corp_b) == 0,
          "U3e the rival holds NONE — effects reach the earning corp only");

    // Fold order is earn order (gate-table order): (10 × 1.25) + 5 = 17.5,
    // not (10 + 5) × 1.25 = 18.75.
    check(f.w.modified_scalar(f.corp_a, modifier_subject::extraction_rate, 10.0f) == 17.5f,
          "U3f fold order is earn order: (10 * 1.25) + 5 = 17.5");
    check(f.w.modified_scalar(f.corp_a, modifier_subject::processing_yield, 10.0f) == 10.0f,
          "U3g another subject is untouched (subject isolation)");
    check(f.w.modified_scalar(f.corp_b, modifier_subject::extraction_rate, 10.0f) == 10.0f,
          "U3h the rival's read is untouched");

    // Monotonic and idempotent, like the earned set itself.
    check(advance_tech_gates(f.w, gates) == 0 &&
          f.w.corp_modifiers.at(f.corp_a).size() == 2,
          "U3i re-running the pass never applies an effect twice");
    f.w.corporations[f.corp_a].balance = -1000.0f;
    advance_tech_gates(f.w, gates);
    check(f.w.corp_modifiers.at(f.corp_a).size() == 2,
          "U3j ...and a later dip below the threshold revokes nothing");
}

// ---------------------------------------------------------------------------
// U4 — end to end through the real economy tick
// ---------------------------------------------------------------------------
void section_u4()
{
    std::printf("\n-- U4  the economy tick reads the modifier (R1 end-to-end) --\n");
    const recipe_registry reg = make_fixture_registry();

    // Baseline: identical world, nothing earned.
    fixture base = make_fixture();
    const economy_report rep0 = run_economy_step(base.w, reg);
    const float a0 = output_of(rep0, base.bld_a);
    const float b0 = output_of(rep0, base.bld_b);
    check(near(a0, 20.0f) && near(b0, 20.0f),
          "U4a baseline: both lone sites draw the nominal 20 (base 20 * rich 2 * wf 0.5)");

    // Modified: same construction, the ×1.25 tech earned by corp A before the tick.
    fixture mod = make_fixture();
    advance_tech_gates(mod.w, fixture_gates_x125());
    const economy_report rep1 = run_economy_step(mod.w, reg);
    const float a1 = output_of(rep1, mod.bld_a);
    const float b1 = output_of(rep1, mod.bld_b);

    check(near(a1, a0 * 1.25f), "U4b the earning corp's extraction moved by exactly *1.25");
    check(b1 == b0, "U4c the rival's output is BIT-identical to baseline");
    check(mod.w.pool_for(mod.corp_a, mod.body).quantities[ri(resource_type::iron_ore)] >
          base.w.pool_for(base.corp_a, base.body).quantities[ri(resource_type::iron_ore)],
          "U4d ...and the extra units really landed in the earner's pool");

    // U4b' — the STACK TAPER pre-pass (the second read site). Two A-owned
    // sites stacked on one tight-reserve tile: in the taper-bound regime the
    // stack's total draw this tick is remaining / deposit_taper_ticks
    // REGARDLESS of rate — but only if the pre-pass sizes the taper against
    // the same modified nominal run_extraction draws. A pre-pass that missed
    // the modifier under-sizes the taper divisor and the total overshoots
    // (15.625 here, vs the correct 12.5).
    const float remaining = 100.0f;
    fixture st = make_fixture();
    add_site(st.w, st.corp_a, add_iron_tile(st.w, st.body, 1, remaining)); // rank 1
    // Re-point A's ORIGINAL site... simpler: author a second site on the SAME
    // tile as the new one so exactly one tile is stacked. Find that tile back:
    const entity_id stack_tile = st.w.corporations.at(st.corp_a).assets.size() >= 2
        ? st.w.buildings.at(st.w.corporations.at(st.corp_a).assets.back()).tile
        : null_entity;
    add_site(st.w, st.corp_a, stack_tile);                                  // rank 2
    advance_tech_gates(st.w, fixture_gates_x125());
    const economy_report rep2 = run_economy_step(st.w, reg);
    float stack_total = 0.0f;
    for (const entity_id bid : st.w.corporations.at(st.corp_a).assets)
        if (st.w.buildings.at(bid).tile == stack_tile)
            stack_total += output_of(rep2, bid);
    check(near(stack_total, remaining / deposit_taper_ticks, 0.01f),
          "U4e stacked + taper-bound: total draw == remaining/taper_ticks, so the "
          "pre-pass sized the taper at the MODIFIED rate");
}

// ---------------------------------------------------------------------------
// U5 — the no-modifier reads hand back the same bits
// ---------------------------------------------------------------------------
void section_u5()
{
    std::printf("\n-- U5  null-corp and no-modifier reads --\n");
    fixture f = make_fixture();
    advance_tech_gates(f.w, fixture_gates_x125()); // A holds a modifier

    const float probe = 3.14159265f;
    check(f.w.modified_scalar(null_entity, modifier_subject::extraction_rate, probe) == probe,
          "U5a null_entity reads the same bits back");
    check(f.w.modified_scalar(f.corp_b, modifier_subject::extraction_rate, probe) == probe,
          "U5b a corp with no modifiers reads the same bits back");

    // The estimator default: extraction_nominal with no corp named applies no
    // modifier, even on a building whose OWNER holds one — the documented
    // pre-BL-479 figure every estimator call site still gets.
    const recipe_registry reg = make_fixture_registry();
    const building_component& ba = f.w.buildings.at(f.bld_a);
    check(extraction_nominal(f.w, reg, ba, 1.0f) == 20.0f,
          "U5c extraction_nominal without a corp is the unmodified figure");
    check(near(extraction_nominal(f.w, reg, ba, 1.0f, f.corp_a), 25.0f),
          "U5d ...and with the earning corp named, the modified one");
}

} // namespace

int main()
{
    std::printf("=== tech effect union harness (BL-479) ===\n");

    section_h();
    section_u1();
    section_u2();
    section_u3();
    section_u4();
    section_u5();

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
