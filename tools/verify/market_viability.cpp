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
//      corporation's return filed that tick, as WINDOW MEANS: play ticks 26-50
//      over the 12 settle ticks. Pooled = sum / sum. TARGET: pooled >= 50%.
//      WINDOWS, NOT SINGLE TICKS (main session, 2026-10-04): the settle's last
//      tick swings 12k-45k across seeds as the opening stock auctions out, so a
//      single-tick ratio passed the broken baseline at 64.8% (seed 12: 204%)
//      while the window form read 27.5% -- the stock auction this gate exists
//      to look past. The single ticks are still printed per seed, as context.
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
//   G5 (BL-1179, the shelf; reported, no target) over play ticks 20-50, read
//      at the end of each tick, against the shelf AS THE PRICE LAW READ IT —
//      snapshotted after the run_economy_step lap (production, construction and
//      dispatch have drawn; the clear has not yet credited a sale), which is
//      the inventory clear_markets' reference prices read:
//        ceiling  of the (market, good) pairs a market's households BID
//                 (household_bid > 0), the share whose price sits at the price
//                 ceiling (>= 95% of ceil_mult x base); of those, the share
//                 priced against a STOCKED shelf (>= 1 unit) and against a shelf
//                 that held the households' whole bid — Ben's measure for k
//                 (MARKETS.md § Price resolution): the smallest k that keeps a
//                 consuming market's price off the ceiling against a stocked
//                 shelf. Above the reservation (reservation_mult x base, where
//                 a processor's draw declines) printed beside it.
//        fill     per household good, the households' fill / bid summed over
//                 every market and the window.
//        centres  heads (k) and live centres at the handoff and the last tick;
//                 centres whose heads rose / fell / held over the play run.
//   W  (the k re-sweep, sprint 49; reported, no target) WATER, in two windows
//      -- early (play ticks 20-50) and late (the last 50 play ticks) -- per tick
//      means over every market whose households bid water, against the shelf
//      as the price law read it (the G5 snapshot):
//        surplus  markets whose shelf held the households' whole bid; the
//                 water units on those shelves; their mean price / base
//        dry      markets whose shelf held < 1 unit; their unfilled bid
//                 (bid - fill); their mean price / base
//        shipped  convoys dispatched this tick carrying water (ids above the
//                 last tick's highest), units, the share a market exported
//                 off its own shelf (corp null), and units bound for a market
//                 that was dry this tick
//      Plus the late window's household fill/bid per good. The hypothesis it
//      tests: at k = 0 a stocked shelf does not lower its price, so the
//      dispatcher's price gate (price_d > price_src x (1 + margin)) sees no
//      gradient between a stocked market and a dry one.
//   L  (BL-1223, the logistics row; reported, no target) read at SEVERAL play
//      ticks -- 10, 25, 50 and the last (those <= --ticks) -- never one, after the
//      run_economy_step lap (production, construction and dispatch have run; the
//      clear has not), the instant water_pair_probe reads. The refusal classes
//      are export_refusal.hpp's (the dispatcher's own rules, in its order), the
//      reader water_pair_probe shares. Per probe tick:
//        shipped  convoys dispatched this tick (ids above the previous tick's
//                 highest), every good, corporations' and markets' own shelf
//                 exports: units; the shelf-export units; and the share of units
//                 bound for a market SHORT of the good (its shelf, at dispatch,
//                 below its last-clear demand for it -- the one tick of want
//                 market_shelf_surplus keeps home) and for a DRY one
//        dry      dry (market, good) pairs, every non-grid good (export_refusal's
//                 dry: the households bid it, the shelf holds < 1 unit), listed
//                 by good; then each dry pair by its BEST class over every
//                 surplus source of that good -- sources under the DISPATCHER's
//                 own test (surplus > 0; water_pair_probe keeps >= 1 unit) --
//                 plus noshelfsurplus (no surplus shelf of the good anywhere),
//                 split on the PRE-STEP pools (last tick's leftover, not this
//                 tick's output): `poolheld` (a corporation's pool on the body
//                 holds >= 1 unit beyond processor_reservation and this tick's
//                 deliveries, outside any standing sell order -- dispatch_convoys'
//                 own surplus) / `ordered` (only under a sell order, which the
//                 corp dispatcher never hauls) / `none`; and `grid` (never cargo)
//        input    every input-starved processor (G1's `input` state, this tick's
//                 report), filed under its scarcest input (limiting_input) at
//                 ITS market: `nomarket` (no tile market: a body pool), `grid`,
//                 `unpriced` (base price 0 there), then -- on the shelf and pool
//                 SNAPSHOTTED after the convoys lap, before the economy step, so
//                 later draws and this tick's shelf export cannot hide stock --
//                 stocked/ceiling (shelf >= 1 unit, price over the BL-1172
//                 fair-price ceiling: shelf_admits false), stocked/thin (pool +
//                 the WHOLE shelf < t_idle of a full run: run_processing's early
//                 idle return even with no contention), stocked/contended (enough
//                 stock, taken by earlier draws or the BL-1209 pro-rata share);
//                 else the best dispatcher class read at dispatch, or
//                 noshelfsurplus/poolheld|none. Per class, how many have the
//                 input priced over the ceiling (a delivery would not unblock
//                 them until the price falls); and how many had the input
//                 shelf-exported off their own market this tick. Pooled by
//                 class and listed by good: BL-1217 (inputs reach processors)'s
//                 diagnosis.
//      Pure: export_refusal.hpp's rule (router caches warmed through a
//      const_cast; nothing the simulation reads is written). --no-logistics
//      drops the row (the R3 check: G1-G4 read identically either way).
//      OVERRIDES for a sweep (default: the shipped registry, untouched):
//        --k X        price_band.shelf_supply_ticks = X
//        --no-spoil   every shelf spoilage rate zero (the pre-BL-1179 shelf)
//
// EXIT. 0 for a completed reading — the targets are reported, not enforced as
// the exit, because the baseline is expected to fail them. 1 only when an
// honesty check fails: a seed that failed to build, no corporations, zero
// processors at the handoff, zero field income at the settle close, no seat,
// or fewer play ticks than the rows need.
//
// Usage (repo root; it loads scripts/*.lua and the seed library by path):
//   build_gen/verify/market_viability.exe [--seeds a,b] [--ticks N] [--k X] [--no-spoil]
// Default seeds: docs/generation/seed_library.json, in library order.
// Build:  bash tools/verify/build_lua_harness.sh market_viability
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "export_refusal.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/resource_names.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
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
#include <tuple>
#include <sstream>
#include <string>
#include <vector>

