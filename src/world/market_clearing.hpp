#pragma once

#include "components.hpp"
#include "economy_system.hpp"

#include <algorithm>
#include "recipe_registry.hpp"
#include "world.hpp"

#include <array>
#include <cstdint>
#include <map>
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

/// BL-1163 (play villages decline): the household MET RATIO at `market` — the
/// share of the population channel's own bid that the market's last clear
/// filled (docs/economy/POPULATION.md § Growth, decline and razing, "The growth
/// basket IS the household basket"). The goods and weights are exactly what
/// `inject_population_demand` bids: `reg.population_demand_basket()` (shared
/// tranche + the band's tranche, era-masked by the registry's fold), less any
/// good the market leaves unpriced (`base_price <= 0`, which the bid skips).
///
/// THE FILL (BL-1196, households consume). The households clearing at a market
/// physically draw their pooled bid off its shelf at the end of each clear
/// (`draw_household_basket`): `household_fill[r] = min(household_bid[r],
/// inventory[r])`. The share is `household_fill / household_bid` — what the
/// people received over what they bid — read from the last clear's two
/// registers (both serialised, world_save_version 35, because the growth pass
/// reads them before the next clear rewrites them).
///
/// Returns the basket-weighted mean of the per-good share; 1.0 when nothing
/// was bid (no market, or no clear yet) — nothing unmet. Pure; deterministic.
///
/// @p recorded (optional) is set false in exactly one case: the market prices
/// at least one basket good but holds NO recorded household bid on any of them
/// -- a market made since the last clear. The 1.0 returned then is "no
/// reading", and the growth gate carries the centre's streak (no growth or
/// decline step) rather than reading it met. A market pricing no basket good,
/// or a missing market, reports recorded = true with 1.0 (nothing to want).
float population_met_ratio(const world& w, const recipe_registry& reg, entity_id market,
                           bool* recorded = nullptr);

/// BL-1196 (households consume; POPULATION.md § Population demand): every
/// market's households TAKE their bid off its shelf — `household_fill[r] =
/// min(household_bid[r], inventory[r])`, decremented from `inventory`. No money
/// moves (the market paid the maker when it bought the stock). Ignores the
/// processor fair-price ceiling: the household's reservation is its elastic
/// bid. Called by clear_markets after every sale has credited the shelf and
/// before the price update. Ascending market id, ascending resource; pure
/// per-market arithmetic. Deterministic.
void draw_household_basket(world& w);

/// BL-1217 lever D (measurement switch `economy.background_demand.consumes`,
/// default false). When on, every market's background basket TAKES its bid
/// (`background_bid`, written by inject_background_demand) off the shelf after
/// the households' draw: `background_fill[r] = min(bid, inventory[r])`. No
/// money moves; no ceiling. When off, the shelf is untouched and
/// `background_fill` reads zero. Ascending market id and resource.
void draw_background_basket(world& w, const recipe_registry& reg);

/// BL-1179 (shelf spoilage; MARKETS.md § The shelf spoils). Every good on every
/// market's SHELF loses its authored share of itself:
/// `inventory[r] -= inventory[r] × reg.shelf_spoilage()[r]`. Only the shelf —
/// a corp pool never spoils. NO MONEY MOVES: the market paid the maker when it
/// bought the stock, so a spoiled unit simply leaves, as an eaten one does. A
/// drain, never a price: called by clear_markets after the households' draw
/// (the tick's last draw) and before the next read of the shelf's share of
/// supply (next tick's reference prices, and the dispatch's pricing read). Ascending market id, ascending resource; a zero rate is skipped, so
/// a registry that authors no rates leaves the shelf bit-identical.
void spoil_market_shelves(world& w, const recipe_registry& reg);

/// BL-1179: every good some market PRICES (`base_price > 0` anywhere) whose
/// shelf-spoilage rate is zero — an authoring gap, since the ruling is that
/// every good on a shelf spoils. The `unpriced_basket_entries` pattern:
/// reported once at campaign start (app.cpp); ascending resource id.
std::vector<resource_type> unspoiled_priced_goods(const world& w, const recipe_registry& reg);

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
/// @param w      World; markets and (corp, market) pools are mutated, and the
///               standing order book (`sell_orders` / `buy_orders`) is read.
/// @param reg    Loaded registry (for processor input reservations).
/// @param report Economy step report (its purchases drive the buy side).
/// @return       Per-corporation cash flow valued at matched prices.
std::unordered_map<entity_id, corp_cash_flow> clear_markets(
    world& w,
    const recipe_registry& reg,
    const economy_report& report);

