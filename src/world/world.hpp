#pragma once

#include "components.hpp"
#include "campaign_battle.hpp" // active_battle (BL-467 battle state, below)
#include "corp_command.hpp" // corp_decision_ring (BL-202 strategic decision log)
#include "era_band.hpp"     // era_band (BL-1101: the world's own recipe band, below)
#include "faithful_unordered_map.hpp" // every unordered store below (BL-1034: copies tick as their source)
#include "law.hpp"          // law (BL-343 enacted-law list, below)
#include "modifier_set.hpp" // scalar_modifier (BL-479 per-corp tech effects, below)
#include "nation_budget.hpp" // nation_budget (Sprint N3 T2: the persistent weight map, below)
#include "province.hpp"     // province_partition (BL-466 province partition, below)
#include "sentiment.hpp"    // sentiment_table (BL-545 relational substrate, below)

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

struct settlement_state; // settlement.hpp — held by pointer below, never by value

/// The system's single asteroid belt — a band between two orbital radii. The
/// belt is not a body (it owns no entity); it is rendered as a thick, translucent
/// textured ring on the Solar canvas, with notable asteroids sitting within it as
/// separate, selectable body entities drawn over the band. A system with no belt
/// has outer_radius_au <= inner_radius_au.
struct asteroid_belt
{
    float inner_radius_au = 0.0f; ///< Inner edge of the band, AU from the star.
    float outer_radius_au = 0.0f; ///< Outer edge of the band, AU from the star.

    /// Whether the system has a belt to draw.
    /// @return True when the band has positive width.
    bool present() const { return outer_radius_au > inner_radius_au && outer_radius_au > 0.0f; }
};

/// BL-1222 (trade-flow lens) — why the player's surplus of a good did not go to a
/// market short of it: the corporation dispatcher's own rules, worst first, so a
/// larger value is CLOSER to sending (LENSES.md § Trade-flow lens). `sent` is not a
/// refusal: some player source shipped the good there this pass.
enum class trade_refusal : std::uint8_t
{
    no_lane = 0, ///< Another body, and no leg off this one is viable.
    gate,        ///< The destination's price does not clear the margin over the source's.
    no_route,    ///< Same body, and no viable leg reaches it.
    costly,      ///< Routed, but the haul eats the margin.
    no_propellant, ///< A space lane, but the pool cannot fuel the launch.
    no_room,     ///< Clears the margin, but cannot absorb more at the landed cost.
    no_funds,    ///< The rule would send, but the corporation cannot pay for the convoy.
    room,        ///< The rule would send; held by one-destination-per-pass or the LP cap.
    sent,        ///< Shipped this pass.
};

/// One player shipment, recorded by `dispatch_convoys` for the Trade-flow lens.
struct trade_flow_shipment
{
    entity_id     source = null_entity; ///< The pool key the cargo left (a market, or a body-level pool).
    entity_id     dest   = null_entity; ///< The destination market.
    std::uint16_t good   = 0;           ///< resource_type index.
    float         units  = 0.0f;        ///< What was actually sent (after any LP trim).
    float         price_d = 0.0f;       ///< The destination price the dispatcher netted against its haul.
};

/// One dispatcher pass's player record: shipments, and the best class per
/// (destination market, good) over the player's surplus sources, for markets short
/// of the good (last clear's demand above its supply).
struct trade_flow_pass
{
    /// The corporation this pass was taken for (the player at the time). The lens
    /// draws only passes whose corp is the one the player holds NOW, so a seat
    /// change never shows the corporation left behind (LENSES.md § Trade-flow lens).
    entity_id corp = null_entity;
    std::vector<trade_flow_shipment> shipments;
    std::map<std::pair<entity_id, std::uint16_t>, trade_refusal> best;
};

/// Result of an intra-body pathfind (BL-077): the terrain-weighted path cost, whether the
/// cheapest path crosses ocean (=> sea mode, else land), and whether the endpoints connect.
/// DIRECTED (BL-1126): a river discounts an edge in one direction, so the cost is the
/// origin -> destination travel cost, read from the destination's flood field and cached under
/// the ORDERED (body, src, dst) key -- never whichever direction a cache happens to hold.
struct logistics_path
{
    float cost          = 0.0f;
    bool  crosses_ocean = false;
    bool  reachable     = false;
    /// The tile sequence of the best path, stored in lo→hi endpoint order — i.e. from
    /// min(src,dst) to max(src,dst) — even though the cost is directed (BL-1126): the
    /// storage order is kept so every reader's orientation step still holds. A caller that
    /// dispatched src→dst reverses this when src != lo. Empty when unreachable; a single tile when src == dst.
    /// Populated by intra_body_path (BL-152, for the convoy vision beam); the cost
    /// fields above stand alone for callers that ignore it.
    std::vector<entity_id> tiles;
};

/// One COMPLETED Dijkstra flood over a body's raster grid, anchored at a single
/// tile: distance, parent index and touched-ocean flag for every cell. The
/// many-to-one layer beneath the per-pair path cache (2026-08-25 warm-start
/// stall): dispatch prices hundreds of origins against the same few destination
/// centres, so one flood per centre answers every pair that touches it at
/// path-reconstruction cost instead of a fresh grid search. A DESTINATION'S
/// field (BL-1126, path cost reads the cache): every edge is priced as the hop
/// TOWARD the anchor, so it answers `intra_body_path(cell, anchor)` for every
/// cell and nothing else — never the reverse route, which a river prices
/// differently. A settled node's parent is final.
struct logistics_flood_field
{
    int                anchor_idx = -1;   ///< Raster index of the anchor tile.
    std::vector<float> dist;              ///< Weighted travel cost cell -> anchor; 1e30f unreached.
    std::vector<int>   came_from;         ///< The cell's next hop toward the anchor, -1 at anchor/unreached.
    std::vector<char>  crossed;           ///< Best path cell -> anchor touches ocean?
};

/// A body's NEAREST-ANCHOR FIELD (BL-1117, settle tick one): for every cell, the
/// Logistic Point anchor nearest it and that anchor's cost, from ONE multi-source
/// Dijkstra seeded at every anchor in `anchors`. It answers `nearest_lp_anchor`
/// for every tile of the body at once; the per-pair loop it replaces flooded the
/// whole body once per anchor (9,038 floods, ~90 s, on seed 0's first convoy).
///
/// A PURE FUNCTION of the body's tiles and of `anchors`: the edges are the flood
/// field's own (each hop priced toward the anchor, BL-1126), so `cost[i]` is exactly
/// the smallest cell-i -> anchor travel cost, and `nearest[i]` an anchor at that least
/// cost, a fixed choice among exact ties (see logistics.cpp § build_lp_anchor_field).
struct lp_anchor_field
{
    std::vector<entity_id> anchors; ///< The anchor tile set it was built over, ascending.
    std::vector<entity_id> nearest; ///< Per raster cell: the nearest anchor tile; null_entity unreached.
    std::vector<float>     cost;    ///< Per raster cell: that anchor's cost; 1e30f unreached.
};

// ---------------------------------------------------------------------------
// World history log (BL-208) — the append-only, serialised world log
// ---------------------------------------------------------------------------
// A SINGLE INTERLEAVED log, not per-body/per-corp logs: those fail on a corp
// acting on a body (every interesting event) and would need a join with no
// shared ordering. Tagged instead, so it projects into either view by
// filtering. Four sources feed it, all additive to their existing consumers:
//   - genesis    — PLANETOLOGY's per-body dated history_event lines, copied in
//                  once at world setup (see seed_genesis_history, history_log.hpp).
//   - checkpoint — planetology_state::checkpoints, migrated alongside genesis
//                  (BL-217 deliberately shaped checkpoint_record so this is a
//                  move, not a rewrite).
//   - decision   — corp_decision_ring's entries (BL-202), additionally logged
//                  (non-evicting) at their push site in corp_ai.cpp.
//   - agency     — agency_event's emission sites (BL-079 reflex tier in
//                  economy_system.cpp; BL-202 strategic tier in corp_ai.cpp).
//   - trade_route — credit_arrived_convoys in supply_system.cpp, only when a
//                  body-pair lane is FIRST established (never on a repeat
//                  completion, which only bumps the existing trade_route).
// See docs/ai/AI_OPPONENT.md for the settled shape and docs/generation/
// GENERATION_LEDGER.md for why this stays a separate mechanism from the ledger
// (disposable/tuning vs. durable/narrative — same instinct, incompatible
// lifetimes). Serialisation lives in history_log.{hpp,cpp}.

/// What kind of event a `world_history_entry` records. Each topic is a
/// filterable view over the one interleaved log (docs/ai/AI_OPPONENT.md).
enum class history_topic : uint8_t
{
    genesis,     ///< A PLANETOLOGY dated history_event line, copied at world setup.
    checkpoint,  ///< A planetology_state::checkpoints branch decision, migrated alongside genesis.
    decision,    ///< A BL-202 strategic corp_decision (the AI's command + rationale).
    agency,      ///< A BL-079/BL-202 agency_event (reflex or strategic tier).
    trade_route, ///< A trade_route's FIRST establishment (not repeat traffic on an existing lane).
};

