#pragma once

#include "plot_history.hpp"          // resource_history_view (drill-down chart data)
#include "ui_state.hpp"
#include "world/economy_system.hpp" // economy_report (passed through to the content)
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <imgui.h>

namespace ui {

/// The sticky detail card (BL-194/195) — a click-opened, canvas-confined detail
/// surface that is now the sole home of the Selection content (the former
/// fold-out Selection panel is gone; the shell column is ledgers-only). Distinct
/// from the transient 200px hover card (hover_card.hpp), which is unchanged and
/// keeps serving the dwell-triggered glance; this is the larger surface a single
/// click pins open. See docs/ui/TOOLTIP.md, docs/ui/SELECTION.md.
///
/// **State model.** A single click already *selects* (SELECTION.md), so the card
/// is open exactly when
/// `ui.selected_entity != null_entity && ui.selected_entity != ui.selection_hidden_for`.
/// Dismissing (the content's `x` button, or Esc handled by app.cpp) sets
/// `selection_hidden_for` to the current selection — the same hide-not-destroy
/// mechanism the fold-out panel used. Binary open/closed only: no animation, no
/// partial reveal (ImGui has no per-window transform, and scaled text is blurry).
///
/// **Placement.** The card freezes at the click position (`ui.card_anchor`,
/// captured by the canvas select-click) and centres there, clamped so the whole
/// card stays inside the passed canvas rect. A programmatic selection leaves the
/// `{-1,-1}` sentinel, which reads as "no anchor → centre on the canvas rect" —
/// the deterministic path the verify harness and post-build auto-select take.
///
/// **Content.** The card frames its own window and calls `draw_selection_content`
/// (selection_panel.cpp) for the body — the full per-kind Selection layout (tile
/// graphs, building operate panel, action|facts), not the terser hover content.
///
/// @param w             World state (mutable — the building layout operates its
///                       building directly; destructive acts still defer through
///                       ui_state::construction).
/// @param reg           Loaded registry (build costs / recipe economics).
/// @param report        Most recent economy step report (building profit / caps).
/// @param ui            Shared UI state; read for the selection + anchor, written
///                       by the close button (hide).
/// **Recursive drill-down (BL-196).** When `ui.card_stack` is non-empty the card
/// shows a drilled view instead of the root content — currently a resource
/// time-series chart (aggregate-vs-this-tile over time), fed by @p history. A back
/// button (or Esc, handled by app.cpp) pops one frame; at the root, dismiss hides.
///
/// @param w             World state (mutable — the building layout operates its
///                       building directly; destructive acts still defer through
///                       ui_state::construction).
/// @param reg           Loaded registry (build costs / recipe economics).
/// @param report        Most recent economy step report (building profit / caps).
/// @param history       Resource-deposit history for the drill-down chart (BL-198);
///                       any member may be null when there is no data yet.
/// @param ui            Shared UI state; read for the selection + anchor + drill
///                       stack, written by the close/back buttons and drill clicks.
/// @param canvas_origin Top-left of the region the card is confined to, screen px
///                       (the free canvas area, clear of the shell chrome).
/// @param canvas_size   Width and height of that region.
void draw_selection_card(world& w, const recipe_registry& reg,
                         const economy_report& report,
                         const resource_history_view& history, ui_state& ui,
                         ImVec2 canvas_origin, ImVec2 canvas_size);

} // namespace ui
