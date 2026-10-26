#pragma once

// THE SITE FINDER (Ben, 2026-10-25): "When the construction tab is open, we should
// use a lens to show which tiles are valid, and open a ledger to compare tiles,
// sortable by profit. This should take into account logistics points also, and flag
// tiles which do not currently have logistics operating."
//
// BUILDING FIRST. The player picks a building in the Construction view; the
// Planetary canvas marks every tile of the player's own ground the Build door would
// accept it on (`placement_rules::can_place_in_world`, the door's era / tech / air
// filters, and the treasury), flags the ones no Logistic Point source reaches, and a
// ledger lists those tiles - estimated profit (the door's own figure, cover applied),
// cost, market, Logistic Point cover, yield - sorted by profit. A row press selects
// and frames the tile; the place action stays the tile's Build door below it.
//
// docs/ui/ledgers/construction.md § The site ledger owns the surface;
// docs/ui/LENSES.md § The placement lens owns the map half.
//
// VIEW state only. Nothing here writes the simulation: the evaluation reads the world
// (it needs a mutable one only for the Logistic Point memo, the same reason
// `update_body_throughput` does) and publishes into `ui_state::site_finder`.

#include "ui_state.hpp"

#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <vector>

namespace ui {

/// Every building the finder offers, in the Build door's vocabulary and fold order:
/// one extraction option per extractable good, one per processing RECIPE (the door
/// prices a recipe, and the recipe is what the profit turns on), then the
/// infrastructure and military types. The door's era band, tech and recipe-unlock
/// filters apply; the air filter is per tile and applied in the evaluation.
std::vector<site_option> site_options(const world& w, const recipe_registry& reg);

/// The Logistic Point cover a building on @p tile would get (`lp_route_to(...).cover`
/// for the nation whose ground it is), cached until the frame stamp moves (body-free:
/// the economy tick, the building count, the logistics generation). @p blocked receives
/// whether a source reached it without a chain to the capital. 0 on nobody's ground.
float site_lp_cover(world& w, entity_id tile, bool* blocked = nullptr);

/// Rebuild the finder's evaluation when its key moves (the chosen building, the
/// active body, the frame stamp, the treasury, the reach budget), and mark it live
/// for this frame. A no-op key costs a comparison.
void update_site_finder(world& w, const recipe_registry& reg, ui_state& ui);

/// Order @p rows by @p col (0 place, 1 profit, 2 cost, 3 market, 4 logistics, 5 yield);
/// every tie broken by tile id ascending, and an unpriced row always last.
void sort_site_rows(std::vector<site_row>& rows, int col, bool desc);

/// Draw the finder: the building picker and, with a building chosen, the site ledger.
/// Calls `update_site_finder` itself.
void draw_site_finder(world& w, const recipe_registry& reg, ui_state& ui);

} // namespace ui
