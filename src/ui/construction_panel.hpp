#pragma once

#include "world/recipe_registry.hpp"
#include "world/world.hpp"

struct ui_state; // forward-declared; the full definition lives in ui_state.hpp.

namespace ui {

/// Draw the Construction Ledger — a ledger-family window (BL-029) with two
/// sections:
///   - a **Queue overview** table listing all active construction items across
///     every body (body / building type / progress / est. ticks remaining /
///     cost remaining); shows "No active construction" when the queue is empty.
///   - a **Selected building** section that resolves the building sitting on
///     the selected tile and shows its configuration and management controls.
///
/// Ledger conventions: opens at `ledger_window_spawn` / `ledger_window_size`
/// (ImGuiCond_Once), starts closed, title "Construction".
///
/// @param w      Read-only world (buildings, tiles, queues).
/// @param reg    Loaded registry (recipe names and per-type economics).
/// @param state  Shared UI state — read (selected_entity) and written (construction).
/// @param p_open Open/closed flag; cleared by the close button.
void draw_construction_panel(const world& w,
                             const recipe_registry& reg,
                             ui_state& state,
                             bool* p_open);

} // namespace ui
