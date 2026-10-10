// household_supply_probe — BL-1193 (household goods unsupplied), the diagnosis
// instrument.
//
// WHY THIS EXISTS. On the shipped 1960 start, centre_decline_trace (BL-1163)
// reads water, clean water and medical supplies LISTED 0 at market across 400
// play ticks, consumer goods falling to ~0 by tick ~100 and food rations at
// 25-45% of demand — while steel_chain_probe sees 15 of 16 clean-water plants
// running on seed 0's first tick. This probe follows each household good from
// its makers to wherever it ends, and the household bid to whoever (if anyone)
// pays for it:
//
//   * the HOUSEHOLD BID — `inject_population_demand`'s per-market pull,
//     recomputed with the formula verbatim (market_clearing.cpp) against the
//     posted price captured BEFORE the tick (the price the injection read);
//   * the MARKET — total demand, listings (`supply`), shelf (`inventory`),
//     corporation pools and their processor reservation, the posted price, and
//     how many markets stand OVER the fair-price ceiling (shelf locked to every
//     draw, `shelf_admits`, components.hpp);
//   * the EXCHANGES of the good this tick, by side: corp -> market (the market
//     as buyer of last resort: who is PAID), market -> corp (a shelf draw: who
//     CONSUMES), corp -> corp (matched);
//   * the MAKERS — processors whose recipe outputs the good and extractors that
//     dig it: placed, running, output, why idle (limiting input, and whether
//     that input's shelf was empty or ceiling-locked), the markets they sit in
//     against the markets that bid, and every exit since the last sample
//     (switched recipe away — to what — decommissioned, removed);
//   * the MAKER CORPORATIONS — count, balance, how many are under water.
//
// A PURE READER. It steps the world with run_settle_tick exactly as
// centre_decline_trace does (12 spectated settle ticks, then play ticks with
// day tick = step) and reads between ticks. It writes nothing the sim reads.
//
// Build:  bash tools/verify/build_lua_harness.sh household_supply_probe
// Run (repo root):
//   build_gen/verify/household_supply_probe.exe [--seeds 0,43,10] [--ticks 200]
//        [--samples 1,12,50,100,200] [--goods food_rations,water,...]
//   Tick T = the state after T economy steps (12 = the seat handoff).
//
// READING (2026-10-04): docs/development/drafts/sprint-49-household-supply-diagnosis.md

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
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

std::string rn(std::size_t r) { return resource_names::name_of(static_cast<resource_type>(r)); }

struct options
{
    std::vector<std::uint32_t> seeds{0, 43, 10};
    int ticks = 200;
    std::set<int> samples{1, 12, 50, 100, 200};
    std::vector<std::size_t> goods;
};

/// Does building `b` make good `g`? Processors by recipe output, extractors by target.
bool makes(const building_component& b, const recipe_registry& reg, std::size_t g)
{
    if (b.type == building_type::processing_facility)
    {
        const recipe* rc = reg.get_recipe(b.recipe);
        return rc != nullptr && rc->outputs[g] > 0.0f;
    }
    if (b.type == building_type::extraction_site)
        return static_cast<std::size_t>(b.target_resource) == g;
    return false;
}

std::string recipe_name(const recipe_registry& reg, uint16_t id)
{
    const recipe* rc = reg.get_recipe(id);
    return rc ? rc->name : std::string("none");
}

/// Exits / entries of makers of one good, accumulated between samples.
struct churn
{
    int switched_away = 0, decommissioned = 0, removed = 0;
    int built = 0, switched_in = 0;
    std::map<std::string, int> away_to; ///< recipe switched to
};

struct tracked
{
    uint16_t recipe = no_recipe;
    resource_type target = resource_type::iron_ore;
    building_type type = building_type::none;
    bool decom = false;
};

using price_snapshot = std::map<entity_id, std::array<float, resource_count>>;

price_snapshot snapshot_prices(const world& w)
{
    price_snapshot s;
    for (const auto& [mid, mc] : w.markets)
    {
        auto& row = s[mid];
        for (std::size_t r = 0; r < resource_count; ++r)
            row[r] = posted_price(mc, r);
    }
    return s;
}

