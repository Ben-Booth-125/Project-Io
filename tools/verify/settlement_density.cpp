// ---------------------------------------------------------------------------
// settlement_density — how DENSE is the settled world, in numbers?
// ---------------------------------------------------------------------------
//
// WHY THIS EXISTS, and why it is not substrate_census.
//
// substrate_census (Sprint B1, NEW-10) answers "how civilised is the world" —
// built share, settled share, road reach, market coverage — and to answer it
// honestly it must run the whole start_new_game ordering including
// `generate_background_firms`, which needs LIVE LUA. That makes it a
// sol2-linked sweep harness. The population-centre DENSITY question is
// strictly narrower and needs none of that, so this harness links the plain
// io_world objects.
//
// WHAT IT MEASURES (BL-610, centres from demography). Centre count and scale
// derive from the Era -1 regions' simulated populations — density is
// history's consequence, not a divisor. The quantities that calibrate
// `k_demography_heads_per_centre` and that BL-611 (province centre anchor)
// depends on are therefore:
//
//     land tiles per centre        — the anchor ruling needs ~1 per ~10;
//     the scale histogram          — "most are small" is Ben's ruling, so the
//                                    shape is REPORTED, never asserted;
//     per-nation coverage          — BL-463's acceptance test, still asserted;
//     per-LAND-PROVINCE coverage   — the fraction of land provinces holding at
//                                    least one centre, the number BL-611 turns
//                                    into a structural guarantee.
//
// THE DIVISOR SWEEP IS GONE. Until BL-610 this harness re-ran placement at
// alternative `k_land_tiles_per_centre` divisors on a copy of the world. The
// campaign path no longer reads a divisor, and the settlement record the new
// path reads lives inside make_hard_coded_world, so a re-run out here cannot
// reproduce the shipped placement. The FALLBACK path (no settlement record)
// still takes the divisor, and one informational row per seed shows what it
// would have produced, so the fallback keeps getting exercised and the old
// baseline stays visible beside the new density.
//
// THIS REPORTS; ITS ONLY GATES ARE STRUCTURAL. Same discipline substrate_census
// and history_sweep argue at length: show the spread across seeds, including
// the ugly tail, BEFORE anyone writes a threshold that hides it.
//
// Usage:  settlement_density [seed_count]      (default 3)
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/placement_rules.hpp"
#include "world/population_generation.hpp"
#include "world/province.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

// The per-body seed derivation hard_coded_world.cpp uses for the primary centre
// pass, mirrored so the fallback row below runs off the same stream.
unsigned primary_seed(unsigned world_seed) { return world_seed ^ 0x70701001u; }

/// Nations holding territory on @p home, in ascending id order.
std::vector<entity_id> nations_on_body(const world& w, entity_id home)
{
    std::vector<entity_id> out;
    for (const auto& [nid, nc] : w.nations)
    {
        for (const entity_id tid : nc.tiles)
        {
            const auto tit = w.tiles.find(tid);
            if (tit != w.tiles.end() && tit->second.body == home) { out.push_back(nid); break; }
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

int land_tiles_of(const world& w, entity_id home)
{
    int n = 0;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == home && !is_water(tc.substrate))
            ++n;
    return n;
}

struct density_row
{
    int    centres     = 0;
    int    uncovered   = 0;            // nations with zero centres
    int    below_two   = 0;            // nations with < 2 (no road backbone)
    int    scale_hist[5] = { 0, 0, 0, 0, 0 };
    long long population = 0;          // thousands (component units)
    double land_per_centre = 0.0;
};

density_row tally(const world& w, entity_id home,
                  const std::vector<entity_id>& nation_ids, int land)
{
    density_row r;
    std::map<entity_id, int> per_nation;
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit == w.tiles.end() || tit->second.body != home) continue;
        ++r.centres;

        const auto pit = w.population_centres.find(cid);
        if (pit != w.population_centres.end())
        {
            r.population += pit->second.population;
            const int s = std::clamp(pit->second.scale, 1, 5);
            ++r.scale_hist[s - 1];
        }

        const auto nit = w.tile_to_nation.find(tid);
        if (nit != w.tile_to_nation.end())
            ++per_nation[nit->second];
    }
    for (const entity_id nid : nation_ids)
    {
        const auto it = per_nation.find(nid);
        const int  c  = (it == per_nation.end()) ? 0 : it->second;
        if (c == 0) ++r.uncovered;
        if (c <  2) ++r.below_two;
    }
    r.land_per_centre = r.centres ? static_cast<double>(land) / r.centres : 0.0;
    return r;
}

/// Per-LAND-PROVINCE coverage on @p home: how many land provinces hold at
/// least one population centre. The number BL-611 (province centre anchor)
/// turns into a structural guarantee.
void province_coverage(const world& w, entity_id home,
                       int& land_provinces, int& covered)
{
    land_provinces = 0;
    covered        = 0;

    std::set<uint32_t> with_centre;
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit == w.tiles.end() || tit->second.body != home) continue;
        const uint32_t pid = w.provinces.province_of(tid);
        if (pid != 0) // 0 = not in any province (P8d: no real id is 0)
            with_centre.insert(pid);
    }

    for (const province& pr : w.provinces.provinces)
    {
        if (pr.body != home) continue;
        if (province_kind_of(w, pr) != province_kind::land) continue;
        ++land_provinces;
        if (with_centre.count(pr.id)) ++covered;
    }
}

