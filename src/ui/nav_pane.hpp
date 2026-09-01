#pragma once

#include "ui_state.hpp"

namespace ui {

/// Draw the left navigation pane.
///
/// A fixed, full-height **icon rail** at the left edge of the window holding a
/// vertical strip of ten square icon slots. Each slot will eventually open one of
/// the game's menus or ledgers; the slot shows a vector glyph (see
/// ui::icons) rather than a worded label, with the menu name in a hover tooltip.
/// For Layer 2 only one slot is wired — the Tile Ledger — and it toggles
/// state.show_tile_ledger; the remaining nine are reserved placeholders.
///
/// @param state      Shared UI state; the Tile Ledger tab toggles show_tile_ledger.
/// @param top_offset Y position of the pane's top edge, in pixels. The profile
///                   panel occupies the top-left corner, so the pane starts
///                   below it.
void draw_nav_pane(ui_state& state, float top_offset = 0.0f);

/// Width of the navigation pane in pixels. Exposed so the caller can lay out
/// other panels clear of the pane. Narrow, since the pane is an icon rail rather
/// than a worded menu (the profile aligns to its own width, not this one).
inline constexpr float nav_pane_width = 56.0f;

/// Close every fold-out ledger/panel so at most one column occupant is shown.
/// The Selection element shares the fold-out column and is mutually exclusive with
/// the ledgers, so a *new* entity selection calls this to take the column (app),
/// exactly as opening a ledger from the rail does.
void close_all_panels(ui_state& state);

/// Whether any fold-out ledger/panel currently owns the column. The Selection
/// element only draws when this is false (the ledger wins the shared slot).
bool any_panel_open(const ui_state& state);

/// Verify-only: the screen-space centre of rail slot @p slot (1-based), as it was
/// ACTUALLY drawn on the last frame. Writes `out_x` / `out_y` and returns true; a
/// slot number outside the drawn range returns false and writes nothing.
///
/// Why it reports the drawn position rather than recomputing the geometry: a check
/// that derives the rect from the same constants the draw uses cannot catch a
/// renumber, and the renumber is the risk (NR-716 — `shell_pass` opens ledgers by
/// NAME, so it is blind to a slot->surface mapping that has silently shifted). A
/// script clicks this point and asserts which surface opened, which is the only
/// form of the check that actually exercises the mapping.
bool nav_slot_centre(int slot, float& out_x, float& out_y);

} // namespace ui
