#pragma once

// ---------------------------------------------------------------------------
// agent_seam — the rendered app hosts a live agent (BL-412)
// ---------------------------------------------------------------------------
// BL-278's --serve is headless-only: main.cpp branches to the line-protocol
// loop INSTEAD of app::run, so an MCP-attached agent plays invisibly. This
// class is the other configuration: the same line protocol (agent_protocol),
// served from inside the rendered app's frame loop, so a human can WATCH an
// external agent play. Research/diagnostics seam per AI_OPPONENT.md § 10g/10h;
// the shipped rival remains the deterministic scorer.
//
// Transport (the § 10 invariant, verbatim): the engine only LISTENS. One
// loopback TCP socket bound to 127.0.0.1, non-blocking, polled from the main
// thread once per frame — no worker thread, no outbound connection, no HTTP
// client, no API key. A local socket the agent connects TO is inside the
// no-cloud line; anything the engine dials out to is not.
//
// Clock & determinism contract (the load-bearing part):
//   - Commands apply at ECON TICK BOUNDARIES ONLY, in arrival order, through
//     apply_corp_command — never mid-interval, where the outcome would depend
//     on which wall-clock day the bytes happened to land.
//   - Every serviced COMMAND is recorded (tick, corp, verb, args, result), so
//     a play transcript is a replay artifact (AI_OPPONENT.md § 10h): applying
//     the same commands at the same boundaries reproduces the same
//     world::state_hash.
//   - The agent gates the clock (BL-412's deliberate pick): the app pauses
//     the sim when a client attaches, and a TICK request releases exactly one
//     econ tick. The human can unpause and reclaim the clock — commands still
//     land only at boundaries, so the transcript stays a replay artifact
//     either way; TICK then waits for the next natural boundary.
//
// Request scheduling, precisely (this is run_serve's strict request order,
// time-sliced onto the frame loop):
//   - CORPS / BODIES / BLACKBOARD / SHUTDOWN / unknown ops answer immediately
//     when nothing is queued ahead of them (they are read-only).
//   - COMMAND defers to the next boundary. Anything arriving BEHIND a
//     deferred COMMAND defers too, so responses never overtake requests —
//     the wire stays framable by a lockstep client.
//   - TICK is a sequence point: it resolves ("OK tick=<n>") when the boundary
//     completes, and no later line is even parsed until then — so pipelined
//     "COMMAND a / TICK / COMMAND b / TICK" applies a and b on consecutive
//     boundaries, exactly as the headless server would.
//   - At a boundary the deferred lines are serviced in arrival order against
//     the post-step world, each command stamped with the tick just completed.
//
// Untrusted boundary (io-standing-rules.md): the peer is arbitrary local
// software. Field validation lives in agent_protocol (reject whole, mutate
// nothing); THIS layer enforces the transport's own domains — a line cap, a
// queue cap and an outbound-buffer cap, each of which drops the connection
// rather than degrading the app. One client at a time; later connections are
// refused until the seat frees.

#include "agent_protocol.hpp"
#include "world/corp_command.hpp"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

struct world;
class recipe_registry;

/// One recorded COMMAND: the replay artifact's row. `tick` is the econ tick
/// the command was applied at (the boundary it landed on); `line` is the raw
/// wire line; `cmd` is meaningful only when `parse_ok`.
struct agent_transcript_entry
{
    int                 tick     = 0;
    bool                parse_ok = false;
    corp_command        cmd{};
    corp_command_result result = corp_command_result::rejected_invalid;
    std::string         line;
};

class agent_seam
{
public:
    agent_seam() = default;
    ~agent_seam();
    agent_seam(const agent_seam&)            = delete;
    agent_seam& operator=(const agent_seam&) = delete;

    /// Bind + listen on 127.0.0.1:@p port (0 = OS-assigned, see port()).
    /// @p actor / @p as_any: the session gate, exactly --serve's --as.
    /// Returns false (with @p err filled) on any socket failure.
    bool listen(uint16_t port, entity_id actor, bool as_any,
                std::string* err = nullptr);

    bool     listening() const { return m_listen_fd != invalid_fd; }
    bool     client_connected() const { return m_client_fd != invalid_fd; }
    uint16_t port() const { return m_port; }

    /// Re-point the session actor (the app pins it to the player corp once a
    /// campaign exists — the seat the agent occupies).
    void set_actor(entity_id actor) { m_session.actor = actor; }

    struct pump_events
    {
        bool attached       = false; ///< A client connected this pump.
        bool detached       = false; ///< The client disconnected (or was dropped).
        bool tick_requested = false; ///< A TICK reached the queue head; the host
                                     ///< should release its clock if it is gated.
    };

    /// Per-frame, main thread: accept/read the socket, answer immediate
    /// requests, stop at the first TICK. Mutates @p w only through
    /// service_line's COMMAND path — which this pump never takes (COMMANDs
    /// defer to drain_boundary), so pump itself is world-read-only.
    pump_events pump(world& w, const recipe_registry& reg, int tick);

    /// True while a TICK awaits its boundary (pump consumes no further lines).
    bool tick_pending() const { return m_tick_pending; }

    /// Tick-boundary drain: service every deferred line in arrival order
    /// against the post-step world, stamping commands with @p tick_completed
    /// and recording them. Call once per econ boundary, after the step.
    void drain_boundary(world& w, const recipe_registry& reg, int tick_completed);

    /// Resolve a pending TICK with "OK tick=<new_tick>". Call after the
    /// boundary's step (and drain) completed.
    void on_tick_stepped(int new_tick);

    const std::vector<agent_transcript_entry>& transcript() const { return m_transcript; }

    /// Append-write the transcript as one line per command:
    /// "T=<tick> R=<result> <raw line>". Returns false on I/O failure.
    bool write_transcript(const std::string& path) const;

    /// Drop the client (keeps listening). Clears pending queues; keeps the
    /// transcript — what was applied stays applied and stays recorded.
    void close_client();

    /// Stop listening and drop the client.
    void shutdown();

private:
    static constexpr long long invalid_fd = -1;

    // Transport domains (untrusted boundary): violating any drops the client.
    static constexpr std::size_t max_line_bytes   = 64 * 1024;
    static constexpr std::size_t max_queued_lines = 1024;
    static constexpr std::size_t max_outbuf_bytes = 4 * 1024 * 1024;

    void accept_pending(pump_events& ev);
    bool read_client(pump_events& ev);          ///< false when the client dropped.
    void split_lines();
    void queue_response(const std::string& out); ///< Buffer + attempt to flush.
    bool flush_outbuf();                         ///< false when the client dropped.
    void drop_client(pump_events* ev);

    long long m_listen_fd = invalid_fd;
    long long m_client_fd = invalid_fd;
    uint16_t  m_port      = 0;

    agent_protocol::session m_session{};

    std::string             m_inbuf;    ///< Raw bytes awaiting a newline.
    std::deque<std::string> m_lines;    ///< Complete lines awaiting classification.
    std::deque<std::string> m_deferred; ///< Lines awaiting the next boundary.
    bool                    m_tick_pending = false;
    std::string             m_outbuf;   ///< Responses awaiting a writable socket.

    std::vector<agent_transcript_entry> m_transcript;
};
