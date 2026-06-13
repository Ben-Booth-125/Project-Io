#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

#include <imgui.h>

namespace ui {

/// Resolve the body a circumplanetary view centres on for the given selection.
///
/// The anchor is the planet (or belt asteroid) the view is built around: the
/// selected body itself if it orbits the star directly, or its parent planet if
/// it is a moon. Returns null_entity if @p active_body is unknown.
///
/// @param w           Read-only world state.
/// @param active_body The current selection (ui_state::active_body).
/// @return            The anchor body, or null_entity if none can be resolved.
entity_id circumplanetary_anchor(const world& w, entity_id active_body);

/// Draw the Circumplanetary Canvas into the given region using the ImGui
/// background draw list.
///
/// A top-down view of a single planet (the circumplanetary anchor) and its
/// moons: the anchor at the centre, an orbital ring per moon, and each moon
/// placed by its orbital radius and angle about the anchor. The selected body
/// carries an outline; hovering a body shows a tooltip. This is the middle rung
/// of the canvas ladder.
///
/// The same function renders both the primary viewport and the inset minimap;
/// all sizes are derived from @p size.
///
/// @param w             Read-only world state.
/// @param state         Shared UI state; mutated on body click (descend),
///                      minimap click (ascend), and (when primary) pan/zoom.
/// @param origin        Top-left of the region, in screen pixels.
/// @param size          Width and height of the region, in screen pixels.
/// @param input_enabled When true, hover, click, zoom, and pan are processed.
/// @param is_minimap    True when this canvas is the minimap inset (the Planetary
///                      screen is primary). A click then ascends to the
///                      Circumplanetary view rather than descending to a surface.
void draw_circumplanetary_canvas(const world& w, ui_state& state, ImVec2 origin, ImVec2 size, bool input_enabled, bool is_minimap);

} // namespace ui