/// One entry in `world::history_log`. `timestamp`'s UNIT DEPENDS ON TOPIC —
/// deliberately, on the same "one field spans two regimes for display reasons"
/// precedent `history_event::years_before_epoch` already establishes (see
/// planetology.hpp): `genesis`/`checkpoint` entries reuse that exact
/// years-before-the-1960-epoch value (positive = the deep/historical past, 0 =
/// epoch) so the genesis chapter merges in on its EXISTING timestamps with no
/// conversion; `decision`/`agency`/`trade_route` entries — which only ever
/// occur DURING the live simulation, strictly after the genesis+checkpoint
/// bulk-insert at world setup — instead carry the sim day tick (a small
/// non-negative int) they were emitted at. A reader must branch on `topic` to
/// know which clock it is reading; nothing in the log itself needs the two
/// clocks to compare numerically, because the vector's append order is
/// already the true chronological order end-to-end (genesis/checkpoint are
/// bulk-inserted once at setup, before any tick runs; everything else is
/// appended strictly in tick order thereafter).
struct world_history_entry
{
    int64_t       timestamp = 0;
    history_topic topic     = history_topic::genesis;
    entity_id     body      = null_entity; ///< Tag; null_entity if not applicable.
    entity_id     corp      = null_entity; ///< Tag; null_entity if not applicable.
    std::string   event;                   ///< Left column — what happened.
    std::string   consequence;              ///< Right column — what it left behind (may be empty).
};

/// BL-1042 (stockpile to budget) — one CARVED centre's slot: which region grew
/// it, its rank inside that region, and the key the carve sorted it on
/// (`urban_population / rank`). Written by `generate_population_centres`'
/// demography path; see `world::gen_carve_centres`.
struct carve_slot
{
    int     region = -1; ///< Index into `settlement_state::regions` (world::gen_settlement).
    int     rank   = 0;  ///< 1-based rank inside the region.
    int64_t key    = 0;  ///< The carve's sort key; the charter budget's split weight.

    bool operator==(const carve_slot&) const = default;
};

/// Why a carved centre was never founded (BL-1042).
enum class carve_drop_reason : uint8_t
{
    body_built_out = 0, ///< no candidate ground left on the body (or the carve outran the candidates)
    no_tile        = 1, ///< the chosen raster index resolved to no tile (defensive; unreached)
};

/// BL-1268 (TRADE.md § Trade in generation) — ONE CELL OF THE HISTORY'S TRADE
/// RECORD, converted to trade points: what history polity `polity` earned at
/// settlement region `region` (an index into `world::gen_settlement`'s regions)
/// across the Exploration and Industrialisation spans. Generation-time only.
struct gen_trade_cell
{
    std::int32_t polity = -1;
    std::int32_t region = -1;
    std::int64_t points = 0;
    /// The history's own readings the points were converted from, kept so a
    /// reader can say WHY a Marketplace stands where it does (the ledger's
    /// question) and a measurement can re-rate without regenerating.
    std::int64_t flow_volume    = 0;
    std::int64_t relation_years = 0;
};

/// The record the retrofit spends, and the two rates it spends it at
/// (`world_gen.trade_retrofit`). Cells sorted ascending by (polity, region).
struct gen_trade_record
{
    std::vector<gen_trade_cell> cells;
    std::int64_t points_per_marketplace = 0;
    int          max_per_market         = 0;
};

/// A carved centre that was never founded, with the reason.
struct carve_dropped_slot
{
    int               region = -1;
    int               rank   = 0;
    int64_t           key    = 0;
    carve_drop_reason reason = carve_drop_reason::body_built_out;
};

/// ECS registry. Entities are plain integer IDs; components are stored in
/// per-type maps. The registry owns all component data for the lifetime of
/// the simulation.
struct world
{
    // --- entity management ---

    /// Allocate a new entity ID. IDs increment monotonically and are never
    /// reused within a session.
    ///
    /// @return A unique, non-zero entity_id.
    entity_id create_entity();

    // --- well-known entities ---

    /// The player's corporation entity. Set by world construction; used by
    /// budget and unit ownership in later layers. No component is attached
    /// yet — the ID alone is sufficient until the budget layer is reached.
    entity_id player_entity = null_entity;

    /// The system's central star (a body entity of type body_type::star). Drawn
    /// at the Solar canvas centre; its name titles the minimap when the Solar
    /// canvas is shown there.
    entity_id star_body = null_entity;

    /// The corporation's home planet. The game opens on this body's surface.
    entity_id home_body = null_entity;

    /// Current sim day tick, mirrored from the sim loop each frame by app. A derived
    /// convenience so read-only UI surfaces can age trade routes for the activity fog
    /// (body_activity_visibility, BL-089) without threading the tick through every
    /// render signature. Not authoritative sim state and not serialised.
    int current_day_tick = 0;

    /// Current ECON tick (the quarter counter), mirrored by every driver before
    /// it steps the economy — app::step_economy, main.cpp's --serve and
    /// --export-blackboard loops, and each harness beside its day-tick write.
    /// Same contract as `current_day_tick`: a derived convenience, not
    /// authoritative, not serialised (zeroed on load, re-seeded by the driver).
    ///
    /// THIS, NOT THE DAY TICK, IS THE CADENCE KEY (BL-568, 2026-08-23). The
    /// corp_ai stagger is `tick % cadence_k == index % cadence_k`; it was keyed
    /// on the day tick, which in the live app is 90n at every quarter boundary,
    /// and 90n mod 4 is only ever 0 or 2 — so rivals at sorted index 1 or 3
    /// (mod 4) NEVER evaluated in a played game, while every harness and
    /// --serve loop (which pass 1..N) rotated all four slots. The econ counter
    /// is what every driver actually advances by one per step.
    int current_econ_tick = 0;

    /// The system's asteroid belt (a band, not a body). belt.present() is false
    /// when the system has no belt.
    asteroid_belt belt;

    /// BL-1101 — THE CAMPAIGN'S RECIPE BAND, a fact about the history this world
    /// was generated with (docs/economy/PRODUCTION.md § The era band; ERAS.md
    /// § Where the ladder starts). Written ONCE, at the Industrialisation fold
    /// in `make_hard_coded_world`: `industrial` iff any living polity's
    /// materials capacity sits at the Industrial rung at the 1960 close
    /// (`derive_campaign_band`, history_sim.hpp — the same
    /// `roster_band_for_capacity` derivation that dates `industrial_year`),
    /// `ancient` otherwise. A seed whose history never crosses is an
    /// ancient-band campaign; the sim is never reshaped to force the other
    /// answer. The epoch names the calendar only.
    ///
    /// READ, NEVER RE-DERIVED: `app::load_economy` and `app::load_game_from`
    /// both band the registry from this field, and every harness that mirrors
    /// the app bands from it too (`band_registry_from_world`, harness_params.hpp)
    /// — so a save never opens on a band its world did not earn and a harness
    /// never bands differently from the app.
    ///
    /// `any` means the fold never wrote it: a hand-built fixture, or a run whose
    /// Industrialisation span was switched off by a harness knob (the shipped
    /// descriptor always runs it). Such a world applies no mask, exactly as a
    /// registry whose band was never set — the harness default this enum was
    /// built around. SERIALISED (world_save.cpp, world_save_version 26); a byte
    /// outside the enum refuses the whole stream.
    era_band campaign_band = era_band::any;

    // --- component stores ---
    // Every unordered store on `world` is a `faithful_unordered_map`: exactly
    // std::unordered_map, except that a COPY keeps the source's iteration order.
    // A tick reads some of these stores in that order (a float sum over
    // `population_centres` among them), so a world copy that reordered them
    // would tick differently from its original (BL-1034). Add new ones as this type.
    faithful_unordered_map<entity_id, body_component>      bodies;
    faithful_unordered_map<entity_id, tile_component>      tiles;
    faithful_unordered_map<entity_id, building_component>  buildings;
    faithful_unordered_map<entity_id, stockpile_component> stockpiles;
    faithful_unordered_map<entity_id, market_component>    markets;
    /// BL-1125 (markets can die): every market folded away at generation, by
    /// its old id, ascending (`folded_market`). ROUTING STATE, saved: a tile
    /// routes to its nearest original centre and a folded one hands it to its
    /// absorber (`market_for_tile`), so the catchments a world was handed
    /// survive a load. Written only by `fold_market_into`; empty on a body
    /// whose markets never folded.
    std::map<entity_id, folded_market>                     folded_markets;
    faithful_unordered_map<entity_id, unit_component>             units;

    /// Population centre entities keyed by their entity ID. Populated by
    /// generate_population_centres() after tile generation; empty until that
    /// call is made for a body. No AI behaviour in the prototype.
    faithful_unordered_map<entity_id, population_centre_component> population_centres;

    /// Maps a population centre entity ID to the tile entity it occupies.
    /// Written alongside population_centres by generate_population_centres().
    faithful_unordered_map<entity_id, entity_id>                   population_centre_tile;

