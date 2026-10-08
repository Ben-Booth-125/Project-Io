// ---------------------------------------------------------------------------
// handoff_idle_probe — BL-1217 (inputs reach processors), G1 diagnosis probe
// ---------------------------------------------------------------------------
// QUESTION. market_viability's G1 reads 71.4% of built processors running at
// the handoff (target 85). The non-running are mostly NOT input-starved: they
// are decommissioned, unstaffed (nolab) or unsupplied. For every built
// processor not running, WHY -- and is the idle state the economy working
// (no buyer), or a defect?
//
// THE WORLD. Seated exactly as market_viability seats it: build_app_start_world,
// the 12-tick settle (run_settle_tick, econ steps 0..11, day 0, spectating),
// seat_player_corporation, then PLAY ticks as run_app_live_window steps them
// (to tick 50 by default). Read at the HANDOFF and at play tick 50.
//
// A PURE READER. One after_lap hook, nothing written:
//   lap "convoys"          (L ticks only) the pre-step processor snapshot and the
//                          pool-surplus status -- market_viability's L row,
//                          processor half, copied (same export_refusal.hpp).
//   lap "run_economy_step" every tick: each market's price/base (the prices the
//                          loss reflex and the idle verb valued at; the clear
//                          has not run); (L ticks) the dispatcher class per
//                          (processor market, input).
//   after the tick         every processor's row state is remembered (last 8),
//                          decommission/resume transitions are recorded with
//                          the tick's economics, workforce_set events are kept.
//
// WHO DECOMMISSIONED. The loss reflex (economy_system.cpp, BL-079) leaves
// loss_streak >= 8; the idle verb (corp_command.cpp) zeroes it. A plant
// decommissioned in the world as built is "atbuild". Firm exit and the seat's
// clean slate DEMOLISH (they never leave a decommissioned plant), so neither
// appears in G1.
//
// THE CLASSES (one key family per G1 state; keys pool by sum over seeds):
//   decom.by.{reflex,verb,atbuild,unknown}
//   decom.why: the row state history up to and including the decommission tick
//     never_ran/<modal state>  no row since first seen ever ran
//     starved                  last row input-starved (G1b's decom_after[input])
//     idle_<state>             ran once, last row nolab / unsupplied / other
//     ran/out_floored          last row ran; worst output price/base <= 0.30
//                              (the reflex's own "floored") -- no buyer / glut
//     ran/inputs_over_rev      ran; input cost >= revenue at the tick's prices
//     ran/fixed_over_margin    ran; revenue - inputs > 0 but < wages + maintenance
//     ran/net_pos              ran and net >= 0 on the decommission tick
//   nolab.why: assigned0 | region_empty (pool supply 0) | contended (corp's
//     (corp, body) pool short: supply = body labour x corp's building-count
//     share) | qualified | contended+qualified | other; with the share of the
//     pool's claimed demand that is DECOMMISSIONED plant (pass 1 does not skip
//     a decommissioned building: it claims, and is granted, labour).
//   unsup.why: supply0 | target0/{verb_dial, at_first_seen, seat_autosolve, unknown}
//   *.dialzero (unsupplied now, or decommissioned after being unsupplied): the
//     market as the workforce dial read it on the tick it set target 0 --
//     unpriced_output | out_forecast_floor (the solver's sqrt forecast, with
//     this plant's full-run output added, sits on the band floor) |
//     inputs_dear[+ceil] | fixed_costs. `dz` lines print the registers (one per
//     (recipe, class) per seed, --examples).
//   decom.starved_input: the limiting input on the decommission tick.
//   input: limiting good, the L class (market_viability's processor block).
//   every idle state also carries margin.{pos,neg,unpriced}: the recipe's unit
//   margin (outputs - inputs per run, at its market's current prices) -- would
//   running pay before wages.
//   builder.{gen,settle,play}: present in the world as built / appeared during
//   the settle (the scored tier, spectating) / appeared in play.
//
// Usage: build_gen/verify/handoff_idle_probe.exe [--seeds a,b] [--ticks N] [--examples N]
// Build:  ./tools/verify/build_lua_harness.sh handoff_idle_probe
// Each seed prints `COUNTS <seed> <key> <value>` lines: pool a split run by
// summing them.
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
#include <deque>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int k_t50 = 50;
constexpr float k_floored = 0.30f; // economy_system.cpp's floored_frac
int g_examples = 2;

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
enum proc_state { ps_run = 0, ps_input, ps_nolab, ps_unsup, ps_decom, ps_build, ps_other, ps_count };
const char* k_state_name[ps_count] = {"run", "input", "nolab", "unsupplied", "decom", "build", "other"};

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

