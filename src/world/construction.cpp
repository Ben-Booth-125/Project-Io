#include "construction.hpp"

#include "logistics.hpp"

#include "economy_system.hpp"  // begin_construction_on_placement (Ben, 2026-10-25)
#include "market_clearing.hpp" // market_for_tile
#include "placement_rules.hpp"
#include "road_generation.hpp" // lay_port_lanes: an instant port lays its lanes (Ben, 2026-10-25)
#include "nation_tech.hpp"     // recipe_unlocked (BL-1273: the nation's tree; BL-588 before it)
#include "recipe_registry.hpp" // recipe_runs_at_tile (defined at the foot of this file)
#include "state_industry.hpp"  // BL-1296: state_runs, state_uncommitted_treasury
#include "world.hpp"

#include <algorithm> // std::find (asset-list removal in demolish_building), std::max, std::clamp
#include <cmath>     // std::lround (BL-323 S3 site-time multiplier)
#include <limits>    // BL-1296: the founding skips the commitment gate

float construction_site_multiplier(const world& w, const recipe_registry& reg,
                                   entity_id tile, building_type type, resource_type target)
{
    const auto tile_it = w.tiles.find(tile);
    if (tile_it == w.tiles.end())
        return 1.0f;
    // BL-323 S3: duration depends on WHERE, not just WHAT. Three multipliers,
    // each 1.0 at the cheapest case, so an anchor-adjacent plains
    // first-of-its-kind build reproduces the flat authored figure exactly:
    //   landform  — logistics.hpp's own per-landform cost (plains 1.0 ..
    //               mountain 2.0), the same number a convoy pays to cross it.
    //   reach     — linear in the tile's distance from its nearest supply
    //               anchor, 1.0 at the anchor up to (1 + site_time_reach_scale)
    //               at the max_logistics_reach budget edge. 1.0 when the reach
    //               rule is off or the field is unbuilt — no scale to normalise by.
    //   stack     — an established site (a tile already carrying a building of
    //               the SAME type) builds faster: one discount step per prior
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
    return landform_factor * reach_factor * stack_factor;
}

float construction_capex(const world& w, const recipe_registry& reg, entity_id tile,
                         building_type type, resource_type target, std::uint16_t recipe)
{
    // Materials priced at the tile market's CURRENT price (base price where none has
    // resolved). The draw itself happens tick by tick at the price prevailing then
    // (economy_system.cpp § run_construction); this is the commitment figure.
    const market_component* mkt = nullptr;
    if (const entity_id mid = market_for_tile(w, tile); mid != null_entity)
        if (const auto mit = w.markets.find(mid); mit != w.markets.end())
            mkt = &mit->second;
    // BL-590: the material cost specific to THIS named building (target/recipe).
    const auto& material_cost_row = reg.resource_build_cost_for(type, target, recipe);
    float material_cost = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (material_cost_row[r] <= 0.0f)
            continue;
        const float p = mkt ? (mkt->price[r] > 0.0f ? mkt->price[r] : mkt->base_price[r]) : 0.0f;
        material_cost += material_cost_row[r] * p;
    }
    const float authored = reg.economics(type).build_cost + material_cost;
    // The draw is the authored cost over the AUTHORED duration per full-rate tick,
    // for as many ticks as the site actually takes — so the ratio is the ROUNDED
    // tick count over the authored duration, not the raw multiplier (a 4-tick build
    // at x1.2 takes 5 ticks and draws 1.25x). An instant build draws nothing per tick.
    const float duration = reg.economics(type).build_duration_ticks;
    if (duration <= 0.0f)
        return authored;
    // Ben, 2026-10-24: the site's DAYS in quarters, unrounded — a last quarter
    // of fewer than 90 days draws only its days (run_construction's need row).
    const float ticks = static_cast<float>(construction_build_days(w, reg, tile, type, target))
                      / static_cast<float>(k_construction_quarter_days);
    // BL-709 construction capacity: drawn flat per build tick, not per unit of
    // material (run_construction's need row), so it scales with the ticks alone.
    float capacity_cost = 0.0f;
    if (const float cap_rate = reg.construction().capacity_per_build_tick; cap_rate > 0.0f && mkt)
    {
        const std::size_t ci = static_cast<std::size_t>(resource_type::construction_capacity);
        capacity_cost = cap_rate * ticks * (mkt->price[ci] > 0.0f ? mkt->price[ci] : mkt->base_price[ci]);
    }
    return authored * ticks / duration + capacity_cost;
}

