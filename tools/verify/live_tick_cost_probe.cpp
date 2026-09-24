// live_tick_cost_probe — NR-915: where does a heavy world's LIVE tick go?
//
// WHY THIS EXISTS. player_seed_sweep --charter-cost times the live tick in six
// laps (harness_params.hpp k_app_tick_phase_names), and its lap 1 holds BOTH
// run_economy_step and BL-995's dispatch_convoys. NR-915 found three library
// worlds (41, 31, 28) running x3-x6 their legacy tick at the pinned 650:2 and
// read the cost as following the SPECIALISTS a world seats. This probe splits
// the tick finer and COUNTS the loop quantities that could scale with a world's
// size, so "which loop, over what" has a number:
//
//   laps  arrivals (advance + credit) | run_economy_step | dispatch_convoys |
//         clear_markets | apply_budget + run_nation_step | tech gates |
//         standings + firm exits
//   dispatch shadow (UNTIMED, read-only, taken just before dispatch): the pools,
//         the (corp, pool, good) triples holding a haulable surplus, and the
//         destination legs dispatch's first loop PRICES — the destinations whose
//         gross price clears the home gate — split intra-body / off-body and by
//         owner (specialist / background firm / other). `origin work` is the
//         sum, over the intra-body legs, of (owner's assets x markets on the
//         body): convoy_origin_tile walks the corp's assets and resolves each
//         one's pool key through pool_key_for_tile, which walks every market
//         (markets_by_body's max-id scan) and then the body's markets.
//   origin replay: convoy_origin_tile called once per intra-body leg, timed —
//         the per-leg cost that repeats for every destination of one pool.
//   caches: logistics_flood_fields / astar_cost_cache sizes across the lap.
//   base-scan replay (tick 1): the corp AI's muster-base candidate scan, replayed
//         read-only per owner class (see base_scan_replay).
//
// FIRST READING (2026-09-24, shipped arc, Release, 3 live ticks): the tail is
// NOT the specialists as such. run_economy_step is 55-95% of the live tick and
// ~93% of it is ONE loop — run_corp_strategic_step's military_base candidate
// (corp_ai.cpp ~1730): every corp holding no base, when its eval is due, calls
// can_place_in_world on EVERY tile of its home nation (no early exit once it
// has an HQ), and each call that reaches the province ceiling rebuilds a
// std::map over every population centre in the world (~1.1 ms at 6-14k
// centres). Nearly every tile is placeable, so the scan repeats every eval.
// Cost ~ (base-less corps due) x (home-nation tiles) x (population centres).
// dispatch_convoys (BL-995) is the other 5-45%: ~72% of it is
// convoy_origin_tile re-resolving the SAME origin for every destination leg
// (assets x markets-on-body per leg, via pool_key_for_tile -> markets_by_body's
// full max-id scan + nearest_market).
//
// A PURE READER. It steps the world through the same calls, in the same order,
// as run_app_live_window (harness_params.hpp) and reads between them. The shadow
// and the replay call only const functions. The world stepped is byte-for-byte
// the sweep's: the shipped start (build_app_start_world, the shipped divisor
// k_stockpile_price_divisor and m), settled, seated, then the live window.
//
// Build:  cmd.exe //c "tools\verify\build_lua_harness.bat live_tick_cost_probe"
// Run (repo root):
//   build_gen/verify/live_tick_cost_probe.exe [--seeds 41,31,28] [--live-ticks 4]
//        [--arc shipped|legacy] [--sample [--top N]] [--quick]
//   --quick   skip the dispatch shadow and the base-scan replay (timing + hash only)
//   Every run ends each seed with a STATE line (BL-1079): world::state_hash and an
//   FNV-1a of the save snapshot after the live window - the before/after proof
//   that a pure speed-up moved no result.
//   --sample  (Windows) an in-process stack sampler over the econ_step and
//             dispatch laps (~1 ms period). Build with a PDB for names:
//             CL="-Zi -FS" _LINK_=-DEBUG (dashes: Git Bash rewrites a leading /),
//             then the build line above.

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/corp_ai.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/province.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#ifdef _WIN32
// --sample: an in-process stack sampler over the econ_step and dispatch laps.
// Build with a PDB (CL=/Zi /FS, _LINK_=/DEBUG) or the report prints addresses.
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <atomic>
#include <thread>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "winmm.lib")
#endif