namespace {

// --- THE TARGETS: Ben's ruling, the sprint 49 form, 2026-10-04 (BL-1184) ----
// Pooled over the seeds read (the 16 curated seeds by default).
constexpr double k_target_g1_running_at_handoff = 0.70; ///< processors running at handoff
constexpr double k_target_g2_income_ratio       = 0.50; ///< field income tick 50 / settle close
constexpr double k_target_g3_firms_alive        = 0.70; ///< firms alive at tick 400 / at handoff
constexpr int    k_g5_idle_window = 100; ///< G5: processors idled up to this play tick are followed

constexpr int k_g1_g2_play_tick     = 50; ///< the "tick 50" reading
constexpr int k_seat_settle_window  = 8;  ///< the seat's last 8 settle quarters
constexpr int k_idle_market_ticks   = 4;  ///< market_census's first play year
constexpr int k_g5_from = 20, k_g5_to = 50; ///< BL-1179: the shelf window (play ticks)
constexpr int k_w_late = 50;                ///< W: the late window, the last N play ticks

// --- sweep overrides (BL-1179): applied to the registry after the build ---
float g_k_override = -1.0f;   ///< < 0: shipped k
bool  g_no_spoil   = false;
bool  g_logistics  = true;    ///< BL-1223: the L row (--no-logistics drops it)
std::vector<int> g_lg_ticks;  ///< L: the probe play ticks (set in main)

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

// --- L: the logistics row (BL-1223) -------------------------------------------
// export_refusal's six dispatcher classes (0..5), then labels this row adds
// OUTSIDE the dispatcher's order (they are not refusals by a rule):
constexpr int x_poolheld  = export_refusal::c_count; ///< no shelf surplus; a corp pool on the body holds one (pre-step)
constexpr int x_none      = x_poolheld + 1;          ///< no shelf surplus and no pool surplus on the body
constexpr int x_ordered   = x_none + 1;              ///< pool surplus exists only under standing sell orders
constexpr int x_grid      = x_ordered + 1;           ///< a grid good: never cargo
constexpr int x_ceiling   = x_grid + 1;              ///< stocked, but priced over the fair-price ceiling
constexpr int x_thin      = x_ceiling + 1;           ///< stocked, but pool + whole shelf < t_idle of a full run
constexpr int x_contended = x_thin + 1;              ///< stocked and enough; other draws / pro-rata took it
constexpr int x_unpriced  = x_contended + 1;         ///< the processor's market has no base price for it
constexpr int x_nomarket  = x_unpriced + 1;          ///< the processor has no tile market (body pool)
constexpr int x_count     = x_nomarket + 1;
const char* const k_x_name[x_count] = {
    "body", "noroute", "gate", "costly", "noroom", "room",
    "noshelfsurplus/poolheld", "noshelfsurplus/none", "noshelfsurplus/ordered", "grid",
    "stocked/ceiling", "stocked/thin", "stocked/contended", "unpriced", "nomarket"};
/// Print orders: furthest from sending first.
const int k_dry_order[]  = {x_none, x_ordered, x_poolheld, 0, 1, 2, 3, 4, 5, x_grid};
const int k_proc_order[] = {x_nomarket, x_grid, x_unpriced, x_ceiling, x_thin, x_contended,
                            x_none, x_ordered, x_poolheld, 0, 1, 2, 3, 4, 5};

struct lg_reading
{
    int    reads = 0;
    double ship_n = 0, ship_u = 0, ship_shelf_u = 0, ship_short_u = 0, ship_dry_u = 0;
    long   consuming = 0, dry = 0;
    std::array<long, resource_count> dry_good{};
    long   dry_best[x_count] = {};
    long   starved = 0, unread = 0;
    long   export_away = 0;      ///< starved whose input the market shelf-exported this tick
    double stocked_shelf = 0;    ///< pre-step shelf units summed over the stocked/* processors
    long   starved_cls[x_count] = {};
    long   starved_over_ceil[x_count] = {}; ///< of each class: the input priced over the ceiling
    std::array<std::array<long, x_count>, resource_count> starved_good{};
    void add(const lg_reading& o)
    {
        reads += o.reads; ship_n += o.ship_n; ship_u += o.ship_u; ship_shelf_u += o.ship_shelf_u;
        ship_short_u += o.ship_short_u; ship_dry_u += o.ship_dry_u;
        consuming += o.consuming; dry += o.dry; starved += o.starved; unread += o.unread;
        export_away += o.export_away; stocked_shelf += o.stocked_shelf;
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            dry_good[g] += o.dry_good[g];
            for (int c = 0; c < x_count; ++c) starved_good[g][c] += o.starved_good[g][c];
        }
        for (int c = 0; c < x_count; ++c)
        {
            dry_best[c] += o.dry_best[c];
            starved_cls[c] += o.starved_cls[c];
            starved_over_ceil[c] += o.starved_over_ceil[c];
        }
    }
};

// --- one seed ----------------------------------------------------------------------

using shelf_snap_fwd = std::map<entity_id, std::array<float, resource_count>>;

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
    // G5 (BL-1187 review): do idled processors ever come back?
    int g5_idled = 0, g5_back = 0, g5_removed = 0;  ///< idled at econ steps <= settle + 100
    int g5_idle_events = 0, g5_resume_events = 0;   ///< every play-tick transition, ticks 1..end
    // G5 (BL-1179)
    long long g5_pairs = 0, g5_ceil = 0, g5_ceil_stocked = 0, g5_ceil_covered = 0, g5_over_res = 0, g5_stocked = 0;
    std::array<double, resource_count> g5_bid{}, g5_fill{};
    long long heads_handoff = 0, heads_end = 0;
    int centres_handoff = 0, centres_end = 0, centres_grew = 0, centres_fell = 0, centres_held = 0;
    // W (the k re-sweep): water, early and late windows
    struct water_win
    {
        int    ticks = 0;
        double cons = 0, surplus_mkts = 0, surplus_units = 0, dry_mkts = 0, dry_unmet = 0;
        double shelf_all = 0;                       ///< water on every market's shelf
        double p_surplus = 0, n_surplus = 0, p_dry = 0, n_dry = 0;
        double ship_n = 0, ship_qty = 0, ship_shelf_qty = 0, ship_to_dry_qty = 0;
        void add(const water_win& o)
        {
            ticks += o.ticks; cons += o.cons; surplus_mkts += o.surplus_mkts; surplus_units += o.surplus_units;
            dry_mkts += o.dry_mkts; dry_unmet += o.dry_unmet; shelf_all += o.shelf_all;
            p_surplus += o.p_surplus; n_surplus += o.n_surplus; p_dry += o.p_dry; n_dry += o.n_dry;
            ship_n += o.ship_n; ship_qty += o.ship_qty; ship_shelf_qty += o.ship_shelf_qty;
            ship_to_dry_qty += o.ship_to_dry_qty;
        }
    };
    water_win w_early, w_late;
    std::array<double, resource_count> late_bid{}, late_fill{};
    double secs = 0.0;
    std::vector<lg_reading> lg; ///< L (BL-1223): one per g_lg_ticks entry
};

