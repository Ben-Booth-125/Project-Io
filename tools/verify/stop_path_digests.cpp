// stop_path_digests — one digest line per (seed, stop point) of make_hard_coded_world,
// for comparing TWO SOURCE TREES rather than two runs of one binary.
//
// WHY THIS EXISTS (BL-1084, the cold review of the generation cursor, 2026-09-25). Every
// determinism check in tools/verify compares a binary with itself: world_determinism
// builds the same seed twice, world_cursor_equivalence compares the composition with its
// own staged copy. Neither can see a refactor that moves BOTH sides together, and the
// only cross-tree pins (player_seed_sweep --digest-check, the seed library's sweep) cover
// full builds and the Exploration stop. The four stop flags the wizard's rounds were built
// on — migration, ancient era, exploration, industrialisation — had no cross-tree evidence
// at all when the monolith was split into stages.
//
// So this harness prints, and asserts nothing. Build it in the old tree and the new one,
// run both with the same arguments, and diff the output: every line must match. It folds
// the world's deep digest (world_deep_digest.hpp, the digest world_determinism prints) and
// the report fields a stopped build publishes — the four span records, the settlement the
// body entry carries, and every per-span counter.
//
// Run:  stop_path_digests [seed ...]        (default seeds: 0 28 46)
// Use:  build and run at the base commit and at HEAD; `diff` the two outputs.

#include "world/hard_coded_world.hpp"
#include "world/era_minus_one.hpp"
#include "world/world_gen_config.hpp"
#include "world_deep_digest.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{

// fold_bytes / fold_i64 / fold_str come from world_deep_digest.hpp.

void fold_lapse(uint64_t& h, const era_timelapse& t)
{
    fold_i64(h, t.region_stride);
    fold_i64(h, t.start_year);
    fold_i64(h, t.years);
    fold_i64(h, static_cast<int64_t>(t.changes.size()));
    for (const owner_change& c : t.changes)
    {
        fold_i64(h, c.year);
        fold_i64(h, c.region);
        fold_i64(h, c.owner);
    }
    // Sizes only for the playback tables: their element structs are wide, and a
    // divergence in them moves the owner stream or the world digest as well.
    fold_i64(h, static_cast<int64_t>(t.steps.size()));
    fold_i64(h, static_cast<int64_t>(t.samples.size()));
    fold_i64(h, static_cast<int64_t>(t.culture_changes.size()));
    fold_i64(h, static_cast<int64_t>(t.events.size()));
}

uint64_t report_digest(const generation_report& r)
{
    uint64_t h = 1469598103934665603ull;
    const int64_t counters[] = {
        r.prehistory_years, r.prehistory_battles, r.prehistory_conquests, r.prehistory_foundings,
        r.exploration_years, r.exploration_battles, r.exploration_conquests, r.exploration_foundings,
        r.industrialisation_years, r.industrialisation_battles, r.industrialisation_conquests,
        r.industrialisation_foundings, r.prehistory_corridors, r.prehistory_junctions,
        r.markets_from_trade};
    for (const int64_t c : counters) fold_i64(h, c);
    fold_i64(h, static_cast<int64_t>(r.bodies.size()));
    for (const generation_report::body_entry& be : r.bodies)
    {
        fold_str(h, be.name);
        fold_i64(h, static_cast<int64_t>(be.id));
        fold_lapse(h, be.migration_timelapse);
        fold_lapse(h, be.prehistory_timelapse);
        fold_lapse(h, be.exploration_timelapse);
        fold_lapse(h, be.industrialisation_timelapse);
        fold_i64(h, static_cast<int64_t>(be.settlement.regions.size()));
        for (const region& rg : be.settlement.regions)
        {
            fold_i64(h, rg.anchor);
            fold_i64(h, rg.founding_culture);
            fold_i64(h, rg.creed_conquered ? 1 : 0);
            fold_i64(h, rg.settle_score_q);
            fold_i64(h, rg.has_market ? 1 : 0);
            fold_i64(h, rg.is_seat ? 1 : 0);
        }
    }
    return h;
}

struct stop_point { const char* name; world_gen_config cfg; };

std::vector<stop_point> stop_points()
{
    std::vector<stop_point> v;
    v.push_back({"migration", {}});         v.back().cfg.stop_after_migration = true;
    v.push_back({"ancient_era", {}});       v.back().cfg.stop_after_ancient_era = true;
    v.push_back({"exploration", {}});       v.back().cfg.stop_after_exploration = true;
    v.push_back({"industrialisation", {}}); v.back().cfg.stop_after_industrialisation = true;
    v.push_back({"full", {}});
    return v;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
        seeds.push_back(static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    if (seeds.empty()) seeds = {0u, 28u, 46u};

    for (const uint32_t seed : seeds)
    {
        for (const stop_point& sp : stop_points())
        {
            world_params p{};
            p.seed = seed;
            generation_report     rep{};
            era_minus_one_fixture fx;
            const world w = make_hard_coded_world(p, &rep, sp.cfg, nullptr, nullptr, &fx);
            std::printf("seed %u  stop %-17s  world %016llX  report %016llX  tiles %zu  bodies %zu\n",
                        seed, sp.name,
                        static_cast<unsigned long long>(deep_digest(w, fx)),
                        static_cast<unsigned long long>(report_digest(rep)),
                        w.tiles.size(), w.bodies.size());
            std::fflush(stdout);
        }
    }
    return 0;
}
