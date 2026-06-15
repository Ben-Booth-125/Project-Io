#pragma once

#include "components.hpp"
#include "economy_system.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <unordered_map>
#include <vector>

// `sell_order` (the manual market side) is defined in components.hpp so both the
// UI state and this clearing system can name it without an include cycle.

/// Per-corporation cash-flow figures from one market clearing, valued at the
/// price resolved this tick (base_price modulated by supply/demand). The balance
/// arithmetic (these, less maintenance and wages) is the budget step's job
/// (budget_system.hpp).
struct corp_cash_flow
{
    float income      = 0.0f; ///< Goods sold × resolved price.
    float expenditure = 0.0f; ///< Inputs auto-bought × resolved price.
};

/// Clear every body market for one economy tick. For each market body:
///   - supply  = each corp's pool surplus above its own processors' next-run input
///               need, listed for sale;
///   - demand  = the processor input shortfalls auto-bought this tick (from the
///               economy report).
/// Market supply/demand are recomputed from scratch each tick; price then resolves
/// from the supply/demand ratio (base_price × sqrt(demand/supply), clamped and
/// EMA-eased) and every sale/purchase is valued at that resolved price. Pools are
/// debited for sales.
///
/// @param w             World; markets (supply/demand/price) and (corp, body) pools are mutated.
/// @param reg           Loaded registry (for processor input reservations).
/// @param report        The economy step report (its purchases drive demand).
/// @param player_orders Optional player sell orders (empty in Layer 3 — the hook).
/// @return              Per-corporation cash flow valued at the resolved price.
std::unordered_map<entity_id, corp_cash_flow> clear_markets(
    world& w,
    const recipe_registry& reg,
    const economy_report& report,
    const std::vector<sell_order>& player_orders = {});
