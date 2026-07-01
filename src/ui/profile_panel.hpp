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

/// Width of the profile panel in pixels. The profile keeps its own width — wide
/// enough for the portrait and name — independent of the narrow icon nav rail
/// below it. Exposed so the header can start at the profile's right edge.
inline constexpr float profile_panel_width = 200.0f;

/// Height of the profile panel in pixels. Exposed so the navigation pane can
/// start below it.
inline constexpr float profile_panel_height = 92.0f;

} // namespace ui
