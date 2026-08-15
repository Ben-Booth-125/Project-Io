#pragma once

#include <cstdint>
#include <vector>

struct ui_state;

// ---------------------------------------------------------------------------
// Drill-through: the one disclosure idiom (BL-214, revised by BL-265)
//
// Every dense surface in the app had invented its own way of showing more —
// a collapsing header here, a pager there, a tooltip somewhere else. This is
// the single idiom they all obey instead. BL-214 shipped it as ONE control with
// two states; BL-265 splits that control in two, because a full-screen takeover
// turned out to be too big a step for "I want to see this one chart properly"
// (Ben, 2026-08-02, after using it):
//
//   FOLDED       a verdict line plus the control gutter. The resting state.
//   EXPANDED     the row grows IN PLACE — one graph, or an accordion of graphs.
//                The rest of the screen keeps working.
//   FULL CANVAS  a takeover of the CANVAS REGION (ui::canvas_rect), showing
//                everything the surface has at once, scrolled.
//
// The middle rung is the Glance/Read/Study stepper's, restored on evidence.
// What is NOT restored is the stepper's single cycling control: these are two
// distinct affordances with two distinct meanings, which is what makes them
// legible where one cycling control was not.
//
// THE STATE MODEL, RESTATED AGAINST THE CANVAS GEOMETRY (BL-265)
//
// BL-214 derived "only ONE thing can be expanded at a time" from expanded being
// a window-wide overlay. Both halves of that derivation moved, so it is restated
// here rather than left to be inferred:
//
//   * The TAKEOVER is single-target, and still for the original reason — it owns
//     one rectangle, so a second one would have nowhere to go. Opening a second
//     replaces the first. That is `fold_state::surface`/`key`.
//   * IN-PLACE expansion is NOT single-target. An accordion of graphs is by
//     definition several rows open at once, so it is a SET
//     (`fold_state::in_place`). A single-target in-place expander would make the
//     accordion unbuildable: opening the second graph would close the first.
//   * Inside a takeover the in-place set is IGNORED — every item renders open,
//     top to bottom. "Anything full screen deserves its full space" only holds
//     if the takeover does not depend on which rows happened to be unfolded.
//     Nothing here writes the set on the takeover's behalf, so returning
//     restores it exactly: the takeover is non-destructive.
//   * The takeover COEXISTS with the fold-out ledger column by design, not by
//     tolerance: `foldout_column_rect()` is entirely left of `canvas_rect()`, so
//     the row that opened a takeover stays visible in the left column while the
//     canvas changes. That is the same event as clicking a body on the canvas —
//     the source stays put, the stage changes. Do not "fix" it by dimming the
//     ledger.
//   * Esc gains ONLY the takeover rung. In-place expansion is not a mode the
//     player is in — it is a reading preference, the kind of state Esc has never
//     collapsed — so `ui::fold` closes the takeover and leaves the set alone.
//
// Three axes, never conflated (the item's own framing):
//   DEPTH   how much of this subject do I want   -> here
//   SUBJECT what am I looking at                 -> ui_state::card_stack (BL-196)
//   HOST    where does this properly live        -> ui::focus_on_entity, the [>] button
//
// Authority: docs/ui/LAYOUT.md § Drill-through.
// ---------------------------------------------------------------------------

/// Every site that owns a disclosure control. `key` disambiguates instances within
/// one surface (a chain stage index, a roll-up card), so a surface with many
/// foldable blocks needs one enumerator, not one per block.
///
/// Adding a surface is two edits: an enumerator here, and a `disclosure_controls`
/// + `fold_overlay_begin` pair at the call site. Nothing else — no new control, no
/// new state, no new container kind.
enum class detail_surface : std::uint8_t
{
    none = 0,          ///< Nothing expanded. The resting value.
    selection_metric,  ///< The tile Selection band's metric card (key = page index).
    building_metric,   ///< The building Selection band's accordion page (key = page index).
    history_story,     ///< History ledger, Story view.
    history_chain,     ///< History ledger, Chain view (key = chain_stage).
    generation_stage,  ///< The New World wizard's chain stages (key = chain_stage).
    corp_rollup,       ///< The Corporation dashboard's roll-up cards (key = card index, BL-248).
    building_section,  ///< Building management in the Buildings tab (key = building_view_section, BL-229).
};

