#include "trade.hpp"

#include "market_clearing.hpp" // market_for_tile, shelf_admits, posted_price
#include "supply_system.hpp"   // price_trade_leg, commit_trade_shipment, trade_room

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace {

entity_id building_body_of(const world& w, const building_component& b)
{
    const auto tit = w.tiles.find(b.tile);
    return tit == w.tiles.end() ? null_entity : tit->second.body;
}

bool trade_building_active(const building_component& b)
{
    return is_trade_building(b.type) && b.ticks_remaining <= 0 && !b.decommissioned;
}

} // namespace

float building_trade_points(const recipe_registry& reg, const building_component& b,
                            float contention, bool upkeep_met)
{
    if (!trade_building_active(b) || !upkeep_met)
        return 0.0f;
    const trade_params& tp = reg.trade();
    if (b.type == building_type::port)
        return tp.port_points;
    const float wt_scalar = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
    const float pts = tp.marketplace_points * b.workforce_assigned * contention * wt_scalar;
    return (std::isfinite(pts) && pts > 0.0f) ? pts : 0.0f;
}

float corp_trade_points(const world& w, const recipe_registry& reg,
                        const economy_report& report, entity_id corp)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return 0.0f;
    float total = 0.0f;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end() || !trade_building_active(bit->second))
            continue;
        const entity_id body = building_body_of(w, bit->second);
        const auto cont = report.workforce_contention.find(std::make_pair(corp, body));
        const float contention = (cont != report.workforce_contention.end()) ? cont->second : 1.0f;
        const bool met = report.upkeep_unmet.count(bid) == 0;
        total += building_trade_points(reg, bit->second, contention, met);
    }
    return total;
}

std::vector<entity_id> corp_trade_markets(const world& w, entity_id corp)
{
    std::vector<entity_id> out;
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return out;
    std::set<entity_id> bodies;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end() || !trade_building_active(bit->second))
            continue;
        const entity_id body = building_body_of(w, bit->second);
        if (body != null_entity)
            bodies.insert(body);
    }
    if (bodies.empty())
        return out;
    for (const auto& [mid, mc] : w.markets)
        if (bodies.count(mc.body) != 0)
            out.push_back(mid);
    std::sort(out.begin(), out.end());
    return out;
}

bool trade_is_valid(const world& w, const recipe_registry& reg, const standing_trade& t)
{
    const std::size_t r = static_cast<std::size_t>(t.resource);
    if (r >= resource_count)
        return false;
    if (w.corporations.find(t.owner) == w.corporations.end())
        return false;
    if (t.from_market == t.to_market)
        return false;
    if (w.markets.find(t.from_market) == w.markets.end() || w.markets.find(t.to_market) == w.markets.end())
        return false;
    if (reg.grid_goods().grid(r) || !(reg.trade().capacity[r] > 0.0f))
        return false;
    return std::isfinite(t.points) && t.points > 0.0f;
}

