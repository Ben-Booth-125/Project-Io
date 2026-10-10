// ---------------------------------------------------------------------------
// tick_profile — where does an econ tick actually spend its time?
//
// WHY THIS EXISTS. econ_stability measures tick COST and its scaling sweep
// varies bodies x corps — it never varies the TILE COUNT, so it is structurally
// blind to any per-tile term. When the homeworld grid tripled (15,120 ->
// 45,240 tiles, 2026-08-12) the real-world rollout went from ~4.7 ms/tick to
// ~165 ms/tick while econ_stability still reported 0.09 ms and a healthy
// sub-linear exponent. Both were true; they were measuring different things.
//
// Guessing at the hot spot cost two rounds of wrong answers (the BL-253 site
// was already hoisted, and micro-optimising it moved nothing). This harness
// exists so the next such question is answered by measurement in one run.
//
// It is a `bench`-class instrument: absolute times, only meaningful on an idle
// machine, and it asserts nothing about them. It prints a breakdown.
// ---------------------------------------------------------------------------

#include "harness_params.hpp"          // build_app_start_world (BL-1086 review)
#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/corp_ai.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/survey_system.hpp"
#include "world/trade.hpp"

#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <unordered_map>
#include <string>

namespace {

using clock_t_ = std::chrono::steady_clock;

struct phase_timer
{
    std::map<std::string, double> ms;
    void add(const char* name, double v) { ms[name] += v; }
};

template <typename F>
double timed(F&& f)
{
    const auto t0 = clock_t_::now();
    f();
    return std::chrono::duration<double, std::milli>(clock_t_::now() - t0).count();
}

} // namespace

int main(int argc, char* argv[])
{
    const int ticks = (argc > 1) ? std::max(10, std::atoi(argv[1])) : 100;

    std::printf("tick_profile — real generated world, %d ticks\n", ticks);

    // THE SHIPPED START (BL-1086 review, 2026-09-26): `build_app_start_world`,
    // harness_params.hpp's app-order mirror — the parsed world_gen config and the
    // works table into generation, the banded registry loaded from Lua, the
    // landscape search and its winner applied with the world's own charter
    // budget. It replaced a bare `make_hard_coded_world(world_params{})` with a
    // DEFAULT-CONSTRUCTED registry and the fallback config: on a budget world
    // generation lays no roster any more (BL-1086), so that world carried no
    // corporation at all, and even before it the tick ran against an empty
    // registry — every recipe absent, so the profile timed a tick with nothing
    // to produce. No bare mode is kept: nothing reads one. GENERATION below
    // includes the search, which is now part of the start.
    const auto gen_t0 = clock_t_::now();
    lua_state lua;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, world_params{}, *start);
    world& w = start->w;
    const generation_report& gen_report = start->report;
    const recipe_registry& reg = start->reg;
    const double gen_ms = std::chrono::duration<double, std::milli>(clock_t_::now() - gen_t0).count();
    std::printf("  WORLD GENERATION + THE SHIPPED START (search, winner): %.0f ms\n", gen_ms);

    std::size_t tile_count = w.tiles.size();
    std::printf("  PRE-EPOCH ERA: %lld years, %lld battles, %lld conquests, %lld foundings\n",
                (long long)gen_report.prehistory_years, (long long)gen_report.prehistory_battles,
                (long long)gen_report.prehistory_conquests, (long long)gen_report.prehistory_foundings);
    std::printf("  tiles=%zu  corps=%zu  markets=%zu  bodies=%zu\n\n",
                tile_count, w.corporations.size(), w.markets.size(), w.bodies.size());

    phase_timer pt;
    const auto t_all0 = clock_t_::now();

    for (int t = 0; t < ticks; ++t)
    {
        economy_report report;
        pt.add("advance_orbits",        timed([&]{ advance_orbits(w, t); }));
        pt.add("advance_convoys",       timed([&]{ advance_convoys(w); }));
        pt.add("run_economy_step",      timed([&]{ report = run_economy_step(w, reg); }));
        // BL-1266: the trade pass replaced dispatch_convoys, after the economy step.
        pt.add("run_trades",            timed([&]{ run_trades(w, reg, report); }));
        std::unordered_map<entity_id, corp_cash_flow> flows;
        pt.add("clear_markets",         timed([&]{ flows = clear_markets(w, reg, report); }));
        pt.add("apply_budget",          timed([&]{ apply_budget(w, reg, flows, report.workforce_contention); }));
        pt.add("credit_arrived",        timed([&]{ credit_arrived_convoys(w, t); }));
        pt.add("advance_surveys",       timed([&]{ advance_surveys(w, t); }));
    }

    const double total = std::chrono::duration<double, std::milli>(clock_t_::now() - t_all0).count();

    std::printf("%-24s %12s %12s %8s\n", "phase", "total ms", "ms/tick", "share");
    for (const auto& [name, ms] : pt.ms)
        std::printf("%-24s %12.1f %12.4f %7.1f%%\n",
                    name.c_str(), ms, ms / ticks, 100.0 * ms / total);
    std::printf("%-24s %12.1f %12.4f\n", "ALL (incl. overhead)", total, total / ticks);

    return 0;
}
