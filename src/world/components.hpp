#pragma once

#include "entity.hpp"

#include <array>
#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// Shared enumerations
// ---------------------------------------------------------------------------

/// Resource types produced, traded, and consumed in the economy. Ordered by the
/// production tiers in docs/economy/RESOURCES.md. All values are defined from the
/// start so array sizes are correct and no data-model retrofit is required as the
/// economy is authored; resources outside the prototype subset simply carry zero
/// tile deposits and no market entries until a later pass authors them.
enum class resource_type : uint8_t
{
    // --- Tier 1: raw materials (Earth-sourced) ---
    iron_ore              = 0,  ///< Backbone structural mineral.
    coal                  = 1,  ///< Carbon energy source and smelting reagent.
    petroleum             = 2,  ///< Liquid hydrocarbon; fuel precursor.
    silica                = 3,  ///< Silicon dioxide; semiconductor and bulk input.
    copper_ore            = 4,  ///< Primary conductive metal ore.
    rare_earth_ore        = 5,  ///< Critical minerals for electronics and magnets.
    agricultural_produce  = 6,  ///< Food crop output; needs water and habitability.
    // --- Tier 1: raw materials (space-sourced) ---
    water                 = 7,  ///< Extracted from surface/subsurface ice.
    iron_nickel_ore       = 8,  ///< Metallic-asteroid feedstock for steel.
    platinum_group_metals = 9,  ///< Ultra-rare catalytic metals; belt's high-value good.
    regolith              = 10, ///< Loose surface material on airless bodies; in-situ build mass.
    // --- Tier 1: ambient (low-value, near-universal) ---
    stone                 = 11, ///< Universal construction aggregate.
    timber                = 12, ///< Construction material and fuel.
    sand                  = 13, ///< Glass precursor; construction aggregate.
    clay                  = 14, ///< Ceramics and construction.
    peat                  = 15, ///< Pre-industrial fuel.
    // --- Tier 2: refined goods (prototype subset) ---
    steel                 = 16, ///< Smelted from iron ore (+ coal).
    refined_fuel          = 17, ///< Refined from petroleum.
    food_rations          = 18, ///< Processed from agricultural produce.
    count                 = 19
};

static constexpr std::size_t resource_count = static_cast<std::size_t>(resource_type::count);

/// Material character of a tile — what it is made of. Determines which resource
/// deposits can appear, its terrain colour, and its base habitability ceiling.
/// One of the two axes of the tile model; see docs/economy/TILES.md.
enum class terrain_composition : uint8_t
{
    barren    = 0,  ///< Dry, dusty, minimal organics; iron ore, coal, petroleum.
    rocky     = 1,  ///< Hard rock outcrops; iron, copper, rare earth ore.
    volcanic  = 2,  ///< Geologically active; rare earth and iron ore. High hazard.
    icy       = 3,  ///< Ice-dominated surface; water ice. Low habitability.
    tundra    = 4,  ///< Cold, sparse vegetation; surface iron, peat.
    grassland = 5,  ///< Open fertile land; agricultural produce. Habitable.
    forest    = 6,  ///< Dense tree cover; timber. Habitable.
    wetland   = 7,  ///< Marsh, bog, floodplain; agricultural produce, clay. Habitable.
    ocean     = 8,  ///< Open deep water; carries no land deposits and no buildings.
    regolith  = 9,  ///< Loose surface material on airless bodies.
    metallic  = 10, ///< High metal content; iron-nickel ore, platinum group metals.
};

/// Physical shape of a tile — its elevation, slope, and form. Modifies the base
/// properties set by composition without changing them. The second axis of the
/// tile model; the build-cost multiplier notes follow docs/economy/TILES.md.
enum class terrain_landform : uint8_t
{
    plains   = 0, ///< Flat, easy access. ×1.0 build cost. Default for most tiles.
    highland = 1, ///< Elevated plateau, moderate slope. ×1.25.
    mountain = 2, ///< Steep peaks; difficult terrain. ×2.0. Boosts mineral richness.
    canyon   = 3, ///< Deep gorge, access from above. ×1.5. Erosion-exposed deposits.
    valley   = 4, ///< Low ground between higher terrain. ×1.1. Fertile.
    crater   = 5, ///< Impact basin; common on airless bodies. ×1.3.
    rift     = 6, ///< Geological fault zone. ×1.6. Strong volcanic association.
};

/// Celestial body classification.
enum class body_type : uint8_t
{
    planet   = 0,
    moon     = 1,
    asteroid = 2,
    station  = 3,
    star     = 4, ///< The system's central star. Sits at the centre, stationary; has no surface.
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
    terrain_composition composition; ///< Material character (geology/ecology).
    terrain_landform    landform;    ///< Physical shape (elevation/slope).
    std::array<float, resource_count> resource_deposit; ///< Available deposit per resource type.
    float      hazard_level;  ///< 0.0 (safe) – 1.0 (extreme hazard).
    float      habitability;  ///< 0.0 (uninhabitable) – 1.0 (hospitable).
};

/// A celestial body — the primary unit of territorial control and the location
/// where extraction, market activity, and conflict occur.
///
/// Orbital model: a body orbits the star unless `parent` is set, in which case
/// it orbits that parent body (a moon around its planet). `orbital_radius_au`
/// and `orbital_angle_rad` are interpreted relative to whichever centre applies.
/// `orbital_angle_rad` advances each frame by `orbital_angular_velocity_rad_per_day`
/// (see advance_orbits in orbital_system.hpp); the authored value is the phase
/// at world construction.
struct body_component
{
    std::string name;
    body_type   type;
    entity_id   parent = null_entity; ///< Body this one orbits; null_entity = orbits the star directly.
    float       orbital_radius_au;    ///< Orbital distance in AU — from the star, or from `parent` if set.
    float       orbital_angle_rad;    ///< Current angular position, radians. 0 = right, increases counter-clockwise.
    float       orbital_angular_velocity_rad_per_day = 0.0f; ///< Angular speed; advances orbital_angle_rad over time. 0 = stationary.
    int         grid_width;           ///< Number of tile columns.
    int         grid_height;          ///< Number of tile rows.
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
