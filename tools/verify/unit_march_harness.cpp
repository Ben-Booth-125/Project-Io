// ---------------------------------------------------------------------------
// Headless unit-march harness (BL-470, unit march seam)
// No SDL / Lua / ImGui — builds over the world/* logic alone.
// ---------------------------------------------------------------------------
// Exercises the three new corp_command verbs (march_unit / halt_unit /
// disband_unit, src/world/corp_command.cpp) and the per-tick resolution pass
// (run_unit_march, src/world/economy_system.cpp) that BL-470 adds — a unit
// no longer stays pinned to its muster tile forever (BL-393's open half).
//
// RE-POINTED 2026-10-27 (BL-1443, tile contact): EVERY MARCH GOES TO A TILE and
// ends on it (Ben: "all marches go to a tile"). The province grain BL-511 gave
// the verb is retired, so each row that named a destination province names a
// destination tile, and each row that asserted province-grain arrival asserts
// arrival ON the tile. The rows themselves are unchanged in what they protect.
//
//   M1  march_unit sets a reachable path on a valid unit/dest TILE, and rejects
//       not-owner / invalid unit / nonexistent tile / no tile (a province-only
//       command) / off-body tile / already-on-that-tile WITHOUT mutating the
//       unit (a full snapshot compare, not just the return code — the
//       untrusted-seam rule).
//   M7  THE UNTRUSTED INPUT BOUNDARY, exhaustively: every id in the entity
//       domain that is not a tile — null, the top of the domain, and a sweep of
//       near-miss and single-bit-flip neighbours of a real tile id — is
//       rejected WHOLE, and the unit is byte-identical afterwards. Plus the
//       APPEND-ONLY enum property: march_unit / halt_unit / disband_unit
//       still hold the exact values BL-470 gave them, and corp_verb_count
//       still equals disband_unit + 1.
//   M2  run_unit_march spends per-CLASS march points against the shared
//       traversal-cost weight, banks fractional carry-over across ticks, and
//       clears the order (with an arrival count) once the unit stands ON its
//       destination tile.
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
#include "world/province.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace {

// BL-654: both upkeep passes now take an `economy_report&` — the shortfall bid
// they place on the market lands in its `wants` / `purchases` / `upkeep_wants`
// registers. Nothing in THIS harness reads those registers, and nothing here can
// write to them either: `price_band_params::reservation_mult` defaults to 0 and
// these fixtures hand-build their registry, so the bid path is off and the draw
// is pool-only exactly as it was before BL-654. One shared scratch report keeps
// that visible rather than scattering a fresh local at every call site.
economy_report g_upkeep_report;

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
    entity_id first_tile = null_entity;
    for (int c = 0; c < gw; ++c)
    {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body        = body;
        tc.grid_x      = c;
        tc.grid_y      = 0;
        tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
        tc.landform    = lf;
        w.tiles[t]     = tc;
        if (c == 0)
            first_tile = t;
    }
    // BL-596: every fixture body needs a supply anchor now that run_unit_march
    // gates on active Logistic Points — an anchorless body refuses EVERY
    // march outright (LOGISTICS.md § Logistic Points constraint 2: cities are
    // the locus). A city at tile 0 anchors the whole row (body_reach_field's
    // own Dijkstra is what M2/M4/M5/M6 already exercise the cost of), and
    // registry_with_march below pairs it with a generous active-LP rate so
    // the cap does not bind unless a test deliberately starves it — see
    // tools/verify/logistic_points_harness.cpp for the tests that DO.
    w.population_centre_tile[w.create_entity()] = first_tile;
    return body;
}

entity_id tile_at(world& w, entity_id body, int c)
{
    return body_tile_grid(w, body)[static_cast<std::size_t>(c)];
}

