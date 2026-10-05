// scorer_clock — how long the economy step's phases take per PLAY tick, on the
// shipped start (BL-1198 cold review: the Well bucket's per-tick cost).
//
// Builds the world as the app does (build_app_start_world), runs the 12-tick
// settle, seats the player, then steps play ticks exactly as market_viability
// does (advance_orbits / advance_surveys over 90 days, run_settle_tick). Each
// play tick it arms the economy step's phase clock (economy_system.hpp,
// BL-1117 — steady-clock stamps nothing in world/* reads back) and sums every
// phase. Prints the mean and max ms per tick of `corp_strategic` (where
// rank_extraction_sites runs) and the whole step.
//
// Wall-clock is READ ONLY, for this report; it feeds nothing.
//
// Build:  bash tools/verify/build_lua_harness.sh scorer_clock
// Run:    build_gen/verify/scorer_clock.exe [--seeds 11,0] [--ticks 50]

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/economy_system.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{11, 0};
    int ticks = 50;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::stringstream ss(argv[++i]); std::string t;
            while (std::getline(ss, t, ',')) if (!t.empty()) seeds.push_back(std::strtoul(t.c_str(), nullptr, 10));
        }
    }
    using ms = std::chrono::duration<double, std::milli>;
    constexpr int k_strategic = 8; // k_economy_step_phase_names[8] == "corp_strategic"
    std::printf("scorer_clock — %d play ticks per seed; ms per tick\n", ticks);
    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        build_app_start_world(lua, p, *start);
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        for (int step = 0; step < k_campaign_settle_ticks; ++step)
            run_settle_tick(w, reg, step, 0, true);
        seat_player_corporation(w, seed, start->land.search.winner_score);

        std::array<double, k_economy_step_phase_count> sum{};
        double strat_max = 0.0, total = 0.0;
        for (int k = 1; k <= ticks; ++k)
        {
            advance_orbits(w, 90.0);
            advance_surveys(w, 90);
            w.current_day_tick = k * 90;
            economy_step_phase_clock clock;
            economy_step_phase_clock_sink() = &clock;
            run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * 90, false);
            economy_step_phase_clock_sink() = nullptr;
            for (int ph = 0; ph < k_economy_step_phase_count; ++ph)
            {
                const double d = ms(clock.stamps[ph + 1] - clock.stamps[ph]).count();
                sum[ph] += d;
                total += d;
            }
            strat_max = std::max(strat_max, ms(clock.stamps[k_strategic + 1] - clock.stamps[k_strategic]).count());
        }
        std::printf("=== seed %u ===  corp_strategic mean %.1f max %.1f | economy step mean %.1f\n", seed,
                    sum[k_strategic] / ticks, strat_max, total / ticks);
        std::printf("   phases (mean):");
        for (int ph = 0; ph < k_economy_step_phase_count; ++ph)
            std::printf(" %s %.1f", k_economy_step_phase_names[ph], sum[ph] / ticks);
        std::printf("\n");
    }
    return 0;
}
