// ---------------------------------------------------------------------------
// save_continuity — does a save/load in the middle of play change what follows?
// (SAVE_BUILDING_REPORT, Ben 2026-11-09)
// ---------------------------------------------------------------------------
// Builds the app's campaign start (`build_app_start_world`), runs the settle
// (`run_settle`) and the seat, then N live quarters. At that point it writes a
// REAL save (`write_save_game`, world snapshot + envelope, the last economy
// report included since save_game_version 27) and reads it back twice:
//
//   A  the UNBROKEN run: the original world, stepped M more quarters.
//   B  LOADED, with the saved report (the v27 load).
//   C  LOADED, the report discarded (what every load did before v27).
//
// Each branch is stepped as app::load_game_from leaves a loaded campaign and
// app::run steps it: the cadence key resumes at k_campaign_settle_ticks + N,
// `current_econ_tick` is econ_tick_after_live_steps(N), the day mirror is set,
// and each quarter advances orbits and surveys by 90 days before
// `run_settle_tick` (run_app_live_window's shape).
//
//   K1  right after the load: B's and C's world snapshot digest equals A's.
//   K2  every building's `estimate_building_reading` (the Selection element's
//       profit bar, the site finder's netting row) off B's LOADED report equals
//       A's off its LIVE report; C's (empty report) is counted as differing.
//   K3  per quarter, the world snapshot digest and the pooled treasury of A, B
//       and C. Reported as MATCH or the first divergent quarter. B == C is
//       expected by construction -- run_settle_tick builds its own report and
//       reads none from the previous tick -- so the report cannot be what moves
//       a loaded world; if A diverges from both, the cause is outside the
//       report (a load-order effect in the world snapshot) and is reported.
//
//   build: bash tools/verify/build_env_harness.sh save_continuity
//          (a live-Lua harness that also compiles src/core/save_game.cpp, so it
//          needs imgui's headers; build_harness.js routes it there).
//   run:   build_gen/verify/save_continuity.exe [seed=7] [N=4] [M=8]   (repo root)
// ---------------------------------------------------------------------------

#include "core/save_game.hpp"
#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/building_profit.hpp"
#include "world/campaign_settle.hpp"
#include "world/orbital_system.hpp"
#include "world/spawn_seat.hpp"
#include "world/survey_system.hpp"
#include "world_digest.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int k_days = 90;

int g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

template <class Map>
std::vector<entity_id> sorted_keys(const Map& m)
{
    std::vector<entity_id> ids;
    ids.reserve(m.size());
    for (const auto& kv : m)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

double pooled_treasury(const world& w)
{
    double t = 0.0;
    for (const entity_id id : sorted_keys(w.nations))
        t += w.nations.at(id).treasury;
    return t;
}

/// One live quarter, k-th since the seat (1-based), as run_app_live_window steps it.
settle_tick_result live_quarter(world& w, const recipe_registry& reg, int k)
{
    const int day = k * k_days;
    advance_orbits(w, static_cast<double>(k_days));
    advance_surveys(w, k_days);
    w.current_day_tick = day;
    return run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day, /*spectating=*/false);
}

bool same_reading(const building_profit& a, const building_profit& b)
{
    return a.has_data == b.has_data && a.revenue == b.revenue && a.input_cost == b.input_cost
        && a.maintenance == b.maintenance && a.wages == b.wages;
}

} // namespace

