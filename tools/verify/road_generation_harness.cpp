// Headless harness for BL-146 (road-network generation). No SDL / Lua / ImGui.
// Builds the hard-coded world and asserts:
//   R1 presence   — road generation stamps road_level > 0 on some Kepler land
//                   tiles (the lattice exists), and never on ocean tiles.
//   R2 three-tier — the tier ladder obeys BL-618 (roads scale with qualification):
//                   the no_prehistory world's nations all sit at the never-
//                   industrialised qualification floor (0.05), below both tier
//                   gates, so the PRIMARY world is an all-Track lattice; the full
//                   ladder — Road, and Highway across a seed sweep — is asserted
//                   on a QUALIFIED regeneration (every nation's qualification
//                   overridden high, road_level reset, generate_roads re-run).
//                   No tile ever carries a tier beyond Highway (ceiling).
//   Q  differential — the same world regenerated at floor vs high qualification
//                   produces measurably different lattices (BL-618's contract):
//                   promoted tiers appear only on the qualified run, and the
//                   qualified lattice is at least as large (redundancy loops).
//   R3 connectivity — every non-anchor population centre with >=1 same-nation peer
//                   sits on, or orthogonally adjacent to, a roaded tile (its lattice
//                   reached it); province-anchor foundings (BL-611) are exempt, and
//                   R3b asserts they are the ONLY centres off the lattice. Uses the
//                   same 4-cardinal + column-wrap topology as gen.
//   R4 determinism — a second generation yields an identical road_level field.
// Exit non-zero on any failure.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/road_generation.hpp"
#include "harness_params.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

static int g_fail = 0;
static void check(bool ok, const char* what)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

/// Re-run road generation on @p w's body with every nation's qualification forced to
/// @p qual: reset the body's road_level field, drop the A*/reach caches (they embed the
/// road discount), and call generate_roads again. Under BL-621's era-relative gates a
/// UNIFORM value grades every nation at percentile 0.5 whatever the value is — the
/// antiquity shape. The differential instrument is the SPREAD variant below. Formerly —
/// all else fixed, only the qualification input moves.
static void regen_roads_at_qualification(world& w, entity_id body, float qual)
{
    for (auto& [nid, nc] : w.nations)
        nc.qualification = qual;
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body)
            tc.road_level = 0;
    w.astar_cost_cache.clear();
    w.body_reach_cost.clear();
    generate_roads(w, body);
}

/// The BL-621 differential instrument: a three-band qualification SPREAD assigned by
/// ascending nation id — bottom third 0.05 (percentile ~0.17, below the Road gate),
/// middle 0.30 (~0.5, Roads), top 0.60 (~0.83, Highways where two majors meet). All
/// else fixed; deterministic by the id sort.
static void regen_roads_with_spread(world& w, entity_id body)
{
    std::vector<entity_id> nids;
    for (const auto& [nid, nc] : w.nations)
        nids.push_back(nid);
    std::sort(nids.begin(), nids.end());
    const int n = static_cast<int>(nids.size());
    for (int i = 0; i < n; ++i)
        w.nations[nids[static_cast<std::size_t>(i)]].qualification =
            (i < n / 3) ? 0.05f : (i < 2 * n / 3) ? 0.30f : 0.60f;
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body)
            tc.road_level = 0;
    w.astar_cost_cache.clear();
    w.body_reach_cost.clear();
    generate_roads(w, body);
}

/// Tier census of @p body's road_level field: counts[1..3], plus total in counts[0].
static void tier_census(const world& w, entity_id body, int counts[4])
{
    counts[0] = counts[1] = counts[2] = counts[3] = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.road_level == 0 || tc.road_level > 3) continue;
        ++counts[0];
        ++counts[tc.road_level];
    }
}