/// W: one tick's water read. `max_convoy_id` is the highest convoy id seen
/// before this tick; every convoy above it was dispatched this tick (ids are
/// allocated monotonically and never reused, world::allocate_convoy_id).
void read_water(const world& w, const shelf_snap_fwd& snap, seed_reading::water_win& ww, std::uint32_t& max_convoy_id)
{
    const std::size_t g = static_cast<std::size_t>(resource_type::water);
    ++ww.ticks;
    std::set<entity_id> dry;
    std::vector<entity_id> mids;
    for (const auto& kv : w.markets) mids.push_back(kv.first);
    std::sort(mids.begin(), mids.end());
    for (const entity_id m : mids)
    {
        const market_component& mc = w.markets.at(m);
        const auto si = snap.find(m);
        const float shelf = si == snap.end() ? 0.0f : si->second[g];
        ww.shelf_all += shelf;
        if (!(mc.household_bid[g] > 0.0f) || !(mc.base_price[g] > 0.0f)) continue;
        ww.cons += 1;
        const double pr = static_cast<double>(mc.price[g] > 0.0f ? mc.price[g] : mc.base_price[g]) / mc.base_price[g];
        if (shelf >= mc.household_bid[g])
        {
            ww.surplus_mkts += 1; ww.surplus_units += shelf; ww.p_surplus += pr; ww.n_surplus += 1;
        }
        else if (shelf < 1.0f)
        {
            ww.dry_mkts += 1; ww.dry_unmet += std::max(0.0f, mc.household_bid[g] - mc.household_fill[g]);
            ww.p_dry += pr; ww.n_dry += 1;
            dry.insert(m);
        }
    }
    std::uint32_t top = max_convoy_id;
    for (const convoy_component& c : w.convoys)
    {
        if (c.id <= max_convoy_id) continue;
        top = std::max(top, c.id);
        if (c.cargo_resource != resource_type::water) continue;
        ww.ship_n += 1; ww.ship_qty += c.cargo_qty;
        if (c.corp == null_entity) ww.ship_shelf_qty += c.cargo_qty;
        if (dry.count(c.dest_market)) ww.ship_to_dry_qty += c.cargo_qty;
    }
    max_convoy_id = top;
}

void print_water(const char* label, const seed_reading::water_win& ww)
{
    const double t = ww.ticks > 0 ? ww.ticks : 1.0;
    std::printf("    W %-5s (%d ticks, per tick): consuming mkts %.1f | surplus mkts %.1f holding %.0f u (p/base %.2f)"
                " | dry mkts %.1f unmet %.0f u (p/base %.2f) | all shelves %.0f u | shipped %.1f convoys %.0f u"
                " (shelf export %.0f u, to a dry mkt %.0f u)\n",
                label, ww.ticks, ww.cons / t, ww.surplus_mkts / t, ww.surplus_units / t,
                ww.n_surplus > 0 ? ww.p_surplus / ww.n_surplus : 0.0, ww.dry_mkts / t, ww.dry_unmet / t,
                ww.n_dry > 0 ? ww.p_dry / ww.n_dry : 0.0, ww.shelf_all / t, ww.ship_n / t, ww.ship_qty / t,
                ww.ship_shelf_qty / t, ww.ship_to_dry_qty / t);
}

/// G5's per-processor record: the decommission flag last seen, the first econ
/// step it was seen idled (-1 never), and whether it was later seen running.
struct g5_rec { bool decom = false; int first_idle = -1; bool back = false; bool gone = false; };

/// Read every processor after one econ step (`step` = 0-based econ step index).
void g5_track(const world& w, std::map<entity_id, g5_rec>& m, int step, bool play, seed_reading& r)
{
    for (auto& [bid, rec] : m)
        if (!rec.gone && !w.buildings.count(bid)) rec.gone = true;
    std::vector<entity_id> ids;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility) ids.push_back(bid);
    std::sort(ids.begin(), ids.end());
    for (const entity_id bid : ids)
    {
        const bool d = w.buildings.at(bid).decommissioned;
        auto [it, fresh] = m.try_emplace(bid);
        g5_rec& rec = it->second;
        if (!fresh && d != rec.decom && play)
            ++(d ? r.g5_idle_events : r.g5_resume_events);
        if (d && rec.first_idle < 0) rec.first_idle = step;
        if (!d && rec.first_idle >= 0 && (fresh || rec.decom)) rec.back = true;
        rec.decom = d;
    }
}


/// BL-1179 G5: the shelf as the price law reads it — every market's inventory
/// after the run_economy_step lap, before clear_markets credits a sale.
struct shelf_snap
{
    int lap = -1;
    std::map<entity_id, std::array<float, resource_count>> inv;
};

void snap_after_lap(const world& w, int lap, void* ctx)
{
    auto* s = static_cast<shelf_snap*>(ctx);
    if (lap != s->lap) return;
    s->inv.clear();
    for (const auto& [mid, mc] : w.markets) s->inv[mid] = mc.inventory;
}

