// ---------------------------------------------------------------------------
// landscape_search — BL-770 slice 3. The phase 6 SEARCH, and the one property
// that is allowed to fail it.
//
// Slice 1 asked whether the objective could see a roster (it could not), slice 2
// gave it the term that can, and this slice builds the search that walks on it.
// The question here is NOT "is the winner good" — the objective is
// viable-but-uneven and slice 1's own caveat says these scores are ORDINAL — it
// is "is the winner the SAME winner, always".
//
// THE BINDING ASSERTION IS R2: same seed, different thread counts, identical
// winner, bit-identical on every scored term. Everything else in this harness
// exists to stop R2 passing vacuously — a search that never moved off its seed
// candidate would be trivially thread-invariant and would prove nothing, so R1
// requires the walk to actually go somewhere before R2's result is read.
//
// Build:  bash tools/verify/build_lua_harness.sh landscape_search_harness
//
// --curve (BL-1136, fewer search evaluations) — THE CONVERGENCE CURVE, a
// reading rather than the R-block above. See `curve::run` below.
//   landscape_search_harness.exe --curve [--seeds 0,28,...] [--rounds 6]
//                                        [--check-k 3] [--check-threads 1] [--print-check]
//   Default seeds: docs/generation/seed_library.json, in library order.
//   --check-threads N scores P1's genuine walk on N threads: P1 is then also
//   thread invariance on the shipped budget world, and its wall time is what
//   that thread count saves.
//   THE CAMPAIGN'S PROOF IS `--curve --check-k 6 --check-threads 2`: the whole
//   six-round walk on the campaign's two threads. --check-k defaults to 3,
//   which compares only the first half of the walk. P1's two negative controls
//   (one proposal's composite +1 ulp, one proposal's placement seed ^1) run
//   wherever P1 does and must each be reported.
//   Run from the repo root (it loads scripts/*.lua and the library by path).
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/corporation_generation.hpp"
#include "world/hard_coded_world.hpp"
#include "world/landscape_search.hpp"
#include "world/market_saturation.hpp"
#include "world/recipe_registry.hpp"
#include "world/stockpile_budget.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>   // std::nextafter (P1's negative control)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{

int failures = 0;

void check(bool ok, const std::string& id, const std::string& what)
{
    std::printf("  [%s] %s  %s\n", ok ? "PASS" : "FAIL", id.c_str(), what.c_str());
    if (!ok)
        ++failures;
}

bool same_candidate(const landscape_candidate& a, const landscape_candidate& b)
{
    return a.corporation_count == b.corporation_count
        && a.placement_seed    == b.placement_seed
        && a.road_tier         == b.road_tier;
}

/// BIT-IDENTICAL, not "close". A tolerance here would hide exactly the drift the
/// assertion exists to catch — a reduction that summed in completion order gives
/// answers that agree to twelve digits and are still non-deterministic.
bool same_score(const landscape_score& a, const landscape_score& b)
{
    return a.composite         == b.composite
        && a.realisation       == b.realisation
        && a.mean_actual       == b.mean_actual
        && a.mean_completeness == b.mean_completeness
        && a.mean_balance      == b.mean_balance
        && a.spread            == b.spread
        && a.market_count      == b.market_count;
}

std::string describe(const landscape_candidate& c)
{
    char buf[96];
    std::snprintf(buf, sizeof buf, "corps=%d placement=%08X road_tier=%u",
                  c.corporation_count, c.placement_seed,
                  static_cast<unsigned>(c.road_tier));
    return buf;
}

void print_result(const char* label, const landscape_search_result& r, double ms)
{
    std::printf("  %-14s  seed composite=%.6f -> WINNER composite=%.6f   (%s)\n",
                label, r.seed_score.composite, r.winner_score.composite,
                describe(r.winner).c_str());
    std::printf("  %-14s  realised %.3f -> %.3f   potential %.5f -> %.5f   "
                "spread %.5f -> %.5f\n", "",
                r.seed_score.realisation, r.winner_score.realisation,
                r.seed_score.mean_completeness, r.winner_score.mean_completeness,
                r.seed_score.spread, r.winner_score.spread);
    std::printf("  %-14s  %d evaluations, %d accepted, %zu path steps, %.0f ms\n",
                "", r.evaluations, r.accepted, r.path.size(), ms);
}

void print_path(const landscape_search_result& r)
{
    std::printf("\n  THE PATH (what each round proposed and what it bought):\n");
    for (const landscape_search_step& s : r.path)
    {
        std::printf("    r%-2d %-10s %-40s composite=%.6f %s\n",
                    s.round, landscape_axis_name(s.axis),
                    describe(s.proposal).c_str(), s.score.composite,
                    s.accepted ? "<- TAKEN" : "");
    }
}

} // namespace

// ---------------------------------------------------------------------------
// --curve — BL-1136 (fewer search evaluations): THE CONVERGENCE CURVE.
//
// GENERATION_STRATEGY.md § Phase 6, "The round count is cut to fit round 6's
// wait": the count stays fixed and stays one number for every world, but it is
// chosen from a measured curve — how much score each further round buys on the
// curated seeds. This is that measurement. A READING, NOT A GATE: nothing here
// judges a composite; the exit code fails only on the instrument's own honesty
// rows (P1 below, and V1-V5).
//
// THE WORLD is the campaign's: `build_app_base_world` on the shipped arc (the
// app's generation inputs, its genesis bridge and survey states, its banded
// registry and first recipe pass), then the search params, charter budget and
// spend exactly as `finish_campaign_world` builds them. The search is the
// library's own `search_landscape`; nothing is restated.
//
// ONE WALK GIVES THE WHOLE CURVE, and P1 is the proof that it does. A round's
// proposals are drawn from a stream keyed by (round, axis) off the INCUMBENT,
// and nothing in a round reads the round count — so a k-round search is
// exactly the first k rounds of the R-round walk, stopped. The curve is read
// off the R-round path's prefixes; P1 then runs a GENUINE k-round search
// (k = --check-k) on every seed and requires it to equal the prefix
// bit-identically: winner, every scored term, evaluations, and every path step.
//
// PER ROUND COUNT k = 0..R, on each seed:
//   * the winner and its composite, the gain over the seed candidate, and the
//     SHARE of the R-round gain k rounds capture ((c_k - c_0) / (c_R - c_0));
//   * the axis whose proposal was taken in round k, if any ("moved by");
//   * V1-V5, market_census's search-validation rows on the k-round result:
//       V1 evaluations == 1 + k x live axes;  V2 the path holds every proposal;
//       V3 the winner never scores below the seed;  V4 every accepted step
//       strictly beat its incumbent and the winner IS the last one;  V5 the
//       winner, RE-LAID on a copy of the base with the same budget and spend
//       and re-scored with the search's own params, is bit-identical on every
//       scored term. On a prefix V1 and V2 are structural (P1 proves the prefix
//       is the genuine k-round result); V3-V5 are real on every row.
//   * whether the k-round winner IS the R-round winner — a seed where it is
//     not is a WORLD-MOVER at that count.
//
// MS PER EVALUATION, MEASURED TWICE (the machine is shared, so a count is the
// robust figure and a time is indicative): the R-round walk's wall time over
// its evaluations, and P1's genuine k-round walk's. V5's re-lays are timed too,
// split into the world copy, the candidate's apply and the score — the three
// things one evaluation is. All wall time is a printed diagnostic; nothing
// reads it back.
// ---------------------------------------------------------------------------
#ifdef _WIN32
// Declared rather than <windows.h>, whose macros would reach every name below.
// FILETIME is two little-endian DWORDs, i.e. one 64-bit count of 100 ns.
extern "C" __declspec(dllimport) int   __stdcall GetThreadTimes(void*, void*, void*, void*, void*);
extern "C" __declspec(dllimport) void* __stdcall GetCurrentThread();
// PROCESS_MEMORY_COUNTERS_EX, restated field for field (psapi.h), and the
// kernel32 export psapi's GetProcessMemoryInfo forwards to.
struct curve_pmc_ex
{
    std::uint32_t cb;
    std::uint32_t PageFaultCount;
    std::size_t   PeakWorkingSetSize;
    std::size_t   WorkingSetSize;
    std::size_t   QuotaPeakPagedPoolUsage;
    std::size_t   QuotaPagedPoolUsage;
    std::size_t   QuotaPeakNonPagedPoolUsage;
    std::size_t   QuotaNonPagedPoolUsage;
    std::size_t   PagefileUsage;
    std::size_t   PeakPagefileUsage;
    std::size_t   PrivateUsage;
};
extern "C" __declspec(dllimport) int   __stdcall K32GetProcessMemoryInfo(void*, void*, std::uint32_t);
extern "C" __declspec(dllimport) void* __stdcall GetCurrentProcess();
#else
#include <time.h>
#endif

