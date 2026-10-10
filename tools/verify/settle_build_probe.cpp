// ---------------------------------------------------------------------------
// settle_build_probe — BL-1234 (settle scorer starves), diagnosis probe
// ---------------------------------------------------------------------------
// QUESTION. processor_placement_probe (BL-1233) found generation places almost
// no starved processors, but the scored-utility build candidate (corp_ai.cpp,
// the processing-facility candidate, BL-439 / BL-1187) adds 220-270 through
// the 12-tick settle and 150+ of them are input-starved at handoff, mostly on
// PROCESSED inputs. Which half of BL-1187's OBTAINABLE test (input_reach.hpp)
// admits them — (1) STOCK, the corp's pool plus the market's shelf covering one
// tick's need at t_idle, or (2) SUPPLY, spare output within reach — and does
// the per-corp read miss draws it cannot see (processors admitted the same tick
// or still under construction, which `building_draw` counts as nothing)?
//
// METHOD. The world is built as processor_placement_probe builds it
// (build_app_start_world, 12 settle ticks, seat_player_corporation), then 20
// play ticks. The tick is run_settle_tick's body restated call for call, so the
// probe can READ between run_economy_step (where the scorer runs, phase 9) and
// the dispatch. `--check` runs a twin world through run_settle_tick itself and
// requires an identical state_hash at every step.
//
// At that read point, for every processing facility the step BUILT (the
// report's `built` agency events), each input is re-asked exactly as the
// candidate asks it (batches = base_rate x 0.5, no self, the corp's pool for
// the tile, a fresh input_reach given this tick's report):
//   pool, shelf (and whether the ceiling admits it), need and t_idle floor,
//   STOCK clause pass, SUPPLY clause pass (input_supply_covers), reachable spare;
//   PIPELINE: other processors under construction (incl. same-step builds) in
//   the reach set's consumer markets that draw the input — the draw the spare
//   cannot see — and whether SUPPLY still passes with that draw netted out.
// The read point is AFTER phases 10-13 (battles, march, unit and building
// upkeep) and after every later corp's commands in the walk, so the re-ask is
// a close but not exact copy of the scorer's moment; `reproduced` counts how
// many builds the re-ask still admits.
//
// OUTCOME: the building's row at handoff (the settle's last report) and at t20
// (play tick 20): running, starved (has_limiting and not active, its limiting
// input), idle, under construction, gone.
//
// A PURE READER: nothing here writes the world except the tick itself.
//
// Usage (repo root): build_gen/verify/settle_build_probe.exe [--seeds a,b] [--list] [--check]
// Build:  ./tools/verify/build_lua_harness.sh settle_build_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/budget_system.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/input_reach.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_step.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/standing.hpp"
#include "world/supply_system.hpp"
#include "world/trade.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr int k_play_ticks = 20;

std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

bool is_extractable(std::size_t g)
{
    for (const resource_type r : placement_rules::k_extractable)
        if (static_cast<std::size_t>(r) == g) return true;
    return false;
}

/// run_settle_tick's body, restated so the probe can read after the economy step.
template <class Read>
economy_report probe_tick(world& w, const recipe_registry& reg, int econ_step, bool spectating, Read&& read)
{
    w.current_econ_tick = econ_step;
    lp_pool_map tick_lp_pools;
    advance_convoys(w);
    credit_arrived_convoys(w, 0);
    economy_report report = run_economy_step(w, reg, spectating, &tick_lp_pools);
    read(report); // READ ONLY
    run_trades(w, reg, report, &tick_lp_pools); // BL-1266: the trade pass (was dispatch_convoys), before the clear
    auto flows = clear_markets(w, reg, report);
    apply_budget(w, reg, flows, report.workforce_contention, &report.budgets,
                 &report.buildings, &report.building_labour);
    run_nation_step(w, reg, report, w.current_econ_tick);
    advance_tech_gates(w);
    (void)compute_corp_standings(w, flows);
    run_firm_exits(w, reg.firm_exit(), &report.firm_exits);
    return report;
}

