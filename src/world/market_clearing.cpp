#include "market_clearing.hpp"

#include <algorithm>
#include <array>

namespace {

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

    // Recompute market supply/demand from this tick; price stays at base_price.
    for (auto& [mid, mc] : w.markets)
    {
        mc.supply.fill(0.0f);
        mc.demand.fill(0.0f);
    }

    // --- Supply: each corp lists its pool surplus above processor reservations ---
    // Visit pools in their (deterministic) map order.
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
        corp_cash_flow& flow = flows[corp];

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float base_price = mc.base_price[r];
            if (base_price <= 0.0f)
                continue; // untraded (e.g. regolith) — never listed

            const float surplus = pool.quantities[r] - reserve[r];
            if (surplus <= 0.0f)
                continue;

            pool.quantities[r] -= surplus;
            mc.supply[r]       += surplus;
            flow.income        += surplus * base_price;
        }
    }

    // --- Player sell orders (Layer 3 framework hook) ---
    // No orders are issued in Layer 3; the loop is the seam Layer 4 fills in.
    for (const sell_order& order : player_orders)
    {
        const auto mit = markets.find(order.body);
        if (mit == markets.end() || order.quantity <= 0.0f)
            continue;
        market_component& mc = w.markets.at(mit->second);
        const std::size_t r = static_cast<std::size_t>(order.resource);
        const float price = std::max(mc.base_price[r], order.floor_price);
        auto pkit = w.corp_body_pools.find(std::make_pair(order.corp, order.body));
        if (pkit == w.corp_body_pools.end())
            continue;
        const float sold = std::min(order.quantity, pkit->second.quantities[r]);
        pkit->second.quantities[r] -= sold;
        mc.supply[r]               += sold;
        flows[order.corp].income   += sold * price;
    }

    // --- Demand: processor input shortfalls auto-bought at base_price ---
    for (const auto& [key, bought] : report.purchases)
    {
        const entity_id corp = key.first;
        const entity_id body = key.second;

        const auto mit = markets.find(body);
        if (mit == markets.end())
            continue;

        market_component& mc = w.markets.at(mit->second);
        corp_cash_flow& flow = flows[corp];

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float qty = bought[r];
            if (qty <= 0.0f)
                continue;
            mc.demand[r]      += qty;
            flow.expenditure  += qty * mc.base_price[r];
        }
    }

    return flows;
}
