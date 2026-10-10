#pragma once

#include "recipe_registry.hpp"
#include "logistics.hpp" // lp_pool_map, nearest_lp_anchor, lp_pool_for_body (BL-597)
#include "world.hpp"

#include <cstddef>
#include <array>
#include <cstdint>
#include <map>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Interdiction (BL-458 — supply lines can be cut)
// ---------------------------------------------------------------------------
// Until this, nothing in the game could stop a convoy: cargo moved from dispatch
// to arrival with no interaction with units, stance or force of any kind, so the
// logistics layer and the military layer shared a world and touched nowhere.
//
// The predicate is STANCE and only stance. Hostility is a declared state a corp
// opts into (Ben, 2026-08-17, the BL-448 ruling) and it is DIRECTED, so a corp
// can be at war and not know it yet — the ambush property. Nothing here makes a
// corp hostile; interdiction only reads `is_hostile`. There is no incidental
// interception by neutrals, no terrain blockade and no ambient banditry, which
// is also what stops this being a random tax on trade.
//
// The outcome is CAPTURE (Ben, 2026-08-17, on NR-310), with destruction as the
// fallback when the cargo cannot be credited anywhere. Cargo already left the
// source pool at dispatch, so both answers conserve; capture is chosen because
// destroy-only would give the scored-utility rival a payoff of zero and it would
// correctly never rank interdiction, shipping a capability only the player ever
// fires.

/// What became of an intercepted convoy's cargo.
enum class interception_outcome : std::uint8_t
{
    captured  = 0, ///< Credited whole to the interceptor's pool at the interception tile's market.
    destroyed = 1, ///< Nothing credited anywhere — the fallback, never a mint.
};

/// One interception, as it happened. Returned by `intercept_convoys` for the
/// surfaces to narrate; deliberately NOT stored on `world` — see that function.
struct interception_record
{
    std::uint32_t        convoy_id        = 0;
    entity_id            victim_corp      = null_entity;
    entity_id            interceptor_corp = null_entity;
    entity_id            interceptor_unit = null_entity;
    entity_id            tile             = null_entity; ///< Where the convoy's head stood.
    entity_id            body             = null_entity;
    resource_type        cargo_resource   = resource_type::iron_ore;
    float                cargo_qty        = 0.0f;
    interception_outcome outcome          = interception_outcome::captured;
    int                  tick             = 0;
};

/// Cut every convoy standing on a tile held by a unit whose owner has declared
/// hostility toward the convoy's corp. The cargo credits the interceptor's
/// pool at the interception tile's market (BL-1003), or — if the interceptor is not
/// a corporation, or the tile resolves to no body — is destroyed. The convoy is
/// then erased: it never arrives and never credits its destination.
///
/// **Conservation is the load-bearing property.** Captured quantity in equals
/// quantity credited, exactly; the destroyed path credits nothing and mints
/// nothing. No path creates goods.
///
/// **Deterministic.** Convoys are walked in `w.convoys` order (dispatch already
/// builds that from sorted corp/market ids); the tile occupancy index is built
/// from a SORTED unit-id walk, so the interceptor chosen on a contested tile is
/// always the lowest-id hostile unit, never whichever the hash landed on. No RNG
/// anywhere — detection and outcome are both total functions of world state.
///
/// Runs at the top of `credit_arrived_convoys`, which is the ONLY seam between
/// `advance_convoys` and crediting that every caller (app, main, every harness)
/// already shares. Exposed separately so a harness can fire it directly.
///
/// @return one record per cut convoy, in the order they were cut. Nothing is
///         stored on `world` — deliberately, and it is not owed: an interception
///         is an EVENT, not state, so it belongs on the tick's report the way
///         `agency_events` and `battle_dispatches` do (economy_system.hpp), not
///         on the serialised world. `credit_arrived_convoys`' `out_cuts`
///         parameter is how a caller collects them; this signature stays for a
///         harness that wants to fire the pass directly.
std::vector<interception_record> intercept_convoys(world& w, int tick);

/// Advance every in-flight convoy by its speed increment. Convoys whose progress
/// reaches >= 1.0 have their `arrived` flag set; they are not yet retired here —
/// call credit_arrived_convoys after the market step to credit and remove them.
///
/// @param w  World; convoy progress fields are mutated in place.
void advance_convoys(world& w);

