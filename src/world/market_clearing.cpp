#include "market_clearing.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

// --- Price resolution constants (TODO § Trade, settled 2026-06-15) ---------
// Price is anchored to the rarity-derived base_price and pushed by this tick's
// supply/demand ratio. A damped (sqrt) elasticity keeps swings readable; the
// move is clamped to a band around base and eased across ticks so a single tick's
// imbalance cannot snap the price.
constexpr float price_floor_mult = 0.25f; ///< Lowest a price may fall: 0.25× base.
constexpr float price_ceil_mult  = 4.0f;  ///< Highest a price may rise: 4× base.
constexpr float price_smoothing  = 0.5f;  ///< EMA factor toward the tick's target price.

/// Resolve one resource's price for a market this tick. The target is
/// `base × sqrt(demand/supply)` (damped elasticity), clamped to the band and
/// reached by an exponential moving average from the prior price. Untraded
/// resources (`base <= 0`) keep their prior price.
float resolve_price(float prior, float base, float supply, float demand)
{
    if (base <= 0.0f)
        return prior; // never traded here — leave it (stays 0)

    float target;
    if (supply <= 0.0f && demand <= 0.0f)
        target = base; // no signal this tick — pull gently back toward base
    else if (supply <= 0.0f)
        target = base * price_ceil_mult; // demand with no supply — top of the band
    else
        target = base * std::sqrt(demand / supply); // demand 0 → target 0 → floored below

    const float lo = base * price_floor_mult;
    const float hi = base * price_ceil_mult;
    target = std::clamp(target, lo, hi);

    const float next = prior + price_smoothing * (target - prior);
    return std::clamp(next, lo, hi);
}

/// One cleared movement of a resource, valued after prices resolve. `market` is
/// the market the movement clears against (its catchment — § Trade, multiple
/// markets per body); `floor` is the seller's minimum unit price (player sell
/// orders), 0 for the automatic surplus path and for purchases.
struct cleared_flow
{
    entity_id   corp;
    entity_id   market;
    std::size_t r;
    float       qty;
    float       floor;
};

/// Markets present on each body, in ascending market-id order (deterministic).
std::unordered_map<entity_id, std::vector<entity_id>> markets_by_body(const world& w)
{
    std::unordered_map<entity_id, std::vector<entity_id>> map;
    map.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
        map[mc.body].push_back(mid);
    for (auto& [body, ids] : map)
        std::sort(ids.begin(), ids.end());
    return map;
}

/// Pick, from a body's markets, the one whose centre tile is nearest `tile`.
/// One market → that market (anchored or not). Several → the nearest anchored
/// centre by squared grid distance (ties → lowest id); an unanchored market is a
/// candidate only if every market on the body is unanchored, in which case the
/// lowest id wins.
entity_id nearest_market(const world& w, const std::vector<entity_id>& body_markets,
                         const tile_component& tile)
{
    if (body_markets.empty())
        return null_entity;
    if (body_markets.size() == 1)
        return body_markets.front();

    entity_id best      = null_entity;
    long long best_dist = 0;
    for (const entity_id mid : body_markets)
    {
        const entity_id centre = w.markets.at(mid).centre_tile;
        const auto cit = w.tiles.find(centre);
        if (cit == w.tiles.end())
            continue; // unanchored — skip while an anchored market exists
        const long long dx = cit->second.grid_x - tile.grid_x;
        const long long dy = cit->second.grid_y - tile.grid_y;
        const long long d  = dx * dx + dy * dy;
        if (best == null_entity || d < best_dist)
        {
            best      = mid;
            best_dist = d;
        }
    }
    return best != null_entity ? best : body_markets.front(); // all unanchored
}

/// Input reservation a corporation needs to keep on a body to feed a full run of
/// its own processors next tick — so it sells only the genuine surplus.
std::array<float, resource_count> processor_reservation(
    const world& w, const recipe_registry& reg, entity_id corp, entity_id body)
{
    std::array<float, resource_count> reserve = {};
    const corporation_component& cc = w.corporations.at(corp);
    const float batches_full_per_workforce =
        reg.economics(building_type::processing_facility).base_rate;

    for (const entity_id bid : cc.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        if (b.type != building_type::processing_facility)
            continue;

        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;

        const recipe* rcp = reg.get_recipe(b.recipe);
        if (!rcp)
            continue;

        const float batches = batches_full_per_workforce * b.workforce_assigned;
        for (std::size_t r = 0; r < resource_count; ++r)
            reserve[r] += rcp->inputs[r] * batches;
    }
    return reserve;
}

/// A corporation's representative tile on a body: the tile of its lowest-id
/// building there. Used to route the corp's body-aggregate supply/demand to a
/// market when the body carries several. `null_entity` if the corp holds nothing
/// on the body.
entity_id representative_tile(const world& w, entity_id corp, entity_id body)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return null_entity;

    entity_id best_building = null_entity;
    entity_id best_tile     = null_entity;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        if (best_building == null_entity || bid < best_building)
        {
            best_building = bid;
            best_tile     = bit->second.tile;
        }
    }
    return best_tile;
}