/// BL-1179 G5: one tick's read of every consuming (market, good) pair.
void read_shelf(const world& w, const recipe_registry& reg, const shelf_snap& snap, seed_reading& r)
{
    const float ceil_mult = reg.price_band().ceil_mult;
    const float res_mult  = reg.price_band().reservation_mult;
    std::vector<entity_id> mids;
    for (const auto& kv : w.markets) mids.push_back(kv.first);
    std::sort(mids.begin(), mids.end());
    for (const entity_id m : mids)
    {
        const market_component& mc = w.markets.at(m);
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            if (!(mc.household_bid[g] > 0.0f) || !(mc.base_price[g] > 0.0f)) continue;
            r.g5_bid[g]  += mc.household_bid[g];
            r.g5_fill[g] += mc.household_fill[g];
            ++r.g5_pairs;
            const auto si = snap.inv.find(m);
            const float shelf = si == snap.inv.end() ? 0.0f : si->second[g];
            const bool stocked = shelf >= 1.0f;
            const bool covered = stocked && shelf >= mc.household_bid[g];
            if (stocked) ++r.g5_stocked;
            if (mc.price[g] >= 0.95f * ceil_mult * mc.base_price[g])
            {
                ++r.g5_ceil;
                if (stocked) ++r.g5_ceil_stocked;
                if (covered) ++r.g5_ceil_covered;
            }
            if (res_mult > 0.0f && mc.price[g] > res_mult * mc.base_price[g]) ++r.g5_over_res;
        }
    }
}

/// One live processor as it stood BEFORE the economy step (after the convoys
/// lap): its market, its owner's pool there and the market's shelf, and
/// whether the fair-price ceiling admitted each good. Price is written only at
/// the end of clear_markets, so the pre-step price is the price the draw saw.
struct proc_snap
{
    entity_id m = null_entity;
    std::array<float, resource_count> shelf{}, pool{};
    std::array<bool, resource_count>  admits{};
};

struct lg_probe
{
    int                    lap_pre = -1; ///< the convoys lap: the pre-step snapshot
    int                    lap = -1;     ///< the run_economy_step lap: the dispatch read
    bool                   armed = false;
    const recipe_registry* reg = nullptr;
    std::uint32_t          max_id = 0;   ///< highest convoy id before this tick
    lg_reading*            cur = nullptr;
    std::map<entity_id, proc_snap> snap; ///< processor -> pre-step state
    /// (processor's market, input good) -> best dispatcher class at the
    /// dispatch read (x_poolheld / x_none where no shelf surplus exists).
    std::map<std::pair<entity_id, std::size_t>, int> proc_best;
    std::set<std::pair<entity_id, std::size_t>> exported; ///< (src market, good) shelf-exported this tick
    /// (body, good) -> x_poolheld / x_ordered / absent (x_none), from the PRE-STEP
    /// pools: last tick's post-clear leftover, not this tick's fresh output.
    std::map<std::pair<entity_id, std::size_t>, int> pool_status;
};

std::vector<entity_id> sorted_processors(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility) ids.push_back(bid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool live_processor(const building_component& b)
{
    return !b.decommissioned && b.ticks_remaining <= 0;
}

/// The pre-step snapshot (after the convoys lap).
void lg_snapshot(const world& w, lg_probe& p)
{
    const recipe_registry& reg = *p.reg;
    const float res_mult = reg.price_band().reservation_mult;
    std::map<entity_id, entity_id> owner;
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id a : cc.assets) owner.emplace(a, cid);
    p.snap.clear();
    for (const entity_id bid : sorted_processors(w))
    {
        const building_component& b = w.buildings.at(bid);
        if (!live_processor(b)) continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (rc == nullptr) continue;
        const entity_id m = market_for_tile(w, b.tile);
        const auto mi = w.markets.find(m);
        if (mi == w.markets.end()) continue; // nomarket: labelled after the tick
        proc_snap s;
        s.m = m;
        const auto oi = owner.find(bid);
        const stockpile_component* pool = oi != owner.end() ? w.find_pool(oi->second, m) : nullptr;
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            if (!(rc->inputs[g] > 0.0f)) continue;
            s.shelf[g]  = mi->second.inventory[g];
            s.pool[g]   = pool ? pool->quantities[g] : 0.0f;
            s.admits[g] = shelf_admits(mi->second, g, res_mult, /*off_buys=*/true);
        }
        p.snap.emplace(bid, s);
    }
}

/// noshelfsurplus split, read PRE-STEP (after the convoys lap): per (body, good),
/// does some corporation's pool on the body hold >= 1 unit the corporations'
/// dispatcher would treat as shippable -- pool less processor_reservation less
/// this tick's deliveries (dispatch_arrived), dispatch_convoys' own surplus
/// (supply_system.cpp) -- outside a standing sell order (`poolheld`), or only
/// under one (`ordered`: dispatch_convoys never hauls an order-controlled
/// (corp, body, good))? Absent = `none`.
void snapshot_pool_status(const world& w, const recipe_registry& reg, lg_probe& p)
{
    p.pool_status.clear();
    std::set<std::tuple<entity_id, entity_id, std::size_t>> ordered;
    for (const sell_order& o : w.sell_orders)
        ordered.insert({o.corp, o.body, static_cast<std::size_t>(o.resource)});
    const grid_goods_params& grid = reg.grid_goods();
    for (const auto& [key, pool] : w.corp_market_pools) // std::map: sorted
    {
        if (!w.corporations.count(key.first)) continue;
        const entity_id body = pool_key_body(w, key.second);
        if (body == null_entity) continue;
        bool any = false;
        for (std::size_t g = 0; g < resource_count && !any; ++g) any = pool.quantities[g] >= 1.0f;
        if (!any) continue;
        const std::array<float, resource_count> res = processor_reservation(w, reg, key.first, key.second);
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            if (grid.grid(g) || !(pool.quantities[g] >= 1.0f)) continue;
            const float surplus = pool.quantities[g] - res[g] - dispatch_arrived(w, key.first, key.second, g);
            if (!(surplus >= 1.0f)) continue;
            int& st = p.pool_status.try_emplace(std::make_pair(body, g), x_ordered).first->second;
            if (!ordered.count({key.first, body, g})) st = x_poolheld;
        }
    }
}

