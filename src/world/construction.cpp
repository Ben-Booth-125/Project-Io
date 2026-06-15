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

    // The load-bearing validity check — the same seam generation uses.
    if (!placement_rules::can_place(tile_it->second, type, target))
        return construction_result::invalid_tile;

    corporation_component& cc = corp_it->second;
    const float cost = reg.economics(type).build_cost;
    if (cc.balance < cost)
        return construction_result::insufficient_funds;

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
    cc.balance -= cost;

    out_building = bld_id;
    return construction_result::placed;
}
