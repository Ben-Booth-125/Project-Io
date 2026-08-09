#pragma once

#include "foldout_column.hpp" // foldout_rect, shell_column_width, minimap_*, comms_dock_rect

#include <imgui.h>

namespace ui {

/// **The shell's rect algebra — one owner (BL-216).**
///
/// Before this module `app.cpp` re-derived the right chrome column's left edge
/// (`disp.x - margin - mm_w`) in FIVE independent places — the minimap origin, the
/// time panel, the system gear, the header's right bound, and the Selection band's
/// right edge — with no owning function. Nothing held them together, and they had
/// already drifted: BL-312 flushed the minimap to the screen edge without the other
/// four following. Adding a region on top of that guarantees the next drift, so the
/// algebra lives here and the call sites ask for a rect.
///
/// This module **consumes** `shell_column_width` / `foldout_column_rect` /
/// `comms_dock_rect` (which stay in `foldout_column.hpp`, where the bottom strip's
/// height is derived) rather than duplicating them — the same rule it exists to
/// enforce on its callers.
///
/// Every width is rounded to a whole pixel by the function it comes from, so each
/// region edge lands on a pixel boundary rather than blurring across two.

/// A screen rect in pixels. BL-216 named this `shell_rect`; a second struct with
/// the same three members would be exactly the duplication this module removes, so
/// the name is an ALIAS of `foldout_column.hpp`'s `foldout_rect`. Callers may use
/// either spelling.
using shell_rect = foldout_rect;

/// The screen gutter `app.cpp` uses between chrome and the display edges. Same
/// value as `chrome_margin` (foldout_column.hpp) and deliberately so — the bottom
/// strip's height derivation assumes the two agree.
inline constexpr float shell_margin = 8.0f;

/// Width (px) of the **right chrome column** — the full-height band holding the time
/// panel on top, the freed middle slot, and the minimap at its foot. Equal to the
/// minimap width by construction: the column exists to give the minimap a place, and
/// the time panel was sized to match it so the right edge stays aligned.
float right_chrome_width(ImVec2 disp);

/// Left edge (px) of the right chrome column — `disp.x - shell_margin -
/// right_chrome_width(disp)`. This is the **clearance** edge: everything to its left
/// (header, Selection band, system gear) stops here.
///
/// The **minimap alone** is flush to the screen edge inside this column (BL-312,
/// Ben 2026-08-06: "wrap the minimap to the full edges of the canvas"), so it is
/// `shell_margin` wider than the column's other occupants and starts
/// `shell_margin` to the RIGHT of this edge. Ask `minimap_rect` for it rather than
/// assuming the two agree — that mismatch is the drift this module was written for.
float right_chrome_left(ImVec2 disp);

/// The minimap box, bottom-right. Flush to the right screen edge (BL-312); the
/// BOTTOM keeps its margin deliberately, because `selection_band_height` is
/// `minimap_height + chrome_margin` precisely so the bottom strip's top edge lands
/// on this rect's top edge. Flushing the bottom too would drop the minimap below the
/// strip and break the "one band across the full width" alignment.
shell_rect minimap_rect(ImVec2 disp);

/// The time panel, top-right — the head of the right chrome column. Height is
/// content-derived (container 5, LAYOUT.md), so the caller measures it and passes it
/// in; only the x/width/margin algebra belongs here.
shell_rect time_panel_rect(ImVec2 disp, float time_h);

/// Total horizontal budget (px) of the screen's **bottom strip** — the run the comms
/// dock and the Selection band share, from the icon rail's right edge to the right
/// chrome column.
///
/// Starts at `nav_pane_width`, not `shell_column_width`: BL-227 docked comms inside
/// the fold-out column's x-range (the icon rail keeps its full height and runs down
/// past the strip), so the strip begins at the rail's edge.
float bottom_band_budget(ImVec2 disp);

/// The Selection band — the bottom strip's right tile, from the comms dock's right
/// edge to the right chrome column. Flush against both by design: the strip carries
/// no internal gutter, and introducing the shell's only gap here would read as a
/// mistake. Height and top edge are `selection_band_height`'s, shared with the dock.
shell_rect selection_band_rect(ImVec2 disp);

/// The **freed centre-right slot** (BL-216) — the exact rect the comms log vacated
/// when BL-227 moved it to the bottom strip, now the home of the pinned-items watch
/// list. Sits between the time panel and the minimap; the right column's split stays
/// two-way elastic (content-derived time panel, ratio-locked minimap, pins take the
/// whole residual), so the minimap does not move.
///
/// @param time_h Measured height of the time panel, as passed to `time_panel_rect`.
shell_rect pinned_panel_rect(ImVec2 disp, float time_h);

} // namespace ui
