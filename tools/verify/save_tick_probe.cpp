// save_tick_probe -- load an .iosave and step the economy the way the app does.
//
//   build_gen/verify/save_tick_probe.exe <save.iosave> [ticks=8] [--econ-tick N] [--empty-registry]
//
// Reads the WORLD half of the IOSG file (the 8-byte magic+version header, then
// the world snapshot; the envelope that follows is the app's and is skipped),
// bands the registry exactly as app::load_game_from does, and runs
// `run_settle_tick` per quarter -- the app's step_economy world half -- under the
// cadence key a loaded campaign resumes at (k_campaign_settle_ticks + econ_tick).
// Prints per tick the seat's filed income and treasury, the pooled income and
// treasury of every nation, and the count of buildings under construction.
//
// --empty-registry runs the same steps on a registry that was NEVER loaded from
// Lua: what a save opened from the main menu of a fresh process stepped on
// before the load path loaded the data layer itself (the 2026-11-09 stall).
//
// The econ tick a save holds lives in its ENVELOPE (the world does not persist
// `current_econ_tick`), which this probe skips: pass it as --econ-tick N (the play
// log header's `econ_tick`; default 13, the 2026-11-09 quicksave).
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "world/world_save.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: save_tick_probe <save.iosave> [ticks] [--empty-registry]\n");
        return 2;
    }
    const std::string path = argv[1];
    int  ticks = 8;
    bool empty = false;
    int  live  = 13;
    for (int i = 2; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--empty-registry") == 0) empty = true;
        else if (std::strcmp(argv[i], "--econ-tick") == 0 && i + 1 < argc) live = std::atoi(argv[++i]);
        else ticks = std::atoi(argv[i]);
    }

    lua_state        lua;
    recipe_registry  reg;
    world_gen_config cfg;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    if (!empty)
    {
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        reg.load_from_lua(lua);
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); return 2; }
    in.seekg(8); // IOSG magic + version; the world snapshot follows
    world w;
    if (!read_world_snapshot(w, in, &reg.military(), &cfg.kepler_base_price))
    {
        std::fprintf(stderr, "read_world_snapshot failed\n");
        return 2;
    }
    reg.set_era(campaign_band_from_world(w)); // as load_game_from

    // As load_game_from: the next step runs at the settle's twelve plus the live
    // quarters already played, and current_econ_tick holds the step before it.
    int econ_step = k_campaign_settle_ticks + live;
    w.current_econ_tick = econ_tick_after_live_steps(live);
    const entity_id seat = player_nation(w);
    std::printf("save_tick_probe: %s registry=%s live_econ_tick=%d seat=%llu recipes=%zu\n",
                path.c_str(), empty ? "EMPTY" : "loaded", live,
                static_cast<unsigned long long>(seat), reg.recipe_count());

    const auto report = [&](const char* tag, int t) {
        double seat_inc = 0, seat_tr = 0, pool_inc = 0, pool_tr = 0;
        for (const auto& [id, n] : w.nations)
        {
            const double inc = n.returns.empty() ? 0.0 : n.returns.back().income;
            pool_inc += inc;
            pool_tr  += n.treasury;
            if (id == seat) { seat_inc = inc; seat_tr = n.treasury; }
        }
        int building = 0;
        for (const auto& [id, b] : w.buildings)
            if (b.ticks_remaining > 0) ++building;
        std::printf("%-5s t%-3d seat_income %10.1f seat_treasury %12.1f | all_income %12.1f "
                    "all_treasury %12.1f | under_construction %d\n",
                    tag, t, seat_inc, seat_tr, pool_inc, pool_tr, building);
    };
    report("load", live);
    for (int i = 0; i < ticks; ++i)
    {
        const int day = 198 + (i + 1) * 90; // the quicksave day 198, a quarter per step
        run_settle_tick(w, reg, econ_step++, day, /*spectating=*/false);
        report("step", live + i + 1);
    }
    return 0;
}