/// The household bid per market for good g, inject_population_demand's formula
/// (market_clearing.cpp) verbatim, priced at `pre` (the posted price the
/// injection read this tick).
std::map<entity_id, float> household_bid(const world& w, const recipe_registry& reg,
                                         std::size_t g, const price_snapshot& pre)
{
    const population_demand_params& pd = reg.population_demand();
    const auto& basket = reg.population_demand_basket();
    std::vector<entity_id> cids;
    for (const auto& kv : w.population_centres) cids.push_back(kv.first);
    std::sort(cids.begin(), cids.end());
    std::map<entity_id, float> out;
    for (const entity_id cid : cids)
    {
        const auto& pcc = w.population_centres.at(cid);
        if (pcc.razed) continue;
        const auto ti = w.population_centre_tile.find(cid);
        if (ti == w.population_centre_tile.end()) continue;
        const entity_id mid = market_for_tile(w, ti->second);
        if (mid == null_entity) continue;
        const market_component& mc = w.markets.at(mid);
        const float base = mc.base_price[g];
        if (base <= 0.0f) continue;
        const float weighted = static_cast<float>(pcc.scale) * pd.demand_scale * basket[g];
        if (weighted <= 0.0f) continue;
        const auto pit = pre.find(mid);
        const float price = (pit != pre.end() && pit->second[g] > 0.0f) ? pit->second[g] : base;
        const float el = std::clamp(std::pow(base / price, pd.demand_elasticity),
                                    pd.elasticity_min, pd.elasticity_max);
        out[mid] += weighted * el;
    }
    return out;
}

