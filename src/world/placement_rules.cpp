#include "world/placement_rules.hpp"

#include "world/hex_neighbors.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"  // BL-513: the province building ceiling
#include "world/nation_tech.hpp" // structure_unlocked (BL-1273; BL-344 before it)
#include "world/world.hpp"

#include <algorithm>

namespace placement_rules {

const char* placement_reason_text(placement_reason r)
{
    switch (r)
    {
        case placement_reason::ok:                return "Can build here";
        case placement_reason::ocean:             return "Cannot build on water";
        case placement_reason::no_deposit:        return "No extractable deposit here";
        case placement_reason::not_coastal:       return "A port must sit on the coast";
        case placement_reason::launchpad_exists:  return "This body already has a launchpad";
        case placement_reason::occupied:          return "Tile already built on";
        case placement_reason::outside_territory: return "Outside your territory";
        case placement_reason::unsurveyed:        return "Body not yet surveyed";
        case placement_reason::slot_full:         return "No build slot free here";
        case placement_reason::no_tile:           return "No such tile";
        case placement_reason::already_road:      return "This tile already has an equal or better road";
        case placement_reason::deposit_present:   return "This terrain already supports a Farm — no Hydroponics Bay needed here";
        case placement_reason::out_of_logistics_range: return "Too far from a city, port or logistics hub to be supplied";
        case placement_reason::tech_locked: return "Locked - the technology that permits this has not been researched";
        case placement_reason::province_full: return "This land already sustains as much as it can - improve it, or build elsewhere";
        case placement_reason::needs_centre: return "Must be built in a population centre";
        case placement_reason::centre_too_small: return "This settlement is too small to host it - a larger centre is needed";
        case placement_reason::not_fresh_water: return "A well must stand on a river or beside a lake";
        case placement_reason::far_from_centre: return "Too far from a population centre to draw a workforce";
        case placement_reason::needs_road: return "No road reaches this tile - lay one to it before building here";
        case placement_reason::no_road_to_extend: return "A road is laid from a road - this tile carries none to build out of";
        case placement_reason::foreign_ground: return "A road may cross a border, but not run on past it - this ground is not your nation's";
        case placement_reason::not_own_ground: return "A nation builds only on its own ground - this tile is not your nation's";
        case placement_reason::site_under_construction: return "A building is already under construction on this tile - one site at a time";
    }
    return "Cannot build here";
}

bool is_water_tile(terrain_substrate sub)
{
    // BL-516: delegates to the terrain-level predicate so there is ONE
    // definition of "is this water". Renamed from `is_ocean_tile` in the same
    // change: since water gained kinds, a function called `is_ocean_tile` that
    // answers true for a lake is a lie waiting to be believed.
    return is_water(sub);
}

bool is_extractable(resource_type r)
{
    for (resource_type e : k_extractable)
        if (e == r)
            return true;
    return false;
}

float extractable_deposit(const tile_component& tc)
{
    float sum = 0.0f;
    for (resource_type r : k_extractable)
        sum += tc.resource_deposit[static_cast<std::size_t>(r)];
    return sum;
}

resource_type richest_extractable(const tile_component& tc, bool& any)
{
    resource_type best = resource_type::iron_ore;
    float best_val = 0.0f;
    any = false;
    for (resource_type r : k_extractable)
    {
        const float v = tc.resource_deposit[static_cast<std::size_t>(r)];
        if (v > best_val)
        {
            best_val = v;
            best     = r;
            any      = true;
        }
    }
    return best;
}

bool can_place_population_centre(const tile_component& tc)
{
    // Ocean tiles never host population centres.
    if (is_water_tile(tc.substrate))
        return false;
    // Uninhabitable tiles (hazard-dominated, airless, etc.) are also excluded.
    return tc.habitability > 0.0f;
}

placement_result can_place_road(const tile_component& tc, std::uint8_t tier)
{
    // BL-1340 (Ben, 2026-10-17): water is NO LONGER a refusal. A road edge may span water
    // and a run of such edges is a bridge, so a water tile with a neighbour can carry one;
    // the real gate is the per-side form below, which this weak form cannot reach.
    //
    // UPGRADE BEFORE EXTEND, which is what "no side named" means (BL-147's `already_road`,
    // generalised; review of 0831de6d). With no direction given, a press on this tile is
    // "improve the road HERE": it is `ok` when the tile carries no crossing at all (the
    // first one is laid) or when some crossable side carries a LOWER tier (that crossing
    // is raised). A tile whose every crossing already stands at or above @p tier is
    // `already_road` — the press does not quietly buy a second crossing on a fresh side,
    // which would let one tile be paid for up to eighteen times. A caller that really
    // wants another direction names the side.
    if (tier == 0 || tier > 3)
        return placement_reason::already_road;
    // BL-1348 (Ben, 2026-10-16): a tile carrying NO crossing used to be the easy `ok`
    // case -- "nothing here yet: the first crossing". It is now the refusal: a road is
    // laid FROM a road, so a tile with none is nowhere to build out of, whatever side the
    // caller might name next.
    if (max_road_tier(tc) == 0)
        return placement_reason::no_road_to_extend;
    bool upgradable = false;
    for (int s = 0; s < 6 && !upgradable; ++s)
    {
        const std::uint8_t t = road_tier_on_side(tc, s);
        upgradable = road_side_is_crossable(s, tc.grid_y) && t > 0 && t < tier;
    }
    return upgradable ? placement_result{placement_reason::ok}
                      : placement_result{placement_reason::already_road};
}

placement_result can_place_road(const world& w, entity_id tile, int side, std::uint8_t tier,
                                entity_id actor)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return placement_reason::ocean; // no tile: nothing to lay a road from
    if (side < 0 || side > 5 || tier == 0 || tier > 3)
        return placement_reason::ocean;
    // ONLY A SIDE A WALK CAN CROSS (Ben, 2026-10-17; LOGISTICS.md § 4): the two diagonals
    // the 4-cardinal walks skip are refused — a road there would be bought, counted and
    // billed and crossed by nothing, "not a road but a receipt".
    if (!road_side_is_crossable(side, tit->second.grid_y))
        return placement_reason::ocean;
    // A ROAD TO NOWHERE CANNOT BE EXPRESSED (LOGISTICS.md § 4): a side with no neighbour
    // is not a crossing. WATER AT EITHER END IS FINE (Ben, 2026-10-17: "Roads can go over
    // water with bridges no problem") — a run of such edges IS the bridge, and the cost
    // per crossing is what keeps a pointless span over open water from being free.
    const entity_id other = road_edge_neighbour(w, tile, side);
    if (other == null_entity)
        return placement_reason::ocean;
    const auto oit = w.tiles.find(other);
    if (oit == w.tiles.end())
        return placement_reason::ocean;
    // A CROSSING THE PLAYER LAYS MUST TOUCH LAND AT ONE END (Ben, 2026-10-17;
    // LOGISTICS.md § 4). A bridge spans a strait, and spanning is all it does: without
    // this, the only brake on walking a road out across open ocean, cell by paid cell, is
    // the price of each crossing. Generation never threads open sea, so the two rules
    // differ only in the middle of a two-cell strait, where the player builds from a shore
    // rather than from the water — accepted as the cost of the brake. GENERATION IS NOT
    // GATED HERE: `lay_road_edge` stays permissive and its routes refuse open sea already.
    if (is_water_tile(tit->second.substrate) && is_water_tile(oit->second.substrate))
        return placement_reason::ocean;
    if (road_tier_on_side(tit->second, side) >= tier)
        return placement_reason::already_road;
    // A ROAD IS LAID FROM A ROAD, ACROSS TO THE NEXT TILE (BL-1348; Ben, 2026-10-16;
    // LOGISTICS.md § 4). The NAMED tile must already carry a roaded edge: a road cannot be
    // dropped in open country, and a frontier is reached one crossing at a time. The rule
    // is bootstrappable because generation's own rule gives every market centre's tile at
    // least a Track, so no world begins with nowhere to build from. Read on the named tile
    // only -- the far end is what the crossing REACHES, and requiring a road there too
    // would make every extension impossible.
    if (max_road_tier(tit->second) == 0)
        return placement_reason::no_road_to_extend;
    // ...AND NEVER FURTHER THAN ONE CROSSING OVER A BORDER (BL-1357; Ben, 2026-10-16).
    // Crossing INTO a neighbour's tile is always permitted (nothing here refuses it -- it
    // is what stops a foreign-held market centre being unconnectable), but the foreign
    // tile the road landed on is NOT a tile this actor may build FROM. So the road stops
    // where it lands: a connection, never a corridor through somebody's country. Asked of
    // the ACTOR's own nation, because "a tile you hold" is the rule's own wording; with no
    // actor named (a generation stamp, a bare affordance) there is no holder to compare
    // against and this half does not fire.
    if (actor != null_entity)
    {
        const entity_id home = owner_home_nation(w, actor);
        if (home != null_entity)
        {
            const auto hn = w.tile_to_nation.find(tile);
            const entity_id holder = hn != w.tile_to_nation.end() ? hn->second : null_entity;
            // BL-1399 (Ben, 2026-10-23; GLOSSARY.md § Own ground): a nation builds only on
            // its own ground, and ground NO nation holds is not its own either. Unowned
            // ground used to be walkable so a remote deposit could be reached; nothing may
            // be built there now, so the chain stops at it exactly as at a border.
            if (holder != home)
                return placement_reason::foreign_ground;
        }
    }
    return placement_reason::ok;
}

