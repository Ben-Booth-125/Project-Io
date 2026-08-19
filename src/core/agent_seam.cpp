#include "agent_seam.hpp"

#include "world/world.hpp"

#include <cstdio>
#include <sstream>

// ---------------------------------------------------------------------------
// Socket portability shim — the ONLY network code in the engine, and all of it
// LISTEN-side (AI_OPPONENT.md § 10: no outbound connection, no HTTP client, no
// API key). Winsock and BSD sockets differ in fd type, close(), non-blocking
// ioctl and the would-block errno; everything below folds those four
// differences into sock_* helpers so the seam logic reads once.
// ---------------------------------------------------------------------------
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
constexpr sock_t sock_invalid = INVALID_SOCKET;
constexpr int    sock_send_flags = 0; // Windows has no SIGPIPE
bool sock_startup()
{
    static bool done = false;
    static bool ok   = false;
    if (!done)
    {
        WSADATA wsa{};
        ok   = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
        done = true;
    }
    return ok;
}
void sock_close(sock_t s) { closesocket(s); }
bool sock_set_nonblocking(sock_t s)
{
    u_long one = 1;
    return ioctlsocket(s, FIONBIO, &one) == 0;
}
bool sock_would_block() { return WSAGetLastError() == WSAEWOULDBLOCK; }
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
constexpr sock_t sock_invalid = -1;
// Without MSG_NOSIGNAL a send() to a peer that already closed raises SIGPIPE
// and KILLS THE PROCESS — an agent dying mid-session must never take the
// rendered app (or a harness) down with it.
constexpr int sock_send_flags = MSG_NOSIGNAL;
bool sock_startup() { return true; }
void sock_close(sock_t s) { ::close(s); }
bool sock_set_nonblocking(sock_t s)
{
    const int flags = fcntl(s, F_GETFL, 0);
    return flags != -1 && fcntl(s, F_SETFL, flags | O_NONBLOCK) != -1;
}
bool sock_would_block() { return errno == EWOULDBLOCK || errno == EAGAIN; }
} // namespace
#endif

namespace {

sock_t as_sock(long long fd) { return static_cast<sock_t>(fd); }

/// First whitespace-delimited token of a request line — the opcode.
std::string op_of(const std::string& line)
{
    std::istringstream iss(line);
    std::string op;
    iss >> op;
    return op;
}

} // namespace

agent_seam::~agent_seam()
{
    shutdown();
}

bool agent_seam::listen(uint16_t port, entity_id actor, bool as_any, std::string* err)
{
    if (listening())
        return true;

    if (!sock_startup())
    {
        if (err) *err = "socket layer initialisation failed";
        return false;
    }

    const sock_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == sock_invalid)
    {
        if (err) *err = "socket() failed";
        return false;
    }

    // Loopback ONLY. Binding INADDR_ANY would put a world-mutating protocol on
    // every interface of the machine; the seam's whole transport claim is that
    // nothing off this host can reach it.
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(port);

    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0)
    {
        if (err) *err = "bind(127.0.0.1:" + std::to_string(port) + ") failed";
        sock_close(fd);
        return false;
    }
    if (::listen(fd, 1) != 0)
    {
        if (err) *err = "listen() failed";
        sock_close(fd);
        return false;
    }
    if (!sock_set_nonblocking(fd))
    {
        if (err) *err = "could not set the listener non-blocking";
        sock_close(fd);
        return false;
    }

    // Report the bound port (port 0 asks the OS to pick — the harness does).
    sockaddr_in bound{};
#ifdef _WIN32
    int blen = sizeof bound;
#else
    socklen_t blen = sizeof bound;
#endif
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &blen) == 0)
        m_port = ntohs(bound.sin_port);
    else
        m_port = port;

    m_listen_fd      = static_cast<long long>(fd);
    m_session.actor  = actor;
    m_session.as_any = as_any;
    return true;
}

void agent_seam::accept_pending(pump_events& ev)
{
    for (;;)
    {
        const sock_t c = ::accept(as_sock(m_listen_fd), nullptr, nullptr);
        if (c == sock_invalid)
            return; // would-block or error: nothing (more) pending either way

        if (client_connected())
        {
            // One seat. A second connection is refused outright rather than
            // queued — silently interleaving two agents' commands under one
            // session actor would make the transcript lie about who played.
            sock_close(c);
            continue;
        }
        if (!sock_set_nonblocking(c))
        {
            sock_close(c);
            continue;
        }
        m_client_fd = static_cast<long long>(c);
        ev.attached = true;
    }
}

bool agent_seam::read_client(pump_events& ev)
{
    char buf[4096];
    for (;;)
    {
        const auto n = ::recv(as_sock(m_client_fd), buf, static_cast<int>(sizeof buf), 0);
        if (n > 0)
        {
            m_inbuf.append(buf, static_cast<std::size_t>(n));
            // Untrusted boundary: a peer streaming bytes with no newline would
            // otherwise grow this buffer without bound.
            if (m_inbuf.size() > max_line_bytes)
            {
                drop_client(&ev);
                return false;
            }
            continue;
        }
        if (n == 0) // orderly close
        {
            drop_client(&ev);
            return false;
        }
        if (sock_would_block())
            return true;
        drop_client(&ev); // hard error
        return false;
    }
}

