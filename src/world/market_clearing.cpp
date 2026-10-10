#include "market_clearing.hpp"

#include "logistics.hpp" // BL-708: body_reach_field / tile_reach_cost — the grid good's listing gate

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <tuple>
#include <vector>

namespace {

// --- Price resolution constants (backlog.json § Trade, settled 2026-06-15) ---------
// Price is anchored to the rarity-derived base_price and pushed by this tick's
// supply/demand ratio. A damped (sqrt) elasticity keeps swings readable; the
// move is clamped to a band around base and eased across ticks so a single tick's
// imbalance cannot snap the price.
/// The band multipliers are NO LONGER constants here (BL-442): they are authored
/// once in scripts/economy.lua under `economy.price_band` and reach both this
/// function and economy_system.cpp's `wf_target_price` through
/// `recipe_registry::price_band()`. They arrive as parameters below.
constexpr float price_smoothing  = k_price_smoothing; ///< EMA factor toward the tick's target price (market_clearing.hpp).

} // namespace

float price_target(float base, float supply, float demand,
                   float price_floor_mult, float price_ceil_mult)
{
    float target;
    if (supply <= 0.0f && demand <= 0.0f)
        target = base; // no signal this tick — pull gently back toward base
    else if (supply <= 0.0f)
        target = base * price_ceil_mult; // demand with no supply — top of the band
    else
        target = base * std::sqrt(demand / supply); // demand 0 → target 0 → floored below

    const float lo = base * price_floor_mult;
    const float hi = base * price_ceil_mult;
    return std::clamp(target, lo, hi);
}

namespace {

/// Resolve one resource's price for a market this tick. The target is
/// `base × sqrt(demand/supply)` (damped elasticity), clamped to the band and
/// reached by an exponential moving average from the prior price. Untraded
/// resources (`base <= 0`) keep their prior price. The band multipliers come from
/// the registry (`price_band()`), not from a local constant — BL-442.
float resolve_price(float prior, float base, float supply, float demand,
                    float price_floor_mult, float price_ceil_mult)
{
    if (base <= 0.0f)
        return prior; // never traded here — leave it (stays 0)

    // BL-995: the unsmoothed target is its own exported function so dispatch
    // sizes a haul against the SAME price law clearing aims at.
    const float target = price_target(base, supply, demand, price_floor_mult, price_ceil_mult);
    const float lo = base * price_floor_mult;
    const float hi = base * price_ceil_mult;

    const float next = prior + price_smoothing * (target - prior);
    return std::clamp(next, lo, hi);
}

/// One side of an order-book entry for a single (market, resource) pair.
struct ob_sell_entry
{
    entity_id corp;
    float     qty;
    float     floor_price; ///< Minimum acceptable unit price (0 for auto-surplus).
    float     rem;         ///< Unmatched remainder: matching drains it, auto-clear sells it.
};

struct ob_buy_entry
{
    entity_id corp;
    float     qty;
    float     max_price;        ///< Maximum acceptable unit price (999 for auto-demand).
    entity_id preferred_seller; ///< Optional counterparty hint; null_entity = no preference.
};

/// One matched trade: both parties, quantities, and clearing price.
struct matched_trade
{
    entity_id seller;
    entity_id buyer;
    entity_id market;
    std::size_t r;
    float     qty;
    float     price; ///< Clearing price = seller's floor_price (ask).
};

/// Markets present on each body, in ascending market-id order (deterministic).
/// Served from world::body_market_index (BL-356), rebuilt when the stamp stops
/// matching the market set (markets are created, never destroyed).
///
/// BL-1079 (live tick speedups): THE STAMP IS O(1) — the market count plus the
/// entity-allocator cursor, `centres_by_body`'s stamp (budget_system.cpp). It was
/// count + max id, and deriving the max id walked every market on EVERY call:
/// dispatch reaches this through pool_key_for_tile once per asset per priced
/// leg, ~509M market visits a live tick on seed 31 (NR-915). A market is an
/// entity, so no market can be created without the cursor moving; the cursor
/// also moves on unrelated creations, which costs a rebuild, never a stale read.
const std::unordered_map<entity_id, std::vector<entity_id>>& markets_by_body(const world& w)
{
    const std::uint32_t cursor = w.next_entity_id();

    if (w.body_market_index_count != w.markets.size() ||
        w.body_market_index_cursor != cursor)
    {
        auto& map = w.body_market_index;
        map.clear();
        map.reserve(w.markets.size());
        for (const auto& [mid, mc] : w.markets)
            map[mc.body].push_back(mid);
        for (auto& [body, ids] : map)
            std::sort(ids.begin(), ids.end());
        // BL-1125: the folded markets, per body, ascending (std::map order).
        auto& folded = w.body_folded_index;
        folded.clear();
        for (const auto& [fid, fm] : w.folded_markets)
            folded[fm.body].push_back(fid);
        // BL-1125: what routing on each body reads, digested (FNV-1a over a
        // fixed order: standing ascending, then folded ascending), so the
        // catchment raster rebuilds only when routing itself could change.
        auto& sig = w.body_market_sig;
        sig.clear();
        const auto mix = [](std::uint64_t& h, std::uint64_t v) {
            for (int b = 0; b < 8; ++b) { h ^= (v >> (8 * b)) & 0xFFu; h *= 1099511628211ull; }
        };
        for (const auto& [body, ids] : map)
        {
            std::uint64_t h = 1469598103934665603ull;
            for (const entity_id mid : ids)
            {
                mix(h, mid);
                mix(h, w.markets.at(mid).centre_tile);
            }
            sig[body] = h;
        }
        for (const auto& [body, ids] : folded)
        {
            std::uint64_t h = sig.count(body) ? sig[body] : 1469598103934665603ull;
            mix(h, 0xF01DEDull);
            for (const entity_id fid : ids)
            {
                const folded_market& fm = w.folded_markets.at(fid);
                mix(h, fid);
                mix(h, fm.centre_tile);
                mix(h, fm.into);
            }
            sig[body] = h;
        }
        w.body_market_index_count  = w.markets.size();
        w.body_market_index_cursor = cursor;
    }
    return w.body_market_index;
}

/// Pick, from a body's markets, the one whose catchment holds `tile`.
/// One market and nothing folded → that market (anchored or not). Otherwise the
/// nearest ORIGINAL centre by squared grid distance, column wrapped — standing
/// markets and the body's folded ones alike (BL-1125), ties → lowest id — and a
/// folded winner hands the tile to the market that absorbed it, so a folded
/// market's catchment passes to its absorber whole (MARKETS.md § Market centres
/// and seeding). An unanchored market is a candidate only if every market on the
/// body is unanchored, in which case the lowest id wins.
entity_id nearest_market(const world& w, const std::vector<entity_id>& body_markets,
                         const tile_component& tile)
{
    if (body_markets.empty())
        return null_entity;
    static const std::vector<entity_id> k_none;
    const auto fit = w.body_folded_index.find(tile.body);
    const std::vector<entity_id>& folded = (fit != w.body_folded_index.end()) ? fit->second : k_none;
    if (body_markets.size() == 1 && folded.empty())
        return body_markets.front();

    // BL-1127: the surface is a cylinder, so the column distance wraps — the
    // same rule every other grid walk uses. Without it a tile by the seam
    // routed to a centre on the far side of the map.
    const auto bit = w.bodies.find(tile.body);
    const long long gw = (bit != w.bodies.end()) ? bit->second.grid_width : 0;

    entity_id best        = null_entity;
    bool      best_folded = false;
    long long best_dist   = 0;
    const auto consider = [&](entity_id id, entity_id centre, bool is_folded) {
        const auto cit = w.tiles.find(centre);
        if (cit == w.tiles.end())
            return; // unanchored — skip while an anchored market exists
        long long dx = cit->second.grid_x - tile.grid_x;
        if (dx < 0) dx = -dx;
        if (gw > 0 && dx > gw - dx) dx = gw - dx;
        const long long dy = cit->second.grid_y - tile.grid_y;
        const long long d  = dx * dx + dy * dy;
        // Strictly nearer, or equally near with a lower id: the two lists are
        // walked separately, so the id tie-break is explicit.
        if (best == null_entity || d < best_dist || (d == best_dist && id < best))
        {
            best        = id;
            best_folded = is_folded;
            best_dist   = d;
        }
    };
    for (const entity_id mid : body_markets)
        consider(mid, w.markets.at(mid).centre_tile, false);
    for (const entity_id fid : folded)
    {
        const folded_market& fm = w.folded_markets.at(fid);
        consider(fid, fm.centre_tile, true);
    }
    if (best == null_entity)
        return body_markets.front(); // all unanchored
    return best_folded ? w.folded_markets.at(best).into : best;
}

} // namespace

