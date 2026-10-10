// ---------------------------------------------------------------------------
// starve_trace_probe — BL-1217 D7 (G1 plants running): the mid-chain starvation
// that kills plants between the handoff and play tick 50.
// ---------------------------------------------------------------------------
// QUESTION. Of the processors running at the handoff, ~28% are not running at
// play tick 50 (handoff_idle_probe round 4, decay.*). Most of the loss is an
// input starvation (starved now, or mothballed / idled after starving). For each
// starving input: WHY does a running plant stop getting it in play?
//
// THE WORLD. Seated exactly as handoff_idle_probe / market_viability seat it:
// build_app_start_world, the 12-tick settle, seat_player_corporation, then PLAY
// ticks as run_app_live_window steps them (to tick 50).
//
// A PURE READER. One after_lap hook, play ticks only, nothing written. Per
// market, per tick, per good:
//   conv lap   shelf_pre (what the econ step's draws see), posted / base, the
//              fair-price ceiling's verdict, every (corp, market) pool summed,
//              convoy arrivals (ids gone since the last tick's last lap) and
//              departures (ids new this lap).
//   econ lap   shelf after the processor + construction draws; price / base
//              (the prices the loss reflex values at).
//   clear lap  shelf after listing / households / background / spoilage;
//              household_fill, background_fill, demand, hauler_want.
//   budget lap shelf after the nation's draws (network upkeep, space programme).
//   after tick production of the good at the market (processor outputs +
//              extraction), and its producers by state (run / idle / decom).
// Per processor running at the handoff, per tick: state, run, limiting input,
// full-run need per input, own pool per input; decommission tick / by / why.
//
// CLASSIFICATION. A plant is LOST when it does not run at tick 50. Its final
// non-running streak starts at T0 (the tick after it last ran). T* = the first
// input-starved tick in [T0, 50] (a decommissioned plant reports no rows, so the
// streak is read up to the decommission); g* = that tick's limiting input. With
// gap = t_idle x need - own pool at T*, M = the plant's market, precedence:
//   c ceiling        shelf_pre(T*) >= gap but the ceiling refuses the shelf
//   d.procs          shelf_pre(T*) >= gap, admitted: other processors / sites
//                    drew it in the pro-rata turn (the plant was outdrawn)
//   d.<taker>        tick T*-1: shelf after the draws + listings >= gap, but
//                    households / background / nation / spoilage / other took it
//                    before T*'s draw (the largest taker names it)
//   f.pool_held      other pools at M hold >= gap of g*, not listed
//   a.producers_died production of g* at M (or, where M made none at the
//                    handoff, on the body) fell below half its ticks 1-3 mean;
//                    the dying producers' own state is the cascade
//   e.opening_stock  production at M never covered the gap (ticks 1-3) and the
//                    plant ran on stock (its pool or the shelf); run-out = T*.
//                    .body_supply where the body's other markets make >= gap
//   b.reach          M made none at any point but the body's other markets made
//                    >= gap (and the plant had no opening stock)
//   f.exported       M made it but convoys hauled >= gap out of M
//   f.other
// Non-starvation losses: f.notstarved.<decay class>.
// The mothballed "ran/inputs_over_rev" plants: unit input cost and revenue at
// tick 1 vs the decommission tick (econ-lap prices) -- input_rose /
// output_fell / both / under_water_from_t1 -- and the moving good's demand /
// supply at both ticks.
// SECOND AXIS (root.*, root_cause.*): M's stock (shelf + pools) drawn down from
// the handoff / never stocked / held; body_has = another market on the body lists
// >= gap under its ceiling at T*. reach.*: the shelf-export dispatcher's best
// class into M at T* (export_refusal.hpp; notshort = M's shelf covered its live
// consumers' need, or nothing was read). bal.<good>.*: M's per-tick means over
// ticks 1..T*-1 (production, arrivals, departures, econ-lap draws, sites' and
// processors' need, upkeep wants, households, background, nation, stock).
// sites_tend.*: construction sites on M drawing g* at the LAST tick, by building
// and owner class (background / rival / player).
// --cf bg_off | ceiling_off: registry measurement switches applied at the
// handoff only (play), for counterfactual G1 (g1.run / g1.built at the last tick).
// TRAJECTORY. Per starving good, per tick, pooled over every (seed, M) where a
// lost plant starved on it: production at M and on the body, shelf_pre,
// price/base, markets over the ceiling, the lost plants' bids filled / unfilled
// / silenced, hauler_want, producers run / idle / decom at M and body.
//
// Usage: build_gen/verify/starve_trace_probe.exe [--seeds a,b] [--ticks N] [--examples N]
// Build:  ./tools/verify/build_lua_harness.sh starve_trace_probe
// Each seed prints `COUNTS <seed> <key> <value>`; the pooled block sums them.
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "export_refusal.hpp"
#include "world/building_profit.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
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

constexpr int R = static_cast<int>(resource_count);
constexpr int k_t50 = 50;
int g_examples = 3;
int g_ticks = k_t50;
/// --cf: a MEASUREMENT switch applied to the registry at the handoff (play only),
/// never a behaviour change: bg_off = the background pull stops consuming;
/// ceiling_off = reservation_mult 0 (the authored off switch: draws buy any shelf).
std::string g_cf;

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
        if (!tok.empty()) out.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

// --- processor states: market_viability's classify, verbatim -----------------
enum proc_state { ps_run = 0, ps_input, ps_nolab, ps_unsup, ps_decom, ps_build, ps_other, ps_gone, ps_count };
const char* k_state_name[ps_count] = {"run", "input", "nolab", "unsupplied", "decom", "build", "other", "gone"};

proc_state classify_row(const building_component& b, const building_report* rep, const recipe_registry& reg)
{
    if (reg.get_recipe(b.recipe) == nullptr)  return ps_other;
    if (rep == nullptr)                       return ps_other;
    if (rep->active)                          return ps_run;
    if (rep->effective_workforce <= 0.0f)     return ps_nolab;
    if (building_supply_scalar(b) <= 0.0f || b.workforce_target <= 0.0f) return ps_unsup;
    if (rep->has_limiting)                    return ps_input;
    return ps_other;
}

proc_state classify(const building_component& b, const building_report* rep, const recipe_registry& reg)
{
    if (b.ticks_remaining > 0) return ps_build;
    if (b.decommissioned)      return ps_decom;
    return classify_row(b, rep, reg);
}

const building_report* row_of(const economy_report& rep, entity_id bid)
{
    const auto it = rep.building_row.find(bid);
    return it == rep.building_row.end() ? nullptr : &rep.buildings[it->second];
}

