#pragma once

#include "world/recipe_registry.hpp"
#include "world/world.hpp"

struct ui_state; // forward-declared; the full definition lives in ui_state.hpp.

namespace ui {

/// Draw the Construction Ledger — a ledger-family panel (BL-029) with a button-strip
/// nav across three bounded views (Build / Manage / Sell Orders).
///
/// Since BL-122 this is re-hosted into the shell fold-out column (foldout_column.hpp),
/// like the other named ledgers: it draws pinned + borderless into the shared column
/// rect, and is closed via the nav rail (accordion) rather than a title-bar 'x'. The
/// former BL-082 caller-supplied spawn/height-cap is gone — the column sits entirely
/// left of the Selection element, so no bottom-clearance anchoring is needed.
///
/// When *p_open is false the function draws nothing.
///
/// @param w          World — read for building/tile lookup; written by management
///                   controls (workforce_target, decommissioned, active_recipe_index).
/// @param reg        Loaded registry (recipe names and per-type economics).
/// @param state      Shared UI state — read (selected_entity) and written (construction).
/// @param p_open     Open/closed flag; gates whether the panel draws.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             ui_state& state,
                             bool* p_open);

} // namespace ui
