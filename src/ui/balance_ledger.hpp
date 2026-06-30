#pragma once

#include "../world/economy_system.hpp" // economy_report (per-corp budget breakdown)
#include "../world/world.hpp"

#include <vector>

namespace ui {

/// Draws the Balance Ledger window. open controls visibility (toggled by nav rail).
///
/// @param w        Read-only world (corporation balances and names).
/// @param report   Latest economy report — its per-corp budget breakdown (BL-072)
///                 drives the itemised cashflow table.
/// @param balance_history  Player balance series (oldest→newest, one per econ
///                 tick); the smoothed trailing net drives the projected runway.
/// @param open     Open/closed flag; the window's close button sets it to false.
void draw_balance_ledger(const world& w, const economy_report& report,
                         const std::vector<float>& balance_history, bool& open);

} // namespace ui
