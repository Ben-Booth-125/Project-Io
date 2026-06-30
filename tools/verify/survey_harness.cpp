// Headless harness for the Survey system (BL-067) — no SDL / Lua / ImGui.
// Builds a tiny world by hand and asserts: cost/duration scale with size+distance
// (R2), deterministic raster region partition + reveal (R3), home starts surveyed
// (R4), concurrent surveys advance independently (R5), and dispatch guards + the
// upfront debit (R6). Kept outside src/ so the CMake glob does not pull it in.
//
// Build (from repo root):
//   g++ -std=c++20 -I src tools/verify/survey_harness.cpp \
//       src/world/world.cpp src/world/survey_system.cpp -o survey_harness
//   ./survey_harness

#include "world/components.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what, double got = 0.0, double want = 0.0)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else { std::printf("  FAIL  %s   (got %.3f, want %.3f)\n", what, got, want); ++g_failures; }
}

static bool near(double a, double b) { return std::fabs(a - b) < 1e-2; }

static entity_id add_body(world& w, float radius_au, int gw, int gh, entity_id parent = null_entity)
{
    const entity_id id = w.create_entity();
    body_component b{};
    b.type = body_type::planet;
    b.parent = parent;
    b.orbital_radius_au = radius_au;
    b.grid_width = gw;
    b.grid_height = gh;
    w.bodies[id] = b;
    return id;
}

