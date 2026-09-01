#pragma once

#include "plot_history.hpp"          // resource_history_view (drill-down chart data)
#include "ui_state.hpp"
#include "world/economy_system.hpp" // economy_report (passed through to the content)
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <imgui.h>

namespace ui {

// The band's height moved to foldout_column.hpp (2026-07-30) and became a
// function of the display: it is now DERIVED from the minimap height so the
// bottom strip's top edge aligns with the minimap's. See
// `ui::selection_band_height(disp_x, disp_y)`.
//
// The band's whole RECT — both flush edges as well as that height — is composed by
// `ui::selection_band_rect(disp)` (BL-216, `shell_metrics.hpp`), which is what
// app.cpp passes in below. Nothing else should re-derive it: the band's left edge is
// the comms dock's right edge and its right edge is the right chrome column's left,
// so a hand-rolled copy drifts the moment either neighbour moves.

/// The Selection band (BL-213 — supersedes the BL-194/195 click-anchored card) —
/// a FIXED bottom band, sandwiched between the shell column and the right chrome
/// column, that is the sole home of the Selection content. It no longer follows
/// the click position (Ben, 2026-07-28: "doing/building" menus need a fixed
/// place at the bottom of the screen, not a widget that floats with the
/// cursor). Distinct from the 200px hover card (hover_card.hpp), which serves the
/// glance only: since BL-228 that card FREEZES where it was summoned and is
/// dismissed by leaving its bounds, and it no longer opens this band — clicking
/// is once again the only thing that does. See docs/ui/TOOLTIP.md,
/// docs/ui/SELECTION.md.
///
/// **State model.** A single click already *selects* (SELECTION.md), and the band
/// is ALWAYS open (BL-266): there is no dismissal and no hide state. With no
/// (valid) selection it rests on the player's own corporation — the band never
/// renders `selection_kind::none`, while `selected_entity` stays null so
/// deselect remains representable.
///
/// **Placement — fixed, not click-anchored (BL-213).** The band fills the exact
/// rect the caller passes (@p band_origin / @p band_size) — bottom-anchored,
/// full width between the shell column and the right chrome column, unaffected
/// by where the triggering click landed. This is a deliberate reversal of the
/// prior freeze-at-click-position behaviour: the "doing" surface always
/// lives in the same place so the player's eye doesn't have to re-find it.
/// **Opening the band does NOT close whatever fold-out ledger is open** — the
/// two no longer compete for the same screen real estate (see LAYOUT.md), so
/// there is no accordion between them.
///
/// **Content.** The band frames its own window and calls `draw_selection_content`
/// (selection_panel.cpp) for the body — the full per-kind Selection layout (tile
/// graphs, building operate panel, action|facts), not the terser hover content.
///
/// **Recursive drill-down (BL-196).** When `ui.card_stack` is non-empty the band
/// shows a drilled view instead of the root content — currently a resource
/// time-series chart (aggregate-vs-this-tile over time), fed by @p history. A back
/// button (or Esc, handled by app.cpp) pops one frame; at the root, Esc opens the
/// system menu (BL-266 — the band itself never hides).
///
/// @param w            World state (mutable — the building layout operates its
///                      building directly; destructive acts still defer through
///                      ui_state::construction).
/// @param reg          Loaded registry (build costs / recipe economics).
/// @param report       Most recent economy step report (building profit / caps).
/// @param history      Resource-deposit history for the drill-down chart (BL-198);
///                      any member may be null when there is no data yet.
/// @param ui           Shared UI state; read for the selection + drill stack,
///                      written by the back button and drill clicks.
/// @param band_origin  Top-left of the fixed band rect, screen px.
/// @param band_size     Width and height of the band rect.
void draw_selection_band(world& w, const recipe_registry& reg,
                         const economy_report& report,
                         const resource_history_view& history, ui_state& ui,
                         ImVec2 band_origin, ImVec2 band_size);

} // namespace ui
