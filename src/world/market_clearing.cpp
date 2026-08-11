#include "market_clearing.hpp"

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
    const population_demand_params& pd = reg.population_demand();

    for (const auto& [centre_id, pcc] : w.population_centres)
    {
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
                continue; // Untradeable — no base price to anchor the elasticity curve.
            const float weighted = scale * pd.demand_basket[r];
            if (weighted <= 0.0f)
                continue;

            const float price   = (mc.price[r] > 0.0f) ? mc.price[r] : base;
            const float elastic = std::clamp(std::pow(base / price, pd.demand_elasticity),
                                             pd.elasticity_min, pd.elasticity_max);
            mc.demand[r] += weighted * elastic;
        }
    }
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

void inject_interbody_demand(world& w, const recipe_registry& reg)
{
    if (w.home_body == null_entity)
        return;
    const entity_id home_market_id = market_for_body(w, w.home_body);
    if (home_market_id == null_entity)
        return;
    const market_component& home = w.markets.at(home_market_id);
    const market_emergence_params& mp = reg.market_emergence();

    for (auto& [mid, mc] : w.markets)
    {
        if (mc.body == w.home_body)
            continue; // The home body pulls demand onto outposts, not itself.

        const float dist   = body_radius_distance_au(w, mc.body, w.home_body);
        const float falloff = 1.0f + mp.distance_falloff * dist;

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float home_shortfall = home.demand[r] - home.supply[r];
            if (home_shortfall <= 0.0f)
                continue;
            if (mc.base_price[r] <= 0.0f)
                continue; // Untradeable here regardless of what's wanted at home.
            mc.demand[r] += home_shortfall * mp.pull_fraction / falloff;
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

    // The standing order book, read from the world (BL-293). Bound by reference
    // rather than copied: nothing below resizes either vector — this pass mutates
    // markets and pools, never the book — so the references stay valid, and the
    // iteration order is the book's own insertion order, which is the TIME half
    // of price-time priority and must not be re-sorted here.
    const std::vector<sell_order>& standing_sells = w.sell_orders;
    const std::vector<buy_order>&  standing_buys  = w.buy_orders;

    for (auto& [mid, mc] : w.markets)
    {
        mc.supply.fill(0.0f);
        mc.demand.fill(0.0f);
    }

    // Inject the elastic nation-substrate demand/supply into markets before the
    // order-book runs. Must come after the zero-reset above so substrate is additive
    // to the order-book quantities, not erased by it. (BL-078: reads last tick's
    // cleared price for the demand elasticity.)
    inject_substrate_demand(w, reg);

    // Population food demand (BL-190) — likewise additive after the reset, so
    // the population's pull reaches price resolution.
    inject_population_demand(w, reg);

    // BL-263: the home body's own unmet demand pulls a discounted slice onto
    // every outpost market, additive after the resets above — without this an
    // outpost with real supply and no local population collapses to the price
    // floor the instant it starts producing.
    inject_interbody_demand(w, reg);

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
    // counterparty for these. Explicit player orders go through the order book and
    // also auto-clear at max(ref_price, floor) when no matching buyer exists, so
    // supply is always guaranteed to clear (prototype invariant).
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

    // BL-130: fill real inventory from this tick's REAL corp-sourced sales only —
    // auto-surplus (auto_sells) and the standing sell orders just listed above
    // (sell_books). Deliberately NOT read off mc.supply[r] as a whole: that array
    // also carries inject_substrate_demand's abstract nation supply (a pricing
    // fiction, never actually sold by anyone), which must not inflate real stock.
    for (const auto_sell_entry& se : auto_sells)
        w.markets.at(se.market).inventory[se.r] += se.qty;
    for (const auto& [mid, sell_by_r] : sell_books)
        for (const auto& [r, entries] : sell_by_r)
            for (const ob_sell_entry& e : entries)
                w.markets.at(mid).inventory[r] += e.qty;

    // Auto-demand: processor input shortfalls from the economy report.
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
            w.markets.at(mid).demand[r] += qty;
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
                                              mc.supply[r], mc.demand[r]);
    }

    // --- Auto-surplus clearing: income at ref_price, pool debited immediately ---
    for (const auto_sell_entry& se : auto_sells)
    {
        const entity_id body = w.markets.at(se.market).body;
        auto pkit = w.corp_body_pools.find(std::make_pair(se.corp, body));
        if (pkit != w.corp_body_pools.end())
        {
            float& q = pkit->second.quantities[se.r];
            q = std::max(0.0f, q - se.qty); // defensive: a pool never goes negative
        }
        flows[se.corp].income += se.qty * ref_price[se.market][se.r];
    }

    // --- Auto-demand clearing: expenditure at ref_price ---
    for (const auto_buy_entry& be : auto_buys)
        flows[be.corp].expenditure += be.qty * ref_price[be.market][be.r];

    // --- Explicit order-book matching (player sell vs player buy) ---
    // Provides preferred-seller routing and a VWAP price signal when priced orders
    // dominate. Unmatched player sell supply auto-clears below (market as buyer of
    // last resort at max(ref_price, floor)), so supply always clears.
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
            q = std::max(0.0f, q - t.qty); // defensive: a pool never goes negative
        }
        flows[t.seller].income     += t.qty * t.price;
        flows[t.buyer].expenditure += t.qty * t.price;
    }

    // --- Auto-clear unmatched player sell supply ---
    // The market is a buyer of last resort at max(ref_price, floor). Each order's
    // own `rem` (left by the matching pass) clears — not a per-seller aggregate,
    // which would subtract the seller's whole matched total from every one of that
    // seller's orders (BL-351). Sorted key order again: income accumulation.
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
                const float clearing = std::max(rp, se.floor_price);
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
                    q = std::max(0.0f, q - se.rem); // defensive: a pool never goes negative
                }
                flows[se.corp].income += se.rem * clearing;
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
