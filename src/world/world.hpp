#pragma once

#include "components.hpp"
#include "campaign_battle.hpp" // active_battle (BL-467 battle state, below)
#include "corp_command.hpp" // corp_decision_ring (BL-202 strategic decision log)
#include "law.hpp"          // law (BL-343 enacted-law list, below)
#include "modifier_set.hpp" // scalar_modifier (BL-479 per-corp tech effects, below)
#include "nation_budget.hpp" // nation_budget (Sprint N3 T2: the persistent weight map, below)
#include "province.hpp"     // province_partition (BL-466 province partition, below)
#include "sentiment.hpp"    // sentiment_table (BL-545 relational substrate, below)

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

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

/// Result of an intra-body pathfind (BL-077): the terrain-weighted path cost, whether the
/// cheapest path crosses ocean (=> sea mode, else land), and whether the endpoints connect.
/// Symmetric in its endpoints (edge cost is the average of the two tiles), so it caches under
/// a canonicalised (body, lo_tile, hi_tile) key.
struct logistics_path
{
    float cost          = 0.0f;
    bool  crosses_ocean = false;
    bool  reachable     = false;
    /// The tile sequence of the best path, in canonical (lo→hi) endpoint order —
    /// i.e. from min(src,dst) to max(src,dst), since the weighted path is symmetric
    /// and cached on the unordered pair. A caller that dispatched src→dst reverses
    /// this when src != lo. Empty when unreachable; a single tile when src == dst.
    /// Populated by intra_body_path (BL-152, for the convoy vision beam); the cost
    /// fields above stand alone for callers that ignore it.
    std::vector<entity_id> tiles;
};

/// One COMPLETED Dijkstra flood over a body's raster grid, anchored at a single
/// tile: distance, parent index and touched-ocean flag for every cell. The
/// many-to-one layer beneath the per-pair path cache (2026-08-25 warm-start
/// stall): dispatch prices hundreds of origins against the same few destination
/// centres, so one flood per centre answers every pair that touches it at
/// path-reconstruction cost instead of a fresh grid search. Byte-identical to
/// the per-pair search's answers — same relaxation order, same tie-breaks, and
/// a settled node's parent is final whether or not the search stopped early.
struct logistics_flood_field
{
    int                anchor_idx = -1;   ///< Raster index of the anchor tile.
    std::vector<float> dist;              ///< Weighted cost anchor -> cell; 1e30f unreached.
    std::vector<int>   came_from;         ///< Parent raster index on the best path, -1 at anchor/unreached.
    std::vector<char>  crossed;           ///< Best path anchor -> cell touches ocean?
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

    // --- component stores ---
    std::unordered_map<entity_id, body_component>      bodies;
    std::unordered_map<entity_id, tile_component>      tiles;
    std::unordered_map<entity_id, building_component>  buildings;
    std::unordered_map<entity_id, stockpile_component> stockpiles;
    std::unordered_map<entity_id, market_component>    markets;
    std::unordered_map<entity_id, unit_component>             units;

    /// Population centre entities keyed by their entity ID. Populated by
    /// generate_population_centres() after tile generation; empty until that
    /// call is made for a body. No AI behaviour in the prototype.
    std::unordered_map<entity_id, population_centre_component> population_centres;

    /// Maps a population centre entity ID to the tile entity it occupies.
    /// Written alongside population_centres by generate_population_centres().
    std::unordered_map<entity_id, entity_id>                   population_centre_tile;

    /// Procedural city name per population centre — the human-readable identity used
    /// by the market ledger's market/city selector and the CSV export. Assigned by
    /// generate_population_centres() from an INDEPENDENT seeded stream (so it does not
    /// perturb world generation). A market's city name resolves via its centre_tile.
    std::unordered_map<entity_id, std::string>                 population_centre_name;

    /// Land-use state per TILE entity, held sparsely: an absent entry is
    /// `undeveloped` (docs/economy/POPULATION.md § Land use). Seeded by
    /// stamp_urban_land_use (BL-612, urban ground stamped) with the urban
    /// footprints under population centres; joins the flat-binary save as its
    /// own store (world_save format v11).
    std::unordered_map<entity_id, land_use_component>          land_use;

    /// Nation entities keyed by their entity ID. Populated by generate_nations()
    /// after tile generation; empty until that call is made for a body.
    std::unordered_map<entity_id, nation_component>    nations;

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
    std::unordered_map<entity_id, entity_id>           tile_to_nation;

    /// Corporation entities keyed by their entity ID. Populated by
    /// generate_corporations() after nation generation; empty until that call
    /// is made. Exactly one entry will have corporation_component::is_player == true,
    /// and world::player_entity will equal that entry's key.
    std::unordered_map<entity_id, corporation_component> corporations;

    /// Shared stockpile pool keyed by (corporation, body). This is the Layer 3
    /// economy's working store — extraction and processing credit/draw it, the
    /// market lists surplus from it. A `std::map` (not unordered) so iteration is
    /// deterministic, mirroring the `tile_to_nation` design rationale: keeping the
    /// pool here off `building_component`/`body_component` lets the economy systems
    /// stay on disjoint files. The per-building `stockpile_component` is unused in L3.
    std::map<std::pair<entity_id, entity_id>, stockpile_component> corp_body_pools;

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
    /// geometry. Held off `body_component` (the `corp_body_pools` rationale) to keep the
    /// body's future flat-binary layout untouched. std::map for deterministic iteration.
    std::map<entity_id, int> body_last_glimpse_tick;

