// trade_retrofit_probe — how many Planetary Marketplaces the history's trade
// record retrofits, where, and who holds them (BL-1268; docs/economy/TRADE.md
// § Trade in generation).
//
// TRADE.md: "How many points a nation earns per unit of history trade or
// relation, and how the record converts into Marketplaces (count, size, owner),
// are proposed by measurement and approved by Ben." This is that measurement.
//
// Per seed, on the world the player is handed (harness_params.hpp
// `build_app_start_world`: the app's generation, the search, the winner's
// apply and the retrofit, `world_gen.lua` parsed and passed):
//   * the record: its raw history (flow volume x years, mutual-treaty
//     partner-years) and its points at the authored rates;
//   * the retrofit: markets with a record, Marketplaces bought, placed, and
//     every unspent point by reason;
//   * per nation (the owner's home nation) and per body, Marketplaces placed;
//   * corporations at the handoff holding one, split specialist / background;
//   * a RATE LADDER: the Marketplaces bought at other rates, from the same
//     per-market raw sums (approximate: one integer division per market rather
//     than per cell, so a rung can read a unit off what it would place).
//
// A READING, NOT A GATE. Nothing asserts a count: the rates are Ben's call.
// The run fails (exit 1) only on its own honesty checks — a placed
// Marketplace not in its owner's assets, one owned by the seat, or a report
// whose points do not account.
//
// Build:  bash tools/verify/build_lua_harness.sh trade_retrofit_probe
// Run:    build_gen/verify/trade_retrofit_probe.exe [seed ...]   (default: the 16 curated)

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/corporation_generation.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

namespace
{

const std::uint32_t k_curated[] = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };

struct rung
{
    std::int64_t flow_per_1000, relation_per_year, price;
    int          cap;
};
const rung k_ladder[] = {
    { 40, 1, 40000, 3 }, { 20, 1, 40000, 3 }, { 80, 1, 40000, 3 },
    { 40, 0, 40000, 3 }, { 40, 2, 40000, 3 }, { 40, 1, 30000, 3 },
    { 40, 1, 50000, 3 }, { 40, 1, 80000, 3 }, { 40, 1, 40000, 1 },
    { 40, 1, 40000, 5 },
};

