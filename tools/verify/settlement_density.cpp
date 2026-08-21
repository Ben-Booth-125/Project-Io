// ---------------------------------------------------------------------------
// settlement_density (Sprint B3) — how DENSE is the settled world, in numbers?
// ---------------------------------------------------------------------------
//
// WHY THIS EXISTS, and why it is not substrate_census.
//
// substrate_census (Sprint B1, NEW-10) answers "how civilised is the world" —
// built share, settled share, road reach, market coverage — and to answer it
// honestly it must run the whole start_new_game ordering including
// `generate_background_firms`, which needs LIVE LUA (economy.lua's demand
// baskets are all-zero on a default registry, so a census without Lua reports
// zero firms). That makes it a sol2-linked, ~1 s/world Release / ~62 s/world
// Debug sweep harness.
//
// The population-centre DENSITY question is strictly narrower and needs none of
// that. Centre placement is a pure function of the tile surface (habitability,
// extractable deposit richness) plus, for the coverage pass, the nation borders.
// No recipe, no firm, no market, no Lua. So this harness links the plain
// io_world objects and reports the one distribution Sprint B3 has to pin a
// constant against:
//
//     centres per body, centres PER NATION, and land tiles per centre.
//
// The per-nation column is the load-bearing one and is why this harness exists
// at all. BL-463 (landed 2026-08-21) made the WORLD count vary with land, which
// was the defect it was written against. It did not ask what the resulting
// distribution looks like from a nation's point of view, and that is the shape
// Ben's "does not appear physically civilised" is actually about: a nation with
// one centre has one roaded tile and no internal network, because
// road_generation.cpp's per-nation backbone skips any nation with `n < 2`
// centres.
//
// THE DIVISOR SWEEP. `generate_population_centres` takes the density divisor as
// a defaulted parameter (default = the shipped `k_land_tiles_per_centre`), so
// this harness can re-run placement at ALTERNATIVE divisors on a COPY of an
// already-generated world — clearing the three centre maps first — and print
// what each would produce. That is the instrument BL-463 § "Direction, not a
// chosen number" asks for: pin the band by measurement, never by picking a
// figure at the derivation site. The world itself is generated once per seed;
// only the (cheap) placement pass is repeated.
//
// THIS REPORTS; ITS ONLY GATES ARE STRUCTURAL. Same discipline substrate_census
// and history_sweep argue at length: show the spread across seeds, including
// the ugly tail, BEFORE anyone writes a threshold that hides it. The PASS/FAIL
// rows below assert only things that must be true for the world to be a world,
// plus one property BL-463 committed to (every nation holds a centre) restated
// here so it is checked by a harness that can run in a Lua-free tree.
//
// PREHISTORY AT ITS DEFAULT (400 years), deliberately, and it is most of the
// runtime. The nation set is what the Era -1 ladder leaves behind, and
// `ensure_national_population_centres` founds one seat per uncovered nation — so
// the centre count is a function of the era. A run under `no_prehistory()` would
// describe a world nobody plays and would understate the count. Measured on this
// box, g++ -O1: ~2.7 s/world, so the three-seed default is ~8 s. Registered as a
// `sweep` in CMakeLists anyway, on substrate_census's own reasoning: a Debug tree
// is far slower, and this harness gates nothing a routine run needs.
//
// Usage:  settlement_density [seed_count]      (default 3)
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/placement_rules.hpp"
#include "world/population_generation.hpp"
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

// The per-body seed derivations hard_coded_world.cpp uses for the two centre
// passes. Mirrored here so a re-run at an alternative divisor reproduces the
// shipped placement EXACTLY when the divisor is the shipped one — which the
// self-check row below asserts, rather than assuming.
unsigned primary_seed(unsigned world_seed)  { return world_seed ^ 0x70701001u; }
unsigned coverage_seed(unsigned world_seed) { return world_seed ^ 0x70702002u; }

struct density_row
{
    int    divisor        = 0;
    int    centres        = 0;
    int    primary_target = 0;   // land / divisor — what the primary pass aimed at
    int    coverage       = 0;   // centres - primary_target: the per-nation top-up
    int    uncovered      = 0;
    int    max_per_nation = 0;
    double land_per_centre    = 0.0;
    double centres_per_nation = 0.0;
    double median_per_nation  = 0.0;
    int    nations_below_two  = 0; // no road backbone (road_generation.cpp `n < 2`)
    int    hist[6]        = { 0, 0, 0, 0, 0, 0 }; // centres-per-nation: 0,1,2,3,4,5+
    long long population  = 0;
};

