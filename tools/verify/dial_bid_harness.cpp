// Headless harness: BL-1217 (G1 plants running) — the background workforce
// dial's buyer signal, `dial_bid` (components.hpp; AI_OPPONENT.md § 11, "the
// dial reads stock-fed consumers", Ben 2026-10-09): posted demand PLUS what
// running processors drew from their owners' pools and did not post as demand,
// held for the scorer's cadence, each unit counted once. No SDL / Lua / ImGui.
// Sibling of unposted_bid_harness; same build line.
//
// Every row drives the REAL economy step (run_economy_step: want, draw,
// collect_dial_pool_draws) and the REAL clear (clear_markets: demand and the
// dial record written together), and reads `dial_bid` at the age the scorer
// reads it — AGE 1: the scorer runs inside the NEXT tick's economy step, after
// that tick's production but before its clear, so it sees the previous clear's
// demand and dial record. (`at_age1` reads with current_econ_tick + 1.)
//
// Fixture: one body, one market M; a coal -> steel processor P owned by the
// PLAYER corp (so the scorer never dials it and the reflex never rescues it);
// coal on M's shelf where a row needs a shelf. N = P's full-run coal need, read
// off a calibration tick (a pool-fed tick's dial record).
//
//   D1 the pool runs dry and P moves to the shelf: N, N, N across the
//      transition (never 2N: the old draw is not held beside the new demand;
//      never 0: the first shelf tick's demand and record are the same clear's).
//   D2 a shelf buyer moves to a refilled pool: N (demand) then N (draw).
//   D3 a starved processor that still posts its want stamps 0 and replaces its
//      old held draw: N, with the record 0 at the new tick (not N + N).
//   D4 a processor that stops entirely (decommissioned: no want, no stamp) ages
//      out: its last draw is read through the hold (cadence 4), and 0 after.
//   D5 two processors on one market share one pool: their draws sum; when the
//      pool covers only one, the other's 0-stamp does not erase the first's draw.
//   D6 market_has_cleared on a market spawned mid-step (maybe_spawn_market):
//      false before its first clear even with an unposted bid recorded on it the
//      same step (the old reading would have said cleared), true after the clear.
// Exits non-zero on any FAIL.

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

namespace {

int g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

constexpr std::size_t r_coal  = static_cast<std::size_t>(resource_type::coal);
constexpr std::size_t r_steel = static_cast<std::size_t>(resource_type::steel);
constexpr int k_hold = 4; // the scorer's cadence (corp_ai_params::cadence_k)

bool near(float a, float b) { return std::fabs(a - b) <= 1e-4f * std::max(1.0f, std::fabs(b)); }

recipe_registry make_registry()
{
    recipe_registry reg;
    building_economics pe;
    pe.base_rate = 1.0f; pe.maintenance = 0.0f; pe.base_wage = 0.0f;
    pe.build_cost = 100.0f; pe.build_duration_ticks = 2.0f;
    reg.set_economics(building_type::processing_facility, pe);
    recipe coal_steel;
    coal_steel.name  = "coal_steel";
    coal_steel.group = "Foundry";
    coal_steel.inputs [r_coal]  = 4.0f;
    coal_steel.outputs[r_steel] = 1.0f;
    reg.add_recipe(coal_steel);
    return reg;
}

struct fixture
{
    world     w;
    entity_id body = null_entity, tile = null_entity, market = null_entity, corp = null_entity;
    entity_id p1 = null_entity, p2 = null_entity;
};

entity_id add_plant(fixture& f, const recipe_registry& reg)
{
    const entity_id p = f.w.create_entity();
    building_component b{};
    b.tile = f.tile; b.type = building_type::processing_facility;
    b.recipe = reg.recipe_id("coal_steel");
    b.target_resource = resource_type::steel;
    b.workforce_assigned = 1.0f;
    b.workforce_auto = false;
    f.w.buildings[p] = b;
    f.w.corporations.at(f.corp).assets.push_back(p);
    return p;
}

fixture make_fixture(const recipe_registry& reg, bool two_plants)
{
    fixture f;
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Anvil"; bc.type = body_type::planet; bc.grid_width = 2; bc.grid_height = 1;
    bc.survey.phase = survey_phase::surveyed;
    w.bodies[f.body] = bc;
    w.home_body = f.body;
    for (int c = 0; c < 2; ++c)
    {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body = f.body; tc.grid_x = c; tc.grid_y = 0;
        tc.substrate = terrain_substrate::rocky; tc.cover = terrain_cover::grass;
        w.tiles[t] = tc;
        if (c == 0) f.tile = t;
    }
    f.market = w.create_entity();
    market_component mc{};
    mc.body = f.body; mc.centre_tile = f.tile;
    mc.base_price[r_coal]  = 2.0f;
    mc.base_price[r_steel] = 20.0f;
    mc.price = mc.base_price;
    w.markets[f.market] = mc;

    f.corp = w.create_entity();
    corporation_component cc;
    cc.name = "Seat"; cc.is_player = true; cc.balance = 1.0e6f; cc.starting_capital = 1.0e6f;
    w.corporations[f.corp] = cc;
    w.player_entity = f.corp;
    f.p1 = add_plant(f, reg);
    if (two_plants)
        f.p2 = add_plant(f, reg);
    w.current_econ_tick = 1;
    return f;
}

/// Set P's pool coal and M's shelf coal exactly, then run one real economy step
/// and the real clear at the current tick, and advance the tick.
void tick(fixture& f, const recipe_registry& reg, float pool_coal, float shelf_coal)
{
    f.w.pool_at(f.corp, f.market).quantities[r_coal] = pool_coal;
    f.w.markets.at(f.market).inventory[r_coal]       = shelf_coal;
    economy_report rep = run_economy_step(f.w, reg, /*spectating=*/false);
    (void)clear_markets(f.w, reg, rep);
    ++f.w.current_econ_tick;
}

/// `dial_bid` as the scorer reads it: inside the next step, before its clear —
/// the record and demand of the clear just run, at age 1. `tick` has already
/// advanced current_econ_tick, so this is the current tick.
float at_age1(const fixture& f)
{
    return dial_bid(f.w.markets.at(f.market), r_coal, f.w.current_econ_tick, k_hold);
}

} // namespace

