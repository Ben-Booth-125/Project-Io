// Headless agent-seam harness (BL-412; no SDL / Lua / ImGui).
//
// BL-412 lets the RENDERED app host an external agent: a loopback listen
// socket (src/core/agent_seam.{hpp,cpp}) speaking --serve's line protocol
// (src/core/agent_protocol.{hpp,cpp}), drained at econ tick boundaries into
// apply_corp_command. This harness replaces the rendered frame loop with the
// same pump/drain/step cycle the app runs, so the seam's two load-bearing
// claims are provable without a window:
//
//   R1  ITEM-SPANNING. A command schedule delivered over the REAL socket —
//       pipelined, chunk-order at TCP's mercy — produces a world state_hash
//       IDENTICAL to the same schedule applied in-process through
//       apply_corp_command, with arrival order as application order and every
//       command landing at a tick boundary. And the recorded transcript
//       (tick, corp, verb, args, result) REPLAYS to the same hash on a fresh
//       world: a play transcript is a replay artifact (AI_OPPONENT.md § 10h).
//
//   R2  UNTRUSTED BOUNDARY. A command with any out-of-domain field — an
//       out-of-range verb, an integer that would wrap its destination, a NaN
//       quantity, a double that is finite but infinite as the float it lands
//       as, a malformed lexeme — is rejected WHOLE with a typed reason, and
//       the world state_hash before the rejection equals the hash after. No
//       clamp, no truncation: the specific non-clamp case (workforce=250
//       neither applies as 250 nor as a clamped 200) is asserted on the
//       destination field itself.
//
// The scene is order_book_harness's shape: a hand-built registry + synthetic
// body/market/corp, so every precondition is stated here rather than hoped
// for from the generator. The client socket below is the HARNESS playing the
// agent's role — the outbound side lives in the test, never in the engine
// (AI_OPPONENT.md § 10: the engine only listens).
//
// Exits non-zero if any assertion FAILs.

#include "core/agent_protocol.hpp"
#include "core/agent_seam.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

// --------------------------------------------------------------------------
// Client-side socket shim (the harness's own; the engine ships listen-only).
// --------------------------------------------------------------------------
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
namespace {
using sock_t = SOCKET;
constexpr sock_t bad_sock = INVALID_SOCKET;
bool client_would_block() { return WSAGetLastError() == WSAEWOULDBLOCK; }
constexpr int client_send_flags = 0; // Windows has no SIGPIPE
void client_close(sock_t s) { closesocket(s); }
bool client_nonblocking(sock_t s) { u_long one = 1; return ioctlsocket(s, FIONBIO, &one) == 0; }
bool client_startup()
{
    static WSADATA wsa{};
    static const bool ok = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    return ok;
}
} // namespace
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
namespace {
using sock_t = int;
constexpr sock_t bad_sock = -1;
bool client_would_block() { return errno == EWOULDBLOCK || errno == EAGAIN; }
constexpr int client_send_flags = MSG_NOSIGNAL; // a dropped seam must not SIGPIPE the harness
void client_close(sock_t s) { ::close(s); }
bool client_nonblocking(sock_t s)
{
    const int f = fcntl(s, F_GETFL, 0);
    return f != -1 && fcntl(s, F_SETFL, f | O_NONBLOCK) != -1;
}
bool client_startup() { return true; }
} // namespace
#endif

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }
int         vi(corp_verb v)     { return static_cast<int>(v); }

std::size_t count_of(const std::string& hay, const std::string& needle)
{
    std::size_t n = 0, at = 0;
    while ((at = hay.find(needle, at)) != std::string::npos)
    {
        ++n;
        at += needle.size();
    }
    return n;
}

// --------------------------------------------------------------------------
// Registry + scene (order_book_harness's shape)
// --------------------------------------------------------------------------
recipe_registry make_registry()
{
    recipe_registry reg;

    building_economics extraction;
    extraction.base_rate            = 20.0f;
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    recipe steel;
    steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
    steel.outputs[ri(resource_type::steel)]   = 1.0f;
    reg.add_recipe(steel);

    return reg;
}