/// What is disclosed right now — the takeover target, and the in-place set.
///
/// VIEW state: not serialised. The takeover is a transient mode and which card a
/// player last opened is a display preference rather than something to restore;
/// the in-place set is a reading preference on a surface that may not even be on
/// screen next frame.
struct fold_state
{
    // ── The takeover: exactly one (surface, key), or none ──
    detail_surface surface = detail_surface::none;
    int            key     = 0;    ///< Meaningful only while `surface != none`.
    bool           focus   = false; ///< Consumed by the first overlay frame after opening.

    // ── In place: a set, packed one (surface, key) per entry ──
    /// Small by construction — a handful of open accordion rows — so a sorted
    /// vector would buy nothing a linear scan does not already give.
    std::vector<std::uint32_t> in_place;
};


namespace ui {

// ── The takeover (full canvas) ─────────────────────────────────────────────

/// Whether this exact (surface, key) is the one that has taken the canvas.
bool is_expanded(const ui_state& ui, detail_surface s, int key = 0);

/// Whether a takeover is open at all — the Esc ladder and the canvases read this.
bool any_expanded(const ui_state& ui);

/// Give this (surface, key) the canvas, displacing whatever had it. Also closes
/// any open roll-up drill, so the takeover opens in its resting state.
void expand(ui_state& ui, detail_surface s, int key = 0);

/// Close the takeover. A no-op when none is open. Deliberately leaves the
/// in-place set untouched — see the header note on the Esc rung.
void fold(ui_state& ui);

// ── In place ───────────────────────────────────────────────────────────────

/// Whether this exact (surface, key) is expanded in place. Several may be.
bool is_open_in_place(const ui_state& ui, detail_surface s, int key = 0);

/// Add or remove this (surface, key) from the in-place set.
void set_open_in_place(ui_state& ui, detail_surface s, int key, bool open);

// ── The controls ───────────────────────────────────────────────────────────

/// Width (px) the control gutter reserves at a row's right edge. Callers that
/// lay a row out by hand (the Selection band's pager) need it; callers that just
/// append `disclosure_controls` do not.
float disclosure_gutter_width();

/// A row's trailing text — a verdict line — CLIPPED short of the control gutter.
///
/// Use it in place of `ImGui::TextColored` for the last text on a foldable row.
/// The gutter is a fixed column and the verdict is variable-length, so without this
/// a long verdict draws straight through the controls: legible in the 900 px wizard,
/// unreadable in the 380 px ledger column, and only ever visible by looking.
void gutter_text(unsigned int colour, const char* text);

/// The control pair, RIGHT-ALIGNED to a fixed gutter at the row's right edge.
///
///     Expandable item                                    ⌄  ›
///
/// Both controls sit in one vertical column across every row — not immediately
/// after the label, because a title's length must never move its controls
/// ("it is disorienting when a button is not in the same column, but does the
/// same job", Ben 2026-08-02). Call it after the row's text; it does its own
/// `SameLine`.
///
///   `⌄` expand in place, becoming `⌃` while open. Toggles.
///   `›` full canvas — opens the takeover.
///
/// @param in_place  false for a surface that already shows its content at rest
///                  (a fixed-rect card, a single-block view). Then only `›` is
///                  drawn, and it keeps the SAME rightmost column as everywhere
///                  else — the gutter's left slot is left empty rather than
///                  letting the full-canvas control slide sideways.
/// @return true on the frame either control changed state.
bool disclosure_controls(ui_state& ui, detail_surface s, int key = 0, bool in_place = true);

/// Open the full-canvas takeover for this (surface, key), if it is the open one.
///
/// Returns true when the caller should draw the overlay's body — and ONLY then must
/// the caller pair it with `fold_overlay_end()`. Returns false when this surface is
/// folded, in which case the caller draws nothing and calls nothing. (Deliberately
/// not ImGui's Begin/End contract, where End is unconditional: an overlay that is
/// usually closed reads better as a guard than as a block with an early-out.)
///
/// Bounded to `ui::canvas_rect()` (BL-265), so the nav rail, header, clock, comms
/// dock and Selection band all survive a takeover — the player never loses their
/// selection to open a chart. The one exception is `generation_stage`: the New
/// World wizard runs before the shell exists, so there is no canvas to take and no
/// chrome to preserve, and its takeover stays window-wide.
///
/// Draws its own header — a return control `‹` top-left, immediately before
/// @p title — and a Separator, so the body starts at the content. The return
/// control is top-left rather than in a right gutter because it acts on the WHOLE
/// VIEW and means "back", and back is top-left everywhere in software, including
/// this app's own zoom ladder.
bool fold_overlay_begin(ui_state& ui, detail_surface s, int key, const char* title);

/// Close an overlay that `fold_overlay_begin` returned true for.
void fold_overlay_end(ui_state& ui);


} // namespace ui
