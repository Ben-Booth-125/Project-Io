#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

#include <imgui.h>

namespace ui {

/// The sticky detail card (BL-194) — a click-opened, canvas-confined detail
/// surface. Distinct from the transient 200px hover card (hover_card.hpp),
/// which is unchanged and keeps serving the dwell-triggered glance; this is
/// the larger surface a single click pins open.
///
/// State model: single-click already *selects* (SELECTION.md), so opening the
/// card piggybacks on the existing selection state rather than introducing a
/// parallel one — the card is open exactly when
/// `ui.selected_entity != null_entity && ui.selected_entity != ui.selection_hidden_for`.
/// Dismissing (the card's `x` button, or Esc) sets `selection_hidden_for` to
/// the current selection, the same hide-not-destroy mechanism the former
/// fold-out Selection element used. Binary open/closed only — no animation,
/// no partial reveal (ImGui has no per-window transform, and scaled text is
/// blurry).
///
/// The card's content is, for now, the shared `draw_hover_content` dispatch —
/// BL-195 (moving the full Selection element wholesale into this frame) and
/// BL-196 (recursive child cards, also built on `draw_hover_content`) both
/// build on this same frame rather than each rolling their own window.
///
/// @param w             Read-only world state.
/// @param ui            Shared UI state; read for the selection, written by
///                       the close button / Esc (hide) and by drill-down
///                       clicks inside the card (BL-196).
/// @param canvas_origin Top-left of the active primary canvas, in screen
///                       pixels — the card is confined (clamped) within it.
/// @param canvas_size   Width and height of the active primary canvas.
void draw_selection_card(const world& w, ui_state& ui,
                         ImVec2 canvas_origin, ImVec2 canvas_size);

} // namespace ui
