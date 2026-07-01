#pragma once

#include "components.hpp"
#include "economy_system.hpp" // economy_report.workforce_contention
#include "market_clearing.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <map>
#include <unordered_map>
#include <utility>

/// Debt interest charged per economy tick on a negative balance (BL-073). An
/// economy tick is one quarter (k_ticks_per_year = 4), so this is the per-quarter
/// rate: ~2 %/qtr ≈ 8 %/yr. A negative balance compounds once per tick by this
/// factor; a non-negative balance is never charged. Single source of truth — both
/// the live budget loop and the econ_bankruptcy harness read this constant, so they
/// can never drift. Interest is a pure function of balance × rate (deterministic).
inline constexpr float k_debt_interest_per_quarter = 0.02f;

/// Apply one economy tick's money loop to every corporation balance:
///   balance += income − expenditure − maintenance − wages
/// where income/expenditure are the market cash flows (goods sold / inputs bought
/// at base_price), maintenance is each building's flat per-tick upkeep, and wages
/// are `effective_workforce × base_wage` per building, where effective workforce is
/// the requested target throttled by the (corp, body) labour-pool contention scalar
/// (a building pays for the labour it actually used, not its target). Balances may go
/// negative (no insolvency consequence in the prototype).
///
/// @param w          World; corporation balances are mutated.
/// @param reg        Loaded registry (maintenance / wage constants per building type).
/// @param flows      Per-corporation market cash flow from clear_markets().
/// @param contention Per-(corp, body) workforce contention from run_economy_step
///                   (`economy_report.workforce_contention`); absent keys read as 1.0.
/// @param breakdown  Optional sink (BL-072): when non-null, receives the per-corp
///                   four-flow split (income / expenditure / maintenance / wages,
///                   plus the BL-073 interest line) whose net() equals the delta
///                   applied to that corp's balance. Null for the headless harnesses,
///                   which only need the balance mutation.
void apply_budget(world& w,
                  const recipe_registry& reg,
                  const std::unordered_map<entity_id, corp_cash_flow>& flows,
                  const std::map<std::pair<entity_id, entity_id>, float>& contention,
                  std::map<entity_id, corp_budget>* breakdown = nullptr);