/// Route a corp's body-aggregate clearing to one market: the market nearest the
/// corp's representative tile on the body (one market → that market; no holdings
/// there → the body's lowest-id market). `null_entity` if the body has no market.
entity_id market_for_corp_on_body(
    const world& w, const std::unordered_map<entity_id, std::vector<entity_id>>& by_body,
    entity_id corp, entity_id body)
{
    const auto it = by_body.find(body);
    if (it == by_body.end() || it->second.empty())
        return null_entity;
    if (it->second.size() == 1)
        return it->second.front();

    const entity_id rep = representative_tile(w, corp, body);
    const auto tit = w.tiles.find(rep);
    if (tit == w.tiles.end())
        return it->second.front();
    return nearest_market(w, it->second, tit->second);
}

} // namespace

entity_id market_for_tile(const world& w, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return null_entity;
    const auto by_body = markets_by_body(w);
    const auto it = by_body.find(tit->second.body);
    if (it == by_body.end())
        return null_entity;
    return nearest_market(w, it->second, tit->second);
}

std::unordered_map<entity_id, corp_cash_flow> clear_markets(
    world& w,
    const recipe_registry& reg,
    const economy_report& report,
    const std::vector<sell_order>& player_orders)
{
    std::unordered_map<entity_id, corp_cash_flow> flows;
    const auto by_body = markets_by_body(w);

    // Recompute market supply/demand from scratch this tick. Quantities move now;
    // price resolves once supply and demand are known, and the cash value of every
    // movement is settled at that resolved price in a final pass.
    for (auto& [mid, mc] : w.markets)
    {
        mc.supply.fill(0.0f);
        mc.demand.fill(0.0f);
    }

    std::vector<cleared_flow> sales; // surplus + player orders (income, priced at max(price, floor))
    std::vector<cleared_flow> buys;  // auto-bought shortfalls (expenditure, priced at price)

    // A (corp, body, resource) the player has a standing sell order for is under
    // *manual* control: the auto-surplus path yields it so the player's order (and
    // its floor price) governs that resource's sale rather than the greedy auto path
    // dumping it at the market price first.
    auto player_controls = [&player_orders](entity_id corp, entity_id body, std::size_t r) {
        for (const sell_order& o : player_orders)
            if (o.corp == corp && o.body == body &&
                static_cast<std::size_t>(o.resource) == r && o.quantity > 0.0f)
                return true;
        return false;
    };

    // --- Supply: each corp lists its pool surplus above processor reservations ---
    // Visit pools in their (deterministic) map order; debit the pool now, value later.
    for (auto& [key, pool] : w.corp_body_pools)
    {
        const entity_id corp = key.first;
        const entity_id body = key.second;

        const entity_id mid = market_for_corp_on_body(w, by_body, corp, body);
        if (mid == null_entity)
            continue; // body has no market — nothing clears here
        if (w.corporations.find(corp) == w.corporations.end())
            continue;

        market_component& mc = w.markets.at(mid);
        const auto reserve = processor_reservation(w, reg, corp, body);

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (mc.base_price[r] <= 0.0f)
                continue; // untraded (e.g. regolith) — never listed
            if (player_controls(corp, body, r))
                continue; // a standing player order governs this resource's sale

            const float surplus = pool.quantities[r] - reserve[r];
            if (surplus <= 0.0f)
                continue;

            pool.quantities[r] -= surplus;
            mc.supply[r]       += surplus;
            sales.push_back({corp, mid, r, surplus, 0.0f});
        }
    }

    // --- Player sell orders (the manual market side) ---
    // Each standing order sells up to its quantity from the (corp, body) pool — the
    // auto path above yielded these resources — listed as supply and valued at
    // max(resolved price, floor) in the final pass.
    for (const sell_order& order : player_orders)
    {
        if (order.quantity <= 0.0f)
            continue;
        const entity_id mid = market_for_corp_on_body(w, by_body, order.corp, order.body);
        if (mid == null_entity)
            continue;
        market_component& mc = w.markets.at(mid);
        const std::size_t r = static_cast<std::size_t>(order.resource);
        auto pkit = w.corp_body_pools.find(std::make_pair(order.corp, order.body));
        if (pkit == w.corp_body_pools.end())
            continue;
        const float sold = std::min(order.quantity, pkit->second.quantities[r]);
        if (sold <= 0.0f)
            continue;
        pkit->second.quantities[r] -= sold;
        mc.supply[r]               += sold;
        sales.push_back({order.corp, mid, r, sold, order.floor_price});
    }

    // --- Demand: processor input shortfalls auto-bought this tick ---
    for (const auto& [key, bought] : report.purchases)
    {
        const entity_id corp = key.first;
        const entity_id body = key.second;

        const entity_id mid = market_for_corp_on_body(w, by_body, corp, body);
        if (mid == null_entity)
            continue;

        market_component& mc = w.markets.at(mid);

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float qty = bought[r];
            if (qty <= 0.0f)
                continue;
            mc.demand[r] += qty;
            buys.push_back({corp, mid, r, qty, 0.0f});
        }
    }

    // --- Resolve price from the now-known supply/demand, eased from the prior price ---
    for (auto& [mid, mc] : w.markets)
        for (std::size_t r = 0; r < resource_count; ++r)
            mc.price[r] = resolve_price(mc.price[r], mc.base_price[r], mc.supply[r], mc.demand[r]);

    // --- Value every movement at the resolved price (player orders honour their floor) ---
    for (const cleared_flow& s : sales)
    {
        const float price = w.markets.at(s.market).price[s.r];
        flows[s.corp].income += s.qty * std::max(price, s.floor);
    }
    for (const cleared_flow& b : buys)
    {
        const float price = w.markets.at(b.market).price[b.r];
        flows[b.corp].expenditure += b.qty * price;
    }

    return flows;
}
