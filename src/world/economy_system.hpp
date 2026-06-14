#pragma once

#include "components.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <array>
#include <map>
#include <utility>
#include <vector>

/// Per-building observability record produced by one economy step. Read by the
/// economy panel (idle/active state, output rate, limiting input) — held by the
/// app between econ ticks and not otherwise persisted.
struct building_report
{
    entity_id     building = null_entity;
    entity_id     corp     = null_entity;
    entity_id     body     = null_entity;
    building_type type     = building_type::none;

    resource_type target_resource = resource_type::iron_ore; ///< Extraction only.
    uint16_t      recipe          = no_recipe;                ///< Processing only.

    bool  active = false; ///< Produced output this tick.
    bool  idle   = false; ///< Produced nothing (no workforce / no deposit / below t_idle / misconfigured).
    float output_quantity = 0.0f; ///< Units credited to the pool this tick (sum of outputs for a processor).

    bool          has_limiting   = false;               ///< Processing: a binding input exists.
    resource_type limiting_input = resource_type::iron_ore; ///< Processing: the scarcest input (pool-relative).
};

/// Result of one economy step: the per-building reports plus the auto-bought
/// input shortfalls per (corp, body), which become market demand and corporate
/// expenditure downstream (market_clearing.hpp / budget_system.hpp).
struct economy_report
{
    std::vector<building_report> buildings;

    /// Per (corporation, body): the input quantities a processor could not cover
    /// from its own pool and auto-bought from the market this tick. Resource-indexed.
    std::map<std::pair<entity_id, entity_id>, std::array<float, resource_count>> purchases;
};

/// Run one economy step over every corporation's buildings: extraction credits
/// the (corp, body) pool with its target resource; processing runs its recipe
/// pool-first under the two-threshold partial-run model (registry t_full/t_idle),
/// accruing outputs and recording the auto-bought shortfall. Deposits never
/// deplete (tile resource_remaining is not touched). Deterministic: corporations
/// are visited in ascending id order, assets in their stored order.
///
/// @param w   World; the (corp, body) pools are mutated in place.
/// @param reg Loaded recipe/economy registry.
/// @return    The step report (building states + auto-bought shortfalls).
economy_report run_economy_step(world& w, const recipe_registry& reg);
