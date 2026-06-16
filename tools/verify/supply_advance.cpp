// Headless harness for the supply layer (BL-039 / BL-038 / BL-045).
// Verifies requirements R1, R4, R5, R6, R7, R8 without SDL / Lua / ImGui.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src tools\verify\supply_advance.cpp ^
//      src\world\world.cpp src\world\supply_system.cpp /Fe:supply_advance.exe
// Run:
//   .\supply_advance.exe

#include "world/components.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

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
    }
}

// ---------------------------------------------------------------------------
// R7 + R8: credit_arrived_convoys credits pool and market supply on arrival
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
    check(near(mkt_supply, 10.0f + 30.0f), "R8: market supply injected",
          mkt_supply, 10.0f + 30.0f);
}

// ---------------------------------------------------------------------------

int main()
{
    std::printf("supply_advance harness — BL-039 supply layer\n\n");
    test_advance_convoys();
    test_logistics_constants();
    test_dispatch_and_gate();
    test_credit_arrived();

    std::printf("\n%s  (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "SOME FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures > 0 ? 1 : 0;
}
