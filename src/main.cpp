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

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
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

/// Fractional sibling of kv_get. `quantity` and `floor_price` (BL-293) are real
/// quantities and real prices — routed through the integer getter above they
/// would silently truncate, so `floor_price=3.75` would list at 3 and undercut
/// the floor the caller asked for. A rounding error in a price is not a rounding
/// error, it is a different order.
double kv_getf(const std::unordered_map<std::string, std::string>& kv, const char* key, double dflt)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return dflt;
    return std::atof(it->second.c_str());
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
        // BL-350's four distinguishable declines. Without these the switch fell
        // through and reported every one of them as "rejected_invalid", which
        // tells an agent its arguments were malformed when in fact they were
        // fine and the SUPPLIER said no. Typed, enumerated failure is the whole
        // reason an out-of-process policy can correct itself (AI_OPPONENT.md
        // § 10a) — collapsing four business outcomes into a syntax error is the
        // one thing that seam must not do. Exhaustive now, so -Wswitch catches
        // the next verb family the way it did not catch this one.
        case corp_command_result::rejected_no_capacity:     return "rejected_no_capacity";
        case corp_command_result::rejected_no_input_access: return "rejected_no_input_access";
        case corp_command_result::rejected_embargo:         return "rejected_embargo";
        case corp_command_result::rejected_reputation:      return "rejected_reputation";
    }
    return "rejected_invalid";
}

// --serve [--ticks N]  (BL-278)
//
// Headless, persistent: builds the canonical world once (identical warm-up to
// --export-blackboard), then reads one request per line from stdin until EOF
// or `SHUTDOWN`, applying the real per-tick sequence and the player-grade
// corp_command seam (apply_corp_command — no bypass). This is the process an
// out-of-process MCP server (tools/mcp/) spawns and talks to; it ships no
// network code itself; the line protocol below is the whole surface.
//
// Requests (space-separated `key=value` tokens after the opcode):
//   TICK                                            -> advance one tick
//   CORPS                                           -> one JSON line per corp, then END
//   BODIES                                          -> one JSON line per body, then END
//   BLACKBOARD corp=<id> ticks=<n>                  -> facts as BL-206 JSONL, then END
//   COMMAND corp=<id> verb=<0-14> subject=<id> tile=<id> type=<0-6> target=<0-30>
//           recipe=<id> workforce=<n> road_tier=<n> unit_type=<n>
//           quantity=<f> floor_price=<f> order=<n> counterparty=<id>
//                                                   -> apply_corp_command
//   SHUTDOWN                                        -> BYE, then exit
//
// Every `corp_verb` is reachable here; which keys a given verb reads is
// `apply_corp_command`'s business, and unread keys cost nothing. Keep this list
// in step with `corp_verb` — the last three verb families were added to the enum
// without their arguments ever reaching this parser, which made them applicable
// in the dictionary and inapplicable on the wire.
// Responses are one line each except BLACKBOARD, which is N JSONL lines + END.
int run_serve(int ticks)
{
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    world w = make_hard_coded_world();

    const uint16_t default_recipe = reg.recipe_id("steel");
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = default_recipe;

    // BL-365: real background corporations, generated now that reg is loaded.
    generate_background_firms(w, reg, /*seed=*/0x8A21F00Du);

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
            corp_command cmd;
            cmd.tick      = tick;
            cmd.corp      = static_cast<entity_id>(kv_get(kv, "corp", 0));
            cmd.verb      = static_cast<corp_verb>(kv_get(kv, "verb", 0));
            cmd.subject   = static_cast<entity_id>(kv_get(kv, "subject", 0));
            cmd.tile      = static_cast<entity_id>(kv_get(kv, "tile", 0));
            cmd.type      = static_cast<building_type>(kv_get(kv, "type", 0));
            cmd.target    = static_cast<resource_type>(kv_get(kv, "target", 0));
            cmd.recipe    = static_cast<uint16_t>(kv_get(kv, "recipe", no_recipe));
            cmd.workforce = static_cast<int>(kv_get(kv, "workforce", 100));
            cmd.road_tier = static_cast<uint8_t>(kv_get(kv, "road_tier", 1));
            // The seam grew three verb families past this parser — BL-324's
            // hire_unit, BL-293's order book, BL-350's procurement — and each
            // one's arguments went unread, so six of the fifteen verbs could
            // only ever be applied with defaults. place_sell_order defaults to
            // quantity 0, which apply_corp_command rejects outright; the three
            // procurement verbs default to order 0 / counterparty null, which
            // never names a real quote or supplier. They were not "partly
            // supported" — they were unreachable, and the dictionary said
            // otherwise.
            cmd.unit_type    = static_cast<uint16_t>(kv_get(kv, "unit_type", 0));
            cmd.quantity     = static_cast<float>(kv_getf(kv, "quantity", 0.0));
            cmd.floor_price  = static_cast<float>(kv_getf(kv, "floor_price", 0.0));
            cmd.order        = static_cast<uint32_t>(kv_get(kv, "order", 0));
            cmd.counterparty = static_cast<entity_id>(kv_get(kv, "counterparty", 0));

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
            // subject, and nothing else on this protocol yields one. The
            // blackboard's market facts are keyed by MARKET id, not by body, so an
            // agent reading state could see prices on a body it had no way to
            // name — it could know a market existed and could not sell into it.
            for (const auto& [id, b] : w.bodies)
            {
                const char* phase = "hidden";
                switch (b.survey.phase)
                {
                    case survey_phase::hidden:     phase = "hidden";     break;
                    case survey_phase::in_transit: phase = "in_transit"; break;
                    case survey_phase::scanning:   phase = "scanning";   break;
                    case survey_phase::surveyed:   phase = "surveyed";   break;
                }
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
            for (const auto& [id, corp] : w.corporations)
                std::cout << "{\"id\":" << static_cast<long>(id)
                          << ",\"name\":\"" << corp.name << "\""
                          << ",\"is_player\":" << (corp.is_player ? "true" : "false")
                          << ",\"home_nation\":" << static_cast<long>(corp.home_nation)
                          << "}" << std::endl;
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
    const uint16_t default_recipe = reg.recipe_id("steel");
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

} // namespace

int main(int argc, char* argv[])
{
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
                for (int j = i + 1; j < argc; ++j)
                    if (std::string(argv[j]) == "--ticks" && j + 1 < argc)
                        ticks = std::max(1, std::atoi(argv[j + 1]));
                return run_serve(ticks);
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

        return app{}.run();
    }
    catch (const std::exception& e)
    {
        // A malformed startup data file — e.g. a type-shape error in a user-edited
        // economy.lua / recipes.lua surfacing as a sol::error — throws rather than
        // aborting the process with an unhandled exception. Report and exit non-zero
        // (BL-110; the standing "no unprotected sol2 calls where errors can occur" rule).
        std::fprintf(stderr, "ProjectIo: fatal error: %s\n", e.what());
        return 1;
    }
}
