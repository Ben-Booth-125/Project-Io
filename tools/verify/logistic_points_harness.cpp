// ---------------------------------------------------------------------------
// Headless active-Logistic-Points harness (BL-596, LP_ACTIVE_MARCH)
// No SDL / Lua / ImGui — builds over the world/* logic alone.
// ---------------------------------------------------------------------------
// LOGISTICS.md § Logistic Points is the authority. Active LP is the first
// real consumer of the Logistic Points design: militaries draw against a
// per-anchor, per-tick RATE (`active_lp_anchor_pools`, logistics.hpp) before
// `run_unit_march` (economy_system.cpp) spends a unit's march points, and a
// granted draw is priced in credits separately from the LP cap itself.
//
//   L1  active_lp_anchor_pools is DETERMINISTIC and UNCACHED: two calls
//       against an unchanged world return byte-identical pools, and nothing
//       about the result depends on iteration order.
//   L2  A march WITHIN the anchor's pool: debits credits, consumes the pool
//       by exactly the LP drawn, and the unit moves exactly as
//       march_points_per_class alone would predict.
//   L3  A march that would EXCEED the pool is refused outright: the unit's
//       order/position/progress and the owning corp's balance are all
//       BYTE-IDENTICAL to the pre-tick snapshot, and the refusal is counted
//       (unit_march_tick::refused_no_lp).
//   L4  An anchorless body refuses every march — there is no active LP to
//       draw against at all (LOGISTICS.md constraint 2: cities are the
//       locus, never a per-corp allowance).
//   L5  CONTENTION: two corps' units near the same anchor, drawing more than
//       the pool can cover between them, resolve in the SAME deterministic
//       order run_unit_march already establishes (mobilised-first, then
//       ascending unit id) — identically across two independent runs.
//   L6  NO CARRY-OVER: running the identical scenario twice in a row within
//       one process (no explicit reset) produces the identical result both
//       times — the pool is recomputed fresh each call, never inherited from
//       the previous tick.
//
// The process exits non-zero if any assertion FAILs.

#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

constexpr uint16_t ROW_INFANTRY = 0; // Levy Spear — unit_class::infantry (unit_roster.cpp)

/// A gw-wide single-row land body, mirroring unit_march_harness.cpp's own
/// make_row_body — but WITHOUT that file's automatic anchor (this harness
/// tests anchor presence/absence directly, so it names its anchor explicitly
/// per test rather than getting one for free).
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
        tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
        tc.landform    = lf;
        w.tiles[t]     = tc;
    }
    return body;
}

entity_id tile_at(world& w, entity_id body, int c)
{
    return body_tile_grid(w, body)[static_cast<std::size_t>(c)];
}

/// Anchors @p tile as a city — the cheapest way to make a tile a supply
/// anchor for this harness (a population centre, per is_supply_anchor).
void add_anchor(world& w, entity_id tile)
{
    w.population_centre_tile[w.create_entity()] = tile;
}

entity_id add_corp(world& w, const char* name, float balance = 10000.0f)
{
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name    = name;
    cc.balance = balance;
    w.corporations[c] = cc;
    return c;
}

entity_id add_unit(world& w, entity_id corp, entity_id tile, int count = 50,
                   uint16_t type = ROW_INFANTRY)
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

/// A RAW legacy-grain order (dest_province == 0 — the back-compat tile path
/// unit_march_harness.cpp's own M2 uses), so this file needs no province
/// partition at all: what is under test here is the LP gate, not march_unit
/// or the province seam.
void order_along_row(world& w, entity_id unit, entity_id body, int from_col, int to_col)
{
    std::vector<entity_id> path;
    const int step = (to_col >= from_col) ? 1 : -1;
    for (int c = from_col; ; c += step)
    {
        path.push_back(tile_at(w, body, c));
        if (c == to_col) break;
    }
    movement_order mo;
    mo.dest       = path.back();
    mo.path       = path;
    mo.next_index = 1;
    mo.progress   = 0.0f;
    w.units.at(unit).order = mo;
}

/// A registry with infantry march points and LP rates set explicitly —
/// nothing defaults from economy.lua (this harness is Lua-free), so every
/// number in a test is named at the call site.
recipe_registry make_registry(float march_pts, float lp_per_anchor, float credit_per_unit_dist)
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    mp.march_points_per_class[static_cast<std::size_t>(unit_class::infantry)] = march_pts;
    mp.active_lp_per_anchor_tick          = lp_per_anchor;
    mp.active_lp_credit_per_unit_distance = credit_per_unit_dist;
    reg.set_military(mp);
    return reg;
}

