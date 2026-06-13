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

entity_id create_simple_body(world& w, const char* name, body_type type,
                             entity_id parent, float radius_au, float angle_rad,
                             int grid_w, int grid_h, float angular_velocity = -1.0f)
{
    const float omega = (angular_velocity >= 0.0f)
                      ? angular_velocity
                      : kepler_angular_velocity(radius_au);

    const entity_id id = w.create_entity();
    w.bodies[id] = body_component{
        .name                                 = name,
        .type                                 = type,
        .parent                               = parent,
        .orbital_radius_au                    = radius_au,
        .orbital_angle_rad                    = angle_rad,
        .orbital_angular_velocity_rad_per_day = omega,
        .grid_width                           = grid_w,
        .grid_height                          = grid_h,
    };
    return id;
}

// Kept for Vesta, which still uses hand-authored tiles.
struct tile_spec
{
    int          grid_x;
    int          grid_y;
    terrain_type terrain;
    float        hazard;
    float        habitability;
    float        iron_ore;
    float        ice;
    float        silicates;
    float        rare_metals;
};

entity_id create_tile(world& w, entity_id body_id, const tile_spec& s)
{
    const entity_id id = w.create_entity();
    w.tiles[id] = tile_component{
        .body             = body_id,
        .grid_x           = s.grid_x,
        .grid_y           = s.grid_y,
        .terrain          = s.terrain,
        .resource_deposit = {s.iron_ore, s.ice, s.silicates, s.rare_metals},
        .hazard_level     = s.hazard,
        .habitability     = s.habitability,
    };
    return id;
}

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
    // TODO (generation): Body grid dimensions should follow the rule that a body
    // is approximately twice as wide as tall (measuring both hemispheres), with a
    // slight row truncation at each pole (top and bottom ~5-10% of rows) that are
    // not navigable terrain. Zoom constraints for the planetary canvas:
    //   - Minimum zoom: ~12 tiles visible across the viewport width.
    //   - Maximum zoom: ~12 tiles of headroom beyond the total grid height.
    // These rules should be enforced when procedural generation is introduced.
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

    // -----------------------------------------------------------------------
    // Cinder — hot inner planet (Mercury analogue, 0.39 AU)
    // 36×186 tile grid. Mostly volcanic and barren; ~60% water.
    // -----------------------------------------------------------------------

    const entity_id cinder = w.create_entity();
    w.bodies[cinder] = body_component{
        .name                                 = "Cinder",
        .type                                 = body_type::planet,
        .parent                               = null_entity,
        .orbital_radius_au                    = 0.39f,
        .orbital_angle_rad                    = 0.40f,
        .orbital_angular_velocity_rad_per_day = kepler_angular_velocity(0.39f),
        .grid_width                           = 186,
        .grid_height                          = 36,
    };

    generate_body_tiles(w, cinder, 186, 36, 0.60f,
        std::vector<land_tier>{ {terrain_type::volcanic, 45}, {terrain_type::barren, 40}, {terrain_type::rocky, 15} },
        /*base_hazard=*/0.55f, /*base_habitability=*/0.20f, /*seed=*/0xC1D0001u);

    // Venus analogue — backdrop body.
    create_simple_body(w, "Veld", body_type::planet, null_entity, 0.72f, 2.60f, 3, 3);

    // -----------------------------------------------------------------------
    // Kepler — temperate rocky planet (Earth analogue, ~1.0 AU)
    // 42×174 tile grid. Primary deposits: iron ore and silicates.
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
        .grid_width                           = 174,
        .grid_height                          = 42,
    };

    auto kepler_tiles = generate_body_tiles(w, kepler, 174, 42, 0.60f,
        std::vector<land_tier>{ {terrain_type::barren, 40}, {terrain_type::rocky, 35},
                                {terrain_type::icy, 15},    {terrain_type::volcanic, 10} },
        /*base_hazard=*/0.15f, /*base_habitability=*/0.70f, /*seed=*/0xE471001u);

    // Attach installations to the first two land tiles found in raster order.
    {
        auto land = first_land_tiles(kepler_tiles, w, 174, 42, 2);

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
    // 18×92 tile grid. Barren and rocky; ~60% water (frozen seas).
    // -----------------------------------------------------------------------

    const entity_id selene = w.create_entity();
    w.bodies[selene] = body_component{
        .name                                 = "Selene",
        .type                                 = body_type::moon,
        .parent                               = kepler,
        .orbital_radius_au                    = 0.30f,
        .orbital_angle_rad                    = 0.0f,
        .orbital_angular_velocity_rad_per_day = 0.30f,
        .grid_width                           = 92,
        .grid_height                          = 18,
    };

    generate_body_tiles(w, selene, 92, 18, 0.60f,
        std::vector<land_tier>{ {terrain_type::barren, 65}, {terrain_type::rocky, 30}, {terrain_type::icy, 5} },
        /*base_hazard=*/0.30f, /*base_habitability=*/0.25f, /*seed=*/0x5E1E001u);

    // Mars analogue — backdrop body.
    create_simple_body(w, "Ochre", body_type::planet, null_entity, 1.52f, 4.70f, 3, 3);

    // -----------------------------------------------------------------------
    // Vesta — important belt asteroid (~2.36 AU)
    // 3×3 tile grid. Primary deposits: ice and rare metals.
    // Hand-authored tiles retained — Vesta is small enough to curate manually.
    // -----------------------------------------------------------------------

    const entity_id vesta = w.create_entity();
    w.bodies[vesta] = body_component{
        .name                                 = "Vesta",
        .type                                 = body_type::asteroid,
        .parent                               = null_entity,
        .orbital_radius_au                    = 2.36f,
        .orbital_angle_rad                    = 3.93f,
        .orbital_angular_velocity_rad_per_day = kepler_angular_velocity(2.36f),
        .grid_width                           = 3,
        .grid_height                          = 3,
    };

    const tile_spec vesta_tiles[] = {
        // x  y   terrain                     haz    hab     Fe      ice    SiO2  rare
        {  0, 0, terrain_type::icy,           0.30f, 0.25f,   0.0f, 300.0f, 20.0f, 15.0f },
        {  1, 0, terrain_type::icy,           0.40f, 0.20f,   5.0f, 450.0f, 10.0f, 20.0f },
        {  2, 0, terrain_type::rocky,         0.50f, 0.15f,  60.0f,  80.0f, 25.0f, 45.0f },
        {  0, 1, terrain_type::icy,           0.35f, 0.22f,   0.0f, 380.0f, 15.0f, 10.0f },
        {  1, 1, terrain_type::icy,           0.25f, 0.28f,  10.0f, 500.0f, 30.0f, 35.0f },
        {  2, 1, terrain_type::rocky,         0.55f, 0.12f,  75.0f,  60.0f, 20.0f, 55.0f },
        {  0, 2, terrain_type::rocky,         0.45f, 0.18f,  50.0f, 120.0f, 15.0f, 40.0f },
        {  1, 2, terrain_type::icy,           0.38f, 0.20f,   0.0f, 420.0f, 10.0f, 25.0f },
        {  2, 2, terrain_type::rocky,         0.60f, 0.10f,  80.0f,  50.0f, 18.0f, 70.0f },
    };

    std::vector<entity_id> vesta_tile_ids;
    vesta_tile_ids.reserve(9);
    for (const auto& spec : vesta_tiles)
        vesta_tile_ids.push_back(create_tile(w, vesta, spec));

    const entity_id vesta_extraction = w.create_entity();
    w.buildings[vesta_extraction] = building_component{
        .tile               = vesta_tile_ids[4], // [1,1]
        .type               = building_type::extraction_site,
        .workforce_assigned = 0.60f,
    };
    w.stockpiles[vesta_extraction] = stockpile_component{};

    const entity_id vesta_market = w.create_entity();
    w.markets[vesta_market] = market_component{
        .body       = vesta,
        .supply     = {  20.0f, 800.0f, 30.0f, 180.0f },
        .demand     = { 150.0f, 100.0f, 80.0f,  50.0f },
        .price      = {   2.5f,   4.0f,  3.0f,  12.0f },
        .base_price = {   2.5f,   4.0f,  3.0f,  12.0f },
    };

    create_simple_body(w, "Ceres",  body_type::asteroid, null_entity, 2.77f, 1.80f, 3, 3);
    create_simple_body(w, "Pallas", body_type::asteroid, null_entity, 2.55f, 5.50f, 2, 2);

    // -----------------------------------------------------------------------
    // Outer planets with parented moons — backdrop bodies.
    // -----------------------------------------------------------------------

    const entity_id bastion = create_simple_body(w, "Bastion", body_type::planet, null_entity, 5.20f, 0.70f, 4, 4);
    create_simple_body(w, "Forge", body_type::moon, bastion, 0.35f, 1.00f, 2, 2, 0.40f);
    create_simple_body(w, "Cyra",  body_type::moon, bastion, 0.55f, 3.50f, 2, 2, 0.25f);

    const entity_id halo = create_simple_body(w, "Halo", body_type::planet, null_entity, 9.58f, 3.50f, 4, 4);
    create_simple_body(w, "Mote", body_type::moon, halo, 0.40f, 2.00f, 2, 2, 0.35f);

    return w;
}