int construction_build_days(const world& w, const recipe_registry& reg,
                            entity_id tile, building_type type, resource_type target)
{
    // Construction is measured in DAYS (Ben, 2026-10-24; PRODUCTION.md § Construction
    // is measured in days): the authored quarters x 90 days, times the site
    // multiplier, rounded to a whole day. 0 stays instant (some infrastructure
    // types are undurationed by design); any real duration is floored at 1 day so
    // the multiplier can never zero out a build that was supposed to take time.
    const float duration = reg.economics(type).build_duration_ticks;
    if (duration <= 0.0f)
        return 0;
    return std::max(1, static_cast<int>(std::lround(
        duration * static_cast<float>(k_construction_quarter_days)
        * construction_site_multiplier(w, reg, tile, type, target))));
}

int construction_build_ticks(const world& w, const recipe_registry& reg,
                             entity_id tile, building_type type, resource_type target)
{
    // The quarters those days span (Ben, 2026-10-24) — `ticks_remaining`'s gate.
    const int days = construction_build_days(w, reg, tile, type, target);
    return (days + k_construction_quarter_days - 1) / k_construction_quarter_days;
}

float construction_initial_workforce(building_type type)
{
    return (type == building_type::port || type == building_type::inland_logistics_hub
            || type == building_type::military_base
            || type == building_type::railway_station) ? 0.0f : 0.5f; // BL-1255: zero staff
}

