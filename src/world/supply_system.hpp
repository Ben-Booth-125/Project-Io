#pragma once

#include "recipe_registry.hpp"
#include "world.hpp"

#include <cstddef>
#include <unordered_map>
#include <unordered_set>

/// Advance every in-flight convoy by its speed increment. Convoys whose progress
/// reaches >= 1.0 have their `arrived` flag set; they are not yet retired here —
/// call credit_arrived_convoys after the market step to credit and remove them.
///
/// @param w  World; convoy progress fields are mutated in place.
void advance_convoys(world& w);

/// Credit and retire all arrived convoys: add cargo_qty of cargo_resource to the
/// destination (corp, body) pool, increase the destination market's supply for that
/// resource (so the next clearing pass reprices), and erase the convoy from
/// world.convoys. Called after clear_markets so the market supply injection takes
/// effect at the *next* tick's clearing pass.
///
/// Also **upserts a persistent trade_route** (BL-088) for each completed inter-body
/// lane before the convoy is erased: the unordered (source-body, dest-body) pair for
/// the convoy's corp gets `last_tick = tick` and its `convoy_count` incremented
/// (a new route is created on first traffic). Intra-body convoys (source and dest on
/// the same body) record nothing. Routes are never erased here — the commercial-sphere
/// fog (BL-089) ages them at read time.
///
/// @param w    World; pools, market supply, convoys, and trade_routes are mutated.
/// @param tick Current sim day tick, stamped onto routes as last-traffic time. The
///             default keeps pre-BL-088 callers (and pure market/pool tests) compiling.
void credit_arrived_convoys(world& w, int tick = 0);

/// Auto-dispatch convoys to fill shortfalls. For each (corp, body, resource) where
/// market demand exceeded supply in the last clearing pass (indicated by the market
/// demand field), search other (corp, body) pools on any body for a surplus of the
/// same resource and dispatch a convoy if one is found and affordable. Logistics
/// cost constants are passed in directly (loaded from economy.lua by the caller).
///
/// Space-mode convoys require a building_type::launchpad in the source corp's assets
/// on the source body. Land-mode is ungated. Sea mode is selected automatically when
/// the intra-body path crosses ocean (path.crosses_ocean — see docs/economy/SUPPLY.md);
/// air mode is not dispatched in the prototype.
///
/// @param w         World; convoys are appended and source pools debited.
/// @param reg       Registry (for building type lookups).
/// @param logistics_cost_land   base_cost_per_unit_distance for land mode.
/// @param logistics_cost_space  base_cost_per_unit_distance for space mode.
void dispatch_convoys(world& w, const recipe_registry& reg,
                      float logistics_cost_land, float logistics_cost_space);

// ---------------------------------------------------------------------------
// The shared dispatch (BL-452)
// ---------------------------------------------------------------------------
// dispatch_convoys above is TWO things bolted together: a shortfall scan that
// decides *what to haul from where*, and the dispatch itself — price the leg,
// commit the cargo, put a convoy on the lane. Only the first half is the
// auto-dispatcher's own opinion. The second half is what a convoy IS, and the
// player's `dispatch_convoy` verb (corp_command.hpp) needs exactly it with the
// scan removed.
//
// So it is factored out here rather than reimplemented there. There is no
// fourth code path: `dispatch_convoys` and `apply_corp_command` call the same
// two functions with the same arguments, so a player's convoy and a rival's of
// the same shape cost the same and travel at the same speed — an assertion
// tools/verify/convoy_command.cpp makes directly, because a silent divergence
// here is exactly the bug a copy would introduce.

/// BL-148/149 logistics-node lookups. `pop_tile_scale` maps a population
/// centre's tile to its scale (tier 1–5 — cities are free hubs); `hub_tiles`
/// holds every completed, active inland_logistics_hub's tile. An intra-body
/// haul is discounted for each such node its A* path crosses.
///
/// Built once per auto-dispatch pass (it is a walk of every building), and once
/// per player command — a single press can afford the walk.
struct logistics_nodes
{
    std::unordered_map<entity_id, int> pop_tile_scale;
    std::unordered_set<entity_id>      hub_tiles;
};

logistics_nodes collect_logistics_nodes(const world& w);

/// One priced candidate leg: what hauling `qty` of resource index `ri` from
/// `src_body` to `dest_market_id` would cost, in credits and in econ ticks.
struct convoy_leg
{
    /// False when the lane cannot be flown at all — no production anchor, no
    /// reachable path, no launchpad on the source body, no propellant to launch
    /// with, or a cost that is not a finite number. Nothing was mutated.
    bool        viable       = false;
    convoy_mode mode         = convoy_mode::land;
    float       cost         = 0.0f; ///< Total credits the haul costs (already node-discounted).
    int         travel_ticks = 1;    ///< Econ ticks the leg takes; convoy speed is 1/this.
};

/// Price one leg. A pure read of the world apart from the A* path cache, which
/// is why `w` is non-const. Mutates no game state and creates nothing.
///
/// @param nodes                From collect_logistics_nodes; the intra-body discount source.
/// @param ri                   Resource index; out-of-range answers `viable = false`.
/// @param qty                  Units of cargo. Non-finite or non-positive answers `viable = false`.
/// @param logistics_cost_space Space-lane base cost per unit distance per unit cargo. Every
///                             caller passes `reg.logistics_cost(convoy_mode::space)`; it stays a
///                             parameter only because `dispatch_convoys` has always taken it.
convoy_leg price_convoy_leg(world& w, const recipe_registry& reg,
                            const logistics_nodes& nodes, entity_id corp_id,
                            entity_id src_body, entity_id dest_market_id,
                            std::size_t ri, float qty, float logistics_cost_space);

/// Commit a priced leg: debit the corp's balance and its source pool, burn the
/// launch's propellant on the space lane, and append the convoy. The ONE place
/// a `convoy_component` is created.
///
/// All-or-nothing. Returns false — having mutated nothing — when the leg is not
/// viable or the corp cannot afford `leg.cost`. The propellant availability
/// gate lives in `price_convoy_leg`, so a viable space leg cannot drive the
/// propellant pool negative here.
bool commit_convoy(world& w, entity_id corp_id, entity_id src_body,
                   entity_id dest_market_id, std::size_t ri, float qty,
                   const convoy_leg& leg);
