#include "core/app.hpp"
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
//   BLACKBOARD corp=<id> ticks=<n>                  -> facts as BL-206 JSONL, then END
//   COMMAND corp=<id> verb=<0-7> subject=<id> tile=<id> type=<0-4> target=<0-22>
//           recipe=<id> workforce=<n> road_tier=<n>  -> apply_corp_command
//   SHUTDOWN                                        -> BYE, then exit
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

    int tick = 0;
    for (int t = 1; t <= ticks; ++t)
    {
        tick = t;
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
            w.current_day_tick = tick;
            dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                             reg.logistics_cost(convoy_mode::space));
            advance_convoys(w);
            economy_report report = run_economy_step(w, reg);
            auto flows = clear_markets(w, reg, report);
            apply_budget(w, reg, flows, report.workforce_contention, &report.budgets,
                         &report.buildings); // BL-343: law enforcement seam
            advance_tech_gates(w); // BL-344: earn techs whose gate is now satisfied
            credit_arrived_convoys(w, tick);
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

            entity_id out_building = null_entity;
            const corp_command_result result = apply_corp_command(w, reg, cmd, &out_building);
            std::cout << "RESULT result=" << corp_command_result_name(result)
                      << " building=" << (result == corp_command_result::applied
                                               ? static_cast<long>(out_building)
                                               : -1)
                      << std::endl;
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
