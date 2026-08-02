#include "history_log.hpp"

#include <algorithm>
#include <istream>
#include <ostream>
#include <utility>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Genesis + checkpoint chapter — the bridge
// ---------------------------------------------------------------------------

/// One line of a body's merged genesis+checkpoint chapter, before it is
/// appended to `world::history_log` (which additionally needs the body tag).
struct chapter_line
{
    int64_t       timestamp;
    history_topic topic;
    std::string   event;
    std::string   consequence;
};

/// Resolve a timestamp for @p c, a checkpoint that itself carries none
/// (checkpoint_record's shape is fixed — see planetology.hpp's own comment on
/// why a timestamp field is not added there). Rule: scan @p history (already
/// chronological, oldest first — history_ladder_harness's H4 asserts this) for
/// every dated line at or before the checkpoint's own stage, and take the LAST
/// (i.e. most recent) match; that is always at least the Accretion (S1) line,
/// which every body emits unconditionally, so the scan never comes up empty
/// in practice.
///
/// This is a DELIBERATE SIMPLIFICATION, not an exact per-line pairing: a stage
/// that resolves more than one checkpoint (Green's "land colonised" then
/// "combustion cleared/failed") does not have a clean 1:1 correspondence
/// between checkpoint_record entries and history_event lines at that stage —
/// some checkpoints have no dedicated history line at all (e.g. a body that
/// already terminated earlier still records a failing Spark checkpoint with
/// no Spark-tagged history line), and others share a stage with a sibling
/// checkpoint that DOES have its own line. Rather than duplicating
/// planetology.cpp's branch logic here to pair them exactly (fragile, and a
/// second place that logic could drift), every checkpoint at a given stage
/// resolves to that stage's LAST dated line — simple, deterministic, and
/// always at least as recent as the true moment. Flagged for Ben's review in
/// NEEDS_REVIEW.json rather than silently picked.
int64_t resolve_checkpoint_timestamp(const std::vector<history_event>& history,
                                     const checkpoint_record& c)
{
    int64_t best  = 0;
    bool    found = false;
    for (const history_event& h : history)
    {
        if (h.stage <= c.stage_id)
        {
            best  = h.years_before_epoch;
            found = true;
        }
    }
    return found ? best : 0;
}

/// One-line narration of a checkpoint decision for the log's `event` column.
std::string narrate_checkpoint(const checkpoint_record& c)
{
    std::string s = chain_stage_title(c.stage_id);
    s += " -- ";
    s += c.branch_taken;
    return s;
}

// ---------------------------------------------------------------------------
// Binary primitives — write side
// ---------------------------------------------------------------------------

void write_u8(std::ostream& out, uint8_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

void write_u32(std::ostream& out, uint32_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

void write_i64(std::ostream& out, int64_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

void write_str(std::ostream& out, const std::string& s)
{
    write_u32(out, static_cast<uint32_t>(s.size()));
    if (!s.empty())
        out.write(s.data(), static_cast<std::streamsize>(s.size()));
}

// ---------------------------------------------------------------------------
// Binary primitives — read side. Each returns false on a short/failed read;
// callers propagate that as a whole-stream rejection rather than partial data.
// ---------------------------------------------------------------------------

bool read_u8(std::istream& in, uint8_t& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

bool read_u32(std::istream& in, uint32_t& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

bool read_i64(std::istream& in, int64_t& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

bool read_str(std::istream& in, std::string& s)
{
    uint32_t len = 0;
    if (!read_u32(in, len))
        return false;
    if (len > history_log_max_field_bytes)
        return false; // corrupt/malicious length prefix — refuse rather than allocate
    s.resize(len);
    if (len > 0)
    {
        in.read(s.data(), static_cast<std::streamsize>(len));
        if (!in)
            return false;
    }
    return true;
}

} // namespace

void seed_genesis_history(world& w, const generation_report& report)
{
    for (const auto& body_entry : report.bodies)
    {
        entity_id body_id = null_entity;
        for (const auto& [id, bc] : w.bodies)
        {
            if (bc.name == body_entry.name)
            {
                body_id = id;
                break;
            }
        }
        if (body_id == null_entity)
            continue; // defensive: a generated body with no matching world entity

        const planetology_state& st = body_entry.state;

        std::vector<chapter_line> lines;
        lines.reserve(st.history.size() + st.checkpoints.size());

        for (const history_event& h : st.history)
            lines.push_back({ h.years_before_epoch, history_topic::genesis, h.event, h.consequence });

        for (const checkpoint_record& c : st.checkpoints)
            lines.push_back({ resolve_checkpoint_timestamp(st.history, c), history_topic::checkpoint,
                              narrate_checkpoint(c),
                              c.viability_result ? "viability floor cleared"
                                                 : "viability floor not cleared" });

        // Chronological, oldest first (descending timestamp — years_before_epoch
        // counts backwards), matching the ordering PLANETOLOGY's own history
        // vector already carries. stable_sort keeps a genesis/checkpoint tie at
        // the same timestamp in their original (genesis-before-checkpoint)
        // relative order.
        std::stable_sort(lines.begin(), lines.end(),
                         [](const chapter_line& a, const chapter_line& b) {
                             return a.timestamp > b.timestamp;
                         });

        for (chapter_line& l : lines)
        {
            world_history_entry e;
            e.timestamp   = l.timestamp;
            e.topic       = l.topic;
            e.body        = body_id;
            e.corp        = null_entity;
            e.event       = std::move(l.event);
            e.consequence = std::move(l.consequence);
            w.history_log.push_back(std::move(e));
        }
    }
}

void write_history_log(const world& w, std::ostream& out)
{
    write_u32(out, history_log_magic);
    write_u32(out, history_log_version);
    write_u32(out, static_cast<uint32_t>(w.history_log.size()));

    for (const world_history_entry& e : w.history_log)
    {
        write_i64(out, e.timestamp);
        write_u8(out, static_cast<uint8_t>(e.topic));
        write_u32(out, e.body);
        write_u32(out, e.corp);
        write_str(out, e.event);
        write_str(out, e.consequence);
    }
}

bool read_history_log(world& w, std::istream& in)
{
    uint32_t magic = 0;
    if (!read_u32(in, magic) || magic != history_log_magic)
        return false; // wrong magic — not a history-log stream at all

    uint32_t version = 0;
    if (!read_u32(in, version) || version != history_log_version)
        return false; // unsupported/mismatched version — refuse rather than misread

    uint32_t count = 0;
    if (!read_u32(in, count))
        return false;
    if (count > history_log_max_entries)
        return false; // corrupt/malicious entry count — refuse before the eager reserve below

    std::vector<world_history_entry> entries;
    entries.reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        int64_t  timestamp = 0;
        uint8_t  topic_id  = 0;
        uint32_t body      = 0;
        uint32_t corp      = 0;

        if (!read_i64(in, timestamp))
            return false;
        if (!read_u8(in, topic_id))
            return false;
        if (topic_id > static_cast<uint8_t>(history_topic::trade_route))
            return false; // unknown topic id — a corrupt or newer-format record
        if (!read_u32(in, body))
            return false;
        if (!read_u32(in, corp))
            return false;

        world_history_entry e;
        e.timestamp = timestamp;
        e.topic     = static_cast<history_topic>(topic_id);
        e.body      = body;
        e.corp      = corp;
        if (!read_str(in, e.event))
            return false;
        if (!read_str(in, e.consequence))
            return false;

        entries.push_back(std::move(e));
    }

    w.history_log = std::move(entries); // replace only on full success
    return true;
}