entity_id market_for_tile_scan(const world& w, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return null_entity;
    const auto& by_body = markets_by_body(w);
    const auto it = by_body.find(tit->second.body);
    if (it == by_body.end())
        return null_entity;
    return nearest_market(w, it->second, tit->second);
}

entity_id market_for_tile(const world& w, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return null_entity;
    const tile_component& tc = tit->second;
    const auto& by_body = markets_by_body(w);
    const auto it = by_body.find(tc.body);
    if (it == by_body.end())
        return null_entity;
    const std::vector<entity_id>& ms = it->second;
    const auto fit = w.body_folded_index.find(tc.body);
    const bool any_folded = fit != w.body_folded_index.end() && !fit->second.empty();
    if (ms.size() == 1 && !any_folded)
        return ms.front(); // one market, nothing folded: no raster needed

    // BL-1125: THE CATCHMENT RASTER, O(1) per call. Built by the very scan it
    // replaces (`nearest_market`, every tile of the body once) and rebuilt only
    // when the body's routing digest moves -- a market created or folded, a
    // centre or an absorber changed. The NR-915 hot path (pool_key_for_tile per
    // asset per priced leg) reads here.
    const auto sit = w.body_market_sig.find(tc.body);
    const std::uint64_t sig = (sit != w.body_market_sig.end()) ? sit->second : 0;
    body_route_cache& rc = w.body_route_index[tc.body];
    if (rc.route.empty() || rc.sig != sig)
    {
        const auto bit = w.bodies.find(tc.body);
        rc.gw = (bit != w.bodies.end()) ? bit->second.grid_width  : 0;
        rc.gh = (bit != w.bodies.end()) ? bit->second.grid_height : 0;
        rc.route.assign(static_cast<std::size_t>(std::max(0, rc.gw))
                            * static_cast<std::size_t>(std::max(0, rc.gh)), null_entity);
        // Each cell's answer depends only on its own tile, so the hash-map
        // walk order cannot reach the result.
        for (const auto& [tid, t] : w.tiles)
        {
            if (t.body != tc.body) continue;
            if (t.grid_x < 0 || t.grid_x >= rc.gw || t.grid_y < 0 || t.grid_y >= rc.gh) continue;
            rc.route[static_cast<std::size_t>(t.grid_y) * static_cast<std::size_t>(rc.gw)
                     + static_cast<std::size_t>(t.grid_x)] = nearest_market(w, ms, t);
        }
        rc.sig = sig;
    }
    if (tc.grid_x >= 0 && tc.grid_x < rc.gw && tc.grid_y >= 0 && tc.grid_y < rc.gh)
    {
        const entity_id r = rc.route[static_cast<std::size_t>(tc.grid_y) * static_cast<std::size_t>(rc.gw)
                                     + static_cast<std::size_t>(tc.grid_x)];
        if (r != null_entity)
            return r;
    }
    return nearest_market(w, ms, tc); // off-raster tile: the scan itself
}

// --- Shelf helpers (BL-1265, MARKETS.md § The shelf economy) -----------------

entity_id market_on_body(const world& w, entity_id body)
{
    const auto& by_body = markets_by_body(w);
    const auto it = by_body.find(body);
    return (it == by_body.end() || it->second.empty()) ? null_entity : it->second.front();
}

entity_id market_body(const world& w, entity_id market)
{
    const auto mit = w.markets.find(market);
    return mit == w.markets.end() ? null_entity : mit->second.body;
}

entity_id corp_home_market(const world& w, entity_id corp, entity_id body)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return market_on_body(w, body);
    const corporation_component& cc = cit->second;

    auto tile_on_body = [&](entity_id bid) -> entity_id {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            return null_entity;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit == w.tiles.end() || tit->second.body != body)
            return null_entity;
        return bit->second.tile;
    };

    if (const entity_id hq_tile = tile_on_body(cc.hq_building); hq_tile != null_entity)
        if (const entity_id mid = market_for_tile(w, hq_tile); mid != null_entity)
            return mid;

    entity_id best = null_entity;
    entity_id best_tile = null_entity;
    for (const entity_id bid : cc.assets)
    {
        const entity_id t = tile_on_body(bid);
        if (t != null_entity && (best == null_entity || bid < best))
        {
            best      = bid;
            best_tile = t;
        }
    }
    if (best_tile != null_entity)
        if (const entity_id mid = market_for_tile(w, best_tile); mid != null_entity)
            return mid;
    return market_on_body(w, body);
}

entity_id corp_hq_market(const world& w, entity_id corp)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return null_entity;
    const auto bit = w.buildings.find(cit->second.hq_building);
    if (bit == w.buildings.end())
        return null_entity;
    const auto tit = w.tiles.find(bit->second.tile);
    if (tit == w.tiles.end())
        return null_entity;
    return corp_home_market(w, corp, tit->second.body);
}

std::vector<entity_id> corp_markets(const world& w, entity_id corp)
{
    std::vector<entity_id> out;
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return out;
    for (const entity_id bid : cit->second.assets)
        if (const auto bit = w.buildings.find(bid); bit != w.buildings.end())
            if (const entity_id mid = market_for_tile(w, bit->second.tile); mid != null_entity)
                out.push_back(mid);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

float corp_shelf_stock(const world& w, entity_id corp, std::size_t r)
{
    if (r >= resource_count)
        return 0.0f;
    // Unpriced goods (base 0) are unbuyable, so they are not stock the
    // corporation could buy (cold review): only priced shelves count.
    float total = 0.0f;
    for (const entity_id mid : corp_markets(w, corp))
        if (w.markets.at(mid).base_price[r] > 0.0f)
            total += std::max(0.0f, w.markets.at(mid).inventory[r]);
    return total;
}

float buy_from_corp_shelves(world& w, entity_id corp, std::size_t r, float qty,
                            float reservation_mult)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end() || r >= resource_count || !(qty > 0.0f))
        return 0.0f;
    float bought = 0.0f;
    for (const entity_id mid : corp_markets(w, corp))
    {
        if (!(qty - bought > 0.0f))
            break;
        market_component& mc = w.markets.at(mid);
        if (!(mc.base_price[r] > 0.0f)) // unpriced == unbuyable, ceiling or not
            continue;
        if (!shelf_admits(mc, r, reservation_mult, /*off_buys=*/true))
            continue;
        const float take = std::min(qty - bought, std::max(0.0f, mc.inventory[r]));
        if (!(take > 0.0f))
            continue;
        const float px = posted_price(mc, r);
        cit->second.balance -= take * px;
        mc.inventory[r]     -= take;
        bought              += take;
        // The exchange record (BL-685): the market sells, the corporation buys.
        exchange_record e;
        e.tick       = w.current_econ_tick;
        e.market     = mid;
        e.resource   = static_cast<resource_type>(r);
        e.quantity   = take;
        e.unit_price = px;
        e.seller     = null_entity;
        e.buyer      = corp;
        w.exchanges.push(e);
    }
    return bought;
}