namespace {

using clk = std::chrono::steady_clock;
double ms(clk::time_point a, clk::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

#ifdef _WIN32
/// Suspends the main thread every ~1 ms while `phase` >= 0, unwinds it with the
/// x64 unwind tables (nothing allocated while it is suspended), keeps the stack.
struct sampler
{
    static constexpr int k_depth = 48;
    struct sample { int phase; int n; DWORD64 pc[k_depth]; };
    HANDLE              target = nullptr;
    std::atomic<int>    phase{-1};
    std::atomic<bool>   quit{false};
    std::vector<sample> samples;
    std::thread         th;

    void start()
    {
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &target,
                        THREAD_ALL_ACCESS, FALSE, 0);
        samples.reserve(100000);
        th = std::thread([this] { loop(); });
    }
    void stop()
    {
        quit = true;
        if (th.joinable())
            th.join();
    }
    void loop()
    {
        timeBeginPeriod(1);
        while (!quit)
        {
            const int ph = phase.load();
            if (ph >= 0 && samples.size() < samples.capacity())
            {
                sample sm{};
                sm.phase = ph;
                if (SuspendThread(target) != static_cast<DWORD>(-1))
                {
                    CONTEXT c{};
                    c.ContextFlags = CONTEXT_FULL;
                    if (GetThreadContext(target, &c))
                    {
                        while (sm.n < k_depth && c.Rip != 0)
                        {
                            sm.pc[sm.n++] = c.Rip;
                            DWORD64 image_base = 0;
                            PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(c.Rip, &image_base, nullptr);
                            if (rf == nullptr)
                            {
                                c.Rip = *reinterpret_cast<DWORD64*>(c.Rsp);
                                c.Rsp += 8;
                            }
                            else
                            {
                                PVOID   hd = nullptr;
                                DWORD64 ef = 0;
                                RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, c.Rip, rf, &c, &hd,
                                                 &ef, nullptr);
                            }
                        }
                    }
                    ResumeThread(target);
                    if (sm.n > 0)
                        samples.push_back(sm);
                }
            }
            Sleep(1);
        }
        timeEndPeriod(1);
    }

