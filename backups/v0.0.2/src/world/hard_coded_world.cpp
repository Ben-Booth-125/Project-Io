#include "hard_coded_world.hpp"

#include "orbital_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <random>
#include <unordered_map>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Procedural tile generation
// ---------------------------------------------------------------------------

// Terrain weight: relative probability of this terrain on land tiles.
struct land_tier
{
    terrain_type terrain;
    int          weight;
};

// The 6 hex neighbours of (col, row) in odd-r offset coordinates.
// Column wraps at gw; rows outside [0, gh) are omitted.
void hex_neighbors(int col, int row, int gw, int gh,
                   std::pair<int,int> out[6], int& count)
{
    // For pointy-top odd-r: even-row and odd-row neighbour offsets.
    static constexpr int even_dc[6] = {  0, -1,  1, -1,  0, -1 };
    static constexpr int even_dr[6] = { -1, -1,  0,  0,  1,  1 };
    static constexpr int odd_dc[6]  = {  0,  1,  1, -1,  0, -1 };
    static constexpr int odd_dr[6]  = { -1, -1,  0,  0,  1,  1 };

    const int* dc = (row & 1) ? odd_dc : even_dc;
    const int* dr = (row & 1) ? odd_dr : even_dr;

    count = 0;
    for (int i = 0; i < 6; ++i)
    {
        const int nr = row + dr[i];
        if (nr < 0 || nr >= gh)
            continue;
        const int nc = ((col + dc[i]) % gw + gw) % gw; // horizontal wrap
        out[count++] = {nc, nr};
    }
}

// Generate a full hex tile grid for a body. Returns the tile entity IDs in
// raster order: index = row * gw + col. Tile placement:
//   1. BFS flood fill from a random interior seed to place contiguous water.
//   2. Land tiles get terrain drawn from `tiers` by weight.
//   3. Resource deposits and hazard/habitability vary by terrain with mild jitter.
//
// `seed` should be unique per body for independent results.
std::vector<entity_id> generate_body_tiles(
    world& w,
    entity_id body_id,
    int gw, int gh,
    float water_fraction,
    const std::vector<land_tier>& tiers,
    float base_hazard,
    float base_habitability,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    const int total        = gw * gh;
    const int water_target = static_cast<int>(static_cast<float>(total) * water_fraction);

    // --- BFS water flood fill ---
    //
    // Body grids follow a ~9:5 width:height ratio (height a little under half the
    // width): the width spans the full circumference (both hemispheres) and the
    // height is pole-to-pole with the non-traversable polar caps truncated. The
    // two planets are 180×84 and Selene (a moon) is 90×42; see PLANETARY.md.
    std::vector<bool> is_water(total, false);
    {
        // Seed the BFS from the entire centre row so the ocean grows as a
        // horizontal equatorial band rather than a radial blob. Shuffled
        // neighbour expansion still produces an irregular coastline.
        std::vector<bool> visited(total, false);
        std::queue<std::pair<int,int>> q;
        const int center_row = gh / 2;
        for (int col = 0; col < gw; ++col)
        {
            visited[col + center_row * gw] = true;
            q.push({col, center_row});
        }

        int water_count = 0;
        while (!q.empty() && water_count < water_target)
        {
            auto [col, row] = q.front();
            q.pop();
            is_water[col + row * gw] = true;
            ++water_count;

            std::pair<int,int> nbrs[6];
            int n;
            hex_neighbors(col, row, gw, gh, nbrs, n);
            // Shuffle neighbours so the ocean grows in an irregular shape.
            for (int i = n - 1; i > 0; --i)
            {
                std::uniform_int_distribution<int> pick(0, i);
                std::swap(nbrs[i], nbrs[pick(rng)]);
            }
            for (int i = 0; i < n; ++i)
            {
                const int nidx = nbrs[i].first + nbrs[i].second * gw;
                if (!visited[nidx])
                {
                    visited[nidx] = true;
                    q.push(nbrs[i]);
                }
            }
        }
    }

    // Cumulative weight table for land terrain selection.
    int total_weight = 0;
    std::vector<int> cumulative;
    cumulative.reserve(tiers.size());
    for (const auto& t : tiers)
    {
        total_weight += t.weight;
        cumulative.push_back(total_weight);
    }

    // --- Create tile entities ---
    std::vector<entity_id> tile_ids(total, null_entity);
    std::uniform_int_distribution<int> weight_roll(0, total_weight - 1);
    std::uniform_real_distribution<float> jitter(-0.05f, 0.05f);
    std::uniform_real_distribution<float> dep_roll(0.0f, 1.0f);

    for (int row = 0; row < gh; ++row)
    {
        for (int col = 0; col < gw; ++col)
        {
            const int idx = col + row * gw;

            terrain_type terrain;
            float hazard, habitability;
            std::array<float, resource_count> deposits = {};

            if (is_water[idx])
            {
                terrain      = terrain_type::water;
                hazard       = std::clamp(base_hazard * 0.3f + jitter(rng), 0.0f, 1.0f);
                habitability = std::clamp(base_habitability * 1.2f + jitter(rng), 0.0f, 1.0f);
            }
            else
            {
                // Pick land terrain by weight.
                const int roll = weight_roll(rng);
                terrain = tiers.back().terrain;
                for (std::size_t i = 0; i < cumulative.size(); ++i)
                {
                    if (roll < cumulative[i]) { terrain = tiers[i].terrain; break; }
                }

                float haz_mult = 1.0f, hab_mult = 1.0f;
                switch (terrain)
                {
                    case terrain_type::barren:   haz_mult = 0.8f;  hab_mult = 1.1f;  break;
                    case terrain_type::rocky:    haz_mult = 1.2f;  hab_mult = 0.85f; break;
                    case terrain_type::icy:      haz_mult = 1.0f;  hab_mult = 0.6f;  break;
                    case terrain_type::volcanic: haz_mult = 2.5f;  hab_mult = 0.2f;  break;
                    default: break;
                }
                hazard       = std::clamp(base_hazard       * haz_mult + jitter(rng), 0.0f, 1.0f);
                habitability = std::clamp(base_habitability * hab_mult + jitter(rng), 0.0f, 1.0f);

                switch (terrain)
                {
                    case terrain_type::barren:
                        deposits[0] = dep_roll(rng) * 150.0f; // iron
                        deposits[2] = dep_roll(rng) * 120.0f; // silicates
                        break;
                    case terrain_type::rocky:
                        deposits[0] = dep_roll(rng) * 200.0f; // iron
                        deposits[2] = dep_roll(rng) * 100.0f; // silicates
                        deposits[3] = dep_roll(rng) *  50.0f; // rare metals
                        break;
                    case terrain_type::icy:
                        deposits[1] = dep_roll(rng) * 400.0f; // ice
                        deposits[2] = dep_roll(rng) *  30.0f; // silicates
                        break;
                    case terrain_type::volcanic:
                        deposits[0] = dep_roll(rng) * 250.0f; // iron
                        deposits[3] = dep_roll(rng) * 150.0f; // rare metals
                        break;
                    default: break;
                }
            }

            const entity_id tile_id = w.create_entity();
            w.tiles[tile_id] = tile_component{
                .body             = body_id,
                .grid_x           = col,
                .grid_y           = row,
                .terrain          = terrain,
                .resource_deposit = deposits,
                .hazard_level     = hazard,
                .habitability     = habitability,
            };
            tile_ids[idx] = tile_id;
        }
    }

    return tile_ids;
}

