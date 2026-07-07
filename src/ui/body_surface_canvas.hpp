#pragma once

#include "ui_state.hpp"
#include "world/economy_system.hpp" // economy_report (Production lens)
#include "world/recipe_registry.hpp" // recipe_registry (Production / Opportunity lenses)
#include "world/world.hpp"

#include <imgui.h>

namespace ui {

/// Draw the Body Surface Canvas into the given region using the ImGui
/// background draw list.
///
/// A pointy-top hex grid for state.active_body: each tile is a coloured
/// hexagon keyed to its terrain, buildings are marked with a small square,
/// and hovering a tile shows its full data. The player can pan (middle mouse)
/// and zoom (scroll wheel).
///
/// This is the bottom rung of the canvas ladder, so it is only ever drawn as
/// the primary viewport — never the minimap. All sizes are derived from @p size.
///
/// @param w             Read-only world state.
/// @param state         Shared UI state; mutated on tile click and pan/zoom.
/// @param reg           Recipe / economy registry — read by the Production and
///                      Opportunity lenses to value outputs and recipes.
/// @param report        Most recent economy-step report — the Production lens reads
///                      per-building output quantities from it.
/// @param origin        Top-left of the region, in screen pixels.
/// @param size          Width and height of the region, in screen pixels.
/// @param input_enabled When true, hover and click are processed. The caller
///                      disables input for whichever canvas the mouse is not over.
/// @param lens_key_anchor Screen point the active lens's on-canvas key hangs from:
///                      its RIGHT edge sits at anchor.x and it is vertically centred on
///                      anchor.y. Pass the minimap's left edge + vertical centre so the
///                      key reads as a drawer folding out from the minimap.
void draw_body_surface_canvas(const world& w, ui_state& state, const recipe_registry& reg,
                              const economy_report& report, ImVec2 origin, ImVec2 size,
                              bool input_enabled, ImVec2 lens_key_anchor);

} // namespace ui
