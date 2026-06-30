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

    bool  active    = false; ///< Produced output this tick.
    bool  idle      = false; ///< Produced nothing (no workforce / no deposit / below t_idle / misconfigured).
    bool  exhausted = false; ///< Extraction only: the tile's deposit reserve is spent ("out of resources").
    float output_quantity = 0.0f; ///< Units credited to the pool this tick (sum of outputs for a processor).

    /// Labour actually applied: `workforce_assigned × contention scalar` for the
    /// building's (corp, body). Equals `workforce_assigned` when labour is uncontended.
    float effective_workforce = 0.0f;

    bool          has_limiting   = false;               ///< Processing: a binding input exists.
    resource_type limiting_input = resource_type::iron_ore; ///< Processing: the scarcest input (pool-relative).
};

/// Per-corporation budget breakdown retained from the budget step (BL-072): the
/// flows that net to the balance delta applied this tick. `income`/`expenditure`
/// are the market cash flows (corp_cash_flow); `maintenance`/`wages` are the
/// operating costs apply_budget computes per building; `interest` is the BL-073
/// debt charge (0 until a corp's balance is negative). `net()` is exactly the
/// delta added to the balance this tick. Populated only when apply_budget is given
/// a breakdown sink — the headless harnesses leave it empty.
struct corp_budget
{
    float income      = 0.0f;
    float expenditure = 0.0f;
    float maintenance = 0.0f;
    float wages       = 0.0f;
    float interest    = 0.0f; ///< BL-073: charged only while balance < 0.

    /// The per-tick balance delta: income less every outflow.
    float net() const { return income - expenditure - maintenance - wages - interest; }
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

    /// Per (corporation, body): the workforce contention scalar applied this tick —
    /// `min(1, supply/demand)`. 1.0 when the corp's labour demand on that body fits
    /// its pool supply; below 1.0 every building on that (corp, body) is throttled
    /// proportionally (docs/economy/POPULATION.md § Workforce model, step 1). Read by
    /// the budget step (wages on effective workforce) and the economy panel.
    std::map<std::pair<entity_id, entity_id>, float> workforce_contention;

    /// Per-body mean habitability aggregate (BL-048): weighted average of all
    /// population-centre tiles on the body. 0.0 = uninhabitable, 1.0 = full.
    std::map<entity_id, float> body_habitability;

    /// Per-corporation budget breakdown for this tick (BL-072): the four+1 flows
    /// netting to the balance delta. Populated by apply_budget when given a
    /// breakdown sink (app's live loop); empty under the headless harnesses, which
    /// do not request it. Read by the Balance Ledger / Economy panel.
    std::map<entity_id, corp_budget> budgets;
};

/// Inject nation-substrate background supply and demand into body markets.
/// Called from clear_markets after the per-tick supply/demand reset, before
/// the order-book pass, so substrate is additive to the market quantities.
///
/// @param w World; market supply/demand arrays are mutated in place.
void inject_substrate_demand(world& w);

/// Run one economy step over every corporation's buildings: extraction credits
/// the (corp, body) pool with its target resource and draws the same amount from
/// the tile's finite `resource_remaining` reserve (tapering as it nears empty,
/// then reporting the building exhausted); processing runs its recipe pool-first
/// under the two-threshold partial-run model (registry t_full/t_idle), accruing
/// outputs and recording the auto-bought shortfall. Deterministic: corporations
/// are visited in ascending id order, assets in their stored order.
///
/// @param w   World; the (corp, body) pools are mutated in place.
/// @param reg Loaded recipe/economy registry.
/// @return    The step report (building states + auto-bought shortfalls).
economy_report run_economy_step(world& w, const recipe_registry& reg);
