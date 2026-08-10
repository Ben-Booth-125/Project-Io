/// @file session_history.cpp
/// See session_history.hpp — bodies moved verbatim from app::step_economy
/// (BL-361); behaviour unchanged.

#include "core/session_history.hpp"

#include "core/sim_loop.hpp"
#include "ui/balance_ledger.hpp"
#include "ui/ui_state.hpp"
#include "world/corp_ai.hpp"
#include "world/economy_system.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>

namespace session_history {

void post_nation_agency_comms(const world& w, const economy_report& report,
                              ui::chat_state& chat, int day_tick)
{
    // Surface this tick's background-corp agency actions (BL-079) as NATION-
    // voiced Public comms lines (BL-212). A rival's internals stay private —
    // the old per-corp, per-building text here broke DISCOVERY.md's already-
    // settled competitor-visibility rule ("rival internals are private... a
    // public AGGREGATE signal is the deliberate one"), the same shape as the
    // market layer already follows. Only the corp's HOME NATION ever posts,
    // phrased as its own first-person statement, and only the tick's single
    // heaviest event per nation — never a full account of everything that
    // happened underneath it.
    {
        auto severity_of = [](agency_event::kind k) -> int {
            switch (k)
            {
            case agency_event::kind::idled:            return 4;
            case agency_event::kind::demolished:       return 4;
            case agency_event::kind::built:             return 3;
            case agency_event::kind::recipe_switch:     return 2;
            case agency_event::kind::resumed:           return 2;
            case agency_event::kind::hired:             return 3;
            case agency_event::kind::road_placed:       return 1;
            case agency_event::kind::workforce_set:     return 1;
            case agency_event::kind::survey_dispatched: return 1;
            }
            return 0;
        };

        std::unordered_map<entity_id, const agency_event*> best_per_nation;
        for (const agency_event& ev : report.agency_events)
        {
            const auto cit = w.corporations.find(ev.corp);
            if (cit == w.corporations.end() || cit->second.home_nation == null_entity)
                continue; // no home nation to speak on this corp's behalf
            const entity_id nation = cit->second.home_nation;
            const auto it = best_per_nation.find(nation);
            if (it == best_per_nation.end() || severity_of(ev.what) > severity_of(it->second->what))
                best_per_nation[nation] = &ev;
        }

        // Sorted ids so the post order is deterministic (unordered_map iteration
        // order is not) — mirrors the sorted corp_ids pattern in the counsel
        // block just below.
        std::vector<entity_id> nation_ids;
        nation_ids.reserve(best_per_nation.size());
        for (const auto& [nid, ev] : best_per_nation)
            nation_ids.push_back(nid);
        std::sort(nation_ids.begin(), nation_ids.end());

        for (const entity_id nid : nation_ids)
        {
            std::string text;
            switch (best_per_nation[nid]->what)
            {
            case agency_event::kind::recipe_switch:
                text = "We are adjusting output priorities in a domestic processing sector.";
                break;
            case agency_event::kind::idled:
                text = "We confirm an easing of activity in a strained sector.";
                break;
            case agency_event::kind::built:
                text = "We welcome new private investment within our borders.";
                break;
            case agency_event::kind::demolished:
                text = "We note the retirement of an aging facility.";
                break;
            case agency_event::kind::workforce_set:
                text = "We are adjusting domestic labour allocation.";
                break;
            case agency_event::kind::resumed:
                text = "We report resumed operations in a recovering sector.";
                break;
            case agency_event::kind::road_placed:
                text = "We announce new infrastructure investment.";
                break;
            case agency_event::kind::survey_dispatched:
                text = "We confirm new exploratory activity within our claims.";
                break;
            case agency_event::kind::hired:
                text = "We acknowledge the mustering of a private security formation on our soil.";
                break;
            }
            ui::chat_post(chat, day_tick, nid, 0, std::move(text));
        }
    }
}

void post_persona_counsel(const world& w, std::vector<persona::pack>& bench,
                          std::unordered_map<entity_id, int>& counsel_channel,
                          ui::chat_state& chat, int day_tick)
{
    // Persona counsel (BL-207 slice 1): every corp due at this strategic-eval
    // boundary gets its seated bench's read of its own blackboard, posted to a
    // per-corp Counsel channel (lazily created on first use). Counsel is
    // advisory only — it never touches the world, only the chat log.
    if (!bench.empty())
    {
        const int tick = day_tick;
        std::vector<entity_id> corp_ids;
        corp_ids.reserve(w.corporations.size());
        for (const auto& [id, cc] : w.corporations)
            corp_ids.push_back(id);
        std::sort(corp_ids.begin(), corp_ids.end());

        for (const entity_id corp : corp_ids)
        {
            if (!corp_strategic_eval_due(w, corp, tick))
                continue;

            const corp_blackboard bb = export_corp_blackboard(w, corp, tick);
            int channel = -1;
            if (const auto it = counsel_channel.find(corp); it != counsel_channel.end())
                channel = it->second;
            else
            {
                ui::chat_channel ch;
                const auto cnit = w.corporations.find(corp);
                ch.name = std::string("Counsel: ")
                        + (cnit != w.corporations.end() ? cnit->second.name : "(unknown)");
                ch.members = { corp, w.player_entity };
                chat.channels.push_back(std::move(ch));
                channel = static_cast<int>(chat.channels.size()) - 1;
                counsel_channel[corp] = channel;
            }

            try
            {
                for (const persona::pack& p : bench)
                {
                    if (p.is_verdict_bench())
                        continue; // slice 1: renders the hunting benches' reads, not aggregated verdicts yet
                    const std::vector<persona::opinion_record> ops = p.evaluate(bb);
                    if (ops.empty())
                        continue;
                    // Bound the chat to one line per pack per eval: the heaviest opinion.
                    const persona::opinion_record* top = &ops.front();
                    for (const auto& op : ops)
                        if (op.w > top->w) top = &op;
                    ui::chat_post(chat, tick, corp, channel,
                                 p.id() + ": " + p.phrase_for(*top));
                }
            }
            catch (const std::exception& e)
            {
                // BL-353: same policy as the load-time guard in load_economy — a
                // pack that loads clean but throws on live data (sol2 errors
                // surface as std::runtime_error) disables counsel rather than
                // killing the session mid-tick. One visible line in the counsel
                // channel; the detail goes to stderr like the load failure does.
                std::fprintf(stderr, "ProjectIo: persona counsel packs disabled: %s\n", e.what());
                ui::chat_post(chat, tick, corp, channel,
                              "The counsel bench has been dismissed: an advisory pack failed.");
                bench.clear();
                break; // bench gone; nothing left to evaluate for the remaining corps
            }
        }
    }
}

void record_histories(const world& w, const recipe_registry& reg,
                      const economy_report& report,
                      const std::unordered_map<entity_id, corp_cash_flow>& flows,
                      ui_state& st, history_stores& h)
{
    // Record player balance, income, and expenditure for the header sparkline and
    // economy panel graphs (BL-063).  All three are capped at plot_history_cap.
    {
        const auto cit = w.corporations.find(w.player_entity);
        ui::push_capped(h.balance,
            cit != w.corporations.end() ? cit->second.balance : 0.0f);

        const auto fit = flows.find(w.player_entity);
        ui::push_capped(h.income,
            fit != flows.end() ? fit->second.income : 0.0f);
        ui::push_capped(h.expenditure,
            fit != flows.end() ? fit->second.expenditure : 0.0f);
    }

    // Snapshot the player-building profit ranking for the Budget ledger's rank-change
    // column (BL-171): keep the last 5 (this tick + the 4 prior = ~a year back).
    {
        std::unordered_map<entity_id, int> ranks;
        const auto ranking = ui::rank_player_buildings_by_profit(w, reg, report);
        for (int i = 0; i < static_cast<int>(ranking.size()); ++i)
            ranks[ranking[i].first] = i;
        h.building_rank.push_back(std::move(ranks));
        if (h.building_rank.size() > 5)
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
        }
    }

