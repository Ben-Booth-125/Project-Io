#pragma once

#include "ui_state.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_designs.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

namespace ui {

/// The unit designer — a full-canvas takeover (`detail_surface::unit_designer`,
/// keyed by land class; docs/ui/ledgers/military.md § The designer,
/// docs/ui/DRILL_THROUGH.md § The geometry).
///
/// "What is this kind of unit carrying, and what can it carry instead?" The class's
/// image fills the middle of the canvas; one box per kit slot sits beside it, joined
/// by a leader line to the point on the image that piece sits (unit_art.hpp). A box
/// shows the slot, the fitted item and its deltas; pressing it opens the slot's item
/// list — base first, each with its deltas against the fitted item, its goods per
/// unit, and, when locked, a lock badge naming the research node. A pick
/// `unit_design_check` refuses is disabled with the seam's reason. The summary
/// under the image is the design's stats and kit goods, previewing the hovered pick.
///
/// A PICK DRAFTS; SAVE COMMITS (Ben, 2026-10-28, NR-1096). A pick changes only the
/// open design's draft (`designer.draft`): the boxes and summary show the draft, and
/// a box whose draft differs from the live design is marked changed. Save queues
/// `designer.pending`, which app::render sends as one `set_unit_design` per changed
/// slot (the seam is per slot); Revert drops the draft. Any close — the toggle
/// press, the return control, Escape, another takeover — asked for with an unsaved
/// draft keeps the designer open and asks Save or Discard.
///
/// Draws nothing unless the designer is the open takeover. Call every frame.
/// BL-1450: ONE DESIGNER, TWO THEATRES. The same machine draws a land class
/// (`detail_surface::unit_designer`, key = land_class; `set_unit_design`) and an
/// air class (`detail_surface::air_designer`, key = air_class; slots airframe /
/// armament / avionics; `set_air_design`) through a theatre adapter, so the draft,
/// Save, Revert and the close catch hold for both. Save queues `designer.pending`
/// with each pick marked land or air; app::render sends the matching verb.
void draw_unit_designer(const world& w, const recipe_registry& reg, ui_state& s);

/// Open the designer on air class @p c, or close it if open there (the toggle rule).
/// The Military ledger Design tab's Air rows use this.
void toggle_air_designer(ui_state& s, air_class c);

/// Whether the designer is open on air class @p c.
bool air_designer_open_on(const ui_state& s, air_class c);

/// Open the designer for @p c — or, if it is already open on @p c, close it (the
/// toggle rule: the press that opened it closes it). Both entries use this: a land
/// class row in the Military ledger's Design tab, and the type name on a unit card.
void toggle_unit_designer(ui_state& s, land_class c);

/// Drop the designer's draft, its close prompt and any queued or answered Save:
/// called when the world is replaced (a load, a new campaign), so a draft from
/// another world is never offered.
void reset_unit_designer(ui_state& s);

/// Whether the designer is open on @p c.
bool unit_designer_open_on(const ui_state& s, land_class c);

/// The design the designer shows for @p c: @p nation's live design with the open
/// draft's picks over it (just the live design when the draft is for another
/// class). What the boxes, the summary — and a stat grid reading the open design —
/// read, so every reader agrees on what "the draft" is.
unit_design unit_designer_draft_design(const world& w, const ui_state& s, entity_id nation, land_class c);

/// `kit_stats` of `unit_designer_draft_design`: the drafted design's numbers.
land_combat_stats unit_designer_draft_stats(const world& w, const ui_state& s, entity_id nation, land_class c);

/// The slots of the open draft that differ from @p nation's live design (0 = clean).
int unit_designer_changed_slots(const world& w, const ui_state& s, entity_id nation);

} // namespace ui
