#pragma once

#include "../world/world.hpp"
#include "ui_state.hpp"

namespace ui {

/// Draws the Balance Ledger window. open controls visibility (toggled by nav rail).
///
/// @param w    Read-only world (corporation balances and names).
/// @param s    UI state (provides w.player_entity as the default selection).
/// @param open Open/closed flag; the window's close button sets it to false.
void draw_balance_ledger(const world& w, const ui_state& s, bool& open);

} // namespace ui
