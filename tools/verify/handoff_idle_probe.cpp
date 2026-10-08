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
//
// ROUND 2 (the dial-zero census). Every processor whose workforce_target fell
// from > 0 to 0 is caught at the run_economy_step lap and read with the
// solver's OWN numbers (solve_workforce_target called read-only on a copy at
// its incumbent target, and an exact replica for the decomposition; the two
// are counted against each other in *.replica_agree):
//   zall.*   every settle zeroing event
//   z503.*   the plants unsupplied at the handoff, or decommissioned whose last
//            state was unsupplied (G1's dial-zeroed set). cls, by precedence:
//            unpriced_output | demand0 (the plant's market bids none of its
//            primary output) | inputs_dear (outputs at BASE price do not beat
//            inputs at posted) | glut (they do, but not at the solver's
//            forecast) | fixed_costs. demand0: buyers on the body / off it and
//            the best export_refusal pair class to them; demand > 0: the
//            handoff price, and whether the solver at the handoff (target 100)
//            would run it. held_*: standing sell orders whose floor is over
//            the price (re-listed every tick, BL-1201) at the plant's market.
//   --ids-out F writes the z503 set; --ids-in F counts that set's states at the
//   handoff (cf503.state.*) -- for a counterfactual build of the same seeds.
// The 2026-10-08 counterfactuals were measurement switches in src (env IO_CF),
// not committed.
//
// ROUND 3 (silenced want as the dial's buyer signal). Each zeroing also records
// `hauler_want` at the plant's market and demand + hauler_want summed over the
// body: z503.dem0.sees_buyer_{own,body}_dw, and zall.t-11.<good>.* (demand vs
// hauler_want at the zeroing markets on settle tick -11). With --ids-in, the
// set's states are also counted at t50 (t50.cf503.state.*).
//   input: limiting good, the L class (market_viability's processor block).
//   every idle state also carries margin.{pos,neg,unpriced}: the recipe's unit
//   margin (outputs - inputs per run, at its market's current prices) -- would
//   running pay before wages.
//   builder.{gen,settle,play}: present in the world as built / appeared during
//   the settle (the scored tier, spectating) / appeared in play.
//
// ROUND 4 (who bids steel / refined_fuel / silicon / clean_water at the settle
// start; food_rations is the control). Keys r4.<tick>.*, armed on settle ticks
// -12 (the first: econ step 0, before any clear), -11 (the dial's big zeroing
// tick: it reads tick -12's clear), -10, -9, -1, and play 1 and 50, all in the
// handoff block:
//   <point>.<good>.*   the market registers at three points: conv (pre-step),
//                      dial (after the economy step -- what the workforce dial
//                      read; no clear has run in between), clear (after
//                      clear_markets): demand / household_bid / background_bid
//                      / hauler_want / unposted_bid / shelf / supply > 0, and
//                      markets whose posted price is over the fair-price ceiling.
//   cons.<good>.<cls>  every processor whose recipe consumes the good, classed
//                      by run_processing's want rule (economy_system.cpp): unbuilt
//                      | decom | target0 | supply0 | nolab | pool_covers (its
//                      owner's pool holds the full-run need, so the want net of
//                      the pool is 0 and it bids nothing; cover_ticks buckets the
//                      pool / need) | ceiling (the want goes to hauler_want) |
//                      bids. cons_recipe on tick -11 only.
//   site.<good>.*      construction sites whose material row names the good.
//   report.<good>.*    the tick report's wants / hauler_wants / upkeep_wants;
//                      upkeep_bldgs, units_drawing, buy_orders, producers(_run).
//   r4.static.*        consumer recipes, band admission, built counts, basket
//                      weights (household, background, unit upkeep).
//   r4.z-11.<good>.<cls>.*  tick -11's zeroed producers of the good, joined to
//                      the register after that tick's clear (own market / body
//                      only / off-body only / none).
//   r4.switch.<settle|play>.<from>.to.<to>  every recipe_switch agency event.
//   z503.dem0.good_hh.<good>.mkt_{has,no}_households: is the zeroed plant's
//                      market one no population centre clears at (household
//                      and background bids land only where a centre does);
//                      z503.dem0.good_tick: the tick of that zeroing.
// And, in the t50 block, decay.*: every processor running at the handoff,
// classed at tick 50 (still_run | starved.<input> | decom.<by>.<why> | nolab |
// unsup.{dial_target0, supply0, target0_other} | other), with decay_top.* and
// decay_recipe.* rollups.
//
// Usage: build_gen/verify/handoff_idle_probe.exe [--seeds a,b] [--ticks N] [--examples N]
// Build:  ./tools/verify/build_lua_harness.sh handoff_idle_probe
// Each seed prints `COUNTS <seed> <key> <value>` lines: pool a split run by
// summing them.
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "export_refusal.hpp"
#include "world/budget_system.hpp"
#include "world/building_profit.hpp"
#include "world/logistics.hpp"
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
#include <limits>
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