entity_id add_corp(world& w, const char* name)
{
    const entity_id c = w.create_entity();
    nation_component cc{};
    cc.name    = name;
    cc.treasury = 10000.0f;
    w.nations[c] = cc;
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

/// A registry whose Era -1 infantry moves @p pts of traversal cost a day;
/// every other class stays at the struct default (0.0 — cannot march).
///
/// BL-596: also authors a deliberately generous active_lp_per_anchor_tick so
/// none of THIS file's pre-existing march tests are gated by the cap — they
/// test march resolution, not LP contention. Set once, high enough that no
/// fixture here (a handful of units, one anchor) can plausibly exhaust it;
/// tools/verify/logistic_points_harness.cpp is where the cap itself, and its
/// refusal path, are actually exercised.
recipe_registry registry_with_march(float pts)
{
    // BL-1445 (move by days): a march is stepped once a DAY, and a class is authored
    // as DAYS PER PLAINS TILE. This file's rows are about march resolution (carry-over,
    // arrival, visit order, recompute), not speed, so a fixture authored at @p pts
    // "points a call" is the class that crosses a plains tile in 1/pts days: each
    // run_unit_march call (one day) then banks exactly pts of traversal cost, the
    // arithmetic every row below was written against. Row 0 is an Era -1 infantry row,
    // so it reads the by-cls table. The authored speeds are move_by_days_harness's.
    recipe_registry reg;
    military_capability_params mp = reg.military();
    mp.march_days_per_cls[static_cast<std::size_t>(unit_class::infantry)] = pts > 0.0f ? 1.0f / pts : 0.0f;
    mp.active_lp_credit_per_unit_distance = 0.0f; // isolate march mechanics from credit pricing here
    reg.set_military(mp);
    return reg;
}

bool order_equal(const movement_order& a, const movement_order& b)
{
    return a.dest == b.dest
        && a.path == b.path && a.next_index == b.next_index
        && approx(a.progress, b.progress, 1e-6f);
}

/// BL-511: every fixture now needs a BUILT partition — march_unit's domain
/// check is `province_partition::find`, so a world with no partition refuses
/// every march. Seed is fixed: the partition is a pure fold from (seed, grid,
/// land mask, terrain), so twin worlds built the same way partition the same
/// way, which is what M5/M6 rely on.
constexpr uint32_t PARTITION_SEED = 0xB1511u;

/// A gw x gh land body. Needed since the 2026-08-21 repartition: provinces are
/// carved from 3x3 blocks with a merge pass that folds anything under
/// `k_province_min_tiles` (7) into a neighbour. On a ONE-ROW body every block is
/// a 3-tile fragment, so the merge cascades and the whole row becomes a single
/// province however long it is — a row fixture simply cannot express "march from
/// one province to another" any more. Two rows of blocks give real provinces.
entity_id make_grid_body(world& w, int gw, int gh,
                         terrain_landform lf = terrain_landform::plains)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.grid_width  = gw;
    bc.grid_height = gh;
    w.bodies[body] = bc;
    entity_id first_tile = null_entity;
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body        = body;
            tc.grid_x      = c;
            tc.grid_y      = r;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
            tc.landform    = lf;
            w.tiles[t]     = tc;
            if (r == 0 && c == 0)
                first_tile = t;
        }
    // BL-596: same anchor requirement as make_row_body, above.
    w.population_centre_tile[w.create_entity()] = first_tile;
    return body;
}

/// Tile at (col,row) on a body built by make_grid_body.
entity_id grid_tile_at(const world& w, entity_id body, int col, int row)
{
    for (const auto& [id, tc] : w.tiles)
        if (tc.body == body && tc.grid_x == col && tc.grid_y == row)
            return id;
    return null_entity;
}

void partition(world& w) { build_province_partition(w, PARTITION_SEED); }

/// The province owning @p tile, or 0. Fixtures name a DESTINATION by naming a
/// tile in it — the province ids themselves are derived and not worth
/// hand-computing in a test.
uint32_t prov_of(const world& w, entity_id tile) { return w.provinces.province_of(tile); }

