#pragma once

#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <imgui.h> // ImVec2 for the caller-supplied spawn geometry (BL-082)

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
/// Ledger conventions: opens at the caller-supplied @p spawn_pos / @p spawn_size
/// (ImGuiCond_Once), starts closed, title "Construction". Unlike the rest of the
/// ledger family (which share `ledger_window_spawn`/`ledger_window_size`), the
/// Construction panel takes its spawn from the caller so app can anchor it clear
/// of the bottom-left Selection element during placement (BL-082) — the moment the
/// player reads the BL-071 affordance/reason surfaces the panel used to occlude.
///
/// The window carries a close button; clicking it clears *p_open so the window
/// closes rather than collapses. When *p_open is false the function draws nothing.
///
/// @param w          World — read for building/tile lookup; written by management
///                   controls (workforce_target, decommissioned, active_recipe_index).
/// @param reg        Loaded registry (recipe names and per-type economics).
/// @param state      Shared UI state — read (selected_entity) and written (construction).
/// @param p_open     Open/closed flag; cleared by the close button.
/// @param spawn_pos  First-open window position (ImGuiCond_Once); movable after.
/// @param spawn_size First-open window size (ImGuiCond_Once); resizable after.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             ui_state& state,
                             bool* p_open,
                             ImVec2 spawn_pos,
                             ImVec2 spawn_size);

} // namespace ui
