#pragma once

#include "recipe_registry.hpp"
#include "logistics.hpp" // lp_pool_map, nearest_lp_anchor, lp_pool_for_body (BL-597)
#include "world.hpp"

#include <cstddef>
#include <array>
#include <cstdint>
#include <map>
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

/// Auto-dispatch: the SELLER chases a NET PRICE (BL-995, SUPPLY.md § Dispatch
/// trigger). For every (corp, market) pool — ascending key — holding a good above
/// its processor reservation, at LAST tick's resolved prices:
///   net(d) = price_d - haul_per_unit(src -> d)
/// and the good goes to argmax_d net(d) (ties to the lower market id) only if
/// net(d) - price_src > reg.dispatch_margin() x price_src. The quantity is
/// min(surplus, q) less what the corp already has in transit to d, with
/// q = supply_d x ((price_d / landed_cost)^2 - 1), or d's unmet demand when d
/// has no supply. Runs AFTER run_economy_step and BEFORE clear_markets, so the
/// seller hauls before auto-surplus sells at home. One rule for every corp,
/// the player's included. There is no shortfall scan: a short market prices
/// the good high and this rule reaches it.
///
/// Space-mode convoys require a building_type::launchpad in the source corp's assets
/// on the source body. Land-mode is ungated. Sea mode is selected automatically when
/// the intra-body path crosses ocean (path.crosses_ocean — see docs/economy/SUPPLY.md);
/// air mode is not dispatched in the prototype.
///
/// @param w         World; convoys are appended and source pools debited.
/// @param reg       Registry (for building type lookups).
/// @param logistics_cost_land   base_cost_per_unit_distance for land mode.
/// @param logistics_cost_space  base_cost_per_unit_distance for space mode.
/// @param shared_lp_pools BL-597: forwarded to every `commit_convoy` call this
///        pass makes. Null (the default) builds one private `lp_pool_map`
///        local to this call, shared across every convoy THIS pass commits
///        (so two hauls drawing on one anchor within one
///        `dispatch_convoys` call already contend, mirroring how
///        `run_unit_march` shares one pool across all its units) but
///        discarded before the caller gets it back — the real per-tick driver
///        passes its own instance to see that same pool drawn down further by
///        this tick's `run_unit_march` call, which is what makes "war flips
///        the queue" observable.
/// @return This pass's counters — dispatched legs and passive-LP refusals,
///        same shape/intent as `unit_march_tick` (BL-596). The auto-dispatch
///        path had NO surfacing at all before this (not even for the
///        pre-existing insolvency refusal `commit_convoy` already gated on);
///        this is the first cut, matching BL-596's own counter-on-the-tick-
///        summary convention rather than inventing a narration pathway.
struct convoy_dispatch_tick
{
    int dispatched    = 0; ///< Convoys committed this pass.
    int refused_no_lp = 0; ///< BL-597: shortfalls refused for want of passive LP.
    /// BL-1071: convoys a MARKET sent from its own shelf this pass (owner
    /// null_entity), counted apart — `dispatched` stays corporations' convoys.
    int market_exports = 0;
};

convoy_dispatch_tick dispatch_convoys(world& w, const recipe_registry& reg,
                      float logistics_cost_land, float logistics_cost_space,
                      lp_pool_map* shared_lp_pools = nullptr);

// ---------------------------------------------------------------------------
// BL-995 — the net-price rule's sizing, shared by dispatch_convoys and the rival
// scorer's directed dispatch (corp_ai.cpp), so both size a haul identically.
// ---------------------------------------------------------------------------

/// Per-pass memo of `processor_reservation` keyed (corp, pool key).
using reservation_memo = std::map<std::pair<entity_id, entity_id>, std::array<float, resource_count>>;

/// Last resolved price of good `r` in `mc`, base price as the fallback; 0 when
/// the market does not price the good at all.
float dispatch_market_price(const market_component& mc, std::size_t r);