#include <atomic>
#include <thread>

namespace curve
{

using clk = std::chrono::steady_clock;

double ms_since(clk::time_point t0)
{
    return std::chrono::duration<double, std::milli>(clk::now() - t0).count();
}

/// This thread's CPU time (user + kernel), ms. The load-robust twin of the wall
/// clock: the reference walk runs serially on the calling thread (thread_count
/// 1; the campaign's is 2), so its CPU time is what one evaluation costs with the machine
/// to itself, whatever else is running. ~15.6 ms resolution on Windows, against
/// evaluations of about a second.
double thread_cpu_ms()
{
#ifdef _WIN32
    std::uint64_t created = 0, exited = 0, kernel = 0, user = 0;
    GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user);
    return static_cast<double>(kernel + user) / 10000.0;
#else
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) * 1e3 + static_cast<double>(ts.tv_nsec) / 1e6;
#endif
}

/// The process's working set and private bytes now, and its lifetime peak
/// working set, MB. Zero off Windows (not measured there).
struct mem_now
{
    double ws_mb = 0, private_mb = 0, peak_ws_mb = 0;
};

mem_now read_mem()
{
    mem_now m;
#ifdef _WIN32
    curve_pmc_ex c{};
    c.cb = sizeof c;
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &c, sizeof c) != 0)
    {
        constexpr double mb = 1024.0 * 1024.0;
        m.ws_mb      = static_cast<double>(c.WorkingSetSize) / mb;
        m.private_mb = static_cast<double>(c.PrivateUsage) / mb;
        m.peak_ws_mb = static_cast<double>(c.PeakWorkingSetSize) / mb;
    }
#endif
    return m;
}

/// The highest working set and private bytes seen while a call runs, sampled
/// every 5 ms from a second thread. The lifetime peak cannot say this: the
/// generation that built the base peaks first. A READ of OS counters only —
/// nothing in the world or the search can see it.
class mem_sampler
{
public:
    mem_sampler() : m_thread([this] {
        while (!m_stop.load(std::memory_order_relaxed))
        {
            const mem_now n = read_mem();
            m_ws      = std::max(m_ws, n.ws_mb);
            m_private = std::max(m_private, n.private_mb);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }) {}
    ~mem_sampler() { stop(); }
    void stop()
    {
        if (m_thread.joinable())
        {
            m_stop.store(true, std::memory_order_relaxed);
            m_thread.join();
        }
    }
    double peak_ws_mb() const      { return m_ws; }
    double peak_private_mb() const { return m_private; }

private:
    std::atomic<bool> m_stop{ false };
    double m_ws = 0, m_private = 0;
    std::thread m_thread;
};

/// The library's seeds in library order — market_census's minimal scan, so the
/// two instruments read one list.
std::vector<std::uint32_t> library_seeds(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<std::uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\"";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        pos += key.size();
        std::size_t p = pos;
        while (p < text.size() && (text[p] == ' ' || text[p] == ':' || text[p] == '\t')) ++p;
        if (p < text.size() && text[p] >= '0' && text[p] <= '9')
            out.push_back(static_cast<std::uint32_t>(std::strtoul(text.c_str() + p, nullptr, 10)));
    }
    return out;
}

std::vector<std::uint32_t> parse_seed_list(const std::string& s)
{
    std::vector<std::uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty())
            out.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

/// market_census's V5 comparison: every scored term bit-identical, and the
/// per-market record the same length. Wider than the R-block's `same_score`,
/// which predates the reach term.
bool same_full_score(const landscape_score& a, const landscape_score& b)
{
    return a.composite           == b.composite
        && a.realisation         == b.realisation
        && a.mean_actual         == b.mean_actual
        && a.mean_completeness   == b.mean_completeness
        && a.mean_balance        == b.mean_balance
        && a.mean_reach          == b.mean_reach
        && a.completeness_spread == b.completeness_spread
        && a.balance_spread      == b.balance_spread
        && a.reach_spread        == b.reach_spread
        && a.spread              == b.spread
        && a.market_count        == b.market_count
        && a.markets.size()      == b.markets.size();
}

bool same_path(const std::vector<landscape_search_step>& a,
               const std::vector<landscape_search_step>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].round != b[i].round || a[i].axis != b[i].axis
            || !same_candidate(a[i].proposal, b[i].proposal)
            || !same_full_score(a[i].score, b[i].score)
            || a[i].accepted != b[i].accepted)
            return false;
    return true;
}

/// P1's comparison: a search result equals the walk's k-round prefix — the
/// winner, every scored term of the winner and the seed, the counts, and every
/// path step. One function, so the negative controls below exercise exactly the
/// comparison P1 makes.
bool same_result(const landscape_search_result& a, const landscape_search_result& b)
{
    return same_candidate(a.winner, b.winner)
        && same_full_score(a.winner_score, b.winner_score)
        && same_full_score(a.seed_score, b.seed_score)
        && a.evaluations == b.evaluations
        && a.accepted    == b.accepted
        && same_path(a.path, b.path);
}

