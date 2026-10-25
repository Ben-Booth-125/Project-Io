#pragma once

#include "world/entity.hpp"
#include "world/logistic_points.hpp"

#include <vector>

struct world;

// ---------------------------------------------------------------------------
// The Logistics lens's RAIL AND SEA LEGS (Ben, 2026-10-25: "a lens that shows each
// chain link" and "points on lanes and rail"; LENSES.md § Logistics lens, "Every link
// of the chain").
//
// `lp_route_flow` (world/logistic_points) lays a request's share on every tile of its
// walk, and for a ROAD source that walk is the road itself. A railway station's field
// is point to point and a port's crosses water, so the flow names only the two ENDS of
// a rail or sea link - and two groups of the nation joined by a line or a crossing read
// as cut apart on the map. This module recovers those links as tile runs the lens can
// colour and number:
//
//   - THE CHAIN AS A TREE, repeated exactly as `lp_route_flow` step 1 repeats
//     `memo_chain`'s closure (the same rounds, ascending building id, the same two
//     tests in the same order), recording the link that admitted each source and the
//     source it hangs from.
//   - THE LEG'S TILES: a rail link runs along the track (`rail_leg_path`, station to
//     station); a sea link runs down the port's own field from the far end to the port
//     (each step to the six-neighbour holding the least distance, the lowest tile id on
//     a tie) - the LANE tiles, sea or lake, since a port's field is `lp_lane_walk`.
//   - THE LEG'S TRAFFIC: every producing building's request routed as the resolver
//     routes it (its qualifying sources nearest first, a tie to the lower id, topped up
//     until covered), each source it draws on carrying its share up its path to the
//     capital - so a leg carries exactly the shares that `lp_route_flow` lays on its
//     two ends because of it, and nothing that only passes one end.
//
// A rail line between two connected stations that the tree did not take is a leg too,
// carrying nothing: CONNECTED, IDLE, which is the read the ruling asked for.
//
// A derived READ for the lens - pure, deterministic (every walk in ascending id), never
// serialised, never read by the simulation. It reads the per-source fields the resolve
// itself routed on, through the caller's memo and the world's sea-field cache.
// ---------------------------------------------------------------------------

namespace ui {

/// One rail or sea link of a nation's chain to its capital, as tiles.
struct lp_leg
{
    lp_network             network   = lp_network::rail; ///< rail or sea (a road link is its road)
    std::vector<entity_id> tiles;                        ///< the run, end to end (both ends included)
    float                  requested = 0.0f; ///< LP/tick: the routed requests crossing it
    float                  granted   = 0.0f; ///< LP/tick: what the resolver granted them
    float                  available = 0.0f; ///< LP/tick: the carrying source's points x its reach across the leg
    bool                   in_tree   = false; ///< the link the chain joined by (false: an idle rail alternative)
};

/// The rail and sea legs of @p nation's chain on @p body, read off @p t (this tick's
/// resolve) through @p memo - which the caller has already used for this nation and
/// body (`lp_route_flow`), so every field is a cache hit. Empty when the nation has no
/// capital on the body (no chain to draw).
std::vector<lp_leg> lp_chain_legs(world& w, lp_reach_memo& memo, const lp_tick& t,
                                  entity_id nation, entity_id body);

} // namespace ui
