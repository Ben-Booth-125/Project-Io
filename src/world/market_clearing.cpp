#include "market_clearing.hpp"

#include "law.hpp" // the D4 import tariff: any_import_tariff_enacted / nation_tariff_rate

#include <algorithm>
#include <array>
#include <cmath>
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
constexpr float price_smoothing  = 0.5f;  ///< EMA factor toward the tick's target price.

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
/// Served from world::body_market_index (BL-356), rebuilt when the count + max-id
/// stamp stops matching the market set (markets are created, never destroyed).
const std::unordered_map<entity_id, std::vector<entity_id>>& markets_by_body(const world& w)
{
    entity_id max_id = null_entity;
    for (const auto& [mid, mc] : w.markets)
        max_id = std::max(max_id, mid);

    if (w.body_market_index_count != w.markets.size() ||
        w.body_market_index_max_id != max_id)
    {
        auto& map = w.body_market_index;
        map.clear();
        map.reserve(w.markets.size());
        for (const auto& [mid, mc] : w.markets)
            map[mc.body].push_back(mid);
        for (auto& [body, ids] : map)
            std::sort(ids.begin(), ids.end());
        w.body_market_index_count  = w.markets.size();
        w.body_market_index_max_id = max_id;
    }
    return w.body_market_index;
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
    const auto& by_body = markets_by_body(w);
    const auto it = by_body.find(tit->second.body);
    if (it == by_body.end())
        return null_entity;
    return nearest_market(w, it->second, tit->second);
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

    for (const auto& [centre_id, pcc] : w.population_centres)
    {
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
            mc.demand[r] += weighted * elastic;
        }
    }
}

