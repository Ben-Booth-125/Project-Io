#pragma once

#include "world/components.hpp"
#include "world/entity.hpp"

#include <vector>

struct world; // forward declaration for can_place_in_world / is_coastal

/// Reusable, screen-independent building-placement validity rules.
///
/// The terrain/deposit checks that decide whether a building may sit on a tile
/// were originally inlined in `corporation_generation.cpp` Pass 3. Layer 4 player
/// construction needs the *exact same* check, so the logic lives here as a single
/// source of truth shared by generation (authored starting assets) and the future
/// player-construction flow. See docs/economy/PRODUCTION.md § Extraction
/// (placement rules).
namespace placement_rules {

/// Why a placement is (in)valid. `ok` is the sole success value; every other
/// value names a *specific* reason the tile was rejected, so the UI can teach the
/// player *why* rather than greying the tile silently (BL-071). This is the single
/// shared placement vocabulary — `construction_result` (construction.hpp) covers the
/// player-flow reasons placement never sees (funds, materials, missing entities) and
/// maps its `invalid_tile` onto these codes.
///
/// Not every value is emitted by `can_place` / `can_place_in_world` today: the
/// terrain seam produces `ocean`, `no_deposit`, `not_coastal`, `launchpad_exists`.
/// The remaining codes (`occupied`, `outside_territory`, `unsurveyed`, `slot_full`)
/// are part of the vocabulary for callers that check those gates and for future
/// placement rules; they are defined here so there is one enum, not two.
enum class placement_reason : uint8_t
{
    ok = 0,            ///< Placement is valid.
    ocean,             ///< Tile is water — no building sits on water.
    no_deposit,        ///< Extraction site with no extractable deposit of the target.
    not_coastal,       ///< Port (or a coastal-only extraction target) must sit next to open water.
    launchpad_exists,  ///< A launchpad already exists on this body (max 1).
    occupied,          ///< Tile already carries a building (reserved vocabulary).
    outside_territory, ///< Tile is outside the player's territory (reserved).
    unsurveyed,        ///< Body not yet surveyed (reserved).
    slot_full,         ///< A per-type build cap is reached (reserved, generic).
    no_tile,           ///< Tile entity does not exist (defensive; mirrors construction_result::no_tile).
    already_road,      ///< Road placement onto a tile that already carries a road (BL-147).
    deposit_present,   ///< Hydroponics Bay (BL-166): tile already carries the terrain deposit a Farm would use.
    out_of_logistics_range, ///< BL-323 S2: too far from any city / port / logistics hub to be supplied.
    tech_locked,       ///< BL-344: the corporation has not earned the tech that unlocks this type.
    province_full,     ///< BL-513: the province is at the total-buildings ceiling its land sustains.
    needs_centre,      ///< BL-615: this building must stand ON a population-centre tile.
    centre_too_small,  ///< BL-615: the hosting centre is below the building's required stratum.
    far_from_centre,   ///< BL-615: no population centre within the building's proximity radius.
};

/// Human-readable one-line explanation for a placement reason, for surfacing on
/// the hover card and the Selection panel's build front door.
const char* placement_reason_text(placement_reason r);

/// The outcome of a placement check: a reason code plus its human string. Converts
/// implicitly to `bool` (true iff `ok`) so every existing `if (can_place(...))` /
/// `!can_place(...)` / `bool x = can_place(...)` call site compiles unchanged; the
/// reason is read only where the UI wants to explain a rejection.
struct placement_result
{
    placement_reason reason = placement_reason::ok;

    constexpr placement_result() = default;
    constexpr placement_result(placement_reason r) : reason(r) {}

    /// True iff the placement is valid. Non-explicit by design (BL-071): the whole
    /// point is that the bool call sites are untouched by the refactor.
    constexpr operator bool() const { return reason == placement_reason::ok; }
    constexpr bool ok() const { return reason == placement_reason::ok; }

