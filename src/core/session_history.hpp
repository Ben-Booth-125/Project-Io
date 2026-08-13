#pragma once

/// @file session_history.hpp
/// The post-step presentation work app::step_economy runs after the economy
/// resolves (BL-361 extraction): the nation-voiced agency comms lines (BL-212),
/// the persona counsel posts (BL-207), and the per-tick history recorders that
/// feed the header sparkline, ledger graphs and resource drill-downs. None of
/// this touches the simulation — it reads the tick's results into presentation
/// state the UI surfaces consume. Bodies moved verbatim from app.cpp.

#include "scripting/persona_pack.hpp"
#include "ui/chat_panel.hpp"
#include "ui/plot_history.hpp"
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
    ui::resource_history&                                 body_resource; ///< app::m_body_resource_hist
    ui::resource_history&                                 tile_resource; ///< app::m_tile_resource_hist
    std::unordered_set<entity_id>&                        tracked_tiles; ///< app::m_tracked_tiles
    std::vector<std::uint64_t>&                           sample_days;   ///< app::m_resource_hist_days
    std::uint64_t&                                        sample_index;  ///< app::m_resource_sample_index
};

/// Surface this tick's background-corp agency actions (BL-079) as NATION-voiced
/// Public comms lines (BL-212) — one line per nation, its heaviest event only.
void post_nation_agency_comms(const world& w, const economy_report& report,
                              ui::chat_state& chat, int day_tick);

/// Persona counsel (BL-207 slice 1): every corp due at this strategic-eval
/// boundary gets its seated bench's read of its own blackboard, posted to a
/// per-corp Counsel channel (lazily created on first use). Advisory only.
/// A pack that throws on live data clears the bench (BL-353) — hence non-const.
///
/// BL-398: the channel is created for every due corp, but only the corp whose
/// channel is currently OPEN (`chat.active_channel`) is actually evaluated —
/// the rest would write lines with no reader at ~1 s/tick. Presentation-only,
/// so the skip touches no simulation state. Splits its own cost into
/// step_economy_phase_ms()[9] (blackboard export) and [10] (pack evaluation).
void post_persona_counsel(const world& w, std::vector<persona::pack>& bench,
                          std::unordered_map<entity_id, int>& counsel_channel,
                          ui::chat_state& chat, int day_tick);

/// The per-tick history recorders: player balance/income/expenditure (BL-063),
/// the building profit-rank snapshot (BL-171), market price/supply/demand
/// series, and the resource-deposit series (BL-198, draining st.card_track_tile).
void record_histories(const world& w, const recipe_registry& reg,
                      const economy_report& report,
                      const std::unordered_map<entity_id, corp_cash_flow>& flows,
                      ui_state& st, history_stores& h);

} // namespace session_history