void lg_after_lap(const world& cw, int lap, void* vctx)
{
    using namespace export_refusal;
    auto* p = static_cast<lg_probe*>(vctx);
    if (!p->armed) return;
    if (lap == p->lap_pre) { lg_snapshot(cw, *p); snapshot_pool_status(cw, *p->reg, *p); return; }
    if (lap != p->lap) return;
    world& w = const_cast<world&>(cw); // router path caches only (export_refusal.hpp)
    const recipe_registry& reg = *p->reg;
    lg_reading& r = *p->cur;
    ++r.reads;
    context x(w, reg);

    // shipped this tick
    std::uint32_t top = p->max_id;
    p->exported.clear();
    for (const convoy_component& c : w.convoys)
    {
        if (c.id <= p->max_id) continue;
        top = std::max(top, c.id);
        const std::size_t g = static_cast<std::size_t>(c.cargo_resource);
        r.ship_n += 1; r.ship_u += c.cargo_qty;
        if (c.corp == null_entity)
        {
            r.ship_shelf_u += c.cargo_qty;
            p->exported.insert(std::make_pair(c.source_market, g));
        }
        const auto di = w.markets.find(c.dest_market);
        if (di == w.markets.end() || g >= resource_count) continue;
        const market_component& dm = di->second;
        if (dm.demand[g] > 0.0f && dm.inventory[g] < dm.demand[g]) r.ship_short_u += c.cargo_qty;
        if (dry(dm, g)) r.ship_dry_u += c.cargo_qty;
    }
    p->max_id = top;

    // the dispatcher's own surplus test (> 0), not water_pair_probe's 1 unit
    std::vector<good_markets> scans(resource_count);
    const auto no_shelf_class = [&](entity_id d, std::size_t g) {
        const auto it = p->pool_status.find(std::make_pair(w.markets.at(d).body, g));
        return it == p->pool_status.end() ? x_none : it->second;
    };

    // dry (market, good) pairs by best class
    for (std::size_t g = 0; g < resource_count; ++g)
    {
        scans[g] = scan_good(x, g, surplus_rule::dispatcher);
        const good_markets& gm = scans[g];
        r.consuming += gm.consuming;
        r.dry += static_cast<long>(gm.dry.size());
        r.dry_good[g] += static_cast<long>(gm.dry.size());
        const bool grid = reg.grid_goods().grid(g);
        for (const entity_id d : gm.dry)
        {
            if (grid) { ++r.dry_best[x_grid]; continue; }
            const int b = classify_destination(x, d, g, gm.sur).best;
            ++r.dry_best[b < 0 ? no_shelf_class(d, g) : b];
        }
    }

    // every snapped processor's every input: the best dispatcher class at its
    // market (the stocked / grid / unpriced cuts are made after the tick)
    p->proc_best.clear();
    for (const auto& [bid, s] : p->snap)
    {
        const recipe* rc = reg.get_recipe(w.buildings.at(bid).recipe);
        if (rc == nullptr) continue;
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            if (!(rc->inputs[g] > 0.0f) || reg.grid_goods().grid(g)) continue;
            const auto key = std::make_pair(s.m, g);
            if (p->proc_best.count(key)) continue;
            const int b = classify_destination(x, s.m, g, scans[g].sur).best;
            p->proc_best.emplace(key, b < 0 ? no_shelf_class(s.m, g) : b);
        }
    }
}

/// After the tick: each input-starved processor (G1's `input`) filed under its
/// scarcest input's class at its market.
void lg_after_tick(const world& w, const economy_report& rep, const recipe_registry& reg, lg_probe& p)
{
    lg_reading& r = *p.cur;
    const float base_rate = reg.economics(building_type::processing_facility).base_rate;
    for (const entity_id bid : sorted_processors(w))
    {
        const building_component& b = w.buildings.at(bid);
        const building_report* row = row_of(rep, bid);
        if (classify(b, row, reg) != ps_input) continue;
        ++r.starved;
        const std::size_t g = static_cast<std::size_t>(row->limiting_input);
        if (g >= resource_count) { ++r.unread; continue; }
        const entity_id m = market_for_tile(w, b.tile);
        int c = -1;
        bool over = false;
        if (w.markets.find(m) == w.markets.end()) c = x_nomarket;
        else
        {
            const auto si = p.snap.find(bid);
            if (si == p.snap.end()) { ++r.unread; continue; }
            const proc_snap& s = si->second;
            const market_component& mc = w.markets.at(m);
            over = mc.base_price[g] > 0.0f && !s.admits[g];
            if (reg.grid_goods().grid(g)) c = x_grid;
            else if (!(mc.base_price[g] > 0.0f)) c = x_unpriced;
            else if (s.shelf[g] >= 1.0f)
            {
                r.stocked_shelf += s.shelf[g];
                // run_processing's early idle return: coverage of a full run
                // with the WHOLE shelf (no pro-rata) still under t_idle. Tested
                // BEFORE the ceiling (cold check, 2026-10-07): a thin shelf idles
                // the processor at any price, so counting it as a ceiling lock
                // overstated what lowering the price would recover. `over` still
                // reports the price beside it.
                const recipe* rc = reg.get_recipe(b.recipe);
                const float wt = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
                const float need = (rc ? rc->inputs[g] : 0.0f) * base_rate * row->effective_workforce
                                 * wt * building_supply_scalar(b);
                const bool thin = need > 0.0f && (s.pool[g] + s.shelf[g]) / need < reg.t_idle();
                if (thin) c = x_thin;
                else if (!s.admits[g]) c = x_ceiling;
                else c = x_contended;
            }
            else
            {
                const auto it = p.proc_best.find(std::make_pair(m, g));
                if (it == p.proc_best.end()) { ++r.unread; continue; }
                c = it->second;
            }
            if (p.exported.count(std::make_pair(m, g))) ++r.export_away;
        }
        ++r.starved_cls[c];
        if (over) ++r.starved_over_ceil[c];
        ++r.starved_good[g][c];
    }
    p.snap.clear();
    p.proc_best.clear();
    p.exported.clear();
    p.pool_status.clear();
}

/// Two after_lap readers on the one hook slot: the existing one first, untouched.
struct hook_pair
{
    void (*a)(const world&, int, void*) = nullptr;
    void* actx = nullptr;
    lg_probe* lg = nullptr;
};

void pair_after_lap(const world& w, int lap, void* ctx)
{
    auto* h = static_cast<hook_pair*>(ctx);
    if (h->a) h->a(w, lap, h->actx);
    lg_after_lap(w, lap, h->lg);
}