double median_of(std::vector<int> v)
{
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2) ? static_cast<double>(v[n / 2])
                   : 0.5 * (static_cast<double>(v[n / 2 - 1]) + static_cast<double>(v[n / 2]));
}

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

/// Tally the centre distribution of @p w over @p home.
density_row tally(const world& w, entity_id home,
                  const std::vector<entity_id>& nation_ids, int land, int divisor)
{
    density_row r;
    r.divisor        = divisor;
    r.primary_target = std::max(1, land / divisor);

    std::map<entity_id, int> per_nation;
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit == w.tiles.end() || tit->second.body != home) continue;
        ++r.centres;

        const auto pit = w.population_centres.find(cid);
        if (pit != w.population_centres.end())
            r.population += pit->second.population;

        const auto nit = w.tile_to_nation.find(tid);
        if (nit != w.tile_to_nation.end())
            ++per_nation[nit->second];
    }
    r.coverage = r.centres - r.primary_target;

    std::vector<int> counts;
    counts.reserve(nation_ids.size());
    for (const entity_id nid : nation_ids)
    {
        const auto it = per_nation.find(nid);
        const int c = (it == per_nation.end()) ? 0 : it->second;
        counts.push_back(c);
        if (c == 0) ++r.uncovered;
        if (c <  2) ++r.nations_below_two;
        ++r.hist[std::min(c, 5)];
        r.max_per_nation = std::max(r.max_per_nation, c);
    }

    r.land_per_centre    = r.centres ? static_cast<double>(land) / r.centres : 0.0;
    r.centres_per_nation = nation_ids.empty()
        ? 0.0 : static_cast<double>(r.centres) / static_cast<double>(nation_ids.size());
    r.median_per_nation  = median_of(counts);
    return r;
}

void print_row(const density_row& r)
{
    std::printf("  %6d | %5d | %5d | %5d | %8.1f | %6.2f | %5.1f | %4d | %5d"
                " | %4d %4d %4d %4d %4d %4d | %8lld\n",
                r.divisor, r.centres, r.primary_target, r.coverage,
                r.land_per_centre, r.centres_per_nation, r.median_per_nation,
                r.max_per_nation, r.nations_below_two,
                r.hist[0], r.hist[1], r.hist[2], r.hist[3], r.hist[4], r.hist[5],
                r.population);
}

void print_header()
{
    std::printf("  divisr | cntrs | prim. | cover |land/cent| c/nat  | med   |  max | <2 ctr"
                " |  n0   n1   n2   n3   n4  n5+ |    pop k\n");
    std::printf("  -------+-------+-------+-------+---------+--------+-------+------+-------"
                "+------------------------------+---------\n");
}

