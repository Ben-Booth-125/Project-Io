#pragma once

#include <cstdint>

struct ui_state;

// ---------------------------------------------------------------------------
// Drill-through: the one disclosure idiom (BL-214)
//
// Every dense surface in the app had invented its own way of showing more —
// a collapsing header here, a pager there, a tooltip somewhere else. This is
// the single idiom they all obey instead, and it has exactly TWO states:
//
//   FOLDED    a verdict line plus a chevron. The resting state.
//   EXPANDED  a true full-screen overlay showing everything the surface has
//             at once — chart, legend, prose, and any drill opened from it.
//
// Expanded is a real mode switch, not an in-place grow. That was Ben's call on
// 2026-07-31 after reviewing four live exemplars prototyping the alternative,
// and it supersedes the three-level Glance/Read/Study stepper the item was
// originally designed around.
//
// The state model falls out of that choice. Because expanded is an overlay,
// only ONE thing can be expanded at a time — so the state is a single
// (surface, key) target, not a remembered level per surface. Folding is
// therefore always unambiguous: there is one thing to fold.
//
// Three axes, never conflated (the item's own framing):
//   DEPTH   how much of this subject do I want   -> here
//   SUBJECT what am I looking at                 -> ui_state::card_stack (BL-196)
//   HOST    where does this properly live        -> ui::focus_on_entity, the [>] button
//
// Authority: docs/ui/LAYOUT.md § Drill-through.
// ---------------------------------------------------------------------------

/// Every site that owns a fold control. `key` disambiguates instances within one
/// surface (a chain stage index, a roll-up card), so a surface with many foldable
/// blocks needs one enumerator, not one per block.
///
/// Adding a surface is two edits: an enumerator here, and a `fold_chevron` +
/// `fold_overlay_begin` pair at the call site. Nothing else — no new control, no
/// new state, no new container kind.
enum class detail_surface : std::uint8_t
{
    none = 0,          ///< Nothing expanded. The resting value.
    selection_metric,  ///< The tile Selection band's metric card (key = page index).
    history_story,     ///< History ledger, Story view.
    history_chain,     ///< History ledger, Chain view (key = chain_stage).
    history_tiles,     ///< History ledger, Tiles view.
    generation_stage,  ///< The New World wizard's chain stages (key = chain_stage).
    corp_rollup,       ///< The Corporation dashboard's roll-up cards (key = card index, BL-248).
};

/// What is expanded right now. `surface == none` means everything is folded.
///
/// VIEW state: not serialised, and deliberately not remembered per surface — a
/// full-screen overlay is a transient mode, and which card a player last opened
/// is a display preference rather than something to restore.
struct fold_state
{
    detail_surface surface = detail_surface::none;
    int            key     = 0; ///< Meaningful only while `surface != none`.
};


namespace ui {

/// Whether this exact (surface, key) is the expanded one.
bool is_expanded(const ui_state& ui, detail_surface s, int key = 0);

/// Whether anything at all is expanded — the Esc ladder and the canvases read this.
bool any_expanded(const ui_state& ui);

/// Expand this (surface, key), folding whatever was expanded before. Also closes
/// any open "why this chart" note, so the overlay opens in its resting state.
void expand(ui_state& ui, detail_surface s, int key = 0);

/// Fold whatever is expanded. A no-op when nothing is.
void fold(ui_state& ui);

/// The fold control: a single chevron, pointing DOWN while folded (there is more
/// below) and UP while expanded. Draws at the current cursor and is sized to one
/// frame height, so it sits on a title row beside the `[>]` / `[x]` cluster.
///
/// Returns true on the frame the state changed. Clicking while expanded folds —
/// the chevron IS a toggle, unlike the superseded stepper, because the two states
/// are symmetric and either one is a thing to undo to.
bool fold_chevron(ui_state& ui, detail_surface s, int key = 0);

/// Open the full-screen overlay for this (surface, key), if it is the expanded one.
///
/// Returns true when the caller should draw the overlay's body — and ONLY then must
/// the caller pair it with `fold_overlay_end()`. Returns false when this surface is
/// folded, in which case the caller draws nothing and calls nothing. (Deliberately
/// not ImGui's Begin/End contract, where End is unconditional: an overlay that is
/// usually closed reads better as a guard than as a block with an early-out.)
///
/// Draws its own header — @p title on the left, a fold-up chevron on the right —
/// and a Separator, so the body starts at the content.
bool fold_overlay_begin(ui_state& ui, detail_surface s, int key, const char* title);

/// Close an overlay that `fold_overlay_begin` returned true for.
void fold_overlay_end(ui_state& ui);


} // namespace ui
