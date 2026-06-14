#pragma once

#include "components.hpp"
#include "economy_system.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <unordered_map>
#include <vector>

/// A player-authored sell order — the Layer 3 framework hook for the manual side
/// of the market. The full authoring UI (what / how much / floor price) lands in
/// the new Layer 4; in Layer 3 no orders are issued and the auto-supply path runs.
struct sell_order
{
    entity_id     corp     = null_entity;
    entity_id     body     = null_entity;
    resource_type resource = resource_type::iron_ore;
    float         quantity = 0.0f;
    float         floor_price = 0.0f; ///< Minimum acceptable unit price; ignored while price == base_price.
};

/// Per-corporation cash-flow figures from one market clearing, valued at
/// base_price. The balance arithmetic (these, less maintenance and wages) is the
/// budget step's job (budget_system.hpp).
struct corp_cash_flow
{
    float income      = 0.0f; ///< Goods sold × base_price.
    float expenditure = 0.0f; ///< Inputs auto-bought × base_price.
};

/// Clear every body market for one economy tick. For each market body:
///   - supply  = each corp's pool surplus above its own processors' next-run input
///               need, listed for sale and sold at base_price;
///   - demand  = the processor input shortfalls auto-bought this tick (from the
///               economy report), bought at base_price.
/// Market supply/demand are recomputed from scratch each tick; price stays at
/// base_price (price resolution is deferred). Pools are debited for sales.
///
/// @param w             World; markets and (corp, body) pools are mutated.
/// @param reg           Loaded registry (for processor input reservations).
/// @param report        The economy step report (its purchases drive demand).
/// @param player_orders Optional player sell orders (empty in Layer 3 — the hook).
/// @return              Per-corporation cash flow valued at base_price.
std::unordered_map<entity_id, corp_cash_flow> clear_markets(
    world& w,
    const recipe_registry& reg,
    const economy_report& report,
    const std::vector<sell_order>& player_orders = {});
