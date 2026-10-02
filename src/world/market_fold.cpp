// BL-1125 (markets can die) — see market_fold.hpp and MARKETS.md § Market
// centres and seeding.

#include "market_fold.hpp"

#include "market_clearing.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <utility>

namespace {

/// The body's markets in ascending id order — never the hash map's order.
std::vector<entity_id> body_markets_ascending(const world& w, entity_id body)
{
    std::vector<entity_id> ids;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body)
            ids.push_back(mid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

/// Body-wide market inventory, summed in ascending market id order.
double body_inventory_total(const world& w, entity_id body)
{
    double sum = 0.0;
    for (const entity_id mid : body_markets_ascending(w, body))
    {
        const market_component& mc = w.markets.at(mid);
        for (std::size_t r = 0; r < resource_count; ++r)
            sum += static_cast<double>(mc.inventory[r]);
    }
    return sum;
}

/// Every corporation's goods in pools keyed by a market on @p body (std::map
/// order: ascending (corp, key)).
double body_market_pool_total(const world& w, entity_id body)
{
    double sum = 0.0;
    for (const auto& [key, pool] : w.corp_market_pools)
    {
        const auto mit = w.markets.find(key.second);
        if (mit == w.markets.end() || mit->second.body != body)
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            sum += static_cast<double>(pool.quantities[r]);
    }
    return sum;
}

/// Population (thousands) of the body's centres, each routed with
/// `market_for_tile` — per market, keyed ascending.
std::map<entity_id, int64_t> catchment_population(const world& w, entity_id body)
{
    std::map<entity_id, int64_t> pop;
    std::vector<entity_id> centres;
    for (const auto& [cid, pcc] : w.population_centres)
        centres.push_back(cid);
    std::sort(centres.begin(), centres.end());
    for (const entity_id cid : centres)
    {
        const auto tit = w.population_centre_tile.find(cid);
        if (tit == w.population_centre_tile.end())
            continue;
        const auto tc = w.tiles.find(tit->second);
        if (tc == w.tiles.end() || tc->second.body != body)
            continue;
        const entity_id mid = market_for_tile(w, tit->second);
        if (mid == null_entity)
            continue;
        pop[mid] += w.population_centres.at(cid).population;
    }
    return pop;
}

int64_t total_of(const std::map<entity_id, int64_t>& m)
{
    int64_t t = 0;
    for (const auto& [k, v] : m) t += v;
    return t;
}

void open_tally(market_fold_tally& t, const world& w, entity_id body)
{
    t.inventory_before     = body_inventory_total(w, body);
    t.pools_before         = body_market_pool_total(w, body);
    t.catchment_pop_before = total_of(catchment_population(w, body));
}

void close_tally(market_fold_tally& t, const world& w, entity_id body)
{
    t.inventory_after     = body_inventory_total(w, body);
    t.pools_after         = body_market_pool_total(w, body);
    t.catchment_pop_after = total_of(catchment_population(w, body));
    t.folds               = static_cast<int>(t.records.size());
}

} // namespace

void fold_market_into(world& w, entity_id folded, entity_id into)
{
    if (folded == into)
        return;
    const auto fit = w.markets.find(folded);
    const auto iit = w.markets.find(into);
    if (fit == w.markets.end() || iit == w.markets.end())
        return;

    // The inventory: every unit the folded market held lands in the absorber.
    for (std::size_t r = 0; r < resource_count; ++r)
        iit->second.inventory[r] += fit->second.inventory[r];

    // The pools: (corp, folded) adds into (corp, into), ascending corp order.
    absorb_body_pool_into_market(w, folded, into);

    w.markets.erase(fit);
    // The per-body market index restamps on the count, which just moved; reset
    // it outright anyway so no read can see the folded id.
    w.body_market_index.clear();
    w.body_market_index_count = std::numeric_limits<std::size_t>::max();
}

market_fold_tally fold_twin_markets(world& w, entity_id body)
{
    market_fold_tally t;
    open_tally(t, w, body);

    // The first (lowest-id) market seen on each centre tile keeps it; every
    // later one on the same tile is its twin.
    std::map<entity_id, entity_id> keeper_of_tile;
    for (const entity_id mid : body_markets_ascending(w, body))
    {
        const entity_id centre = w.markets.at(mid).centre_tile;
        if (centre == null_entity)
            continue; // unanchored: never a twin
        const auto [it, fresh] = keeper_of_tile.emplace(centre, mid);
        if (fresh)
            continue;
        t.records.push_back(market_fold_record{ mid, it->second });
    }
    for (const market_fold_record& r : t.records)
        fold_market_into(w, r.folded, r.into);

    close_tally(t, w, body);
    return t;
}