namespace {

/// The one build door, for both owner kinds (BL-1296). @p corp is WHO the tech
/// and placement gates ask about (the building corporation; for a state build,
/// the nation's champion — the same home nation, so the same tree). @p funds is
/// what the builder may commit (a corporation's balance; a treasury's
/// uncommitted part). @p assets is the owner's asset list the new building
/// joins. Every check runs before anything mutates.
construction_result construct_for(world& w, const recipe_registry& reg,
                                  entity_id corp, entity_id tile,
                                  building_type type, resource_type target,
                                  entity_id& out_building, std::uint16_t recipe,
                                  float funds, std::vector<entity_id>& assets)
{
    out_building = null_entity;

    const auto tile_it = w.tiles.find(tile);
    if (tile_it == w.tiles.end())
        return construction_result::no_tile;

    // BL-433 era gate. First, and ahead of every tile check, for the same reason
    // BL-344's tech gate sits ahead of the terrain ones: a type outside the
    // campaign's era is unavailable EVERYWHERE, so "this does not exist in this
    // era" is a truer refusal than "that tile is not coastal". The registry's
    // band is `any` unless a campaign set one, so no harness is affected.
    if (!reg.building_available(type))
        return construction_result::era_locked;

    // BL-692 (2026-08-29): the BL-428 chain-depth gate that stood here is GONE.
    // Ben's ruling: "most direct unlocks should be a consequence of research, not
    // just economy... allow both corporations and companies to build any building,
    // or use any method surfaced with current tech." Tech is now the only lock on
    // a method — depth no longer refuses anything at any door. The METRIC survives
    // (recipe_required_depth / corp_reached_depth / depth_of), because chain_depth's
    // R2 uses it as a dominance axis and produced_ever is pinned by the save format;
    // it simply no longer gates. See docs/economy/PRODUCTION.md § Chain depth.

    // BL-588 tech-recipe gate — now the ONLY recipe-level lock at this door
    // (BL-692 removed the depth one that used to sit above it). Reuses
    // `construction_result::tech_locked`, the same value the structure-level
    // gate already returns, so an agent reading the seam cannot tell a structure
    // lock from a recipe lock apart — which is the point
    // (corp_command_result::rejected_tech_locked mirrors this one level up).
    // `corp` is guaranteed non-null here (the no_corp check above already
    // returned), so this is never the ungated no-corp case.
    if (!recipe_unlocked(w, reg, corp, recipe))
        return construction_result::tech_locked;

    // Propellant routes follow the body's air (Ben, 2026-10-09; PRODUCTION.md
    // § Chemical Plant): the one predicate, asked at the build door.
    if (type == building_type::processing_facility)
        if (const ::recipe* rc = reg.get_recipe(recipe); rc != nullptr && !recipe_runs_at_tile(w, *rc, tile))
            return construction_result::wrong_air;

    // Tile-level validity check (ocean / deposit / terrain).
    if (!placement_rules::can_place(tile_it->second, type, target))
        return construction_result::invalid_tile;

    // BL-1255 (LOGISTICS.md § 4c; PRODUCTION.md § Infrastructure buildings): a
    // railway station stands on a CITY tile TOUCHING a rail line, one to a tile.
    // Asked here, at the one build door, so no path can stand a station off the
    // line — whatever the registry's placement gate says.
    if (type == building_type::railway_station)
    {
        bool city = false;
        for (const auto& [centre, ctile] : w.population_centre_tile)
        {
            (void)centre;
            if (ctile == tile) { city = true; break; }
        }
        if (!city || !tile_touches_track(w, tile))
            return construction_result::invalid_tile;
        for (const auto& [bid, b] : w.buildings)
        {
            (void)bid;
            if (b.type == building_type::railway_station && b.tile == tile)
                return construction_result::invalid_tile;
        }
    }

    // World-level checks: coastal (Port), body-count cap (Launchpad), and the
    // BL-323 S2 logistics-reach budget. The reach field is built HERE rather than
    // inside the check, because the check takes a const world and must not trigger
    // a Dijkstra; this is the authoritative gate and it holds a mutable world.
    body_reach_field(w, tile_it->second.body);
    const placement_rules::placement_result pr =
        placement_rules::can_place_in_world(w, tile, type, target,
                                            reg.construction().max_logistics_reach,
                                            corp, // BL-344: the tech gate asks WHO is building
                                            // BL-615: the stratum gate asks WHICH named
                                            // building — the type's authored gate, with a
                                            // processing recipe's own radius override.
                                            reg.placement_gate_for(type, recipe));
    if (!pr)
    {
        if (pr.reason == placement_rules::placement_reason::out_of_logistics_range)
            return construction_result::out_of_range;
        if (pr.reason == placement_rules::placement_reason::tech_locked)
            return construction_result::tech_locked;
        // BL-1347: A BUILDING NEEDS A ROAD, and the refusal says so rather than becoming
        // one more `invalid_tile`. The flattening this joins the exceptions to is BL-1364's
        // subject (every placement reason but the launchpad's collapses here); this rule
        // is the one a player can act on -- bring a road to the tile -- so it is the one
        // that must not arrive as "invalid tile".
        if (pr.reason == placement_rules::placement_reason::needs_road)
            return construction_result::needs_road;
        // BL-1399: a nation builds only on its own ground, and the refusal says so.
        if (pr.reason == placement_rules::placement_reason::not_own_ground)
            return construction_result::foreign_ground;
        if (type == building_type::launchpad)
            return construction_result::slot_occupied;
        return construction_result::invalid_tile; // Port not coastal
    }

    // Affordability (BL-095): the nation must be able to afford the whole build to
    // start it. The flat build cost is debited AT PLACEMENT, once (Ben, 2026-10-25,
    // below); the materials are drawn in arrears by run_construction. BL-1066
    // (Ben, 2026-09-23): "the whole build" is what the build will DRAW — flat cost
    // plus materials, times the site multiplier (PRODUCTION.md § Construction site
    // time). Pricing the authored cost admitted builds the corp could not finish.
    const float total_cost = construction_capex(w, reg, tile, type, target, recipe);
    if (funds < total_cost)
        return construction_result::insufficient_funds;

    // Create and author the building, mirroring corporation_generation.cpp Pass 3.
    const entity_id bld_id = w.create_entity();

    building_component bc;
    bc.tile               = tile;
    bc.type               = type;
    // Staff producing buildings so they run; ports, logistics hubs and military
    // bases take no L3 production action (passive infrastructure), so they staff
    // at zero. A research institute is staffed at construction like the
    // University (Ben, 2026-10-10; PRODUCTION.md): it adds RP only while
    // staffed (nation_tech.cpp, advance_research).
    bc.workforce_assigned = construction_initial_workforce(type);
    // Build-time pacing (playtest patch, 2026-07-06): the building sits idle
    // for its build ticks before economy_system lets it produce. BL-323 S3: that
    // duration depends on WHERE, not just WHAT (construction_build_ticks above),
    // read before this building is inserted so it does not count itself toward
    // the stack discount.
    // Construction is measured in DAYS (Ben, 2026-10-24): the site's days of work,
    // and the quarters they span as the gate every pass reads.
    bc.days_remaining  = construction_build_days(w, reg, tile, type, target);
    bc.ticks_remaining = (bc.days_remaining + k_construction_quarter_days - 1) / k_construction_quarter_days;

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
        // The default asks the body's air too (default_recipe_id_at), so no
        // path seeds a route the wrong_air gate above would refuse.
        bc.recipe = (recipe != no_recipe) ? recipe : reg.default_recipe_id_at(w, tile); // BL-429
    }

