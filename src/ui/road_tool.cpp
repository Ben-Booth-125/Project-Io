#include "road_tool.hpp"

#include "world/logistics.hpp"        // road_edge_neighbour — the side's other end
#include "world/placement_rules.hpp"  // can_place_road — THE gate, asked not copied

namespace ui
{

std::vector<road_target> road_crossing_targets(const world& w, entity_id tile,
                                               std::uint8_t tier, entity_id actor)
{
    std::vector<road_target> out;
    if (w.tiles.find(tile) == w.tiles.end())
        return out;
    out.reserve(4); // at most four sides a cardinal walk can cross
    for (int s = 0; s < 6; ++s)
    {
        // ONE QUESTION, ASKED OF THE RULES. `can_place_road` refuses a side no
        // cost walk can cross, a side with no neighbour, a span with water at both
        // ends, a crossing already at or above this tier, a tile carrying no road to
        // build out of (BL-1348) and ground another nation holds (BL-1357) — six
        // conditions this file deliberately does not restate.
        if (!placement_rules::can_place_road(w, tile, s, tier, actor))
            continue;
        const entity_id other = road_edge_neighbour(w, tile, s);
        if (other == null_entity)
            continue; // can_place_road already refuses this; belt and braces
        out.push_back({s, other});
    }
    return out;
}

placement_rules::placement_reason road_crossing_refusal(const world& w, entity_id tile,
                                                        std::uint8_t tier, entity_id actor)
{
    using reason = placement_rules::placement_reason;
    if (w.tiles.find(tile) == w.tiles.end())
        return reason::no_tile;
    // ONE WALK, THE SAME GATE. The rules are asked side by side and the answers RANKED,
    // rather than the first one winning: a tile usually refuses for several reasons at
    // once, and which of them the player can act on differs. Foreign ground is first
    // because no amount of money or upgrading opens it; a tile with no road to build out
    // of is next, because the player fixes it elsewhere; "already at this tier" is last,
    // because it is the benign one.
    bool foreign = false, no_road = false, already = false, any_side = false;
    for (int s = 0; s < 6; ++s)
    {
        const placement_rules::placement_result r =
            placement_rules::can_place_road(w, tile, s, tier, actor);
        if (r)
            return reason::ok; // the set is not empty after all
        switch (r.reason)
        {
            case reason::foreign_ground:    foreign  = true; any_side = true; break;
            case reason::no_road_to_extend: no_road  = true; any_side = true; break;
            case reason::already_road:      already  = true; any_side = true; break;
            default: break; // ocean: an uncrossable side or no neighbour that way
        }
    }
    if (foreign)  return reason::foreign_ground;
    if (no_road)  return reason::no_road_to_extend;
    if (already)  return reason::already_road;
    return any_side ? reason::already_road : reason::ocean;
}

} // namespace ui