void seed_opening_stock(world& w, entity_id corp, const std::array<float, resource_count>& stock)
{
    stockpile_component& held = w.gen_opening_stock[corp];
    for (std::size_t r = 0; r < resource_count; ++r)
        held.quantities[r] += stock[r];
}

void place_opening_stock(world& w)
{
    // WORLD BUILD ONLY (CORPORATION_GENERATION.md § Pass 4b, Ben 2026-10-10):
    // the same total generation seeded, placed on the shelves of the markets
    // each corporation sits in, its share split over its own markets. The
    // split weights each market by how many of the corporation's buildings
    // its catchment holds — a firm with three works in one catchment and one
    // in another puts three quarters of its stock where three quarters of its
    // works are. Ascending corp id (std::map), ascending market id within.
    // No money moves: it is the market's stock from here on.
    for (const auto& [corp, held] : w.gen_opening_stock)
    {
        const auto cit = w.corporations.find(corp);
        if (cit == w.corporations.end())
            continue;
        std::map<entity_id, int> weight;
        for (const entity_id bid : cit->second.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            if (const entity_id mid = market_for_tile(w, bit->second.tile); mid != null_entity)
                ++weight[mid];
        }
        if (weight.empty())
            if (const entity_id mid = corp_hq_market(w, corp); mid != null_entity)
                weight[mid] = 1;
        int total = 0;
        for (const auto& [mid, n] : weight)
            total += n;
        if (total <= 0)
            continue; // no market anywhere: nothing to place it on
        auto& placed = w.gen_opening_placed[corp];
        for (const auto& [mid, n] : weight)
        {
            market_component& mc = w.markets.at(mid);
            const float share = static_cast<float>(n) / static_cast<float>(total);
            std::array<float, resource_count> put{};
            for (std::size_t r = 0; r < resource_count; ++r)
                if (held.quantities[r] > 0.0f)
                {
                    put[r] = held.quantities[r] * share;
                    mc.inventory[r] += put[r];
                }
            placed.emplace_back(mid, put);
        }
    }
    w.gen_opening_stock.clear();
}

void unplace_opening_stock(world& w, entity_id corp)
{
    const auto pit = w.gen_opening_placed.find(corp);
    if (pit == w.gen_opening_placed.end())
        return;
    stockpile_component& held = w.gen_opening_stock[corp];
    for (const auto& [mid, put] : pit->second)
    {
        const auto mit = w.markets.find(mid);
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (!(put[r] > 0.0f))
                continue;
            float take = put[r];
            if (mit != w.markets.end())
            {
                float& inv = mit->second.inventory[r];
                take = std::min(take, std::max(0.0f, inv));
                inv -= take;
            }
            held.quantities[r] += take;
        }
    }
    w.gen_opening_placed.erase(pit);
}

void inject_population_demand(world& w, const recipe_registry& reg)
{
    // BL-190: lives here (not run_economy_step) so it lands after
    // clear_markets' demand reset — injected earlier in the tick it was
    // erased before pricing (2026-07-31 ordering fix).
    //
    // BL-368: generalises the old single-resource (agricultural_produce, flat
    // 1/scale) stub into a real per-centre basket across the tradeable set,
    // price-elastic exactly like the nation-substrate model (BL-078) —
    // cheaper than base -> consume more, dearer -> less. Population is a pure
    // CONSUMER here: no supply term is added, unlike inject_substrate_demand.
    //
    // BL-640: the basket is ERA-BANDED, exactly as recipes are (MARKETS.md
    // § Demand channels, property 2). `population_demand_basket()` is the
    // registry's fold of the shared tranche plus every banded row the campaign's
    // band admits — an ancient household wants ceramics, cloth, leather and
    // dressed stone where an industrial one wants clean water, consumer goods
    // and medical supplies. Read it, NOT pd.demand_basket, which is the shared
    // tranche alone.
    const population_demand_params& pd = reg.population_demand();
    const std::array<float, resource_count>& basket = reg.population_demand_basket();

    // ASCENDING CENTRE ID (BL-1050). Many centres inject into ONE market's
    // `mc.demand[r] +=`, so this is a cross-centre float accumulation and float
    // addition does not associate: walked in `population_centres`' bucket order
    // the demand a market prices against would depend on that store's layout,
    // which a save/load rebuilds (world_save.cpp re-inserts in id order) and
    // another standard library lays out differently again. The order is now a
    // property of the ids alone.
    // BL-1196 (households consume): the household share of `demand` is kept on
    // its own register, `household_bid`, so the end-of-clear draw knows how much
    // the people want and the growth gate can read fill / bid. Zeroed on every
    // market first: a market no centre clears at bids nothing this tick.
    for (auto& [mid, mc] : w.markets)
    {
        (void)mid;
        mc.household_bid.fill(0.0f);
        mc.household_weight.fill(0.0f); // BL-1203: dispatch-only, same zeroing
    }

    std::vector<entity_id> centre_ids;
    centre_ids.reserve(w.population_centres.size());
    for (const auto& [cid, pcc] : w.population_centres)
    {
        (void)pcc;
        centre_ids.push_back(cid);
    }
    std::sort(centre_ids.begin(), centre_ids.end());

    for (const entity_id centre_id : centre_ids)
    {
        const population_centre_component& pcc = w.population_centres.at(centre_id);
        if (pcc.razed)
            continue; // BL-624 (razed settlement tier): a razed centre has no
                      // heads to feed — it injects no demand until re-settled.
        const auto tile_it = w.population_centre_tile.find(centre_id);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const entity_id mid = market_for_tile(w, tile_it->second);
        if (mid == null_entity)
            continue;
        market_component& mc = w.markets.at(mid);
        const float scale = static_cast<float>(pcc.scale) * pd.demand_scale;

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float base = mc.base_price[r];
            if (base <= 0.0f)
                continue; // Untradeable — no base price to anchor the elasticity
                          // curve. BL-652: this skip is SILENT by construction and
                          // must not be the only record of it — `unpriced_basket_entries`
                          // is the named diagnostic, reported at startup and failed
                          // on by demand_census.
            const float weighted = scale * basket[r];
            if (weighted <= 0.0f)
                continue;

            const float price   = (mc.price[r] > 0.0f) ? mc.price[r] : base;
            const float elastic = std::clamp(std::pow(base / price, pd.demand_elasticity),
                                             pd.elasticity_min, pd.elasticity_max);
            const float bid = weighted * elastic;
            mc.demand[r]        += bid;
            mc.household_bid[r] += bid; // same order, same addends: ascending centre id
            // BL-1203: the bid before its elastic factor, for the dispatcher to
            // re-read at a cargo's landed price. The price law never reads it.
            mc.household_weight[r] += weighted;
        }
    }
}

