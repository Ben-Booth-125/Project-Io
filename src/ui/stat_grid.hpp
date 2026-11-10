#pragma once

// ---------------------------------------------------------------------------
// THE STAT GRID (BL-1454; docs/ui/ledgers/military.md § The stat grid).
//
// "What is this unit, in numbers?" One grid, drawn by one function, used by every
// surface that shows a unit's or a design's numbers: the unit card and the unit group
// card on the Selection band, the wing card, and the land and air designers. So the
// card and the designer cannot disagree about what a number is or where it sits.
//
// A grid is a list of labelled BANDS - Condition, Supply, Combat, Field for a land
// unit - each named at its left, at most three cells a row (Combat wraps). A CELL is
// the stat's glyph (icons::stat) on the left and its value right-aligned; a stored
// stat carries a thin bar beneath in its counter colour; a group's cell carries its
// lowest-to-highest range in smaller type beneath; a designer's cell carries a signed
// delta against the live design. A cell has no text label: hovering it shows the
// stat's name and one line on what it does.
//
// The builders below turn a unit, a group, a wing or a design into bands; the draw
// knows nothing about units. Hidden values (a rival's internals, an unseen rival)
// are built as "—" by the builders, never by the draw.
// ---------------------------------------------------------------------------

#include "icons.hpp"

#include "world/recipe_registry.hpp"
#include "world/sea_combat.hpp" // ship_live_stats
#include "world/unit_designs.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <imgui.h>

#include <string>
#include <vector>

namespace ui {

/// One cell, built.
struct stat_cell
{
    icons::stat_glyph glyph = icons::stat_glyph::hp;
    const char* name   = "";     ///< The tooltip's title ("Soft attack").
    const char* what   = "";     ///< The tooltip's one line on what it does.
    std::string value;           ///< As drawn; "—" when hidden.
    std::string detail;          ///< An optional dim tooltip line ("48 of 50 men").
    std::string range;           ///< A group's "lo–hi"; empty otherwise.
    std::string delta;           ///< A designer's "+12" / "−5"; empty when no change.
    bool        delta_good = true; ///< Colours the delta: an improvement or a loss.
    float       bar = -1.0f;     ///< 0..1 under a stored stat; < 0 draws no bar.
    ImU32       bar_col = 0;
};

/// One labelled band of cells.
struct stat_band
{
    const char*            name = "";
    std::vector<stat_cell> cells;
};

using stat_bands = std::vector<stat_band>;

/// The height `draw_stat_grid` will take for @p bands, pixels.
float stat_grid_height(const stat_bands& bands);

/// Draw @p bands at the cursor, @p width wide, and advance the cursor past the grid.
/// @p id names the grid for the verify record (`stat_grids_drawn`) and its ImGui ids.
void draw_stat_grid(const char* id, const stat_bands& bands, float width);

// ---- The builders -----------------------------------------------------------

/// Has the player SEEN @p unit (a rival's)? Seen while it fights one of the player's
/// units in a live battle, or while the last air day revealed it to the player's nation
/// (`world::air_revealed`) - military.md § The stat grid: "in a battle ... or revealed
/// by recon". The player's own units are always seen.
bool unit_seen_by_player(const world& w, entity_id unit);

/// One land unit's bands: Condition, Supply, Combat, Field. A rival's internals read
/// "—"; an unseen rival's every cell does.
stat_bands unit_stat_bands(const world& w, const recipe_registry& reg, entity_id unit);

/// A unit group's bands: each cell the group's mean, its range beneath.
stat_bands unit_group_stat_bands(const world& w, const recipe_registry& reg,
                                 const std::vector<entity_id>& group);

/// A land design's bands: Combat and Field only (a design has no condition or stocks),
/// @p shown's numbers with each one's signed delta against @p live. Speed is the
/// class's alone and carries none.
stat_bands land_design_stat_bands(const recipe_registry& reg, land_class c,
                                  const land_combat_stats& shown, const land_combat_stats& live);

/// An air wing's bands: Condition (HP, morale, supply), Combat (air attack, soft
/// attack, hard attack, strategic), Field (recon, range). A rival's wing reads "—".
stat_bands wing_stat_bands(const world& w, const recipe_registry& reg, entity_id wing);

/// BL-1534: a ship's bands: Condition (HP, morale, supply), Combat (attack, detection, stealth),
/// Field (speed, range, and a transport's capacity). A rival's ship reads "—".
stat_bands ship_stat_bands(const world& w, const recipe_registry& reg, entity_id ship);

/// BL-1534: a ship design's bands, with signed deltas of @p shown against @p live. @p range is
/// the class's alone and carries none.
stat_bands ship_design_stat_bands(const ship_live_stats& shown, const ship_live_stats& live, int range);

/// An air design's bands: its HP at full under Condition, then Combat and Field, with
/// signed deltas of @p shown against @p live.
stat_bands air_design_stat_bands(const air_combat_stats& shown, const air_combat_stats& live);

// ---- The verify record --------------------------------------------------------

struct drawn_stat_cell
{
    std::string band, stat, value, range, delta;
    bool        bar = false;
    float       x = 0.0f, y = 0.0f; ///< The cell's centre, screen pixels.
};

struct drawn_stat_grid
{
    std::string                  id;
    int                          frame = -1; ///< ImGui frame it was drawn on.
    std::vector<drawn_stat_cell> cells;
};

/// Every grid drawn, one entry per id, its latest drawing. A reader keeps those whose
/// `frame` is the latest frame (`ImGui::GetFrameCount()`): the grids on screen now.
const std::vector<drawn_stat_grid>& stat_grids_drawn();

} // namespace ui