/// Strip every population centre from @p w so the fallback row can re-run.
void clear_centres(world& w, entity_id home)
{
    std::vector<entity_id> doomed;
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit != w.tiles.end() && tit->second.body == home)
            doomed.push_back(cid);
    }
    for (const entity_id cid : doomed)
    {
        w.population_centres.erase(cid);
        w.population_centre_tile.erase(cid);
        w.population_centre_name.erase(cid);
    }
}

} // namespace

int main(int argc, char** argv)
{
    int seed_count = 3;
    if (argc > 1)
    {
        const int n = std::atoi(argv[1]);
        if (n > 0) seed_count = n;
    }

    std::printf("settlement_density — demography-derived centre density across %d seed(s)\n",
                seed_count);
    std::printf("k_demography_heads_per_centre = %d heads; fallback divisor"
                " k_land_tiles_per_centre = %d.\n",
                k_demography_heads_per_centre, k_land_tiles_per_centre);

    long long agg_land = 0, agg_centres = 0, agg_pop = 0;
    long long agg_prov = 0, agg_prov_cov = 0;
    int       agg_hist[5]   = { 0, 0, 0, 0, 0 };
    int       agg_uncovered = 0, agg_below_two = 0;
    bool      fallback_ok   = true;

    std::printf("\n  seed |  land | cntrs |land/cent|   s1    s2    s3    s4    s5 |"
                "    pop k | n<1 | n<2 | landprov | covered | cover%%\n");
    std::printf("  -----+-------+-------+---------+------------------------------+"
                "----------+-----+-----+----------+---------+-------\n");

    for (int s = 0; s < seed_count; ++s)
    {
        world_params wp;
        wp.seed = static_cast<unsigned>(s);
        const world base = make_hard_coded_world(wp);

        const entity_id home    = base.home_body;
        const int       land    = land_tiles_of(base, home);
        const auto      nations = nations_on_body(base, home);

        const density_row r = tally(base, home, nations, land);

        int prov = 0, cov = 0;
        province_coverage(base, home, prov, cov);

        agg_land      += land;
        agg_centres   += r.centres;
        agg_pop       += r.population;
        agg_uncovered += r.uncovered;
        agg_below_two += r.below_two;
        agg_prov      += prov;
        agg_prov_cov  += cov;
        for (int i = 0; i < 5; ++i) agg_hist[i] += r.scale_hist[i];

        std::printf("  %4d | %5d | %5d | %7.2f | %4d  %4d  %4d  %4d  %4d |"
                    " %8lld | %3d | %3d | %8d | %7d | %5.1f%%\n",
                    s, land, r.centres, r.land_per_centre,
                    r.scale_hist[0], r.scale_hist[1], r.scale_hist[2],
                    r.scale_hist[3], r.scale_hist[4],
                    r.population, r.uncovered, r.below_two, prov, cov,
                    prov ? 100.0 * cov / prov : 0.0);

        // The FALLBACK row (informational): what the no-settlement path would
        // place on this same tile surface. Exercises the divisor path the
        // campaign no longer takes, and keeps the old baseline visible.
        {
            world scratch = base;
            clear_centres(scratch, home);
            generate_population_centres(scratch, home, primary_seed(wp.seed),
                                        /*settlement=*/nullptr);
            const density_row f = tally(scratch, home, nations, land);
            if (f.centres <= 0) fallback_ok = false;
            std::printf("   fbk |       | %5d | %7.2f | %4d  %4d  %4d  %4d  %4d |"
                        " %8lld |     |     |          |         |\n",
                        f.centres, f.land_per_centre,
                        f.scale_hist[0], f.scale_hist[1], f.scale_hist[2],
                        f.scale_hist[3], f.scale_hist[4], f.population);
        }
    }

    std::printf("\nAGGREGATE over %d seed(s)\n", seed_count);
    std::printf("  %lld centres over %lld land tiles — %.2f land tiles per centre\n",
                agg_centres, agg_land,
                agg_centres ? static_cast<double>(agg_land) / agg_centres : 0.0);
    std::printf("  scale histogram: s1=%d s2=%d s3=%d s4=%d s5=%d (total pop %lldk)\n",
                agg_hist[0], agg_hist[1], agg_hist[2], agg_hist[3], agg_hist[4], agg_pop);
    std::printf("  land provinces: %lld, holding a centre: %lld (%.1f%%)\n",
                agg_prov, agg_prov_cov,
                agg_prov ? 100.0 * agg_prov_cov / agg_prov : 0.0);

    // ---- structural checks -------------------------------------------------
    std::printf("\nSTRUCTURAL CHECKS\n");
    check(agg_land > 0,    "S1  every seed produced land on the home body");
    check(agg_centres > 0, "S2  the campaign path places population centres");
    check(agg_uncovered == 0,
          "S3  every nation on the home body holds at least one centre (BL-463's acceptance test)");
    check(fallback_ok,
          "S4  the no-settlement fallback path still places centres");

    std::printf("\n%s (%d failure(s))\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
