#pragma once

#include "ui_state.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ui {

/// Per-building profitability readout (BL-074) — the building's estimated net
/// per-tick contribution and the lines that make it up.
///
/// Declared here (it was previously an undeclared file-local definition) so the
/// Corporation dashboard's Production drill can show a building's economics through
/// the SAME builder the Selection band uses, rather than re-deriving the same sums
/// a second time and letting the two answers drift.
void draw_building_profit(const world& w, const recipe_registry& reg,
                          const economy_report& report, entity_id id);

/// One page of the unit Selection card's accordion — the pager the building card
/// once had (retired BL-1300), kept for the Soldier card (placeholder, BL-393 notes units are largely
/// inert today). Strength: the DERIVED strength (BL-459) with its count/quality/
/// supply derivation and the BL-454 upkeep line; Roster: roster-type name + owner.
enum class unit_page_kind { strength, roster };

/// A titled accordion page for the unit card.
struct unit_page
{
    std::string   label;
    unit_page_kind kind;
};

/// Every accordion page for @p id (a unit/unit-stack entity). Always both
/// pages today — a unit_component always carries strength/count/type/owner —
/// but kept as a list (not hard-coded two pages) so the shared draw dispatch
/// below stays the one thing both this and a future full-canvas takeover read,
/// one shared page list.
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
/// four-column tile element (preview, province card, tile card, action grid —
/// BL-1300) and draws NO header row; a **building** takes the same element for
/// its host tile with the building's stack expanded in the tile column; the
/// remaining kinds take their own layouts under a header row: the kind icon +
/// title and a **'go to'** button (routes through ui::focus_on_entity). There is
/// no close button — the Selection band is always open (BL-266).
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
/// @param ui       UI state; read for the selection, written by 'go to' (focus),
///                 the close button (hide), and the tile "Construct Buildings" button.
void draw_selection_content(world& w, const recipe_registry& reg,
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

/// The method grid's OFFER for building @p b: the browse indices (recipe_at) of
/// the rows the grid lists — same group as the active recipe, and runnable on
/// the building's body's air (the active recipe always listed). The one list
/// `draw_production_method_section` draws from; exposed so a verify check reads
/// what the grid offers rather than re-deriving it.
std::vector<int> method_grid_candidates(const world& w, const recipe_registry& reg,
                                        const building_component& b);

/// The method grid's Switch press onto ABSOLUTE recipe id @p recipe for building
/// @p id: `try_switch_recipe` for the player, and the refusal recorded for the
/// status line under the grid. The one body the grid's own button runs.
recipe_switch_result method_grid_press(world& w, const recipe_registry& reg, entity_id id,
                                       std::uint16_t recipe);

/// The status line the grid draws under building @p id's rows after a refused
/// press, or "" when there is none.
const char* method_grid_status(entity_id id);

/// The building whose grid actually DREW that status line on the grid's last
/// drawn frame, `null_entity` when it drew none. Read by the air_gate check.
entity_id method_grid_status_drawn_for();

/// The **Workforce** lever — the placeholder trend graph plus the 0-100 slider that
/// writes `workforce_target` and clears `workforce_auto` (a manual edit pins the
/// target). Relocated off the Selection card by the same 2026-08-29 ruling as
/// `draw_production_method_section`, and shared for the same reason.
void draw_building_workforce_page(building_component& b);

} // namespace ui