    static std::string name_of(DWORD64 pc, bool with_line)
    {
        alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 512];
        auto* si = reinterpret_cast<SYMBOL_INFO*>(buf);
        si->SizeOfStruct = sizeof(SYMBOL_INFO);
        si->MaxNameLen   = 511;
        DWORD64 disp = 0;
        std::string out;
        if (SymFromAddr(GetCurrentProcess(), pc, &disp, si))
            out = si->Name;
        else
        {
            char hex[32];
            std::snprintf(hex, sizeof hex, "0x%llx", static_cast<unsigned long long>(pc));
            out = hex;
        }
        if (with_line)
        {
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof line;
            DWORD ldisp = 0;
            if (SymGetLineFromAddr64(GetCurrentProcess(), pc, &ldisp, &line))
            {
                std::string f = line.FileName;
                const std::size_t sl = f.find_last_of("\\/");
                if (sl != std::string::npos)
                    f = f.substr(sl + 1);
                out += " @" + f + ":" + std::to_string(line.LineNumber);
            }
        }
        return out;
    }

    void report(int ph, const char* label, int top) const
    {
        std::map<DWORD64, std::string> fn_cache, line_cache;
        const auto fn = [&](DWORD64 pc) -> const std::string& {
            auto it = fn_cache.find(pc);
            if (it == fn_cache.end())
                it = fn_cache.emplace(pc, name_of(pc, false)).first;
            return it->second;
        };
        const auto ln = [&](DWORD64 pc) -> const std::string& {
            auto it = line_cache.find(pc);
            if (it == line_cache.end())
                it = line_cache.emplace(pc, name_of(pc, true)).first;
            return it->second;
        };
        std::map<std::string, int> incl, self, self_line, caller_line;
        int total = 0;
        for (const sample& sm : samples)
        {
            if (sm.phase != ph)
                continue;
            ++total;
            std::set<std::string> seen;
            for (int i = 0; i < sm.n; ++i)
            {
                const std::string& nm = fn(sm.pc[i]);
                if (i == 0)
                {
                    ++self[nm];
                    ++self_line[ln(sm.pc[i])];
                }
                if (seen.insert(nm).second)
                    ++incl[nm];
                // every call site on the stack, once per sample: which LINE of
                // which caller the time sits under
                if (i > 0)
                {
                    const std::string& cl = ln(sm.pc[i] - 1);
                    if (seen.insert("@" + cl).second)
                        ++caller_line[cl];
                }
            }
        }
        std::printf("\n--- SAMPLES %s: %d (~1 ms each) ---\n", label, total);
        if (total == 0)
            return;
        const auto dump = [&](const std::map<std::string, int>& m, const char* what) {
            std::vector<std::pair<int, std::string>> v;
            for (const auto& kv : m)
                v.push_back({kv.second, kv.first});
            std::sort(v.rbegin(), v.rend());
            std::printf("  %s:\n", what);
            for (int i = 0; i < top && i < static_cast<int>(v.size()); ++i)
                std::printf("    %5.1f%%  %s\n", 100.0 * v[i].first / total, v[i].second.c_str());
        };
        dump(incl, "inclusive (function)");
        dump(caller_line, "inclusive (call site)");
        dump(self, "self (leaf function)");
        dump(self_line, "self (leaf line)");
    }
};
sampler* g_sampler = nullptr;
void sample_phase(int ph) { if (g_sampler) g_sampler->phase = ph; }
#else
void sample_phase(int) {}
#endif

struct options
{
    std::vector<std::uint32_t> seeds{41, 31, 28};
    int                        live_ticks = 4;
    world_arc                  arc        = world_arc::shipped;
    bool                       sample     = false;
    bool                       quick      = false;   // skip the shadow and the base-scan replay
    int                        top        = 40;
};

constexpr int k_laps = 7;
const char* k_lap_names[k_laps] = {"arrivals", "econ_step", "dispatch", "clear",
                                   "budget+nation", "tech", "stand+exits"};

enum owner_class { own_spec = 0, own_firm = 1, own_other = 2, own_count = 3 };
const char* k_owner_names[own_count] = {"spec", "firm", "other"};

struct shadow
{
    long long pools = 0;
    long long triples[own_count] = {};          // surplus (corp, pool, good)
    long long legs_intra[own_count] = {};       // first-loop legs priced, same body
    long long legs_space[own_count] = {};       // first-loop legs priced, off body
    long long origin_work = 0;                  // sum assets x body markets over intra legs
    long long pool_key_calls = 0;               // pool_key_for_tile calls those legs make
    double    origin_replay_ms = 0.0;           // convoy_origin_tile, once per intra leg
    double    shadow_ms = 0.0;
};