    /// Lazily-built per-body raster index (grid_y*grid_width + grid_x -> tile entity, or
    /// null_entity for an absent cell), for O(1) neighbour lookup in intra-body pathfinding
    /// (BL-077). A derived cache, not authored state: built on first use by body_tile_grid(),
    /// a pure function of the body's tiles (independent of tiles-map iteration order).
    std::unordered_map<entity_id, std::vector<entity_id>> body_tile_index;

    /// Route-cost cache for intra-body A* (BL-077), keyed by (body, lo_tile, hi_tile) with the
    /// tile pair canonicalised (the weighted path is symmetric). A derived cache; invalidated
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

    /// Per-body LOGISTICS REACH FIELD (BL-323 S2): raster-indexed weighted cost from each
    /// tile to its nearest supply anchor — a city, a port, or an inland logistics hub.
    /// Infinity where no anchor is reachable. A derived cache like the two above, built on
    /// first use by `body_reach_field()` and invalidated wherever `astar_cost_cache` is
    /// (road placement, building placement/demolition), because the same mutations move it.
    ///
    /// One multi-source Dijkstra per body rather than an A* per candidate tile: placement
    /// asks this question for every tile under the cursor, and the armed-build tint asks it
    /// for the whole visible grid at once, so a per-query search would be the wrong shape.
    std::unordered_map<entity_id, std::vector<float>> body_reach_cost;

    /// Techs each corporation has EARNED, by tech id (BL-344). Per-corp, never
    /// global: two corporations research independently, and a gate that read a
    /// world-wide set would unlock a rival's content for the player. `std::map`
    /// + `std::set` for deterministic iteration (the `corp_body_pools`
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
    /// (the `corp_body_pools` rationale). Empty at world setup, and stays
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
    /// are created at runtime but never destroyed, so count + max id catches every
    /// mutation. Mutable so the const read path (market_for_tile) can refresh it.
    mutable std::unordered_map<entity_id, std::vector<entity_id>> body_market_index;
    mutable std::size_t body_market_index_count  = 0;           ///< markets.size() at build.
    mutable entity_id   body_market_index_max_id = null_entity; ///< Max market id at build.

    /// THE ORDER BOOK (BL-293) — standing sell orders, world-wide, in the order
    /// they were placed. Read by `clear_markets` every economy tick; written only
    /// through the `place_sell_order` / `remove_sell_order` corp verbs, by the
    /// player and by rival corps alike (Ben, 2026-08-07: "the AI must be able to
    /// trade as a player does").
    ///
    /// It lived on `ui_state` until 2026-08-07, which had two costs: a
    /// `corp_command` had nothing to mutate, so no text-driven player could trade;
    /// and a standing order sat outside the save seam entirely. Both are the same
    /// mistake — the player's game-intent was being held by the surface that draws
    /// it rather than by the world that runs it.
    ///
    /// A `std::vector`, insertion-ordered, exactly like `convoys` and
    /// `trade_routes`: the order book is a queue, and price-time priority means
    /// TIME is load-bearing, so insertion order is semantic and must not be
    /// re-sorted. Erasure is by `id`, not by index, so a removal never renumbers a
    /// surviving order.
    std::vector<sell_order> sell_orders;

    /// The buy side of the same book. `clear_markets` matches it against
    /// `sell_orders`; no press authors one yet (see buy_order).
    std::vector<buy_order> buy_orders;

    /// Next stable order handle. Save-format state: it must persist, or a load
    /// followed by a placement would mint an id a live order already holds.
    /// Monotonic and never reused — an erased order's id does not come back.
    uint32_t next_order_id = 1;

    /// Allocate the next stable order id. Deterministic (a plain counter, in
    /// command-application order); the single point ids are minted from.
    ///
    /// @return A fresh, never-before-issued order handle (always nonzero).
    uint32_t allocate_order_id() { return next_order_id++; }

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
    std::unordered_map<entity_id, condition_set> corp_embargo_conditions;

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

    /// Stockpile pool for a (corporation, body) pair, inserting an empty pool on
    /// first access. The single point through which the economy systems read and
    /// write the shared pool.
    ///
    /// @param corp Corporation entity id.
    /// @param body Body entity id.
    /// @return     Reference to the (corp, body) stockpile, created if absent.
    stockpile_component& pool_for(entity_id corp, entity_id body)
    {
        return corp_body_pools[std::make_pair(corp, body)];
    }

    /// Authored effective workforce supply per (corp, body) — Layer 4 step 1 of the
    /// labour-pool model (docs/economy/POPULATION.md § Workforce model). Absent
    /// entries fall back to `default_workforce_supply`; population centres replace
    /// this authored value with a population-derived figure in step 2. Held off the
    /// component structs (the `corp_body_pools` rationale) so the economy stays on
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
    /// canonicalisation of the econ-tick snapshot — every corporation's balance,
    /// every building's dial state (workforce target/assigned/auto, recipe,
    /// decommissioned, ticks_remaining), every market's resolved price array, every
    /// corp/body stockpile pool, and the order book. Sorted by entity id
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

// ---------------------------------------------------------------------------
// Ownership accessors (BL-068 — competitor information asymmetry)
// ---------------------------------------------------------------------------
// Ownership lives in `corporation_component::assets` (the forward list of owned
// buildings); there is no reverse index, so these scan the ~dozen corporations.
// Read-side only — no stored backing, no tick, no determinism impact. A reverse
// `building_to_corp` map is the natural O(1) backing if profiling ever warrants;
// the accessor is the stable seam either way.

/// Resolve the corporation that owns @p building by scanning each corporation's
/// `assets`. Siblings of `pool_for` / `workforce_supply`.
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