void draw_household_basket(world& w)
{
    // BL-1196 (households consume; POPULATION.md § Population demand: "a
    // population centre consumes a basket of goods drawn from the local
    // market"). The household bid stops being a pricing pull only: the people
    // TAKE what they bid off the market's shelf, `min(bid, inventory)` per good,
    // and that leaves the shelf for good — the Household channel's terminal
    // sink (MARKETS.md § Demand channels, property 4).
    //
    // MONEY: none moves. The shelf is the market's, and the market already paid
    // the maker for every unit on it when it bought the stock as buyer of last
    // resort (FINANCE.md § The money loop). Consumption moves goods, not credits.
    //
    // NO CEILING. A household draws whatever its bid names regardless of
    // `shelf_admits`: the fair-price ceiling is a PROCESSOR's reservation price
    // (BL-1172), and a household's reservation is already in its bid — the
    // price elasticity shrinks the bid as the price climbs.
    //
    // SEVERAL CENTRES, ONE MARKET: the bid is pooled (inject_population_demand
    // sums it in ascending centre id), so a short shelf fills every centre
    // there in the same share — pro rata, fill / bid. Each market's draw reads
    // and writes only its own arrays, but walk ascending market id and
    // ascending resource anyway so no hash order is ever one edit away.
    std::vector<entity_id> mids;
    mids.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mc;
        mids.push_back(mid);
    }
    std::sort(mids.begin(), mids.end());
    for (const entity_id mid : mids)
    {
        market_component& mc = w.markets.at(mid);
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float bid   = std::max(0.0f, mc.household_bid[r]);
            const float shelf = std::max(0.0f, mc.inventory[r]);
            const float take  = std::min(bid, shelf);
            mc.household_fill[r] = take;
            if (take > 0.0f)
                mc.inventory[r] = shelf - take;
        }
    }
}

void spoil_market_shelves(world& w, const recipe_registry& reg)
{
    // BL-1179 (MARKETS.md § The shelf spoils). The shelf is the market's,
    // bought as buyer of last resort, and nobody tends it: each good loses a
    // fixed share of what stands on it every tick. Goods leave; no credits move
    // (the market already paid the maker). Ascending market id and resource so
    // no hash order is ever one edit away, though each market's arithmetic
    // reads and writes only its own array.
    const std::array<float, resource_count>& rate = reg.shelf_spoilage();
    bool any = false;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (rate[r] > 0.0f) { any = true; break; }
    if (!any)
        return; // no rates authored: the pre-BL-1179 shelf, untouched
    std::vector<entity_id> mids;
    mids.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mc;
        mids.push_back(mid);
    }
    std::sort(mids.begin(), mids.end());
    for (const entity_id mid : mids)
    {
        market_component& mc = w.markets.at(mid);
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (!(rate[r] > 0.0f) || !(mc.inventory[r] > 0.0f))
                continue;
            mc.inventory[r] = std::max(0.0f, mc.inventory[r] - mc.inventory[r] * rate[r]);
        }
    }
}

std::vector<resource_type> unspoiled_priced_goods(const world& w, const recipe_registry& reg)
{
    std::array<bool, resource_count> priced{};
    for (const auto& [mid, mc] : w.markets) // a pure OR: the map's layout cannot reach it
    {
        (void)mid;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (mc.base_price[r] > 0.0f)
                priced[r] = true;
    }
    std::vector<resource_type> out;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (priced[r] && !(reg.shelf_spoilage()[r] > 0.0f))
            out.push_back(static_cast<resource_type>(r));
    return out;
}

float population_met_ratio(const world& w, const recipe_registry& reg, entity_id market, bool* recorded)
{
    if (recorded)
        *recorded = true;
    const auto mit = w.markets.find(market);
    if (mit == w.markets.end())
        return 1.0f;
    const market_component& mc = mit->second;
    // The SAME vector inject_population_demand multiplies by — never
    // pd.demand_basket (the shared tranche alone) and never a second list.
    const std::array<float, resource_count>& basket = reg.population_demand_basket();
    float acc = 0.0f, weight = 0.0f;
    bool  bids_something = false; // a priced basket good the households would bid on
    for (std::size_t r = 0; r < resource_count; ++r) // resource index ascending: fixed float order
    {
        const float bw = basket[r];
        if (bw <= 0.0f || mc.base_price[r] <= 0.0f)
            continue; // not in the bid (inject_population_demand's two skips)
        bids_something = true;
        const float bid = mc.household_bid[r];
        if (bid <= 0.0f)
            continue; // no clear has recorded the bid yet
        // BL-1196: what the households DREW off the shelf over what they bid.
        acc    += bw * std::min(1.0f, mc.household_fill[r] / bid);
        weight += bw;
    }
    // Review fix (household stack): a market that prices basket goods but has
    // recorded no bid on any of them has not been cleared since it was made --
    // its 1.0 is "no reading", not "fully met". Say so, so the growth gate
    // carries the centre's streak instead of counting a spurious met tick.
    if (recorded && bids_something && !(weight > 0.0f))
        *recorded = false;
    return (weight > 0.0f) ? acc / weight : 1.0f;
}

void inject_background_demand(world& w, const recipe_registry& reg)
{
    // BL-340/BL-365: the offstage economy's own pull on the mid-chain
    // processing goods, pooled per market catchment rather than bid per centre
    // (unlike inject_population_demand above) — real background firms alone
    // would under-consume these during the early game before enough of them
    // exist.
    //
    // BL-640: banded, NOT deleted. All five goods are industrial, so this pass
    // injects nothing in an ancient campaign — but it remains the stopgap
    // standing in for the Industry channel (BL-641) until that lands.
    const background_demand_params& bd = reg.background_demand();
    const std::array<float, resource_count>& basket = reg.background_demand_basket();

    // Per-MARKET population scale (BL-1226, background pull per doc): each
    // centre's scale goes to the ONE market whose catchment holds its tile —
    // `market_for_tile`, the attribution inject_population_demand uses, so the
    // household and background channels agree on which market a centre feeds.
    // MARKETS.md step 3: "A body's pull is SPLIT across its markets in
    // proportion to their catchment population, never granted whole to each".
    // The old per-body sum was applied whole to EVERY market on the body, so a
    // body carved into N markets bid N times its pull; the body total is now
    // conserved across its markets.
    //
    // ASCENDING CENTRE ID (BL-1050): `market_scale[mid] +=` is a float
    // accumulation. An ordered KEY orders the buckets, not the addends within
    // one, so the addends are walked in id order — a save/load rebuilds
    // `population_centres` in another layout. The membership filter is
    // order-free and runs unordered; only the accumulation is sorted.
    std::vector<entity_id> scale_centre_ids;
    scale_centre_ids.reserve(w.population_centres.size());
    for (const auto& [cid, pcc] : w.population_centres)
    {
        if (pcc.razed)
            continue; // BL-624: a razed centre has no heads — no pull, exactly
                      // as inject_population_demand skips it.
        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        if (w.tiles.find(tile_it->second) == w.tiles.end())
            continue;
        scale_centre_ids.push_back(cid);
    }
    std::sort(scale_centre_ids.begin(), scale_centre_ids.end());

    // BL-1217 lever D: the per-market bid record restarts every clear, on
    // every market (a market whose catchment holds no live centre bids nothing).
    for (auto& [mid, mc] : w.markets)
    {
        (void)mid;
        mc.background_bid.fill(0.0f);
    }

    std::map<entity_id, float> market_scale;
    for (const entity_id cid : scale_centre_ids)
    {
        const entity_id mid = market_for_tile(w, w.population_centre_tile.at(cid));
        if (mid == null_entity)
            continue;
        market_scale[mid] += static_cast<float>(w.population_centres.at(cid).scale);
    }

    for (auto& [mid, mc] : w.markets)
    {
        const auto sit = market_scale.find(mid);
        if (sit == market_scale.end() || sit->second <= 0.0f)
            continue;
        const float scale = sit->second * bd.demand_scale;

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float base = mc.base_price[r];
            if (base <= 0.0f)
                continue; // untradeable — no base price to anchor the elasticity
                          // curve. BL-652: named by `unpriced_basket_entries`, for
                          // the reason on inject_population_demand's copy of this line.
            const float weighted = scale * basket[r];
            if (weighted <= 0.0f)
                continue; // spacecraft_components (and anything else unlisted, or
                          // out of band) stays at 0.

            const float price   = (mc.price[r] > 0.0f) ? mc.price[r] : base;
            const float elastic = std::clamp(std::pow(base / price, bd.demand_elasticity),
                                             bd.elasticity_min, bd.elasticity_max);
            mc.demand[r] += weighted * elastic;
            mc.background_bid[r] = weighted * elastic; // one write per (market, good)
        }
    }
}

