#pragma once
#include "../world/world.hpp"
#include "ui_state.hpp"

namespace ui {

/// Draws the Market Ledger window. open controls visibility (toggled by nav rail).
///
/// @param w    Read-only world (markets, bodies).
/// @param s    Current UI state (unused directly; passed for future extension).
/// @param open Open/closed flag; cleared by the close button.
void draw_market_ledger(const world& w, const ui_state& s, bool& open);

} // namespace ui
