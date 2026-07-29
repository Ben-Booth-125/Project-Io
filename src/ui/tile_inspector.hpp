#pragma once

#include "ui_state.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world.hpp"

namespace ui {

/// Draw the Tile Ledger window — the nav rail's slot 9, "History" (MENU.md's
/// 2026-06-15 rename). Displays a body selector, the body's ORAL-HISTORY
/// BIOGRAPHY (BL-211 — the dated, causal lines Planetology/Continents-Drift
/// already produce, given a home here for the first time), then a table of
/// every tile on the selected body (composition, landform, hazard,
/// habitability, deposits), its buildings, and its local market state.
///
/// The window carries a close button; clicking it sets *p_open to false so the
/// window fully closes rather than merely collapsing. When *p_open is false the
/// function draws nothing. Reopen it from the navigation pane.
///
/// @param w       Read-only reference to the current world state.
/// @param s       Current canvas/nav state; seeds the body selector default and
///                holds the ledger's view/round selection (BL-211).
/// @param report  The Planetology/Continents-Drift generation report captured at
///                world-gen (app::m_generation_report) — the biography source.
/// @param p_open  Open/closed flag. Cleared by the window's close button.
void draw_tile_inspector(const world& w, ui_state& s,
                         const generation_report& report, bool* p_open);

} // namespace ui