    // CREDITS UP FRONT, MATERIALS IN ARREARS (Ben, 2026-10-25): the site pays its
    // whole flat build cost now (the gate above admitted it), and builds from today
    // at the rate its shelf allows less what other sites have claimed; each day's
    // materials are drawn at the next economy tick. A founding site (unpriced
    // `funds`) is priced by generation and pays nothing here.
    begin_construction_on_placement(w, reg, corp, bc,
                                    /*founding=*/funds == std::numeric_limits<float>::max());

    w.buildings[bld_id]  = bc;
    w.stockpiles[bld_id] = stockpile_component{};

    // BL-366: a non-extraction placement may fill the tile's composition cap and
    // fire the one-way urban transform. Checked after insertion so the new
    // building itself counts toward the cap.
    if (type != building_type::extraction_site)
        placement_rules::maybe_transform_to_urban(w, tile);

    // BL-263: COMPLETION is the market-emergence trigger, and for an instant
    // build (ticks_remaining == 0 here — build_duration_ticks <= 0, above)
    // placement IS completion. The run_construction pacing loop (economy_system.cpp)
    // only ever visits buildings that started with ticks_remaining > 0, so this is
    // the one completion this function must catch itself.
    if (bc.ticks_remaining <= 0)
        maybe_spawn_market(w, reg, tile_it->second.body, tile);

    assets.push_back(bld_id);
    // Ben, 2026-10-25: the flat build cost was debited above, once; the materials
    // are drawn in arrears by run_construction as the build progresses.

    // Before this clear existed, a newly placed port/hub extended the reach
    // field only when something unrelated (a road) happened to clear the cache —
    // the rule took effect at an arbitrary later moment. The anchor still waits
    // on completion (is_supply_anchor's ticks_remaining contract), so only an
    // INSTANT-completing port/hub changes anything at placement; a durative one
    // is caught by run_construction's completion clear. Any other type changes
    // no cached answer, and the corp AI places buildings at tick rate
    // (2026-08-12 — see invalidate_logistics_caches' narrowing note).
    if (building_affects_logistics(type) && bc.ticks_remaining <= 0)
        invalidate_logistics_caches(w);
    // EVERY PORT LAYS LANES (Ben, 2026-10-25; LOGISTICS.md § 4b): placement IS completion
    // for an instant port, so it joins every standing port within fifty here; a durative
    // one lays them on its completion day (advance_construction_to).
    if (type == building_type::port && bc.ticks_remaining <= 0)
        lay_port_lanes(w, bld_id);

