#pragma once

// ---------------------------------------------------------------------------
// BL-1454 — the unit stat grid (docs/ui/ledgers/military.md § The stat grid;
// docs/ui/SELECTION.md § The unit element's layout; Ben, 2026-10-29)
// ---------------------------------------------------------------------------
// Every stat a land unit fights with, in ONE grid, drawn by one widget that the
// unit card (Selection band) and the unit designer both call, so the two cannot
// disagree. A cell is a label and a value; the stored stats (HP, readiness,
// morale and the three carried stocks) carry a bar in the canvas counter's
// colours; a cell may carry a signed delta (the designer's, against the live
// design). Immediate-mode: the cells are rebuilt every frame from the world.
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/land_combat_stats.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <imgui.h>

#include <string>
#include <vector>

namespace ui {

/// One cell of the grid.
struct stat_cell
{
    std::string label;
    std::string value;          ///< "—" when the viewer may not see it.
    float       bar      = -1.0f; ///< 0..1 for a stored stat; < 0 draws no bar.
    ImU32       bar_col  = 0;
    bool        has_delta = false;
    int         delta     = 0;  ///< Signed, in the value's own units.
    bool        delta_pct = false; ///< The delta is a percentage point change.
};

/// Draw @p cells as a grid @p width wide (columns fitted to the width, 2..4).
/// @p id scopes the ImGui table.
void draw_stat_grid(const char* id, const std::vector<stat_cell>& cells, float width);

/// The design-stat cells of a land design: soft attack, hard attack, defence,
/// breakthrough, hardness (a percentage), recon, stealth, then speed in days per
/// plains tile. @p delta_from, when given, fills each cell's signed delta as
/// `s - *delta_from` (speed has none: it is the class's alone).
std::vector<stat_cell> land_design_cells(const land_combat_stats& s, float march_days,
                                         const land_combat_stats* delta_from = nullptr);

/// The unit card's cells for @p u: HP, readiness, morale, food, ammunition,
/// equipment, then `land_design_cells` of `unit_land_stats(u)`. A rival's internals
/// (the first six) always read "—" (BL-068); its design stats read "—" until the
/// player has seen it (`rival_unit_seen`).
std::vector<stat_cell> unit_card_cells(const world& w, const recipe_registry& reg, const unit_component& u,
                                       entity_id unit_id);

/// Has the player SEEN rival unit @p unit — fought it (a battle in `world::battles`
/// with the unit on one side and the player's nation on the other) or had it
/// revealed by recon (`world::air_revealed` for the player's nation)? The player's
/// own units are always seen.
bool rival_unit_seen(const world& w, entity_id unit);

} // namespace ui
