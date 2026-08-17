// Headless harness for the supply layer (BL-039 / BL-038 / BL-045 / BL-354).
// Verifies requirements R1, R4, R5, R6, R7, R8 and econ-tick orbital purity
// without SDL / Lua / ImGui.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src tools\verify\supply_advance.cpp ^
//      src\world\world.cpp src\world\supply_system.cpp ^
//      src\world\market_clearing.cpp src\world\orbital_system.cpp ^
//      /Fo:build_gen\verify\supply_advance\ /Fe:build_gen\verify\supply_advance.exe
// Run:
//   .\build_gen\verify\supply_advance.exe

#include "world/components.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <vector>

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;

static void check(bool cond, const char* what, float got = 0.0f, float want = 0.0f)
{
    if (cond)
        std::printf("  PASS  %s\n", what);
    else
    {
        std::printf("  FAIL  %s   (got %.4f, want %.4f)\n", what, got, want);
        ++g_failures;
    }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// ---------------------------------------------------------------------------
// R1: advance_convoys increments progress; sets arrived when >= 1.0
// ---------------------------------------------------------------------------
static void test_advance_convoys()
{
    std::printf("--- R1: advance_convoys ---\n");
    world w;

    convoy_component c;
    c.speed    = 0.4f;
    c.progress = 0.0f;
    c.arrived  = false;
    w.convoys.push_back(c);

    advance_convoys(w);
    check(!w.convoys[0].arrived, "not arrived after first advance (0.4 < 1.0)");
    check(near(w.convoys[0].progress, 0.4f), "progress == 0.4 after 1 tick",
          w.convoys[0].progress, 0.4f);

    advance_convoys(w);
    check(!w.convoys[0].arrived, "not arrived after second advance (0.8 < 1.0)");
    check(near(w.convoys[0].progress, 0.8f), "progress == 0.8 after 2 ticks",
          w.convoys[0].progress, 0.8f);

    advance_convoys(w);
    check(w.convoys[0].arrived, "arrived after third advance (1.2 >= 1.0)");
    check(near(w.convoys[0].progress, 1.0f), "progress clamped to 1.0",
          w.convoys[0].progress, 1.0f);
}

// ---------------------------------------------------------------------------
// R4: logistics cost constants reachable via recipe_registry
// ---------------------------------------------------------------------------
static void test_logistics_constants()
{
    std::printf("--- R4: logistics constants ---\n");
    recipe_registry reg;
    // Defaults from economy.lua: land=0.02, sea=0.05, air=0.15, space=1.00
    check(near(reg.logistics_cost(convoy_mode::land),  0.02f), "land cost == 0.02",
          reg.logistics_cost(convoy_mode::land), 0.02f);
    check(near(reg.logistics_cost(convoy_mode::sea),   0.05f), "sea cost == 0.05",
          reg.logistics_cost(convoy_mode::sea), 0.05f);
    check(near(reg.logistics_cost(convoy_mode::air),   0.15f), "air cost == 0.15",
          reg.logistics_cost(convoy_mode::air), 0.15f);
    check(near(reg.logistics_cost(convoy_mode::space), 1.00f), "space cost == 1.00",
          reg.logistics_cost(convoy_mode::space), 1.00f);

    // set_logistics_cost mutates correctly
    reg.set_logistics_cost(convoy_mode::land, 0.03f);
    check(near(reg.logistics_cost(convoy_mode::land), 0.03f), "set_logistics_cost mutates",
          reg.logistics_cost(convoy_mode::land), 0.03f);
}

// ---------------------------------------------------------------------------
// R5: dispatch_convoys debits corp balance by logistics_cost × distance × qty
// R6: space-mode requires launchpad on source body (gate check)
// ---------------------------------------------------------------------------
static void test_dispatch_and_gate()
{
    std::printf("--- R5 + R6: dispatch_convoys ---\n");
    world w;
    recipe_registry reg;

    // Two bodies: src (has launchpad + iron surplus) and dest (iron shortfall in market).
    entity_id src_body  = w.create_entity();
    entity_id dest_body = w.create_entity();
    entity_id corp_id   = w.create_entity();

    // Bodies at known positions (1 AU apart along x-axis → distance = 1 AU).
    body_component bsrc;
    bsrc.name             = "Source";
    bsrc.type             = body_type::planet;
    bsrc.orbital_radius_au = 1.0f;
    bsrc.orbital_angle_rad = 0.0f;  // (1,0)
    bsrc.grid_width = bsrc.grid_height = 4;
    w.bodies[src_body] = bsrc;

    body_component bdst;
    bdst.name             = "Dest";
    bdst.type             = body_type::planet;
    bdst.orbital_radius_au = 2.0f;
    bdst.orbital_angle_rad = 0.0f;  // (2,0) → distance = 1.0 AU
    bdst.grid_width = bdst.grid_height = 4;
    w.bodies[dest_body] = bdst;

    // Corp with starting balance.
    corporation_component corp;
    corp.balance = 500.0f;
    corp.is_player = true;
    w.corporations[corp_id] = corp;
    w.player_entity = corp_id;

    // Dest market with iron shortfall (demand > supply).
    entity_id dest_mkt = w.create_entity();
    {
        market_component mc{};
        mc.body = dest_body;
        mc.supply[ri(resource_type::iron_ore)] = 0.0f;
        mc.demand[ri(resource_type::iron_ore)] = 50.0f;
        w.markets[dest_mkt] = mc;
    }

    // Source pool with iron surplus.
    w.pool_for(corp_id, src_body).quantities[ri(resource_type::iron_ore)] = 100.0f;

    // --- Test gate: NO launchpad on source body → no convoy dispatched ---
    dispatch_convoys(w, reg,
                     reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space));
    check(w.convoys.empty(), "R6: no convoy without launchpad");
    const float balance_before_gate = w.corporations.at(corp_id).balance;
    check(near(balance_before_gate, 500.0f), "R6: balance unchanged without launchpad",
          balance_before_gate, 500.0f);

    // --- Add launchpad building on source body ---
    entity_id tile_id   = w.create_entity();
    entity_id asset_id  = w.create_entity();
    {
        tile_component tc{};
        tc.body = src_body;
        tc.grid_x = 0; tc.grid_y = 0;
        w.tiles[tile_id] = tc;
    }
    {
        building_component bc{};
        bc.tile = tile_id;
        bc.type = building_type::launchpad;
        w.buildings[asset_id] = bc;
    }
    w.corporations[corp_id].assets.push_back(asset_id);

    // --- BL-308 gate: a pad with NO propellant is as shut as no pad at all ---
    dispatch_convoys(w, reg,
                     reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space));
    check(w.convoys.empty(), "R6: no convoy from an UNFUELLED launchpad (BL-308)");
    check(near(w.corporations.at(corp_id).balance, 500.0f),
          "R6: balance unchanged without propellant",
          w.corporations.at(corp_id).balance, 500.0f);

    // Fuel the pad. 3 units of propellant on the source body; a launch burns 1.
    w.pool_for(corp_id, src_body).quantities[ri(resource_type::propellant)] = 3.0f;

    // Also add a source market (needed by credit_arrived_convoys).
    entity_id src_mkt = w.create_entity();
    {
        market_component mc{};
        mc.body = src_body;
        w.markets[src_mkt] = mc;
    }

    dispatch_convoys(w, reg,
                     reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space));

    check(!w.convoys.empty(), "R5: convoy dispatched with launchpad");

    if (!w.convoys.empty())
    {
        const convoy_component& conv = w.convoys[0];
        check(conv.cargo_resource == resource_type::iron_ore, "cargo resource = iron_ore");
        check(near(conv.cargo_qty, 50.0f), "cargo qty = shortfall (50)",
              conv.cargo_qty, 50.0f);
        check(conv.mode == convoy_mode::space, "mode = space");

        // cost = space_rate × distance × qty = 1.0 × 1.0 × 50 = 50
        const float expected_cost = 1.0f * 1.0f * 50.0f;
        const float balance_after = w.corporations.at(corp_id).balance;
        check(near(balance_after, 500.0f - expected_cost),
              "R5: corp balance debited by logistics cost",
              balance_after, 500.0f - expected_cost);

        // Source pool debited.
        const float src_qty = w.pool_for(corp_id, src_body).quantities[ri(resource_type::iron_ore)];
        check(near(src_qty, 100.0f - 50.0f), "R5: source pool debited",
              src_qty, 100.0f - 50.0f);

        // BL-308: the launch burned exactly one unit of propellant — per LAUNCH,
        // not per unit of cargo and not per AU.
        const float prop_left = w.pool_for(corp_id, src_body).quantities[ri(resource_type::propellant)];
        check(near(prop_left, 3.0f - 1.0f), "R6: one launch burns one propellant (BL-308)",
              prop_left, 2.0f);
    }
}

