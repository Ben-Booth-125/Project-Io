#pragma once

/// @file session_history.hpp
/// The post-step presentation work app::step_economy runs after the economy
/// resolves (BL-361 extraction): the convoy-loss notes (BL-1534) and the per-tick
/// history recorders that feed the header sparkline, ledger graphs and resource
/// drill-downs. None of
/// this touches the simulation — it reads the tick's results into presentation
/// state the UI surfaces consume. Bodies moved verbatim from app.cpp.

#include "ui/plot_history.hpp"
#include "world/battle_system.hpp" // battle_dispatch (BL-1445: the per-day overload)
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct economy_report;
struct ui_state;

struct ui_state;

namespace session_history {

/// The app-owned, unserialised history stores step_economy appends to each tick
/// — a struct of references so the recorders keep writing app's own members
/// rather than growing a parallel state object.
struct history_stores
{
    std::vector<float>&                                   balance;       ///< app::m_balance_history
    std::vector<float>&                                   income;        ///< app::m_income_history
    std::vector<float>&                                   expenditure;   ///< app::m_expenditure_history
    ui::market_plot_history&                              market;        ///< app::m_market_history
    std::deque<std::unordered_map<entity_id, int>>&       building_rank; ///< app::m_building_rank_hist
};

/// BL-1534 (LOGISTICS.md § 7): keep this tick's interceptions the player is party to for a few
/// ticks, so the Convoys ledger states a lost cargo's cause and the canvas marks its tile. View
/// state only; older ones drop here.
void note_convoy_losses(const world& w, const economy_report& report, ::ui_state& ui_s);

/// The per-tick history recorders: player balance/income/expenditure (BL-063),
/// the building profit-rank snapshot (BL-171), market price/supply/demand
/// series.
void record_histories(const world& w, const recipe_registry& reg,
                      const economy_report& report,
                      const std::unordered_map<entity_id, corp_cash_flow>& flows,
                      ui_state& st, history_stores& h);

} // namespace session_history
