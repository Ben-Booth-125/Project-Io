#include "decision_trace.hpp"

#include "corp_ai.hpp"       // corp_verb_label / corp_decision_reason_label
#include "corp_command.hpp"  // corp_decision / corp_command
#include "resource_names.hpp"

#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// FORMAT: JSONL — one self-contained JSON object per line.
// ---------------------------------------------------------------------------
//
// Chosen over CSV for three reasons, in the order they mattered:
//
// 1. IT MUST BE ABLE TO GROW INTO THE CORPUS WITHOUT BREAKING ITS READERS.
//    AI_OPPONENT.md § 10d names this artifact for a second life: "every
//    decision is logged as an input/output pair -- the blackboard state and
//    dictionary slice that went in, the command and its rationale that came
//    out", and notes the corpus and the replay log are the SAME artifact. That
//    input half is a NESTED, VARIABLE-SHAPED object (a blackboard slice), which
//    CSV cannot hold at all without escaping a serialised blob into a cell. In
//    JSONL it is one more key. Equally, adding a key here does not disturb any
//    existing reader, whereas a new CSV column breaks every positional one.
//    This item does NOT build the corpus, but the brief's constraint was to not
//    build something that could never become it, and column-oriented CSV is
//    exactly that.
//
// 2. STREAM-SAFETY. Every line is independently complete and independently
//    parseable, so a run killed mid-write, or a file read while the sim is
//    still going, still yields every complete line. A reader needs no header
//    and no state.
//
// 3. IT SATISFIES BOTH HALVES OF "HUMAN AND SCRIPT". One decision per line is
//    greppable by eye (`grep '"verb":"build"'`), and `jq` / `json.loads` parse
//    it with no custom code. CSV wins on raw skimmability; JSON's key names buy
//    that back by making each field self-describing, which matters more in a
//    file whose columns a reader has not memorised.
//
// SCHEMA. Line 1 is a meta object (`"kind":"meta"`) carrying the schema
// version -- still a valid JSON object, so "every line is an object" holds and
// a naive reader can skip it on `kind`. Every subsequent line is
// `"kind":"decision"` with these ALWAYS-PRESENT keys:
//
//   tick, corp, verb, reason, score, runner_up
//
// plus the command arguments that are NON-DEFAULT for that decision (defaults
// are corp_command.hpp's and are stable): subject, tile, building, resource,
// recipe, workforce, road_tier, unit_type, quantity, floor_price, order,
// counterparty, province. Omitting defaults keeps a line short enough to read
// without costing a script anything -- absent key means documented default,
// which is idiomatic JSON and is why the omission is safe here and would not
// have been in CSV.
//
// Floats print with %.9g: nine significant digits is what an IEEE-754 float
// needs to round-trip exactly, so the trace does not quietly lose precision a
// corpus consumer would want.

namespace
{

std::FILE*         g_file    = nullptr;
unsigned long long g_written = 0;

/// Headless label for `building_type`. `ui::building_type_name` is the
/// project's canonical one but lives in src/ui and would drag a UI dependency
/// into this SDL-free TU, so this mirrors it locally -- the same trade
/// `agency_kind_label` in corp_ai.cpp already makes.
///
/// The switch is deliberately EXHAUSTIVE WITH NO `default:` label, so adding a
/// `building_type` makes the compiler warn here rather than letting a new
/// building silently trace as the fallback string. The fallback below is for
/// an out-of-range cast only.
const char* building_label(building_type t)
{
    switch (t)
    {
        case building_type::none:                 return "none";
        case building_type::extraction_site:      return "extraction_site";
        case building_type::processing_facility:  return "processing_facility";
        case building_type::port:                 return "port";
        case building_type::launchpad:            return "launchpad";
        case building_type::inland_logistics_hub: return "inland_logistics_hub";
        case building_type::military_base:        return "military_base";
        case building_type::research_institute:   return "research_institute";
        case building_type::schooling:            return "schooling";
        case building_type::university:           return "university";
    }
    return "building#?";
}

/// Minimal JSON string escaping. Every string this file writes today is an
/// ASCII identifier from our own tables, so this never fires -- it exists so a
/// future field carrying freer text (a rationale string, per § 10d) cannot
/// produce a line that fails to parse.
void write_escaped(std::FILE* f, const char* s)
{
    for (const char* p = s; *p; ++p)
    {
        switch (*p)
        {
            case '"':  std::fputs("\\\"", f); break;
            case '\\': std::fputs("\\\\", f); break;
            case '\n': std::fputs("\\n", f);  break;
            case '\r': std::fputs("\\r", f);  break;
            case '\t': std::fputs("\\t", f);  break;
            default:
                if (static_cast<unsigned char>(*p) < 0x20)
                    std::fprintf(f, "\\u%04x", static_cast<unsigned>(*p));
                else
                    std::fputc(*p, f);
        }
    }
}

} // namespace

