// Sea-leg census (Sprint B2 — the BL-188 un-parking decision).
//
// WHY THIS EXISTS. BL-188 (coastal ports / sea trade) is parked, and the Sprint
// B2 brief asks whether the road-generation cuts unblock it. They do not — they
// are a change to the road RASTER — so the real question is what the sea mode
// currently is, and that is an arithmetic claim nobody had measured.
//
// `logistics_path::crosses_ocean` is a BOOLEAN over the whole route. Two
// consequences follow from that single bit, and this harness measures both
// rather than asserting them from a reading of the source:
//
//   S1  HOW MUCH of a sea-mode route is actually water. If the median sea-mode
//       route is 3% water, then "sea mode" is in practice a land route that
//       happened to clip a shoreline, and the whole route is being priced and
//       timed as a voyage.
//
//   S2  WHICH WATER KIND flips the bit. BL-516 made water three things
//       (lake / coast / ocean) but tile_traversal_cost and crosses_ocean both
//       still read the undifferentiated `is_water`. A route flipped to sea mode
//       by a LAKE tile is the clearest case of the bit meaning nothing.
//
//   S3  WHAT THE BIT COSTS. The supply layer charges
//           haul_per_unit = logistics_cost(mode) * path.cost
//       with land = 0.02 and sea = 0.05 (scripts/economy.lua), and times the
//       haul at coastal_km_per_day (130) instead of caravan_km_per_day (25).
//       So one water tile makes the WHOLE route 2.5x dearer and 5.2x faster.
//       S3 reports the money delta a per-leg split would produce, against the
//       whole-route charge shipped today.
//
// The sea rate itself is the finding BL-188's own settled design contradicts:
// the item rules (2026-08-02) that "sea's advantage is a lower per-unit RATE";
// the shipped rate is 2.5x the land rate. This harness does not change it —
// a calibration constant is Ben's call — it measures what depends on it.
//
// REPORT-FIRST. S1/S2/S3 print distributions. The assertions are vacuity guards
// (it measured a non-empty world) plus one real invariant, S4: the census is
// deterministic across two generations of the same seed.
//
// Route population: every ordered market pair on a body, which is exactly the
// set `dispatch_convoys` prices intra-body legs over (supply_system.cpp).
//
// Run: build_gen/verify/sea_leg_census [seeds]

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

static int g_fail = 0;
static void check(bool ok, const char* what)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

namespace {

// The shipped constants this census reads. Restated rather than linked because
// the Lua-authored values live in scripts/economy.lua and this harness is
// deliberately Lua-free; if economy.lua moves, these must move with it.
constexpr float kLandRate = 0.02f; // logistics.base_cost_per_unit_distance.land
constexpr float kSeaRate  = 0.05f; // logistics.base_cost_per_unit_distance.sea

struct route_stat
{
    float cost        = 0.0f;
    int   tiles       = 0;
    int   water_tiles = 0;
    float water_cost  = 0.0f; ///< The water tiles' share of path.cost, approximated
                              ///< by their per-tile traversal cost.
    bool  any_lake    = false;
    bool  any_coast   = false;
    bool  any_ocean   = false;
};

struct census
{
    int                nation_routes = 0; ///< every reachable market pair measured
    int                sea_routes    = 0;
    std::vector<float> water_share;       ///< water tiles / total tiles, sea routes only
    int                flipped_by_lake_only  = 0; ///< sea mode, and no coast/ocean tile at all
    int                flipped_by_no_ocean   = 0; ///< sea mode, and no OPEN-ocean tile at all
    double             charged_whole_route   = 0.0; ///< sum over sea routes: kSeaRate * cost
    double             charged_split         = 0.0; ///< sum: kSeaRate*water + kLandRate*land
};

/// Every reachable ordered market pair on one body, measured.
void measure_body(world& w, entity_id body, const std::vector<entity_id>& centres, census& c)
{
    for (std::size_t i = 0; i < centres.size(); ++i)
        for (std::size_t j = i + 1; j < centres.size(); ++j)
        {
            const logistics_path& p = intra_body_path(w, body, centres[i], centres[j]);
            if (!p.reachable || p.tiles.empty())
                continue;
            ++c.nation_routes;
            if (!p.crosses_ocean)
                continue;
            ++c.sea_routes;

            route_stat rs;
            rs.cost  = p.cost;
            rs.tiles = static_cast<int>(p.tiles.size());
            for (const entity_id t : p.tiles)
            {
                const auto it = w.tiles.find(t);
                if (it == w.tiles.end())
                    continue;
                const terrain_substrate s = it->second.substrate;
                if (!is_water(s))
                    continue;
                ++rs.water_tiles;
                rs.water_cost += tile_traversal_cost(it->second);
                if (s == terrain_substrate::lake)  rs.any_lake  = true;
                if (is_coastal_water(s))           rs.any_coast = true;
                if (is_open_ocean(s))              rs.any_ocean = true;
            }
            c.water_share.push_back(rs.tiles ? static_cast<float>(rs.water_tiles)
                                                   / static_cast<float>(rs.tiles)
                                             : 0.0f);
            if (rs.any_lake && !rs.any_coast && !rs.any_ocean)
                ++c.flipped_by_lake_only;
            if (!rs.any_ocean)
                ++c.flipped_by_no_ocean;

            const float land_cost = std::max(0.0f, rs.cost - rs.water_cost);
            c.charged_whole_route += static_cast<double>(kSeaRate) * rs.cost;
            c.charged_split += static_cast<double>(kSeaRate) * rs.water_cost
                               + static_cast<double>(kLandRate) * land_cost;
        }
}

census run_census(world& w)
{
    census c;
    // Market centre tiles, grouped by body. std::map keeps body order ascending
    // and the per-body tile lists are sorted, so the walk is layout-independent.
    std::map<entity_id, std::vector<entity_id>> by_body;
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mid;
        if (mc.body == null_entity || mc.centre_tile == null_entity)
            continue;
        by_body[mc.body].push_back(mc.centre_tile);
    }
    for (auto& [body, centres] : by_body)
    {
        std::sort(centres.begin(), centres.end());
        centres.erase(std::unique(centres.begin(), centres.end()), centres.end());
        measure_body(w, body, centres, c);
    }
    return c;
}

