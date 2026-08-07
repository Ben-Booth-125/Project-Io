#pragma once

#include "../world/standing.hpp"
#include "../world/world.hpp"
#include "ui_state.hpp"

#include <vector>

namespace ui {

/// Draws the Corporation Overview Dashboard. open controls visibility (toggled by nav rail).
///
/// @param w         The world to read corporations from (const; selection mutation is via s).
/// @param standings This tick's per-corp standing profile (BL-262 first slice), precomputed by
///                   the caller via compute_corp_standings — the player's row shows exact
///                   figures, every other row shows bands only (BL-068 competitor-visibility).
/// @param s         Shared UI state; clicking a row sets s.selected_entity.
/// @param open      Visibility flag; the window's close button writes false.
void draw_corporation_panel(const world& w, const std::vector<corp_standing>& standings,
                             ui_state& s, bool& open);

} // namespace ui
