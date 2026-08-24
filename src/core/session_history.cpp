/// @file session_history.cpp
/// See session_history.hpp — bodies moved verbatim from app::step_economy
/// (BL-361); behaviour unchanged.

#include "core/session_history.hpp"

#include "core/app.hpp"      // step_economy_phase_ms — the shared phase accumulators
#include "core/battle_dispatch_text.hpp" // BL-468: the phrase bank (own TU, so it can be harnessed)
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

void post_battle_dispatches(const world& w, const economy_report& report,
                            ui::chat_state& chat, int day_tick)
{
    if (report.battle_dispatches.empty())
        return;

    // Channel 1 is the standing Field channel (chat_panel.hpp). Guarded rather
    // than assumed: a chat_state built before BL-468 has one channel, and posting
    // to an index that does not exist would be worse than posting to Public.
    const int field = (static_cast<int>(chat.channels.size()) > ui::chat_state::k_field_channel)
                          ? ui::chat_state::k_field_channel
                          : ui::chat_state::k_public_channel;

    for (const battle_dispatch& d : report.battle_dispatches)
    {
        // VOICED BY THE ATTACKER'S CORP, and only for fights the player is in.
        // BL-212 keeps corporations out of Public precisely so a rival's
        // internals never leak through comms; a dispatch naming two rivals'
        // strengths would be that leak by another route. Rival-vs-rival fights
        // are seen on the canvas marker instead, which says WHERE without saying
        // how it is going. Flagged for Ben — see the review log.
        const bool players = (d.attacker == w.player_entity || d.defender == w.player_entity);
        if (!players)
            continue;
        ui::chat_post(chat, day_tick, w.player_entity, field,
                      battle_dispatch_line(w, d));
    }
}

void post_contract_events(const world& w, const economy_report& report,
                          ui::chat_state& chat, int day_tick)
{
    if (report.contract_events.empty())
        return;

    // Channel 0 is the standing Public channel — always present (chat_panel.hpp),
    // so no guard is needed the way post_battle_dispatches needs one for Field.
    for (const contract_dispatch& d : report.contract_events)
        ui::chat_post(chat, day_tick, d.client, ui::chat_state::k_public_channel,
                      contract_dispatch_line(w, d));
}

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
            // Order-book traffic is the quietest thing a corp does — routine
            // commerce, not a change in the industrial landscape — so it only
            // reaches the nation feed on a tick where nothing else happened.
            case agency_event::kind::order_placed:      return 1;
            case agency_event::kind::order_removed:     return 1;
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
            case agency_event::kind::order_placed:
                text = "Domestic producers are bringing stock to market.";
                break;
            case agency_event::kind::order_removed:
                text = "A domestic producer has withdrawn stock from sale.";
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

        // BL-398 instrumentation: split this phase's cost into its two halves —
        // the C++ blackboard export and the sol2 pack evaluation — through the
        // same accumulator surface the other step_economy phases report on
        // ([9] and [10]; [7] remains the total). Naming the slow PHASE was never
        // the hard part — naming which half of it is the cost is, and bounding
        // the other half is a fix that measures as no fix at all.
        using clk = std::chrono::steady_clock;
        auto& acc = step_economy_phase_ms();
        auto add_lap = [&acc](std::size_t slot, clk::time_point from) {
            acc[slot] += std::chrono::duration_cast<std::chrono::microseconds>(
                             clk::now() - from).count() / 1000.0;
        };
        std::vector<entity_id> corp_ids;
        corp_ids.reserve(w.corporations.size());
        for (const auto& [id, cc] : w.corporations)
            corp_ids.push_back(id);
        std::sort(corp_ids.begin(), corp_ids.end());

        for (const entity_id corp : corp_ids)
        {
            if (!corp_strategic_eval_due(w, corp, tick))
                continue;

            // Channel first, and for EVERY due corp — it is a name and two ids,
            // and creating it here keeps the channel roster (and its indices)
            // identical to what the unbounded version produced, so the combo
            // still lists every corp whose counsel the player could read.
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

            // BL-398: evaluate only the corp whose counsel channel is OPEN. The
            // player reads one channel at a time, so benching every due corp
            // wrote lines nobody would ever scroll to — ~1 s/tick of hitch for
            // output with no reader. Counsel is presentation (see the header):
            // it reads the world and writes the chat log, nothing else, so
            // skipping a corp changes no simulation state and no determinism —
            // the same tick replays identically whichever channel is open.
            //
            // This also IS the per-tick cap: at most one channel is active, so
            // at most one corp is evaluated per tick, no separate backstop.
            //
            // Cost: a channel fills from the tick it is opened, not
            // retroactively — the player waits one cadence for the first line.
            if (channel != chat.active_channel)
                continue;

            const auto t_bb = clk::now();
            const corp_blackboard bb = export_corp_blackboard(w, corp, tick);
            add_lap(9, t_bb); // blackboard export (C++ world scan)

            const auto t_eval = clk::now();
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
                add_lap(10, t_eval); // sol2 pack evaluation (per pack, per corp)
            }
            catch (const std::exception& e)
            {
                add_lap(10, t_eval);
                // BL-353: same policy as the load-time guard in load_economy — a
                // pack that loads clean but throws on live data (sol2 errors
                // surface as std::runtime_error) disables counsel rather than
                // killing the session mid-tick. One visible line in the counsel
                // channel; the detail goes to stderr like the load failure does.
                // Since BL-398 only an open channel evaluates, so a bad pack now
                // surfaces when the player first reads counsel, not at tick one.
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