shadow take_shadow(const world& w, const recipe_registry& reg,
                   const std::set<entity_id>& specs, const std::set<entity_id>& firms)
{
    const auto s0 = clk::now();
    shadow sh;
    sh.pools = static_cast<long long>(w.corp_market_pools.size());

    std::map<entity_id, long long> body_markets;
    for (const auto& [mid, m] : w.markets)
        ++body_markets[m.body];
    std::vector<entity_id> market_ids;
    for (const auto& [mid, m] : w.markets)
        market_ids.push_back(mid);
    std::sort(market_ids.begin(), market_ids.end());

    std::set<std::tuple<entity_id, entity_id, std::size_t>> order_controlled;
    for (const sell_order& o : w.sell_orders)
        if (o.quantity > 0.0f)
            order_controlled.insert({o.corp, o.body, static_cast<std::size_t>(o.resource)});

    const grid_goods_params& grid_rules = reg.grid_goods();
    const float              margin     = reg.dispatch_margin();

    struct leg_ref { entity_id corp; entity_id src; };
    std::vector<leg_ref> intra_legs;

    for (const auto& [key, pool] : w.corp_market_pools)
    {
        const entity_id corp_id = key.first;
        const entity_id src_key = key.second;
        const auto cit = w.corporations.find(corp_id);
        if (cit == w.corporations.end())
            continue;
        const entity_id src_body = pool_key_body(w, src_key);
        if (src_body == null_entity)
            continue;
        const int oc = specs.count(corp_id) ? own_spec : firms.count(corp_id) ? own_firm : own_other;
        const auto reserve = processor_reservation(w, reg, corp_id, src_key);
        for (std::size_t ri = 0; ri < resource_count; ++ri)
        {
            if (grid_rules.grid(ri))
                continue;
            if (order_controlled.count({corp_id, src_body, ri}) != 0)
                continue;
            const float surplus = pool.quantities[ri] - reserve[ri]
                                - dispatch_arrived(w, corp_id, src_key, ri);
            if (!(surplus > 0.0f))
                continue;
            ++sh.triples[oc];
            const float price_src = dispatch_home_price(w, src_key, ri);
            const float gate      = price_src + margin * price_src;
            for (const entity_id dest_id : market_ids)
            {
                if (dest_id == src_key)
                    continue;
                const market_component& dm = w.markets.at(dest_id);
                if (!(dispatch_market_price(dm, ri) > gate))
                    continue;
                if (dm.body == src_body)
                {
                    ++sh.legs_intra[oc];
                    const long long a = static_cast<long long>(cit->second.assets.size());
                    const bool is_market = w.markets.count(src_key) != 0;
                    if (is_market)
                    {
                        sh.pool_key_calls += a;
                        sh.origin_work += a * body_markets[src_body];
                    }
                    intra_legs.push_back({corp_id, src_key});
                }
                else
                    ++sh.legs_space[oc];
            }
        }
    }
    sh.shadow_ms = ms(s0, clk::now());

    // The replay: the origin resolution each intra-body leg repeats.
    const auto r0 = clk::now();
    entity_id sink = 0;
    for (const leg_ref& l : intra_legs)
        sink ^= convoy_origin_tile(w, w.corporations.at(l.corp), l.src);
    sh.origin_replay_ms = ms(r0, clk::now());
    if (sink == 0xFFFFFFFFu)
        std::printf("(sink)\n");
    return sh;
}