/// The k-round result, read off the R-round walk: the seed, then every path
/// step of rounds 0..k-1, the incumbent replaced exactly where the walk
/// replaced it. P1 checks this against a genuine k-round search.
landscape_search_result prefix_of(const landscape_search_result& full, int k)
{
    landscape_search_result r;
    r.seed_candidate = full.seed_candidate;
    r.seed_score     = full.seed_score;
    r.winner         = full.seed_candidate;
    r.winner_score   = full.seed_score;
    r.charter_refused   = full.charter_refused;
    r.charter_refusal   = full.charter_refusal;
    r.charter_fell_back = full.charter_fell_back;
    for (const landscape_search_step& s : full.path)
    {
        if (s.round >= k) break;
        r.path.push_back(s);
        if (s.accepted)
        {
            r.winner       = s.proposal;
            r.winner_score = s.score;
            ++r.accepted;
            ++r.accepted_by_axis[static_cast<int>(s.axis)];
        }
    }
    r.evaluations = 1 + static_cast<int>(r.path.size()); // the walk's own count
    return r;
}

struct row
{
    int  k = 0;
    int  evaluations = 0;
    landscape_candidate winner{};
    landscape_score     score{};
    int  moved_axis = -1;   ///< the axis taken IN round k (k >= 1); -1 = held
    int  accepted = 0;
    bool v1 = false, v2 = false, v3 = false, v4 = false, v5 = false;
    bool is_final = false;  ///< the k-round winner IS the R-round winner
    bool all() const { return v1 && v2 && v3 && v4 && v5; }
};

struct seed_curve
{
    std::uint32_t seed = 0;
    bool   budget_world = false;
    bool   refused = false, fell_back = false;
    int    live_axes = 0;
    int    markets = 0;
    std::vector<row> rows;              ///< k = 0..R
    double ms_eval_walk = 0;            ///< measurement 1: the R-round walk / its evaluations
    double ms_eval_check = 0;           ///< measurement 2: P1's genuine k-round walk
    double cpu_eval_walk = 0;           ///< the same two, in this thread's CPU time
    double cpu_eval_check = 0;
    int    check_k = -1;
    int    check_threads = 1;
    double ms_check_s = 0;              ///< P1's genuine walk, wall seconds
    double ms_walk_first_k_s = 0;       ///< the serial walk's seed + first k rounds, wall seconds
    bool   p1 = false;
    bool   nc_ran = false, nc_score = false, nc_candidate = false; ///< P1's negative controls
    double relay_copy = 0, relay_apply = 0, relay_score = 0; ///< V5's split, mean ms
    int    relays = 0;
    double t_gen_s = 0;
    // Memory, MB (Windows only; 0 elsewhere). Sampled every 5 ms during each walk.
    double mem_base_ws = 0, mem_base_private = 0;    ///< before the walk: the base world held
    double mem_walk_ws = 0, mem_walk_private = 0;    ///< peak during the serial walk
    double mem_check_ws = 0, mem_check_private = 0;  ///< peak during P1's walk (--check-threads)
    double mem_copy_private = 0;                     ///< one `world` copy of the base, private bytes,
                                                     ///< taken before any walk (see run_seed)
    bool   mem_copy_first_seed = false;              ///< true reading; false: a lower bound
    double mem_process_peak_ws = 0;                  ///< the process's lifetime peak, after the seed
    // The carve ledger (BL-1086), read off the build's report.
    bool   ledger_from_budget = false;   ///< the carve counted the budget's planned charters
    int    ledger_nations = 0;           ///< nations the carve counted a competitor in
    long long ledger_competitors = 0;    ///< the competitors it counted, summed
    long long ledger_specialists = 0, ledger_firms = 0;
    int    base_corporations = 0;        ///< corporations on the base world, before the search
    bool   l1 = true;                    ///< the ledger IS the plan on the finished world's budget
    bool   l2 = true;                    ///< a budget world lays no roster before the search
};

const char* short_axis(int a)
{
    switch (a)
    {
    case 0: return "roster";
    case 1: return "placement";
    case 2: return "road_tier";
    default: return "-";
    }
}

double share(double ck, double c0, double cr)
{
    return (cr - c0) != 0.0 ? (ck - c0) / (cr - c0) : 1.0;
}