/// Credit and retire all arrived convoys: add cargo_qty of cargo_resource to the
/// destination (corp, dest market) pool (BL-1003), increase the destination market's supply for that
/// resource (so the next clearing pass reprices), and erase the convoy from
/// world.convoys. Called after clear_markets so the market supply injection takes
/// effect at the *next* tick's clearing pass.
///
/// Also **upserts a persistent trade_route** (BL-088) for each completed inter-body
/// lane before the convoy is erased: the unordered (source-body, dest-body) pair for
/// the convoy's corp gets `last_tick = tick` and its `convoy_count` incremented
/// (a new route is created on first traffic). Intra-body convoys (source and dest on
/// the same body) record nothing. Routes are never erased here — the commercial-sphere
/// fog (BL-089) ages them at read time.
///
/// Runs `intercept_convoys` FIRST (BL-458), so a cut convoy never reaches the
/// crediting loop below and never delivers.
///
/// @param w        World; pools, market supply, convoys, and trade_routes are mutated.
/// @param tick     Current sim day tick, stamped onto routes as last-traffic time. The
///                 default keeps pre-BL-088 callers (and pure market/pool tests) compiling.
/// @param out_cuts Optional sink for this tick's interceptions, APPENDED to in the
///                 order `intercept_convoys` cut them. Before this existed the records
///                 were computed and dropped on the floor (`(void)intercept_convoys`),
///                 which is why interdiction shipped SILENT — NR-407. A convoy that is
///                 cut is erased here, so this is the ONLY moment the fact exists;
///                 nothing downstream can reconstruct it. Null (the default) keeps
///                 every existing caller compiling and discards as before.
void credit_arrived_convoys(world& w, int tick = 0,
                            std::vector<interception_record>* out_cuts = nullptr);

// ---------------------------------------------------------------------------
// The dispatcher RETIRED (BL-1265 / BL-1266; TRADE.md § What trade replaces):
// the corporation convoy (a pool hauling its own surplus) and the market export
// went with corporation pools. A convoy is now only ever a TRADE's shipment
// (trade.hpp, `run_trades`), bought off one shelf and landed on another.
// ---------------------------------------------------------------------------

/// Last resolved price of good `r` in `mc`, base price as the fallback; 0 when
/// the market does not price the good at all.
float dispatch_market_price(const market_component& mc, std::size_t r);

/// Supply `dest` can absorb before its UNSMOOTHED target price (`price_target`)
/// falls to `landed_cost`: S* - S with S* = D x (base / landed)^2, clamped by the
/// price band (+infinity when landed sits below the band's floor, 0 at or above
/// its ceiling or with no demand); a zero-supply market absorbs its unmet demand.
/// The derivation is written out at the definition. Auto trade sizes a route by
/// it (TRADE.md § Auto and reserved trade).
float dispatch_absorbable(const world& w, const recipe_registry& reg, entity_id dest,
                          std::size_t r, float landed_cost);

/// Supply already on its way into `dest`'s next clear: EVERY owner's cargo
/// bound there (held convoys included) plus this tick's landings on it.
float trade_pending(const world& w, entity_id dest, std::size_t r);

/// max(0, absorbable - pending): what one more shipment into `dest` may carry
/// at `landed_cost` and still sell above it.
float trade_room(const world& w, const recipe_registry& reg, entity_id dest, std::size_t r,
                 float landed_cost);

// ---------------------------------------------------------------------------
// The shipment (TRADE.md § A trade; SUPPLY.md)
// ---------------------------------------------------------------------------
// A trade buys on one shelf, pays the haul, and lands on another. Pricing the
// leg and committing the shipment are the two functions below, shared by every
// trade — manual or auto, the player's or a rival's — so a player's shipment and
// a rival's of the same shape cost the same and travel at the same speed.

/// BL-148/149 logistics-node lookups. `pop_tile_scale` maps a population
/// centre's tile to its scale (tier 1–5 — cities are free hubs); `hub_tiles`
/// holds every completed, active inland_logistics_hub's tile. An intra-body
/// haul is discounted for each such node its A* path crosses.
///
/// Built once per trade pass (it is a walk of every building).
struct logistics_nodes
{
    std::unordered_map<entity_id, int> pop_tile_scale;
    std::unordered_set<entity_id>      hub_tiles;
};

logistics_nodes collect_logistics_nodes(const world& w);

/// One priced candidate leg: what hauling `qty` of resource index `ri` from
/// the source market's shelf to `dest_market_id` would cost, in credits and in
/// econ ticks.
struct convoy_leg
{
    /// False when the lane cannot be run at all — no reachable path, no
    /// launchpad on the source body, no propellant on its shelf to launch with,
    /// or a cost that is not a finite number. Nothing was mutated.
    bool        viable       = false;
    convoy_mode mode         = convoy_mode::land;
    float       cost         = 0.0f; ///< Total credits the haul costs (already node-discounted).
    int         travel_ticks = 1;    ///< Econ ticks the leg takes; convoy speed is 1/this.
    /// BL-1195: the route's waypoints, copied onto the convoy at commit so its lane
    /// follows the legs priced here (convoy_component::origin_tile / port_a / port_b).
    /// All null on a space lane; the ports null on a single overland leg.
    entity_id   origin_tile  = null_entity;
    entity_id   port_a       = null_entity;
    entity_id   port_b       = null_entity;
};

