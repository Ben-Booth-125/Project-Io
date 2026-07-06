#include "construction.hpp"

#include "market_clearing.hpp" // market_for_tile
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

    // Material cost (BL-044/BL-095-lite): a building's resource_build_cost is
    // displayed as materials but bought from the local market at its prevailing
    // price, folded into the credit cost — "everything costs money", the
    // resource line is what that money buys. This sidesteps the bootstrapping
    // deadlock a corp-pool-only gate produced (a fresh corp's own pool starts
    // empty and its refined surplus auto-sells each tick, so no building was
    // ever placeable) without yet modelling a depletable market stockpile
    // (that gate/charge distinction is the open question left to BL-095).
    const market_component* mkt = nullptr;
    {
        const entity_id mid = market_for_tile(w, tile);
        if (mid != null_entity)
        {
            const auto mit = w.markets.find(mid);
            if (mit != w.markets.end())
                mkt = &mit->second;
        }
    }
    float material_cost = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (econ.resource_build_cost[r] <= 0.0f)
            continue;
        const float p = mkt ? (mkt->price[r] > 0.0f ? mkt->price[r] : mkt->base_price[r]) : 0.0f;
        material_cost += econ.resource_build_cost[r] * p;
    }

    const float total_cost = econ.build_cost + material_cost;
    if (cc.balance < total_cost)
        return construction_result::insufficient_funds;

    // Create and author the building, mirroring corporation_generation.cpp Pass 3.
    const entity_id bld_id = w.create_entity();

    building_component bc;
    bc.tile               = tile;
    bc.type               = type;
    // Staff producing buildings so they run; ports take no L3 production action.
    bc.workforce_assigned = (type == building_type::port) ? 0.0f : 0.5f;
    // Build-time pacing (playtest patch, 2026-07-06): the building sits idle
    // for build_duration_ticks economy ticks before economy_system lets it produce.
    bc.ticks_remaining = static_cast<int>(econ.build_duration_ticks);

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
    cc.balance -= total_cost;

    out_building = bld_id;
    return construction_result::placed;
}