// --- round 2: the dial-zero census (every zeroing event) ---------------------
// A processor whose workforce_target went from > 0 to 0 during the economy step
// is seen at the run_economy_step lap (the strategic tier has applied its
// commands; the clear has not run, so the market registers are the ones the
// solver read). For each event: the solver's OWN numbers, replicated exactly
// (economy_system.cpp solve_workforce_target: contention 1.0 as the scorer
// passes it, the 0..200 step-10 tier search, the change-from-current supply
// term, posted input prices, the BL-1232 grid pooling) and checked against a
// read-only call of solve_workforce_target on a copy of the building at its
// incumbent target.
struct zero_rec
{
    int   tick = 0;
    std::size_t g = resource_count; ///< primary output
    entity_id m = null_entity;
    std::string cls;      ///< unpriced_output | demand0 | glut | inputs_dear | fixed_costs
    float demand = 0, supply = 0, price_ratio = 0;
    bool  solver_zero = false;   ///< solve_workforce_target itself returns 0
    bool  replica_zero = false;  ///< the replica's tier search returns 0
    bool  margin_base_pos = false; ///< outputs at BASE price beat inputs at posted
    int   buyers_body = 0, buyers_off = 0; ///< other markets bidding the good (this body / others)
    int   best_cls = -1;         ///< best export_refusal pair class own market -> a buyer market
    float held = 0;              ///< supply listed by standing sell orders whose floor is over the price
    float hw_own = 0;            ///< round 3: hauler_want (silenced processor/construction want) at the plant's market
    float dw_body = 0;           ///< round 3: demand + hauler_want summed over the markets on the plant's body
    int   prev_target = 0;
};

float wf_target_price_rep(float base, float supply, float demand, float fl, float ce)
{
    if (base <= 0.0f) return 0.0f;
    float t;
    if (supply <= 0.0f && demand <= 0.0f) t = base;
    else if (supply <= 0.0f)              t = base * ce;
    else                                  t = base * std::sqrt(demand / supply);
    return std::clamp(t, base * fl, base * ce);
}

struct solver_view { float rev = 0, in = 0, opex = 0, rev_base = 0; int best = -1; };

/// The solver's net at the incumbent target T and its tier search.
solver_view solver_replica(world& w, const recipe_registry& reg, const building_component& b, int T)
{
    solver_view out;
    const recipe* rcp = reg.get_recipe(b.recipe);
    const auto mi = w.markets.find(market_for_tile(w, b.tile));
    if (!rcp || mi == w.markets.end()) return out;
    const market_component* mkt = &mi->second;
    const building_economics& e = reg.economics(b.type);
    const float hab = body_mean_habitability(w, mkt->body);
    const float eff = b.workforce_assigned * 1.0f;
    const float now = std::clamp(T / 100.0f, 0.0f, 2.0f);
    grid_good_pool gpool;
    const grid_good_figures* gsd = nullptr;
    if (reg.grid_goods().any())
    {
        bool touches = false;
        for (std::size_t r = 0; r < resource_count && !touches; ++r)
            touches = reg.grid_goods().grid(r) && grid_good_crosses_markets(r)
                   && (rcp->outputs[r] > 0.0f || rcp->inputs[r] > 0.0f);
        if (touches)
        {
            gpool = pool_grid_good_figures(w, reg);
            if (const auto mg = gpool.market_grid.find(mi->first); mg != gpool.market_grid.end())
                gsd = &gpool.grid_sd.at(mg->second);
        }
    }
    const price_band_params& pb = reg.price_band();
    const auto price_of = [&](std::size_t r, float d) {
        const bool pooled = gsd != nullptr && reg.grid_goods().grid(r) && grid_good_crosses_markets(r);
        const float base = pooled ? grid_good_pricing_supply(*gsd, r, pb.shelf_supply_ticks) : mkt->supply[r];
        return wf_target_price_rep(mkt->base_price[r], std::max(0.0f, base + d), pooled ? gsd->demand[r] : mkt->demand[r],
                                   pb.floor_mult, pb.ceil_mult);
    };
    const auto net_at = [&](int wt, solver_view* sv) {
        const float wts = wt / 100.0f;
        building_component pr = b;
        pr.workforce_target = static_cast<float>(wt);
        const building_opex opex = compute_building_opex(pr, e, 1.0f, hab, reg.idle_maintenance_floor());
        const float runs = e.base_rate * eff * wts, runs_now = e.base_rate * eff * now;
        float rev = 0, in = 0, rb = 0;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (rcp->outputs[r] > 0.0f)
            {
                rev += rcp->outputs[r] * runs * price_of(r, rcp->outputs[r] * (runs - runs_now));
                rb  += rcp->outputs[r] * runs * mkt->base_price[r];
            }
            if (rcp->inputs[r] > 0.0f) in += rcp->inputs[r] * runs * posted_price(*mkt, r);
        }
        if (sv) { sv->rev = rev; sv->in = in; sv->opex = opex.maintenance + opex.wages; sv->rev_base = rb; }
        return rev - in - opex.maintenance - opex.wages;
    };
    net_at(T, &out);
    float best_net = -std::numeric_limits<float>::infinity();
    for (int wt = 0; wt <= 200; wt += 10)
    {
        const float n = net_at(wt, nullptr);
        if (n > best_net) { best_net = n; out.best = wt; }
    }
    return out;
}

