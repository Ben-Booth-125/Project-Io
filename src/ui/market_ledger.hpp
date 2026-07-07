#pragma once

#include "plot_history.hpp"
#include "ui_state.hpp"
#include "world/world.hpp"

namespace ui {

/// Draws the Market Ledger window. open controls visibility (toggled by nav rail).
///
/// @param w       Read-only world (markets, bodies).
/// @param s       Current UI state (unused directly; passed for future extension).
/// @param history Per-market, per-resource price / supply / demand time series for
///                trend plots (BL-063). Empty series render as "(no data yet)".
/// @param open    Open/closed flag; cleared by the close button.
void draw_market_ledger(const world& w,
                        const ui_state& s,
                        const market_plot_history& history,
                        bool& open);

/// The city name of a market — the population centre anchoring its `centre_tile`
/// (`world::population_centre_name`), or the body name as a fallback when the market is
/// unanchored / unnamed. This is the market/city identity shown in the ledger's second
/// selector and emitted by the CSV export. See generation city naming.
std::string market_city_name(const world& w, entity_id market_id);

} // namespace ui
