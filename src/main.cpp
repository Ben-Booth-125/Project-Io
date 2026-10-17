#include "core/agent_protocol.hpp"
#include "core/app.hpp"
#include "core/sim_loop.hpp"
#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/campaign_settle.hpp"        // BL-1325: run_settle_tick, the app's one world tick
#include "world/digitisation_tree_data.hpp" // BL-1325: io::dg::nodes (majors held)
#include "world/nation_tech.hpp"            // BL-1325: nation_tech_state
#include "world/orbital_system.hpp"         // BL-1325: advance_orbits
#include "world/spawn_seat.hpp"             // BL-1325: seat_player_corporation
#include "world/nation_step.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/finish_campaign_world.hpp" // BL-1085: --serve opens on the searched, settled world
#include "world/hard_coded_world.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/survey_system.hpp"
#include "world/stockpile_budget.hpp" // BL-1042: the budget the search-less paths state
#include "world/trade.hpp" // BL-1266: run_trades

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <memory>
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// The line-protocol parsing, validation and opcode handlers moved VERBATIM to
// src/core/agent_protocol.{hpp,cpp} when BL-412 gave the protocol a second
// host (the rendered app's agent seam). run_serve below is now that shared
// unit's headless host: it owns the world, the warm-up and the clock (TICK),
// and delegates every request line to agent_protocol::service_line.

// --serve [--ticks N] [--as <corp-id|any>]  (BL-278)
// --load <path>                             (BL-536 - open a saved campaign)
//
// Headless, persistent: builds the canonical world once (identical warm-up to
// --export-blackboard), then reads one request per line from stdin until EOF
// or `SHUTDOWN`, applying the real per-tick sequence and the player-grade
// corp_command seam (apply_corp_command — no bypass). This is the process an
// out-of-process MCP server (tools/mcp/) spawns and talks to; it ships no
// network code itself; the line protocol below is the whole surface.
//
// The line protocol is an EXTERNAL INPUT SURFACE, not a trusted in-process
// call. So the session carries an ACTOR (BL-387): the one corp this process
// will act as — the player corp by default, `--as <corp-id>` to pin another,
// `--as any` to lift the gate entirely (the explicit research opt-in for
// bot-vs-bot corpus runs, never the default). A COMMAND naming any other corp
// answers `RESULT result=rejected_not_owner building=-1` without reaching
// apply_corp_command, and a BLACKBOARD naming any other corp answers a single
// line `ERR result=rejected_not_owner` followed by `END` (BL-397 — the
// lines-then-END shape holds even on refusal; CORPS and BODIES stay public,
// matching the BL-068 competitor-visibility rule). The refusal lives HERE, at
// the protocol layer, because
// apply_corp_command must stay permissive: it is also how the in-process
// scorer legitimately commands every rival, and a corp check inside it would
// break the AI it exists to serve.
//
// For the same reason, every COMMAND integer field is parsed wide and checked
// against its destination's real domain (BL-396); any violation answers
// `RESULT result=rejected_invalid building=-1` without reaching the seam.
// Nothing is truncated, wrapped or clamped — see kv_get_checked. Floats are
// checked as FLOATS (1e300 is a finite double and an infinite float), and
// negative quantity / floor_price is refused at the wire.
//
// Requests (space-separated `key=value` tokens after the opcode):
//   TICK                                            -> advance one tick
//   CORPS                                           -> one JSON line per corp, then END
//   BODIES                                          -> one JSON line per body, then END
//   BLACKBOARD corp=<id> ticks=<n>                  -> facts as BL-206 JSONL, then END
//   COMMAND corp=<id> verb=<corp_verb> subject=<id> tile=<id> type=<building_type>
//           target=<resource_type>
//           recipe=<id> workforce=<n> road_tier=<n> unit_type=<n>
//           quantity=<f> floor_price=<f> order=<n> counterparty=<id>
//           province=<province::id> nation=<id>   (BL-1296: state industry)
//                                                   -> apply_corp_command
//   SHUTDOWN                                        -> BYE, then exit
//
// `type` and `target` carry the raw enum values of `building_type` and
// `resource_type`; deliberately NOT written out as numeric ranges here, because
// a hard-coded range is a second definition that goes stale the moment either
// enum grows. This block previously claimed `verb=<0-7>` while the enum held
// fifteen, and `type=<0-4>` while it held seven. `verb` lost its numeric range
// for the same reason with BL-628 (it read `<0-14>` while the enum held
// twenty-eight); `corp_verb_count` in corp_command.hpp is the one definition,
// and `agent_protocol.cpp` gates the wire against it.
//
// Every `corp_verb` is reachable here; which keys a given verb reads is
// `apply_corp_command`'s business, and unread keys cost nothing. Keep this list
// in step with `corp_verb` — the last three verb families were added to the enum
// without their arguments ever reaching this parser, which made them applicable
// in the dictionary and inapplicable on the wire.
//
// BL-511 (2026-08-21) DID need a new key. `march_unit`'s destination moved from
// a tile to a province, and a province id is not an entity_id — it is derived
// from (body rank | block | component) and lives in its own uint32 domain — so
// `tile=` could not be reused for it without conflating two id spaces. The verb
// itself did not move: the enum is serialised and append-only, so `march_unit`
// keeps its value and only the field it reads changed.
//
// BL-452's convoy pair (2026-08-17) needed NO new key: `dispatch_convoy` reads
// subject / counterparty / target / quantity and `hold_convoy` reads order, all
// of which this parser already range-checks. Worth stating rather than leaving
// to be rediscovered — the precedent above is a parser that silently lagged the
// enum, and the reason it does not lag here is that these verbs reuse fields,
// not that anyone remembered to extend it.
// Responses are one line each except BLACKBOARD, which is N JSONL lines + END.
int run_serve(int ticks, long long as_corp, bool as_any)
{
    // BL-1085 -- THE CAMPAIGN START THE APP OPENS ON, searched and settled
    // (STARTUP.md § Handoff: "run_serve adopts the searched and settled
    // world"). The world is built from world_gen.lua's PARSED config and the
    // works table, then `finish_campaign_world` runs -- the band, the recipe
    // passes, the landscape search and its winner, the twelve-tick settle --
    // the same call in the same order as round 6's worker and Begin's cold
    // worker, so the state hash printed below equals a cold `--autostart`'s on
    // the same (default) params. Until 2026-09-24 this path laid the UNSEARCHED
    // seed candidate (NR-909); `--verify` still does, for its goldens.
    lua_state lua;
    lua.load("scripts/world_gen.lua");
    world_gen_config cfg{};
    cfg.load_from_lua(lua);
    works_registry works;
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    const world_params params{};
    generation_report  rep;
    world w = make_hard_coded_world(params, &rep, cfg, /*progress=*/nullptr, &works);
    const finish_campaign_result fin =
        finish_campaign_world(w, rep, reg, params, cfg, /*progress=*/nullptr);
    // Before the warm-up ticks: the same moment --autostart hashes (after the
    // settle, before play), so the two lines compare digit for digit.
    std::fprintf(stderr, "[serve] searched and settled: %zu corporations, winner placement=%08X; "
                         "state_hash=%016llX seed=%u\n",
                 w.corporations.size(), fin.search.winner.placement_seed,
                 static_cast<unsigned long long>(w.state_hash(w.current_day_tick)),
                 params.seed);

    // BL-387: the session actor, resolved after world construction so the
    // default can be the player corp. Only meaningful when !as_any; a pinned
    // id that names no corp simply has every command answer rejected_no_corp,
    // the same as it would in-process.
    const entity_id session_actor = (as_corp >= 0)
        ? static_cast<entity_id>(as_corp)
        : w.player_entity;

    // One econ tick, in one place. The warm-up loop and the TICK opcode used to
    // carry byte-identical copies of this sequence, which is how the survey step
    // came to be missing from BOTH rather than from one.
    // BL-1275 review: live econ steps continue the SETTLED world's counter, as
    // run_spectate_headless does (first live step = settled tick + 1), so the
    // research age, its price index and the AI's settle guard read live
    // quarters as live. Session tick t (the protocol's) maps to econ step
    // econ0 + t.
    const int econ0 = std::max(w.current_econ_tick, k_campaign_settle_ticks - 1);
    const auto step_one_tick = [&](int t) {
        w.current_day_tick  = t;
        w.current_econ_tick = econ0 + t;
        // BL-597: one shared passive/active Logistic Point pool for this
        // tick, handed to both the convoy dispatch (passive) and the march
        // pass inside run_economy_step (active) — so the two genuinely
        // contend, per LOGISTICS.md's bifold table and the "war flips the
        // queue" finding. Local to this tick; never persisted.
        lp_pool_map tick_lp_pools;
        // BL-1266: advance -> arrivals -> economy -> trade -> clearing (app.cpp's order).
        advance_convoys(w);
        credit_arrived_convoys(w, t);
        economy_report report = run_economy_step(w, reg, /*spectating=*/false, &tick_lp_pools);
        run_trades(w, reg, report, &tick_lp_pools); // BL-1266: trade, before the clear
        auto flows = clear_markets(w, reg, report);
        apply_budget(w, reg, flows, report.workforce_contention, &report.budgets,
                     &report.buildings,        // BL-343: law enforcement seam
                     &report.building_labour); // BL-614: wages on the per-building grant
        run_nation_step(w, reg, report, w.current_econ_tick); // Sprint N3: nations score, spend, dispatch
        // Age any dispatched survey by the days this tick spans (app.cpp does the
        // same on the day boundary). Without it the geographic fog never lifted
        // here: `survey` was an applicable verb whose effect never arrived, so no
        // tile ever became visible and `build` / `place_road` had no discoverable
        // target on the one seam an out-of-process agent has. The agent could pay
        // for discovery and then wait forever.
        advance_surveys(w, sim_loop::econ_tick_days);
    };

    int tick = 0;
    for (int t = 1; t <= ticks; ++t)
    {
        tick = t;
        step_one_tick(t);
    }

    std::string line;
    while (std::getline(std::cin, line))
    {
        const agent_protocol::session sess{session_actor, as_any};
        std::string                   out;
        const agent_protocol::host_op op =
            agent_protocol::service_line(line, w, reg, tick, sess, out);

        if (op == agent_protocol::host_op::tick)
        {
            // The headless host drives the clock itself: TICK advances one
            // econ tick synchronously. (The live seam's TICK instead releases
            // the app's gated clock — same request, host-owned meaning.)
            ++tick;
            step_one_tick(tick);
            out += "OK tick=" + std::to_string(tick) + "\n";
        }

        std::cout << out << std::flush;
        if (op == agent_protocol::host_op::shutdown)
            return 0;
    }
    return 0;
}


