// Headless harness for BL-146 (road-network generation). No SDL / Lua / ImGui.
// Builds the hard-coded world and asserts:
//   R1 presence   — road generation stamps road_level > 0 on some Kepler land
//                   tiles (the lattice exists), and never on ocean tiles.
//   R2 three-tier — all three tiers appear — Track (1), Road (2), Highway (3) —
//                   and no tile carries a tier beyond Highway (generation ceiling).
//   R3 connectivity — every population centre with >=1 same-nation peer sits on,
//                   or orthogonally adjacent to, a roaded tile (its lattice reached
//                   it). Uses the same 4-cardinal + column-wrap topology as gen.
//   R4 determinism — a second generation yields an identical road_level field.
// Exit non-zero on any failure.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <map>
#include <vector>

static int g_fail = 0;
static void check(bool ok, const char* what)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
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
            if (tc.composition == terrain_composition::ocean) ++roaded_ocean;
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

    // R2 three-tier ladder (BL-172): Track (1) / Road (2) / Highway (3).
    std::printf("      (tiers: track=%d road=%d highway=%d over=%d)\n",
                track_tiles, road_tiles, highway_tiles, over_highway);
    check(track_tiles > 0, "R2 track roads present (road_level 1)");
    check(road_tiles > 0, "R2 road-tier roads present (road_level 2)");

    // The highway tier needs two City+ centres to land ADJACENT in the road graph
    // (edge_tier requires both endpoints at scale >= 3). Population scale is drawn
    // from a fixed weighted distribution — ~30% are City+ — so whether any such
    // PAIR ends up adjacent is spatial luck, not a property of the generator.
    //
    // Asserting it on one world therefore makes this check seed-fragile: it broke
    // when BL-167 began rolling the homeworld's ocean fraction, which reshuffled
    // population placement without changing anything about roads. Assert instead
    // that the tier is REACHABLE, by scanning a handful of seeds. If no seed
    // produces one, that is a real regression and this still fails.
    {
        int seeds_with_highway = 0, first = -1;
        for (uint32_t s = 0; s < 8; ++s)
        {
            world_params wp;
            wp.seed = s * 0x9E3779B1u;
            const world ws = make_hard_coded_world(no_prehistory(wp));
            for (const auto& [tid, tc] : ws.tiles)
                if (tc.road_level == 3)
                {
                    ++seeds_with_highway;
                    if (first < 0) first = static_cast<int>(s);
                    break;
                }
        }
        std::printf("      (highway tier present on %d of 8 seeds; first at seed index %d)\n",
                    seeds_with_highway, first);
        check(seeds_with_highway > 0, "R2 highway tier (road_level 3) is reachable");
    }
    check(over_highway == 0, "R2 no tile exceeds the highway tier (road_level <= 3)");

    // R3 connectivity — a centre with a same-nation peer must touch a road tile
    // (itself or a 4-cardinal neighbour, column-wrapped).
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
    // Count same-nation peers per nation among centres.
    std::map<entity_id, int> centres_per_nation;
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end() || tit->second.body != kepler) continue;
        centres_per_nation[nation_of(tile)]++;
    }
    int checked = 0, connected = 0;
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end() || tit->second.body != kepler) continue;
        if (centres_per_nation[nation_of(tile)] < 2) continue; // isolated centre: no edge owed
        ++checked;
        const int r = tit->second.grid_y, c = tit->second.grid_x;
        const bool touches = road_at(r, c) || road_at(r, (c + 1) % gw) ||
                             road_at(r, (c + gw - 1) % gw) || road_at(r - 1, c) || road_at(r + 1, c);
        if (touches) ++connected;
    }
    std::printf("      (connectivity: %d/%d non-isolated centres touch a road)\n", connected, checked);
    check(checked > 0 && connected == checked,
          "R3 every non-isolated population centre touches its road lattice");

    // R4 determinism — regenerate and compare the whole road_level field by tile.
    world w2 = make_hard_coded_world(no_prehistory());
    int mismatches = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        const auto it = w2.tiles.find(tid);
        if (it == w2.tiles.end() || it->second.road_level != tc.road_level) ++mismatches;
    }
    check(mismatches == 0, "R4 road_level field identical across two generations");

    std::printf("%s (%d failure(s))\n", g_fail ? "ROAD GEN AUDIT FAILED" : "ROAD GEN AUDIT OK", g_fail);
    return g_fail ? 1 : 0;
}
