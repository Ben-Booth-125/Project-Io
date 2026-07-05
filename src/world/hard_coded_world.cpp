#include "hard_coded_world.hpp"

#include "corporation_generation.hpp"
#include "nation_generation.hpp"
#include "orbital_system.hpp"
#include "population_generation.hpp"
#include "tile_generation.hpp"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <utility>

namespace {

// Build a resource-indexed array from a short list of (resource, value) pairs,
// leaving every unlisted resource at zero. Keeps the market authoring readable
// now that resource_count spans the full economy enum.
std::array<float, resource_count> resource_array(
    std::initializer_list<std::pair<resource_type, float>> entries)
{
    std::array<float, resource_count> a{};
    for (const auto& [res, value] : entries)
        a[static_cast<std::size_t>(res)] = value;
    return a;
}

} // namespace

world make_hard_coded_world()
{
    world w;

    w.player_entity = w.create_entity();

    // -----------------------------------------------------------------------
    // System layout — a loose approximation of Sol.
    // -----------------------------------------------------------------------

    // Helios — the central star. A stationary body at the system centre with no
    // surface; its name titles the Solar minimap. Drawn through the same body
    // pass as every other body (with a star style), so it needs no special case.
    const entity_id helios = w.create_entity();
    w.bodies[helios] = body_component{
        .name                                 = "Helios",
        .type                                 = body_type::star,
        .parent                               = null_entity,
        .orbital_radius_au                    = 0.0f,
        .orbital_angle_rad                    = 0.0f,
        .orbital_angular_velocity_rad_per_day = 0.0f,
        .grid_width                           = 0,
        .grid_height                          = 0,
    };
    w.star_body = helios;

    // -----------------------------------------------------------------------
    // Cinder — hot inner planet (Mercury analogue, 0.39 AU)
    // 180×84 tile grid. Airless and scorching: no liquid water; volcanic and
    // barren surface, with rift zones and mountain ranges from high geology.
    // -----------------------------------------------------------------------

    const entity_id cinder = w.create_entity();
    w.bodies[cinder] = body_component{
        .name                                 = "Cinder",
        .type                                 = body_type::planet,
        .parent                               = null_entity,
        .orbital_radius_au                    = 0.39f,
        .orbital_angle_rad                    = 0.40f,
        .orbital_angular_velocity_rad_per_day = kepler_angular_velocity(0.39f),
        .grid_width                           = 180,
        .grid_height                          = 84,
    };

    generate_body_tiles(w, cinder, 180, 84,
        body_profile{
            .temperature    = temperature_class::scorching,
            .atmosphere     = atmosphere_class::none,
            .hydrology      = hydrological_state::none,
            .geology        = geological_activity::high,
            .water_fraction = 0.0f,
            .bias           = composition_bias::standard,
        },
        /*seed=*/0xC1D0001u);

    // -----------------------------------------------------------------------
    // Kepler — temperate home planet (Earth analogue, ~1.0 AU)
    // 180×84 tile grid. Full climate gradient, 60% ocean, grassland and forest
    // belts. Two installations and a market authored after tile generation.
    // -----------------------------------------------------------------------

    const entity_id kepler = w.create_entity();
    w.bodies[kepler] = body_component{
        .name                                 = "Kepler",
        .type                                 = body_type::planet,
        .parent                               = null_entity,
        .orbital_radius_au                    = 1.00f,
        .orbital_angle_rad                    = 1.05f,
        .orbital_angular_velocity_rad_per_day = kepler_angular_velocity(1.00f),
        .grid_width                           = 180,
        .grid_height                          = 84,
    };

    // Kepler is the corporation's home planet — the game opens on its surface.
    w.home_body = kepler;

    auto kepler_tiles = generate_body_tiles(w, kepler, 180, 84,
        body_profile{
            .temperature    = temperature_class::temperate,
            .atmosphere     = atmosphere_class::thick,
            .hydrology      = hydrological_state::liquid,
            .geology        = geological_activity::moderate,
            .water_fraction = 0.60f,
            .bias           = composition_bias::standard,
        },
        /*seed=*/0xE471001u);

    // Kepler is the only body with a political layer in the prototype: 8–12
    // nations placed over its land tiles. Selene/Cinder/Pallas stay unclaimed.
    // See docs/generation/NATION_GENERATION.md.
    // Population centres must be placed before generate_nations so that Pass 6
    // (substrate density) can reference them during nation territory assignment.
    generate_population_centres(w, kepler, /*seed=*/0x70701001u);

    // BL-053: over-seed (18) with tighter separation, then merge down to 14 so the
    // map reads as a varied, "grown" political layer — a few large powers, several
    // mid, many small — rather than ~10 near-uniform Voronoi cells.
    generate_nations(w, kepler, kepler_tiles, 180, 84,
        nation_params{ .nation_count = 18, .min_seed_separation = 5, .merge_to = 14 },
        /*seed=*/0x4A71012u);

    // Attach installations to the first two land tiles found in raster order.
    {
        auto land = first_land_tiles(kepler_tiles, w, 180, 84, 2);

        const entity_id kepler_extraction = w.create_entity();
        w.buildings[kepler_extraction] = building_component{
            .tile               = land.size() > 0 ? land[0] : null_entity,
            .type               = building_type::extraction_site,
            .workforce_assigned = 0.75f,
        };
        w.stockpiles[kepler_extraction] = stockpile_component{};

        const entity_id kepler_processor = w.create_entity();
        w.buildings[kepler_processor] = building_component{
            .tile               = land.size() > 1 ? land[1] : null_entity,
            .type               = building_type::processing_facility,
            .workforce_assigned = 0.50f,
        };
        w.stockpiles[kepler_processor] = stockpile_component{};
    }

    // Kepler markets — one per major population centre (scale >= 3), anchored to
    // that centre's tile so catchment routing (market_for_tile) partitions the map.
    // Resources outside the tradeable prototype subset stay at base 0 and are
    // never traded. Supply/demand are seeded by the substrate injection each tick;
    // no warm-start values are set here (BL-035 handles that separately).
    {
        const market_component kepler_market_template{
            .body       = kepler,
            .base_price = resource_array({ {resource_type::iron_ore,               2.5f},
                                           {resource_type::petroleum,              3.5f},
                                           {resource_type::water,                  1.5f},
                                           {resource_type::agricultural_produce,   3.0f},
                                           {resource_type::steel,                  8.0f},
                                           {resource_type::refined_fuel,          10.0f},
                                           {resource_type::food_rations,           6.0f} }),
        };
        int markets_seeded = 0;
        for (const auto& [cid, pcc] : w.population_centres)
        {
            if (pcc.scale < 3)
                continue;
            const auto tile_it = w.population_centre_tile.find(cid);
            if (tile_it == w.population_centre_tile.end())
                continue;
            const auto tc_it = w.tiles.find(tile_it->second);
            if (tc_it == w.tiles.end() || tc_it->second.body != kepler)
                continue;

            market_component mc = kepler_market_template;
            mc.centre_tile = tile_it->second;
            mc.price       = mc.base_price; // start at canonical base
            w.markets[w.create_entity()] = mc;
            ++markets_seeded;
        }
        // Fallback: if no large centres were generated, seed one unanchored market.
        if (markets_seeded == 0)
        {
            market_component mc = kepler_market_template;
            mc.price = mc.base_price;
            w.markets[w.create_entity()] = mc;
        }
    }

    // Corporations: 6–10 actors registered in the generated nations, including
    // the player's (which sets w.player_entity). Runs after the nations exist and
    // after the pre-authored Kepler installations are in w.buildings, so corporate
    // asset placement collision-avoids those tiles. See CORPORATION_GENERATION.md.
    generate_corporations(w, corporation_params{ .corporation_count = 8 },
        /*seed=*/0x4A71012u);

    // Player unit stub on Kepler.
    const entity_id kepler_unit = w.create_entity();
    w.units[kepler_unit] = unit_component{
        .body  = kepler,
        .owner = w.player_entity,
        .count = 50,
    };

    // -----------------------------------------------------------------------
    // Selene — Kepler's moon (Luna analogue)
    // 90×42 tile grid (the planet ratio at half scale). Airless regolith surface,
    // icy at the polar rows only, crater-dominated landforms.
    // -----------------------------------------------------------------------

    const entity_id selene = w.create_entity();
    w.bodies[selene] = body_component{
        .name                                 = "Selene",
        .type                                 = body_type::moon,
        .parent                               = kepler,
        .orbital_radius_au                    = 0.30f,
        .orbital_angle_rad                    = 0.0f,
        .orbital_angular_velocity_rad_per_day = 0.30f,
        .grid_width                           = 90,
        .grid_height                          = 42,
    };

    generate_body_tiles(w, selene, 90, 42,
        body_profile{
            .temperature    = temperature_class::cold,
            .atmosphere     = atmosphere_class::none,
            .hydrology      = hydrological_state::polar_frozen,
            .geology        = geological_activity::none,
            .water_fraction = 0.0f,
            .bias           = composition_bias::standard,
        },
        /*seed=*/0x5E1E001u);

    // -----------------------------------------------------------------------
    // Asteroid belt — a band beyond Kepler. The belt itself is not a body; it
    // is system-level data the Solar canvas renders as a textured ring. One
    // notable asteroid (Pallas) sits within the band as a separate, selectable
    // body entity drawn over it, carrying a small tile grid (no water) so it is
    // explorable like the planets. See backlog.json (asteroid belt) and SOLAR.md.
    // -----------------------------------------------------------------------
    w.belt = asteroid_belt{ /*inner_radius_au=*/2.10f, /*outer_radius_au=*/3.30f };

    struct notable_asteroid
    {
        const char* name;
        float       radius_au;
        float       angle_rad;
        uint32_t    seed;
    };
    constexpr notable_asteroid notables[] = {
        { "Pallas", 3.05f, 4.6f, 0x9A11A5u },
    };

    for (const notable_asteroid& a : notables)
    {
        const entity_id id = w.create_entity();
        w.bodies[id] = body_component{
            .name                                 = a.name,
            .type                                 = body_type::asteroid,
            .parent                               = null_entity,
            .orbital_radius_au                    = a.radius_au,
            .orbital_angle_rad                    = a.angle_rad,
            .orbital_angular_velocity_rad_per_day = kepler_angular_velocity(a.radius_au),
            .grid_width                           = 30,
            .grid_height                          = 14,
        };

        // Metallic asteroid: mostly metallic surface with rocky minority and
        // scattered craters; no organics, no water.
        generate_body_tiles(w, id, 30, 14,
            body_profile{
                .temperature    = temperature_class::cold,
                .atmosphere     = atmosphere_class::none,
                .hydrology      = hydrological_state::none,
                .geology        = geological_activity::none,
                .water_fraction = 0.0f,
                .bias           = composition_bias::metallic,
            },
            a.seed);
    }

    return w;
}