int main()
{
    world w = make_hard_coded_world(no_prehistory());
    const entity_id kepler = w.home_body;
    const auto bit = w.bodies.find(kepler);
    if (bit == w.bodies.end()) { std::printf("[FAIL] no home body\n"); return 1; }
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;

    // Raster index of Kepler tiles (grid_y*gw + grid_x -> tile), for adjacency.
    std::vector<entity_id> grid(static_cast<std::size_t>(gw) * gh, null_entity);
    int roaded_land = 0, roaded_ocean = 0;
    int track_tiles = 0, road_tiles = 0, highway_tiles = 0, over_highway = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != kepler) continue;
        if (tc.grid_x >= 0 && tc.grid_x < gw && tc.grid_y >= 0 && tc.grid_y < gh)
            grid[static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x] = tid;
        if (tc.road_level > 0)
        {
            if (is_water(tc.substrate)) ++roaded_ocean;
            else                                              ++roaded_land;
            if      (tc.road_level == 1) ++track_tiles;
            else if (tc.road_level == 2) ++road_tiles;
            else if (tc.road_level == 3) ++highway_tiles;
            else                         ++over_highway;
        }
    }

    // R1 presence.
    check(roaded_land > 0, "R1 road lattice exists (some Kepler land tile has road_level > 0)");
    check(roaded_ocean == 0, "R1 no road_level stamped on ocean tiles");

    // R2 three-tier ladder (BL-172), gated by qualification PERCENTILE (BL-618 as
    // amended by BL-621 — Ben's 2026-08-25 ruling on NR-641). The no_prehistory
    // world's nations all tie at the seeding floor, which under era-relative gates
    // grades every one at percentile 0.5: the primary world carries a Roads
    // backbone and no Highways — the antiquity shape, and that IS the assertion.
    std::printf("      (tiers, all-tied world: track=%d road=%d highway=%d over=%d)\n",
                track_tiles, road_tiles, highway_tiles, over_highway);
    check(track_tiles > 0, "R2 track roads present (road_level 1)");
    check(road_tiles > 0 && highway_tiles == 0,
          "R2 an all-tied world keeps Roads and promotes no Highway (BL-621)");

    // The full ladder needs qualified nations. The highway tier further needs two
    // City+ centres ADJACENT in the backbone graph (both endpoints scale >= 3;
    // since BL-620 the backbone spans towns-and-up only). Centre scales are carved
    // from the Era -1 demography (BL-610) and City+ centres are rare, so whether
    // any such PAIR shares a nation is spatial luck, not a property of the
    // generator — assert the tier is REACHABLE across a seed sweep, each world
    // regenerated at high qualification. If no seed produces one, that is a real
    // regression and this still fails.
    {
        int seeds_with_highway = 0, seeds_with_road = 0, first = -1;
        for (uint32_t s = 0; s < 8; ++s)
        {
            world_params wp;
            wp.seed = s * 0x9E3779B1u;
            world ws = make_hard_coded_world(no_prehistory(wp));
            regen_roads_with_spread(ws, ws.home_body);
            int tc4[4];
            tier_census(ws, ws.home_body, tc4);
            if (tc4[2] > 0) ++seeds_with_road;
            if (tc4[3] > 0)
            {
                ++seeds_with_highway;
                if (first < 0) first = static_cast<int>(s);
            }
        }
        std::printf("      (qualified regen: road tier on %d of 8 seeds; highway on %d of 8, first at seed index %d)\n",
                    seeds_with_road, seeds_with_highway, first);
        check(seeds_with_road == 8, "R2 road tier (road_level 2) appears on every qualified seed");
        check(seeds_with_highway > 0, "R2 highway tier (road_level 3) is reachable when qualified");
    }
    check(over_highway == 0, "R2 no tile exceeds the highway tier (road_level <= 3)");

    // R3 connectivity — a centre with a same-nation peer must touch a road tile
    // (itself or a 4-cardinal neighbour, column-wrapped).
    //
    // PROVINCE ANCHORS ARE EXEMPT (contract change with BL-610/BL-611, asserted since
    // BL-620): anchor foundings land AFTER generate_roads by structural necessity —
    // they need the province partition, and the partition reads the finished road
    // raster (roads bind provinces; a post-partition road stamp would break the
    // partition-recompute contract, province_partition_harness P6). So an anchor
    // carries no street and owes no lattice contact; R3b instead asserts anchors are
    // the ONLY centres off the lattice. Whether anchors should eventually join it
    // (via a partition rebuild) is an open design call.
    auto nation_of = [&](entity_id t) -> entity_id {
        const auto it = w.tile_to_nation.find(t);
        return (it != w.tile_to_nation.end()) ? it->second : null_entity;
    };
    auto road_at = [&](int r, int c) -> bool {
        if (r < 0 || r >= gh) return false;
        const entity_id t = grid[static_cast<std::size_t>(r) * gw + c];
        if (t == null_entity) return false;
        const auto it = w.tiles.find(t);
        return it != w.tiles.end() && it->second.road_level > 0;
    };
    auto is_anchor = [&](entity_id cid) -> bool {
        const auto it = w.population_centres.find(cid);
        return it != w.population_centres.end() && it->second.province_anchor;
    };
    // Count same-nation peers per nation among centres.
    std::map<entity_id, int> centres_per_nation;
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end() || tit->second.body != kepler) continue;
        centres_per_nation[nation_of(tile)]++;
    }
    int checked = 0, connected = 0, anchors = 0, off_lattice_non_anchor = 0;
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end() || tit->second.body != kepler) continue;
        const int r = tit->second.grid_y, c = tit->second.grid_x;
        const bool touches = road_at(r, c) || road_at(r, (c + 1) % gw) ||
                             road_at(r, (c + gw - 1) % gw) || road_at(r - 1, c) || road_at(r + 1, c);
        if (is_anchor(cid)) { ++anchors; continue; }
        if (!touches) ++off_lattice_non_anchor;
        if (centres_per_nation[nation_of(tile)] < 2) continue; // isolated centre: no edge owed
        ++checked;
        if (touches) ++connected;
    }
    std::printf("      (connectivity: %d/%d non-isolated non-anchor centres touch a road; %d anchors exempt)\n",
                connected, checked, anchors);
    check(checked > 0 && connected == checked,
          "R3 every non-isolated, non-anchor population centre touches its road lattice");
    check(off_lattice_non_anchor == 0,
          "R3b province anchors are the only centres off the lattice");

    // R4 determinism — regenerate and compare the whole road_level field by tile.
    world w2 = make_hard_coded_world(no_prehistory());
    int mismatches = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        const auto it = w2.tiles.find(tid);
        if (it == w2.tiles.end() || it->second.road_level != tc.road_level) ++mismatches;
    }
    check(mismatches == 0, "R4 road_level field identical across two generations");

    // Q — the BL-621 era-relative contract (Ben, 2026-08-25, ruling on NR-641): gates
    // read percentile standing, not absolute qualification. (Mutates w and w2; keep last.)
    {
        regen_roads_at_qualification(w, kepler, 0.05f); // uniform: everyone percentile 0.5
        int uni[4];
        tier_census(w, kepler, uni);
        std::printf("      (uniform floor: total=%d t/r/h=%d/%d/%d)\n",
                    uni[0], uni[1], uni[2], uni[3]);
        check(uni[3] == 0, "Q1a an all-tied world promotes no Highway (percentile 0.5 < 0.8)");
        check(uni[2] > 0,  "Q1b an all-tied world keeps a Roads backbone (0.5 >= 0.4) - the antiquity shape");

        regen_roads_at_qualification(w2, kepler, 0.60f); // uniform at a DIFFERENT absolute level
        int uni2[4];
        tier_census(w2, kepler, uni2);
        check(uni[0] == uni2[0] && uni[1] == uni2[1] && uni[2] == uni2[2] && uni[3] == uni2[3],
              "Q2 absolute level is irrelevant: uniform 0.05 and uniform 0.60 lattices are identical");

        regen_roads_with_spread(w2, kepler);
        int spr[4];
        tier_census(w2, kepler, spr);
        std::printf("      (spread: total=%d t/r/h=%d/%d/%d)\n",
                    spr[0], spr[1], spr[2], spr[3]);
        check(spr[1] != uni[1] || spr[2] != uni[2] || spr[3] != uni[3],
              "Q3 relative standing is what matters: a spread world's tier census differs from uniform");

        // Determinism of the instrument itself: the uniform regen equals a second uniform regen.
        regen_roads_at_qualification(w2, kepler, 0.05f);
        int uni3[4];
        tier_census(w2, kepler, uni3);
        check(uni[0] == uni3[0] && uni[1] == uni3[1] && uni[2] == uni3[2] && uni[3] == uni3[3],
              "Q4 the regeneration instrument is deterministic (two uniform regens agree)");
    }

    std::printf("%s (%d failure(s))\n", g_fail ? "ROAD GEN AUDIT FAILED" : "ROAD GEN AUDIT OK", g_fail);
    return g_fail ? 1 : 0;
}
