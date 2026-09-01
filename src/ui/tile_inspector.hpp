#pragma once

#include "ui_state.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world.hpp"

namespace ui {

/// The History (Tile) ledger's view indices, as parked in `ui_state::history_view`.
///
/// PUBLIC because the canvas routes to one of them: a plate press under the
/// Continent lens opens this ledger on Tectonics (BL-660), and a bare `3` at that
/// call site is exactly the hand-kept literal this file's own clamp comment warns
/// about — the Ages renumbering already broke saved indices once.
enum history_view_id { history_view_story = 0,
                       history_view_chain,
                       history_view_ages,
                       history_view_tectonics };

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
/// @param gen_params The descriptor the live world was built from
///                (app::m_active_world_params). The Ages view needs it to
///                replay the era on the span, clock and seed GENERATION ran,
///                rather than on a second set constructed here — see
///                world/era_minus_one.hpp for why a second derivation is the
///                defect rather than the repair.
/// @param p_open  Open/closed flag. Cleared by the window's close button.
void draw_tile_inspector(const world& w, ui_state& s,
                         const generation_report& report,
                         const world_params& gen_params, bool* p_open);

} // namespace ui
