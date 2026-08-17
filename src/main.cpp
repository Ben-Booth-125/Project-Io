#include "core/app.hpp"
#include "core/sim_loop.hpp"
#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/survey_system.hpp"
#include "world/tech_gate.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
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

/// Parse the ` key=value` tokens of a --serve request line into a lookup map.
/// The opcode itself (first token) is not included.
std::unordered_map<std::string, std::string> parse_kv_tokens(std::istringstream& iss)
{
    std::unordered_map<std::string, std::string> kv;
    std::string tok;
    while (iss >> tok)
    {
        const auto eq = tok.find('=');
        if (eq == std::string::npos)
            continue;
        kv[tok.substr(0, eq)] = tok.substr(eq + 1);
    }
    return kv;
}

long kv_get(const std::unordered_map<std::string, std::string>& kv, const char* key, long dflt)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return dflt;
    return std::atol(it->second.c_str());
}

/// BL-396: the checked sibling of kv_get, for the COMMAND parse — the one
/// opcode whose fields feed narrow types and array indices. Parses WIDE
/// (long long), then refuses (clears `ok`) anything that fails to parse as an
/// integer at all or falls outside [lo, hi], the destination's REAL domain.
/// Never truncates, never wraps, never clamps: `verb=256` used to truncate to
/// 0 and BUILD A BUILDING answering `applied`, `type=200` indexed the
/// per-type economics array out of bounds and segfaulted the process, and
/// `workforce=4294967396` wrapped to a legal 100 and applied. An absent key
/// keeps its default, which is in-domain by construction — absent is not
/// malformed, exactly kv_getf's reading.
///
/// strtoll saturates to LLONG_MAX/MIN on overflow, and every domain this
/// parser passes tops out at uint32's max, so the saturated value is itself
/// out of [lo, hi] and no errno check is needed.
long long kv_get_checked(const std::unordered_map<std::string, std::string>& kv,
                         const char* key, long long dflt,
                         long long lo, long long hi, bool* ok)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return dflt;
    char* end = nullptr;
    const long long v = std::strtoll(it->second.c_str(), &end, 10);
    // *end must be the terminator: "7xyz" is a malformed token, not a 7 with
    // decoration — tolerating the tail is the same silent substitution the
    // range check refuses, one lexeme later.
    if (end == it->second.c_str() || *end != '\0' || v < lo || v > hi)
    {
        *ok = false;
        return dflt;
    }
    return v;
}

/// Fractional sibling of kv_get. `quantity` and `floor_price` (BL-293) are real
/// quantities and real prices — routed through the integer getter above they
/// would silently truncate, so `floor_price=3.75` would list at 3 and undercut
/// the floor the caller asked for. A rounding error in a price is not a rounding
/// error, it is a different order.
///
/// NON-FINITE INPUT IS REJECTED, and `ok` is how — not by substituting the
/// default, which is a different order rather than a refused one.
///
/// `std::atof` happily parses "nan", "inf" and an overflowing "1e400", and the
/// seam's own validation cannot catch them: `place_sell_order` guards
/// `floor_price < 0.0f`, and every comparison against NaN is false, so a NaN
/// price passes validation, is stored in `world.sell_orders`, is folded into
/// `world::state_hash`, is written to the save stream — and reaches
/// `clear_markets`' book sort, where a value neither less than, equal to, nor
/// greater than any other stops the comparator being a strict weak ordering and
/// makes `std::sort` undefined behaviour. An infinity is the same story one step
/// later, overflowing a `static_cast<int>` in the procurement lead time.
///
/// The first cut of this guard returned the default instead, and that was wrong
/// for a reason worth keeping: the two keys sharing this getter have defaults
/// that mean OPPOSITE things downstream. `quantity`'s 0 is rejected by the seam,
/// so substituting it is a refusal by accident. `floor_price`'s 0 is meaningful
/// — the seam reads it as "accept the market price" — so substituting it turns
/// "sell only above this floor" into "sell at market, every tick", answers
/// `applied`, and issues no diagnostic. That is precisely the silent
/// order-substitution the truncation note above argues against, arrived at by a
/// different route.
double kv_getf(const std::unordered_map<std::string, std::string>& kv, const char* key,
               double dflt, bool* ok = nullptr)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return dflt; // absent is not malformed — the default stands
    // strtod with the same full-token rule as the integer path: atof parses
    // "abc" to 0.0, and 0.0 here means "accept the market price" — the exact
    // silent order-substitution the note above argues against.
    char* end = nullptr;
    const double v = std::strtod(it->second.c_str(), &end);
    if (end == it->second.c_str() || *end != '\0' || !std::isfinite(v))
    {
        if (ok) *ok = false;
        return dflt;
    }
    return v;
}