int auto_road_side(const world& w, entity_id tile, std::uint8_t tier, entity_id actor)
{
    // UPGRADE BEFORE EXTEND (review of 0831de6d). "No side named" means "improve the road
    // here", so the crossing the tile ALREADY has comes first: the highest existing tier
    // this press would raise, ties to the lower side index. Only a tile with no crossing
    // at all takes a NEW one, and then the cheapest neighbour. Without this an auto press
    // bought a fresh edge on a fresh side every time — one tile paid for up to eighteen
    // times, nothing joined. `can_place_road` refuses a side no walk can cross (Ben,
    // 2026-10-17), so only the four the row parity reaches are ever offered.
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return -1;
    {
        int best_up = -1;
        std::uint8_t have = 0;
        for (int s = 0; s < 6; ++s)
        {
            const std::uint8_t t = road_tier_on_side(tit->second, s);
            if (t == 0 || t >= tier || t <= have)
                continue;
            if (!can_place_road(w, tile, s, tier, actor))
                continue;
            have    = t;
            best_up = s;
        }
        if (best_up >= 0)
            return best_up;
        if (max_road_tier(tit->second) > 0)
            return -1; // every crossing it has is already at or above this tier
    }
    int   best = -1;
    float best_cost = 0.0f;
    for (int s = 0; s < 6; ++s)
    {
        if (!can_place_road(w, tile, s, tier, actor))
            continue;
        const entity_id other = road_edge_neighbour(w, tile, s);
        const auto oit = w.tiles.find(other);
        if (oit == w.tiles.end())
            continue;
        const float c = tile_traversal_cost(oit->second);
        if (best < 0 || c < best_cost) // ties keep the lower side index
        {
            best = s;
            best_cost = c;
        }
    }
    return best;
}

