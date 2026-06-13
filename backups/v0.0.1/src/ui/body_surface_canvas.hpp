#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

#include <imgui.h>

namespace ui {

/// Draw the Body Surface Canvas into the given region using the ImGui
/// background draw list.
///
/// A pointy-top hex grid for state.active_body: each tile is a coloured
/// hexagon keyed to its terrain, buildings are marked with a small square,
/// and hovering a tile shows its full data. When primary, the player can pan
/// (middle mouse) and zoom (scroll wheel). The minimap always shows the full
/// auto-fit framing.
///
/// The same function renders both the primary viewport and the inset minimap;
/// all sizes are derived from @p size.
///
/// @param w             Read-only world state.
/// @param state         Shared UI state; mutated on tile click, minimap swap, and pan/zoom.
/// @param origin        Top-left of the region, in screen pixels.
/// @param size          Width and height of the region, in screen pixels.
/// @param input_enabled When true, hover and click are processed. The caller
///                      disables input for whichever canvas the mouse is not over.
void draw_body_surface_canvas(const world& w, ui_state& state, ImVec2 origin, ImVec2 size, bool input_enabled);

} // namespace ui
