#pragma once

#include "../world/world.hpp"
#include "ui_state.hpp"

namespace ui {

/// Draws the Corporation Overview Dashboard. open controls visibility (toggled by nav rail).
///
/// @param w    The world to read corporations from (const; selection mutation is via s).
/// @param s    Shared UI state; clicking a row sets s.selected_entity.
/// @param open Visibility flag; the window's close button writes false.
void draw_corporation_panel(const world& w, ui_state& s, bool& open);

} // namespace ui
