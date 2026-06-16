#pragma once

#include "components.hpp"

#include <map>
#include <unordered_map>
#include <utility>

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

    /// Nation entities keyed by their entity ID. Populated by generate_nations()
    /// after tile generation; empty until that call is made for a body.
    std::unordered_map<entity_id, nation_component>    nations;

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

private:
    uint32_t m_next_id = 1; ///< Zero is null_entity; live IDs start at 1.
};
