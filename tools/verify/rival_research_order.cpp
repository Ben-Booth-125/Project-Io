// rival_research_order — print the order every nation completes research nodes in, live, over N
// quarters from the seat, so two trees' lists can be diffed. A measurement, not a pass/fail: a
// change to the tree or the research scorer is a world-mover the world-bytes pins cannot see,
// because research opens at the handover, after their last sample.
//
//   rival_research_order [--seeds a,b,c] [--quarters Q]
//
// One line per completion, in quarter order then ascending nation id then pick order:
//   seed quarter nation_id node_id
// Compare two builds with tools/verify/rival_research_diff.js. Nothing is queued by this
// tool: the only queues are the scorer's, and the seat's nation holds none.

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/nation_tech.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

int run(std::uint32_t seed, int quarters)
{
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, wp, *start);
    world& w = start->w;
    recipe_registry& reg = start->reg;
    run_app_validation_settle(w, reg);
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    if (seat.seated == null_entity)
    {
        std::printf("# seed %u: no seat\n", seed);
        return 1;
    }

    std::map<entity_id, std::bitset<io::dg::node_count>> seen;
    const int first = k_app_validation_ticks;
    constexpr int k_econ_tick_days = 90; // sim_loop::econ_tick_days
    int rows = 0;
    for (int qtr = 1; qtr <= quarters; ++qtr)
    {
        const int day = qtr * k_econ_tick_days;
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = day;
        (void)run_settle_tick(w, reg, first + (qtr - 1), day, /*spectating=*/false);

        std::vector<entity_id> ids;
        for (const auto& kv : w.nation_tech)
            ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (const entity_id nid : ids)
        {
            const auto& held = w.nation_tech.at(nid).held;
            auto& had = seen[nid];
            for (int n = 0; n < io::dg::node_count; ++n)
                if (held.test(static_cast<std::size_t>(n)) && !had.test(static_cast<std::size_t>(n)))
                {
                    std::printf("%u %d %u %s\n", seed, qtr, static_cast<unsigned>(nid), io::dg::nodes[n].id);
                    ++rows;
                }
            had = held;
        }
    }
    std::printf("# seed %u: %d completions over %d quarters, %d nations researching, state_hash %016llx\n",
                seed, rows, quarters, static_cast<int>(w.nation_tech.size()),
                static_cast<unsigned long long>(w.state_hash(w.current_day_tick)));
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{0u, 46u};
    int quarters = 40;
    for (int i = 1; i < argc; ++i)
    {
        const bool more = i + 1 < argc;
        if (!std::strcmp(argv[i], "--quarters") && more)
            quarters = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--seeds") && more)
        {
            seeds.clear();
            const std::string s(argv[++i]);
            std::size_t at = 0;
            while (at <= s.size())
            {
                const std::size_t c = std::min(s.find(',', at), s.size());
                seeds.push_back(static_cast<std::uint32_t>(std::strtoul(s.substr(at, c - at).c_str(), nullptr, 10)));
                at = c + 1;
            }
        }
    }
    int bad = 0;
    for (const std::uint32_t s : seeds)
        bad += run(s, quarters);
    return bad;
}