int main()
{
    std::printf("Survey system harness (BL-067)\n");

    world w;
    const entity_id home = add_body(w, 1.0f, 40, 40);
    const entity_id near_small = add_body(w, 1.2f,  8,  8);   // D=0.2, 64 tiles, 1 region
    const entity_id far_large  = add_body(w, 5.0f, 80, 80);   // D=4.0, 6400 tiles, 100 regions
    w.home_body = home;

    const entity_id corp = w.create_entity();
    { corporation_component cc; cc.balance = 50000.0f; cc.is_player = true; w.corporations[corp] = cc; }
    w.player_entity = corp;

    init_survey_states(w);

    // --- R4: home starts surveyed; others hidden ---
    std::printf("\nR4 — home surveyed, others hidden\n");
    check(w.bodies[home].survey.phase == survey_phase::surveyed, "home phase == surveyed");
    check(survey_tile_visible(w.bodies[home].survey, 40, 40, 0, 0),   "home tile (0,0) visible");
    check(survey_tile_visible(w.bodies[home].survey, 40, 40, 39, 39), "home tile (39,39) visible");
    check(w.bodies[near_small].survey.phase == survey_phase::hidden, "near body hidden");
    check(!survey_tile_visible(w.bodies[near_small].survey, 8, 8, 0, 0), "hidden body tile masked");

    // --- R3: deterministic raster partition ---
    std::printf("\nR3 — deterministic raster region partition\n");
    check(survey_region_count(80, 80) == 100, "region_count(80,80) == 100", survey_region_count(80,80), 100);
    check(survey_region_count(8, 8)   == 1,   "region_count(8,8) == 1",     survey_region_count(8,8), 1);
    check(survey_region_count(20, 20) == 9,   "region_count(20,20) == 9 (3x3)", survey_region_count(20,20), 9);
    check(survey_region_of(80, 80, 0, 0)   == 0,  "region_of(0,0) == 0");
    check(survey_region_of(80, 80, 8, 0)   == 1,  "region_of(8,0) == 1 (next col)");
    check(survey_region_of(80, 80, 0, 8)   == 10, "region_of(0,8) == 10 (next row, 10 cols)");
    check(survey_region_of(80, 80, 79, 79) == 99, "region_of(79,79) == 99 (last)");
    // pure: identical results on a second call
    check(survey_region_of(80, 80, 33, 41) == survey_region_of(80, 80, 33, 41), "region_of is pure/deterministic");
    // tile visibility tracks regions_done: regions 0..4 visible at regions_done=5
    survey_state ss; ss.phase = survey_phase::scanning; ss.regions_total = 100; ss.regions_done = 5;
    check( survey_tile_visible(ss, 80, 80, 0, 0),   "region 0 visible at regions_done=5");
    check(!survey_tile_visible(ss, 80, 80, 8, 24),  "region 31 (tile 8,24) masked at regions_done=5");
    check(!survey_tile_visible(ss, 80, 80, 40, 0),  "region 5 masked at regions_done=5 (boundary)");
    check( survey_tile_visible(ss, 80, 80, 32, 0),  "region 4 visible at regions_done=5 (boundary)");

    // --- R2: cost & duration scale with size and distance ---
    std::printf("\nR2 — cost/duration scale with size + distance\n");
    const float cost_near = survey_cost(w, near_small);
    const float cost_far  = survey_cost(w, far_large);
    check(near(cost_near, 932.0), "cost(near) == 500 + 2000*0.2 + 0.5*64 = 932", cost_near, 932.0);
    check(near(cost_far, 11700.0), "cost(far) == 500 + 2000*4 + 0.5*6400 = 11700", cost_far, 11700.0);
    check(cost_far > cost_near, "far body costs more than near body");
    const survey_schedule sn = survey_compute_schedule(w, near_small);
    const survey_schedule sf = survey_compute_schedule(w, far_large);
    check(sf.transit_ticks > sn.transit_ticks, "transit grows with distance", sf.transit_ticks, sn.transit_ticks);
    check(sf.scan_ticks    > sn.scan_ticks,    "scan grows with size",        sf.scan_ticks,    sn.scan_ticks);
    check(sf.total_ticks == sf.transit_ticks + sf.scan_ticks, "total == transit + scan");
    check(sn.regions_total == 1 && sf.regions_total == 100, "schedule region counts match grid");
    // moon uses parent radius (distance term)
    const entity_id moon = add_body(w, 0.01f, 8, 8, far_large);
    check(near(survey_distance_au(w, moon), 4.0), "moon uses parent's heliocentric radius (D=4)", survey_distance_au(w, moon), 4.0);

    // --- R6: dispatch guards + upfront debit ---
    std::printf("\nR6 — dispatch guards + upfront debit\n");
    check(dispatch_survey(w, home) == survey_dispatch_result::already_surveyed, "dispatch(home) -> already_surveyed");
    check(dispatch_survey(w, 99999) == survey_dispatch_result::invalid, "dispatch(bad id) -> invalid");
    // insufficient funds: drop balance below far cost, attempt, expect no debit
    w.corporations[corp].balance = 1000.0f;
    check(dispatch_survey(w, far_large) == survey_dispatch_result::insufficient_funds, "dispatch(far) with 1000 -> insufficient_funds");
    check(near(w.corporations[corp].balance, 1000.0), "balance unchanged after insufficient_funds", w.corporations[corp].balance, 1000.0);
    // success debits exactly cost
    w.corporations[corp].balance = 50000.0f;
    check(dispatch_survey(w, near_small) == survey_dispatch_result::success, "dispatch(near) -> success");
    check(near(w.corporations[corp].balance, 50000.0 - 932.0), "balance debited by exactly cost(near)", w.corporations[corp].balance, 50000.0 - 932.0);
    check(w.bodies[near_small].survey.phase == survey_phase::in_transit, "near now in_transit");
    check(dispatch_survey(w, near_small) == survey_dispatch_result::in_progress, "second dispatch(near) -> in_progress");

    // --- R5: concurrent independence + full timeline ---
    std::printf("\nR5 — concurrent surveys advance independently\n");
    check(dispatch_survey(w, far_large) == survey_dispatch_result::success, "dispatch(far) now affordable -> success");
    // Tick day-by-day; near (total 22) must finish well before far (total 263).
    int near_done_day = -1, far_done_day = -1;
    for (int day = 1; day <= 400; ++day)
    {
        advance_surveys(w, 1);
        if (near_done_day < 0 && w.bodies[near_small].survey.phase == survey_phase::surveyed) near_done_day = day;
        if (far_done_day  < 0 && w.bodies[far_large].survey.phase  == survey_phase::surveyed)  far_done_day  = day;
    }
    check(near_done_day == sn.total_ticks, "near surveyed at total_ticks (22)", near_done_day, sn.total_ticks);
    check(far_done_day  == sf.total_ticks, "far surveyed at total_ticks (263)", far_done_day, sf.total_ticks);
    check(near_done_day < far_done_day, "near completes before far (independent schedules)");
    check(w.bodies[far_large].survey.regions_done == 100, "far fully revealed (100/100)", w.bodies[far_large].survey.regions_done, 100);

    // --- partial reveal (headless analog of R1 visual): mid-scan some regions visible, some masked ---
    std::printf("\nPartial-reveal progression (R1 analog)\n");
    world w2;
    const entity_id h2 = add_body(w2, 1.0f, 40, 40);
    const entity_id b2 = add_body(w2, 5.0f, 80, 80);
    w2.home_body = h2;
    const entity_id c2 = w2.create_entity();
    { corporation_component cc; cc.balance = 50000.0f; cc.is_player = true; w2.corporations[c2] = cc; }
    w2.player_entity = c2;
    init_survey_states(w2);
    dispatch_survey(w2, b2);
    const survey_schedule s2 = survey_compute_schedule(w2, b2);
    // advance through transit; nothing revealed yet
    advance_surveys(w2, s2.transit_ticks);
    check(w2.bodies[b2].survey.phase == survey_phase::scanning, "after transit -> scanning");
    check(w2.bodies[b2].survey.regions_done == 0, "no regions revealed during transit");
    check(!survey_tile_visible(w2.bodies[b2].survey, 80, 80, 0, 0), "tile masked at scan start");
    // advance ~half the scan; expect a partial reveal
    advance_surveys(w2, s2.scan_ticks / 2);
    const int mid = w2.bodies[b2].survey.regions_done;
    check(mid > 0 && mid < 100, "mid-scan: some regions revealed, some masked", mid, 50);
    check( survey_tile_visible(w2.bodies[b2].survey, 80, 80, 0, 0),  "region 0 visible mid-scan");
    check(!survey_tile_visible(w2.bodies[b2].survey, 80, 80, 79, 79), "last region still masked mid-scan");

    // --- ETA must count down by exactly 1/day through transit AND scan ---
    // Regression guard for the scanning-phase ETA freeze: a single-region body is the
    // sharp case (regions_done never moves mid-scan, so an ETA derived from it stalls).
    std::printf("\nETA countdown (scanning ETA must not freeze)\n");
    {
        world w3;
        const entity_id h3   = add_body(w3, 1.0f, 40, 40);
        const entity_id n3   = add_body(w3, 1.2f,  8,  8);   // 1 region — the frozen-ETA case
        const entity_id f3   = add_body(w3, 5.0f, 80, 80);   // 100 regions
        w3.home_body = h3;
        const entity_id c3 = w3.create_entity();
        { corporation_component cc; cc.balance = 50000.0f; cc.is_player = true; w3.corporations[c3] = cc; }
        w3.player_entity = c3;
        init_survey_states(w3);
        const survey_schedule en = survey_compute_schedule(w3, n3);
        const survey_schedule ef = survey_compute_schedule(w3, f3);
        dispatch_survey(w3, n3);
        dispatch_survey(w3, f3);
        bool near_ok = true, far_ok = true;
        for (int day = 1; day <= ef.total_ticks; ++day)
        {
            advance_surveys(w3, 1);
            if (w3.bodies[n3].survey.phase != survey_phase::surveyed &&
                survey_eta_days(w3, n3) != en.total_ticks - day) near_ok = false;
            if (w3.bodies[f3].survey.phase != survey_phase::surveyed &&
                survey_eta_days(w3, f3) != ef.total_ticks - day) far_ok = false;
        }
        check(near_ok, "single-region body ETA counts down 1/day (no freeze)");
        check(far_ok,  "multi-region body ETA counts down 1/day through scan");
    }

    std::printf("\n%s — %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