/// The HOME price of a pool: its market's last resolved price, or 0 for a
/// body-level pool (no market to sell into) or a good its market does not price.
float dispatch_home_price(const world& w, entity_id src_key, std::size_t r);

/// Supply `dest` can absorb before its UNSMOOTHED target price (`price_target`)
/// falls to `landed_cost`: S* - S with S* = D x (base / landed)^2, clamped by the
/// price band (+infinity when landed sits below the band's floor, 0 at or above
/// its ceiling or with no demand); a zero-supply market absorbs its unmet demand.
/// The derivation is written out at the definition.
float dispatch_absorbable(const world& w, const recipe_registry& reg, entity_id dest,
                          std::size_t r, float landed_cost);

/// Supply already on its way into `dest`'s next clear: EVERY corp's convoys
/// bound there (held ones included), plus every corp's stock in (corp, dest)
/// above its processor reservation. `sorted_corp_ids` fixes the float order.
float dispatch_pending(const world& w, const recipe_registry& reg, entity_id dest,
                       std::size_t r, const std::vector<entity_id>& sorted_corp_ids,
                       reservation_memo& memo);

/// max(0, absorbable - pending): what one more haul into `dest` may carry.
float dispatch_room(const world& w, const recipe_registry& reg, entity_id dest, std::size_t r,
                    float landed_cost, const std::vector<entity_id>& sorted_corp_ids,
                    reservation_memo& memo);

/// This tick's delivery of good `r` into the (corp, key) pool
/// (`world::arrived_this_tick`) — not shippable until it has met a clear.
float dispatch_arrived(const world& w, entity_id corp, entity_id key, std::size_t r);

/// BL-1071 (shelf stock moves): the stock of good `r` on `market`'s SHELF
/// (`market_component::inventory`) the market may export this pass — the shelf
/// less what its own consumers need (last clear's demand for the good there),
/// less this tick's shelf deliveries; never negative. The reading and why it was
/// chosen are written out at the definition. `dispatch_convoys` exports it, after
/// every corporation's dispatch, by the same net-price rule: same body only, from
/// the market's centre, into the room left once every pending cargo AND the
/// destination's own shelf are subtracted. The convoy's owner is `null_entity`;
/// it lands on the destination shelf, and no balance moves (the market holds no
/// treasury — the haul is the margin the export gives up).
float market_shelf_surplus(const world& w, entity_id market, std::size_t r);

// ---------------------------------------------------------------------------
// The shared dispatch (BL-452)
// ---------------------------------------------------------------------------
// dispatch_convoys above is TWO things bolted together: the net-price rule
// (BL-995) that decides *what to haul from where to where*, and the dispatch
// itself — price the leg, commit the cargo, put a convoy on the lane. Only the
// first half is the auto-dispatcher's own opinion. The second half is what a
// convoy IS, and the player's `dispatch_convoy` verb (corp_command.hpp) needs
// exactly it with the rule removed.
//
// So it is factored out here rather than reimplemented there. There is no
// fourth code path: `dispatch_convoys` and `apply_corp_command` call the same
// two functions with the same arguments, so a player's convoy and a rival's of
// the same shape cost the same and travel at the same speed — an assertion
// tools/verify/convoy_command.cpp makes directly, because a silent divergence
// here is exactly the bug a copy would introduce.

/// BL-148/149 logistics-node lookups. `pop_tile_scale` maps a population
/// centre's tile to its scale (tier 1–5 — cities are free hubs); `hub_tiles`
/// holds every completed, active inland_logistics_hub's tile. An intra-body
/// haul is discounted for each such node its A* path crosses.
///
/// Built once per auto-dispatch pass (it is a walk of every building), and once
/// per player command — a single press can afford the walk.
struct logistics_nodes
{
    std::unordered_map<entity_id, int> pop_tile_scale;
    std::unordered_set<entity_id>      hub_tiles;
};

logistics_nodes collect_logistics_nodes(const world& w);

