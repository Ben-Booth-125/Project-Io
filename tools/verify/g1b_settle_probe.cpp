// ---------------------------------------------------------------------------
// g1b_settle_probe — BL-1217 G1b (G1 plants running): why is each processor
// that is input-starved AT THE HANDOFF starved?
// ---------------------------------------------------------------------------
// THE SET. market_viability's G1b, counted the same way: after the 12-tick
// settle and seat_player_corporation, every built processor that is `input`
// on the settle's last report, plus every decommissioned processor whose LAST
// report row was `input` (mothballed after starving). The probe recounts it
// per seed (g1b.now / g1b.decom) so it can be checked against the gate.
//
// THE WINDOW. The settle itself: econ steps 0..11 = ticks -12..-1. One
// after_lap hook (a pure reader; nothing written) records, per market and
// tick: the shelf at the conv / econ / clear / budget laps, posted / base and
// the fair-price ceiling's verdict, every pool at the market, convoy arrivals
// and departures, household / background fills, hauler_want; per processor and
// tick: state, limiting input, full-run need, its owner's pool at its market;
// per producer: running / idle / decommissioned / under construction, and
// production by market. BL-1209 ration rows are kept per tick.
//
// CLASSES (one per plant; precedence top to bottom). T* = the plant's last
// reported tick (the handoff, -1, for one starved now); g = its limiting input
// at T*; M = its market; gap = t_idle x need - own pool at T*.
//   g.grid             g is a grid good (never cargo, never shelf)
//   d.ceiling          shelf_pre(T*) >= gap at M, but posted > the ceiling
//   c.rationed.*       shelf_pre(T*) >= gap, admitted: the BL-1209 share or
//                      earlier draws left it short (who else drew: procs/sites)
//   c.taken.<taker>    at T*-1 the shelf after listings held >= gap; households /
//                      background / nation / spoilage took it before T*'s draw
//   g.pool_held.<why>  other corps' pools at M hold >= gap of g, not on the
//                      shelf at the draw: opening_held (BL-1217 D5 hold) |
//                      sell_order | reserved_for_idle_plants / _live_plants
//                      (processor_reservation) | unlisted_other (in practice:
//                      convoy cargo landed this tick, listed at the clear)
//   f.placed|switched  the plant was built, or switched recipe, during the
//                      settle and has never run on its current recipe
//   g.underproduced    M produces g (or receives it) but less than the gap
//   e.surplus_unshipped.<cls>  a producer of g runs elsewhere on the body
//                      WITHIN REACH (input_reach::market_within_reach, the
//                      sim's own test), its market shows a shelf surplus, and
//                      nothing arrives; <cls> = the export dispatcher's best
//                      class into M at T* (export_refusal.hpp)
//   e.no_surplus_at_source.<why>  ... but the reachable producers' markets
//                      show no shelf surplus: consumed_local | pooled |
//                      opening_held
// Switch history (`sw=`, tick:from>to/strategic|reflex, strategic = a
// set_recipe in the decision ring that tick) and the decommissioner
// (`idle_src=`: reflex, or strategic.r<corp_decision_reason>) print per plant.
//   b.<state>          producers of g within reach exist (incl. M) but none
//                      ran at T*: build / idle (and why) / decom
//   a.unreachable      producers of g exist on the body, none within reach
//   a.none_on_body     no producer of g on the body at all
//   g.other
// The supply class (the same ladder with f skipped) prints beside f.
// For decommissioned plants: who (loss_streak >= 8: the reflex; else the
// strategic idle verb), when, and the workforce target at the time.
// A TRAJECTORY block per limiting good pools, over every G1b plant's (seed, M,
// g), each settle tick: production at M / body, shelf, posted/ceiling,
// markets over the ceiling, the plants' want filled / unfilled / silenced,
// producers at M and on the body by state.
// `P` lines: one per G1b plant, every field (machine-readable).
//
// --cf ceiling_off | bg_off: a registry MEASUREMENT switch applied from the
// first settle tick (probe-only counterfactual), then the G1b recount.
// --cf release_silenced: the probe WRITES the world (counterfactual only): at
// the econ lap, every (market, input) where a live processor's want is
// silenced by the ceiling has its held opening stock released, as a posted
// want would release it at the clear.
//
// READING 2026-10-09 (base 6ecdd441, 16 library seeds): 314 = the gate's G1b
// exactly (170 now, 144 decom); --cf release_silenced 314 (no change),
// bg_off 263 / 2728 = 9.6%, ceiling_off 406 / 2724 = 14.9%.
//
// Usage: build_gen/verify/g1b_settle_probe.exe [--seeds a,b] [--cf X]
// Build:  ./tools/verify/build_lua_harness.sh g1b_settle_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "export_refusal.hpp"
#include "world/building_profit.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/input_reach.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
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

constexpr int NT = k_campaign_settle_ticks; // 12
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

// --- market_viability's classify, verbatim ------------------------------------
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

struct counts
{
    std::map<std::string, long> c;
    void add(const std::string& k, long v = 1) { c[k] += v; }
};

using arr = std::array<float, resource_count>;

/// One market, one settle tick.
struct mrec
{
    arr pre{}, econ{}, clr{}, bud{};
    arr posted{}, base{}, price{};
    arr pools{};
    arr held{};                ///< opening_stock_held at this market, every corp (conv lap)
    arr reserved{};            ///< pool units auto-surplus keeps for the owner's processors here (min(pool, processor_reservation))
    arr reserved_idle{};       ///< ... of which only decommissioned / dial-zeroed / unbuilt processors' rows hold
    arr ordered{};             ///< pool units under a standing sell order (order_controls: not auto-listed)
    arr sur{};                 ///< market_shelf_surplus (conv lap)
    arr arrived{}, departed{}, departed_shelf{};
    arr hh{}, bg{}, dem{}, hw{};
    arr prod{};
    arr p_run{}, p_idle{}, p_decom{}, p_build{};
    std::array<bool, resource_count> admits{};
    arr xbest = [] { arr a{}; a.fill(-2.0f); return a; }();
};