void draw_background_basket(world& w, const recipe_registry& reg)
{
    // BL-1217 lever D (behind economy.background_demand.consumes: authored TRUE
    // in scripts/economy.lua; the C++ struct default is false, so a hand-built
    // registry does not draw). The background basket is not a pricing pull only:
    // it TAKES what it bid off the market's shelf,
    // exactly as draw_household_basket does and on the same terms -- NO MONEY
    // MOVES (the market paid the maker when it bought the stock as buyer of
    // last resort), and NO CEILING (the bid's elasticity is its reservation).
    // One bid per (market, good), so a short shelf fills it pro rata trivially.
    // Ascending market id, ascending resource.
    //
    // BL-1217 G1b R3 (MARKETS.md step 3, re-ruled Ben 2026-10-09): the pull
    // draws AFTER the processors. It leaves one tick of the market's processor
    // want (`processor_want`, written by clear_markets this clear: posted want
    // plus the want the ceiling silenced, processors only) on the shelf and
    // draws only what stands above it: min(bid, max(0, shelf - want)).
    const bool consumes = reg.background_demand().consumes;
    std::vector<entity_id> mids;
    mids.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mc;
        mids.push_back(mid);
    }
    std::sort(mids.begin(), mids.end());
    for (const entity_id mid : mids)
    {
        market_component& mc = w.markets.at(mid);
        mc.background_fill.fill(0.0f);
        if (!consumes)
            continue; // switch off: the shelf is untouched (pre-lever behaviour)
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float bid   = std::max(0.0f, mc.background_bid[r]);
            const float shelf = std::max(0.0f, mc.inventory[r]);
            const float above = std::max(0.0f, shelf - std::max(0.0f, mc.processor_want[r]));
            const float take  = std::min(bid, above);
            mc.background_fill[r] = take;
            if (take > 0.0f)
                mc.inventory[r] = shelf - take;
        }
    }
}

namespace {

/// BL-647: the campaign-fixed preference weight of nation @p nation for the
/// resource at index @p r — the "national character" half of the endemic
/// channel, making different nations crave different luxuries.
///
/// A PURE FUNCTION, not stored state: FNV-1a over the nation's entity id, its
/// generated name and character axes (all fixed at generation, all already
/// persisted), and the resource index. Same inputs after a save/load, so the
/// craving is fixed for the campaign without touching the serialisation seam,
/// and no RNG stream runs in the tick loop. Folding the name and the three
/// character axes — not the id alone — is what seeds it from the nation's
/// GENERATED identity: two worlds whose phonology and settlement record differ
/// crave differently even where entity ids coincide.
///
/// Shape: uniform in [1 − spread, 1 + spread), mean 1.0 for every spread, so
/// the authored spread tunes ASYMMETRY without moving the channel's total.
float nation_preference(entity_id nation, const nation_component* nc,
                        std::size_t r, float spread)
{
    std::uint64_t h = 1469598103934665603ull; // FNV-1a 64 offset basis
    const auto mix_byte = [&h](std::uint8_t b) {
        h ^= b;
        h *= 1099511628211ull; // FNV-1a 64 prime
    };
    for (int i = 0; i < 8; ++i)
        mix_byte(static_cast<std::uint8_t>((static_cast<std::uint64_t>(nation) >> (8 * i)) & 0xFF));
    if (nc != nullptr)
    {
        for (const char ch : nc->name)
            mix_byte(static_cast<std::uint8_t>(ch));
        mix_byte(static_cast<std::uint8_t>(nc->politics));
        mix_byte(static_cast<std::uint8_t>(nc->posture));
        mix_byte(static_cast<std::uint8_t>(nc->focus));
    }
    mix_byte(static_cast<std::uint8_t>(r & 0xFF));
    mix_byte(static_cast<std::uint8_t>((r >> 8) & 0xFF));

    // Avalanche finalizer (MurmurHash3 fmix64). NOT decorative: raw FNV-1a
    // diffuses a late-mixed byte into the HIGH bits only weakly, and the
    // resource index is the LAST thing mixed — without this the top-24-bit
    // read below barely moved across goods, and every nation craved all four
    // luxuries near-identically (caught red by endemic_demand_harness E2).
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ull;
    h ^= h >> 33;

    // Top 24 bits → uniform [0, 1). Integer path is bit-identical everywhere.
    const float u = static_cast<float>((h >> 40) & 0xFFFFFFull) / 16777216.0f;
    return 1.0f - spread + 2.0f * spread * u;
}

} // namespace

void inject_endemic_demand(world& w, const recipe_registry& reg)
{
    // BL-647: the Endemic trade channel (MARKETS.md § Demand channels — the
    // register's eighth row). Tobacco, spices, coffee and furs were on the
    // roster, extractable, priced — and wanted by nothing in any band. This is
    // their buyer: a household pull that scales with WEALTH rather than
    // headcount, so it is the first demand whose SIZE is a function of
    // prosperity and it rewards a player who has made somewhere rich.
    const endemic_demand_params& ed = reg.endemic_demand();
    const std::array<float, resource_count>& basket = reg.endemic_demand_basket();
    if (ed.wealth_scale <= 0.0f)
        return; // authored off (the default) — hand-built registries and every
                // pre-BL-647 golden inject nothing.

    // --- Per-nation wealth -------------------------------------------------
    //
    // THE WEALTH READ IS A DELEGATED CALL (this agent's, BL-647): the nation's
    // treasury plus the summed POSITIVE balances of the corporations domiciled
    // in it (corporation_component::home_nation). Chosen over the alternatives
    // because it is the quantity that most directly moves with player success —
    // grow a profitable corp in a nation and that nation's luxury pull grows
    // the same quarter — while the treasury half carries the state's own
    // riches (levies, tariffs). Rejected: population-centre scale × a wealth
    // proxy (headcount-scaled, the exact thing this channel must not be) and
    // trailing quarterly-return averages (same signal, one quarter staler).
    // A corp in debt contributes zero rather than draining its neighbours'
    // riches — insolvency is not negative luxury appetite.
    //
    // Accumulated over SORTED corp ids so the float sum cannot vary with
    // `w.corporations`' unordered layout (the BL-406 class of defect).
    std::vector<entity_id> corp_ids;
    corp_ids.reserve(w.corporations.size());
    for (const auto& [cid, cc] : w.corporations)
    {
        (void)cc;
        corp_ids.push_back(cid);
    }
    std::sort(corp_ids.begin(), corp_ids.end());

    std::map<entity_id, float> nation_wealth; // nation id -> credits
    for (const entity_id cid : corp_ids)
    {
        const corporation_component& cc = w.corporations.at(cid);
        if (cc.home_nation == null_entity)
            continue;
        if (cc.balance > 0.0f)
            nation_wealth[cc.home_nation] += cc.balance;
    }
    for (const auto& [nid, nc] : w.nations)
        if (nc.treasury > 0.0f)
            nation_wealth[nid] += nc.treasury; // one add per key: order-free.

    // --- How many markets share each nation's pull -------------------------
    // A nation's craving is split evenly across the markets anchored in its
    // territory, so its TOTAL pull is independent of how many markets BL-096
    // carved it into. Integer increments: order-free over the unordered map.
    std::map<entity_id, int> nation_markets;
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mid;
        const auto it = w.tile_to_nation.find(mc.centre_tile);
        if (it != w.tile_to_nation.end() && it->second != null_entity)
            nation_markets[it->second] += 1;
    }

    for (auto& [mid, mc] : w.markets)
    {
        (void)mid;
        const auto nit = w.tile_to_nation.find(mc.centre_tile);
        if (nit == w.tile_to_nation.end() || nit->second == null_entity)
            continue; // no owning nation (an off-world outpost): no craving
                      // lands here — the home body's luxury shortfall reaches
                      // outposts through inject_interbody_demand instead.
        const entity_id nation = nit->second;
        const auto wit = nation_wealth.find(nation);
        if (wit == nation_wealth.end() || wit->second <= 0.0f)
            continue; // no wealth, no luxury pull — the channel's whole point.
        const float share =
            wit->second / static_cast<float>(nation_markets.at(nation));
        const auto nat_it = w.nations.find(nation);
        const nation_component* nc =
            (nat_it != w.nations.end()) ? &nat_it->second : nullptr;

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float base = mc.base_price[r];
            if (base <= 0.0f)
                continue; // Untradeable here. For an endemic good this skip is
                          // DESIGNED, not a BL-652 authoring fault: a world
                          // carries only the luxuries its biosphere rolled
                          // (RESOURCES.md § Mercantile), an absent one is
                          // priced nowhere, and its basket weight is inert on
                          // that world — which is why this channel is
                          // deliberately NOT added to unpriced_basket_entries.
            const float weighted = share * ed.wealth_scale * basket[r]
                * nation_preference(nation, nc, r, ed.preference_spread);
            if (weighted <= 0.0f)
                continue;

            // The population basket's elasticity shape, reused — not a second
            // elasticity model.
            const float price   = (mc.price[r] > 0.0f) ? mc.price[r] : base;
            const float elastic = std::clamp(std::pow(base / price, ed.demand_elasticity),
                                             ed.elasticity_min, ed.elasticity_max);
            mc.demand[r] += weighted * elastic;
        }
    }
}

