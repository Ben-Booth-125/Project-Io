// ---------------------------------------------------------------------------
// market_viability — BL-1184 (market viability gate), the sprint 49 instrument
// ---------------------------------------------------------------------------
// ONE READING that says whether the market economy is viable from day 1, on
// the SHIPPED start, against the targets Ben set (the sprint 49 form,
// 2026-10-04). Every other sprint 49 item is judged against it: a world-mover
// runs it before and after, in isolation, and reports which rows moved.
//
// THE WORLD. Per seed, once: `build_app_start_world` (harness_params.hpp, the
// app's begin_new_game + start_new_game_prelude), then the 12-tick settle
// exactly as the app's validation run steps it (`run_settle_tick`, econ steps
// 0..11, day tick 0, spectating), then the seat (`seat_player_corporation` on
// the winner's static score, app::seat_player), then PLAY ticks exactly as
// `run_app_live_window` steps them (advance_orbits + advance_surveys over the
// quarter's 90 days, the day tick mirrored, `run_settle_tick` at econ step
// 12 + k - 1, day 90k, not spectating). The tick body is called directly
// rather than through the helpers only because the instrument needs each
// tick's economy_report and a read hook after the clear lap.
//
// A PURE READER. Nothing here writes a field the simulation reads; the hook is
// a const read after the clear lap (market_census's exchange tap). The wall
// clock is read for the runtime line only.
//
// TICKS. "Handoff" = the end of the settle (after its 12th tick, before the
// first play tick). "Tick N" = play tick N, 1-based, counted from the handoff
// (so tick 50 is the 62nd economy tick the world has run). --ticks is the
// number of PLAY ticks (default 400).
//
// THE ROWS.
//   G1 processors (building_type::processing_facility, every body) by state,
//      at the handoff (the settle's last tick's economy_report) and at tick 50:
//        run         the report row is active (produced output this tick)
//        input       INPUT-STARVED: idle, coverage of the scarcest input < t_idle
//        nolab       idle with no effective workforce
//        unsupplied  idle with labour but zero batches: supply scalar 0 (BL-641)
//                    or a zero workforce target
//        decom       decommissioned: idled by the loss reflex (economy_system's
//                    persistent-loser idle) or an idle verb. Split on a second
//                    line by the processor's state on the LAST tick it reported
//                    (ran / input-starved / other / never reported since the
//                    world was handed to the settle), so a plant mothballed
//                    because it starved is told from one laid idle.
//        other       no recipe, no report row, idle for another reason
//        build       under construction (ticks_remaining > 0)
//      share running = run / BUILT processors (every state but build). A mill
//      mid-build is not yet plant that could run; the construction backlog is
//      G4's row. The share over ALL processors, build included, is printed
//      beside it. (This denominator is the instrument's call, not the ruling's
//      wording; it matches the 2026-10-04 reading's "~225 processors".)
//      TARGET: pooled share running at the handoff >= 70%.
//   G2 field income per tick: the sum of `quarterly_return::income` over every
//      corporation's return filed that tick, at the settle's last tick and at
//      tick 50, and the ratio tick50 / settle-close. Pooled = sum / sum.
//      TARGET: pooled ratio >= 50%. For context only, the settle's 12-tick
//      MEAN and the play 26-50 mean are printed: the 2026-10-04 reading's
//      "54-74k at the settle close, 10-19k by tick 50" were those window means
//      (firm_attrition_trace's rows), not single ticks.
//   G3 firms alive at the last play tick that existed at the handoff, over the
//      firms at the handoff (corporations, seat included). Pooled = sum / sum.
//      TARGET: pooled >= 70%. (A firm that appears after the handoff is not in
//      the cohort; the total count at the end is printed beside it.)
//   G4 (reported, no target):
//      seat    the seated corp's OPERATING net (income - expenditure -
//              maintenance - wages - levies - upkeep; interest excluded, as
//              spawn_solvency defines it) for each of the last 8 settle
//              quarters, read from its filed returns; then each of its
//              processors at the handoff with the number of those 8 ticks it
//              ran.
//      idle-mkt home-body markets with no exchange row in any of the first 4
//              play ticks — market_census's CLEARS NOTHING, the same tap. A
//              non-zero `lost` count (exchange ring overflow) makes it an
//              UPPER bound.
//      bldgs   all buildings at tick 50: live / under construction / decommissioned.
//
// EXIT. 0 for a completed reading — the targets are reported, not enforced as
// the exit, because the baseline is expected to fail them. 1 only when an
// honesty check fails: a seed that failed to build, no corporations, zero
// processors at the handoff, zero field income at the settle close, no seat,
// or fewer play ticks than the rows need.
//
// Usage (repo root; it loads scripts/*.lua and the seed library by path):
//   build_gen/verify/market_viability.exe [--seeds a,b] [--ticks N]
// Default seeds: docs/generation/seed_library.json, in library order.
// Build:  bash tools/verify/build_lua_harness.sh market_viability
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

