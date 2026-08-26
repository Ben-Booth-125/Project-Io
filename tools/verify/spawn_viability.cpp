// spawn_viability — the Sprint 20 spawn-viability sweep instrument (BL-626).
//
// THE QUESTION IT MEASURES. Corporations open as new charters (base_capital = 0,
// Ben's 2026-07-06 steer): the opening balance is whatever the 80-tick pre-game
// warm start earns. Different starts therefore provide different opportunities,
// and whether a given spawn is VIABLE — solvent, earning, able to act — is an
// emergent fact nobody has measured. This harness measures it, across seeds,
// for EVERY corporation in the world (Ben, 2026-08-26: "there is nothing too
// special about player corporations, so we can use every angle to gather
// data"). The spawn-form design (Sprint 20's second half) waits on this data.
//
// WHAT ONE RUN DOES. For each seed s in [0, seed_count): generate the real
// world (make_hard_coded_world with the full prehistory, exactly as a campaign
// does), add background firms and default recipes in the app's own order, then
// run `warm_ticks` economy ticks mirroring app::step_economy — shared LP pool,
// cadence counter, corp AI included. By default the strategic tier runs
// SPECTATING (BL-409): with no human seat every corp evaluates, player slot
// included, so every corp is a comparable data point. `--played` instead runs
// the shipped warm start (spectating = false), where the player-slot corp runs
// its buildings but takes no strategic actions — the trajectory a player
// actually inherits. The DELTA between the two modes is itself data: how much
// of a spawn's viability depends on acting versus endowment.
//
// PER-CORP RECORD (one CSV row per corp per seed): identity (name, class:
// player-slot / rival / background, focus, home nation), holdings open/close,
// balance open/min/close, ticks spent in debt, trailing-year net per quarter,
// strategic decisions taken, stockpile units at close, and three report-first
// verdict flags — solvent (closing balance >= 0), earning (trailing-year net
// > 0), active (took >= 1 strategic decision or grew its holdings). The
// verdict COMBINATION rule (what "viable" finally means) is deliberately not
// asserted here: first-cut definition is logged for Ben's review (NR entry,
// 2026-08-26); this instrument reports the ingredients.
//
// REPORT-FIRST, THREE HARD ASSERTIONS ONLY (the road_reach_census idiom —
// numbers to watch, not pinned bands):
//   1. Vacuity: every seed produced >= 1 corp and >= 1 named (non-background)
//      corp, and the warm start actually ran.
//   2. Determinism: the first seed re-run end-to-end reproduces the identical
//      state_hash and every closing balance bit-for-bit.
//   3. Attribution: the decision-ring drain never saturated (per-tick pushes
//      stayed under the ring's 256 capacity), so per-corp action counts are
//      trustworthy.
//
// Absolute numbers are confounded by the open economy pathologies (the
// 2026-08-25 close-out caution) — read the output RELATIVELY: across seeds,
// across classes, across focus, and later across spawn forms on a fixed seed.
//
// No SDL / ImGui; needs a live Lua state (scripts/recipes.lua +
// scripts/economy.lua through recipe_registry), so it is CMake-only, like
// pregame_balance_harness — build_harness.js refuses it by name (the NR-558
// standing gap).
//
// Build:  cmake --build build --target spawn_viability
// Run:    ./build/spawn_viability [seed_count=8] [warm_ticks=80]
//                                 [--played] [--lean] [--csv <path>]
//   --played  spectating OFF: the shipped warm start (player slot passive).
//   --lean    skip the 400-year prehistory (fast smoke; NOT campaign-real —
//             never mix lean rows with real rows in one analysis).
//   --csv     output path (default build_gen/verify/spawn_viability.csv).

#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok)
        ++g_failures;
}

const char* focus_name(industrial_focus f)
{
    switch (f)
    {
    case industrial_focus::extraction: return "extraction";
    case industrial_focus::processing: return "processing";
    case industrial_focus::trade:      return "trade";
    }
    return "unknown";
}

