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

#include <vector>

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
/// (capacity > 0, not a grid good) and finite points > 0? The command seam and
/// the save loader both refuse a trade this rejects.
bool trade_is_valid(const world& w, const recipe_registry& reg, const standing_trade& t);

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