// --export-blackboard <corp|all> [--out <dir>] [--ticks N]  (BL-206)
//
// Headless: builds the canonical world, loads the Lua economy data, runs N
// warm-up ticks of the real per-tick sequence (advance + credit convoys →
// run_economy_step → dispatch convoys → clear_markets → apply_budget), then
// dumps each requested corp's visibility-honest blackboard as
// blackboard_<corp>_<tick>.jsonl. No SDL; deterministic by construction.
int run_blackboard_export(const std::string& which, const std::string& out_dir, int ticks)
{
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    // BL-573: same app-layer threading as run_serve, above.
    world w = make_hard_coded_world();

    // Author default recipes onto generated processors (mirrors app::load_economy).
    // BL-429; the body's air (Ben, 2026-10-09): default_recipe_id_at, per tile.
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = reg.default_recipe_id_at(w, b.tile);

    // BL-1044 — THE CHARTER BUDGET, SPENT ON THE SEED CANDIDATE (Ben,
    // 2026-09-21, NR-909). This path does not search. The Industrialisation span
    // runs by default (make_hard_coded_world()'s default params, seed 0), so
    // its world carries a stockpile budget, and a non-empty one is spent as the
    // harness's unsearched apply spends it — the search's seed candidate. An
    // EMPTY budget (a world the span did not run on, or a rejected one) lays
    // exactly the BL-365 call below, byte for byte.
    const seed_candidate_spend scs = spend_stockpile_on_seed_candidate(
        w, reg, /*world_seed=*/0u, world_gen_config{}.corporation_count);
    if (!scs.spent)
    {
        if (scs.stockpile.rejected)
            std::fprintf(stderr, "[stockpile_budget] headless run: the stockpile budget was REJECTED "
                                 "(%s); the pre-budget web is laid\n", scs.stockpile.rejection.c_str());
        // BL-1185: world-gen's roster was laid before the registry existed;
        // its processors are made chain-feasible now that it does.
        // The seat is redrawn (if it must be) on world-gen's own corp stream:
        // world seed 0 here, xor generate_corporations' salt.
        const chain_roster_enforcement cre =
            enforce_chain_feasible_roster(w, reg, /*seed=*/0u ^ 0x4A71012u);
        std::fprintf(stderr, "[chain_roster] headless run: %d processor(s) re-decided, %d unplaced, "
                             "%d specialist(s) holdless; seat %u%s\n",
                     cre.processors_redecided, cre.processors_unplaced, cre.holdless,
                     static_cast<unsigned>(cre.seat), cre.seat_redrawn ? " (redrawn)" : "");
        // BL-365: real background corporations, generated now that reg is loaded.
        generate_background_firms(w, reg, /*seed=*/0x8A21F00Du);
        place_opening_stock(w); // BL-1265: the firms' opening stock goes on the shelves
    }
    else
    {
        place_opening_stock(w); // BL-1265: the spent roster's opening stock goes on the shelves
        std::fprintf(stderr, "[stockpile_budget] headless run: %lld points spent on the seed candidate "
                             "(%zu specialists, %zu firms)%s\n",
                     static_cast<long long>(scs.report.points_spent),
                     scs.report.specialists.size(), scs.report.firms.size(),
                     scs.report.refused     ? " — spend REFUSED, the no-budget world"
                     : scs.report.fell_back ? " — no specialist affordable, the no-budget world (NR-910)"
                                            : "");
    }

    // BL-1275 review: this world is not settled, but its ticks are live play —
    // number econ steps from the first live step, as run_serve does.
    const int econ0 = std::max(w.current_econ_tick, k_campaign_settle_ticks - 1);
    for (int t = 1; t <= ticks; ++t)
    {
        w.current_day_tick  = t;
        w.current_econ_tick = econ0 + t;
        // BL-597: see step_one_tick's own comment above — one shared LP pool
        // per tick for both the passive (convoy) and active (march) draws.
        lp_pool_map tick_lp_pools;
        // BL-1266: advance -> arrivals -> economy -> trade -> clearing (app.cpp's order).
        advance_convoys(w);
        credit_arrived_convoys(w, t);
        economy_report report = run_economy_step(w, reg, /*spectating=*/false, &tick_lp_pools);
        run_trades(w, reg, report, &tick_lp_pools); // BL-1266: trade, before the clear
        auto flows = clear_markets(w, reg, report);
        apply_budget(w, reg, flows, report.workforce_contention, &report.budgets,
                     &report.buildings,        // BL-343: law enforcement seam
                     &report.building_labour); // BL-614: wages on the per-building grant
        run_nation_step(w, reg, report, w.current_econ_tick); // Sprint N3: nations score, spend, dispatch
    }

    std::filesystem::create_directories(out_dir);
    std::vector<entity_id> corp_ids;
    for (const auto& kv : w.corporations)
        if (which == "all" || std::to_string(kv.first) == which)
            corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    if (corp_ids.empty())
    {
        std::fprintf(stderr, "ProjectIo: --export-blackboard: no corp matches '%s'\n",
                     which.c_str());
        return 1;
    }
    for (const entity_id corp : corp_ids)
    {
        const corp_blackboard bb = export_corp_blackboard(w, corp, ticks);
        const std::string path = out_dir + "/blackboard_" + std::to_string(corp) + "_"
                               + std::to_string(ticks) + ".jsonl";
        std::ofstream f(path, std::ios::binary); // binary: byte-stable across platforms
        to_jsonl(bb, f);
        f.flush();
        if (!f) // BL-363: an unwritable path (missing dir, permissions) is a failure, not "wrote"
        {
            std::fprintf(stderr, "ProjectIo: --export-blackboard: failed to write %s\n",
                         path.c_str());
            return 1;
        }
        std::printf("wrote %s (%zu facts)\n", path.c_str(), bb.facts.size());
    }
    return 0;
}

