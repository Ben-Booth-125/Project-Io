// ---------------------------------------------------------------------------
// Headless unit-march harness (BL-470, unit march seam)
// No SDL / Lua / ImGui — builds over the world/* logic alone.
// ---------------------------------------------------------------------------
// Exercises the three new corp_command verbs (march_unit / halt_unit /
// disband_unit, src/world/corp_command.cpp) and the per-tick resolution pass
// (run_unit_march, src/world/economy_system.cpp) that BL-470 adds — a unit
// no longer stays pinned to its muster tile forever (BL-393's open half).
//
//   M1  march_unit sets a reachable path on a valid unit/dest, and rejects
//       not-owner / invalid unit / invalid tile / ocean dest / off-body dest
//       / already-there WITHOUT mutating the unit (a full snapshot compare,
//       not just the return code — the untrusted-seam rule).
//   M2  run_unit_march spends per-CLASS march points against the shared
//       traversal-cost weight, banks fractional carry-over across ticks, and
//       clears the order (with an arrival count) once the unit reaches dest.
//       A class authored at zero march points never moves.
//   M3  halt_unit clears an order and rejects a halted/unordered unit;
//       disband_unit erases the unit and rejects a non-owned or unknown one.
//   M4  NR-344 ("war flips the queue"): corp_is_mobilised reads the stance
//       table correctly in BOTH directions (declaring hostile AND being
//       declared against both mobilise), and the pass does not crash or
//       diverge when a mobilised and a peaceful corp both hold live orders
//       in the same tick — see the test's own comment for what is and is
//       not observable yet.
//   M5  DETERMINISM: state-hash replay across two independently constructed
//       worlds, running identical command sequences and economy ticks with
//       marching units in flight, agrees at every tick checkpoint.
//   M6  The blocked-step recompute path (a corrupted path — the only way to
//       trigger it, since terrain never actually blocks a step in this
//       engine today) recomputes IDENTICALLY across two independent runs of
//       the same corruption — no iteration-order dependence.
//
// The process exits non-zero if any assertion FAILs.

#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

constexpr uint16_t ROW_INFANTRY = 0; // Levy Spear — unit_class::infantry (unit_roster.cpp)

/// Build a body carrying a `gw`-wide, single-row grid of uniform land tiles
/// (mirrors tools/verify/logistics_harness.cpp's make_grid — the shared
/// convention for a minimal pathable fixture).
entity_id make_row_body(world& w, int gw, terrain_landform lf = terrain_landform::plains)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.grid_width  = gw;
    bc.grid_height = 1;
    w.bodies[body] = bc;
    for (int c = 0; c < gw; ++c)
    {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body        = body;
        tc.grid_x      = c;
        tc.grid_y      = 0;
        tc.composition = terrain_composition::grassland;
        tc.landform    = lf;
        w.tiles[t]     = tc;
    }
    return body;
}

entity_id tile_at(world& w, entity_id body, int c)
{
    return body_tile_grid(w, body)[static_cast<std::size_t>(c)];
}

entity_id add_corp(world& w, const char* name)
{
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name    = name;
    cc.balance = 10000.0f;
    w.corporations[c] = cc;
    return c;
}

entity_id add_unit(world& w, entity_id corp, entity_id tile, uint16_t type = ROW_INFANTRY, int count = 50)
{
    const entity_id u = w.create_entity();
    unit_component uc{};
    uc.owner    = corp;
    uc.position = tile;
    uc.count    = count;
    uc.type     = type;
    w.units[u]  = uc;
    return u;
}

/// A registry with `march_points_per_class[infantry]` authored to @p pts;
/// every other class stays at the struct default (0.0 — cannot march).
recipe_registry registry_with_march(float pts)
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    mp.march_points_per_class[static_cast<std::size_t>(unit_class::infantry)] = pts;
    reg.set_military(mp);
    return reg;
}

bool order_equal(const movement_order& a, const movement_order& b)
{
    return a.dest == b.dest && a.path == b.path && a.next_index == b.next_index
        && approx(a.progress, b.progress, 1e-6f);
}

} // namespace

// ---------------------------------------------------------------------------
// M1 — march_unit: legality and rejection-mutates-nothing
// ---------------------------------------------------------------------------

