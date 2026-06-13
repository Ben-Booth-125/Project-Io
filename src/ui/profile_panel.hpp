#pragma once

namespace ui {

/// Draw the player corporation profile panel.
///
/// A compact, fixed panel pinned to the top-left corner of the window, above the
/// navigation pane. Shows the player faction at a glance: a portrait placeholder,
/// the corporation name, and a line or two of basic standing. Layer 2 is a static
/// placeholder — faction identity is still undecided (see docs/CONCEPT.md) so the
/// panel only reserves the shape of the data. See docs/ui/PROFILE.md.
void draw_profile_panel();

/// Width of the profile panel in pixels. The profile keeps its own width — wide
/// enough for the portrait and name — independent of the narrow icon nav rail
/// below it. Exposed so the header can start at the profile's right edge.
inline constexpr float profile_panel_width = 200.0f;

/// Height of the profile panel in pixels. Exposed so the navigation pane can
/// start below it.
inline constexpr float profile_panel_height = 92.0f;

} // namespace ui
