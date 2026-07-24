#include "hard_coded_world.hpp"

#include "corporation_generation.hpp"
#include "nation_generation.hpp"
#include "orbital_system.hpp"
#include "population_generation.hpp"
#include "river_generation.hpp"
#include "road_generation.hpp"
#include "tile_generation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <random>
#include <utility>
#include <vector>

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

// Map an abundance tier to the deposit multiplier passed to generate_body_tiles.
// Earth-like `standard` is the ceiling (1.0×); leaner tiers step down (never up).
// See GENERATION_STRATEGY.md § The resource ceiling.
float deposit_scalar_for(abundance_level a)
{
    switch (a)
    {
        case abundance_level::sparse:   return 0.40f;
        case abundance_level::lean:     return 0.65f;
        case abundance_level::standard: return 1.00f;
    }
    return 1.00f;
}

} // namespace

world make_hard_coded_world(world_params params)
{
    world w;

    // The resource-abundance multiplier every body's deposit pass is scaled by.
    // At the default `standard` tier this is 1.0f, so a default-params world is
    // bit-identical to the pre-BL-114 generation.
    const float deposit_scalar = deposit_scalar_for(params.abundance);

    // Nation knob → Voronoi params. Preserve the over-seed/merge shape (BL-053 retune:
    // 34 seeds merged to 24): merge to the requested count, pre-seed extra so the merge
    // pass has real material to absorb (a wider gap gives stronger size variance).
    // Clamped to a sane floor.
    const int merge_to   = params.nation_count < 2 ? 2 : params.nation_count;
    const int pre_seed_n = merge_to + 10;

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
        /*seed=*/params.seed ^ 0xC1D0001u, deposit_scalar);

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

    generation_record kepler_record;
    auto kepler_tiles = generate_body_tiles(w, kepler, 180, 84,
        body_profile{
            .temperature    = temperature_class::temperate,
            .atmosphere     = atmosphere_class::thick,
            .hydrology      = hydrological_state::liquid,
            .geology        = geological_activity::moderate,
            .water_fraction = 0.60f,
            .bias           = composition_bias::standard,
        },
        /*seed=*/params.seed ^ 0xE471001u, deposit_scalar, &kepler_record);

    // River network (BL-170): downhill trace over the Pass-1 height field just
    // captured above, from high ground to ocean/basin, stamping tile.river_edges.
    // Deterministic — a pure function of the height field and tile compositions,
    // no RNG needed. Independent of nations/population, so it can run immediately.
    generate_rivers(w, kepler, kepler_record);

    // Kepler is the only body with a political layer in the prototype: 8–12
    // nations placed over its land tiles. Selene/Cinder/Pallas stay unclaimed.
    // See docs/generation/NATION_GENERATION.md.
    // Population centres must be placed before generate_nations so that Pass 6
    // (substrate density) can reference them during nation territory assignment.
    generate_population_centres(w, kepler, /*seed=*/params.seed ^ 0x70701001u);

    // BL-053: over-seed (34) with tighter separation, then merge down to 24 so the
    // map reads as a varied, "grown" political layer — a few large powers, several
    // mid, many small — rather than near-uniform Voronoi cells.
    generate_nations(w, kepler, kepler_tiles, 180, 84,
        nation_params{ .nation_count = pre_seed_n, .min_seed_separation = 5, .merge_to = merge_to },
        /*seed=*/params.seed ^ 0x4A71012u);

    // Road network (BL-146): stamp each nation's road lattice onto tile.road_level,
    // after nations + population centres exist. Deterministic; no seed of its own —
    // a pure function of the generated tiles/nations/centres.
    generate_roads(w, kepler);

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

    // Kepler markets — population-anchored but RESOURCE-CARVED (BL-096). Markets
    // still anchor to population-centre tiles (catchment routing via market_for_tile
    // partitions the map), but how finely a nation's territory is split into markets
    // is shaped by its tradeable-resource concentration: a resource-rich nation
    // fractures into more markets (a lower population-scale gate admits more of its
    // centres), a barren one folds into its neighbour (a higher gate — its smaller
    // centres get no market and their tiles route to the nearest neighbour's).
    // Nations are the carving actor, so a resource cluster spanning two nations'
    // territory yields two markets. One-pass at world-gen, deterministic; a small
    // seeded jitter varies the borderline split per campaign (fresh XOR offset,
    // uncorrelated with the nation/corp streams). Fuller co-generation is BL-132.
    // Resources outside the tradeable prototype subset stay at base 0 and are never
    // traded. Supply/demand are seeded by the substrate injection each tick.
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

        // Per-nation tradeable-resource concentration = mean raw-deposit richness
        // per owned tile over the tradeable subset. Classified against the
        // cross-nation mean into a population-scale gate (2 = fracture, 3 = normal,
        // 4 = fold). Tunable via rich_factor / barren_factor.
        constexpr resource_type tradeable_raws[] = { resource_type::iron_ore,
                                                      resource_type::petroleum,
                                                      resource_type::water,
                                                      resource_type::agricultural_produce };
        constexpr float rich_factor   = 1.30f; // concentration >= mean × this → fracture (gate 2)
        constexpr float barren_factor = 0.70f; // concentration <  mean × this → fold    (gate 4)

        std::map<entity_id, float> concentration; // std::map → ascending id (deterministic)
        for (const auto& [nid, nc] : w.nations)
        {
            float sum = 0.0f;
            for (const resource_type rt : tradeable_raws)
                sum += nc.resource_abundance[static_cast<std::size_t>(rt)];
            const float tiles = static_cast<float>(std::max<std::size_t>(1, nc.tiles.size()));
            concentration[nid] = sum / tiles;
        }
        // Seeded jitter in ascending nation-id order (deterministic; fresh offset).
        {
            std::mt19937 jitter_rng(params.seed ^ 0xA5310096u);
            std::uniform_real_distribution<float> jitter(0.85f, 1.15f);
            for (auto& [nid, c] : concentration)
                c *= jitter(jitter_rng);
        }
        float mean_conc = 0.0f;
        if (!concentration.empty())
        {
            for (const auto& [nid, c] : concentration)
                mean_conc += c;
            mean_conc /= static_cast<float>(concentration.size());
        }
        auto gate_for_nation = [&](entity_id nid) -> int {
            const auto it = concentration.find(nid);
            if (it == concentration.end() || mean_conc <= 0.0f)
                return 3;
            if (it->second >= mean_conc * rich_factor)   return 2; // fracture: more markets
            if (it->second <  mean_conc * barren_factor) return 4; // fold: fewer markets
            return 3;
        };

        // Seed markets in ascending centre-id order for deterministic market ids.
        std::vector<entity_id> centre_ids;
        centre_ids.reserve(w.population_centres.size());
        for (const auto& [cid, pcc] : w.population_centres)
            centre_ids.push_back(cid);
        std::sort(centre_ids.begin(), centre_ids.end());

        int markets_seeded = 0;
        for (const entity_id cid : centre_ids)
        {
            const population_centre_component& pcc = w.population_centres.at(cid);
            const auto tile_it = w.population_centre_tile.find(cid);
            if (tile_it == w.population_centre_tile.end())
                continue;
            const auto tc_it = w.tiles.find(tile_it->second);
            if (tc_it == w.tiles.end() || tc_it->second.body != kepler)
                continue;

            // Nation-carved population-scale gate for this centre's owning nation.
            int gate = 3;
            const auto nit = w.tile_to_nation.find(tile_it->second);
            if (nit != w.tile_to_nation.end())
                gate = gate_for_nation(nit->second);
            if (pcc.scale < gate)
                continue; // folds into a neighbouring market (routed by market_for_tile)

            market_component mc = kepler_market_template;
            mc.centre_tile = tile_it->second;
            mc.price       = mc.base_price; // start at canonical base
            w.markets[w.create_entity()] = mc;
            ++markets_seeded;
        }
        // Fallback: if no centre qualified, seed one unanchored market.
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
        /*seed=*/params.seed ^ 0x4A71012u);

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
        /*seed=*/params.seed ^ 0x5E1E001u, deposit_scalar);

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
            params.seed ^ a.seed, deposit_scalar);
    }

    return w;
}