float pct(int n, int d) { return d ? 100.0f * static_cast<float>(n) / static_cast<float>(d) : 0.0f; }

float quantile(std::vector<float> v, float q)
{
    if (v.empty())
        return 0.0f;
    std::sort(v.begin(), v.end());
    const std::size_t idx = static_cast<std::size_t>(q * static_cast<float>(v.size() - 1) + 0.5f);
    return v[std::min(idx, v.size() - 1)];
}

} // namespace

int main(int argc, char** argv)
{
    const int seeds = (argc > 1) ? std::atoi(argv[1]) : 8;

    census tot;
    std::printf("  seed  routes  sea-mode   share   median-water%%  lake-only  no-open-ocean\n");
    for (int s = 0; s < seeds; ++s)
    {
        world_params wp;
        wp.seed  = static_cast<std::uint32_t>(s) * 0x9E3779B1u;
        world ws = make_hard_coded_world(no_prehistory(wp));
        const census c = run_census(ws);
        std::printf("  %4d  %6d  %8d  %5.1f%%  %12.1f%%  %9d  %13d\n", s, c.nation_routes,
                    c.sea_routes, pct(c.sea_routes, c.nation_routes),
                    100.0f * quantile(c.water_share, 0.5f), c.flipped_by_lake_only,
                    c.flipped_by_no_ocean);

        tot.nation_routes += c.nation_routes;
        tot.sea_routes += c.sea_routes;
        tot.flipped_by_lake_only += c.flipped_by_lake_only;
        tot.flipped_by_no_ocean += c.flipped_by_no_ocean;
        tot.charged_whole_route += c.charged_whole_route;
        tot.charged_split += c.charged_split;
        tot.water_share.insert(tot.water_share.end(), c.water_share.begin(), c.water_share.end());
    }

    std::printf("\n  S1  sea-mode routes: %d of %d (%.1f%%)\n", tot.sea_routes, tot.nation_routes,
                pct(tot.sea_routes, tot.nation_routes));
    std::printf("      water share of a sea-mode route  p10 %.1f%%  median %.1f%%  p90 %.1f%%  max %.1f%%\n",
                100.0f * quantile(tot.water_share, 0.10f), 100.0f * quantile(tot.water_share, 0.50f),
                100.0f * quantile(tot.water_share, 0.90f), 100.0f * quantile(tot.water_share, 1.0f));
    {
        int one_tile = 0;
        for (const float f : tot.water_share)
            if (f > 0.0f && f <= 0.05f)
                ++one_tile;
        std::printf("      routes <= 5%% water (a clip, not a voyage): %d of %d (%.1f%%)\n", one_tile,
                    tot.sea_routes, pct(one_tile, tot.sea_routes));
    }

    std::printf("  S2  flipped to sea mode with NO open-ocean tile: %d (%.1f%% of sea routes)\n",
                tot.flipped_by_no_ocean, pct(tot.flipped_by_no_ocean, tot.sea_routes));
    std::printf("      flipped by a LAKE alone (no coast, no ocean): %d (%.1f%%)\n",
                tot.flipped_by_lake_only, pct(tot.flipped_by_lake_only, tot.sea_routes));

    std::printf("  S3  per-unit haulage charged on sea-mode routes, whole-route rate vs per-leg split:\n");
    std::printf("      whole route @ sea rate %.4f = %.1f;  split (water@sea + land@land) = %.1f\n",
                static_cast<double>(kSeaRate), tot.charged_whole_route, tot.charged_split);
    std::printf("      overcharge factor %.2fx (sea rate %.3f is %.1fx the land rate %.3f)\n",
                tot.charged_split > 0.0 ? tot.charged_whole_route / tot.charged_split : 0.0,
                static_cast<double>(kSeaRate), static_cast<double>(kSeaRate / kLandRate),
                static_cast<double>(kLandRate));

    check(tot.nation_routes > 0, "S0 census measured a non-empty route set");

    // S4 determinism — the census is a pure read, so the same seed twice must
    // produce an identical route population and an identical sea-mode count.
    world_params wp0;
    wp0.seed = 0;
    world      a  = make_hard_coded_world(no_prehistory(wp0));
    world      b  = make_hard_coded_world(no_prehistory(wp0));
    const census ca = run_census(a);
    const census cb = run_census(b);
    std::printf("      (routes %d vs %d; sea %d vs %d)\n", ca.nation_routes, cb.nation_routes,
                ca.sea_routes, cb.sea_routes);
    check(ca.nation_routes == cb.nation_routes && ca.sea_routes == cb.sea_routes
              && ca.water_share == cb.water_share,
          "S4 census identical across two generations of the same seed");

    std::printf("%s (%d failure(s))\n", g_fail ? "SEA LEG CENSUS FAILED" : "SEA LEG CENSUS OK", g_fail);
    return g_fail ? 1 : 0;
}
