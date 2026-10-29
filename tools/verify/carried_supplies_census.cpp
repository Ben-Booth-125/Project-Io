// carried_supplies_census.cpp — BL-1452 (carried supplies). A MEASUREMENT, not a gate:
// on a generated campaign world (the app's start, its settle, then live quarters),
// what the units' supply factors and carried stocks do — how many units sit under full
// cover, how many are hungry or starving, and how many men starvation took.
//
//   build_gen/verify/carried_supplies_census.exe [--seed N] [--ticks T]
//
// Build: bash tools/verify/build_lua_harness.sh carried_supplies_census. Run from the
// repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/carried_supplies.hpp"
#include "world/components.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace {

void census(const world& w, const char* when)
{
    int units = 0, men = 0, est = 0, full_cover = 0, part_cover = 0, no_cover = 0;
    int hungry = 0, starving = 0;
    long long food = 0, ammo = 0, equip = 0;
    for (const auto& [uid, u] : w.units)
    {
        if (u.count <= 0) continue;
        ++units;
        men += u.count;
        est += unit_establishment(u);
        if (u.supply_factor_permille >= 1000) ++full_cover;
        else if (u.supply_factor_permille <= 0) ++no_cover;
        else ++part_cover;
        hungry   += unit_hungry(u) ? 1 : 0;
        starving += unit_starving(u) ? 1 : 0;
        food += u.food_permille; ammo += u.ammo_permille; equip += u.equipment_permille;
    }
    const int n = units > 0 ? units : 1;
    std::printf("  %-10s units %4d  men %7d / est %7d  cover full %4d part %4d none %4d  "
                "hungry %4d starving %4d  mean food %4lld ammo %4lld equip %4lld\n",
                when, units, men, est, full_cover, part_cover, no_cover, hungry, starving,
                food / n, ammo / n, equip / n);
}

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
    run_app_validation_settle(w, start->reg);
    census(w, "handoff");
    // One window: run_app_live_window stamps its own day ticks from 90, so it is
    // called once for the whole run rather than a tick at a time.
    run_app_live_window(w, start->reg, k_app_validation_ticks, ticks);
    char label[16];
    std::snprintf(label, sizeof(label), "t%d", ticks);
    census(w, label);
    return 0;
}