// --- THE TARGETS: Ben's ruling, the sprint 49 form, 2026-10-04 (BL-1184) ----
// Pooled over the seeds read (the 16 curated seeds by default).
constexpr double k_target_g1_running_at_handoff = 0.70; ///< processors running at handoff
constexpr double k_target_g2_income_ratio       = 0.50; ///< field income tick 50 / settle close
constexpr double k_target_g3_firms_alive        = 0.70; ///< firms alive at tick 400 / at handoff

constexpr int k_g1_g2_play_tick     = 50; ///< the "tick 50" reading
constexpr int k_seat_settle_window  = 8;  ///< the seat's last 8 settle quarters
constexpr int k_idle_market_ticks   = 4;  ///< market_census's first play year

// --- seeds (market_census's minimal scan of the library) ---------------------

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

// --- processor states ----------------------------------------------------------

enum proc_state { ps_run = 0, ps_input, ps_nolab, ps_unsup, ps_decom, ps_build, ps_other, ps_count };
const char* k_state_name[ps_count] = {"run", "input", "nolab", "unsupplied", "decom", "build", "other"};

/// The state a REPORT ROW says, ignoring the building's flags (used to remember
/// what a processor was doing on the last tick it ran production at all).
proc_state classify_row(const building_component& b, const building_report* rep,
                        const recipe_registry& reg)
{
    if (reg.get_recipe(b.recipe) == nullptr)  return ps_other;
    if (rep == nullptr)                       return ps_other;
    if (rep->active)                          return ps_run;
    if (rep->effective_workforce <= 0.0f)     return ps_nolab;
    // run_processing's first early return with labour present: zero batches from
    // a zero supply scalar (BL-641) or a zero workforce target. Before any input
    // is read, so has_limiting is false here.
    if (building_supply_scalar(b) <= 0.0f || b.workforce_target <= 0.0f) return ps_unsup;
    // has_limiting is set for EVERY processor with an input (running ones too);
    // on an idle row past the two checks above it means coverage < t_idle.
    if (rep->has_limiting)                    return ps_input;
    return ps_other;
}

proc_state classify(const building_component& b, const building_report* rep,
                    const recipe_registry& reg)
{
    if (b.ticks_remaining > 0)                return ps_build;
    if (b.decommissioned)                     return ps_decom;
    return classify_row(b, rep, reg);
}

/// Per processor, its row state on the last tick it had a report row (a
/// decommissioned or unfinished building gets none). Absent = never reported.
using last_row_map = std::map<entity_id, proc_state>;

void remember_rows(const world& w, const economy_report& rep, const recipe_registry& reg,
                   last_row_map& last)
{
    for (const building_report& br : rep.buildings)
    {
        if (br.type != building_type::processing_facility) continue;
        const auto bi = w.buildings.find(br.building);
        if (bi != w.buildings.end()) last[br.building] = classify_row(bi->second, &br, reg);
    }
}

