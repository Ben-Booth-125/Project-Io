#pragma once

// BL-1266 — TRADE (docs/economy/TRADE.md). Trade is the only way goods move
// between markets (Ben, 2026-10-10): a corporation's Planetary Marketplaces and
// Ports make TRADE POINTS each tick; each point moves a good's authored
// CAPACITY in units; a TRADE buys on one market's shelf, pays the haul, and the
// goods land and sell on another's. Manual trades spend the points an owner
// reserves; auto trade spends the rest on the best-margin routes.
//
// The shipment itself — the priced leg and the convoy — is supply_system's
// (`price_trade_leg`, `commit_trade_shipment`); this file decides what ships.

#include "economy_system.hpp" // economy_report: the want/fill registers a trade buys through
#include "logistics.hpp"      // lp_pool_map
#include "recipe_registry.hpp"
#include "world.hpp"

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

struct logistics_nodes; // supply_system.hpp

/// One trade pass's counters (TRADE.md), for the tick summary and the harnesses.
struct trade_tick
{
    int   corps_trading    = 0;    ///< Corporations that made any trade points.
    float points_made      = 0.0f; ///< Every trade point made this tick.
    float points_spent     = 0.0f; ///< Points that moved goods (units sent / capacity).
    int   manual_shipments = 0;    ///< Shipments of manual trades.
    int   auto_shipments   = 0;    ///< Shipments auto trade chose.
    int   refused_no_lp    = 0;    ///< Shipments the passive Logistic Point cap refused outright.
    float units_shipped    = 0.0f; ///< Units of every good sent.
};

/// Is @p t a TRADE building — one that makes trade points (TRADE.md § The
/// Planetary Marketplace: the Marketplace, and Ports)?
inline bool is_trade_building(building_type t)
{
    return t == building_type::planetary_marketplace || t == building_type::port;
}

/// The trade points ONE building makes this tick (TRADE.md § The Planetary
/// Marketplace). Zero unless it is a completed, active trade building whose
/// upkeep was met this tick (`economy_report::upkeep_unmet`). A Marketplace
/// makes `trade.marketplace_points` per unit of EFFECTIVE workforce — its
/// staffed rate, as a processor's base_rate is its output per unit of effective
/// workforce: `workforce_assigned x contention x clamp(workforce_target/100,
/// 0, 2)`. A Port (it staffs at zero) makes `trade.port_points` flat.
/// @p contention is the (owner, body) labour scalar this tick (1 if unknown).
float building_trade_points(const recipe_registry& reg, const building_component& b,
                            float contention, bool upkeep_met);

/// The trade points @p corp's trade buildings make this tick (the sum of
/// `building_trade_points`, its assets in stored order).
float corp_trade_points(const world& w, const recipe_registry& reg,
                        const economy_report& report, entity_id corp);

/// The markets @p corp's trade reaches (TRADE.md § Auto and reserved trade:
/// "among the markets the owner's Marketplaces reach"): every market on a body
/// where it holds a completed, active trade building, ascending market id. A
/// route between two of them still needs a viable leg — between bodies, a
/// Launchpad and propellant on the source body (`price_trade_leg`).
std::vector<entity_id> corp_trade_markets(const world& w, entity_id corp);

/// Is @p t a well-formed manual trade in @p w as it stands — a live owner
/// corporation, two distinct existing markets, a good trade carries
/// (capacity > 0, not a grid good) and finite points > 0? The command seam refuses a trade
/// this rejects; the save loader, which has no registry, checks the owner,
/// the markets, the points and the per-owner cap, and the trade pass re-checks
/// this every tick (a trade it rejects ships nothing).
bool trade_is_valid(const world& w, const recipe_registry& reg, const standing_trade& t);

/// The same-body haul memo one ranking pass keeps (cost is linear in quantity,
/// so a one-unit leg prices every quantity): per (source, destination) market,
/// corporation- and good-independent. NaN = no route. Callers hold one per pass
/// and hand it to every `rank_trade_routes` call in that pass.
struct trade_haul_memo
{
    std::map<std::pair<entity_id, entity_id>, float> intra;
};

/// One route auto trade (and the scorer's trade candidate) can rank: good @p r
/// from market @p a to market @p b.
struct trade_route_offer
{
    float       score;  ///< Margin per POINT: `margin_per_unit x capacity(r)`.
    entity_id   a;
    entity_id   b;
    std::size_t r;
    float       landed; ///< Source price + haul per unit (the cost a unit lands at).
    float       margin_per_unit; ///< `price_B - price_A - haul per unit`.
};

/// The haul per unit of good @p ri from @p a to @p b for @p corp: same body, the
/// corporation-independent market leg (memoised in @p memo); between bodies,
/// the corporation's space lane (`price_trade_leg`). NaN when not viable.
float trade_haul_per_unit(world& w, const recipe_registry& reg, const logistics_nodes& nodes,
                          trade_haul_memo& memo, entity_id corp, entity_id a, entity_id b,
                          std::size_t ri);

/// THE ROUTE RANKING auto trade spends by (TRADE.md § Auto and reserved trade),
/// shared with the scorer's trade candidate (AI_OPPONENT.md § 11, "a rival may
/// set its own trades") so there is one estimate, not two. Fills @p out with
/// every route among @p reach that earns more than `dispatch_margin()` of its
/// source price — the source shelf holds the good under the fair-price
/// ceiling, `(price_B - price_A - haul) > margin x price_A` — best margin per
/// point first (ties: source, destination, good, ascending). Reads public
/// prices and the network's haul only. Writes nothing but logistics caches and
/// @p memo. Deterministic: sorted walks.
void rank_trade_routes(world& w, const recipe_registry& reg, const logistics_nodes& nodes,
                       trade_haul_memo& memo, entity_id corp,
                       const std::vector<entity_id>& reach, std::vector<trade_route_offer>& out);

/// THE TRADE PASS, once per economy tick, after `run_economy_step` (its
/// production has landed and its draws are made) and before `clear_markets`
/// (which bills the trades' purchases and prices their wants). For every
/// corporation, ascending id:
///   1. its trade points this tick (`corp_trade_points`), stored on
///      `corporation_component::trade_points`;
///   2. its MANUAL trades (`world::trades`, placement order) spend up to its
///      reserve (`trade_reserve`, clamped to the points made): each ships up to
///      `points x capacity(R)` — bought, hauled, landed — margin or no margin;
///   3. AUTO spends the rest on the routes with the best margin per point,
///      `(price_B - price_A - haul per unit) x capacity(R)`, among the markets
///      `corp_trade_markets` names, best first (ties: source, destination,
///      good, ascending), each route sized to what B can absorb above the
///      landed cost (`trade_room`) and what A's shelf holds, until the points
///      run out or no route earns more than `dispatch_margin()` of its source
///      price.
/// The player's shipments are recorded for the Trade-flow lens
/// (`world::player_trade_flow`). Deterministic: sorted walks only.
///
/// @param shared_lp_pools  The tick's shared Logistic Point pool (the march
///                         drew first). Null: a private pool for this pass.
trade_tick run_trades(world& w, const recipe_registry& reg, economy_report& report,
                      lp_pool_map* shared_lp_pools = nullptr);
