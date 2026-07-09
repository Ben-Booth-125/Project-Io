// Headless harness for the commercial-sphere activity fog (BL-089).
// Verifies body_activity_visibility's tiers (unknown / known_stale / known / visible)
// from routes + live convoys + ownership + tick, and its independence from survey
// phase — without SDL / Lua / ImGui.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src tools\verify\commercial_fog_harness.cpp ^
//      src\world\world.cpp /Fe:commercial_fog_harness.exe
// Run: .\commercial_fog_harness.exe
//
// Registered as the `commercial_fog_harness` CMake target (verifier-headless skill).

#include "world/components.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cmath>
#include <limits>
#include <numbers>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

// A body with a market centred on it.
struct body_market { entity_id body; entity_id market; };
static body_market make_body_market(world& w, const char* name)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.name = name; bc.type = body_type::planet;
    bc.grid_width = bc.grid_height = 4;
    w.bodies[body] = bc;
    const entity_id market = w.create_entity();
    market_component mc{}; mc.body = body;
    w.markets[market] = mc;
    return { body, market };
}

// Give the world a player corp; return its id.
static entity_id make_player(world& w)
{
    const entity_id p = w.create_entity();
    corporation_component pc{};
    pc.is_player = true;
    w.corporations[p] = pc;
    w.player_entity = p;
    return p;
}

// Place a player building on `body`.
static void add_player_building(world& w, entity_id body)
{
    const entity_id tile = w.create_entity();
    tile_component tc{}; tc.body = body;
    w.tiles[tile] = tc;
    const entity_id bld = w.create_entity();
    building_component bc{}; bc.tile = tile;
    w.buildings[bld] = bc;
    w.corporations[w.player_entity].assets.push_back(bld);
}

static void add_route(world& w, entity_id a, entity_id b, entity_id corp, int last_tick)
{
    trade_route r; r.body_a = a; r.body_b = b; r.corp = corp; r.last_tick = last_tick; r.convoy_count = 1;
    w.trade_routes.push_back(r);
}