/// Price one trade leg from @p src_market's shelf to @p dest_market_id for
/// @p corp_id. A pure read of the world apart from the A* path cache, which is
/// why `w` is non-const. Same body: `price_market_leg`, centre to centre,
/// corporation-independent. Between bodies (TRADE.md § A trade, "Between
/// bodies"): the space lane, viable only when @p corp_id holds a Launchpad on
/// the source body AND the source shelf holds the launch's propellant
/// (`launch_draw_per_convoy`) under the fair-price ceiling. Never viable for a
/// grid good (BL-708), a market to itself, an unknown market or corp, or a
/// non-finite / non-positive @p qty.
convoy_leg price_trade_leg(world& w, const recipe_registry& reg,
                           const logistics_nodes& nodes, entity_id corp_id,
                           entity_id src_market, entity_id dest_market_id,
                           std::size_t ri, float qty);

/// The corporation-independent SAME-BODY leg of `qty` units from `src_market`'s
/// centre to `dest_market`'s centre — the intra-body router, port gate, node
/// discount and handling every shipment uses. Not viable across bodies, from or
/// to an unanchored market, or to itself.
///
/// THE ONE QUESTION "is this pair viable" (BL-1186): viable exactly when the pair
/// routes overland, or land -> port -> sea -> port -> land through two active Ports
/// (SUPPLY.md § Logistical cost) — the ports need not sit on either market centre.
/// BL-1185's placement asks this same call, so placement and shipping cannot disagree.
convoy_leg price_market_leg(world& w, const recipe_registry& reg,
                            const logistics_nodes& nodes, entity_id src_market,
                            entity_id dest_market, float qty);

/// True if `corp` has a launchpad building whose tile is on `body`.
bool corp_has_launchpad_on(const world& w, const corporation_component& corp, entity_id body);

/// The goods ONE space-lane launch burns (BL-308) — per LAUNCH, not per tonne and
/// not per AU: the pad is the thing being fuelled, so a launch costs the same
/// whatever it carries. BL-1265: the trader BUYS it off the source shelf.
///
/// EXPORTED, AND AS A VECTOR, because of BL-648: `tools/verify/chain_depth.cpp`
/// R1 resolves each consumer exemption against a registry of the passes that
/// REALLY draw. This is THE definition: `price_trade_leg` gates on this vector
/// and `commit_trade_shipment` buys it, so the draw and the thing the registry
/// reads are one object.
const std::array<float, resource_count>& launch_draw_per_convoy();

struct economy_report; // economy_system.hpp: the tick's want and fill registers

/// Commit a priced trade shipment (TRADE.md § A trade, steps 1-2): BUY up to
/// @p qty of good @p ri off @p src_market's shelf at its posted price, under the
/// fair-price ceiling (into `report.purchases`, billed by the clear, and
/// `report.wants`, so the buying reads to the price as any bid does); on a space
/// leg also buy the launch's propellant there; PAY THE HAUL from the trader's
/// balance; and append the convoy carrying the cargo to @p dest_market_id, where
/// it lands and is sold on arrival (`credit_arrived_convoys`). The ONE place a
/// `convoy_component` is created.
///
/// THE SEND is the least of @p qty, the shelf (less the launch's own propellant
/// when the cargo is that good) and — on a same-body leg — what the PASSIVE
/// Logistic Point anchor nearest the source market's centre still admits this
/// tick (BL-597, by cargo quantity; BL-1186 E1 trims rather than refuses).
/// Trade points are the owner's capacity; LP the place's; a shipment passes both.
/// A trimmed cargo pays its share of the leg's cost (cost is linear in quantity).
///
/// All-or-nothing on refusal: returns false, having mutated nothing, when the
/// leg is not viable, the ceiling refuses the shelf, the shelf or the LP anchor
/// admits nothing, or the trader cannot cover the haul plus the purchase.
///
/// @param shared_lp_pools  BL-597: the tick's shared LP pool, so passive draws
///                         contend with the same tick's marches. Null: private.
/// @param out_refused_no_lp Optional; set true (never false) when this call
///                         refused SPECIFICALLY for want of passive LP.
/// @param out_sent         Optional; the units actually sent on success.
/// @param io_committed     Optional; the purchases this trader has already
///                         committed this tick (billed at the clear, not yet
///                         off its balance). The solvency gate weighs the
///                         balance LESS this, and a success adds this
///                         shipment's purchase to it — so a trader shipping
///                         many routes in one pass cannot overdraw (cold
///                         review). Null: the balance alone.
bool commit_trade_shipment(world& w, const recipe_registry& reg, economy_report& report,
                           entity_id corp_id, entity_id src_market, entity_id dest_market_id,
                           std::size_t ri, float qty, const convoy_leg& leg,
                           lp_pool_map* shared_lp_pools = nullptr,
                           bool* out_refused_no_lp = nullptr,
                           float* out_sent = nullptr,
                           float* io_committed = nullptr);
