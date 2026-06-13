#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

#include <imgui.h>

namespace ui {

/// Human-readable label for an overlay mode (legend chip, tooltips).
///
/// @param m Overlay mode.
/// @return  Null-terminated display name.
const char* overlay_mode_name(overlay_mode m);

/// Short overlay label for the compact bottom control strip, distinct from
/// overlay_mode_name's fuller legend text (e.g. "Supply" vs "Supply routes").
///
/// @param m Overlay mode.
/// @return  Null-terminated short display name.
const char* overlay_mode_short_name(overlay_mode m);

/// Toggle an overlay mode from the overlay control strip: selecting the
/// already-active mode clears it back to overlay_mode::none.
///
/// @param ui UI state to mutate.
/// @param m  Mode to toggle on (or off if it is already active).
void toggle_overlay(ui_state& ui, overlay_mode m);

/// Draw the overlay-lens control strip: a row of mode toggle buttons pinned to
/// the bottom-left of the shell, running from the navigation-pane edge inward
/// (clear of the centred scale/zoom control). The active lens's button is
/// highlighted; clicking it again clears the overlay. This replaces the former
/// minimap mode-bar dots, giving the controls a clearer, labelled affordance.
///
/// @param ui       UI state; the buttons toggle ui.overlay (via toggle_overlay).
/// @param left_x   Left edge of the strip, screen pixels (the nav-pane inner edge).
/// @param bottom_y Bottom edge to anchor the strip against, screen pixels.
void draw_overlay_controls(ui_state& ui, float left_x, float bottom_y);

/// Draw the overlay pass for a canvas, on top of the base canvas and below the
/// ImGui chrome. Switches on ui.overlay (and the rung) to render the active
/// lens.
///
/// This is the building block for the canvas overlays later layers add. Layer 2
/// has no overlay data, so the pass currently draws nothing — the active lens is
/// named by the bottom overlay control strip (draw_overlay_controls), not an
/// on-canvas chip. It remains the single extension point where Layer 5 supply
/// routes (convoy lines with progress markers between bodies on the Solar
/// canvas), a market/price lens, and faction presence will hang their drawing,
/// keyed by mode and rung.
///
/// @param w      Read-only world state (the overlay data source, later).
/// @param ui     UI state; ui.overlay selects the lens.
/// @param level  Which rung this canvas is (overlays differ per rung).
/// @param origin Top-left of the canvas region, screen pixels.
/// @param size   Canvas region size, screen pixels.
/// @param dl     Draw list to render into (the background draw list).
void draw_canvas_overlay(const world& w, const ui_state& ui, canvas_level level,
                         ImVec2 origin, ImVec2 size, ImDrawList* dl);

} // namespace ui
