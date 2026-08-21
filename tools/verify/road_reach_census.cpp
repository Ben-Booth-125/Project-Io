// Road-reach census (Sprint B2 — the three structural cuts in road_generation.cpp).
//
// Measures the metric those cuts exist to move: how many nations end generation
// with NO roaded tile anywhere in their territory. A road-less nation is not a
// stylistic gap — road_level feeds road_traversal_multiplier, so such a nation is
// simply more expensive to reach than the map's shape says it should be.
//
// Reports, over the 8-seed census set (the same sweep road_generation_harness
// uses for its highway-reachability check):
//   C1 road-less nations   — count and share, per seed and in total.
//   C2 single-centre share — how much of C1 is nations with one centre
//                            (road_generation.cpp's <2-centre early return).
//   C3 no-ocean invariant  — no tile with road_level > 0 is ocean.
//   C4 determinism         — regenerating seed 0 yields an identical road_level
//                            field AND an identical state_hash.
// Report-first by design: C1/C2 print numbers, C3/C4 assert.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/world.hpp"

#include <cstdint>
#include <cstdio>
#include <map>
#include <set>

static int g_fail = 0;
static void check(bool ok, const char* what)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

namespace {

struct seed_result
{
    int nations = 0;
    int roadless = 0;
    int roadless_single_centre = 0;
    int roaded_ocean = 0;
};

seed_result census(const world& w)
{
    seed_result r;
    // Centres per nation.
    std::map<entity_id, int> centres;
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        (void)cid;
        const auto it = w.tile_to_nation.find(tile);
        if (it != w.tile_to_nation.end() && it->second != null_entity)
            centres[it->second]++;
    }
    // Any roaded tile per nation; ocean-road violations.
    std::set<entity_id> has_road;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.road_level == 0)
            continue;
        if (is_water(tc.substrate))
            ++r.roaded_ocean;
        const auto it = w.tile_to_nation.find(tid);
        if (it != w.tile_to_nation.end() && it->second != null_entity)
            has_road.insert(it->second);
    }
    for (const auto& [nation, n] : centres)
    {
        ++r.nations;
        if (has_road.count(nation) == 0)
        {
            ++r.roadless;
            if (n < 2)
                ++r.roadless_single_centre;
        }
    }
    return r;
}

} // namespace

int main()
{
    int tot_nations = 0, tot_roadless = 0, tot_single = 0, tot_ocean = 0;

    std::printf("  seed  nations  road-less  (single-centre)\n");
    for (std::uint32_t s = 0; s < 8; ++s)
    {
        world_params wp;
        wp.seed = s * 0x9E3779B1u;
        const world ws = make_hard_coded_world(no_prehistory(wp));
        const seed_result r = census(ws);
        std::printf("  %4u  %7d  %9d  %14d\n", s, r.nations, r.roadless, r.roadless_single_centre);
        tot_nations += r.nations;
        tot_roadless += r.roadless;
        tot_single += r.roadless_single_centre;
        tot_ocean += r.roaded_ocean;
    }
    std::printf("      (C1 road-less: %d of %d nations = %.1f%%; C2 of those, %d are single-centre)\n",
                tot_roadless, tot_nations,
                tot_nations ? 100.0 * tot_roadless / tot_nations : 0.0, tot_single);
    check(tot_nations > 0, "C1 census measured a non-empty nation set");
    check(tot_ocean == 0, "C3 no road_level stamped on an ocean tile");

    // C4 determinism — same seed twice, identical road field and state hash.
    world_params wp0;
    wp0.seed = 0;
    world a = make_hard_coded_world(no_prehistory(wp0));
    world b = make_hard_coded_world(no_prehistory(wp0));
    int mismatches = 0;
    for (const auto& [tid, tc] : a.tiles)
    {
        const auto it = b.tiles.find(tid);
        if (it == b.tiles.end() || it->second.road_level != tc.road_level)
            ++mismatches;
    }
    const std::uint64_t ha = a.state_hash(a.current_day_tick);
    const std::uint64_t hb = b.state_hash(b.current_day_tick);
    std::printf("      (state_hash %016llX vs %016llX)\n",
                static_cast<unsigned long long>(ha), static_cast<unsigned long long>(hb));
    check(mismatches == 0, "C4 road_level field identical across two generations");
    check(ha == hb, "C4 state_hash identical across two generations");

    std::printf("%s (%d failure(s))\n", g_fail ? "ROAD REACH CENSUS FAILED" : "ROAD REACH CENSUS OK", g_fail);
    return g_fail ? 1 : 0;
}
