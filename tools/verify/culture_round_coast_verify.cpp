// ---------------------------------------------------------------------------
// BL-947 verification harness — CULTURE_ROUND_COASTS_TO_400BCE.
// ---------------------------------------------------------------------------
// Confirms the fix: round 3's DISPLAYED span (report->prehistory_years,
// colonisation_start_year-relative) always closes at the Empires round's own
// opening year (era_minus_one_sim_params(p).start_year — -400 at the
// defaults) UNLESS the migration's own derived end ran later, in which case
// the true (later) year is kept and shown honestly rather than clamped.
//
// Also checks the coast is STATIC: the migration timelapse folded at the
// coasted end year carries the exact same `changes`/`events` as the one
// folded at the true derived end -- only `years` (the span length) grows.
// A coast that quietly added ownership churn past the true end would show up
// as a size or content mismatch here.
//
// Determinism: re-generates two of the sweep's seeds and checks the coasted
// end year is bit-identical across both runs.
//
// The process exits non-zero if any assertion FAILs.

#include "world/hard_coded_world.hpp"
#include "world/era_minus_one.hpp"
#include "world/world_gen_config.hpp"
#include "world/colonisation.hpp"

#include <cstdio>
#include <cstdint>
#include <algorithm>

namespace {
int g_pass = 0, g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

const generation_report::body_entry* kepler_of(const generation_report& r)
{
    for (const auto& b : r.bodies)
        if (b.is_homeworld) return &b;
    return nullptr;
}
} // namespace

int main()
{
    std::printf("=== BL-947 verification: the Culture round coasts to 400 BCE ===\n");

    world_gen_config cfg;
    cfg.stop_after_migration = true;

    const int N = 60;
    int at_boundary = 0, honest_overrun = 0, mismatched = 0;

    for (int i = 0; i < N; ++i)
    {
        world_params p;
        p.seed = 0xC001D00Du + static_cast<uint32_t>(i) * 0x9E3779B9u;
        const int64_t boundary = era_minus_one_sim_params(p).start_year;

        generation_report rep;
        make_hard_coded_world(p, &rep, cfg);
        const generation_report::body_entry* home = kepler_of(rep);
        if (!home) continue;

        const int64_t raw_end     = home->settlement.migration_end_year;
        const int64_t shown_end   = colonisation_start_year + rep.prehistory_years;
        const int64_t expect_end  = std::max(raw_end, boundary);

        if (shown_end != expect_end) ++mismatched;
        if (raw_end <= boundary) ++at_boundary; else ++honest_overrun;

        // The lapse folded through must carry the true migration's changes
        // and events UNCHANGED -- the coast adds no new ones. Compare
        // against a fold at the raw derived end.
        const era_timelapse coasted = build_migration_timelapse(
            home->settlement, colonisation_start_year, shown_end);
        const era_timelapse raw = build_migration_timelapse(
            home->settlement, colonisation_start_year, raw_end);
        const bool static_coast =
            coasted.changes.size() == raw.changes.size()
            && coasted.events.size() == raw.events.size()
            && std::equal(coasted.changes.begin(), coasted.changes.end(), raw.changes.begin(),
                         [](const owner_change& a, const owner_change& b) {
                             return a.year == b.year && a.region == b.region && a.owner == b.owner;
                         });
        if (!static_coast)
        {
            std::printf("  seed %10u  STATIC-COAST MISMATCH (changes %zu vs %zu, events %zu vs %zu)\n",
                        p.seed, coasted.changes.size(), raw.changes.size(),
                        coasted.events.size(), raw.events.size());
        }
        check(static_coast, "per-seed  the coast segment adds no new ownership/culture churn");
        check(shown_end == expect_end, "per-seed  the displayed end year matches max(true end, boundary)");
    }

    std::printf("=== %d at/before boundary (coasted forward), %d honest overrun (kept as-is), "
                "%d mismatched, of %d seeds ===\n",
                at_boundary, honest_overrun, mismatched, N);

    // Determinism: same seed, same coasted end year, twice.
    {
        world_params p;
        p.seed = 0xC001D00Du;
        generation_report r1, r2;
        make_hard_coded_world(p, &r1, cfg);
        make_hard_coded_world(p, &r2, cfg);
        const generation_report::body_entry* k1 = kepler_of(r1);
        const generation_report::body_entry* k2 = kepler_of(r2);
        check(k1 && k2 && r1.prehistory_years == r2.prehistory_years,
              "determinism  two generations of the same seed report the same coasted span");
    }

    std::printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
