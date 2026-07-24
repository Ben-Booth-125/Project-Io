#include "world/placement_rules.hpp"

#include "world/world.hpp"

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
        case placement_reason::has_farm_affinity: return "Terrestrial farming already viable here — Hydroponics Bay fills the gap Farm can't reach";
    }
    return "Cannot build here";
}

bool is_ocean_tile(terrain_composition comp)
{
    return comp == terrain_composition::ocean;
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
    if (is_ocean_tile(tc.composition))
        return false;
    // Uninhabitable tiles (hazard-dominated, airless, etc.) are also excluded.
    return tc.habitability > 0.0f;
}

placement_result can_place_road(const tile_component& tc, std::uint8_t tier)
{
    // No road on water — roads are land infrastructure.
    if (is_ocean_tile(tc.composition))
        return placement_reason::ocean;
    // Upgrade-in-place (BL-172): a tile already carrying an equal-or-better road is a no-op — you
    // can raise a Track to a Road/Highway, but not re-lay the same or a lower tier. Reject rather
    // than silently re-charge (BL-147 rejected any road; the tier check generalises that).
    if (tc.road_level >= tier)
        return placement_reason::already_road;
    return placement_reason::ok;
}

placement_result can_place(const tile_component& tc, building_type type, resource_type target)
{
    // No building ever sits on water.
    if (is_ocean_tile(tc.composition))
        return placement_reason::ocean;

    switch (type)
    {
        case building_type::extraction_site:
            // Extraction must sit on a non-zero deposit of a prototype-extractable
            // target resource.
            if (is_extractable(target)
                && tc.resource_deposit[static_cast<std::size_t>(target)] > 0.0f)
                return placement_reason::ok;
            return placement_reason::no_deposit;

        case building_type::hydroponics_bay:
            // BL-166: valid on any tile LACKING the terrestrial farming affinity — the
            // logical inverse of Farm's own predicate (agricultural_produce deposit > 0),
            // so the two aren't both trivially valid everywhere. Not gated by era/tech.
            if (has_terrestrial_farm_affinity(tc))
                return placement_reason::has_farm_affinity;
            return placement_reason::ok;

        case building_type::processing_facility:
        case building_type::port:
        case building_type::inland_logistics_hub: // BL-149: any non-ocean land tile (the land-network node).
        case building_type::fishing_wharf:        // BL-168: coastal-adjacency is a world-level check (is_coastal); see can_place_in_world.
        case building_type::none:
        default:
            // Any non-ocean land tile is valid for non-extraction buildings.
            return placement_reason::ok;
    }
}

bool has_terrestrial_farm_affinity(const tile_component& tc)
{
    return tc.resource_deposit[static_cast<std::size_t>(resource_type::agricultural_produce)] > 0.0f;
}

bool is_coastal(const world& w, entity_id tile_id)
{
    const auto tc_it = w.tiles.find(tile_id);
    if (tc_it == w.tiles.end())
        return false;
    const tile_component& tc = tc_it->second;
    const entity_id body = tc.body;

    const auto body_it = w.bodies.find(body);
    if (body_it == w.bodies.end())
        return false;
    const int gw = body_it->second.grid_width;
    const int gh = body_it->second.grid_height;

    // Build a tile lookup: flat_index → entity_id for this body.
    // (Only for the body's tiles — most calls are short-circuit on first ocean hit.)
    // Odd-r offset neighbours (pointy-top hexes).
    static const int even_off[6][2] = {{+1,0},{0,-1},{-1,-1},{-1,0},{-1,+1},{0,+1}};
    static const int odd_off[6][2]  = {{+1,0},{+1,-1},{0,-1},{-1,0},{0,+1},{+1,+1}};
    const int (*off)[2] = (tc.grid_y & 1) ? odd_off : even_off;

    for (int n = 0; n < 6; ++n)
    {
        const int nrow = tc.grid_y + off[n][1];
        if (nrow < 0 || nrow >= gh)
            continue;
        int ncol = tc.grid_x + off[n][0];
        // Columns wrap (horizontal cylinder).
        if (ncol < 0) ncol += gw;
        else if (ncol >= gw) ncol -= gw;

        // Find the neighbour tile by scanning tiles on this body.
        // Linear scan is acceptable — this is a player-action path, not a hot loop.
        for (const auto& [nid, ntc] : w.tiles)
        {
            if (ntc.body == body && ntc.grid_x == ncol && ntc.grid_y == nrow)
            {
                if (is_ocean_tile(ntc.composition))
                    return true;
                break;
            }
        }
    }
    return false;
}

placement_result can_place_in_world(const world& w, entity_id tile_id,
                                    building_type type, resource_type target)
{
    const auto tc_it = w.tiles.find(tile_id);
    if (tc_it == w.tiles.end())
        return placement_reason::no_tile;
    // Tile-level terrain/deposit check first — propagate its specific reason.
    if (const placement_result tile_ok = can_place(tc_it->second, type, target); !tile_ok)
        return tile_ok;

    if (type == building_type::port || type == building_type::fishing_wharf)
    {
        // BL-168: Fishing Wharf reuses the same runtime hex-neighbour coastal predicate as
        // Port — no stored per-tile coastal flag, per the settled design.
        if (!is_coastal(w, tile_id))
            return placement_reason::not_coastal;
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

    return placement_reason::ok;
}

} // namespace placement_rules
