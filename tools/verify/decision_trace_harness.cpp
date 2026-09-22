// Headless harness (BL-704): the per-rival decision trace is complete, and it
// is an observation rather than an input.
//
// The trace (src/world/decision_trace.*) is an opt-in JSONL sink appended to as
// each `corp_decision` is recorded. It exists because `corp_decision_ring` holds
// 256 entries and WRAPS: a run long enough to be interesting turns the ring over
// several times, so reading it at the end yields the last 256 decisions and
// silently drops the rest. This harness holds the two claims that make the sink
// trustworthy:
//
//   T1 — THE LOAD-BEARING ROW. Tracing changes NOTHING. Two independently
//        generated worlds, same seed, same tick count, one rolled with the sink
//        open and one with it closed, must end on an identical
//        `world::state_hash`. If this goes red the trace has become an input to
//        the simulation, and whatever made it one is wrong. Deliberately checked
//        against a SECOND, INDEPENDENTLY CONSTRUCTED world rather than by
//        re-hashing one world twice, which would prove only that the hash is a
//        function.
//
//   T2 — THE TRACE IS COMPLETE, i.e. it does not lie by omission. The written
//        line count must equal `corp_decision_ring::total` (the lifetime push
//        counter, which does not wrap) — every decision the sim recorded reached
//        the file. Paired with a guard that the run actually OVERFLOWED the ring,
//        because a run that never wrapped could not distinguish this sink from
//        the end-of-run dump it replaces.
//
//   T3 — WHAT IS WRITTEN PARSES AND IS SANE. Every line is a JSON object; the
//        meta line comes first; every decision line carries the six always-present
//        keys; ticks are non-decreasing (insertion order is application order);
//        and no score field is NaN/inf textually.
//
//   T4 — OFF BY DEFAULT AND FREE. `g_enabled` is false before any `open()`, and
//        a `record()` against a closed sink writes nothing and counts nothing.
//
// VACUITY. A green result about an economy that is not there is this tier's
// least visible failure mode, so two guards run before any conclusion:
//   1. NOTHING IS LOADED BY PATH — the registry is hand-built below, mirroring
//      scripts/economy.lua exactly as spectator_determinism and ai_skill_harness
//      do, so no relative-path script load can silently no-op from the wrong
//      working directory and leave a registry with no recipes.
//   2. AN EXPLICIT ANTI-VACUITY ASSERTION — the traced rollout must record
//      decisions across several verb families. If the economy were hollow, no
//      corp would act, T2 would hold trivially at 0 == 0, and this guard fails
//      instead.
//
// Hand-builds a recipe_registry (Lua-free, per verifier-headless convention);
// links the world TU superset via CMake's IO_VERIFY_HARNESSES glob, so no
// build-file entry is needed.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/decision_trace.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{

int  g_failures = 0;
void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

// ---------------------------------------------------------------------------
// Rollout parameters
// ---------------------------------------------------------------------------
// 6000 ticks, and the length is MEASURED rather than picked. T2 needs the run to
// push more than `corp_decision_ring::capacity` (256) decisions, because a run
// that never wraps cannot tell this streaming sink apart from the end-of-run
// ring dump it replaces — the completeness claim would pass vacuously.
//
// IT WAS 2400, CHOSEN AGAINST A RATE THAT HAS SINCE MOVED, and the way it broke
// is the lesson. The original table (seed 0, the hand-built registry below) read:
//
//     ticks  300   600   900  1200  1800  2400
//     pushed 119   140   162   183   226   269
//
// 269 against a capacity of 256 is FIVE PERCENT of headroom, and the decision
// rate is a property of the SCORER, not of this file. BL-417 replaced the build
// score's `net^2 / capex` with `net / capex`, which cut the churn — 2400 ticks
// now pushes 207 — and T2's anti-vacuity guard went red. Correctly: the run had
// genuinely stopped wrapping, and the guard is the only reason that was visible
// rather than a completeness claim quietly passing over nothing.
//
// Re-measured 2026-09-01 under the linear score:
//
//     ticks  2400  3600  4800  6000
//     pushed  207   267   327   387
//
// A clean ~5 per 100 after the front-load. 6000 is chosen for 51% headroom over
// the 256 wrap rather than the 5% that made a single scorer change break this —
// the rate will move again the next time the scorer does, and the guard should
// survive that without a re-tune. `--ticks` re-measures the table.
//
// The rate is far slower than a back-of-envelope "8 corps x 7 commands per
// evaluation" suggests: corps evaluate on a staggered cadence, most candidates
// are rejected, and the early land-grab is what produces the initial burst.
//
// This is also why the STREAMING design still earns its place at this rate: the
// overflow is a certainty over a campaign rather than over a test, and the sink
// costs the same either way, so it removes the failure mode instead of
// deferring it. A caveat worth keeping in view — this fixture's registry is
// deliberately thin (three recipes, seven corps); the shipped game loads the
// full scripts/recipes.lua + economy.lua and will not have this rate.
constexpr uint32_t k_seed  = 0;
int                k_ticks = 6000;   ///< --ticks overrides it, for re-measuring the rate