/// One corp's sweep record over a single seed's warm start.
struct corp_track
{
    entity_id   id = null_entity;
    std::string name;
    std::string cls;    // "player-slot" | "rival" | "background"
    std::string focus;
    std::string nation;
    std::size_t holdings_open  = 0;
    std::size_t holdings_close = 0;
    float       bal_open  = 0.0f;
    float       bal_min   = 0.0f;
    float       bal_close = 0.0f;
    int         ticks_in_debt = 0;
    float       net_trailing_qtr = 0.0f; ///< (bal[T] - bal[T-4]) / 4.
    int         decisions = 0;
    float       stock_units_close = 0.0f;
    bool solvent = false, earning = false, active = false;
};

struct seed_result
{
    uint32_t                seed = 0;
    std::vector<corp_track> corps;
    uint64_t                final_hash = 0;
    bool                    ring_saturated = false;
};

/// One app-faithful economy tick: app::step_economy minus the UI plumbing.
/// Cadence counter, shared LP pool, corp AI (spectating per BL-409) included.
void tick(world& w, const recipe_registry& reg, int t, bool spectating)
{
    w.current_econ_tick = t - 1; // app: current_econ_tick = m_econ_steps++ (from 0)
    lp_pool_map pools;
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space), &pools);
    advance_convoys(w);
    const economy_report report = run_economy_step(w, reg, spectating, &pools);
    const auto flows = clear_markets(w, reg, report);
    apply_budget(w, reg, flows, report.workforce_contention, nullptr);
    credit_arrived_convoys(w, t);
}

/// Count the decisions pushed since `prev_total`, attributed per corp.
/// Returns false (saturated) if more than `capacity` landed in one drain —
/// attribution is then untrustworthy for this seed.
bool drain_decisions(const corp_decision_ring& ring, std::size_t& prev_total,
                     std::map<entity_id, int>& per_corp)
{
    const std::size_t fresh = ring.total - prev_total;
    prev_total = ring.total;
    if (fresh == 0)
        return true;
    if (fresh > corp_decision_ring::capacity)
        return false;
    for (std::size_t k = 0; k < fresh; ++k)
    {
        std::size_t idx;
        if (ring.entries.size() < corp_decision_ring::capacity)
            idx = ring.entries.size() - 1 - k;
        else
            idx = (ring.next + 2 * corp_decision_ring::capacity - 1 - k)
                  % corp_decision_ring::capacity;
        ++per_corp[ring.entries[idx].corp];
    }
    return true;
}

seed_result run_seed(uint32_t seed, int warm_ticks, bool spectating, bool lean,
                     const recipe_registry& reg)
{
    world_params p;
    p.seed = seed;
    if (lean)
        p = no_prehistory(p);

    world w = make_hard_coded_world(p);
    // App ordering (app.cpp start_new_game): background firms, then default
    // recipes, then the warm-start ticks.
    generate_background_firms(w, reg, p.seed ^ 0x8A21F00Du);
    assign_default_recipes(w, reg);

    seed_result res;
    res.seed = seed;

    // Opening snapshot + tracking state.
    std::map<entity_id, corp_track>          tracks;
    std::map<entity_id, std::vector<float>>  balances; // per-corp trajectory
    for (const auto& [id, cc] : w.corporations)
    {
        corp_track ct;
        ct.id    = id;
        ct.name  = cc.name;
        ct.cls   = (id == w.player_entity) ? "player-slot"
                 : cc.is_background        ? "background" : "rival";
        ct.focus = focus_name(cc.focus);
        const auto nit = w.nations.find(cc.home_nation);
        ct.nation        = (nit != w.nations.end()) ? nit->second.name : "(none)";
        ct.holdings_open = cc.assets.size();
        ct.bal_open      = cc.balance;
        ct.bal_min       = cc.balance;
        tracks[id] = std::move(ct);
        balances[id].push_back(cc.balance);
    }

    std::map<entity_id, int> decisions;
    std::size_t              ring_seen = w.ai_decisions.total;

    for (int t = 1; t <= warm_ticks; ++t)
    {
        tick(w, reg, t, spectating);
        if (!drain_decisions(w.ai_decisions, ring_seen, decisions))
            res.ring_saturated = true;
        for (auto& [id, ct] : tracks)
        {
            const auto cit = w.corporations.find(id);
            if (cit == w.corporations.end())
                continue; // corp removed mid-warm-start: closing stats freeze
            const float bal = cit->second.balance;
            balances[id].push_back(bal);
            ct.bal_min = std::min(ct.bal_min, bal);
            if (bal < 0.0f)
                ++ct.ticks_in_debt;
        }
    }
    res.final_hash = w.state_hash(warm_ticks);

    for (auto& [id, ct] : tracks)
    {
        const auto& hist = balances[id];
        ct.bal_close = hist.back();
        const std::size_t n = hist.size();
        if (n > 4)
            ct.net_trailing_qtr = (hist[n - 1] - hist[n - 5]) / 4.0f;
        const auto cit = w.corporations.find(id);
        if (cit != w.corporations.end())
            ct.holdings_close = cit->second.assets.size();
        const auto dit = decisions.find(id);
        ct.decisions = (dit != decisions.end()) ? dit->second : 0;
        for (const auto& [key, pool] : w.corp_body_pools)
            if (key.first == id)
                for (std::size_t r = 0; r < resource_count; ++r)
                    ct.stock_units_close += pool.quantities[r];
        ct.solvent = ct.bal_close >= 0.0f;
        ct.earning = ct.net_trailing_qtr > 0.0f;
        ct.active  = ct.decisions > 0 || ct.holdings_close > ct.holdings_open;
        res.corps.push_back(ct);
    }
    return res;
}