struct counts
{
    std::map<std::string, long> c;
    void add(const std::string& k, long v = 1) { c[k] += v; }
};

using arr = std::array<float, resource_count>;

/// One market, one play tick.
struct mrec
{
    arr pre{}, econ{}, clr{}, bud{};      ///< shelf at the end of each lap
    arr posted{}, base{}, price{};        ///< conv lap posted / base; econ lap price
    arr pools{};                           ///< every (corp, market) pool, conv lap
    arr arrived{}, departed{}, departed_shelf{};             ///< convoy cargo arriving at / leaving this market
    arr hh{}, bg{}, dem{}, sup{}, hw{};    ///< clear lap registers
    arr prod{};                            ///< production of the good here this tick
    arr p_run{}, p_idle{}, p_decom{};      ///< producers of the good here, by state
    arr site_need{}, proc_need{}, upk{};   ///< conv lap: sites' material need per tick, live processors' full-run need; report upkeep wants
    std::array<bool, resource_count> admits{};
    arr xbest = [] { arr a{}; a.fill(-2.0f); return a; }(); ///< econ lap: best export_refusal class to here (-2 not short / not read, -1 no source)
};

/// One processor running at the handoff, one play tick.
struct prec
{
    int state = ps_other;
    float run = 0.0f;
    int lim = -1;
    arr need{}, pool{};
    bool row = false;
};

struct decom_info
{
    int tick = -1;
    const char* by = "unknown";
    int last = -1;    ///< last row state
    int lim = -1;     ///< last row's limiting input (when input-starved)
    std::string why;  ///< ran/inputs_over_rev ...
    float rev = 0, inp = 0;
};

struct convoy_seen { entity_id src = null_entity, dst = null_entity; std::size_t g = 0; float q = 0; };

struct probe
{
    const recipe_registry* reg = nullptr;
    int lap_pre = -1, lap_econ = -1, lap_clear = -1, lap_bud = -1;
    bool armed = false;
    int tick = 0;
    std::map<entity_id, mrec>* cur = nullptr;   ///< this tick's per-market record
    std::map<entity_id, std::map<entity_id, arr>>* own_pool = nullptr; ///< plant -> (pre) own pool; filled at conv lap
    const std::vector<entity_id>* plants = nullptr;
    const std::map<entity_id, entity_id>* owner = nullptr;
    std::map<uint32_t, convoy_seen> convoys;      ///< as at the end of the last tick's last lap
    const std::set<std::pair<entity_id, std::size_t>>* tracked = nullptr; ///< (market, input) of the plants
};

void after_lap(const world& w, int lap, void* ctx)
{
    probe& p = *static_cast<probe*>(ctx);
    if (!p.armed) return;
    const float res_mult = p.reg->price_band().reservation_mult;
    std::map<entity_id, mrec>& M = *p.cur;
    if (lap == p.lap_pre)
    {
        for (const auto& [mid, mc] : w.markets)
        {
            mrec& r = M[mid];
            for (std::size_t g = 0; g < resource_count; ++g)
            {
                r.pre[g] = mc.inventory[g];
                r.posted[g] = posted_price(mc, g);
                r.base[g] = mc.base_price[g];
                r.admits[g] = shelf_admits(mc, g, res_mult, /*off_buys=*/true);
            }
        }
        for (const auto& [key, pool] : w.corp_market_pools)
        {
            const auto it = M.find(key.second);
            if (it == M.end()) continue;
            for (std::size_t g = 0; g < resource_count; ++g) it->second.pools[g] += std::max(0.0f, pool.quantities[g]);
        }
        std::set<uint32_t> now;
        for (const convoy_component& c : w.convoys) now.insert(c.id);
        for (const auto& [id, cs] : p.convoys)
            if (!now.count(id))
                if (auto it = M.find(cs.dst); it != M.end()) it->second.arrived[cs.g] += cs.q;
        // who will draw this tick: construction sites' material rows, live processors' full runs
        {
            const float rate = p.reg->economics(building_type::processing_facility).base_rate;
            for (const auto& [bid, b] : w.buildings)
            {
                const auto it = M.find(market_for_tile(w, b.tile));
                if (it == M.end()) continue;
                if (b.ticks_remaining > 0)
                {
                    const float dur = p.reg->economics(b.type).build_duration_ticks;
                    if (!(dur > 0.0f)) continue;
                    const auto& cost = p.reg->resource_build_cost_for(b.type, b.target_resource, b.recipe);
                    for (std::size_t g = 0; g < resource_count; ++g) it->second.site_need[g] += cost[g] / dur;
                    continue;
                }
                if (b.decommissioned || b.type != building_type::processing_facility) continue;
                const recipe* rc = p.reg->get_recipe(b.recipe);
                if (!rc) continue;
                const float bf = rate * b.workforce_assigned * std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f) * building_supply_scalar(b);
                for (std::size_t g = 0; g < resource_count; ++g) it->second.proc_need[g] += rc->inputs[g] * bf;
            }
        }
        // the plants' own pools
        for (const entity_id bid : *p.plants)
        {
            const auto bi = w.buildings.find(bid);
            const auto oi = p.owner->find(bid);
            if (bi == w.buildings.end() || oi == p.owner->end()) continue;
            const entity_id mid = market_for_tile(w, bi->second.tile);
            const stockpile_component* pool = w.find_pool(oi->second, mid);
            arr a{};
            if (pool) for (std::size_t g = 0; g < resource_count; ++g) a[g] = pool->quantities[g];
            (*p.own_pool)[bid][0] = a; // slot 0: this tick
        }
        return;
    }
    if (lap == p.lap_econ)
    {
        for (const auto& [mid, mc] : w.markets)
        {
            mrec& r = M[mid];
            for (std::size_t g = 0; g < resource_count; ++g) { r.econ[g] = mc.inventory[g]; r.price[g] = mc.price[g]; }
        }
        // departures: convoys dispatched this tick (dispatch runs inside this lap)
        for (const convoy_component& c : w.convoys)
            if (!p.convoys.count(c.id))
                if (auto it = M.find(c.source_market); it != M.end())
                {
                    const std::size_t g = static_cast<std::size_t>(c.cargo_resource);
                    if (c.corp == null_entity) it->second.departed_shelf[g] += c.cargo_qty;
                    else it->second.departed[g] += c.cargo_qty;
                }
        // why no shelf on the body ships to a short tracked market: the
        // export dispatcher's own rules (export_refusal.hpp), best source class
        if (p.tracked)
        {
            world& mw = const_cast<world&>(w); // router path caches only (export_refusal.hpp)
            std::unique_ptr<export_refusal::context> x;
            std::map<std::size_t, export_refusal::good_markets> scans;
            for (const auto& [mid, g] : *p.tracked)
            {
                const auto it = M.find(mid);
                if (it == M.end()) continue;
                mrec& r = it->second;
                if (!(r.pre[g] < r.proc_need[g] + r.site_need[g])) continue; // not short
                if (!x) x = std::make_unique<export_refusal::context>(mw, *p.reg);
                auto si = scans.find(g);
                if (si == scans.end())
                    si = scans.emplace(g, export_refusal::scan_good(*x, g, export_refusal::surplus_rule::dispatcher)).first;
                const export_refusal::best_result b = export_refusal::classify_destination(*x, mid, g, si->second.sur);
                r.xbest[g] = static_cast<float>(b.best);
            }
        }
        return;
    }
    if (lap == p.lap_clear)
    {
        for (const auto& [mid, mc] : w.markets)
        {
            mrec& r = M[mid];
            for (std::size_t g = 0; g < resource_count; ++g)
            {
                r.clr[g] = mc.inventory[g];
                r.hh[g] = mc.household_fill[g];
                r.bg[g] = mc.background_fill[g];
                r.dem[g] = mc.demand[g];
                r.sup[g] = mc.supply[g];
                r.hw[g] = mc.hauler_want[g];
            }
        }
        return;
    }
    if (lap == p.lap_bud)
    {
        for (const auto& [mid, mc] : w.markets)
        {
            mrec& r = M[mid];
            for (std::size_t g = 0; g < resource_count; ++g) r.bud[g] = mc.inventory[g];
        }
        return;
    }
    if (lap == k_campaign_settle_lap_count - 1)
    {
        p.convoys.clear();
        for (const convoy_component& c : w.convoys)
            p.convoys[c.id] = convoy_seen{c.source_market, c.dest_market, static_cast<std::size_t>(c.cargo_resource), c.cargo_qty};
    }
}

