#include "hard_coded_world.hpp"

#include <vector>

namespace {

// Local spec used only during world construction. Not part of the runtime
// data model — tile_component is what the simulation reads.
struct tile_spec
{
    int          grid_x;
    int          grid_y;
    terrain_type terrain;
    float        hazard;
    float        habitability;
    // resource_deposit order: iron_ore, ice, silicates, rare_metals
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

} // namespace

world make_hard_coded_world()
{
    world w;

    // Player corporation entity. Holds no component in Layer 1; the budget
    // layer will attach a corporation_component when revenue attribution is needed.
    w.player_entity = w.create_entity();

    // -----------------------------------------------------------------------
    // Kepler — temperate rocky planet
    // 4×4 tile grid, 1.0 AU from star.
    // Primary deposits: iron ore and silicates.
    // Two volcanic tiles carry elevated rare-metal readings.
    // -----------------------------------------------------------------------

    const entity_id kepler = w.create_entity();
    w.bodies[kepler] = body_component{
        .name              = "Kepler",
        .type              = body_type::planet,
        .orbital_radius_au = 1.00f,
        .grid_width        = 4,
        .grid_height       = 4,
    };

    // Each row covers one band of terrain — northern lowlands, industrial
    // belt, highland ridge, southern plains.
    const tile_spec kepler_tiles[] = {
        // x  y   terrain                     haz    hab     Fe      ice   SiO2   rare
        {  0, 0, terrain_type::barren,        0.10f, 0.70f, 120.0f,  5.0f,  80.0f, 10.0f },
        {  1, 0, terrain_type::rocky,         0.25f, 0.50f, 200.0f,  0.0f,  60.0f, 25.0f },
        {  2, 0, terrain_type::rocky,         0.30f, 0.45f, 175.0f,  0.0f,  70.0f, 30.0f },
        {  3, 0, terrain_type::barren,        0.15f, 0.65f,  90.0f, 10.0f,  95.0f, 15.0f },
        {  0, 1, terrain_type::barren,        0.05f, 0.80f, 110.0f, 20.0f,  85.0f,  5.0f },
        {  1, 1, terrain_type::rocky,         0.20f, 0.55f, 160.0f,  0.0f,  50.0f, 20.0f },
        {  2, 1, terrain_type::volcanic,      0.65f, 0.20f, 250.0f,  0.0f,  30.0f, 90.0f },
        {  3, 1, terrain_type::barren,        0.10f, 0.75f, 100.0f, 15.0f,  90.0f,  8.0f },
        {  0, 2, terrain_type::rocky,         0.35f, 0.40f, 180.0f,  0.0f,  55.0f, 35.0f },
        {  1, 2, terrain_type::barren,        0.08f, 0.78f, 115.0f, 10.0f,  88.0f, 12.0f },
        {  2, 2, terrain_type::barren,        0.12f, 0.72f, 105.0f, 25.0f,  92.0f,  7.0f },
        {  3, 2, terrain_type::volcanic,      0.70f, 0.15f, 280.0f,  0.0f,  20.0f,110.0f },
        {  0, 3, terrain_type::rocky,         0.40f, 0.35f, 195.0f,  0.0f,  45.0f, 40.0f },
        {  1, 3, terrain_type::barren,        0.18f, 0.60f, 130.0f,  8.0f,  75.0f, 18.0f },
        {  2, 3, terrain_type::rocky,         0.28f, 0.48f, 165.0f,  0.0f,  65.0f, 28.0f },
        {  3, 3, terrain_type::barren,        0.08f, 0.82f, 125.0f, 12.0f,  88.0f,  9.0f },
    };

    std::vector<entity_id> kepler_tile_ids;
    kepler_tile_ids.reserve(16);
    for (const auto& spec : kepler_tiles)
        kepler_tile_ids.push_back(create_tile(w, kepler, spec));

    // Two installations on Kepler.
    // Extraction site at [1,0] — rocky tile with high iron-ore deposit.
    const entity_id kepler_extraction = w.create_entity();
    w.buildings[kepler_extraction] = building_component{
        .tile               = kepler_tile_ids[1],
        .type               = building_type::extraction_site,
        .workforce_assigned = 0.75f,
    };
    w.stockpiles[kepler_extraction] = stockpile_component{};

    // Processing facility at [0,0] — adjacent barren tile, easy build cost.
    const entity_id kepler_processor = w.create_entity();
    w.buildings[kepler_processor] = building_component{
        .tile               = kepler_tile_ids[0],
        .type               = building_type::processing_facility,
        .workforce_assigned = 0.50f,
    };
    w.stockpiles[kepler_processor] = stockpile_component{};

    // Market for Kepler.
    // Iron ore and silicates in good supply; ice undersupplied relative to demand.
    const entity_id kepler_market = w.create_entity();
    w.markets[kepler_market] = market_component{
        .body       = kepler,
        .supply     = { 500.0f, 100.0f, 350.0f,  50.0f },
        .demand     = { 400.0f, 200.0f, 300.0f, 150.0f },
        .price      = {   2.5f,   4.0f,   3.0f,  12.0f }, // seeded to base until first tick
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
    // Vesta — icy outer moon
    // 3×3 tile grid, 5.2 AU from star.
    // Primary deposits: ice and rare metals.
    // Hazard levels are higher throughout; habitability is low.
    // -----------------------------------------------------------------------

    const entity_id vesta = w.create_entity();
    w.bodies[vesta] = body_component{
        .name              = "Vesta",
        .type              = body_type::moon,
        .orbital_radius_au = 5.20f,
        .grid_width        = 3,
        .grid_height       = 3,
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

    // Extraction site on Vesta at [1,1] — highest ice concentration.
    const entity_id vesta_extraction = w.create_entity();
    w.buildings[vesta_extraction] = building_component{
        .tile               = vesta_tile_ids[4], // [1,1]
        .type               = building_type::extraction_site,
        .workforce_assigned = 0.60f,
    };
    w.stockpiles[vesta_extraction] = stockpile_component{};

    // Market for Vesta.
    // Ice in heavy supply; iron ore and silicates are scarce at this distance.
    const entity_id vesta_market = w.create_entity();
    w.markets[vesta_market] = market_component{
        .body       = vesta,
        .supply     = {  20.0f, 800.0f, 30.0f, 180.0f },
        .demand     = { 150.0f, 100.0f, 80.0f,  50.0f },
        .price      = {   2.5f,   4.0f,  3.0f,  12.0f },
        .base_price = {   2.5f,   4.0f,  3.0f,  12.0f },
    };

    return w;
}
