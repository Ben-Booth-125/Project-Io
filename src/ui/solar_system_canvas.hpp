#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

#include <imgui.h>

namespace ui {

/// Draw the Solar System Canvas into the given region using the ImGui
/// background draw list.
///
/// A top-down 2D view of the system: the star at centre, an orbital ring per
/// body, and each body placed by its orbital radius and angle. The selected
/// body carries an outline; hovering a body shows a tooltip.
///
/// The same function renders both the primary viewport and the inset minimap;
/// all sizes are derived from @p size, so the caller simply passes a smaller
/// region for the minimap.
///
/// @param w             Read-only world state.
/// @param state         Shared UI state; mutated on body click and minimap swap.
/// @param origin        Top-left of the region, in screen pixels.
/// @param size          Width and height of the region, in screen pixels.
/// @param input_enabled When true, hover and click are processed. The caller
///                      disables input for whichever canvas the mouse is not over.
void draw_solar_system_canvas(const world& w, ui_state& state, ImVec2 origin, ImVec2 size, bool input_enabled);

} // namespace ui