// Scan raster order and return the first `n` land (non-water) tile IDs.
std::vector<entity_id> first_land_tiles(const std::vector<entity_id>& tile_ids,
                                        const world& w, int gw, int gh, int n)
{
    std::vector<entity_id> result;
    result.reserve(n);
    for (int row = 0; row < gh && static_cast<int>(result.size()) < n; ++row)
        for (int col = 0; col < gw && static_cast<int>(result.size()) < n; ++col)
        {
            const entity_id id = tile_ids[col + row * gw];
            if (id != null_entity && w.tiles.at(id).terrain != terrain_type::water)
                result.push_back(id);
        }
    return result;
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
    // 180×84 tile grid. Mostly volcanic and barren; ~60% water.
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

    generate_body_tiles(w, cinder, 180, 84, 0.60f,
        std::vector<land_tier>{ {terrain_type::volcanic, 45}, {terrain_type::barren, 40}, {terrain_type::rocky, 15} },
        /*base_hazard=*/0.55f, /*base_habitability=*/0.20f, /*seed=*/0xC1D0001u);

    // -----------------------------------------------------------------------
    // Kepler — temperate rocky planet (Earth analogue, ~1.0 AU)
    // 180×84 tile grid. Primary deposits: iron ore and silicates.
    // Two installations and a market authored after tile generation.
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

    auto kepler_tiles = generate_body_tiles(w, kepler, 180, 84, 0.60f,
        std::vector<land_tier>{ {terrain_type::barren, 40}, {terrain_type::rocky, 35},
                                {terrain_type::icy, 15},    {terrain_type::volcanic, 10} },
        /*base_hazard=*/0.15f, /*base_habitability=*/0.70f, /*seed=*/0xE471001u);

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

    // Kepler market.
    const entity_id kepler_market = w.create_entity();
    w.markets[kepler_market] = market_component{
        .body       = kepler,
        .supply     = { 500.0f, 100.0f, 350.0f,  50.0f },
        .demand     = { 400.0f, 200.0f, 300.0f, 150.0f },
        .price      = {   2.5f,   4.0f,   3.0f,  12.0f },
        .base_price = {   2.5f,   4.0f,   3.0f,  12.0f },
    };

    // Player unit stub on Kepler.
    const entity_id kepler_unit = w.create_entity();
    w.units[kepler_unit] = unit_component{
        .body  = kepler,
        .owner = w.player_entity,
        .count = 50,
    };

    // -----------------------------------------------------------------------
    // Selene — Kepler's moon (Luna analogue)
    // 90×42 tile grid (the planet ratio at half scale). Barren and rocky;
    // ~60% water (frozen seas).
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

    generate_body_tiles(w, selene, 90, 42, 0.60f,
        std::vector<land_tier>{ {terrain_type::barren, 65}, {terrain_type::rocky, 30}, {terrain_type::icy, 5} },
        /*base_hazard=*/0.30f, /*base_habitability=*/0.25f, /*seed=*/0x5E1E001u);

    return w;
}