/// One priced candidate leg: what hauling `qty` of resource index `ri` from
/// source pool `src_key` to `dest_market_id` would cost, in credits and in econ ticks.
struct convoy_leg
{
    /// False when the lane cannot be flown at all — no production anchor, no
    /// reachable path, no launchpad on the source body, no propellant to launch
    /// with, or a cost that is not a finite number. Nothing was mutated.
    bool        viable       = false;
    convoy_mode mode         = convoy_mode::land;
    float       cost         = 0.0f; ///< Total credits the haul costs (already node-discounted).
    int         travel_ticks = 1;    ///< Econ ticks the leg takes; convoy speed is 1/this.

};

/// Price one leg. A pure read of the world apart from the A* path cache, which
/// is why `w` is non-const. Mutates no game state and creates nothing.
///
/// @param nodes                From collect_logistics_nodes; the intra-body discount source.
/// @param src_key              BL-1003: the SOURCE POOL KEY — the market whose (corp, market)
///                             pool the cargo leaves, or a body id for a market-less body's
///                             body-level pool. The source body is `pool_key_body(src_key)`;
///                             the intra-body origin is `convoy_origin_tile(src_key)`; the
///                             launch draw reads this pool. `src_key == dest_market_id`
///                             (a haul into the market the goods already sit in) answers
///                             `viable = false`.
/// @param ri                   Resource index; out-of-range answers `viable = false`.
/// @param qty                  Units of cargo. Non-finite or non-positive answers `viable = false`.
/// @param logistics_cost_space Space-lane base cost per unit distance per unit cargo. Every
///                             caller passes `reg.logistics_cost(convoy_mode::space)`; it stays a
///                             parameter only because `dispatch_convoys` has always taken it.
convoy_leg price_convoy_leg(world& w, const recipe_registry& reg,
                            const logistics_nodes& nodes, entity_id corp_id,
                            entity_id src_key, entity_id dest_market_id,
                            std::size_t ri, float qty, float logistics_cost_space,
                            const entity_id* known_origin = nullptr);
// BL-1079 (live tick speedups): `known_origin`, when given, is the caller's
// already-resolved `convoy_origin_tile(corp, src_key)` — the SAME value this
// function would compute (null_entity included), handed in so a caller pricing
// one pool against every destination resolves it once per pool, not once per
// leg. It must be computed against the world as it stands; nothing between the
// two may move the corp's buildings or the market set.

/// BL-1071: price a MARKET's own export leg of `qty` units from `src_market`'s
/// centre to `dest_market`'s centre — the same intra-body routing, sea gate, node
/// discount and cost a corporation's leg uses (price_convoy_leg), with no
/// corporation, launchpad or propellant. Not viable across bodies, from or to an
/// unanchored market, or to itself.
convoy_leg price_market_export_leg(world& w, const recipe_registry& reg,
                                   const logistics_nodes& nodes, entity_id src_market,
                                   entity_id dest_market, float qty);

/// Tile of the corp's lowest-id building on `body` (BL-077's production anchor).
/// `null_entity` if the corp holds nothing on the body.
entity_id corp_representative_tile(const world& w, const corporation_component& corp, entity_id body);

/// BL-1003 — where a haul out of the source pool `src_key` starts: for a market
/// pool, the corp's lowest-id building in that market's catchment, else the
/// market's own centre tile (stock that arrived by convoy sits at the market);
/// for a body-level pool, `corp_representative_tile` on that body.
entity_id convoy_origin_tile(const world& w, const corporation_component& corp, entity_id src_key);

/// The goods ONE space-mode convoy launch burns from the dispatching corp's
/// on-body pool (BL-308) — per LAUNCH, not per tonne and not per AU: the pad is
/// the thing being fuelled, so a launch costs the same whatever it carries.
///
/// EXPORTED, AND AS A VECTOR, because of BL-648 rather than for generality's
/// own sake. `tools/verify/chain_depth.cpp` R1 no longer accepts a prose string
/// as proof that a good has a consumer; it resolves each exemption against a
/// registry of the passes that REALLY draw, and a private `constexpr float` in
/// supply_system.cpp's anonymous namespace could only reach that registry as a
/// second hand-maintained copy — precisely the loophole BL-648 closes. This is
/// THE definition: `price_convoy_leg` gates on this vector and `commit_convoy`
/// debits it, so the draw and the thing the registry reads are one object, and
/// naming a second launch consumable is a one-line change here that the
/// registry picks up with no harness edit at all.
const std::array<float, resource_count>& launch_draw_per_convoy();