/// One processor, one settle tick.
struct prec
{
    bool seen = false;
    int state = ps_other;
    int lim = -1;
    float run = 0.0f;
    float wt = 0.0f;
    std::uint16_t recipe = 0;
    arr need{}, pool{};
    bool row = false;
};

/// A producer building's state at one tick: 0 run, 1 idle, 2 decom, 3 build.
using pstate_map = std::map<entity_id, int>;

struct convoy_seen { entity_id src = null_entity, dst = null_entity; std::size_t g = 0; float q = 0; };

struct probe
{
    const recipe_registry* reg = nullptr;
    int lap_pre = -1, lap_econ = -1, lap_clear = -1, lap_bud = -1;
    int tick = 0;
    std::vector<std::map<entity_id, mrec>>* MT = nullptr;
    std::vector<std::map<entity_id, prec>>* PT = nullptr;
    std::map<uint32_t, convoy_seen> convoys;
    std::set<std::pair<entity_id, std::size_t>> tracked; ///< this tick: (market, input) predicted short
};

std::map<entity_id, entity_id> owner_map(const world& w)
{
    std::map<entity_id, entity_id> owner;
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id a : cc.assets) owner.emplace(a, cid);
    return owner;
}

void after_lap(const world& w, int lap, void* ctx)
{
    probe& p = *static_cast<probe*>(ctx);
    const float res_mult = p.reg->price_band().reservation_mult;
    std::map<entity_id, mrec>& M = (*p.MT)[p.tick];
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
        for (const auto& [key, h] : w.opening_stock_held)
        {
            const auto it = M.find(key.second);
            if (it == M.end()) continue;
            for (std::size_t g = 0; g < resource_count; ++g) it->second.held[g] += std::max(0.0f, h[g]);
        }
        for (auto& [mid, r] : M)
            for (std::size_t g = 0; g < resource_count; ++g) r.sur[g] = market_shelf_surplus(w, mid, g);
        // what auto-surplus keeps off the shelf: the processor reservation (every
        // processor of the corp at the market -- the rule does not ask whether it
        // runs), split into the part only idle plants hold; and standing sell orders
        {
            const float rate = p.reg->economics(building_type::processing_facility).base_rate;
            for (const auto& [key, pool] : w.corp_market_pools)
            {
                const auto it = M.find(key.second);
                const auto ci = w.corporations.find(key.first);
                if (it == M.end() || ci == w.corporations.end()) continue;
                const std::array<float, resource_count> res = processor_reservation(w, *p.reg, key.first, key.second);
                std::array<float, resource_count> live{};
                for (const entity_id bid : ci->second.assets)
                {
                    const auto bi = w.buildings.find(bid);
                    if (bi == w.buildings.end() || bi->second.type != building_type::processing_facility) continue;
                    const building_component& b = bi->second;
                    if (pool_key_for_tile(w, b.tile) != key.second) continue;
                    if (b.ticks_remaining > 0 || b.decommissioned || b.workforce_target <= 0.0f) continue;
                    const recipe* rc = p.reg->get_recipe(b.recipe);
                    if (!rc) continue;
                    for (std::size_t g = 0; g < resource_count; ++g) live[g] += rc->inputs[g] * rate * b.workforce_assigned;
                }
                const entity_id body = w.markets.at(key.second).body;
                for (std::size_t g = 0; g < resource_count; ++g)
                {
                    const float q = std::max(0.0f, pool.quantities[g]);
                    const float rv = std::min(q, res[g]);
                    it->second.reserved[g] += rv;
                    it->second.reserved_idle[g] += rv - std::min(q, live[g]);
                    bool ord = false;
                    for (const auto& so : w.sell_orders)
                        if (so.corp == key.first && so.body == body && static_cast<std::size_t>(so.resource) == g) { ord = true; break; }
                    if (ord) it->second.ordered[g] += q;
                }
            }
        }
        std::set<uint32_t> now;
        for (const convoy_component& c : w.convoys) now.insert(c.id);
        for (const auto& [id, cs] : p.convoys)
            if (!now.count(id))
                if (auto it = M.find(cs.dst); it != M.end()) it->second.arrived[cs.g] += cs.q;
        // every live processor's owner pool, and which (market, input) look short
        const std::map<entity_id, entity_id> owner = owner_map(w);
        const float rate = p.reg->economics(building_type::processing_facility).base_rate;
        const float t_idle = p.reg->t_idle();
        p.tracked.clear();
        for (const auto& [bid, b] : w.buildings)
        {
            if (b.type != building_type::processing_facility) continue;
            const entity_id mid = market_for_tile(w, b.tile);
            const auto oi = owner.find(bid);
            prec& pr = (*p.PT)[p.tick][bid];
            if (oi != owner.end())
                if (const stockpile_component* pool = w.find_pool(oi->second, mid))
                    for (std::size_t g = 0; g < resource_count; ++g) pr.pool[g] = pool->quantities[g];
            if (b.ticks_remaining > 0 || b.decommissioned) continue;
            const recipe* rc = p.reg->get_recipe(b.recipe);
            const auto mi = M.find(mid);
            if (!rc || mi == M.end()) continue;
            const float bf = rate * b.workforce_assigned * std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f) * building_supply_scalar(b);
            for (std::size_t g = 0; g < resource_count; ++g)
            {
                if (!(rc->inputs[g] > 0.0f) || p.reg->grid_goods().grid(g)) continue;
                const float need = rc->inputs[g] * bf;
                const float shelf = mi->second.admits[g] ? std::max(0.0f, mi->second.pre[g]) : 0.0f;
                if (need > 0.0f && pr.pool[g] + shelf < t_idle * need) p.tracked.insert({mid, g});
            }
        }
        return;
    }
    if (lap == p.lap_econ)
    {
        // --cf release_silenced (probe-only counterfactual; WRITES the world): a
        // processor's want the ceiling silenced counts as a bid for the held-stock
        // release, as a posted want does (market_clearing.cpp, BL-1217 D5). Run
        // here, before this tick's clear reads the holds.
        if (g_cf == "release_silenced" && !w.opening_stock_held.empty())
        {
            world& mw = const_cast<world&>(w);
            const std::map<entity_id, entity_id> owner = owner_map(w);
            const float rate = p.reg->economics(building_type::processing_facility).base_rate;
            std::set<std::pair<entity_id, std::size_t>> silenced;
            for (const auto& [bid, b] : w.buildings)
            {
                if (b.type != building_type::processing_facility || b.ticks_remaining > 0 || b.decommissioned) continue;
                const recipe* rc = p.reg->get_recipe(b.recipe);
                const entity_id mid = market_for_tile(w, b.tile);
                const auto mi = w.markets.find(mid);
                if (!rc || mi == w.markets.end()) continue;
                const float bf = rate * b.workforce_assigned * std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f) * building_supply_scalar(b);
                if (!(bf > 0.0f)) continue;
                const auto oi = owner.find(bid);
                const stockpile_component* pool = oi != owner.end() ? w.find_pool(oi->second, mid) : nullptr;
                for (std::size_t g = 0; g < resource_count; ++g)
                {
                    if (!(rc->inputs[g] > 0.0f) || shelf_admits(mi->second, g, res_mult, true)) continue;
                    if (rc->inputs[g] * bf > (pool ? pool->quantities[g] : 0.0f)) silenced.insert({mid, g});
                }
            }
            for (auto& [key, h] : mw.opening_stock_held)
                for (std::size_t g = 0; g < resource_count; ++g)
                    if (silenced.count({key.second, g})) h[g] = 0.0f;
        }
        for (const auto& [mid, mc] : w.markets)
        {
            mrec& r = M[mid];
            for (std::size_t g = 0; g < resource_count; ++g) { r.econ[g] = mc.inventory[g]; r.price[g] = mc.price[g]; }
        }
        for (const convoy_component& c : w.convoys)
            if (!p.convoys.count(c.id))
                if (auto it = M.find(c.source_market); it != M.end())
                {
                    const std::size_t g = static_cast<std::size_t>(c.cargo_resource);
                    if (c.corp == null_entity) it->second.departed_shelf[g] += c.cargo_qty;
                    else it->second.departed[g] += c.cargo_qty;
                }
        if (!p.tracked.empty())
        {
            world& mw = const_cast<world&>(w); // router path caches only (export_refusal.hpp)
            export_refusal::context x(mw, *p.reg);
            std::map<std::size_t, export_refusal::good_markets> scans;
            for (const auto& [mid, g] : p.tracked)
            {
                const auto it = M.find(mid);
                if (it == M.end()) continue;
                auto si = scans.find(g);
                if (si == scans.end())
                    si = scans.emplace(g, export_refusal::scan_good(x, g, export_refusal::surplus_rule::dispatcher)).first;
                it->second.xbest[g] = static_cast<float>(export_refusal::classify_destination(x, mid, g, si->second.sur).best);
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

void producer_goods(const recipe_registry& reg, const building_component& b, std::vector<std::size_t>& out)
{
    out.clear();
    if (b.type == building_type::extraction_site) { out.push_back(static_cast<std::size_t>(b.target_resource)); return; }
    if (b.type != building_type::processing_facility) return;
    const recipe* rc = reg.get_recipe(b.recipe);
    if (!rc) return;
    for (std::size_t g = 0; g < resource_count; ++g) if (rc->outputs[g] > 0.0f) out.push_back(g);
}

/// After the tick: production, the producer census, every producer's state.
void census_producers(const world& w, const recipe_registry& reg, const economy_report& rep,
                      std::map<entity_id, mrec>& M, pstate_map& ps)
{
    std::vector<std::size_t> gs;
    for (const auto& [bid, b] : w.buildings)
    {
        producer_goods(reg, b, gs);
        if (gs.empty()) continue;
        const auto it = M.find(market_for_tile(w, b.tile));
        const building_report* row = row_of(rep, bid);
        const bool run = b.ticks_remaining <= 0 && !b.decommissioned && row && row->active;
        const int st = b.ticks_remaining > 0 ? 3 : b.decommissioned ? 2 : run ? 0 : 1;
        ps[bid] = st;
        if (it == M.end()) continue;
        mrec& r = it->second;
        for (const std::size_t g : gs)
        {
            if (st == 3) r.p_build[g] += 1.0f;
            else if (st == 2) r.p_decom[g] += 1.0f;
            else if (st == 0) r.p_run[g] += 1.0f;
            else r.p_idle[g] += 1.0f;
        }
        if (!run) continue;
        for (std::size_t g = 0; g < resource_count; ++g) r.prod[g] += building_output(w, reg, bid, b, g, &rep);
    }
}

struct traj_row
{
    double pairs = 0, prod_m = 0, prod_body = 0, shelf = 0, pr_ceil = 0, over_ceiling = 0, hh = 0, bg = 0, hw = 0, pools_m = 0, arrived = 0;
    double filled = 0, unfilled = 0, silenced = 0, plants = 0;
    double p_run_m = 0, p_idle_m = 0, p_decom_m = 0, p_build_m = 0, p_run_b = 0, p_idle_b = 0, p_decom_b = 0, p_build_b = 0;
};
std::map<std::string, std::vector<traj_row>> g_traj;

struct seed_out
{
    std::uint32_t seed = 0;
    std::string fail;
    counts c;
    double secs = 0;
};

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

    if (!g_cf.empty())
    {
        recipe_registry& mreg = const_cast<recipe_registry&>(reg); // measurement only (--cf)
        if (g_cf == "bg_off") { background_demand_params b = reg.background_demand(); b.consumes = false; mreg.set_background_demand(b); }
        else if (g_cf == "release_silenced") {}
        else if (g_cf == "ceiling_off") { price_band_params pb = reg.price_band(); pb.reservation_mult = 0.0f; mreg.set_price_band(pb); }
        else { out.fail = "unknown --cf " + g_cf; return; }
    }

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
    std::vector<std::map<entity_id, mrec>> MT(NT);
    std::vector<std::map<entity_id, prec>> PT(NT);
    std::vector<pstate_map> PS(NT);
    std::vector<std::vector<shelf_ration_row>> RAT(NT);
    p.MT = &MT; p.PT = &PT;
    settle_tick_hooks hooks;
    hooks.after_lap = after_lap;
    hooks.ctx = &p;

    // the world as built
    std::map<entity_id, std::uint16_t> recipe_at_build;
    std::set<entity_id> at_build;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility) { at_build.insert(bid); recipe_at_build[bid] = b.recipe; }
    std::map<entity_id, entity_id> mkt_body;
    for (const auto& [mid, mc] : w.markets) mkt_body[mid] = mc.body;

    std::map<entity_id, int> built_tick, switch_tick, decom_tick, last_row_tick, first_seen;
    std::map<entity_id, const char*> decom_by;
    std::map<entity_id, float> decom_wt;
    std::map<entity_id, std::pair<int, int>> last_row; // (state, lim)
    std::set<entity_id> decom_before;
    for (const auto& [bid, b] : w.buildings) if (b.decommissioned) decom_before.insert(bid);
    const float base_rate = reg.economics(building_type::processing_facility).base_rate;

    economy_report last_settle;
    std::map<entity_id, std::string> switch_hist;   // bid -> "t-11:a>b/reflex;..."
    std::map<entity_id, std::string> idle_src;      // bid -> "strategic.<reason>" | "reflex"
    std::map<entity_id, std::uint16_t> recipe_prev = recipe_at_build;
    for (int step = 0; step < NT; ++step)
    {
        p.tick = step;
        const std::size_t ring_before = w.ai_decisions.total;
        settle_tick_result res = run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true, &hooks);
        const economy_report& rep = res.report;
        // the strategic tier's commands this tick (the ring's newest entries)
        std::map<std::pair<entity_id, int>, int> strat; // (subject, verb) -> reason
        {
            const corp_decision_ring& ring = w.ai_decisions;
            const std::size_t fresh = std::min<std::size_t>(ring.total - ring_before, ring.entries.size());
            if (ring.total - ring_before > ring.entries.size()) C.add("probe.ring_overflow");
            const std::size_t n = ring.entries.size();
            for (std::size_t i = 0; i < fresh; ++i)
            {
                // newest-last order: the slot before 'next' (wrapped) is the newest
                const std::size_t idx = n < corp_decision_ring::capacity ? n - 1 - i : (ring.next + n - 1 - i) % n;
                const corp_decision& d = ring.entries[idx];
                strat[{d.command.subject, static_cast<int>(d.command.verb)}] = static_cast<int>(d.reason);
            }
        }
        for (const agency_event& ev : rep.agency_events)
        {
            if (ev.what == agency_event::kind::recipe_switch)
            {
                const bool st = strat.count({ev.building, static_cast<int>(corp_verb::set_recipe)}) > 0;
                const recipe* from = recipe_prev.count(ev.building) ? reg.get_recipe(recipe_prev[ev.building]) : nullptr;
                const recipe* to = reg.get_recipe(ev.new_recipe);
                switch_hist[ev.building] += "t" + std::to_string(step - NT) + ":" + (from ? from->name : std::string("?")) + ">" +
                                            (to ? to->name : std::string("?")) + (st ? "/strategic;" : "/reflex;");
                C.add(std::string("switch_events.") + (st ? "strategic" : "reflex"));
                recipe_prev[ev.building] = ev.new_recipe;
            }
            if (ev.what == agency_event::kind::idled)
            {
                const auto si = strat.find({ev.building, static_cast<int>(corp_verb::idle)});
                idle_src[ev.building] = si != strat.end() ? "strategic.r" + std::to_string(si->second) : "reflex";
            }
        }
        census_producers(w, reg, rep, MT[step], PS[step]);
        RAT[step] = rep.shelf_rations;
        for (const agency_event& ev : rep.agency_events)
        {
            if (ev.what == agency_event::kind::built && !built_tick.count(ev.building)) built_tick[ev.building] = step;
            if (ev.what == agency_event::kind::recipe_switch) switch_tick[ev.building] = step;
        }
        for (const auto& [bid, b] : w.buildings)
        {
            if (b.type != building_type::processing_facility) continue;
            if (!first_seen.count(bid)) first_seen[bid] = step;
            prec& pr = PT[step][bid];
            pr.seen = true;
            const building_report* row = row_of(rep, bid);
            pr.state = classify(b, row, reg);
            pr.row = row != nullptr;
            pr.wt = b.workforce_target;
            pr.recipe = b.recipe;
            if (row)
            {
                pr.run = row->run;
                pr.lim = row->has_limiting ? static_cast<int>(row->limiting_input) : -1;
                if (b.ticks_remaining <= 0)
                {
                    const proc_state s = classify_row(b, row, reg);
                    last_row[bid] = {s, s == ps_input ? pr.lim : -1};
                    last_row_tick[bid] = step;
                }
            }
            const recipe* rc = reg.get_recipe(b.recipe);
            const float eff = row ? row->effective_workforce : b.workforce_assigned;
            const float bf = base_rate * eff * std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f) * building_supply_scalar(b);
            if (rc) for (std::size_t g = 0; g < resource_count; ++g) pr.need[g] = rc->inputs[g] * bf;
            if (b.decommissioned && !decom_tick.count(bid) && !decom_before.count(bid))
            {
                decom_tick[bid] = step;
                decom_by[bid] = b.loss_streak >= 8 ? "reflex" : b.loss_streak == 0 ? "verb" : "unknown";
                decom_wt[bid] = b.workforce_target;
            }
            if (!b.decommissioned) { decom_before.erase(bid); decom_tick.erase(bid); }
        }
        if (step == NT - 1) last_settle = std::move(res.report);
    }

    // reach, read at the end of the settle (before the seat), both forms
    input_reach ir_gen = make_input_reach(w, reg);
    input_reach ir_play = make_input_reach(w, reg);
    ir_play.report = &last_settle;
    std::map<entity_id, entity_id> owner_settle = owner_map(w);
    std::vector<std::pair<entity_id, entity_id>> producers_all; // (bid, market) snapshot
    std::map<entity_id, std::vector<std::size_t>> makes;        // bid -> goods it MAKES
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility && b.type != building_type::extraction_site) continue;
        std::vector<std::size_t> gs;
        for (std::size_t g = 0; g < resource_count; ++g) if (building_makes(w, reg, bid, g)) gs.push_back(g);
        if (!gs.empty()) { makes[bid] = gs; producers_all.push_back({bid, market_for_tile(w, b.tile)}); }
    }
    std::map<entity_id, entity_id> bld_body;
    for (const auto& [bid, b] : w.buildings) { const entity_id bm = market_for_tile(w, b.tile); bld_body[bid] = bm != null_entity ? mkt_body[bm] : null_entity; }

    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    if (seat.seated == null_entity) { out.fail = "no corporation seated"; return; }

    // ------------------------------------------------------------------ the G1b set
    std::vector<entity_id> set;
    int built = 0;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility) continue;
        const proc_state s = classify(b, row_of(last_settle, bid), reg);
        if (s == ps_build) continue;
        ++built;
        if (s == ps_input) { set.push_back(bid); C.add("g1b.now"); }
        else if (s == ps_decom)
        {
            const auto it = last_row.find(bid);
            if (it != last_row.end() && it->second.first == ps_input) { set.push_back(bid); C.add("g1b.decom"); }
        }
    }
    C.add("g1.built", built);
    std::sort(set.begin(), set.end());

    const float t_idle = reg.t_idle();
    const float res_mult = reg.price_band().reservation_mult;
    const std::map<entity_id, entity_id> owner_end = owner_map(w);
    std::map<std::pair<entity_id, std::size_t>, std::vector<entity_id>> traj_plants;
    const auto body_sum = [&](int k, entity_id body, std::size_t g, arr mrec::*f) {
        float s = 0.0f;
        for (const auto& [mid, r] : MT[k]) if (mkt_body[mid] == body) s += (r.*f)[g];
        return s;
    };

    for (const entity_id bid : set)
    {
        const building_component& b = w.buildings.at(bid);
        const bool decom = b.decommissioned;
        const int Ts = last_row_tick.count(bid) ? last_row_tick[bid] : NT - 1;
        const prec& ps = PT[Ts][bid];
        const int gi = decom ? last_row[bid].second : ps.lim;
        const recipe* rc = reg.get_recipe(b.recipe);
        const std::string rn = rc ? rc->name : std::string("?");
        const std::string who_tag = decom ? "decom" : "now";
        if (gi < 0) { C.add("cls.g.nolim"); continue; }
        const std::size_t g = static_cast<std::size_t>(gi);
        const std::string G = gname(g);
        const entity_id mid = market_for_tile(w, b.tile);
        const entity_id body = bld_body[bid];
        const auto oi = owner_end.find(bid);
        const auto ci = oi != owner_end.end() ? w.corporations.find(oi->second) : w.corporations.end();
        const char* oclass = ci == w.corporations.end() ? "noowner" : ci->second.is_player ? "player" : ci->second.is_background ? "background" : "rival";

        C.add("recipe." + rn);
        C.add("input." + G);
        C.add("recipe_input." + rn + "." + G);
        C.add("owner." + std::string(oclass));
        // origin and history
        const bool placed = built_tick.count(bid) > 0 || !at_build.count(bid);
        const bool switched = switch_tick.count(bid) > 0 || (at_build.count(bid) && recipe_at_build[bid] != b.recipe);
        int ran_ticks = 0, ran_on_current = 0, first_input = -1;
        for (int k = 0; k <= Ts; ++k)
        {
            const auto it = PT[k].find(bid);
            if (it == PT[k].end() || !it->second.seen) continue;
            if (it->second.state == ps_run) { ++ran_ticks; if (it->second.recipe == b.recipe) ++ran_on_current; }
            if (it->second.state == ps_input && first_input < 0) first_input = k;
        }
        // starved since: the start of the final input streak
        int streak_from = Ts;
        for (int k = Ts; k >= 0; --k)
        {
            const auto it = PT[k].find(bid);
            if (it == PT[k].end() || it->second.state != ps_input) break;
            streak_from = k;
        }
        C.add(std::string("origin.") + (placed ? "settle_built" : "generation") + (switched ? ".switched" : ""));
        C.add(std::string("ran_in_settle.") + (ran_ticks == 0 ? "never" : ran_on_current == 0 ? "only_prior_recipe" : "yes"));
        C.add("streak_from.t" + std::to_string(streak_from - NT));

        if (decom)
        {
            const int dt = decom_tick.count(bid) ? decom_tick[bid] : -99;
            C.add(std::string("decom.by.") + (decom_by.count(bid) ? decom_by[bid] : "atbuild"));
            C.add("decom.tick.t" + std::to_string(dt - NT));
            C.add(std::string("decom.wt.") + (decom_wt[bid] <= 0.0f ? "0" : "pos"));
        }

        if (reg.grid_goods().grid(g))
        {
            C.add("cls.g.grid");
            C.add("cls_input.g.grid." + G);
            continue;
        }
        const float need = ps.need[g];
        const float own = std::max(0.0f, ps.pool[g]);
        const float gap = std::max(0.0f, t_idle * need - own);
        const mrec& mT = MT[Ts][mid];
        const float ceil = res_mult * mT.base[g];

        // producers of g: state at T* x reach
        int pr_m[4] = {}, pr_reach[4] = {}, pr_unreach[4] = {}, pr_other_body = 0;
        int pr_reach_gen = 0, pr_reach_play = 0;
        std::set<entity_id> reach_mkts;
        for (const auto& [pid, pm] : producers_all)
        {
            const auto& gs = makes[pid];
            if (std::find(gs.begin(), gs.end(), g) == gs.end()) continue;
            if (bld_body[pid] != body) { ++pr_other_body; continue; }
            const auto si = PS[Ts].find(pid);
            const int st = si == PS[Ts].end() ? 3 : si->second; // absent at T* = placed later: counts as build
            if (pm == mid) { ++pr_m[st]; ++pr_reach[st]; reach_mkts.insert(pm); continue; }
            const bool rg = pm != null_entity && mid != null_entity && market_within_reach(w, reg, ir_gen, pm, mid, g);
            const bool rp = pm != null_entity && mid != null_entity && market_within_reach(w, reg, ir_play, pm, mid, g);
            if (rg) ++pr_reach_gen;
            if (rp) ++pr_reach_play;
            if (rg || rp) { ++pr_reach[st]; reach_mkts.insert(pm); }
            else ++pr_unreach[st];
        }
        int n_reach = 0, n_unreach = 0;
        for (int s = 0; s < 4; ++s) { n_reach += pr_reach[s]; n_unreach += pr_unreach[s]; }

        // the supply ladder
        std::string cls;
        std::string who;
        if (mT.pre[g] >= gap && gap > 0.0f && !mT.admits[g]) cls = "d.ceiling";
        else if (mT.pre[g] >= gap && gap > 0.0f)
        {
            int procs = 0, sites = 0;
            for (const shelf_ration_row& rr : RAT[Ts])
                if (rr.market == mid && rr.r == g && rr.building != bid) (rr.phase == 'c' ? sites : procs) += 1;
            cls = std::string("c.rationed.") + (procs && sites ? "procs+sites" : procs ? "procs" : sites ? "sites" : "unattributed");
        }
        if (cls.empty() && Ts > 0)
        {
            const mrec& mp = MT[Ts - 1][mid];
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
                cls = std::string("c.taken.") + best->second;
            }
        }
        if (cls.empty() && mT.pools[g] - own >= gap && gap > 0.0f)
        {
            const float other = mT.pools[g] - own;
            const std::string why = mT.held[g] >= gap ? "opening_held"
                                  : mT.ordered[g] >= gap ? "sell_order"
                                  : mT.reserved_idle[g] >= gap ? "reserved_for_idle_plants"
                                  : mT.reserved[g] >= gap ? "reserved_for_live_plants"
                                  : other - mT.reserved[g] >= gap ? "unlisted_other" : "reserved_mixed";
            cls = "g.pool_held." + why;
        }
        const float prodM = Ts > 0 ? MT[Ts - 1][mid].prod[g] + MT[Ts - 1][mid].arrived[g] + mT.arrived[g] : mT.arrived[g];
        std::string supply;
        if (cls.empty())
        {
            if (prodM > 0.0f) supply = "g.underproduced";
            else if (pr_reach[0] > 0) // a producer within reach RAN at T*, nothing arrived
            {
                float src_sur = 0, src_pool = 0, src_held = 0, src_prod = 0, src_hh = 0, src_dem = 0;
                for (const entity_id sm : reach_mkts)
                {
                    if (sm == mid) continue;
                    const mrec& q = MT[Ts][sm];
                    src_sur += q.sur[g]; src_pool += q.pools[g]; src_held += q.held[g];
                    if (Ts > 0) { src_prod += MT[Ts - 1][sm].prod[g]; src_hh += MT[Ts - 1][sm].hh[g] + MT[Ts - 1][sm].bg[g]; }
                    src_dem += q.dem[g];
                }
                const int xc = static_cast<int>(mT.xbest[g]);
                if (src_sur > 0.0f)
                    supply = std::string("e.surplus_unshipped.") + (xc == -2 ? "notread" : xc < 0 ? "nosource" : export_refusal::k_cls[xc]);
                else
                    supply = std::string("e.no_surplus_at_source.") + (src_held >= gap ? "opening_held" : src_pool >= gap ? "pooled" : "consumed_local");
                C.add("e_src.prod_x10", std::lround(10 * src_prod));
                C.add("e_src.hhbg_x10", std::lround(10 * src_hh));
                C.add("e_src.demand_x10", std::lround(10 * src_dem));
                C.add("e_src.n");
            }
            else if (n_reach > 0)
            {
                // which state: under construction beats idle beats decom
                supply = pr_reach[3] ? "b.build" : pr_reach[1] ? "b.idle" : "b.decom";
            }
            else if (n_unreach > 0) supply = std::string("a.unreachable") + (pr_unreach[0] ? ".running" : ".notrunning");
            else supply = "a.none_on_body";
            const bool never_ran_current = ran_on_current == 0;
            if ((placed || switched) && never_ran_current) cls = std::string("f.") + (placed ? "placed" : "switched");
            else cls = supply;
        }
        const std::string top = cls.substr(0, cls.find('.', 2));
        C.add("cls." + cls);
        C.add("cls_top." + top);
        C.add("cls_top_who." + top + "." + who_tag);
        C.add("cls_admit." + top + "." + (mT.admits[g] ? "under_ceiling" : "over_ceiling"));
        C.add(std::string("cls_hw.") + top + "." + (mT.hw[g] > 0.0f ? "silenced_want" : "no_silenced"));
        C.add("cls_input." + top + "." + G);
        C.add("cls_recipe." + top + "." + rn + "." + G);
        if (!supply.empty() && cls[0] == 'f') C.add("f_supply." + supply);
        if (decom) C.add("cls_decomby." + top + "." + (decom_by.count(bid) ? decom_by[bid] : "atbuild"));
        // the b sub-state: idle producers -- why (their own state at T*)
        if (cls.rfind("b.idle", 0) == 0 || supply.rfind("b.idle", 0) == 0)
            for (const auto& [pid, pm] : producers_all)
            {
                const auto& gs = makes[pid];
                if (std::find(gs.begin(), gs.end(), g) == gs.end() || bld_body[pid] != body || !reach_mkts.count(pm)) continue;
                const auto si = PS[Ts].find(pid);
                if (si == PS[Ts].end() || si->second != 1) continue;
                const building_component& pb = w.buildings.at(pid);
                std::string why = pb.type == building_type::extraction_site ? "ext" : "proc";
                if (pb.type == building_type::processing_facility)
                {
                    const auto pi = PT[Ts].find(pid);
                    if (pi != PT[Ts].end())
                    {
                        why += std::string(".") + k_state_name[pi->second.state];
                        if (pi->second.state == ps_input && pi->second.lim >= 0) why += "." + gname(static_cast<std::size_t>(pi->second.lim));
                    }
                }
                else why += pb.workforce_target <= 0.0f ? ".target0" : pb.workforce_assigned <= 0.0f ? ".unlaboured" : ".other";
                C.add("b_idle_why." + G + "." + why);
            }

        // the P line
        int tw_filled = 0, tw_unfilled = 0, tw_sil = 0;
        for (int k = 0; k <= Ts; ++k)
        {
            const auto it = PT[k].find(bid);
            if (it == PT[k].end() || !it->second.seen || it->second.state == ps_build || it->second.state == ps_decom) continue;
            const prec& q = it->second;
            if (!(q.need[g] > q.pool[g])) continue;
            if (!MT[k][mid].admits[g]) ++tw_sil;
            else if (q.state == ps_run && !(q.lim == static_cast<int>(g) && q.run < 1.0f)) ++tw_filled;
            else ++tw_unfilled;
        }
        C.add("want.silenced_ticks", tw_sil);
        C.add("want.filled_ticks", tw_filled);
        C.add("want.unfilled_ticks", tw_unfilled);
        float prod_body_T = Ts > 0 ? body_sum(Ts - 1, body, g, &mrec::prod) : 0.0f;
        std::printf("P %u %llu %s %s %s cls=%s supply=%s owner=%s origin=%s%s ran=%d/%d streak_from=%d Ts=%d "
                    "need=%.2f own=%.2f gap=%.2f shelf=%.2f admit=%d posted=%.2f ceil=%.2f base=%.2f prodM=%.2f prodBody=%.2f hh=%.2f bg=%.2f hw=%.2f heldM=%.2f poolsM=%.2f resM=%.2f resIdleM=%.2f ordM=%.2f "
                    "prodM_ext[run/idle/dec/bld]=%d/%d/%d/%d reach[r/i/d/b]=%d/%d/%d/%d unreach[r/i/d/b]=%d/%d/%d/%d otherbody=%d reachgen=%d reachplay=%d "
                    "decom_by=%s decom_t=%d want[f/u/s]=%d/%d/%d idle_src=%s sw=%s\n",
                    seed, static_cast<unsigned long long>(bid), who_tag.c_str(), rn.c_str(), G.c_str(), cls.c_str(),
                    supply.empty() ? "-" : supply.c_str(), oclass, placed ? "settle" : "gen", switched ? "+switched" : "",
                    ran_on_current, ran_ticks, streak_from - NT, Ts - NT, need, own, gap, mT.pre[g], mT.admits[g] ? 1 : 0,
                    mT.posted[g], ceil, mT.base[g], prodM, prod_body_T, Ts > 0 ? MT[Ts - 1][mid].hh[g] : 0.0f,
                    Ts > 0 ? MT[Ts - 1][mid].bg[g] : 0.0f, mT.hw[g], mT.held[g], mT.pools[g] - own, mT.reserved[g], mT.reserved_idle[g], mT.ordered[g],
                    pr_m[0], pr_m[1], pr_m[2], pr_m[3], pr_reach[0], pr_reach[1], pr_reach[2], pr_reach[3],
                    pr_unreach[0], pr_unreach[1], pr_unreach[2], pr_unreach[3], pr_other_body, pr_reach_gen, pr_reach_play,
                    decom ? (decom_by.count(bid) ? decom_by[bid] : "atbuild") : "-",
                    decom && decom_tick.count(bid) ? decom_tick[bid] - NT : 0, tw_filled, tw_unfilled, tw_sil,
                    idle_src.count(bid) ? idle_src[bid].c_str() : "-", switch_hist.count(bid) ? switch_hist[bid].c_str() : "-");
        traj_plants[{mid, g}].push_back(bid);
    }

    // the trajectory, pooled per good over (seed, M, g)
    for (const auto& [key, plants] : traj_plants)
    {
        const entity_id mid = key.first;
        const std::size_t g = key.second;
        std::vector<traj_row>& tr = g_traj[gname(g)];
        if (tr.size() < static_cast<std::size_t>(NT)) tr.resize(NT);
        const entity_id body = mkt_body[mid];
        for (int k = 0; k < NT; ++k)
        {
            const mrec& m = MT[k][mid];
            traj_row& r = tr[k];
            r.pairs += 1;
            r.prod_m += m.prod[g];
            r.prod_body += body_sum(k, body, g, &mrec::prod);
            r.shelf += m.pre[g];
            r.pr_ceil += m.base[g] > 0.0f && res_mult > 0.0f ? m.posted[g] / (res_mult * m.base[g]) : 0.0f;
            r.over_ceiling += m.admits[g] ? 0 : 1;
            r.hh += m.hh[g]; r.bg += m.bg[g]; r.hw += m.hw[g];
            r.pools_m += m.pools[g]; r.arrived += m.arrived[g];
            r.p_run_m += m.p_run[g]; r.p_idle_m += m.p_idle[g]; r.p_decom_m += m.p_decom[g]; r.p_build_m += m.p_build[g];
            r.p_run_b += body_sum(k, body, g, &mrec::p_run);
            r.p_idle_b += body_sum(k, body, g, &mrec::p_idle);
            r.p_decom_b += body_sum(k, body, g, &mrec::p_decom);
            r.p_build_b += body_sum(k, body, g, &mrec::p_build);
            for (const entity_id bid : plants)
            {
                const auto it = PT[k].find(bid);
                if (it == PT[k].end() || !it->second.seen) continue;
                const prec& pr = it->second;
                if (pr.state == ps_decom || pr.state == ps_build) continue;
                r.plants += 1;
                if (!(pr.need[g] > pr.pool[g])) continue;
                if (!m.admits[g]) r.silenced += 1;
                else if (pr.state == ps_run && !(pr.lim == static_cast<int>(g) && pr.run < 1.0f)) r.filled += 1;
                else r.unfilled += 1;
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
        else if (!std::strcmp(argv[i], "--cf") && i + 1 < argc) g_cf = argv[++i];
        else { std::fprintf(stderr, "usage: g1b_settle_probe [--seeds a,b] [--cf ceiling_off|bg_off|release_silenced]\n"); return 2; }
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::fprintf(stderr, "no seeds\n"); return 2; }
    std::printf("g1b_settle_probe: %zu seeds, cf %s\n", seeds.size(), g_cf.empty() ? "none" : g_cf.c_str());
    counts pooled;
    int fails = 0;
    for (const std::uint32_t s : seeds)
    {
        seed_out o;
        run_seed(s, o);
        if (!o.fail.empty()) { std::printf("seed %u FAIL %s\n", s, o.fail.c_str()); ++fails; continue; }
        std::printf("seed %u (%.0f s) built %ld g1b now %ld decom %ld\n", s, o.secs, o.c.c["g1.built"], o.c.c["g1b.now"], o.c.c["g1b.decom"]);
        for (const auto& [k, v] : o.c.c) std::printf("COUNTS %u %s %ld\n", s, k.c_str(), v);
        for (const auto& [k, v] : o.c.c) pooled.add(k, v);
        std::fflush(stdout);
    }
    std::printf("\nPOOLED over %zu seeds (%d failed)\n", seeds.size(), fails);
    for (const auto& [k, v] : pooled.c) std::printf("  %-80s %7ld\n", k.c_str(), v);
    const long g1b = pooled.c["g1b.now"] + pooled.c["g1b.decom"];
    std::printf("\nG1b %ld / %ld = %.1f%% (now %ld, decom %ld)\n", g1b, pooled.c["g1.built"],
                pooled.c["g1.built"] ? 100.0 * g1b / pooled.c["g1.built"] : 0.0, pooled.c["g1b.now"], pooled.c["g1b.decom"]);
    std::printf("\nTRAJECTORY over the settle (pooled sums over each limiting good's (seed, market) pairs; pr/ceil and ceil%% are means)\n");
    for (const auto& [G, tr] : g_traj)
    {
        std::printf("  %s\n   tick pairs  prodM prodBody   shelf pr/ceil ceil%%     hh     bg     hw  poolsM  arrv  wantOK wantNO silen plants  pRunM pIdlM pDecM pBldM  pRunB pIdlB pDecB pBldB\n", G.c_str());
        for (int k = 0; k < static_cast<int>(tr.size()); ++k)
        {
            const traj_row& r = tr[k];
            const double n = r.pairs > 0 ? r.pairs : 1;
            std::printf("   %4d %5.0f %6.1f %8.1f %7.1f %7.2f %5.0f %6.1f %6.1f %6.1f %7.1f %5.1f %6.0f %6.0f %5.0f %6.0f %6.0f %5.0f %5.0f %5.0f %6.0f %5.0f %5.0f %5.0f\n",
                        k - NT, r.pairs, r.prod_m, r.prod_body, r.shelf, r.pr_ceil / n, 100.0 * r.over_ceiling / n, r.hh, r.bg, r.hw,
                        r.pools_m, r.arrived, r.filled, r.unfilled, r.silenced, r.plants,
                        r.p_run_m, r.p_idle_m, r.p_decom_m, r.p_build_m, r.p_run_b, r.p_idle_b, r.p_decom_b, r.p_build_b);
        }
    }
    return fails ? 1 : 0;
}