int failures = 0;
void check(bool ok, const char* what, std::uint32_t seed)
{
    if (!ok)
    {
        std::printf("FAIL  seed %u: %s\n", seed, what);
        ++failures;
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    if (seeds.empty())
        seeds.assign(std::begin(k_curated), std::end(k_curated));

    lua_state lua;
    std::printf("trade_retrofit_probe: %zu seeds\n", seeds.size());

    std::vector<std::int64_t> ladder_total(std::size(k_ladder), 0);
    int sum_placed = 0, sum_corps = 0, sum_holding = 0, sum_markets = 0;
    for (const std::uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed         = seed;
        app_start_world s;
        build_app_start_world(lua, p, s);
        const world&                       w = s.w;
        const marketplace_retrofit_report& r = s.land.retrofit;
        const trade_retrofit_params&       rates = s.cfg.trade_retrofit;

        std::int64_t flow = 0, rel = 0, pts = 0;
        std::map<std::int32_t, std::int64_t> polity_pts;
        for (const gen_trade_cell& c : w.gen_trade_record.cells)
        {
            flow += c.flow_volume;
            rel  += c.relation_years;
            pts  += c.points;
            polity_pts[c.polity] += c.points;
        }
        check(pts == r.points_total, "record points != report total", seed);
        std::int64_t placed_pts = 0;
        for (const marketplace_retrofit_row& row : r.rows)
            placed_pts += static_cast<std::int64_t>(row.bought) * rates.points_per_marketplace;
        check(placed_pts + r.points_no_market + r.points_below_one + r.points_over_cap == r.points_total,
              "points do not account", seed);

        // Marketplaces in the world, by owner / nation / body.
        std::map<entity_id, entity_id> owner_of; // building -> corp
        std::vector<entity_id> corp_ids;
        for (const auto& kv : w.corporations)
            corp_ids.push_back(kv.first);
        std::sort(corp_ids.begin(), corp_ids.end());
        for (const entity_id cid : corp_ids)
            for (const entity_id bid : w.corporations.at(cid).assets)
                owner_of[bid] = cid;
        int total_mp = 0, mp_owned = 0;
        std::map<entity_id, int> by_nation, by_body;
        std::map<entity_id, int> by_corp;
        for (const auto& [bid, bc] : w.buildings)
        {
            if (bc.type != building_type::planetary_marketplace)
                continue;
            ++total_mp;
            const auto o = owner_of.find(bid);
            check(o != owner_of.end(), "a Marketplace no corporation owns", seed);
            if (o == owner_of.end())
                continue;
            ++mp_owned;
            check(o->second != w.player_entity || w.player_entity == null_entity,
                  "a Marketplace retrofitted for the seat", seed);
            ++by_corp[o->second];
            ++by_nation[w.corporations.at(o->second).home_nation];
            if (const auto t = w.tiles.find(bc.tile); t != w.tiles.end())
                ++by_body[t->second.body];
        }
        check(total_mp == r.placed, "Marketplaces in the world != placed", seed);
        int spec = 0, bg = 0, spec_hold = 0, bg_hold = 0;
        for (const entity_id cid : corp_ids)
        {
            const bool background = w.corporations.at(cid).is_background;
            (background ? bg : spec)++;
            if (by_corp.count(cid) != 0)
                (background ? bg_hold : spec_hold)++;
        }

        std::printf("\nseed %u: record %zu cells over %zu polities; flow %lld, relation %lld partner-years "
                    "-> %lld points (rates %lld/1000, %lld/yr; %lld a Marketplace, cap %d)\n",
                    seed, w.gen_trade_record.cells.size(), polity_pts.size(),
                    static_cast<long long>(flow), static_cast<long long>(rel), static_cast<long long>(pts),
                    static_cast<long long>(rates.flow_points_per_1000),
                    static_cast<long long>(rates.relation_points_per_year),
                    static_cast<long long>(rates.points_per_marketplace), rates.max_per_market);
        std::printf("  markets %zu on the world, %d with record; bought %d, placed %d (no owner %d, no site %d); "
                    "unspent: no market %lld, below one %lld, over cap %lld\n",
                    w.markets.size(), r.markets_with_record, r.bought, r.placed, r.no_owner, r.no_site,
                    static_cast<long long>(r.points_no_market), static_cast<long long>(r.points_below_one),
                    static_cast<long long>(r.points_over_cap));
        std::printf("  corporations %zu (specialists %d, background %d); holding a Marketplace %zu "
                    "(specialists %d, background %d)\n",
                    corp_ids.size(), spec, bg, by_corp.size(), spec_hold, bg_hold);
        std::printf("  per nation:");
        for (const auto& [n, k] : by_nation)
        {
            const auto nit = w.nations.find(n);
            std::printf(" %s=%d", nit != w.nations.end() ? nit->second.name.c_str() : "?", k);
        }
        std::printf("\n  per body:");
        for (const auto& [b, k] : by_body)
        {
            const auto bit = w.bodies.find(b);
            std::printf(" %s=%d", bit != w.bodies.end() ? bit->second.name.c_str() : "?", k);
        }
        std::printf("\n  markets (points, bought, placed):");
        for (const marketplace_retrofit_row& row : r.rows)
            if (row.bought > 0)
                std::printf(" [%llu: %lld, %d, %d]", static_cast<unsigned long long>(row.market),
                            static_cast<long long>(row.points), row.bought, row.placed);
        // One machine-readable line per market with a record: what a rate
        // ladder is read from offline (the owner is found at every rate).
        for (const marketplace_retrofit_row& row : r.rows)
        {
            const auto oc = w.corporations.find(row.owner);
            std::printf("\nROW %u %llu %lld %lld %llu %d %d %llu", seed,
                        static_cast<unsigned long long>(row.market),
                        static_cast<long long>(row.flow_volume), static_cast<long long>(row.relation_years),
                        static_cast<unsigned long long>(row.owner),
                        oc != w.corporations.end() && oc->second.is_background ? 1 : 0, row.owner_holdings,
                        static_cast<unsigned long long>(oc != w.corporations.end() ? oc->second.home_nation
                                                                                   : null_entity));
        }
        std::printf("\n  ladder (flow/1000, rel/yr, price, cap -> bought):");
        for (std::size_t i = 0; i < std::size(k_ladder); ++i)
        {
            const rung& g = k_ladder[i];
            std::int64_t bought = 0;
            for (const marketplace_retrofit_row& row : r.rows)
            {
                const std::int64_t mp = row.flow_volume * g.flow_per_1000 / 1000
                                      + row.relation_years * g.relation_per_year;
                bought += std::min<std::int64_t>(g.cap, mp / g.price);
            }
            ladder_total[i] += bought;
            std::printf(" (%lld,%lld,%lld,%d)->%lld", static_cast<long long>(g.flow_per_1000),
                        static_cast<long long>(g.relation_per_year), static_cast<long long>(g.price), g.cap,
                        static_cast<long long>(bought));
        }
        std::printf("\n");
        std::fflush(stdout);
        sum_placed  += r.placed;
        sum_corps   += static_cast<int>(corp_ids.size());
        sum_holding += static_cast<int>(by_corp.size());
        sum_markets += static_cast<int>(w.markets.size());
    }

    std::printf("\nTOTAL over %zu seeds: %d Marketplaces placed on %d markets; %d of %d corporations hold one\n",
                seeds.size(), sum_placed, sum_markets, sum_holding, sum_corps);
    std::printf("ladder totals:");
    for (std::size_t i = 0; i < std::size(k_ladder); ++i)
        std::printf(" (%lld,%lld,%lld,%d)->%lld", static_cast<long long>(k_ladder[i].flow_per_1000),
                    static_cast<long long>(k_ladder[i].relation_per_year),
                    static_cast<long long>(k_ladder[i].price), k_ladder[i].cap,
                    static_cast<long long>(ladder_total[i]));
    std::printf("\n%s (%d failures)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