const char* corp_command_result_name(corp_command_result r)
{
    switch (r)
    {
        case corp_command_result::applied:          return "applied";
        case corp_command_result::rejected_no_corp:  return "rejected_no_corp";
        case corp_command_result::rejected_not_owner: return "rejected_not_owner";
        case corp_command_result::rejected_invalid:  return "rejected_invalid";
        case corp_command_result::rejected_placement: return "rejected_placement";
        case corp_command_result::rejected_funds:    return "rejected_funds";
        case corp_command_result::rejected_state:    return "rejected_state";
        case corp_command_result::rejected_tech_locked: return "rejected_tech_locked";
        case corp_command_result::rejected_era_locked: return "rejected_era_locked";
        case corp_command_result::rejected_depth_locked: return "rejected_depth_locked";
        // BL-350's four distinguishable declines. Without these the switch fell
        // through and reported every one of them as "rejected_invalid", which
        // tells an agent its arguments were malformed when in fact they were
        // fine and the SUPPLIER said no. Typed, enumerated failure is the whole
        // reason an out-of-process policy can correct itself (AI_OPPONENT.md
        // § 10a) — collapsing four business outcomes into a syntax error is the
        // one thing that seam must not do.
        //
        // Exhaustive now — but note that -Wswitch was ALREADY enabled and had
        // already been warning about exactly this. The build runs -Wall without
        // -Werror, so the warning was emitted on every compile and ignored. The
        // guard is the reader, not the flag.
        case corp_command_result::rejected_no_capacity:     return "rejected_no_capacity";
        case corp_command_result::rejected_no_input_access: return "rejected_no_input_access";
        case corp_command_result::rejected_embargo:         return "rejected_embargo";
        case corp_command_result::rejected_reputation:      return "rejected_reputation";
        case corp_command_result::rejected_cooldown:        return "rejected_cooldown"; // BL-430
    }
    return "rejected_invalid";
}