// --- the L row's processor half (market_viability.cpp, BL-1223), copied ------
constexpr int x_poolheld  = export_refusal::c_count;
constexpr int x_none      = x_poolheld + 1;
constexpr int x_ordered   = x_none + 1;
constexpr int x_grid      = x_ordered + 1;
constexpr int x_ceiling   = x_grid + 1;
constexpr int x_thin      = x_ceiling + 1;
constexpr int x_contended = x_thin + 1;
constexpr int x_unpriced  = x_contended + 1;
constexpr int x_nomarket  = x_unpriced + 1;
constexpr int x_count     = x_nomarket + 1;
const char* const k_x_name[x_count] = {
    "body", "noroute", "gate", "costly", "noroom", "room",
    "noshelfsurplus/poolheld", "noshelfsurplus/none", "noshelfsurplus/ordered", "grid",
    "stocked/ceiling", "stocked/thin", "stocked/contended", "unpriced", "nomarket"};

struct proc_snap
{
    entity_id m = null_entity;
    std::array<float, resource_count> shelf{}, pool{};
    std::array<bool, resource_count>  admits{};
};

struct market_px { std::array<float, resource_count> price{}, base{}, posted{}, supply{}, demand{}; };

struct probe
{
    const recipe_registry* reg = nullptr;
    int lap_pre = -1, lap_econ = -1;
    bool l_armed = false;
    std::map<entity_id, market_px> px;              ///< at the economy lap, every tick
    std::map<entity_id, proc_snap> snap;            ///< L: pre-step
    std::map<std::pair<entity_id, std::size_t>, int> proc_best;
    struct pool_stat { int st = x_ordered; float src = 0.0f, home = 0.0f; };
    std::map<std::pair<entity_id, std::size_t>, pool_stat> pool_status;
};

bool live_processor(const building_component& b) { return !b.decommissioned && b.ticks_remaining <= 0; }

std::vector<entity_id> sorted_processors(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility) ids.push_back(bid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::map<entity_id, entity_id> owner_map(const world& w)
{
    std::map<entity_id, entity_id> owner;
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id a : cc.assets) owner.emplace(a, cid);
    return owner;
}

void lg_snapshot(const world& w, probe& p)
{
    const recipe_registry& reg = *p.reg;
    const float res_mult = reg.price_band().reservation_mult;
    const std::map<entity_id, entity_id> owner = owner_map(w);
    p.snap.clear();
    for (const entity_id bid : sorted_processors(w))
    {
        const building_component& b = w.buildings.at(bid);
        if (!live_processor(b)) continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (rc == nullptr) continue;
        const entity_id m = market_for_tile(w, b.tile);
        const auto mi = w.markets.find(m);
        if (mi == w.markets.end()) continue;
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

void snapshot_pool_status(const world& w, const recipe_registry& reg, probe& p)
{
    p.pool_status.clear();
    const order_floor_map ordered = collect_order_floors(w);
    const grid_goods_params& grid = reg.grid_goods();
    for (const auto& [key, pool] : w.corp_market_pools)
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
            const auto [it, fresh] = p.pool_status.try_emplace(std::make_pair(body, g));
            probe::pool_stat& ps = it->second;
            if (!ordered.count({key.first, body, g})) { ps.st = x_poolheld; continue; }
            if (ps.st != x_ordered) continue;
            const float src = dispatch_source_price(w, ordered, key.first, key.second, g);
            if (fresh || src < ps.src) { ps.src = src; ps.home = dispatch_home_price(w, key.second, g); }
        }
    }
}

void lg_econ_lap(const world& cw, probe& p)
{
    using namespace export_refusal;
    world& w = const_cast<world&>(cw); // router path caches only (export_refusal.hpp)
    const recipe_registry& reg = *p.reg;
    context x(w, reg);
    const float lg_margin = reg.dispatch_margin();
    const auto no_shelf_class = [&](entity_id d, std::size_t g) {
        const auto it = p.pool_status.find(std::make_pair(w.markets.at(d).body, g));
        if (it == p.pool_status.end()) return static_cast<int>(x_none);
        const probe::pool_stat& ps = it->second;
        if (ps.st != x_ordered) return ps.st;
        const float pd = dispatch_market_price(w.markets.at(d), g);
        const bool floor_refuses = !(pd > ps.src * (1.0f + lg_margin)) && pd > ps.home * (1.0f + lg_margin);
        return floor_refuses ? static_cast<int>(x_ordered) : static_cast<int>(x_poolheld);
    };
    std::vector<good_markets> scans(resource_count);
    std::vector<bool> scanned(resource_count, false);
    p.proc_best.clear();
    for (const auto& [bid, s] : p.snap)
    {
        const recipe* rc = reg.get_recipe(w.buildings.at(bid).recipe);
        if (rc == nullptr) continue;
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            if (!(rc->inputs[g] > 0.0f) || reg.grid_goods().grid(g)) continue;
            const auto key = std::make_pair(s.m, g);
            if (p.proc_best.count(key)) continue;
            if (!scanned[g]) { scans[g] = scan_good(x, g, surplus_rule::dispatcher); scanned[g] = true; }
            const int b = classify_destination(x, s.m, g, scans[g].sur).best;
            p.proc_best.emplace(key, b < 0 ? no_shelf_class(s.m, g) : b);
        }
    }
}