/// A province id that is guaranteed NOT in @p w's partition. Walks up from
/// `from` until it misses — never fabricated blindly, because a fabricated id
/// that happens to exist would turn a rejection test into a silent pass.
uint32_t absent_province_id(const world& w, uint32_t from)
{
    for (uint32_t candidate = from; candidate < from + 4096u; ++candidate)
        if (w.provinces.find(candidate) == nullptr)
            return candidate;
    return no_province; // the structurally unreachable sentinel — never a real id
}

} // namespace

// ---------------------------------------------------------------------------
// M1 — march_unit: legality and rejection-mutates-nothing
// ---------------------------------------------------------------------------

void m1_march_unit_legality()
{
    std::printf("\n-- M1  march_unit: sets a reachable path to a TILE; a rejection mutates nothing --\n");

    world w;
    const entity_id body = make_grid_body(w, 12, 6);
    const entity_id a    = add_corp(w, "A");
    const entity_id b    = add_corp(w, "B");
    const entity_id t0   = grid_tile_at(w, body, 0, 0);
    const entity_id t5   = grid_tile_at(w, body, 10, 4);
    partition(w);
    const entity_id unit = add_unit(w, a, t0);
    recipe_registry reg  = registry_with_march(1.0f);

    // -- applied: a fresh, reachable, on-body, non-degenerate order --
    corp_command cmd;
    cmd.corp = a; cmd.verb = corp_verb::march_unit; cmd.subject = unit; cmd.tile = t5;
    const corp_command_result r = apply_corp_command(w, reg, cmd);
    check(r == corp_command_result::applied, "march_unit applies for the owning nation to a reachable dest tile");
    const unit_component& u = w.units.at(unit);
    check(u.order.dest == t5, "order.dest is the commanded TILE");
    check(!u.order.path.empty() && u.order.path.front() == t0 && u.order.path.back() == t5,
          "path runs from the unit's own tile to the commanded tile");
    check(u.order.next_index == 1, "next_index starts at 1 (path[0] is already occupied)");
    check(approx(u.order.progress, 0.0f), "progress starts at 0");

    // `province` is no longer read by this verb (2026-10-27). Setting it to
    // nonsense must change nothing — the strongest single statement that the
    // payload actually moved rather than being read from two places.
    {
        world w2;
        const entity_id b2 = make_grid_body(w2, 12, 6);
        const entity_id c2 = add_corp(w2, "A");
        const entity_id s2 = grid_tile_at(w2, b2, 0, 0), d2 = grid_tile_at(w2, b2, 10, 4);
        partition(w2);
        const entity_id u2 = add_unit(w2, c2, s2);
        corp_command c; c.corp = c2; c.verb = corp_verb::march_unit; c.subject = u2;
        c.tile     = d2;
        c.province = 987654u; // garbage; unread by this verb now
        check(apply_corp_command(w2, reg, c) == corp_command_result::applied,
              "march_unit ignores `province` entirely — a garbage province does not affect it");
        const movement_order& o2 = w2.units.at(u2).order;
        check(o2.dest == d2 && o2.path.front() == s2 && o2.path.back() == d2,
              "  ...and produces exactly the order the tile alone specifies");
    }

    const movement_order snapshot = u.order;

    // -- rejected_not_owner: caller names a nation that does not own the unit --
    {
        corp_command bad = cmd; bad.corp = b; bad.tile = grid_tile_at(w, body, 2, 0);
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_not_owner, "march_unit rejects a non-owning nation");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
    // -- rejected_invalid: unknown unit --
    {
        corp_command bad = cmd; bad.subject = static_cast<entity_id>(999999);
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_invalid, "march_unit rejects an unknown unit");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
    // -- rejected_invalid: a tile id that is not a tile --
    {
        corp_command bad = cmd; bad.tile = static_cast<entity_id>(0x7FFFFF00u);
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_invalid, "march_unit rejects a nonexistent tile id");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
    // -- rejected_invalid: the unset default, and a province-only command --
    {
        corp_command bad = cmd; bad.tile = null_entity;
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_invalid,
              "march_unit rejects the unset default (null tile) — an omitted field is not a destination");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");

        corp_command prov_only; prov_only.corp = a; prov_only.verb = corp_verb::march_unit;
        prov_only.subject = unit; prov_only.province = prov_of(w, t5);
        check(apply_corp_command(w, reg, prov_only) == corp_command_result::rejected_invalid,
              "a march_unit naming only a PROVINCE (the retired grain) names no tile and is rejected");
        check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
    }
    // -- WATER: a tile in a water province is refused for a land row --
    {
        world w3; // a separate world — re-partitioning `w` mid-test would move its ids
        const entity_id ob = w3.create_entity();
        body_component obc3{}; obc3.grid_width = 1; obc3.grid_height = 1; w3.bodies[ob] = obc3;
        const entity_id ot3 = w3.create_entity();
        tile_component otc3{}; otc3.body = ob; otc3.substrate = terrain_substrate::ocean;
        w3.tiles[ot3] = otc3;
        partition(w3);
        const uint32_t sea_pid = prov_of(w3, ot3);
        check(sea_pid != 0, "an ocean tile HAS a province (BL-516 draws provinces over water)");
        check(province_kind_of(w3, sea_pid) != province_kind::land,
              "  ...and that province is not a land province");

        entity_id water_tile = null_entity;
        for (const province& p : w.provinces.provinces)
            if (province_kind_of(w, p) != province_kind::land && !p.tiles.empty())
            {
                water_tile = p.tiles.front();
                break;
            }
        if (water_tile != null_entity)
        {
            corp_command wet = cmd; wet.tile = water_tile;
            check(apply_corp_command(w, reg, wet) == corp_command_result::rejected_invalid,
                  "march_unit rejects a tile of a WATER province outright (units are land-bound)");
            check(order_equal(w.units.at(unit).order, snapshot), "  ...and mutates nothing");
        }
    }
    // -- rejected_invalid: a tile on a different body --
    {
        world w4;
        const entity_id home  = make_row_body(w4, 6);
        const entity_id other = make_row_body(w4, 6);
        const entity_id c4    = add_corp(w4, "A");
        const entity_id h0    = tile_at(w4, home, 0);
        const entity_id o5    = tile_at(w4, other, 5);
        partition(w4);
        const entity_id u4 = add_unit(w4, c4, h0);
        corp_command bad; bad.corp = c4; bad.verb = corp_verb::march_unit; bad.subject = u4;
        bad.tile = o5;
        const auto rr = apply_corp_command(w4, reg, bad);
        check(rr == corp_command_result::rejected_invalid,
              "march_unit rejects a tile off the unit's own body");
        check(w4.units.at(u4).order.dest == null_entity, "  ...and mutates nothing");
    }
    // -- rejected_state: the unit already stands on the commanded tile --
    {
        corp_command bad = cmd; bad.tile = u.position;
        const auto rr = apply_corp_command(w, reg, bad);
        check(rr == corp_command_result::rejected_state,
              "march_unit rejects the tile the unit already stands on");
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
    // 20-wide (was 10). The cylinder-wrap trap this comment has warned about
    // since BL-470 bit again under BL-511: the destination moved from col 4 to
    // col 8 (so the destination PROVINCE cannot contain the tick-1/tick-2
    // tiles), and on a 10-wide row the wrap route to col 8 is 2 hops against
    // the direct route's 8, so the unit marched WEST. At 20 wide the direct
    // route is 8 hops and the wrap is 12 — unambiguous again.
    const entity_id body = make_row_body(w, 20);
    const entity_id a    = add_corp(w, "A");
    const entity_id t0 = tile_at(w, body, 0), t1 = tile_at(w, body, 1),
                    t3 = tile_at(w, body, 3), t4 = tile_at(w, body, 4);
    partition(w);
    const entity_id unit = add_unit(w, a, t0);
    recipe_registry reg   = registry_with_march(1.5f); // 1.5 pts/tick, plains costs 1.0/hop

    // The destination is col 8: far enough that ticks 1 and 2 are mid-march, so
    // the points arithmetic below is about carry-over, not about arrival.
    const entity_id t8 = tile_at(w, body, 8);
    corp_command cmd; cmd.corp = a; cmd.verb = corp_verb::march_unit; cmd.subject = unit; cmd.tile = t8;
    check(apply_corp_command(w, reg, cmd) == corp_command_result::applied, "order placed (fixture setup)");

    const unit_march_tick r1 = run_unit_march(w, reg);
    check(w.units.at(unit).position == t1, "tick 1: 1.5 pts covers exactly one 1.0-cost hop, banks 0.5");
    check(approx(w.units.at(unit).order.progress, 0.5f), "tick 1: 0.5 banked as fractional carry-over");
    check(r1.arrived == 0, "tick 1: not yet arrived");

    const unit_march_tick r2 = run_unit_march(w, reg);
    check(w.units.at(unit).position == t3, "tick 2: 1.5 + 0.5 banked = 2.0 pts covers two more hops");
    check(approx(w.units.at(unit).order.progress, 0.0f), "tick 2: carry-over exactly exhausted");
    check(r2.arrived == 0, "tick 2: not yet arrived");

    // Run to arrival: every march goes to a tile and ends ON it (2026-10-27).
    int arrivals = 0, ticks_run = 0;
    while (w.units.at(unit).order.dest != null_entity && ticks_run < 30)
    {
        arrivals += run_unit_march(w, reg).arrived;
        ++ticks_run;
    }
    const entity_id final_pos = w.units.at(unit).position;
    check(final_pos == t8, "the unit ends ON its destination tile — arrival is tile-grain");
    check(w.units.at(unit).order.dest == null_entity, "the order clears itself on arrival");
    check(arrivals == 1, "arrival is counted exactly once");
    (void)t4;

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
        // A RAW tile order, built by hand: what is under test here is the
        // per-class points gate, not the seam.
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
    // 9x9 (was a 4-wide row). Post-repartition a 4-wide row is ONE province,
    // so the march below was rejected_state and there was no order to halt.
    // 9x9 carves four clean interior provinces; (1,1) and (5,5) are in two of
    // them, verified by the precondition below rather than assumed.
    const entity_id body = make_grid_body(w, 9, 9);
    const entity_id a = add_corp(w, "A"), b = add_corp(w, "B");
    const entity_id t0 = grid_tile_at(w, body, 1, 1), t3 = grid_tile_at(w, body, 5, 5);
    partition(w);
    check(t0 != t3, "start and march target are different tiles (fixture precondition)");
    const entity_id unit = add_unit(w, a, t0);
    recipe_registry reg = registry_with_march(1.0f);

    corp_command march; march.corp = a; march.verb = corp_verb::march_unit; march.subject = unit;
    march.tile = t3;
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
    // 9x9 (was a 10-wide row). Under BL-511 a destination in the SAME province
    // as the unit's start is rejected_state, and since the 2026-08-21
    // repartition a 10-wide row is ONE province — so BOTH orders were refused
    // and this row proved nothing about visitation. What this test needs is
    // two units starting in DIFFERENT provinces and marching to a THIRD they
    // share: (1,1) and (5,1) are two of 9x9's clean interior provinces, and
    // (5,5) is a third. The preconditions below assert the separation rather
    // than assuming it.
    const entity_id body = make_grid_body(w, 9, 9);
    const entity_id tA = grid_tile_at(w, body, 1, 1), tC = grid_tile_at(w, body, 5, 1);
    // Two DIFFERENT tiles: two nations never share one (2026-10-27), so a shared
    // destination tile would leave whichever arrived second blocked short of it.
    const entity_id destA = grid_tile_at(w, body, 5, 5), destC = grid_tile_at(w, body, 6, 5);
    partition(w);
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

    for (int i = 0; i < 20; ++i) run_unit_march(w, reg);
    check(w.units.at(unit_b).position == destA, "b's (mobilised) unit still reaches its destination tile");
    check(w.units.at(unit_c).position == destC, "c's (peaceful) unit still reaches its destination tile");
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
    // 9x9 (was a 12-wide row). MEASURED, twice over: the 2026-08-21
    // repartition carves 3x3 blocks and then folds anything under
    // k_province_min_tiles into a neighbour, and on a one-row body EVERY block
    // is a 3-tile fragment — so a 12-wide row merges to a single province and
    // marching within it is rejected_state. (Rows only start yielding two
    // provinces at 20 tiles, which is why M2's 20-wide row still works.)
    // 9x9 gives four clean interior provinces; (1,1) and (5,5) sit in two of
    // them, and the preconditions below assert that rather than trusting it.
    t.body = make_grid_body(t.w, 9, 9);
    t.corp = add_corp(t.w, "Replay Corp");
    t.u_from = grid_tile_at(t.w, t.body, 1, 1);
    t.u_to   = grid_tile_at(t.w, t.body, 5, 5);
    partition(t.w); // BL-511: march_unit needs a built partition
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
    // The result is ASSERTED, not discarded. If the order is refused — which
    // is exactly what the 2026-08-21 repartition did to this fixture's old
    // one-row body — the loop below still runs eight ticks with nothing
    // marching, and "state_hash agrees" passes while proving nothing at all.
    check(apply_corp_command(t1.w, reg, m1) == corp_command_result::applied
              && apply_corp_command(t2.w, reg, m2) == corp_command_result::applied,
          "both replay orders were actually placed (fixture setup)");

    bool all_equal = true;
    int  ticks_in_flight = 0; // ticks that BEGAN with a live order — see below
    for (int tick = 1; tick <= 8; ++tick)
    {
        if (t1.w.units.at(t1.unit).order.dest != null_entity)
            ++ticks_in_flight;
        run_unit_march(t1.w, reg);
        run_unit_march(t2.w, reg);
        run_unit_upkeep(t1.w, reg, g_upkeep_report);
        run_unit_upkeep(t2.w, reg, g_upkeep_report);
        if (t1.w.state_hash(tick) != t2.w.state_hash(tick))
        {
            all_equal = false;
            std::printf("    tick %d: hash mismatch (0x%llx vs 0x%llx)\n", tick,
                        static_cast<unsigned long long>(t1.w.state_hash(tick)),
                        static_cast<unsigned long long>(t2.w.state_hash(tick)));
        }
    }
    check(all_equal, "state_hash agrees at every tick across two independent runs, marching units in flight");
    // "IN FLIGHT" IS PART OF THE CLAIM, so it is asserted rather than assumed.
    // A unit that arrives on tick 1 would leave seven of the eight hash
    // checkpoints comparing two idle worlds — still true, but no longer a
    // statement about marching. The bar is deliberately more than one tick.
    std::printf("    unit held a live order for %d of 8 ticks\n", ticks_in_flight);
    check(ticks_in_flight >= 3,
          "the unit was genuinely mid-march across multiple hash checkpoints, not arrived on tick 1");
    check(t1.w.units.at(t1.unit).position == t1.u_to && t2.w.units.at(t2.unit).position == t2.u_to,
          "both units actually reached the destination tile by the end of the replay");
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
    // 9x9 (was a 6-wide row). Post-repartition a 6-wide row is ONE province,
    // so the order below was rejected_state, `order.path` stayed EMPTY, and
    // the corruption line then indexed past the end of an empty vector — which
    // segfaulted the process mid-M5 and meant M6 and M7 never ran at all.
    // That is why the result is asserted now instead of discarded: a fixture
    // whose setup silently fails is worse than one that fails loudly.
    const entity_id body = make_grid_body(w, 9, 9);
    const entity_id corp = add_corp(w, "Corp");
    const entity_id start = grid_tile_at(w, body, 1, 1), dest = grid_tile_at(w, body, 5, 5);
    partition(w);
    const entity_id unit = add_unit(w, corp, start);

    corp_command cmd; cmd.corp = corp; cmd.verb = corp_verb::march_unit; cmd.subject = unit;
    cmd.tile = dest;
    check(apply_corp_command(w, reg, cmd) == corp_command_result::applied,
          "the order to corrupt was actually placed (fixture setup)");
    check(w.units.at(unit).order.path.size() > w.units.at(unit).order.next_index,
          "  ...and its path has a next hop to corrupt (fixture setup)");

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
    check(d1 == d2, "both runs name the same destination tile");
    check(w1.units.at(u1).position == d1 && w2.units.at(u2).position == d2,
          "recovery from the block still reaches the original destination TILE on both runs");
}