    /// The human string for this result's reason.
    const char* message() const { return placement_reason_text(reason); }
};

/// The raw resources the Layer 3 extraction buildings can actually target
/// (Mine → iron ore, Oil Platform → petroleum, Ice Extractor → water,
/// Farm → agricultural produce). An extraction site is only productive on a tile
/// carrying one of these. See docs/economy/PRODUCTION.md § Layer 3 prototype scope.
///
/// BL-323 S1 (2026-08-08): widened from the original four to every resource
/// tile_generation.cpp already deposits but no target reached — the Mine's full
/// mineral spread (coal, silica, copper ore, rare earth ore), the Quarry/Lumber
/// Camp ambient goods (stone, sand, clay, timber), and the Era 1 off-world pair
/// (Ice Extractor's water is already covered; Surface Extractor's iron-nickel
/// ore, platinum group metals, regolith). No placement_rules.cpp change needed —
/// `can_place`, the build-mode target picker, and the resource presentation
/// table are all already generic over this list.
inline constexpr resource_type k_extractable[] = {
    resource_type::iron_ore,
    resource_type::petroleum,
    resource_type::water,
    resource_type::agricultural_produce,
    resource_type::coal,
    resource_type::silica,
    resource_type::copper_ore,
    resource_type::rare_earth_ore,
    resource_type::stone,
    resource_type::sand,
    resource_type::clay,
    resource_type::timber,
    resource_type::iron_nickel_ore,
    resource_type::platinum_group_metals,
    resource_type::regolith,
    // BL-429 slice 2: peat's deposits were authored in tile_generation.cpp
    // (wetland tiles) but never added here, so no extraction_site could ever
    // target it — a real, pre-existing gap, not new scope. Peat Cutting is now
    // placeable, and the Peat Kiln recipe (recipes.lua) gives it a consumer.
    resource_type::peat,
    // BL-586 slice 2 (2026-08-24): `hides` (endemic, tile_generation.cpp's
    // C -> D pass off planetology::endemics) and `fibre` (ambient, the same
    // cover-based mechanic as agricultural_produce) both get real recipe
    // consumers this change (Tannery, Weaver) — registered here so an
    // extraction_site can actually target them, following BL-429 slice 2's
    // own peat precedent rather than repeating its gap.
    resource_type::hides,
    resource_type::fibre,
    // BL-647 (endemic luxury demand): the four PRE-EXISTING endemic goods,
    // deposited by the same C -> D pass since BL-191 but never added here —
    // the gap BL-586 slice 2 recorded and left. They join in the same change
    // that gives them a buyer (inject_endemic_demand, market_clearing.cpp):
    // a luxury basket naming four goods nothing can mine is a channel that
    // cannot clear. No placement_rules.cpp change needed — can_place, the
    // build-mode target picker and the presentation table are generic over
    // this list (BL-323 S1's own observation).
    resource_type::tobacco,
    resource_type::spices,
    resource_type::coffee,
    resource_type::furs,
};

/// True if the given substrate is WATER OF ANY KIND — ocean, coast or lake.
/// Buildings are never placed on water, whichever kind it is.
///
/// A one-line SUBSTRATE test since BL-519: water is a property of the ground.
/// BL-516 split it into the three kinds on that same axis, and renamed this
/// from `is_ocean_tile` in the same change — the question every caller was
/// asking is "is this water", and the old name stopped being true of the
/// answer. It delegates to `is_water` (components.hpp), the single definition;
/// this spelling exists because most callers are already in placement.
bool is_water_tile(terrain_substrate sub);

/// True if `r` is one of the prototype-extractable resources.
bool is_extractable(resource_type r);

/// Summed deposit of the prototype-extractable resources on a tile.
float extractable_deposit(const tile_component& tc);

/// The richest prototype-extractable resource on a tile. `any` is set false when
/// the tile carries none (callers fall back to a default target).
resource_type richest_extractable(const tile_component& tc, bool& any);

/// May the player place a road segment on this tile (BL-147)? A road is a per-tile
/// mutation (raises tile_component.road_level, lowering its A* traversal cost), not a
/// building. Valid only on a non-ocean land tile that does not already carry a road.
///
/// @param tc    The candidate tile.
/// @param tier  The road tier to place (BL-172): 1=Track, 2=Road, 3=Highway. Upgrade-in-place —
///              valid only if strictly higher than the tile's current road_level.
/// @return    `ok`, or `ocean` / `already_road` (already at or above @p tier). Converts to bool.
placement_result can_place_road(const tile_component& tc, std::uint8_t tier = 1);

/// Returns true if a population centre may be placed on this tile.
///
/// A population centre requires a non-ocean, non-deep-ocean land tile with
/// positive habitability. Ocean tiles (substrate == ocean) and tiles whose
/// habitability is zero (harsh, uninhabitable terrain) are rejected.
///
/// @param tc  The candidate tile.
/// @return    True if a population centre may be placed here.
bool can_place_population_centre(const tile_component& tc);

/// The load-bearing validity check: may a building of `type` targeting
/// `target` be placed on tile `tc`?
///
/// - Never on ocean (any building type).
/// - extraction_site: only on a non-zero deposit of `target`, and `target` must
///   be a prototype-extractable resource. **Exception (Fishing Wharf, BL-168):**
///   an `agricultural_produce` target with no deposit still passes this tile-only
///   check — its coastal requirement needs world access, so it is enforced by
///   `can_place_in_world` instead of rejected here.
/// - processing_facility targeting `agricultural_produce` (Hydroponics Bay,
///   BL-166): only where the terrestrial Farm deposit was NOT seeded
///   (`resource_deposit[agricultural_produce] == 0`) — the mirror image of the
///   extraction deposit check. Every other processing target/recipe is
///   unaffected (any non-ocean land tile).
/// - port / inland_logistics_hub / none: any non-ocean land tile (`target` ignored).
///
/// @param tc      The candidate tile.
/// @param type    The building to place.
/// @param target  The extraction target resource (ignored for most non-extraction types).
/// @return        A placement_result: `ok`, `ocean`, `no_deposit`, or `deposit_present`.
///                Converts to bool (true iff ok) for the existing boolean call sites.
placement_result can_place(const tile_component& tc, building_type type, resource_type target);

/// True if the tile at `tile_id` is coastal — has at least one SEA neighbour in
/// the hex grid. Used to enforce Port placement rules (BL-043).
///
/// BL-516 NARROWED this from "any water neighbour" to "any sea neighbour"
/// (`coast` or `ocean`, never `lake`). A port, a Fishing Wharf and a
/// coastal-only extraction target all mean the sea; before water had kinds the
/// code could not tell a lakeshore from a shoreline and counted both. This is
/// the only water test in the file whose ANSWER changes.
///
/// @param w       The world (reads tile components).
/// @param tile_id The tile to test.
/// @return        True if any of the 6 hex neighbours is sea (coast or ocean).
bool is_coastal(const world& w, entity_id tile_id);

/// Full placement check including world-level constraints (BL-043):
///  1. Tile-level can_place (ocean / deposit / terrain).
///  2. Port: tile must be coastal (is_coastal).
///  3. Fishing Wharf (BL-168): an extraction_site targeting agricultural_produce
///     with no deposit on the tile must also be coastal (is_coastal).
///  4. Launchpad: at most 1 per body; returns false when one already exists.
///
/// Use this at player construction time; generation uses can_place directly
/// (world state is incomplete during generation passes).
///
/// @param w       The world (reads tiles, buildings for count checks).
/// @param tile_id The candidate tile entity.
/// @param type    Building type to place.
/// @param target  Extraction target (ignored for non-extraction types).
/// @return        A placement_result: `ok`, the tile-level reason from can_place, or
///                `not_coastal` / `launchpad_exists`. Converts to bool for existing
///                boolean call sites.
/// @param max_reach  BL-323 S2 logistics-reach budget, in the same weighted units
///                   `logistics.hpp`'s cost function uses. **Negative disables the
///                   rule** — the default, so every pre-existing call site keeps its
///                   old meaning and only callers that opt in enforce reach. Requires
///                   `body_reach_field()` to have been built for the tile's body; when
///                   it has not, the rule is skipped rather than guessed at (see
///                   `tile_reach_cost`'s -1 contract).
/// @param corp       BL-344 tech gate: the corporation asking. `null_entity`
///                   (the default) disables the check, so every pre-existing
///                   call site keeps its old meaning — a check that only
///                   describes a TILE has no corp to ask about, while a check
///                   that gates an ACTION does. When set, a type gated behind an
///                   unearned tech is refused with `tech_locked`.
/// @param gate       BL-615 stratum placement gate (POPULATION.md § Strata gate
///                   buildings) for the SPECIFIC named building being placed —
///                   resolve it with `recipe_registry::placement_gate_for`. The
///                   default gates nothing, so every pre-existing call site
///                   keeps its old meaning; only callers with registry access
///                   (construct_building, the authoritative seam) enforce it.
///                   Refusals: `needs_centre` (no population centre on the
///                   tile), `centre_too_small` (a centre, but below
///                   `min_centre_scale`), `far_from_centre` (no centre within
///                   `centre_proximity_radius` grid steps on this body).
placement_result can_place_in_world(const world& w, entity_id tile_id,
                                    building_type type, resource_type target,
                                    float max_reach = -1.0f,
                                    entity_id corp = null_entity,
                                    placement_gate gate = {});

// ---------------------------------------------------------------------------
// Building stacks (Ben's 2026-07-22 call: "buildings can be stacked, so you can
// build up to a max defined by resource richness"; rule settled 2026-07-26, BL-193)
// ---------------------------------------------------------------------------
// A tile is not a single build slot. Several extraction sites may work the same
// deposit, up to a ceiling the deposit itself sets — a rich seam supports a
// bigger operation than a thin one.
//
// Two rules govern the stack, and they pull against each other on purpose
// (docs/economy/PRODUCTION.md § Building stacks):
//
//   * **Output diminishes per site.** The k-th site of a stack — counted in
//     stored order, oldest first — yields `k_stack_output_decay^(k-1)` of what it
//     would yield alone. Two sites make 1.8x, five make 3.36x, never 5x.
//   * **Depletion is honest and shared.** Every site draws REAL reserve for what
//     it extracts, so a full stack drains the deposit faster than its output
//     multiple, and the whole stack tapers and exhausts together (the stack
//     pre-pass in economy_system.cpp § run_extraction).
//
// So stacking buys throughput now at the cost of the tile's life; spreading buys
// life at the cost of transport and land. Neither dominates. There are no clamps
// and no fake caps under this — the curve IS the economics.

/// Deposit richness that buys one additional extraction site. Deposits generate in
/// the 0-250 band (tile_generation.cpp § generate_deposits), so this puts a thin
/// seam at 1 site and the richest tiles at 5.
///
/// SETTLED (BL-193, 2026-07-26). Kept at 50 deliberately: with the decay curve
/// above carrying the economics, the ceiling is a **legibility** bound — how many
/// markers a tile can host and a player can reason about — not the balance lever.
inline constexpr float k_richness_per_site = 50.0f;

/// Per-site output decay across a stack (BL-193). The k-th site yields
/// `k_stack_output_decay^(k-1)` of a lone site's rate: 1, 0.8, 0.64, 0.512, 0.4096,
/// summing to 1.0 / 1.8 / 2.44 / 2.952 / 3.3616 for stacks of 1-5.
inline constexpr float k_stack_output_decay = 0.8f;

/// Output multiplier for the @p rank-th site of a stack — `k_stack_output_decay`
/// raised to `rank - 1`, by repeated multiplication rather than `std::pow` so the
/// figure is bit-identical everywhere the simulation runs.
///
/// @param rank 1-based position in the tile's stack (stored order, oldest first).
///             Anything at or below 1 reads as rank 1 — a lone site is never
///             diminished for want of a stack entry.
/// @return     The scalar on this site's nominal output (0 < s <= 1).
float stack_output_scalar(int rank);

/// Sites of @p type (and @p target, for extraction) that @p tc can carry at once.
///
/// Extraction scales with the target resource's deposit richness — one site per
/// `k_richness_per_site` of deposit, at least one wherever a deposit exists at
/// all. Every other building type is bounded by `non_extraction_stack_cap` — the
/// question BL-193 deferred, answered by BL-366.
///
/// @param tc     The candidate tile.
/// @param type   Building type.
/// @param target Extraction target (ignored for non-extraction types).
/// @return       Maximum simultaneous buildings of that kind on the tile (>= 1).
int stack_capacity(const tile_component& tc, building_type type, resource_type target);

/// BL-366: ceiling on **total** non-extraction buildings (processors, ports, hubs,
/// admin, amenity, military base, research institute — combined, not per type) a
/// tile of the given starting substrate and cover can carry before it transforms to
/// `urban`. Extraction stacking is a separate, richness-bound axis untouched by
/// this cap. `urban` itself returns a high ceiling, soft-bounded in practice by
/// workforce contention rather than this number; `ocean` returns 0 (no buildings
/// there at all — `can_place` already refuses it).
///
/// SPLIT BY MEANING (BL-519): the substrate sets the base — how much weight the
/// ground itself will take — and the cover modifies it, because a forest is
/// harder to build on than the rock beneath it and a paved tile is easier than
/// either. The pre-split per-composition numbers are reproduced exactly; see the
/// calibration table in the .cpp.
///
/// EVERY WATER KIND returns 0 (BL-516) — no buildings there at all, and
/// `can_place` already refuses them.
///
/// @param sub     The tile's terrain_substrate.
/// @param cov     The tile's terrain_cover.
/// @return        Non-extraction building ceiling for that pair.
int non_extraction_stack_cap(terrain_substrate sub, terrain_cover cov);

/// Total non-extraction buildings (every type except extraction_site) standing on
/// @p tile_id — the aggregate `non_extraction_stack_cap` is the ceiling for.
///
/// @param w       Read-only world state.
/// @param tile_id Tile to count on.
/// @return        Count of non-extraction buildings on the tile.
int non_extraction_buildings_on_tile(const world& w, entity_id tile_id);

/// BL-366: if @p tile_id's non-extraction building count has reached its
/// cap, flips `terrain_cover` to `urban` and LEAVES THE SUBSTRATE ALONE. One-way — a no-op
/// if the tile is already `urban` or `ocean`. Pure function of tile state (no
/// RNG); call after a non-extraction building is placed.
///
/// @param w       Mutable world state.
/// @param tile_id Tile to check.
/// @return        true if the tile transformed this call.
bool maybe_transform_to_urban(world& w, entity_id tile_id);

/// The tile's stack of @p type / @p target buildings in **stored order, oldest
/// first** — ascending entity id, which is creation order because ids are handed
/// out monotonically (`world::create_entity`).
///
/// The order is load-bearing, not cosmetic: it fixes which site is rank 1 and so
/// which site takes the undiminished yield. It must therefore never be read off
/// `world::buildings`' own (unordered) iteration order.
///
/// Counts every standing site, including decommissioned and still-building ones —
/// the same population `buildings_on_tile` counts against `stack_capacity`, so a
/// site's rank is simply its place in the tile's build order.
///
/// @param w       Read-only world state.
/// @param tile_id Tile to walk.
/// @param type    Building type to match.
/// @param target  Extraction target to match (ignored for non-extraction types).
/// @return        Matching building ids, ascending.
std::vector<entity_id> stack_members(const world& w, entity_id tile_id,
                                     building_type type, resource_type target);

/// 1-based rank of @p building_id within its own tile's stack (see stack_members).
///
/// @param w           Read-only world state.
/// @param building_id The building to locate.
/// @return            Its rank, or 0 if the building no longer exists.
int stack_rank(const world& w, entity_id building_id);

/// Count buildings of @p type (and @p target, for extraction) already standing on
/// @p tile_id. The current occupancy that `stack_capacity` is the ceiling for.
///
/// @param w       Read-only world state.
/// @param tile_id Tile to count on.
/// @param type    Building type to match.
/// @param target  Extraction target to match (ignored for non-extraction types).
/// @return        Number of matching buildings on the tile.
int buildings_on_tile(const world& w, entity_id tile_id,
                      building_type type, resource_type target);

} // namespace placement_rules
