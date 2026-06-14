#pragma once

#include "world/world.hpp"

namespace ui {

/// Draw the Tile Ledger window (Layer 1 tile inspector).
///
/// Displays a body selector and a table of every tile on the selected body,
/// showing composition, landform, hazard level, habitability, and all resource
/// deposits.
/// Also lists buildings and the local market state for the selected body.
///
/// The window carries a close button; clicking it sets *p_open to false so the
/// window fully closes rather than merely collapsing. When *p_open is false the
/// function draws nothing. Reopen it from the navigation pane.
///
/// @param w       Read-only reference to the current world state.
/// @param p_open  Open/closed flag. Cleared by the window's close button.
void draw_tile_inspector(const world& w, bool* p_open);

} // namespace ui
