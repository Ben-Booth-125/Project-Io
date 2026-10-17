#include "road_tool.hpp"

#include "world/logistics.hpp"        // road_edge_neighbour — the side's other end
#include "world/placement_rules.hpp"  // can_place_road — THE gate, asked not copied

namespace ui
{

std::vector<road_target> road_crossing_targets(const world& w, entity_id tile,
                                               std::uint8_t tier)
{
    std::vector<road_target> out;
    if (w.tiles.find(tile) == w.tiles.end())
        return out;
    out.reserve(4); // at most four sides a cardinal walk can cross
    for (int s = 0; s < 6; ++s)
    {
        // ONE QUESTION, ASKED OF THE RULES. `can_place_road` refuses a side no
        // cost walk can cross, a side with no neighbour, a span with water at both
        // ends, and a crossing already at or above this tier — four conditions
        // this file deliberately does not restate.
        if (!placement_rules::can_place_road(w, tile, s, tier))
            continue;
        const entity_id other = road_edge_neighbour(w, tile, s);
        if (other == null_entity)
            continue; // can_place_road already refuses this; belt and braces
        out.push_back({s, other});
    }
    return out;
}

} // namespace ui
