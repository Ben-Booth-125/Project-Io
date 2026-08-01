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

/// Sentinel for `ui_state::why_note_open` meaning "open the FIRST question log
/// drawn this frame, whichever it turns out to be".
///
/// It exists for the verify harness: a Lua script cannot compute an ImGui id (the
/// ids are stack-dependent and only exist mid-frame), so without this the open
/// state of a note would be unverifiable and every capture would show it closed.
/// The first `why_note` to see the sentinel claims it by writing its own id, so the
/// one-note-at-a-time invariant holds from the next frame on.
inline constexpr unsigned int why_note_first = 0xFFFFFFFFu;

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

/// The chart question log (BL-247): a closed-by-default "Why this chart" toggle
/// revealing exactly two lines — the question this chart answers, and why this
/// evidence justifies that answer.
///
/// It is NOT a tutorial and never suggests what to ask next; it documents, on
/// request, what a chart that already exists is for. Distinct from a derivation
/// caption, which answers *how a number was computed* — a chart may want both, and
/// they must not be merged into one blob of text.
///
/// Both strings are optional as a pair: passing nullptr for either draws no toggle
/// at all, so an unlabelled chart costs nothing. At most one note is open at a
/// time, and opening or folding any overlay closes it — the note is meant to be
/// read once, not to become a way of reading the chart's normal content.
///
/// Lives here rather than in `charts.hpp` because it needs `ui_state`, and
/// `charts::` is deliberately a pure draw-primitive namespace that knows nothing
/// about it. Returns true while the note is open.
bool why_note(ui_state& ui, const char* answers, const char* because);

/// As above, but with the note's identity supplied by the caller.
///
/// A host that must RESERVE the note's height before opening a fixed-size container
/// needs to know whether the note is open *before* drawing it, which means knowing
/// its id first. The generation chart rows are exactly that case: no scrollbar, so
/// an unreserved open note would clip. Prefer the overload above wherever the height
/// is free.
bool why_note(ui_state& ui, unsigned int id, const char* answers, const char* because);

} // namespace ui