int main()
{
    std::printf("dial_bid_harness — BL-1217 the dial's buyer signal\n");
    const recipe_registry reg = make_registry();

    // Calibration: N, P's full-run coal need, off a pool-fed tick's record.
    float N = 0.0f;
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, 1000.0f, 0.0f);
        const market_component& m = f.w.markets.at(f.market);
        N = m.dial_pool_draw[r_coal];
        std::printf("  calibration: N = %.4f coal per run (demand %.4f)\n", N, m.demand[r_coal]);
        check(N > 0.0f && m.demand[r_coal] == 0.0f,
              "C0 not vacuous: a pool-fed P records a positive draw and posts no coal demand");
    }

    std::printf("D1 the pool runs dry: P moves to the shelf\n");
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, N, 10.0f * N);
        const float a = at_age1(f);
        tick(f, reg, N, 10.0f * N);
        const float b = at_age1(f);
        tick(f, reg, 0.0f, 10.0f * N); // the pool is dry: P posts N and buys off the shelf
        const market_component& m = f.w.markets.at(f.market);
        const float c = at_age1(f);
        const bool  posted = near(m.demand[r_coal], N) && m.dial_pool_draw[r_coal] == 0.0f;
        tick(f, reg, 0.0f, 10.0f * N);
        const float d = at_age1(f);
        std::printf("    dial_bid: %.4f %.4f | %.4f %.4f\n", a, b, c, d);
        check(near(a, N) && near(b, N), "D1 pool-fed ticks read N (the held draw)");
        check(posted, "D1 not vacuous: on the first dry tick P posts N as demand and its record is stamped 0");
        check(near(c, N), "D1 the transition tick reads N: never 2N (old draw beside new demand), never 0");
        check(near(d, N), "D1 ... and the next shelf tick reads N");
    }

    std::printf("D2 a shelf buyer moves to a refilled pool\n");
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, 0.0f, 10.0f * N);
        const float a = at_age1(f);
        const bool  on_shelf = near(f.w.markets.at(f.market).demand[r_coal], N);
        tick(f, reg, N, 10.0f * N);
        const float b = at_age1(f);
        const bool  on_pool = f.w.markets.at(f.market).demand[r_coal] == 0.0f
                           && near(f.w.markets.at(f.market).dial_pool_draw[r_coal], N);
        std::printf("    dial_bid: %.4f then %.4f\n", a, b);
        check(on_shelf && on_pool, "D2 not vacuous: P posts N on the shelf tick, draws N from the pool the next");
        check(near(a, N) && near(b, N), "D2 W then W: N (demand) then N (draw), never 2N");
    }

    std::printf("D3 a starved processor that still posts stamps 0\n");
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, N, 0.0f);
        const int t_fed = f.w.markets.at(f.market).dial_pool_draw_tick[r_coal];
        const float a = at_age1(f);
        tick(f, reg, 0.0f, 0.0f); // nothing in the pool or on the shelf: P is starved, still posts N
        const market_component& m = f.w.markets.at(f.market);
        const float b = at_age1(f);
        std::printf("    dial_bid: %.4f then %.4f (record %.4f at tick %d, was tick %d)\n", a, b,
                    m.dial_pool_draw[r_coal], m.dial_pool_draw_tick[r_coal], t_fed);
        check(near(a, N), "D3 the pool-fed tick reads N");
        check(near(m.demand[r_coal], N), "D3 not vacuous: the starved P still posts N as demand");
        check(m.dial_pool_draw[r_coal] == 0.0f && m.dial_pool_draw_tick[r_coal] > t_fed,
              "D3 ... and STAMPS 0 at the new tick, replacing its old held draw");
        check(near(b, N), "D3 the starved tick reads N, not N + N");
    }

    std::printf("D4 a processor that stops entirely ages out after the hold\n");
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, N, 0.0f);
        const int t_rec = f.w.markets.at(f.market).dial_pool_draw_tick[r_coal];
        f.w.buildings.at(f.p1).decommissioned = true; // no want, no draw, no stamp
        bool held = true;
        float last = 0.0f;
        int   age  = f.w.current_econ_tick - t_rec;
        for (; age <= k_hold; ++age)
        {
            held = held && near(at_age1(f), N) && f.w.markets.at(f.market).demand[r_coal] == 0.0f;
            tick(f, reg, 0.0f, 0.0f);
        }
        last = at_age1(f);
        std::printf("    record from tick %d; read at age %d = %.4f\n", t_rec, f.w.current_econ_tick - t_rec, last);
        check(f.w.markets.at(f.market).dial_pool_draw_tick[r_coal] == t_rec,
              "D4 not vacuous: a stopped processor writes no new record");
        check(held, "D4 its last draw is read at every age up to the hold (cadence 4)");
        check(last == 0.0f, "D4 ... and 0 once the record is older than the hold");
    }

    std::printf("D5 two processors share one pool\n");
    {
        fixture f = make_fixture(reg, true);
        tick(f, reg, 2.0f * N, 0.0f);
        const float both = f.w.markets.at(f.market).dial_pool_draw[r_coal];
        const float a = at_age1(f);
        tick(f, reg, N, 10.0f * N); // the pool covers ONE: the other posts N and buys off the shelf
        const market_component& m = f.w.markets.at(f.market);
        const float rec = m.dial_pool_draw[r_coal];
        const float b = at_age1(f);
        std::printf("    pool-fed both: record %.4f, dial %.4f | one: record %.4f demand %.4f dial %.4f\n",
                    both, a, rec, m.demand[r_coal], b);
        check(near(both, 2.0f * N) && near(a, 2.0f * N), "D5 two pool-fed processors' draws sum (2N)");
        check(near(m.demand[r_coal], N), "D5 not vacuous: when the pool covers one, the other posts N");
        check(near(rec, N), "D5 ... and its 0-stamp does not erase the first's draw (record N, not 0)");
        check(near(b, 2.0f * N), "D5 the dial reads both buyers once each (2N)");
    }

    std::printf("D6 market_has_cleared on a market spawned mid-step\n");
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, N, 0.0f); // the home market has cleared
        // A second body with no market; spawned the way a building's completion
        // spawns one inside the economy step.
        const entity_id b2 = f.w.create_entity();
        body_component b2c{};
        b2c.name = "Far"; b2c.type = body_type::planet; b2c.grid_width = 1; b2c.grid_height = 1;
        f.w.bodies[b2] = b2c;
        const entity_id t2 = f.w.create_entity();
        tile_component t2c{};
        t2c.body = b2; t2c.grid_x = 0; t2c.grid_y = 0;
        t2c.substrate = terrain_substrate::rocky; t2c.cover = terrain_cover::grass;
        f.w.tiles[t2] = t2c;
        const entity_id mid = maybe_spawn_market(f.w, reg, b2, t2);
        check(mid != null_entity, "D6 not vacuous: the market spawns");
        if (mid != null_entity)
        {
            // The same step's production pass records an unposted bid on it.
            note_unposted_bid(f.w.markets.at(mid), r_coal, 5.0f, f.w.current_econ_tick);
            f.w.pool_at(f.corp, mid).quantities[r_steel] = 50.0f; // a pool its first clear will list
            const market_component& m2 = f.w.markets.at(mid);
            check(m2.unposted_bid[r_coal] > 0.0f, "D6 not vacuous: it carries an unposted bid before any clear");
            check(!market_has_cleared(m2, f.w.current_econ_tick),
                  "D6 ... yet it reads as NEVER cleared before its first clear");
            economy_report none;
            (void)clear_markets(f.w, reg, none);
            check(market_has_cleared(f.w.markets.at(mid), f.w.current_econ_tick),
                  "D6 ... and as cleared after its first clear");
        }
    }

    std::printf(g_fail ? "FAIL (%d failures)\n" : "ALL PASS (0 failures)\n", g_fail);
    return g_fail ? 1 : 0;
}
