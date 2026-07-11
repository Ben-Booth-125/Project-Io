#pragma once

#include "../world/economy_system.hpp" // economy_report (per-corp budget breakdown)
#include "../world/world.hpp"
#include "economy_panel.hpp"            // player_plot_history (profit-chart series)
#include "ui_state.hpp"                 // ui_state (stubbed Tax/Wages tier state)

namespace ui {

/// Draws the Budget ledger window (nav rail "Budget"; open controls visibility).
///
/// Redesigned to Ben's 2026-07-11 mockup (BL-171): a corp-name header, a **profit
/// line chart** over recent econ ticks (which replaces the former itemised cashflow
/// table — that detail is intentionally dropped here, to reappear in a dedicated
/// breakdown menu later), stubbed **Tax** and **Wages** tier controls (the player
/// policy levers designed under BL-155 — drawn and selectable but with no economic
/// effect yet), an **Assets** summary (buildings owned / income / cargo value), and a
/// **placeholder** top-buildings-by-profit rank table.
///
/// @param w        Read-only world (player corporation, pools, markets).
/// @param report   Latest economy report — per-corp budget breakdown (income for the
///                 Assets summary).
/// @param history  Player balance/income/expenditure series (oldest→newest, one per
///                 econ tick); profit per tick = income − expenditure drives the chart.
/// @param ui       UI state; the stubbed Tax/Wages tier selectors read/write it.
/// @param open     Open/closed flag; the window's close button sets it to false.
void draw_balance_ledger(const world& w, const economy_report& report,
                         const player_plot_history& history, ui_state& ui, bool& open);

} // namespace ui
