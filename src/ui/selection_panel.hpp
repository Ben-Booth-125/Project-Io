#pragma once

#include "ui_state.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

namespace ui {

/// Draw the Selection info element — a pinned panel showing detail of the
/// current selection (ui_state::selected_entity), docked in the **shell fold-out
/// column** (foldout_column_rect), where it is mutually exclusive with the ledgers:
/// a new selection closes any open ledger to take the column, and the Selection
/// draws only when no ledger owns the column (app gates it on ui::any_panel_open).
/// See docs/ui/SELECTION.md.
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
/// For a selected **tile** the panel also hosts the **build front door**
/// (docs/ui/SELECTION.md): the buildable types (via placement_rules), their cost,
/// and a build action that enqueues a construction request on that tile (executed
/// by app against the mutable world). This is the contextual, per-tile entry to
/// construction — distinct from the broad buildings overview, which earns a menu.
///
/// @param w        World state (the content source). Mutable because the building
///                 element operates its building directly — production method,
///                 workforce target, idle — the same way draw_construction_panel
///                 does. Destructive acts still take the deferred pending-request
///                 path (ui_state::construction), since erasing a building mid-draw
///                 would invalidate the iteration that is drawing it.
/// @param reg      Loaded registry — build costs for the build front door.
/// @param report   Most recent economy step report — the Population lens reads its
///                 body_habitability to show a population centre's workforce cap;
///                 the building element reads its per-building row for output/rate.
/// @param ui       UI state; read for the selection, written by 'go to' (focus),
///                 the close button (hide), and the build front door (enqueue).
///
/// The panel fills the shell fold-out column (foldout_column_rect); it takes no
/// layout parameters. Tile and building selections take dedicated vertical layouts;
/// the remaining kinds keep the older action|facts split.
void draw_selection_panel(world& w, const recipe_registry& reg,
                          const economy_report& report, ui_state& ui);

/// Draw the **tile construction ledger** (BL-162) — the tile-contextual surface that
/// actually lets the player build. Opened by the tile Selection element's "Construct
/// Buildings" button (which sets ui_state::show_build_ledger); reads
/// ui_state::selected_entity as the target tile. Fills the shell fold-out column
/// (foldout_column_rect), mutually exclusive with the Selection element and the
/// nav-rail ledgers — the app draws it in place of the Selection panel while its flag
/// is set and no nav ledger owns the column.
///
/// Lists every building type placeable on the tile (via placement_rules) with a
/// placeholder image, its full construction cost (budget + materials, from the
/// registry), a reason-coded validity read, and a Build action that enqueues a
/// construction request on the tile (ui_state::construction.pending_tile) — executed
/// by app against the mutable world, the same seam the placement-mode canvas click
/// uses. First pass: profit charting (per BL-162) is a follow-on; the images are
/// placeholders.
///
/// @param w    Read-only world (tile + deposits + player balance).
/// @param reg  Loaded registry — per-type build costs.
/// @param ui   UI state; read for the selected tile, written by the close button and
///             the Build action (enqueue).
void draw_construction_ledger(const world& w, const recipe_registry& reg, ui_state& ui);

} // namespace ui
