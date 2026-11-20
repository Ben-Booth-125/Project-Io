/// @file session_history.cpp
/// See session_history.hpp — bodies moved verbatim from app::step_economy
/// (BL-361); behaviour unchanged.

#include "core/session_history.hpp"

#include "core/app.hpp"      // step_economy_phase_ms — the shared phase accumulators
#include "core/battle_dispatch_text.hpp" // interception_line — the Convoys ledger's Lost words (BL-1534)
#include "core/sim_loop.hpp"
#include "ui/balance_ledger.hpp"
#include "ui/ui_state.hpp"
#include "world/corp_ai.hpp"
#include "world/economy_system.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <utility>

namespace session_history {

void note_convoy_losses(const world& w, const economy_report& report, ::ui_state& ui_s)
{
    constexpr int k_ticks_kept = 3; // LOGISTICS.md § 7: "marked for a few ticks"
    const int now = w.current_econ_tick;
    auto& kept = ui_s.convoy_losses;
    kept.erase(std::remove_if(kept.begin(), kept.end(),
                              [&](const ::convoy_loss& l) { return now - l.tick >= k_ticks_kept; }),
               kept.end());
    const entity_id me = player_nation(w);
    for (const interception_record& r : report.interceptions)
    {
        if (r.outcome == interception_outcome::returned_by_front)
            continue; // it came home whole: nothing was lost
        std::string line = interception_line(w, r, me);
        if (!line.empty())
            kept.push_back({std::move(line), r.tile, now, r.victim_corp == me});
    }
}

void record_histories(const world& w, const recipe_registry& reg,
                      const economy_report& report,
                      const std::unordered_map<entity_id, corp_cash_flow>& flows,
                      ui_state& st, history_stores& h)
{
    // Record player balance, income, and expenditure for the header sparkline and
    // the Budget ledger's profit chart (BL-063).  All three are capped at plot_history_cap.
    // BL-1406: the player's money is its nation's treasury once folded —
    // `purse_of(player_actor)` reads either owner, and the cash flows are keyed
    // by the same owner id.
    {
        const entity_id actor = player_actor(w);
        const float*    purse = purse_of(w, actor);
        ui::push_capped(h.balance, purse != nullptr ? *purse : 0.0f);

        const auto fit = flows.find(actor);
        ui::push_capped(h.income,
            fit != flows.end() ? fit->second.income : 0.0f);
        ui::push_capped(h.expenditure,
            fit != flows.end() ? fit->second.expenditure : 0.0f);
    }

    // Snapshot the player-building profit ranking for the Budget ledger's rank-change
    // column (BL-171): keep a year and one (this tick + the year's ticks prior; BL-1642,
    // the tick is a month, so 13 snapshots and the oldest is a year back).
    {
        std::unordered_map<entity_id, int> ranks;
        const auto ranking = ui::rank_player_buildings_by_profit(w, reg, report);
        for (int i = 0; i < static_cast<int>(ranking.size()); ++i)
            ranks[ranking[i].first] = i;
        h.building_rank.push_back(std::move(ranks));
        if (h.building_rank.size() > static_cast<std::size_t>(k_econ_ticks_per_year + 1))
            h.building_rank.pop_front();
    }

    // Record market price / supply / demand snapshots for the market ledger graphs.
    for (const auto& [mid, mc] : w.markets)
    {
        auto& mh = h.market[mid];
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (mc.base_price[r] <= 0.0f)
                continue; // resource not traded here; skip
            ui::push_capped(mh[r].price,  mc.price[r]);
            ui::push_capped(mh[r].supply, mc.supply[r]);
            ui::push_capped(mh[r].demand, mc.demand[r]);
            ui::push_capped(mh[r].shelf,  mc.inventory[r]);
        }
    }

}

} // namespace session_history
