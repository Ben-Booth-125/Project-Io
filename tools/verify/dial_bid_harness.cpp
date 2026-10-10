// Headless harness: BL-1217 (G1 plants running) — the background workforce
// dial's buyer signal, `dial_bid` (components.hpp; AI_OPPONENT.md § 11). No
// SDL / Lua / ImGui. Sibling of unposted_bid_harness; same build line.
//
// BL-1265 (MARKETS.md § The shelf economy, Ben 2026-10-10): corporations hold
// no pools, so every want a processor has is POSTED demand, and the dial's
// stock-fed draw register retired with the pool — `dial_bid` collapses to the
// market's posted demand. The rows that tested the pool-draw register (a pool
// running dry, a refilled pool, two processors sharing a pool, the draw held
// for the cadence) retired with it; what stands is the collapsed rule, checked
// on the real step and clear, and D6, which never read a pool.
//
// Every row drives the REAL economy step and the REAL clear, and reads
// `dial_bid` at the age the scorer reads it — AGE 1: the scorer runs inside the
// NEXT tick's economy step, after that tick's production but before its clear.
//
// Fixture: one body, one market M; a coal -> steel processor P owned by the
// PLAYER corp (so the scorer never dials it and the reflex never rescues it);
// coal on M's shelf. N = P's full-run coal need, read off a calibration tick.
//
//   D1 a shelf-fed P posts N as demand, and the dial reads N, tick after tick.
//   D3 a starved P (an empty shelf) still posts its whole want: the dial reads N.
//   D4 a P that stops entirely (decommissioned: no want) reads 0 the next tick —
//      nothing is held any more.
//   D5 two processors on one shelf: their posted wants sum (2N).
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

/// Set M's shelf coal exactly, then run one real economy step and the real
/// clear at the current tick, and advance the tick. (BL-1265: no pool to set.)
void tick(fixture& f, const recipe_registry& reg, float shelf_coal)
{
    f.w.markets.at(f.market).inventory[r_coal] = shelf_coal;
    economy_report rep = run_economy_step(f.w, reg, /*spectating=*/false);
    (void)clear_markets(f.w, reg, rep);
    ++f.w.current_econ_tick;
}

/// `dial_bid` as the scorer reads it: inside the next step, before its clear —
/// the demand of the clear just run, at age 1. `tick` has already advanced
/// current_econ_tick, so this is the current tick.
float at_age1(const fixture& f)
{
    return dial_bid(f.w.markets.at(f.market), r_coal, f.w.current_econ_tick, k_hold);
}

} // namespace

int main()
{
    std::printf("dial_bid_harness — BL-1217 the dial's buyer signal (BL-1265: posted demand)\n");
    const recipe_registry reg = make_registry();

    // Calibration: N, P's full-run coal need, off a shelf-fed tick's posted demand.
    float N = 0.0f;
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, 1000.0f);
        const market_component& m = f.w.markets.at(f.market);
        N = m.demand[r_coal];
        std::printf("  calibration: N = %.4f coal per run (posted demand)\n", N);
        check(N > 0.0f, "C0 not vacuous: a shelf-fed P posts a positive coal demand");
    }

    std::printf("D1 a shelf-fed P: the dial reads its posted want\n");
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, 10.0f * N);
        const float a = at_age1(f);
        tick(f, reg, 10.0f * N);
        const float b = at_age1(f);
        std::printf("    dial_bid: %.4f %.4f\n", a, b);
        check(near(a, N) && near(b, N), "D1 every shelf-fed tick reads N (posted demand, never 2N)");
    }

    std::printf("D3 a starved processor still posts its whole want\n");
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, 0.0f); // nothing on the shelf: P is starved, still posts N
        const market_component& m = f.w.markets.at(f.market);
        const float b = at_age1(f);
        std::printf("    dial_bid: %.4f (demand %.4f)\n", b, m.demand[r_coal]);
        check(near(m.demand[r_coal], N), "D3 not vacuous: the starved P still posts N as demand");
        check(near(b, N), "D3 the starved tick reads N");
    }

    std::printf("D4 a processor that stops entirely reads 0 the next tick\n");
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, 10.0f * N);
        const float a = at_age1(f);
        f.w.buildings.at(f.p1).decommissioned = true; // no want
        tick(f, reg, 10.0f * N);
        const float b = at_age1(f);
        std::printf("    dial_bid: %.4f then %.4f\n", a, b);
        check(near(a, N), "D4 not vacuous: the running P read N");
        check(b == 0.0f, "D4 once it stops, the dial reads 0 (no stock-fed draw is held any more)");
    }

    std::printf("D5 two processors on one shelf\n");
    {
        fixture f = make_fixture(reg, true);
        tick(f, reg, 10.0f * N);
        const float a = at_age1(f);
        std::printf("    dial_bid: %.4f\n", a);
        check(near(a, 2.0f * N), "D5 two shelf-fed processors' posted wants sum (2N)");
    }

    std::printf("D6 market_has_cleared on a market spawned mid-step\n");
    {
        fixture f = make_fixture(reg, false);
        tick(f, reg, 10.0f * N); // the home market has cleared
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
            f.w.land_goods(f.corp, mid, r_steel, 50.0f); // a landing its first clear will list (BL-1265)
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