struct proc_tally
{
    int n[ps_count] = {};
    /// The decom bucket split by what the processor was doing on the last tick it
    /// reported: [run], [input], [any other row state], [never reported since the
    /// world was built] -- so "mothballed after starving" is told from "mothballed
    /// while running" and from "laid decommissioned".
    int decom_after[4] = {};
    int total() const { int t = 0; for (int i = 0; i < ps_count; ++i) t += n[i]; return t; }
    /// THE G1 DENOMINATOR: processors that EXIST AS PLANT — every state but
    /// under construction. A mill mid-build is not yet a processor that could
    /// run; the construction backlog is G4's row, not G1's.
    int built() const { return total() - n[ps_build]; }
    double share_run() const { const int t = built(); return t ? static_cast<double>(n[ps_run]) / t : 0.0; }
    double share_run_incl_build() const { const int t = total(); return t ? static_cast<double>(n[ps_run]) / t : 0.0; }
    void add(const proc_tally& o)
    {
        for (int i = 0; i < ps_count; ++i) n[i] += o.n[i];
        for (int i = 0; i < 4; ++i) decom_after[i] += o.decom_after[i];
    }
};

const building_report* row_of(const economy_report& rep, entity_id bid)
{
    const auto it = rep.building_row.find(bid);
    return it == rep.building_row.end() ? nullptr : &rep.buildings[it->second];
}

proc_tally tally_processors(const world& w, const economy_report& rep, const recipe_registry& reg,
                            const last_row_map& last)
{
    proc_tally t;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility) continue;
        const proc_state s = classify(b, row_of(rep, bid), reg);
        ++t.n[s];
        if (s == ps_decom)
        {
            const auto it = last.find(bid);
            ++t.decom_after[it == last.end() ? 3 : it->second == ps_run ? 0 : it->second == ps_input ? 1 : 2];
        }
    }
    return t;
}

/// The field's income this tick: every corporation's return just filed.
/// apply_budget files one return per corporation per tick, so `returns.back()`
/// read straight after the tick is that tick's; a corp wound up this tick is
/// gone from the map and contributes nothing.
double field_income(const world& w)
{
    double s = 0.0;
    for (const auto& [cid, cc] : w.corporations)
    {
        (void)cid;
        if (!cc.returns.empty()) s += cc.returns.back().income;
    }
    return s;
}

double operating_net(const quarterly_return& q)
{
    return static_cast<double>(q.income) - q.expenditure - q.maintenance - q.wages - q.levies - q.upkeep;
}

// --- the exchange tap (market_census's CLEARS NOTHING) ---------------------------

int clear_lap_index()
{
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
        if (std::strcmp(k_campaign_settle_lap_names[i], "clear_markets") == 0)
            return i;
    return -1;
}

struct exchange_tap
{
    int                  clear_lap  = -1;
    std::size_t          last_total = 0;
    std::size_t          lost       = 0;
    std::set<entity_id>  active;     ///< markets with any exchange row in a tapped tick
};

void tap_after_lap(const world& w, int lap, void* ctx)
{
    auto* t = static_cast<exchange_tap*>(ctx);
    if (lap != t->clear_lap) return;
    const std::size_t total = w.exchanges.total;
    const std::size_t fresh = total - t->last_total;
    const std::size_t held  = std::min(fresh, w.exchanges.size());
    t->lost += fresh - held;
    const std::size_t n = w.exchanges.size();
    for (std::size_t i = n - held; i < n; ++i)
        t->active.insert(w.exchanges.oldest_first(i).market);
    t->last_total = total;
}

// --- one seed ----------------------------------------------------------------------

