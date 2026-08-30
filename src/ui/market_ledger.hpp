#pragma once

#include "plot_history.hpp"
#include "ui_state.hpp"
#include "world/world.hpp"

namespace ui {

/// Draws the Market Ledger window. open controls visibility (toggled by nav rail).
///
/// @param w       Read-only world (markets, bodies).
/// @param s       Current UI state — mutated to track the ledger's own tab
///                (`market_ledger_view`) and to consume a pending focus request
///                (`market_ledger_focus`, BL-159) that jumps the selectors to a
///                given market and opens Sell Orders.
/// @param history Per-market, per-resource price / supply / demand time series for
///                trend plots (BL-063). Empty series render as "(no data yet)".
/// @param open    Open/closed flag; cleared by the close button.
///
/// The Sell Orders tab reads standing orders from `world::sell_orders` (BL-293)
/// and, since the book is world state, ADDS/REMOVES by enqueuing a `corp_command`
/// onto `s.pending_order_commands` — the const `world&` here cannot be mutated
/// directly, so `app::render` applies the request through `apply_corp_command`.
///
/// The Convoys tab (BL-453) does the same for `world::convoys`: one row per
/// in-flight convoy of the player's corp — cargo, endpoints, mode, progress,
/// TICKS TO ARRIVAL and the haul cost already paid — with a Hold press that
/// enqueues `hold_convoy`. It is the only surface that reports arrival time;
/// the three canvases draw convoys but list none of this. Unlike Sell Orders it
/// is NOT scoped to the selected market: "what is on its way to me" spans the
/// whole corp.
void draw_market_ledger(const world& w,
                        ui_state& s,
                        const market_plot_history& history,
                        bool& open);

/// One row of the Goods table AS DRAWN (BL-686). Recorded by `draw_goods_tab`
/// each frame and read by the verify layer.
///
/// It records what the surface actually computed and put on screen, so a check
/// can cross it against `market_component` rather than re-deriving the same
/// figures and comparing them to themselves — which is what a check written
/// against the world alone would do, and it would pass against a table that drew
/// nothing at all. `expect_no_clipping` is vacuous on this class of surface
/// (NR-663: zero records over visibly clipped frames), so this is the assertion
/// that has to carry the weight.
struct goods_row_record
{
    resource_type resource{};
    const char*   name        = "";
    float         price       = 0.0f;  ///< `market_component::price[r]` as drawn.
    float         base_price  = 0.0f;  ///< `market_component::base_price[r]`.
    float         body_avg    = 0.0f;  ///< Mean price across the body's markets.
    float         vs_base     = 0.0f;  ///< `price / base_price`, the drawn column.
    int           samples     = 0;     ///< Points the row's graph plotted (<= 8).
    float         name_avail  = 0.0f;  ///< Px the name column actually offered.
    float         name_needed = 0.0f;  ///< Px the full name needed at this font.
};

/// One chip of the nation presence row (BL-688), as drawn.
struct nation_chip_record
{
    entity_id   nation = null_entity;
    std::string name;
    std::string initials;
};

/// The nation presence row's chips as last drawn, in draw order.
const std::vector<nation_chip_record>& nation_chips();

/// The Goods table's rows as last drawn, in draw order. Empty when the Goods view
/// was not on screen.
const std::vector<goods_row_record>& goods_rows();

/// The market the ledger last drew the Goods table for (`null_entity` if none).
entity_id goods_market();

/// The city name of a market — the population centre anchoring its `centre_tile`
/// (`world::population_centre_name`), or the body name as a fallback when the market is
/// unanchored / unnamed. This is the market/city identity shown in the ledger's second
/// selector and emitted by the CSV export. See generation city naming.
std::string market_city_name(const world& w, entity_id market_id);

} // namespace ui