void after_lap(const world& w, int lap, void* ctx)
{
    probe& p = *static_cast<probe*>(ctx);
    if (p.l_armed && lap == p.lap_pre) { lg_snapshot(w, p); snapshot_pool_status(w, *p.reg, p); return; }
    if (lap != p.lap_econ) return;
    p.px.clear();
    for (const auto& [mid, mc] : w.markets)
    {
        market_px& m = p.px[mid];
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            m.price[g] = mc.price[g]; m.base[g] = mc.base_price[g]; m.posted[g] = posted_price(mc, g);
            m.supply[g] = mc.supply[g]; m.demand[g] = mc.demand[g];
        }
    }
    if (p.l_armed) lg_econ_lap(w, p);
}

/// The L class of an input-starved processor (market_viability's lg_after_tick).
int l_class(const world& w, const recipe_registry& reg, const probe& p, entity_id bid,
            const building_component& b, const building_report& row)
{
    const std::size_t g = static_cast<std::size_t>(row.limiting_input);
    if (g >= resource_count) return -1;
    const entity_id m = market_for_tile(w, b.tile);
    if (w.markets.find(m) == w.markets.end()) return x_nomarket;
    const auto si = p.snap.find(bid);
    if (si == p.snap.end()) return -1;
    const proc_snap& s = si->second;
    const market_component& mc = w.markets.at(m);
    if (reg.grid_goods().grid(g)) return x_grid;
    if (!(mc.base_price[g] > 0.0f)) return x_unpriced;
    if (s.shelf[g] >= 1.0f)
    {
        const float base_rate = reg.economics(building_type::processing_facility).base_rate;
        const recipe* rc = reg.get_recipe(b.recipe);
        const float wt = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
        const float need = (rc ? rc->inputs[g] : 0.0f) * base_rate * row.effective_workforce * wt
                         * building_supply_scalar(b);
        const bool thin = need > 0.0f && (s.pool[g] + s.shelf[g]) / need < reg.t_idle();
        if (thin) return x_thin;
        if (!s.admits[g]) return x_ceiling;
        return x_contended;
    }
    const auto it = p.proc_best.find(std::make_pair(m, g));
    return it == p.proc_best.end() ? -1 : it->second;
}

// --- per-processor history -------------------------------------------------------
struct decom_rec
{
    int tick = 0;
    const char* by = "unknown";
    std::string why;
    float rev = 0, inp = 0, maint = 0, wages = 0, out_ratio = 1.0f, run = 0;
    std::string floored_good;
    int last = -1;
    std::string dem;
    std::string lim; ///< the last row's limiting input, when it was input-starved
};

struct hist
{
    int first_tick = -1000;            ///< -1000 = in the world as built
    const char* builder = "gen";
    float target_first = -1.0f;
    std::deque<int> rows;              ///< row states, last 8
    bool ever_ran = false;
    int state_count[ps_count] = {};    ///< every row since first seen
    bool decom = false;
    decom_rec d;
    int resumes = 0;
    int wf_tick = -9999, wf_value = -1; ///< last workforce_set agency event
    std::string wfz; ///< the market as the dial saw it when it last set target 0
};

struct counts
{
    std::map<std::string, long> c;
    void add(const std::string& k, long v = 1) { c[k] += v; }
};

/// Unit margin of a recipe at a market's current prices: outputs - inputs per run.
/// -1: unpriced (some output with no base price), 0: <= 0, 1: > 0.
int unit_margin(const world& w, const recipe_registry& reg, const building_component& b, float* out_ratio,
                std::string* worst_good)
{
    const recipe* rc = reg.get_recipe(b.recipe);
    const auto mi = w.markets.find(market_for_tile(w, b.tile));
    if (!rc || mi == w.markets.end()) return -1;
    const market_component& m = mi->second;
    float v = 0.0f, worst = 1e9f;
    for (std::size_t g = 0; g < resource_count; ++g)
    {
        const float p = m.price[g] > 0.0f ? m.price[g] : m.base_price[g];
        if (rc->outputs[g] > 0.0f)
        {
            if (!(m.base_price[g] > 0.0f)) return -1;
            v += rc->outputs[g] * p;
            const float r = m.price[g] / m.base_price[g];
            if (r < worst) { worst = r; if (worst_good) *worst_good = gname(g); }
        }
        if (rc->inputs[g] > 0.0f && !reg.grid_goods().grid(g)) v -= rc->inputs[g] * p;
    }
    if (out_ratio) *out_ratio = worst;
    return v > 0.0f ? 1 : 0;
}


