#pragma once

#include "entity.hpp"

#include <array>
#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// Shared enumerations
// ---------------------------------------------------------------------------

/// Resource types produced, traded, and consumed in the economy.
enum class resource_type : uint8_t
{
    iron_ore    = 0,
    ice         = 1,
    silicates   = 2,
    rare_metals = 3,
    count       = 4
};

static constexpr std::size_t resource_count = static_cast<std::size_t>(resource_type::count);

/// Surface classification of a tile. Affects construction cost, extraction
/// yield, and combat conditions (deferred).
enum class terrain_type : uint8_t
{
    barren   = 0, ///< Flat, easy to build on, low extraction cost.
    rocky    = 1, ///< Irregular surface, moderate yield, higher build cost.
    icy      = 2, ///< Ice-dominated; high ice deposit, low habitability.
    volcanic = 3, ///< High hazard; elevated rare metal deposits.
};

/// Celestial body classification.
enum class body_type : uint8_t
{
    planet   = 0,
    moon     = 1,
    asteroid = 2,
    station  = 3,
};

/// Type of surface installation.
enum class building_type : uint8_t
{
    none                = 0,
    extraction_site     = 1,
    processing_facility = 2,
    port                = 3,
};

// ---------------------------------------------------------------------------
// Component structs
// ---------------------------------------------------------------------------

/// Fixed physical description of a tile. Properties are authored at world
/// creation and never mutated during play.
///
/// resource_deposit is indexed by static_cast<std::size_t>(resource_type).
/// A zero value means the resource is absent on this tile.
struct tile_component
{
    entity_id  body;      ///< Body this tile belongs to.
    int        grid_x;    ///< Column index within the body's tile grid.
    int        grid_y;    ///< Row index within the body's tile grid.
    terrain_type terrain;
    std::array<float, resource_count> resource_deposit; ///< Available deposit per resource type.
    float      hazard_level;  ///< 0.0 (safe) – 1.0 (extreme hazard).
    float      habitability;  ///< 0.0 (uninhabitable) – 1.0 (hospitable).
};

/// A celestial body — the primary unit of territorial control and the location
/// where extraction, market activity, and conflict occur.
struct body_component
{
    std::string name;
    body_type   type;
    float       orbital_radius_au; ///< Distance from the system's star in AU.
    int         grid_width;        ///< Number of tile columns.
    int         grid_height;       ///< Number of tile rows.
};

/// Surface installation stub. Combat and production logic are added in later
/// layers; this struct exists so the data model does not need retrofitting.
struct building_component
{
    entity_id     tile;               ///< Tile this building occupies.
    building_type type;
    float         workforce_assigned; ///< 0.0–1.0 fraction of allocated workforce.
};

/// Pooled resource quantities held by an entity.
/// Used for building output buffers, convoy cargo, and similar stores.
///
/// Indexed by static_cast<std::size_t>(resource_type).
struct stockpile_component
{
    std::array<float, resource_count> quantities = {};
};

/// Local exchange for a single body. Supply, demand, and prices resolve at
/// each economy tick boundary.
///
/// All arrays are indexed by static_cast<std::size_t>(resource_type).
struct market_component
{
    entity_id body;
    std::array<float, resource_count> supply;
    std::array<float, resource_count> demand;
    std::array<float, resource_count> price;      ///< Current resolved price; set to base_price until first tick.
    std::array<float, resource_count> base_price; ///< Rarity-derived floor; authored at world creation.
};

/// Deployable unit stub. Combat rules, faction AI, and transport are deferred;
/// this struct exists so the field-level data model is in place.
struct unit_component
{
    entity_id body;  ///< Body where the unit is currently located.
    entity_id owner; ///< Corporation or faction entity that controls this unit.
    int       count; ///< Number of units in the group.
};
