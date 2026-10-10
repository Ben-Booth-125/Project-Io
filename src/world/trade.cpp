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

std::map<entity_id, float> market_trade_points(const world& w, const recipe_registry& reg,
                                               const economy_report* report, entity_id corp)
{
    std::map<entity_id, float> out;
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return out;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end() || !trade_building_active(bit->second))
            continue;
        const entity_id mid = market_for_tile(w, bit->second.tile);
        if (mid == null_entity)
            continue; // no market under it: no shelf to trade from
        float contention = 1.0f;
        bool  met        = true;
        if (report != nullptr)
        {
            const entity_id body = building_body_of(w, bit->second);
            const auto cont = report->workforce_contention.find(std::make_pair(corp, body));
            if (cont != report->workforce_contention.end())
                contention = cont->second;
            met = report->upkeep_unmet.count(bid) == 0;
        }
        const float p = building_trade_points(reg, bit->second, contention, met);
        if (p > 0.0f)
            out[mid] += p;
    }
    return out;
}

float corp_trade_points(const world& w, const recipe_registry& reg,
                        const economy_report& report, entity_id corp)
{
    float total = 0.0f;
    for (const auto& [mid, p] : market_trade_points(w, reg, &report, corp))
        total += p; // ascending market id: a fixed summation order
    return total;
}

std::vector<entity_id> corp_trade_markets(const world& w, entity_id corp)
{
    std::vector<entity_id> out;
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return out;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end() || !trade_building_active(bit->second))
            continue;
        if (const entity_id mid = market_for_tile(w, bit->second.tile); mid != null_entity)
            out.push_back(mid);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
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
    if (!(std::isfinite(t.points) && t.points > 0.0f))
        return false;
    // Ben, 2026-10-10 (NR-1018): a manual trade obeys auto's reach — it leaves
    // from a market where the owner holds a trade building.
    const std::vector<entity_id> src = corp_trade_markets(w, t.owner);
    return std::binary_search(src.begin(), src.end(), t.from_market);
}

float trade_haul_per_unit(world& w, const recipe_registry& reg, const logistics_nodes& nodes,
                          trade_haul_memo& memo, entity_id corp, entity_id a, entity_id b,
                          std::size_t ri)
{
    const market_component& ma = w.markets.at(a);
    const market_component& mb = w.markets.at(b);
    if (ma.body == mb.body)
    {
        const auto key = std::make_pair(a, b);
        auto it = memo.intra.find(key);
        if (it == memo.intra.end())
        {
            const convoy_leg leg = price_market_leg(w, reg, nodes, a, b, 1.0f);
            it = memo.intra.emplace(key, leg.viable ? leg.cost : std::nanf("")).first;
        }
        return it->second;
    }
    const convoy_leg leg = price_trade_leg(w, reg, nodes, corp, a, b, ri, 1.0f);
    return leg.viable ? leg.cost : std::nanf("");
}