std::vector<unpriced_basket_entry> unpriced_basket_entries(const world& w,
                                                           const recipe_registry& reg)
{
    // Does ANY market price it? A pure OR over an unordered map, so the map's
    // layout cannot reach the answer. Taken once rather than per basket entry.
    std::array<bool, resource_count> priced{};
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mid;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (mc.base_price[r] > 0.0f)
                priced[r] = true;
    }

    // The registry's ERA-RESOLVED folds — the exact vectors the two injectors
    // multiply by, not the shared `any` tranche on the params, so a band whose
    // basket never names the good is never accused of naming it.
    const std::array<float, resource_count>* baskets[2] = {
        &reg.population_demand_basket(), &reg.background_demand_basket()
    };
    const char* const channels[2] = { "household", "background" };

    std::vector<unpriced_basket_entry> out;
    for (std::size_t c = 0; c < 2; ++c)
        for (std::size_t r = 0; r < resource_count; ++r)
            if ((*baskets[c])[r] > 0.0f && !priced[r])
                out.push_back({ static_cast<resource_type>(r), channels[c] });
    return out;
}

namespace {

/// The lowest-id market on @p body, or `null_entity` if none — the same stable
/// pick `pool_key_for_body` makes for a body that has markets.
entity_id market_for_body(const world& w, entity_id body)
{
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body && (best == null_entity || mid < best))
            best = mid;
    return best;
}

/// Distance proxy (AU) between two bodies, for BL-263's opening-price markup and
/// inter-body demand pull — NOT the precise tick-pure angular distance
/// `supply_system.cpp`'s convoy routing uses (that drives real haul cost and
/// speed; this only shapes a price curve). A moon is approximated at its
/// parent's radius, matching that function's own moon approximation. Pure
/// function of generation-time orbital_radius_au — no tick dependency, so this
/// distance never changes across a campaign.
float body_radius_distance_au(const world& w, entity_id a, entity_id b)
{
    if (a == b)
        return 0.0f;
    auto radius = [&](entity_id id) -> float {
        const auto it = w.bodies.find(id);
        if (it == w.bodies.end())
            return 0.0f;
        const body_component* bc = &it->second;
        // Moon: approximate at its parent's orbital radius (mirrors
        // supply_system.cpp's body_distance_au moon handling).
        if (bc->parent != null_entity)
        {
            const auto pit = w.bodies.find(bc->parent);
            if (pit != w.bodies.end())
                bc = &pit->second;
        }
        return bc->orbital_radius_au;
    };
    return std::fabs(radius(a) - radius(b));
}

} // namespace

entity_id maybe_spawn_market(world& w, const recipe_registry& reg, entity_id body, entity_id tile)
{
    if (market_for_body(w, body) != null_entity)
        return null_entity; // one market per body off-world; already have one.

    const market_emergence_params& mp = reg.market_emergence();
    const entity_id home_market = (w.home_body != null_entity)
        ? market_for_body(w, w.home_body) : null_entity;

    market_component mc;
    mc.body        = body;
    mc.centre_tile = tile; // the completed building's own tile — the site that earned it.

    if (home_market != null_entity)
    {
        const market_component& hm = w.markets.at(home_market);
        const float dist = body_radius_distance_au(w, body, w.home_body);
        const float markup = 1.0f + mp.price_distance_gain * dist;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (hm.base_price[r] <= 0.0f)
                continue; // Untradeable at home stays untradeable here.
            mc.base_price[r] = hm.base_price[r] * markup;
        }
    }
    // Else: home body carries no market (a degenerate harness fixture) — the new
    // market opens all-zero base_price, the same safe "untradeable" fallback
    // every unauthored resource already gets; no crash, no special case needed.
    mc.price = mc.base_price; // start at the seeded opening price, same as generation.

    const entity_id mid = w.create_entity();
    w.markets[mid] = mc;
    return mid;
}

market_supply_snapshot snapshot_market_supply(const world& w)
{
    market_supply_snapshot snap;
    snap.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
        snap[mid] = mc.supply;
    return snap;
}

namespace {

/// The COUNTERPART home-body market for resource @p r: the home-body market
/// carrying the greatest demand for that resource, with the lowest market id
/// breaking ties (BL-406, Ben's ruling 2026-08-15, option c).
///
/// This replaces `market_for_body(w, w.home_body)` as the pull's source, and the
/// difference is the whole point of BL-406. That call returned the LOWEST-ID
/// market of the many BL-096 carves onto the home body and treated its demand as
/// the body's — a market holding 5% of the body's demand on the MSVC build and a
/// different market entirely on a g++ build of the same seed, because `w.markets`
/// is unordered and the lowest id is not a portable choice. Same-seed
/// reproducibility WITHIN a binary always held, so that was never a determinism
/// violation; it was a price input decided by an implementation accident.
///
/// The relation dissolves the defect rather than repairing it: no single market
/// is asked to stand for a body any more. An outpost selling resource r is pulled
/// by the home-body market that actually wants r most — a counterparty, not an
/// aggregate. Under today's model every home-body market sits at the same
/// distance from a given outpost, so the relation is many-to-one keyed on the
/// RESOURCE; the outpost dimension is carried entirely by the distance falloff.
/// If markets ever acquire a per-market haul cost, this is where a per-(outpost,
/// resource) counterpart would go.
///
/// Order-independent by construction — strictly-greater demand wins, and an exact
/// tie is broken by the smaller id — so every standard library's traversal of
/// `w.markets` names the same market. That property is asserted, not assumed:
/// `interbody_pull_harness` re-derives it over a reversed traversal.
entity_id counterpart_home_market(const world& w, std::size_t r)
{
    entity_id best        = null_entity;
    float     best_demand = 0.0f;
    for (const auto& [mid, mc] : w.markets)
    {
        if (mc.body != w.home_body)
            continue;
        const float d = mc.demand[r];
        if (best == null_entity || d > best_demand || (d == best_demand && mid < best))
        {
            best        = mid;
            best_demand = d;
        }
    }
    return best;
}

} // namespace

