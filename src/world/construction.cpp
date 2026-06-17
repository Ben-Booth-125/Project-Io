#include "construction.hpp"

#include "placement_rules.hpp"

construction_result construct_building(world& w, const recipe_registry& reg,
                                       entity_id corp, entity_id tile,
                                       building_type type, resource_type target,
                                       entity_id& out_building)
{
    out_building = null_entity;

    const auto corp_it = w.corporations.find(corp);
    if (corp_it == w.corporations.end())
        return construction_result::no_corp;

    const auto tile_it = w.tiles.find(tile);
    if (tile_it == w.tiles.end())
        return construction_result::no_tile;

    // Tile-level validity check (ocean / deposit / terrain).
    if (!placement_rules::can_place(tile_it->second, type, target))
        return construction_result::invalid_tile;

    // World-level checks: coastal (Port), body-count cap (Launchpad).
    // can_place passed, so failures here are world-state reasons, not terrain.
    if (!placement_rules::can_place_in_world(w, tile, type, target))
    {
        if (type == building_type::launchpad)
            return construction_result::slot_occupied;
        return construction_result::invalid_tile; // Port not coastal
    }

    corporation_component& cc = corp_it->second;
    const building_economics& econ = reg.economics(type);
    if (cc.balance < econ.build_cost)
        return construction_result::insufficient_funds;

    // Resource material cost (BL-044): check corp pool on the building's body.
    const entity_id tile_body = tile_it->second.body;
    stockpile_component& pool = w.pool_for(corp, tile_body);
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (econ.resource_build_cost[r] > 0.0f &&
            pool.quantities[r] < econ.resource_build_cost[r])
            return construction_result::insufficient_materials;
    }

    // Create and author the building, mirroring corporation_generation.cpp Pass 3.
    const entity_id bld_id = w.create_entity();

    building_component bc;
    bc.tile               = tile;
    bc.type               = type;
    // Staff producing buildings so they run; ports take no L3 production action.
    bc.workforce_assigned = (type == building_type::port) ? 0.0f : 0.5f;

    if (type == building_type::extraction_site)
    {
        bc.target_resource = target;
    }
    else if (type == building_type::processing_facility)
    {
        // Seed the default recipe so a freshly built processor is productive; the
        // player reconfigures it via building management. Mirrors app::load_economy.
        bc.recipe = reg.recipe_id("steel");
    }

    w.buildings[bld_id]  = bc;
    w.stockpiles[bld_id] = stockpile_component{};

    cc.assets.push_back(bld_id);
    cc.balance -= econ.build_cost;
    // Deduct material costs from corp pool (BL-044).
    for (std::size_t r = 0; r < resource_count; ++r)
        pool.quantities[r] -= econ.resource_build_cost[r];

    out_building = bld_id;
    return construction_result::placed;
}
