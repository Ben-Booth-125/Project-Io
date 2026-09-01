#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

namespace ui {

/// Draws the Convoys ledger — nav rail slot 7, directly after Market.
///
/// "What is on its way, and what is it costing me?" One row per convoy of the
/// player's corp in flight: cargo and quantity, origin -> destination, a progress
/// bar carrying the ETA in **qtr**, and the haul cost already paid. A Hold press
/// enqueues `hold_convoy` onto `s.pending_order_commands` — the const `world&`
/// here cannot be mutated, so `app::render` applies it through
/// `apply_corp_command`, which is what keeps the player's Hold and an agent's
/// `hold_convoy` the same act.
///
/// WHY IT IS ITS OWN LEDGER (BL-689). It was the Market ledger's third tab and it
/// was never a market question: `MARKETS.md` owns clearing and the order book,
/// while a convoy is cargo in transit and belongs to `SUPPLY.md` — "Logistics is
/// the road, Supply is the traffic". A tab strip can arm only one lens, so
/// opening the Market ledger for a logistics read armed the price wash; its own
/// slot arms `supply_routes`, the literal map twin of this list.
///
/// NO BODY/MARKET SELECTORS, deliberately. A convoy is a route between two places
/// and is not scoped by the market you happen to be looking at — part of why the
/// tab sat oddly beside two market-scoped views. The list spans the whole corp.
///
/// @param w    Read-only world (convoys, markets, population centres).
/// @param s    UI state — mutated only to enqueue the Hold/Release command.
/// @param open Open/closed flag; toggled by the nav rail.
void draw_convoys_ledger(const world& w, ui_state& s, bool& open);

} // namespace ui