void m1_march_unit_legality()
{
    std::printf("\n-- M1  march_unit: sets a reachable path; a rejection mutates nothing --\n");

    world w;
    const entity_id body = make_row_body(w, 8);
    const entity_id a    = add_corp(w, "A");
    const entity_id b    = add_corp(w, "B");
    const entity_id t0   = tile_at(w, body, 0);
    const entity_id t5   = tile_at(w, body, 5);
    const entity_id unit = add_unit(w, a, t0);
    recipe_registry reg  = registry_with_march(1.0f);

    // -- applied: a fresh, reachable, on-body, non-degenerate order --
    corp_command cmd;
    cmd.corp = a; cmd.verb = corp_verb::march_unit; cmd.subject = unit; cmd.tile = t5;
    const corp_command_result r = apply_corp_command(w, reg, cmd);
    check(r == corp_command_result::applied, "march_unit applies for the owning corp to a reachable dest");
    const unit_component& u = w.units.at(unit);
    check(u.order.dest == t5, "order.dest is the commanded tile");
    check(!u.order.path.empty() && u.order.path.front() == t0 && u.order.path.back() == t5,
          "path runs from the unit's own tile to dest");
    check(u.order.next_index == 1, "next_index starts at 1 (path[0] is already occupied)");
    check(approx(u.order.progress, 0.0f), "progress starts at 0");

    const movement_order snapshot = u.order;

    // -- rejected_not_owner: caller names a corp that does not own the unit --
    {
        corp_command bad = cmd; bad.corp = b; bad.tile = tile_at(w, body, 2);
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_not_owner, "march_unit rejects a non-owning corp");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
    // -- rejected_invalid: unknown unit --
    {
        corp_command bad = cmd; bad.subject = static_cast<entity_id>(999999);
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_invalid, "march_unit rejects an unknown unit");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
    // -- rejected_invalid: unknown tile --
    {
        corp_command bad = cmd; bad.tile = static_cast<entity_id>(999999);
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_invalid, "march_unit rejects an unknown destination tile");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
    // -- rejected_invalid: ocean destination --
    {
        const entity_id ocean_body = w.create_entity();
        body_component obc{}; obc.grid_width = 1; obc.grid_height = 1; w.bodies[ocean_body] = obc;
        const entity_id ot = w.create_entity();
        tile_component otc{}; otc.body = ocean_body; otc.composition = terrain_composition::ocean;
        w.tiles[ot] = otc;
        corp_command bad = cmd; bad.tile = ot;
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_invalid, "march_unit rejects an ocean destination");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
    // -- rejected_invalid: destination on a different body --
    {
        const entity_id other_body = make_row_body(w, 2);
        corp_command bad = cmd; bad.tile = tile_at(w, other_body, 0);
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_invalid, "march_unit rejects a destination off the unit's own body");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
    // -- rejected_state: dest already == position --
    {
        corp_command bad = cmd; bad.tile = u.position;
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_state, "march_unit rejects a dest equal to the unit's own tile");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
}

// ---------------------------------------------------------------------------
// M2 — run_unit_march: per-class points, fractional carry-over, arrival
// ---------------------------------------------------------------------------