/// BL-1201 (orders are price floors, Ben 2026-10-05; MARKETS.md step 4): a
/// standing sell order whose POOL has held no surplus — no stock above the
/// processor reservation in any of its corp's market pools on its body, read
/// before any order's claim — for this many consecutive clearing ticks is removed
/// by `clear_markets`, and the good returns to auto-surplus. The same rule for
/// the player and for rival corps.
///
/// WHY 4 (one year of quarterly ticks): a pool fed by convoys or by a processor
/// whose input comes and goes can stand empty for a tick or two between
/// deliveries while the order is still wanted, so 1-2 would close an order the
/// next delivery needs and make its owner place it again. Much longer leaves a
/// dead order governing a good for years. Four quarters rides out a delivery gap
/// and returns an abandoned good to auto-surplus inside a year. It equals the
/// rival scorer's evaluation cadence (corp_ai_params::cadence_k = 4), so a run
/// of empty ticks spans one full look by every rival.
inline constexpr uint8_t sell_order_empty_close_ticks = 4;

/// The EMA factor `resolve_price` eases a market price toward its target by, each
/// clear: next = prior + k_price_smoothing x (target - prior). Exported for
/// BL-1203's first-clear guard in `dispatch_absorbable` (supply_system.cpp), which
/// projects the price a cargo will SELL at on its arrival tick.
inline constexpr float k_price_smoothing = 0.5f;

/// The UNSMOOTHED price a market aims at for one good this tick: `base x
/// sqrt(demand / supply)`, `base x ceil` for demand with no supply, `base` with
/// neither, clamped to [floor_mult, ceil_mult] x base. `resolve_price` eases the
/// market price toward this; BL-995's dispatch sizes a haul against it directly
/// (the eased price lags, and a size read off it overshoots every tick).
float price_target(float base, float supply, float demand,
                   float price_floor_mult, float price_ceil_mult);

/// BL-1172 — THE SHELF IS SUPPLY, AS FAR AS IT CAN SELL (MARKETS.md § Price
/// resolution, Ben 2026-10-03): the supply the price law reads for good `r`
/// on `m` is the listings recorded in `supply` PLUS the shelf's share,
/// `min(inventory, k x demand)` with k = `price_band_params::
/// shelf_supply_ticks` — the stock this market's demand would take off the
/// shelf within k ticks. A shelf larger than that is a glut, and counting it
/// whole floors the market's prices (measured: the field fell to 1/1/11 firms
/// on seeds 0/10/28). k <= 0 is listings only, the law before the ruling —
/// and the shipped k is 0 until shelf spoilage (BL-1179).
/// Every caller of `price_target` passes this, never `supply` alone:
/// clearing's resolution and dispatch's haul sizing (`dispatch_absorbable`)
/// aim at the same law. TWO OTHER READERS OF THE PRICE LAW STILL READ
/// LISTINGS ONLY (`market_component::supply`): the workforce scorer's
/// `wf_target_price` (economy_system.cpp) and corp_ai's glut forecast
/// (corp_ai.cpp, the projected-supply / demand ratio). At the shipped k = 0
/// they agree with this; if k is raised they diverge, and should be moved
/// onto `pricing_supply` with it.
///
/// WHICH INVENTORY, WHICH DEMAND. Each is read off `m` as it stands when the
/// caller asks, and both callers ask at the same point of the tick: after the
/// tick's draws, before clearing credits this clear's listings to the shelf —
/// so a listing counts once, as a listing, in the tick it is made, and as
/// shelf from the next tick until it is drawn.
///   * clear_markets resolves after its demand phase, so `demand` is THIS
///     tick's full register: every channel's want, after the no-bid rule (a
///     draw over the fair-price ceiling never entered it), population,
///     background, endemic, interbody and standing buy orders.
///   * dispatch runs before that clear, so `demand` is still the LAST clear's
///     register (clearing zeroes it only at its own top) — the same D it
///     already sizes a haul against, paired here as it always was with the
///     last clear's listings.
/// Negative figures read as zero. Pure; deterministic.
/// BL-1209: the cap is k x (demand + suppressed want), `hauler_want` (above).
inline float pricing_supply(const market_component& m, std::size_t r, float shelf_supply_ticks)
{
    const float listed = std::max(0.0f, m.supply[r]);
    if (!(shelf_supply_ticks > 0.0f))
        return listed;
    const float shelf = std::max(0.0f, m.inventory[r]);
    // BL-1209 (MARKETS.md § Price resolution, "The shelf's share reads the want
    // the ceiling silenced", Ben 2026-10-07): the cap counts the SUPPRESSED want
    // (`hauler_want`, BL-1203's register — processor inputs and construction
    // materials unbid over the fair-price ceiling) beside `demand`. It is read
    // HERE ONLY: it never enters `demand`, never bids, never pays; it only lets
    // a stocked shelf count as the supply it is, so a shelf priced over the
    // ceiling against silenced buyers can fall back to where they return. With
    // an empty shelf the min is 0 and the price law is exactly the base's; with
    // k = 0 the early return above means it is never read at all. Both callers
    // read the register at the same point they read `demand`: clearing after it
    // rewrites both this tick, dispatch the last clear's.
    const float wants = std::max(0.0f, m.demand[r]) + std::max(0.0f, m.hauler_want[r]);
    const float sells = shelf_supply_ticks * wants;
    return listed + std::min(shelf, sells);
}