struct seed_reading
{
    std::uint32_t seed = 0;
    bool   built = false;
    std::string fail;                  ///< non-empty: an honesty check failed
    proc_tally g1_handoff, g1_t50;
    double inc_close = 0.0, inc_t50 = 0.0;
    double inc_settle_mean = 0.0;      ///< mean over the 12 settle ticks (the reading's "settle close")
    double inc_t26_50_mean = 0.0;      ///< mean over play ticks 26..50 (firm_attrition_trace's window)
    double secs_build = 0.0, secs_settle = 0.0, secs_play = 0.0;
    int    decom_at_build = 0, proc_at_build = 0; ///< processors when the world is handed to the settle
    int    firms_handoff = 0, firms_survived = 0, firms_end = 0, last_tick = 0;
    // G4
    entity_id seat = null_entity;
    std::string seat_name;
    std::vector<double> seat_opnet;    ///< the last 8 settle quarters, oldest first
    struct seat_proc { entity_id id; std::string recipe; int ran; const char* state; };
    std::vector<seat_proc> seat_procs;
    int home_markets = 0, idle_markets = 0;
    std::size_t tap_lost = 0;
    int b_live = 0, b_build = 0, b_decom = 0;
    double secs = 0.0;
};

void run_seed(std::uint32_t seed, int ticks, seed_reading& r)
{
    using clk = std::chrono::steady_clock;
    const clk::time_point t0 = clk::now();
    r.seed = seed;

    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try
    {
        build_app_start_world(lua, p, *start);
    }
    catch (const std::exception& e)
    {
        r.fail = std::string("world build threw: ") + e.what();
        return;
    }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    const clk::time_point t_built = clk::now();
    r.secs_build = std::chrono::duration<double>(t_built - t0).count();
    if (w.corporations.empty()) { r.fail = "no corporations after the build"; return; }
    r.built = true;

    for (const auto& [bid, b] : w.buildings)
    {
        (void)bid;
        if (b.type != building_type::processing_facility) continue;
        ++r.proc_at_build;
        if (b.decommissioned) ++r.decom_at_build;
    }
    last_row_map last_row;

    // --- the settle: econ steps 0..11, day 0, spectating (the validation run) ---
    std::map<entity_id, int> ran_window;   // processor -> active ticks in the last 8 settle ticks
    economy_report last_settle;
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        settle_tick_result res = run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true);
        r.inc_settle_mean += field_income(w) / k_campaign_settle_ticks;
        remember_rows(w, res.report, reg, last_row);
        if (step >= k_campaign_settle_ticks - k_seat_settle_window)
            for (const building_report& br : res.report.buildings)
                if (br.type == building_type::processing_facility && br.active)
                    ++ran_window[br.building];
        if (step == k_campaign_settle_ticks - 1)
        {
            r.inc_close = field_income(w);
            last_settle = std::move(res.report);
        }
    }

    const clk::time_point t_settled = clk::now();
    r.secs_settle = std::chrono::duration<double>(t_settled - t_built).count();

    // --- the seat (app::seat_player) ---
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    r.seat = seat.seated;

    // --- G1 at the handoff (the settle's last report, the world after the seat) ---
    r.g1_handoff = tally_processors(w, last_settle, reg, last_row);
    if (r.g1_handoff.total() == 0) r.fail = "zero processors at the handoff";
    if (r.inc_close <= 0.0 && r.fail.empty()) r.fail = "zero field income at the settle close";
    if (r.seat == null_entity && r.fail.empty()) r.fail = "no corporation seated";

    std::set<entity_id> cohort;
    for (const auto& [cid, cc] : w.corporations) { (void)cc; cohort.insert(cid); }
    r.firms_handoff = static_cast<int>(cohort.size());

    // --- G4: the seat's last 8 settle quarters and its processors ---
    if (const auto it = w.corporations.find(r.seat); it != w.corporations.end())
    {
        const corporation_component& sc = it->second;
        r.seat_name = sc.name;
        const std::size_t n = sc.returns.size();
        const std::size_t from = n > static_cast<std::size_t>(k_seat_settle_window) ? n - k_seat_settle_window : 0;
        for (std::size_t i = from; i < n; ++i) r.seat_opnet.push_back(operating_net(sc.returns[i]));
        std::vector<entity_id> assets(sc.assets.begin(), sc.assets.end());
        std::sort(assets.begin(), assets.end());
        for (const entity_id a : assets)
        {
            const auto bi = w.buildings.find(a);
            if (bi == w.buildings.end() || bi->second.type != building_type::processing_facility) continue;
            const recipe* rc = reg.get_recipe(bi->second.recipe);
            const auto rw = ran_window.find(a);
            r.seat_procs.push_back({a, rc ? rc->name : std::string("(no recipe)"),
                                    rw == ran_window.end() ? 0 : rw->second,
                                    k_state_name[classify(bi->second, row_of(last_settle, a), reg)]});
        }
    }

    // --- play: run_app_live_window's tick state, read-hooked over the first 4 ---
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == w.home_body) ++r.home_markets;
    exchange_tap tap;
    tap.clear_lap  = clear_lap_index();
    tap.last_total = w.exchanges.total;
    constexpr int k_econ_tick_days = 90; // sim_loop::econ_tick_days (harness_params.hpp)
    for (int k = 1; k <= ticks; ++k)
    {
        const int day = k * k_econ_tick_days;
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = day;
        settle_tick_hooks hooks;
        if (k <= k_idle_market_ticks) { hooks.after_lap = tap_after_lap; hooks.ctx = &tap; }
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day,
                                                 /*spectating=*/false, &hooks);
        if (k == k_idle_market_ticks)
        {
            for (const auto& [mid, mc] : w.markets)
                if (mc.body == w.home_body && !tap.active.count(mid)) ++r.idle_markets;
            r.tap_lost = tap.lost;
        }
        if (k <= k_g1_g2_play_tick) remember_rows(w, res.report, reg, last_row);
        if (k > k_g1_g2_play_tick - 25 && k <= k_g1_g2_play_tick)
            r.inc_t26_50_mean += field_income(w) / 25.0;
        if (k == k_g1_g2_play_tick)
        {
            r.g1_t50  = tally_processors(w, res.report, reg, last_row);
            r.inc_t50 = field_income(w);
            for (const auto& [bid, b] : w.buildings)
            {
                (void)bid;
                if (b.decommissioned) ++r.b_decom;
                else if (b.ticks_remaining > 0) ++r.b_build;
                else ++r.b_live;
            }
        }
        r.last_tick = k;
    }
    for (const entity_id c : cohort)
        if (w.corporations.count(c)) ++r.firms_survived;
    r.firms_end = static_cast<int>(w.corporations.size());
    if (ticks < k_g1_g2_play_tick && r.fail.empty())
        r.fail = "fewer play ticks than the tick-50 rows need";
    const clk::time_point t_end = clk::now();
    r.secs_play = std::chrono::duration<double>(t_end - t_settled).count();
    r.secs = std::chrono::duration<double>(t_end - t0).count();
}