/// Commit a priced leg: debit the corp's balance and its source pool, burn the
/// launch's propellant on the space lane, and append the convoy. The ONE place
/// a `convoy_component` is created.
///
/// All-or-nothing. Returns false — having mutated nothing — when the leg is not
/// viable or the corp cannot afford `leg.cost`. The propellant availability
/// gate lives in `price_convoy_leg`, so a viable space leg cannot drive the
/// propellant pool negative here.
///
/// @param src_body   The body the cargo leaves (the passive-LP gate's body).
/// @param src_market BL-1003: the SOURCE MARKET — the pool debited is (corp, src_market),
///                   and it is recorded as the convoy's `source_market`. `null_entity`
///                   (or any non-market id) only for a market-less source body, whose
///                   body-level pool (corp, src_body) is debited instead. Must be the
///                   same source `price_convoy_leg` priced (its `src_key`).
///
/// BL-597 (LOGISTICS.md § Logistic Points): before any mutation, an
/// intra-body leg (`leg.mode != convoy_mode::space`) must also clear the
/// PASSIVE-LP admissibility gate — LOGISTICS.md rule 1, "LP is a CAP, not a
/// PRICE": no second credit charge, `leg.cost` (haulage) stays the only
/// price, LP only decides whether the leg is admissible at all. The corp's
/// dispatch tile (`convoy_origin_tile` of the source pool, the same
/// origin `price_convoy_leg` routes from) draws against its NEAREST anchor's
/// pool (`nearest_lp_anchor`, logistics.hpp — the same reduction BL-596's
/// active march gate uses), by the leg's CARGO QUANTITY (Ben, 2026-08-25,
/// ruling on NR-620): one passive LP admits one unit of goods through the
/// anchor, as one active LP admits one march-point's worth of movement.
/// Deliberately NOT distance — LOGISTICS.md constraint 3 ("if cost is
/// proportional to distance, LP *is* haulage cost again") and rule 1
/// (distance is already priced, in credits) both forbid that, and measured
/// it collapsed real convoy traffic by 73%.
/// A space leg (inter-body) skips this entirely: LOGISTICS.md's Logistic
/// Points design is tile-grounded infrastructure ("cities are the locus"),
/// out of scope for a lane with no intra-body path at all — matching
/// BL-596's own march gate, which likewise only fires for a unit walking a
/// tile path.
///
/// @param reg              For `reg.military().active_lp_per_anchor_tick` — the
///                         ONE per-anchor LP rate LOGISTICS.md's bifold table
///                         splits by use, not two authored numbers; see that
///                         field's own doc comment (recipe_registry.hpp).
/// @param shared_lp_pools  BL-597: forwarded to (and lazily built through)
///                         `lp_pool_for_body`, same contract as
///                         `run_unit_march`'s parameter of the same name —
///                         null (the default) gets a private, per-call-fresh
///                         pool; a non-null instance shared with the same
///                         tick's `run_unit_march` call makes active and
///                         passive draws genuinely contend.
/// @param out_refused_no_lp Optional; set true (never false) when this call
///                         refused SPECIFICALLY for want of passive LP,
///                         distinct from `false` returned for any other
///                         reason (not viable, insolvent). Mirrors
///                         `unit_march_tick::refused_no_lp`'s naming.
bool commit_convoy(world& w, const recipe_registry& reg, entity_id corp_id, entity_id src_body,
                   entity_id src_market, entity_id dest_market_id,
                   std::size_t ri, float qty, const convoy_leg& leg,
                   lp_pool_map* shared_lp_pools = nullptr,
                   bool* out_refused_no_lp = nullptr);
