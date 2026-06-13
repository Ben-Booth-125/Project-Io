#pragma once

namespace ui {

/// Draw the player corporation profile panel.
///
/// A compact, fixed panel pinned to the top-left corner of the window, above
/// the navigation pane and aligned to its width. Shows the player faction at a
/// glance: a portrait placeholder, the corporation name, and a line or two of
/// basic standing. Layer 2 is a static placeholder — faction identity is still
/// undecided (see docs/CONCEPT.md) so the panel only reserves the shape of the
/// data. See docs/ui/PROFILE.md.
void draw_profile_panel();

/// Height of the profile panel in pixels. Exposed so the navigation pane can
/// start below it.
inline constexpr float profile_panel_height = 92.0f;

} // namespace ui
