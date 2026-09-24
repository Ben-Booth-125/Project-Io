// ---------------------------------------------------------------------------
// gen_step_costs -- what each captioned generation step costs, and whether the
// loading bar ever holds still (BL-1072, a wizard wait says what it is doing)
// ---------------------------------------------------------------------------
// STARTUP.md § A wait never looks stopped: the outer bar's steps are weighted
// by what each pass COSTS, measured, and every long pass reports progress
// within itself. This instrument is the measurement and the check.
//
//   1. Builds the world the app builds (world_gen.lua's PARSED config, the
//      works table, the shipped params) once per seed, with a fixture -- so
//      generation prints its own `[gen steps]` line: wall milliseconds per
//      step label, which is what `generation_step_cost_ms` is read from.
//   2. Watches the same `generation_progress` sink the loading screen reads,
//      from a second thread, every 50 ms, exactly as a renderer would: the
//      outer fraction (`generation_progress::fraction`), the caption, and the
//      longest stretch the fraction did not move. That stretch is the item's
//      DONE WHEN ("the bar never holds still for more than ~1 s"), measured.
//
// A REPORT, NOT A GATE. Wall clock varies with the machine and the load; the
// stall figure is read by a human against the ~1 s the item names, and the
// process exits 0 unless the build itself fails. The sink is a write-only tap,
// so a watched build is the same world as an unwatched one (world_determinism
// holds the digests).
//
// Usage:  gen_step_costs.exe [seed ...]      (hex or decimal; default 0 and 28)
// Build:  cmd //c tools\verify\build_lua_harness.bat gen_step_costs
//         Run from the repo root: it loads scripts/world_gen.lua and works.lua.

#include "harness_params.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/works_roster.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using clk = std::chrono::steady_clock;

double secs_between(clk::time_point a, clk::time_point b)
{
    return std::chrono::duration<double>(b - a).count();
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
        seeds.push_back(static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 0)));
    if (seeds.empty()) seeds = {0u, 28u};

    lua_state lua;
    lua.load("scripts/world_gen.lua");
    world_gen_config cfg{};
    cfg.load_from_lua(lua);
    works_registry works;
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);

    for (const uint32_t seed : seeds)
    {
        world_params params{};
        params.seed = seed;

        // Heap, as the app holds its wizard sinks: the carve arrays make the
        // struct ~66 KB.
        auto prog = std::make_unique<generation_progress>();
        prog->begin_wait();
        prog->stage_count.store(generation_stage_count(cfg), std::memory_order_relaxed);

        std::atomic<bool> done{false};
        double longest_still = 0.0, still_since_s = 0.0;
        int    longest_label = -1;
        float  longest_at    = 0.0f;
        std::vector<std::string> caption_trail;

        std::thread watcher([&] {
            const clk::time_point t0 = clk::now();
            float  last_f = -1.0f;
            int    last_label = -1;
            clk::time_point still_from = t0;
            while (!done.load(std::memory_order_acquire))
            {
                const float f  = prog->fraction();
                const int   li = prog->label.load(std::memory_order_relaxed);
                const clk::time_point now = clk::now();
                if (li != last_label && li >= 0 && li < generation_stage_label_count)
                {
                    char line[160];
                    std::snprintf(line, sizeof line, "%6.1f s  %5.1f%%  %s",
                                  secs_between(t0, now), f * 100.0f,
                                  generation_stage_labels[li]);
                    caption_trail.emplace_back(line);
                    last_label = li;
                }
                // "Moved" = a visible step on a 420 px bar: a thousandth.
                if (f > last_f + 0.001f)
                {
                    last_f     = f;
                    still_from = now;
                }
                else
                {
                    const double still = secs_between(still_from, now);
                    if (still > longest_still)
                    {
                        longest_still = still;
                        longest_label = li;
                        longest_at    = f;
                        still_since_s = secs_between(t0, still_from);
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });

        era_minus_one_fixture fx;
        generation_report rep;
        const clk::time_point t0 = clk::now();
        world w = make_hard_coded_world(params, &rep, cfg, prog.get(), &works, &fx);
        const double total_s = secs_between(t0, clk::now());
        done.store(true, std::memory_order_release);
        watcher.join();

        std::printf("seed %u: built in %.2f s, final fraction %.3f, stage %d/%d\n", seed,
                    total_s, prog->fraction(), prog->stage.load(), prog->stage_count.load());
        std::printf("  weight total %lld (plan)\n",
                    static_cast<long long>(prog->weight_total.load()));
        for (const std::string& s : caption_trail) std::printf("  %s\n", s.c_str());
        std::printf("  LONGEST STILL: %.2f s from %.1f s at %.1f%% under \"%s\"\n",
                    longest_still, still_since_s, longest_at * 100.0f,
                    longest_label >= 0 && longest_label < generation_stage_label_count
                        ? generation_stage_labels[longest_label] : "?");
        std::printf("  nations %zu, corps %zu\n", w.nations.size(), w.corporations.size());
        std::fflush(stdout);
    }
    return 0;
}