namespace decision_trace
{

bool open(const std::string& path)
{
    close();
    g_file = std::fopen(path.c_str(), "wb");
    if (!g_file)
        return false; // A trace that cannot be written is a lost observation,
                      // never a failed tick: leave g_enabled false and return.

    g_written = 0;
    std::fprintf(g_file,
                 "{\"kind\":\"meta\",\"schema\":\"io.decision_trace\",\"version\":1}\n");
    g_enabled = true;
    return true;
}

void close()
{
    g_enabled = false; // Cleared FIRST: after this the hot path cannot reach
                       // g_file, whatever happens below.
    if (g_file)
    {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

bool is_open() { return g_file != nullptr; }

unsigned long long written() { return g_written; }

void record_(const corp_decision& d)
{
    if (!g_file)
        return;

    const corp_command& c = d.command;

    std::fprintf(g_file, "{\"kind\":\"decision\",\"tick\":%d,\"corp\":%u,\"verb\":\"",
                 d.tick, static_cast<unsigned>(d.corp));
    write_escaped(g_file, corp_verb_label(c.verb));
    std::fputs("\",\"reason\":\"", g_file);
    write_escaped(g_file, corp_decision_reason_label(d.reason));
    std::fprintf(g_file, "\",\"score\":%.9g,\"runner_up\":%.9g",
                 static_cast<double>(d.winning_score),
                 static_cast<double>(d.runner_up));

    // --- non-default command arguments -------------------------------------
    if (c.subject != null_entity)
        std::fprintf(g_file, ",\"subject\":%u", static_cast<unsigned>(c.subject));
    if (c.tile != null_entity)
        std::fprintf(g_file, ",\"tile\":%u", static_cast<unsigned>(c.tile));
    if (c.type != building_type::none)
    {
        std::fputs(",\"building\":\"", g_file);
        write_escaped(g_file, building_label(c.type));
        std::fputc('"', g_file);
    }
    // `target` doubles as build's extracted good and place_sell_order's cargo,
    // and its default (iron_ore) is a REAL value rather than a sentinel, so it
    // is emitted only for the verbs that actually read it -- printing it on a
    // set_workforce row would assert a resource the command does not carry.
    if (c.verb == corp_verb::build || c.verb == corp_verb::place_sell_order ||
        c.verb == corp_verb::request_quote || c.verb == corp_verb::dispatch_convoy)
    {
        std::fputs(",\"resource\":\"", g_file);
        write_escaped(g_file, resource_names::name_of(c.target).c_str());
        std::fputc('"', g_file);
    }
    if (c.recipe != no_recipe)
        std::fprintf(g_file, ",\"recipe\":%u", static_cast<unsigned>(c.recipe));
    if (c.verb == corp_verb::set_workforce)
        std::fprintf(g_file, ",\"workforce\":%d", c.workforce);
    if (c.verb == corp_verb::place_road)
        std::fprintf(g_file, ",\"road_tier\":%u", static_cast<unsigned>(c.road_tier));
    if (c.verb == corp_verb::hire_unit)
        std::fprintf(g_file, ",\"unit_type\":%u", static_cast<unsigned>(c.unit_type));
    if (c.quantity != 0.0f)
        std::fprintf(g_file, ",\"quantity\":%.9g", static_cast<double>(c.quantity));
    if (c.floor_price != 0.0f)
        std::fprintf(g_file, ",\"floor_price\":%.9g", static_cast<double>(c.floor_price));
    if (c.order != 0)
        std::fprintf(g_file, ",\"order\":%u", c.order);
    if (c.counterparty != null_entity)
        std::fprintf(g_file, ",\"counterparty\":%u", static_cast<unsigned>(c.counterparty));

    std::fputs("}\n", g_file);
    ++g_written;
}

namespace
{

/// Environment opt-in, evaluated ONCE at static-init time so the hot path never
/// pays for a getenv. Set `IO_DECISION_TRACE=<path>` to trace a normal run --
/// the app, a harness, anything that links this TU -- with no code change and
/// no CLI plumbing. Unset (the default) leaves the sink closed and `g_enabled`
/// false, which is the "costs nothing when off" state.
///
/// This initialiser touches only this file's own statics and getenv, so it is
/// free of the static-initialisation-order hazard.
[[maybe_unused]] const bool g_env_opt_in = []
{
    const char* p = std::getenv("IO_DECISION_TRACE");
    return (p && *p) ? open(p) : false;
}();

} // namespace

} // namespace decision_trace
