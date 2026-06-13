#pragma once

#include "components.hpp"

#include <unordered_map>

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

    // --- component stores ---
    std::unordered_map<entity_id, body_component>      bodies;
    std::unordered_map<entity_id, tile_component>      tiles;
    std::unordered_map<entity_id, building_component>  buildings;
    std::unordered_map<entity_id, stockpile_component> stockpiles;
    std::unordered_map<entity_id, market_component>    markets;
    std::unordered_map<entity_id, unit_component>      units;

private:
    uint32_t m_next_id = 1; ///< Zero is null_entity; live IDs start at 1.
};