void sample(const world& w, const recipe_registry& reg, const options& opt, int tick,
            const settle_tick_result& res, const price_snapshot& pre,
            const std::vector<exchange_record>& rows, std::map<std::size_t, churn>& ch)
{
    const float res_mult = reg.price_band().reservation_mult;
    std::vector<entity_id> mids;
    for (const auto& kv : w.markets) mids.push_back(kv.first);
    std::sort(mids.begin(), mids.end());

    std::map<entity_id, entity_id> owner;
    {
        std::vector<entity_id> cids;
        for (const auto& kv : w.corporations) cids.push_back(kv.first);
        std::sort(cids.begin(), cids.end());
        for (const entity_id c : cids)
            for (const entity_id b : w.corporations.at(c).assets) owner[b] = c;
    }
    std::vector<entity_id> bids;
    for (const auto& kv : w.buildings) bids.push_back(kv.first);
    std::sort(bids.begin(), bids.end());

    std::printf("--- tick %d ---\n", tick);
    for (const std::size_t g : opt.goods)
    {
        const auto hh = household_bid(w, reg, g, pre);
        float hh_total = 0.0f;
        for (const auto& [m, v] : hh) hh_total += v;

        float dem = 0, sup = 0, shelf = 0, pools = 0, reserve = 0, locked_shelf = 0;
        float pb_w = 0, pb_n = 0;
        int locked_mk = 0, bid_mk = 0, bid_locked = 0;
        for (const entity_id m : mids)
        {
            const market_component& mc = w.markets.at(m);
            if (mc.base_price[g] <= 0.0f) continue;
            dem += mc.demand[g]; sup += mc.supply[g]; shelf += std::max(0.0f, mc.inventory[g]);
            const bool admits = shelf_admits(mc, g, res_mult, true);
            const float pbase = posted_price(mc, g) / mc.base_price[g];
            if (!admits) { ++locked_mk; locked_shelf += std::max(0.0f, mc.inventory[g]); }
            const auto hit = hh.find(m);
            if (hit != hh.end() && hit->second > 0.0f)
            {
                ++bid_mk; if (!admits) ++bid_locked;
                pb_w += pbase * hit->second; pb_n += hit->second;
            }
        }
        // BL-1265: corporations hold no pools (and so no processor reservation);
        // the pool and reserve columns read 0.

        // Exchanges of g this tick, by side.
        float sold_q = 0, sold_v = 0, drawn_q = 0, drawn_v = 0, matched_q = 0;
        std::set<entity_id> sellers, buyers;
        for (const exchange_record& e : rows)
        {
            if (static_cast<std::size_t>(e.resource) != g) continue;
            if (e.seller != null_entity && e.buyer == null_entity)
            { sold_q += e.quantity; sold_v += e.quantity * e.unit_price; sellers.insert(e.seller); }
            else if (e.seller == null_entity && e.buyer != null_entity)
            { drawn_q += e.quantity; drawn_v += e.quantity * e.unit_price; buyers.insert(e.buyer); }
            else if (e.seller != null_entity && e.buyer != null_entity)
                matched_q += e.quantity;
        }

        // Makers.
        int placed = 0, building = 0, running = 0, nolab = 0, starved = 0, other = 0;
        float out_q = 0;
        std::map<std::string, int> limit_empty, limit_locked, limit_have;
        std::set<entity_id> maker_mk, maker_corps;
        for (const entity_id bid : bids)
        {
            const building_component& b = w.buildings.at(bid);
            if (b.decommissioned || !makes(b, reg, g)) continue;
            ++placed;
            const auto oit = owner.find(bid);
            if (oit != owner.end()) maker_corps.insert(oit->second);
            const entity_id mid = market_for_tile(w, b.tile);
            maker_mk.insert(mid);
            if (b.ticks_remaining > 0) { ++building; continue; }
            const auto rit = res.report.building_row.find(bid);
            const building_report* br = (rit != res.report.building_row.end())
                                            ? &res.report.buildings[rit->second] : nullptr;
            if (br == nullptr) { ++other; continue; }
            if (br->active) { ++running; out_q += br->output_quantity; continue; }
            if (br->effective_workforce <= 0.0f) { ++nolab; continue; }
            if (br->has_limiting)
            {
                ++starved;
                const std::size_t li = static_cast<std::size_t>(br->limiting_input);
                const auto mit = w.markets.find(mid);
                if (mit == w.markets.end()) { limit_empty[rn(li)]++; continue; }
                const market_component& mc = mit->second;
                if (mc.inventory[li] <= 0.0f) limit_empty[rn(li)]++;
                else if (!shelf_admits(mc, li, res_mult, true)) limit_locked[rn(li)]++;
                else limit_have[rn(li)]++;
                continue;
            }
            ++other;
        }
        int bid_with_maker = 0;
        for (const auto& [m, v] : hh) if (v > 0.0f && maker_mk.count(m)) ++bid_with_maker;
        int under = 0; float bal = 0;
        for (const entity_id c : maker_corps)
        {
            const float cb = w.corporations.at(c).balance;
            bal += cb; if (cb < 0.0f) ++under;
        }

        std::printf("  %-16s HH bid %8.1f in %2d mkts (%2d w/ maker, %2d over ceiling) | demand %8.1f listed %8.1f shelf %9.1f (locked %9.1f in %2d mkts) pools %8.1f (reserved %7.1f) | px/base %.2f\n",
                    rn(g).c_str(), hh_total, bid_mk, bid_with_maker, bid_locked, dem, sup, shelf,
                    locked_shelf, locked_mk, pools, reserve, pb_n > 0 ? pb_w / pb_n : 0.0f);
        std::printf("  %-16s exch: corp->mkt %7.1f u @%.2f (%zu sellers) | mkt->corp draw %7.1f u @%.2f (%zu buyers) | matched %.1f\n",
                    "", sold_q, sold_q > 0 ? sold_v / sold_q : 0.0f, sellers.size(), drawn_q,
                    drawn_q > 0 ? drawn_v / drawn_q : 0.0f, buyers.size(), matched_q);
        std::printf("  %-16s makers: placed %3d building %2d running %3d out %7.1f no-labour %2d input-starved %3d other %2d | in %2d mkts | corps %2d bal %.0f (%d < 0)\n",
                    "", placed, building, running, out_q, nolab, starved, other,
                    static_cast<int>(maker_mk.size()), static_cast<int>(maker_corps.size()), bal, under);
        if (starved > 0)
        {
            std::string s;
            for (const auto& [n, c] : limit_empty)  s += " " + n + ":empty-shelf " + std::to_string(c);
            for (const auto& [n, c] : limit_locked) s += " " + n + ":ceiling-locked " + std::to_string(c);
            for (const auto& [n, c] : limit_have)   s += " " + n + ":shelf-admits " + std::to_string(c);
            std::printf("  %-16s starved on:%s\n", "", s.c_str());
        }
        churn& c = ch[g];
        std::string away;
        for (const auto& [n, k] : c.away_to) away += " " + n + ":" + std::to_string(k);
        std::printf("  %-16s since last sample: built %d switched-in %d | switched-away %d (%s) decommissioned %d removed %d\n",
                    "", c.built, c.switched_in, c.switched_away, away.empty() ? "-" : away.c_str() + 1,
                    c.decommissioned, c.removed);
        c = churn{};
    }
    std::fflush(stdout);
}