void m2_resolution_and_carry_over()
{
    std::printf("\n-- M2  run_unit_march: per-class points, carry-over, arrival --\n");

    world w;
    // 10-wide, marching col0 -> col4: direct = 4 hops, the cylinder-wrap
    // shortcut = 10 - 4 = 6 hops, so the direct route is unambiguously
    // shortest (a 5-wide grid would make the 1-hop WRAP the shortest path
    // instead, which is what this test tripped over before it was widened).
    const entity_id body = make_row_body(w, 10);
    const entity_id a    = add_corp(w, "A");
    const entity_id t0 = tile_at(w, body, 0), t1 = tile_at(w, body, 1),
                    t3 = tile_at(w, body, 3), t4 = tile_at(w, body, 4);
    const entity_id unit = add_unit(w, a, t0);
    recipe_registry reg   = registry_with_march(1.5f); // 1.5 pts/tick, plains costs 1.0/hop

    corp_command cmd; cmd.corp = a; cmd.verb = corp_verb::march_unit; cmd.subject = unit; cmd.tile = t4;
    check(apply_corp_command(w, reg, cmd) == corp_command_result::applied, "order placed (fixture setup)");

    const unit_march_tick r1 = run_unit_march(w, reg);
    check(w.units.at(unit).position == t1, "tick 1: 1.5 pts covers exactly one 1.0-cost hop, banks 0.5");
    check(approx(w.units.at(unit).order.progress, 0.5f), "tick 1: 0.5 banked as fractional carry-over");
    check(r1.arrived == 0, "tick 1: not yet arrived");

    const unit_march_tick r2 = run_unit_march(w, reg);
    check(w.units.at(unit).position == t3, "tick 2: 1.5 + 0.5 banked = 2.0 pts covers two more hops");
    check(approx(w.units.at(unit).order.progress, 0.0f), "tick 2: carry-over exactly exhausted");
    check(r2.arrived == 0, "tick 2: not yet arrived");

    const unit_march_tick r3 = run_unit_march(w, reg);
    check(w.units.at(unit).position == t4, "tick 3: final hop reaches dest");
    check(w.units.at(unit).order.dest == null_entity, "tick 3: the order clears itself on arrival");
    check(r3.arrived == 1, "tick 3: arrival is counted");

    // A class authored at zero march points never moves.
    {
        world w2;
        const entity_id body2 = make_row_body(w2, 3);
        const entity_id a2    = add_corp(w2, "A2");
        const entity_id s0 = tile_at(w2, body2, 0), s2 = tile_at(w2, body2, 2);
        const entity_id cav = add_unit(w2, a2, s0, static_cast<uint16_t>(0)); // still infantry row...
        // ...force the CLASS the resolver reads to naval (0 pts) directly on
        // the order, bypassing march_unit (which only needs a valid roster
        // row): what is under test is the per-class GATE, not the seam.
        movement_order mo; mo.dest = s2; mo.path = {s0, tile_at(w2, body2, 1), s2}; mo.next_index = 1;
        w2.units.at(cav).order = mo;
        recipe_registry reg0; // every class defaults to 0.0 march points
        const unit_march_tick r0 = run_unit_march(w2, reg0);
        check(w2.units.at(cav).position == s0, "a class with 0 march points never advances");
        check(r0.marching == 1, "...but the order is still counted as held/marching");
    }
}

// ---------------------------------------------------------------------------
// M3 — halt_unit / disband_unit
// ---------------------------------------------------------------------------

void m3_halt_and_disband()
{
    std::printf("\n-- M3  halt_unit / disband_unit --\n");

    world w;
    const entity_id body = make_row_body(w, 4);
    const entity_id a = add_corp(w, "A"), b = add_corp(w, "B");
    const entity_id t0 = tile_at(w, body, 0), t3 = tile_at(w, body, 3);
    const entity_id unit = add_unit(w, a, t0);
    recipe_registry reg = registry_with_march(1.0f);

    corp_command march; march.corp = a; march.verb = corp_verb::march_unit; march.subject = unit; march.tile = t3;
    check(apply_corp_command(w, reg, march) == corp_command_result::applied, "order placed (fixture setup)");

    corp_command halt; halt.corp = a; halt.verb = corp_verb::halt_unit; halt.subject = unit;
    check(apply_corp_command(w, reg, halt) == corp_command_result::applied, "halt_unit clears a live order");
    check(w.units.at(unit).order.dest == null_entity, "  ...order.dest is null after halt");

    check(apply_corp_command(w, reg, halt) == corp_command_result::rejected_state,
          "halt_unit on an already-halted unit is rejected_state");

    corp_command disband_wrong; disband_wrong.corp = b; disband_wrong.verb = corp_verb::disband_unit;
    disband_wrong.subject = unit;
    check(apply_corp_command(w, reg, disband_wrong) == corp_command_result::rejected_not_owner,
          "disband_unit rejects a non-owning corp");
    check(w.units.find(unit) != w.units.end(), "  ...and the unit still exists");

    corp_command disband; disband.corp = a; disband.verb = corp_verb::disband_unit; disband.subject = unit;
    check(apply_corp_command(w, reg, disband) == corp_command_result::applied,
          "disband_unit erases the unit for its owner");
    check(w.units.find(unit) == w.units.end(), "  ...the unit is gone from w.units");

    check(apply_corp_command(w, reg, disband) == corp_command_result::rejected_invalid,
          "disband_unit on an already-gone unit is rejected_invalid");
}