    /// Procedural city name per population centre — the human-readable identity used
    /// by the market ledger's market/city selector and the CSV export. Assigned by
    /// generate_population_centres() from an INDEPENDENT seeded stream (so it does not
    /// perturb world generation). A market's city name resolves via its centre_tile.
    faithful_unordered_map<entity_id, std::string>                 population_centre_name;

    /// Land-use state per TILE entity, held sparsely: an absent entry is
    /// `undeveloped` (docs/economy/POPULATION.md § Land use). Seeded by
    /// stamp_urban_land_use (BL-612, urban ground stamped) with the urban
    /// footprints under population centres; joins the flat-binary save as its
    /// own store (world_save format v11).
    faithful_unordered_map<entity_id, land_use_component>          land_use;

    /// Nation entities keyed by their entity ID. Populated by generate_nations()
    /// after tile generation; empty until that call is made for a body.
    faithful_unordered_map<entity_id, nation_component>    nations;

    /// What each nation CARES ABOUT -- the persistent weight vector the national
    /// budget pass (BL-537, `run_national_budget`) spends by. Keyed by nation
    /// entity id. Sprint N3 T2 (NR-569a: on `world`, NOT on `nation_component`).
    ///
    /// THE CONTRACT WITH THE SCORER. `score_national_budgets` (BL-542,
    /// nation_ai.hpp) returns weights for ONLY the nations DUE on a tick under
    /// its staggered cadence -- not for every nation. The caller that holds this
    /// map merges that result in by OVERWRITING one slot at a time and leaving
    /// the rest standing, so a nation scored on tick t keeps spending by that
    /// vector on t+1..t+k-1 rather than spending one tick in `cadence_k`. This
    /// is that map; nothing else may hold one.
    ///
    /// EMPTINESS IS THE INERTNESS PROOF. A nation absent here has no budget, and
    /// `run_national_budget` early-outs on an empty map (nation_budget.cpp), so
    /// a world in which no nation has ever been scored spends nothing -- which
    /// is the state every generated world starts in, and why `world_determinism`
    /// is unmoved by this field's existence.
    ///
    /// A `std::map` so the save leg and any hash fold walk it in ascending id
    /// with no sort. SERIALISED (world_save.cpp, beside `nations`; format v3).
    std::map<entity_id, nation_budget> nation_budgets;

    /// Maps a tile entity ID to the nation entity ID that owns it.
    /// Absent entries are unclaimed (ocean tiles and bodies without nation generation).
    /// Written by generate_nations() alongside the nation_component.tiles list.
    faithful_unordered_map<entity_id, entity_id>           tile_to_nation;

    /// Land tiles the colonisation span's diffusion actually reached AND could
    /// farm (BL-849, colonisation seeds the partition). Mirrors `tile_to_nation`'s
    /// shape and rule: absent means "not settled" — an unreached tile, a body
    /// the migration never ran on, or water (colonisation covers land only).
    ///
    /// WRITTEN BEFORE `build_province_partition` RUNS, from
    /// `settlement_state::settled_cells` (`run_settlement`'s colonisation field,
    /// `colonisation_field::farmable` — see `hard_coded_world.cpp`). Nothing
    /// downstream writes it, and it is NOT SERIALISED: like the reverse index
    /// `tile_to_nation` is, it is a generation-time index with no committed
    /// record to rebuild it from — the colonisation field itself is discarded
    /// once this is written.
    ///
    /// NO LONGER A PARTITION INPUT (Ben, 2026-09-27, NR-954 B; BL-1150, a
    /// centre's fill crosses the settled line): BL-849's settlement lock, which
    /// held a province to one side of this line, retired. The record stays the
    /// settled line's one index, read by the instruments that measure it
    /// (province_partition_harness P2d, centre_census C7b).
    ///
    /// A `std::set` so a deterministic walk over it needs no sort of its own.
    std::set<entity_id>                                 tile_settled;

    /// Corporation entities keyed by their entity ID. Populated by
    /// generate_corporations() after nation generation; empty until that call
    /// is made. Exactly one entry will have corporation_component::is_player == true,
    /// and world::player_entity will equal that entry's key.
    ///
    /// THE ONE-is_player INVARIANT, and the one path that can break it. On a
    /// charter-budget world the spend picks the player among the specialists it
    /// charters. A budget no centre can buy a specialist with falls back to the
    /// no-budget world, which seats from its own roster (NR-910), so the
    /// invariant holds there. The walk's RESIDUAL does not: a centre that
    /// afforded a specialist and found no ground for one leaves a world with no
    /// specialist, the spawn seat finds nobody to seat, and no entry is the
    /// player. That case is reported, never patched: the app prints the count
    /// on its seat line and player_seed_sweep fails a row that seats other
    /// than exactly one.
    faithful_unordered_map<entity_id, corporation_component> corporations;

    /// The Era -1 settlement record the SPECIALIST roster pass reads (BL-977):
    /// `generate_corporations` derives each corp's focus, ownership class and
    /// home province from `settlement_state::regions` / `charter` /
    /// `median_industrial_year`. World-gen holds the record in a local and
    /// passes a pointer; the landscape search regenerates the roster per
    /// candidate AFTER world-gen has returned, so it needs the same record —
    /// or every searched roster would fall to the national-character fallback
    /// and lose the specialists premise (BL-219, BL-631).
    ///
    /// A GENERATION-TIME INDEX, like `tile_settled` above: written once at the
    /// end of `make_hard_coded_world`, shared (read-only) by every per-candidate
    /// copy of the world, and NOT SERIALISED — the search runs only at new-game,
    /// and a loaded world never regenerates its roster. Null after a load.
    std::shared_ptr<const settlement_state> gen_settlement;

    /// BL-1042 (stockpile to budget; INDUSTRIALISATION.md Part III) — THE CARVE
    /// INDEX: every population centre the demography carve founded, keyed by
    /// centre id, bound to the (region, rank, key) SLOT it materialises. A
    /// region's industry points reach its campaign centres through this index
    /// (`build_stockpile_budget`, stockpile_budget.hpp). Centres founded any
    /// other way — the land-area fallback, the coverage foundings, the province
    /// anchors — are absent, and hold no share. A spilled centre is bound to the
    /// region that GREW it, not the one it stands in (`nearest_region` would
    /// mis-bind it).
    ///
    /// A GENERATION-TIME INDEX on exactly `gen_settlement`'s footing: written
    /// once by `generate_population_centres`, read by the new-game budget, and
    /// NOT SERIALISED (no save field, no `state_hash` or snapshot fold) — the
    /// budget is spent at new-game and a loaded world never rebuilds it. Empty
    /// after a load. A `std::map` so any walk over it is ascending id.
    std::map<entity_id, carve_slot> gen_carve_centres;

    /// BL-1042 — the carved centres that were NEVER founded, in carve order,
    /// with why. Their slots' share of a region's points is counted unspent
    /// under its own reason rather than spread over the region's founded
    /// centres. Same footing as `gen_carve_centres`: generation-time, NOT
    /// SERIALISED, empty after a load.
    std::vector<carve_dropped_slot> gen_carve_dropped;

    /// BL-1268 (TRADE.md § Trade in generation) — THE HISTORY'S TRADE RECORD in
    /// trade points, built at the end of `make_hard_coded_world` and SPENT by
    /// `retrofit_marketplaces` once the corporations are chartered. Same footing
    /// as `gen_carve_centres`: generation-time, NOT SERIALISED (no save field, no
    /// `state_hash` fold), empty after a load.
    ::gen_trade_record gen_trade_record;

    /// BL-1265 (the shelf economy; MARKETS.md § The shelf economy, Ben 2026-10-10):
    /// CORPORATIONS HOLD NO STOCKPILES. Every good is on a market's shelf
    /// (`market_component::inventory`). This is the one place a corporation's
    /// goods wait, and only during generation: the opening stockpile each
    /// corporation is seeded (`seed_opening_stock`) is held here, per corp,
    /// until the markets stand, then `place_opening_stock` puts every unit on
    /// the shelves of the markets the corporation sits in and empties this
    /// (CORPORATION_GENERATION.md § Pass 4b). GENERATION-TIME ONLY: empty before
    /// the first tick, so it is neither saved nor hashed. A `std::map` so the
    /// placement walks ascending corp id.
    std::map<entity_id, stockpile_component> gen_opening_stock;

    /// BL-1217 D6 (Ben, 2026-10-09, the exceptions) — the PRE-AUTHORED
    /// installation's processor, recorded when `make_hard_coded_world` authors
    /// it: the one processor `assign_default_recipes` gives its default whatever
    /// the want. A GENERATION-TIME MARKER, deliberately neither saved nor
    /// hashed: the default pass runs only on a freshly generated world (a saved
    /// world holds no recipe-less processor), so a loaded world's null here
    /// exempts nothing it could ever meet. Copied with the world, like any
    /// member, so a search candidate's copy carries it.
    entity_id authored_processor = null_entity;