/// The exchange rows pushed since `prev_total`, oldest first.
std::vector<exchange_record> new_rows(const world& w, std::size_t prev_total)
{
    const exchange_record_ring& ring = w.exchanges;
    std::vector<exchange_record> out;
    const std::size_t n = ring.total - prev_total;
    const std::size_t cap = exchange_record_ring::capacity;
    if (n > cap)
        std::printf("  WARN: %zu exchange rows this tick exceed the ring (%zu); reading the newest only\n", n, cap);
    const std::size_t sz = ring.size();
    const std::size_t take = std::min(n, sz);
    for (std::size_t i = sz - take; i < sz; ++i)
        out.push_back(ring.oldest_first(i));
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    options opt;
    std::string goods_arg = "food_rations,agricultural_produce,water,clean_water,consumer_goods,medical_supplies,steel";
    for (int i = 1; i < argc; ++i)
    {
        auto list = [&](std::vector<std::string>& out) {
            std::stringstream ss(argv[++i]); std::string t;
            while (std::getline(ss, t, ',')) if (!t.empty()) out.push_back(t);
        };
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) opt.ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        { std::vector<std::string> v; list(v); opt.seeds.clear(); for (auto& s : v) opt.seeds.push_back(std::strtoul(s.c_str(), nullptr, 10)); }
        else if (!std::strcmp(argv[i], "--samples") && i + 1 < argc)
        { std::vector<std::string> v; list(v); opt.samples.clear(); for (auto& s : v) opt.samples.insert(std::atoi(s.c_str())); }
        else if (!std::strcmp(argv[i], "--goods") && i + 1 < argc) goods_arg = argv[++i];
    }
    {
        std::stringstream ss(goods_arg); std::string t;
        while (std::getline(ss, t, ','))
        {
            bool ok = false;
            const resource_type r = resource_names::resource_from_name(t, ok);
            if (ok) opt.goods.push_back(static_cast<std::size_t>(r));
            else std::printf("unknown good %s\n", t.c_str());
        }
    }
    std::printf("household_supply_probe — BL-1193; settle %d ticks then play\n", k_campaign_settle_ticks);
    for (const std::uint32_t seed : opt.seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        build_app_start_world(lua, p, *start);
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        std::printf("=== seed %u === reservation_mult %.2f ceil_mult %.2f shelf_supply_ticks %.2f\n", seed,
                    reg.price_band().reservation_mult, reg.price_band().ceil_mult,
                    reg.price_band().shelf_supply_ticks);
        {
            std::string b;
            const auto& basket = reg.population_demand_basket();
            for (std::size_t r = 0; r < resource_count; ++r)
                if (basket[r] > 0.0f) { char buf[64]; std::snprintf(buf, sizeof buf, " %s %.2f", rn(r).c_str(), basket[r]); b += buf; }
            std::printf("  household basket (per scale point):%s\n", b.c_str());
        }

        std::map<entity_id, tracked> prev;
        for (const auto& [bid, b] : w.buildings) prev[bid] = {b.recipe, b.target_resource, b.type, b.decommissioned};
        std::map<std::size_t, churn> ch;

        for (int step = 0; step < opt.ticks; ++step)
        {
            const price_snapshot pre = snapshot_prices(w);
            const std::size_t prev_total = w.exchanges.total;
            const bool settle = step < k_campaign_settle_ticks;
            const settle_tick_result res = run_settle_tick(w, reg, step, settle ? 0 : step, settle);
            const std::vector<exchange_record> rows = new_rows(w, prev_total);

            // Churn: diff each building's maker status against the last tick.
            std::map<entity_id, tracked> now;
            for (const auto& [bid, b] : w.buildings) now[bid] = {b.recipe, b.target_resource, b.type, b.decommissioned};
            for (const std::size_t g : opt.goods)
            {
                churn& c = ch[g];
                auto mk = [&](const tracked& t) {
                    if (t.decom) return false;
                    building_component tmp{};
                    tmp.type = t.type; tmp.recipe = t.recipe; tmp.target_resource = t.target;
                    return makes(tmp, reg, g);
                };
                for (const auto& [bid, t] : prev)
                {
                    if (!mk(t)) continue;
                    const auto nit = now.find(bid);
                    if (nit == now.end()) { ++c.removed; continue; }
                    if (nit->second.decom) { ++c.decommissioned; continue; }
                    if (!mk(nit->second)) { ++c.switched_away; c.away_to[recipe_name(reg, nit->second.recipe)]++; }
                }
                for (const auto& [bid, t] : now)
                {
                    if (!mk(t)) continue;
                    const auto pit = prev.find(bid);
                    if (pit == prev.end()) ++c.built;
                    else if (!mk(pit->second) && !pit->second.decom) ++c.switched_in;
                    else if (!mk(pit->second)) ++c.built; // resumed from decommission
                }
            }
            prev.swap(now);

            if (opt.samples.count(step + 1))
                sample(w, reg, opt, step + 1, res, pre, rows, ch);
        }
    }
    return 0;
}