placement_result can_place(const tile_component& tc, building_type type, resource_type target)
{
    // No building ever sits on water.
    if (is_water_tile(tc.substrate))
        return placement_reason::ocean;

    switch (type)
    {
        case building_type::extraction_site:
            // Extraction must sit on a non-zero deposit of a prototype-extractable
            // target resource.
            // BL-366: an urban tile is built over — no new extraction or
            // ambient-resource placement. Sites already standing when the
            // transform fired are grandfathered (this only gates NEW placement).
            if (tc.cover == terrain_cover::urban)
                return placement_reason::no_deposit;
            if (is_extractable(target)
                && tc.resource_deposit[static_cast<std::size_t>(target)] > 0.0f)
                return placement_reason::ok;
            // Fishing Wharf (BL-168): an agricultural_produce site with no terrestrial
            // deposit can still work a coastal tile. That needs world access
            // (is_coastal), so this tile-only check passes it through rather than
            // rejecting on no_deposit — can_place_in_world enforces the coastal gate.
            if (target == resource_type::agricultural_produce)
                return placement_reason::ok;
            // Well (BL-1198, NR-971): the same pass-through for water — its
            // fresh-water gate (is_fresh_water_adjacent) needs world access too,
            // so can_place_in_world enforces it.
            if (target == resource_type::water)
                return placement_reason::ok;
            return placement_reason::no_deposit;

        case building_type::processing_facility:
            // Hydroponics Bay (BL-166): only where the terrestrial Farm deposit was
            // NOT seeded — habitability/terrain gated, the mirror image of the
            // extraction deposit check above. Every other processing target/recipe
            // (Smelter, Refinery, Food Processor, ...) is unaffected.
            if (target == resource_type::agricultural_produce
                && tc.resource_deposit[static_cast<std::size_t>(resource_type::agricultural_produce)] > 0.0f)
                return placement_reason::deposit_present;
            return placement_reason::ok;

        case building_type::port:
        case building_type::inland_logistics_hub: // BL-149: any non-ocean land tile (the land-network node).
        case building_type::military_base:        // BL-325 S1: any non-ocean land tile, no deposit — the
                                                  // reach rule still applies at the world level, and the
                                                  // base is NOT an anchor, so it earns no exemption there.
        case building_type::research_institute:  // BL-332: same shape as military_base — passive,
                                                  // no deposit needed, any non-ocean land tile.
        case building_type::none:
        default:
            // Any non-ocean land tile is valid for non-extraction buildings.
            return placement_reason::ok;
    }
}

