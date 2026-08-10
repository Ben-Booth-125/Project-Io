#include "world/placement_rules.hpp"

#include "world/logistics.hpp"
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
            // Fishing Wharf (BL-168): an agricultural_produce site with no terrestrial
            // deposit can still work a coastal tile. That needs world access
            // (is_coastal), so this tile-only check passes it through rather than
            // rejecting on no_deposit — can_place_in_world enforces the coastal gate.
            if (target == resource_type::agricultural_produce)
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
        case building_type::none:
        default:
            // Any non-ocean land tile is valid for non-extraction buildings.
            return placement_reason::ok;
    }
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

        if (raster != nullptr)
        {
            const entity_id nid = (*raster)[static_cast<std::size_t>(nrow)
                                                * static_cast<std::size_t>(gw)
                                            + static_cast<std::size_t>(ncol)];
            if (nid == null_entity)
                continue;
            const auto nit = w.tiles.find(nid);
            if (nit != w.tiles.end() && is_ocean_tile(nit->second.composition))
                return true;
            continue;
        }

        // Fallback: find the neighbour by scanning tiles on this body.
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
                                    building_type type, resource_type target,
                                    float max_reach)
{
    const auto tc_it = w.tiles.find(tile_id);
    if (tc_it == w.tiles.end())
        return placement_reason::no_tile;
    // Tile-level terrain/deposit check first — propagate its specific reason.
    if (const placement_result tile_ok = can_place(tc_it->second, type, target); !tile_ok)
        return tile_ok;

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

    // Stack ceiling: a tile carries several sites working one deposit, but not
    // without limit. Checked last — a full stack is the least interesting reason
    // to refuse, and the terrain reasons above teach the player more.
    if (buildings_on_tile(w, tile_id, type, target)
        >= stack_capacity(tc_it->second, type, target))
        return placement_reason::slot_full;

    return placement_reason::ok;
}

// --- Building stacks (BL-193; the rule and both constants live in the header) --

float stack_output_scalar(int rank)
{
    // Repeated multiplication, not std::pow: the same product in the same order on
    // every platform, so a stacked site's yield cannot drift a save out of sync.
    float s = 1.0f;
    for (int k = 1; k < rank; ++k)
        s *= k_stack_output_decay;
    return s;
}

int stack_capacity(const tile_component& tc, building_type type, resource_type target)
{
    if (type != building_type::extraction_site)
        return 1;

    const float richness = tc.resource_deposit[static_cast<std::size_t>(target)];
    const int   cap      = static_cast<int>(richness / k_richness_per_site);
    return cap < 1 ? 1 : cap;
}

int buildings_on_tile(const world& w, entity_id tile_id,
                      building_type type, resource_type target)
{
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

} // namespace placement_rules