    out_building = bld_id;
    return construction_result::placed;
}

} // namespace

construction_result construct_building(world& w, const recipe_registry& reg,
                                       entity_id corp, entity_id tile,
                                       building_type type, resource_type target,
                                       entity_id& out_building,
                                       std::uint16_t recipe)
{
    // BL-1407: every builder is a NATION (the corporation is deleted); this is
    // the nation's own door under its old name, kept for its callers.
    return construct_nation(w, reg, corp, tile, type, target, recipe, out_building);
}

construction_result construct_nation(world& w, const recipe_registry& reg,
                                     entity_id nation, entity_id tile,
                                     building_type type, resource_type target,
                                     std::uint16_t recipe, entity_id& out_building,
                                     bool founding)
{
    // BL-1399 (Ben, 2026-10-23; NATIONS.md § What a nation owns): the nation builds
    // for itself, any type in any sector, with no corporation between - every major
    // and every minor. The nation is its own actor at the door, so the tech gate reads
    // its own tree and the own-ground gate (placement_rules::can_place_in_world) its
    // own territory. Paid from the treasury's uncommitted part; a founding build
    // (generation's) is admitted unpriced, as before.
    out_building = null_entity;
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return construction_result::no_corp;
    return construct_for(w, reg, nation, tile, type, target, out_building, recipe,
                         founding ? std::numeric_limits<float>::max()
                                  : state_uncommitted_treasury(w, nation),
                         nit->second.state_assets);
}

construction_result construct_state_building(world& w, const recipe_registry& reg,
                                             entity_id nation, entity_id tile,
                                             std::uint16_t recipe, entity_id& out_building,
                                             bool founding)
{
    return construct_nation(w, reg, nation, tile, building_type::processing_facility,
                            resource_type::iron_ore, recipe, out_building, founding);
}

construction_result construct_state_port(world& w, const recipe_registry& reg,
                                         entity_id nation, entity_id tile,
                                         entity_id& out_building, bool founding)
{
    return construct_nation(w, reg, nation, tile, building_type::port,
                            resource_type::iron_ore, no_recipe, out_building, founding);
}

construction_result construct_state_station(world& w, const recipe_registry& reg,
                                            entity_id nation, entity_id tile,
                                            entity_id& out_building)
{
    return construct_nation(w, reg, nation, tile, building_type::railway_station,
                            resource_type::iron_ore, no_recipe, out_building, false);
}

construction_result construct_state_logistics(world& w, const recipe_registry& reg,
                                              entity_id nation, entity_id tile,
                                              building_type type, entity_id& out_building)
{
    if (type != building_type::inland_logistics_hub)
    {
        out_building = null_entity;
        return construction_result::invalid_tile; // BL-1309: the nation's hub door
    }
    return construct_nation(w, reg, nation, tile, type, resource_type::iron_ore, no_recipe,
                            out_building, false);
}

float track_tile_cost(const world& w, const recipe_registry& reg, entity_id tile)
{
    const recipe_registry::rail_params& rp = reg.rail();
    float steel_price = 0.0f;
    if (const entity_id mid = market_for_tile(w, tile); mid != null_entity)
        if (const auto mit = w.markets.find(mid); mit != w.markets.end())
            steel_price = posted_price(mit->second, static_cast<std::size_t>(resource_type::steel));
    return rp.track_build_cost + rp.track_steel_per_tile * steel_price;
}