/// The corp AI's MUSTER-BASE candidate scan (corp_ai.cpp, the military_base
/// block of run_corp_strategic_step): a corp holding no base walks EVERY tile of
/// its home nation through can_place_in_world (no early exit once it has an HQ
/// anchor — it wants the nearest placeable tile). Every tile that passes the
/// earlier rules reaches the province ceiling, which rebuilds a std::map over
/// every population centre in the world (province.cpp population_scale_by_tile).
/// Replayed here, read-only, per owner class — calls, how many reached the
/// province check, and the wall time.
void base_scan_replay(const world& w, const recipe_registry& reg,
                      const std::set<entity_id>& specs, const std::set<entity_id>& firms)
{
    long long corps_scanning[own_count] = {}, due_scanning[own_count] = {};
    long long calls[own_count] = {}, reached_province[own_count] = {}, placeable[own_count] = {};
    double    ms_by[own_count] = {};
    const float reach_budget = reg.construction().max_logistics_reach;
    std::vector<entity_id> ids;
    for (const auto& [cid, c] : w.corporations)
        ids.push_back(cid);
    std::sort(ids.begin(), ids.end());
    for (const entity_id cid : ids)
    {
        const corporation_component& cc = w.corporations.at(cid);
        const int oc = specs.count(cid) ? own_spec : firms.count(cid) ? own_firm : own_other;
        bool has_base = false;
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit != w.buildings.end() && bit->second.type == building_type::military_base
                && !bit->second.decommissioned)
                has_base = true;
        }
        if (has_base)
            continue;
        const auto nit = w.nations.find(cc.home_nation);
        if (nit == w.nations.end())
            continue;
        ++corps_scanning[oc];
        if (corp_strategic_eval_due(w, cid, w.current_econ_tick))
            ++due_scanning[oc];
        const auto t0 = clk::now();
        // BL-1079: the scan runs under the same memo corp_ai.cpp opens for it.
        const province_ceiling_scope ceiling_memo(w);
        for (const entity_id tid : nit->second.tiles)
        {
            ++calls[oc];
            const placement_rules::placement_result r = placement_rules::can_place_in_world(
                w, tid, building_type::military_base, resource_type::iron_ore, reach_budget, cid);
            if (r.reason == placement_rules::placement_reason::ok || r.reason == placement_rules::placement_reason::province_full)
                ++reached_province[oc];
            if (r.reason == placement_rules::placement_reason::ok)
                ++placeable[oc];
        }
        ms_by[oc] += ms(t0, clk::now());
    }
    std::printf("   BASE-SCAN REPLAY (corps with no military base walk their home nation):\n");
    for (int c = 0; c < own_count; ++c)
        std::printf("     %-5s corps scanning %lld (eval due now %lld): can_place_in_world calls %lld, "
                    "reached the province ceiling %lld (placeable %lld), %.0f ms\n",
                    k_owner_names[c], corps_scanning[c], due_scanning[c], calls[c],
                    reached_province[c], placeable[c], ms_by[c]);
    // One province-ceiling evaluation, the unit cost behind every reached call.
    for (const auto& [cid, c] : w.corporations)
    {
        const auto hq = w.buildings.find(c.hq_building);
        if (hq == w.buildings.end())
            continue;
        const uint32_t pid = w.provinces.province_of(hq->second.tile);
        if (pid == 0)
            continue;
        const auto t0 = clk::now();
        int sink = 0;
        for (int i = 0; i < 20; ++i)
            sink += province_building_ceiling(w, pid);
        std::printf("     one province_building_ceiling: %.3f ms (population centres %zu)%s\n",
                    ms(t0, clk::now()) / 20.0, w.population_centre_tile.size(),
                    sink == -12345 ? " " : "");
        break;
    }
}

