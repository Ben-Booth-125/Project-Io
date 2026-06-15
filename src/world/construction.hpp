#pragma once

#include "components.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

/// Outcome of a player-driven construction attempt. Only `placed` mutates the
/// world; every refusal leaves the world untouched.
enum class construction_result : uint8_t
{
    placed = 0,          ///< Building created, added to the corp, and paid for.
    invalid_tile,        ///< placement_rules::can_place rejected the tile (ocean / wrong deposit).
    insufficient_funds,  ///< The corporation cannot afford the build cost.
    no_corp,             ///< The corporation entity does not exist.
    no_tile,             ///< The tile entity does not exist.
};

/// Attempt to construct a building of `type` (targeting `target` for an extraction
/// site) on `tile` for corporation `corp`, paying the registry build cost from the
/// corporation's balance. The Layer 4 build front door (docs/ui/SELECTION.md, the
/// tile Selection element) and the construction-panel placement seam both route
/// through this single function, which mirrors the generation-time placement
/// (placement_rules + the building-component authoring in corporation_generation.cpp)
/// but is player-driven and debits the balance.
///
/// On success: a new building entity is created with a `stockpile_component`, its
/// `building_component` is authored (workforce staffed at 0.5, 0 for a port; the
/// extraction target set; a processing facility seeded with the default "steel"
/// recipe, reconfigurable later via building management), the building is appended
/// to the corporation's `assets`, the build cost is debited, and `out_building`
/// receives the new id.
///
/// @param w            World; mutated only on success.
/// @param reg          Loaded registry (build cost + the default recipe).
/// @param corp         Corporation building (and paying) — usually w.player_entity.
/// @param tile         Target tile entity.
/// @param type         Building type to construct.
/// @param target       Extraction target (ignored for non-extraction types).
/// @param out_building Set to the new building id on success; null_entity otherwise.
/// @return             `placed` on success, else the reason it was refused.
construction_result construct_building(world& w, const recipe_registry& reg,
                                       entity_id corp, entity_id tile,
                                       building_type type, resource_type target,
                                       entity_id& out_building);
