#pragma once

#include "../world/economy_system.hpp" // economy_report (per-corp budget breakdown)
#include "../world/world.hpp"
#include "economy_panel.hpp"            // player_plot_history (profit-chart series)
#include "ui_state.hpp"                 // ui_state (stubbed Tax/Wages tier state)

#include <unordered_map>
#include <utility>
#include <vector>

class recipe_registry; // building-profit estimate for the rank table

namespace ui {

/// Player buildings ranked by estimated net profit per tick, descending (BL-171).
/// Shared by the Budget ledger (its current ranking) and app (per-econ-tick snapshots
/// that feed the ledger's rank-change column). Buildings the report hasn't touched are
/// omitted. Pairs are {building entity, net profit/tick}.
std::vector<std::pair<entity_id, float>>
rank_player_buildings_by_profit(const world& w, const recipe_registry& reg,
                                const economy_report& report);

/// Draws the Budget ledger window (nav rail "Budget"; open controls visibility).
///
/// Redesigned to Ben's 2026-07-11 mockup (BL-171): a corp-name header, a **profit
/// line chart** over recent econ ticks (which replaces the former itemised cashflow
/// table — that detail is intentionally dropped here, to reappear in a dedicated
/// breakdown menu later), stubbed **Tax** and **Wages** tier controls (the player
/// policy levers designed under BL-155 — drawn and selectable but with no economic
/// effect yet), an **Assets** summary (buildings owned / income / cargo value), and a
/// **top-8 buildings-by-profit** table with a rank-change-vs-a-year-ago column.
///
/// @param w           Read-only world (player corporation, pools, markets, buildings).
/// @param reg         Loaded registry — per-building profit estimate for the rank table.
/// @param report      Latest economy report — budget breakdown + per-building output.
/// @param history     Player balance/income/expenditure series; profit per tick =
///                    income − expenditure drives the chart.
/// @param prior_rank  Player-building ranking (entity → rank) as of 4 econ ticks ago,
///                    kept by app; drives the rank-change column (empty → all "new").
/// @param ui          UI state; the stubbed Tax/Wages tier selectors read/write it.
/// @param open        Open/closed flag; the window's close button sets it to false.
void draw_balance_ledger(const world& w, const recipe_registry& reg,
                         const economy_report& report, const player_plot_history& history,
                         const std::unordered_map<entity_id, int>& prior_rank,
                         ui_state& ui, bool& open);

} // namespace ui
