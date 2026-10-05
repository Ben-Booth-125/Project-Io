// well_census — BL-1198 (water where people live), the measurement.
//
// Per curated seed, on the shipped 1960 start (build_app_start_world, the same
// world the app and household_supply_probe build):
//
//   * GROUND — land tiles that qualify for a Well (placement_rules::is_well_site:
//     no ice deposit, a river along the tile or a lake beside it), split by
//     route (river / lake-only), and how many of them are unoccupied;
//   * MARKETS — how many markets hold at least one qualifying tile in their
//     catchment (market_for_tile), of all markets;
//   * BUILT — water extraction sites by kind (Well vs Ice Extractor), placed,
//     running and output, at the requested sample ticks (multi-tick: the scorer
//     must find, build and staff a Well, which no one-tick row can see).
//
// A PURE READER. Steps the world with run_settle_tick exactly as
// household_supply_probe does (settle ticks spectated, then play ticks).
//
// Build:  bash tools/verify/build_lua_harness.sh well_census
// Run:    build_gen/verify/well_census.exe [--seeds 0,43,10] [--ticks 200]
//                                          [--samples 0,12,50,200] [--scorer]

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint32_t> parse_seeds(const char* s)
{
    std::vector<std::uint32_t> out;
    std::stringstream ss(s); std::string t;
    while (std::getline(ss, t, ',')) if (!t.empty()) out.push_back(std::strtoul(t.c_str(), nullptr, 10));
    return out;
}

void ground(const world& w)
{
    int land = 0, wells = 0, river = 0, lake_only = 0, free_wells = 0;
    std::set<entity_id> occupied;
    for (const auto& [bid, b] : w.buildings) occupied.insert(b.tile);
    std::set<entity_id> markets_with, all_markets;
    for (const auto& [mid, mc] : w.markets) all_markets.insert(mid);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (is_water(tc.substrate) || tc.habitability <= 0.0f) continue;
        ++land;
        if (!placement_rules::is_well_site(w, tid, resource_type::water)) continue;
        ++wells;
        if (tc.river_edges != 0) ++river; else ++lake_only;
        if (!occupied.count(tid)) ++free_wells;
        if (const entity_id m = market_for_tile(w, tid); m != null_entity) markets_with.insert(m);
    }
    std::printf("  GROUND habitable land %d | Well-qualifying %d (%.1f%%): river %d, lake-only %d, unoccupied %d\n",
                land, wells, land ? 100.0 * wells / land : 0.0, river, lake_only, free_wells);
    std::printf("  MARKETS with a Well-qualifying tile: %zu of %zu\n", markets_with.size(), all_markets.size());
}

void built(const world& w, const economy_report* rep, int tick)
{
    std::map<entity_id, const building_report*> by_id;
    if (rep) for (const building_report& br : rep->buildings) by_id[br.building] = &br;
    int n_well = 0, run_well = 0, n_ice = 0, run_ice = 0;
    float out_well = 0.0f, out_ice = 0.0f;
    std::set<entity_id> mk_well, mk_ice;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::extraction_site || b.target_resource != resource_type::water
            || b.decommissioned)
            continue;
        const bool well = placement_rules::is_well_site(w, b.tile, b.target_resource);
        const auto it = by_id.find(bid);
        const bool active = it != by_id.end() && it->second->active;
        // A Well's output is all water; an ice site's report sums its basket
        // (BL-437), which is still the figure it credited this tick.
        const float out = (it != by_id.end()) ? it->second->output_quantity : 0.0f;
        const entity_id m = market_for_tile(w, b.tile);
        if (well) { ++n_well; run_well += active; out_well += out; if (m != null_entity) mk_well.insert(m); }
        else      { ++n_ice;  run_ice  += active; out_ice  += out; if (m != null_entity) mk_ice.insert(m); }
    }
    std::printf("  tick %3d  WELLS placed %d running %d output %.1f in %zu markets | ICE placed %d running %d output %.1f in %zu markets\n",
                tick, n_well, run_well, out_well, mk_well.size(), n_ice, run_ice, out_ice, mk_ice.size());
}

/// The scorer's Well bucket as rank_extraction_sites keeps it (corp_ai.cpp,
/// BL-1198): habitability descending, tile id ascending; the demand weight is
/// one number for every Well so it cannot reorder them; only placeable tiles
/// (reach aside) enter, as in the scorer. Survey fog is not
/// applied here (an upper bound on what the scorer sees). For the top @p n,
/// the placement verdict WITH the seam's logistics-reach budget — the gate a
/// scorer pick meets at construct_building.
void top_wells(world& w, const recipe_registry& reg, int n)
{
    std::vector<std::pair<float, entity_id>> c;
    for (const auto& [tid, tc] : w.tiles)
        if (!is_water(tc.substrate) && placement_rules::is_well_site(w, tid, resource_type::water)
            && placement_rules::can_place_in_world(w, tid, building_type::extraction_site,
                                                   resource_type::water))
            c.push_back({tc.habitability, tid});
    std::sort(c.begin(), c.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second < b.second;
    });
    int tied = 0;
    for (const auto& e : c) if (!c.empty() && e.first == c.front().first) ++tied;
    std::printf("  SCORER well bucket: %zu candidates, %d tied at the top habitability %.3f\n",
                c.size(), tied, c.empty() ? 0.0f : c.front().first);
    const float budget = reg.construction().max_logistics_reach;
    for (int i = 0; i < n && i < static_cast<int>(c.size()); ++i)
    {
        const entity_id tid = c[static_cast<std::size_t>(i)].second;
        body_reach_field(w, w.tiles.at(tid).body);
        const auto pr = placement_rules::can_place_in_world(w, tid, building_type::extraction_site,
                                                            resource_type::water, budget);
        std::printf("    #%d tile %llu hab %.3f market %llu reach %.1f (budget %.1f): %s\n", i + 1,
                    static_cast<unsigned long long>(tid), c[static_cast<std::size_t>(i)].first,
                    static_cast<unsigned long long>(market_for_tile(w, tid)), tile_reach_cost(w, tid), budget,
                    placement_rules::placement_reason_text(pr.reason));
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{0, 43, 10};
    int ticks = 200;
    bool scorer = false; // --scorer: the SCORER diagnostic at the handoff (warms the reach cache)
    std::set<int> samples{0, 12, 50, 200};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--scorer")) scorer = true;
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seeds(argv[++i]);
        else if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--samples") && i + 1 < argc)
        {
            samples.clear();
            for (const std::uint32_t s : parse_seeds(argv[++i])) samples.insert(static_cast<int>(s));
        }
    }
    std::printf("well_census — BL-1198; settle %d ticks then play\n", k_campaign_settle_ticks);
    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        build_app_start_world(lua, p, *start);
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        std::printf("=== seed %u ===\n", seed);
        ground(w);
        if (samples.count(0)) built(w, nullptr, 0);
        for (int step = 0; step < ticks; ++step)
        {
            const bool settle = step < k_campaign_settle_ticks;
            const settle_tick_result res = run_settle_tick(w, reg, step, settle ? 0 : step, settle);
            if (samples.count(step + 1)) built(w, &res.report, step + 1);
            if (scorer && step + 1 == k_campaign_settle_ticks) top_wells(w, reg, 4);
        }
        if (samples.count(ticks)) ground(w);
    }
    return 0;
}