// ---------------------------------------------------------------------------
// R7 + R8: credit_arrived_convoys credits the pool and leaves market supply
// alone (BL-382 — a direct supply write was zeroed by the next clear_markets
// before pricing read it, while the pre-clear AI scorer did read it; the cargo
// reaches supply via the auto-surplus path off the pool instead)
// ---------------------------------------------------------------------------
static void test_credit_arrived()
{
    std::printf("--- R7 + R8: credit_arrived_convoys ---\n");
    world w;

    entity_id dest_body = w.create_entity();
    entity_id corp_id   = w.create_entity();
    entity_id dest_mkt  = w.create_entity();

    body_component bc{};
    bc.name = "Dest"; bc.type = body_type::planet;
    bc.orbital_radius_au = 1.0f; bc.orbital_angle_rad = 0.0f;
    bc.grid_width = bc.grid_height = 4;
    w.bodies[dest_body] = bc;

    market_component mc{};
    mc.body = dest_body;
    mc.supply[ri(resource_type::iron_ore)] = 10.0f;
    w.markets[dest_mkt] = mc;

    // Pre-seed the dest pool.
    w.pool_for(corp_id, dest_body).quantities[ri(resource_type::iron_ore)] = 5.0f;

    // Arrived convoy carrying 30 iron.
    convoy_component cv{};
    cv.dest_market    = dest_mkt;
    cv.cargo_resource = resource_type::iron_ore;
    cv.cargo_qty      = 30.0f;
    cv.progress       = 1.0f;
    cv.arrived        = true;
    cv.corp           = corp_id;
    w.convoys.push_back(cv);

    credit_arrived_convoys(w);

    check(w.convoys.empty(), "convoy retired after credit");

    const float pool_qty = w.pool_for(corp_id, dest_body).quantities[ri(resource_type::iron_ore)];
    check(near(pool_qty, 5.0f + 30.0f), "R7: pool credited",
          pool_qty, 5.0f + 30.0f);

    const float mkt_supply = w.markets.at(dest_mkt).supply[ri(resource_type::iron_ore)];
    check(near(mkt_supply, 10.0f), "R8: market supply untouched (BL-382)",
          mkt_supply, 10.0f);
}