double pct(double x) { return 100.0 * x; }

void print_tally(const char* label, const proc_tally& t)
{
    std::printf("  %-10s built %4d: run %4d  input %4d  nolab %4d  unsupplied %4d  decom %4d  other %4d"
                "  -> running %5.1f%%  | + under construction %4d (running of all %5.1f%%)\n",
                label, t.built(), t.n[ps_run], t.n[ps_input], t.n[ps_nolab], t.n[ps_unsup],
                t.n[ps_decom], t.n[ps_other], pct(t.share_run()), t.n[ps_build],
                pct(t.share_run_incl_build()));
    std::printf("  %-10s decom by its last reported state: ran %d  input-starved %d  other %d  never reported %d\n",
                "", t.decom_after[0], t.decom_after[1], t.decom_after[2], t.decom_after[3]);
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 400;
    std::vector<std::uint32_t> seeds;
    bool from_args = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) { seeds = parse_seed_list(argv[++i]); from_args = true; }
        else { std::fprintf(stderr, "usage: market_viability [--seeds a,b] [--ticks N]\n"); return 2; }
    }
    if (!from_args)
    {
        seeds = library_seeds("docs/generation/seed_library.json");
        if (seeds.empty())
        {
            std::printf("FATAL  docs/generation/seed_library.json not found or carries no seeds "
                        "(run from the repo root)\n");
            return 1;
        }
    }

    std::printf("market_viability — BL-1184 (market viability gate)\n");
    std::printf("targets (Ben, the sprint 49 form, 2026-10-04), pooled: G1 running at handoff >= %.0f%%,"
                " G2 income t%d/settle-close >= %.0f%%, G3 firms alive t%d/handoff >= %.0f%%\n",
                pct(k_target_g1_running_at_handoff), k_g1_g2_play_tick, pct(k_target_g2_income_ratio),
                ticks, pct(k_target_g3_firms_alive));
    std::printf("seeds (%s, %zu):", from_args ? "--seeds" : "docs/generation/seed_library.json", seeds.size());
    for (const std::uint32_t s : seeds) std::printf(" %u", s);
    std::printf("   play ticks %d (tick N = play tick N after the 12-tick settle)\n", ticks);
    std::fflush(stdout);

    std::vector<seed_reading> rs;
    rs.reserve(seeds.size());
    for (const std::uint32_t seed : seeds)
    {
        rs.emplace_back();
        seed_reading& r = rs.back();
        run_seed(seed, ticks, r);
        std::printf("\n=== seed %u  (%.0f s: build %.0f, settle %.0f, play %.0f)%s%s ===\n", seed, r.secs,
                    r.secs_build, r.secs_settle, r.secs_play,
                    r.fail.empty() ? "" : "  HONESTY FAIL: ", r.fail.c_str());
        if (!r.built) { std::fflush(stdout); continue; }
        std::printf(" G1 processors  (as handed to the settle: %d, %d of them decommissioned)\n",
                    r.proc_at_build, r.decom_at_build);
        print_tally("handoff", r.g1_handoff);
        print_tally("tick 50", r.g1_t50);
        std::printf(" G2 field income/tick  settle close %10.0f  tick 50 %10.0f  ratio %5.1f%%"
                    "   (context: settle mean %.0f, play 26-50 mean %.0f)\n",
                    r.inc_close, r.inc_t50, r.inc_close > 0 ? pct(r.inc_t50 / r.inc_close) : 0.0,
                    r.inc_settle_mean, r.inc_t26_50_mean);
        std::printf(" G3 firms  handoff %d  alive of those at tick %d: %d (%5.1f%%)  total corps then %d\n",
                    r.firms_handoff, r.last_tick, r.firms_survived,
                    r.firms_handoff ? pct(static_cast<double>(r.firms_survived) / r.firms_handoff) : 0.0,
                    r.firms_end);
        std::printf(" G4 seat %llu \"%s\"  operating net, last %d settle quarters:",
                    static_cast<unsigned long long>(r.seat), r.seat_name.c_str(), k_seat_settle_window);
        double sm = 0;
        for (const double x : r.seat_opnet) { std::printf(" %.1f", x); sm += x; }
        std::printf("  (mean %.1f)\n", r.seat_opnet.empty() ? 0.0 : sm / r.seat_opnet.size());
        if (r.seat_procs.empty()) std::printf("    seat processors: none\n");
        for (const auto& sp : r.seat_procs)
            std::printf("    seat processor %llu %-24s ran %d/%d settle ticks, %s at handoff\n",
                        static_cast<unsigned long long>(sp.id), sp.recipe.c_str(), sp.ran,
                        k_seat_settle_window, sp.state);
        std::printf("    home markets clearing nothing over play ticks 1-%d: %d of %d%s\n",
                    k_idle_market_ticks, r.idle_markets, r.home_markets,
                    r.tap_lost ? "  (UPPER bound: exchange ring overflowed)" : "");
        std::printf("    buildings at tick 50: live %d  under construction %d  decommissioned %d\n",
                    r.b_live, r.b_build, r.b_decom);
        std::fflush(stdout);
    }

    // --- the table and the pool ---
    std::printf("\n seed | built@h run  inp  oth  run%% | built@50 run  inp  oth  run%% |  inc close   inc t50  ratio | firms h  alive  %% | seat opnet(mean8) procs ran | idle mkts | bld live/constr\n");
    proc_tally ph, p50;
    double ic = 0, i50 = 0;
    long long fh = 0, fs = 0;
    int honesty_fail = 0;
    double secs = 0;
    for (const seed_reading& r : rs)
    {
        secs += r.secs;
        if (!r.fail.empty()) ++honesty_fail;
        if (!r.built) { std::printf(" %4u | FAILED: %s\n", r.seed, r.fail.c_str()); continue; }
        ph.add(r.g1_handoff); p50.add(r.g1_t50);
        ic += r.inc_close; i50 += r.inc_t50; fh += r.firms_handoff; fs += r.firms_survived;
        double sm = 0; for (const double x : r.seat_opnet) sm += x;
        int sran = 0; for (const auto& sp : r.seat_procs) if (sp.ran > 0) ++sran;
        const auto oth = [](const proc_tally& t) { return t.built() - t.n[ps_run] - t.n[ps_input]; };
        std::printf(" %4u | %6d %4d %4d %4d %5.1f | %7d %4d %4d %4d %5.1f | %9.0f %9.0f %5.1f%% | %6d %5d %5.1f | %8.1f %3d/%-3d | %3d/%-3d | %4d/%d\n",
                    r.seed, r.g1_handoff.built(), r.g1_handoff.n[ps_run], r.g1_handoff.n[ps_input], oth(r.g1_handoff),
                    pct(r.g1_handoff.share_run()),
                    r.g1_t50.built(), r.g1_t50.n[ps_run], r.g1_t50.n[ps_input], oth(r.g1_t50), pct(r.g1_t50.share_run()),
                    r.inc_close, r.inc_t50, r.inc_close > 0 ? pct(r.inc_t50 / r.inc_close) : 0.0,
                    r.firms_handoff, r.firms_survived,
                    r.firms_handoff ? pct(static_cast<double>(r.firms_survived) / r.firms_handoff) : 0.0,
                    r.seat_opnet.empty() ? 0.0 : sm / r.seat_opnet.size(), sran, static_cast<int>(r.seat_procs.size()),
                    r.idle_markets, r.home_markets, r.b_live, r.b_build);
    }
    const double g1 = ph.share_run();
    const double g2 = ic > 0 ? i50 / ic : 0.0;
    const double g3 = fh > 0 ? static_cast<double>(fs) / static_cast<double>(fh) : 0.0;
    std::printf("\n pooled over %zu seeds (%.0f s)\n", rs.size(), secs);
    print_tally("handoff", ph);
    print_tally("tick 50", p50);
    std::printf(" %s  G1 processors running at handoff  %5.1f%%  (target >= %.0f%%)\n",
                g1 >= k_target_g1_running_at_handoff ? "PASS" : "FAIL", pct(g1), pct(k_target_g1_running_at_handoff));
    std::printf(" %s  G2 field income t%d / settle close  %5.1f%%  (%.0f / %.0f; target >= %.0f%%)\n",
                g2 >= k_target_g2_income_ratio ? "PASS" : "FAIL", k_g1_g2_play_tick, pct(g2), i50, ic,
                pct(k_target_g2_income_ratio));
    std::printf(" %s  G3 firms alive t%d / handoff       %5.1f%%  (%lld / %lld; target >= %.0f%%)\n",
                g3 >= k_target_g3_firms_alive ? "PASS" : "FAIL", ticks, pct(g3), fs, fh, pct(k_target_g3_firms_alive));
    if (honesty_fail)
        std::printf(" HONESTY: %d seed(s) failed an honesty check (see above) — the reading is incomplete\n", honesty_fail);
    std::printf("market_viability: G1 %.1f/70 G2 %.1f/50 G3 %.1f/70\n", pct(g1), pct(g2), pct(g3));
    return honesty_fail ? 1 : 0;
}
