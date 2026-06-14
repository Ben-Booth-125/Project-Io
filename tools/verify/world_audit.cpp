// Throwaway headless harness: builds the hard-coded world and reports
//   (S2) Kepler's forest + wetland tile fraction (biome-balance target ~>=3%);
//   (S1) whether every placed extraction asset sits on a tile carrying a
//        non-zero prototype-extractable deposit (iron ore / petroleum / water /
//        agricultural produce) on valid (non-ocean) terrain.
// No SDL / Lua / ImGui. Kept outside src/ so the CMake glob ignores it.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <map>

static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    world w = make_hard_coded_world();

    // --- S2: Kepler composition histogram ---
    const entity_id kepler = w.home_body;
    std::map<int, int> hist;
    int land = 0, total = 0;
    int forest = 0, wetland = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != kepler)
            continue;
        ++total;
        ++hist[static_cast<int>(tc.composition)];
        if (tc.composition != terrain_composition::ocean)
            ++land;
        if (tc.composition == terrain_composition::forest)  ++forest;
        if (tc.composition == terrain_composition::wetland) ++wetland;
    }

    std::printf("Kepler tiles: %d total, %d land\n", total, land);
    const float fw_frac = total ? 100.0f * static_cast<float>(forest + wetland) / total : 0.0f;
    std::printf("  forest=%d (%.2f%%)  wetland=%d (%.2f%%)  forest+wetland=%.2f%% of all tiles\n",
                forest, total ? 100.0f * forest / total : 0.0f,
                wetland, total ? 100.0f * wetland / total : 0.0f, fw_frac);
    std::printf("  S2 target forest+wetland >= 3%% of all tiles: %s\n",
                fw_frac >= 3.0f ? "PASS" : "FAIL");

    // --- S1: extraction asset placement audit ---
    const resource_type extractable[] = {
        resource_type::iron_ore, resource_type::petroleum,
        resource_type::water, resource_type::agricultural_produce };

    int extraction_assets = 0, bad = 0;
    for (const auto& [cid, corp] : w.corporations)
    {
        for (entity_id bid : corp.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end() || bit->second.type != building_type::extraction_site)
                continue;
            ++extraction_assets;
            const auto tit = w.tiles.find(bit->second.tile);
            if (tit == w.tiles.end())
            {
                ++bad; std::printf("  BAD: extraction asset on missing tile\n"); continue;
            }
            const tile_component& tc = tit->second;
            bool ocean = (tc.composition == terrain_composition::ocean);
            float dep = 0.0f;
            for (resource_type r : extractable) dep += tc.resource_deposit[ri(r)];
            const float tgt = tc.resource_deposit[ri(bit->second.target_resource)];
            if (ocean || dep <= 0.0f || tgt <= 0.0f)
            {
                ++bad;
                std::printf("  BAD: extraction asset corp=%u ocean=%d extractable_dep=%.3f target_dep=%.3f\n",
                            static_cast<unsigned>(cid), ocean ? 1 : 0, dep, tgt);
            }
        }
    }
    std::printf("Extraction assets: %d, invalid placements: %d\n", extraction_assets, bad);
    std::printf("  S1 R2 all extraction assets on a valid extractable deposit: %s\n",
                bad == 0 ? "PASS" : "FAIL");

    return (fw_frac >= 3.0f && bad == 0) ? 0 : 1;
}
