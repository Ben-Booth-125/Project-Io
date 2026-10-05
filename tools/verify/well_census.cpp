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
//   * INCOME (--income) — market_viability's G2 window (play ticks 26-50):
//     mean field income, split by the corporations owning a water site (Well
//     or ice) and the rest; water sold BY corporations (exchange rows with a
//     corp seller), split by whether the seller owns a Well or an ice site; and
//     the window's corp-sale revenue per good, to diff two runs. Plus the
//     handoff cohort alive at the last tick (market_viability's G3).
//
// STEPPING mirrors market_viability (the app's order): the 12-tick settle
// spectated at day 0, the seat, then play ticks with advance_orbits /
// advance_surveys over 90 days each. Sample tick N = play tick N; 0 = handoff.
// A READER otherwise: it writes nothing the sim reads (except --scorer, which
// warms the derived logistics-reach cache).
//
// Build:  bash tools/verify/build_lua_harness.sh well_census
// Run:    build_gen/verify/well_census.exe [--seeds 0,43,10] [--ticks 400]
//                                          [--samples 0,50,200,400] [--scorer] [--income]

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
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
    // Water's posted price over its base, the mean over the markets that list it.
    double px = 0.0; int np = 0;
    const std::size_t rw = static_cast<std::size_t>(resource_type::water);
    for (const auto& [mid, mc] : w.markets)
        if (mc.base_price[rw] > 0.0f) { px += mc.price[rw] / mc.base_price[rw]; ++np; }
    std::printf("  tick %3d  WELLS placed %d running %d output %.1f in %zu markets | ICE placed %d running %d output %.1f in %zu markets | water px/base %.2f\n",
                tick, n_well, run_well, out_well, mk_well.size(), n_ice, run_ice, out_ice, mk_ice.size(),
                np ? px / np : 0.0);
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

/// Which water site, if any, each corporation owns: bit 1 a Well, bit 2 ice.
std::map<entity_id, int> water_owners(const world& w)
{
    std::map<entity_id, int> out;
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id a : cc.assets)
        {
            const auto bi = w.buildings.find(a);
            if (bi == w.buildings.end() || bi->second.decommissioned) continue;
            const building_component& b = bi->second;
            if (b.type != building_type::extraction_site || b.target_resource != resource_type::water) continue;
            out[cid] |= placement_rules::is_well_site(w, b.tile, b.target_resource) ? 1 : 2;
        }
    return out;
}

struct income_window
{
    double field = 0, water_owner = 0, rest = 0;                      // field income
    double sold_well = 0, sold_ice = 0, sold_other = 0, sold_qty = 0; // water sold by corps
    std::array<double, resource_count> by_good{};                     // corp-sale revenue per good
};

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{0, 43, 10};
    int ticks = 400;
    bool scorer = false; // --scorer: the SCORER diagnostic at the handoff (warms the reach cache)
    bool income = false; // --income: the G2-window attribution
    std::set<int> samples{0, 50, 200, 400};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--scorer")) scorer = true;
        else if (!std::strcmp(argv[i], "--income")) income = true;
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seeds(argv[++i]);
        else if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--samples") && i + 1 < argc)
        {
            samples.clear();
            for (const std::uint32_t s : parse_seeds(argv[++i])) samples.insert(static_cast<int>(s));
        }
    }
    std::printf("well_census — BL-1198; settle %d ticks, seat, then %d play ticks\n", k_campaign_settle_ticks, ticks);
    constexpr int k_econ_tick_days = 90;        // sim_loop::econ_tick_days
    constexpr int k_win_lo = 26, k_win_hi = 50; // market_viability's G2 window
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
        economy_report last;
        for (int step = 0; step < k_campaign_settle_ticks; ++step)
        {
            settle_tick_result res = run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true);
            if (step == k_campaign_settle_ticks - 1) last = std::move(res.report);
        }
        seat_player_corporation(w, seed, start->land.search.winner_score);
        if (samples.count(0)) built(w, &last, 0);
        if (scorer) top_wells(w, reg, 4);
        std::set<entity_id> cohort;
        for (const auto& [cid, cc] : w.corporations) { (void)cc; cohort.insert(cid); }

        income_window iw;
        for (int k = 1; k <= ticks; ++k)
        {
            const int day = k * k_econ_tick_days;
            advance_orbits(w, static_cast<double>(k_econ_tick_days));
            advance_surveys(w, k_econ_tick_days);
            w.current_day_tick = day;
            const std::size_t prev_total = w.exchanges.total;
            const settle_tick_result res =
                run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day, /*spectating=*/false);
            if (samples.count(k)) built(w, &res.report, k);
            if (income && k >= k_win_lo && k <= k_win_hi)
            {
                const std::map<entity_id, int> own = water_owners(w);
                for (const auto& [cid, cc] : w.corporations)
                {
                    if (cc.returns.empty()) continue;
                    const double inc = cc.returns.back().income;
                    iw.field += inc;
                    (own.count(cid) ? iw.water_owner : iw.rest) += inc;
                }
                const exchange_record_ring& ring = w.exchanges;
                const std::size_t n = std::min<std::size_t>(ring.total - prev_total, ring.size());
                for (std::size_t i = ring.size() - n; i < ring.size(); ++i)
                {
                    const exchange_record& e = ring.oldest_first(i);
                    if (e.seller == null_entity) continue;
                    const double v = static_cast<double>(e.quantity) * e.unit_price;
                    iw.by_good[static_cast<std::size_t>(e.resource)] += v;
                    if (e.resource != resource_type::water) continue;
                    iw.sold_qty += e.quantity;
                    const auto o = own.find(e.seller);
                    const int bits = (o == own.end()) ? 0 : o->second;
                    ((bits & 1) ? iw.sold_well : (bits & 2) ? iw.sold_ice : iw.sold_other) += v;
                }
            }
        }
        if (income)
        {
            const double nw = k_win_hi - k_win_lo + 1;
            std::printf("  INCOME play %d-%d mean/tick: field %.0f = water-site owners %.0f + rest %.0f\n",
                        k_win_lo, k_win_hi, iw.field / nw, iw.water_owner / nw, iw.rest / nw);
            std::printf("  WATER sold by corps, mean/tick: %.1f u for %.0f cr (Well owners %.0f, ice owners %.0f, other %.0f)\n",
                        iw.sold_qty / nw, (iw.sold_well + iw.sold_ice + iw.sold_other) / nw,
                        iw.sold_well / nw, iw.sold_ice / nw, iw.sold_other / nw);
            std::printf("  SALES by good, mean/tick:");
            for (std::size_t r = 0; r < resource_count; ++r)
                if (iw.by_good[r] > 0.5 * nw)
                    std::printf(" %s %.0f", resource_names::name_of(static_cast<resource_type>(r)).c_str(),
                                iw.by_good[r] / nw);
            std::printf("\n");
        }
        int alive = 0;
        for (const entity_id c : cohort) alive += w.corporations.count(c) ? 1 : 0;
        std::printf("  FIRMS handoff %zu, alive at play tick %d: %d\n", cohort.size(), ticks, alive);
        if (samples.count(ticks)) ground(w);
    }
    return 0;
}
