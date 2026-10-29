// carried_supplies_census.cpp — BL-1452 (carried supplies). A MEASUREMENT, not a gate:
// on a generated campaign world (the app's start, its settle, then live quarters as
// market_viability steps them), what the units' supply factors and carried stocks do.
//
// Per Ben's ruling (2026-10-29: a garrison past its roads is unfed even at home), it
// counts HOME GARRISONS — units standing on their own nation's ground — that reach
// zero food at a tick end, and how many of those then starve to death inside the
// window (gone from the world after being seen at zero food and not in a battle).
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
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
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

    // The live window, a tick at a time, exactly as run_app_live_window steps it.
    std::set<entity_id> home_zero_food, any_zero_food, starved_home, starved_any;
    std::map<entity_id, bool> seen_zero; // unit -> seen at zero food, not in a battle
    for (int k = 1; k <= ticks; ++k)
    {
        const int day = k * 90;
        advance_orbits(w, 90.0);
        advance_surveys(w, 90);
        w.current_day_tick = day;
        (void)run_settle_tick(w, start->reg, k_app_validation_ticks + (k - 1), day, /*spectating=*/false);

        for (auto it = seen_zero.begin(); it != seen_zero.end(); ++it)
            if (w.units.count(it->first) == 0)
            {
                starved_any.insert(it->first);
                if (home_zero_food.count(it->first)) starved_home.insert(it->first);
            }
        for (const entity_id g : starved_any) seen_zero.erase(g);

        const std::set<entity_id> fighting = units_in_live_battles(w);
        for (const auto& [uid, u] : w.units)
        {
            if (u.count <= 0 || !unit_starving(u)) continue;
            any_zero_food.insert(uid);
            if (at_home(w, u)) home_zero_food.insert(uid);
            if (fighting.count(uid) == 0) seen_zero[uid] = true;
            else seen_zero.erase(uid);
        }
        if (k == 10 || k == 25 || k == ticks)
        {
            char label[16];
            std::snprintf(label, sizeof(label), "t%d", k);
            census(w, label);
        }
    }
    std::printf("  STARVATION seed %u: home garrisons at zero food %zu (any unit %zu); "
                "starved to death in the window: home %zu (any %zu)\n",
                seed, home_zero_food.size(), any_zero_food.size(), starved_home.size(),
                starved_any.size());
    return 0;
}
