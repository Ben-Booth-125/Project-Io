// ---------------------------------------------------------------------------
// province_capacity_probe — is the per-tile building cap actually BINDING?
// ---------------------------------------------------------------------------
//
// WHY THIS EXISTS. Ben, 2026-08-20: decouple play from tiles, and use "a derived
// building limit on a province, allowing us to pack more productivity into a
// smaller space, and hopefully that will give markets the required goods needed
// at the start."
//
// The hoped-for mechanism has a precondition nobody has measured: relaxing a cap
// only adds buildings if something is currently being REFUSED by that cap. The
// substrate census puts the shipped world at 0.4-1.4% built, which is the shape
// of a world with far more room than tenants — and if that is what is happening,
// a province-pooled budget changes nothing at all, and the thin market at tick
// zero has a different cause entirely.
//
// So this probe REPORTS, and gates nothing. It answers three questions:
//
//   Q1  How many land tiles sit AT their non-extraction stack cap? (A tile at cap
//       is a tile where the next placement is refused — the only place a pooled
//       province budget could possibly help.)
//   Q2  What does the province-pooled budget look like against actual use — sum
//       of per-tile caps across a province's tiles, versus buildings standing in
//       it? This is the number the proposed limit would replace.
//   Q3  Where is the built mass? Buildings per province, and the fullest province
//       in the world, so "pack more into a smaller space" has a baseline.
//
// It measures the world at the point the PLAYER first sees it — after
// generate_background_firms — for the same reason substrate_census does: a
// measurement taken before the world's real industry exists is simply wrong.
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/hard_coded_world.hpp"
#include "world/placement_rules.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <unordered_map>
#include <vector>

