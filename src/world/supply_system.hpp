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
/// @param w  World; pools, market supply, and convoys vector are mutated.
void credit_arrived_convoys(world& w);

/// Auto-dispatch convoys to fill shortfalls. For each (corp, body, resource) where
/// market demand exceeded supply in the last clearing pass (indicated by the market
/// demand field), search other (corp, body) pools on any body for a surplus of the
/// same resource and dispatch a convoy if one is found and affordable. Logistics
/// cost constants are passed in directly (loaded from economy.lua by the caller).
///
/// Space-mode convoys require a building_type::launchpad in the source corp's assets
/// on the source body. Land-mode is ungated. Sea and air modes are not dispatched
/// in the prototype (no ports/airfields gating logic yet).
///
/// @param w         World; convoys are appended and source pools debited.
/// @param reg       Registry (for building type lookups).
/// @param logistics_cost_land   base_cost_per_unit_distance for land mode.
/// @param logistics_cost_space  base_cost_per_unit_distance for space mode.
void dispatch_convoys(world& w, const recipe_registry& reg,
                      float logistics_cost_land, float logistics_cost_space);