    // Resource-deposit time series (BL-198): the aggregate (Σ remaining deposit per
    // body per resource) every tick; a tracked tile's own series lazily, from the
    // first drill-down into it. All share the sample-day axis (X), capped in
    // lockstep. One pass over the tiles accumulates the body totals and snapshots
    // any tracked tile — O(tiles·resources)/tick, the same order as the econ step.
    {
        // Drain the card's lazy-tracking request (a tile whose resource drill is
        // open) so the tile starts recording THIS tick, aligned to today's sample.
        if (st.card_track_tile != null_entity)
        {
            h.tracked_tiles.insert(st.card_track_tile);
            st.card_track_tile = null_entity;
        }

        // Date the sample by its quarter index (econ ticks are quarterly). This
        // equals day_tick in live play but also advances when the verify harness
        // steps the economy without turning the sim clock, so the Year/Quarter axis
        // always progresses. See h.sample_index.
        ui::push_capped(h.sample_days,
                        h.sample_index * static_cast<std::uint64_t>(sim_loop::econ_tick_days));
        ++h.sample_index;

        std::unordered_map<entity_id, std::array<float, resource_count>> body_sum;
        for (const auto& [tid, tc] : w.tiles)
        {
            auto& acc = body_sum[tc.body];
            for (std::size_t r = 0; r < resource_count; ++r)
                acc[r] += tc.resource_deposit[r];

            if (h.tracked_tiles.count(tid))
            {
                auto& th = h.tile_resource[tid];
                for (std::size_t r = 0; r < resource_count; ++r)
                    ui::push_capped(th[r], tc.resource_deposit[r]);
            }
        }
        for (const auto& [bid, acc] : body_sum)
        {
            auto& bh = h.body_resource[bid];
            for (std::size_t r = 0; r < resource_count; ++r)
                ui::push_capped(bh[r], acc[r]);
        }
    }
}

} // namespace session_history
