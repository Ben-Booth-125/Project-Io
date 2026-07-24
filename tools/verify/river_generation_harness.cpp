// Headless harness for BL-170 (river/freshwater generation). No SDL / Lua / ImGui.
// Builds the hard-coded world and asserts:
//   V1 presence      — river generation stamps river_edges != 0 on some Kepler
//                      land tile (the network exists).
//   V2 edge symmetry — every stamped edge is set on BOTH bordering tiles (a
//                      river never has a one-sided bit).
//   V3 land feature  — a river edge is never stamped between two ocean tiles
//                      (rivers may end at a coast — a mouth edge legitimately
//                      borders one land + one ocean tile — but never run
//                      ocean-to-ocean through open sea).
//   V4 logistics     — a river-adjacent land tile's traversal cost is strictly
//                      cheaper than the same landform/road tier without a river
//                      (river_traversal_multiplier actually discounts A*).
//   V5 determinism   — a second generation yields an identical river_edges field.
// Exit non-zero on any failure.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
static void check(bool ok, const char* what)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

// Mirrors river_generation.cpp's cube-direction hex_neighbours (reciprocal by
// construction: direction i's opposite is always (i+3)%6) so the harness can
// re-derive edges independently of the generator's internals. NOT
// tile_generation.cpp's odd-r offset table, which is not reciprocal.
static void hex_neighbours(int col, int row, int gw, int gh,
                           std::pair<int, int> out[6], int& count)
{
    static constexpr int dx[6] = {  1,  1,  0, -1, -1,  0 };
    static constexpr int dz[6] = {  0, -1, -1,  0,  1,  1 };
    const int x = col - (row - (row & 1)) / 2;
    const int z = row;
    count = 0;
    for (int i = 0; i < 6; ++i)
    {
        const int nx = x + dx[i];
        const int nz = z + dz[i];
        if (nz < 0 || nz >= gh) { out[i] = { -1, -1 }; continue; }
        const int ncol = nx + (nz - (nz & 1)) / 2;
        out[i] = { ((ncol % gw) + gw) % gw, nz };
        ++count;
    }
}

int main()
{
    world w = make_hard_coded_world();
    const entity_id kepler = w.home_body;
    const auto bit = w.bodies.find(kepler);
    if (bit == w.bodies.end()) { std::printf("[FAIL] no home body\n"); return 1; }
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;

    std::vector<entity_id> grid(static_cast<std::size_t>(gw) * gh, null_entity);
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == kepler && tc.grid_x >= 0 && tc.grid_x < gw && tc.grid_y >= 0 && tc.grid_y < gh)
            grid[static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x] = tid;

    // V1 presence.
    int river_tiles = 0;
    entity_id sample_river_tile = null_entity;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != kepler || tc.river_edges == 0) continue;
        ++river_tiles;
        if (tc.composition != terrain_composition::ocean && sample_river_tile == null_entity)
            sample_river_tile = tid;
    }
    check(river_tiles > 0, "V1 river network exists (some Kepler tile has river_edges != 0)");

    // V3 land feature: no river edge borders two ocean tiles.
    bool ocean_to_ocean = false;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != kepler || tc.river_edges == 0 || tc.composition != terrain_composition::ocean)
            continue;
        std::pair<int, int> nb[6];
        int cnt = 0;
        hex_neighbours(tc.grid_x, tc.grid_y, gw, gh, nb, cnt);
        for (int i = 0; i < 6; ++i)
        {
            if (!(tc.river_edges & (1u << i)) || nb[i].first == -1) continue;
            const entity_id nid = grid[static_cast<std::size_t>(nb[i].second) * gw + nb[i].first];
            const auto nit = w.tiles.find(nid);
            if (nit != w.tiles.end() && nit->second.composition == terrain_composition::ocean)
                ocean_to_ocean = true;
        }
    }
    check(!ocean_to_ocean, "V3 no river edge borders two ocean tiles (rivers end at the coast, don't cross open sea)");

    // V2 edge symmetry: for every tile with a river bit set toward direction i,
    // the neighbour in that direction must have the reciprocal opposite bit set
    // back toward this tile (guaranteed by the generator's cube-direction
    // construction; re-derived independently here rather than assumed).
    bool symmetric = true;
    int edges_checked = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != kepler || tc.river_edges == 0) continue;
        std::pair<int, int> nb[6];
        int cnt = 0;
        hex_neighbours(tc.grid_x, tc.grid_y, gw, gh, nb, cnt);
        for (int i = 0; i < 6; ++i)
        {
            if (!(tc.river_edges & (1u << i))) continue;
            if (nb[i].first == -1) { symmetric = false; continue; } // bit set toward an off-grid direction
            ++edges_checked;
            const entity_id nid = grid[static_cast<std::size_t>(nb[i].second) * gw + nb[i].first];
            const auto nit = w.tiles.find(nid);
            const int opp = (i + 3) % 6;
            if (nit == w.tiles.end() || !(nit->second.river_edges & (1u << opp)))
                symmetric = false;
        }
    }
    check(edges_checked > 0 && symmetric, "V2 every river edge is set on both bordering tiles");

    // V4 logistics discount: build two identical tile_component values (plains,
    // no road) differing only in river_edges, and assert the river-adjacent one
    // is strictly cheaper via the shared road/river multiplier stack.
    {
        tile_component plain{};
        plain.composition = terrain_composition::grassland;
        plain.landform     = terrain_landform::plains;
        plain.road_level   = 0;
        plain.river_edges  = 0;

        tile_component riverside = plain;
        riverside.river_edges = 0b1; // one edge set

        const float base_cost = landform_logistics_cost(plain.landform);
        const float plain_cost     = base_cost * road_traversal_multiplier(plain.road_level)
                                                * river_traversal_multiplier(plain.river_edges);
        const float riverside_cost = base_cost * road_traversal_multiplier(riverside.road_level)
                                                * river_traversal_multiplier(riverside.river_edges);
        check(riverside_cost < plain_cost, "V4 river-adjacent tile traversal cost is strictly cheaper");

        // And it stacks multiplicatively with a road tier rather than replacing it.
        tile_component roaded_riverside = riverside;
        roaded_riverside.road_level = 2; // Road tier
        const float roaded_cost = base_cost * road_traversal_multiplier(roaded_riverside.road_level);
        const float roaded_river_cost = base_cost * road_traversal_multiplier(roaded_riverside.road_level)
                                                    * river_traversal_multiplier(roaded_riverside.river_edges);
        check(roaded_river_cost < roaded_cost,
              "V4b river discount stacks multiplicatively with an existing road tier");
    }

    // V5 determinism: regenerate the whole world and compare river_edges bit-for-bit.
    {
        world w2 = make_hard_coded_world();
        const entity_id kepler2 = w2.home_body;
        bool match = true;
        int compared = 0;
        for (const auto& [tid, tc] : w.tiles)
        {
            if (tc.body != kepler) continue;
            const auto it2 = w2.tiles.find(tid);
            if (it2 == w2.tiles.end() || it2->second.body != kepler2 || it2->second.river_edges != tc.river_edges)
            {
                match = false;
                break;
            }
            ++compared;
        }
        check(match && compared > 0, "V5 river generation is deterministic (identical river_edges on regeneration)");
    }

    std::printf("----------------------------------------------------------------\n");
    std::printf("Result: %s (exit %d)\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