/// The recipe's PRIMARY output (largest outputs x base) at its market, as the
/// workforce solver forecasts it: unpriced (no base price), no_demand (the
/// market bids none), glut (demand/supply under floor_mult^2: the solver's
/// sqrt forecast sits on the floor), demand_ok. "No buyer" = the first three.
std::string out_demand_class(const world& w, const recipe_registry& reg, const building_component& b)
{
    const recipe* rc = reg.get_recipe(b.recipe);
    const auto mi = w.markets.find(market_for_tile(w, b.tile));
    if (!rc || mi == w.markets.end()) return "nomarket";
    const market_component& m = mi->second;
    std::size_t best = resource_count;
    float bv = -1.0f;
    for (std::size_t g = 0; g < resource_count; ++g)
        if (rc->outputs[g] > 0.0f && rc->outputs[g] * m.base_price[g] > bv) { bv = rc->outputs[g] * m.base_price[g]; best = g; }
    if (best == resource_count) return "nooutput";
    if (!(m.base_price[best] > 0.0f)) return "unpriced_output";
    if (!(m.demand[best] > 0.0f)) return "no_demand";
    const float f = reg.price_band().floor_mult;
    if (m.supply[best] > 0.0f && m.demand[best] / m.supply[best] < f * f) return "glut";
    return "demand_ok";
}


/// Why the workforce dial set a processor's target to 0, read off its market at
/// the economy lap of that tick (the registers the solver read; it runs before
/// the clear). The solver forecasts each output at base x sqrt(demand / (supply
/// + this plant's full-run output)), clamped to the band, and bills each input
/// at its POSTED price (BL-1232). Classes: unpriced_output | out_forecast_floor
/// (the forecast is on the band floor: no buyer for this much output) |
/// inputs_dear (inputs at posted >= outputs at forecast; "+ceil" when an input's
/// posted price is over the fair-price ceiling) | fixed_costs (a positive unit
/// margin that does not cover wages + maintenance) | nomarket.
std::string dial_zero_class(const recipe_registry& reg, const building_component& b, const market_px* m)
{
    const recipe* rc = reg.get_recipe(b.recipe);
    if (!rc || !m) return "nomarket";
    const price_band_params& pb = reg.price_band();
    const float runs = reg.economics(building_type::processing_facility).base_rate * b.workforce_assigned;
    float out_v = 0.0f, in_v = 0.0f;
    bool floor = true, any_out = false, over = false;
    for (std::size_t g = 0; g < resource_count; ++g)
    {
        if (rc->outputs[g] > 0.0f)
        {
            const float base = m->base[g];
            if (!(base > 0.0f)) return "unpriced_output";
            any_out = true;
            const float sup = m->supply[g] + rc->outputs[g] * runs;
            float t = sup <= 0.0f ? base * pb.ceil_mult : base * std::sqrt(m->demand[g] / sup);
            t = std::clamp(t, base * pb.floor_mult, base * pb.ceil_mult);
            if (t > base * pb.floor_mult * 1.01f) floor = false;
            out_v += rc->outputs[g] * t;
        }
        if (rc->inputs[g] > 0.0f && !reg.grid_goods().grid(g))
        {
            in_v += rc->inputs[g] * m->posted[g];
            if (m->base[g] > 0.0f && m->posted[g] > m->base[g] * pb.reservation_mult) over = true;
        }
    }
    if (!any_out) return "nooutput";
    if (floor) return "out_forecast_floor";
    if (in_v >= out_v) return over ? "inputs_dear+ceil" : "inputs_dear";
    return "fixed_costs";
}

struct seed_out
{
    std::uint32_t seed = 0;
    std::string fail;
    counts h, t50;
    double secs = 0;
};

const char* margin_key(int m) { return m < 0 ? "unpriced" : m > 0 ? "pos" : "neg"; }

