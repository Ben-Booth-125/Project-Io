// carried_supplies_census.cpp — BL-1452 (carried supplies). A MEASUREMENT, not a gate:
// on a generated campaign world (the app's start, its 12-tick settle, then live quarters
// as market_viability steps them), what the units' supply factors and carried stocks do.
//
// Per Ben's ruling (2026-10-29: a garrison past its roads is unfed even at home), it
// counts HOME GARRISONS — units standing on their own nation's ground — that reach zero
// food at a tick end, and how many of those then STARVE TO DEATH: gone from the world
// after being seen at zero food and not in a battle at that tick end (a unit that was
// starving and then died in a battle it entered later is still counted — an upper
// bound). The settle and the 50-tick window (market_viability's) are counted apart.
//
//   build_gen/verify/carried_supplies_census.exe [--seed N] [--ticks T]
//
// Build: bash tools/verify/build_lua_harness.sh carried_supplies_census. Run from the
// repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/battle_system.hpp"
#include "world/campaign_settle.hpp"
#include "world/carried_supplies.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/nation_budget.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>

namespace {

bool at_home(const world& w, const unit_component& u)
{
    const auto it = w.tile_to_nation.find(u.position);
    return it != w.tile_to_nation.end() && it->second == u.owner;
}

void census(const world& w, const char* when)
{
    int units = 0, men = 0, est = 0, full_cover = 0, part_cover = 0, below_even = 0, no_cover = 0;
    int hungry = 0, starving = 0, home = 0;
    long long food = 0, ammo = 0, equip = 0;
    for (const auto& [uid, u] : w.units)
    {
        if (u.count <= 0) continue;
        ++units;
        home += at_home(w, u) ? 1 : 0;
        men += u.count;
        est += unit_establishment(u);
        if (u.supply_factor_permille >= 1000) ++full_cover;
        else if (u.supply_factor_permille <= 0) ++no_cover;
        else ++part_cover;
        below_even += u.supply_factor_permille < 667 ? 1 : 0;
        hungry   += unit_hungry(u) ? 1 : 0;
        starving += unit_starving(u) ? 1 : 0;
        food += u.food_permille; ammo += u.ammo_permille; equip += u.equipment_permille;
    }
    const int n = units > 0 ? units : 1;
    std::printf("  %-8s units %4d (home %4d) men %7d / est %7d  cover full %4d part %4d (<667 %4d) none %4d  "
                "hungry %4d starving %4d  mean food %4lld ammo %4lld equip %4lld\n",
                when, units, home, men, est, full_cover, part_cover, below_even, no_cover, hungry,
                starving, food / n, ammo / n, equip / n);
}

/// Which trigger holds the units under cover: their tile's Logistic Point share, or a
/// garrison bill the nation's military line does not pay (no weight on it, or no money).
void triggers(const world& w, const lp_tick& lp, int tick)
{
    int lp_known = 0, lp_short = 0, lp_below = 0, unpaid_line = 0, under = 0;
    for (const auto& [uid, u] : w.units)
    {
        if (u.count <= 0) continue;
        under += u.supply_factor_permille < 1000 ? 1 : 0;
        if (const auto it = lp.supply_share.find(uid); lp.resolved && it != lp.supply_share.end())
        {
            ++lp_known;
            lp_short += it->second < 1000 ? 1 : 0;
            lp_below += it->second < 667 ? 1 : 0;
        }
        const auto nit = w.nations.find(u.owner);
        const auto bit = w.nation_budgets.find(u.owner);
        const std::size_t mi = static_cast<std::size_t>(budget_priority::military_research);
        const bool line = nit != w.nations.end() && nit->second.treasury > 0.0f
                       && bit != w.nation_budgets.end() && bit->second.weights[mi] > 0.0f;
        unpaid_line += line ? 0 : 1;
    }
    std::printf("  triggers settle t%-2d: under full cover %3d | LP share known %3d, <1000 %3d, <667 %3d | "
                "owner with no paid military line %3d\n", tick, under, lp_known, lp_short, lp_below,
                unpaid_line);
}

/// Zero-food and starved-to-death tracking over one phase.
struct tracker
{
    std::set<entity_id> home_zero, any_zero, starved_home, starved_any, watch, watch_home;

    void observe(const world& w)
    {
        for (auto it = watch.begin(); it != watch.end();)
        {
            if (w.units.count(*it) == 0)
            {
                starved_any.insert(*it);
                if (watch_home.count(*it)) starved_home.insert(*it);
                it = watch.erase(it);
            }
            else
                ++it;
        }
        const std::set<entity_id> fighting = units_in_live_battles(w);
        for (const auto& [uid, u] : w.units)
        {
            if (u.count <= 0 || !unit_starving(u)) continue;
            any_zero.insert(uid);
            const bool home = at_home(w, u);
            if (home) home_zero.insert(uid);
            if (fighting.count(uid) == 0)
            {
                watch.insert(uid);
                if (home) watch_home.insert(uid);
            }
        }
    }

    void print(std::uint32_t seed, const char* phase) const
    {
        std::printf("  STARVATION seed %u %-7s: home garrisons at zero food %3zu (any unit %3zu); "
                    "starved to death: home %3zu (any %3zu)\n",
                    seed, phase, home_zero.size(), any_zero.size(), starved_home.size(),
                    starved_any.size());
    }
};

} // namespace

int main(int argc, char** argv)
{
    std::uint32_t seed = 0;
    int ticks = 50;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) ticks = std::atoi(argv[++i]);
    }
    std::printf("=== carried supplies census (BL-1452), seed %u, %d live ticks ===\n", seed, ticks);

    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    world& w = start->w;
    census(w, "start");

    // The settle, a tick at a time, as run_app_validation_settle steps it.
    tracker settle;
    for (int step = 0; step < k_app_validation_ticks; ++step)
    {
        const settle_tick_result res = run_settle_tick(w, start->reg, step, /*day_tick=*/0,
                                                       /*spectating=*/true);
        settle.observe(w);
        if (step == 3) census(w, "settle4");
        if (step == 3 || step == k_app_validation_ticks - 1)
            triggers(w, res.report.logistic_points, step + 1);
    }
    clear_research_at_handover(w);
    census(w, "handoff");

    // The live window, a tick at a time, exactly as run_app_live_window steps it. The
    // watch carries over: a unit starving at the handoff that dies in the window
    // counts in the window.
    tracker live;
    live.watch      = settle.watch;
    live.watch_home = settle.watch_home;
    for (int k = 1; k <= ticks; ++k)
    {
        const int day = k * 90;
        advance_orbits(w, 90.0);
        advance_surveys(w, 90);
        w.current_day_tick = day;
        (void)run_settle_tick(w, start->reg, k_app_validation_ticks + (k - 1), day, /*spectating=*/false);
        live.observe(w);
        if (k == 10 || k == ticks)
        {
            char label[16];
            std::snprintf(label, sizeof(label), "t%d", k);
            census(w, label);
        }
    }
    settle.print(seed, "settle");
    live.print(seed, "t1-t50");
    return 0;
}