std::map<entity_id, entity_id> owner_map(const world& w)
{
    std::map<entity_id, entity_id> owner;
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id a : cc.assets) owner.emplace(a, cid);
    return owner;
}

/// The good a building produces into a market, for the producer census.
void producer_goods(const recipe_registry& reg, const building_component& b, std::vector<std::size_t>& out)
{
    out.clear();
    if (b.type == building_type::extraction_site) { out.push_back(static_cast<std::size_t>(b.target_resource)); return; }
    if (b.type != building_type::processing_facility) return;
    const recipe* rc = reg.get_recipe(b.recipe);
    if (!rc) return;
    for (std::size_t g = 0; g < resource_count; ++g) if (rc->outputs[g] > 0.0f) out.push_back(g);
}

/// After the tick: production and the producer census per market.
void census_producers(const world& w, const recipe_registry& reg, const economy_report& rep,
                      std::map<entity_id, mrec>& M)
{
    std::vector<std::size_t> gs;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.ticks_remaining > 0) continue;
        producer_goods(reg, b, gs);
        if (gs.empty()) continue;
        const auto it = M.find(market_for_tile(w, b.tile));
        if (it == M.end()) continue;
        mrec& r = it->second;
        const building_report* row = row_of(rep, bid);
        const bool run = !b.decommissioned && row && row->active;
        for (const std::size_t g : gs)
        {
            if (b.decommissioned) r.p_decom[g] += 1.0f;
            else if (run) r.p_run[g] += 1.0f;
            else r.p_idle[g] += 1.0f;
        }
        if (!run) continue;
        if (b.type == building_type::extraction_site) { r.prod[gs[0]] += row->output_quantity; continue; }
        const recipe* rc = reg.get_recipe(b.recipe);
        float tot = 0.0f;
        for (float o : rc->outputs) tot += o;
        const float runs = tot > 0.0f ? row->output_quantity / tot : 0.0f;
        for (const std::size_t g : gs) r.prod[g] += rc->outputs[g] * runs;
    }
}

struct seed_out
{
    std::uint32_t seed = 0;
    std::string fail;
    counts c;
    double secs = 0;
};

/// Pooled trajectory per good per tick.
struct traj_row
{
    double prod_m = 0, prod_body = 0, shelf = 0, pr_ratio = 0, pairs = 0, over_ceiling = 0, hw = 0, hh = 0, bg = 0;
    double bid_filled = 0, bid_unfilled = 0, silenced = 0, plants = 0;
    double p_run_m = 0, p_idle_m = 0, p_decom_m = 0, p_run_b = 0, p_idle_b = 0, p_decom_b = 0;
    double pools_m = 0, arrived = 0, departed = 0;
};
std::map<std::string, std::vector<traj_row>> g_traj; // good -> tick 0..ticks