/// Classify every built processor not running into `c`.
void read_idle(const world& w, const recipe_registry& reg, const economy_report& rep, const probe& p,
               const std::map<entity_id, hist>& H, entity_id seat, counts& c, std::uint32_t seed,
               const char* label)
{
    const std::map<entity_id, entity_id> owner = owner_map(w);
    // pool claims per (corp, body): total assigned, decommissioned assigned,
    // decommissioned GRANTED (assigned x building_labour)
    struct pool_claim { float all = 0, decom = 0, decom_granted = 0; };
    std::map<std::pair<entity_id, entity_id>, pool_claim> claims;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.ticks_remaining > 0) continue;
        if (b.type != building_type::processing_facility && b.type != building_type::extraction_site) continue;
        const auto oi = owner.find(bid);
        if (oi == owner.end()) continue;
        const entity_id body = pool_key_body(w, pool_key_for_tile(w, b.tile));
        pool_claim& pc = claims[{oi->second, body}];
        pc.all += b.workforce_assigned;
        if (b.decommissioned)
        {
            pc.decom += b.workforce_assigned;
            const auto bl = rep.building_labour.find(bid);
            pc.decom_granted += b.workforce_assigned * (bl != rep.building_labour.end() ? bl->second : 1.0f);
        }
    }
    std::map<std::string, int> shown;
    const auto example = [&](const std::string& cls, const char* fmt, auto... args) {
        if (shown[cls]++ >= g_examples) return;
        std::printf("    ex %s seed %u %-34s ", label, seed, cls.c_str());
        std::printf(fmt, args...);
        std::printf("\n");
    };

    for (const entity_id bid : sorted_processors(w))
    {
        const building_component& b = w.buildings.at(bid);
        const building_report* row = row_of(rep, bid);
        const proc_state s = classify(b, row, reg);
        if (s == ps_build) continue;
        c.add("built");
        c.add(std::string("state.") + k_state_name[s]);
        if (s == ps_run) continue;
        const auto hi = H.find(bid);
        const hist* h = hi != H.end() ? &hi->second : nullptr;
        const recipe* rc = reg.get_recipe(b.recipe);
        const std::string rname = rc ? rc->name : std::string("(none)");
        const auto oi = owner.find(bid);
        const bool is_seat = oi != owner.end() && oi->second == seat;
        const std::string S = k_state_name[s];
        c.add(S + ".builder." + (h ? h->builder : "unknown"));
        if (is_seat) c.add(S + ".owner_seat");
        c.add(S + ".recipe." + rname);
        std::string wg;
        float wr = 1.0f;
        const int um = unit_margin(w, reg, b, &wr, &wg);
        c.add(S + ".margin." + margin_key(um));
        const std::string dem_now = out_demand_class(w, reg, b);
        c.add(S + ".outdem_now." + dem_now);

        if (s == ps_decom)
        {
            if (!h || !h->decom) { c.add("decom.by.untracked"); continue; }
            const decom_rec& d = h->d;
            c.add(std::string("decom.by.") + d.by);
            c.add("decom.why." + d.why);
            if (!d.lim.empty()) c.add("decom.starved_input." + d.lim);
            if (!d.lim.empty()) c.add("decom.starved_recipe_input." + rname + "." + d.lim);
            c.add(std::string("decom.last_state.") + (d.last < 0 ? "norow" : k_state_name[d.last]));
            c.add("decom.why_dem." + d.why + "." + (d.dem.empty() ? std::string("atbuild") : d.dem));
            c.add(std::string("decom.by_why.") + d.by + "." + d.why);
            c.add(std::string("decom.builder_why.") + h->builder + "." + d.why);
            if (!d.floored_good.empty()) c.add("decom.floored_good." + d.floored_good);
            if (h->wfz.size() && d.why.find("unsupplied") != std::string::npos)
            {
                c.add("decom.dialzero." + h->wfz);
                c.add("decom.dialzero_tick." + std::to_string(h->wf_tick));
                c.add("decom.dialzero_recipe." + rname + "." + h->wfz);
            }
            if (d.why.rfind("ran/", 0) == 0) c.add("decom.ran_recipe." + rname);
            char tk[32];
            std::snprintf(tk, sizeof tk, "%d", d.tick);
            c.add(std::string("decom.tick.") + (d.tick <= -1000 ? "atbuild" : tk));
            example("decom." + d.why, "%llu %-22s by %-7s t%d builder %s rev %.1f inp %.1f maint %.1f wages %.1f outr %.2f run %.2f",
                    static_cast<unsigned long long>(bid), rname.c_str(), d.by, d.tick, h->builder,
                    d.rev, d.inp, d.maint, d.wages, d.out_ratio, d.run);
        }
        else if (s == ps_nolab)
        {
            const entity_id body = row ? row->body : null_entity;
            const auto ci = row ? rep.workforce_contention.find({row->corp, row->body}) : rep.workforce_contention.end();
            const float scalar = ci != rep.workforce_contention.end() ? ci->second : 1.0f;
            const auto bl = rep.building_labour.find(bid);
            const float grant = bl != rep.building_labour.end() ? bl->second : scalar;
            float qual = 1.0f;
            if (rc && rc->qualified_workforce > 0.0f)
                if (const auto tn = w.tile_to_nation.find(b.tile); tn != w.tile_to_nation.end())
                    if (const auto qi = rep.qualified_contention.find({tn->second, body}); qi != rep.qualified_contention.end())
                        qual = qi->second;
            std::string why;
            if (b.workforce_assigned <= 0.0f) why = "assigned0";
            else if (scalar <= 0.0f) why = "region_empty";
            else if (scalar < 1.0f && qual < 1.0f) why = "contended+qualified";
            else if (scalar < 1.0f) why = "contended";
            else if (qual < 1.0f) why = "qualified";
            else why = "other";
            c.add("nolab.why." + why);
            const pool_claim* pc = nullptr;
            if (oi != owner.end())
                if (const auto pi = claims.find({oi->second, pool_key_body(w, pool_key_for_tile(w, b.tile))}); pi != claims.end())
                    pc = &pi->second;
            if (pc && pc->decom_granted >= b.workforce_assigned && b.workforce_assigned > 0.0f)
                c.add("nolab.decom_held_enough_labour");
            if (pc && pc->all > 0.0f)
                c.add("nolab.decom_claim_permille_sum", static_cast<long>(1000.0f * pc->decom / pc->all));
            c.add("nolab.scalar_permille_sum", static_cast<long>(1000.0f * scalar));
            example("nolab." + why, "%llu %-22s assigned %.2f grant %.2f scalar %.3f qual %.2f decom-claim %.2f/%.2f granted-to-decom %.2f",
                    static_cast<unsigned long long>(bid), rname.c_str(), b.workforce_assigned, grant, scalar, qual,
                    pc ? pc->decom : 0.0f, pc ? pc->all : 0.0f, pc ? pc->decom_granted : 0.0f);
        }
        else if (s == ps_unsup)
        {
            std::string why;
            if (building_supply_scalar(b) <= 0.0f) why = "supply0";
            else if (h && h->wf_value == 0) why = "target0/verb_dial";
            else if (h && h->target_first == 0.0f && h->wf_tick < -1000) why = "target0/at_first_seen";
            else if (is_seat) why = "target0/seat_autosolve";
            else why = "target0/unknown";
            c.add("unsup.why." + why);
            c.add("unsup.why_margin." + why + "." + margin_key(um));
            c.add("unsup.why_dem." + why + "." + dem_now);
            if (h && !h->wfz.empty()) c.add("unsup.dialzero." + h->wfz);
            if (h && !h->wfz.empty()) c.add("unsup.dialzero_tick." + std::to_string(h->wf_tick));
            if (h && !h->wfz.empty()) c.add("unsup.dialzero_recipe." + rname + "." + h->wfz);
            example("unsup." + why, "%llu %-22s target %.0f supply %d wf_set t%d margin %s worst-out %s %.2f",
                    static_cast<unsigned long long>(bid), rname.c_str(), static_cast<double>(b.workforce_target),
                    b.supply_factor_permille, h ? h->wf_tick : -1, margin_key(um), wg.c_str(), wr);
        }
        else if (s == ps_input)
        {
            const std::size_t g = static_cast<std::size_t>(row->limiting_input);
            const std::string gn = g < resource_count ? gname(g) : std::string("?");
            const int lc = p.l_armed || !p.snap.empty() ? l_class(w, reg, p, bid, b, *row) : -1;
            const std::string cls = lc < 0 ? std::string("unread") : std::string(k_x_name[lc]);
            c.add("input.good." + gn);
            c.add("input.class." + cls);
            c.add("input.good_class." + gn + "." + cls);
            c.add(std::string("input.builder_class.") + (h ? h->builder : "unknown") + "." + cls);
        }
        else
        {
            c.add(std::string("other.why.") + (rc == nullptr ? "norecipe" : row == nullptr ? "norow" : "rowidle_unlimited"));
        }
    }
}