// ---------------------------------------------------------------------------
// M7 — BL-511: the untrusted input boundary, and the append-only enum
// ---------------------------------------------------------------------------
//
// A destination tile id can arrive over `--serve` from outside the process. The
// standing rule (io-standing-rules.md) is that it must be validated as the
// value that LANDS in the destination, that a violation rejects the WHOLE
// command, and that a rejection mutates NOTHING. There are TWO gates and they
// are not interchangeable:
//
//   * the WIRE gate (agent_protocol.cpp) proves the value FITS an entity_id
//     without a narrowing cast truncating or wrapping it into a different tile;
//   * the SEAM gate (the world's tile store) proves the value IS A TILE.
//
// This test owns the second. It sweeps the whole in-range-but-absent space it
// can reach — 0, UINT32_MAX, and every near-miss neighbour of a real id — and
// asserts each one is refused with the unit BYTE-IDENTICAL afterwards, order
// and position both.

void m7_untrusted_boundary_and_enum()
{
    std::printf("\n-- M7  an out-of-domain destination tile rejects whole and mutates nothing --\n");

    // --- the append-only enum property -----------------------------------
    // BL-511 retargeted march_unit's PAYLOAD. The verb is serialised, so its
    // VALUE and POSITION must be exactly what BL-470 gave it: a stored command
    // written before this change must still decode to the same verb.
    check(static_cast<uint8_t>(corp_verb::march_unit) == 21, "march_unit still holds value 21 (append-only)");
    check(static_cast<uint8_t>(corp_verb::halt_unit) == 22, "halt_unit still holds value 22 (append-only)");
    check(static_cast<uint8_t>(corp_verb::disband_unit) == 23, "disband_unit still holds value 23 (append-only)");
    // BL-467 (2026-08-21) APPENDED withdraw_from_battle, and this row caught it —
    // which is the row working, not breaking. It is NARROWED to what the guard is
    // actually for rather than deleted: the property was never "no verb is ever
    // added" (verbs are appended routinely), it is "no EXISTING verb renumbers,
    // and the wire range gate tracks the real last enumerator". The three pins
    // above are the first half and are untouched; these two are the second.
    check(static_cast<uint8_t>(corp_verb::withdraw_from_battle) == 24,
          "withdraw_from_battle was APPENDED at 24, renumbering nothing (BL-467)");
    // RE-SPECIFIED 2026-08-26. This pinned withdraw_from_battle as the LAST verb,
    // which contradicted the property stated two lines above ("verbs are appended
    // routinely"): it had already gone red at 28 verbs before Sprint 20 touched
    // anything, and BL-628's buy_corporation took it to 29. The real property is
    // that the range gate still COVERS this verb -- an appended verb must widen the
    // count, never leave it behind, which is what would silently truncate the wire.
    check(corp_verb_count > static_cast<uint8_t>(corp_verb::withdraw_from_battle),
          "corp_verb_count still covers withdraw_from_battle — the wire range gate is not stale");

    // --- the rejection property ------------------------------------------
    world w;
    // 9x9 (was a 12-wide row). Post-repartition a 12-wide row is ONE province,
    // so the "live order" this whole sweep is measured against could not be
    // placed. The sweep is only meaningful against state that COULD be
    // clobbered — see the comment at the order below.
    const entity_id body = make_grid_body(w, 9, 9);
    const entity_id a    = add_corp(w, "A");
    const entity_id t0   = grid_tile_at(w, body, 1, 1);
    const entity_id t9   = grid_tile_at(w, body, 5, 5);
    partition(w);
    const entity_id unit = add_unit(w, a, t0);
    recipe_registry reg  = registry_with_march(1.0f);

    const entity_id good = t9;
    check(w.tiles.count(good) != 0, "the fixture has a real destination tile (precondition)");

    // Put a LIVE order on the unit first: a rejection that mutates nothing is
    // only meaningfully tested against state that COULD be clobbered. An
    // untouched default order would pass a "mutates nothing" check even if the
    // seam wrote a fresh default over it.
    corp_command ok_cmd;
    ok_cmd.corp = a; ok_cmd.verb = corp_verb::march_unit; ok_cmd.subject = unit; ok_cmd.tile = good;
    check(apply_corp_command(w, reg, ok_cmd) == corp_command_result::applied, "a live order is in place (fixture setup)");

    const movement_order before_order = w.units.at(unit).order;
    const entity_id      before_pos   = w.units.at(unit).position;
    const std::size_t    before_units = w.units.size();

    // Every id worth trying: the unset default, the top of the domain, and a
    // window of near-misses around a REAL id — the values a fuzzer or a
    // confused agent actually produces. Only ids genuinely absent from the
    // partition are asserted on; a near-miss that happens to BE a province is
    // skipped rather than turned into a false pass.
    std::vector<entity_id> candidates;
    candidates.push_back(null_entity);
    candidates.push_back(std::numeric_limits<entity_id>::max());
    candidates.push_back(std::numeric_limits<entity_id>::max() - 1u);
    for (uint32_t d = 1; d <= 64; ++d)
    {
        candidates.push_back(good + d);
        if (good >= d) candidates.push_back(good - d);
    }
    // The bit-flip family: a single flipped bit of a valid id is exactly the
    // "narrowed / wrapped into a DIFFERENT province" failure the standing rule
    // names, and the one a range check alone would wave through.
    for (int bit = 0; bit < 32; ++bit)
        candidates.push_back(good ^ (1u << bit));

    int tried = 0, wrong_code = 0, mutated = 0;
    for (const entity_id bad_id : candidates)
    {
        if (w.tiles.count(bad_id) != 0)
            continue; // a real tile — not a rejection case
        ++tried;
        corp_command bad = ok_cmd; bad.tile = bad_id;
        if (apply_corp_command(w, reg, bad) != corp_command_result::rejected_invalid)
            ++wrong_code;
        const unit_component& u = w.units.at(unit);
        if (!order_equal(u.order, before_order) || u.position != before_pos
            || w.units.size() != before_units)
            ++mutated;
    }
    std::printf("    swept %d out-of-domain tile ids\n", tried);
    check(tried >= 32, "the sweep actually covered a meaningful number of absent ids");
    check(wrong_code == 0, "EVERY out-of-domain tile id is rejected_invalid — none is coerced");
    check(mutated == 0, "EVERY such rejection leaves the unit byte-identical — order, position, roster");

    // And the live order still works afterwards: the sweep proved rejection,
    // not paralysis.
    check(w.units.at(unit).order.dest == good, "the pre-existing valid order survived the whole sweep");
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
    m7_untrusted_boundary_and_enum();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