// --spectate-headless [--until YEAR] [--seed N] [--out DIR]  (BL-1325)
//
// A SPECTATED CAMPAIGN WITH NO WINDOW (Ben, 2026-10-15: "use spectate without
// rendering ... going over the speed cap for economy ticks"). The world is the
// one the app opens on -- world_gen.lua's parsed config, make_hard_coded_world,
// finish_campaign_world (search + settle), then the seat DRAW the app takes
// when nobody is asked (`seat_player_corporation`, app::seat_player; under
// spectate the seat is a viewpoint only, AI_OPPONENT.md § 10i). Each quarter
// then runs the app's live econ boundary: surveys and orbits aged by the
// quarter's 90 days, the day mirror, and `run_settle_tick` -- the ONE world
// tick app::step_economy runs -- with spectating=true, so the strategic tier
// evaluates every corporation. The cadence key continues from the settle's
// twelve steps, exactly as Begin sets it (app.cpp, m_econ_steps).
//
// Uncapped: no sim_loop, no frame pacing; the CPU is the clock. Output is
// DIR/run_<seed>.json, one record per in-game year; wall-clock readings sit
// in a separate "timing" object so two runs of one seed compare byte-equal
// everywhere else.
std::string json_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (const char c : s)
    {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (static_cast<unsigned char>(c) < 0x20)
        {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
            out += buf;
        }
        else out += c;
    }
    return out;
}

std::string json_num(double v)
{
    if (!std::isfinite(v)) return "null";
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.6f", v);
    return buf;
}