void inject_background_demand(world& w, const recipe_registry& reg)
{
    // BL-340/BL-365: the offstage economy's own pull on the mid-chain
    // processing goods, world-scale rather than per-centre (unlike
    // inject_population_demand above) — real background firms alone would
    // under-consume these during the early game before enough of them exist.
    //
    // BL-640: banded, NOT deleted. All six goods are industrial, so this pass
    // injects nothing in an ancient campaign — but it remains the stopgap
    // standing in for the Industry channel (BL-641) until that lands.
    const background_demand_params& bd = reg.background_demand();
    const std::array<float, resource_count>& basket = reg.background_demand_basket();

    // Per-body population scale: sum of every centre's scale on that body,
    // gathered once so every market on a multi-market body (BL-096) sees the
    // same pull. std::map keyed by entity_id → deterministic accumulation
    // order regardless of population_centres' unordered_map layout.
    std::map<entity_id, float> body_scale;
    for (const auto& [cid, pcc] : w.population_centres)
    {
        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto tit = w.tiles.find(tile_it->second);
        if (tit == w.tiles.end())
            continue;
        body_scale[tit->second.body] += static_cast<float>(pcc.scale);
    }

    for (auto& [mid, mc] : w.markets)
    {
        const auto sit = body_scale.find(mc.body);
        if (sit == body_scale.end() || sit->second <= 0.0f)
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
/// pick `supply_system.cpp`'s own `market_for_body` makes (duplicated rather
/// than shared: that one lives in an anonymous namespace, internal linkage).
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

std::unordered_map<entity_id, corp_cash_flow> clear_markets(
    world& w,
    const recipe_registry& reg,
    const economy_report& report)
{
    std::unordered_map<entity_id, corp_cash_flow> flows;
    const auto& by_body = markets_by_body(w);

    // --- The exchange record (BL-685) ---------------------------------------
    //
    // One row per exchange, appended to `w.exchanges` at each of the FOUR points
    // below where this pass actually moves goods for money. It is written beside
    // the cash-flow accrual it belongs to and never in a fifth pass of its own:
    // `quantity * unit_price` is the same product the neighbouring statement adds
    // to `corp_cash_flow`, so the record cannot come to disagree with the money
    // loop. Authority: docs/economy/MARKETS.md § The exchange record.
    //
    // `unit_price` IS THE PRICE CLEARING RESOLVED. For the three auto paths that
    // is `ref_price` — the market as counterparty of last resort — and for a
    // matched trade it is the price the match executed on. It is never read back
    // off a `sell_order::floor_price` or a `buy_order::max_price`: those are the
    // reservation prices an order CARRIED into clearing, and an order is honoured
    // at clearing, so what a seller asked and what they got are different numbers.
    //
    // DETERMINISM. The append order is a total order over the clearing walk, not
    // whatever a hash container happens to iterate in, and each of the four sites
    // walks a sequence that is already deterministic for its own reason:
    //   1. `auto_sells`  — built from `world::corp_body_pools`, a std::map, with
    //                      the resource index ascending inside each pool.
    //   2. `auto_buys`   — built from `economy_report::purchases`, a std::map
    //                      (BL-422 made it one, for this same class of reason).
    //   3. `trades`      — built over `book_mids` sorted ascending and each
    //                      market's resources sorted ascending.
    //   4. the auto-clear — the same sorted walk as 3, then each order book's own
    //                      insertion order, which is the TIME half of price-time
    //                      priority and is itself state.
    // `world::markets` is an unordered_map and is NEVER the thing walked to emit a
    // row. Nothing below reads a wall clock or an unseeded source.
    //
    // A ZERO-QUANTITY row is not an exchange and is dropped: it would spend a ring
    // slot on nothing and put a divide-by-zero in every reader that averages.
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

    // The standing order book, read from the world (BL-293). Bound by reference
    // rather than copied: nothing below resizes either vector — this pass mutates
    // markets and pools, never the book — so the references stay valid, and the
    // iteration order is the book's own insertion order, which is the TIME half
    // of price-time priority and must not be re-sorted here.
    const std::vector<sell_order>& standing_sells = w.sell_orders;
    const std::vector<buy_order>&  standing_buys  = w.buy_orders;

    // Captured BEFORE the reset below, so it carries the PREVIOUS tick's
    // end-of-tick supply — the only honest subtrahend available to
    // inject_interbody_demand, which runs before this tick's supply is written
    // (BL-404). Local to this pass: nothing is persisted and the save format is
    // untouched.
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

    // BL-263: the home body's own unmet demand pulls a discounted slice onto
    // every outpost market, additive after the resets above — without this an
    // outpost with real supply and no local population collapses to the price
    // floor the instant it starts producing. Runs AFTER the two demand
    // injections above, because BL-406's counterpart selection reads the demand
    // they deposit: the counterpart for a resource is whichever home-body market
    // wants it most, and that is not knowable until this tick's demand is in.
    inject_interbody_demand(w, reg, prior_supply);

    // A (corp, body, resource) with a standing sell order against it is under
    // manual control: the auto-surplus path yields so the floor-priced order
    // governs the sale, not the greedy auto path. True for any corp's order now,
    // not just the player's — a rival that places an order gets the same deal.
    auto order_controls = [&standing_sells](entity_id corp, entity_id body, std::size_t r) {
        for (const sell_order& o : standing_sells)
            if (o.corp == corp && o.body == body &&
                static_cast<std::size_t>(o.resource) == r && o.quantity > 0.0f)
                return true;
        return false;
    };

    // --- Separate auto-clearing entries from the explicit-order books ---
    //
    // Auto-surplus and auto-demand (processor shortfalls) always clear at the
    // reference price derived from supply/demand ratios — the market is a perfect
    // counterparty for these. Explicit orders go through the order book and, when
    // no matching buyer exists, auto-clear at the resolved price — never above it.
    // The floor is a reservation price: an order whose floor sits above the
    // resolved price holds its stock instead of selling, so listed supply is NOT
    // guaranteed to clear.
    struct auto_sell_entry { entity_id corp; entity_id market; std::size_t r; float qty; };
    struct auto_buy_entry  { entity_id corp; entity_id market; std::size_t r; float qty; };
    std::vector<auto_sell_entry> auto_sells;
    std::vector<auto_buy_entry>  auto_buys;

    // Explicit priced standing orders: supply/demand recorded into mc AND into order books.
    std::unordered_map<entity_id, std::unordered_map<std::size_t, std::vector<ob_sell_entry>>> sell_books;
    std::unordered_map<entity_id, std::unordered_map<std::size_t, std::vector<ob_buy_entry>>>  buy_books;

    // Auto-surplus: each corp's pool above its processor reservation.
    for (auto& [key, pool] : w.corp_body_pools)
    {
        const entity_id corp = key.first;
        const entity_id body = key.second;

        const entity_id mid = market_for_corp_on_body(w, by_body, corp, body);
        if (mid == null_entity)
            continue;
        if (w.corporations.find(corp) == w.corporations.end())
            continue;

        const market_component& mc = w.markets.at(mid);
        const auto reserve = processor_reservation(w, reg, corp, body);

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (mc.base_price[r] <= 0.0f)
                continue;
            if (order_controls(corp, body, r))
                continue;

            const float surplus = pool.quantities[r] - reserve[r];
            if (surplus <= 0.0f)
                continue;

            w.markets.at(mid).supply[r] += surplus;
            auto_sells.push_back({corp, mid, r, surplus});
        }
    }

    // Standing sell orders: into mc.supply and the explicit sell book.
    //
    // Several orders may name the same (corp, body, resource) — the command seam
    // permits it deliberately (only the AI's scorer declines to duplicate), and a
    // player can always add two overlapping orders in the ledger. So the pool has
    // to be RESERVED across them: each order lists against what the earlier orders
    // on the same triple have not already claimed. Without this the pool is
    // debited once per order downstream and the corp is paid for goods it never
    // held — money and goods from nothing. Insertion order decides who gets the
    // stock, which is the same time priority the matching pass uses.
    std::map<std::tuple<entity_id, entity_id, std::size_t>, float> listed_from_pool;

    for (const sell_order& order : standing_sells)
    {
        if (order.quantity <= 0.0f)
            continue;
        const entity_id mid = market_for_corp_on_body(w, by_body, order.corp, order.body);
        if (mid == null_entity)
            continue;
        const std::size_t r = static_cast<std::size_t>(order.resource);
        const auto pkit = w.corp_body_pools.find(std::make_pair(order.corp, order.body));
        if (pkit == w.corp_body_pools.end())
            continue;

        float& claimed = listed_from_pool[{order.corp, order.body, r}];
        const float unclaimed = pkit->second.quantities[r] - claimed;
        const float available = std::min(order.quantity, unclaimed);
        if (available <= 0.0f)
            continue; // an earlier order already spoke for the whole pool
        claimed += available;

        w.markets.at(mid).supply[r] += available;
        sell_books[mid][r].push_back({order.corp, available, order.floor_price, available});
    }

    // BL-130 fills real inventory from this tick's REAL corp-sourced sales only —
    // never read off mc.supply[r] as a whole, which also carries
    // inject_population_demand's/inject_background_demand's pure demand-side pulls
    // and any residual pricing-only signal.
    //
    // BL-422: the credit used to happen HERE, at listing time, on every listed
    // quantity. That was true while everything listed also sold; under BL-386's
    // reservation rule an order whose floor exceeds the resolved price holds its
    // stock, and inventory is not a display field — economy_system draws processor
    // inputs from it — so a held order's listed quantity became stock a buyer paid
    // for that no seller ever parted with. The credit now happens at each of the
    // three points a pool is actually debited (auto-surplus clearing, matched
    // trades, auto-clear), so inventory gains exactly what pools lose. Keeping the
    // two in the same statement is the whole point: they cannot drift apart.

    // BL-441 — the demand register reads the WANT, and only the want.
    //
    // This line used to read `report.purchases`, the FILL, so a processor that
    // needed 16 units of an input and could draw 2 told the market it wanted 2.
    // The resource then read to resolve_price as one almost nobody wants, its
    // price never rose, and scarcity was invisible — resolve_price was never
    // broken, it was being starved of input. `report.wants` carries what each
    // consumer set out to buy whether or not the draw succeeded, which is the
    // number a price is supposed to answer. Nothing is PAID against this loop.
    //
    // std::map, so accumulation is over a sorted key set — the ordering BL-422's
    // latent unordered_map float-accumulation nondeterminism was found in.
    for (const auto& [key, wanted] : report.wants)
    {
        const entity_id corp = key.first;
        const entity_id body = key.second;

        const entity_id mid = market_for_corp_on_body(w, by_body, corp, body);
        if (mid == null_entity)
            continue;

        for (std::size_t r = 0; r < resource_count; ++r)
            if (wanted[r] > 0.0f)
                w.markets.at(mid).demand[r] += wanted[r];
    }

    // The FILL, kept strictly separate: goods actually delivered to a consumer
    // this tick. This is the money side — auto_buys feeds the VWAP accumulator
    // and the expenditure a corp is actually charged — so it must go on reading
    // `purchases`. Billing the want instead would pay for deliveries nobody made,
    // which is BL-422's defect in the opposite direction. Also a std::map.
    for (const auto& [key, bought] : report.purchases)
    {
        const entity_id corp = key.first;
        const entity_id body = key.second;

        const entity_id mid = market_for_corp_on_body(w, by_body, corp, body);
        if (mid == null_entity)
            continue;

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float qty = bought[r];
            if (qty <= 0.0f)
                continue;
            auto_buys.push_back({corp, mid, r, qty});
        }
    }

    // Standing buy orders: into mc.demand and the explicit buy book.
    for (const buy_order& order : standing_buys)
    {
        if (order.quantity <= 0.0f)
            continue;
        const entity_id mid = market_for_corp_on_body(w, by_body, order.corp, order.body);
        if (mid == null_entity)
            continue;
        const std::size_t r = static_cast<std::size_t>(order.resource);
        w.markets.at(mid).demand[r] += order.quantity;
        buy_books[mid][r].push_back({order.corp, order.quantity, order.max_price,
                                     order.preferred_seller});
    }

    // --- Reference prices from accumulated supply/demand ---
    // Computed once, before any clearing, so all income/expenditure uses the same price.
    std::unordered_map<entity_id, std::array<float, resource_count>> ref_price;
    for (const auto& [mid, mc] : w.markets)
    {
        ref_price[mid] = {};
        for (std::size_t r = 0; r < resource_count; ++r)
            ref_price[mid][r] = resolve_price(mc.price[r], mc.base_price[r],
                                              mc.supply[r], mc.demand[r],
                                              reg.price_band().floor_mult,
                                              reg.price_band().ceil_mult);
    }

    // --- Auto-surplus clearing: income at ref_price, pool debited immediately ---
    for (const auto_sell_entry& se : auto_sells)
    {
        const entity_id body = w.markets.at(se.market).body;
        auto pkit = w.corp_body_pools.find(std::make_pair(se.corp, body));
        if (pkit != w.corp_body_pools.end())
        {
            float& q = pkit->second.quantities[se.r];
            const float left = std::min(q, se.qty); // defensive: a pool never goes negative
            q -= left;
            w.markets.at(se.market).inventory[se.r] += left; // BL-422: credit what left
        }
        flows[se.corp].income += se.qty * ref_price[se.market][se.r];
        // Exchange row 1 of 4: the market is the BUYER of last resort here, and
        // has no corp behind it — `null_entity` on that side means the market
        // itself, not an unknown counterparty (components.hpp, exchange_record).
        record_exchange(se.market, se.r, se.qty, ref_price[se.market][se.r],
                        se.corp, null_entity);
    }

    // --- Auto-demand clearing: expenditure at ref_price ---
    for (const auto_buy_entry& be : auto_buys)
    {
        flows[be.corp].expenditure += be.qty * ref_price[be.market][be.r];
        // Row 2 of 4: the mirror of row 1 — the market is the SELLER, so this is
        // the buy side of a player's history (what I bought, at what price).
        record_exchange(be.market, be.r, be.qty, ref_price[be.market][be.r],
                        null_entity, be.corp);
    }

    // --- Explicit order-book matching (player sell vs player buy) ---
    // Provides preferred-seller routing and a VWAP price signal when priced orders
    // dominate. Unmatched player sell supply auto-clears below (market as buyer of
    // last resort at the resolved price); a floor above it means hold, not sell.
    std::vector<matched_trade> trades;

    // The books are unordered maps and trade order feeds float accumulation into
    // flows, so iterate them in sorted key order (the markets_by_body idiom).
    std::vector<entity_id> book_mids;
    book_mids.reserve(sell_books.size());
    for (const auto& [mid, sell_by_r] : sell_books)
        book_mids.push_back(mid);
    std::sort(book_mids.begin(), book_mids.end());

    auto sorted_resources = [](const auto& by_r) {
        std::vector<std::size_t> rs;
        rs.reserve(by_r.size());
        for (const auto& [r, entries] : by_r)
            rs.push_back(r);
        std::sort(rs.begin(), rs.end());
        return rs;
    };

    for (const entity_id mid : book_mids)
    {
        auto& sell_by_r = sell_books[mid];
        for (const std::size_t r : sorted_resources(sell_by_r))
        {
            std::vector<ob_sell_entry>& sellers = sell_by_r[r];
            auto& buyers_map = buy_books[mid];
            auto  bit        = buyers_map.find(r);

            if (bit == buyers_map.end())
                continue; // no explicit buyers — handled by auto-clear below

            std::vector<ob_buy_entry>& buyers = bit->second;

            // Sort: cheapest seller first, then corp id for determinism.
            std::sort(sellers.begin(), sellers.end(),
                [](const ob_sell_entry& a, const ob_sell_entry& b) {
                    return a.floor_price != b.floor_price
                        ? a.floor_price < b.floor_price
                        : a.corp < b.corp;
                });

            // Sort: highest bidder first, then corp id for determinism.
            std::sort(buyers.begin(), buyers.end(),
                [](const ob_buy_entry& a, const ob_buy_entry& b) {
                    return a.max_price != b.max_price
                        ? a.max_price > b.max_price
                        : a.corp < b.corp;
                });

            // Working copies of remaining buy quantities; sellers drain their own
            // `rem` in place, which survives into the auto-clear pass below.
            std::vector<float> buy_rem(buyers.size());
            for (std::size_t i = 0; i < buyers.size();  ++i) buy_rem[i]  = buyers[i].qty;

            // Match: highest-priority buyer draws from the cheapest compatible seller.
            for (std::size_t bi = 0; bi < buyers.size(); ++bi)
            {
                if (buy_rem[bi] <= 0.0f)
                    continue;

                const ob_buy_entry& buyer = buyers[bi];

                float cheapest_price = -1.0f;
                for (std::size_t si = 0; si < sellers.size(); ++si)
                {
                    if (sellers[si].rem <= 0.0f)
                        continue;
                    if (sellers[si].floor_price > buyer.max_price)
                        continue;
                    if (cheapest_price < 0.0f || sellers[si].floor_price < cheapest_price)
                        cheapest_price = sellers[si].floor_price;
                }
                if (cheapest_price < 0.0f)
                    continue;

                for (int pass = 0; pass <= 1 && buy_rem[bi] > 0.0f; ++pass)
                {
                    for (std::size_t si = 0; si < sellers.size() && buy_rem[bi] > 0.0f; ++si)
                    {
                        if (sellers[si].rem <= 0.0f)
                            continue;

                        const bool is_preferred =
                            buyer.preferred_seller != null_entity &&
                            sellers[si].corp == buyer.preferred_seller;

                        if (pass == 0 && !is_preferred)
                            continue;
                        if (pass == 1 && is_preferred)
                            continue;

                        const float ask = sellers[si].floor_price;
                        if (ask > buyer.max_price)
                            continue;

                        if (is_preferred && ask > cheapest_price * 1.10f)
                            continue;

                        const float fill = std::min(buy_rem[bi], sellers[si].rem);
                        sellers[si].rem -= fill;
                        buy_rem[bi]     -= fill;

                        trades.push_back({sellers[si].corp, buyer.corp, mid, r, fill, ask});
                    }
                }
            }
        }
    }

    // --- Debit pools and accrue cash flows for matched explicit trades ---
    for (const matched_trade& t : trades)
    {
        const entity_id body = w.markets.at(t.market).body;
        auto pkit = w.corp_body_pools.find(std::make_pair(t.seller, body));
        if (pkit != w.corp_body_pools.end())
        {
            float& q = pkit->second.quantities[t.r];
            const float left = std::min(q, t.qty); // defensive: a pool never goes negative
            q -= left;
            w.markets.at(t.market).inventory[t.r] += left; // BL-422: credit what left
        }
        flows[t.seller].income     += t.qty * t.price;
        flows[t.buyer].expenditure += t.qty * t.price;
        // Row 3 of 4: the only path with a REAL corp on both sides. `t.price` is
        // the price the match executed on — read off the matching pass, not off
        // the standing order, which is why the tariff block below prices the duty
        // against the same number.
        record_exchange(t.market, t.r, t.qty, t.price, t.seller, t.buyer);
    }

    // --- Import tariff (Sprint D4) ------------------------------------------
    // A market already resolves to a jurisdiction: `market_component::centre_tile`
    // through `world::tile_to_nation`. That hook existed and was unused — no
    // sale in this economy had ever been a CROSS-BORDER sale, only a sale.
    //
    // The rule, in one line: a matched trade whose BUYER is domiciled outside the
    // market's own nation pays that nation's enacted duty, and the duty is
    // credited to that nation's treasury. A same-nation sale is charged nothing —
    // a tariff that taxed domestic trade would be a sales tax wearing the wrong
    // name.
    //
    // It is charged ONLY on matched explicit trades, and that is a principled
    // limit rather than an oversight: a matched trade is the only clearing path
    // with a real counterparty on both sides. The auto-surplus and buyer-of-
    // last-resort paths trade against the market itself, and taxing an import
    // from nobody would invent the second party the flow does not have.
    //
    // CONSERVATION. The buyer's expenditure rises by exactly the amount the
    // treasury rises by, in the same statement — a transfer, never a mint and
    // never a burn. `apply_budget` charges expenditure unconditionally (a corp's
    // balance may go negative), so the two sides cannot drift apart on a
    // solvency edge.
    //
    // Gated on `any_import_tariff_enacted`, so with no tariff law enacted — the
    // shipped default — not one line below runs and the tick is bit-identical to
    // the pre-tariff arithmetic.
    if (any_import_tariff_enacted(w))
    {
        // `trades` is built in the deterministic book order above, so this float
        // accumulation into each treasury is the same number every run.
        for (const matched_trade& t : trades)
        {
            const market_component& mc = w.markets.at(t.market);
            if (mc.centre_tile == null_entity)
                continue; // an unanchored market sits in no jurisdiction

            const auto nit = w.tile_to_nation.find(mc.centre_tile);
            if (nit == w.tile_to_nation.end())
                continue;
            const entity_id market_nation = nit->second;

            const auto bcit = w.corporations.find(t.buyer);
            if (bcit == w.corporations.end())
                continue;
            if (bcit->second.home_nation == market_nation)
                continue; // domestic buyer: not an import

            const float rate = nation_tariff_rate(w, market_nation,
                                                  static_cast<resource_type>(t.r));
            if (rate <= 0.0f)
                continue;

            const auto tnit = w.nations.find(market_nation);
            if (tnit == w.nations.end())
                continue; // no treasury to pay into: charge nothing

            const float duty = t.qty * t.price * rate;
            flows[t.buyer].expenditure += duty;
            tnit->second.treasury      += duty;
        }
    }

    // --- Auto-clear unmatched player sell supply ---
    // The market is a buyer of last resort at the RESOLVED price, never above it —
    // the market pays no counterparty's money, so it cannot honour a price the
    // supply/demand state does not support. The floor is a reservation price: an
    // order whose floor exceeds the resolved price clears nothing and its stock
    // stays in the pool. Each cleared order's own `rem` (left by the matching
    // pass) clears — not a per-seller aggregate, which would subtract the seller's
    // whole matched total from every one of that seller's orders (BL-351). Sorted
    // key order again: income accumulation.
    for (const entity_id mid : book_mids)
    {
        const auto& sell_by_r = sell_books[mid];
        const entity_id body = w.markets.at(mid).body;
        for (const std::size_t r : sorted_resources(sell_by_r))
        {
            const float rp = ref_price[mid][r];
            for (const ob_sell_entry& se : sell_by_r.at(r))
            {
                if (se.rem <= 0.0f)
                    continue;
                if (se.floor_price > rp)
                    continue; // reservation price above the market: hold, don't sell
                auto pkit = w.corp_body_pools.find(std::make_pair(se.corp, body));
                if (pkit != w.corp_body_pools.end())
                {
                    // Sell only what matching left unsold, order by order. The
                    // aggregate-per-seller form this replaced charged one corp's
                    // matched volume against every one of its orders — the mirror
                    // of the over-listing bug above, under-selling instead of
                    // over-selling (BL-351; BL-293 fixed the same fault by drawing
                    // a matched total down, which `rem` expresses directly).
                    float& q = pkit->second.quantities[r];
                    const float left = std::min(q, se.rem); // defensive: never negative
                    q -= left;
                    w.markets.at(mid).inventory[r] += left; // BL-422: credit what left
                }
                flows[se.corp].income += se.rem * rp;
                // Row 4 of 4: the buyer-of-last-resort sale. The order carried a
                // floor of `se.floor_price` and cleared at `rp` — the record takes
                // `rp`, because that is what the seller GOT. An order whose floor
                // sat above `rp` never reaches here (it held its stock), so no
                // row is written for a sale that did not happen.
                record_exchange(mid, r, se.rem, rp, se.corp, null_entity);
            }
        }
    }

    // --- Price update: ref_price (pre-computed from supply/demand) ---
    // Explicit priced trades provide a VWAP signal; when they occurred, ease toward
    // the VWAP. Otherwise use the supply/demand reference price directly (it already
    // incorporates the EMA from resolve_price).
    {
        struct vwap_acc { float vol = 0.0f; float qty = 0.0f; };
        std::unordered_map<entity_id, std::array<vwap_acc, resource_count>> vwap;
        for (const matched_trade& t : trades)
        {
            vwap[t.market][t.r].vol += t.price * t.qty;
            vwap[t.market][t.r].qty += t.qty;
        }

        for (auto& [mid, mc] : w.markets)
        {
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                const auto vit = vwap.find(mid);
                if (vit != vwap.end() && vit->second[r].qty > 0.0f)
                {
                    const float tick_vwap = vit->second[r].vol / vit->second[r].qty;
                    mc.price[r] = mc.price[r] + price_smoothing * (tick_vwap - mc.price[r]);
                }
                else
                {
                    mc.price[r] = ref_price[mid][r];
                }
            }
        }
    }

    return flows;
}
