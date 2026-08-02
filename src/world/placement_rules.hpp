#pragma once

#include "world/components.hpp"
#include "world/entity.hpp"

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
inline constexpr resource_type k_extractable[] = {
    resource_type::iron_ore,
    resource_type::petroleum,
    resource_type::water,
    resource_type::agricultural_produce,
};

/// True if the given composition is ocean — buildings are never placed on water.
bool is_ocean_tile(terrain_composition comp);

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
/// positive habitability. Ocean tiles (composition == ocean) and tiles whose
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

/// True if the tile at `tile_id` is coastal — has at least one ocean neighbour
/// in the hex grid. Used to enforce Port placement rules (BL-043).
///
/// @param w       The world (reads tile components).
/// @param tile_id The tile to test.
/// @return        True if any of the 6 hex neighbours is an ocean tile.
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
placement_result can_place_in_world(const world& w, entity_id tile_id,
                                    building_type type, resource_type target);

// ---------------------------------------------------------------------------
// Building stacks (Ben's 2026-07-22 call: "buildings can be stacked, so you can
// build up to a max defined by resource richness")
// ---------------------------------------------------------------------------
// A tile is not a single build slot. Several extraction sites may work the same
// deposit, up to a ceiling the deposit itself sets — a rich seam supports a
// bigger operation than a thin one.
//
// PROVISIONAL RULE. The shape ("richness sets the ceiling") is settled; the
// constant below is not, and the wider question this hangs off — whether stacked
// sites split one nominal output or each draw their own, and how depletion
// behaves across a stack — is an open design item. Both the placement gate and
// the building panel's "how many more" readout call `stack_capacity`, so
// settling the rule means changing this one function.

/// Sites of @p type (and @p target, for extraction) that @p tc can carry at once.
///
/// Extraction scales with the target resource's deposit richness — one site per
/// `k_richness_per_site` of deposit, at least one wherever a deposit exists at
/// all. Every other building type returns 1: richness says nothing about how many
/// refineries a tile can host, so stacking them is not yet a rule.
///
/// @param tc     The candidate tile.
/// @param type   Building type.
/// @param target Extraction target (ignored for non-extraction types).
/// @return       Maximum simultaneous buildings of that kind on the tile (>= 1).
int stack_capacity(const tile_component& tc, building_type type, resource_type target);

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