int main(int argc, char** argv)
{
    int seed_count = 3;
    if (argc > 1)
    {
        const int n = std::atoi(argv[1]);
        if (n > 0) seed_count = n;
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    lua.load("scripts/works.lua");

    recipe_registry reg;
    reg.load_from_lua(lua);
    world_gen_config gen_cfg;
    gen_cfg.load_from_lua(lua);
    works_registry works;
    works.load_from_lua(lua);

    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded - run from the repo root.\n");
        return 2;
    }

    std::printf("=== province_capacity_probe - %d seed%s, post-background-firms ===\n",
                seed_count, seed_count == 1 ? "" : "s");
    std::printf("REPORTS ONLY. Nothing here gates; the question is whether the cap binds.\n\n");
    std::printf("seed | land  provinces | bldgs | tiles@cap  %%land | prov used/cap  util%% | fullest prov\n");

    long long tot_at_cap = 0, tot_land = 0, tot_bldg = 0, tot_cap = 0;

    for (int i = 0; i < seed_count; ++i)
    {
        world_params p;
        p.seed = static_cast<uint32_t>(i);
        world w = make_hard_coded_world(p, nullptr, gen_cfg, nullptr, &works);
        assign_default_recipes(w, reg);
        generate_background_firms(w, reg, p.seed ^ 0x8A21F00Du);
        assign_default_recipes(w, reg);

        // Buildings standing on each tile, split the way the cap splits them:
        // extraction sites are bounded by deposit richness, everything else by
        // the composition's non_extraction_stack_cap.
        std::unordered_map<entity_id, int> non_extraction_on_tile;
        for (const auto& [bid, b] : w.buildings)
            if (b.type != building_type::extraction_site)
                ++non_extraction_on_tile[b.tile];

        int land = 0, at_cap = 0;
        long long cap_total = 0;
        for (const auto& [tid, tc] : w.tiles)
        {
            if (tc.composition == terrain_composition::ocean)
                continue;
            ++land;
            const int cap = placement_rules::non_extraction_stack_cap(tc.composition);
            cap_total += cap;
            const auto it = non_extraction_on_tile.find(tid);
            const int used = (it == non_extraction_on_tile.end()) ? 0 : it->second;
            if (used >= cap)
                ++at_cap;
        }

        // Province-pooled view: what the proposed limit would be measured against.
        std::map<uint32_t, int> prov_used;
        std::map<uint32_t, int> prov_cap;
        for (const province& pr : w.provinces.provinces)
        {
            int c = 0;
            for (const entity_id t : pr.tiles)
            {
                const auto tit = w.tiles.find(t);
                if (tit != w.tiles.end())
                    c += placement_rules::non_extraction_stack_cap(tit->second.composition);
            }
            prov_cap[pr.id] = c;
            prov_used[pr.id] = 0;
        }
        for (const auto& [bid, b] : w.buildings)
        {
            const uint32_t pid = w.provinces.province_of(b.tile);
            const auto it = prov_used.find(pid);
            if (it != prov_used.end())
                ++it->second;
        }

        long long used_sum = 0, cap_sum = 0;
        int fullest_used = 0, fullest_cap = 0;
        uint32_t fullest_id = 0;
        for (const auto& [pid, used] : prov_used)
        {
            used_sum += used;
            cap_sum  += prov_cap[pid];
            if (used > fullest_used)
            {
                fullest_used = used;
                fullest_cap  = prov_cap[pid];
                fullest_id   = pid;
            }
        }

        // Q4 (added on the spot, because Q1 came back zero): if the per-tile cap
        // refuses nothing, what DOES bound the built world? Background firms are
        // generated per body until they cover `target_ratio` (0.90) of demand OR
        // hit `max_firms_per_body` (40) — a bound whose own comment calls it a
        // "hard bound - never infinite-loop", i.e. a safety valve, not a design
        // target. If a body stops AT 40, the generator ran out of permission
        // before it ran out of demand to cover, and the market opens thin.
        std::map<entity_id, int> firms_per_body;
        for (const auto& [cid, corp] : w.corporations)
        {
            if (!corp.is_background)
                continue;
            entity_id home = null_entity;
            if (!corp.assets.empty())
            {
                const auto bit = w.buildings.find(corp.assets.front());
                if (bit != w.buildings.end())
                {
                    const auto tit = w.tiles.find(bit->second.tile);
                    if (tit != w.tiles.end())
                        home = tit->second.body;
                }
            }
            ++firms_per_body[home];
        }
        int bodies_at_cap = 0, max_firms = 0;
        for (const auto& [bid, n] : firms_per_body)
        {
            if (n >= 40) ++bodies_at_cap;
            max_firms = std::max(max_firms, n);
        }

        const int bldgs = static_cast<int>(w.buildings.size());
        std::printf("%4d | %5d %9d | %5d | %8d %6.2f | %6lld/%-6lld %5.1f | #%u %d/%d\n",
                    i, land, static_cast<int>(w.provinces.provinces.size()), bldgs,
                    at_cap, land ? 100.0 * at_cap / land : 0.0,
                    used_sum, cap_sum, cap_sum ? 100.0 * double(used_sum) / double(cap_sum) : 0.0,
                    fullest_id, fullest_used, fullest_cap);
        std::printf("       background firms: most on any body = %d%s (cap 40)\n",
                    max_firms, bodies_at_cap ? "  <-- AT THE CAP" : "");
        std::fflush(stdout);

        tot_at_cap += at_cap;
        tot_land   += land;
        tot_bldg   += bldgs;
        tot_cap    += cap_total;
    }

    std::printf("\nTOTALS: %lld land tiles, %lld buildings, %lld tiles at cap (%.3f%% of land)\n",
                tot_land, tot_bldg, tot_at_cap,
                tot_land ? 100.0 * double(tot_at_cap) / double(tot_land) : 0.0);
    std::printf("Aggregate non-extraction capacity across all land: %lld slots for %lld buildings"
                " (%.2f%% used)\n",
                tot_cap, tot_bldg, tot_cap ? 100.0 * double(tot_bldg) / double(tot_cap) : 0.0);
    std::printf("\nREAD THIS BEFORE CHANGING A CAP: if tiles-at-cap is ~0, the per-tile ceiling\n"
                "is refusing nothing, and a province-pooled budget cannot add a single building.\n");
    return 0;
}
