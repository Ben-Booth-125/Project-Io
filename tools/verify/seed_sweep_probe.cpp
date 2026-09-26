// Seed-sweep reproduction probe: generate a world per seed and report which
// seeds fail. Written 2026-08-12 to reproduce a crash in generate_corporations
// ("Placing companies") that did NOT occur on seed 0, so the harnesses missed it.
#include "world/hard_coded_world.hpp"
#include "world/corporation_generation.hpp"
#include "world/recipe_registry.hpp"
#include "world/stockpile_budget.hpp"   // spend_stockpile_on_seed_candidate (BL-1086 review)
#include "world/world_gen_config.hpp"

#include <cstdio>
#include <future>
#include <exception>

int main(int argc, char* argv[])
{
    const int n = (argc > 1) ? std::atoi(argv[1]) : 12;
    int failures = 0;
    for (int i = 0; i < n; ++i)
    {
        world_params p;
        p.seed = static_cast<uint32_t>(i);
        std::printf("seed %2d ... ", i);
        std::fflush(stdout);
        try
        {
            generation_report rep;
            // RUN IT ON A WORKER THREAD, exactly as app::begin_new_game does.
            // The interactive crash did not reproduce on the main thread, and a
            // worker gets a different (smaller) default stack on Windows.
            auto fut = std::async(std::launch::async, [&] {
                return make_hard_coded_world(p, &rep);
            });
            world w = fut.get();
            // The untested gap: company placement runs only from the start, so
            // no bare generation reaches it. It is literally "placing companies",
            // which is where the crash was reported.
            //
            // BL-1086 review (2026-09-26): ON A BUDGET WORLD THE PLACING IS THE
            // CHARTER WEB. Generation lays no roster there, so this probe now
            // places companies as the headless paths do (main.cpp, run_verify):
            // `spend_stockpile_on_seed_candidate`, the search's seed candidate
            // chartered from the world's own budget, and `generate_background_
            // firms` only where there is no budget to spend. The registry is
            // still default-constructed (the probe stays Lua-free), so the web
            // charters its specialists and no firm (no good has demand) — the
            // question here is whether placement THROWS, not what it builds.
            recipe_registry reg;
            const seed_candidate_spend scs = spend_stockpile_on_seed_candidate(
                w, reg, p.seed, world_gen_config{}.corporation_count);
            if (scs.spent)
                std::printf("[charter web: %zu specialists, %zu firms] ",
                            scs.report.specialists.size(), scs.report.firms.size());
            else
            {
                const auto firms = generate_background_firms(w, reg, p.seed ^ 0x8A21F00Du);
                std::printf("[no budget: firms=%zu] ", firms.size());
            }
            std::printf("ok  (corps=%zu nations=%zu regions_hint=%lld battles=%lld)\n",
                        w.corporations.size(), w.nations.size(),
                        (long long)rep.prehistory_foundings, (long long)rep.prehistory_battles);
        }
        catch (const std::exception& e)
        {
            std::printf("THREW: %s\n", e.what());
            ++failures;
        }
        catch (...)
        {
            std::printf("THREW: unknown\n");
            ++failures;
        }
    }
    std::printf("=== %d seed(s) failed of %d ===\n", failures, n);
    return failures ? 1 : 0;
}