bool order_equal(const movement_order& a, const movement_order& b)
{
    return a.dest == b.dest && a.dest_province == b.dest_province
        && a.path == b.path && a.next_index == b.next_index
        && approx(a.progress, b.progress, 1e-6f);
}

} // namespace

// ---------------------------------------------------------------------------
// L1 — pool generation is deterministic and uncached
// ---------------------------------------------------------------------------

void l1_pool_determinism()
{
    std::printf("\n-- L1  active_lp_anchor_pools: deterministic, uncached --\n");

    world w;
    const entity_id body = make_row_body(w, 10);
    const entity_id anchor_a = tile_at(w, body, 2);
    const entity_id anchor_b = tile_at(w, body, 7);
    add_anchor(w, anchor_a);
    add_anchor(w, anchor_b);

    const auto p1 = active_lp_anchor_pools(w, body, 12.5f);
    const auto p2 = active_lp_anchor_pools(w, body, 12.5f);

    check(p1.size() == 2, "two anchors on this body -> two pool entries");
    check(p1.size() == p2.size(), "two independent calls agree on entry count");
    bool all_match = (p1.size() == p2.size());
    for (const auto& [tile, val] : p1)
    {
        const auto it = p2.find(tile);
        if (it == p2.end() || !approx(it->second, val))
            all_match = false;
    }
    check(all_match, "every (anchor, rate) pair is byte-identical across two calls");
    check(p1.find(anchor_a) != p1.end() && approx(p1.at(anchor_a), 12.5f),
          "anchor A's pool equals the authored per-anchor rate");
    check(p1.find(anchor_b) != p1.end() && approx(p1.at(anchor_b), 12.5f),
          "anchor B's pool equals the authored per-anchor rate");

    // A zero rate or an anchorless body must return an EMPTY map, not a
    // zero-valued one — "no active LP exists here" is a distinct answer from
    // "an anchor exists with nothing left".
    const auto p_zero_rate = active_lp_anchor_pools(w, body, 0.0f);
    check(p_zero_rate.empty(), "a zero per-anchor rate yields no pool entries at all");

    world w2;
    const entity_id body2 = make_row_body(w2, 6); // no anchors placed
    const auto p_no_anchor = active_lp_anchor_pools(w2, body2, 10.0f);
    check(p_no_anchor.empty(), "an anchorless body yields no pool entries at all");
}

// ---------------------------------------------------------------------------
// L2 — a march within the pool debits credits, consumes the pool, moves
// ---------------------------------------------------------------------------

void l2_granted_draw()
{
    std::printf("\n-- L2  a march within the anchor's pool is granted --\n");

    world w;
    const entity_id body = make_row_body(w, 10);
    add_anchor(w, tile_at(w, body, 0));
    const entity_id corp = add_corp(w, "A", 10000.0f);
    const entity_id unit = add_unit(w, corp, tile_at(w, body, 0), /*count*/ 100);
    order_along_row(w, unit, body, 0, 5);

    // march_points 2.0/tick, plains costs 1.0/hop -> would_be_points == 2.0
    // this tick (no banked carry-over yet). Pool of 50.0 comfortably covers
    // it, so this is purely a "does the granted path work" check.
    recipe_registry reg = make_registry(/*march_pts*/ 2.0f, /*lp_per_anchor*/ 50.0f,
                                        /*credit_per_unit_dist*/ 0.1f);

    const float balance_before = w.corporations.at(corp).balance;
    const unit_march_tick r = run_unit_march(w, reg);

    check(r.refused_no_lp == 0, "no refusal — the pool comfortably covers this draw");
    check(w.units.at(unit).position == tile_at(w, body, 2),
          "the unit moved exactly as march_points_per_class alone predicts (2.0 pts / 1.0 per hop)");

    // credit_cost = rate * would_be_points * count = 0.1 * 2.0 * 100 = 20.0
    const float expected_cost = 0.1f * 2.0f * 100.0f;
    check(approx(w.corporations.at(corp).balance, balance_before - expected_cost),
          "the corp's balance is debited by rate x LP-drawn x head-count exactly");

    // The anchor's pool for THIS call was ephemeral — recomputing it fresh
    // must show the FULL rate again, not the drawn-down remainder, because
    // nothing about LP persists across a tick boundary.
    const auto pools_next = active_lp_anchor_pools(w, body, 50.0f);
    check(approx(pools_next.at(tile_at(w, body, 0)), 50.0f),
          "a freshly recomputed pool shows the full rate — the draw did not persist");
}

