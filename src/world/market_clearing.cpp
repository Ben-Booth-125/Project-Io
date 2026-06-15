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

/// One cleared movement of a resource on a body, valued after prices resolve.
/// `floor` is the seller's minimum unit price (player sell orders); 0 for the
/// automatic surplus path and for purchases.
struct cleared_flow
{
    entity_id   corp;
    entity_id   body;
    std::size_t r;
    float       qty;
    float       floor;
};

/// Map each body that has a market to its market entity id. Layer 3 authors only
/// Kepler's market; bodies without one are skipped by clearing.
std::unordered_map<entity_id, entity_id> body_to_market(const world& w)
{
    std::unordered_map<entity_id, entity_id> map;
    map.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
        map[mc.body] = mid;
    return map;
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

} // namespace

std::unordered_map<entity_id, corp_cash_flow> clear_markets(
    world& w,
    const recipe_registry& reg,
    const economy_report& report,
    const std::vector<sell_order>& player_orders)
{
    std::unordered_map<entity_id, corp_cash_flow> flows;
    const auto markets = body_to_market(w);

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

        const auto mit = markets.find(body);
        if (mit == markets.end())
            continue; // body has no market — nothing clears here
        if (w.corporations.find(corp) == w.corporations.end())
            continue;

        market_component& mc = w.markets.at(mit->second);
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
            sales.push_back({corp, body, r, surplus, 0.0f});
        }
    }

    // --- Player sell orders (the manual market side) ---
    // Each standing order sells up to its quantity from the (corp, body) pool — the
    // auto path above yielded these resources — listed as supply and valued at
    // max(resolved price, floor) in the final pass.
    for (const sell_order& order : player_orders)
    {
        const auto mit = markets.find(order.body);
        if (mit == markets.end() || order.quantity <= 0.0f)
            continue;
        market_component& mc = w.markets.at(mit->second);
        const std::size_t r = static_cast<std::size_t>(order.resource);
        auto pkit = w.corp_body_pools.find(std::make_pair(order.corp, order.body));
        if (pkit == w.corp_body_pools.end())
            continue;
        const float sold = std::min(order.quantity, pkit->second.quantities[r]);
        if (sold <= 0.0f)
            continue;
        pkit->second.quantities[r] -= sold;
        mc.supply[r]               += sold;
        sales.push_back({order.corp, order.body, r, sold, order.floor_price});
    }

    // --- Demand: processor input shortfalls auto-bought this tick ---
    for (const auto& [key, bought] : report.purchases)
    {
        const entity_id corp = key.first;
        const entity_id body = key.second;

        const auto mit = markets.find(body);
        if (mit == markets.end())
            continue;

        market_component& mc = w.markets.at(mit->second);

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float qty = bought[r];
            if (qty <= 0.0f)
                continue;
            mc.demand[r] += qty;
            buys.push_back({corp, body, r, qty, 0.0f});
        }
    }

    // --- Resolve price from the now-known supply/demand, eased from the prior price ---
    for (auto& [mid, mc] : w.markets)
        for (std::size_t r = 0; r < resource_count; ++r)
            mc.price[r] = resolve_price(mc.price[r], mc.base_price[r], mc.supply[r], mc.demand[r]);

    // --- Value every movement at the resolved price (player orders honour their floor) ---
    for (const cleared_flow& s : sales)
    {
        const float price = w.markets.at(markets.at(s.body)).price[s.r];
        flows[s.corp].income += s.qty * std::max(price, s.floor);
    }
    for (const cleared_flow& b : buys)
    {
        const float price = w.markets.at(markets.at(b.body)).price[b.r];
        flows[b.corp].expenditure += b.qty * price;
    }

    return flows;
}