// ---------------------------------------------------------------------------
// R8: two-body price convergence via repeated convoy deliveries
//
// Body A: abundant iron supply (large corp pool) → low market price.
// Body B: iron demand only (no supply, no pool) → price at ceiling.
// Each delivery tick: seed an arrived convoy, credit it, clear markets.
// After 8 deliveries the body B pool accumulates → price falls.
// ---------------------------------------------------------------------------
static void test_price_convergence()
{
    std::printf("--- R8: two-body price convergence ---\n");
    world w;
    recipe_registry reg;

    entity_id body_a = w.create_entity();
    entity_id body_b = w.create_entity();
    entity_id corp   = w.create_entity();

    body_component ba{};
    ba.name = "BodyA"; ba.type = body_type::planet;
    ba.orbital_radius_au = 1.0f; ba.orbital_angle_rad = 0.0f;
    ba.grid_width = ba.grid_height = 4;
    w.bodies[body_a] = ba;

    body_component bb{};
    bb.name = "BodyB"; bb.type = body_type::planet;
    bb.orbital_radius_au = 2.0f; bb.orbital_angle_rad = 0.0f;
    bb.grid_width = bb.grid_height = 4;
    w.bodies[body_b] = bb;

    corporation_component cc{};
    cc.balance = 1000.0f;
    w.corporations[corp] = cc;

    entity_id mkt_a = w.create_entity();
    {
        market_component mc{};
        mc.body = body_a;
        mc.base_price[ri(resource_type::iron_ore)] = 1.0f;
        mc.price[ri(resource_type::iron_ore)]      = 1.0f;
        w.markets[mkt_a] = mc;
    }
    w.pool_for(corp, body_a).quantities[ri(resource_type::iron_ore)] = 500.0f;

    entity_id mkt_b = w.create_entity();
    {
        market_component mc{};
        mc.body = body_b;
        mc.base_price[ri(resource_type::iron_ore)] = 1.0f;
        mc.price[ri(resource_type::iron_ore)]      = 1.0f;
        w.markets[mkt_b] = mc;
    }

    // Standing iron_ore shortfall on body B drives demand each tick.
    //
    // BL-441 moved the demand register from `purchases` to `wants`: `purchases`
    // is now the FILL (the money side — what a consumer was actually delivered
    // and billed for) and `wants` is the BID (what it set out to buy, whether or
    // not the draw succeeded). This fixture is the bid — a standing shortfall on
    // a body with NO supply, which is by construction a want that is never
    // filled — so it has to be authored on `wants`. Left on `purchases` alone it
    // registered zero demand and body B's price sat exactly on base, which is
    // what this row caught at integration. `purchases` is kept alongside because
    // the corp is still the party being charged; the two carrying the same number
    // here is a property of the fixture, not of the engine.
    economy_report report;
    std::array<float, resource_count> shortfall{};
    shortfall[ri(resource_type::iron_ore)] = 50.0f;
    report.wants[{corp, body_b}]     = shortfall;
    report.purchases[{corp, body_b}] = shortfall;

    // --- Phase 1: diverge without convoys (3 ticks to settle EMA) ---
    for (int i = 0; i < 3; ++i)
        clear_markets(w, reg, report);

    const float price_b_before = w.markets.at(mkt_b).price[ri(resource_type::iron_ore)];
    check(price_b_before > 1.0f,
          "divergence: body B price above base (demand, no supply)",
          price_b_before, 1.0f);

    // --- Phase 2: deliver 60 iron to body B each tick ---
    for (int tick = 0; tick < 8; ++tick)
    {
        convoy_component cv{};
        cv.dest_market    = mkt_b;
        cv.cargo_resource = resource_type::iron_ore;
        cv.cargo_qty      = 60.0f;
        cv.progress       = 1.0f;
        cv.arrived        = true;
        cv.corp           = corp;
        w.convoys.push_back(cv);

        credit_arrived_convoys(w);
        clear_markets(w, reg, report);
    }

    const float price_b_after = w.markets.at(mkt_b).price[ri(resource_type::iron_ore)];
    check(price_b_after < price_b_before,
          "convergence: body B price falls after repeated convoy deliveries",
          price_b_after, price_b_before);

    std::printf("  INFO  price_b: before=%.3f  after=%.3f\n", price_b_before, price_b_after);
}