// --serve [--ticks N] [--as <corp-id|any>]  (BL-278)
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
//   COMMAND corp=<id> verb=<0-14> subject=<id> tile=<id> type=<building_type>
//           target=<resource_type>
//           recipe=<id> workforce=<n> road_tier=<n> unit_type=<n>
//           quantity=<f> floor_price=<f> order=<n> counterparty=<id>
//                                                   -> apply_corp_command
//   SHUTDOWN                                        -> BYE, then exit
//
// `type` and `target` carry the raw enum values of `building_type` and
// `resource_type`; deliberately NOT written out as numeric ranges here, because
// a hard-coded range is a second definition that goes stale the moment either
// enum grows. This block previously claimed `verb=<0-7>` while the enum held
// fifteen, and `type=<0-4>` while it held seven.
//
// Every `corp_verb` is reachable here; which keys a given verb reads is
// `apply_corp_command`'s business, and unread keys cost nothing. Keep this list
// in step with `corp_verb` — the last three verb families were added to the enum
// without their arguments ever reaching this parser, which made them applicable
// in the dictionary and inapplicable on the wire.
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
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    world w = make_hard_coded_world();

    const uint16_t default_recipe = reg.default_recipe_id(); // BL-429
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = default_recipe;

    // BL-365: real background corporations, generated now that reg is loaded.
    generate_background_firms(w, reg, /*seed=*/0x8A21F00Du);

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
    const auto step_one_tick = [&](int t) {
        w.current_day_tick = t;
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));
        advance_convoys(w);
        economy_report report = run_economy_step(w, reg);
        auto flows = clear_markets(w, reg, report);
        apply_budget(w, reg, flows, report.workforce_contention, &report.budgets,
                     &report.buildings); // BL-343: law enforcement seam
        advance_tech_gates(w); // BL-344: earn techs whose gate is now satisfied
        credit_arrived_convoys(w, t);
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
        std::istringstream iss(line);
        std::string op;
        iss >> op;

        if (op == "SHUTDOWN")
        {
            std::cout << "BYE" << std::endl;
            return 0;
        }
        else if (op == "TICK")
        {
            ++tick;
            step_one_tick(tick);
            std::cout << "OK tick=" << tick << std::endl;
        }
        else if (op == "BLACKBOARD")
        {
            const auto kv = parse_kv_tokens(iss);
            const entity_id corp = static_cast<entity_id>(kv_get(kv, "corp", 0));
            // BL-397: reads are gated by the same session actor as writes.
            // The blackboard is a corp's PRIVATE view — cash, pools, recipes —
            // and this opcode used to export whichever corp the line named.
            // The refusal keeps the lines-then-END shape a streaming client
            // frames by, so ERR here is a line before END, not a terminator.
            if (!as_any && corp != session_actor)
            {
                std::cout << "ERR result=rejected_not_owner" << std::endl;
                std::cout << "END" << std::endl;
                continue;
            }
            const int bb_ticks = static_cast<int>(kv_get(kv, "ticks", tick));
            const corp_blackboard bb = export_corp_blackboard(w, corp, bb_ticks);
            std::ostringstream out;
            to_jsonl(bb, out);
            std::cout << out.str();
            std::cout << "END" << std::endl;
        }
        else if (op == "COMMAND")
        {
            const auto kv = parse_kv_tokens(iss);

            // BL-396: every integer field parses WIDE and is range-checked
            // against its destination's REAL domain before any narrowing cast
            // (kv_get_checked). The narrow static_casts this replaces were the
            // wire's whole validation story, and each failure mode was a
            // different lie: truncation applied a command the caller never
            // sent, a wrap applied a legal-looking value, and an out-of-enum
            // `type` indexed past the economics table. A violation rejects the
            // COMMAND whole, before apply_corp_command.
            bool ok = true;
            constexpr long long id_max =
                static_cast<long long>(std::numeric_limits<entity_id>::max());
            const long long verb_v = kv_get_checked(kv, "verb", 0, 0, corp_verb_count - 1, &ok);
            const long long corp_v = kv_get_checked(kv, "corp", 0, 0, id_max, &ok);
            const long long subj_v = kv_get_checked(kv, "subject", 0, 0, id_max, &ok);
            const long long tile_v = kv_get_checked(kv, "tile", 0, 0, id_max, &ok);
            const long long type_v = kv_get_checked(kv, "type", 0, 0, building_type_count - 1, &ok);
            const long long tgt_v  = kv_get_checked(kv, "target", 0, 0,
                                                    static_cast<long long>(resource_count) - 1, &ok);
            const long long rcp_v  = kv_get_checked(kv, "recipe", no_recipe, 0, 0xFFFF, &ok);
            const long long wf_v   = kv_get_checked(kv, "workforce", 100, 0, 200, &ok);
            const long long road_v = kv_get_checked(kv, "road_tier", 1, 1, 3, &ok);
            // unit_type only needs to FIT its uint16 destination:
            // apply_corp_command range-checks it against the live
            // unit_roster_table() itself, so the wire's only job is stopping a
            // wrap from re-aiming it at a different roster row.
            const long long unit_v  = kv_get_checked(kv, "unit_type", 0, 0, 0xFFFF, &ok);
            const long long order_v = kv_get_checked(
                kv, "order", 0, 0,
                static_cast<long long>(std::numeric_limits<uint32_t>::max()), &ok);
            const long long cp_v    = kv_get_checked(kv, "counterparty", 0, 0, id_max, &ok);

            // Floats narrow FIRST, then test finiteness: 1e300 is a finite
            // double and an infinite float, and the seam stores floats.
            // kv_getf's own double-level guard still catches nan/inf/1e400 —
            // and see its comment for why the default must never stand in for
            // a malformed value. Negative quantity / floor_price is refused
            // here too: the seam guards them for place_sell_order, but a
            // malformed line should not reach the seam whatever the verb.
            bool floats_ok = true;
            const float qty_v   = static_cast<float>(kv_getf(kv, "quantity", 0.0, &floats_ok));
            const float floor_v = static_cast<float>(kv_getf(kv, "floor_price", 0.0, &floats_ok));
            if (!std::isfinite(qty_v) || !std::isfinite(floor_v)
                || qty_v < 0.0f || floor_v < 0.0f)
                floats_ok = false;

            if (!ok || !floats_ok)
            {
                std::cout << "RESULT result=rejected_invalid building=-1" << std::endl;
                continue;
            }

            // The seam grew three verb families past this parser — BL-324's
            // hire_unit, BL-293's order book, BL-350's procurement — and each
            // one's arguments went unread, so those verbs could only ever be
            // applied with defaults. FIVE were unreachable outright:
            // place_sell_order defaulted to quantity 0, which
            // apply_corp_command rejects before anything else;
            // remove_sell_order/accept_quote/cancel_contract defaulted to
            // order 0, naming no real order; request_quote defaulted to a null
            // counterparty. `hire_unit` was the SIXTH and the genuinely
            // partly-supported case — unit_type 0 is a valid roster index, so
            // it worked, but only ever raised roster row 0.
            corp_command cmd;
            cmd.tick         = tick;
            cmd.corp         = static_cast<entity_id>(corp_v);
            cmd.verb         = static_cast<corp_verb>(verb_v);
            cmd.subject      = static_cast<entity_id>(subj_v);
            cmd.tile         = static_cast<entity_id>(tile_v);
            cmd.type         = static_cast<building_type>(type_v);
            cmd.target       = static_cast<resource_type>(tgt_v);
            cmd.recipe       = static_cast<uint16_t>(rcp_v);
            cmd.workforce    = static_cast<int>(wf_v);
            cmd.road_tier    = static_cast<uint8_t>(road_v);
            cmd.unit_type    = static_cast<uint16_t>(unit_v);
            cmd.quantity     = qty_v;
            cmd.floor_price  = floor_v;
            cmd.order        = static_cast<uint32_t>(order_v);
            cmd.counterparty = static_cast<entity_id>(cp_v);

            // BL-387: refuse to act as a corp the session is not. Before this
            // gate the only validation of `corp` was that the corp EXISTS, so
            // any caller could command any rival by name — verified in play,
            // rival balances moved by tens of millions. The gate sits here and
            // not in apply_corp_command (see the protocol block above).
            if (!as_any && cmd.corp != session_actor)
            {
                std::cout << "RESULT result=rejected_not_owner building=-1" << std::endl;
                continue;
            }

            entity_id out_building = null_entity;
            const corp_command_result result = apply_corp_command(w, reg, cmd, &out_building);
            std::cout << "RESULT result=" << corp_command_result_name(result)
                      << " building=" << (result == corp_command_result::applied
                                               ? static_cast<long>(out_building)
                                               : -1)
                      << std::endl;
        }
        else if (op == "BODIES")
        {
            // What a body is called and whether it is surveyed yet. The sibling of
            // CORPS, and needed for the same reason (NR-061): `survey`,
            // `place_sell_order` and `request_quote` all take a BODY id as their
            // subject, and the protocol had no direct way to enumerate them.
            //
            // Precisely: the blackboard is not silent about bodies — `pool:<res>`
            // facts are keyed by the corp's own (corp, body) pool, and
            // `body_activity` by a known body — so an agent could recover the ids
            // of bodies it already trades on or already sees. What it could NOT
            // do is name a body it has no pool and no activity on, which is every
            // body worth SURVEYING, and it could never recover a name or a survey
            // phase for any of them. The market facts do not help: they are keyed
            // by MARKET id, so a price is legible on a body the agent cannot
            // address.
            // Ascending id, not map order. `w.bodies` is an unordered_map, and
            // an agent's transcript is a replay artifact — two runs of the same
            // seed must produce the same bytes, or a trace corpus records the
            // allocator's mood alongside the world's. CORPS sorts for the same
            // reason.
            std::vector<entity_id> body_ids;
            body_ids.reserve(w.bodies.size());
            for (const auto& [id, b] : w.bodies) body_ids.push_back(id);
            std::sort(body_ids.begin(), body_ids.end());
            for (const entity_id id : body_ids)
            {
                const body_component& b = w.bodies.at(id);
                const char* phase = "hidden";
                switch (b.survey.phase)
                {
                    case survey_phase::hidden:     phase = "hidden";     break;
                    case survey_phase::in_transit: phase = "in_transit"; break;
                    case survey_phase::scanning:   phase = "scanning";   break;
                    case survey_phase::surveyed:   phase = "surveyed";   break;
                }
                // NOTE for whoever reads this output: on a `hidden` body
                // regions_total is 0, and that means NOT YET COMPUTED, not
                // "nothing to survey" — the count is derived from the grid at
                // dispatch (survey_system.cpp). Reading 0 as "empty" is exactly
                // the misreading a pull-based fact interface invites, so the
                // field is emitted with the phase beside it rather than alone.
                std::cout << "{\"id\":" << static_cast<long>(id)
                          << ",\"name\":\"" << b.name << "\""
                          << ",\"survey\":\"" << phase << "\""
                          << ",\"regions_done\":" << b.survey.regions_done
                          << ",\"regions_total\":" << b.survey.regions_total
                          << "}" << std::endl;
            }
            std::cout << "END" << std::endl;
        }
        else if (op == "CORPS")
        {
            // Who can act on this seam: one JSON line per corporation, then END.
            // An agent's first question is "who am I?" — nothing else on the
            // protocol answers it (NR-061).
            // Ascending id, for the same reason BODIES sorts: a play transcript
            // is a replay artifact, and two runs of the same seed must agree
            // byte-for-byte or a trace corpus records the allocator's mood.
            std::vector<entity_id> corp_ids;
            corp_ids.reserve(w.corporations.size());
            for (const auto& [id, corp] : w.corporations) corp_ids.push_back(id);
            std::sort(corp_ids.begin(), corp_ids.end());
            for (const entity_id id : corp_ids)
            {
                const corporation_component& corp = w.corporations.at(id);
                std::cout << "{\"id\":" << static_cast<long>(id)
                          << ",\"name\":\"" << corp.name << "\""
                          << ",\"is_player\":" << (corp.is_player ? "true" : "false")
                          << ",\"home_nation\":" << static_cast<long>(corp.home_nation)
                          << "}" << std::endl;
            }
            std::cout << "END" << std::endl;
        }
        else
        {
            std::cout << "ERR unknown op '" << op << "'" << std::endl;
        }
    }
    return 0;
}



