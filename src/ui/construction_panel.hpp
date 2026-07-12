#pragma once

#include "world/economy_system.hpp" // economy_report
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

struct ui_state; // forward-declared; the full definition lives in ui_state.hpp.

namespace ui {

/// Draw the Building Ledger — a ledger-family panel (BL-029, renamed + redesigned
/// BL-143) with a button-strip nav across two bounded views: Construction (the
/// in-progress build queue) and Buildings (every player building, with aggregates).
/// The old Build front-door and Sell Orders tab are gone — building is now reached
/// only via tile selection (BL-139), and sell orders live on the Market Ledger
/// (BL-159).
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
/// @param report     Latest economy report (BL-143) — per-building active/idle status
///                   for the Buildings tab's status column.
/// @param state      Shared UI state — read (selected_entity) and written (construction).
/// @param p_open     Open/closed flag; gates whether the panel draws.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             const economy_report& report,
                             ui_state& state,
                             bool* p_open);

/// Draw the tile-scoped **Manage** ledger — the contextual building-management surface
/// reached from the tile Selection element's "Manage Buildings" action (and a selected
/// building's "Manage" button). It shows the per-building management block (workforce /
/// recipe / decommission) for the building the selection resolves to, pinned + borderless
/// in the shared fold-out column — the symmetric counterpart to the tile construction
/// ledger (draw_construction_ledger). Gated by ui.show_manage_ledger; clears that flag and
/// draws nothing when the selection resolves to no building.
///
/// @param w    World — read for building/tile lookup; written by the management controls.
/// @param reg  Loaded registry (recipe names and per-type economics).
/// @param ui   Shared UI state — read (selected_entity) and written (show_manage_ledger).
void draw_building_manage_ledger(world& w, const recipe_registry& reg, ui_state& ui);

} // namespace ui