    /// Active convoys — goods in transit. Appended by dispatch_convoys, advanced by
    /// advance_convoys, and retired (erased) by credit_arrived_convoys in
    /// supply_system.hpp. A std::vector (not a map) because convoys have no persistent
    /// entity ID. Order is dispatch-time insertion; stable between ticks.
    ///
    /// BL-452: "identified by index while in flight" (as this read until
    /// 2026-08-17) stopped being adequate the moment a convoy became a command
    /// SUBJECT — an arrival erases from the middle of the vector, so an index a
    /// press composed last frame names a different convoy this frame. Each
    /// convoy now carries a `convoy_component::id` from `allocate_convoy_id`
    /// below; the index remains the storage order and nothing else.
    std::vector<convoy_component> convoys;

    /// BL-1265 (MARKETS.md § The shelf economy: "landing is selling") — what
    /// LANDED this tick, per (owner, market): a building's output, a trade's
    /// cargo arriving, a captured cargo. Every landing goes here; the tick's
    /// `clear_markets` lists it as this tick's supply, pays its owner the
    /// quantity at the market's clearing price (the market is the counterparty,
    /// bid or no bid), moves it onto the shelf, and empties this. So goods land
    /// on the shelf in the tick they are made, at that tick's clear.
    ///
    /// TRANSIENT, never saved, never hashed: every writer runs in the tick
    /// before its clear (arrivals, production, capture), and the clear empties
    /// it — saves are taken between ticks, when it is always empty. A
    /// `std::map`: the clear walks it in ascending (owner, market) order.
    std::map<std::pair<entity_id, entity_id>, stockpile_component> landed_this_tick;

    /// Land @p qty of good @p r for @p owner on @p market this tick (see
    /// `landed_this_tick`). Non-positive or non-finite quantities are ignored.
    void land_goods(entity_id owner, entity_id market, std::size_t r, float qty)
    {
        if (!(qty > 0.0f) || !std::isfinite(qty) || r >= resource_count || market == null_entity)
            return;
        landed_this_tick[std::make_pair(owner, market)].quantities[r] += qty;
    }

    /// What @p owner has landed on @p market this tick in good @p r (0 if none).
    float landed(entity_id owner, entity_id market, std::size_t r) const
    {
        const auto it = landed_this_tick.find(std::make_pair(owner, market));
        return (it == landed_this_tick.end() || r >= resource_count) ? 0.0f : it->second.quantities[r];
    }

    /// BL-1222 (trade-flow lens) — what the PLAYER corporation's dispatcher did
    /// over its last few passes, for the Trade-flow lens (LENSES.md § Trade-flow
    /// lens). WRITE-ONLY for the simulation: `dispatch_convoys` appends one pass
    /// and nothing in `world/*` reads it. TRANSIENT: never saved, never folded
    /// into a state hash, so a loaded game shows the lens from its first pass on.
    /// Newest pass at the back; at most `trade_flow_window` passes are kept, the
    /// trailing window the lens sizes its arrows over. Each pass carries the corp
    /// it was taken for; nothing clears the window on a seat change, so the lens
    /// filters on `trade_flow_pass::corp == player_entity`.
    std::vector<trade_flow_pass> player_trade_flow;
    static constexpr std::size_t trade_flow_window = 4;

    /// Next stable convoy handle. Monotonic and never reused, exactly like
    /// `next_order_id`: an arrived convoy's id does not come back, so a command
    /// naming a convoy that has already landed is refused rather than silently
    /// re-aimed at whichever convoy inherited its slot.
    uint32_t next_convoy_id = 1;

    /// Allocate the next stable convoy id. Deterministic (a plain counter, in
    /// dispatch order); the single point convoy ids are minted from.
    ///
    /// @return A fresh, never-before-issued convoy handle (always nonzero).
    uint32_t allocate_convoy_id() { return next_convoy_id++; }

    /// Persistent trade routes — durable body-pair lanes a corporation's commerce
    /// has run (BL-088). Upserted by credit_arrived_convoys when a convoy completes
    /// a lane; never erased (aging to 'stale' is a read-time concern owned by the
    /// commercial-sphere fog, BL-089). A std::vector mirroring `convoys`; insertion
    /// order, stable between ticks. The flat-binary serialisation seam now exists
    /// (`history_log` below, BL-208) but `trade_routes` itself does not join it
    /// directly — only a derived `world_history_entry` (topic=trade_route) is
    /// written, and only when a lane is FIRST established.
    std::vector<trade_route> trade_routes;

    /// The append-only world history log (BL-208) — see the `history_topic` /
    /// `world_history_entry` doc comments above for the four sources and the
    /// per-topic timestamp convention. THIS is the project's first flat-binary
    /// serialisation seam (history_log.{hpp,cpp}); everything else in `world`
    /// remains unserialised. Populated once at world setup with the genesis +
    /// checkpoint chapters (seed_genesis_history), then appended to live as the
    /// simulation runs. Never erased or evicted — unlike `ai_decisions`'s 256-cap
    /// ring, this is meant to grow for the life of a campaign.
    std::vector<world_history_entry> history_log;

    /// The province partition (BL-466) — every body's land tiles carved into
    /// small, contiguous, purely spatial cells. Built once at the end of
    /// `make_hard_coded_world` from the world seed and the finished tile map;
    /// derived, but STORED, because BL-467 folds a province id into a battle's
    /// seed stream and a battle must not be re-identified by a lazy rebuild.
    /// Walk it in ascending `province::id` — that order is the contract, not an
    /// implementation detail (see province.hpp). It joins the flat-binary
    /// serialisation seam as the TRAILING section of the history-log stream, so
    /// a stream written before BL-466 is still a valid prefix.
    /// NOT folded into `state_hash`, and that is deliberate rather than an
    /// oversight (the omission corp_modifiers documents, documented here too
    /// after NR-401 asked). `state_hash` folds the fields a TICK may mutate, so a
    /// divergence in it means the simulation diverged; the partition is
    /// generation output and never moves once built, so including it would make
    /// every tick hash carry a constant. Its determinism is checked where it
    /// belongs — `determinism_harness` compares the partition field-for-field
    /// across two generations of the same seed, and
    /// `province_partition_harness` P6/P7 recompute it from the stored seed.
    /// This matters more than it reads: BL-467 folds a province id into a
    /// battle seed, so a silent partition regression would move battle
    /// outcomes.
    province_partition provinces;

    /// Current holder of each province (BL-569, province holder) — one
    /// entity_id per province, POSITIONALLY ALIGNED with `provinces.provinces`
    /// (ascending `province::id` order, the partition's own contract), NOT
    /// indexed by the raw id, which is a derived tile id and not compact.
    /// `null_entity` for a non-land province and for a land province with no
    /// recorded holder. Use `province_holder_for(w, province_id)`
    /// (province.hpp) rather than indexing this directly.
    ///
    /// Seeded once at generation (`seed_province_holders`, called from
    /// `make_hard_coded_world` right after `build_province_partition`) from
    /// `tile_to_nation`'s plurality over each province's tiles. Moved
    /// thereafter by `run_battles` (battle_system.cpp) when a battle closes
    /// decisively — `province_holder[p]` becomes the winner's
    /// `battle_dispatch::field_held_by` — and left untouched by a stalemate.
    ///
    /// UNLIKE `provinces` ITSELF, this IS folded into `state_hash`: the
    /// partition is generation output and never moves once built, but the
    /// holder is exactly the kind of tick-mutable state the hash exists to
    /// catch a divergence in (the same distinction battles/provinces already
    /// draw, see the note above). Deliberately narrow: `tile_to_nation` (the
    /// territorial map) does NOT follow the holder — the political map
    /// changing hands is unowned work. See docs/military/MILITARY.md § The
    /// field.
    std::vector<entity_id> province_holder;

    /// Battles in progress (BL-467) — the record that makes a fight a thing in
    /// the world rather than an answer someone computed. Until this existed both
    /// resolvers were compiled, harnessed and CALLED BY NOTHING.
    ///
    /// KEPT SORTED by (province, attacker, defender) — `battle_order_less`. That
    /// is the contract, not an implementation detail, and it is load-bearing for
    /// exactly the reason the province partition's ascending-id walk is: the
    /// containers battles are DISCOVERED from (`units`, `corporations`) are
    /// unordered, so the sorted record is where order-independence is recovered.
    ///
    /// FOLDED INTO `state_hash`, unlike `provinces` — and the two decisions are
    /// the same rule applied to different things, not an inconsistency. The rule
    /// is "state_hash folds the fields a TICK may mutate". The partition is
    /// generation output and never moves once built, so folding it would make
    /// every tick hash carry a constant; a battle is created, stepped and ended
    /// BY ticks, so a divergence in it is exactly the kind of divergence the hash
    /// exists to catch. The fold contributes nothing when the list is empty, so a
    /// world that never fights hashes as it always did.
    ///
    /// NOT SERIALISED, deliberately and on precedent: no battle serialiser exists,
    /// so determinism is asserted by IN-MEMORY REPLAY only, never a save
    /// round-trip (the BL-448 precedent, which BL-467's design cites verbatim). A
    /// save taken mid-battle therefore drops the fight — acceptable while nothing
    /// can save mid-tick, and the thing to revisit first if that changes.
    std::vector<active_battle> battles;