void rank_trade_routes(world& w, const recipe_registry& reg, const logistics_nodes& nodes,
                       trade_haul_memo& memo, entity_id corp,
                       const std::vector<entity_id>& sources, std::vector<trade_route_offer>& out)
{
    out.clear();
    const trade_params& tp = reg.trade();
    const float res_mult = reg.price_band().reservation_mult;
    const float margin   = reg.dispatch_margin();
    // Ben, 2026-10-10 (NR-1018): reach runs from market centre to market
    // centre — every market a leg reaches from the source's centre is a
    // destination (the leg prices and gates it: same body, overland or by
    // Ports; another body, the trader's pad and propellant). Ascending id.
    std::vector<entity_id> dests;
    dests.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mc;
        dests.push_back(mid);
    }
    std::sort(dests.begin(), dests.end());
    for (const entity_id a : sources)
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
            for (const entity_id b : dests)
            {
                if (b == a)
                    continue;
                const float price_b = dispatch_market_price(w.markets.at(b), ri);
                // A gross price that cannot clear the margin cannot clear it net.
                if (!(price_b - price_a > margin * price_a))
                    continue;
                const float haul = trade_haul_per_unit(w, reg, nodes, memo, corp, a, b, ri);
                if (!std::isfinite(haul))
                    continue;
                const float m = price_b - price_a - haul;
                if (!(m > margin * price_a))
                    continue;
                out.push_back({m * tp.capacity[ri], a, b, ri, price_a + haul, m});
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const trade_route_offer& x, const trade_route_offer& y) {
        if (x.score != y.score) return x.score > y.score;
        if (x.a != y.a)         return x.a < y.a;
        if (x.b != y.b)         return x.b < y.b;
        return x.r < y.r;
    });
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
    // Ben, 2026-10-10 (NR-1018): each trade building makes its points for the
    // MARKET it stands in, and they are spent on trades leaving that market.
    std::map<entity_id, std::map<entity_id, float>> points; // corp -> market -> points
    for (const entity_id corp : corp_ids)
    {
        std::map<entity_id, float> pm = market_trade_points(w, reg, &report, corp);
        float p = 0.0f;
        for (const auto& [mid, v] : pm)
            p += v;
        w.corporations.at(corp).trade_points = p;
        if (p > 0.0f)
        {
            points[corp] = std::move(pm);
            out.points_made += p;
            ++out.corps_trading;
        }
    }

    // BL-1222 (trade-flow lens): the PLAYER's shipments this pass. WRITE-ONLY.
    trade_flow_pass tf;
    tf.corp = w.player_entity;

    lp_pool_map local_pools;
    lp_pool_map* lp = (shared_lp_pools != nullptr) ? shared_lp_pools : &local_pools;
    if (!points.empty())
    {
        const logistics_nodes nodes = collect_logistics_nodes(w);

        // Same-body haul per unit, memoised per (source, destination) market for
        // the pass (`trade_haul_memo`).
        trade_haul_memo haul_memo;

        // Ship up to `units` of good `ri` from `a` to `b` for `corp`. Returns
        // the units actually sent.
        // Purchases each trader has committed this pass (billed at the clear):
        // the solvency gate weighs its balance less these (cold review).
        std::map<entity_id, float> committed;
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
                                       &refused_lp, &sent, &committed[corp]))
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

        std::vector<trade_route_offer> cands;

        for (auto& [corp, mpts] : points)
        {
            const corporation_component& cc = w.corporations.at(corp);
            float made = 0.0f;
            for (const auto& [mid, v] : mpts)
                made += v;
            // ONE reserve per corporation (Ben, 2026-10-10): the points held
            // back from auto for its manual trades, clamped to what it makes.
            float reserve_left = std::clamp(cc.trade_reserve, 0.0f, made);

            // --- MANUAL: the owner's trades, in placement order, spend the
            // reserve out of the points of the market each one leaves.
            for (std::size_t i = 0; i < w.trades.size() && reserve_left > 0.0f; ++i)
            {
                const standing_trade t = w.trades[i]; // a copy: shipping appends convoys, not trades
                if (t.owner != corp || !trade_is_valid(w, reg, t))
                    continue;
                const auto mit = mpts.find(t.from_market);
                if (mit == mpts.end() || !(mit->second > 0.0f))
                    continue; // its source made no points this tick
                const std::size_t ri = static_cast<std::size_t>(t.resource);
                const float pts  = std::min({t.points, reserve_left, mit->second});
                const float sent = ship(corp, t.from_market, t.to_market, ri, pts * tp.capacity[ri],
                                        /*is_auto=*/false);
                const float used = std::min(pts, sent / tp.capacity[ri]);
                reserve_left -= used;
                mit->second  -= used;
                out.points_spent += used;
            }

            // --- AUTO: what is left at each market, less the reserve manual
            // trades did not spend — held back from every market in proportion
            // to what it has left (ascending id), so a reserve stays reserved.
            float left_total = 0.0f;
            for (const auto& [mid, v] : mpts)
                left_total += std::max(0.0f, v);
            if (!(left_total > reserve_left))
                continue;
            const float keep = (left_total > 0.0f) ? (1.0f - reserve_left / left_total) : 0.0f;
            std::vector<entity_id> sources;
            for (auto& [mid, v] : mpts)
            {
                v = std::max(0.0f, v) * keep;
                if (v > 1e-6f)
                    sources.push_back(mid);
            }
            if (sources.empty())
                continue;
            rank_trade_routes(w, reg, nodes, haul_memo, corp, sources, cands);
            for (const trade_route_offer& c : cands)
            {
                float& auto_pts = mpts.at(c.a);
                if (!(auto_pts > 1e-6f))
                    continue;
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

    // What each market's anchor still admits once the pass is done — the
    // Logistic Points a further shipment leaving there could have used. Each
    // market writes only its own field, so the (unordered) walk's order is
    // immaterial; building a body's pool on first touch is deterministic.
    for (auto& [mid, mc] : w.markets)
        mc.trade_lp_spare = market_lp_left(w, reg, mid, *lp);

    // BL-1222: roll the player's record into the lens's trailing window.
    w.player_trade_flow.push_back(std::move(tf));
    while (w.player_trade_flow.size() > world::trade_flow_window)
        w.player_trade_flow.erase(w.player_trade_flow.begin());

    return out;
}