// ---------------------------------------------------------------------------
// M4 — NR-344, "war flips the queue"
// ---------------------------------------------------------------------------

void m4_war_flips_the_queue()
{
    std::printf("\n-- M4  NR-344: mobilisation reads the stance table, both directions --\n");

    world w;
    const entity_id a = add_corp(w, "A"), b = add_corp(w, "B"), c = add_corp(w, "C");

    check(!corp_is_mobilised(w, a) && !corp_is_mobilised(w, b) && !corp_is_mobilised(w, c),
          "at peace, nobody is mobilised");

    declare_hostile(w, a, b); // a -> b
    check(corp_is_mobilised(w, a), "the DECLARING corp is mobilised");
    check(corp_is_mobilised(w, b), "the corp DECLARED AGAINST is mobilised too — 'being attacked mobilises'");
    check(!corp_is_mobilised(w, c), "an uninvolved corp stays peaceful");

    // The pass itself must not crash or diverge with a mobilised corp and a
    // peaceful corp both holding live orders in one tick. There is no shared
    // logistics-point pool yet (BL-464) for the reorder to visibly change —
    // see run_unit_march's own doc comment — so what this proves is
    // "runs cleanly and reaches the same destinations", not a contention
    // outcome that does not exist to observe yet.
    const entity_id body = make_row_body(w, 4);
    const entity_id tA = tile_at(w, body, 0), tC = tile_at(w, body, 2);
    const entity_id destA = tile_at(w, body, 3), destC = tile_at(w, body, 3);
    // b (mobilised) gets a HIGHER entity id than c (peaceful)'s unit below,
    // so ascending-unit-id order alone would visit c's unit first; the
    // reorder is what NR-344 asks for.
    const entity_id unit_b = add_unit(w, b, tA);
    const entity_id unit_c = add_unit(w, c, tC);
    recipe_registry reg = registry_with_march(1.0f);

    corp_command mb; mb.corp = b; mb.verb = corp_verb::march_unit; mb.subject = unit_b; mb.tile = destA;
    corp_command mc; mc.corp = c; mc.verb = corp_verb::march_unit; mc.subject = unit_c; mc.tile = destC;
    check(apply_corp_command(w, reg, mb) == corp_command_result::applied, "b's order placed (fixture setup)");
    check(apply_corp_command(w, reg, mc) == corp_command_result::applied, "c's order placed (fixture setup)");

    for (int i = 0; i < 6; ++i) run_unit_march(w, reg);
    check(w.units.at(unit_b).position == destA, "b's (mobilised) unit still reaches its destination");
    check(w.units.at(unit_c).position == destC, "c's (peaceful) unit still reaches its destination");
}

// ---------------------------------------------------------------------------
// M5 — determinism: state-hash replay with marching units in flight
// ---------------------------------------------------------------------------

struct twin_world
{
    world w;
    entity_id body, corp, u_from, u_to, unit;
};

twin_world build_twin(int tick0)
{
    twin_world t;
    t.body = make_row_body(t.w, 7);
    t.corp = add_corp(t.w, "Replay Corp");
    t.u_from = tile_at(t.w, t.body, 0);
    t.u_to   = tile_at(t.w, t.body, 6);
    t.unit   = add_unit(t.w, t.corp, t.u_from);
    (void)tick0;
    return t;
}