    /// Proximity-glimpse stamps (BL-099) — the sim day tick at which a player convoy
    /// last passed within `glimpse_radius_au_default` (AU) of this body while completing
    /// an inter-body lane. Sampled once at the discrete completion tick by
    /// record_proximity_glimpses (orbits have already advanced for that frame), so no
    /// past position is ever reconstructed — the fog reads the stamp, never recomputes
    /// geometry. Held off `body_component` (the `corp_market_pools` rationale) to keep the
    /// body's future flat-binary layout untouched. std::map for deterministic iteration.
    std::map<entity_id, int> body_last_glimpse_tick;

    /// Lazily-built per-body raster index (grid_y*grid_width + grid_x -> tile entity, or
    /// null_entity for an absent cell), for O(1) neighbour lookup in intra-body pathfinding
    /// (BL-077). A derived cache, not authored state: built on first use by body_tile_grid(),
    /// a pure function of the body's tiles (independent of tiles-map iteration order).
    faithful_unordered_map<entity_id, std::vector<entity_id>> body_tile_index;

    /// Route-cost cache for intra-body A* (BL-077), keyed by the ORDERED (body, src, dst) --
    /// a path is directed (BL-1126). A derived cache; invalidated
    /// when road_level changes (road placement, BL-147). Keeps per-Tick per-lane A* off the
    /// dispatch hot path.
    std::map<std::tuple<entity_id, entity_id, entity_id>, logistics_path> astar_cost_cache;

    /// Completed flood fields for intra-body pathfinding, keyed (body, anchor tile) —
    /// see logistics_flood_field. A derived cache with the SAME invalidation contract
    /// as astar_cost_cache (cleared together by invalidate_logistics_caches and on
    /// load); rebuilt lazily by intra_body_path on a pair-cache miss. ~410 KB per
    /// anchor on the 312x145 grid, bounded by the distinct destination centres queried
    /// between invalidations — deliberately spending memory to keep rival road
    /// placement (BL-599) from turning every tick into hundreds of grid searches.
    std::map<std::pair<entity_id, entity_id>, logistics_flood_field> logistics_flood_fields;

    /// BL-1186 (goods cross markets): the LEG-confined siblings of the two caches above —
    /// a land leg never enters water, a sea leg runs port -> water -> port (SUPPLY.md
    /// § Logistical cost: mode is a property of the leg). Flood fields keyed (body, anchor
    /// tile, leg_domain), pair paths keyed by the ORDERED (body, src, dst, leg_domain).
    /// Derived caches with the SAME invalidation contract (invalidate_logistics_caches,
    /// clear_derived_state); kept apart so the unconfined counts the warm-start probes read
    /// stay theirs. Pure functions of the body's tiles.
    std::map<std::tuple<entity_id, entity_id, std::uint8_t>, logistics_flood_field> leg_flood_fields;
    std::map<std::tuple<entity_id, entity_id, entity_id, std::uint8_t>, logistics_path> leg_path_cache;

    /// BL-1186: per body, the tiles carrying a BUILT, ACTIVE Port, ascending and unique —
    /// the candidate ends of a sea leg. Derived; cleared with the logistics caches, whose
    /// contract already covers a port completing, idling or being demolished
    /// (building_affects_logistics).
    std::map<entity_id, std::vector<entity_id>> body_port_tiles;

    /// Per-body LOGISTICS REACH FIELD (BL-323 S2): raster-indexed weighted cost from each
    /// tile to its nearest supply anchor — a city, a port, or an inland logistics hub.
    /// Infinity where no anchor is reachable. A derived cache like the two above, built on
    /// first use by `body_reach_field()` and invalidated wherever `astar_cost_cache` is
    /// (road placement, building placement/demolition), because the same mutations move it.
    ///
    /// One multi-source Dijkstra per body rather than an A* per candidate tile: placement
    /// asks this question for every tile under the cursor, and the armed-build tint asks it
    /// for the whole visible grid at once, so a per-query search would be the wrong shape.
    faithful_unordered_map<entity_id, std::vector<float>> body_reach_cost;

    /// BL-1230 (power crosses markets) — THE POWER GRID, province grain
    /// (LOGISTICS.md § 3a, "The province is the grid's cell"). Wired province id
    /// -> its grid id (the LOWEST wired province id in the grid). A province with
    /// no road tile is dark and absent. A derived cache on the same footing as the
    /// reach field: built lazily by `province_power_grid()`, cleared by
    /// invalidate_logistics_caches (every road write calls it) and by
    /// clear_derived_state, never serialised. `power_grid_built` distinguishes
    /// "not built" from "built, every province dark"; `power_grid_stamp` is the
    /// partition's province count at build, so a partition redrawn under the
    /// cache rebuilds it rather than being read stale.
    std::map<std::uint32_t, std::uint32_t> power_grid_of_province;
    bool        power_grid_built = false;
    std::size_t power_grid_stamp = 0;

    /// BL-1195: the LOGISTICS CACHE GENERATION — a stamp that changes every time the
    /// logistics caches above are dropped (invalidate_logistics_caches,
    /// clear_derived_state). VIEW-ONLY: read by the UI's per-convoy lane cache to know
    /// a re-route happened, and by nothing in the simulation. Never serialised, never
    /// hashed, never compared across worlds. Drawn from one process-wide counter
    /// (bump_logistics_cache_generation), so two worlds — a freshly loaded save and
    /// the one it replaced — never share a value.
    std::uint64_t logistics_cache_generation = 0;

    /// Per-body NEAREST LOGISTIC POINT ANCHOR (BL-1117) — see lp_anchor_field. A
    /// derived cache on the same footing as the three above: built lazily by
    /// `nearest_lp_anchor`, cleared by invalidate_logistics_caches and by
    /// clear_derived_state, never serialised. Rebuilt in place when a caller's
    /// anchor pool names a different anchor set than the one it was built over,
    /// so no answer ever depends on what was cached. std::map: ordered, so a copy
    /// or a load cannot reorder it (and it is never iterated by the sim).
    std::map<entity_id, lp_anchor_field> lp_anchor_fields;

    /// Techs each corporation has EARNED, by tech id (BL-344). Per-corp, never
    /// global: two corporations research independently, and a gate that read a
    /// world-wide set would unlock a rival's content for the player. `std::map`
    /// + `std::set` for deterministic iteration (the `corp_market_pools`
    /// rationale). Empty at world setup — nothing is earned until a tech's
    /// `condition_set` is satisfied and `advance_tech_gates` records it.
    std::map<entity_id, std::set<std::string>> earned_techs;

    /// Enacted and un-enacted laws (BL-343). A world-level list rather than a
    /// per-corp one: a law is enacted over the world and charged to whoever it
    /// applies to, which is what makes it a governing-body instrument rather
    /// than a corporate setting (BL-094). A `std::vector` in authored order;
    /// evaluation order is therefore fixed and the money loop is deterministic.
    std::vector<law> laws;

    /// True iff `corp` has earned `tech_id`. The single read point for the tech
    /// gate and for `condition_subject::research`, so the two cannot disagree.
    ///
    /// @param corp    Corporation entity id.
    /// @param tech_id Tech id as authored in scripts/tech_tree.lua.
    /// @return        Whether that corporation has earned that tech.
    bool has_tech(entity_id corp, const std::string& tech_id) const
    {
        const auto it = earned_techs.find(corp);
        return it != earned_techs.end() && it->second.count(tech_id) != 0;
    }

    /// Scalar modifiers each corporation has been granted by earned techs'
    /// `modify_scalar` effects (BL-479; laws join via BL-480). Per-corp and
    /// append-ordered: `advance_tech_gates` appends a tech's effects at the
    /// earn moment, in gate-table order, so the fold order below is the earn
    /// order and is deterministic. NOT folded into `state_hash`: the canonical
    /// fact is the earned set, and hashing an accumulation of it would only
    /// double-count.
    ///
    /// DERIVED IN ORIGIN BUT ORDER-BEARING, SO IT IS SERIALISED (BL-536,
    /// NR-510). This comment used to say "recomputable from `earned_techs`
    /// × the gate table", and BL-107 carried a change-note instructing the save
    /// format to re-fold it on load. Both were wrong; the save does neither.
    /// `advance_tech_gates` appends in gate-table order WITHIN ONE CALL, but it
    /// runs every tick — so the stored sequence is ordered by (earn tick, gate
    /// index), while a re-fold can only reproduce (gate index). The two diverge
    /// as soon as a corp satisfies a higher-index gate before a lower-index one,
    /// which nothing prevents. It matters because `modified_scalar` folds in
    /// stored order across `add` and `multiply`, which do not commute. The order
    /// is information the earned set does not carry, so it is state.
    /// `std::map` for deterministic iteration
    /// (the `corp_market_pools` rationale). Empty at world setup, and stays
    /// empty for the life of any world whose techs carry only
    /// unlock_structure — which is what keeps such a world bit-identical to
    /// the pre-BL-479 build.
    std::map<entity_id, std::vector<scalar_modifier>> corp_modifiers;

