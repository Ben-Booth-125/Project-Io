#include "core/agent_protocol.hpp"
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

// The line-protocol parsing, validation and opcode handlers moved VERBATIM to
// src/core/agent_protocol.{hpp,cpp} when BL-412 gave the protocol a second
// host (the rendered app's agent seam). run_serve below is now that shared
// unit's headless host: it owns the world, the warm-up and the clock (TICK),
// and delegates every request line to agent_protocol::service_line.

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
//           province=<province::id>
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

        // --autostart: boot straight into a new campaign, headlessly, and exit.
        // Added 2026-08-12 to reproduce a crash that appears ONLY on the
        // interactive path: --verify calls setup_world directly and therefore
        // never reaches generate_background_firms or the warm start, so the
        // whole tail of start_new_game had no automated coverage at all.
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--autostart-windowed")
            {
                app a;
                a.host_agent(agent_port);
                return a.run(app::autostart_mode::smoke);
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
                app a;
                a.host_agent(agent_port);
                return a.run(app::autostart_mode::play);
            }

        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--autostart")
            {
                // Review 2026-08-19 #9: this path used to ignore --host-agent
                // silently; wire it like every sibling path.
                app a;
                a.host_agent(agent_port);
                return a.run_autostart();
            }

        {
            app a;
            a.host_agent(agent_port);
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