void agent_seam::split_lines()
{
    std::size_t start = 0;
    for (;;)
    {
        const std::size_t nl = m_inbuf.find('\n', start);
        if (nl == std::string::npos)
            break;
        std::string line = m_inbuf.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') // tolerate CRLF peers
            line.pop_back();
        m_lines.push_back(std::move(line));
        start = nl + 1;
    }
    m_inbuf.erase(0, start);
}

void agent_seam::queue_response(const std::string& out)
{
    if (!client_connected() || out.empty())
        return;
    m_outbuf += out;
    if (m_outbuf.size() > max_outbuf_bytes)
    {
        // A peer that sends requests but never reads responses would park the
        // whole app behind a full socket buffer. Backpressure by disconnect.
        drop_client(nullptr);
        return;
    }
    flush_outbuf();
}

bool agent_seam::flush_outbuf()
{
    while (client_connected() && !m_outbuf.empty())
    {
        const auto n = ::send(as_sock(m_client_fd), m_outbuf.data(),
                              static_cast<int>(m_outbuf.size()), sock_send_flags);
        if (n > 0)
        {
            m_outbuf.erase(0, static_cast<std::size_t>(n));
            continue;
        }
        if (n < 0 && sock_would_block())
            return true; // try again next pump
        drop_client(nullptr);
        return false;
    }
    return client_connected();
}

void agent_seam::drop_client(pump_events* ev)
{
    if (!client_connected())
        return;
    sock_close(as_sock(m_client_fd));
    m_client_fd = invalid_fd;
    // Unserviced input dies with the connection: a command that never got a
    // RESULT was never applied, and now never will be — the clean contract.
    // The transcript keeps everything that WAS applied.
    m_inbuf.clear();
    m_lines.clear();
    m_deferred.clear();
    m_outbuf.clear();
    m_tick_pending = false;
    if (ev)
        ev->detached = true;
}

agent_seam::pump_events agent_seam::pump(world& w, const recipe_registry& reg, int tick)
{
    pump_events ev;
    if (!listening())
        return ev;

    accept_pending(ev);
    if (!client_connected())
        return ev;

    if (!read_client(ev))
        return ev;
    split_lines();

    if (m_lines.size() + m_deferred.size() > max_queued_lines)
    {
        drop_client(&ev); // untrusted boundary: bounded request backlog
        return ev;
    }

    // Classify in strict arrival order, stopping at the first TICK — nothing
    // past a sequence point is even parsed until its boundary resolves.
    while (!m_tick_pending && !m_lines.empty())
    {
        std::string line = std::move(m_lines.front());
        m_lines.pop_front();

        const std::string op = op_of(line);
        if (op == "TICK")
        {
            m_tick_pending    = true;
            ev.tick_requested = true;
            break;
        }
        if (op == "COMMAND" || !m_deferred.empty())
        {
            // COMMANDs wait for the boundary; anything behind one waits too,
            // so responses stay in request order.
            m_deferred.push_back(std::move(line));
            continue;
        }

        // Read-only op with a clear queue: answer now.
        std::string out;
        const auto  hop = agent_protocol::service_line(line, w, reg, tick,
                                                       m_session, out, nullptr);
        queue_response(out);
        if (hop == agent_protocol::host_op::shutdown)
        {
            // The AGENT leaves; the app stays. run_serve exits the process
            // here, but this host's process is somebody's open game window.
            flush_outbuf();
            drop_client(&ev);
            return ev;
        }
        if (!client_connected())
            return ev; // queue_response may have dropped a non-reading peer
    }

    flush_outbuf();
    return ev;
}

void agent_seam::drain_boundary(world& w, const recipe_registry& reg, int tick_completed)
{
    if (!client_connected() || m_deferred.empty())
        return;

    while (!m_deferred.empty())
    {
        std::string line = std::move(m_deferred.front());
        m_deferred.pop_front();

        std::string                  out;
        agent_protocol::command_echo echo;
        const auto hop = agent_protocol::service_line(line, w, reg, tick_completed,
                                                      m_session, out, &echo);
        if (echo.was_command)
        {
            agent_transcript_entry e;
            e.tick     = tick_completed;
            e.parse_ok = echo.parse_ok;
            e.cmd      = echo.cmd;
            e.result   = echo.result;
            e.line     = std::move(line);
            m_transcript.push_back(std::move(e));
        }
        queue_response(out);
        if (hop == agent_protocol::host_op::shutdown)
        {
            flush_outbuf();
            drop_client(nullptr);
            return;
        }
        if (!client_connected())
            return;
    }
}

void agent_seam::on_tick_stepped(int new_tick)
{
    if (!m_tick_pending)
        return;
    m_tick_pending = false;
    if (client_connected())
        queue_response("OK tick=" + std::to_string(new_tick) + "\n");
}

bool agent_seam::write_transcript(const std::string& path) const
{
    if (m_transcript.empty())
        return true;
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    for (const agent_transcript_entry& e : m_transcript)
        std::fprintf(f, "T=%d R=%s %s\n", e.tick,
                     agent_protocol::result_name(e.result), e.line.c_str());
    std::fclose(f);
    return true;
}

void agent_seam::close_client()
{
    drop_client(nullptr);
}

void agent_seam::shutdown()
{
    drop_client(nullptr);
    if (listening())
    {
        sock_close(as_sock(m_listen_fd));
        m_listen_fd = invalid_fd;
    }
}