/// BL-1230 (power crosses markets; LOGISTICS.md § 3a): a grid good that crosses
/// markets (`grid_good_crosses_markets`) prices against its GRID's pooled
/// registers, not one market's. `grid_good_figures` is one grid's three pooled
/// registers (listings, shelf, demand + silenced want) and its pooled demand;
/// `grid_good_pool` maps every market whose centre is wired to its grid and
/// every grid to its figures. Built by `pool_grid_good_figures` — THE ONE
/// pooling, read by clear_markets' price resolution and by the workforce
/// solver's price forecast (BL-1232) alike. Sums run over ascending market id.
struct grid_good_figures
{
    std::array<float, resource_count> listed{}, shelf{}, wants{}, demand{};
};
struct grid_good_pool
{
    std::map<entity_id, std::uint32_t>          market_grid;
    std::map<std::uint32_t, grid_good_figures> grid_sd;
};
grid_good_pool pool_grid_good_figures(world& w, const recipe_registry& reg);

/// BL-1232 review: the grid a building on @p tile FEEDS. A producer lists into
/// its tile's market (`market_for_tile`), and a market's shelf is on the grid of
/// its CENTRE tile's province (LOGISTICS.md § 3a; the grid clear's
/// `shelves_on`) — so a generator serves the grid of its market's centre, which
/// need not be its own tile's grid. 0 when the tile has no market or the
/// centre's province is dark. (Its DRAW side is its own tile's grid,
/// `tile_power_grid`.)
std::uint32_t tile_feed_power_grid(world& w, entity_id tile);

/// The pooled pricing supply: listed + min(shelf, k x wants) over the GRID —
/// the shelf cap taken once, at the grid (review round 2 of BL-1230). At one
/// market on a grid this is exactly `pricing_supply`.
inline float grid_good_pricing_supply(const grid_good_figures& sd, std::size_t r,
                                      float shelf_supply_ticks)
{
    return sd.listed[r] + ((shelf_supply_ticks > 0.0f)
                               ? std::min(sd.shelf[r], shelf_supply_ticks * sd.wants[r])
                               : 0.0f);
}

/// Input reservation a corporation needs to keep in ONE goods pool to feed a
/// full run of the processors that draw that pool next tick — so it sells only
/// the genuine surplus. BL-1003: a processor draws the pool of its own tile
/// market (`pool_key_for_tile`), so only processors keyed to @p pool_key
/// reserve against it. BL-995: shared by clearing's auto-surplus and by
/// `dispatch_convoys`, so what a seller may haul is exactly what it would list.
/// @pre `corp` is a key of `w.corporations`.
std::array<float, resource_count> processor_reservation(
    const world& w, const recipe_registry& reg, entity_id corp, entity_id pool_key);

/// What AUTO-SURPLUS holds back in one pool: `processor_reservation`, plus — where
/// the corp holds a Launchpad that burns from this pool (`launch_burns_from_pool`,
/// the launch gate's own test) — the WHOLE stock of every launch-drawn good
/// (`launch_draw_per_convoy`: propellant). "A pad's pool keeps its propellant"
/// (MARKETS.md step 4, Ben 2026-10-09): auto-surplus lists none of it, so a pad
/// stays fuelled. A pool with no pad reserves exactly `processor_reservation`.
/// Shared by clearing's auto-surplus and `dispatch_convoys` (BL-995: what a
/// seller may haul is exactly what it would list). A STANDING SELL ORDER reads
/// `processor_reservation` instead, so the corp can still sell its propellant.
/// @pre `corp` is a key of `w.corporations`.
std::array<float, resource_count> auto_surplus_reservation(
    const world& w, const recipe_registry& reg, entity_id corp, entity_id pool_key);

/// Resolve which market a tile clears against (its market catchment). Among the
/// markets on the tile's body: a body with a single market routes there
/// unconditionally; with several, the tile clears against the market whose
/// `centre_tile` is nearest by grid distance (ties → lowest market id; markets
/// with no centre are ignored when an anchored one exists). Returns `null_entity`
/// if the tile's body has no market.
entity_id market_for_tile(const world& w, entity_id tile);

/// BL-1125: the same answer as `market_for_tile`, computed by the full scan
/// over the body's original centres every call (no catchment raster). For
/// verification only -- the raster must route identically on every tile.
entity_id market_for_tile_scan(const world& w, entity_id tile);
