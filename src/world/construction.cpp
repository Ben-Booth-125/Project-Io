#include "construction.hpp"

#include "logistics.hpp"

#include "market_clearing.hpp" // market_for_tile
#include "placement_rules.hpp"

#include <algorithm> // std::find (asset-list removal in demolish_building), std::max, std::clamp
#include <cmath>     // std::lround (BL-323 S3 site-time multiplier)

construction_result construct_building(world& w, const recipe_registry& reg,
                                       entity_id corp, entity_id tile,
                                       building_type type, resource_type target,
                                       entity_id& out_building,
                                       std::uint16_t recipe)
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

    // World-level checks: coastal (Port), body-count cap (Launchpad), and the
    // BL-323 S2 logistics-reach budget. The reach field is built HERE rather than
    // inside the check, because the check takes a const world and must not trigger
    // a Dijkstra; this is the authoritative gate and it holds a mutable world.
    body_reach_field(w, tile_it->second.body);
    const placement_rules::placement_result pr =
        placement_rules::can_place_in_world(w, tile, type, target,
                                            reg.construction().max_logistics_reach,
                                            corp); // BL-344: the tech gate asks WHO is building
    if (!pr)
    {
        if (pr.reason == placement_rules::placement_reason::out_of_logistics_range)
            return construction_result::out_of_range;
        if (pr.reason == placement_rules::placement_reason::tech_locked)
            return construction_result::tech_locked;
        if (type == building_type::launchpad)
            return construction_result::slot_occupied;
        return construction_result::invalid_tile; // Port not coastal
    }

    corporation_component& cc = corp_it->second;
    const building_economics& econ = reg.economics(type);

    // Material cost (BL-044 → BL-095): a building's resource_build_cost is bought
    // from the local market — but under BL-095 it is no longer a single up-front
    // debit. Placement gates on affordability (you must be able to afford the whole
    // build to commit to it), then construction is *paid as it is built*: each
    // economy tick the build draws 1/build_duration_ticks of its materials as real
    // market demand and pays the resolved price, and the flat build_cost accrues at
    // the same pace (economy_system.cpp § run_construction). The rate is gated on
    // the market's recent supply of those materials, so a starved build stretches
    // or pauses rather than completing instantly. The affordability figure below is
    // priced at the *current* market price for the commitment check; the actual
    // spend happens tick by tick at the price prevailing then.
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

    // Affordability is a commitment gate only (BL-095): the corp must be able to
    // afford the whole build to start it, but is not debited here — payment is
    // spread across construction by run_construction (pay-as-you-build).
    const float total_cost = econ.build_cost + material_cost;
    if (cc.balance < total_cost)
        return construction_result::insufficient_funds;

    // Create and author the building, mirroring corporation_generation.cpp Pass 3.
    const entity_id bld_id = w.create_entity();

    building_component bc;
    bc.tile               = tile;
    bc.type               = type;
    // Staff producing buildings so they run; ports, logistics hubs and military
    // bases take no L3 production action (passive infrastructure), so they staff
    // at zero.
    bc.workforce_assigned =
        (type == building_type::port || type == building_type::inland_logistics_hub
         || type == building_type::military_base
         || type == building_type::research_institute) ? 0.0f : 0.5f; // BL-332: passive, like military_base
    // Build-time pacing (playtest patch, 2026-07-06): the building sits idle
    // for build_duration_ticks economy ticks before economy_system lets it produce.
    //
    // BL-323 S3: that duration now depends on WHERE, not just WHAT. Three
    // multipliers, each 1.0 at the cheapest case so an anchor-adjacent plains
    // first-of-its-kind build reproduces the old flat behaviour exactly:
    //   landform  — reuse logistics.hpp's own per-landform cost (plains 1.0 ..
    //               mountain 2.0), the same number a convoy pays to cross it.
    //   reach     — linear in the tile's distance from its nearest supply
    //               anchor, 1.0 at the anchor up to (1 + site_time_reach_scale)
    //               at the max_logistics_reach budget edge. Skipped (1.0) when
    //               the reach rule is disabled or the field/tile is unreachable
    //               — there is then no scale to normalise against.
    //   stack     — an established site (a tile that already carries a building
    //               of the SAME type) builds faster: one discount step per prior
    //               building, floored at site_time_stack_min.
    const construction_params& cparams = reg.construction();
    const float landform_factor = landform_logistics_cost(tile_it->second.landform);
    float reach_factor = 1.0f;
    if (cparams.max_logistics_reach > 0.0f)
    {
        const float reach = tile_reach_cost(w, tile);
        if (reach >= 0.0f)
        {
            const float frac = std::clamp(reach / cparams.max_logistics_reach, 0.0f, 1.0f);
            reach_factor = 1.0f + frac * cparams.site_time_reach_scale;
        }
    }
    const int existing = placement_rules::buildings_on_tile(w, tile, type, target);
    const float stack_factor = std::max(cparams.site_time_stack_min,
        1.0f - static_cast<float>(existing) * cparams.site_time_stack_discount);
    const float site_multiplier = landform_factor * reach_factor * stack_factor;
    // 0 stays instant (some infrastructure types are undurationed by design);
    // any real duration is floored at 1 tick so the multiplier can never zero
    // out a build that was supposed to take time.
    bc.ticks_remaining = econ.build_duration_ticks <= 0.0f ? 0
        : std::max(1, static_cast<int>(std::lround(econ.build_duration_ticks * site_multiplier)));

    if (type == building_type::extraction_site)
    {
        bc.target_resource = target;
    }
    else if (type == building_type::processing_facility)
    {
        // Seed the caller's recipe when one was chosen (BL-162: the construction
        // ledger prices one row per recipe, so the built processor must run the recipe
        // the player's bar described). Otherwise the historic default, so a freshly
        // built processor is still productive; the player reconfigures it via building
        // management. Mirrors app::load_economy.
        bc.recipe = (recipe != no_recipe) ? recipe : reg.recipe_id("steel");
    }

    w.buildings[bld_id]  = bc;
    w.stockpiles[bld_id] = stockpile_component{};

    cc.assets.push_back(bld_id);
    // BL-095: no up-front debit — run_construction charges build_cost + materials
    // incrementally as the build progresses (pay-as-you-build).

    // Before this clear existed, a newly placed port/hub extended the reach
    // field only when something unrelated (a road) happened to clear the cache —
    // the rule took effect at an arbitrary later moment. The anchor still waits
    // on completion (is_supply_anchor's ticks_remaining contract); the clear
    // just guarantees the field is honest whenever it is next read.
    invalidate_logistics_caches(w);

    out_building = bld_id;
    return construction_result::placed;
}