void inject_interbody_demand(world& w,
                             const recipe_registry& reg,
                             const market_supply_snapshot& prior_supply)
{
    if (w.home_body == null_entity)
        return;
    const market_emergence_params& mp = reg.market_emergence();

    // The counterpart per resource, resolved once — it is a property of the home
    // body this tick, not of the outpost being filled, so resolving it inside the
    // market loop would recompute the same answer for every outpost.
    std::array<entity_id, resource_count> counterpart{};
    counterpart.fill(null_entity);
    for (std::size_t r = 0; r < resource_count; ++r)
        counterpart[r] = counterpart_home_market(w, r);

    for (auto& [mid, mc] : w.markets)
    {
        if (mc.body == w.home_body)
            continue; // The home body pulls demand onto outposts, not itself.

        const float dist   = body_radius_distance_au(w, mc.body, w.home_body);
        const float falloff = 1.0f + mp.distance_falloff * dist;

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const entity_id src = counterpart[r];
            if (src == null_entity)
                continue; // No market on the home body at all.
            const market_component& cm = w.markets.at(src);

            // THE SUBTRACTION IS REAL NOW (BL-404, option b). It was a no-op for
            // as long as it read `cm.supply` directly: `clear_markets` zeroes
            // every market's supply immediately above this call and the supply
            // writes land after it, so the subtrahend was identically 0.0f and
            // every outpost received pull_fraction of GROSS home demand. The fix
            // is not to reorder a pass whose ordering is already load-bearing
            // (the supply writes are themselves demand-sensitive) but to net
            // against the counterpart's END-OF-TICK supply from the PREVIOUS
            // tick, captured by snapshot_market_supply before the reset loop.
            // One tick of lag, deterministic, and honest about what it is.
            const auto it = prior_supply.find(src);
            const float supply_last_tick =
                (it != prior_supply.end()) ? it->second[r] : 0.0f;

            const float shortfall = cm.demand[r] - supply_last_tick;
            if (shortfall <= 0.0f)
                continue; // The counterpart already meets its own appetite.
            if (mc.base_price[r] <= 0.0f)
                continue; // Untradeable here regardless of what's wanted at home.
            mc.demand[r] += shortfall * mp.pull_fraction / falloff;
        }
    }
}

std::uint32_t tile_feed_power_grid(world& w, entity_id tile)
{
    const entity_id mid = market_for_tile(w, tile);
    if (mid == null_entity)
        return 0;
    const auto mit = w.markets.find(mid);
    if (mit == w.markets.end() || mit->second.centre_tile == null_entity)
        return 0;
    return tile_power_grid(w, mit->second.centre_tile);
}

grid_good_pool pool_grid_good_figures(world& w, const recipe_registry& reg)
{
    grid_good_pool out;
    const grid_goods_params& grid_rules = reg.grid_goods();
    if (!grid_rules.any())
        return out;
    std::vector<entity_id> mids;
    mids.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mc;
        mids.push_back(mid);
    }
    std::sort(mids.begin(), mids.end());
    for (const entity_id mid : mids)
    {
        const market_component& mc = w.markets.at(mid);
        if (mc.centre_tile == null_entity)
            continue;
        const std::uint32_t g = tile_power_grid(w, mc.centre_tile);
        if (g == 0)
            continue;
        out.market_grid.emplace(mid, g);
        auto& sd = out.grid_sd[g];
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (!grid_rules.grid(r) || !grid_good_crosses_markets(r))
                continue;
            sd.listed[r] += std::max(0.0f, mc.supply[r]);
            sd.shelf[r]  += std::max(0.0f, mc.inventory[r]);
            sd.wants[r]  += std::max(0.0f, mc.demand[r]) + std::max(0.0f, mc.hauler_want[r]);
            sd.demand[r] += mc.demand[r];
        }
    }
    return out;
}

