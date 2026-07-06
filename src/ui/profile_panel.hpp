#pragma once

#include "world/world.hpp"

namespace ui {

/// Draw the player corporation profile panel.
///
/// A compact, fixed panel pinned to the top-left corner of the window, above the
/// navigation pane. Shows the player faction at a glance: a portrait placeholder,
/// the corporation name, its parent (registering) nation, and its industrial
/// focus — read live from the player corporation in @p w. See docs/ui/PROFILE.md.
///
/// @param w World — read for the player corporation and its home nation.
void draw_profile_panel(const world& w);

/// Legacy profile width, retained as the **floating-ledger spawn anchor** only
/// (`ledger_chrome.hpp` seeds `ledger_window_spawn` off it, for the still-floating
/// Tile Ledger). Since BL-122 the profile/identity tile takes the full shell-column
/// width `shell_column_width(disp.x)` (foldout_column.hpp), not this constant, and the
/// header starts at that same column edge — so this no longer describes the profile's
/// width. Superseded for layout; kept until the Tile Ledger migrates off the floating
/// spawn.
inline constexpr float profile_panel_width = 200.0f;

/// Height of the profile panel in pixels. Exposed so the navigation pane can
/// start below it.
inline constexpr float profile_panel_height = 92.0f;

} // namespace ui