void run_seed(lua_state& lua, const options& o, std::uint32_t seed)
{
    world_params p = arc_params(o.arc);
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    const auto g0 = clk::now();
    build_app_start_world(lua, p, *start);
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    const std::set<entity_id> specs(start->land.specialists.begin(), start->land.specialists.end());
    const std::set<entity_id> firms(start->land.firms.begin(), start->land.firms.end());

    run_app_validation_settle(w, reg);
    const spawn_seat_result res = seat_player_corporation(w, p.seed, start->land.search.winner_score);
    (void)res;

    std::map<entity_id, long long> body_markets;
    for (const auto& [mid, m] : w.markets)
        ++body_markets[m.body];
    long long max_body_markets = 0;
    for (const auto& kv : body_markets)
        max_body_markets = std::max(max_body_markets, kv.second);
    std::size_t spec_assets = 0, firm_assets = 0, other_assets = 0;
    for (const auto& [cid, c] : w.corporations)
        (specs.count(cid) ? spec_assets : firms.count(cid) ? firm_assets : other_assets) += c.assets.size();

    std::printf("\n=== seed %u (%s arc) — built+settled+seated in %.1f s ===\n", seed,
                world_arc_name(o.arc), ms(g0, clk::now()) / 1000.0);
    std::printf("corps %zu: specialists %zu (assets %zu), firms %zu (assets %zu), other %zu "
                "(assets %zu); buildings %zu; markets %zu on %zu bodies (max %lld on one); "
                "pools %zu; convoys %zu\n",
                w.corporations.size(), specs.size(), spec_assets, firms.size(), firm_assets,
                w.corporations.size() - specs.size() - firms.size(), other_assets,
                w.buildings.size(), w.markets.size(), body_markets.size(), max_body_markets,
                w.corp_market_pools.size(), w.convoys.size());

    constexpr int k_econ_tick_days = 90;
    double lap_sum[k_laps] = {};
    double tick_sum = 0.0;
    for (int k = 1; k <= o.live_ticks; ++k)
    {
        const int day = k * k_econ_tick_days;
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = day;
        w.current_econ_tick = k_app_validation_ticks + (k - 1);
        lp_pool_map tick_lp_pools;
        double lap[k_laps] = {};

        auto t0 = clk::now();
        advance_convoys(w);
        credit_arrived_convoys(w, day);
        auto t1 = clk::now();
        lap[0] = ms(t0, t1);

        const std::size_t ff_e0 = w.logistics_flood_fields.size();
        const std::size_t ac_e0 = w.astar_cost_cache.size();
        t0 = clk::now();
        sample_phase(0);
        economy_report report = run_economy_step(w, reg, /*spectating=*/false, &tick_lp_pools);
        sample_phase(-1);
        t1 = clk::now();
        lap[1] = ms(t0, t1);
        const std::size_t ff_e1 = w.logistics_flood_fields.size();
        const std::size_t ac_e1 = w.astar_cost_cache.size();

        const shadow sh = o.quick ? shadow{} : take_shadow(w, reg, specs, firms);   // untimed in the laps
        const std::size_t conv0 = w.convoys.size();

        t0 = clk::now();
        sample_phase(1);
        const convoy_dispatch_tick dt = dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                                                         reg.logistics_cost(convoy_mode::space),
                                                         &tick_lp_pools);
        sample_phase(-1);
        t1 = clk::now();
        lap[2] = ms(t0, t1);
        const std::size_t ff_d1 = w.logistics_flood_fields.size();
        const std::size_t ac_d1 = w.astar_cost_cache.size();

        t0 = clk::now();
        const auto flows = clear_markets(w, reg, report);
        t1 = clk::now();
        lap[3] = ms(t0, t1);

        t0 = clk::now();
        apply_budget(w, reg, flows, report.workforce_contention,
                     &report.budgets, &report.buildings, &report.building_labour);
        run_nation_step(w, reg, report, w.current_econ_tick);
        t1 = clk::now();
        lap[4] = ms(t0, t1);

        t0 = clk::now();
        advance_tech_gates(w);
        t1 = clk::now();
        lap[5] = ms(t0, t1);

        t0 = clk::now();
        (void)compute_corp_standings(w, flows);
        run_firm_exits(w, reg.firm_exit(), &report.firm_exits);
        t1 = clk::now();
        lap[6] = ms(t0, t1);

        double tick = 0.0;
        for (int i = 0; i < k_laps; ++i)
        {
            tick += lap[i];
            lap_sum[i] += lap[i];
        }
        tick_sum += tick;
        std::printf("tick %d: %.0f ms |", k, tick);
        for (int i = 0; i < k_laps; ++i)
            std::printf(" %s %.0f", k_lap_names[i], lap[i]);
        std::printf("\n   dispatch shadow: pools %lld; surplus triples", sh.pools);
        for (int c = 0; c < own_count; ++c)
            std::printf(" %s %lld", k_owner_names[c], sh.triples[c]);
        std::printf("; legs priced intra");
        for (int c = 0; c < own_count; ++c)
            std::printf(" %s %lld", k_owner_names[c], sh.legs_intra[c]);
        std::printf(", off-body");
        for (int c = 0; c < own_count; ++c)
            std::printf(" %s %lld", k_owner_names[c], sh.legs_space[c]);
        std::printf("\n   origin work %lld (pool_key_for_tile calls %lld); origin replay %.0f ms; "
                    "shadow %.0f ms; dispatched %d (convoys %zu -> %zu)\n",
                    sh.origin_work, sh.pool_key_calls, sh.origin_replay_ms, sh.shadow_ms,
                    dt.dispatched, conv0, w.convoys.size());
        std::printf("   caches: flood fields econ %zu->%zu, dispatch ->%zu; astar pairs econ %zu->%zu, "
                    "dispatch ->%zu\n", ff_e0, ff_e1, ff_d1, ac_e0, ac_e1, ac_d1);
        if (k == 1 && !o.quick)
            base_scan_replay(w, reg, specs, firms);
    }
    std::printf("MEAN over %d live ticks: %.0f ms |", o.live_ticks, tick_sum / o.live_ticks);
    for (int i = 0; i < k_laps; ++i)
        std::printf(" %s %.0f (%.0f%%)", k_lap_names[i], lap_sum[i] / o.live_ticks,
                    tick_sum > 0 ? 100.0 * lap_sum[i] / tick_sum : 0.0);
    std::printf("\n");

    // BL-1079: the live-tick STATE after the window, so a pure speed-up can be
    // proved result-identical by running this probe on the tree before and after
    // it. Two readings: world::state_hash (the tick-mutating fields) and an
    // FNV-1a over the whole flat-binary save snapshot (convoys, shelves, pools,
    // population, the order book - everything the save carries).
    {
        std::ostringstream snap(std::ios::binary);
        write_world_snapshot(w, snap);
        const std::string bytes = snap.str();
        std::uint64_t fh = 1469598103934665603ull;
        for (const unsigned char c : bytes)
        {
            fh ^= c;
            fh *= 1099511628211ull;
        }
        std::printf("STATE seed %u after %d live ticks: state_hash %016llX snapshot %016llX "
                    "(%zu bytes; convoys %zu)\n",
                    seed, o.live_ticks,
                    static_cast<unsigned long long>(w.state_hash(w.current_econ_tick)),
                    static_cast<unsigned long long>(fh), bytes.size(), w.convoys.size());
    }
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--seeds" && i + 1 < argc)
        {
            o.seeds.clear();
            const std::string s = argv[++i];
            std::size_t pos = 0;
            while (pos <= s.size())
            {
                const std::size_t c = s.find(',', pos);
                o.seeds.push_back(static_cast<std::uint32_t>(
                    std::strtoul(s.substr(pos, c - pos).c_str(), nullptr, 10)));
                if (c == std::string::npos)
                    break;
                pos = c + 1;
            }
        }
        else if (a == "--live-ticks" && i + 1 < argc) o.live_ticks = std::atoi(argv[++i]);
        else if (a == "--sample")                 o.sample = true;
        else if (a == "--quick")                  o.quick = true;
        else if (a == "--top" && i + 1 < argc)    o.top = std::atoi(argv[++i]);
        else if (a == "--arc" && i + 1 < argc)
            o.arc = std::string(argv[++i]) == "legacy" ? world_arc::legacy : world_arc::shipped;
        else
        {
            std::printf("usage: %s [--seeds 41,31,28] [--live-ticks N] [--arc shipped|legacy] [--quick]\n", argv[0]);
            return 2;
        }
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef _WIN32
    sampler smp;
    if (o.sample)
    {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        g_sampler = &smp;
        smp.start();
    }
#endif
    lua_state lua;
    for (const std::uint32_t s : o.seeds)
        run_seed(lua, o, s);
#ifdef _WIN32
    if (o.sample)
    {
        smp.stop();
        smp.report(0, "run_economy_step (all seeds, all live ticks)", o.top);
        smp.report(1, "dispatch_convoys (all seeds, all live ticks)", o.top);
    }
#endif
    return 0;
}