int main(int argc, char** argv)
{
    const std::uint32_t seed = argc > 1 ? static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10)) : 7u;
    const int N = argc > 2 ? std::atoi(argv[2]) : 4;
    const int M = argc > 3 ? std::atoi(argv[3]) : 8;
    std::printf("save_continuity: seed %u, %d live quarters, save, %d more\n", seed, N, M);

    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    world& a = start->w;
    recipe_registry& reg = start->reg;
    run_settle(a, reg);
    seat_player_corporation(a, seed, start->land.search.winner_score);

    settle_tick_result last;
    for (int k = 1; k <= N; ++k)
        last = live_quarter(a, reg, k);

    // --- the save, as app::save_game_to writes it ---------------------------
    save_envelope env;
    env.econ_tick = static_cast<uint64_t>(N);
    env.day_tick  = static_cast<uint64_t>(N * k_days);
    env.params    = p;
    env.last_econ_report.buildings            = last.report.buildings;
    env.last_econ_report.budgets              = last.report.budgets;
    env.last_econ_report.workforce_contention = last.report.workforce_contention;
    env.last_econ_report.building_labour      = last.report.building_labour;
    env.last_econ_report.upkeep_unmet         = last.report.upkeep_unmet;
    env.last_econ_report.logistic_points      = last.report.logistic_points;
    env.last_corp_standings                   = last.standings;
    std::filesystem::create_directories("build_gen/verify");
    const std::string path = "build_gen/verify/save_continuity.iosave";
    check(write_save_game(path, a, env), "the save was written");

    // --- two loads, as app::load_game_from reads one -------------------------
    auto load = [&](world& w, save_envelope& e) {
        const bool ok = read_save_game(path, w, e, &reg.military(), &start->cfg.kepler_base_price);
        reg.set_era(campaign_band_from_world(w));
        w.current_day_tick  = static_cast<int>(e.day_tick);
        w.current_econ_tick = econ_tick_after_live_steps(static_cast<long long>(e.econ_tick));
        return ok;
    };
    world b, c;
    save_envelope eb, ec;
    check(load(b, eb), "B loaded");
    check(load(c, ec), "C loaded");
    ec.last_econ_report = economy_report{}; // the pre-v27 load: the report empty
    ec.last_corp_standings.clear();
    std::printf("  report rows saved %zu, loaded %zu; standings %zu\n", last.report.buildings.size(),
                eb.last_econ_report.buildings.size(), eb.last_corp_standings.size());

    // K1
    const auto da = world_state_digest(a), db = world_state_digest(b), dc = world_state_digest(c);
    std::printf("  K1 digest A %016llX  B %016llX  C %016llX\n", (unsigned long long)da,
                (unsigned long long)db, (unsigned long long)dc);
    check(da == db && da == dc, "K1 the loaded worlds' snapshot digest equals the unbroken world's");

    // K2: the first frame's readers.
    int same_b = 0, diff_b = 0, same_c = 0, diff_c = 0, with_data = 0;
    for (const entity_id bid : sorted_keys(a.buildings))
    {
        const building_profit ra = estimate_building_reading(a, reg, last.report, bid);
        const building_profit rb = estimate_building_reading(b, reg, eb.last_econ_report, bid);
        const building_profit rc = estimate_building_reading(c, reg, ec.last_econ_report, bid);
        with_data += ra.has_data ? 1 : 0;
        (same_reading(ra, rb) ? same_b : diff_b)++;
        (same_reading(ra, rc) ? same_c : diff_c)++;
    }
    std::printf("  K2 buildings %zu (with data %d): B same %d differ %d | C same %d differ %d\n",
                a.buildings.size(), with_data, same_b, diff_b, same_c, diff_c);
    check(diff_b == 0 && with_data > 0, "K2 every building's profit reading off the LOADED report equals the live one");

    // K3: the quarters after the load.
    int first_ab = -1, first_ac = -1, first_bc = -1;
    for (int k = N + 1; k <= N + M; ++k)
    {
        live_quarter(a, reg, k);
        live_quarter(b, reg, k);
        live_quarter(c, reg, k);
        const auto ha = world_state_digest(a), hb = world_state_digest(b), hc = world_state_digest(c);
        const double ta = pooled_treasury(a), tb = pooled_treasury(b), tc = pooled_treasury(c);
        std::printf("  q%-3d A %016llX %14.3f | B %016llX %14.3f | C %016llX %14.3f\n", k,
                    (unsigned long long)ha, ta, (unsigned long long)hb, tb, (unsigned long long)hc, tc);
        if (first_ab < 0 && (ha != hb || ta != tb)) first_ab = k;
        if (first_ac < 0 && (ha != hc || ta != tc)) first_ac = k;
        if (first_bc < 0 && (hb != hc || tb != tc)) first_bc = k;
    }
    auto verdict = [](int q) { return q < 0 ? std::string("MATCH") : "diverge at q" + std::to_string(q); };
    std::printf("  K3 unbroken vs loaded-with-report (A/B): %s\n", verdict(first_ab).c_str());
    std::printf("  K3 unbroken vs loaded-empty-report (A/C): %s\n", verdict(first_ac).c_str());
    std::printf("  K3 loaded-with vs loaded-empty (B/C): %s\n", verdict(first_bc).c_str());
    check(first_bc < 0, "K3 the saved report does not move the world (B == C every quarter)");
    std::remove(path.c_str());

    std::printf("\n%s (%d failure(s))\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
