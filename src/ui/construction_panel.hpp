#pragma once

#include "world/building_profit.hpp" // building_profit — the per-building figure the roster sums
#include "world/economy_system.hpp" // economy_report
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <string>
#include <vector>

struct ui_state; // forward-declared; the full definition lives in ui_state.hpp.

namespace ui {

/// The NAME the Buildings view files @p b under — the same words the Build door
/// offers, so a player recognises the row as the thing they built:
/// `extraction_building_name(target_resource)` for an extraction site ("Quarry"),
/// the active recipe's own `group` for a processing facility ("Metal Foundry", the
/// BL-434 grouping vocabulary), and `building_type_name` for everything else.
///
/// The grouping key is the display name itself, deliberately: two buildings the
/// player would call the same thing belong on the same row, and nothing finer is
/// visible to them.
std::string building_group_name(const recipe_registry& reg, const building_component& b);

/// One row of the Buildings view: a building TYPE the player owns at least one of.
struct building_group
{
    std::string            name;    ///< Display name — the grouping key.
    int                    count = 0;   ///< How many the player owns.
    float                  total = 0.0f; ///< Sum of the members' per-quarter net profit.
    std::vector<entity_id> members;  ///< The buildings themselves, best net first.
};

/// The player's estate grouped by type — one entry per named building type the
/// player owns at least one of, each carrying a count and the summed per-quarter
/// net profit of its members.
///
/// `count` is every player building of that type, INCLUDING one still under
/// construction; `total` sums only the members `estimate_building_profit` can
/// actually price (`has_data`), a building with no estimate contributing zero
/// rather than a fabricated figure. The per-building number is the same one
/// `rank_player_buildings_by_profit` ranks the Balance ledger on, so the two
/// surfaces cannot report different profits for the same building.
///
/// Deterministic: groups sorted by name, members by net descending then by entity
/// id, so the walk over `world::buildings` (an unordered container) cannot vary the
/// order a player or a check sees.
std::vector<building_group> player_building_groups(const world& w,
                                                   const recipe_registry& reg,
                                                   const economy_report& report);

/// Draw the **Construction ledger** — nav rail slot 3, a button-strip nav across two
/// bounded views:
///
///  - **Buildings** (the default) — the player's estate grouped by type, one row per
///    type with a count and a total profit. Expanding a row lists that type's own
///    buildings; pressing one selects it and draws its levers (Method, Workforce),
///    which live here since Ben's 2026-08-29 ruling that the Selection card's centre
///    presents and never operates. It is the default because the queue is empty most
///    of the time and the player always owns buildings — opening on an empty queue
///    makes the ledger's front door an empty room.
///  - **Construction** — the collapsible build queue, then the tile-selection build
///    bar (`draw_construction_ledger_body`, selection_panel.cpp). ONE construction
///    element with TWO DOORS: this tab is what the rail slot opens and what the tile
///    Selection element's Construct button opens, so there is a single build bar.
///
/// Since BL-122 this is re-hosted into the shell fold-out column (foldout_column.hpp),
/// like the other named ledgers: it draws pinned + borderless into the shared column
/// rect, and is closed via the nav rail (accordion) rather than a title-bar 'x'. The
/// former BL-082 caller-supplied spawn/height-cap is gone — the column sits entirely
/// left of the Selection element, so no bottom-clearance anchoring is needed.
///
/// When *p_open is false the function draws nothing.
///
/// @param w          World — read for building/tile lookup; written by management
///                   controls (workforce_target, decommissioned, active_recipe_index).
/// @param reg        Loaded registry (recipe names and per-type economics).
/// @param report     Latest economy report (BL-143) — per-building active/idle status
///                   for the Buildings tab's status column.
/// @param state      Shared UI state — read (selected_entity) and written (construction).
/// @param p_open     Open/closed flag; gates whether the panel draws.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             const economy_report& report,
                             ui_state& state,
                             bool* p_open);

} // namespace ui