struct class_agg
{
    int n = 0, solvent = 0, earning = 0, active = 0;
    std::vector<float> closes;
};

float median(std::vector<float>& v)
{
    if (v.empty()) return 0.0f;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

} // namespace

int real_main(int argc, char** argv);

int main(int argc, char** argv)
{
    try { return real_main(argc, argv); }
    catch (const std::exception& e) { std::printf("EXCEPTION: %s\n", e.what()); return 99; }
    catch (...) { std::printf("UNKNOWN EXCEPTION\n"); return 98; }
}

int real_main(int argc, char** argv)
{
    int         seed_count = 8;
    int         warm_ticks = 80;
    bool        spectating = true;
    bool        lean       = false;
    std::string csv_path   = "build_gen/verify/spawn_viability.csv";

    int positional = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--played") == 0)      spectating = false;
        else if (std::strcmp(argv[i], "--lean") == 0)   lean = true;
        else if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) csv_path = argv[++i];
        else
        {
            const int n = std::atoi(argv[i]);
            if (n <= 0)
            {
                std::printf("usage: %s [seed_count] [warm_ticks] [--played] [--lean] [--csv <path>]\n",
                            argv[0]);
                return 2;
            }
            (positional++ == 0 ? seed_count : warm_ticks) = n;
        }
    }

    std::printf("spawn_viability: %d seed(s) x %d warm ticks | mode=%s | prehistory=%s\n\n",
                seed_count, warm_ticks, spectating ? "spectate (every corp acts)" : "played (player slot passive)",
                lean ? "SKIPPED (--lean, not campaign-real)" : "full (campaign-real)");

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    std::filesystem::create_directories(std::filesystem::path(csv_path).parent_path());
    std::FILE* csv = std::fopen(csv_path.c_str(), "w");
    if (!csv)
    {
        std::printf("cannot open CSV output %s\n", csv_path.c_str());
        return 2;
    }
    std::fprintf(csv, "mode,prehistory,seed,corp,class,focus,nation,name,"
                      "holdings_open,holdings_close,bal_open,bal_min,bal_close,"
                      "ticks_in_debt,net_trailing_qtr,decisions,stock_units_close,"
                      "solvent,earning,active\n");

    std::map<std::string, class_agg> by_class;
    std::map<std::string, class_agg> by_focus;
    bool vacuity_ok = true, ring_ok = true;
    std::vector<seed_result> results;

    for (int s = 0; s < seed_count; ++s)
    {
        seed_result res = run_seed(static_cast<uint32_t>(s), warm_ticks, spectating, lean, reg);
        int named = 0, solvent = 0;
        for (const corp_track& ct : res.corps)
        {
            if (ct.cls != "background") ++named;
            if (ct.solvent) ++solvent;
            std::fprintf(csv, "%s,%s,%u,%llu,%s,%s,\"%s\",\"%s\",%zu,%zu,%.2f,%.2f,%.2f,%d,%.2f,%d,%.1f,%d,%d,%d\n",
                         spectating ? "spectate" : "played", lean ? "lean" : "full",
                         res.seed, static_cast<unsigned long long>(ct.id),
                         ct.cls.c_str(), ct.focus.c_str(), ct.nation.c_str(), ct.name.c_str(),
                         ct.holdings_open, ct.holdings_close,
                         static_cast<double>(ct.bal_open), static_cast<double>(ct.bal_min),
                         static_cast<double>(ct.bal_close), ct.ticks_in_debt,
                         static_cast<double>(ct.net_trailing_qtr), ct.decisions,
                         static_cast<double>(ct.stock_units_close),
                         ct.solvent ? 1 : 0, ct.earning ? 1 : 0, ct.active ? 1 : 0);
            class_agg& ca = by_class[ct.cls];
            class_agg& fa = by_focus[ct.focus];
            for (class_agg* a : {&ca, &fa})
            {
                ++a->n;
                a->solvent += ct.solvent ? 1 : 0;
                a->earning += ct.earning ? 1 : 0;
                a->active  += ct.active ? 1 : 0;
                a->closes.push_back(ct.bal_close);
            }
        }
        std::printf("seed %2u: %3zu corps (%d named) | solvent %3zu%% | hash %016llX%s\n",
                    res.seed, res.corps.size(), named,
                    res.corps.empty() ? 0 : 100 * static_cast<std::size_t>(solvent) / res.corps.size(),
                    static_cast<unsigned long long>(res.final_hash),
                    res.ring_saturated ? " | RING SATURATED" : "");
        if (res.corps.empty() || named == 0)
            vacuity_ok = false;
        if (res.ring_saturated)
            ring_ok = false;
        results.push_back(std::move(res));
    }
    std::fclose(csv);

    std::printf("\nPer class:\n");
    for (auto& [cls, a] : by_class)
        std::printf("  %-11s n=%4d | solvent %3d%% | earning %3d%% | active %3d%% | close min/med/max %.0f / %.0f / %.0f\n",
                    cls.c_str(), a.n, a.n ? 100 * a.solvent / a.n : 0,
                    a.n ? 100 * a.earning / a.n : 0, a.n ? 100 * a.active / a.n : 0,
                    static_cast<double>(*std::min_element(a.closes.begin(), a.closes.end())),
                    static_cast<double>(median(a.closes)),
                    static_cast<double>(*std::max_element(a.closes.begin(), a.closes.end())));
    std::printf("Per focus:\n");
    for (auto& [f, a] : by_focus)
        std::printf("  %-11s n=%4d | solvent %3d%% | earning %3d%% | active %3d%% | close med %.0f\n",
                    f.c_str(), a.n, a.n ? 100 * a.solvent / a.n : 0,
                    a.n ? 100 * a.earning / a.n : 0, a.n ? 100 * a.active / a.n : 0,
                    static_cast<double>(median(a.closes)));
    std::printf("\nCSV: %s\n\n", csv_path.c_str());

    // --- Hard assertions -----------------------------------------------------
    check(vacuity_ok, "vacuity: every seed produced corps, including named (non-background) ones");
    check(ring_ok, "attribution: the decision-ring drain never saturated (action counts trustworthy)");
    {
        const seed_result& first = results.front();
        seed_result again = run_seed(first.seed, warm_ticks, spectating, lean, reg);
        bool identical = again.final_hash == first.final_hash
                      && again.corps.size() == first.corps.size();
        if (identical)
            for (std::size_t i = 0; i < again.corps.size(); ++i)
                if (again.corps[i].bal_close != first.corps[i].bal_close)
                    identical = false;
        check(identical, "determinism: first seed re-run reproduces state_hash and every closing balance");
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