int run_spectate_headless(int until_year, std::uint32_t seed, const std::string& out_dir)
{
    using clk = std::chrono::steady_clock;
    const auto t_start = clk::now();

    lua_state lua;
    lua.load("scripts/world_gen.lua");
    world_gen_config cfg{};
    cfg.load_from_lua(lua);
    works_registry works;
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    world_params params{};
    params.seed = seed;
    generation_report rep;
    world w = make_hard_coded_world(params, &rep, cfg, /*progress=*/nullptr, &works);
    const finish_campaign_result fin =
        finish_campaign_world(w, rep, reg, params, cfg, /*progress=*/nullptr);
    // The app's no-player-to-ask route (app::seat_player): the weighted draw.
    const spawn_seat_result seat =
        seat_player_corporation(w, params.seed, fin.search.winner_score);
    const double gen_seconds = std::chrono::duration<double>(clk::now() - t_start).count();

    // THE CADENCE KEY (BL-568), numbered as Begin numbers it: the settle
    // stamped econ steps 0..k_campaign_settle_ticks-1, and the app's first live
    // quarter is k_campaign_settle_ticks (app.cpp, `m_econ_steps =
    // k_campaign_settle_ticks`). Continue from the settled world's own counter
    // and say so if it ever disagrees with the app's constant.
    const int first_live_step = w.current_econ_tick + 1;
    if (first_live_step != k_campaign_settle_ticks)
        std::fprintf(stderr, "[spectate-headless] WARNING: settled current_econ_tick=%d, so the first "
                             "live step %d differs from the app's %d\n",
                     w.current_econ_tick, first_live_step, k_campaign_settle_ticks);

    const int epoch = static_cast<int>(params.epoch_year);
    if (until_year <= epoch)
    {
        std::fprintf(stderr, "ProjectIo: --spectate-headless: --until %d is not after the epoch %d\n",
                     until_year, epoch);
        return 1;
    }
    std::printf("[spectate-headless] seed=%u epoch=%d until=%d; world built in %.1f s "
                "(%zu nations, %zu corporations, seat %u%s); state_hash=%016llX\n",
                seed, epoch, until_year, gen_seconds, w.nations.size(), w.corporations.size(),
                static_cast<unsigned>(seat.seated), seat.floor_unmet ? " FLOOR UNMET" : "",
                static_cast<unsigned long long>(w.state_hash(w.current_day_tick)));
    std::fflush(stdout);

    std::vector<entity_id> nation_ids;
    for (const auto& kv : w.nations) nation_ids.push_back(kv.first);
    std::sort(nation_ids.begin(), nation_ids.end());

    // Per-nation corp sales over the year (sum of the four quarters' clear
    // income, corps keyed by home nation) -- the nearest output measure the
    // tick already computes; reset at each year record.
    std::map<entity_id, double> sales_year;

    std::ostringstream years;
    bool first_year = true;
    const auto record_year = [&](int year, int econ_ticks_run) {
        std::map<entity_id, entity_id> tile_nation;
        for (const entity_id n : nation_ids)
            for (const entity_id t : w.nations.at(n).tiles) tile_nation[t] = n;
        std::map<entity_id, long long> pop_k;
        for (const auto& [cid, pc] : w.population_centres)
        {
            const auto tit = w.population_centre_tile.find(cid);
            if (tit == w.population_centre_tile.end()) continue;
            const auto nit = tile_nation.find(tit->second);
            if (nit != tile_nation.end()) pop_k[nit->second] += pc.population;
        }
        std::map<entity_id, double> corp_bal;
        std::map<entity_id, double> corp_inc_q; // latest filed quarterly_return::income
        std::map<entity_id, double> corp_net_q; // latest filed quarterly_return::net
        std::map<entity_id, int>    corp_n;
        for (const auto& [cid, c] : w.corporations)
        {
            corp_bal[c.home_nation] += c.balance;
            corp_n[c.home_nation] += 1;
            if (!c.returns.empty())
            {
                corp_inc_q[c.home_nation] += c.returns.back().income;
                corp_net_q[c.home_nation] += c.returns.back().net;
            }
        }

        years << (first_year ? "\n" : ",\n") << "    {\"year\": " << year
              << ", \"econ_ticks\": " << econ_ticks_run << ", \"nations\": [";
        first_year = false;
        bool first_n = true;
        for (const entity_id n : nation_ids)
        {
            const nation_component& nc = w.nations.at(n);
            int held = 0, majors = 0, queued = 0;
            long long pool_milli = 0;
            if (const auto tit = w.nation_tech.find(n); tit != w.nation_tech.end())
            {
                held = static_cast<int>(tit->second.held.count());
                for (int i = 0; i < io::dg::node_count; ++i)
                    if (tit->second.held.test(static_cast<std::size_t>(i))
                        && io::dg::nodes[static_cast<std::size_t>(i)].t == io::dg::tier::major)
                        ++majors;
                queued     = static_cast<int>(tit->second.queue.size());
                pool_milli = tit->second.pool_milli;
            }
            years << (first_n ? "\n" : ",\n") << "      {\"id\": " << n
                  << ", \"name\": \"" << json_escape(nc.name) << "\""
                  << ", \"major\": " << (is_major_nation(w, n) ? "true" : "false")
                  << ", \"treasury\": " << json_num(nc.treasury)
                  << ", \"population_k\": " << pop_k[n]
                  << ", \"corp_sales_year\": " << json_num(sales_year[n])
                  << ", \"corp_balance_sum\": " << json_num(corp_bal[n])
                  << ", \"corporations\": " << corp_n[n]
                  << ", \"corp_income_last_q\": " << json_num(corp_inc_q[n])
                  << ", \"corp_net_last_q\": " << json_num(corp_net_q[n])
                  << ", \"state_assets\": " << nc.state_assets.size()
                  << ", \"tech_held\": " << held
                  << ", \"majors_held\": " << majors
                  << ", \"research_queue\": " << queued
                  << ", \"rp_pool\": " << json_num(static_cast<double>(pool_milli) / 1000.0)
                  << ", \"tax_tier\": " << nc.tax_tier              // BL-1335
                  << ", \"discontent\": " << json_num(nc.discontent) // BL-1335
                  << "}";
            first_n = false;
        }
        years << "\n    ]}";
        sales_year.clear();
    };

    record_year(epoch, 0);

    const int total_ticks = (until_year - epoch) * 4; // 4 quarters of 90 days; 360-day years
    std::vector<double> decade_seconds;
    auto decade_t0 = clk::now();
    const auto t_run0 = decade_t0;
    for (int k = 1; k <= total_ticks; ++k)
    {
        // The app's frame at a quarter boundary (app.cpp, run loop): orbits and
        // surveys aged to the boundary day, the day mirrored onto the world,
        // then step_economy's world half.
        const int day = k * sim_loop::econ_tick_days;
        advance_orbits(w, static_cast<double>(sim_loop::econ_tick_days));
        advance_surveys(w, sim_loop::econ_tick_days);
        w.current_day_tick = day;
        const settle_tick_result tick =
            run_settle_tick(w, reg, first_live_step + (k - 1), day, /*spectating=*/true);
        for (const auto& [cid, f] : tick.flows)
            if (const auto cit = w.corporations.find(cid); cit != w.corporations.end())
                sales_year[cit->second.home_nation] += f.income;

        if (k % 4 != 0) continue;
        const int year = epoch + k / 4;
        double sales_sum = 0.0; // BL-1335 readout: the year's corp sales, before record_year resets it
        for (const auto& [n, v] : sales_year) sales_sum += v;
        record_year(year, k);
        if ((year - epoch) % 10 == 0 || year == until_year)
        {
            const auto now = clk::now();
            const double secs = std::chrono::duration<double>(now - decade_t0).count();
            decade_t0 = now;
            decade_seconds.push_back(secs);
            double treas = 0.0;
            int held = 0, neg = 0;
            for (const entity_id n : nation_ids)
            {
                treas += w.nations.at(n).treasury;
                if (w.nations.at(n).treasury < 0.0f) ++neg;
            }
            for (const auto& [n, ts] : w.nation_tech) held += static_cast<int>(ts.held.count());
            // BL-1335: the tax tiers (median) and how many nations' Discontent bites.
            std::vector<int> tiers;
            int biting = 0;
            for (const entity_id n : nation_ids)
            {
                tiers.push_back(w.nations.at(n).tax_tier);
                if (w.nations.at(n).discontent > 0.5f) ++biting;
            }
            std::sort(tiers.begin(), tiers.end());
            const int median_tier = tiers.empty() ? 0 : tiers[tiers.size() / 2];
            std::printf("[spectate-headless] %d: %.1f s wall | treasury sum %.0f (%d negative) | "
                        "tech held %d | corps %zu | corp sales %.0f | median tax tier %d | "
                        "discontent>0.5 %d | state_hash=%016llX\n",
                        year, secs, treas, neg, held, w.corporations.size(), sales_sum, median_tier,
                        biting, static_cast<unsigned long long>(w.state_hash(w.current_day_tick)));
            std::fflush(stdout);
        }
    }
    const double run_seconds = std::chrono::duration<double>(clk::now() - t_run0).count();
    const unsigned long long final_hash = w.state_hash(w.current_day_tick);

    // BL-1327: the final space-asset store, counted by class then state.
    int space_count[4][4] = {};
    for (const auto& [sid, sa] : w.space_assets)
        ++space_count[static_cast<int>(sa.cls)][static_cast<int>(sa.state)];
    static const char* const k_space_cls[4] = {"satellite", "rover", "crewed_mission", "station_module"};
    std::string space_json = "{";
    for (int c = 0; c < 4; ++c)
    {
        space_json += std::string(c ? ", " : "") + "\"" + k_space_cls[c] + "\": [";
        for (int s = 0; s < 4; ++s)
            space_json += (s ? ", " : "") + std::to_string(space_count[c][s]);
        space_json += "]";
    }
    space_json += ", \"order\": \"in_transit, active, decaying, lost\"}";

    std::filesystem::create_directories(out_dir);
    const std::string path = out_dir + "/run_" + std::to_string(seed) + ".json";
    std::ofstream f(path, std::ios::binary);
    f << "{\n  \"seed\": " << seed << ",\n  \"epoch_year\": " << epoch
      << ",\n  \"until_year\": " << until_year << ",\n  \"econ_ticks\": " << total_ticks
      << ",\n  \"seat\": " << seat.seated
      << ",\n  \"first_live_econ_step\": " << first_live_step
      << ",\n  \"space_assets\": " << space_json
      << ",\n  \"final_state_hash\": \"";
    char hbuf[24];
    std::snprintf(hbuf, sizeof hbuf, "%016llX", final_hash);
    f << hbuf << "\",\n  \"years\": [" << years.str() << "\n  ],\n  \"timing\": {\"generation_seconds\": "
      << json_num(gen_seconds) << ", \"run_seconds\": " << json_num(run_seconds)
      << ", \"seconds_per_decade\": [";
    for (std::size_t i = 0; i < decade_seconds.size(); ++i)
        f << (i ? ", " : "") << json_num(decade_seconds[i]);
    f << "]}\n}\n";
    f.flush();
    if (!f)
    {
        std::fprintf(stderr, "ProjectIo: --spectate-headless: failed to write %s\n", path.c_str());
        return 1;
    }
    std::printf("[spectate-headless] reached %d after %d econ ticks in %.1f s (+%.1f s generation); "
                "final state_hash=%s; wrote %s\n",
                until_year, total_ticks, run_seconds, gen_seconds, hbuf, path.c_str());
    return 0;
}