void m5_determinism_replay()
{
    std::printf("\n-- M5  determinism: state-hash replay with marching units in flight --\n");

    twin_world t1 = build_twin(0);
    twin_world t2 = build_twin(0);
    check(t1.unit == t2.unit && t1.u_to == t2.u_to,
          "twin worlds allocate identical entity ids (fresh, same construction order)");

    recipe_registry reg = registry_with_march(1.3f);

    corp_command m1; m1.corp = t1.corp; m1.verb = corp_verb::march_unit; m1.subject = t1.unit; m1.tile = t1.u_to;
    corp_command m2; m2.corp = t2.corp; m2.verb = corp_verb::march_unit; m2.subject = t2.unit; m2.tile = t2.u_to;
    apply_corp_command(t1.w, reg, m1);
    apply_corp_command(t2.w, reg, m2);

    bool all_equal = true;
    for (int tick = 1; tick <= 8; ++tick)
    {
        run_unit_march(t1.w, reg);
        run_unit_march(t2.w, reg);
        run_unit_upkeep(t1.w, reg);
        run_unit_upkeep(t2.w, reg);
        if (t1.w.state_hash(tick) != t2.w.state_hash(tick))
        {
            all_equal = false;
            std::printf("    tick %d: hash mismatch (0x%llx vs 0x%llx)\n", tick,
                        static_cast<unsigned long long>(t1.w.state_hash(tick)),
                        static_cast<unsigned long long>(t2.w.state_hash(tick)));
        }
    }
    check(all_equal, "state_hash agrees at every tick across two independent runs, marching units in flight");
    check(t1.w.units.at(t1.unit).position == t1.u_to && t2.w.units.at(t2.unit).position == t2.u_to,
          "both units actually arrived by the end of the replay");
}

// ---------------------------------------------------------------------------
// M6 — blocked-step recompute is deterministic
// ---------------------------------------------------------------------------

/// Build a fixture, place an order, then CORRUPT the next hop so the pass's
/// blocked-step guard fires — the only way to trigger it in this engine
/// today, since terrain never actually changes post-generation. Returns the
/// harness result of one `run_unit_march` call.
unit_march_tick run_corrupted_recompute(world& w, entity_id& out_unit, entity_id& out_dest,
                                        const recipe_registry& reg)
{
    const entity_id body = make_row_body(w, 6);
    const entity_id corp = add_corp(w, "Corp");
    const entity_id start = tile_at(w, body, 0), dest = tile_at(w, body, 5);
    const entity_id unit = add_unit(w, corp, start);

    corp_command cmd; cmd.corp = corp; cmd.verb = corp_verb::march_unit; cmd.subject = unit; cmd.tile = dest;
    apply_corp_command(w, reg, cmd);

    // Corrupt the SECOND path step so it points at a tile on a DIFFERENT
    // body — a step the blocked-check rejects on sight. `dest` (and the
    // body) are unchanged, so a route still exists and the recompute should
    // succeed and eventually reach it.
    const entity_id other_body = make_row_body(w, 1);
    unit_component& u = w.units.at(unit);
    u.order.path[u.order.next_index] = tile_at(w, other_body, 0);

    out_unit = unit;
    out_dest = dest;
    return run_unit_march(w, reg);
}

void m6_recompute_determinism()
{
    std::printf("\n-- M6  a corrupted (blocked) path recomputes identically across two runs --\n");

    recipe_registry reg = registry_with_march(1.0f);

    world w1, w2;
    entity_id u1, d1, u2, d2;
    const unit_march_tick r1 = run_corrupted_recompute(w1, u1, d1, reg);
    const unit_march_tick r2 = run_corrupted_recompute(w2, u2, d2, reg);

    check(r1.recomputed >= 1 && r1.recomputed == r2.recomputed,
          "the recompute path fires, and fires the same number of times, both runs");
    check(order_equal(w1.units.at(u1).order, w2.units.at(u2).order),
          "the resulting order (dest/path/next_index/progress) is identical across runs");
    check(w1.units.at(u1).position == w2.units.at(u2).position,
          "the resulting position is identical across runs");

    // Run both to completion and confirm the recovered path still reaches
    // the (unchanged) destination — the block did not strand the unit.
    for (int i = 0; i < 10; ++i) { run_unit_march(w1, reg); run_unit_march(w2, reg); }
    check(w1.units.at(u1).position == d1 && w2.units.at(u2).position == d2,
          "recovery from the block still reaches the original destination on both runs");
}

int main()
{
    std::printf("=== unit_march_harness (BL-470, unit march seam) ===\n");

    m1_march_unit_legality();
    m2_resolution_and_carry_over();
    m3_halt_and_disband();
    m4_war_flips_the_queue();
    m5_determinism_replay();
    m6_recompute_determinism();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