std::unordered_map<entity_id, corp_cash_flow> clear_markets(
    world& w,
    const recipe_registry& reg,
    const economy_report& report)
{
    std::unordered_map<entity_id, corp_cash_flow> flows;

    // --- The exchange record (BL-685) ---------------------------------------
    //
    // One row per exchange, appended to `w.exchanges` at each of the TWO points
    // below where this pass moves goods for money — a landing sold to the
    // market, and a shelf draw bought from it. Written beside the cash-flow
    // accrual it belongs to, so the record cannot disagree with the money loop.
    // Authority: docs/economy/MARKETS.md § The exchange record.
    //
    // `unit_price` IS THE PRICE THE EXCHANGE WAS MADE AT: the clearing price
    // (`ref_price`) for a landing — the market is the counterparty — and the
    // POSTED price for a shelf draw (BL-1172, `posted_price`).
    //
    // DETERMINISM. Both sites walk a std::map (`world::landed_this_tick`,
    // `economy_report::purchases`) with the resource index ascending inside each
    // row. `world::markets` is an unordered_map and is NEVER the thing walked to
    // emit a row. A ZERO-QUANTITY row is not an exchange and is dropped.
    auto record_exchange = [&w](entity_id market, std::size_t r, float qty, float price,
                                entity_id seller, entity_id buyer) {
        if (qty <= 0.0f)
            return;
        exchange_record e;
        e.tick       = w.current_econ_tick;
        e.market     = market;
        e.resource   = static_cast<resource_type>(r);
        e.quantity   = qty;
        e.unit_price = price;
        e.seller     = seller;
        e.buyer      = buyer;
        w.exchanges.push(e);
    };

    // Captured BEFORE the reset below, so it carries the PREVIOUS tick's
    // end-of-tick supply — the only honest subtrahend available to
    // inject_interbody_demand, which runs before this tick's supply is written
    // (BL-404). Local to this pass: nothing is persisted.
    const market_supply_snapshot prior_supply = snapshot_market_supply(w);

    for (auto& [mid, mc] : w.markets)
    {
        mc.supply.fill(0.0f);
        mc.demand.fill(0.0f);
    }

    // Population food demand (BL-190) — additive after the reset above, so the
    // population's pull reaches price resolution.
    inject_population_demand(w, reg);

    // BL-340/BL-365: background-industrial demand for the mid-chain processing
    // goods, additive alongside population demand. See inject_background_demand.
    inject_background_demand(w, reg);

    // BL-647: endemic-luxury demand — a wealth-scaled, character-flavoured
    // pull for the endemic goods, additive alongside the two above. See
    // inject_endemic_demand.
    inject_endemic_demand(w, reg);

    // BL-263: the home body's own unmet demand pulls a discounted slice onto
    // every outpost market, additive after the resets above. Runs AFTER the
    // three demand injections above, because BL-406's counterpart selection
    // reads the demand they deposit.
    inject_interbody_demand(w, reg, prior_supply);

    // --- LANDINGS: this tick's supply (BL-1265; MARKETS.md § The shelf economy)
    //
    // Everything that landed this tick — a building's output, a trade's cargo,
    // a captured cargo (`world::landed_this_tick`) — is this tick's supply on
    // the market it landed on, and is SOLD there on landing: the market is the
    // counterparty, bid or no bid (Ben, 2026-10-10). Listed here, before the
    // reference prices, so the landing moves the price it is paid at; paid and
    // shelved below, once the prices stand. A landing on a market that has
    // gone (folded or erased under it) has nowhere to sell and is dropped with
    // its row — nothing is paid for goods no shelf received. An unpriced good
    // (base 0) still lands on the shelf; it is paid nothing, since no price
    // clears it.
    for (const auto& [key, landed] : w.landed_this_tick)
    {
        const auto mkit = w.markets.find(key.second);
        if (mkit == w.markets.end())
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (landed.quantities[r] > 0.0f)
                mkit->second.supply[r] += landed.quantities[r];
    }

    // BL-441 — the demand register reads the WANT, and only the want.
    //
    // `report.wants` carries what each consumer set out to buy whether or not
    // the draw succeeded, which is the number a price is supposed to answer.
    // Nothing is PAID against this loop. std::map: a sorted accumulation. Keyed
    // (buyer, market) — the market the draw happened in.
    for (const auto& [key, wanted] : report.wants)
    {
        const auto mkit = w.markets.find(key.second);
        if (mkit == w.markets.end())
            continue;

        for (std::size_t r = 0; r < resource_count; ++r)
            if (wanted[r] > 0.0f)
                mkit->second.demand[r] += wanted[r];
    }

    // BL-1217 G1b R3 (MARKETS.md step 3, "the pull draws after the processors",
    // Ben 2026-10-09): the PROCESSOR part of the demand just posted, per market —
    // one tick of processor want, which draw_background_basket leaves on the
    // shelf. Rewritten whole every clear, on every market; std::map: a sorted
    // accumulation (ascending corp within a market).
    for (auto& [mid, mc] : w.markets)
    {
        (void)mid;
        mc.processor_want.fill(0.0f);
    }
    for (const auto& [key, wanted] : report.processor_wants)
    {
        const auto mkit = w.markets.find(key.second);
        if (mkit == w.markets.end())
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (wanted[r] > 0.0f)
                mkit->second.processor_want[r] += wanted[r];
    }

    // BL-1203: the want the fair-price ceiling silenced — copied to the market,
    // NOT into `demand`: it never bids and is never paid against (BL-1172). The
    // price law reads it in ONE place only, the cap on the shelf's share
    // (`pricing_supply`), below. Rewritten whole every clear. std::map: sorted.
    for (auto& [mid, mc] : w.markets)
    {
        (void)mid;
        mc.hauler_want.fill(0.0f);
    }
    for (const auto& [key, suppressed] : report.hauler_wants)
    {
        const auto mkit = w.markets.find(key.second);
        if (mkit == w.markets.end())
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (suppressed[r] > 0.0f)
                mkit->second.hauler_want[r] += suppressed[r];
    }

    // --- Reference prices from accumulated supply/demand ---
    // Computed once, before any clearing, so every sale this tick uses the same
    // price. BL-1172: supply here is this tick's landings PLUS the shelf's share,
    // at most k ticks of this tick's demand (`pricing_supply`), read now — after
    // the tick's draws, before the landings below move onto the shelf.
    //
    // BL-1230 (power crosses markets; LOGISTICS.md § 3a): POWER's price clears
    // against its GRID — each market on a grid resolves the grid good against
    // the grid's pooled figures (`pool_grid_good_figures`), from its OWN prior
    // price and base. Sums run over ascending market id.
    const grid_goods_params& grid_rules = reg.grid_goods();
    const bool               any_grid   = grid_rules.any();
    const grid_good_pool gpool = any_grid ? pool_grid_good_figures(w, reg) : grid_good_pool{};
    const auto& market_grid = gpool.market_grid;
    const auto& grid_sd     = gpool.grid_sd;
    std::unordered_map<entity_id, std::array<float, resource_count>> ref_price;
    for (const auto& [mid, mc] : w.markets)
    {
        ref_price[mid] = {};
        const auto mg = market_grid.find(mid);
        const auto* sd = (mg != market_grid.end()) ? &grid_sd.at(mg->second) : nullptr;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const bool  pooled = (sd != nullptr) && grid_rules.grid(r) && grid_good_crosses_markets(r);
            const float k      = reg.price_band().shelf_supply_ticks;
            const float supply = pooled ? grid_good_pricing_supply(*sd, r, k)
                                        : pricing_supply(mc, r, k);
            const float demand = pooled ? sd->demand[r] : mc.demand[r];
            ref_price[mid][r] = resolve_price(mc.price[r], mc.base_price[r], supply, demand,
                                              reg.price_band().floor_mult,
                                              reg.price_band().ceil_mult);
        }
    }

    // --- Landings sold: income at the clearing price, goods onto the shelf ---
    // The market is the BUYER here and has no corp behind it — `null_entity` on
    // that side of the exchange row means the market itself. The owner is
    // paid the quantity at `ref_price`, this tick's clearing price, and the
    // goods move onto the shelf in the same statement, so the shelf gains
    // exactly what was paid for. A landing whose owner is not a corporation
    // (no corp behind a cargo) shelves the goods and pays nobody.
    for (const auto& [key, landed] : w.landed_this_tick)
    {
        const entity_id owner = key.first;
        const entity_id mid   = key.second;
        const auto mkit = w.markets.find(mid);
        if (mkit == w.markets.end())
            continue;
        const bool paid = w.corporations.find(owner) != w.corporations.end();
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float qty = landed.quantities[r];
            if (!(qty > 0.0f))
                continue;
            mkit->second.inventory[r] += qty;
            if (!paid)
                continue;
            const float px = ref_price[mid][r];
            flows[owner].income += qty * px;
            record_exchange(mid, r, qty, px, owner, null_entity);
        }
    }
    w.landed_this_tick.clear();

    // --- Shelf draws billed at the POSTED price ---
    // BL-1172 (FINANCE.md § Standing-force upkeep, Ben 2026-10-03): "a draw from
    // a market's shelf is decided and billed at the price that stood when it was
    // made — the price it checked against the ceiling — with one exchange row at
    // that price". Every fill here is a quantity a draw took off a market's
    // shelf this tick — upkeep, processor inputs, construction, a trade's
    // purchase — each capped by what the shelf held, and each checked
    // `posted_price` against the fair-price ceiling before drawing
    // (`shelf_admits`). `market_component::price` is not written until the end
    // of this pass, so `posted_price` read here IS the price each draw saw.
    // The market is the SELLER; whoever stocked the shelf was paid on landing.
    // std::map: a sorted walk.
    for (const auto& [key, bought] : report.purchases)
    {
        const entity_id buyer = key.first;
        const auto mkit = w.markets.find(key.second);
        if (mkit == w.markets.end())
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float qty = bought[r];
            if (qty <= 0.0f)
                continue;
            const float px = posted_price(mkit->second, r);
            flows[buyer].expenditure += qty * px;
            record_exchange(key.second, r, qty, px, null_entity, buyer);
        }
    }

    // BL-1196 (households consume): the people take their basket off the shelf,
    // AFTER every landing this tick has stocked it — so a unit made this tick
    // can feed a household this tick — and after every price the clear bills at
    // was fixed. The nation's network upkeep and space programme draw LATER in
    // the tick from what households leave (Ben, 2026-10-05: households come
    // before the nation — MARKETS.md step 12).
    draw_household_basket(w);

    // BL-1217 lever D: the background basket draws what the households left,
    // before spoilage and before the nation's later claims, leaving one tick of
    // processor want (BL-1217 G1b R3).
    draw_background_basket(w, reg);

    // BL-1179 (shelf spoilage): after the households' draw and before the next
    // tick's reference prices read the shelf's share of supply. Goods leave; no
    // credits move.
    spoil_market_shelves(w, reg);

    // --- Price update: the clearing price stands -------------------------------
    // The order book's VWAP signal retired with the book (BL-1265): every
    // exchange is with the market, at the price the supply/demand state set.
    for (auto& [mid, mc] : w.markets)
        mc.price = ref_price[mid];

    return flows;
}