// ---------------------------------------------------------------------------
// BL-354: econ-tick orbital purity — dispatch is a pure function of tick.
//
// Two candidate source bodies on moving orbits compete to fill a shortfall on a
// third. Run the same authored world twice over three econ ticks; in run B,
// scribble garbage into every body's live orbital_angle_rad before each dispatch
// (simulating arbitrary frame-rate drift). Source choice, dispatch cost, convoy
// speed and arrival step count must be identical — dispatch may read only the
// tick-pure orbital_angle_at_tick.
// ---------------------------------------------------------------------------

struct purity_trace
{
    std::vector<float>     costs;         ///< Balance debited per dispatch.
    std::vector<float>     speeds;        ///< Convoy speed (1 / distance AU).
    std::vector<float>     qtys;          ///< Cargo quantity per dispatch.
    std::vector<entity_id> sources;       ///< Chosen source market per dispatch.
    std::vector<int>       arrival_steps; ///< advance_convoys calls until arrival.
};

static purity_trace run_purity_world(bool perturb_angles)
{
    world w;
    recipe_registry reg;

    entity_id corp_id = w.create_entity();
    entity_id src_a   = w.create_entity();
    entity_id src_b   = w.create_entity();
    entity_id dest    = w.create_entity();

    auto author_body = [&](entity_id id, const char* name, float radius,
                           float epoch, float omega) {
        body_component bc{};
        bc.name = name;
        bc.type = body_type::planet;
        bc.orbital_radius_au                    = radius;
        bc.orbital_angle_rad                    = epoch;
        bc.orbital_epoch_angle_rad              = epoch;
        bc.orbital_angular_velocity_rad_per_day = omega;
        bc.grid_width = bc.grid_height = 4;
        w.bodies[id] = bc;
    };
    author_body(src_a, "SrcA", 1.0f, 0.3f, 0.05f);
    author_body(src_b, "SrcB", 3.0f, 4.0f, 0.02f);
    author_body(dest,  "Dest", 2.0f, 1.0f, 0.01f);

    corporation_component corp;
    corp.balance = 100000.0f;
    w.corporations[corp_id] = corp;

    // A market on every body (sources need one so the convoy records its origin).
    entity_id dest_mkt = null_entity;
    for (entity_id body : { src_a, src_b, dest })
    {
        entity_id mkt = w.create_entity();
        market_component mc{};
        mc.body = body;
        w.markets[mkt] = mc;
        if (body == dest)
            dest_mkt = mkt;
    }

    // Fuelled launchpad on both source bodies.
    for (entity_id body : { src_a, src_b })
    {
        entity_id tile = w.create_entity();
        {
            tile_component tc{};
            tc.body = body;
            w.tiles[tile] = tc;
        }
        entity_id pad = w.create_entity();
        {
            building_component bc{};
            bc.tile = tile;
            bc.type = building_type::launchpad;
            w.buildings[pad] = bc;
        }
        w.corporations[corp_id].assets.push_back(pad);
    }

    purity_trace trace;
    for (const int tick : { 90, 180, 270 })
    {
        // Fresh shortfall + surpluses each econ tick.
        w.markets.at(dest_mkt).demand[ri(resource_type::iron_ore)] = 50.0f;
        w.markets.at(dest_mkt).supply[ri(resource_type::iron_ore)] = 0.0f;
        for (entity_id body : { src_a, src_b })
        {
            w.pool_for(corp_id, body).quantities[ri(resource_type::iron_ore)]   = 100.0f;
            w.pool_for(corp_id, body).quantities[ri(resource_type::propellant)] = 5.0f;
        }

        w.current_day_tick = tick;

        // Run B: the live angles carry arbitrary frame-drift garbage. Dispatch
        // must not notice.
        if (perturb_angles)
            for (auto& [id, body] : w.bodies)
                body.orbital_angle_rad = 5.777f + 0.13f * static_cast<float>(id + tick);

        const float balance_before = w.corporations.at(corp_id).balance;
        dispatch_convoys(w, reg,
                         reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));

        trace.costs.push_back(balance_before - w.corporations.at(corp_id).balance);
        if (w.convoys.size() == 1)
        {
            trace.speeds.push_back(w.convoys[0].speed);
            trace.qtys.push_back(w.convoys[0].cargo_qty);
            trace.sources.push_back(w.convoys[0].source_market);
        }
        else
        {
            trace.speeds.push_back(-1.0f);
            trace.qtys.push_back(-1.0f);
            trace.sources.push_back(null_entity);
        }

        int steps = 0;
        while (!w.convoys.empty() && !w.convoys[0].arrived && steps < 10000)
        {
            advance_convoys(w);
            ++steps;
        }
        trace.arrival_steps.push_back(steps);
        credit_arrived_convoys(w, tick);
    }
    return trace;
}