bool track_tile_extends(world& w, entity_id owner, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return false;
    const auto bit = w.bodies.find(tit->second.body);
    if (bit == w.bodies.end())
        return false;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const int x = tit->second.grid_x, y = tit->second.grid_y;
    const std::vector<entity_id>& grid = body_tile_grid(w, tit->second.body);
    const int cand[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                         y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
    for (const int c : cand)
    {
        if (c < 0 || static_cast<std::size_t>(c) >= grid.size())
            continue;
        const auto rt = w.rail_track.find(grid[static_cast<std::size_t>(c)]);
        if (rt != w.rail_track.end() && rt->second == owner)
            return true;
    }
    return false;
}

construction_result lay_track(world& w, const recipe_registry& reg, entity_id nation,
                              entity_id tile)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end() || !is_major_nation(w, nation)) // BL-1404: the saved fact
        return construction_result::no_corp; // a major's state industry lays track
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return construction_result::no_tile;
    if (!reg.building_available(building_type::railway_station))
        return construction_result::era_locked; // no railway in this era
    if (is_water(tit->second.substrate) || w.rail_track.count(tile) != 0)
        return construction_result::invalid_tile; // land only; once per tile
    const auto tn = w.tile_to_nation.find(tile);
    if (tn == w.tile_to_nation.end() || tn->second != nation)
        return construction_result::invalid_tile; // on its own ground
    // Laid tile to tile: the tile extends the nation's own line, or opens one at a
    // city (where its station will stand).
    bool city = false;
    for (const auto& [centre, ctile] : w.population_centre_tile)
    {
        (void)centre;
        if (ctile == tile) { city = true; break; }
    }
    if (!city && !track_tile_extends(w, nation, tile))
        return construction_result::invalid_tile;
    // THE STEEL IS REAL (the BL-1255 fix round): `track_steel_per_tile` units are
    // BOUGHT OFF THE TILE MARKET'S SHELF at its posted price — the stock drawn,
    // the treasury billed — the state-purchase shape every market draw takes
    // (network upkeep's and the space programme's market fallback: the goods
    // leave the shelf, the money leaves the treasury). No market, or a shelf
    // short of the steel: refused (insufficient_funds), nothing mutated.
    const float steel = reg.rail().track_steel_per_tile;
    market_component* mkt = nullptr;
    if (const entity_id mid = market_for_tile(w, tile); mid != null_entity)
        if (const auto mit = w.markets.find(mid); mit != w.markets.end())
            mkt = &mit->second;
    const std::size_t si = static_cast<std::size_t>(resource_type::steel);
    if (steel > 0.0f && (mkt == nullptr || !(mkt->inventory[si] >= steel)))
        return construction_result::insufficient_funds;
    const float cost = track_tile_cost(w, reg, tile);
    if (!std::isfinite(cost) || cost < 0.0f || state_uncommitted_treasury(w, nation) < cost)
        return construction_result::insufficient_funds;
    // Commit. Instant, like a road: paid up front from the treasury.
    if (steel > 0.0f)
        mkt->inventory[si] -= steel;
    nit->second.treasury -= cost;
    w.state_ledger[nation].construction += cost;
    w.rail_track.emplace(tile, nation);
    invalidate_logistics_caches(w);
    return construction_result::placed;
}

bool demolish_state_building(world& w, entity_id nation, entity_id building)
{
    const auto bld_it = w.buildings.find(building);
    if (bld_it == w.buildings.end())
        return false;
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return false;
    std::vector<entity_id>& sa = nit->second.state_assets;
    const auto it = std::find(sa.begin(), sa.end(), building);
    if (it == sa.end())
        return false;
    const building_type demolished_type = bld_it->second.type;
    // A SITE CANCELLED BEFORE IT COMPLETES REFUNDS ITS CREDITS IN FULL (Ben,
    // 2026-10-25): the whole flat cost it paid at placement goes back to the
    // treasury, booked on the next quarterly return as a refund. The materials it
    // drew are not returned — they were spent on the site — and any it still owed
    // are cleared, not drawn (the building and its debt go together). A standing
    // building refunds nothing.
    if (bld_it->second.ticks_remaining > 0 && bld_it->second.build_credit_paid > 0.0f)
    {
        nation_component& nc = nit->second;
        const float back = bld_it->second.build_credit_paid;
        if (nc.refund_unbooked == 0.0f)
            nc.refund_opening = nc.treasury;
        nc.treasury        += back;
        nc.refund_unbooked += back;
    }
    sa.erase(it);
    w.buildings.erase(bld_it);
    w.stockpiles.erase(building);
    if (building_affects_logistics(demolished_type))
        invalidate_logistics_caches(w);
    return true;
}

