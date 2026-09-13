// ---------------------------------------------------------------------------
// BL-947 measurement harness — CULTURE_ROUND_COASTS_TO_400BCE.
// ---------------------------------------------------------------------------
// Before implementing the coast (docs/generation/CIVILISATION.md § "The span
// is 400 BCE to 1200 CE"), this measures whether `settlement_state::
// migration_end_year` (the migration's own derived terminating year) ever
// exceeds the Empires round's own opening year -- the boundary the Culture
// round is meant to coast forward TO. The doc names an overrun "a defect in
// the migration, not in this boundary", so this is diagnostic, not a pass/
// fail gate: it reports the frequency across a sweep rather than asserting
// zero.
//
// Uses `gen_cfg.stop_after_migration = true` so every world skips the ~23s
// Era -1 sim and only pays for planetology + the migration walk.
//
// Not wired into any build target permanently (no entry in the CLAUDE.md
// skill list) -- run once via build_harness.js for BL-947's own report, kept
// here since a re-measurement after a migration-side change is cheap insurance.

#include "world/hard_coded_world.hpp"
#include "world/era_minus_one.hpp"
#include "world/world_gen_config.hpp"

#include <cstdio>
#include <cstdint>

int main()
{
    std::printf("=== BL-947 measurement: migration_end_year vs the Empires round's own start ===\n");

    world_gen_config cfg;
    cfg.stop_after_migration = true;

    const int N = 60;
    int overruns = 0;
    int64_t worst_over = 0;
    uint32_t worst_seed = 0;

    for (int i = 0; i < N; ++i)
    {
        world_params p;
        p.seed = 0xC001D00Du + static_cast<uint32_t>(i) * 0x9E3779B9u;
        // Defaults: epoch_year = 0, prehistory_years = 400 -> the Empires
        // round's own start is -400 (400 BCE), exactly the boundary
        // CIVILISATION.md names.
        const int64_t boundary = era_minus_one_sim_params(p).start_year;

        generation_report rep;
        make_hard_coded_world(p, &rep, cfg);

        const generation_report::body_entry* home = nullptr;
        for (const auto& b : rep.bodies)
            if (b.is_homeworld) { home = &b; break; }
        if (!home) { std::printf("seed %10u  NO HOMEWORLD (skipped)\n", p.seed); continue; }

        const int64_t mey = home->settlement.migration_end_year;
        const bool over = mey > boundary;
        if (over)
        {
            ++overruns;
            const int64_t by = mey - boundary;
            if (by > worst_over) { worst_over = by; worst_seed = p.seed; }
        }
        std::printf("seed %10u  migration_end_year=%6lld  boundary=%6lld  %s\n",
                    p.seed, static_cast<long long>(mey), static_cast<long long>(boundary),
                    over ? "OVERRUN" : "ok");
    }

    std::printf("=== %d/%d seeds overran the -400 boundary", overruns, N);
    if (overruns > 0)
        std::printf(" (worst: seed %u, %lld years over)", worst_seed, static_cast<long long>(worst_over));
    std::printf(" ===\n");
    return 0;
}