/// Standing sell orders on @p mid's body for good @p g whose floor is over the
/// market's price: what each lists from its (corp, mid) pool every tick and holds
/// (market_clearing.cpp, the order listing pass, BL-1201). Caps ignored.
float held_listing(const world& w, const recipe_registry& reg, entity_id mid, std::size_t g)
{
    const auto mi = w.markets.find(mid);
    if (mi == w.markets.end()) return 0.0f;
    float held = 0.0f;
    std::set<entity_id> corps;
    for (const sell_order& o : w.sell_orders)
        if (o.body == mi->second.body && static_cast<std::size_t>(o.resource) == g
            && o.floor_price > mi->second.price[g])
            corps.insert(o.corp);
    for (const entity_id c : corps)
    {
        const auto pk = w.corp_market_pools.find(std::make_pair(c, mid));
        if (pk == w.corp_market_pools.end()) continue;
        const float s = pk->second.quantities[g] - processor_reservation(w, reg, c, mid)[g];
        if (s > 0.0f) held += s;
    }
    return held;
}

zero_rec make_zero_rec(world& w, const recipe_registry& reg, entity_id bid, const building_component& b,
                       int prev_target, int tick, std::unique_ptr<export_refusal::context>& xc,
                       std::map<std::pair<entity_id, std::size_t>, std::array<int, 3>>& memo)
{
    zero_rec z;
    z.tick = tick;
    z.prev_target = prev_target;
    const recipe* rc = reg.get_recipe(b.recipe);
    const entity_id mid = market_for_tile(w, b.tile);
    const auto mi = w.markets.find(mid);
    if (!rc || mi == w.markets.end()) { z.cls = "nomarket"; return z; }
    const market_component& m = mi->second;
    z.m = mid;
    float bv = -1.0f;
    for (std::size_t g = 0; g < resource_count; ++g)
        if (rc->outputs[g] > 0.0f && rc->outputs[g] * m.base_price[g] > bv) { bv = rc->outputs[g] * m.base_price[g]; z.g = g; }
    if (z.g == resource_count) { z.cls = "nooutput"; return z; }
    const std::size_t g = z.g;
    z.demand = m.demand[g];
    z.supply = m.supply[g];
    z.price_ratio = m.base_price[g] > 0.0f ? m.price[g] / m.base_price[g] : 0.0f;
    building_component copy = b;
    copy.workforce_target = static_cast<float>(prev_target);
    z.solver_zero = solve_workforce_target(w, reg, copy, 1.0f, 1, nullptr) == 0;
    const solver_view sv = solver_replica(w, reg, copy, prev_target);
    z.replica_zero = sv.best == 0;
    z.margin_base_pos = sv.rev_base > sv.in;
    z.held = held_listing(w, reg, mid, g);
    z.hw_own = m.hauler_want[g];
    for (const auto& [om, omc] : w.markets)
        if (omc.body == m.body) z.dw_body += omc.demand[g] + omc.hauler_want[g];
    if (!(m.base_price[g] > 0.0f)) z.cls = "unpriced_output";
    else if (!(m.demand[g] > 0.0f)) z.cls = "demand0";
    else if (!(sv.rev_base > sv.in)) z.cls = "inputs_dear";
    else if (!(sv.rev > sv.in)) z.cls = "glut";
    else z.cls = "fixed_costs";
    if (z.cls == "unpriced_output") return z;
    // buyers elsewhere, and the best dispatcher class from the plant's market
    if (const auto mm = memo.find({mid, g}); mm != memo.end())
    {
        z.buyers_body = mm->second[0]; z.buyers_off = mm->second[1]; z.best_cls = mm->second[2];
        return z;
    }
    std::vector<entity_id> mids;
    for (const auto& [om, omc] : w.markets)
        if (om != mid && omc.demand[g] > 0.0f && omc.base_price[g] > 0.0f) mids.push_back(om);
    std::sort(mids.begin(), mids.end());
    for (const entity_id d : mids)
    {
        if (w.markets.at(d).body != m.body) { ++z.buyers_off; continue; }
        ++z.buyers_body;
        if (!xc) xc = std::make_unique<export_refusal::context>(w, reg);
        const int c = export_refusal::classify_pair(*xc, mid, d, g).c;
        if (c > z.best_cls) z.best_cls = c;
    }
    memo[{mid, g}] = {z.buyers_body, z.buyers_off, z.best_cls};
    return z;
}

