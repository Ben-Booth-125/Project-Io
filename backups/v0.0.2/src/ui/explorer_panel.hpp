#pragma once

namespace ui {

/// Draw the explorer panel.
///
/// A panel on the right edge of the window, below the time column and above the
/// minimap. The explorer is the player's pinning and quick-navigation surface:
/// a curated list of pinned UI elements (units, bodies, buildings, markets)
/// that can be jumped to in one click. Layer 2 is an empty placeholder — no
/// pinning is wired yet. See docs/ui/EXPLORER.md.
///
/// @param x      Left edge of the panel (aligned with the minimap).
/// @param y      Top edge of the panel (below the time controls).
/// @param width  Panel width (same as the minimap).
/// @param height Panel height (down to just above the minimap).
void draw_explorer_panel(float x, float y, float width, float height);

} // namespace ui
