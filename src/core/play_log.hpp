#pragma once

// ---------------------------------------------------------------------------
// The play log — a write-only record of one SEATED session, for comparing how
// the human plays with how the AI plays (tools/session/play_vs_ai.js reads it).
// ---------------------------------------------------------------------------
//
// WHAT IT IS. One JSONL file per seated session, opened when the session
// reaches play (a new campaign's handoff, or a save loaded into play), at
// play_logs/<YYYYMMDD-HHMMSS>_seed<N>.jsonl. Three record kinds:
//
//   {"kind":"header", ...}   once: seed, epoch, the seat nation and champion,
//                            the build's git hash when the build knew it, the
//                            wall-clock time the log opened, every nation.
//   {"kind":"cmd", ...}      every command the human issues: each press that
//                            reaches apply_corp_command from app.cpp, and the
//                            four direct doors app.cpp drives without it
//                            (construction, road, demolition, survey). An
//                            agent attached through --host-agent is logged too,
//                            with "source":"agent".
//   {"kind":"quarter", ...}  once per econ tick (a quarter), plus one opening
//                            snapshot: every nation's treasury, budget weights,
//                            research, corps, income, population, and the AI
//                            corps' scored decisions since the last snapshot.
//
// THE LOG IS AN OBSERVATION AND NEVER AN INPUT. Every entry point takes
// `const world&` and reads no lazily-built cache; nothing in the simulation
// reads this object. The wall clock appears ONLY in the file name and the
// header's "opened_at" — it is never handed to the sim. The same property the
// decision trace asserts (decision_trace.hpp) holds here, and it is checked the
// same way: a headless `--autostart --seat` with IO_PLAY_LOG=1 hashes equal to
// one without.
//
// NEVER THROWS INTO THE SIM. A file that will not open, or a write that fails,
// is reported once on stderr and the log disables itself for the session.
// Each record is flushed as it is written, so a crash loses at most the
// record in flight.
//
// GATING. app::run (the interactive and windowed-autostart paths) logs unless
// the environment sets IO_PLAY_LOG=0; the headless --autostart logs only when
// IO_PLAY_LOG=1 (so harness runs do not litter play_logs/); --verify and a
// spectated session never log.

#include "world/corp_command.hpp"
#include "world/digitisation_tree_data.hpp"
#include "world/entity.hpp"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

struct world;
struct economy_report;

class play_log
{
public:
    play_log() = default;
    ~play_log() { close(); }
    play_log(const play_log&)            = delete;
    play_log& operator=(const play_log&) = delete;

    /// Open a fresh log for the session now reaching play and write the header
    /// and an opening snapshot. Closes any log already open. @p loaded_from is
    /// the save path when play opened from a load, else empty. @p seat_picked:
    /// the seat was taken by a pick (`--seat` / the seat screen), which is
    /// recorded as the session's first cmd line.
    /// @return true if the log is open.
    bool open(const world& w, const economy_report& last_report, std::uint32_t seed,
              int epoch_year, std::uint64_t day_tick, bool seat_picked,
              const std::string& loaded_from);

    void close();
    bool is_open() const { return m_file != nullptr; }
    const std::string& path() const { return m_path; }

    /// One command that went through apply_corp_command. @p source is
    /// "player" (a press in app.cpp) or "agent" (the --host-agent seam).
    void command(const world& w, const corp_command& cmd, corp_command_result r,
                 std::uint64_t day_tick, const char* source, bool parse_ok = true);

    /// One act through a door app.cpp drives WITHOUT apply_corp_command (build,
    /// road, demolish, survey). @p verb uses the cmd lines' verb names so the
    /// comparison counts them with the seam's; @p result is that door's own
    /// result name. The remaining fields are written when non-null/nonzero.
    void direct(const world& w, const char* verb, const char* result, std::uint64_t day_tick,
                entity_id tile, entity_id subject, int building_type, int resource, int road_tier);

    /// The quarter just stepped: every nation's state and the AI decisions
    /// recorded since the last snapshot. @p report is the step's report.
    void quarter(const world& w, const economy_report& report, std::uint64_t day_tick);

    /// The machine name of a verb (the enumerator's own spelling) — stable,
    /// exhaustive, and what play_vs_ai.js counts by.
    static const char* verb_name(corp_verb v);

private:
    void write_line(const std::string& line);
    void snapshot(const world& w, const economy_report& report, std::uint64_t day_tick,
                  bool opening);

    std::FILE*  m_file   = nullptr;
    bool        m_failed = false; ///< A write failed this session; stay closed.
    std::string m_path;
    entity_id   m_seat_nation = null_entity;
    int         m_quarters    = 0;

    /// Each nation's held set at the last snapshot, to name what it gained.
    std::map<entity_id, std::bitset<io::dg::node_count>> m_prev_held;
    /// `corp_decision_ring::total` at the last snapshot.
    std::size_t m_seen_decisions = 0;
};