static void test_orbital_purity()
{
    std::printf("--- BL-354: econ-tick orbital purity ---\n");

    // The tick-pure angle reads only authored fields: scribbling the live angle
    // changes nothing, and the same tick always reproduces the same angle.
    {
        body_component bc{};
        bc.orbital_radius_au                    = 1.0f;
        bc.orbital_epoch_angle_rad              = 0.7f;
        bc.orbital_angular_velocity_rad_per_day = 0.02f;
        const float before = orbital_angle_at_tick(bc, 500);
        bc.orbital_angle_rad = 9.9f; // garbage live angle
        const float after   = orbital_angle_at_tick(bc, 500);
        check(before == after, "orbital_angle_at_tick ignores live orbital_angle_rad",
              after, before);
        check(orbital_angle_at_tick(bc, 500) == before,
              "orbital_angle_at_tick reproducible from tick alone",
              orbital_angle_at_tick(bc, 500), before);
        check(orbital_angle_at_tick(bc, 501) != before,
              "orbital_angle_at_tick advances with the tick",
              orbital_angle_at_tick(bc, 501), before);
    }

    const purity_trace clean     = run_purity_world(false);
    const purity_trace perturbed = run_purity_world(true);

    bool convoys_each_tick = true;
    for (const float s : clean.speeds)
        if (s <= 0.0f)
            convoys_each_tick = false;
    check(convoys_each_tick, "one convoy dispatched every econ tick");

    for (std::size_t i = 0; i < clean.costs.size(); ++i)
    {
        std::printf("  INFO  tick %zu: cost=%.4f speed=%.4f steps=%d src=%u\n",
                    i, clean.costs[i], clean.speeds[i], clean.arrival_steps[i],
                    clean.sources[i]);
        check(clean.costs[i] == perturbed.costs[i],
              "dispatch cost identical under angle perturbation",
              perturbed.costs[i], clean.costs[i]);
        check(clean.speeds[i] == perturbed.speeds[i],
              "convoy speed identical under angle perturbation",
              perturbed.speeds[i], clean.speeds[i]);
        check(clean.qtys[i] == perturbed.qtys[i],
              "cargo qty identical under angle perturbation",
              perturbed.qtys[i], clean.qtys[i]);
        check(clean.sources[i] == perturbed.sources[i],
              "source choice identical under angle perturbation",
              static_cast<float>(perturbed.sources[i]),
              static_cast<float>(clean.sources[i]));
        check(clean.arrival_steps[i] == perturbed.arrival_steps[i],
              "arrival step count identical under angle perturbation",
              static_cast<float>(perturbed.arrival_steps[i]),
              static_cast<float>(clean.arrival_steps[i]));
    }
}

// ---------------------------------------------------------------------------

int main()
{
    std::printf("supply_advance harness — BL-039 supply layer\n\n");
    test_advance_convoys();
    test_logistics_constants();
    test_dispatch_and_gate();
    test_credit_arrived();
    test_price_convergence();
    test_orbital_purity();

    std::printf("\n%s  (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "SOME FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures > 0 ? 1 : 0;
}
