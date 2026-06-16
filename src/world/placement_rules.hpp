#pragma once

#include "world/components.hpp"

/// Reusable, screen-independent building-placement validity rules.
///
/// The terrain/deposit checks that decide whether a building may sit on a tile
/// were originally inlined in `corporation_generation.cpp` Pass 3. Layer 4 player
/// construction needs the *exact same* check, so the logic lives here as a single
/// source of truth shared by generation (authored starting assets) and the future
/// player-construction flow. See docs/economy/PRODUCTION.md § Extraction
/// (placement rules).
namespace placement_rules {

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
///   be a prototype-extractable resource.
/// - processing_facility / port / none: any non-ocean land tile (`target` ignored).
///
/// @param tc      The candidate tile.
/// @param type    The building to place.
/// @param target  The extraction target resource (ignored for non-extraction types).
/// @return        True if placement is valid.
bool can_place(const tile_component& tc, building_type type, resource_type target);

} // namespace placement_rules
