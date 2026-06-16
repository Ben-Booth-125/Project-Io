#include "supply_system.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

void advance_convoys(world& w)
{
    for (auto& convoy : w.convoys)
    {
        if (convoy.arrived)
            continue;
        convoy.progress += convoy.speed;
        if (convoy.progress >= 1.0f)
        {
            convoy.progress = 1.0f;
            convoy.arrived  = true;
        }
    }
}

void credit_arrived_convoys(world& w)
{
    // Credit in insertion order, then erase in one sweep.
    for (const auto& convoy : w.convoys)
    {
        if (!convoy.arrived)
            continue;

        // Find the destination market's body so we can credit the (corp, body) pool.
        const auto mit = w.markets.find(convoy.dest_market);
        if (mit == w.markets.end())
            continue;
        const entity_id dest_body = mit->second.body;

        // Credit the destination pool.
        w.pool_for(convoy.corp, dest_body).quantities[
            static_cast<std::size_t>(convoy.cargo_resource)] += convoy.cargo_qty;

        // Inject into market supply so the next clearing pass prices it.
        mit->second.supply[static_cast<std::size_t>(convoy.cargo_resource)] += convoy.cargo_qty;
    }

    // Retire arrived convoys.
    w.convoys.erase(
        std::remove_if(w.convoys.begin(), w.convoys.end(),
                       [](const convoy_component& c) { return c.arrived; }),
        w.convoys.end());
}

namespace {

/// Euclidean distance (AU) between two body entities using their current
/// orbital positions. Moons are approximated at their parent's position.
float body_distance_au(const world& w, entity_id a, entity_id b)
{
    if (a == b)
        return 0.0f;

    auto pos = [&](entity_id id) -> std::pair<float, float> {
        const auto it = w.bodies.find(id);
        if (it == w.bodies.end())
            return {0.0f, 0.0f};
        const body_component& bc = it->second;
        const float r = bc.orbital_radius_au;
        const float theta = bc.orbital_angle_rad;
        return { r * std::cos(theta), r * std::sin(theta) };
    };

    const auto [ax, ay] = pos(a);
    const auto [bx, by] = pos(b);
    const float dx = ax - bx;
    const float dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

/// True if `corp` has a launchpad building whose tile is on `body`.
bool corp_has_launchpad_on(const world& w, const corporation_component& corp, entity_id body)
{
    for (entity_id asset : corp.assets)
    {
        const auto bit = w.buildings.find(asset);
        if (bit == w.buildings.end())
            continue;
        if (bit->second.type != building_type::launchpad)
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            return true;
    }
    return false;
}

/// Find the market entity for a given body. Returns null_entity if none exists.
entity_id market_for_body(const world& w, entity_id body)
{
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body)
            return mid;
    return null_entity;
}

} // namespace

void dispatch_convoys(world& w, const recipe_registry& reg,
                      float logistics_cost_land, float logistics_cost_space)
{
    // One dispatch pass per (corp, dest_body, resource) shortfall.
    // Shortfall = market demand exceeded supply in the last clearing pass.
    // We fix quantities at a single batch = shortfall amount, capped by source surplus.

    for (auto& [corp_id, corp] : w.corporations)
    {
        // Iterate markets (not corp pools) so we catch shortfalls even on bodies
        // where the corp has no existing pool entry.
        for (const auto& [dest_market_id, dest_market] : w.markets)
        {
            const entity_id dest_body = dest_market.body;

            for (std::size_t ri = 0; ri < resource_count; ++ri)
            {
                const float shortfall = dest_market.demand[ri] - dest_market.supply[ri];
                if (shortfall <= 0.0f)
                    continue;

                // Find cheapest reachable source.
                entity_id best_src_body    = null_entity;
                entity_id best_src_market  = null_entity;
                float     best_cost        = std::numeric_limits<float>::max();
                float     best_qty         = 0.0f;
                convoy_mode best_mode      = convoy_mode::land;

                for (auto& [src_key, src_pool] : w.corp_body_pools)
                {
                    if (src_key.first != corp_id)
                        continue;
                    const entity_id src_body = src_key.second;
                    if (src_body == dest_body)
                        continue; // same body: no convoy needed

                    const float surplus = src_pool.quantities[ri];
                    if (surplus <= 0.0f)
                        continue;

                    const convoy_mode mode = convoy_mode::space;
                    if (!corp_has_launchpad_on(w, corp, src_body))
                        continue;

                    const float dist = body_distance_au(w, src_body, dest_body);
                    const float qty  = std::min(surplus, shortfall);
                    const float cost = logistics_cost_space * dist * qty;

                    if (cost < best_cost)
                    {
                        best_src_body   = src_body;
                        best_src_market = market_for_body(w, src_body);
                        best_cost       = cost;
                        best_qty        = qty;
                        best_mode       = mode;
                    }
                }

                if (best_src_body == null_entity)
                    continue;
                if (corp.balance < best_cost)
                    continue;

                // Debit cost and source pool; create the convoy.
                corp.balance -= best_cost;
                w.pool_for(corp_id, best_src_body).quantities[ri] -= best_qty;

                // Speed: 1 / distance in AU gives roughly 1 tick per AU. Clamp to > 0.
                const float dist = body_distance_au(w, best_src_body, dest_body);
                const float speed = (dist > 0.0f) ? (1.0f / dist) : 1.0f;

                convoy_component c;
                c.source_market  = best_src_market;
                c.dest_market    = dest_market_id;
                c.mode           = best_mode;
                c.cargo_resource = static_cast<resource_type>(ri);
                c.cargo_qty      = best_qty;
                c.progress       = 0.0f;
                c.speed          = speed;
                c.corp           = corp_id;
                c.arrived        = false;
                w.convoys.push_back(c);
            }
        }
    }
    (void)reg;
    (void)logistics_cost_land; // land-mode dispatch deferred (same-body shortfalls resolved by production)
}