int main()
{
    std::printf("=== commercial_fog_harness (BL-089) ===\n");
    constexpr int now = 100;
    constexpr int fresh = route_fresh_ticks_default; // 90

    // R2: unknown — a body with no route, no presence, no lane, not home.
    {
        world w; make_player(w);
        const body_market frontier = make_body_market(w, "Frontier");
        check(body_activity_visibility(w, frontier.body, now) == activity_vis::unknown,
              "no route / presence / lane -> unknown");
    }

    // R2: known — a fresh player route reaches the body.
    {
        world w; const entity_id p = make_player(w);
        const body_market home = make_body_market(w, "Home");
        const body_market b    = make_body_market(w, "B");
        add_route(w, home.body, b.body, p, /*last_tick=*/50); // now-50 = 50 <= 90
        check(body_activity_visibility(w, b.body, now, fresh) == activity_vis::known,
              "fresh player route -> known");
    }

    // R2: known_stale — the route is old.
    {
        world w; const entity_id p = make_player(w);
        const body_market home = make_body_market(w, "Home");
        const body_market b    = make_body_market(w, "B");
        add_route(w, home.body, b.body, p, /*last_tick=*/5);  // now-5 = 95 > 90
        check(body_activity_visibility(w, b.body, now, fresh) == activity_vis::known_stale,
              "stale player route -> known_stale");
        check(body_activity_visibility(w, b.body, now, fresh) != activity_vis::unknown,
              "stale never falls back to unknown");
    }

    // R2: visible — a live player convoy lane touches the body.
    {
        world w; const entity_id p = make_player(w);
        const body_market a = make_body_market(w, "A");
        const body_market b = make_body_market(w, "B");
        convoy_component cv{};
        cv.source_market = a.market; cv.dest_market = b.market; cv.corp = p;
        w.convoys.push_back(cv);
        check(body_activity_visibility(w, b.body, now) == activity_vis::visible,
              "live player lane -> visible");
    }

    // R2: visible — the player owns a building on the body.
    {
        world w; const entity_id p = make_player(w); (void)p;
        const body_market b = make_body_market(w, "B");
        add_player_building(w, b.body);
        check(body_activity_visibility(w, b.body, now) == activity_vis::visible,
              "player presence -> visible");
    }

    // R2: home_body always visible.
    {
        world w; make_player(w);
        const body_market home = make_body_market(w, "Home");
        w.home_body = home.body;
        check(body_activity_visibility(w, home.body, now) == activity_vis::visible,
              "home_body -> visible");
    }

    // R2: independence from survey phase — a surveyed-but-unrouted body is still
    // unknown for activity; an unsurveyed-but-routed body is known.
    {
        world w; const entity_id p = make_player(w);
        const body_market home = make_body_market(w, "Home");
        const body_market surveyed_unrouted = make_body_market(w, "Surveyed");
        w.bodies[surveyed_unrouted.body].survey.phase = survey_phase::surveyed;
        check(body_activity_visibility(w, surveyed_unrouted.body, now) == activity_vis::unknown,
              "surveyed but unrouted -> unknown (activity independent of survey)");

        const body_market unsurveyed_routed = make_body_market(w, "Routed");
        w.bodies[unsurveyed_routed.body].survey.phase = survey_phase::hidden;
        add_route(w, home.body, unsurveyed_routed.body, p, 60);
        check(body_activity_visibility(w, unsurveyed_routed.body, now, fresh) == activity_vis::known,
              "unsurveyed but routed -> known");
    }

    // R2 (BL-099 proximity glimpse): sample-and-store at a player convoy completion tick.
    // A lane along the x-axis through the origin: A(1,0) -- B(-1,0); a near body 0.1 AU off
    // the segment glimpses, a far body 0.5 AU off (beyond R = 0.25) does not.
    {
        const float pi      = std::numbers::pi_v<float>;
        const float half_pi = pi * 0.5f;

        world w; const entity_id p = make_player(w);
        const body_market a    = make_body_market(w, "LaneA");
        const body_market b    = make_body_market(w, "LaneB");
        const body_market near = make_body_market(w, "FrontierNear");
        const body_market far  = make_body_market(w, "FrontierFar");
        w.bodies[a.body].orbital_radius_au = 1.0f;    w.bodies[a.body].orbital_angle_rad = 0.0f;
        w.bodies[b.body].orbital_radius_au = 1.0f;    w.bodies[b.body].orbital_angle_rad = pi;
        w.bodies[near.body].orbital_radius_au = 0.1f; w.bodies[near.body].orbital_angle_rad = half_pi;
        w.bodies[far.body].orbital_radius_au  = 0.5f; w.bodies[far.body].orbital_angle_rad  = half_pi;

        check(std::fabs(body_closest_approach_au(w, near.body, a.body, b.body) - 0.1f) < 1e-4f,
              "closest approach of near body == 0.1 AU");
        check(body_closest_approach_au(w, far.body, a.body, b.body) > glimpse_radius_au_default,
              "closest approach of far body exceeds R");
        check(body_closest_approach_au(w, a.body, a.body, b.body) > 1e30f,
              "a lane endpoint returns the not-glimpsed sentinel");

        record_proximity_glimpses(w, a.body, b.body, /*tick=*/40);
        check(body_activity_visibility(w, near.body, now) == activity_vis::known_stale,
              "frontier within R -> known_stale (glimpsed)");
        check(body_activity_visibility(w, far.body, now) == activity_vis::unknown,
              "frontier beyond R -> stays unknown");
        check(body_activity_visibility(w, a.body, now) == activity_vis::unknown,
              "a lane endpoint is not self-glimpsed");

        // A glimpse never rises above known_stale, even sampled exactly at 'now'.
        record_proximity_glimpses(w, a.body, b.body, now);
        check(body_activity_visibility(w, near.body, now) == activity_vis::known_stale,
              "a fresh glimpse is still only known_stale (never known/visible)");

        // A body's own fresh route outranks a glimpse.
        add_route(w, a.body, near.body, p, /*last_tick=*/now);
        check(body_activity_visibility(w, near.body, now, fresh) == activity_vis::known,
              "own fresh route outranks a glimpse -> known");
    }

    // R2 (BL-099): a glimpse older than the freshness window decays back to unknown.
    {
        const float pi      = std::numbers::pi_v<float>;
        const float half_pi = pi * 0.5f;
        world w; make_player(w);
        const body_market a    = make_body_market(w, "LaneA");
        const body_market b    = make_body_market(w, "LaneB");
        const body_market near = make_body_market(w, "FrontierNear");
        w.bodies[a.body].orbital_radius_au = 1.0f;    w.bodies[a.body].orbital_angle_rad = 0.0f;
        w.bodies[b.body].orbital_radius_au = 1.0f;    w.bodies[b.body].orbital_angle_rad = pi;
        w.bodies[near.body].orbital_radius_au = 0.1f; w.bodies[near.body].orbital_angle_rad = half_pi;
        record_proximity_glimpses(w, a.body, b.body, /*tick=*/1); // now-1 = 99 > 90
        check(body_activity_visibility(w, near.body, now, fresh, glimpse_fresh_ticks_default) == activity_vis::unknown,
              "a glimpse older than the freshness window decays to unknown");
    }

    // R2 (BL-099): the glimpse set is a pure function of inputs — re-sampling the same lane
    // at the same tick is idempotent (no orbital drift, no RNG).
    {
        const float pi      = std::numbers::pi_v<float>;
        const float half_pi = pi * 0.5f;
        world w; make_player(w);
        const body_market a    = make_body_market(w, "LaneA");
        const body_market b    = make_body_market(w, "LaneB");
        const body_market near = make_body_market(w, "FrontierNear");
        w.bodies[a.body].orbital_radius_au = 1.0f;    w.bodies[a.body].orbital_angle_rad = 0.0f;
        w.bodies[b.body].orbital_radius_au = 1.0f;    w.bodies[b.body].orbital_angle_rad = pi;
        w.bodies[near.body].orbital_radius_au = 0.1f; w.bodies[near.body].orbital_angle_rad = half_pi;
        record_proximity_glimpses(w, a.body, b.body, 40);
        const auto snapshot = w.body_last_glimpse_tick;
        record_proximity_glimpses(w, a.body, b.body, 40);
        check(w.body_last_glimpse_tick == snapshot,
              "re-sampling the same lane at the same tick is idempotent (deterministic set)");
    }

    std::printf("\n%s  (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
