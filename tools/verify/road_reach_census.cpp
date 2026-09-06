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
#include "world/road_generation.hpp" // C7: stamp_history_roads, called directly
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

    // -----------------------------------------------------------------------
    // C6 — THE ANCIENT NETWORK (BL-768), and the only place in the project that
    // can see it.
    //
    // Every census above declares `no_prehistory()`, which is correct for its
    // own subject and blind to this one: with the Era -1 sim off there are no
    // recorded corridors, so `stamp_history_roads` is a no-op and the road field
    // is exactly the national lattice. The ancient half is therefore invisible
    // to every existing road check — including this file's own C1-C5 — and
    // asserting it needs a world with the era ON.
    //
    // The measurement is a DIFFERENCE, not an absolute: one world at the
    // shipping defaults with the era on, the same seed with it off, and the
    // delta in the tier census. "How many road tiles should an ancient world
    // have" has no answer independent of how much history happened in it, while
    // "does the era leave more road on the ground" has exactly one.
    //
    // BE PRECISE ABOUT WHAT THE DELTA IS, because it is easy to over-read. The
    // era moves populations, sacks cities and redraws borders, so an era-ON
    // world has a DIFFERENT national lattice as well as an ancient one, and the
    // delta below is the era's whole contribution to the road field rather than
    // the stamp's alone. It is the right row for "did anything happen"; C7 is
    // the row that isolates the stamp and its tier rule.
    {
        std::printf("\n--- C6  the ancient network (BL-768), era ON vs OFF at one seed ---\n");
        world_params on0;             // Shipping defaults: epoch 0, 400 prehistory years.
        on0.seed = 0;
        const world w_on  = make_hard_coded_world(on0);
        const world w_off = make_hard_coded_world(no_prehistory(on0));

        auto tiers = [](const world& ww, int out4[4], int& ocean) {
            out4[0] = out4[1] = out4[2] = out4[3] = 0;
            ocean = 0;
            for (const auto& [tid, tc] : ww.tiles)
            {
                if (tc.body != ww.home_body || tc.road_level == 0) continue;
                if (is_water(tc.substrate)) ++ocean;
                const int t = tc.road_level < 4 ? tc.road_level : 3;
                ++out4[0];
                if (t >= 1 && t <= 3) ++out4[t];
            }
        };
        int on4[4], off4[4], on_ocean = 0, off_ocean = 0;
        tiers(w_on, on4, on_ocean);
        tiers(w_off, off4, off_ocean);
        std::printf("      era OFF: roaded %d  (track %d / road %d / highway %d)\n",
                    off4[0], off4[1], off4[2], off4[3]);
        std::printf("      era ON : roaded %d  (track %d / road %d / highway %d)   delta %+d tiles\n",
                    on4[0], on4[1], on4[2], on4[3], on4[0] - off4[0]);

        check(on4[0] > off4[0],
              "C6 the era adds road tiles the national lattice did not lay (the pass is not a no-op)");
        check(on_ocean == 0, "C6 no ancient road is stamped on water");

        // Determinism of the ancient half specifically. C4 above proves it for
        // the era-off field; nothing proved it for the corridor record, whose
        // whole point is that it is produced by a 400-year simulation.
        const world w_on2 = make_hard_coded_world(on0);
        int road_mismatch = 0;
        for (const auto& [tid, tc] : w_on.tiles)
        {
            const auto it = w_on2.tiles.find(tid);
            if (it == w_on2.tiles.end() || it->second.road_level != tc.road_level)
                ++road_mismatch;
        }
        check(road_mismatch == 0,
              "C6 the ancient road field is identical across two era-ON generations");

        // -------------------------------------------------------------------
        // C7 — THE ANCIENT TIER RULE, isolated (BL-768).
        //
        // C6 measures the era's whole effect; this measures the rule. The pass
        // is called DIRECTLY on a copy of the era-OFF world with one synthetic
        // corridor, which is exactly why `history_road_node` is a flattened pair
        // of integers rather than a `settlement_state` — the case is two structs
        // and no settled world.
        //
        // The era-OFF world carries ZERO highway tiles (C6 prints it), and that
        // is what makes the promotion rung unambiguous: any highway tile after a
        // single stamp came from this rule and nothing else.
        //
        //   traffic below the threshold, no works -> Track,   no highway
        //   traffic at    the threshold, no works -> Road,    no highway
        //   traffic at    the threshold, both ends worked -> Highway
        //   works at ONE end only                          -> no promotion
        //
        // The last row is the one worth having: it is the difference between "a
        // work promotes the corridor" and "a single Way Station promotes every
        // line radiating out of one region".
        {
            std::printf("\n--- C7  the ancient tier rule, isolated ---\n");
            // Two population-centre tiles on the home body, far enough apart to
            // give the stamp a real route. Sorted ids, so the pick is stable.
            std::vector<std::pair<entity_id, entity_id>> centres; // (centre, tile)
            for (const auto& [cid, tid] : w_off.population_centre_tile)
            {
                const auto it = w_off.tiles.find(tid);
                if (it != w_off.tiles.end() && it->second.body == w_off.home_body
                    && !is_water(it->second.substrate))
                    centres.push_back({cid, tid});
            }
            std::sort(centres.begin(), centres.end());
            check(centres.size() >= 2, "C7 the census world has centres to route between");

            auto highways = [](const world& ww) {
                int n = 0;
                for (const auto& [tid, tc] : ww.tiles)
                    if (tc.body == ww.home_body && tc.road_level >= 3) ++n;
                return n;
            };
            auto roads_or_better = [](const world& ww) {
                int n = 0;
                for (const auto& [tid, tc] : ww.tiles)
                    if (tc.body == ww.home_body && tc.road_level >= 2) ++n;
                return n;
            };

            if (centres.size() >= 2)
            {
                const auto ta = w_off.tiles.at(centres.front().second);
                const auto tb = w_off.tiles.at(centres.back().second);
                const int base_hw = highways(w_off);
                const int base_rd = roads_or_better(w_off);
                std::printf("      base: highway %d, road-or-better %d\n", base_hw, base_rd);

                auto run = [&](int uses, int reach_a, int reach_b, int& hw, int& rd) {
                    world ww = w_off; // value copy: each case starts from the same field
                    const std::vector<history_road_node> nodes = {
                        { ta.grid_x, ta.grid_y, reach_a },
                        { tb.grid_x, tb.grid_y, reach_b } };
                    const std::vector<history_corridor> cor = {
                        { 0, 1, static_cast<int32_t>(uses) } };
                    stamp_history_roads(ww, ww.home_body, nodes, cor);
                    hw = highways(ww);
                    rd = roads_or_better(ww);
                };

                int hw = 0, rd = 0;
                run(1, 0, 0, hw, rd);
                check(hw == base_hw, "C7a one walk, no works -> Track: no highway appears");
                const int track_rd = rd;

                run(4, 0, 0, hw, rd);
                check(hw == base_hw, "C7b four walks, no works -> Road: still no highway");
                check(rd > track_rd, "C7b four walks lay road-tier tiles a single walk did not");

                run(4, 200, 0, hw, rd);
                check(hw == base_hw,
                      "C7c a work at ONE end does not promote (the corridor is only as good as its worse end)");

                run(4, 200, 200, hw, rd);
                check(hw > base_hw,
                      "C7d four walks with BOTH ends worked reach the Highway rung — the works payoff");

                run(1, 200, 200, hw, rd);
                check(hw == base_hw,
                      "C7e works alone do not reach Highway: promotion is one rung, off the traffic tier");
            }
        }
    }

    std::printf("%s (%d failure(s))\n", g_fail ? "ROAD REACH CENSUS FAILED" : "ROAD REACH CENSUS OK", g_fail);
    return g_fail ? 1 : 0;
}