// ---------------------------------------------------------------------------
// L3 — a march exceeding the pool is refused outright, mutating nothing
// ---------------------------------------------------------------------------

void l3_refused_mutates_nothing()
{
    std::printf("\n-- L3  a march exceeding the pool is refused outright --\n");

    world w;
    const entity_id body = make_row_body(w, 10);
    add_anchor(w, tile_at(w, body, 0));
    const entity_id corp = add_corp(w, "A", 10000.0f);
    const entity_id unit = add_unit(w, corp, tile_at(w, body, 0), 100);
    order_along_row(w, unit, body, 0, 5);

    // march_pts 3.0 this tick, pool only 1.0 -> refused.
    recipe_registry reg = make_registry(3.0f, /*lp_per_anchor*/ 1.0f, 0.1f);

    const movement_order  order_before   = w.units.at(unit).order;
    const entity_id       position_before = w.units.at(unit).position;
    const float           balance_before  = w.corporations.at(corp).balance;

    const unit_march_tick r = run_unit_march(w, reg);

    check(r.refused_no_lp == 1, "the refusal is counted");
    check(r.marching == 1, "the unit still counts as having held a live order this tick");
    check(w.units.at(unit).position == position_before, "position is untouched");
    check(order_equal(w.units.at(unit).order, order_before), "the order (dest/path/next_index/progress) is untouched");
    check(approx(w.corporations.at(corp).balance, balance_before), "the corp's balance is untouched — no partial draw");
}

// ---------------------------------------------------------------------------
// L4 — an anchorless body refuses every march
// ---------------------------------------------------------------------------

void l4_no_anchor_no_march()
{
    std::printf("\n-- L4  an anchorless body has no active LP to draw against --\n");

    world w;
    const entity_id body = make_row_body(w, 6); // deliberately no add_anchor call
    const entity_id corp = add_corp(w, "A");
    const entity_id unit = add_unit(w, corp, tile_at(w, body, 0), 100);
    order_along_row(w, unit, body, 0, 3);

    recipe_registry reg = make_registry(2.0f, /*lp_per_anchor*/ 1.0e6f /* generous, doesn't matter */, 0.1f);
    const unit_march_tick r = run_unit_march(w, reg);

    check(r.refused_no_lp == 1, "refused — no anchor exists, so no active LP exists to draw against");
    check(w.units.at(unit).position == tile_at(w, body, 0), "the unit did not move");
}

// ---------------------------------------------------------------------------
// L5 — contention resolves in the SAME deterministic order across two runs
// ---------------------------------------------------------------------------

/// Build one contention fixture: two corps, each with one infantry unit
/// starting adjacent to a single anchor whose pool cannot cover both draws.
struct contention_fixture
{
    world      w;
    entity_id  body;
    entity_id  corp_a, corp_b;
    entity_id  unit_a, unit_b;
};

contention_fixture build_contention()
{
    contention_fixture f;
    f.body = make_row_body(f.w, 10);
    add_anchor(f.w, tile_at(f.w, f.body, 0));
    f.corp_a = add_corp(f.w, "A");
    f.corp_b = add_corp(f.w, "B");
    // Both units start one hop apart from the anchor and from each other, so
    // neither is closer to the anchor than the other in a way that would
    // make "nearest anchor" itself the tiebreaker — this test is about VISIT
    // ORDER, not distance.
    f.unit_a = add_unit(f.w, f.corp_a, tile_at(f.w, f.body, 1), 100);
    f.unit_b = add_unit(f.w, f.corp_b, tile_at(f.w, f.body, 2), 100);
    order_along_row(f.w, f.unit_a, f.body, 1, 5);
    order_along_row(f.w, f.unit_b, f.body, 2, 6);
    return f;
}

