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

/// Inject population demand into body markets (BL-190 ordering fix + BL-368
/// basket generalisation: a plain market-demand pull, never a starvation
/// mechanic). Each population centre pulls a price-elastic, multi-resource
/// basket (population_demand_params, economy.population_demand in Lua) into its
/// catchment market (market_for_tile, so BL-096 multi-market bodies route by
/// nearest centre) — population is a pure CONSUMER, no supply term. Called from
/// clear_markets after the per-tick supply/demand reset, before the order-book
/// pass, so the demand is additive and survives into price resolution.
/// Deterministic — no RNG.
///
/// @param w World; market demand arrays are mutated in place.
void inject_population_demand(world& w, const recipe_registry& reg);

/// Inject background-industrial demand into body markets (BL-340/BL-365). A
/// world-scale pull for the mid-chain processing goods (silicon, refined_copper,
/// ree_alloy, machinery, alloys, electronics — deliberately NOT
/// spacecraft_components, whose only intended buyer is the militia's
/// procurement contracts) — the offstage economy's own appetite for these
/// goods, layered on top of whatever real background corporations produce and
/// consume. Scaled per market by that market's body's total population scale
/// (sum of every population centre's `scale` on the body), same price-elastic
/// shape as inject_population_demand. Called from clear_markets alongside it,
/// after the per-tick supply/demand reset. Deterministic — no RNG.
///
/// @param w World; market demand arrays are mutated in place.
void inject_background_demand(world& w, const recipe_registry& reg);

/// BL-263: if @p body carries no market yet, create one — the spontaneous
/// market emergence trigger fires the tick a body's FIRST building completes
/// (any corporation; investment, not presence). No-op if the body already has a
/// market (including the home body's own BL-096 carved seeding, which always
/// predates any completion trigger) or the body id is invalid.
///
/// Opening prices are seeded from `market_for_body(w, w.home_body)`'s own
/// base_price, marked up by distance (market_emergence_params.price_distance_gain)
/// — not from world_gen's flat base_price table and not from EMA, which cannot
/// run with no history. If the home body itself has no market (a degenerate
/// harness fixture), the new market opens with all-zero base_price — untradeable
/// but harmless, the same safe fallback every other resource with base_price 0
/// already gets.
///
/// The new market's `centre_tile` is the completed building's own tile — an
/// off-world outpost has no population-centre tile to anchor to, and this is at
/// least the site that earned the market. Deterministic: a pure function of
/// world state (body distances, market ids) with no RNG; market ids come from
/// `world::create_entity`'s monotonic counter, never container size.
///
/// @param w        Mutable world; may insert into `w.markets`.
/// @param reg      Loaded registry (market_emergence_params).
/// @param body     Body the just-completed building sits on.
/// @param tile     The completed building's own tile (the new market's centre).
/// @return         The new market's id, or `null_entity` if none was created.
entity_id maybe_spawn_market(world& w, const recipe_registry& reg, entity_id body, entity_id tile);

/// BL-263: pulls a discounted slice of the home body's own unmet demand
/// (demand - supply, when positive) onto every OTHER body's market, per
/// resource — "an outpost market clears primarily against inter-body demand...
/// nobody builds a mine on a moon to sell to the moon." Without this an outpost
/// with real supply and no local population collapses to the price floor the
/// instant it starts producing. Called from clear_markets after
/// inject_population_demand (additive, same ordering contract). No-op if the
/// home body has no market. Deterministic — no RNG, a pure function of the
/// already-accumulated demand/supply this tick.
///
/// @param w   World; every non-home-body market's demand is mutated in place.
/// @param reg Loaded registry (market_emergence_params).
void inject_interbody_demand(world& w, const recipe_registry& reg);

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
