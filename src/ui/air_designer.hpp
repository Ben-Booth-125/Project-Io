#pragma once

#include "ui_state.hpp"
#include "world/components.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

namespace ui {

/// BL-1450 — the designer opened on an AIR class (docs/ui/ledgers/military.md
/// § Grouped by theatre: "each opens the same full-canvas designer with its own
/// slots (airframe, armament, avionics)"; MILITARY.md § Air).
///
/// The same rung and the same geometry as the land designer (unit_designer.hpp):
/// the class in the middle, one box per slot beside it joined by a leader line,
/// a box's press opening the slot's kit list (base first; locked kit listed with
/// the research node that opens it), a summary under the image. It is its own
/// surface (`detail_surface::air_designer`, key = air_class) and its own state
/// (`ui_state::air_designer`) because the air kit is its own catalogue and its own
/// verb: a pick queues `air_designer.pending_*`, which app::render sends as one
/// `set_air_design`, and the greying reads `air_design_check`. There is no air
/// art yet, so the image is the aircraft glyph on the placeholder panel.
///
/// Draws nothing unless it is the open takeover. Call every frame.
void draw_air_designer(const world& w, const recipe_registry& reg, ui_state& s);

/// Open the air designer on @p c, or close it if it is open there (the toggle rule).
void toggle_air_designer(ui_state& s, air_class c);

/// Whether the air designer is open on @p c.
bool air_designer_open_on(const ui_state& s, air_class c);

} // namespace ui