    /// `base` with every modifier `corp` holds on `subject` folded in, in
    /// stored (earn) order. THE single read point for the effect side, as
    /// `has_tech` is for the earned set — an economy read site asks this and
    /// nothing else, so two sites cannot fold differently.
    ///
    /// Returns `base` UNTOUCHED — the same bits, no arithmetic performed —
    /// when `corp` is `null_entity`, holds no modifiers, or holds none on this
    /// subject. That is the R1 guarantee: a world with no modify_scalar tech
    /// pays nothing and drifts nowhere.
    ///
    /// @param corp    Corporation whose modifiers apply (`null_entity` = none).
    /// @param subject The scalar being read.
    /// @param base    The unmodified value the call site computed.
    /// @return        The modified value (== `base` when nothing applies).
    float modified_scalar(entity_id corp, modifier_subject subject, float base) const
    {
        const auto it = corp_modifiers.find(corp);
        if (it == corp_modifiers.end())
            return base;
        float v = base;
        for (const scalar_modifier& m : it->second)
            if (m.subject == subject)
                v = apply_scalar_modifier(v, m);
        return v;
    }

    /// Per-body market index (BL-356): body -> its markets in ascending id order.
    /// A derived cache like body_tile_index, serving market_for_tile / clear_markets
    /// (market_clearing.cpp) so the hot read path stops rebuilding the grouping per
    /// call. Rebuilt when the stamp below stops matching the market set — markets
    /// are created at runtime but never destroyed, and every market is an entity,
    /// so count + the allocator cursor catches every mutation in O(1) (BL-1079:
    /// the stamp was count + max id, and the max id cost a walk of every market
    /// per call). Mutable so the const read path (market_for_tile) can refresh it.
    mutable faithful_unordered_map<entity_id, std::vector<entity_id>> body_market_index;
    mutable std::size_t   body_market_index_count  = 0; ///< markets.size() at build.
    mutable std::uint32_t body_market_index_cursor = 0; ///< next_entity_id() at build.
    /// BL-1125: body -> its FOLDED markets' ids, ascending; rebuilt with
    /// `body_market_index` (same stamp; `fold_market_into` resets it).
    mutable faithful_unordered_map<entity_id, std::vector<entity_id>> body_folded_index;
    /// BL-1125: body -> a digest of what routing reads on it (its standing
    /// markets' ids and centres, its folded markets' ids, centres and
    /// absorbers), recomputed with `body_market_index`. The route raster below
    /// rebuilds only when this moves, so an unrelated entity creation (which
    /// restamps the index) costs O(markets), never a re-route of every tile.
    mutable faithful_unordered_map<entity_id, std::uint64_t> body_market_sig;
    /// BL-1125: body -> its catchment raster (`body_route_cache`), so
    /// `market_for_tile` is O(1) in play. Derived, never saved; cleared by
    /// clear_derived_state.
    mutable faithful_unordered_map<entity_id, body_route_cache> body_route_index;

    /// Per-body population-centre index (BL-1050): body -> its centres in
    /// ASCENDING ID ORDER. The same derived cache as `body_market_index` above
    /// and for the same reason, one rung sharper: `body_mean_habitability`
    /// (budget_system.cpp) is called per building per tick and per build
    /// candidate, and without this each call walks EVERY centre in the world and
    /// hashes twice per centre to find the handful on one body. That cost is
    /// older than the index — budget_system's own `hab_cache` exists to blunt it
    /// at one call site — and BL-1050's sorted walk sits on top of it.
    ///
    /// The ascending order is the point, not a convenience: the mean is a float
    /// sum, so the summation order must be a property of the ids and never of
    /// `population_centres`' layout, which a load rebuilds and another standard
    /// library lays out differently again.
    ///
    /// THE STAMP IS O(1), unlike the market index's: the count plus the entity
    /// allocator's cursor, never a walk for a max id — a per-call walk of 5417
    /// centres is the cost this index exists to remove. Population centres are
    /// written once by `generate_population_centres` and never created, erased or
    /// moved between bodies afterwards (a razed centre keeps its entry and its
    /// tile), so the count alone would already catch every mutation today; the
    /// cursor is what closes an erase paired with an insert, since no insert can
    /// happen without moving it. `population_centre_tile` is written alongside
    /// `population_centres` and shares that lifetime. Mutable so the const read
    /// path can refresh it.
    mutable faithful_unordered_map<entity_id, std::vector<entity_id>> body_centre_index;
    mutable std::size_t   body_centre_index_count  = 0; ///< population_centres.size() at build.
    mutable std::uint32_t body_centre_index_cursor = 0; ///< next_entity_id() at build.

    // THE ORDER BOOK RETIRED (BL-1265; MARKETS.md § The shelf economy, Ben
    // 2026-10-10): no standing buy or sell orders — everyone buys at the posted
    // price, under the fair-price ceiling, and production is sold on landing.
    // Procurement (below) is not the order book and stays.

    /// BL-1266 (TRADE.md § A trade) — every MANUAL trade, world-wide, in the
    /// order it was set: the order each owner's reserved points are spent in.
    /// Written only through the `set_trade` / `clear_trade` corp verbs; read by
    /// `run_trades` every tick. Auto trade is not stored: it is chosen afresh
    /// each tick from prices. A `std::vector`, insertion-ordered like
    /// `convoys`; erasure is by `id`, so a removal never renumbers a survivor.
    std::vector<standing_trade> trades;

    /// Next stable trade handle. Save-format state: a load followed by a new
    /// trade must not mint an id a live trade already holds. Monotonic.
    uint32_t next_trade_id = 1;

    /// Allocate the next stable trade id (always nonzero). Deterministic: a
    /// plain counter, in command-application order.
    uint32_t allocate_trade_id() { return next_trade_id++; }

    /// Live procurement quotes (BL-350) — the answer to `request_quote`,
    /// before `accept_quote` converts one into a `procurement_contract`. A
    /// parallel object to the order book, not an entry in it (that item's
    /// Q4) — see procurement_quote's own comment.
    std::vector<procurement_quote> procurement_quotes;

    /// Accepted procurement contracts, paced like a BL-095 build: a deposit
    /// debited at accept_quote, the remainder drawn across `lead_time_ticks`,
    /// delivered to the buyer's pool on completion. See procurement_contract.
    std::vector<procurement_contract> procurement_contracts;

    /// Next stable procurement handle (quotes and contracts share one
    /// namespace — the same "stable across erase, unlike a vector index"
    /// contract as `next_order_id`).
    uint32_t next_procurement_id = 1;
    uint32_t allocate_procurement_id() { return next_procurement_id++; }

    /// A supplier's standing embargo predicate (BL-350's Q2, the law/embargo
    /// decline condition) — keyed by the SUPPLIER corp; `request_quote`
    /// evaluates it against the buyer. Absent = no entry = the default
    /// `condition_set{}` (empty = always true = no embargo), so a supplier
    /// with no authored embargo declines nothing on this axis. Nothing
    /// authors a non-empty entry yet — the read path is the mechanism BL-350
    /// exists to prove condition_set reaches procurement "for free"; content
    /// (an enacted law that populates this) is a BL-343/BL-350 follow-on.
    faithful_unordered_map<entity_id, condition_set> corp_embargo_conditions;

    /// THE RELATIONAL SUBSTRATE (BL-545): one directed, continuous, DERIVED
    /// value from an observer to a subject, on two dimensions (Access, Trust),
    /// at every grain — corp->corp, nation->corp, nation->nation alike.
    /// `sentiment.hpp` owns the meaning; this field is the ONLY join between
    /// that translation unit and the world, and it is one-directional on
    /// purpose: sentiment.cpp is never handed a `world&`, so it cannot reach a
    /// stance table (THE INVARIANT — sentiment informs a declaration, it never
    /// makes one).
    ///
    /// BL-546 (2026-08-23) folded `corp_reputation` into it. There is no
    /// second store: procurement reputation is the TRUST dimension at
    /// (buyer, supplier) grain, read through `procurement_reputation` below
    /// and written through `note_conduct`. A `std::map` inside, so every walk
    /// is a sorted walk over ids (BL-158).
    sentiment_table sentiment;

    /// BL-350's Q3 counterparty reputation, as a VIEW rather than a store
    /// (BL-546). Reputation IS sentiment's Trust dimension from the BUYER
    /// toward the SUPPLIER — directed, keyed exactly as `corp_reputation` was,
    /// and neutral (0) for a pair that has never dealt.
    ///
    /// The single read point. `request_quote`'s floor test reads this, and
    /// nothing else reads the axis today; because the row DECAYS toward
    /// neutral, "below the floor" is a temporary state and no consumer may
    /// treat it as terminal (BL-391, dissolved rather than special-cased).
    ///
    /// @param buyer    The corp forming the opinion (the observer).
    /// @param supplier The corp it is about (the subject).
    /// @return         Trust, or 0 for an absent row.
    float procurement_reputation(entity_id buyer, entity_id supplier) const
    {
        return sentiment_toward(sentiment, buyer, supplier).trust;
    }

