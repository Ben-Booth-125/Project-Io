#pragma once

namespace ui {

/// Draw the budget + resource overview header.
///
/// A full-width strip across the top of the canvas area, between the profile
/// (top-left) and the time column (top-right). It is the player's persistent
/// financial and material dashboard: the treasury balance and a deliberately
/// scarce strip of headline stockpiles. Layer 2 is a placeholder showing
/// zeroed figures — the values are not yet wired to the economy. See
/// docs/ui/HEADER.md.
///
/// @param left  Left edge of the strip (clear of the navigation pane).
/// @param right Right edge of the strip (clear of the time column).
void draw_header_panel(float left, float right);

/// Height of the header strip in pixels.
inline constexpr float header_panel_height = 52.0f;

} // namespace ui