namespace {

/// The six-neighbour scan `is_coastal` was written as, lifted out so the Well's
/// lake test (BL-1198) walks exactly the same neighbours by exactly the same
/// route. True if any in-grid hex neighbour of @p tc satisfies @p pred on its
/// substrate. Pure; an existence test, so the answer is order-independent.
template <typename Pred>
bool any_neighbour_substrate(const world& w, const tile_component& tc, Pred pred)
{
    const entity_id body = tc.body;

    const auto body_it = w.bodies.find(body);
    if (body_it == w.bodies.end())
        return false;
    const int gw = body_it->second.grid_width;
    const int gh = body_it->second.grid_height;

    // Neighbour resolution goes through the per-body raster index when it is built
    // (world.body_tile_index, the BL-077 derived cache) — O(1) per neighbour. This
    // stopped being a cold player-action path once corp_ai began calling the
    // placement family per candidate tile per evaluation (BL-360). This function
    // holds const world& so it cannot build the index itself; a world where it is
    // absent (hand-built harness fixtures, pre-index calls) falls back to the old
    // per-body linear scan, which resolves the same neighbour.
    const std::vector<entity_id>* raster = nullptr;
    if (const auto rit = w.body_tile_index.find(body);
        rit != w.body_tile_index.end()
        && rit->second.size() == static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        raster = &rit->second;

    // Odd-r offset neighbours (pointy-top hexes; canonical table, BL-363).
    const int (*off)[2] = hex_neighbors::offsets(tc.grid_y);

    for (int n = 0; n < 6; ++n)
    {
        const int nrow = tc.grid_y + off[n][1];
        if (nrow < 0 || nrow >= gh)
            continue;
        int ncol = tc.grid_x + off[n][0];
        // Columns wrap (horizontal cylinder).
        if (ncol < 0) ncol += gw;
        else if (ncol >= gw) ncol -= gw;

        if (raster != nullptr)
        {
            const entity_id nid = (*raster)[static_cast<std::size_t>(nrow)
                                                * static_cast<std::size_t>(gw)
                                            + static_cast<std::size_t>(ncol)];
            if (nid == null_entity)
                continue;
            const auto nit = w.tiles.find(nid);
            if (nit != w.tiles.end() && pred(nit->second.substrate))
                return true;
            continue;
        }

        // Fallback: find the neighbour by scanning tiles on this body.
        for (const auto& [nid, ntc] : w.tiles)
        {
            if (ntc.body == body && ntc.grid_x == ncol && ntc.grid_y == nrow)
            {
                if (pred(ntc.substrate))
                    return true;
                break;
            }
        }
    }
    return false;
}

} // namespace

bool is_coastal(const world& w, entity_id tile_id)
{
    const auto tc_it = w.tiles.find(tile_id);
    if (tc_it == w.tiles.end())
        return false;
    // BL-516: SEA, not merely water. `is_coastal` gates ports, the
    // Fishing Wharf and coastal-only extraction — a lakeshore is not a
    // coast, and now the data can say so. This is the one place the
    // narrowing changes an answer the old code gave; every other water
    // test here is widened to `is_water` and answers exactly as before.
    return any_neighbour_substrate(w, tc_it->second,
                                   [](terrain_substrate s) { return is_sea(s); });
}

bool is_fresh_water_adjacent(const world& w, entity_id tile_id)
{
    const auto tc_it = w.tiles.find(tile_id);
    if (tc_it == w.tiles.end())
        return false;
    const tile_component& tc = tc_it->second;
    // A Well stands on land; water ground is never "adjacent" to itself.
    if (is_water(tc.substrate))
        return false;
    // A river is an EDGE on the tile (BL-170): any side carrying one means the
    // river runs through or along this tile. generate_rivers sets the bit on
    // BOTH tiles sharing the side, so each bank qualifies.
    if (tc.river_edges != 0)
        return true;
    // Or a lake beside it — fresh, never the sea (is_coastal's mirror).
    return any_neighbour_substrate(w, tc,
                                   [](terrain_substrate s) { return is_lake(s); });
}

bool is_well_site(const world& w, entity_id tile_id, resource_type target)
{
    if (target != resource_type::water)
        return false;
    const auto tc_it = w.tiles.find(tile_id);
    if (tc_it == w.tiles.end())
        return false;
    // An ice deposit makes it an Ice Extractor, whatever runs past it.
    if (tc_it->second.resource_deposit[static_cast<std::size_t>(resource_type::water)] > 0.0f)
        return false;
    return is_fresh_water_adjacent(w, tile_id);
}

bool is_wharf_site(const world& w, entity_id tile_id, resource_type target)
{
    if (target != resource_type::agricultural_produce)
        return false;
    const auto tc_it = w.tiles.find(tile_id);
    if (tc_it == w.tiles.end())
        return false;
    // A produce deposit makes it a Farm, whatever sea lies beside it — the
    // same `<= 0` test can_place_in_world's Wharf gate reads.
    if (tc_it->second.resource_deposit[static_cast<std::size_t>(resource_type::agricultural_produce)] > 0.0f)
        return false;
    // BL-1218: a Wharf stands on the SHORE, never in the water — the Well's
    // `is_fresh_water_adjacent` rule. A coast-substrate water tile borders open
    // ocean, so without this it read as a Wharf site: the construction ledger
    // offered a Wharf there that `can_place` then refused as "Cannot build on
    // water". World callers already filtered water ground, so only the UI moves.
    if (is_water(tc_it->second.substrate))
        return false;
    return is_coastal(w, tile_id);
}

bool is_depositless_site(const world& w, entity_id tile_id, resource_type target)
{
    return is_well_site(w, tile_id, target) || is_wharf_site(w, tile_id, target);
}

namespace {

/// BL-615: the highest stratum among population centres anchored on
/// @p tile_id, or -1 when none is. A max over `world::population_centres`'
/// unordered iteration is order-independent, so the answer is deterministic
/// whatever the container's layout.
int centre_scale_on_tile(const world& w, entity_id tile_id)
{
    int best = -1;
    for (const auto& [pid, pcc] : w.population_centres)
    {
        const auto it = w.population_centre_tile.find(pid);
        if (it != w.population_centre_tile.end() && it->second == tile_id && pcc.scale > best)
            best = pcc.scale;
    }
    return best;
}

/// BL-615: true if SOME population centre on @p tc's body sits within
/// @p radius grid steps of it. Wrapped squared grid distance — columns wrap
/// (the body is a horizontal cylinder), rows do not — the same metric every
/// other proximity read in the codebase uses (nearest_market, corp-holding
/// contiguity, cradle spacing). An existence test over an unordered container
/// is order-independent: deterministic by construction.
bool centre_within_radius(const world& w, const tile_component& tc, int radius)
{
    int gw = 0;
    if (const auto bit = w.bodies.find(tc.body); bit != w.bodies.end())
        gw = bit->second.grid_width;

    const long long r2 = static_cast<long long>(radius) * radius;
    for (const auto& [pid, pcc] : w.population_centres)
    {
        const auto tit = w.population_centre_tile.find(pid);
        if (tit == w.population_centre_tile.end())
            continue;
        const auto ctc_it = w.tiles.find(tit->second);
        if (ctc_it == w.tiles.end() || ctc_it->second.body != tc.body)
            continue;
        long long dx = ctc_it->second.grid_x - tc.grid_x;
        if (dx < 0) dx = -dx;
        if (gw > 0 && gw - dx < dx)
            dx = gw - dx; // shorter way round the cylinder
        long long dy = ctc_it->second.grid_y - tc.grid_y;
        if (dy < 0) dy = -dy;
        if (dx * dx + dy * dy <= r2)
            return true;
    }
    return false;
}

} // namespace

placement_result can_place_in_world(const world& w, entity_id tile_id,
                                    building_type type, resource_type target,
                                    float max_reach, entity_id corp,
                                    placement_gate gate)
{
    const auto tc_it = w.tiles.find(tile_id);
    if (tc_it == w.tiles.end())
        return placement_reason::no_tile;
    // Tile-level terrain/deposit check first — propagate its specific reason.
    if (const placement_result tile_ok = can_place(tc_it->second, type, target); !tile_ok)
        return tile_ok;

    // BL-344 tech gate. Ahead of the world-level terrain checks on purpose: a
    // locked building type is locked everywhere, so "you have not researched
    // this" is the truer refusal than "that tile is not coastal". Skipped
    // entirely when the caller named no corp (see the header).
    if (!structure_unlocked(w, corp, type))
        return placement_reason::tech_locked;

    // BL-1399 OWN GROUND (Ben, 2026-10-23; NATIONS.md § Own ground): a nation builds
    // only on its own tiles - never another nation's, never ground no nation owns. Asked
    // of the builder's nation, for every building kind. Skipped when no builder is named
    // (a generation stamp, a bare affordance), like the tech gate above.
    if (corp != null_entity)
    {
        const entity_id home = owner_home_nation(w, corp);
        const auto hn = w.tile_to_nation.find(tile_id);
        const entity_id holder = hn != w.tile_to_nation.end() ? hn->second : null_entity;
        if (home == null_entity || holder != home)
            return placement_reason::not_own_ground;
    }

    if (type == building_type::port)
    {
        if (!is_coastal(w, tile_id))
            return placement_reason::not_coastal;
    }
    else if (type == building_type::extraction_site
             && target == resource_type::agricultural_produce
             && tc_it->second.resource_deposit[static_cast<std::size_t>(resource_type::agricultural_produce)] <= 0.0f)
    {
        // Fishing Wharf (BL-168): no terrestrial deposit here, so it must be coastal.
        if (!is_coastal(w, tile_id))
            return placement_reason::not_coastal;
    }
    else if (type == building_type::extraction_site
             && target == resource_type::water
             && tc_it->second.resource_deposit[static_cast<std::size_t>(resource_type::water)] <= 0.0f)
    {
        // Well (BL-1198, NR-971): no ice deposit here, so it must stand on fresh
        // water — a river along the tile or a lake beside it.
        if (!is_fresh_water_adjacent(w, tile_id))
            return placement_reason::not_fresh_water;
    }
    else if (type == building_type::launchpad)
    {
        // Count launchpads already on this body.
        const entity_id body = tc_it->second.body;
        for (const auto& [bid, bc] : w.buildings)
        {
            if (bc.type != building_type::launchpad)
                continue;
            const auto btc_it = w.tiles.find(bc.tile);
            if (btc_it != w.tiles.end() && btc_it->second.body == body)
                return placement_reason::launchpad_exists; // Already one on this body.
        }
    }

    // BL-615 stratum placement gates (POPULATION.md § Strata gate buildings):
    // where the workforce lives decides where some buildings may stand. Checked
    // after the terrain/type reasons (which teach more about THIS tile) and
    // before the logistics reach rule (this one names the missing thing — a
    // settlement — where reach names only a cost). The gate is authored DATA on
    // the building definition, resolved by the caller
    // (recipe_registry::placement_gate_for); a default gate gates nothing, so
    // every pre-BL-615 call site is unchanged.
    if (gate.requires_centre || gate.min_centre_scale > 0)
    {
        const int scale = centre_scale_on_tile(w, tile_id);
        if (scale < 0)
            return placement_reason::needs_centre;
        if (scale < gate.min_centre_scale)
            return placement_reason::centre_too_small;
    }
    if (gate.centre_proximity_radius > 0
        && !centre_within_radius(w, tc_it->second, gate.centre_proximity_radius))
        return placement_reason::far_from_centre;

    // Logistics reach (BL-323 S2): the site must be suppliable. Checked after the
    // terrain reasons (which teach more) and before the stack ceiling (which teaches
    // least). Skipped entirely when the caller passes no budget, or when the body's
    // reach field has not been built — a rule enforced on a guess is worse than one
    // not enforced, so `tile_reach_cost`'s -1 means "unknown", never "unreachable".
    //
    // A supply anchor itself always passes: a port or hub IS the anchor, so gating it
    // on being near one would make the first node on a fresh coast unplaceable and
    // the rule unbootstrappable.
    //
    // FIRST-ANCHOR BOOTSTRAP (Ben's ruling, 2026-08-08). The anchor-tile exemption
    // above only covers tiles that already ARE anchors, which no tile on a virgin
    // body is — so before this, a body with no city/port/hub refused every
    // placement including the first hub, and Era 1 off-world expansion was
    // impossible. An anchor-TYPE placement therefore skips the rule when the body
    // has no anchor of any kind yet. Existence ends the exemption (see
    // body_has_supply_anchor): the second hub must chain within reach of the
    // first, which is the discipline the rule exists to create.
    if (max_reach >= 0.0f && !::is_supply_anchor(w, tile_id))
    {
        const bool placing_anchor = type == building_type::port
                                 || type == building_type::inland_logistics_hub;
        const bool bootstrap = placing_anchor
                            && !::body_has_supply_anchor(w, tc_it->second.body);
        if (!bootstrap)
        {
            const float reach = ::tile_reach_cost(w, tile_id);
            if (reach >= 0.0f && !(reach <= max_reach))
                return placement_reason::out_of_logistics_range;
        }
    }

    // A BUILDING NEEDS A ROAD (BL-1347; Ben, 2026-10-16; PRODUCTION.md § A building
    // needs a road). A tile carries a road when ANY of its hex sides carries a roaded
    // edge — a crossing INTO the tile is a road ON it, from either side, which is why
    // this reads `tile_has_road` (the whole six-side mask) and not one named side.
    //
    // EXTRACTION IS NOT EXEMPT (Ben's call): a remote deposit is worked only once a road
    // reaches it, which with BL-1348 means a chain of crossings built out to it. That is
    // the intended cost of reaching far ore, and the reason the richest tile on the body
    // is a decision rather than a lookup.
    //
    // Checked beside the stack and province ceilings and ahead of them, because this one
    // names a thing the player can FIX on this tile — bring a road to it — where a full
    // stack and a full province only say "not here". Generated buildings are bound by the
    // same rule at their own end rather than exempted here: the road passes lay a crossing
    // to every site generation stamps (`lay_site_roads`, road_generation.hpp), so the 1960
    // world holds no building this would have refused.
    if (!tile_has_road(tc_it->second))
        return placement_reason::needs_road;

    // ONE SITE PER TILE (Ben, 2026-10-26: "To slow the pace of the game down, we need
    // to add a limit so that tiles can only construct one building at a time";
    // PRODUCTION.md § One site under construction per tile). A tile holding a building
    // under construction takes no second building of any type until it completes. Here,
    // in the one world-level placement check, so the build door, the scorer's
    // candidates and every preview refuse it alike. Generation is not touched: every
    // generated building stands complete, so no generated tile holds a site.
    if (tile_has_site_under_construction(w, tile_id))
        return placement_reason::site_under_construction;

    // Stack ceiling: a tile carries several sites working one deposit, but not
    // without limit. Checked last — a full stack is the least interesting reason
    // to refuse, and the terrain reasons above teach the player more.
    //
    // BL-366: non-extraction occupancy is counted in aggregate (every type
    // combined) against the tile's cap, not per building type — the cap
    // bounds the whole non-extraction stack, which is what fires the urban
    // transform.
    const int occupancy = (type == building_type::extraction_site)
        ? buildings_on_tile(w, tile_id, type, target)
        : non_extraction_buildings_on_tile(w, tile_id);
    if (occupancy >= stack_capacity(tc_it->second, type, target))
        return placement_reason::slot_full;

    // Province ceiling (BL-513) — "how much can this land sustain?"
    //
    // A SECOND, INDEPENDENT LIMIT, not a replacement for the stack cap above.
    // The stack cap asks whether the DEPOSIT under this tile supports another
    // site and answers from richness; this asks whether the LAND supports
    // another building at all, and answers from area + infrastructure +
    // habitability + population (province.hpp holds the shape). Both must pass.
    //
    // TYPE-AGNOSTIC by Ben's ruling, 2026-08-21: it bounds the TOTAL standing in
    // the province and says nothing about the mix. Extraction sites count toward
    // it like everything else — they occupy land too — while staying separately
    // bounded by their deposit.
    //
    // Checked LAST, and deliberately: every reason above teaches the player
    // something about THIS tile, whereas this one is about the neighbourhood.
    // A -1 ceiling means the partition has not been built (or the tile is
    // unpartitioned) and is UNKNOWN, never "no room" — the rule is skipped
    // rather than guessed at, the same contract tile_reach_cost uses.
    if (const uint32_t province_id = w.provinces.province_of(tile_id); province_id != 0)
    {
        const int ceiling = ::province_building_ceiling(w, province_id);
        if (ceiling >= 0 && ::province_buildings_standing(w, province_id) >= ceiling)
            return placement_reason::province_full;
    }

    return placement_reason::ok;
}

// --- Building stacks (BL-193; the rule and both constants live in the header) --

float stack_output_scalar(int /*rank*/)
{
    // Every site in a stack yields in full (Ben, 2026-10-26: "There's already
    // depletion"); the shared taper in run_extraction's stack pre-pass is the brake.
    return 1.0f;
}

int stack_capacity(const tile_component& tc, building_type type, resource_type target)
{
    if (type != building_type::extraction_site)
        return non_extraction_stack_cap(tc.substrate, tc.cover);

    const float richness = tc.resource_deposit[static_cast<std::size_t>(target)];
    const int   cap      = static_cast<int>(richness / k_richness_per_site);
    return cap < 1 ? 1 : cap;
}

// BL-366's non-extraction ceiling, SPLIT BY MEANING (BL-519).
//
// The pre-split table was one number per composition, and that hid the two
// separate questions it was answering: how much weight will the GROUND take, and
// how much does what is GROWING ON IT get in the way. Splitting them is what lets
// a forested rocky upland be harder to build on than the bare rock beside it,
// which the single slot could not say.
//
// CALIBRATED, not re-tuned. Every pre-split composition reproduces its old number:
//
//   old composition   substrate     cover    base + modifier = cap   (was)
//   ---------------   -----------   ------   ---------------------   -----
//   grassland         sedimentary   grass      6 +  0 =  6              6
//   forest            sedimentary   forest     6 +  0 =  6              6
//   wetland           sedimentary   marsh      6 +  0 =  6              6
//   tundra            sedimentary   scrub      6 - 3 =  3               3
//   barren            barren        none       4 +  0 =  4              4
//   rocky             rocky         none       4 +  0 =  4              4
//   regolith          regolith      none       4 +  0 =  4              4
//   metallic          metallic      none       4 +  0 =  4              4
//   volcanic          volcanic      none       2 +  0 =  2              2
//   icy               icy           none       2 +  0 =  2              2
//   urban             (any)         urban     12 (pinned)              12
//   ocean             ocean         none       0 (pinned)               0
//
// Grassland/Forest/Wetland are the settlement-favoured ground (POPULATION.md);
// Volcanic/Icy carry TILES.md's lowest habitability ceiling; Ocean is exempt
// (can_place already refuses buildings there).
int non_extraction_stack_cap(terrain_substrate sub, terrain_cover cov)
{
    // Two pins that short-circuit the ground entirely, and both are real rather
    // than convenient. Water takes nothing. Urban is BL-366's terminal state, and
    // since Ben made urban a COVER (2026-08-21) it now sits on top of a substrate
    // that survives it — so the cap must come from the paving, not from the
    // geology the city happens to stand on.
    if (is_water(sub)) // BL-516: every water kind takes nothing
        return 0;
    if (cov == terrain_cover::urban)
        return 12; // Soft-bounded in practice by workforce contention, not this number.

    int base = 4;
    switch (sub)
    {
        case terrain_substrate::sedimentary: base = 6; break;
        case terrain_substrate::barren:
        case terrain_substrate::rocky:
        case terrain_substrate::regolith:
        case terrain_substrate::metallic:    base = 4; break;
        case terrain_substrate::volcanic:
        case terrain_substrate::icy:         base = 2; break;
        case terrain_substrate::ocean: // BL-516: unreachable — the is_water guard
        case terrain_substrate::coast: // above returns first; listed so the
        case terrain_substrate::lake:        return 0; // switch stays total.
    }

    // The cover's own contribution. Scrub costs 3 on soil because that pair IS the
    // old tundra row — thin, cold ground was never as buildable as the meadow next
    // to it, and the pre-split table said so with a 3. Everywhere else a cover is
    // something to clear rather than something that changes what the ground bears,
    // so it costs at most one slot and never takes the tile below 1.
    int mod = 0;
    switch (cov)
    {
        case terrain_cover::scrub:  mod = (sub == terrain_substrate::sedimentary) ? -3 : -1; break;
        case terrain_cover::forest: mod = (sub == terrain_substrate::sedimentary) ?  0 : -1; break;
        case terrain_cover::marsh:  mod = (sub == terrain_substrate::sedimentary) ?  0 : -1; break;
        case terrain_cover::snow:
        case terrain_cover::dunes:
        case terrain_cover::ash:    mod = -1; break;
        case terrain_cover::grass:
        case terrain_cover::salt:
        case terrain_cover::none:
        case terrain_cover::urban:  mod = 0; break;
    }
    return std::max(1, base + mod);
}

int non_extraction_buildings_on_tile(const world& w, entity_id tile_id)
{
    // BL-1205: inside a province_ceiling_scope, the scope's one building walk.
    if (const int scoped = scoped_non_extraction_buildings_on_tile(w, tile_id); scoped >= 0)
        return scoped;
    int n = 0;
    for (const auto& [bid, bc] : w.buildings)
    {
        if (bc.tile == tile_id && bc.type != building_type::extraction_site)
            ++n;
    }
    return n;
}

bool maybe_transform_to_urban(world& w, entity_id tile_id)
{
    const auto it = w.tiles.find(tile_id);
    if (it == w.tiles.end())
        return false;
    tile_component& tc = it->second;
    if (tc.cover == terrain_cover::urban || is_water(tc.substrate)) // BL-516
        return false;
    if (non_extraction_buildings_on_tile(w, tile_id) >= non_extraction_stack_cap(tc.substrate, tc.cover))
    {
        // THE TRANSFORM NOW WRITES ONE AXIS, AND THAT IS THE BUG BL-519 FIXED.
        // Before the split this line overwrote the composition, so paving a
        // metallic tile destroyed the fact that it was metallic — a slot conflict
        // that had already shipped. Urban is a COVER (Ben, 2026-08-21): the city
        // still erases what GREW here, and the geology underneath survives it.
        tc.cover         = terrain_cover::urban;
        tc.cover_density = k_cover_density_max; // paved is not "sparsely paved"
        // Urban carries the same High habitability ceiling as the
        // settlement-favoured ground it supersedes (grass/forest on sedimentary,
        // derive_environment in tile_generation.cpp) — established
        // infrastructure, not raw terrain, now bounds who can live here. Raised,
        // never lowered: marginal ground (scrub/volcanic/icy) that built
        // up to urban should not be stuck at its pre-transform ceiling.
        tc.habitability = std::max(tc.habitability, 0.80f);
        return true;
    }
    return false;
}

bool tile_has_site_under_construction(const world& w, entity_id tile_id)
{
    // Inside a province_ceiling_scope, the scope's one building walk.
    if (const int scoped = scoped_sites_on_tile(w, tile_id); scoped >= 0)
        return scoped > 0;
    for (const auto& [bid, bc] : w.buildings)
    {
        (void)bid;
        if (bc.tile == tile_id && bc.ticks_remaining > 0)
            return true;
    }
    return false;
}

int buildings_on_tile(const world& w, entity_id tile_id,
                      building_type type, resource_type target)
{
    // BL-1205: inside a province_ceiling_scope, the scope's one building walk.
    if (const int scoped = scoped_buildings_on_tile(w, tile_id, type, target); scoped >= 0)
        return scoped;
    int n = 0;
    for (const auto& [bid, bc] : w.buildings)
    {
        if (bc.tile != tile_id || bc.type != type)
            continue;
        // Extraction stacks are per-target: three iron sites and one petroleum site
        // on one tile are two separate stacks against two separate deposits.
        if (type == building_type::extraction_site && bc.target_resource != target)
            continue;
        ++n;
    }
    return n;
}

std::vector<entity_id> stack_members(const world& w, entity_id tile_id,
                                     building_type type, resource_type target)
{
    std::vector<entity_id> members;
    for (const auto& [bid, bc] : w.buildings)
    {
        if (bc.tile != tile_id || bc.type != type)
            continue;
        // Per-target, exactly as buildings_on_tile counts: three iron sites and one
        // petroleum site on one tile are two stacks against two deposits.
        if (type == building_type::extraction_site && bc.target_resource != target)
            continue;
        members.push_back(bid);
    }
    // world::buildings is an unordered_map — sorting is what makes "stored order"
    // mean anything at all, and it is the only thing standing between the decay
    // curve and a non-deterministic answer.
    std::sort(members.begin(), members.end());
    return members;
}

int stack_rank(const world& w, entity_id building_id)
{
    const auto it = w.buildings.find(building_id);
    if (it == w.buildings.end())
        return 0;
    const building_component&    b       = it->second;
    const std::vector<entity_id> members = stack_members(w, b.tile, b.type, b.target_resource);
    for (std::size_t i = 0; i < members.size(); ++i)
        if (members[i] == building_id)
            return static_cast<int>(i) + 1;
    return 0;
}

int prospective_stack_rank(const world& w, entity_id tile_id,
                           building_type type, resource_type target)
{
    return static_cast<int>(stack_members(w, tile_id, type, target).size()) + 1;
}

} // namespace placement_rules
