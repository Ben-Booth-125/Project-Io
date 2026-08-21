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
//   C5 stranded tracks     — (added 2026-08-21) how many nations hold a roaded tile
//                            that has NO roaded neighbour anywhere, and are therefore
//                            still off the lattice. C1 going to zero says every nation
//                            has A road; C5 is what says whether that road CONNECTS.
//                            Kept as a separate row deliberately: cut 1 satisfied C1 by
//                            stamping a Track on each centre's own tile, so C1 alone
//                            can no longer distinguish "reached" from "marked".
// Report-first by design: C1/C2/C5 print numbers, C3/C4 assert.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <tuple>
#include <vector>

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
    // C5 (added 2026-08-21) — the OTHER half of cut 1's question. Cut 1 gave every
    // centre's own tile a Track, which took the road-less count to zero. But "has a
    // roaded tile" and "is ON the lattice" are different claims: a lone Track with no
    // roaded neighbour is a nation the network still does not reach. C5 counts those,
    // so the zero above cannot be read as more than it says.
    int single_centre_nations = 0;
    int stranded_tracks       = 0; ///< nations whose ONLY roaded tiles have no roaded neighbour
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
    // C5 — is a roaded tile CONNECTED? Index every roaded LAND tile by (body, gx, gy)
    // so a nation's tiles can be asked whether any roaded 4-neighbour exists. The
    // neighbour set is global, not per-nation: a border link joining two nations'
    // lattices is exactly the case that must count as connected. Columns wrap
    // east-west, matching the logistics topology road_generation itself scans on.
    std::set<std::tuple<entity_id, int, int>> roaded;
    for (const auto& [tid, tc] : w.tiles)
    {
        (void)tid;
        if (tc.road_level > 0 && !is_water(tc.substrate))
            roaded.insert({ tc.body, tc.grid_x, tc.grid_y });
    }
    std::map<entity_id, std::vector<std::tuple<entity_id, int, int>>> nation_roads;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.road_level == 0 || is_water(tc.substrate))
            continue;
        const auto it = w.tile_to_nation.find(tid);
        if (it != w.tile_to_nation.end() && it->second != null_entity)
            nation_roads[it->second].push_back({ tc.body, tc.grid_x, tc.grid_y });
    }
    auto connected = [&](const std::tuple<entity_id, int, int>& cell) {
        const entity_id body = std::get<0>(cell);
        const int       gx   = std::get<1>(cell);
        const int       gy   = std::get<2>(cell);
        const auto      bit  = w.bodies.find(body);
        const int       gw   = (bit != w.bodies.end()) ? std::max(1, bit->second.grid_width) : 1;
        const int dx[4] = { 1, -1, 0, 0 };
        const int dy[4] = { 0, 0, 1, -1 };
        for (int k = 0; k < 4; ++k)
        {
            const int nx = ((gx + dx[k]) % gw + gw) % gw; // columns wrap
            const int ny = gy + dy[k];
            if (roaded.count({ body, nx, ny }))
                return true;
        }
        return false;
    };

    for (const auto& [nation, n] : centres)
    {
        ++r.nations;
        if (n < 2)
            ++r.single_centre_nations;
        if (has_road.count(nation) == 0)
        {
            ++r.roadless;
            if (n < 2)
                ++r.roadless_single_centre;
            continue;
        }
        const auto nr = nation_roads.find(nation);
        if (nr == nation_roads.end())
            continue;
        bool any_connected = false;
        for (const auto& cell : nr->second)
            if (connected(cell))
            {
                any_connected = true;
                break;
            }
        if (!any_connected)
            ++r.stranded_tracks;
    }
    return r;
}

} // namespace

int main()
{
    int tot_nations = 0, tot_roadless = 0, tot_single = 0, tot_ocean = 0;
    int tot_one_centre = 0, tot_stranded = 0;

    std::printf("  seed  nations  road-less  (single-centre)  1-centre  stranded\n");
    for (std::uint32_t s = 0; s < 8; ++s)
    {
        world_params wp;
        wp.seed = s * 0x9E3779B1u;
        const world ws = make_hard_coded_world(no_prehistory(wp));
        const seed_result r = census(ws);
        std::printf("  %4u  %7d  %9d  %14d  %8d  %8d\n", s, r.nations, r.roadless,
                    r.roadless_single_centre, r.single_centre_nations, r.stranded_tracks);
        tot_nations += r.nations;
        tot_roadless += r.roadless;
        tot_single += r.roadless_single_centre;
        tot_ocean += r.roaded_ocean;
        tot_one_centre += r.single_centre_nations;
        tot_stranded += r.stranded_tracks;
    }
    std::printf("      (C1 road-less: %d of %d nations = %.1f%%; C2 of those, %d are single-centre)\n",
                tot_roadless, tot_nations,
                tot_nations ? 100.0 * tot_roadless / tot_nations : 0.0, tot_single);
    std::printf("      (C5 single-centre nations: %d of %d = %.1f%%; STRANDED — roaded but with no\n"
                "           roaded neighbour anywhere, so still off the lattice: %d = %.1f%% of nations)\n",
                tot_one_centre, tot_nations,
                tot_nations ? 100.0 * tot_one_centre / tot_nations : 0.0, tot_stranded,
                tot_nations ? 100.0 * tot_stranded / tot_nations : 0.0);
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