void run_seed(std::uint32_t seed, int ticks, seed_out& out)
{
    const auto t0 = std::chrono::steady_clock::now();
    out.seed = seed;
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
        if (std::strcmp(k_campaign_settle_lap_names[i], "convoys") == 0) p.lap_pre = i;
        if (std::strcmp(k_campaign_settle_lap_names[i], "run_economy_step") == 0) p.lap_econ = i;
    }

    std::map<entity_id, hist> H;
    for (const entity_id bid : sorted_processors(w))
    {
        const building_component& b = w.buildings.at(bid);
        hist& h = H[bid];
        h.target_first = b.workforce_target;
        if (b.decommissioned)
        {
            h.decom = true;
            h.d.tick = -1000;
            h.d.by = "atbuild";
            h.d.why = b.loss_streak >= 8 ? "atbuild/streak" : "atbuild";
        }
    }

    std::map<std::string, int> dz_shown;
    // after each tick: rows, transitions, events
    const auto observe = [&](int t, const economy_report& rep, const char* phase) {
        for (const agency_event& ev : rep.agency_events)
            if (ev.what == agency_event::kind::workforce_set)
                if (auto hi = H.find(ev.building); hi != H.end())
                {
                    hi->second.wf_tick = t; hi->second.wf_value = ev.value;
                    if (ev.value == 0)
                        if (const auto bi = w.buildings.find(ev.building); bi != w.buildings.end())
                        {
                            const auto pi = p.px.find(market_for_tile(w, bi->second.tile));
                            hi->second.wfz = dial_zero_class(reg, bi->second, pi != p.px.end() ? &pi->second : nullptr);
                            // One line per (recipe, class) per seed: the registers the dial read.
                            const recipe* rc = reg.get_recipe(bi->second.recipe);
                            const std::string key = (rc ? rc->name : std::string("?")) + "." + hi->second.wfz;
                            if (rc && pi != p.px.end() && dz_shown[key]++ < g_examples)
                            {
                                const market_px& m = pi->second;
                                const float runs = reg.economics(building_type::processing_facility).base_rate
                                                 * bi->second.workforce_assigned;
                                std::printf("    dz seed %u t%d %llu %-36s", seed, t,
                                            static_cast<unsigned long long>(ev.building), key.c_str());
                                for (std::size_t g = 0; g < resource_count; ++g)
                                {
                                    if (rc->outputs[g] > 0.0f)
                                        std::printf(" OUT %s base %.2f px %.2f sup %.1f dem %.1f +q %.1f",
                                                    gname(g).c_str(), m.base[g], m.price[g], m.supply[g], m.demand[g],
                                                    rc->outputs[g] * runs);
                                    if (rc->inputs[g] > 0.0f)
                                        std::printf(" IN %s x%.1f base %.2f posted %.2f", gname(g).c_str(),
                                                    rc->inputs[g], m.base[g], m.posted[g]);
                                }
                                std::printf("\n");
                            }
                        }
                }
        for (const entity_id bid : sorted_processors(w))
        {
            const building_component& b = w.buildings.at(bid);
            auto [it, fresh] = H.try_emplace(bid);
            hist& h = it->second;
            if (fresh) { h.first_tick = t; h.builder = phase; h.target_first = b.workforce_target; }
            const building_report* row = row_of(rep, bid);
            if (row && b.ticks_remaining <= 0)
            {
                const proc_state rs = classify_row(b, row, reg);
                h.rows.push_back(rs);
                if (h.rows.size() > 8) h.rows.pop_front();
                ++h.state_count[rs];
                if (rs == ps_run) h.ever_ran = true;
            }
            if (b.decommissioned && !h.decom)
            {
                h.decom = true;
                decom_rec& d = h.d;
                d = decom_rec{};
                d.tick = t;
                d.by = b.loss_streak >= 8 ? "reflex" : b.loss_streak == 0 ? "verb" : "unknown";
                const proc_state last = row ? classify_row(b, row, reg) : ps_other;
                d.last = row ? static_cast<int>(last) : -1;
                d.dem = out_demand_class(w, reg, b);
                if (row && last == ps_input && static_cast<std::size_t>(row->limiting_input) < resource_count)
                    d.lim = gname(static_cast<std::size_t>(row->limiting_input));
                if (row)
                {
                    const building_profit bp = estimate_building_profit(w, reg, rep, bid);
                    d.maint = bp.maintenance;
                    d.wages = bp.wages;
                    d.run   = row->run;
                    const recipe* rc = reg.get_recipe(b.recipe);
                    const entity_id m = market_for_tile(w, b.tile);
                    const auto pi = p.px.find(m);
                    if (rc && pi != p.px.end())
                    {
                        float tot = 0.0f;
                        for (float o : rc->outputs) tot += o;
                        const float runs = tot > 0.0f ? row->output_quantity / tot : 0.0f;
                        float worst = 1e9f;
                        for (std::size_t g = 0; g < resource_count; ++g)
                        {
                            const float pr = pi->second.price[g] > 0.0f ? pi->second.price[g] : pi->second.base[g];
                            d.rev += rc->outputs[g] * runs * pr;
                            d.inp += rc->inputs[g] * runs * pr;
                            if (rc->outputs[g] > 0.0f && pi->second.base[g] > 0.0f)
                            {
                                const float r = pi->second.price[g] / pi->second.base[g];
                                if (r < worst) { worst = r; d.floored_good = gname(g); }
                            }
                        }
                        d.out_ratio = worst;
                    }
                }
                if (!h.ever_ran)
                {
                    int best = ps_other, bn = -1;
                    for (int s = 0; s < ps_count; ++s) if (h.state_count[s] > bn) { bn = h.state_count[s]; best = s; }
                    d.why = std::string("never_ran/") + (bn > 0 ? k_state_name[best] : "norows");
                    d.floored_good.clear();
                }
                else if (last == ps_input) { d.why = "starved"; d.floored_good.clear(); }
                else if (last != ps_run) { d.why = std::string("idle_") + k_state_name[last]; d.floored_good.clear(); }
                else
                {
                    if (d.out_ratio <= k_floored) d.why = "ran/out_floored";
                    else
                    {
                        d.floored_good.clear();
                        if (d.inp >= d.rev) d.why = "ran/inputs_over_rev";
                        else if (d.rev - d.inp < d.maint + d.wages) d.why = "ran/fixed_over_margin";
                        else d.why = "ran/net_pos";
                    }
                }
            }
            else if (!b.decommissioned && h.decom)
            {
                h.decom = false;
                ++h.resumes;
            }
        }
    };

    economy_report last_settle;
    settle_tick_hooks hooks;
    hooks.after_lap = after_lap;
    hooks.ctx = &p;
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        p.l_armed = (step == k_campaign_settle_ticks - 1);
        settle_tick_result res = run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true, &hooks);
        observe(step - k_campaign_settle_ticks, res.report, "settle");
        if (step == k_campaign_settle_ticks - 1) last_settle = std::move(res.report);
    }
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    if (seat.seated == null_entity) { out.fail = "no corporation seated"; return; }
    read_idle(w, reg, last_settle, p, H, seat.seated, out.h, seed, "h");
    p.l_armed = false;
    p.snap.clear();

    constexpr int k_econ_tick_days = 90;
    for (int k = 1; k <= ticks; ++k)
    {
        const int day = k * k_econ_tick_days;
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = day;
        p.l_armed = (k == k_t50);
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day,
                                                 /*spectating=*/false, &hooks);
        observe(k, res.report, "play");
        if (k == k_t50) read_idle(w, reg, res.report, p, H, seat.seated, out.t50, seed, "t50");
        p.l_armed = false;
    }
    out.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