// ---------------------------------------------------------------------------
// Crash reporting (2026-08-12)
// ---------------------------------------------------------------------------
//
// A double-clicked exe has no console, so BOTH failure shapes died silently in
// player hands: a std::exception lands in main's catch block whose message goes
// to an invisible stderr, and a hardware fault (access violation, stack
// overflow) never reaches C++ handling at all. Everything below funnels into
// crash.log in the working directory — the first thing to read whenever "the
// app just closed".

/// Append one line to crash.log (working directory — next to the exe for a
/// normal double-click launch) and mirror it to stderr for terminal runs.
void crash_log(const char* what)
{
    if (std::FILE* f = std::fopen("crash.log", "a"))
    {
        std::fprintf(f, "%s\n", what);
        std::fclose(f);
    }
    std::fprintf(stderr, "%s\n", what);
}

/// Mirror SDL's whole log stream to stderr, flushed per line.
///
/// WHY THIS EXISTS (NR-694, 2026-08-28). SDL's default sink on Windows is
/// OutputDebugString plus a console SDL attaches for itself, and NEITHER
/// reaches a redirected pipe. So `ProjectIo --verify-all scripts/verify > log`
/// wrote a ZERO-BYTE log across a two-hour run: verify_api.cpp's per-script
/// progress line, every `Golden PASS/FAIL` line and any Lua error were all
/// emitted and all invisible.
///
/// The cost was never cosmetic. With no output, a script that ran and produced
/// NOTHING (NR-693, shell_pass) is indistinguishable from one the run has not
/// reached yet, and a hang is indistinguishable from slow work — both had to be
/// inferred from capture filenames and CPU sampling instead of read off a log.
///
/// stderr rather than stdout, so a caller can still redirect the two apart.
/// Flushed every line because the case that needs the log most is the one that
/// never exits, and a buffered tail of a hung process shows nothing at all.
void log_to_stderr(void*, int category, SDL_LogPriority priority, const char* message)
{
    const char* tag = priority >= SDL_LOG_PRIORITY_ERROR ? "ERROR: "
                    : priority == SDL_LOG_PRIORITY_WARN  ? "WARN: "
                                                         : "";
    std::fprintf(stderr, "%s%s\n", tag, message);
    std::fflush(stderr);

#ifdef _WIN32
    // Keep the debugger's view intact — replacing the sink would otherwise take
    // OutputDebugString away from anyone attached in Visual Studio.
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#endif
    (void)category;
}

