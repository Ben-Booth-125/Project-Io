#pragma once

#include "ui_state.hpp"
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
/// SAVING IS PER PICK. A live pick queues `designer.pending_*`, which app::render
/// sends as one `set_unit_design` (the seam is per slot): the choice is at once the
/// class's live design — new hires take it; units in the field take it by refit.
///
/// Draws nothing unless the designer is the open takeover. Call every frame.
void draw_unit_designer(const world& w, ui_state& s);

/// Open the designer for @p c — or, if it is already open on @p c, close it (the
/// toggle rule: the press that opened it closes it). Both entries use this: a land
/// class row in the Military ledger's Design tab, and the type name on a unit card.
void toggle_unit_designer(ui_state& s, land_class c);

/// Whether the designer is open on @p c.
bool unit_designer_open_on(const ui_state& s, land_class c);

} // namespace ui
