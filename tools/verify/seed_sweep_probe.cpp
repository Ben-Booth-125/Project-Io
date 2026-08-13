// Seed-sweep reproduction probe: generate a world per seed and report which
// seeds fail. Written 2026-08-12 to reproduce a crash in generate_corporations
// ("Placing companies") that did NOT occur on seed 0, so the harnesses missed it.
#include "world/hard_coded_world.hpp"
#include "world/corporation_generation.hpp"
#include "world/recipe_registry.hpp"

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
            // The untested gap: generate_background_firms runs ONLY from
            // start_new_game, so no harness and not even --verify reaches it.
            // It is literally "placing companies", which is where the crash was
            // reported.
            recipe_registry reg;
            const auto firms = generate_background_firms(w, reg, p.seed ^ 0x8A21F00Du);
            std::printf("[firms=%zu] ", firms.size());
            std::printf("ok  (corps=%zu nations=%zu provinces_hint=%lld battles=%lld)\n",
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
