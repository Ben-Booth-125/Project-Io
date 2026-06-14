#pragma once

#include "components.hpp"
#include "market_clearing.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <unordered_map>

/// Apply one economy tick's money loop to every corporation balance:
///   balance += income − expenditure − maintenance − wages
/// where income/expenditure are the market cash flows (goods sold / inputs bought
/// at base_price), maintenance is each building's flat per-tick upkeep, and wages
/// are `workforce_assigned × base_wage` per building. Balances may go negative
/// (no insolvency consequence in the prototype).
///
/// @param w     World; corporation balances are mutated.
/// @param reg   Loaded registry (maintenance / wage constants per building type).
/// @param flows Per-corporation market cash flow from clear_markets().
void apply_budget(world& w,
                  const recipe_registry& reg,
                  const std::unordered_map<entity_id, corp_cash_flow>& flows);
