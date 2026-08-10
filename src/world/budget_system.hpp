#pragma once

#include "components.hpp"
#include "economy_system.hpp" // economy_report.workforce_contention
#include "market_clearing.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

/// Debt interest charged per economy tick on a negative balance (BL-073). An
/// economy tick is one quarter (k_ticks_per_year = 4), so this is the per-quarter
/// rate: ~2 %/qtr ≈ 8 %/yr. A negative balance compounds once per tick by this
/// factor; a non-negative balance is never charged. Single source of truth — both
/// the live budget loop and the econ_bankruptcy harness read this constant, so they
/// can never drift. Interest is a pure function of balance × rate (deterministic).
inline constexpr float k_debt_interest_per_quarter = 0.02f;

/// Per-building operating cost for one tick (BL-074): the maintenance and wage
/// components apply_budget charges. Extracted as the single source of the formula so
/// the per-building profitability estimate (building_profit.hpp) computes the same
/// numbers the live budget loop does — no drift.
struct building_opex
{
    float maintenance = 0.0f;
    float wages       = 0.0f;
};

/// Compute a building's maintenance + wages for one tick. `contention_scalar` is the
/// (corp, body) labour throttle (1.0 uncontended); `mean_hab` is the body's mean
/// population habitability (clamped to [0.1, 2.0] internally, exactly as the budget
/// loop does). Decommissioned buildings pay only fixed material maintenance, no wages.
building_opex compute_building_opex(const building_component& b,
                                    const building_economics& e,
                                    float contention_scalar,
                                    float mean_hab);

/// Mean population-centre habitability on a body (BL-074 helper): the average of every
/// population centre's habitability whose tile sits on `body`, or 1.0 when the body has
/// none. Unclamped — callers clamp as needed (compute_building_opex does).
float body_mean_habitability(const world& w, entity_id body);

/// Apply one economy tick's money loop to every corporation balance:
///   balance += income − expenditure − maintenance − wages
/// where income/expenditure are the market cash flows (goods sold / inputs bought
/// at base_price), maintenance is each building's flat per-tick upkeep, and wages
/// are `effective_workforce × base_wage` per building, where effective workforce is
/// the requested target throttled by the (corp, body) labour-pool contention scalar
/// (a building pays for the labour it actually used, not its target). Balances may go
/// negative; a negative balance then accrues debt interest (BL-073).
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
/// @param production Optional (BL-343): this tick's per-building reports, from
///                   `economy_report::buildings`. The LAW ENFORCEMENT SEAM — the
///                   extraction levy is a per-unit charge on raw output, so it is
///                   charged where the flow is ACCOUNTED (here) rather than where
///                   the price is RESOLVED (clear_markets): a law is a modifier
///                   over the market, never an override of it (law.hpp). Null
///                   means no levy is charged, which is exactly what the
///                   pre-BL-343 arithmetic did — so every existing caller that
///                   omits it keeps a bit-identical balance.
void apply_budget(world& w,
                  const recipe_registry& reg,
                  const std::unordered_map<entity_id, corp_cash_flow>& flows,
                  const std::map<std::pair<entity_id, entity_id>, float>& contention,
                  std::map<entity_id, corp_budget>* breakdown = nullptr,
                  const std::vector<building_report>* production = nullptr);
