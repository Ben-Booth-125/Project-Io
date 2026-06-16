#pragma once

#include "world/recipe_registry.hpp"
#include "world/world.hpp"

struct ui_state; // forward-declared; the full definition lives in ui_state.hpp.

namespace ui {

/// Draw the Layer 4 construction / building-management panel — the building
/// surface added as v0.0.5 "Layer 4 UI groundwork" (see ROADMAP.md). It carries
/// two sections:
///   - a **Build** section whose buttons arm placement mode by writing the shared
///     construction_state (type / target / active) on @p state; and
///   - a **Selected building** section that resolves the building sitting on the
///     selected tile and shows its configuration read-only.
///
/// This is a *non-mutating scaffold*: the Build buttons only set placement-mode UI
/// state, and the per-building controls (recipe combo, workforce slider, sell-order
/// button) are DISABLED display-only stubs. The functional loop — build-cost spend,
/// world mutation, recipe/workforce authoring — lands in v0.0.6 (TODO § Infrastructure).
///
/// The window carries a close button; clicking it clears *p_open so the window
/// closes rather than collapses. When *p_open is false the function draws nothing.
///
/// @param w      World — read for building/tile lookup; written by management controls
///               (workforce_target, decommissioned, active_recipe_index).
/// @param reg    Loaded registry (recipe names and per-type economics).
/// @param state  Shared UI state — read (selected_entity) and written (construction).
/// @param p_open Open/closed flag; cleared by the close button.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             ui_state& state,
                             bool* p_open);

} // namespace ui