// --export-blackboard <corp|all> [--out <dir>] [--ticks N]  (BL-206)
//
// Headless: builds the canonical world, loads the Lua economy data, runs N
// warm-up ticks of the real per-tick sequence (dispatch/advance convoys →
// run_economy_step → clear_markets → apply_budget → credit convoys), then
// dumps each requested corp's visibility-honest blackboard as
// blackboard_<corp>_<tick>.jsonl. No SDL; deterministic by construction.
int run_blackboard_export(const std::string& which, const std::string& out_dir, int ticks)
{
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    world w = make_hard_coded_world();

    // Author default recipes onto generated processors (mirrors app::load_economy).
    const uint16_t default_recipe = reg.default_recipe_id(); // BL-429
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = default_recipe;

    // BL-365: real background corporations, generated now that reg is loaded.
    generate_background_firms(w, reg, /*seed=*/0x8A21F00Du);

    for (int t = 1; t <= ticks; ++t)
    {
        w.current_day_tick = t;
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));
        advance_convoys(w);
        economy_report report = run_economy_step(w, reg);
        auto flows = clear_markets(w, reg, report);
        apply_budget(w, reg, flows, report.workforce_contention, &report.budgets,
                     &report.buildings); // BL-343: law enforcement seam
        advance_tech_gates(w); // BL-344: earn techs whose gate is now satisfied
        credit_arrived_convoys(w, t);
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
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--verify-all")
            {
                std::string dir = "scripts/verify";
                if (i + 1 < argc && std::string(argv[i + 1]) != "--bless")
                    dir = argv[i + 1];
                return app{}.run_verify_all(dir, bless);
            }
        }

        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--verify")
            {
                // The script is the next non-flag argument, else the default.
                std::string script = "scripts/verify/corporation_lens.lua";
                if (i + 1 < argc && std::string(argv[i + 1]) != "--bless")
                    script = argv[i + 1];
                return app{}.run_verify(script, bless);
            }
        }

        // --autostart: boot straight into a new campaign, headlessly, and exit.
        // Added 2026-08-12 to reproduce a crash that appears ONLY on the
        // interactive path: --verify calls setup_world directly and therefore
        // never reaches generate_background_firms or the warm start, so the
        // whole tail of start_new_game had no automated coverage at all.
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--autostart-windowed")
                return app{}.run(app::autostart_mode::smoke);

        // --autostart-play: walk the wizard like the smoke test, then STAY OPEN.
        // Added 2026-08-16 after --autostart-windowed was used to open the game
        // for Ben to look at: it renders 120 frames and exits, so from the far
        // side of the screen it is indistinguishable from a crash ~2 s in. The
        // standing practice is to open the live app whenever he is asked to weigh
        // in on a visual, so that path needs a flag that does not self-terminate.
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--autostart-play")
                return app{}.run(app::autostart_mode::play);

        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--autostart")
                return app{}.run_autostart();

        return app{}.run();
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
