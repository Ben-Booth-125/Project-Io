#pragma once

#include "ui_state.hpp"
#include "world/contract_template.hpp" // contract_template_registry (BL-577: the contract card's predicate)
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdint>
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
///
/// **Method and Workforce are gone too** (Ben, 2026-08-29 — SELECTION.md § The centre
/// presents; it does not operate). Both pages' whole content was a control, and the
/// card's centre presents data and never operates: they are drawn by the Construction
/// ledger's Buildings view instead (`draw_production_method_section` /
/// `draw_building_workforce_page`, declared below and called from
/// `construction_panel.cpp` — the same bodies, relocated, never a second set of
/// controls writing `workforce_target` and `try_switch_recipe`). What is left here
/// REPORTS: Profitability, and Status.
enum class building_page_kind { profitability, status };

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
///
/// @p god_view (BL-408): spectator god view — a rival building additionally
/// offers the read-only Profitability page (pages carrying controls stay off
/// the rival card even then). Defaults false so every existing caller keeps
/// today's BL-068 page set unchanged.
std::vector<building_page> building_pages(const world& w, const recipe_registry& reg,
                                          const economy_report& report, entity_id id,
                                          bool god_view = false);

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
/// For a selected **tile** the layout's "Construct Buildings" button opens the
/// Construction ledger on its Construction view — the contextual, per-tile entry to
/// construction, and the SAME view the nav rail's slot 3 opens, so the two doors
/// cannot show two different build bars.
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
/// @param templates BL-577: the authored contract-template roster, read only
///                 for the contract card's predicate text (see
///                 selection_card.hpp's own comment on why this is a
///                 separate object from @p reg).
/// @param ui       UI state; read for the selection, written by 'go to' (focus),
///                 the close button (hide), and the tile "Construct Buildings" button.
void draw_selection_content(world& w, const recipe_registry& reg,
                            const economy_report& report,
                            const contract_template_registry& templates, ui_state& ui);

/// Draw the CURRENT page of the building Selection card's accordion (the page
/// named by ui_state::selection_building_page against that building's own
/// building_pages() list) at whatever size the caller's container gives it.
/// The full-canvas takeover (selection_card.cpp, detail_surface::building_metric)
/// uses this so the overlay renders the exact same page content the in-band
/// accordion does — the pager index is the fold key, same idiom as the tile
/// card's card_resource_page / tile_metrics pairing.
void draw_building_page_expanded(world& w, const recipe_registry& reg,
                                 const economy_report& report, ui_state& ui);

/// Real name lookup for a unit's roster type (`unit_component::type` indexes
/// `unit_roster_table()` directly). An out-of-range index falls back to the
/// honest "Type %u" the rest of the codebase uses for opaque indices.
///
/// Declared here (it was previously an undeclared file-local definition) so
/// the Contracts ledger's force picker (BL-576) can name a candidate unit
/// through the SAME lookup the Soldier card's Roster page uses, rather than
/// re-deriving the same name a second time and letting the two drift.
std::string unit_roster_display_name(std::uint16_t type);

/// Draw the **tile construction ledger** — the tile-contextual build bar that actually
/// lets the player build, INLINE into whatever window is current. It is a section of
/// the Construction ledger's Construction view (`construction_panel.cpp`), which is the
/// one place it is drawn: the nav rail's slot 3 and the tile Selection element's
/// Construct button are two doors onto that single view, so neither door can show a
/// build bar the other does not.
///
/// Reads `ui_state::selected_entity` as the target tile. With no tile selected it draws
/// **"Select a tile"** and nothing else — deliberately, not as a placeholder: a richer
/// empty state would have to list the tiles the player COULD build on, and any ordering
/// of that list is a recommendation (CONCEPT.md § the interface does not decide).
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
/// @param ui   UI state; read for the selected tile, written by the Build action
///             (enqueue).
void draw_construction_ledger_body(const world& w, const recipe_registry& reg, ui_state& ui);

/// The **Method** lever — the tiled recipe grid whose Switch buttons call
/// `try_switch_recipe`. Only meaningful for a `processing_facility`; draws nothing
/// for any other type, so a caller may call it unconditionally.
///
/// Lived on the building Selection card's accordion until Ben's 2026-08-29 ruling that
/// the card's centre presents and never operates. Declared here so the Construction
/// ledger's Buildings view can call THE SAME BODY rather than grow a second recipe
/// switcher: two sets of controls writing the same field is how two surfaces come to
/// disagree.
void draw_production_method_section(world& w, const recipe_registry& reg, entity_id id);

/// The **Workforce** lever — the placeholder trend graph plus the 0-100 slider that
/// writes `workforce_target` and clears `workforce_auto` (a manual edit pins the
/// target). Relocated off the Selection card by the same 2026-08-29 ruling as
/// `draw_production_method_section`, and shared for the same reason.
void draw_building_workforce_page(building_component& b);

} // namespace ui
