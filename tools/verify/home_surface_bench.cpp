// Headless bench + sanity for generate_home_surface_preview — the wizard's
// real-tile preview seam (extracted from make_hard_coded_world 2026-08-07).
// Two jobs:
//   * SANITY: the preview surface is the SAME surface make_hard_coded_world
//     builds for Kepler — same substrate on every tile, across seeds. This is
//     the whole point of the extraction; if it drifts, the preview is lying.
//   * BENCH: per-call wall time, printed. The wizard calls this on every
//     control move, so the number decides whether the call stays synchronous.
//     Informational (machines differ) — the PASS bar is generous (< 1s).

#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/world.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;
void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

} // namespace

int main()
{
    using clock = std::chrono::steady_clock;

    double worst_ms = 0.0, total_ms = 0.0;
    int    runs = 0;

    for (uint32_t seed : { 0u, 1u, 42u, 0xBEEFu, 90210u })
    {
        world_params params;
        params.seed = seed;

        const auto t0 = clock::now();
        world scratch;
        const entity_id probe = scratch.create_entity();
        const auto preview_tiles = generate_home_surface_preview(scratch, probe, params);
        const double ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
        worst_ms = ms > worst_ms ? ms : worst_ms;
        total_ms += ms;
        ++runs;
        std::printf("  seed %-8u  %7.1f ms  (%zu tiles)\n",
                    seed, ms, preview_tiles.size());

        check(preview_tiles.size()
                  == static_cast<std::size_t>(home_grid_width * home_grid_height),
              "preview surface is a full home grid");

        // The full build, for the parity check. Slow (all bodies + political
        // layer), which is exactly why the wizard gets the carve-out instead.
        // Kepler's tiles are recovered raster-ordered from their grid coords.
        world full = make_hard_coded_world(no_prehistory(params));
        std::vector<terrain_substrate> full_comp(
            static_cast<std::size_t>(home_grid_width * home_grid_height),
            terrain_substrate::ocean);
        std::size_t found = 0;
        for (const auto& [id, t] : full.tiles)
            if (t.body == full.home_body)
            {
                full_comp[static_cast<std::size_t>(t.grid_y * home_grid_width
                                                   + t.grid_x)] = t.substrate;
                ++found;
            }
        bool same = found == preview_tiles.size();
        if (same)
            for (std::size_t i = 0; i < preview_tiles.size(); ++i)
                if (scratch.tiles.at(preview_tiles[i]).substrate != full_comp[i])
                    { same = false; break; }
        check(same, "preview substrate == make_hard_coded_world Kepler, tile for tile");
    }

    std::printf("bench: %d runs, mean %.1f ms, worst %.1f ms\n",
                runs, total_ms / runs, worst_ms);
    // CEILING RAISED 1s -> 3s, 2026-08-12, for the 3x map (15,120 -> 45,240
    // tiles). The old bar was authored against a preview a third this size;
    // measured worst is now ~1220 ms, so 3s keeps roughly the same proportional
    // headroom the 1s bar gave before rather than being fitted to the new
    // number. Scaled, not merely widened — a bar moved to just above whatever
    // was measured stops being a regression detector.
    check(worst_ms < 3000.0, "worst preview under the 3s ceiling");

    std::printf(g_failures ? "FAILURES: %d\n" : "ALL PASS (0 failures)\n", g_failures);
    return g_failures ? 1 : 0;
}
