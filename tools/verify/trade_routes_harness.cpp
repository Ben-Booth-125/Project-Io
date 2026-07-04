// Headless harness for persistent trade routes (BL-088).
// Verifies that credit_arrived_convoys upserts a durable trade_route per completed
// inter-body lane, and that body_of_market resolves correctly — without SDL / Lua / ImGui.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src tools\verify\trade_routes_harness.cpp ^
//      src\world\world.cpp src\world\supply_system.cpp ^
//      src\world\market_clearing.cpp /Fe:trade_routes_harness.exe
// Run:
//   .\trade_routes_harness.exe
//
// Registered as the `trade_routes_harness` CMake target (verifier-headless skill).

#include "world/components.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond)
        std::printf("  PASS  %s\n", what);
    else
    {
        std::printf("  FAIL  %s\n", what);
        ++g_failures;
    }
}

// Build a body with a market centred on it; returns {body, market}.
struct body_market { entity_id body; entity_id market; };

static body_market make_body_market(world& w, const char* name)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.name = name;
    bc.type = body_type::planet;
    bc.orbital_radius_au = 1.0f;
    bc.orbital_angle_rad = 0.0f;
    bc.grid_width = bc.grid_height = 4;
    w.bodies[body] = bc;

    const entity_id market = w.create_entity();
    market_component mc{};
    mc.body = body;
    w.markets[market] = mc;

    return { body, market };
}

// Seed an arrived convoy from src_market to dest_market for corp.
static void arrive_convoy(world& w, entity_id corp, entity_id src_market, entity_id dest_market)
{
    convoy_component cv{};
    cv.source_market  = src_market;
    cv.dest_market    = dest_market;
    cv.cargo_resource = resource_type::iron_ore;
    cv.cargo_qty      = 10.0f;
    cv.progress       = 1.0f;
    cv.arrived        = true;
    cv.corp           = corp;
    w.convoys.push_back(cv);
}

// ---------------------------------------------------------------------------
// R3: body_of_market resolves a market entity to its owning body.
// ---------------------------------------------------------------------------
static void test_body_of_market()
{
    std::printf("--- R3: body_of_market ---\n");
    world w;
    const body_market a = make_body_market(w, "A");
    check(body_of_market(w, a.market) == a.body, "market resolves to its body");
    check(body_of_market(w, null_entity) == null_entity, "null market -> null body");
    check(body_of_market(w, w.create_entity()) == null_entity, "unknown market -> null body");
}

// ---------------------------------------------------------------------------
// R1: a completed player convoy upserts exactly one route for the correct
//     unordered body-pair, stamped with the completion tick.
// ---------------------------------------------------------------------------
static void test_upsert_one_route()
{
    std::printf("--- R1: one completed convoy establishes one route ---\n");
    world w;
    const entity_id corp = w.create_entity();
    const body_market a = make_body_market(w, "A");
    const body_market b = make_body_market(w, "B");

    arrive_convoy(w, corp, a.market, b.market);
    credit_arrived_convoys(w, 42);

    check(w.trade_routes.size() == 1, "exactly one route created");
    if (!w.trade_routes.empty())
    {
        const trade_route& r = w.trade_routes.front();
        const bool pair_ok = (r.body_a == a.body && r.body_b == b.body) ||
                             (r.body_a == b.body && r.body_b == a.body);
        check(pair_ok, "route connects the two convoy-endpoint bodies");
        check(r.corp == corp, "route records the dispatching corp");
        check(r.last_tick == 42, "route last_tick == completion tick");
        check(r.convoy_count == 1, "route convoy_count == 1");
    }
    check(w.convoys.empty(), "convoy retired after credit");
}

// ---------------------------------------------------------------------------
// R1: a second convoy on the same lane (either direction) bumps count + tick
//     without duplicating the route.
// ---------------------------------------------------------------------------
static void test_repeat_lane_no_duplicate()
{
    std::printf("--- R1: repeat lane bumps count, no duplicate ---\n");
    world w;
    const entity_id corp = w.create_entity();
    const body_market a = make_body_market(w, "A");
    const body_market b = make_body_market(w, "B");

    arrive_convoy(w, corp, a.market, b.market);
    credit_arrived_convoys(w, 10);
    // Reverse direction — the unordered pair must map to the SAME route.
    arrive_convoy(w, corp, b.market, a.market);
    credit_arrived_convoys(w, 25);

    check(w.trade_routes.size() == 1, "still exactly one route (unordered pair)");
    if (!w.trade_routes.empty())
    {
        const trade_route& r = w.trade_routes.front();
        check(r.convoy_count == 2, "convoy_count incremented to 2");
        check(r.last_tick == 25, "last_tick advanced to the newer completion");
    }
}

// ---------------------------------------------------------------------------
// R1: distinct corp on the same body-pair is a distinct route.
// ---------------------------------------------------------------------------
static void test_distinct_corp_distinct_route()
{
    std::printf("--- R1: distinct corp -> distinct route ---\n");
    world w;
    const entity_id corp1 = w.create_entity();
    const entity_id corp2 = w.create_entity();
    const body_market a = make_body_market(w, "A");
    const body_market b = make_body_market(w, "B");

    arrive_convoy(w, corp1, a.market, b.market);
    arrive_convoy(w, corp2, a.market, b.market);
    credit_arrived_convoys(w, 5);

    check(w.trade_routes.size() == 2, "two corps on one lane -> two routes");
}

// ---------------------------------------------------------------------------
// R1: an intra-body convoy (same body both ends) records no route.
// ---------------------------------------------------------------------------
static void test_intra_body_records_nothing()
{
    std::printf("--- R1: intra-body convoy records nothing ---\n");
    world w;
    const entity_id corp = w.create_entity();
    const body_market a = make_body_market(w, "A");
    // A second market on the SAME body.
    const entity_id market2 = w.create_entity();
    market_component mc{};
    mc.body = a.body;
    w.markets[market2] = mc;

    arrive_convoy(w, corp, a.market, market2);
    credit_arrived_convoys(w, 7);

    check(w.trade_routes.empty(), "no route for a same-body lane");
}

int main()
{
    std::printf("=== trade_routes_harness (BL-088) ===\n");
    test_body_of_market();
    test_upsert_one_route();
    test_repeat_lane_no_duplicate();
    test_distinct_corp_distinct_route();
    test_intra_body_records_nothing();

    std::printf("\n%s  (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
