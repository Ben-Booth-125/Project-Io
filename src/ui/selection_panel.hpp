#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

namespace ui {

/// Draw the Selection info element — a pinned panel showing detail of the
/// current selection (ui_state::selected_entity), docked in the bottom-left
/// above the overlay-lens strip. See docs/ui/SELECTION.md.
///
/// The panel is **polymorphic by selection kind** (selection.hpp): it dispatches
/// on selection_kind_of to the matching per-entity content builder
/// (entity_summary.hpp). Its header carries a **'go to'** button (routes through
/// ui::focus_on_entity) and a **close** button that *hides* the panel until the
/// next selection (it is not destroyed, and has no navigation-rail entry).
///
/// The panel draws nothing when there is no valid selection, or when the current
/// selection has been dismissed. It is anchored by its **bottom** edge, matching
/// draw_overlay_controls' convention, so the caller passes the y to sit above.
///
/// @param w        Read-only world state (the content source).
/// @param ui       UI state; read for the selection, written by 'go to' (focus)
///                 and by the close button (hide).
/// @param left_x   Left edge of the panel, screen pixels (the nav-pane inner edge).
/// @param bottom_y Bottom edge to anchor the panel against, screen pixels.
void draw_selection_panel(const world& w, ui_state& ui, float left_x, float bottom_y);

} // namespace ui