struct scene
{
    world     w;
    entity_id body   = null_entity;
    entity_id tile   = null_entity;
    entity_id market = null_entity;
    entity_id corp   = null_entity; ///< The session actor (the seat the agent occupies).
    entity_id bld    = null_entity; ///< Completed extraction site owned by `corp`.
    entity_id other  = null_entity; ///< A corp the session is NOT (the actor-gate probe).
};

/// Deterministic by construction: entity ids come from world's own counter in
/// a fixed creation order, so three make_scene() calls yield identical worlds.
scene make_scene()
{
    scene s;

    s.body = s.w.create_entity();
    {
        body_component bc{};
        bc.name              = "Anvil";
        bc.type              = body_type::planet;
        bc.orbital_radius_au = 1.0f;
        bc.grid_width = bc.grid_height = 4;
        bc.survey.phase = survey_phase::surveyed;
        s.w.bodies[s.body] = bc;
    }

    s.tile = s.w.create_entity();
    {
        tile_component tc{};
        tc.body        = s.body;
        tc.grid_x      = 0;
        tc.grid_y      = 0;
        tc.substrate = terrain_substrate::rocky;
        tc.landform    = terrain_landform::plains;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = 1.0f;
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
        s.w.tiles[s.tile] = tc;
    }

    s.market = s.w.create_entity();
    {
        market_component mc{};
        mc.body = s.body;
        mc.base_price[ri(resource_type::iron_ore)] = 4.0f;
        mc.base_price[ri(resource_type::steel)]    = 8.0f;
        mc.price = mc.base_price;
        s.w.markets[s.market] = mc;
    }

    s.bld = s.w.create_entity();
    {
        building_component b{};
        b.tile               = s.tile;
        b.type               = building_type::extraction_site;
        b.target_resource    = resource_type::iron_ore;
        b.workforce_assigned = 0.5f;
        b.workforce_target   = 100;
        b.workforce_auto     = false;
        s.w.buildings[s.bld] = b;
    }

    s.corp = s.w.create_entity();
    {
        corporation_component cc;
        cc.name             = "Session Actor Co";
        cc.is_player        = true; // the seat the agent occupies, as in the live app
        cc.starting_capital = 1000.0f;
        cc.balance          = 1000.0f;
        cc.assets.push_back(s.bld);
        s.w.corporations[s.corp] = cc;
    }
    s.w.player_entity = s.corp;

    s.other = s.w.create_entity();
    {
        corporation_component cc;
        cc.name    = "Bystander Holdings";
        cc.balance = 0.0f;
        s.w.corporations[s.other] = cc;
    }

    // Stock to sell: 100 steel in the (corp, body) pool.
    s.w.pool_for(s.corp, s.body).quantities[ri(resource_type::steel)] = 100.0f;

    return s;
}

/// One econ tick, identical for the socket run, the in-process run and the
/// replay — the deterministic frame both hashes are measured in.
void step_tick(world& w, const recipe_registry& reg, int t)
{
    w.current_day_tick = t;
    economy_report rep   = run_economy_step(w, reg);
    auto           flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets, &rep.buildings);
}

