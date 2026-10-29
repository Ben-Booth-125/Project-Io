#pragma once

#include "ui_state.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

namespace ui {

/// Draws the Military ledger — nav rail slot 7 (Ben, 2026-10-27), where the
/// reserved Corp. Strategy stood. Design: docs/ui/ledgers/military.md.
///
/// "What force can I raise, where, at what cost — and what is it raised with?"
/// Two views behind a `nav_button` strip:
///
/// - **Recruit** (default): a base picker over the player nation's completed
///   military bases, the roster (every row the campaign band admits, dimmed with
///   why where the nation cannot field it), each with a **Raise** press, and the
///   muster queue (`world::musters`, the player's own orders).
/// - **Design**: one row per land class with its live design's numbers; a row
///   press opens the full-canvas unit designer for that class (unit_designer.hpp),
///   and pressing the lit row again closes it. Below, the obsolete-type toggles.
///
/// GROUPED BY THEATRE (BL-1450; military.md § Grouped by theatre): both tabs set
/// their rows under collapsible Land and Air headers, each with its count. Recruit's
/// Air holds an airbase picker, the six air rows (Raise queues `military.pending_wing_*`,
/// which app sends as `hire_wing`) and the player's wings, each a press selecting it;
/// Design's Air holds the six air classes, each opening the air designer
/// (unit_designer.hpp).
///
/// RAISE IS NOT A SECOND HIRE PATH. The press writes `construction.pending_hire_tile`
/// / `pending_hire_unit_type`, which `app::render` executes through the
/// `hire_unit` verb — the same request the tile card's Hire press makes.
///
/// @param w    Read-only world.
/// @param reg  Registry: the era (roster band) and the military hire prices.
/// @param s    UI state — the ledger's view state, the hire request, and the
///             Construction-ledger door in the no-base empty state.
/// @param open Open/closed flag; toggled by the nav rail.
void draw_military_ledger(const world& w, const recipe_registry& reg, ui_state& s, bool& open);

} // namespace ui