// ---------------------------------------------------------------------------
// Hand-built registry — mirrors scripts/economy.lua (see VACUITY note above)
// ---------------------------------------------------------------------------
recipe_registry make_registry()
{
    recipe_registry reg;

    building_economics extraction;
    extraction.base_rate            = 20.0f; // BL-436 calibration: mirrors economy.lua
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    extraction.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 20.0f;
    extraction.richness_reference = 0.0f; // mirrors economy.lua: DISABLED pending BL-436 calibration
    extraction.richness_min       = 0.25f;
    extraction.richness_max       = 2.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    building_economics processing;
    processing.base_rate            = 8.0f;  // BL-436 calibration: mirrors economy.lua
    processing.maintenance          = 10.0f;
    processing.base_wage            = 12.0f;
    processing.build_cost           = 200.0f;
    processing.build_duration_ticks = 3.0f;
    processing.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 25.0f;
    reg.set_economics(building_type::processing_facility, processing);

    building_economics port;
    port.maintenance          = 8.0f;
    port.base_wage            = 6.0f;
    port.build_cost           = 150.0f;
    port.build_duration_ticks = 2.0f;
    port.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 20.0f;
    reg.set_economics(building_type::port, port);

    building_economics launchpad;
    launchpad.maintenance          = 20.0f;
    launchpad.base_wage            = 15.0f;
    launchpad.build_cost           = 500.0f;
    launchpad.build_duration_ticks = 6.0f;
    launchpad.resource_build_cost[static_cast<std::size_t>(resource_type::steel)]        = 50.0f;
    launchpad.resource_build_cost[static_cast<std::size_t>(resource_type::refined_fuel)] = 20.0f;
    reg.set_economics(building_type::launchpad, launchpad);

    building_economics hub;
    hub.maintenance          = 12.0f;
    hub.base_wage            = 8.0f;
    hub.build_cost           = 250.0f;
    hub.build_duration_ticks = 3.0f;
    hub.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 30.0f;
    reg.set_economics(building_type::inland_logistics_hub, hub);

    building_economics base;
    base.maintenance          = 15.0f;
    base.base_wage            = 10.0f;
    base.build_cost           = 300.0f;
    base.build_duration_ticks = 4.0f;
    base.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 35.0f;
    reg.set_economics(building_type::military_base, base);

    recipe steel;
    steel.name = "steel";
    steel.inputs[static_cast<std::size_t>(resource_type::iron_ore)] = 2.0f;
    steel.outputs[static_cast<std::size_t>(resource_type::steel)]   = 1.0f;
    reg.add_recipe(steel);

    recipe fuel;
    fuel.name = "refined_fuel";
    fuel.inputs[static_cast<std::size_t>(resource_type::petroleum)]     = 2.0f;
    fuel.outputs[static_cast<std::size_t>(resource_type::refined_fuel)] = 1.0f;
    reg.add_recipe(fuel);

    recipe food;
    food.name = "food_rations";
    food.inputs[static_cast<std::size_t>(resource_type::agricultural_produce)] = 2.0f;
    food.outputs[static_cast<std::size_t>(resource_type::food_rations)]        = 1.0f;
    reg.add_recipe(food);

    return reg;
}

void run_tick(world& w, const recipe_registry& reg, int tick)
{
    w.current_day_tick  = tick;
    w.current_econ_tick = tick;
    const economy_report rep = run_economy_step(w, reg, /*spectating=*/false);
    const auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, nullptr);
    advance_tech_gates(w);
}

struct rollout
{
    uint64_t           final_hash = 0;
    unsigned long long ring_total = 0; ///< Lifetime decisions pushed (does not wrap).
};

/// Build a FRESH world and roll it forward `ticks` ticks. Each call constructs
/// its own world, so the T1 comparison is between two independently generated
/// worlds rather than one world hashed twice.
rollout run_rollout(uint32_t seed, int ticks)
{
    world_params params;
    params.seed = seed;
    world w = make_hard_coded_world(no_prehistory(params));
    const recipe_registry reg = make_registry();

    for (int t = 1; t <= ticks; ++t)
        run_tick(w, reg, t);

    rollout m;
    m.final_hash = w.state_hash(ticks);
    m.ring_total = static_cast<unsigned long long>(w.ai_decisions.total);
    return m;
}