struct input_rec
{
    std::size_t r = 0;
    float need = 0, floor = 0, pool = 0, shelf = 0;
    bool  shelf_ok = false, stock = false, supply = false;
    float spare = 0;
    int   pipe_n = 0, same_n = 0;      ///< pipeline processors (all) / of them built this same step
    float pipe_need = 0, same_need = 0;
    bool  supply_net = false;          ///< SUPPLY with the pipeline's draw netted out
};

enum status : int { st_run = 0, st_starved, st_idle, st_decom, st_building, st_gone, st_n };
const char* k_st[st_n] = {"running", "starved", "idle", "decommissioned", "building", "gone"};

/// A build's state at one read (handoff or t20), and its limiting input's market then.
struct outcome_rec
{
    int st = st_gone;
    std::size_t lim = resource_count;
    float run = 0, shelf = 0, pool = 0, spare = 0, floor = 0;
    bool shelf_ok = false;
};

struct build_rec
{
    int step = 0;
    entity_id bid = null_entity, corp = null_entity, market = null_entity;
    uint16_t recipe = no_recipe;
    bool reproduced = false;
    std::vector<input_rec> ins;
    outcome_rec h, t; ///< at handoff (settle builds only) and at t20
};

/// One (market, good) shelf per step: what stands on it and what draws it.
struct trace_row
{
    int step = 0;
    float shelf = 0; bool shelf_ok = false;
    int sites = 0;   float site_draw = 0; ///< processors under construction in the market whose basket takes g; per-tick material draw
    int procs = 0;   float proc_draw = 0; ///< standing laboured processors in the market drawing g; nominal per-tick draw
    float spare = 0;                      ///< reachable spare (play reading)
};

/// Pooled over seeds.
struct pool_t
{
    int builds_settle = 0, builds_play = 0, reproduced = 0;
    std::array<int, st_n> st_h{}, st_t{};
    // starved at handoff (settle builds): by admit clause of the limiting input
    // 0 stock only, 1 supply only, 2 both, 3 neither (not reproduced)
    std::array<int, 4> clause{};
    std::array<int, 4> clause_t{}; // same, at t20 (builds through tick 28)
    int processed = 0, raw = 0, grid = 0;
    int pipe_any = 0, pipe_same = 0, net_fails = 0, net_fails_supply_admit = 0;
    std::map<std::size_t, std::array<int, 4>> by_good; ///< limiting good -> clause counts
    std::vector<float> stock_cover_ticks;             ///< (pool+shelf)/need for stock-admitted limiting inputs
    int stock_admit = 0;
    int stock_ceiled_h = 0, stock_drained_h = 0, stock_other_h = 0; ///< their limiting shelf at handoff
    std::vector<float> shelf_dec, shelf_h;            ///< that shelf at decision / at handoff
    // running counterparts for contrast
    std::array<int, 4> clause_run{};
};

int clause_of(const input_rec& i)
{
    if (i.stock && i.supply) return 2;
    if (i.stock) return 0;
    if (i.supply) return 1;
    return 3;
}
const char* k_clause[4] = {"stock only", "supply only", "both", "neither (not reproduced)"};

const input_rec* find_in(const build_rec& b, std::size_t r)
{
    for (const input_rec& i : b.ins) if (i.r == r) return &i;
    return nullptr;
}

