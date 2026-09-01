#pragma once

#include "components.hpp"
#include "economy_system.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <array>
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

/// BL-647: inject endemic-luxury demand into nation-anchored markets — the
/// Endemic trade channel (docs/economy/MARKETS.md § Demand channels; the design
/// is docs/economy/RESOURCES.md § Mercantile). A pure demand-side pull for the
/// endemic goods (tobacco, spices, coffee, furs) that scales with a nation's
/// WEALTH rather than its headcount — so it rewards a player who has made
/// somewhere rich — flavoured per (nation, good) by a seeded, campaign-fixed
/// preference weight so different nations crave different luxuries and the
/// trade route is directional by construction: extract where it grows, sell
/// where the money is.
///
/// Wealth is the nation's treasury plus the summed positive balances of the
/// corporations domiciled in it, split evenly across the markets anchored in
/// the nation's territory (tile_to_nation over market_component::centre_tile)
/// so a nation's total pull is independent of how many markets BL-096 carved
/// it into. A market with no owning nation — an off-world outpost — receives
/// nothing here; the home body's luxury shortfall reaches outposts through
/// inject_interbody_demand like every other unmet want.
///
/// Same price-elastic shape as the two injectors above (deliberately not a
/// second elasticity model). Called from clear_markets after them, before
/// inject_interbody_demand. Deterministic — no RNG at tick time: the
/// preference weight is a pure hash of the nation's generated identity, and
/// the per-nation wealth sum accumulates over sorted corp ids. Tunables in
/// `scripts/economy.lua` § `endemic_demand`; wealth_scale defaults 0 so a
/// hand-built registry injects nothing.
///
/// @param w World; market demand arrays are mutated in place.
void inject_endemic_demand(world& w, const recipe_registry& reg);

/// BL-652 — one basket entry the injectors CANNOT price, named.
struct unpriced_basket_entry
{
    resource_type resource;
    /// Which basket names it: "household" (`economy.population_demand`) or
    /// "background" (`economy.background_demand`). A literal, never owned.
    const char*   channel;
};

/// BL-652: every (channel, resource) pair that a demand basket NAMES with a
/// positive weight and that NO market in @p w carries a base price for.
///
/// WHY IT IS A NAMED SURFACE AND NOT A SILENT `continue`. Both injectors skip
/// such an entry — "untradeable, no base price to anchor the elasticity curve"
/// — and that skip is invisible from the outside: a basket that is authored but
/// unpriced looks exactly like a channel nobody ever wrote. Two separate bugs
/// hid behind that silence on one day in August 2026. The demand census read as
/// having no background demand at all, and `spawn_solvency` measured a whole
/// spawn diagnosis in a world where background demand did not exist. NEITHER
/// FAILED. Both quietly answered a question about a different world.
///
/// The combination is always either a missing script (`world_gen.lua`, which
/// carries `kepler_market.base_price`, was not loaded) or an authoring error (a
/// basket names a resource the price table does not), and it is never intended.
/// So callers should treat a non-empty result as a fault: the app reports it on
/// startup, and `tools/verify/demand_census.cpp` FAILS on it.
///
/// One entry per (channel, resource) — the FIRST occurrence, not one per market
/// or per population centre, which would bury the finding in thousands of rows.
/// Deterministic: channel order then resource-index order, and the price probe
/// over `w.markets` is a pure OR, so the unordered map's layout cannot reach the
/// result.
std::vector<unpriced_basket_entry> unpriced_basket_entries(const world& w,
                                                           const recipe_registry& reg);

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

/// Every market's supply arrays, keyed by market id — a snapshot, not a view.
/// Exists for one caller: `clear_markets` takes one BEFORE its per-tick reset so
/// `inject_interbody_demand` has a real supply to net against (BL-404). Never
/// persisted; the save format knows nothing about it.
using market_supply_snapshot =
    std::unordered_map<entity_id, std::array<float, resource_count>>;

/// Capture every market's current supply. Call before `clear_markets` zeroes it.
market_supply_snapshot snapshot_market_supply(const world& w);

/// BL-263: pulls a discounted slice of the home body's unmet demand onto every
/// OTHER body's market, per resource — "an outpost market clears primarily
/// against inter-body demand... nobody builds a mine on a moon to sell to the
/// moon." Without this an outpost with real supply and no local population
/// collapses to the price floor the instant it starts producing.
///
/// **Which home market (BL-406, Ben's ruling 2026-08-15).** Per resource, the
/// COUNTERPART: the home-body market carrying the greatest demand for that
/// resource, lowest market id breaking ties. It used to be
/// `market_for_body(w, w.home_body)` — the lowest-id market of the many BL-096
/// carves onto the home body, holding 5% of the body's demand and a different
/// market again under a different standard library. No single market stands for
/// the body now, and the pick is a fact about the economy rather than about
/// container order.
///
/// **What it nets against (BL-404).** @p prior_supply, the previous tick's
/// end-of-tick supply. Reading `market.supply` directly made the subtraction a
/// no-op: `clear_markets` zeroes supply immediately before this call and writes
/// it after, so every outpost was pulled by GROSS home demand. One tick of lag
/// is the price of not reordering a pass whose ordering is load-bearing.
///
/// Called from clear_markets after inject_population_demand and
/// inject_background_demand — that order is now REQUIRED, not merely additive:
/// the counterpart is chosen by this tick's demand, which those two deposit.
/// No-op if the home body has no market. Deterministic — no RNG, and the
/// counterpart rule is a total order, so no dependence on `w.markets`' traversal.
///
/// @param w             World; every non-home-body market's demand is mutated.
/// @param reg           Loaded registry (market_emergence_params).
/// @param prior_supply  Snapshot from before this tick's reset. Passing an empty
///                      map means "no prior supply known" and nets against zero —
///                      the pre-BL-404 behaviour, correct only for a fresh world.
void inject_interbody_demand(world& w,
                             const recipe_registry& reg,
                             const market_supply_snapshot& prior_supply);

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