    /// Record ONE occurrence of observable conduct against the substrate, at
    /// the moment it happens. The single write point, so the two procurement
    /// writers cannot drift from one another or from the authored weights.
    ///
    /// Applied immediately rather than queued: the pre-BL-546 reputation
    /// writers mutated in place, and a queue would have delayed a cancellation
    /// by a tick — a behaviour change this migration is required not to make.
    /// The tick's own decay half runs at the head of `run_economy_step`, so
    /// "decay first, then this tick's conduct" still holds.
    ///
    /// @param p         Authored factor weights (recipe_registry::sentiment()).
    /// @param observer  Who forms the opinion.
    /// @param subject   Who it is about.
    /// @param kind      Which authored row of conduct this is.
    /// @param magnitude Occurrence count / natural size; 1.0 is "once".
    void note_conduct(const sentiment_params& p, entity_id observer, entity_id subject,
                      sentiment_factor_kind kind, float magnitude = 1.0f)
    {
        apply_sentiment_events(sentiment, p,
                               { sentiment_event{ observer, subject, kind, magnitude } });
    }

    /// Corp stance (BL-448): the directed hostility map, keyed (from, to) —
    /// presence means `from` has declared hostile toward `to`; absence means
    /// no such declaration, NOT "not hostile" in the other direction. Not
    /// `standing.{hpp,cpp}` (BL-262's coarse power read — a different axis).
    /// std::set, not a bool-valued map — presence alone is the fact; there is
    /// nothing else to store per entry. See src/world/stance.hpp for the
    /// verbs and the two-table HYBRID rationale (Ben, 2026-08-17, NR-302).
    std::set<std::pair<entity_id, entity_id>> corp_hostile_pairs;

    /// Corp stance (BL-448): the symmetric, ACCEPTED friendship table, keyed
    /// canonical (min id, max id) — order-independent by construction. A row
    /// here is always evidence both corps chose it (reached only via
    /// offer_friendship + accept_friendship). Declaring hostility between the
    /// pair dissolves the row atomically — see stance.hpp's declare_hostile.
    std::set<std::pair<entity_id, entity_id>> corp_friend_pairs;

    /// Corp stance (BL-448): PENDING friendship offers, keyed directed
    /// (offerer, target). Deliberately a separate table from
    /// `corp_friend_pairs` (invariant 1 in stance.hpp) — an unaccepted offer
    /// must never be readable as a friendship by any consumer that only
    /// checks the friendship table.
    std::set<std::pair<entity_id, entity_id>> corp_friend_offers;

    /// Strategic AI decision log (BL-202): a fixed 256-entry ring of the most
    /// recent corp commands + score rationale, in deterministic application
    /// order. Derived observability (the chat feed / harness read it), not
    /// save-format state — it does not join the serialisation seam.
    corp_decision_ring ai_decisions;

    /// The exchange record (BL-685): a ring of the most recent realised
    /// exchanges, appended one row per exchange by `clear_markets` in its own
    /// deterministic clearing order. Authority: docs/economy/MARKETS.md
    /// § The exchange record.
    ///
    /// The opposite call to `ai_decisions` one line above, and the contrast is
    /// the point: a decision is observability, an exchange is a thing that
    /// HAPPENED to the player's balance and their stock. So this IS save-format
    /// state and travels in the world snapshot — a loaded campaign opens with
    /// its trade history intact rather than a blank ledger.
    ///
    /// It carries REVENUE and no margin, structurally — see `exchange_record`
    /// in components.hpp for why a profit column cannot be derived here.
    ///
    /// DELIBERATELY NOT IN `state_hash` (below). The hash canonicalises the
    /// state a divergence would show up IN — balances, dials, resolved prices,
    /// pools, the order book — and this ring is a pure downstream observation of
    /// exactly those: an exchange that differed between two runs differed
    /// because a price, a pool or an order did, and the hash already sees that.
    /// Folding it in would add no detection and would move every golden hash
    /// (`tools/verify/spectator_determinism.cpp` among them) for a quantity that
    /// is a consequence rather than a cause. `exchange_record_harness` asserts
    /// the ring's own run-to-run identity directly instead.
    exchange_record_ring exchanges;

    /// Authored effective workforce supply per (corp, body) — Layer 4 step 1 of the
    /// labour-pool model (docs/economy/POPULATION.md § Workforce model). Absent
    /// entries fall back to `default_workforce_supply`; population centres replace
    /// this authored value with a population-derived figure in step 2. Held off the
    /// component structs (the `corp_market_pools` rationale) so the economy stays on
    /// disjoint files.
    static constexpr float default_workforce_supply = 3.0f;
    std::map<std::pair<entity_id, entity_id>, float> workforce_supply_overrides;

    /// Effective workforce available to `corp` on `body` this tick. The labour the
    /// corporation's buildings on that body contend for under the pool model.
    ///
    /// @param corp Corporation entity id.
    /// @param body Body entity id.
    /// @return     Authored supply if present, else `default_workforce_supply`.
    float workforce_supply(entity_id corp, entity_id body) const
    {
        const auto it = workforce_supply_overrides.find(std::make_pair(corp, body));
        return (it != workforce_supply_overrides.end()) ? it->second : default_workforce_supply;
    }

    /// Tick-boundary state hash (BL-204): an FNV-1a checksum over a deterministic
    /// canonicalisation of the econ-tick snapshot — every corporation's balance
    /// and `is_player` flag with `player_entity` (the seat), every building's dial
    /// state (workforce target/assigned/auto, recipe, decommissioned,
    /// ticks_remaining), every market's resolved price array, every corp/body
    /// stockpile pool, and the order book. Sorted by entity id
    /// (map/unordered_map iteration order is not itself trusted) so two
    /// structurally-identical worlds hash identically regardless of container
    /// internals — with the deliberate exception of the order book, whose *stored*
    /// sequence is itself state (price-time priority), so it is hashed as stored.
    ///
    /// Two roles, one function: (1) today, a same-seed-two-runs regression primitive
    /// for the AI skill harness (BL-204) — a divergence flags a determinism leak in
    /// the corp-AI seam; (2) later, the lockstep desync detector floated in
    /// MULTIPLAYER_PRINCIPLES.md — a remote peer's hash mismatch at a tick boundary
    /// is the desync signal. `tick` is folded in so a hash is tick-scoped (comparing
    /// hashes across different ticks is meaningless by construction).
    ///
    /// THE SEAT IS THE ONE FOLDED FIELD A TICK NEVER MOVES (BL-1082, Ben's ruling
    /// of 2026-09-24). The rule this header states elsewhere — "state_hash folds
    /// the fields a TICK may mutate" — stands for everything else; the seat is
    /// folded because a different pick on the same world is a different campaign,
    /// and a hash that could not tell two seats apart could not say that (seed,
    /// pick) reproduces the seat (`tools/verify/seat_pick_check.js`,
    /// `scripts/verify/seat_pick.lua` S5). Two picks on one world hash apart;
    /// the same pick, or the draw and a pick of the drawn firm, hash alike.
    ///
    /// @param tick The sim day tick this snapshot is taken at (folded into the hash).
    /// @return An FNV-1a 64-bit checksum of the canonicalised snapshot.
    uint64_t state_hash(int tick) const;

    // --- Save-format access to the id counter (BL-536) -----------------------
    //
    // The allocator's cursor is SAVE STATE, for the same reason `next_order_id`
    // and `next_convoy_id` are and say so: a load that restarted allocation at 1
    // would hand out ids that live entities already hold, and the collision would
    // surface as one entity silently becoming another. It stays private, with a
    // reader and a restorer either side of the seam rather than a public field —
    // nothing but the serialiser has any business setting it.

    /// The next id `create_entity` will hand out. For the serialiser only.
    /// @return The allocator's current cursor.
    uint32_t next_entity_id() const { return m_next_id; }

    /// Restore the allocator's cursor on load. For the serialiser only.
    ///
    /// Clamped up to 1 rather than trusting the stream: a zero would make the
    /// next `create_entity` return `null_entity`, which every consumer reads as
    /// "no entity". A corrupt stream should fail its own guards long before
    /// this, and if one gets here it must not produce a world that looks valid.
    ///
    /// @param next Cursor read from the snapshot.
    void set_next_entity_id(uint32_t next) { m_next_id = (next < 1u) ? 1u : next; }

private:
    uint32_t m_next_id = 1; ///< Zero is null_entity; live IDs start at 1.
};

