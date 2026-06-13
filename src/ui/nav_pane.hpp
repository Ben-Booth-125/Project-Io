#pragma once

#include "ui_state.hpp"

namespace ui {

/// Draw the left navigation pane.
///
/// A fixed, full-height column at the left edge of the window holding a
/// vertical strip of ten tab slots. Each slot will eventually open one of the
/// game's menus or ledgers. For Layer 2 only the first slot is wired — the
/// Tile Ledger — and it toggles state.show_tile_ledger; the remaining nine are
/// reserved placeholders.
///
/// @param state      Shared UI state; the Tile Ledger tab toggles show_tile_ledger.
/// @param top_offset Y position of the pane's top edge, in pixels. The profile
///                   panel occupies the top-left corner, so the pane starts
///                   below it.
void draw_nav_pane(ui_state& state, float top_offset = 0.0f);

/// Width of the navigation pane in pixels. Exposed so the caller can lay out
/// other panels clear of the pane.
inline constexpr float nav_pane_width = 200.0f;

} // namespace ui
