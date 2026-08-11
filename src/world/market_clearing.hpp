#pragma once

#include "components.hpp"
#include "economy_system.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <unordered_map>
#include <vector>

// `sell_order` and `buy_order` (both sides of the order book) are defined in
// components.hpp so both the UI state and this clearing system can name them
// without an include cycle.

/// Per-corporation cash-flow figures from one market clearing, valued at the
/// price resolved this tick (base_price modulated by supply/demand). The balance
/// arithmetic (these, less maintenance and wages) is the budget step's job
/// (budget_system.hpp).
struct corp_cash_flow
{
    float income      = 0.0f; ///< Goods sold × resolved price.
    float expenditure = 0.0f; ///< Inputs auto-bought × resolved price.
};

/// Inject population food demand into body markets (BL-190: a plain
/// market-demand pull, never a starvation mechanic). Each population centre adds
/// 1 unit of agricultural_produce demand per scale level to its catchment market
/// (market_for_tile, so BL-096 multi-market bodies route by nearest centre).
/// Called from clear_markets after the per-tick supply/demand reset, before the
/// order-book pass, so the demand is additive and survives into price
/// resolution. Deterministic — no RNG.
///
/// @param w World; market demand arrays are mutated in place.
void inject_population_demand(world& w, const recipe_registry& reg);

/// Clear every body market for one economy tick using a per-(body, resource)
/// matched order book. For each market and resource:
///   - Sell side: each corp's pool surplus above its processors' next-run need,
///     plus every standing sell order in `w.sell_orders` (floor-priced).
///   - Buy side: processor input shortfalls from the economy report, plus every
///     standing buy order in `w.buy_orders` (max-price limited).
/// Orders are sorted by price priority (cheapest seller first, highest bidder
/// first) with corp id as the deterministic tiebreaker. Matching proceeds buyer-
/// first: each buyer draws from the cheapest compatible seller; a preferred_seller
/// hint wins ties and is matched when up to 10% more expensive than the cheapest
/// alternative. Clearing price per match = seller's floor price (ask). Volume-
/// weighted average price of all matches drives the EMA price update. Unmatched
/// surplus/shortfall still updates mc.supply/demand for the UI. Pools are debited
/// only for matched sell quantities.
///
/// THE BOOK IS READ FROM THE WORLD, NOT PASSED IN (BL-293, 2026-08-07). It used
/// to arrive as two caller-supplied vectors owned by `ui_state`, which made
/// clearing something the UI *drove* rather than something the simulation *does*
/// — a headless tick sold nothing standing, and no corp_command could reach the
/// book. Ben's ruling: "Order book needs to be a background process, the AI must
/// be able to trade as a player does." An empty book is the prior pooled model
/// exactly, so existing econ_harness expectations are unchanged.
///
/// @param w      World; markets and (corp, body) pools are mutated, and the
///               standing order book (`sell_orders` / `buy_orders`) is read.
/// @param reg    Loaded registry (for processor input reservations).
/// @param report Economy step report (its purchases drive the buy side).
/// @return       Per-corporation cash flow valued at matched prices.
std::unordered_map<entity_id, corp_cash_flow> clear_markets(
    world& w,
    const recipe_registry& reg,
    const economy_report& report);

/// Resolve which market a tile clears against (its market catchment). Among the
/// markets on the tile's body: a body with a single market routes there
/// unconditionally; with several, the tile clears against the market whose
/// `centre_tile` is nearest by grid distance (ties → lowest market id; markets
/// with no centre are ignored when an anchored one exists). Returns `null_entity`
/// if the tile's body has no market.
entity_id market_for_tile(const world& w, entity_id tile);