// ---------------------------------------------------------------------------
// A deliberately tiny JSONL reader
// ---------------------------------------------------------------------------
// Enough to answer T3's questions (is the key there, what is its value) without
// dragging a JSON dependency into the verify tier. Values are read as raw text
// between the delimiters, which is all the assertions below compare.
struct line_fields
{
    std::map<std::string, std::string> kv;
    bool                               parsed = false;
};

line_fields parse_line(const std::string& s)
{
    line_fields out;
    if (s.size() < 2 || s.front() != '{' || s.back() != '}')
        return out;

    std::size_t i = 1;
    while (i < s.size() - 1)
    {
        const std::size_t ks = s.find('"', i);
        if (ks == std::string::npos) break;
        const std::size_t ke = s.find('"', ks + 1);
        if (ke == std::string::npos) return out; // unterminated key: malformed
        const std::string key = s.substr(ks + 1, ke - ks - 1);

        const std::size_t colon = s.find(':', ke);
        if (colon == std::string::npos) return out;

        std::size_t vs = colon + 1;
        std::string value;
        if (vs < s.size() && s[vs] == '"')
        {
            const std::size_t ve = s.find('"', vs + 1);
            if (ve == std::string::npos) return out;
            value = s.substr(vs + 1, ve - vs - 1);
            i     = ve + 1;
        }
        else
        {
            std::size_t ve = vs;
            while (ve < s.size() && s[ve] != ',' && s[ve] != '}') ++ve;
            value = s.substr(vs, ve - vs);
            i     = ve;
        }
        out.kv[key] = value;
        if (i < s.size() && s[i] == ',') ++i;
    }
    out.parsed = true;
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    // --ticks: the rate this harness sizes against is a property of the SCORER,
    // so it moves when the scorer does (BL-417 dropped it 269 -> 207). The flag
    // is how the table in the header above gets re-measured instead of guessed.
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc)
            k_ticks = std::max(1, std::atoi(argv[++i]));
    std::printf("decision_trace_harness (BL-704) — seed %u, %d ticks\n\n", k_seed, k_ticks);

    // -----------------------------------------------------------------------
    // T4 — off by default (asserted FIRST, before anything opens a sink)
    // -----------------------------------------------------------------------
    std::printf("T4 — off by default, and free when off\n");
    // The sink also opts in from IO_DECISION_TRACE (decision_trace.cpp), which
    // is a DELIBERATE way to fail T4: "off by default" is false when the
    // environment asked for it. Say so, so the three rows below read as the
    // environment being set rather than as the default having regressed.
    if (const char* env = std::getenv("IO_DECISION_TRACE"))
        std::printf("     NOTE: IO_DECISION_TRACE=%s is set, so the sink opted in at\n"
                    "     start-up and T4's three rows below are EXPECTED to fail. Unset it\n"
                    "     to test the default. (T1/T2/T3 remain meaningful either way.)\n", env);
    check(!decision_trace::g_enabled, "g_enabled is false before any open()");
    check(!decision_trace::is_open(), "no sink is open before any open()");
    {
        // A record() against a closed sink must be a no-op, not a crash and not
        // a write. This is the exact call the hot path makes every decision.
        corp_decision d;
        d.tick = 1;
        decision_trace::record(d);
        check(decision_trace::written() == 0, "record() on a closed sink writes nothing");
    }
    std::printf("\n");

    // -----------------------------------------------------------------------
    // The two rollouts
    // -----------------------------------------------------------------------
    const std::string trace_path = "decision_trace_harness.jsonl";
    std::remove(trace_path.c_str());

    // Untraced FIRST, so the traced run cannot be said to have "warmed" anything.
    const rollout untraced = run_rollout(k_seed, k_ticks);

    const bool opened = decision_trace::open(trace_path);
    const rollout traced = run_rollout(k_seed, k_ticks);
    const unsigned long long written = decision_trace::written();
    decision_trace::close();

    std::printf("  untraced: hash %016llX, ring total %llu\n",
                static_cast<unsigned long long>(untraced.final_hash), untraced.ring_total);
    std::printf("  traced:   hash %016llX, ring total %llu, lines written %llu\n\n",
                static_cast<unsigned long long>(traced.final_hash), traced.ring_total, written);

    // -----------------------------------------------------------------------
    // Anti-vacuity — the economy actually ran
    // -----------------------------------------------------------------------
    std::printf("Anti-vacuity guards\n");
    check(opened, "the trace sink opened");
    check(traced.ring_total > 0, "the rollout recorded at least one decision");
    std::printf("\n");

    // -----------------------------------------------------------------------
    // T1 — THE LOAD-BEARING ROW: tracing is an observation, never an input
    // -----------------------------------------------------------------------
    std::printf("T1 — tracing does not change the simulation\n");
    check(traced.final_hash == untraced.final_hash,
          "state_hash is identical with tracing ON and OFF");
    check(traced.ring_total == untraced.ring_total,
          "the same number of decisions is recorded either way");
    std::printf("\n");

    // -----------------------------------------------------------------------
    // T2 — completeness: the trace does not lie by omission
    // -----------------------------------------------------------------------
    std::printf("T2 — the trace is complete (survives the ring's wrap)\n");
    check(traced.ring_total > corp_decision_ring::capacity,
          "the run OVERFLOWED the 256-entry ring (so the claim is not vacuous)");
    check(written == traced.ring_total,
          "written lines == corp_decision_ring::total (nothing lost to the wrap)");
    std::printf("     ring capacity %zu; the run pushed %llu, so an end-of-run dump\n"
                "     would have reported only the last %zu (%.0f%% lost).\n",
                corp_decision_ring::capacity, traced.ring_total,
                corp_decision_ring::capacity,
                traced.ring_total ? 100.0 * (1.0 - static_cast<double>(corp_decision_ring::capacity) /
                                                       static_cast<double>(traced.ring_total))
                                  : 0.0);
    std::printf("\n");

    // -----------------------------------------------------------------------
    // T3 — what was written parses, and is sane
    // -----------------------------------------------------------------------
    std::printf("T3 — the written trace parses and is well-formed\n");
    std::ifstream in(trace_path);
    check(in.good(), "the trace file is readable");

    std::vector<std::string> lines;
    for (std::string s; std::getline(in, s);)
    {
        if (!s.empty() && s.back() == '\r') s.pop_back(); // tolerate CRLF
        if (!s.empty()) lines.push_back(s);
    }

    check(!lines.empty(), "the trace file is non-empty");
    check(lines.size() == written + 1, "line count == decisions + the meta line");

    bool meta_ok = false;
    if (!lines.empty())
    {
        const line_fields m = parse_line(lines[0]);
        meta_ok = m.parsed && m.kv.count("kind") && m.kv.at("kind") == "meta" &&
                  m.kv.count("schema") && m.kv.count("version");
    }
    check(meta_ok, "line 1 is the meta object (kind/schema/version)");

    bool        all_parse   = true;
    bool        all_keys    = true;
    bool        ticks_sane  = true;
    bool        scores_sane = true;
    int         last_tick   = 0;
    std::set<std::string> verbs;
    std::set<std::string> reasons;
    std::set<std::string> corps;

    for (std::size_t i = 1; i < lines.size(); ++i)
    {
        const line_fields f = parse_line(lines[i]);
        if (!f.parsed) { all_parse = false; continue; }

        for (const char* k : {"kind", "tick", "corp", "verb", "reason", "score", "runner_up"})
            if (!f.kv.count(k)) all_keys = false;

        if (!f.kv.count("tick")) { ticks_sane = false; continue; }
        const int t = std::atoi(f.kv.at("tick").c_str());
        if (t < last_tick) ticks_sane = false; // insertion order IS application order
        last_tick = t;

        for (const char* k : {"score", "runner_up"})
        {
            if (!f.kv.count(k)) { scores_sane = false; continue; }
            const std::string& v = f.kv.at(k);
            if (v.find("nan") != std::string::npos || v.find("inf") != std::string::npos ||
                v.find("NAN") != std::string::npos || v.find("INF") != std::string::npos)
                scores_sane = false;
        }

        if (f.kv.count("verb"))   verbs.insert(f.kv.at("verb"));
        if (f.kv.count("reason")) reasons.insert(f.kv.at("reason"));
        if (f.kv.count("corp"))   corps.insert(f.kv.at("corp"));
    }

    check(all_parse, "every decision line is a parseable JSON object");
    check(all_keys, "every decision line carries the six always-present keys");
    check(ticks_sane, "ticks are non-decreasing (insertion order == application order)");
    check(scores_sane, "no score or runner_up is NaN/inf");
    std::printf("     distinct corps %zu, verbs %zu, reasons %zu\n",
                corps.size(), verbs.size(), reasons.size());

    // The anti-vacuity assertion proper: a hollow economy would produce a file
    // that passes every structural row above while describing nothing.
    check(corps.size() >= 2, "the trace covers at least two distinct corps (per-RIVAL, not one)");
    check(verbs.size() >= 2, "the trace covers at least two distinct verb families");
    std::printf("\n");

    // A short excerpt, so a reader of this harness's output can see the shape.
    std::printf("Excerpt (first 5 decision lines)\n");
    for (std::size_t i = 1; i < lines.size() && i <= 5; ++i)
        std::printf("  %s\n", lines[i].c_str());
    std::printf("\n");

    std::printf("%s — %d failure(s)\n", g_failures ? "FAILED" : "OK", g_failures);
    return g_failures ? 1 : 0;
}