void l5_contention_order()
{
    std::printf("\n-- L5  contention resolves by the SAME deterministic priority run_unit_march already uses --\n");

    // Pool covers exactly ONE unit's draw (2.0), not both (4.0 combined) —
    // whichever unit is visited first (mobilised-first, then ascending id)
    // gets the draw; the other is refused.
    recipe_registry reg = make_registry(2.0f, /*lp_per_anchor*/ 2.0f, 0.05f);

    for (int run = 0; run < 2; ++run)
    {
        contention_fixture f = build_contention();
        const bool a_before_b = f.unit_a < f.unit_b;

        const unit_march_tick r = run_unit_march(f.w, reg);

        const std::string msg = "exactly one of the two units is refused (run "
                               + std::to_string(run) + ")";
        check(r.refused_no_lp == 1, msg.c_str());

        // At peace, ascending unit id decides visit order (run_unit_march's
        // own doc comment / NR-344 partition). The lower-id unit should have
        // moved; the higher-id one should be exactly where it started.
        const entity_id first_id  = a_before_b ? f.unit_a : f.unit_b;
        const entity_id second_id = a_before_b ? f.unit_b : f.unit_a;
        const entity_id first_start  = a_before_b ? tile_at(f.w, f.body, 1) : tile_at(f.w, f.body, 2);
        const entity_id second_start = a_before_b ? tile_at(f.w, f.body, 2) : tile_at(f.w, f.body, 1);

        check(f.w.units.at(first_id).position != first_start,
              "the lower-id (visited-first) unit moved");
        check(f.w.units.at(second_id).position == second_start,
              "the higher-id (visited-second) unit was refused and stayed put");
    }
}

// ---------------------------------------------------------------------------
// L6 — no carry-over: identical scenario run twice in the SAME process
// agrees with itself both times, proving the pool is fresh every call
// ---------------------------------------------------------------------------

void l6_no_hidden_carry_over()
{
    std::printf("\n-- L6  nothing about LP persists across a tick boundary --\n");

    // Two INDEPENDENT worlds built identically, ticked the SAME number of
    // times in the SAME process with no explicit cache reset between them —
    // if any hidden state carried over from one world's ticks into the
    // other's (a static, a thread-local, anything not scoped to the call),
    // this diverges. It should not: active_lp_anchor_pools recomputes fresh
    // every call and lives only on the stack of run_unit_march.
    recipe_registry reg = make_registry(1.5f, /*lp_per_anchor*/ 3.0f, 0.1f);

    auto run_scenario = [&]() {
        world w;
        const entity_id body = make_row_body(w, 12);
        add_anchor(w, tile_at(w, body, 0));
        const entity_id corp = add_corp(w, "A");
        const entity_id unit = add_unit(w, corp, tile_at(w, body, 0), 100);
        order_along_row(w, unit, body, 0, 8);
        int refusals = 0;
        for (int i = 0; i < 6; ++i)
            refusals += run_unit_march(w, reg).refused_no_lp;
        return std::pair<entity_id, int>{ w.units.at(unit).position, refusals };
    };

    const auto first  = run_scenario();
    const auto second = run_scenario();

    check(first.first == second.first,
          "final position agrees across two runs of the identical scenario, back to back");
    check(first.second == second.second,
          "refusal count agrees across two runs of the identical scenario, back to back");

    // A THIRD run, this time interleaved tick-by-tick with a fresh second
    // world, so any accidental global/static state would have a chance to
    // bleed between the two before either finishes.
    world wa, wb;
    const entity_id body_a = make_row_body(wa, 12), body_b = make_row_body(wb, 12);
    add_anchor(wa, tile_at(wa, body_a, 0));
    add_anchor(wb, tile_at(wb, body_b, 0));
    const entity_id corp_a = add_corp(wa, "A"), corp_b = add_corp(wb, "A");
    const entity_id unit_a = add_unit(wa, corp_a, tile_at(wa, body_a, 0), 100);
    const entity_id unit_b = add_unit(wb, corp_b, tile_at(wb, body_b, 0), 100);
    order_along_row(wa, unit_a, body_a, 0, 8);
    order_along_row(wb, unit_b, body_b, 0, 8);
    for (int i = 0; i < 6; ++i)
    {
        run_unit_march(wa, reg);
        run_unit_march(wb, reg);
    }
    check(wa.units.at(unit_a).position == wb.units.at(unit_b).position,
          "interleaving two independent worlds' ticks changes nothing about either's outcome");
}

int main()
{
    std::printf("=== logistic_points_harness (BL-596, LP_ACTIVE_MARCH) ===\n");

    l1_pool_determinism();
    l2_granted_draw();
    l3_refused_mutates_nothing();
    l4_no_anchor_no_march();
    l5_contention_order();
    l6_no_hidden_carry_over();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
