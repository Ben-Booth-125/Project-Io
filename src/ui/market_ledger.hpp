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

/// The city name of a market — the population centre anchoring its `centre_tile`
/// (`world::population_centre_name`), or the body name as a fallback when the market is
/// unanchored / unnamed. This is the market/city identity shown in the ledger's second
/// selector and emitted by the CSV export. See generation city naming.
std::string market_city_name(const world& w, entity_id market_id);

} // namespace ui
