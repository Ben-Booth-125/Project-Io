#pragma once

#include "world/economy_system.hpp" // economy_report
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

struct ui_state; // forward-declared; the full definition lives in ui_state.hpp.

namespace ui {

/// Draw the Construction ledger — a ledger-family panel (BL-029, redesigned BL-143,
/// slimmed to a single view on Ben's 2026-07-16 steer) answering one question: "what's
/// building?" (the in-progress build queue).
///
/// The former "Buildings" tab — a player-building roster plus the selected building's
/// management detail — is gone: a single building's detail is a *selection* question,
/// so it now lives in the Selection element (selection_panel.hpp
/// § draw_selection_panel), reached by selecting the building. The Build front door
/// moved to the tile Selection element (BL-139) and Sell Orders to the Market Ledger
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
/// @param w          World — read for the in-progress build queue.
/// @param reg        Loaded registry (per-type economics for the queue's cost read).
/// @param state      Shared UI state — the panel_view field (single view; retained so
///                   nav_button's toggle rule still closes the ledger).
/// @param p_open     Open/closed flag; gates whether the panel draws.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             ui_state& state,
                             bool* p_open);

} // namespace ui