seed_curve run_seed(lua_state& lua, std::uint32_t seed, int rounds, int check_k,
                    int check_threads, bool print_check, bool first_seed_in_process,
                    int& failures)
{
    seed_curve sc;
    sc.seed = seed;
    std::printf("\n=== seed %u ===========================================================\n", seed);
    std::fflush(stdout);

    world_params p = arc_params(world_arc::shipped);
    p.seed = seed;
    auto out = std::make_unique<app_start_world>();
    clk::time_point t0 = clk::now();
    build_app_base_world(lua, p, *out);
    sc.t_gen_s = ms_since(t0) / 1000.0;
    const world& base = out->w;

    // finish_campaign_world's step 4, param for param (the harness mirror,
    // apply_shipped_landscape, builds the same): the campaign's search params,
    // the world's own stockpile budget at the shipped divisor and its spend.
    landscape_search_params sp = shipped_search_params(seed, out->cfg.corporation_count);
    sp.rounds = rounds;
    // THE WALK IS THE SERIAL REFERENCE. The campaign scores on
    // k_campaign_search_threads (2, BL-1136); the walk every row is read off
    // runs on ONE, so P1 with --check-threads compares the campaign's threaded
    // search against it rather than against itself.
    sp.thread_count = 1;
    const stockpile_budget sb    = build_stockpile_budget(base, k_stockpile_price_divisor);
    const charter_spend_params spend = stockpile_charter_spend(sb);
    sp.budget = &sb.budget;
    sp.spend  = spend;

    {
        const mem_now m = read_mem();
        sc.mem_base_ws      = m.ws_mb;
        sc.mem_base_private = m.private_mb;
    }
    // ONE WORLD COPY'S COST, taken HERE, before any walk (the BL-1136 review):
    // once the walks have run, their freed copies' pages sit in the heap and a
    // fresh copy reuses them, so a copy measured after them reads near zero.
    // This one is the first copy of the base this seed makes. On the first
    // seed of a process it is a true reading; on a later seed the heap may
    // still hold a previous seed's pages, so it is a LOWER BOUND there (the
    // row says which).
    {
        const double private_before = read_mem().private_mb;
        {
            world probe = base;
            sc.mem_copy_private = read_mem().private_mb - private_before;
        }
        sc.mem_copy_first_seed = first_seed_in_process;
    }
    t0 = clk::now();
    double cpu0 = thread_cpu_ms();
    mem_sampler walk_mem;
    const landscape_search_result walk = search_landscape(base, out->reg, sp);
    const double ms_walk  = ms_since(t0);
    const double cpu_walk = thread_cpu_ms() - cpu0;
    walk_mem.stop();
    sc.mem_walk_ws      = walk_mem.peak_ws_mb();
    sc.mem_walk_private = walk_mem.peak_private_mb();
    sc.refused      = walk.charter_refused;
    sc.fell_back    = walk.charter_fell_back;
    sc.budget_world = !sb.budget.empty() && !walk.charter_refused && !walk.charter_fell_back;
    sc.live_axes    = sc.budget_world ? landscape_axis_count - 1 : landscape_axis_count;
    sc.markets      = walk.seed_score.market_count;
    sc.ms_eval_walk  = ms_walk / std::max(1, walk.evaluations);
    sc.cpu_eval_walk = cpu_walk / std::max(1, walk.evaluations);

    // --- THE CARVE LEDGER (BL-1086) ----------------------------------------
    // L1: on a budget world the carve's planned charters, counted at bump 11
    //     from the budget generation built there, are the plan re-derived from
    //     the budget THIS world's finish builds (`sb`, off world::gen_settlement)
    //     — nation for nation, specialists and firms. It is what proves the two
    //     budgets are one budget, so the carve read the roster the search's
    //     budget buys.
    // L2: a budget world carries no corporation before the search: no roster
    //     was laid to be discarded.
    {
        const generation_report& rep = out->report;
        sc.ledger_from_budget = rep.carve_competitors_from_budget;
        sc.ledger_nations     = static_cast<int>(rep.carve_competitors.size());
        for (const generation_report::carve_competitor_row& r : rep.carve_competitors)
        {
            sc.ledger_competitors += r.competitors;
            sc.ledger_specialists += r.planned_specialists;
            sc.ledger_firms       += r.planned_firms;
        }
        sc.base_corporations = static_cast<int>(base.corporations.size());
        if (sc.budget_world)
        {
            const std::map<entity_id, charter_nation_plan> plan =
                plan_charters_by_nation(base, sb.budget, spend);
            sc.l1 = rep.carve_competitors_from_budget && plan.size() == rep.carve_competitors.size();
            std::size_t i = 0;
            for (const auto& [nid, pl] : plan)
            {
                if (!sc.l1) break;
                const generation_report::carve_competitor_row& r = rep.carve_competitors[i++];
                sc.l1 = r.nation == nid && r.planned_specialists == pl.specialists
                     && r.planned_firms == pl.firms && r.competitors == pl.total();
            }
            sc.l2 = sc.base_corporations == 0;
        }
        else
        {
            sc.l1 = !rep.carve_competitors_from_budget;   // a roster world's carve read the roster
        }
        if (!sc.l1) ++failures;
        if (!sc.l2) ++failures;
    }

    // --- P1: a GENUINE k-round search is the prefix, bit for bit ---------
    if (check_k >= 0 && check_k <= rounds)
    {
        landscape_search_params cp = sp;
        cp.rounds       = check_k;
        cp.print_rounds = print_check; // --print-check: the genuine walk's own round lines
        // --check-threads N: the genuine run scores each round's proposals on N
        // threads, so P1 is ALSO thread invariance on this (budget) world —
        // the serial walk's prefix against a threaded search — and its wall
        // time is what that thread count would save. Its CPU time is then the
        // calling thread's join, not the work, and is not reported.
        cp.thread_count = check_threads;
        t0 = clk::now();
        cpu0 = thread_cpu_ms();
        mem_sampler check_mem;
        const landscape_search_result genuine = search_landscape(base, out->reg, cp);
        const double ms_check  = ms_since(t0);
        const double cpu_check = thread_cpu_ms() - cpu0;
        check_mem.stop();
        sc.mem_check_ws      = check_mem.peak_ws_mb();
        sc.mem_check_private = check_mem.peak_private_mb();
        sc.cpu_eval_check = check_threads <= 1 ? cpu_check / std::max(1, genuine.evaluations) : -1.0;
        const landscape_search_result pre = prefix_of(walk, check_k);
        sc.check_k       = check_k;
        sc.check_threads = check_threads;
        sc.ms_eval_check = ms_check / std::max(1, genuine.evaluations);
        sc.ms_check_s    = ms_check / 1000.0;
        for (int i = 0; i <= check_k && i < static_cast<int>(walk.round_ms.size()); ++i)
            sc.ms_walk_first_k_s += walk.round_ms[static_cast<std::size_t>(i)] / 1000.0;
        sc.p1 = same_result(genuine, pre);
        if (!sc.p1) ++failures;

        // NEGATIVE CONTROLS (the BL-1136 review): P1 must be ABLE to fail. The
        // threaded side is perturbed by the smallest amounts that are still a
        // different walk -- one proposal's composite moved by ONE ULP, and one
        // proposal's placement seed by one bit -- and the same comparison has
        // to report each. A comparator that could not see these would pass a
        // thread count that changed the walk.
        if (!genuine.path.empty())
        {
            const std::size_t mid = genuine.path.size() / 2;
            landscape_search_result nc = genuine;
            nc.path[mid].score.composite =
                std::nextafter(nc.path[mid].score.composite, std::numeric_limits<double>::infinity());
            sc.nc_score = !same_result(nc, pre);
            nc = genuine;
            nc.path.back().proposal.placement_seed ^= 1u;
            sc.nc_candidate = !same_result(nc, pre);
            sc.nc_ran = true;
            if (!sc.nc_score) ++failures;
            if (!sc.nc_candidate) ++failures;
        }
    }

    // --- the curve: k = 0..R off the walk's prefixes ---------------------
    // V5 is one re-lay per DISTINCT winner (a held round keeps its incumbent),
    // cached by the candidate it re-laid.
    struct relay_memo { landscape_candidate c; bool ok; };
    std::vector<relay_memo> memo;
    const auto v5_of = [&](const landscape_candidate& c, const landscape_score& s) {
        for (const relay_memo& m : memo)
            if (same_candidate(m.c, c)) return m.ok;
        clk::time_point a = clk::now();
        world relay = base;
        sc.relay_copy += ms_since(a);
        a = clk::now();
        apply_landscape_candidate(relay, out->reg, c, /*regenerate_specialists=*/true,
                                  &sb.budget, spend, /*report=*/nullptr);
        sc.relay_apply += ms_since(a);
        a = clk::now();
        const landscape_score again = score_landscape(relay, out->reg, sp.score);
        sc.relay_score += ms_since(a);
        ++sc.relays;
        const bool ok = same_full_score(again, s);
        memo.push_back({ c, ok });
        return ok;
    };

    for (int k = 0; k <= rounds; ++k)
    {
        const landscape_search_result r = prefix_of(walk, k);
        row w;
        w.k           = k;
        w.evaluations = r.evaluations;
        w.winner      = r.winner;
        w.score       = r.winner_score;
        w.accepted    = r.accepted;
        if (k >= 1)
            for (const landscape_search_step& s : walk.path)
                if (s.round == k - 1 && s.accepted)
                    w.moved_axis = static_cast<int>(s.axis);
        w.v1 = r.evaluations == 1 + k * sc.live_axes;
        w.v2 = static_cast<int>(r.path.size()) == k * sc.live_axes;
        w.v3 = compare_landscape(r.winner_score, r.seed_score) >= 0;
        landscape_score incumbent = r.seed_score;
        bool strict = true;
        int  taken  = 0;
        for (const landscape_search_step& s : r.path)
        {
            if (!s.accepted) continue;
            ++taken;
            if (compare_landscape(s.score, incumbent) <= 0) strict = false;
            incumbent = s.score;
        }
        w.v4 = strict && taken == r.accepted && same_full_score(incumbent, r.winner_score);
        w.v5 = v5_of(r.winner, r.winner_score);
        w.is_final = same_candidate(r.winner, walk.winner);
        if (!w.all()) ++failures;
        sc.rows.push_back(w);
    }
    if (sc.relays > 0)
    {
        sc.relay_copy  /= sc.relays;
        sc.relay_apply /= sc.relays;
        sc.relay_score /= sc.relays;
    }

    // --- the seed's table ---------------------------------------------------
    std::printf("  %s, %d live axes, %d markets; generation %.1f s\n",
                sc.budget_world ? "BUDGET WORLD (roster axis skipped)"
                : sc.refused    ? "budget REFUSED (the no-budget search)"
                : sc.fell_back  ? "budget opens no specialist (the no-budget search, NR-910)"
                                : "no budget (the no-budget search)",
                sc.live_axes, sc.markets, sc.t_gen_s);
    const double c0 = sc.rows.front().score.composite;
    const double cr = sc.rows.back().score.composite;
    std::printf("   k  evals  winner                                 composite     gain%%    share  moved-by   V1-V5  =R%d\n",
                rounds);
    for (const row& w : sc.rows)
    {
        std::printf("  %2d  %5d  %-38s %.9f  %+7.3f%%  %6.1f%%  %-9s  %c%c%c%c%c  %s\n",
                    w.k, w.evaluations, describe(w.winner).c_str(), w.score.composite,
                    c0 != 0.0 ? 100.0 * (w.score.composite / c0 - 1.0) : 0.0,
                    100.0 * share(w.score.composite, c0, cr), short_axis(w.moved_axis),
                    w.v1 ? 'P' : 'F', w.v2 ? 'P' : 'F', w.v3 ? 'P' : 'F', w.v4 ? 'P' : 'F',
                    w.v5 ? 'P' : 'F', w.is_final ? "yes" : "no");
    }
    std::printf("  ms/eval: walk %.0f (%d evals, %.1f s)", sc.ms_eval_walk, walk.evaluations,
                ms_walk / 1000.0);
    if (sc.check_k >= 0)
        std::printf("  |  P1 genuine %d-round walk on %d thread(s) %.0f (%.1f s, the serial walk's "
                    "first %d rounds %.1f s)  |  P1 %s",
                    sc.check_k, sc.check_threads, sc.ms_eval_check, sc.ms_check_s, sc.check_k,
                    sc.ms_walk_first_k_s,
                    sc.p1 ? "PASS (prefix == genuine, bit for bit)" : "FAIL");
    if (sc.nc_ran)
        std::printf("\n  P1 negative controls: one proposal's composite +1 ulp %s; one proposal's "
                    "placement seed ^1 %s",
                    sc.nc_score ? "DETECTED" : "MISSED (FAIL)",
                    sc.nc_candidate ? "DETECTED" : "MISSED (FAIL)");
    std::printf("\n  CPU ms/eval: walk %.0f  |  P1 %.0f", sc.cpu_eval_walk, sc.cpu_eval_check);
    std::printf("\n  one evaluation, split (V5's %d re-lays, mean): copy %.0f ms, apply %.0f ms, score %.0f ms\n",
                sc.relays, sc.relay_copy, sc.relay_apply, sc.relay_score);
    sc.mem_process_peak_ws = read_mem().peak_ws_mb;
    std::printf("  carve ledger: %s, %d nations, %lld competitors (%lld specialists + %lld firms planned); "
                "%d corporations on the base before the search  |  L1 %s  L2 %s\n",
                sc.ledger_from_budget ? "the BUDGET'S PLANNED CHARTERS" : "the laid roster",
                sc.ledger_nations, sc.ledger_competitors, sc.ledger_specialists, sc.ledger_firms,
                sc.base_corporations, sc.l1 ? "PASS" : "FAIL", sc.l2 ? "PASS" : "FAIL");
    std::printf("  memory, MB (working set / private): base held %.0f / %.0f; serial walk peak %.0f / %.0f; "
                "P1 walk on %d thread(s) peak %.0f / %.0f; one world copy %.0f private (%s); "
                "process lifetime peak working set %.0f\n",
                sc.mem_base_ws, sc.mem_base_private, sc.mem_walk_ws, sc.mem_walk_private,
                sc.check_threads, sc.mem_check_ws, sc.mem_check_private, sc.mem_copy_private,
                sc.mem_copy_first_seed ? "before any walk, first seed: a true reading"
                                       : "before any walk, a later seed: a LOWER BOUND",
                sc.mem_process_peak_ws);
    // One machine-readable line per seed, for a sweep to collect.
    std::printf("CURVE seed=%u live=%d markets=%d ms_eval_walk=%.1f ms_eval_check=%.1f "
                "cpu_eval_walk=%.1f cpu_eval_check=%.1f check_threads=%d check_s=%.2f "
                "walk_first_k_s=%.2f mem_base=%.0f mem_walk=%.0f mem_check=%.0f mem_copy=%.0f "
                "mem_peak=%.0f ledger_budget=%d ledger_competitors=%lld base_corps=%d l1=%d l2=%d "
                "winner_placement=%08X winner_tier=%u p1=%d composite",
                seed, sc.live_axes, sc.markets, sc.ms_eval_walk, sc.ms_eval_check,
                sc.cpu_eval_walk, sc.cpu_eval_check, sc.check_threads, sc.ms_check_s,
                sc.ms_walk_first_k_s, sc.mem_base_private, sc.mem_walk_private,
                sc.mem_check_private, sc.mem_copy_private, sc.mem_process_peak_ws,
                sc.ledger_from_budget ? 1 : 0, sc.ledger_competitors, sc.base_corporations,
                sc.l1 ? 1 : 0, sc.l2 ? 1 : 0, walk.winner.placement_seed,
                static_cast<unsigned>(walk.winner.road_tier), sc.p1 ? 1 : 0);
    for (const row& w : sc.rows) std::printf(" %.12g", w.score.composite);
    std::printf(" moved");
    for (const row& w : sc.rows) std::printf(" %d", w.moved_axis);
    std::printf(" valid");
    for (const row& w : sc.rows) std::printf(" %d", w.all() ? 1 : 0);
    std::printf("\n");
    std::fflush(stdout);
    return sc;
}