std::vector<std::uint32_t> parse_seeds(const std::string& s)
{
    std::vector<std::uint32_t> out;
    std::size_t a = 0;
    while (a <= s.size())
    {
        const std::size_t b = s.find(',', a);
        const std::string tok = s.substr(a, b == std::string::npos ? std::string::npos : b - a);
        if (!tok.empty()) out.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    bool list = false, check = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--list")) list = true;
        else if (!std::strcmp(argv[i], "--check")) check = true;
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seeds(argv[++i]);
        else { std::fprintf(stderr, "usage: settle_build_probe [--seeds a,b] [--list] [--check]\n"); return 2; }
    }
    std::printf("settle_build_probe - BL-1234: what admits the settle scorer's starved processors\n");
    pool_t P;
    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params wp;
        wp.seed = seed;
        auto start = std::make_unique<app_start_world>();
        try { build_app_start_world(lua, wp, *start); }
        catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); continue; }
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        std::unique_ptr<world> twin;
        if (check) twin = std::make_unique<world>(w);

        const float t_idle = reg.t_idle();
        const float batches = reg.economics(building_type::processing_facility).base_rate * 0.5f;
        std::vector<build_rec> recs;
        std::set<std::pair<entity_id, std::size_t>> watch;
        std::map<std::pair<entity_id, std::size_t>, std::vector<trace_row>> trace;
        int hash_mismatch = 0;

        for (int step = 0; step < k_campaign_settle_ticks + k_play_ticks; ++step)
        {
            const bool settle = step < k_campaign_settle_ticks;
            if (step == k_campaign_settle_ticks)
            {
                seat_player_corporation(w, seed, start->land.search.winner_score);
                if (twin) seat_player_corporation(*twin, seed, start->land.search.winner_score);
            }
            auto read = [&](const economy_report& report) {
                // OUTCOME reads, at the handoff tick (the settle's last) and at t20,
                // on the world after this step's production: the build's state and
                // its limiting input's market then.
                const bool at_h = step == k_campaign_settle_ticks - 1;
                const bool at_t = step == k_campaign_settle_ticks + k_play_ticks - 1;
                if (at_h || at_t)
                {
                    std::map<entity_id, const building_report*> rows;
                    for (const building_report& r : report.buildings) rows[r.building] = &r;
                    input_reach jr = make_input_reach(w, reg);
                    jr.report = &report;
                    for (build_rec& b : recs)
                    {
                        if (at_h && b.step >= k_campaign_settle_ticks) continue;
                        outcome_rec& o = at_h ? b.h : b.t;
                        const auto bit = w.buildings.find(b.bid);
                        if (bit == w.buildings.end()) { o.st = st_gone; continue; }
                        if (bit->second.ticks_remaining > 0) { o.st = st_building; continue; }
                        if (bit->second.decommissioned) { o.st = st_decom; continue; }
                        const auto ri = rows.find(b.bid);
                        if (ri == rows.end()) { o.st = st_idle; continue; }
                        o.run = ri->second->run;
                        if (ri->second->active) o.st = st_run;
                        else if (ri->second->has_limiting) o.st = st_starved;
                        else { o.st = st_idle; continue; }
                        if (!ri->second->has_limiting) continue;
                        const std::size_t g = std::size_t(ri->second->limiting_input);
                        o.lim = g;
                        const auto mit = w.markets.find(b.market);
                        if (mit != w.markets.end())
                        {
                            o.shelf = std::max(0.0f, mit->second.inventory[g]);
                            o.shelf_ok = shelf_admits(mit->second, g, jr.reservation_mult, true);
                        }
                        if (const stockpile_component* pl = nullptr) // BL-1265: corporations hold no pools
                            o.pool = std::max(0.0f, pl->quantities[g]);
                        o.spare = reachable_supply(w, reg, jr, b.market, g, b.bid).spare;
                        for (const input_rec& i : b.ins) if (i.r == g) o.floor = i.floor;
                    }
                }
                std::vector<entity_id> built;
                for (const agency_event& e : report.agency_events)
                {
                    if (e.what != agency_event::kind::built) continue;
                    const auto bit = w.buildings.find(e.building);
                    if (bit == w.buildings.end() || bit->second.type != building_type::processing_facility) continue;
                    built.push_back(e.building);
                }
                // TRACE every (market, input) a scorer build has asked about so far
                if (!watch.empty())
                {
                    input_reach tr = make_input_reach(w, reg);
                    tr.report = &report;
                    std::map<std::pair<entity_id, std::size_t>, trace_row> now;
                    for (const auto& k : watch)
                    {
                        trace_row& t = now[k];
                        t.step = step;
                        const auto mit = w.markets.find(k.first);
                        if (mit != w.markets.end())
                        {
                            t.shelf = std::max(0.0f, mit->second.inventory[k.second]);
                            t.shelf_ok = shelf_admits(mit->second, k.second, tr.reservation_mult, true);
                        }
                        t.spare = reachable_supply(w, reg, tr, k.first, k.second, null_entity).spare;
                    }
                    std::vector<entity_id> ids;
                    for (const auto& kv : w.buildings) ids.push_back(kv.first);
                    std::sort(ids.begin(), ids.end());
                    for (const entity_id bid : ids)
                    {
                        const building_component& b = w.buildings.at(bid);
                        if (b.decommissioned) continue;
                        const entity_id m = market_for_tile(w, b.tile);
                        if (b.ticks_remaining > 0)
                        {
                            const float dur = reg.economics(b.type).build_duration_ticks;
                            if (!(dur > 0.0f)) continue;
                            const auto& row = reg.resource_build_cost_for(b.type, b.target_resource, b.recipe);
                            for (std::size_t g = 0; g < resource_count; ++g)
                                if (row[g] > 0.0f)
                                    if (auto it = now.find({m, g}); it != now.end()) { ++it->second.sites; it->second.site_draw += row[g] / dur; }
                            continue;
                        }
                        for (std::size_t g = 0; g < resource_count; ++g)
                        {
                            const float d = building_draw(reg, b, g);
                            if (d > 0.0f)
                                if (auto it = now.find({m, g}); it != now.end()) { ++it->second.procs; it->second.proc_draw += d; }
                        }
                    }
                    for (const auto& [k, t] : now) trace[k].push_back(t);
                }
                if (built.empty()) return;
                std::sort(built.begin(), built.end());
                input_reach ir = make_input_reach(w, reg);
                ir.report = &report;
                // under-construction processors: the pipeline
                std::vector<entity_id> pipe;
                for (const auto& [bid, b] : w.buildings)
                    if (b.type == building_type::processing_facility && b.ticks_remaining > 0 && !b.decommissioned)
                        pipe.push_back(bid);
                std::sort(pipe.begin(), pipe.end());
                const std::set<entity_id> same(built.begin(), built.end());
                std::map<entity_id, entity_id> owner;
                for (const auto& [cid, cc] : w.corporations) for (const entity_id a : cc.assets) owner.emplace(a, cid);

                for (const entity_id bid : built)
                {
                    const building_component& b = w.buildings.at(bid);
                    const recipe* rc = reg.get_recipe(b.recipe);
                    if (!rc) continue;
                    build_rec br;
                    br.step = step; br.bid = bid; br.recipe = b.recipe;
                    br.corp = owner.count(bid) ? owner[bid] : null_entity;
                    br.market = market_for_tile(w, b.tile);
                    const stockpile_component* pl = nullptr; // BL-1265: corporations hold no pools
                    std::array<float, resource_count> cost{};
                    br.reproduced = recipe_inputs_obtainable(w, reg, ir, br.market, pl, *rc, batches, null_entity, cost);
                    const auto mit = w.markets.find(br.market);
                    const market_component* mk = mit == w.markets.end() ? nullptr : &mit->second;
                    for (std::size_t r = 0; r < resource_count; ++r)
                    {
                        if (!(rc->inputs[r] > 0.0f)) continue;
                        input_rec in;
                        in.r = r;
                        in.need = rc->inputs[r] * batches;
                        in.floor = in.need * t_idle;
                        in.pool = pl ? std::max(0.0f, pl->quantities[r]) : 0.0f;
                        in.shelf_ok = mk && shelf_admits(*mk, r, ir.reservation_mult, true);
                        in.shelf = mk ? std::max(0.0f, mk->inventory[r]) : 0.0f;
                        in.stock = input_obtainable(w, reg, ir, br.market, pl, r, in.need, null_entity, false).obtainable;
                        in.supply = input_supply_covers(w, reg, ir, br.market, r, in.need, null_entity);
                        in.spare = reachable_supply(w, reg, ir, br.market, r, null_entity).spare;
                        // the reach set's consumer markets (the draw the spare nets)
                        std::set<entity_id> dm{br.market};
                        if (const auto si = ir.supply_memo.find({br.market, r}); si != ir.supply_memo.end())
                            for (const auto& d : si->second.draws) dm.insert(d.market);
                        for (const entity_id pid : pipe)
                        {
                            if (pid == bid) continue;
                            const building_component& pb = w.buildings.at(pid);
                            const recipe* prc = reg.get_recipe(pb.recipe);
                            if (!prc || !(prc->inputs[r] > 0.0f)) continue;
                            if (!dm.count(market_for_tile(w, pb.tile))) continue;
                            const float nd = prc->inputs[r] * judged_batches(reg, pb);
                            ++in.pipe_n; in.pipe_need += nd;
                            if (same.count(pid)) { ++in.same_n; in.same_need += nd; }
                        }
                        in.supply_net = in.supply && (in.spare - in.pipe_need) >= in.floor;
                        br.ins.push_back(in);
                        watch.insert({br.market, r});
                    }
                    if (settle) ++P.builds_settle; else ++P.builds_play;
                    if (br.reproduced) ++P.reproduced;
                    recs.push_back(std::move(br));
                }
            };
            economy_report rep = probe_tick(w, reg, step, settle, read);
            if (twin)
            {
                (void)run_settle_tick(*twin, reg, step, 0, settle);
                if (twin->state_hash(step) != w.state_hash(step)) ++hash_mismatch;
            }
            (void)rep;
        }

        // per-seed tallies
        int ns = 0, n_starved_h = 0, n_starved_t = 0;
        std::array<int, 4> cl{};
        std::array<int, st_n> sh{}, stt{};
        for (const build_rec& b : recs)
        {
            ++stt[b.t.st];
            if (b.step >= k_campaign_settle_ticks) continue;
            ++ns; ++sh[b.h.st]; ++P.st_h[b.h.st]; ++P.st_t[b.t.st];
            if (b.h.st == st_run)
            {
                // contrast: the clause that admitted a running build's tightest input
                const input_rec* worst = nullptr;
                for (const input_rec& i : b.ins) if (!worst || clause_of(i) < clause_of(*worst)) worst = &i;
                if (worst) ++P.clause_run[clause_of(*worst)];
            }
            if (b.h.st != st_starved) continue;
            ++n_starved_h;
            const input_rec* in = find_in(b, b.h.lim);
            if (!in) continue;
            const int c = clause_of(*in);
            ++cl[c]; ++P.clause[c]; ++P.by_good[b.h.lim][c];
            if (reg.grid_goods().grid(b.h.lim)) ++P.grid;
            else if (is_extractable(b.h.lim)) ++P.raw;
            else ++P.processed;
            if (in->pipe_n > 0) ++P.pipe_any;
            if (in->same_n > 0) ++P.pipe_same;
            if (in->supply && !in->supply_net) ++P.net_fails;
            if (c == 1 && !in->supply_net) ++P.net_fails_supply_admit;
            if (in->stock)
            {
                ++P.stock_admit;
                P.stock_cover_ticks.push_back(in->need > 0 ? (in->pool + (in->shelf_ok ? in->shelf : 0.0f)) / in->need : 0.0f);
                // what became of that stock by handoff: the ceiling now refuses the
                // shelf, or the shelf (with the pool) fell below the run's floor
                if (!b.h.shelf_ok) ++P.stock_ceiled_h;
                else if (b.h.shelf + b.h.pool < in->floor) ++P.stock_drained_h;
                else ++P.stock_other_h;
                P.shelf_dec.push_back(in->shelf); P.shelf_h.push_back(b.h.shelf);
            }
        }
        for (const build_rec& b : recs)
            if (b.t.st == st_starved && b.step + 3 <= k_campaign_settle_ticks + k_play_ticks - 1)
            {
                ++n_starved_t;
                if (const input_rec* in = find_in(b, b.t.lim)) ++P.clause_t[clause_of(*in)];
            }

        std::printf("\n-- seed %u: scorer processor builds settle %d, play %d; re-ask reproduced %d of %zu%s\n",
                    seed, ns, int(recs.size()) - ns,
                    int(std::count_if(recs.begin(), recs.end(), [](const build_rec& b) { return b.reproduced; })),
                    recs.size(), check ? (hash_mismatch ? "  CHECK: HASH MISMATCH" : "  CHECK: hash identical every step") : "");
        if (check && hash_mismatch) std::printf("   %d steps differ from run_settle_tick\n", hash_mismatch);
        std::printf("   settle builds at handoff:");
        for (int i = 0; i < st_n; ++i) std::printf(" %s %d", k_st[i], sh[i]);
        std::printf("\n   all builds at t20:");
        for (int i = 0; i < st_n; ++i) std::printf(" %s %d", k_st[i], stt[i]);
        std::printf("\n   starved at handoff %d, limiting input admitted by:", n_starved_h);
        for (int i = 0; i < 4; ++i) std::printf(" [%s] %d", k_clause[i], cl[i]);
        std::printf("\n   builds per step:");
        {
            std::map<int, int> per;
            for (const build_rec& b : recs) ++per[b.step];
            for (const auto& [s, n] : per) std::printf(" %d:%d", s, n);
        }
        std::printf("\n");
        {
            std::map<std::pair<entity_id, std::size_t>, int> hot;
            for (const build_rec& b : recs)
                if (b.step < k_campaign_settle_ticks && b.h.st == st_starved) ++hot[{b.market, b.h.lim}];
            for (const auto& [k, n] : hot)
            {
                std::printf("   TRACE market %u %s (%d starved at handoff): step shelf[ceiled] | sites(material draw/t) | standing procs(draw/t) | spare | scorer builds this step asking it\n",
                            unsigned(k.first), gname(k.second).c_str(), n);
                for (const trace_row& t : trace[k])
                {
                    int asked = 0;
                    for (const build_rec& b : recs) if (b.step == t.step && b.market == k.first) for (const input_rec& i : b.ins) if (i.r == k.second) ++asked;
                    std::printf("     t%-2d %9.1f%s | %3d (%7.2f) | %3d (%7.2f) | %8.2f | %d\n", t.step, t.shelf, t.shelf_ok ? "  " : " C",
                                t.sites, t.site_draw, t.procs, t.proc_draw, t.spare, asked);
                }
            }
        }
        int shown = 0;
        for (const build_rec& b : recs)
        {
            if (!list && (b.step >= k_campaign_settle_ticks || b.h.st != st_starved || shown >= 12)) continue;
            ++shown;
            const recipe* rc = reg.get_recipe(b.recipe);
            std::printf("   t%-2d corp %-5u %-22s h:%-8s t20:%-8s", b.step, unsigned(b.corp), rc ? rc->name.c_str() : "?",
                        b.step < k_campaign_settle_ticks ? k_st[b.h.st] : "-", k_st[b.t.st]);
            if (b.h.st == st_starved)
                std::printf(" lim %s (handoff: shelf %.1f%s pool %.1f spare %.1f)", gname(b.h.lim).c_str(), b.h.shelf,
                            b.h.shelf_ok ? "" : " ceiled", b.h.pool, b.h.spare);
            std::printf("%s\n", b.reproduced ? "" : " [re-ask refuses]");
            for (const input_rec& i : b.ins)
                std::printf("       %-16s need %6.2f floor %6.2f | pool %7.2f shelf %7.2f%s | STOCK %d SUPPLY %d spare %7.2f | pipe %d (%.2f), same-step %d (%.2f) -> net %d\n",
                            gname(i.r).c_str(), i.need, i.floor, i.pool, i.shelf, i.shelf_ok ? "" : " (ceiled)",
                            int(i.stock), int(i.supply), i.spare, i.pipe_n, i.pipe_need, i.same_n, i.same_need, int(i.supply_net));
        }
        std::fflush(stdout);
    }

    std::printf("\n== POOLED (%zu seeds) ==\n", seeds.size());
    std::printf("   scorer processor builds: settle %d, play %d; the re-ask at the read point reproduces %d of %d\n",
                P.builds_settle, P.builds_play, P.reproduced, P.builds_settle + P.builds_play);
    std::printf("   settle builds at handoff:"); for (int i = 0; i < st_n; ++i) std::printf(" %s %d", k_st[i], P.st_h[i]);
    std::printf("\n   settle builds at t20:");      for (int i = 0; i < st_n; ++i) std::printf(" %s %d", k_st[i], P.st_t[i]);
    int sh = 0; for (int i = 0; i < 4; ++i) sh += P.clause[i];
    std::printf("\n   STARVED AT HANDOFF %d: limiting input admitted by", sh);
    for (int i = 0; i < 4; ++i) std::printf(" [%s] %d (%.0f%%)", k_clause[i], P.clause[i], sh ? 100.0 * P.clause[i] / sh : 0.0);
    std::printf("\n     limiting input processed %d, raw %d, grid %d\n", P.processed, P.raw, P.grid);
    std::printf("     pipeline drawing the same input in the reach set at decision: %d; of them built the same step: %d\n", P.pipe_any, P.pipe_same);
    std::printf("     SUPPLY passed but fails with the pipeline netted: %d (of supply-only admits: %d)\n", P.net_fails, P.net_fails_supply_admit);
    if (!P.stock_cover_ticks.empty())
    {
        std::vector<float> v = P.stock_cover_ticks; std::sort(v.begin(), v.end());
        std::printf("     STOCK-admitted (%d): ticks of full-run need the pool+shelf held at decision: median %.2f, p25 %.2f, p75 %.2f, max %.2f\n",
                    P.stock_admit, v[v.size() / 2], v[v.size() / 4], v[(3 * v.size()) / 4], v.back());
    }
    if (!P.shelf_dec.empty())
    {
        std::vector<float> a = P.shelf_dec, b = P.shelf_h; std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
        std::printf("     STOCK-admitted at handoff: shelf ceiled %d, shelf+pool below the floor %d, other %d; limiting shelf median at decision %.1f -> at handoff %.1f\n",
                    P.stock_ceiled_h, P.stock_drained_h, P.stock_other_h, a[a.size() / 2], b[b.size() / 2]);
    }
    std::printf("   RUNNING at handoff, tightest input admitted by:");
    for (int i = 0; i < 4; ++i) std::printf(" [%s] %d", k_clause[i], P.clause_run[i]);
    int st = 0; for (int i = 0; i < 4; ++i) st += P.clause_t[i];
    std::printf("\n   STARVED AT t20 (all scorer builds complete by then) %d:", st);
    for (int i = 0; i < 4; ++i) std::printf(" [%s] %d", k_clause[i], P.clause_t[i]);
    std::printf("\n   by limiting good (stock / supply / both / neither):\n");
    std::vector<std::pair<int, std::size_t>> order;
    for (const auto& [g, a] : P.by_good) order.push_back({a[0] + a[1] + a[2] + a[3], g});
    std::sort(order.rbegin(), order.rend());
    for (const auto& [n, g] : order)
    {
        const auto& a = P.by_good[g];
        std::printf("     %-18s %4d : %d / %d / %d / %d%s\n", gname(g).c_str(), n, a[0], a[1], a[2], a[3],
                    is_extractable(g) ? " (raw)" : "");
    }
    return 0;
}