void print_counts(const char* label, const counts& c, int built)
{
    std::printf("  [%s] built %d\n", label, built);
    for (const auto& [k, v] : c.c)
        std::printf("    %-60s %7ld  %5.1f%%\n", k.c_str(), v, built ? 100.0 * v / built : 0.0);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    int ticks = k_t50;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seed_list(argv[++i]);
        else if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--examples") && i + 1 < argc) g_examples = std::atoi(argv[++i]);
        else { std::fprintf(stderr, "usage: handoff_idle_probe [--seeds a,b] [--ticks N] [--examples N]\n"); return 2; }
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::fprintf(stderr, "no seeds\n"); return 2; }
    std::printf("handoff_idle_probe: %zu seeds, %d play ticks\n", seeds.size(), ticks);
    counts ph, p50;
    int fails = 0;
    for (const std::uint32_t s : seeds)
    {
        seed_out o;
        run_seed(s, ticks, o);
        if (!o.fail.empty()) { std::printf("seed %u FAIL %s\n", s, o.fail.c_str()); ++fails; continue; }
        const int bh = static_cast<int>(o.h.c["built"]), b50 = static_cast<int>(o.t50.c["built"]);
        std::printf("seed %u  (%.0f s)  handoff built %d run %ld (%.1f%%)  t50 built %d run %ld\n", s, o.secs, bh,
                    o.h.c["state.run"], bh ? 100.0 * o.h.c["state.run"] / bh : 0.0, b50, o.t50.c["state.run"]);
        for (const auto& [k, v] : o.h.c)   std::printf("COUNTS %u h.%s %ld\n", s, k.c_str(), v);
        for (const auto& [k, v] : o.t50.c) std::printf("COUNTS %u t50.%s %ld\n", s, k.c_str(), v);
        for (const auto& [k, v] : o.h.c)   ph.add(k, v);
        for (const auto& [k, v] : o.t50.c) p50.add(k, v);
        std::fflush(stdout);
    }
    std::printf("\nPOOLED over %zu seeds (%d failed)\n", seeds.size(), fails);
    print_counts("handoff", ph, static_cast<int>(ph.c["built"]));
    print_counts("t50", p50, static_cast<int>(p50.c["built"]));
    return fails ? 1 : 0;
}