/// Strip every population centre from @p w so placement can be re-run.
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

    // The divisors swept. The shipped value is first so the self-check row can
    // compare a re-run against the world as generated.
    const std::vector<int> divisors = { k_land_tiles_per_centre, 300, 200, 150, 120, 100, 80, 60 };

    std::printf("settlement_density — population-centre density across %d seed(s)\n", seed_count);
    std::printf("Home grid %dx%d = %d tiles. Shipped divisor k_land_tiles_per_centre = %d.\n",
                home_grid_width, home_grid_height, home_grid_width * home_grid_height,
                k_land_tiles_per_centre);
    std::printf("`<2 ctr` = nations that get NO road backbone (road_generation.cpp `n < 2`).\n");

    // Aggregate per divisor across seeds.
    std::map<int, density_row> agg;
    long long agg_land = 0, agg_nations = 0;
    bool self_check_ok = true;

    for (int s = 0; s < seed_count; ++s)
    {
        world_params wp;
        wp.seed = static_cast<unsigned>(s);
        const world base = make_hard_coded_world(wp);

        const entity_id home    = base.home_body;
        const int       land    = land_tiles_of(base, home);
        const auto      nations = nations_on_body(base, home);

        agg_land    += land;
        agg_nations += static_cast<long long>(nations.size());

        std::printf("\nSEED %d — home body: %d land tiles, %d nations\n",
                    s, land, static_cast<int>(nations.size()));
        print_header();

        const density_row shipped = tally(base, home, nations, land, k_land_tiles_per_centre);
        print_row(shipped);

        for (const int d : divisors)
        {
            world scratch = base;                       // deep copy: plain maps
            clear_centres(scratch, home);
            generate_population_centres(scratch, home, primary_seed(wp.seed), d);
            ensure_national_population_centres(scratch, home, coverage_seed(wp.seed));

            const density_row r = tally(scratch, home, nations, land, d);

            // Self-check: re-running at the SHIPPED divisor must reproduce the
            // world exactly, or every other row in this table is measuring a
            // different pipeline than the one that ships.
            if (d == k_land_tiles_per_centre)
            {
                if (r.centres != shipped.centres || r.population != shipped.population)
                {
                    self_check_ok = false;
                    std::printf("  !! re-run at shipped divisor differs: %d centres / %lldk"
                                " vs %d / %lldk\n",
                                r.centres, r.population, shipped.centres, shipped.population);
                }
                continue; // already printed as the shipped row
            }

            print_row(r);

            density_row& a = agg[d];
            a.divisor        = d;
            a.centres        += r.centres;
            a.primary_target += r.primary_target;
            a.coverage       += r.coverage;
            a.uncovered      += r.uncovered;
            a.nations_below_two += r.nations_below_two;
            a.population     += r.population;
            a.max_per_nation = std::max(a.max_per_nation, r.max_per_nation);
            for (int i = 0; i <= 5; ++i) a.hist[i] += r.hist[i];
        }

        density_row& a0 = agg[k_land_tiles_per_centre];
        a0.divisor        = k_land_tiles_per_centre;
        a0.centres        += shipped.centres;
        a0.primary_target += shipped.primary_target;
        a0.coverage       += shipped.coverage;
        a0.uncovered      += shipped.uncovered;
        a0.nations_below_two += shipped.nations_below_two;
        a0.population     += shipped.population;
        a0.max_per_nation = std::max(a0.max_per_nation, shipped.max_per_nation);
        for (int i = 0; i <= 5; ++i) a0.hist[i] += shipped.hist[i];
    }

    // ---- aggregate ---------------------------------------------------------
    std::printf("\nAGGREGATE over %d seed(s) — %lld land tiles, %lld nations\n",
                seed_count, agg_land, agg_nations);
    std::printf("  divisr | cntrs | prim. | cover |land/cent| c/nat  | <2 ctr | share <2"
                " |  n0   n1   n2   n3   n4  n5+\n");
    std::printf("  -------+-------+-------+-------+---------+--------+--------+----------"
                "+------------------------------\n");
    for (const auto& [d, a] : agg)
    {
        const double lpc  = a.centres ? static_cast<double>(agg_land) / a.centres : 0.0;
        const double cpn  = agg_nations ? static_cast<double>(a.centres) / agg_nations : 0.0;
        const double shr  = agg_nations
            ? 100.0 * static_cast<double>(a.nations_below_two) / agg_nations : 0.0;
        std::printf("  %6d | %5d | %5d | %5d | %8.1f | %6.2f | %6d | %7.1f%%"
                    " | %4d %4d %4d %4d %4d %4d\n",
                    d, a.centres, a.primary_target, a.coverage, lpc, cpn,
                    a.nations_below_two, shr,
                    a.hist[0], a.hist[1], a.hist[2], a.hist[3], a.hist[4], a.hist[5]);
    }

    // ---- structural checks -------------------------------------------------
    std::printf("\nSTRUCTURAL CHECKS\n");
    const density_row& shipped = agg[k_land_tiles_per_centre];
    check(agg_land > 0,        "S1  every seed produced land on the home body");
    check(shipped.centres > 0, "S2  the shipped divisor places population centres");
    check(shipped.uncovered == 0,
          "S3  every nation on the home body holds at least one centre (BL-463's acceptance test)");
    check(self_check_ok,
          "S4  re-running placement at the shipped divisor reproduces the generated world");

    std::printf("\n%s (%d failure(s))\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