/// BL-1195: stamp @p w with a fresh logistics cache generation (see
/// world::logistics_cache_generation). The counter is process-wide and only ever
/// grows, so no two invalidations — on any world — share a value. View-only: the
/// simulation never reads it, so it cannot touch determinism.
inline void bump_logistics_cache_generation(world& w)
{
    static std::atomic<std::uint64_t> s_next{1};
    w.logistics_cache_generation = s_next.fetch_add(1, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Ownership accessors (BL-068 — competitor information asymmetry)
// ---------------------------------------------------------------------------
// Ownership lives in `corporation_component::assets` (the forward list of owned
// buildings); there is no reverse index, so these scan the ~dozen corporations.
// Read-side only — no stored backing, no tick, no determinism impact. A reverse
// `building_to_corp` map is the natural O(1) backing if profiling ever warrants;
// the accessor is the stable seam either way.

// ---------------------------------------------------------------------------
// Shelf helpers (BL-1265 — the shelf economy). Defined in market_clearing.cpp,
// beside `market_for_tile`, whose catchment partition they resolve through.
// ---------------------------------------------------------------------------

/// The lowest-id market on @p body, or `null_entity` when the body has none —
/// for a caller that genuinely names no tile.
entity_id market_on_body(const world& w, entity_id body);

/// The body a market sits on, or `null_entity` for an unknown market.
entity_id market_body(const world& w, entity_id market);

/// A corporation's HOME market on @p body — the shelf it buys from and lands
/// on when nothing names a tile (a hire, a procurement delivery): the market
/// of its HQ tile if the HQ is on @p body, else of its lowest-id building
/// there, else `market_on_body`. `null_entity` when the body has no market.
entity_id corp_home_market(const world& w, entity_id corp, entity_id body);

/// The corporation's home market on its own HQ's body (`corp_home_market`
/// there), or `null_entity` for a corp with no HQ on a body with a market.
entity_id corp_hq_market(const world& w, entity_id corp);

/// The markets @p corp SITS IN: every market whose catchment holds one of its
/// buildings, ascending id, each once. The shelves "the corporation's stock" is
/// read from now corporations hold none (MARKETS.md § The shelf economy).
std::vector<entity_id> corp_markets(const world& w, entity_id corp);

/// The stock of good @p r on the shelves of the markets @p corp sits in
/// (`corp_markets`), summed ascending. What a reading of "what the corporation
/// holds" means under the shelf economy: what it can buy where it stands.
float corp_shelf_stock(const world& w, entity_id corp, std::size_t r);

/// BUY @p qty of good @p r off the shelves of the markets @p corp sits in,
/// lowest market id first, at each shelf's POSTED price, debiting the buyer's
/// balance now (a purchase outside the economy step, such as a hire). Buys only
/// where the fair-price ceiling admits the shelf (@p reservation_mult; 0 = the
/// ceiling is off). Returns the units bought (at most @p qty); the caller
/// checks `corp_shelf_stock` first when it needs all-or-nothing.
float buy_from_corp_shelves(world& w, entity_id corp, std::size_t r, float qty,
                            float reservation_mult);

/// GENERATION ONLY (CORPORATION_GENERATION.md § Pass 4b): add @p stock to
/// @p corp's opening stockpile, held in `world::gen_opening_stock` until
/// `place_opening_stock` puts it on the shelves. Generation's one door for an
/// opening stockpile; every generation site that seeds one calls it.
void seed_opening_stock(world& w, entity_id corp, const std::array<float, resource_count>& stock);

/// GENERATION ONLY: place every corporation's held opening stock on the
/// shelves of the markets it sits in, split over them by how many of its
/// buildings each market's catchment holds (ascending market id; the HQ market
/// alone when it has no building on a market), and empty
/// `world::gen_opening_stock`. No money moves: the stock is the market's from
/// then on (MARKETS.md § The shelf economy). A corporation with no market at
/// all places nothing. Idempotent; called once the markets stand, and again
/// (as a no-op, or for a roster regenerated since) before the first tick.
void place_opening_stock(world& w);

/// Resolve the corporation that owns @p building by scanning each corporation's
/// `assets`. Siblings of `pool_at` / `workforce_supply`.
///
/// @param w        Read-only world state.
/// @param building Building entity id to resolve.
/// @return         Owning corporation id, or `null_entity` if unowned.
entity_id owner_corp_of(const world& w, entity_id building);

/// True iff @p building is owned by the player's corporation. The single branch
/// point for the visibility rule (BL-068): everything not player-owned is treated
/// uniformly as a rival.
///
/// @param w        Read-only world state.
/// @param building Building entity id to test.
/// @return         Whether the owning corporation has `is_player` set.
bool is_player_owned(const world& w, entity_id building);

/// Resolve the body a market entity sits on. Collapses market-level identity to
/// body-level for the trade-route / fog systems (a body may host several markets;
/// all share one lane for visibility). Sibling of `owner_corp_of`.
///
/// @param w      Read-only world state.
/// @param market Market entity id to resolve.
/// @return       Owning body id, or `null_entity` if the market is unknown.
entity_id body_of_market(const world& w, entity_id market);

// ---------------------------------------------------------------------------
// Commercial-sphere activity fog (BL-089)
// ---------------------------------------------------------------------------
// The player's trade network is their intelligence network: where their goods
// flow, the world lights up. A body-level *activity* fog, independent of the
// geographic survey fog (BL-067) — a body can be Known (a route reaches it) yet
// unsurveyed. Derived on demand from trade_routes (BL-088) + live convoys +
// ownership + the current tick; nothing new is stored or serialised.

/// Activity visibility tier of a body, from the player's commercial reach.
enum class activity_vis : uint8_t
{
    unknown,      ///< Outside the player's network: a public astronomy dot only.
    known_stale,  ///< A player route once reached it, but traffic has gone cold.
    known,        ///< A fresh player route reaches it: a coarse market pulse reads.
    visible,      ///< A live player lane touches it, or the player owns a building there.
};

/// Freshness window in sim day ticks: a route whose last completion is within this
/// many ticks of "now" reads as `known`; older reads as `known_stale`. One quarter
/// (90 days) by default — a calibration constant (headless-tuned).
inline constexpr int route_fresh_ticks_default = 90;

/// Proximity-glimpse corridor half-width in AU (BL-099): a body whose closest approach
/// to a completed player lane's endpoint->endpoint segment is within this distance gets a
/// faint glimpse. A calibration constant (headless-tuned) — set so a frontier body one
/// hop off a major lane glimpses while distant bodies do not.
inline constexpr float glimpse_radius_au_default = 0.25f;

/// Freshness window in sim day ticks for a proximity glimpse (BL-099): a body glimpsed
/// within this many ticks of "now" reads as `known_stale`; older decays back to
/// `unknown`. A glimpse is fainter than a route, so it never rises to `known`/`visible`.
inline constexpr int glimpse_fresh_ticks_default = 90;

/// Body-level activity visibility for the player, derived from routes + live convoys
/// + ownership + the current tick. Pure and deterministic; no stored state.
/// `home_body` and any body the player owns a building on are always `visible`.
/// Independent of survey phase (a surveyed-but-unrouted body is still `unknown` for
/// activity; an unsurveyed-but-routed body is `known`).
///
/// @param w                 Read-only world state.
/// @param body              Body to classify.
/// @param now_tick          Current sim day tick (for route freshness).
/// @param route_fresh_ticks Freshness window; defaults to route_fresh_ticks_default.
/// @return                  The body's activity tier for the player.
activity_vis body_activity_visibility(const world& w, entity_id body, int now_tick,
                                      int route_fresh_ticks   = route_fresh_ticks_default,
                                      int glimpse_fresh_ticks = glimpse_fresh_ticks_default);

// ---------------------------------------------------------------------------
// Proximity-glimpse peek (BL-099)
// ---------------------------------------------------------------------------
// The third illumination geometry over the activity fog (after endpoints + corridors):
// a body a player convoy merely passes NEAR (not an endpoint) on a completed lane gets a
// faint, decaying glimpse. Deterministic by sample-and-store — the live orbital_angle_rad
// is render-only state (advanced per frame from wall-clock time; the econ money loop
// instead reads the tick-pure orbital_angle_at_tick, orbital_system.hpp — BL-354), so
// live positions cannot be reconstructed at a later read; instead the closest-approach
// set is sampled once at the discrete completion tick and the glimpse tick is stored.
// No per-frame proximity test, no orbital-drift flicker, no RNG.

/// Closest approach (AU) of `body`'s current position to the lane's endpoint->endpoint
/// line segment, using the flat orbital-plane projection the sim uses (r*cos(theta),
/// r*sin(theta)). Pure read; factored out so the headless harness can assert the geometry
/// directly. Returns a large sentinel if `body` is an endpoint or any id is unknown.
float body_closest_approach_au(const world& w, entity_id body, entity_id lane_a, entity_id lane_b);

/// Sample every body's closest approach to the just-completed player lane (lane_a, lane_b)
/// and stamp a proximity glimpse (body_last_glimpse_tick[body] = tick) on any body within
/// `radius_au` that is neither an endpoint nor the star. Called from credit_arrived_convoys
/// at the discrete completion tick, when orbits have already advanced for the frame — so the
/// sampled positions ARE the completion-tick positions.
void record_proximity_glimpses(world& w, entity_id lane_a, entity_id lane_b, int tick,
                               float radius_au = glimpse_radius_au_default);