trade_tick run_trades(world& w, const recipe_registry& reg, economy_report& report,
                      lp_pool_map* shared_lp_pools)
{
    trade_tick out;
    const trade_params& tp = reg.trade();

    std::vector<entity_id> corp_ids;
    corp_ids.reserve(w.corporations.size());
    for (const auto& [cid, cc] : w.corporations)
    {
        (void)cc;
        corp_ids.push_back(cid);
    }
    std::sort(corp_ids.begin(), corp_ids.end());

    // Every corporation's points this tick are stored whether or not it trades,
    // so a building lost or an upkeep unmet reads as zero, not as last tick's.
    std::map<entity_id, float> points;
    for (const entity_id corp : corp_ids)
    {
        const float p = corp_trade_points(w, reg, report, corp);
        w.corporations.at(corp).trade_points = p;
        if (p > 0.0f)
        {
            points[corp] = p;
            out.points_made += p;
            ++out.corps_trading;
        }
    }

    // BL-1222 (trade-flow lens): the PLAYER's shipments this pass. WRITE-ONLY.
    trade_flow_pass tf;
    tf.corp = w.player_entity;

    if (!points.empty())
    {
        lp_pool_map local_pools;
        lp_pool_map* lp = (shared_lp_pools != nullptr) ? shared_lp_pools : &local_pools;
        const logistics_nodes nodes = collect_logistics_nodes(w);
        const float res_mult = reg.price_band().reservation_mult;
        const float margin   = reg.dispatch_margin();

        // Same-body haul per unit, per (source, destination) market, priced once
        // per pass on a one-unit leg (cost is linear in quantity): corporation-
        // and good-independent (`price_market_leg`). NaN = no route.
        std::map<std::pair<entity_id, entity_id>, float> intra_haul;
        auto haul_per_unit = [&](entity_id corp, entity_id a, entity_id b, std::size_t ri) {
            const market_component& ma = w.markets.at(a);
            const market_component& mb = w.markets.at(b);
            if (ma.body == mb.body)
            {
                const auto key = std::make_pair(a, b);
                auto it = intra_haul.find(key);
                if (it == intra_haul.end())
                {
                    const convoy_leg leg = price_market_leg(w, reg, nodes, a, b, 1.0f);
                    it = intra_haul.emplace(key, leg.viable ? leg.cost : std::nanf("")).first;
                }
                return it->second;
            }
            const convoy_leg leg = price_trade_leg(w, reg, nodes, corp, a, b, ri, 1.0f);
            return leg.viable ? leg.cost : std::nanf("");
        };

        // Ship up to `units` of good `ri` from `a` to `b` for `corp`. Returns
        // the units actually sent.
        auto ship = [&](entity_id corp, entity_id a, entity_id b, std::size_t ri, float units,
                        bool is_auto) -> float {
            if (!(units > 0.0f) || !std::isfinite(units))
                return 0.0f;
            const convoy_leg leg = price_trade_leg(w, reg, nodes, corp, a, b, ri, units);
            if (!leg.viable)
                return 0.0f;
            bool  refused_lp = false;
            float sent       = 0.0f;
            if (!commit_trade_shipment(w, reg, report, corp, a, b, ri, units, leg, lp,
                                       &refused_lp, &sent))
            {
                if (refused_lp)
                    ++out.refused_no_lp;
                return 0.0f;
            }
            if (is_auto)
                ++out.auto_shipments;
            else
                ++out.manual_shipments;
            out.units_shipped += sent;
            if (corp == w.player_entity)
            {
                tf.shipments.push_back({a, b, static_cast<std::uint16_t>(ri), sent,
                                        dispatch_market_price(w.markets.at(b), ri)});
                tf.best[std::make_pair(b, static_cast<std::uint16_t>(ri))] = trade_refusal::sent;
            }
            return sent;
        };

        struct candidate
        {
            float       score;  // margin per point
            entity_id   a;
            entity_id   b;
            std::size_t r;
            float       landed; // source price + haul per unit
        };
        std::vector<candidate> cands;

        for (const auto& [corp, made] : points)
        {
            const corporation_component& cc = w.corporations.at(corp);
            const float reserve = std::clamp(cc.trade_reserve, 0.0f, made);
            float auto_pts      = made - reserve;

            // --- MANUAL: the owner's trades, in placement order, spend the reserve.
            float reserve_left = reserve;
            for (std::size_t i = 0; i < w.trades.size() && reserve_left > 0.0f; ++i)
            {
                const standing_trade t = w.trades[i]; // a copy: shipping appends convoys, not trades
                if (t.owner != corp || !trade_is_valid(w, reg, t))
                    continue;
                const std::size_t ri = static_cast<std::size_t>(t.resource);
                const float pts  = std::min(t.points, reserve_left);
                const float sent = ship(corp, t.from_market, t.to_market, ri, pts * tp.capacity[ri],
                                        /*is_auto=*/false);
                const float used = sent / tp.capacity[ri];
                reserve_left -= std::min(pts, used);
                out.points_spent += std::min(pts, used);
            }

            // --- AUTO: the unreserved points, best margin per point first.
            if (!(auto_pts > 0.0f))
                continue;
            const std::vector<entity_id> reach = corp_trade_markets(w, corp);
            if (reach.size() < 2)
                continue;
            cands.clear();
            for (const entity_id a : reach)
            {
                const market_component& ma = w.markets.at(a);
                for (std::size_t ri = 0; ri < resource_count; ++ri)
                {
                    if (!(tp.capacity[ri] > 0.0f) || reg.grid_goods().grid(ri))
                        continue;
                    if (!(ma.inventory[ri] > 0.0f) || !shelf_admits(ma, ri, res_mult, /*off_buys=*/true))
                        continue;
                    const float price_a = posted_price(ma, ri);
                    if (!(price_a > 0.0f))
                        continue;
                    for (const entity_id b : reach)
                    {
                        if (b == a)
                            continue;
                        const float price_b = dispatch_market_price(w.markets.at(b), ri);
                        // A gross price that cannot clear the margin cannot clear it net.
                        if (!(price_b - price_a > margin * price_a))
                            continue;
                        const float haul = haul_per_unit(corp, a, b, ri);
                        if (!std::isfinite(haul))
                            continue;
                        const float m = price_b - price_a - haul;
                        if (!(m > margin * price_a))
                            continue;
                        cands.push_back({m * tp.capacity[ri], a, b, ri, price_a + haul});
                    }
                }
            }
            std::sort(cands.begin(), cands.end(), [](const candidate& x, const candidate& y) {
                if (x.score != y.score) return x.score > y.score;
                if (x.a != y.a)         return x.a < y.a;
                if (x.b != y.b)         return x.b < y.b;
                return x.r < y.r;
            });
            for (const candidate& c : cands)
            {
                if (!(auto_pts > 1e-6f))
                    break;
                const float cap   = tp.capacity[c.r];
                const float shelf = std::max(0.0f, w.markets.at(c.a).inventory[c.r]);
                const float room  = trade_room(w, reg, c.b, c.r, c.landed);
                const float units = std::min({shelf, room, auto_pts * cap});
                if (!(units > 1e-4f))
                    continue;
                const float sent = ship(corp, c.a, c.b, c.r, units, /*is_auto=*/true);
                const float used = std::min(auto_pts, sent / cap);
                auto_pts        -= used;
                out.points_spent += used;
            }
        }
    }

    // BL-1222: roll the player's record into the lens's trailing window.
    w.player_trade_flow.push_back(std::move(tf));
    while (w.player_trade_flow.size() > world::trade_flow_window)
        w.player_trade_flow.erase(w.player_trade_flow.begin());

    return out;
}
