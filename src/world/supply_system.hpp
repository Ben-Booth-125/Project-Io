#pragma once

#include "recipe_registry.hpp"
#include "world.hpp"

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
