// BL-1125 (markets can die) — see market_fold.hpp and MARKETS.md § Market
// centres and seeding.

#include "market_fold.hpp"

#include "logistics.hpp"
#include "market_clearing.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <utility>

namespace {

/// The body's standing markets in ascending id order — never the hash map's order.
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

/// Every corporation's goods in pools keyed by a market on @p body, standing or
/// folded (a pool left on a folded key would be stock the fold lost track of;
/// counting it keeps such a leak visible as a before/after match that hides
/// nothing). std::map order: ascending (corp, key).
double body_market_pool_total(const world& w, entity_id body)
{
    double sum = 0.0;
    for (const auto& [key, pool] : w.corp_market_pools)
    {
        const auto mit = w.markets.find(key.second);
        const auto fit = w.folded_markets.find(key.second);
        const bool on_body = (mit != w.markets.end() && mit->second.body == body)
                          || (fit != w.folded_markets.end() && fit->second.body == body);
        if (!on_body)
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

/// Every tile of the body, routed: raster order, null where the grid is empty.
std::vector<entity_id> route_every_tile(world& w, entity_id body)
{
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    std::vector<entity_id> route(grid.size(), null_entity);
    for (std::size_t i = 0; i < grid.size(); ++i)
        if (grid[i] != null_entity)
            route[i] = market_for_tile(w, grid[i]);
    return route;
}

struct pass_opening
{
    std::vector<entity_id> route;
};

pass_opening open_tally(market_fold_tally& t, world& w, entity_id body)
{
    t.inventory_before = body_inventory_total(w, body);
    t.pools_before     = body_market_pool_total(w, body);
    return pass_opening{ route_every_tile(w, body) };
}

void apply_and_close(market_fold_tally& t, world& w, entity_id body, const pass_opening& open)
{
    // Applied in (into, folded) order, so the float sums an absorber takes run
    // in one fixed order whatever order the walk discovered them in.
    std::sort(t.records.begin(), t.records.end(),
              [](const market_fold_record& a, const market_fold_record& b) {
                  if (a.into != b.into) return a.into < b.into;
                  return a.folded < b.folded;
              });
    std::map<entity_id, entity_id> into_of;
    for (const market_fold_record& r : t.records)
    {
        fold_market_into(w, r.folded, r.into);
        into_of[r.folded] = r.into;
        if (r.across_water) ++t.folds_across_water;
    }

    t.inventory_after = body_inventory_total(w, body);
    t.pools_after     = body_market_pool_total(w, body);
    t.folds           = static_cast<int>(t.records.size());

    // THE CATCHMENT CHECK: a tile that routed to a market this pass folded
    // must now route to that market's absorber; every other tile must route
    // where it did.
    const std::vector<entity_id> after = route_every_tile(w, body);
    for (std::size_t i = 0; i < after.size(); ++i)
    {
        entity_id expect = open.route[i];
        const auto it = into_of.find(expect);
        if (it != into_of.end())
        {
            expect = it->second;
            ++t.catchment_tiles_moved;
        }
        if (after[i] != expect)
            ++t.catchment_misrouted;
    }
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

    // The catchment: the routing record, and every record that named the
    // folded market as its absorber now names the market that absorbed it.
    for (auto& [fid, fm] : w.folded_markets)
        if (fm.into == folded)
            fm.into = into;
    w.folded_markets[folded] = folded_market{ fit->second.body, fit->second.centre_tile, into };

    w.markets.erase(fit);
    // The per-body indexes restamp on the count, which just moved; reset them
    // outright anyway so no read can see the folded id as standing.
    w.body_market_index.clear();
    w.body_folded_index.clear();
    w.body_market_sig.clear();
    w.body_route_index.clear();
    w.body_market_index_count = std::numeric_limits<std::size_t>::max();
}

market_fold_tally fold_twin_markets(world& w, entity_id body)
{
    market_fold_tally t;
    const pass_opening open = open_tally(t, w, body);

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
        t.records.push_back(market_fold_record{ mid, it->second, false });
    }

    apply_and_close(t, w, body, open);
    return t;
}

market_fold_tally fold_markets_by_gravity(world& w, entity_id body, float reach,
                                          const std::set<entity_id>* port_centres)
{
    market_fold_tally t;
    const pass_opening open = open_tally(t, w, body);
    if (!(reach > 0.0f))
    {
        apply_and_close(t, w, body, open);
        return t;
    }

    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    const std::map<entity_id, int64_t> pop = catchment_population(w, body);

    // Anchored markets only: an unanchored market has no place to stand in
    // anyone's reach, and no reach of its own.
    std::vector<entity_id> order;
    std::map<entity_id, entity_id> market_on_tile; // centre tile -> market (twins already folded)
    for (const entity_id mid : body_markets_ascending(w, body))
    {
        const entity_id centre = w.markets.at(mid).centre_tile;
        if (w.tiles.find(centre) == w.tiles.end())
            continue;
        order.push_back(mid);
        market_on_tile.emplace(centre, mid); // lowest id keeps a shared tile
    }
    const auto pop_of = [&](entity_id mid) -> int64_t {
        const auto it = pop.find(mid);
        return it != pop.end() ? it->second : 0;
    };
    // LARGEST FIRST: catchment population descending, ties to the lower id.
    // A total order, so the sort's result is unique.
    std::sort(order.begin(), order.end(), [&](entity_id a, entity_id b) {
        const int64_t pa = pop_of(a), pb = pop_of(b);
        if (pa != pb) return pa > pb;
        return a < b;
    });
    std::map<entity_id, std::size_t> rank;
    for (std::size_t k = 0; k < order.size(); ++k)
        rank.emplace(order[k], k);
    const auto has_port = [&](entity_id mid) -> bool {
        return port_centres == nullptr
            || port_centres->count(w.markets.at(mid).centre_tile) != 0;
    };

    std::map<entity_id, bool> folded;
    for (const entity_id m : order)
    {
        if (folded[m])
            continue; // absorbed by a larger market: it absorbs nothing
        const std::size_t m_rank = rank.at(m);
        const entity_id   centre = w.markets.at(m).centre_tile;

        // Over land, always; across water only from a ported absorber, and
        // only to a ported market (checked per target below).
        std::map<entity_id, bool> reached; // market -> reached by land
        for (const auto& [idx, cost] : bounded_cost_to_tile(w, body, centre, reach, /*land_only=*/true))
        {
            const auto it = market_on_tile.find(grid[static_cast<std::size_t>(idx)]);
            if (it != market_on_tile.end()) reached[it->second] = true;
        }
        if (has_port(m))
        {
            for (const auto& [idx, cost] : bounded_cost_to_tile(w, body, centre, reach, /*land_only=*/false))
            {
                const auto it = market_on_tile.find(grid[static_cast<std::size_t>(idx)]);
                if (it == market_on_tile.end() || reached.count(it->second) != 0)
                    continue;
                if (has_port(it->second))
                    reached[it->second] = false; // only across water
            }
        }
        for (const auto& [x, by_land] : reached) // ascending market id
        {
            if (x == m || folded[x] || rank.at(x) < m_rank)
                continue; // itself, already gone, or LARGER than m
            folded[x] = true;
            t.records.push_back(market_fold_record{ x, m, !by_land });
        }
    }

    apply_and_close(t, w, body, open);
    return t;
}