struct probe
{
    int cur_tick = 0;
    std::map<entity_id, int>* prev_target = nullptr;
    std::map<entity_id, zero_rec>* zeros = nullptr;  ///< the last zeroing per processor
    std::vector<zero_rec>* events = nullptr;          ///< every zeroing
    const recipe_registry* reg = nullptr;
    int lap_pre = -1, lap_econ = -1, lap_clear = -1;
    void* r4 = nullptr;                                   ///< round 4 state (r4_state)
    void (*r4_hook)(const world&, int lap, void* r4) = nullptr;
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
    if (p.r4_hook) p.r4_hook(w, lap, p.r4);
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
    if (p.prev_target && p.zeros)
    {
        world& mw = const_cast<world&>(w); // router path caches only (export_refusal.hpp); the solver reads
        std::unique_ptr<export_refusal::context> xc;
        std::map<std::pair<entity_id, std::size_t>, std::array<int, 3>> memo;
        for (const entity_id bid : sorted_processors(w))
        {
            const building_component& b = w.buildings.at(bid);
            if (!live_processor(b) || b.workforce_target > 0.0f) continue;
            const auto pt = p.prev_target->find(bid);
            if (pt == p.prev_target->end() || pt->second <= 0) continue;
            zero_rec z = make_zero_rec(mw, *p.reg, bid, b, pt->second, p.cur_tick, xc, memo);
            (*p.zeros)[bid] = z;
            p.events->push_back(z);
        }
    }
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


// --- round 2: the census of the dial-zeroed plants at the handoff ------------
std::string g_ids_out, g_ids_in;               ///< --ids-out / --ids-in files
std::map<std::uint32_t, std::set<entity_id>> g_ids_base; ///< --ids-in: seed -> the baseline's dial-zeroed set
std::FILE* g_ids_file = nullptr;

const char* pair_cls_name(int c) { return c < 0 ? "none" : export_refusal::k_cls[c]; }

void census(world& w, const recipe_registry& reg, const economy_report& rep, const std::map<entity_id, hist>& H,
            const std::map<entity_id, zero_rec>& zeros, const std::vector<zero_rec>& events, counts& c,
            std::uint32_t seed)
{
    const float fl = reg.price_band().floor_mult;
    for (const zero_rec& z : events)
    {
        if (z.tick >= 0) continue; // settle events only
        c.add("zall.n");
        if (z.tick == -11 && z.g < resource_count)
        {
            // round 3 hypothesis: on tick -11, is the zeroing market's want silenced?
            const std::string k = "zall.t-11." + gname(z.g) + ".";
            c.add(k + "n");
            c.add(k + "demand_pos", z.demand > 0.0f ? 1 : 0);
            c.add(k + "hw_pos", z.hw_own > 0.0f ? 1 : 0);
            c.add(k + "demand_sum_x10", static_cast<long>(10.0f * z.demand));
            c.add(k + "hw_sum_x10", static_cast<long>(10.0f * z.hw_own));
            c.add(k + "body_dw_pos", z.dw_body > 0.0f ? 1 : 0);
        }
        c.add("zall.cls." + z.cls);
        c.add(std::string("zall.solver_zero.") + (z.solver_zero ? "1" : "0"));
        c.add(std::string("zall.replica_agree.") + (z.solver_zero == z.replica_zero ? "1" : "0"));
    }
    for (const entity_id bid : sorted_processors(w))
    {
        const building_component& b = w.buildings.at(bid);
        const proc_state s = classify(b, row_of(rep, bid), reg);
        if (g_ids_base.count(seed) && g_ids_base[seed].count(bid))
            c.add(std::string("cf503.state.") + k_state_name[s]);
        const auto hi = H.find(bid);
        const bool dz = s == ps_unsup
            || (s == ps_decom && hi != H.end() && hi->second.decom && hi->second.d.last == ps_unsup);
        if (!dz) continue;
        if (g_ids_file) std::fprintf(g_ids_file, "%u %llu\n", seed, static_cast<unsigned long long>(bid));
        c.add("z503.n");
        c.add(std::string("z503.state.") + k_state_name[s]);
        const auto zi = zeros.find(bid);
        if (zi == zeros.end()) { c.add("z503.norec"); continue; }
        const zero_rec& z = zi->second;
        c.add("z503.cls." + z.cls);
        c.add(std::string("z503.solver_zero.") + (z.solver_zero ? "1" : "0"));
        c.add(std::string("z503.replica_agree.") + (z.solver_zero == z.replica_zero ? "1" : "0"));
        c.add("z503.tick." + std::to_string(z.tick));
        const recipe* rc = reg.get_recipe(b.recipe);
        const std::string rn = rc ? rc->name : std::string("?");
        c.add("z503.cls_recipe." + z.cls + "." + rn);
        if (z.g >= resource_count || z.cls == "unpriced_output" || z.cls == "nomarket") continue;
        c.add("z503.held_at_zero." + std::string(z.held > 0.0f ? (z.held >= z.demand ? "ge_demand" : "lt_demand") : "none"));
        const float held_now = held_listing(w, reg, z.m, z.g);
        if (z.held > 0.0f) c.add(std::string("z503.held_persists.") + (held_now > 0.0f ? "yes" : "no"));
        if (z.demand > 0.0f)
        {
            c.add(std::string("z503.dempos.margin_base_pos.") + (z.margin_base_pos ? "1" : "0"));
            const market_component& m = w.markets.at(z.m);
            const float r = m.base_price[z.g] > 0.0f ? m.price[z.g] / m.base_price[z.g] : 0.0f;
            c.add(std::string("z503.dempos.handoff_price.") + (r <= fl * 1.02f ? "floor" : r < 0.9f ? "below0.9" : "ge0.9"));
            c.add(std::string("z503.dempos.handoff_demand.") + (m.demand[z.g] > 0.0f ? "pos" : "zero"));
            building_component copy = b;
            copy.workforce_target = 100.0f;
            copy.decommissioned = false;
            const int now = solve_workforce_target(w, reg, copy, 1.0f, 1, nullptr);
            c.add(std::string("z503.dempos.solver_at_handoff.") + (now > 0 ? "runs" : "zero"));
            c.add("z503.dempos.cls_handoff." + z.cls + "." + (now > 0 ? "runs" : "zero"));
        }
        else
        {
            c.add(std::string("z503.dem0.margin_base_pos.") + (z.margin_base_pos ? "1" : "0"));
            c.add(std::string("z503.dem0.sees_buyer_own_dw.") + (z.hw_own > 0.0f ? "yes" : "no"));
            c.add(std::string("z503.dem0.sees_buyer_body_dw.") + (z.dw_body > 0.0f ? "yes" : "no"));
            c.add(std::string("z503.dem0.buyers_body.") + (z.buyers_body > 0 ? "yes" : "none")
                  + (z.buyers_off > 0 ? "+offbody" : ""));
            c.add(std::string("z503.dem0.best_pair.") + pair_cls_name(z.best_cls));
            c.add("z503.dem0.recipe." + rn + "." + pair_cls_name(z.best_cls));
            const market_component& m = w.markets.at(z.m);
            c.add(std::string("z503.dem0.handoff_demand.") + (m.demand[z.g] > 0.0f ? "pos" : "zero"));
            // round 4: is the plant on a market no population centre clears at
            // (household / background bids land only where a centre does)?
            bool hh = false;
            for (std::size_t r = 0; r < resource_count && !hh; ++r) hh = m.household_bid[r] > 0.0f;
            c.add("z503.dem0.good_hh." + gname(z.g) + (hh ? ".mkt_has_households" : ".mkt_no_households"));
            c.add("z503.dem0.good_tick." + gname(z.g) + "." + std::to_string(z.tick));
        }
    }
}

// --- round 4: who bids steel / refined_fuel / silicon / clean_water ----------
// Every channel that can put a good into `demand` (MARKETS.md § Demand
// channels), read for the four goods the dial found no bidder for, plus
// food_rations as the control. Pure reads; keys "r4.<tick>.<...>".
struct r4_snap
{
    entity_id corp = null_entity, m = null_entity;
    int   target = 0;
    bool  build = false, decom = false;
    float supply_scalar = 1.0f;
    std::vector<float> pool;   ///< owner's pool at its market, per r4 good
    std::vector<char>  admits; ///< the fair-price ceiling admits the shelf, per r4 good
};

struct r4_state
{
    const recipe_registry* reg = nullptr;
    std::vector<std::size_t> goods;
    int  lap_pre = -1, lap_econ = -1, lap_clear = -1;
    bool armed = false;
    std::string label;
    counts* c = nullptr;
    std::map<entity_id, r4_snap> snap;                     ///< pre-step (convoys lap)
    std::map<entity_id, std::vector<float>> post_dem;      ///< tick -11, after the clear
    std::map<entity_id, std::vector<float>> post_hw;       ///< tick -11, hauler_want after the clear
};

void r4_registers(const world& w, const r4_state& s, const std::string& point, counts& c)
{
    for (const std::size_t g : s.goods)
    {
        const std::string k = "r4." + s.label + "." + point + "." + gname(g) + ".";
        c.add(k + "mkts", 0);
        for (const auto& [mid, mc] : w.markets)
        {
            (void)mid;
            if (!(mc.base_price[g] > 0.0f)) { c.add(k + "unpriced_mkt"); continue; }
            c.add(k + "mkts");
            if (mc.demand[g] > 0.0f)         c.add(k + "demand_pos");
            if (mc.household_bid[g] > 0.0f)  c.add(k + "household_pos");
            if (mc.background_bid[g] > 0.0f) c.add(k + "background_pos");
            if (mc.hauler_want[g] > 0.0f)    c.add(k + "hauler_want_pos");
            if (mc.unposted_bid[g] > 0.0f && mc.unposted_bid_tick[g] == w.current_econ_tick) c.add(k + "unposted_pos");
            if (mc.inventory[g] >= 1.0f)     c.add(k + "shelf_pos");
            if (mc.supply[g] > 0.0f)         c.add(k + "supply_pos");
            if (mc.base_price[g] > 0.0f && posted_price(mc, g) > mc.base_price[g] * s.reg->price_band().reservation_mult)
                c.add(k + "posted_over_ceiling");
            c.add(k + "demand_x10", std::lround(10.0f * std::max(0.0f, mc.demand[g])));
        }
    }
}

bool r4_consumes(const recipe_registry& reg, const building_component& b, const std::vector<std::size_t>& goods)
{
    if (b.ticks_remaining > 0)
    {
        const auto& cost = reg.resource_build_cost_for(b.type, b.target_resource, b.recipe);
        for (const std::size_t g : goods) if (cost[g] > 0.0f) return true;
    }
    if (b.type != building_type::processing_facility) return false;
    const recipe* rc = reg.get_recipe(b.recipe);
    if (!rc) return false;
    for (const std::size_t g : goods) if (rc->inputs[g] > 0.0f) return true;
    return false;
}

void r4_snapshot(const world& w, r4_state& s)
{
    const recipe_registry& reg = *s.reg;
    const std::map<entity_id, entity_id> owner = owner_map(w);
    const float res_mult = reg.price_band().reservation_mult;
    s.snap.clear();
    for (const auto& [bid, b] : w.buildings)
    {
        if (!r4_consumes(reg, b, s.goods)) continue;
        r4_snap r;
        const auto oi = owner.find(bid);
        r.corp = oi != owner.end() ? oi->second : null_entity;
        r.m = market_for_tile(w, b.tile);
        r.target = static_cast<int>(b.workforce_target);
        r.build = b.ticks_remaining > 0;
        r.decom = b.decommissioned;
        r.supply_scalar = building_supply_scalar(b);
        const auto mi = w.markets.find(r.m);
        const stockpile_component* pool = r.corp != null_entity ? w.find_pool(r.corp, r.m) : nullptr;
        for (const std::size_t g : s.goods)
        {
            r.pool.push_back(pool ? pool->quantities[g] : 0.0f);
            r.admits.push_back(mi != w.markets.end() && shelf_admits(mi->second, g, res_mult, /*off_buys=*/true) ? 1 : 0);
        }
        s.snap.emplace(bid, std::move(r));
    }
}

void r4_hook(const world& w, int lap, void* ctx)
{
    r4_state& s = *static_cast<r4_state*>(ctx);
    if (!s.armed) return;
    if (lap == s.lap_pre) { r4_snapshot(w, s); r4_registers(w, s, "conv", *s.c); return; }
    if (lap == s.lap_econ) { r4_registers(w, s, "dial", *s.c); return; }
    if (lap == s.lap_clear)
    {
        r4_registers(w, s, "clear", *s.c);
        if (s.label == "t-11")
            for (const auto& [mid, mc] : w.markets)
            {
                std::vector<float>& d = s.post_dem[mid];
                std::vector<float>& h = s.post_hw[mid];
                for (const std::size_t g : s.goods) { d.push_back(mc.demand[g]); h.push_back(mc.hauler_want[g]); }
            }
    }
}

/// After an armed tick: every consumer of an r4 good, classed by why it did or
/// did not bid this tick (economy_system.cpp run_processing's want rule, read
/// from the pre-step snapshot and the tick's report), and every other channel.
void r4_after_tick(const world& w, const economy_report& rep, r4_state& s, bool detail)
{
    const recipe_registry& reg = *s.reg;
    counts& c = *s.c;
    const std::string L = "r4." + s.label + ".";
    const float base_rate = reg.economics(building_type::processing_facility).base_rate;
    for (const auto& [bid, r] : s.snap)
    {
        const auto bi = w.buildings.find(bid);
        if (bi == w.buildings.end()) continue;
        const building_component& b = bi->second;
        const recipe* rc = reg.get_recipe(b.recipe);
        for (std::size_t i = 0; i < s.goods.size(); ++i)
        {
            const std::size_t g = s.goods[i];
            const std::string G = gname(g);
            if (r.build)
            {
                const auto& cost = reg.resource_build_cost_for(b.type, b.target_resource, b.recipe);
                if (cost[g] > 0.0f)
                    c.add(L + "site." + G + "." + (r.corp == null_entity ? "noowner" : r.admits[i] ? "bids" : "ceiling"));
            }
            if (b.type != building_type::processing_facility || !rc || !(rc->inputs[g] > 0.0f)) continue;
            std::string cls;
            float need = 0.0f;
            const building_report* row = row_of(rep, bid);
            if (r.build) cls = "unbuilt";
            else if (r.decom) cls = "decom";
            else if (r.target <= 0) cls = "target0";
            else if (!(r.supply_scalar > 0.0f)) cls = "supply0";
            else if (!row || !(row->effective_workforce > 0.0f)) cls = "nolab";
            else
            {
                need = rc->inputs[g] * base_rate * row->effective_workforce
                     * std::clamp(r.target / 100.0f, 0.0f, 2.0f) * r.supply_scalar;
                if (r.pool[i] >= need) cls = "pool_covers";
                else if (!r.admits[i]) cls = "ceiling";
                else cls = "bids";
            }
            c.add(L + "cons." + G + "." + cls);
            if (cls == "bids") c.add(L + "cons." + G + ".bid_x10", std::lround(10.0f * (need - r.pool[i])));
            if (cls == "pool_covers")
            {
                c.add(L + "cons." + G + ".pool_covers_x10", std::lround(10.0f * need));
                const float cover = need > 0.0f ? r.pool[i] / need : 0.0f; // full-run ticks the pool holds
                c.add(L + "cons." + G + ".cover_ticks." + (cover < 2.0f ? "lt2" : cover < 6.0f ? "2-6" : cover < 13.0f ? "6-13" : "ge13"));
            }
            if (detail) c.add(L + "cons_recipe." + G + "." + rc->name + "." + cls);
        }
    }
    // The report's want registers (processors + sites + upkeep) and the rest.
    for (const std::size_t g : s.goods)
    {
        const std::string G = gname(g);
        float wants = 0, hw = 0, upk = 0;
        for (const auto& [k, v] : rep.wants) wants += v[g];
        for (const auto& [k, v] : rep.hauler_wants) hw += v[g];
        for (const auto& [k, v] : rep.upkeep_wants) upk += v[g];
        c.add(L + "report." + G + ".wants_x10", std::lround(10.0f * wants));
        c.add(L + "report." + G + ".hauler_wants_x10", std::lround(10.0f * hw));
        c.add(L + "report." + G + ".upkeep_wants_x10", std::lround(10.0f * upk));
        for (const buy_order& o : w.buy_orders)
            if (static_cast<std::size_t>(o.resource) == g) c.add(L + "buy_orders." + G);
        for (const auto& [bid, b] : w.buildings)
        {
            if (b.ticks_remaining > 0 || b.decommissioned) continue;
            if (building_upkeep_goods(reg.building_upkeep(), b.type, w.campaign_band)[g] > 0.0f)
                c.add(L + "upkeep_bldgs." + G);
            if (b.type != building_type::processing_facility) continue;
            const recipe* rc = reg.get_recipe(b.recipe);
            if (rc && rc->outputs[g] > 0.0f)
            {
                c.add(L + "producers." + G);
                const building_report* row = row_of(rep, bid);
                if (row && row->active) c.add(L + "producers_run." + G);
            }
        }
        if (reg.military().upkeep.goods_per_head[g] > 0.0f) c.add(L + "units_drawing." + G, static_cast<long>(w.units.size()));
    }
}

/// Static, once per seed: which recipes consume each r4 good and whether the
/// campaign band admits them; the household / background basket weights.
void r4_static(const world& w, const r4_state& s, counts& c)
{
    const recipe_registry& reg = *s.reg;
    for (const std::size_t g : s.goods)
    {
        const std::string G = gname(g);
        for (std::size_t id = 0; id < reg.recipe_count(); ++id)
        {
            const recipe* rc = reg.get_recipe(static_cast<uint16_t>(id));
            if (!rc || !(rc->inputs[g] > 0.0f)) continue;
            c.add("r4.static." + G + ".recipe." + rc->name + (era_permits(w.campaign_band, rc->era) ? ".in_band" : ".out_of_band"));
            long built = 0;
            for (const auto& [bid, b] : w.buildings)
                if (b.type == building_type::processing_facility && b.recipe == id) ++built;
            c.add("r4.static." + G + ".recipe_built." + rc->name, built);
        }
        c.add("r4.static." + G + ".household_x100", std::lround(100.0f * reg.population_demand_basket()[g]));
        c.add("r4.static." + G + ".background_x100", std::lround(100.0f * reg.background_demand_basket()[g]));
        c.add("r4.static." + G + ".unit_gph_x1000", std::lround(1000.0f * reg.military().upkeep.goods_per_head[g]));
    }
}

/// Tick -11's zeroed producers of an r4 good: did their market (or body, or any
/// market) bid the good AFTER that tick's clear -- the register the dial would
/// read next -- when it read none before?
void r4_zero_join(const world& w, const r4_state& s, const std::vector<zero_rec>& events, counts& c)
{
    for (const zero_rec& z : events)
    {
        if (z.tick != -11 || z.g >= resource_count || z.m == null_entity) continue;
        const auto gi = std::find(s.goods.begin(), s.goods.end(), z.g);
        if (gi == s.goods.end()) continue;
        const std::size_t i = static_cast<std::size_t>(gi - s.goods.begin());
        const std::string k = "r4.z-11." + gname(z.g) + "." + z.cls + ".";
        c.add(k + "n");
        const auto pd = s.post_dem.find(z.m);
        const bool own = pd != s.post_dem.end() && pd->second[i] > 0.0f;
        const auto ph = s.post_hw.find(z.m);
        const bool own_hw = ph != s.post_hw.end() && ph->second[i] > 0.0f;
        bool body = false, any = false;
        const entity_id zb = w.markets.count(z.m) ? w.markets.at(z.m).body : null_entity;
        for (const auto& [mid, d] : s.post_dem)
        {
            if (!(d[i] > 0.0f)) continue;
            any = true;
            if (w.markets.count(mid) && w.markets.at(mid).body == zb) body = true;
        }
        c.add(k + (own ? "own_after_clear_pos" : body ? "body_only_after_clear" : any ? "offbody_only_after_clear" : "none_after_clear"));
        if (own_hw) c.add(k + "own_hw_after_clear_pos");
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
        if (std::strcmp(k_campaign_settle_lap_names[i], "clear_markets") == 0) p.lap_clear = i;
    }
    // round 4
    r4_state r4;
    r4.reg = &reg;
    r4.lap_pre = p.lap_pre; r4.lap_econ = p.lap_econ; r4.lap_clear = p.lap_clear;
    r4.c = &out.h;
    for (const char* n : {"steel", "refined_fuel", "silicon", "clean_water", "food_rations"})
    {
        bool ok = false;
        const resource_type rt = resource_names::resource_from_name(n, ok);
        if (ok) r4.goods.push_back(static_cast<std::size_t>(rt));
    }
    p.r4 = &r4;
    p.r4_hook = r4_hook;
    r4_static(w, r4, out.h);
    r4.label = "build";
    r4_registers(w, r4, "pre", out.h);
    const auto r4_arm = [&](int t) {
        r4.armed = (t == -12 || t == -11 || t == -10 || t == -9 || t == -1 || t == 1 || t == k_t50);
        r4.label = "t" + std::to_string(t);
    };

    std::map<entity_id, int> prev_target;
    std::map<entity_id, zero_rec> zeros;
    std::vector<zero_rec> events;
    const auto snap_targets = [&]() {
        prev_target.clear();
        for (const entity_id bid : sorted_processors(w))
            prev_target[bid] = static_cast<int>(w.buildings.at(bid).workforce_target);
    };
    snap_targets();
    p.prev_target = &prev_target;
    p.zeros = &zeros;
    p.events = &events;
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

    // round 4: recipe switches (the reflex's floored-output rescue and the
    // scorer's margin chase), keyed from -> to, settle vs play.
    std::map<entity_id, uint16_t> r4_prev_recipe;
    const auto r4_snap_recipes = [&]() {
        r4_prev_recipe.clear();
        for (const entity_id bid : sorted_processors(w)) r4_prev_recipe[bid] = w.buildings.at(bid).recipe;
    };
    r4_snap_recipes();

    std::map<std::string, int> dz_shown;
    // after each tick: rows, transitions, events
    const auto observe = [&](int t, const economy_report& rep, const char* phase) {
        for (const agency_event& ev : rep.agency_events)
            if (ev.what == agency_event::kind::recipe_switch)
            {
                const auto pr = r4_prev_recipe.find(ev.building);
                const recipe* from = pr != r4_prev_recipe.end() ? reg.get_recipe(pr->second) : nullptr;
                const recipe* to = reg.get_recipe(ev.new_recipe);
                out.h.add(std::string("r4.switch.") + phase + "." + (from ? from->name : std::string("?")) + ".to."
                          + (to ? to->name : std::string("?")));
            }
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
        p.cur_tick = step - k_campaign_settle_ticks;
        r4_arm(p.cur_tick);
        settle_tick_result res = run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true, &hooks);
        if (r4.armed) r4_after_tick(w, res.report, r4, p.cur_tick == -11);
        observe(step - k_campaign_settle_ticks, res.report, "settle");
        r4_snap_recipes();
        snap_targets();
        if (step == k_campaign_settle_ticks - 1) last_settle = std::move(res.report);
    }
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    if (seat.seated == null_entity) { out.fail = "no corporation seated"; return; }
    read_idle(w, reg, last_settle, p, H, seat.seated, out.h, seed, "h");
    census(w, reg, last_settle, H, zeros, events, out.h, seed);
    r4_zero_join(w, r4, events, out.h);
    // round 4, Q2: the processors running at the handoff, followed to tick 50.
    std::vector<entity_id> run_h;
    for (const entity_id bid : sorted_processors(w))
        if (classify(w.buildings.at(bid), row_of(last_settle, bid), reg) == ps_run) run_h.push_back(bid);
    p.l_armed = false;
    p.snap.clear();
    r4_snap_recipes(); // the seat's clean slate may have demolished some