bool demolish_building(world& w, entity_id corp, entity_id building)
{

    const auto bld_it = w.buildings.find(building);
    if (bld_it == w.buildings.end())
        return false;

    // BL-1407: every owner is a NATION; its door removes the building, its
    // stockpile and its entry in `state_assets`, and refuses one it does not own.
    return demolish_state_building(w, corp, building);
}

construction_result place_road(world& w, const recipe_registry& reg,
                               entity_id corp, entity_id tile, std::uint8_t tier,
                               std::uint8_t side)
{
    // BL-1402 (NATIONS.md § What a nation owns, § Own ground): the payer is
    // OWNER-GENERIC — a corporation's balance or a nation's treasury
    // (`purse_of`). The own-ground rule already asks the actor's nation
    // (`placement_rules::can_place_road` reads `owner_home_nation`), so a nation
    // laying road as itself needs only its purse here.
    float* const purse = purse_of(w, corp);
    if (purse == nullptr)
        return construction_result::no_corp;
    const bool nation_payer = true; // BL-1407: every payer is a nation

    const auto tile_it = w.tiles.find(tile);
    if (tile_it == w.tiles.end())
        return construction_result::no_tile;

    // BL-1340: the SIDE the road crosses. `k_road_side_auto` resolves here, once, so the
    // refusals below read the side that would actually be laid.
    const int chosen = (side == k_road_side_auto)
                           ? placement_rules::auto_road_side(w, tile, tier, corp)
                           : static_cast<int>(side);
    if (chosen < 0 || chosen > 5)
        return construction_result::invalid_tile;

    // Validity: land at one end, a neighbour that way, a tier that strictly upgrades the
    // crossing (BL-172 / BL-1340), a ROAD ON THE NAMED TILE to build out of (BL-1348), and
    // ground this corp's nation may build from (BL-1357 -- one crossing over a border, and
    // no further). The ACTOR is handed in: it is what makes "a tile you hold" askable.
    const placement_rules::placement_result rr =
        placement_rules::can_place_road(w, tile, chosen, tier, corp);
    if (!rr)
    {
        if (rr.reason == placement_rules::placement_reason::no_road_to_extend)
            return construction_result::needs_road;
        if (rr.reason == placement_rules::placement_reason::foreign_ground)
            return construction_result::foreign_ground;
        return construction_result::invalid_tile;
    }

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
    // A corporation's room is its balance (the comparison it always made); a
    // nation's is its UNCOMMITTED treasury, never below zero — a nation never
    // runs into debt (BL-1402).
    if (purse_room_of(w, corp) < total_cost)
        return construction_result::insufficient_funds;

    // Commit: raise the EDGE to the chosen road tier (both halves — BL-1340), THEN debit,
    // and invalidate the logistics caches (the road field moved, so cached path and reach
    // costs are stale — world.hpp invalidation contract). The lay goes first so no float
    // ever makes the round trip out of the balance and back: a debit-then-refund is not
    // exactly the identity (review of 0831de6d).
    if (!lay_road_edge(w, tile, chosen, tier)) // BL-172: 1=Track, 2=Road, 3=Highway
        return construction_result::invalid_tile;
    *purse -= total_cost;
    // A nation's road is booked where its track is (lay_track): the state
    // ledger's construction line, so the treasury's fall is explained.
    if (nation_payer)
        w.state_ledger[corp].construction += total_cost;
    invalidate_logistics_caches(w);

    return construction_result::placed;
}

// Declared beside `recipe_runs_on_body` in recipe_registry.hpp; defined here, in
// a Lua-free TU, so the light (SDL/Lua-free) harness build links it. (It lived in
// tech_gate.cpp until BL-1273 retired that file.)
bool recipe_runs_at_tile(const world& w, const recipe& rc, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return true;
    const auto bit = w.bodies.find(tit->second.body);
    return bit == w.bodies.end() || recipe_runs_on_body(rc, bit->second);
}