#ifdef _WIN32
/// Last-chance SEH filter: names the fault and the address, then lets the
/// process die. No unwinding, no allocation beyond fopen — by the time this
/// runs the process state is not trustworthy.
LONG WINAPI seh_crash_filter(EXCEPTION_POINTERS* ep)
{
    char buf[192];
    const DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    void* addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    std::snprintf(buf, sizeof buf,
                  "ProjectIo: fatal hardware exception 0x%08lX at %p%s",
                  static_cast<unsigned long>(code), addr,
                  code == 0xC0000005u ? " (access violation)"
                  : code == 0xC00000FDu ? " (stack overflow)" : "");
    crash_log(buf);
    return EXCEPTION_CONTINUE_SEARCH; // still crash — this only reports
}
#endif

} // namespace

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(seh_crash_filter);
#endif
    // Installed before anything can log, so the harness's own progress lines are
    // visible from the first one (NR-694). Unconditional rather than gated on
    // --verify: an interactive run's stderr costs nothing when nobody reads it,
    // and gating would mean the one mode that most needs a log is the one whose
    // logging is special-cased.
    SDL_SetLogOutputFunction(log_to_stderr, nullptr);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    // The third silent death shape: std::terminate (an exception escaping a
    // noexcept boundary or a thread, a double-throw in unwind). Neither the
    // catch below nor the SEH filter above sees it.
    std::set_terminate([] {
        crash_log("ProjectIo: std::terminate called (exception escaped a "
                  "noexcept boundary or a thread)");
        std::abort();
    });
    try
    {
        // Headless visual-verification mode: `ProjectIo --verify [script.lua] [--bless]`
        // runs a deterministic capture session and exits, instead of the interactive
        // loop. With --bless each capture overwrites its golden reference (golden-image
        // diffing). See docs/development/DEVELOPMENT_PRACTICES.md § Visual verification.
        bool bless = false;
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--bless")
                bless = true;

        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--export-blackboard")
            {
                std::string which = "all";
                std::string out_dir = "blackboards";
                int ticks = 12; // matches the app's warm-start (three in-game years)
                if (i + 1 < argc && argv[i + 1][0] != '-')
                    which = argv[i + 1];
                for (int j = i + 1; j < argc; ++j)
                {
                    if (std::string(argv[j]) == "--out" && j + 1 < argc)
                        out_dir = argv[j + 1];
                    if (std::string(argv[j]) == "--ticks" && j + 1 < argc)
                        ticks = std::max(1, std::atoi(argv[j + 1]));
                }
                return run_blackboard_export(which, out_dir, ticks);
            }
        }

        // --spectate-headless (BL-1325): a spectated campaign to a year, no
        // window, uncapped. Dispatched before any SDL/renderer/ImGui exists.
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--spectate-headless")
            {
                int           until   = 2040;
                std::uint32_t seed    = 0;
                std::string   out_dir = "spectate_runs";
                for (int j = i + 1; j < argc; ++j)
                {
                    const std::string a = argv[j];
                    if (a == "--until" && j + 1 < argc) until = std::atoi(argv[j + 1]);
                    if (a == "--seed" && j + 1 < argc)
                        seed = static_cast<std::uint32_t>(std::strtoul(argv[j + 1], nullptr, 10));
                    if (a == "--out" && j + 1 < argc) out_dir = argv[j + 1];
                }
                return run_spectate_headless(until, seed, out_dir);
            }
        }

        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--serve")
            {
                int ticks = 12; // matches the app's warm-start (three in-game years)
                // BL-387: the session actor. Absent -> the player corp (resolved
                // once the world exists, so -1 is the "default" sentinel here).
                // `--as any` is the explicit research opt-in (bot-vs-bot corpus
                // generation) — the permissive mode must be asked for.
                long long as_corp = -1;
                bool      as_any  = false;
                for (int j = i + 1; j < argc; ++j)
                {
                    if (std::string(argv[j]) == "--ticks" && j + 1 < argc)
                        ticks = std::max(1, std::atoi(argv[j + 1]));
                    if (std::string(argv[j]) == "--as" && j + 1 < argc)
                    {
                        if (std::string(argv[j + 1]) == "any")
                            as_any = true;
                        else
                            as_corp = std::atoll(argv[j + 1]);
                    }
                }
                return run_serve(ticks, as_corp, as_any);
            }
        }

        // --verify-all [dir]: the whole committed suite against ONE world
        // generation (BL-423 — regenerating the identical deterministic world
        // per script cost ~40 s x 79 launches). Checked before --verify so the
        // longer flag is not shadowed by a prefix scan; both are exact matches,
        // but the order documents the intent.
        // --verify-budget <seconds>: the per-script wall-clock backstop (BL-714 /
        // NR-695). 0 disables it. Parsed before the dispatch below so both --verify
        // and --verify-all honour it.
        double verify_budget = -1.0;
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (std::string(argv[i]) == "--verify-budget")
                verify_budget = std::atof(argv[i + 1]);
        }

        // "The next argument, unless it is a flag." It used to read "unless it is
        // --bless", which meant `--verify-all --verify-budget 60` took
        // "--verify-budget" as the SCRIPT DIRECTORY and swept nothing. Any leading
        // "--" is a flag; a path never starts with one.
        const auto positional = [argc, argv](int i, const char* fallback) {
            if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0)
                return std::string(argv[i + 1]);
            return std::string(fallback);
        };

        // `app` is neither copyable nor movable, so it is built in place and the
        // budget written onto it before the run starts.
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--verify-all")
            {
                auto a_owner = std::make_unique<app>(); // heap: seven ~180 KB app locals overflowed the 1 MB Debug stack (2026-10-14)
                app& a = *a_owner;
                if (verify_budget >= 0.0) a.m_verify_script_budget_s = verify_budget;
                return a.run_verify_all(positional(i, "scripts/verify"), bless);
            }
        }

        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--verify")
            {
                auto a_owner = std::make_unique<app>(); // heap: seven ~180 KB app locals overflowed the 1 MB Debug stack (2026-10-14)
                app& a = *a_owner;
                if (verify_budget >= 0.0) a.m_verify_script_budget_s = verify_budget;
                return a.run_verify(positional(i, "scripts/verify/corporation_lens.lua"), bless);
            }
        }

        // --host-agent [port]  (BL-412): the RENDERED app opens the live agent
        // control seam — a loopback listen socket speaking --serve's line
        // protocol, drained at econ tick boundaries (src/core/agent_seam.hpp).
        // Off by default: an interactive session grows a network listener only
        // when explicitly asked to host one. Composes with --autostart-play
        // (open the game for watching, then attach an agent). Default port
        // 7717; the engine still only LISTENS (AI_OPPONENT.md § 10).
        uint16_t agent_port = 0;
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--host-agent")
            {
                agent_port = 7717;
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    const long p = std::atol(argv[i + 1]);
                    if (p < 1 || p > 65535)
                    {
                        std::fprintf(stderr,
                                     "ProjectIo: --host-agent port must be 1..65535\n");
                        return 1;
                    }
                    agent_port = static_cast<uint16_t>(p);
                }
            }
        }

        // --spectate (BL-695): open the session with NOBODY SEATED, so the
        // scored-utility layer evaluates every corporation and the human only
        // watches (AI_OPPONENT.md § 10i). Until now the mode was reachable
        // ONLY from `verify.spectate()` behind --verify, which made "watch the
        // AI play" a headless-script capability and not something a person
        // could do; this is the route in.
        //
        // ENTRY AT START, deliberately. § 10i removes the prohibition's
        // SUBJECT rather than excepting the rule, which is a fact about the
        // whole session. Flipping it mid-run would change, halfway through,
        // which corps the scorer may legally act on — a separate argument
        // nobody has made — so there is no in-game control that clears it.
        //
        // Applied to every RENDERED entry path below, the way --host-agent is
        // (review 2026-08-19 #9: a path that silently ignores a startup flag
        // is a bug). Not applied to --verify (scripts drive verify.spectate
        // themselves) nor to --serve (that seam's session actor IS a seat).
        bool spectate = false;
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--spectate")
                spectate = true;

        // --epoch <year>  (BL-705): the calendar year play opens on in every
        // world this process generates — world_params::epoch_year — and nothing
        // else. Generation reads no epoch, and the recipe band is derived per
        // nation from the history, never from a year (BL-1101, NR-929). EPOCH 0
        // IS RETIRED (Ben, 2026-09-25, NR-920; BL-1114): it once selected the
        // ancient roster, and a flag that silently kept meaning something else
        // would be worse than a refusal. There is no band override; a nation
        // opens ancient only where its own history never crossed the rung
        // (docs/generation/INDUSTRIALISATION.md § The epoch and the band).
        //
        // Applies to a NEW world. --load carries the save's own epoch. It is
        // parsed here, below the --verify / --verify-all / --serve dispatch, so
        // it does NOT reach those: their goldens and pinned bands are all taken
        // against the default world, and giving them a second epoch would mean
        // a second golden set. Deliberate, not an oversight.
        bool         epoch_set  = false;
        std::int64_t epoch_year = world_params{}.epoch_year;
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) != "--epoch")
                continue;
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "ProjectIo: --epoch needs a calendar year\n");
                return 1;
            }
            // Validated as the value that LANDS, not as a convenient parse: a
            // bare atoll would read "--epoch nineteen-sixty" as 0 and silently
            // generate the wrong world. Negative years are legitimate (BCE,
            // astronomical numbering), so the band is symmetric.
            const std::string  arg = argv[i + 1];
            std::size_t        used = 0;
            long long          y    = 0;
            try                       { y = std::stoll(arg, &used); }
            catch (const std::exception&) { used = 0; }
            if (used != arg.size() || y < -100000 || y > 100000)
            {
                std::fprintf(stderr,
                             "ProjectIo: --epoch expects a calendar year in "
                             "-100000..100000 (got \"%s\")\n", arg.c_str());
                return 1;
            }
            if (y == 0)
            {
                std::fprintf(stderr,
                             "ProjectIo: --epoch 0 is retired. It no longer selects the "
                             "ancient roster: the band comes from each nation's own "
                             "history. Omit --epoch for the 1960 start.\n");
                return 1;
            }
            epoch_year = y;
            epoch_set  = true;
        }

        // --seat <nation-id>  (BL-1076; BL-1281 names a MAJOR nation, whose
        // champion is seated): the seat, picked at launch rather than on
        // the selection canvas -- the route an agent takes to the pre-play act
        // (the in-play agent hosts refuse `take_seat`). AN UNTRUSTED INPUT
        // (io-standing-rules.md, AI-facing seam): parsed WIDE, the whole token
        // must be digits, and the value is range-checked against entity_id's
        // real domain before any narrowing -- a malformed or out-of-range id
        // refuses the launch outright rather than truncating into some other
        // firm. Whether the id names a MAJOR NATION of the world is the
        // tail's check (app::take_seat_pick), the only place that has the
        // ranking; a refused pick seats nobody and a headless run fails.
        bool      seat_set  = false;
        long long seat_corp = -1;
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) != "--seat")
                continue;
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "ProjectIo: --seat needs a major nation id\n");
                return 1;
            }
            const std::string arg  = argv[i + 1];
            std::size_t       used = 0;
            long long         v    = -1;
            try                           { v = std::stoll(arg, &used); }
            catch (const std::exception&) { used = 0; }
            const long long id_max =
                static_cast<long long>(std::numeric_limits<entity_id>::max());
            if (arg.empty() || used != arg.size() || arg[0] == '-' || arg[0] == '+'
                || v < 0 || v > id_max
                || static_cast<entity_id>(v) == null_entity)
            {
                std::fprintf(stderr,
                             "ProjectIo: --seat expects a major nation entity id in "
                             "0..%lld (got \"%s\")\n", id_max, arg.c_str());
                return 1;
            }
            seat_corp = v;
            seat_set  = true;
        }

        // --autostart: boot straight into a new campaign, headlessly, and exit.
        // Added 2026-08-12 to reproduce a crash that appears ONLY on the
        // interactive path: --verify calls setup_world directly and therefore
        // never reaches generate_background_firms or the warm start, so the
        // whole tail of start_new_game had no automated coverage at all.
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--autostart-windowed")
            {
                auto a_owner = std::make_unique<app>(); // heap: seven ~180 KB app locals overflowed the 1 MB Debug stack (2026-10-14)
                app& a = *a_owner;
                a.host_agent(agent_port);
                if (spectate)
                    a.spectate_session();
                if (epoch_set) a.set_epoch_year(epoch_year);
                if (seat_set) a.set_seat_pick(seat_corp);
                return a.run(app::autostart_mode::smoke);
            }

        // --bench-ground [--dwell <ms>] [--bench-background <s>]  (BL-1247):
        // the ground benchmark. NOT a --verify mode: --verify bakes what each
        // frame draws synchronously, which is the wrong thing to time. This is
        // the live app — the play walk with a reader's dwell on every round
        // (default 10 s; --dwell 0 clicks straight through), then a scripted
        // drive of the Planetary canvas on the reference 1720x1080 window. It
        // prints PASS/FAIL per budget of TECH_FOUNDATIONS.md § Target hardware,
        // the ground's total paint time and the process's peak memory, and exits.
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--bench-ground")
            {
                long long     dwell_ms     = 10000;
                double        background_s = 0.0;
                std::uint32_t seed         = 0; // the reference world: runs compare like with like
                for (int j = 1; j + 1 < argc; ++j)
                {
                    const std::string a = argv[j];
                    if (a == "--dwell")
                        dwell_ms = std::max(0LL, std::atoll(argv[j + 1]));
                    if (a == "--bench-background")
                        background_s = std::max(0.0, std::atof(argv[j + 1]));
                    if (a == "--bench-seed")
                        seed = static_cast<std::uint32_t>(std::strtoul(argv[j + 1], nullptr, 10));
                }
                auto a_owner = std::make_unique<app>();
                app& a = *a_owner;
                if (epoch_set) a.set_epoch_year(epoch_year);
                if (seat_set) a.set_seat_pick(seat_corp);
                a.set_bench_ground(dwell_ms, background_s, seed);
                return a.run(app::autostart_mode::bench_ground);
            }

        // --autostart-play: walk the wizard like the smoke test, then STAY OPEN.
        // Added 2026-08-16 after --autostart-windowed was used to open the game
        // for Ben to look at: it renders 120 frames and exits, so from the far
        // side of the screen it is indistinguishable from a crash ~2 s in. The
        // standing practice is to open the live app whenever he is asked to weigh
        // in on a visual, so that path needs a flag that does not self-terminate.
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--autostart-play")
            {
                auto a_owner = std::make_unique<app>(); // heap: seven ~180 KB app locals overflowed the 1 MB Debug stack (2026-10-14)
                app& a = *a_owner;
                a.host_agent(agent_port);
                if (spectate)
                    a.spectate_session();
                if (epoch_set) a.set_epoch_year(epoch_year);
                if (seat_set) a.set_seat_pick(seat_corp);
                return a.run(app::autostart_mode::play);
            }

        // --autostart-adopt (BL-1073): the wizard's last round builds the world,
        // Begin adopts it, the tail runs to play; prints the state hash that
        // tools/verify/begin_adopts_check.js compares with a cold --autostart.
        // Checked BEFORE --autostart only for readability: the match is exact.
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--autostart-adopt")
            {
                auto a_owner = std::make_unique<app>(); // heap: seven ~180 KB app locals overflowed the 1 MB Debug stack (2026-10-14)
                app& a = *a_owner;
                if (epoch_set) a.set_epoch_year(epoch_year);
                if (seat_set) a.set_seat_pick(seat_corp);
                return a.run_autostart_adopt();
            }

        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--autostart")
            {
                // Review 2026-08-19 #9: this path used to ignore --host-agent
                // silently; wire it like every sibling path.
                auto a_owner = std::make_unique<app>(); // heap: seven ~180 KB app locals overflowed the 1 MB Debug stack (2026-10-14)
                app& a = *a_owner;
                a.host_agent(agent_port);
                if (spectate)
                    a.spectate_session();
                if (epoch_set) a.set_epoch_year(epoch_year);
                if (seat_set) a.set_seat_pick(seat_corp);
                return a.run_autostart();
            }

        // --load <path>: open straight into a saved campaign (BL-536). Checked
        // last, alongside the plain interactive path it modifies, because it is
        // that path with a different starting point -- not a mode of its own.
        {
            std::string load_path;
            for (int i = 1; i < argc; ++i)
                if (std::string(argv[i]) == "--load" && i + 1 < argc)
                    load_path = argv[i + 1];

            auto a_owner = std::make_unique<app>(); // heap: seven ~180 KB app locals overflowed the 1 MB Debug stack (2026-10-14)
            app& a = *a_owner;
            a.host_agent(agent_port);
            if (spectate)
                a.spectate_session();
            if (epoch_set) a.set_epoch_year(epoch_year);
            if (seat_set) a.set_seat_pick(seat_corp);
            if (!load_path.empty())
                a.open_save(load_path);
            return a.run();
        }
    }
    catch (const std::exception& e)
    {
        // A malformed startup data file — e.g. a type-shape error in a user-edited
        // economy.lua / recipes.lua surfacing as a sol::error — throws rather than
        // aborting the process with an unhandled exception. Report and exit non-zero
        // (BL-110; the standing "no unprotected sol2 calls where errors can occur" rule).
        const std::string msg = std::string("ProjectIo: fatal error: ") + e.what();
        crash_log(msg.c_str());
        return 1;
    }
}
