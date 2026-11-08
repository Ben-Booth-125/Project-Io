#pragma once

// THE SITE FINDER (Ben, 2026-10-25): "When the construction tab is open, we should
// use a lens to show which tiles are valid, and open a ledger to compare tiles,
// sortable by profit. This should take into account logistics points also, and flag
// tiles which do not currently have logistics operating."
//
// THE GRID (Ben, 2026-11-08): "We should list building types in a grid, showing
// estimated returns without having to select a building type. If a province is
// clicked, it should restrict the search space to just buildings placed on that
// province." The grid sums what the table would list for every option, through the
// same gates and the same quote, spread over frames; a selected province scopes it.
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

#include "plot_history.hpp"
#include "ui_state.hpp"
#include "world/site_quote.hpp"

#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <string>
#include <vector>

namespace ui {

/// THE fold a building sits in, for the Build door and the finder alike ("Extraction",
/// "Processing", "Infrastructure", "Military"), so the two cannot file one building
/// twice. A processing facility reads its recipe: Power Generation is Infrastructure.
const char* construction_fold_of(const recipe_registry& reg, building_type t, std::uint16_t recipe_id);

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

/// The app's last economy report, which the quote reads for what producers really made.
/// The pointer must outlive the UI: it is the app's own record.
void set_quote_report(const economy_report* report);

/// THE quote a building would earn on @p tile (world/site_quote.hpp), for the Build door
/// and the site finder alike, so a row and the door quote one figure. Cached until the
/// frame stamp moves, because the door asks for every candidate on every frame.
site_quote quote_for_tile(world& w, const recipe_registry& reg, entity_id tile, building_type type,
                          resource_type target, std::uint16_t recipe, float lp_cover);

/// Under this many quarters of a recipe input on the market's shelf, the door and the
/// finder warn (Ben, 2026-11-05; the default he reads live; construction.md § The site
/// ledger).
inline constexpr float k_shelf_runway_warn_quarters = 4.0f;
/// How many quarters of the shelf's own trail the net drain averages over.
inline constexpr int k_shelf_trail_quarters = 4;

/// THE reading behind both warnings: a recipe input the plant would run through soon.
/// For each input, quarters = shelf / (the plant's use + the market's net drain, where
/// the shelf is already falling). Power is the grid's, so one shelf is not its reading.
/// Recipe inputs only: construction materials have their own stall reading.
struct shelf_runway
{
    bool          warn        = false;
    resource_type good        = resource_type::iron_ore; ///< The soonest input.
    float         quarters    = 0.0f;
    float         shelf       = 0.0f;
    float         plant_use   = 0.0f; ///< Units a quarter this plant would add, at its cover.
    bool          drain_known = false; ///< The shelf's trail has two samples. False after a load.
    float         drain       = 0.0f; ///< Units a quarter the shelf is falling, 0 if it is not.
    int           others      = 0;    ///< How many more inputs are under the threshold.
};

/// The market history whose shelf trail the runway reads. Must outlive the UI.
void set_quote_history(const market_plot_history* history);

/// Forget every cached quote, runway and finder evaluation, for a verify fixture that
/// writes a shelf: nothing in play moves a shelf between the stamps the cache keys on.
void invalidate_site_evaluation();

/// The runway of a building on @p tile at @p lp_cover, cached with the quote so the door
/// and the finder read one figure. Never warns for a plant that would not run at all.
shelf_runway shelf_runway_for(const world& w, const recipe_registry& reg, entity_id tile,
                              building_type type, std::uint16_t recipe, float lp_cover);

/// The warning's sentence, naming the good and the quarters. "" when it does not warn.
std::string shelf_runway_text(const shelf_runway& s);

/// Rebuild the finder's evaluation when its key moves (the chosen building, the
/// active body, the frame stamp, the treasury, the reach budget, the scope), and mark it live
/// for this frame. A no-op key costs a comparison.
void update_site_finder(world& w, const recipe_registry& reg, ui_state& ui);

/// Order @p rows by @p col (0 place, 1 profit, 2 cost, 3 market, 4 logistics, 5 yield);
/// every tie broken by tile id ascending, and an unpriced row always last.
void sort_site_rows(std::vector<site_row>& rows, int col, bool desc);

/// Draw the finder: the scope line, the building grid and, with a building chosen, the site ledger.
/// Calls `update_site_finder` itself.
void draw_site_finder(world& w, const recipe_registry& reg, ui_state& ui);

} // namespace ui
