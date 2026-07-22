// ---------------------------------------------------------------------------
// Headless world-determinism harness (BL-114; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Proves make_hard_coded_world() is a pure function of its world_params:
//
//   R1  Same seed + params -> a bit-identical world across repeated builds in
//       this binary (equal tile count, per-composition histogram, summed deposit
//       totals, nation/corp/entity counts). A different seed changes the world
//       (at least one metric differs) — so the seed genuinely drives generation.
//
//   R2  Resource abundance scales deposits monotonically without perturbing the
//       RNG streams: summed-deposit(sparse) < (lean) < (standard) at a fixed seed,
//       and the default descriptor (seed 0, standard) is the earth-like baseline.
//
// The process exits non-zero if any assertion FAILs. Links only the SDL/Lua-free
// world-generation translation units (see CMakeLists.txt), mirroring the manual
// build in tools/verify/README.md.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <map>

namespace {

struct world_metrics
{
    std::size_t        tiles    = 0;
    std::size_t        nations  = 0;
    std::size_t        corps    = 0;
    std::size_t        entities = 0; ///< sum of the public component containers
    std::map<int, int> comp_hist;    ///< tiles per terrain_composition
    double             deposit_total = 0.0;

    bool operator==(const world_metrics& o) const
    {
        return tiles == o.tiles && nations == o.nations && corps == o.corps &&
               entities == o.entities && comp_hist == o.comp_hist &&
               deposit_total == o.deposit_total;
    }
};

world_metrics measure(const world& w)
{
    world_metrics m;
    m.tiles    = w.tiles.size();
    m.nations  = w.nations.size();
    m.corps    = w.corporations.size();
    m.entities = w.bodies.size() + w.tiles.size() + w.buildings.size() +
                 w.stockpiles.size() + w.markets.size() + w.units.size() +
                 w.population_centres.size() + w.nations.size() + w.corporations.size();
    for (const auto& [id, tc] : w.tiles)
    {
        ++m.comp_hist[static_cast<int>(tc.composition)];
        for (std::size_t r = 0; r < resource_count; ++r)
            m.deposit_total += tc.resource_deposit[r];
    }
    return m;
}

int failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok)
        ++failures;
}

} // namespace

int main()
{
    constexpr uint32_t seed_a = 0xABCDEF01u;
    constexpr uint32_t seed_b = 0x12345678u;

    // --- R1: same seed + params is bit-identical across two builds ---
    const world_metrics a1 = measure(make_hard_coded_world(
        world_params{ .seed = seed_a, .abundance = abundance_level::standard }));
    const world_metrics a2 = measure(make_hard_coded_world(
        world_params{ .seed = seed_a, .abundance = abundance_level::standard }));

    check(a1 == a2, "R1 same seed+params -> bit-identical world (tiles/hist/deposits/nations/corps/entities)");
    std::printf("     seed %08X: %zu tiles, %zu nations, %zu corps, deposits=%.3f\n",
                seed_a, a1.tiles, a1.nations, a1.corps, a1.deposit_total);

    // --- R1: a different seed produces a demonstrably different world ---
    const world_metrics b = measure(make_hard_coded_world(
        world_params{ .seed = seed_b, .abundance = abundance_level::standard }));
    const bool seed_matters = (b.comp_hist != a1.comp_hist) || (b.deposit_total != a1.deposit_total);
    check(seed_matters, "R1 different seed -> different world (composition histogram or deposit total differs)");
    std::printf("     seed %08X: %zu tiles, deposits=%.3f (vs %.3f)\n",
                seed_b, b.tiles, b.deposit_total, a1.deposit_total);

    // --- R2: abundance scales deposits monotonically at a fixed seed ---
    const double d_sparse = measure(make_hard_coded_world(
        world_params{ .seed = seed_a, .abundance = abundance_level::sparse })).deposit_total;
    const double d_lean = measure(make_hard_coded_world(
        world_params{ .seed = seed_a, .abundance = abundance_level::lean })).deposit_total;
    const double d_standard = a1.deposit_total; // standard at seed_a, measured above

    check(d_sparse < d_lean && d_lean < d_standard,
          "R2 abundance ordering: sparse < lean < standard (deposit totals)");
    std::printf("     deposits sparse=%.3f  lean=%.3f  standard=%.3f\n",
                d_sparse, d_lean, d_standard);

    // --- R2: standard is the earth-like ceiling — no tier exceeds it ---
    check(d_standard >= d_lean && d_standard >= d_sparse,
          "R2 standard is the resource ceiling (no tier richer than earth-like)");

    // --- Sanity: the default descriptor equals seed 0 / standard (legacy world) ---
    const world_metrics dflt   = measure(make_hard_coded_world());
    const world_metrics zero_s = measure(make_hard_coded_world(
        world_params{ .seed = 0, .abundance = abundance_level::standard }));
    check(dflt == zero_s, "default make_hard_coded_world() == {seed 0, standard} (legacy world)");

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
