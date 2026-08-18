#pragma once

#include "ui_state.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <string>
#include <vector>

namespace ui {

/// One page of the tile Selection band's metric accordion — a single comparable
/// figure for the selected tile against a reference (the top-decile tile for a
/// deposited resource, the body average for habitability / hazard).
///
/// Extracted from `draw_tile_selection` (BL-214) so the in-band chart and the
/// full-screen fold overlay chart the *same* page rather than two lists built by
/// imitation. The pager index into a `tile_metrics` vector is also the fold key,
/// which is what makes "expand THIS metric" addressable.
struct tile_metric
{
    std::string label;
    float       tile_val  = 0.0f;
    float       ref_val   = 0.0f;
    const char* ref_label = "";
    float       ceiling   = 1.0f;
    int         resource_index = -1; ///< -1 = not drillable (no time-series history kept).
};

/// Every metric page for @p tile, in pager order: each deposited resource, then the
/// tile's own habitability and hazard. Empty when @p tile is not a tile.
std::vector<tile_metric> tile_metrics(const world& w, entity_id tile);

/// Draw one metric page's clustered two-column chart into [@p mn, @p mx]. Shared by
/// the in-band accordion and the fold overlay, so the two cannot drift apart.
void draw_tile_metric_chart(ImDrawList* dl, ImVec2 mn, ImVec2 mx, const tile_metric& m);

/// Per-building profitability readout (BL-074) — the building's estimated net
/// per-tick contribution and the lines that make it up.
///
/// Declared here (it was previously an undeclared file-local definition) so the
/// Corporation dashboard's Production drill can show a building's economics through
/// the SAME builder the Selection band uses, rather than re-deriving the same sums
/// a second time and letting the two answers drift.
void draw_building_profit(const world& w, const recipe_registry& reg,
                          const economy_report& report, entity_id id);

/// One page of the building Selection card's accordion (supersedes BL-431's three
/// independently-toggled Method / Chain / Depth sections — a page IS the opened
/// state, so there is nothing left inside it to fold). Mirrors `tile_metric`'s role
/// for the tile card.
///
/// Chain, Depth and Lifecycle are gone (2026-08-15 playtest rework): Chain's
/// input-basket content folded into Profitability as a chart; Depth was cut
/// outright; Lifecycle's Mothball/Dismantle controls moved onto the building
/// card's action grid (see draw_building_selection_body) instead of a page.
enum class building_page_kind { profitability, method, workforce, status };

/// A titled accordion page — the pager label plus which content to dispatch.
struct building_page
{
    std::string        label;
    building_page_kind kind;
};

/// Every accordion page that applies to @p id, in pager order. The one list both
/// the in-band accordion (draw_building_selection_body) and the full-canvas
/// takeover (selection_card.cpp) read, so they cannot show different pages for
/// the same building — the same shared-list precedent as `tile_metrics`.
std::vector<building_page> building_pages(const world& w, const recipe_registry& reg,
                                          const economy_report& report, entity_id id);

/// Draw one accordion page's content, dispatched by kind. Shared by the in-band
/// accordion and the full-canvas takeover so the two cannot drift apart. Takes
/// `ui_state&` (was world+reg+report+id only) because the Lifecycle page's
/// Dismantle control defers through `ui.construction.pending_demolish` — the
/// same deferred-erase seam the old construction-panel detail used, needed
/// here because erasing from `w.buildings` mid-draw would invalidate the
/// iteration that is drawing it.
void draw_building_page(world& w, const recipe_registry& reg, const economy_report& report,
                        entity_id id, building_page_kind kind, ui_state& ui);

/// One page of the unit Selection card's accordion — mirrors building_page_kind's
/// role for the new Soldier card (placeholder, BL-393 notes units are largely
/// inert today). Strength: the DERIVED strength (BL-459) with its count/quality/
/// supply derivation and the BL-454 upkeep line; Roster: roster-type name + owner.
enum class unit_page_kind { strength, roster };

/// A titled accordion page for the unit card — mirrors `building_page`.
struct unit_page
{
    std::string   label;
    unit_page_kind kind;
};

/// Every accordion page for @p id (a unit/unit-stack entity). Always both
/// pages today — a unit_component always carries strength/count/type/owner —
/// but kept as a list (not hard-coded two pages) so the shared draw dispatch
/// below stays the one thing both this and a future full-canvas takeover read,
/// the same precedent as tile_metrics/building_pages.
std::vector<unit_page> unit_pages(const world& w, entity_id id);

/// Draw one unit accordion page's content, dispatched by kind. Read-only —
/// unlike the building card, nothing on this placeholder card mutates the
/// unit. Takes the registry since BL-454, for the strength page's upkeep line.
void draw_unit_page(const world& w, const recipe_registry& reg,
                    entity_id id, unit_page_kind kind);

/// Draw the **Selection content** — the polymorphic detail of the current
/// selection (ui_state::selected_entity), emitted into whatever window the caller
/// has opened. This is **frame-agnostic**: it owns no window and takes no layout
/// parameters, sizing itself from the live content region. Since BL-195 the sole
/// caller is the **Selection band** (selection_card.cpp) — the former fold-out
/// Selection panel is gone, and the shell column is ledgers-only. See
/// docs/ui/SELECTION.md, docs/ui/TOOLTIP.md.
///
/// The content is **polymorphic by selection kind** (selection.hpp): it dispatches
/// on selection_kind_of to the matching per-entity layout — a **tile** takes the
/// BL-123 vertical stack (placeholder image, per-resource production graphs, action
/// grid); a **player building** takes its operate-the-building layout; the remaining
/// kinds take the action|facts split. It draws a header row: the kind icon + title
/// and a **'go to'** button (routes through ui::focus_on_entity). There is no
/// close button — the Selection band is always open (BL-266).
///
/// The content draws nothing when there is no valid selection. The band frame
/// never lets that happen: with no valid selection it substitutes the player's
/// own corporation (the BL-266 resting state) before calling here.
///
/// For a selected **tile** the layout's "Construct Buildings" button opens the tile
/// construction ledger (ui_state::show_build_ledger) in the shell column — the
/// contextual, per-tile entry to construction, distinct from the broad buildings
/// overview which earns a menu.
///
/// @param w        World state (the content source). Mutable because the building
///                 layout operates its building directly — production method,
///                 workforce target, idle — the same way draw_construction_panel
///                 does. Destructive acts still take the deferred pending-request
///                 path (ui_state::construction), since erasing a building mid-draw
///                 would invalidate the iteration that is drawing it.
/// @param reg      Loaded registry — build costs / recipe economics.
/// @param report   Most recent economy step report — the building layout reads its
///                 per-building row for output/rate; the Population facts read its
///                 body_habitability for a population centre's workforce cap.
/// @param ui       UI state; read for the selection, written by 'go to' (focus),
///                 the close button (hide), and the tile "Construct Buildings" button.
void draw_selection_content(world& w, const recipe_registry& reg,
                            const economy_report& report, ui_state& ui);

/// Draw the CURRENT page of the building Selection card's accordion (the page
/// named by ui_state::selection_building_page against that building's own
/// building_pages() list) at whatever size the caller's container gives it.
/// The full-canvas takeover (selection_card.cpp, detail_surface::building_metric)
/// uses this so the overlay renders the exact same page content the in-band
/// accordion does — the pager index is the fold key, same idiom as the tile
/// card's card_resource_page / tile_metrics pairing.
void draw_building_page_expanded(world& w, const recipe_registry& reg,
                                 const economy_report& report, ui_state& ui);

/// Draw the **tile construction ledger** (BL-162) — the tile-contextual surface that
/// actually lets the player build. Opened by the tile Selection element's "Construct
/// Buildings" button (which sets ui_state::show_build_ledger); reads
/// ui_state::selected_entity as the target tile. Fills the shell fold-out column
/// (foldout_column_rect), mutually exclusive with the Selection element and the
/// nav-rail ledgers — the app draws it in place of the Selection panel while its flag
/// is set and no nav ledger owns the column.
///
/// Lists every building type placeable on the tile (via placement_rules) with its
/// full construction cost as one credit total (build cost plus materials priced at
/// the local market), an expected net-per-tick magnitude bar drawn on a ceiling
/// shared across the list, the payback that capex implies, a reason-coded validity
/// read, and a Build action that enqueues a construction request on the tile
/// (ui_state::construction.pending_tile) — executed by app against the mutable
/// world, the same seam the placement-mode canvas click uses. Candidates sort by
/// expected net descending (BL-162), so the best options need no scrolling.
///
/// @param w    Read-only world (tile + deposits + player balance).
/// @param reg  Loaded registry — per-type build costs.
/// @param ui   UI state; read for the selected tile, written by the close button and
///             the Build action (enqueue).
void draw_construction_ledger(const world& w, const recipe_registry& reg, ui_state& ui);

} // namespace ui