void run_seed(std::uint32_t seed, seed_out& out)
{
    const auto t0 = std::chrono::steady_clock::now();
    out.seed = seed;
    counts& C = out.c;
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, wp, *start); }
    catch (const std::exception& e) { out.fail = std::string("world build threw: ") + e.what(); return; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    if (w.corporations.empty()) { out.fail = "no corporations"; return; }

    probe p;
    p.reg = &reg;
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
    {
        const char* n = k_campaign_settle_lap_names[i];
        if (!std::strcmp(n, "convoys")) p.lap_pre = i;
        if (!std::strcmp(n, "run_economy_step")) p.lap_econ = i;
        if (!std::strcmp(n, "clear_markets")) p.lap_clear = i;
        if (!std::strcmp(n, "apply_budget+nation_step")) p.lap_bud = i;
    }
    settle_tick_hooks hooks;
    hooks.after_lap = after_lap;
    hooks.ctx = &p;

    economy_report last_settle;
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        p.armed = (step == k_campaign_settle_ticks - 1); // only to seed the convoy id set
        std::map<entity_id, mrec> scratch;
        std::map<entity_id, std::map<entity_id, arr>> sp;
        std::vector<entity_id> none;
        std::map<entity_id, entity_id> no_owner;
        p.cur = &scratch; p.own_pool = &sp; p.plants = &none; p.owner = &no_owner;
        settle_tick_result res = run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true, &hooks);
        if (step == k_campaign_settle_ticks - 1) last_settle = std::move(res.report);
    }
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    if (seat.seated == null_entity) { out.fail = "no corporation seated"; return; }

    if (!g_cf.empty())
    {
        recipe_registry& mreg = const_cast<recipe_registry&>(reg); // measurement only (--cf)
        if (g_cf == "bg_off") { background_demand_params b = reg.background_demand(); b.consumes = false; mreg.set_background_demand(b); }
        else if (g_cf == "ceiling_off") { price_band_params pb = reg.price_band(); pb.reservation_mult = 0.0f; mreg.set_price_band(pb); }
        else { out.fail = "unknown --cf " + g_cf; return; }
    }
    std::vector<entity_id> run_h;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility && classify(b, row_of(last_settle, bid), reg) == ps_run)
            run_h.push_back(bid);
    std::sort(run_h.begin(), run_h.end());
    std::map<entity_id, entity_id> owner = owner_map(w);
    std::map<entity_id, entity_id> plant_mkt;
    for (const entity_id bid : run_h) plant_mkt[bid] = market_for_tile(w, w.buildings.at(bid).tile);

    const int T = g_ticks;
    std::vector<std::map<entity_id, mrec>> MT(T + 1);            // tick -> market -> rec
    std::map<entity_id, std::vector<prec>> PT;                    // plant -> tick -> rec
    for (const entity_id bid : run_h) PT[bid].resize(T + 1);
    std::map<entity_id, decom_info> DI;                           // every producer / plant decommission
    std::map<entity_id, std::pair<int, int>> last_row;            // bid -> (state, lim) last row seen
    std::map<entity_id, entity_id> mkt_body;
    for (const auto& [mid, mc] : w.markets) mkt_body[mid] = mc.body;
    std::map<entity_id, std::map<entity_id, arr>> own_pool;
    std::set<entity_id> decom_at_handoff;
    for (const auto& [bid, b] : w.buildings) if (b.decommissioned) decom_at_handoff.insert(bid);
    const float base_rate = reg.economics(building_type::processing_facility).base_rate;

    p.plants = &run_h;
    p.owner = &owner;
    p.own_pool = &own_pool;
    std::set<std::pair<entity_id, std::size_t>> tracked;
    for (const entity_id bid : run_h)
        if (const recipe* rc = reg.get_recipe(w.buildings.at(bid).recipe))
            for (std::size_t g = 0; g < resource_count; ++g)
                if (rc->inputs[g] > 0.0f && !reg.grid_goods().grid(g)) tracked.insert({plant_mkt[bid], g});
    p.tracked = &tracked;
    constexpr int k_econ_tick_days = 90;
    for (int k = 1; k <= T; ++k)
    {
        const int day = k * k_econ_tick_days;
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = day;
        p.armed = true;
        p.tick = k;
        p.cur = &MT[k];
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day,
                                                 /*spectating=*/false, &hooks);
        const economy_report& rep = res.report;
        if (k == T)
            for (const auto& [bid, b] : w.buildings)
            {
                if (b.type != building_type::processing_facility) continue;
                const proc_state s = classify(b, row_of(rep, bid), reg);
                if (s == ps_build) continue;
                C.add("g1.built");
                if (s == ps_run) C.add("g1.run");
            }
        census_producers(w, reg, rep, MT[k]);
        for (const auto& [key, v] : rep.upkeep_wants)
            if (auto it = MT[k].find(key.second); it != MT[k].end())
                for (std::size_t g = 0; g < resource_count; ++g) it->second.upk[g] += v[g];
        // decommission records for every building (producers and plants)
        for (const auto& [bid, b] : w.buildings)
        {
            const building_report* row = row_of(rep, bid);
            if (b.decommissioned && !DI.count(bid) && !decom_at_handoff.count(bid))
            {
                decom_info d;
                d.tick = k;
                d.by = b.loss_streak >= 8 ? "reflex" : b.loss_streak == 0 ? "verb" : "unknown";
                const auto lr = last_row.find(bid);
                if (row && b.type == building_type::processing_facility)
                {
                    const proc_state s = classify_row(b, row, reg);
                    d.last = s;
                    d.lim = s == ps_input ? static_cast<int>(row->limiting_input) : -1;
                }
                else if (lr != last_row.end()) { d.last = lr->second.first; d.lim = lr->second.second; }
                DI[bid] = d;
            }
            if (!b.decommissioned) decom_at_handoff.erase(bid);
            if (row && b.type == building_type::processing_facility && b.ticks_remaining <= 0)
            {
                const proc_state s = classify_row(b, row, reg);
                last_row[bid] = {s, s == ps_input ? static_cast<int>(row->limiting_input) : -1};
            }
        }
        for (const entity_id bid : run_h)
        {
            prec& pr = PT[bid][k];
            const auto bi = w.buildings.find(bid);
            if (bi == w.buildings.end()) { pr.state = ps_gone; continue; }
            const building_component& b = bi->second;
            const building_report* row = row_of(rep, bid);
            pr.state = classify(b, row, reg);
            pr.row = row != nullptr;
            if (row) { pr.run = row->run; pr.lim = row->has_limiting ? static_cast<int>(row->limiting_input) : -1; }
            const recipe* rc = reg.get_recipe(b.recipe);
            const float eff = row ? row->effective_workforce : b.workforce_assigned;
            const float bf = base_rate * eff * std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f) * building_supply_scalar(b);
            if (rc) for (std::size_t g = 0; g < resource_count; ++g) pr.need[g] = rc->inputs[g] * bf;
            if (const auto op = own_pool.find(bid); op != own_pool.end()) pr.pool = op->second[0];
            // the reflex's economics on the decommission tick (econ-lap prices)
            if (b.decommissioned)
                if (auto di = DI.find(bid); di != DI.end() && di->second.tick == k && rc)
                {
                    const mrec& m = MT[k][plant_mkt[bid]];
                    for (std::size_t g = 0; g < resource_count; ++g)
                    {
                        const float px = m.price[g] > 0.0f ? m.price[g] : m.base[g];
                        di->second.rev += rc->outputs[g] * px;
                        if (!reg.grid_goods().grid(g)) di->second.inp += rc->inputs[g] * px;
                    }
                }
        }
        // carry the plant's need forward where a row was missing
        for (const entity_id bid : run_h)
        {
            prec& pr = PT[bid][k];
            bool any = false;
            for (float v : pr.need) any = any || v > 0.0f;
            if ((!any || !pr.row) && k > 1) pr.need = PT[bid][k - 1].need;
        }
    }

    // ------------------------------------------------------------------ analysis
    const float t_idle = reg.t_idle();
    const auto body_sum = [&](int k, entity_id body, entity_id except, std::size_t g, arr mrec::*f) {
        float s = 0.0f;
        for (const auto& [mid, r] : MT[k])
            if (mid != except && mkt_body[mid] == body) s += (r.*f)[g];
        return s;
    };
    const auto mean_over = [&](entity_id mid, std::size_t g, int a, int b2, arr mrec::*f) {
        float s = 0.0f; int n = 0;
        for (int k = std::max(1, a); k <= std::min(T, b2); ++k) { s += (MT[k][mid].*f)[g]; ++n; }
        return n ? s / n : 0.0f;
    };
    const auto mean_body = [&](entity_id body, std::size_t g, int a, int b2, arr mrec::*f) {
        float s = 0.0f; int n = 0;
        for (int k = std::max(1, a); k <= std::min(T, b2); ++k) { s += body_sum(k, body, null_entity, g, f); ++n; }
        return n ? s / n : 0.0f;
    };
    std::map<std::string, int> shown;
    const std::map<entity_id, entity_id> owner_end = owner_map(w);
    std::set<std::pair<entity_id, std::size_t>> traj_pairs;
    std::map<std::pair<entity_id, std::size_t>, std::vector<entity_id>> traj_plants;

    for (const entity_id bid : run_h)
    {
        const std::vector<prec>& H = PT[bid];
        if (H[T].state == ps_run) { C.add("still_run"); continue; }
        C.add("lost");
        const entity_id mid = plant_mkt[bid];
        const auto bi = w.buildings.find(bid);
        const recipe* rc = bi != w.buildings.end() ? reg.get_recipe(bi->second.recipe) : nullptr;
        const std::string rn = rc ? rc->name : std::string("?");
        int last_run = 0;
        for (int k = 1; k <= T; ++k) if (H[k].state == ps_run) last_run = k;
        const int T0 = last_run + 1;
        C.add(std::string("lost.T0_band.") + (T0 <= 10 ? "01-10" : T0 <= 25 ? "11-25" : "26-50"));
        const auto di = DI.find(bid);
        const int dtick = di != DI.end() ? di->second.tick : T + 1;
        int Ts = -1, gs = -1;
        for (int k = T0; k <= std::min(T, dtick); ++k)
            if (H[k].state == ps_input && H[k].lim >= 0) { Ts = k; gs = H[k].lim; break; }
        if (Ts < 0 && di != DI.end() && di->second.lim >= 0 && dtick >= T0) { Ts = dtick; gs = di->second.lim; }
        std::string final_cls = std::string(k_state_name[H[T].state]);
        if (H[T].state == ps_decom && di != DI.end()) final_cls += std::string(".") + di->second.by;
        C.add("lost.final." + final_cls);

        // the mothballed "inputs cost >= revenue" set: input price up, or output price down?
        if (H[T].state == ps_decom && di != DI.end() && di->second.last == ps_run && rc)
        {
            const decom_info& d = di->second;
            const bool over = d.inp >= d.rev;
            C.add(std::string("ran_decom.") + (over ? "inputs_over_rev" : "other"));
            if (over)
            {
                const mrec& m1 = MT[1][mid];
                const mrec& mT = MT[d.tick][mid];
                float rev1 = 0, in1 = 0, revT = 0, inT = 0;
                std::size_t gi = resource_count, go = resource_count;
                float gi_v = -1, go_v = -1;
                for (std::size_t g = 0; g < resource_count; ++g)
                {
                    const float p1 = m1.price[g] > 0.0f ? m1.price[g] : m1.base[g];
                    const float pT = mT.price[g] > 0.0f ? mT.price[g] : mT.base[g];
                    rev1 += rc->outputs[g] * p1; revT += rc->outputs[g] * pT;
                    if (!reg.grid_goods().grid(g)) { in1 += rc->inputs[g] * p1; inT += rc->inputs[g] * pT; }
                    if (rc->outputs[g] * m1.base[g] > go_v) { go_v = rc->outputs[g] * m1.base[g]; go = g; }
                    if (rc->inputs[g] > 0.0f && !reg.grid_goods().grid(g)
                        && std::fabs(rc->inputs[g] * (pT - p1)) > gi_v) { gi_v = std::fabs(rc->inputs[g] * (pT - p1)); gi = g; }
                }
                const bool under1 = in1 >= rev1;
                const bool in_up = inT > in1 * 1.05f, out_dn = revT < rev1 * 0.95f;
                const std::string cls = under1 ? (in_up || out_dn ? "under_water_t1_and_worse" : "under_water_from_t1")
                                      : in_up && out_dn ? "both" : in_up ? "input_rose" : out_dn ? "output_fell" : "flat";
                C.add("ior.cls." + cls);
                C.add("ior.recipe." + rn + "." + cls);
                C.add("ior.decom_tick_band." + std::string(d.tick <= 10 ? "01-10" : d.tick <= 25 ? "11-25" : "26-50"));
                if (go < resource_count && m1.base[go] > 0.0f)
                {
                    const float r1 = m1.price[go] / m1.base[go], rT = mT.price[go] / mT.base[go];
                    C.add("ior.out_ratio_x100_t1", std::lround(100 * r1));
                    C.add("ior.out_ratio_x100_tD", std::lround(100 * rT));
                    if (out_dn)
                    {
                        // why did it fall: the output's supply up, or demand down?
                        const float s1 = MT[1][mid].sup[go], sT = mT.sup[go], d1 = MT[1][mid].dem[go], dT = mT.dem[go];
                        const std::string why = sT > s1 * 1.1f && dT >= d1 * 0.9f ? "supply_up"
                                              : dT < d1 * 0.9f && sT <= s1 * 1.1f ? "demand_down"
                                              : sT > s1 * 1.1f ? "supply_up+demand_down" : "flat_sd";
                        C.add("ior.out_fell_why." + why);
                        C.add("ior.out_fell_good." + gname(go) + "." + why);
                    }
                }
                if (in_up && gi < resource_count)
                {
                    const float s1 = MT[1][mid].sup[gi], sT = mT.sup[gi], d1 = MT[1][mid].dem[gi], dT = mT.dem[gi];
                    const std::string why = dT > d1 * 1.1f && sT >= s1 * 0.9f ? "demand_up"
                                          : sT < s1 * 0.9f && dT <= d1 * 1.1f ? "supply_down"
                                          : sT < s1 * 0.9f ? "supply_down+demand_up" : "flat_sd";
                    C.add("ior.in_rose_why." + why);
                    C.add("ior.in_rose_good." + gname(gi) + "." + why);
                }
                C.add("ior.rev_x10_t1", std::lround(10 * rev1)); C.add("ior.rev_x10_tD", std::lround(10 * revT));
                C.add("ior.in_x10_t1", std::lround(10 * in1));   C.add("ior.in_x10_tD", std::lround(10 * inT));
                if (shown["ior." + cls]++ < g_examples)
                    std::printf("    ex seed %u ior %-26s %llu %-22s t%d rev %.2f->%.2f in %.2f->%.2f\n", seed, cls.c_str(),
                                static_cast<unsigned long long>(bid), rn.c_str(), d.tick, rev1, revT, in1, inT);
            }
        }

        if (Ts < 0 || gs < 0)
        {
            C.add("cause.f.notstarved." + final_cls);
            continue;
        }
        const std::size_t g = static_cast<std::size_t>(gs);
        const std::string G = gname(g);
        C.add("starved.n");
        C.add("starved.good." + G);
        C.add("starved.Ts_sum." + G, Ts);
        C.add("starved.recipe." + rn + "." + G);
        if (mid == null_entity || !MT[Ts].count(mid)) { C.add("cause.f.nomarket." + G); continue; }
        if (reg.grid_goods().grid(g)) { C.add("cause.f.grid." + G); continue; }
        // who builds on this market with g* in the material row (world at the last tick):
        // the site's building and its owner's class; once per (M, g*) pair
        if (traj_pairs.insert({mid, g}).second)
            for (const auto& [sid, sb] : w.buildings)
            {
                if (sb.ticks_remaining <= 0 || market_for_tile(w, sb.tile) != mid) continue;
                if (!(reg.resource_build_cost_for(sb.type, sb.target_resource, sb.recipe)[g] > 0.0f)) continue;
                const auto so = owner_end.find(sid);
                const auto sc = so != owner_end.end() ? w.corporations.find(so->second) : w.corporations.end();
                const char* who = sc == w.corporations.end() ? "noowner"
                                : sc->second.is_player ? "player" : sc->second.is_background ? "background" : "rival";
                std::string what = sb.type == building_type::processing_facility
                                 ? (reg.get_recipe(sb.recipe) ? "proc." + reg.get_recipe(sb.recipe)->name : std::string("proc.?"))
                                 : sb.type == building_type::extraction_site ? "ext." + gname(static_cast<std::size_t>(sb.target_resource))
                                 : "other";
                C.add("sites_tend." + G + "." + what);
                C.add("sites_tend_who." + G + "." + who);
            }
        traj_plants[{mid, g}].push_back(bid);
        const prec& ps = H[Ts];
        const float need = ps.need[g];
        const float gap = std::max(0.0f, t_idle * need - std::max(0.0f, ps.pool[g]));
        const mrec& mT = MT[Ts][mid];
        const entity_id body = mkt_body[mid];
        std::string cause;
        if (mT.pre[g] >= gap && gap > 0.0f && !mT.admits[g]) cause = "c.ceiling";
        else if (mT.pre[g] >= gap && gap > 0.0f)
        {
            // Outdrawn in the pro-rata turn. Was the shelf short because the
            // final channels took a big share of what was listed the tick before?
            cause = "d.procs_sites";
            if (Ts > 1)
            {
                const mrec& mp = MT[Ts - 1][mid];
                const float taken = mp.hh[g] + mp.bg[g] + std::max(0.0f, mp.clr[g] - mp.bud[g]);
                const float peak = mp.clr[g] + mp.hh[g] + mp.bg[g];
                cause += peak > 0.0f && taken >= 0.25f * peak
                       ? (mp.bg[g] >= mp.hh[g] ? ".bg_drained" : ".hh_drained") : ".procs_only";
            }
            // How many processors at M wanted the good, and the shelf's cover of their sum.
            float want = 0.0f;
            int n = 0;
            for (const auto& [ob, op] : PT)
                if (plant_mkt[ob] == mid && op[Ts].state != ps_decom && op[Ts].state != ps_gone && op[Ts].need[g] > 0.0f)
                { want += op[Ts].need[g]; ++n; }
            C.add("dprocs.cover." + std::string(want > 0.0f && mT.pre[g] / want < 0.5f ? "lt0.5" : "ge0.5"));
            C.add("dprocs.consumers_sum", n);
        }
        if (cause.empty() && Ts > 1)
        {
            const mrec& mp = MT[Ts - 1][mid];
            // what stood on the shelf once the tick's listings landed (before the takers)
            const float spoil = reg.shelf_spoilage()[g];
            const float spoiled = spoil > 0.0f && spoil < 1.0f ? mp.clr[g] * spoil / (1.0f - spoil) : 0.0f;
            const float listed_peak = mp.clr[g] + mp.hh[g] + mp.bg[g] + spoiled;
            const float nation = std::max(0.0f, mp.clr[g] - mp.bud[g]);
            const float rest = std::max(0.0f, mp.bud[g] - mT.pre[g]);
            if (listed_peak >= gap && gap > 0.0f)
            {
                const std::pair<float, const char*> takers[] = {
                    {mp.hh[g], "households"}, {mp.bg[g], "background"}, {nation, "nation"},
                    {spoiled, "spoilage"}, {rest, "other_post_budget"}};
                const auto* best = &takers[0];
                for (const auto& t : takers) if (t.first > best->first) best = &t;
                cause = std::string("d.") + best->second;
                if (!mp.admits[g]) cause += "+ceiling_prev";
            }
        }
        if (cause.empty() && mT.pools[g] - std::max(0.0f, ps.pool[g]) >= gap && gap > 0.0f) cause = "f.pool_held";
        if (cause.empty())
        {
            const float pm13 = mean_over(mid, g, 1, 3, &mrec::prod) + mean_over(mid, g, 1, 3, &mrec::arrived);
            const float pmT  = mean_over(mid, g, Ts - 3, Ts - 1, &mrec::prod) + mean_over(mid, g, Ts - 3, Ts - 1, &mrec::arrived);
            const float pb13 = mean_body(body, g, 1, 3, &mrec::prod);
            const float pbT  = mean_body(body, g, Ts - 3, Ts - 1, &mrec::prod);
            const bool m_made = pm13 >= 0.25f * gap && pm13 > 0.0f;
            const float pool1 = PT[bid][1].pool[g];
            const float shelf1 = MT[1][mid].pre[g];
            if (m_made && pmT < 0.5f * pm13) cause = "a.producers_died.market";
            else if (!m_made && pb13 > 0.0f && pbT < 0.5f * pb13 && pb13 >= gap && (pool1 + shelf1) < t_idle * PT[bid][1].need[g] * 3.0f)
                cause = "a.producers_died.body";
            else if (pm13 < t_idle * need && (pool1 + shelf1) >= t_idle * PT[bid][1].need[g])
            {
                cause = pool1 >= t_idle * PT[bid][1].need[g] ? "e.opening_stock.pool" : "e.opening_stock.shelf";
                cause += pb13 - mean_over(mid, g, 1, 3, &mrec::prod) >= gap ? ".body_supply" : ".no_body_supply";
            }
            else if (pm13 <= 0.0f && pb13 >= gap) cause = "b.reach";
            else if (mean_over(mid, g, Ts - 3, Ts - 1, &mrec::departed) >= gap) cause = "f.exported";
            else if (m_made) cause = "f.market_makes_short";   // production holds, but below this plant's need
            else cause = "f.other";
            // the cascade: the producers of g at M (or the body) that ran at tick 1 and not at Ts-1
            if (cause.rfind("a.", 0) == 0)
            {
                const bool bodywide = cause == "a.producers_died.body";
                std::vector<std::size_t> gsv;
                for (const auto& [pid, pb] : w.buildings)
                {
                    producer_goods(reg, pb, gsv);
                    if (std::find(gsv.begin(), gsv.end(), g) == gsv.end()) continue;
                    const entity_id pm = market_for_tile(w, pb.tile);
                    if (bodywide ? mkt_body[pm] != body : pm != mid) continue;
                    if (pb.ticks_remaining > 0) { C.add("cascade." + G + ".under_construction"); continue; }
                    const auto dd = DI.find(pid);
                    const bool dead = dd != DI.end() && dd->second.tick <= Ts;
                    std::string pc;
                    if (dead)
                        pc = std::string("decom.") + dd->second.by + "." +
                             (dd->second.last >= 0 ? k_state_name[dd->second.last] : "norow") +
                             (dd->second.lim >= 0 ? "." + gname(static_cast<std::size_t>(dd->second.lim)) : std::string());
                    else if (pb.type == building_type::processing_facility)
                    {
                        const auto lr = last_row.find(pid);
                        pc = lr == last_row.end() ? "norow" : std::string(k_state_name[lr->second.first])
                             + (lr->second.second >= 0 ? "." + gname(static_cast<std::size_t>(lr->second.second)) : std::string());
                    }
                    else pc = "extraction_live";
                    C.add("cascade." + G + "." + (pb.type == building_type::extraction_site ? "ext." : "proc.") + pc);
                }
            }
        }
        // ROOT TAGS (the second axis). M's stock = shelf + every pool at M.
        //   drawdown      stock(T*) < half stock(1), stock(1) >= 3 x gap: an opening stock drawn down
        //   never_stocked stock(1) < 3 x gap
        //   held          otherwise
        // body: another market on the body lists >= gap of g* under its ceiling at T*.
        // who drew M's shelf over ticks 1..T*: sites / processors / upkeep (by need) and the clear's takers.
        {
            const float st1 = MT[1][mid].pre[g] + MT[1][mid].pools[g];
            const float stT = mT.pre[g] + mT.pools[g];
            const std::string root = st1 < 3.0f * std::max(gap, 1e-3f) ? "never_stocked" : stT < 0.5f * st1 ? "drawdown" : "held";
            float body_listed = 0.0f;
            for (const auto& [om, r] : MT[Ts])
                if (om != mid && mkt_body[om] == body && r.admits[g]) body_listed += r.pre[g];
            const std::string bt = body_listed >= std::max(gap, 1e-3f) ? "body_has" : "body_none";
            const std::string top = cause.substr(0, cause.find('.', 2));
            C.add("root." + root);
            C.add("root_cause." + top + "." + root + "." + bt);
            C.add("root_good." + G + "." + root + "." + bt);
            // the export dispatcher's verdict on hauling a body shelf here, at T*
            const auto xname = [](float v) {
                const int c = static_cast<int>(v);
                return std::string(c == -2 ? "notshort" : c < 0 ? "nosource" : export_refusal::k_cls[c]);
            };
            const std::string xc = xname(mT.xbest[g]);
            C.add("reach." + G + "." + xc);
            C.add("reach_cause." + top + "." + xc);
            if (bt == "body_has") C.add("reach_bodyhas." + G + "." + xc);
            // M's draw / supply balance over ticks 1..T*-1 (means per tick)
            float site = 0, proc = 0, upk = 0, hh = 0, bg = 0, nat = 0, drawn = 0, prod = 0, arr = 0, dep = 0, deps = 0;
            int n = 0;
            for (int k = 1; k < Ts; ++k)
            {
                const mrec& r = MT[k][mid];
                site += r.site_need[g]; proc += r.proc_need[g]; upk += r.upk[g];
                hh += r.hh[g]; bg += r.bg[g]; nat += std::max(0.0f, r.clr[g] - r.bud[g]);
                drawn += std::max(0.0f, r.pre[g] - r.econ[g]);
                prod += r.prod[g]; arr += r.arrived[g]; dep += r.departed[g]; deps += r.departed_shelf[g];
                ++n;
            }
            if (n > 0)
            {
                const std::string K = "bal." + G + ".";
                C.add(K + "n");
                C.add(K + "prod_x10", std::lround(10 * prod / n));
                C.add(K + "arrived_x10", std::lround(10 * arr / n));
                C.add(K + "departed_x10", std::lround(10 * dep / n));
                C.add(K + "departed_shelf_x10", std::lround(10 * deps / n));
                C.add(K + "econ_drawn_x10", std::lround(10 * drawn / n));
                C.add(K + "site_need_x10", std::lround(10 * site / n));
                C.add(K + "proc_need_x10", std::lround(10 * proc / n));
                C.add(K + "upkeep_want_x10", std::lround(10 * upk / n));
                C.add(K + "hh_x10", std::lround(10 * hh / n));
                C.add(K + "bg_x10", std::lround(10 * bg / n));
                C.add(K + "nation_x10", std::lround(10 * nat / n));
                C.add(K + "stock1_x10", std::lround(10 * st1));
                C.add(K + "stockT_x10", std::lround(10 * stT));
                C.add(K + "body_listed_T_x10", std::lround(10 * body_listed));
            }
        }
        C.add("cause." + cause);
        C.add("cause_good." + G + "." + cause.substr(0, cause.find('.', 2)));
        C.add("cause_good_full." + G + "." + cause);
        C.add("cause_final." + final_cls + "." + cause.substr(0, cause.find('.', 2)));
        C.add(std::string("cause_Ts_band.") + cause.substr(0, 1) + "." + (Ts <= 10 ? "01-10" : Ts <= 25 ? "11-25" : "26-50"));
        if (cause.rfind("e.", 0) == 0) C.add("e.runout_tick_sum", Ts), C.add("e.runout_n");
        if (shown[cause]++ < g_examples)
        {
            std::printf("    ex seed %u %-40s %llu %-22s %s Ts %d need %.2f pool %.2f shelf %.2f admit %d pr/b %.2f | M prod t1-3 %.2f tT %.2f | body prod %.2f | hh %.2f bg %.2f\n",
                        seed, cause.c_str(), static_cast<unsigned long long>(bid), rn.c_str(), G.c_str(), Ts, need, ps.pool[g],
                        mT.pre[g], mT.admits[g] ? 1 : 0, mT.base[g] > 0 ? mT.posted[g] / mT.base[g] : 0.0f,
                        mean_over(mid, g, 1, 3, &mrec::prod), mean_over(mid, g, Ts - 3, Ts - 1, &mrec::prod),
                        mean_body(body, g, Ts - 3, Ts - 1, &mrec::prod), Ts > 1 ? MT[Ts - 1][mid].hh[g] : 0.0f,
                        Ts > 1 ? MT[Ts - 1][mid].bg[g] : 0.0f);
        }
    }

    // trajectory, pooled
    for (const auto& [key, plants] : traj_plants)
    {
        const entity_id mid = key.first;
        const std::size_t g = key.second;
        std::vector<traj_row>& tr = g_traj[gname(g)];
        if (tr.size() < static_cast<std::size_t>(T + 1)) tr.resize(T + 1);
        const entity_id body = mkt_body[mid];
        for (int k = 1; k <= T; ++k)
        {
            const mrec& m = MT[k][mid];
            traj_row& r = tr[k];
            r.pairs += 1;
            r.prod_m += m.prod[g];
            r.prod_body += body_sum(k, body, null_entity, g, &mrec::prod);
            r.shelf += m.pre[g];
            r.pr_ratio += m.base[g] > 0.0f ? m.posted[g] / m.base[g] : 0.0f;
            r.over_ceiling += m.admits[g] ? 0 : 1;
            r.hw += m.hw[g]; r.hh += m.hh[g]; r.bg += m.bg[g];
            r.pools_m += m.pools[g]; r.arrived += m.arrived[g]; r.departed += m.departed[g];
            r.p_run_m += m.p_run[g]; r.p_idle_m += m.p_idle[g]; r.p_decom_m += m.p_decom[g];
            r.p_run_b += body_sum(k, body, null_entity, g, &mrec::p_run);
            r.p_idle_b += body_sum(k, body, null_entity, g, &mrec::p_idle);
            r.p_decom_b += body_sum(k, body, null_entity, g, &mrec::p_decom);
            for (const entity_id bid : plants)
            {
                const prec& pr = PT[bid][k];
                if (pr.state == ps_decom || pr.state == ps_gone) continue;
                r.plants += 1;
                const bool wants = pr.need[g] > pr.pool[g];
                if (!wants) continue;
                if (!m.admits[g]) r.silenced += 1;
                else if (pr.state == ps_run && !(pr.lim == static_cast<int>(g) && pr.run < 1.0f)) r.bid_filled += 1;
                else r.bid_unfilled += 1;
            }
        }
    }
    out.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seed_list(argv[++i]);
        else if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) g_ticks = std::max(4, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--examples") && i + 1 < argc) g_examples = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--cf") && i + 1 < argc) g_cf = argv[++i];
        else { std::fprintf(stderr, "usage: starve_trace_probe [--seeds a,b] [--ticks N] [--examples N] [--cf bg_off|ceiling_off]\n"); return 2; }
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::fprintf(stderr, "no seeds\n"); return 2; }
    std::printf("starve_trace_probe: %zu seeds, %d play ticks, cf %s\n", seeds.size(), g_ticks, g_cf.empty() ? "none" : g_cf.c_str());
    counts pooled;
    int fails = 0;
    for (const std::uint32_t s : seeds)
    {
        seed_out o;
        run_seed(s, o);
        if (!o.fail.empty()) { std::printf("seed %u FAIL %s\n", s, o.fail.c_str()); ++fails; continue; }
        std::printf("seed %u (%.0f s) lost %ld starved %ld\n", s, o.secs, o.c.c["lost"], o.c.c["starved.n"]);
        for (const auto& [k, v] : o.c.c) std::printf("COUNTS %u %s %ld\n", s, k.c_str(), v);
        for (const auto& [k, v] : o.c.c) pooled.add(k, v);
        std::fflush(stdout);
    }
    std::printf("\nPOOLED over %zu seeds (%d failed)\n", seeds.size(), fails);
    for (const auto& [k, v] : pooled.c) std::printf("  %-70s %7ld\n", k.c_str(), v);
    std::printf("\nTRAJECTORY (pooled sums over each good's starving (seed, market) pairs; pr/b and ceil are means)\n");
    for (const auto& [G, tr] : g_traj)
    {
        std::printf("  %s\n   tick pairs  prodM  prodBody   shelf  pr/b  ceil%%     hw     hh     bg  poolsM  arrv  dept  bidOK bidNO silen plants  pRunM pIdlM pDecM  pRunB pIdlB pDecB\n", G.c_str());
        for (int k = 1; k < static_cast<int>(tr.size()); ++k)
        {
            if (!(k <= 5 || k % 5 == 0)) continue;
            const traj_row& r = tr[k];
            const double n = r.pairs > 0 ? r.pairs : 1;
            std::printf("   %4d %5.0f %6.1f %9.1f %7.1f %5.2f %5.0f %6.1f %6.1f %6.1f %7.1f %5.1f %5.1f %6.0f %5.0f %5.0f %6.0f %6.0f %5.0f %5.0f %6.0f %5.0f %5.0f\n",
                        k, r.pairs, r.prod_m, r.prod_body, r.shelf, r.pr_ratio / n, 100.0 * r.over_ceiling / n, r.hw, r.hh,
                        r.bg, r.pools_m, r.arrived, r.departed, r.bid_filled, r.bid_unfilled, r.silenced, r.plants,
                        r.p_run_m, r.p_idle_m, r.p_decom_m, r.p_run_b, r.p_idle_b, r.p_decom_b);
        }
    }
    return fails ? 1 : 0;
}