    constexpr int k_econ_tick_days = 90;
    for (int k = 1; k <= ticks; ++k)
    {
        const int day = k * k_econ_tick_days;
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = day;
        p.l_armed = (k == k_t50);
        p.cur_tick = k;
        r4_arm(k);
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day,
                                                 /*spectating=*/false, &hooks);
        if (r4.armed) r4_after_tick(w, res.report, r4, false);
        observe(k, res.report, "play");
        r4_snap_recipes();
        if (k == k_t50)
            for (const entity_id bid : run_h)
            {
                const auto bi = w.buildings.find(bid);
                out.t50.add("decay.n");
                if (bi == w.buildings.end()) { out.t50.add("decay.gone"); continue; }
                const building_component& b = bi->second;
                const building_report* row = row_of(res.report, bid);
                const proc_state s = classify(b, row, reg);
                const hist* h = H.count(bid) ? &H.at(bid) : nullptr;
                const recipe* rc = reg.get_recipe(b.recipe);
                const std::string rn = rc ? rc->name : std::string("?");
                std::string cls;
                if (s == ps_run) cls = "still_run";
                else if (s == ps_input)
                    cls = std::string("starved.") + (row && static_cast<std::size_t>(row->limiting_input) < resource_count
                                                     ? gname(static_cast<std::size_t>(row->limiting_input)) : std::string("?"));
                else if (s == ps_decom)
                    cls = std::string("decom.") + (h && h->decom ? h->d.by : "untracked") + "." + (h && h->decom ? h->d.why : std::string("?"));
                else if (s == ps_nolab) cls = "nolab";
                else if (s == ps_unsup)
                    cls = building_supply_scalar(b) <= 0.0f ? "unsup.supply0"
                        : (h && h->wf_value == 0 ? "unsup.dial_target0" : "unsup.target0_other");
                else cls = std::string("other.") + k_state_name[s];
                out.t50.add("decay." + cls);
                if (s != ps_run)
                {
                    const std::string top = cls.substr(0, cls.find('.'));
                    out.t50.add("decay_top." + top);
                    out.t50.add("decay_recipe." + rn + "." + top);
                    if (h && h->decom && s == ps_decom && !h->d.lim.empty()) out.t50.add("decay.decom_starved_input." + h->d.lim);
                    if (h && h->decom && s == ps_decom) out.t50.add(std::string("decay.decom_tick_band.") + (h->d.tick <= 10 ? "1-10" : h->d.tick <= 25 ? "11-25" : "26-50"));
                }
            }
        snap_targets();
        if (k == k_t50) read_idle(w, reg, res.report, p, H, seat.seated, out.t50, seed, "t50");
        if (k == k_t50 && g_ids_base.count(seed))
            for (const entity_id bid : g_ids_base[seed])
                if (const auto bi = w.buildings.find(bid); bi != w.buildings.end())
                    out.t50.add(std::string("cf503.state.") + k_state_name[classify(bi->second, row_of(res.report, bid), reg)]);
                else
                    out.t50.add("cf503.state.gone");
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
        else if (!std::strcmp(argv[i], "--ids-out") && i + 1 < argc) g_ids_out = argv[++i];
        else if (!std::strcmp(argv[i], "--ids-in") && i + 1 < argc) g_ids_in = argv[++i];
        else { std::fprintf(stderr, "usage: handoff_idle_probe [--seeds a,b] [--ticks N] [--examples N] [--ids-out F] [--ids-in F]\n"); return 2; }
    }
    if (!g_ids_out.empty()) g_ids_file = std::fopen(g_ids_out.c_str(), "w");
    if (!g_ids_in.empty())
        if (std::FILE* f = std::fopen(g_ids_in.c_str(), "r"))
        {
            unsigned s = 0; unsigned long long id = 0;
            while (std::fscanf(f, "%u %llu", &s, &id) == 2) g_ids_base[s].insert(static_cast<entity_id>(id));
            std::fclose(f);
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
    if (g_ids_file) std::fclose(g_ids_file);
    return fails ? 1 : 0;
}