// --------------------------------------------------------------------------
// The agent side: a plain TCP client on 127.0.0.1
// --------------------------------------------------------------------------
sock_t connect_client(uint16_t port)
{
    if (!client_startup())
        return bad_sock;
    const sock_t c = ::socket(AF_INET, SOCK_STREAM, 0);
    if (c == bad_sock)
        return bad_sock;
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(port);
    if (::connect(c, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0)
    {
        client_close(c);
        return bad_sock;
    }
    if (!client_nonblocking(c))
    {
        client_close(c);
        return bad_sock;
    }
    return c;
}

void client_recv_into(sock_t c, std::string& rx)
{
    char buf[4096];
    for (;;)
    {
        const auto n = ::recv(c, buf, static_cast<int>(sizeof buf), 0);
        if (n > 0)
        {
            rx.append(buf, static_cast<std::size_t>(n));
            continue;
        }
        return; // closed or would-block: either way, done for this poll
    }
}

/// Drive the seam exactly as app::run does — pump each "frame", and when a
/// TICK awaits its boundary: drain (commands land, stamped with the completed
/// tick), step one econ tick, resolve the TICK. Returns when the client has
/// seen BYE (or the iteration guard trips).
struct socket_run_result
{
    int         final_tick = 0;
    std::string rx;      ///< Everything the client received.
    bool        saw_bye = false;
};

socket_run_result drive_socket_session(scene& s, const recipe_registry& reg,
                                       agent_seam& seam, sock_t client,
                                       const std::string& script)
{
    socket_run_result r;

    std::size_t sent = 0;
    while (sent < script.size())
    {
        const auto n = ::send(client, script.data() + sent,
                              static_cast<int>(script.size() - sent), client_send_flags);
        if (n > 0)
        {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        if (!client_would_block())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (int guard = 0; guard < 5000 && !r.saw_bye; ++guard)
    {
        seam.pump(s.w, reg, r.final_tick);
        if (seam.tick_pending())
        {
            seam.drain_boundary(s.w, reg, r.final_tick); // commands land pre-step,
            step_tick(s.w, reg, r.final_tick + 1);       // stamped with the tick
            ++r.final_tick;                              // just completed —
            seam.on_tick_stepped(r.final_tick);          // run_serve's exact contract
        }
        client_recv_into(client, r.rx);
        if (r.rx.find("BYE") != std::string::npos)
        {
            r.saw_bye = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return r;
}

} // namespace

int main()
{
    const recipe_registry reg = make_registry();

    // The schedule under test, as wire lines. Interval 0 (before the first
    // TICK): a public read plus one command; interval 1: two commands;
    // interval 2: one command. Three ticks total, then SHUTDOWN.
    const int steel = static_cast<int>(resource_type::steel);

    scene probe = make_scene(); // only for composing ids in the lines below
    const auto line_set_wf = [&](entity_id corp, entity_id bld, const char* wf) {
        return "COMMAND corp=" + std::to_string(corp)
             + " verb=" + std::to_string(vi(corp_verb::set_workforce))
             + " subject=" + std::to_string(bld)
             + " workforce=" + std::string(wf);
    };
    const std::string c1 = line_set_wf(probe.corp, probe.bld, "150");
    const std::string c2 = "COMMAND corp=" + std::to_string(probe.corp)
                         + " verb=" + std::to_string(vi(corp_verb::place_sell_order))
                         + " subject=" + std::to_string(probe.body)
                         + " target=" + std::to_string(steel)
                         + " quantity=20 floor_price=2.5";
    const std::string c3 = "COMMAND corp=" + std::to_string(probe.corp)
                         + " verb=" + std::to_string(vi(corp_verb::idle))
                         + " subject=" + std::to_string(probe.bld);
    const std::string c4 = "COMMAND corp=" + std::to_string(probe.corp)
                         + " verb=" + std::to_string(vi(corp_verb::resume))
                         + " subject=" + std::to_string(probe.bld);

    const std::string script = "CORPS\n" + c1 + "\nTICK\n"
                             + c2 + "\n" + c3 + "\nTICK\n"
                             + c4 + "\nTICK\nSHUTDOWN\n";

    // -----------------------------------------------------------------------
    // R1a — the socket run
    // -----------------------------------------------------------------------
    uint64_t                            hash_socket = 0;
    std::vector<agent_transcript_entry> transcript;
    int                                 final_tick = 0;
    {
        scene s = make_scene();

        agent_seam  seam;
        std::string err;
        const bool  listening = seam.listen(0, s.corp, /*as_any=*/false, &err);
        check(listening, "R1.0 seam listens on a loopback port");
        if (!listening)
        {
            std::printf("      listen error: %s\n", err.c_str());
            return 1;
        }

        const sock_t client = connect_client(seam.port());
        check(client != bad_sock, "R1.1 an external client can connect to it");
        if (client == bad_sock)
            return 1;

        const socket_run_result r = drive_socket_session(s, reg, seam, client, script);
        client_close(client);

        check(r.saw_bye, "R1.2 SHUTDOWN answers BYE (and only ends the session, not a process)");
        check(r.final_tick == 3, "R1.3 exactly the three requested ticks ran");
        check(count_of(r.rx, "RESULT result=applied") == 4,
              "R1.4 all four commands answer applied over the wire");
        check(r.rx.find("OK tick=1") != std::string::npos
                  && r.rx.find("OK tick=2") != std::string::npos
                  && r.rx.find("OK tick=3") != std::string::npos,
              "R1.5 each TICK resolves with its boundary's OK line");
        check(count_of(r.rx, "END") == 1 && r.rx.find("Session Actor Co") != std::string::npos,
              "R1.6 the CORPS read answered immediately, rows then END");
        // Arrival order is application order, and every command landed on a
        // boundary: the transcript tags must read 0, 1, 1, 2 in verb order
        // c1, c2, c3, c4.
        transcript = seam.transcript();
        const bool shape =
            transcript.size() == 4
            && transcript[0].tick == 0 && transcript[0].cmd.verb == corp_verb::set_workforce
            && transcript[1].tick == 1 && transcript[1].cmd.verb == corp_verb::place_sell_order
            && transcript[2].tick == 1 && transcript[2].cmd.verb == corp_verb::idle
            && transcript[3].tick == 2 && transcript[3].cmd.verb == corp_verb::resume;
        check(shape, "R1.7 transcript records (tick, corp, verb, args) in arrival order at boundaries");

        hash_socket = s.w.state_hash(r.final_tick);
        final_tick  = r.final_tick;
        check(s.w.buildings.at(s.bld).workforce_target == 150,
              "R1.8 the socket-delivered set_workforce actually landed");
    }

    // -----------------------------------------------------------------------
    // R1b — the same schedule, in-process (no socket, no parser)
    // -----------------------------------------------------------------------
    {
        scene s = make_scene();

        const auto apply = [&](corp_verb verb, int tick, float qty = 0.0f,
                               float floor = 0.0f, int wf = 100) {
            corp_command cmd;
            cmd.tick = tick;
            cmd.corp = s.corp;
            cmd.verb = verb;
            switch (verb)
            {
                case corp_verb::set_workforce:
                    cmd.subject   = s.bld;
                    cmd.workforce = wf;
                    break;
                case corp_verb::place_sell_order:
                    cmd.subject     = s.body;
                    cmd.target      = resource_type::steel;
                    cmd.quantity    = qty;
                    cmd.floor_price = floor;
                    break;
                default:
                    cmd.subject = s.bld;
                    break;
            }
            return apply_corp_command(s.w, reg, cmd);
        };

        bool all_applied = true;
        all_applied &= apply(corp_verb::set_workforce, 0, 0, 0, 150) == corp_command_result::applied;
        step_tick(s.w, reg, 1);
        all_applied &= apply(corp_verb::place_sell_order, 1, 20.0f, 2.5f) == corp_command_result::applied;
        all_applied &= apply(corp_verb::idle, 1) == corp_command_result::applied;
        step_tick(s.w, reg, 2);
        all_applied &= apply(corp_verb::resume, 2) == corp_command_result::applied;
        step_tick(s.w, reg, 3);

        check(all_applied, "R1.9 the in-process reference schedule applies cleanly");
        const uint64_t hash_direct = s.w.state_hash(3);
        check(hash_direct == hash_socket,
              "R1.10 socket-delivered schedule == in-process schedule (state_hash identical)");
    }

    // -----------------------------------------------------------------------
    // R1c — replay the recorded transcript on a fresh world
    // -----------------------------------------------------------------------
    {
        scene s = make_scene();

        bool results_match = true;
        std::size_t at = 0;
        for (int t = 0; t < final_tick; ++t)
        {
            for (; at < transcript.size() && transcript[at].tick == t; ++at)
                if (transcript[at].parse_ok)
                    results_match &=
                        apply_corp_command(s.w, reg, transcript[at].cmd)
                        == transcript[at].result;
            step_tick(s.w, reg, t + 1);
        }
        check(at == transcript.size(), "R1.11 replay consumed every transcript entry");
        check(results_match, "R1.12 every replayed command reproduces its recorded result");
        check(s.w.state_hash(final_tick) == hash_socket,
              "R1.13 the transcript REPLAYS to the same state_hash (replay artifact)");
    }

    // -----------------------------------------------------------------------
    // R2 — out-of-domain commands: rejected whole, typed, zero mutation
    // -----------------------------------------------------------------------
    {
        scene s = make_scene();

        agent_seam  seam;
        check(seam.listen(0, s.corp, false, nullptr), "R2.0 fresh seam listens");
        const sock_t client = connect_client(seam.port());
        check(client != bad_sock, "R2.1 client connects");
        if (client == bad_sock)
            return 1;

        const std::string cmd_head = "COMMAND corp=" + std::to_string(s.corp);
        const std::string bad =
              cmd_head + " verb=99 subject=" + std::to_string(s.bld) + "\n"                         // out-of-range verb
            + line_set_wf(s.corp, s.bld, "4294967396") + "\n"                                        // would wrap to 100
            + line_set_wf(s.corp, s.bld, "250") + "\n"                                               // in-type, out-of-domain
            + line_set_wf(s.corp, s.bld, "7xyz") + "\n"                                              // malformed lexeme
            + cmd_head + " verb=" + std::to_string(vi(corp_verb::place_sell_order))
                       + " subject=" + std::to_string(s.body) + " target=" + std::to_string(steel)
                       + " quantity=nan floor_price=2\n"                                             // NaN
            + cmd_head + " verb=" + std::to_string(vi(corp_verb::place_sell_order))
                       + " subject=" + std::to_string(s.body) + " target=" + std::to_string(steel)
                       + " quantity=5 floor_price=1e300\n"                                           // finite double, infinite float
            + "COMMAND corp=" + std::to_string(s.other) + " verb="
                       + std::to_string(vi(corp_verb::set_workforce))
                       + " subject=" + std::to_string(s.bld) + " workforce=50\n"                     // actor gate
            + "FROBNICATE\n";                                                                        // unknown op

        std::size_t sent = 0;
        while (sent < bad.size())
        {
            const auto n = ::send(client, bad.data() + sent,
                                  static_cast<int>(bad.size() - sent), client_send_flags);
            if (n > 0) { sent += static_cast<std::size_t>(n); continue; }
            if (!client_would_block()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const uint64_t h_before = s.w.state_hash(0);

        // Pump until the whole batch is queued (no TICK in it, so nothing can
        // resolve yet), then drain at a synthetic boundary WITHOUT stepping —
        // the isolation that lets the hash comparison see the rejections alone.
        std::string rx;
        for (int i = 0; i < 200; ++i)
        {
            seam.pump(s.w, reg, 0);
            client_recv_into(client, rx);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        seam.drain_boundary(s.w, reg, 0);
        for (int i = 0; i < 200 && count_of(rx, "\n") < 8; ++i)
        {
            seam.pump(s.w, reg, 0);
            client_recv_into(client, rx);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        client_close(client);

        const uint64_t h_after = s.w.state_hash(0);
        check(h_before == h_after,
              "R2.2 state_hash before == after: a rejection mutates NOTHING");
        check(count_of(rx, "RESULT result=rejected_invalid building=-1") == 6,
              "R2.3 all six malformed commands rejected WHOLE with the typed reason");
        check(count_of(rx, "RESULT result=rejected_not_owner building=-1") == 1,
              "R2.4 the actor gate refuses a corp the session is not (typed)");
        check(count_of(rx, "ERR unknown op") == 1,
              "R2.5 an unknown opcode answers ERR, not silence");
        check(s.w.buildings.at(s.bld).workforce_target == 100,
              "R2.6 workforce=250 neither applied nor CLAMPED to 200 (rejected whole)");
        check(s.w.sell_orders.empty(),
              "R2.7 no order book entry from the NaN / overflow prices");
    }

    std::printf("\nagent_seam_harness: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