std::string good_name(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

void print_lg(const char* label, const lg_reading& r, int top_goods)
{
    const double n = r.reads > 0 ? r.reads : 1.0;
    const double u = r.ship_u > 0 ? r.ship_u : 1.0;
    std::printf("    L %-6s (%d read%s, per read): shipped %.1f convoys %.0f u (shelf export %.0f u)"
                " -> to a SHORT mkt %.1f%%, to a DRY mkt %.1f%%\n",
                label, r.reads, r.reads == 1 ? "" : "s", r.ship_n / n, r.ship_u / n, r.ship_shelf_u / n,
                100.0 * r.ship_short_u / u, 100.0 * r.ship_dry_u / u);
    std::printf("      dry (mkt, good) %.1f of %.1f consuming:", r.dry / n, r.consuming / n);
    std::vector<std::pair<long, std::size_t>> dg;
    for (std::size_t g = 0; g < resource_count; ++g)
        if (r.dry_good[g] > 0) dg.push_back({-r.dry_good[g], g});
    std::sort(dg.begin(), dg.end());
    for (std::size_t i = 0; i < dg.size() && static_cast<int>(i) < top_goods; ++i)
        std::printf(" %s %.1f", good_name(dg[i].second).c_str(), -dg[i].first / n);
    if (static_cast<int>(dg.size()) > top_goods) std::printf(" (+%zu goods)", dg.size() - top_goods);
    std::printf("\n      dry by BEST class:");
    const double dd = r.dry > 0 ? r.dry : 1.0;
    for (const int c : k_dry_order)
        std::printf("  %s %.1f (%.0f%%)", k_x_name[c], r.dry_best[c] / n, 100.0 * r.dry_best[c] / dd);
    const double sd = r.starved > 0 ? r.starved : 1.0;
    std::printf("\n      INPUT-STARVED processors %.1f; scarcest input's class at its market:", r.starved / n);
    long stocked = 0, over = 0;
    for (const int c : k_proc_order)
    {
        over += r.starved_over_ceil[c];
        if (r.starved_cls[c] == 0) continue; // nonzero classes only
        std::printf("  %s %.1f (%.0f%%", k_x_name[c], r.starved_cls[c] / n, 100.0 * r.starved_cls[c] / sd);
        if (r.starved_over_ceil[c] > 0) std::printf("; %.1f over ceiling", r.starved_over_ceil[c] / n);
        std::printf(")");
    }
    stocked = r.starved_cls[x_ceiling] + r.starved_cls[x_thin] + r.starved_cls[x_contended];
    std::printf("\n        stocked/* %.1f (pre-step shelf mean %.1f u) | input priced over the ceiling %.1f"
                " | input shelf-exported from its market this tick %.1f",
                stocked / n, stocked ? r.stocked_shelf / stocked : 0.0, over / n, r.export_away / n);
    if (r.unread) std::printf("  UNREAD %ld", r.unread);
    std::printf("\n      starved by input:");
    std::vector<std::pair<long, std::size_t>> sg;
    for (std::size_t g = 0; g < resource_count; ++g)
    {
        long s = 0;
        for (int c = 0; c < x_count; ++c) s += r.starved_good[g][c];
        if (s > 0) sg.push_back({-s, g});
    }
    std::sort(sg.begin(), sg.end());
    if (sg.empty()) std::printf(" none");
    for (std::size_t i = 0; i < sg.size() && static_cast<int>(i) < top_goods; ++i)
    {
        const std::size_t g = sg[i].second;
        std::printf(" %s %.1f [", good_name(g).c_str(), -sg[i].first / n);
        bool first = true;
        for (const int c : k_proc_order)
            if (r.starved_good[g][c] > 0)
            {
                std::printf("%s%s %.1f", first ? "" : ", ", k_x_name[c], r.starved_good[g][c] / n);
                first = false;
            }
        std::printf("]");
    }
    if (static_cast<int>(sg.size()) > top_goods) std::printf(" (+%zu inputs)", sg.size() - top_goods);
    std::printf("\n");
}

std::map<entity_id, int> centre_heads(const world& w)
{
    std::map<entity_id, int> out;
    for (const auto& [cid, pc] : w.population_centres)
        if (!pc.razed) out[cid] = pc.population;
    return out;
}

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
    if (g_k_override >= 0.0f)
    {
        price_band_params pb = start->reg.price_band();
        pb.shelf_supply_ticks = g_k_override;
        start->reg.set_price_band(pb);
    }
    if (g_no_spoil)
        start->reg.set_shelf_spoilage({});
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
    std::map<entity_id, g5_rec> g5;
    g5_track(w, g5, 0, false, r); // the world as handed to the settle
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        settle_tick_result res = run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true);
        r.inc_settle_mean += field_income(w) / k_campaign_settle_ticks;
        g5_track(w, g5, step, false, r);
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
    const std::map<entity_id, int> heads0 = centre_heads(w);
    r.centres_handoff = static_cast<int>(heads0.size());
    for (const auto& [cid, h] : heads0) { (void)cid; r.heads_handoff += h; }

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
    shelf_snap snap;
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
        if (std::strcmp(k_campaign_settle_lap_names[i], "run_economy_step") == 0) snap.lap = i;
    exchange_tap tap;
    tap.clear_lap  = clear_lap_index();
    tap.last_total = w.exchanges.total;
    constexpr int k_econ_tick_days = 90; // sim_loop::econ_tick_days (harness_params.hpp)
    std::uint32_t max_convoy_id = 0;
    for (const convoy_component& c : w.convoys) max_convoy_id = std::max(max_convoy_id, c.id);
    // L (BL-1223): its own convoy-id watermark, so the W row's is untouched.
    lg_probe lg;
    lg.reg = &reg;
    lg.lap = snap.lap; // the run_economy_step lap
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
        if (std::strcmp(k_campaign_settle_lap_names[i], "convoys") == 0) lg.lap_pre = i;
    lg.max_id = max_convoy_id;
    if (g_logistics) r.lg.assign(g_lg_ticks.size(), lg_reading{});
    for (int k = 1; k <= ticks; ++k)
    {
        const int day = k * k_econ_tick_days;
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = day;
        settle_tick_hooks hooks;
        if (k <= k_idle_market_ticks) { hooks.after_lap = tap_after_lap; hooks.ctx = &tap; }
        else if ((k >= k_g5_from && k <= k_g5_to) || k > ticks - k_w_late)
        { hooks.after_lap = snap_after_lap; hooks.ctx = &snap; }
        static_assert(k_idle_market_ticks < k_g5_from, "the G5 shelf hook and the exchange tap share one slot");
        lg.armed = false;
        hook_pair pair;
        if (g_logistics)
            for (std::size_t i = 0; i < g_lg_ticks.size(); ++i)
                if (g_lg_ticks[i] == k) { lg.armed = true; lg.cur = &r.lg[i]; }
        if (lg.armed)
        {
            pair.a = hooks.after_lap; pair.actx = hooks.ctx; pair.lg = &lg;
            hooks.after_lap = pair_after_lap; hooks.ctx = &pair;
        }
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day,
                                                 /*spectating=*/false, &hooks);
        if (lg.armed) lg_after_tick(w, res.report, reg, lg);
        for (const convoy_component& c : w.convoys) lg.max_id = std::max(lg.max_id, c.id);
        if (k == k_idle_market_ticks)
        {
            for (const auto& [mid, mc] : w.markets)
                if (mc.body == w.home_body && !tap.active.count(mid)) ++r.idle_markets;
            r.tap_lost = tap.lost;
        }
        g5_track(w, g5, k_campaign_settle_ticks + (k - 1), true, r);
        if (k <= k_g1_g2_play_tick) remember_rows(w, res.report, reg, last_row);
        if (k >= k_g5_from && k <= k_g5_to) read_shelf(w, reg, snap, r);
        if (k >= k_g5_from && k <= k_g5_to) read_water(w, snap.inv, r.w_early, max_convoy_id);
        else if (k > ticks - k_w_late && k > k_g5_to) read_water(w, snap.inv, r.w_late, max_convoy_id);
        else for (const convoy_component& c : w.convoys) max_convoy_id = std::max(max_convoy_id, c.id);
        if (k > ticks - k_w_late)
            for (const auto& [mid, mc] : w.markets)
                for (std::size_t g = 0; g < resource_count; ++g)
                    if (mc.household_bid[g] > 0.0f) { r.late_bid[g] += mc.household_bid[g]; r.late_fill[g] += mc.household_fill[g]; }
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
    for (const auto& [bid, rec] : g5)
    {
        (void)bid;
        if (rec.first_idle < 0 || rec.first_idle > k_campaign_settle_ticks + k_g5_idle_window - 1)
            continue;
        ++r.g5_idled;
        if (rec.back) ++r.g5_back;
        else if (rec.gone) ++r.g5_removed;
    }
    for (const entity_id c : cohort)
        if (w.corporations.count(c)) ++r.firms_survived;
    r.firms_end = static_cast<int>(w.corporations.size());
    {
        const std::map<entity_id, int> heads1 = centre_heads(w);
        r.centres_end = static_cast<int>(heads1.size());
        for (const auto& [cid, h] : heads1)
        {
            r.heads_end += h;
            const auto it = heads0.find(cid);
            if (it == heads0.end()) continue;
            if (h > it->second) ++r.centres_grew;
            else if (h < it->second) ++r.centres_fell;
            else ++r.centres_held;
        }
    }
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
        else if (!std::strcmp(argv[i], "--k") && i + 1 < argc) g_k_override = static_cast<float>(std::atof(argv[++i]));
        else if (!std::strcmp(argv[i], "--no-spoil")) g_no_spoil = true;
        else if (!std::strcmp(argv[i], "--no-logistics")) g_logistics = false;
        else { std::fprintf(stderr, "usage: market_viability [--seeds a,b] [--ticks N] [--k X] [--no-spoil] [--no-logistics]\n"); return 2; }
    }
    // L (BL-1223): several play ticks, never one -- 10, 25, 50 and the last.
    for (const int t : {10, 25, k_g1_g2_play_tick, ticks})
        if (t >= 1 && t <= ticks && std::find(g_lg_ticks.begin(), g_lg_ticks.end(), t) == g_lg_ticks.end())
            g_lg_ticks.push_back(t);
    std::sort(g_lg_ticks.begin(), g_lg_ticks.end());
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
    if (g_k_override >= 0.0f || g_no_spoil)
        std::printf("OVERRIDES: %s%s\n",
                    g_k_override >= 0.0f ? ("shelf_supply_ticks = " + std::to_string(g_k_override) + " ").c_str() : "",
                    g_no_spoil ? "shelf spoilage OFF" : "");
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
        std::printf(" G5 processors idled in the settle or play ticks 1-%d: %d; came back by tick %d: %d (%.1f%%);"
                    " removed while idled: %d | play transitions: idled %d, resumed %d\n",
                    k_g5_idle_window, r.g5_idled, r.last_tick, r.g5_back,
                    r.g5_idled ? pct(static_cast<double>(r.g5_back) / r.g5_idled) : 0.0, r.g5_removed,
                    r.g5_idle_events, r.g5_resume_events);
        const double pairs = r.g5_pairs > 0 ? static_cast<double>(r.g5_pairs) : 1.0;
        std::printf(" G5 shelf, ticks %d-%d: consuming pairs %lld  at ceiling %5.1f%%  at ceiling against a stocked shelf %5.1f%%"
                    " (a shelf holding the whole bid %5.1f%%)  over reservation %5.1f%%  priced against stock %5.1f%%\n",
                    k_g5_from, k_g5_to, r.g5_pairs, pct(r.g5_ceil / pairs), pct(r.g5_ceil_stocked / pairs),
                    pct(r.g5_ceil_covered / pairs), pct(r.g5_over_res / pairs), pct(r.g5_stocked / pairs));
        std::printf("    household fill/bid:");
        for (std::size_t g = 0; g < resource_count; ++g)
            if (r.g5_bid[g] > 0.0)
                std::printf(" %s %.0f/%.0f (%.0f%%)", resource_names::name_of(static_cast<resource_type>(g)).c_str(),
                            r.g5_fill[g], r.g5_bid[g], pct(r.g5_fill[g] / r.g5_bid[g]));
        std::printf("\n    centres: handoff %d (%lldk heads) -> tick %d %d (%lldk heads); grew %d fell %d held %d\n",
                    r.centres_handoff, r.heads_handoff, r.last_tick, r.centres_end, r.heads_end,
                    r.centres_grew, r.centres_fell, r.centres_held);
        print_water("early", r.w_early);
        print_water("late", r.w_late);
        std::printf("    late household fill/bid (last %d ticks):", k_w_late);
        for (std::size_t g = 0; g < resource_count; ++g)
            if (r.late_bid[g] > 0.0)
                std::printf(" %s %.0f%%", resource_names::name_of(static_cast<resource_type>(g)).c_str(),
                            pct(r.late_fill[g] / r.late_bid[g]));
        std::printf("\n");
        if (g_logistics && !r.lg.empty())
        {
            std::printf(" L logistics (reported, no target):\n");
            lg_reading all;
            for (std::size_t i = 0; i < r.lg.size(); ++i)
            {
                const std::string lbl = "t" + std::to_string(g_lg_ticks[i]);
                print_lg(lbl.c_str(), r.lg[i], 6);
                all.add(r.lg[i]);
            }
            print_lg("all", all, 8);
        }
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
        ic += r.inc_settle_mean; i50 += r.inc_t26_50_mean; fh += r.firms_handoff; fs += r.firms_survived;
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
    std::printf(" %s  G2 field income play 26-%d mean / settle mean  %5.1f%%  (%.0f / %.0f; target >= %.0f%%)\n",
                g2 >= k_target_g2_income_ratio ? "PASS" : "FAIL", k_g1_g2_play_tick, pct(g2), i50, ic,
                pct(k_target_g2_income_ratio));
    std::printf(" %s  G3 firms alive t%d / handoff       %5.1f%%  (%lld / %lld; target >= %.0f%%)\n",
                g3 >= k_target_g3_firms_alive ? "PASS" : "FAIL", ticks, pct(g3), fs, fh, pct(k_target_g3_firms_alive));
    if (honesty_fail)
        std::printf(" HONESTY: %d seed(s) failed an honesty check (see above) — the reading is incomplete\n", honesty_fail);
    {
        long long gi = 0, gb = 0, ge = 0, gr = 0;
        for (const seed_reading& r : rs) { gi += r.g5_idled; gb += r.g5_back; ge += r.g5_idle_events; gr += r.g5_resume_events; }
        std::printf(" G5 (reported) idled processors that came back: %lld / %lld (%.1f%%) | play transitions idled %lld resumed %lld\n",
                    gb, gi, gi ? pct(static_cast<double>(gb) / gi) : 0.0, ge, gr);
    }
    {
        long long pr = 0, pc = 0, pcs = 0, pcc = 0, pst = 0, po = 0; long long h0 = 0, h1 = 0; int gr = 0, fe = 0;
        std::array<double, resource_count> bid{}, fill{};
        for (const seed_reading& r : rs)
        {
            pr += r.g5_pairs; pc += r.g5_ceil; pcs += r.g5_ceil_stocked; pcc += r.g5_ceil_covered;
            pst += r.g5_stocked; po += r.g5_over_res;
            h0 += r.heads_handoff; h1 += r.heads_end; gr += r.centres_grew; fe += r.centres_fell;
            for (std::size_t g = 0; g < resource_count; ++g) { bid[g] += r.g5_bid[g]; fill[g] += r.g5_fill[g]; }
        }
        const double d = pr > 0 ? static_cast<double>(pr) : 1.0;
        std::printf(" G5 shelf t%d-%d pooled: at ceiling %5.1f%%  at ceiling+stocked %5.1f%%  at ceiling+whole bid %5.1f%%"
                    "  over reservation %5.1f%%  stocked %5.1f%%  | heads %lldk -> %lldk, centres grew %d fell %d\n",
                    k_g5_from, k_g5_to, pct(pc / d), pct(pcs / d), pct(pcc / d), pct(po / d), pct(pst / d), h0, h1, gr, fe);
        std::printf("    pooled household fill/bid:");
        for (std::size_t g = 0; g < resource_count; ++g)
            if (bid[g] > 0.0)
                std::printf(" %s %.0f%%", resource_names::name_of(static_cast<resource_type>(g)).c_str(), pct(fill[g] / bid[g]));
        std::printf("\n");
        seed_reading::water_win we, wl;
        std::array<double, resource_count> lb{}, lf{};
        for (const seed_reading& r : rs)
        {
            we.add(r.w_early); wl.add(r.w_late);
            for (std::size_t g = 0; g < resource_count; ++g) { lb[g] += r.late_bid[g]; lf[g] += r.late_fill[g]; }
        }
        std::printf(" W pooled (per seed-tick):\n");
        print_water("early", we);
        print_water("late", wl);
        std::printf("    pooled late household fill/bid:");
        for (std::size_t g = 0; g < resource_count; ++g)
            if (lb[g] > 0.0)
                std::printf(" %s %.0f%%", resource_names::name_of(static_cast<resource_type>(g)).c_str(), pct(lf[g] / lb[g]));
        std::printf("\n");
    }
    lg_reading lg_all;
    if (g_logistics)
    {
        std::printf(" L pooled (sum / sum over seeds; per read = per seed-probe):\n");
        for (std::size_t i = 0; i < g_lg_ticks.size(); ++i)
        {
            lg_reading t;
            for (const seed_reading& r : rs)
                if (i < r.lg.size()) t.add(r.lg[i]);
            const std::string lbl = "t" + std::to_string(g_lg_ticks[i]);
            print_lg(lbl.c_str(), t, 6);
            lg_all.add(t);
        }
        print_lg("all", lg_all, 10);
    }
    std::printf("market_viability: G1 %.1f/70 G2 %.1f/50 G3 %.1f/70", pct(g1), pct(g2), pct(g3));
    if (g_logistics)
    {
        // appended (BL-1223), never reordered: the L row's headline numbers
        const double u = lg_all.ship_u > 0 ? lg_all.ship_u : 1.0;
        const double sd = lg_all.starved > 0 ? static_cast<double>(lg_all.starved) : 1.0;
        int top = k_proc_order[0];
        for (const int c : k_proc_order)
            if (lg_all.starved_cls[c] > lg_all.starved_cls[top]) top = c;
        std::printf(" | L to-short %.1f%% dry/read %.1f starved/read %.1f top-class %s %.0f%%",
                    pct(lg_all.ship_short_u / u), lg_all.reads ? static_cast<double>(lg_all.dry) / lg_all.reads : 0.0,
                    lg_all.reads ? static_cast<double>(lg_all.starved) / lg_all.reads : 0.0, k_x_name[top],
                    pct(lg_all.starved_cls[top] / sd));
    }
    std::printf("\n");
    return honesty_fail ? 1 : 0;
}
