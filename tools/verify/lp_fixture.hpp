// lp_fixture.hpp - BL-1361: what a hand-built trade fixture needs now that a shipment
// draws Logistic Points again (LOGISTICS.md § How a tile's points get to it, "A
// shipment draws points again"). A shipment draws at its source market's centre,
// from the pool of the NATION whose ground that is, and only where a running source
// covers it. Fixtures that build a bare grid have no nation, so every shipment would
// be refused for want of points - which is the rule working, not the subject those
// fixtures test. This stages the smallest honest world in which the rule admits them:
// one nation (no capital, so every source passes the chain test) holding the body's
// unowned ground, and a running road hub on each tile named. Name hub tiles OFF a
// haul's path where the fixture prices one: a hub on the path discounts the haul.
#pragma once

#include "world/components.hpp"
#include "world/logistics.hpp" // body_tile_grid
#include "world/world.hpp"

#include <initializer_list>

namespace lp_fixture
{

/// Give every unowned tile of @p body to one new capital-less nation, and put a
/// running inland logistics hub (a road source, `lp_source_points[0]`) on each of
/// @p hub_tiles - @p copies of them on each, for a fixture whose subject is not the
/// Logistic Point cap and which must never meet it. Returns the nation.
inline entity_id stage(world& w, entity_id body, std::initializer_list<entity_id> hub_tiles,
                       int copies = 1)
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.name = "LP fixture";
    for (const entity_id t : body_tile_grid(w, body))
        if (t != null_entity && w.tile_to_nation.count(t) == 0)
        {
            nc.tiles.push_back(t);
            w.tile_to_nation[t] = n;
        }
    w.nations[n] = nc;
    for (const entity_id t : hub_tiles)
        for (int k = 0; k < copies; ++k)
        {
            const entity_id hub = w.create_entity();
            building_component hb{};
            hb.type            = building_type::inland_logistics_hub;
            hb.tile            = t;
            hb.ticks_remaining = 0;
            w.buildings[hub]   = hb;
        }
    return n;
}

} // namespace lp_fixture