int run(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    int rounds  = 6;
    int check_k = 3;
    int check_threads = 1;
    bool print_check = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
            seeds = parse_seed_list(argv[++i]);
        else if (std::strcmp(argv[i], "--rounds") == 0 && i + 1 < argc)
            rounds = std::max(0, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--check-k") == 0 && i + 1 < argc)
            check_k = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--check-threads") == 0 && i + 1 < argc)
            check_threads = std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--print-check") == 0)
            print_check = true;
    }
    if (seeds.empty())
        seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty())
    {
        std::printf("FATAL: no seeds (run from the repo root, or pass --seeds)\n");
        return 2;
    }

    std::printf("landscape_search --curve — BL-1136, the search's score per round count\n"
                "  %zu seeds, R = %d rounds, P1 genuine check at k = %d on %d thread(s)\n",
                seeds.size(), rounds, check_k, check_threads);

    lua_state lua;
    int failures = 0;
    std::vector<seed_curve> all;
    for (const std::uint32_t s : seeds)
        all.push_back(run_seed(lua, s, rounds, check_k, check_threads, print_check,
                               /*first_seed_in_process=*/all.empty(), failures));

    // --- pooled -----------------------------------------------------------------
    std::printf("\n=== POOLED over %zu seeds ============================================\n",
                all.size());
    int gained = 0;
    for (const seed_curve& sc : all)
        if (sc.rows.back().score.composite != sc.rows.front().score.composite) ++gained;
    std::printf("  %d of %zu seeds gain anything over %d rounds; share = (c_k - c_0) / (c_R - c_0)\n",
                gained, all.size(), rounds);
    std::printf("   k  pooled-share  mean-share  min-share  seeds=R%d  valid  taken in round k (roster/placement/road)\n",
                rounds);
    for (int k = 0; k <= rounds; ++k)
    {
        double num = 0.0, den = 0.0, mean = 0.0, mn = 1.0;
        int at_final = 0, valid = 0, n_gain = 0;
        int taken[landscape_axis_count] = { 0, 0, 0 };
        for (const seed_curve& sc : all)
        {
            const double c0 = sc.rows.front().score.composite;
            const double cr = sc.rows.back().score.composite;
            const double ck = sc.rows[static_cast<std::size_t>(k)].score.composite;
            num += ck - c0;
            den += cr - c0;
            if (cr != c0)
            {
                const double s = share(ck, c0, cr);
                mean += s;
                mn = std::min(mn, s);
                ++n_gain;
            }
            if (sc.rows[static_cast<std::size_t>(k)].is_final) ++at_final;
            if (sc.rows[static_cast<std::size_t>(k)].all()) ++valid;
            const int m = sc.rows[static_cast<std::size_t>(k)].moved_axis;
            if (m >= 0) ++taken[m];
        }
        std::printf("  %2d  %11.1f%%  %9.1f%%  %8.1f%%  %6d  %5d  %d/%d/%d\n", k,
                    den != 0.0 ? 100.0 * num / den : 100.0,
                    n_gain ? 100.0 * mean / n_gain : 100.0, n_gain ? 100.0 * mn : 100.0,
                    at_final, valid, taken[0], taken[1], taken[2]);
    }

    // --- what each count costs, per seed ---------------------------------------
    std::printf("\n  seconds each count SAVES against R = %d, per seed: (R - k) x live axes x ms/eval\n"
                "  ms/eval = the mean of the two CPU-time measurements (the search is serial on\n"
                "  one thread, so this is its cost with the machine to itself); the two wall\n"
                "  measurements are printed beside it and are indicative on a shared machine\n",
                rounds);
    std::printf("  seed  cpu ms/eval (walk, P1)  wall (walk, P1)  ");
    for (int k = 0; k < rounds; ++k) std::printf("   k=%d", k);
    std::printf("\n");
    for (const seed_curve& sc : all)
    {
        // A threaded P1 has no CPU figure (the calling thread only joins):
        // the walk's is then the one measurement.
        const double ms = (sc.check_k >= 0 && sc.cpu_eval_check >= 0.0)
                              ? 0.5 * (sc.cpu_eval_walk + sc.cpu_eval_check)
                              : sc.cpu_eval_walk;
        std::printf("  %4u  %5.0f (%5.0f, %5.0f)    (%5.0f, %5.0f)  ", sc.seed, ms,
                    sc.cpu_eval_walk, sc.cpu_eval_check, sc.ms_eval_walk, sc.ms_eval_check);
        for (int k = 0; k < rounds; ++k)
            std::printf("  %5.1f", (rounds - k) * sc.live_axes * ms / 1000.0);
        std::printf("\n");
    }

    int p1_fail = 0;
    for (const seed_curve& sc : all)
        if (sc.check_k >= 0 && !sc.p1) ++p1_fail;
    std::printf("\n  P1 (a genuine k-round search IS the walk's prefix): %s\n",
                p1_fail == 0 ? "PASS on every seed" : "FAIL on at least one seed");
    std::printf("\n%s — %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}

} // namespace curve

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--curve") == 0)
            return curve::run(argc, argv);

    std::printf("landscape_search — BL-770 slice 3, the phase 6 greedy search\n");

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    const world_gen_config gen_cfg = parsed_gen_config(lua);

    // The standing vacuity guard: a registry that loaded nothing reports every
    // recipe costless and every chain closed.
    if (reg.recipe_count() == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    // ONE base world, generated ONCE. Ben's point 3: candidates vary rosters,
    // placements and road tiers — never worlds — which is the whole reason the
    // search is affordable and the reason `search_landscape` takes a const base.
    const world_params wp = no_prehistory();
    const world base = make_hard_coded_world(wp, nullptr, gen_cfg);

    landscape_search_params sp;
    sp.rounds = 4;
    sp.seed   = 0x5EA12C00u;
    sp.start  = landscape_candidate{ 8, 0xC0FFEEu, 1 };

    // ------------------------------------------------------------------
    // R1 — the search runs, and it MOVES. A search that never left its seed
    //      would make R2 below true for the wrong reason.
    // ------------------------------------------------------------------
    std::printf("\nR1. the walk\n");
    landscape_search_result serial;
    {
        const auto t0 = std::chrono::steady_clock::now();
        sp.thread_count = 1;
        serial = search_landscape(base, reg, sp);
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count();
        print_result("serial", serial, ms);
    }

    check(serial.evaluations == 1 + landscape_axis_count * sp.rounds, "R1.1",
          "FIXED ROUNDS - evaluations are exactly 1 + 3 x rounds, so the work does "
          "not depend on the landscape (no convergence exit, no hidden threshold)");
    check(static_cast<int>(serial.path.size()) == landscape_axis_count * sp.rounds, "R1.2",
          "every round proposed on all three axes and the whole path was recorded");
    check(compare_landscape(serial.winner_score, serial.seed_score) >= 0, "R1.3",
          "the winner is never WORSE than the seed candidate (strict-improvement-only "
          "acceptance cannot regress)");
    check(serial.accepted > 0, "R1.4",
          "the walk actually MOVED off its seed - without this, thread-invariance "
          "below would be true of a search that never searched");

    // Strict improvement, checked on the path rather than on the outcome: every
    // accepted step must beat the incumbent it replaced.
    {
        landscape_score incumbent = serial.seed_score;
        bool strict = true;
        for (const landscape_search_step& s : serial.path)
        {
            if (!s.accepted)
                continue;
            if (compare_landscape(s.score, incumbent) <= 0)
                strict = false;
            incumbent = s.score;
        }
        check(strict, "R1.5",
              "the incumbent was replaced ONLY on a STRICT improvement - a tie leaves "
              "it standing rather than churning between equals");
        check(same_score(incumbent, serial.winner_score), "R1.6",
              "the reported winner IS the last accepted step - the path and the result "
              "are one record, not two");
    }

    // ------------------------------------------------------------------
    // R2 — THE BINDING ASSERTION. Same seed, different thread counts.
    // ------------------------------------------------------------------
    std::printf("\nR2. THREAD-COUNT INVARIANCE (the binding constraint)\n");
    for (const int threads : { 2, 3, 4, 8 })
    {
        landscape_search_params tp = sp;
        tp.thread_count = threads;
        const auto t0 = std::chrono::steady_clock::now();
        const landscape_search_result r = search_landscape(base, reg, tp);
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count();

        char lbl[32];
        std::snprintf(lbl, sizeof lbl, "%d threads", threads);
        print_result(lbl, r, ms);

        char id[16];
        std::snprintf(id, sizeof id, "R2.%d", threads);
        const bool ok = same_candidate(r.winner, serial.winner)
                     && same_score(r.winner_score, serial.winner_score)
                     && r.evaluations == serial.evaluations
                     && r.accepted    == serial.accepted;
        check(ok, id,
              std::string("winner and every scored term are IDENTICAL to the serial run at ")
                  + lbl + " - the argmax reads a total order, never completion order");
    }

    // ------------------------------------------------------------------
    // R3 — replay. The same seed twice is the same search.
    // ------------------------------------------------------------------
    std::printf("\nR3. replay\n");
    {
        landscape_search_params tp = sp;
        tp.thread_count = 1;
        const landscape_search_result again = search_landscape(base, reg, tp);
        bool path_same = again.path.size() == serial.path.size();
        for (std::size_t i = 0; path_same && i < again.path.size(); ++i)
            path_same = same_candidate(again.path[i].proposal, serial.path[i].proposal)
                     && same_score(again.path[i].score, serial.path[i].score)
                     && again.path[i].accepted == serial.path[i].accepted;
        check(path_same, "R3.1",
              "re-running the same seed reproduces the WHOLE PATH bit-identically, not "
              "merely the same winner");
    }

    // ------------------------------------------------------------------
    // R4 — the seed is a real input. Two search seeds must be able to walk
    //      differently, or the perturbation stream is not being consulted.
    // ------------------------------------------------------------------
    std::printf("\nR4. the perturbation stream is live\n");
    {
        landscape_search_params tp = sp;
        tp.seed = 0x0BADF00Du;
        tp.thread_count = 1;
        const landscape_search_result other = search_landscape(base, reg, tp);
        print_result("seed 0BADF00D", other, 0.0);
        bool any_different = false;
        for (std::size_t i = 0; i < other.path.size() && i < serial.path.size(); ++i)
            if (!same_candidate(other.path[i].proposal, serial.path[i].proposal))
                any_different = true;
        check(any_different, "R4.1",
              "a different search seed proposes different candidates - the seeded stream "
              "is actually driving the perturbations");
        check(same_score(other.seed_score, serial.seed_score), "R4.2",
              "both searches start from the SAME scored seed candidate - the search seed "
              "moves the walk, never the starting point");
    }

    // ------------------------------------------------------------------
    // R5 — PER-AXIS DISCRIMINATION. Slice 1's question, asked once per axis
    //      instead of once per candidate: does the objective SEE this axis?
    //      An axis it cannot see still costs a third of every round's work and
    //      contributes nothing, so this is a measurement the search's caller
    //      needs, not a nicety.
    //
    //      IT REPORTS RATHER THAN FAILS, deliberately, exactly as slice 1's
    //      harness did: a blind axis is a finding about the OBJECTIVE, not a
    //      defect in the search, and a permanently-red suite is how a real
    //      regression gets missed. What DOES fail here is the control — if the
    //      fixture never moved, nothing below is readable.
    // ------------------------------------------------------------------
    std::printf("\nR5. per-axis discrimination\n");
    {
        int base_hist[4] = { 0, 0, 0, 0 };
        for (const auto& kv : base.tiles)
            if (kv.second.road_level > 0 && kv.second.road_level < 4)
                ++base_hist[kv.second.road_level];
        std::printf("    base road field: tier1=%d tier2=%d tier3=%d\n",
                    base_hist[1], base_hist[2], base_hist[3]);

        landscape_score at[4];
        int at_top[4] = { 0, 0, 0, 0 };   // tiles at tier 3 AFTER the candidate applied
        for (std::uint8_t t = 1; t <= 3; ++t)
        {
            world w = base;
            apply_landscape_candidate(w, reg, landscape_candidate{ 8, 0xC0FFEEu, t });
            for (const auto& kv : w.tiles)
                if (kv.second.road_level >= 3)
                    ++at_top[t];
            at[t] = score_landscape(w, reg);
            std::printf("    road_tier=%u  highway tiles=%-5d  potential=%.5f  "
                        "ACTUAL=%.5f  composite=%.9f\n",
                        static_cast<unsigned>(t), at_top[t], at[t].mean_completeness,
                        at[t].mean_actual, at[t].composite);
        }

        // THE CONTROL, and it compares the SAME quantity across candidates — the
        // first cut counted "tiles at or above tier t", which is trivially equal
        // for every t once the uplift has run and therefore controlled nothing.
        check(at_top[3] > at_top[1], "R5.0",
              "the tier axis was actually APPLIED - the highway count moves between "
              "tier 1 and tier 3, so a flat score is about the objective, not a no-op");

        // NR-793, THE DECISIVE MEASUREMENT (Ben, 2026-09-07: "run it").
        //
        // A flat score has two possible causes and they call for opposite
        // fixes. Either (a) the tier moves the REACH FIELD and the objective
        // then fails to read the difference — a blind objective, fixed by a new
        // term; or (b) the tier does not move the reach field AT ALL, because
        // roads sit where the economy already is and the 24.0 budget's frontier
        // is out where there are no roads to upgrade — in which case no term
        // could see it and the axis itself is the wrong one.
        //
        // `in_reach_tiles` is the boolean the objective actually consumes:
        // tiles inside `max_logistics_reach` of their market. Summing it either
        // side of the uplift separates (a) from (b) in one number.
        for (std::uint8_t t = 1; t <= 3; t += 2)
        {
            world w = base;
            apply_landscape_candidate(w, reg, landscape_candidate{ 8, 0xC0FFEEu, t });
            const auto rows = measure_market_completeness(w, reg, classify_resources(w, reg));
            long long catch_sum = 0, reach_sum = 0, raws_sum = 0;
            for (const auto& r : rows)
            {
                catch_sum += r.catchment_tiles;
                reach_sum += r.in_reach_tiles;
                raws_sum  += r.raws_in_reach;
            }
            std::printf("    road_tier=%u  catchment=%lld  IN_REACH=%lld  raws_in_reach=%lld\n",
                        static_cast<unsigned>(t), catch_sum, reach_sum, raws_sum);
        }

        // NR-793 PART 2 (Ben, 2026-09-07): re-run the axis on a LIVE world.
        //
        // Everything above runs on the default-seed fixture, which carries TWO
        // markets, a balance term of exactly 0 and therefore a composite of
        // exactly 0 for every candidate. A conclusion about what the objective
        // can SEE cannot rest on a world where the objective evaluates to zero
        // for every input. Seed ABCDEF01 carries ten markets and a live
        // composite, and it is one of the same control worlds the score
        // harness already uses — same construction, so nothing new is invented
        // here to make the number come out.
        std::printf("\n    -- the same axis on a TEN-MARKET world (seed ABCDEF01) --\n");
        {
            landscape_score live[4];
            for (std::uint8_t t = 1; t <= 3; t += 2)
            {
                world_params lp = wp;
                lp.seed = 0xABCDEF01u;
                world w = make_hard_coded_world(lp, nullptr, gen_cfg);
                assign_default_recipes(w, reg);
                corporation_params cp;
                cp.corporation_count = 8;
                generate_corporations(w, cp, 0xC0FFEEu);
                generate_background_firms(w, reg, 0xC0FFEEu);

                apply_landscape_candidate(w, reg, landscape_candidate{ 8, 0xC0FFEEu, t });
                const auto rows = measure_market_completeness(w, reg, classify_resources(w, reg));
                long long reach_sum = 0, raws_sum = 0;
                for (const auto& r : rows)
                {
                    reach_sum += r.in_reach_tiles;
                    raws_sum  += r.raws_in_reach;
                }
                live[t] = score_landscape(w, reg);
                std::printf("    tier=%u  mkts=%d  IN_REACH=%lld  raws=%lld  potential=%.5f  "
                            "ACTUAL=%.5f  balance=%.5f  spread=%.5f  composite=%.9f\n",
                            static_cast<unsigned>(t), live[t].market_count, reach_sum, raws_sum,
                            live[t].mean_completeness, live[t].mean_actual,
                            live[t].mean_balance, live[t].spread, live[t].composite);
            }
            std::printf("    VERDICT ON A LIVE OBJECTIVE: the road axis is %s\n",
                        same_score(live[1], live[3])
                            ? "STILL INVISIBLE - the fixture was not what hid it"
                            : "SEEN - the earlier finding was an artefact of a degenerate fixture");
        }

        const bool road_seen = !same_score(at[1], at[3]);
        std::printf("    FINDING: the road/infrastructure axis is %s\n",
                    road_seen ? "SEEN by the objective"
                              : "INVISIBLE to the objective - every term is bit-identical "
                                "across tier 1, 2 and 3");
        if (!road_seen)
            std::printf("      Raising all %d road tiles from Track to Highway moves no\n"
                        "      term. The tier scales traversal COST (x0.67/x0.50/x0.40);\n"
                        "      the objective reads catchment MEMBERSHIP and terminal\n"
                        "      closure, both of which are booleans this world's reach\n"
                        "      field does not flip. One of the two has to change before\n"
                        "      this axis earns its third of every round.\n",
                        base_hist[1] + base_hist[2] + base_hist[3]);
    }

    // ------------------------------------------------------------------
    // R6 — THREAD-COUNT INVARIANCE ON A BUDGET WORLD (the BL-1136 review).
    //      R2 runs on a world with no budget, so it never reaches the path the
    //      campaign actually threads: `apply_landscape_candidate`'s budget
    //      overload, `remove_specialist_roster` and `charter_web_from_budget`.
    //      Here the same base carries the harness's SYNTHETIC charter budget
    //      (harness_params.hpp: seeded draws, never a density claim), so every
    //      evaluation charters its web from a budget, serial against 2 and 4
    //      threads. Non-vacuity: the budget branch ran (no refusal, no
    //      fallback, the roster axis skipped) and the winner's apply chartered
    //      a web (points spent, specialists and firms laid).
    // ------------------------------------------------------------------
    std::printf("\nR6. thread-count invariance on a BUDGET world (charter_web_from_budget)\n");
    {
        const charter_budget       synth = synthetic_charter_budget(base, wp.seed, 60, 1.0);
        const charter_spend_params spend = synthetic_charter_spend();
        landscape_search_params bp = sp;
        bp.rounds       = 2;
        bp.budget       = &synth;
        bp.spend        = spend;
        bp.print_rounds = false;
        bp.thread_count = 1;
        const landscape_search_result b1 = search_landscape(base, reg, bp);
        print_result("budget serial", b1, 0.0);

        const bool budget_branch = !synth.empty() && !b1.charter_refused && !b1.charter_fell_back
                                && b1.evaluations == 1 + bp.rounds * (landscape_axis_count - 1);
        check(budget_branch, "R6.0",
              "the search took the BUDGET branch - not refused, not fallen back, the roster "
              "axis skipped (1 + 2 x rounds evaluations)");
        {
            world laid = base;
            charter_spend_report rep;
            apply_landscape_candidate(laid, reg, b1.winner, /*regenerate_specialists=*/true,
                                      &synth, spend, &rep);
            check(rep.points_spent > 0 && !rep.specialists.empty() && !rep.firms.empty(), "R6.1",
                  "the winner's apply CHARTERED a web from the budget (points spent, specialists "
                  "and firms laid) - so the threads below scored charter_web_from_budget");
        }
        for (const int threads : { 2, 4 })
        {
            landscape_search_params tp = bp;
            tp.thread_count = threads;
            const landscape_search_result r = search_landscape(base, reg, tp);
            const bool path_same = curve::same_path(r.path, b1.path);   // every scored term
            char id[16];
            std::snprintf(id, sizeof id, "R6.%d", threads);
            check(path_same && same_candidate(r.winner, b1.winner)
                      && curve::same_full_score(r.winner_score, b1.winner_score)
                      && r.evaluations == b1.evaluations,
                  id, std::string("on a budget world, ") + std::to_string(threads)
                          + " threads give the serial walk's winner, scores and whole path");
        }
    }

    print_path(serial);

    std::printf("\n--- THE SLICE'S QUESTION ---\n"
                "  The search exists and it is DETERMINISTIC: one seed, five thread\n"
                "  counts, one winner, bit-identical on every scored term. The walk\n"
                "  improved the seed candidate's composite %.6f -> %.6f (+%.1f%%);\n"
                "  accepted by axis: roster %d, placement %d, road_tier %d.\n",
                serial.seed_score.composite, serial.winner_score.composite,
                100.0 * (serial.winner_score.composite / serial.seed_score.composite - 1.0),
                serial.accepted_by_axis[0], serial.accepted_by_axis[1],
                serial.accepted_by_axis[2]);

    // ------------------------------------------------------------------
    // The caveat slice 1 printed, carried forward unchanged. It bears on this
    // slice more than on that one: a SEARCH over a shrinking field ranks
    // degrees of failure with more conviction than a scorer does.
    // ------------------------------------------------------------------
    std::printf("\n--- SCORES HERE ARE ORDINAL AND PROVISIONAL ---\n"
                "  Sprint 33's growth gate is UNMET (BL-759 re-based figures). The search\n"
                "  ORDERS candidates; it does not evidence that the winner is viable.\n");

    std::printf("\n%s — %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