bool demolish_building(world& w, entity_id corp, entity_id building)
{
    const auto bld_it = w.buildings.find(building);
    if (bld_it == w.buildings.end())
        return false;

    const auto corp_it = w.corporations.find(corp);
    if (corp_it == w.corporations.end())
        return false;

    // Ownership is the corp's `assets` list (world.hpp § Ownership accessors —
    // there is no reverse index). Not finding the id there means this corp does
    // not own the building, which is also the refusal condition.
    corporation_component& cc = corp_it->second;
    const auto asset_it = std::find(cc.assets.begin(), cc.assets.end(), building);
    if (asset_it == cc.assets.end())
        return false;

    cc.assets.erase(asset_it);
    w.buildings.erase(bld_it);
    w.stockpiles.erase(building); // no-op if the building never had one

    // A demolished port/hub stops anchoring supply; before this clear the stale
    // reach field kept offering tiles anchored on the ghost.
    invalidate_logistics_caches(w);

    return true;
}

construction_result place_road(world& w, const recipe_registry& reg,
                               entity_id corp, entity_id tile, std::uint8_t tier)
{
    const auto corp_it = w.corporations.find(corp);
    if (corp_it == w.corporations.end())
        return construction_result::no_corp;

    const auto tile_it = w.tiles.find(tile);
    if (tile_it == w.tiles.end())
        return construction_result::no_tile;

    // Validity: non-ocean land, and this tier strictly upgrades the tile (BL-172).
    if (!placement_rules::can_place_road(tile_it->second, tier))
        return construction_result::invalid_tile;

    corporation_component& cc = corp_it->second;
    const road_economics&  re = reg.road_econ(tier);

    // Material cost priced from the tile's local market, mirroring construct_building — but
    // debited up front in one shot (a road is instant, not a durative pay-as-you-build).
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
        if (re.resource_build_cost[r] <= 0.0f)
            continue;
        const float p = mkt ? (mkt->price[r] > 0.0f ? mkt->price[r] : mkt->base_price[r]) : 0.0f;
        material_cost += re.resource_build_cost[r] * p;
    }

    const float total_cost = re.build_cost + material_cost;
    if (cc.balance < total_cost)
        return construction_result::insufficient_funds;

    // Commit: debit, raise the tile to the chosen road tier, invalidate the
    // logistics caches (road_level changed, so cached path and reach costs are
    // stale — world.hpp invalidation contract).
    cc.balance -= total_cost;
    tile_it->second.road_level = tier; // BL-172: 1=Track, 2=Road, 3=Highway (upgrade-in-place).
    invalidate_logistics_caches(w);

    return construction_result::placed;
}
