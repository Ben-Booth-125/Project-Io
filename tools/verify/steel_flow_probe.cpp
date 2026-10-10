// ---------------------------------------------------------------------------
// steel_flow_probe — BL-1229 (steel stays home), diagnosis probe
// ---------------------------------------------------------------------------
// QUESTION. inflow_probe (BL-1217) found steel NOT short system-wide, yet its
// starved users are refused: 'ordered' 31% (dispatch_convoys skips a
// (corp, body, good) under a standing sell order), 'gate' 25%, 'noroom' 28%
// (an upper bound). Why does existing steel stay home?
//
// THE WORLD. Seated exactly as market_viability / inflow_probe seat it:
// build_app_start_world, the 12-tick settle (spectating), seat_player_corporation,
// then PLAY ticks as run_app_live_window steps them.
//
// A PURE READER. Nothing the simulation reads is written; the router's path
// caches are warmed through a const_cast (export_refusal.hpp's argument).
//
// PART A — THE ORDER BOOK, every play tick (lap 1, after the economy step +
// dispatch, before the clear; and lap 2, after the clear):
//   every standing sell order, by id: who placed it —
//     settle/seat   placed during the 12-tick settle (id < the book cursor at
//                   seating) on the corp that was then SEATED: the scorer acted
//                   on it while nobody was seated, and the seat inherits it
//     settle/rival  placed during the settle on a background corp (scorer)
//     play/rival    placed in play on a background corp (scorer)
//     play/seat     placed in play on the seated corp (nothing should)
//   its floor vs the market price at its pools, how long it stands (first and
//   last tick seen, still standing at the end), the surplus it holds out of
//   dispatch (the corp's pools on the body above reservation and this tick's
//   arrivals — exactly what dispatch_convoys would have considered), how much
//   of that sold at the clear, and the COUNTERFACTUAL: the dispatcher's own rule
//   (gate, leg, margin, room) re-run as if the order were absent — how much of
//   the held surplus a paying destination with room would have taken.
//
// PART B — STARVED STEEL USERS at reads 10/25/50/100: every processor whose
// limiting input is steel (G1's `input` state), and every (corp pool X on the
// same body, X != M) holding steel surplus, classed by dispatch_convoys' rules
// for X -> M: ordered | gate | noroute | costly | noroom | room. Since BL-1229's
// fix (an order is a floor, not a hold) an ordered pool is classed by the normal
// rule at max(home, floor), and 'ordered' is a pair only the floor refuses.
// For 'gate':
// source price vs destination price, the destination's base, ceiling
// (reservation_mult x base), demand, listed supply, shelf and the suppressed
// want (hauler_want). For noroom/room: room read PRE-PASS — pending less every
// convoy THIS tick's dispatch pass committed to (M, steel) — so it is no longer
// an upper bound.
//
// PART C — ALL GOODS: the same class tally for every starved input, to name any
// good whose starved users are mostly 'ordered'.
//
// Usage (repo root): build_gen/verify/steel_flow_probe.exe [--seeds a,b] [--ticks N]
// Build:  ./tools/verify/build_lua_harness.sh steel_flow_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/resource_names.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {

constexpr int k_sel_ticks[] = {10, 25, 50, 100};
constexpr std::size_t k_steel = static_cast<std::size_t>(resource_type::steel);

std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

enum origin : int { o_settle_seat = 0, o_settle_rival, o_play_rival, o_play_seat, o_count };
const char* k_origin[o_count] = {"settle/seat (inherited at handoff)", "settle/rival (scorer)",
                                 "play/rival (scorer)", "play/seat"};

enum pcls : int { p_ordered = 0, p_gate, p_noroute, p_costly, p_noroom, p_room, p_count };
const char* k_pcls[p_count] = {"ordered", "gate", "noroute", "costly", "noroom(pre-pass)", "room"};

struct order_rec
{
    entity_id corp = null_entity, body = null_entity;
    std::size_t g = 0;
    float floor = 0;
    int origin = 0;
    int first = -1, last = -1;
    int ticks_surplus = 0;       ///< ticks the order held surplus at dispatch
    int ticks_floor_bound = 0;   ///< ticks the floor sat above the post-clear price at a pool with surplus
    double held = 0;             ///< surplus held out of dispatch, summed over ticks
    double sold = 0;             ///< of it, sold at the clear (pool drawdown)
    double cf_paying = 0;        ///< counterfactual: a paying destination with room would have taken
    double price_over_base = 0, floor_over_base = 0; int pn = 0;
    double cf_net_gain = 0;      ///< sum over cf units of (net_d - price_src)
};

struct pair_tally
{
    std::array<double, p_count> units{};
    std::array<int, p_count> n{};
    int starved = 0;
    double unmet = 0;
};

struct gate_tally
{
    int n = 0;
    double w = 0; ///< surplus-weighted
    int dest_cheaper = 0, within_margin = 0;
    double ps = 0, pd = 0, ps_b = 0, pd_b = 0; ///< weighted sums: price, price/base
    int dest_over_ceiling = 0, dest_silenced = 0, src_over_ceiling = 0;
    double dest_demand = 0, dest_supply = 0, dest_shelf = 0, dest_hw = 0;
};

struct room_tally
{
    int n_post0 = 0, n_pre0 = 0, n_pre_pos = 0;
    double absorb = 0, pend_post = 0, pend_pre = 0;
    int absorb0 = 0;
};

struct run_out
{
    std::map<std::size_t, std::array<int, o_count>> orders_by_good;   ///< distinct orders seen
    std::array<order_rec, o_count> steel_by_origin{};                 ///< summed recs (steel)
    std::array<int, o_count> steel_orders{}, steel_closed{};
    double steel_life = 0; int steel_life_n = 0;
    pair_tally steel;
    std::map<std::size_t, pair_tally> goods;
    gate_tally gate;
    room_tally room;
    double steel_pool_total = 0, steel_pool_ordered = 0; int steel_pool_reads = 0;
    /// BL-1229 (an order is a floor, not a hold): steel convoys committed by the
    /// tick's dispatch pass, and of them those hauled out of an ORDERED
    /// (corp, body, steel) pool - zero by construction under the old hold.
    double steel_shipped = 0, steel_shipped_ordered = 0;
};

using units_map = std::map<std::tuple<entity_id, std::size_t, entity_id>, std::array<double, p_count>>;
units_map g_lap_units; ///< lap 1's pair classes for the sel tick, read by analyse_starved

struct probe
{
    int lap_conv = -1, lap_econ = -1, lap_clear = -1;
    const recipe_registry* reg = nullptr;
    std::uint32_t max_convoy = 0;
    std::uint32_t seat_cut = 0;
    entity_id seat = null_entity;
    int tick = 0;
    bool sel = false;
    std::map<std::uint32_t, order_rec> orders;
    /// lap-1 per order: surplus held per (corp, market) pool, for the clear's drawdown
    std::map<std::uint32_t, std::map<entity_id, float>> pre_pool;
    run_out* R = nullptr;
};

float pool_q(const world& w, entity_id corp, entity_id key, std::size_t g)
{
    const auto it = w.corp_market_pools.find({corp, key});
    return it == w.corp_market_pools.end() ? 0.0f : it->second.quantities[g];
}

/// The dispatcher's rule for one pool and one good, as if no order stood on it:
/// the units a paying destination with room would take, and its net gain/unit.
std::pair<float, float> counterfactual(world& w, const recipe_registry& reg, const logistics_nodes& nodes,
                                       const std::vector<entity_id>& corp_ids, const std::vector<entity_id>& mids,
                                       reservation_memo& memo, entity_id corp, entity_id X, std::size_t g,
                                       float surplus)
{
    const float margin = reg.dispatch_margin();
    const float price_src = dispatch_home_price(w, X, g);
    const float gate = price_src + margin * price_src;
    const entity_id src_body = pool_key_body(w, X);
    struct cand { float net; entity_id d; float haul; };
    std::vector<cand> cs;
    for (const entity_id d : mids)
    {
        if (d == X) continue;
        const market_component& dm = w.markets.at(d);
        const float pd = dispatch_market_price(dm, g);
        if (!(pd > gate)) continue;
        const float probe_qty = std::min(surplus, 1.0f);
        const convoy_leg leg = price_convoy_leg(w, reg, nodes, corp, X, d, g, probe_qty,
                                                reg.logistics_cost(convoy_mode::space));
        if (!leg.viable) continue;
        const float haul = leg.cost / probe_qty;
        const float net = pd - haul;
        if (!(net - price_src > margin * price_src)) continue;
        cs.push_back({net, d, haul});
        (void)src_body;
    }
    std::sort(cs.begin(), cs.end(), [](const cand& a, const cand& b) {
        return a.net != b.net ? a.net > b.net : a.d < b.d; });
    float left = surplus, took = 0, gain = 0;
    for (const cand& c : cs)
    {
        if (!(left > 0.0f)) break;
        const float room = dispatch_room(w, reg, c.d, g, price_src + c.haul, corp_ids, memo);
        const float q = std::min(left, room);
        if (!(q > 0.0f) || !std::isfinite(q)) continue;
        took += q; left -= q; gain += q * (c.net - price_src);
        break; // the dispatcher sends to ONE destination per (pool, good) per pass
    }
    return {took, took > 0 ? gain / took : 0.0f};
}

void lap_econ(world& w, probe& p)
{
    const recipe_registry& reg = *p.reg;
    std::vector<entity_id> corp_ids, mids;
    for (const auto& kv : w.corporations) corp_ids.push_back(kv.first);
    for (const auto& kv : w.markets) mids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    std::sort(mids.begin(), mids.end());
    reservation_memo memo;
    const logistics_nodes nodes = collect_logistics_nodes(w);
    const auto reserve = [&](entity_id corp, entity_id key) -> const std::array<float, resource_count>& {
        auto it = memo.find({corp, key});
        if (it == memo.end()) it = memo.emplace(std::make_pair(corp, key), processor_reservation(w, reg, corp, key)).first;
        return it->second;
    };

    p.pre_pool.clear();
    for (const sell_order& o : w.sell_orders)
    {
        const std::size_t g = static_cast<std::size_t>(o.resource);
        auto [it, fresh] = p.orders.try_emplace(o.id);
        order_rec& r = it->second;
        if (fresh)
        {
            r.corp = o.corp; r.body = o.body; r.g = g; r.floor = o.floor_price; r.first = p.tick;
            const bool seat = o.corp == p.seat;
            r.origin = o.id < p.seat_cut ? (seat ? o_settle_seat : o_settle_rival) : (seat ? o_play_seat : o_play_rival);
            p.R->orders_by_good[g][r.origin] += 1;
        }
        r.last = p.tick;
        // the surplus it holds out of dispatch: every pool of the corp on the body
        float held = 0;
        for (auto q = w.corp_market_pools.lower_bound({o.corp, entity_id{0}});
             q != w.corp_market_pools.end() && q->first.first == o.corp; ++q)
        {
            const entity_id X = q->first.second;
            if (pool_key_body(w, X) != o.body) continue;
            const float s = q->second.quantities[g] - reserve(o.corp, X)[g] - dispatch_arrived(w, o.corp, X, g);
            if (!(s > 0.0f)) continue;
            held += s;
            p.pre_pool[o.id][X] = q->second.quantities[g];
            if (g == k_steel)
            {
                const auto [cq, cg] = counterfactual(w, reg, nodes, corp_ids, mids, memo, o.corp, X, g, s);
                r.cf_paying += cq;
                r.cf_net_gain += cq * cg;
            }
            const auto mi = w.markets.find(X);
            if (mi != w.markets.end() && mi->second.base_price[g] > 0.0f)
            {
                r.price_over_base += dispatch_market_price(mi->second, g) / mi->second.base_price[g];
                r.floor_over_base += o.floor_price / mi->second.base_price[g];
                ++r.pn;
            }
        }
        if (held > 0.0f) { ++r.ticks_surplus; r.held += held; }
    }

    // steel stock: how much of every corp pool's steel surplus sits under an order
    {
        std::set<std::pair<entity_id, entity_id>> ordered;
        for (const sell_order& o : w.sell_orders)
            if (static_cast<std::size_t>(o.resource) == k_steel) ordered.insert({o.corp, o.body});
        double tot = 0, ord = 0;
        for (const auto& [k, pool] : w.corp_market_pools)
        {
            if (!w.corporations.count(k.first)) continue;
            const float s = pool.quantities[k_steel] - reserve(k.first, k.second)[k_steel];
            if (!(s > 0.0f)) continue;
            tot += s;
            if (ordered.count({k.first, pool_key_body(w, k.second)})) ord += s;
        }
        p.R->steel_pool_total += tot; p.R->steel_pool_ordered += ord; ++p.R->steel_pool_reads;
        // BL-1229: this tick's steel convoys, and those out of an ordered pool
        for (const convoy_component& c : w.convoys)
        {
            if (c.id <= p.max_convoy || static_cast<std::size_t>(c.cargo_resource) != k_steel) continue;
            p.R->steel_shipped += c.cargo_qty;
            const entity_id sb = c.source_market != null_entity ? pool_key_body(w, c.source_market) : null_entity;
            if (ordered.count({c.corp, sb})) p.R->steel_shipped_ordered += c.cargo_qty;
        }
    }

    if (!p.sel) return;

    // ---- PART B / C: starved users, every (pool X -> M) pair ----
    // BL-1229 (an order is a floor, not a hold): an ordered pool is hauled by the
    // normal rule with max(home, highest floor) as its home price. 'ordered' now
    // names a pair the FLOOR refuses - one the home price alone would pass.
    std::map<std::tuple<entity_id, entity_id, std::size_t>, float> ordered;
    for (const sell_order& o : w.sell_orders)
    {
        float& f = ordered[{o.corp, o.body, static_cast<std::size_t>(o.resource)}];
        f = std::max(f, o.floor_price);
    }
    std::map<std::pair<entity_id, std::size_t>, float> new_to;
    for (const convoy_component& c : w.convoys)
        if (c.id > p.max_convoy && !c.arrived) new_to[{c.dest_market, static_cast<std::size_t>(c.cargo_resource)}] += c.cargo_qty;

    const float margin = reg.dispatch_margin();
    const float res_mult = reg.price_band().reservation_mult;
    const float base_rate = reg.economics(building_type::processing_facility).base_rate;
    (void)base_rate;
    // starved: the report is not available at lap 1, so a processor's starvation
    // is recorded at the clear lap (analyse below) — here we stash the pair classes
    // for every (body, g, M) with a processor consuming g.
    std::map<std::tuple<entity_id, std::size_t, entity_id>, std::array<double, p_count>> cls_units;
    std::map<std::tuple<entity_id, std::size_t, entity_id>, std::array<int, p_count>> cls_n;
    std::set<std::tuple<entity_id, std::size_t, entity_id>> dests;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility || b.decommissioned || b.ticks_remaining > 0) continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (!rc) continue;
        const entity_id m = market_for_tile(w, b.tile);
        const auto mi = w.markets.find(m);
        if (mi == w.markets.end()) continue;
        for (std::size_t g = 0; g < resource_count; ++g)
            if (rc->inputs[g] > 0.0f && !reg.grid_goods().grid(g)) dests.insert({mi->second.body, g, m});
    }
    std::map<std::pair<entity_id, std::size_t>, float> pend_post;
    for (const auto& [body, g, M] : dests)
    {
        const market_component& dm = w.markets.at(M);
        const float price_d = dispatch_market_price(dm, g);
        for (const auto& [pk, pool] : w.corp_market_pools)
        {
            const entity_id corp = pk.first, X = pk.second;
            if (X == M || !w.corporations.count(corp) || pool_key_body(w, X) != body) continue;
            const float s = pool.quantities[g] - reserve(corp, X)[g] - dispatch_arrived(w, corp, X, g);
            if (!(s > 0.0f)) continue;
            int c;
            const float home = dispatch_home_price(w, X, g);
            const auto  ofl  = ordered.find({corp, body, g});
            const float price_src = ofl != ordered.end() ? std::max(home, ofl->second) : home;
            const bool  floor_up  = price_src > home; // the floor, not home, is the bar
            if (!(price_d > price_src + margin * price_src))
            {
                c = (floor_up && price_d > home + margin * home) ? p_ordered : p_gate;
                if (c == p_gate && g == k_steel && p.R)
                {
                    gate_tally& G = p.R->gate;
                    ++G.n; G.w += s;
                    if (price_d <= price_src) ++G.dest_cheaper; else ++G.within_margin;
                    G.ps += s * price_src; G.pd += s * price_d;
                    const auto xi = w.markets.find(X);
                    const float bs = xi != w.markets.end() ? xi->second.base_price[g] : 0.0f;
                    const float bd = dm.base_price[g];
                    if (bs > 0) G.ps_b += s * price_src / bs;
                    if (bd > 0) G.pd_b += s * price_d / bd;
                    if (bd > 0 && price_d > res_mult * bd) ++G.dest_over_ceiling;
                    if (bs > 0 && price_src > res_mult * bs) ++G.src_over_ceiling;
                    if (dm.hauler_want[g] > 0.0f) ++G.dest_silenced;
                    G.dest_demand += s * dm.demand[g]; G.dest_supply += s * dm.supply[g];
                    G.dest_shelf += s * dm.inventory[g]; G.dest_hw += s * dm.hauler_want[g];
                }
            }
            else
            {
                const convoy_leg leg = price_convoy_leg(w, reg, nodes, corp, X, M, g, 1.0f,
                                                        reg.logistics_cost(convoy_mode::space));
                if (!leg.viable) c = p_noroute;
                else if (!(price_d - leg.cost - price_src > margin * price_src))
                    c = (floor_up && price_d - leg.cost - home > margin * home) ? p_ordered : p_costly;
                else
                {
                    const float absorb = dispatch_absorbable(w, reg, M, g, price_src + leg.cost);
                    auto pi = pend_post.find({M, g});
                    if (pi == pend_post.end())
                        pi = pend_post.emplace(std::make_pair(M, g), dispatch_pending(w, reg, M, g, corp_ids, memo)).first;
                    const float nt = new_to.count({M, g}) ? new_to[{M, g}] : 0.0f;
                    const float pre = std::max(0.0f, pi->second - nt);
                    const bool room_pre = absorb > 0.0f && absorb - pre > 0.0f;
                    c = room_pre ? p_room : p_noroom;
                    if (g == k_steel && p.R)
                    {
                        room_tally& RT = p.R->room;
                        if (!(absorb > 0.0f)) ++RT.absorb0;
                        if (!(absorb - pi->second > 0.0f)) ++RT.n_post0;
                        if (!room_pre) ++RT.n_pre0; else ++RT.n_pre_pos;
                        RT.absorb += std::isfinite(absorb) ? absorb : 0.0; RT.pend_post += pi->second; RT.pend_pre += pre;
                    }
                }
            }
            cls_units[{body, g, M}][c] += s;
            cls_n[{body, g, M}][c] += 1;
        }
    }
    // the report is not out until the tick ends: analyse_starved reads these
    g_lap_units = std::move(cls_units);
    (void)cls_n;
}

void lap_clear(const world& w, probe& p)
{
    for (const sell_order& o : w.sell_orders)
    {
        auto it = p.orders.find(o.id);
        if (it == p.orders.end()) continue;
        order_rec& r = it->second;
        const auto pp = p.pre_pool.find(o.id);
        if (pp == p.pre_pool.end()) continue;
        bool bound = false;
        for (const auto& [X, q1] : pp->second)
        {
            const float q2 = pool_q(w, o.corp, X, r.g);
            r.sold += std::max(0.0f, q1 - q2);
            const auto mi = w.markets.find(X);
            if (mi != w.markets.end() && o.floor_price > mi->second.price[r.g]) bound = true;
        }
        if (bound) ++r.ticks_floor_bound;
    }
    // orders closed at this clear: their lap-1 pools still count as sold
    for (const auto& [id, pools] : p.pre_pool)
    {
        bool live = false;
        for (const sell_order& o : w.sell_orders) if (o.id == id) { live = true; break; }
        if (live) continue;
        order_rec& r = p.orders.at(id);
        for (const auto& [X, q1] : pools) r.sold += std::max(0.0f, q1 - pool_q(w, r.corp, X, r.g));
    }
}

void after_lap(const world& cw, int lap, void* ctx)
{
    auto* p = static_cast<probe*>(ctx);
    if (lap == p->lap_econ) lap_econ(const_cast<world&>(cw), *p);
    else if (lap == p->lap_clear) lap_clear(cw, *p);
}

void analyse_starved(world& w, const recipe_registry& reg, const economy_report& rep, run_out& R,
                     const units_map& units,
                     int tick, std::uint32_t seed)
{
    const float base_rate = reg.economics(building_type::processing_facility).base_rate;
    pair_tally tick_steel;
    for (const building_report& br : rep.buildings)
    {
        const auto bi = w.buildings.find(br.building);
        if (bi == w.buildings.end()) continue;
        const building_component& b = bi->second;
        if (b.type != building_type::processing_facility || b.decommissioned || b.ticks_remaining > 0) continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (!rc) continue;
        const float eff = br.effective_workforce;
        if (br.active || !(eff > 0.0f) || building_supply_scalar(b) <= 0.0f || b.workforce_target <= 0.0f || !br.has_limiting)
            continue;
        const std::size_t g = static_cast<std::size_t>(br.limiting_input);
        if (g >= resource_count || reg.grid_goods().grid(g)) continue;
        const entity_id m = market_for_tile(w, b.tile);
        const auto mi = w.markets.find(m);
        if (mi == w.markets.end()) continue;
        const float wt = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
        const float need = rc->inputs[g] * base_rate * eff * wt * building_supply_scalar(b);
        pair_tally& T = R.goods[g];
        ++T.starved; T.unmet += need;
        if (g == k_steel) { ++R.steel.starved; R.steel.unmet += need; ++tick_steel.starved; tick_steel.unmet += need; }
        const auto ui = units.find({mi->second.body, g, m});
        if (ui == units.end()) continue;
        double tot = 0;
        for (int c = 0; c < p_count; ++c) tot += ui->second[c];
        if (!(tot > 0)) continue;
        // pool this processor's need over the classes of the stock on its body
        for (int c = 0; c < p_count; ++c)
        {
            const double u = need * ui->second[c] / tot;
            T.units[c] += u;
            if (g == k_steel) { R.steel.units[c] += u; tick_steel.units[c] += u; }
        }
    }
    double tu = 0;
    for (int c = 0; c < p_count; ++c) tu += tick_steel.units[c];
    std::printf("  seed %u t%3d steel-starved %d (need %.1f u/t); classes of off-M steel surplus on the body:", seed, tick,
                tick_steel.starved, tick_steel.unmet);
    for (int c = 0; c < p_count; ++c) std::printf(" %s %.0f%%", k_pcls[c], tu > 0 ? 100.0 * tick_steel.units[c] / tu : 0.0);
    std::printf("\n");
}

void run_seed(std::uint32_t seed, int ticks, run_out& R)
{
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, wp, *start); }
    catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); return; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    std::printf("\n=== seed %u ===\n", seed);

    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        run_settle_tick(w, reg, step, 0, true);
    seat_player_corporation(w, seed, start->land.search.winner_score);

    probe p;
    p.reg = &reg;
    p.R = &R;
    p.seat = w.player_entity;
    p.seat_cut = w.next_order_id;
    {
        int n = 0, ns = 0, steel = 0;
        for (const sell_order& o : w.sell_orders)
        {
            ++n; if (o.corp == p.seat) ++ns;
            if (static_cast<std::size_t>(o.resource) == k_steel) ++steel;
        }
        std::printf("  at seating: %d standing sell orders (%d on the seated corp %u), %d on steel; floor multiple of base on the book:",
                    n, ns, unsigned(p.seat), steel);
        double fm = 0; int fn = 0;
        for (const sell_order& o : w.sell_orders)
        {
            const entity_id mid = [&] { for (const auto& [id, m] : w.markets) if (m.body == o.body && m.base_price[static_cast<std::size_t>(o.resource)] > 0) return id; return null_entity; }();
            if (mid != null_entity) { fm += o.floor_price / w.markets.at(mid).base_price[static_cast<std::size_t>(o.resource)]; ++fn; }
        }
        std::printf(" mean %.2f\n", fn ? fm / fn : 0.0);
    }
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
    {
        if (!std::strcmp(k_campaign_settle_lap_names[i], "convoys")) p.lap_conv = i;
        if (!std::strcmp(k_campaign_settle_lap_names[i], "run_economy_step")) p.lap_econ = i;
        if (!std::strcmp(k_campaign_settle_lap_names[i], "clear_markets")) p.lap_clear = i;
    }
    for (const convoy_component& c : w.convoys) p.max_convoy = std::max(p.max_convoy, c.id);

    constexpr int k_days = 90;
    for (int k = 1; k <= ticks; ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        p.tick = k;
        p.sel = std::find(std::begin(k_sel_ticks), std::end(k_sel_ticks), k) != std::end(k_sel_ticks);
        settle_tick_hooks hooks;
        hooks.after_lap = after_lap;
        hooks.ctx = &p;
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, &hooks);
        if (p.sel)
            analyse_starved(w, reg, res.report, R, g_lap_units, k, seed);
        for (const convoy_component& c : w.convoys) p.max_convoy = std::max(p.max_convoy, c.id);
        std::fflush(stdout);
    }

    // ---- per-seed order summary ----
    std::array<order_rec, o_count> by{};
    std::array<int, o_count> n{}, closed{};
    for (const auto& [id, r] : p.orders)
    {
        if (r.g != k_steel) continue;
        order_rec& s = by[r.origin];
        ++n[r.origin];
        if (r.last < ticks) ++closed[r.origin];
        s.ticks_surplus += r.ticks_surplus; s.ticks_floor_bound += r.ticks_floor_bound;
        s.held += r.held; s.sold += r.sold; s.cf_paying += r.cf_paying; s.cf_net_gain += r.cf_net_gain;
        s.price_over_base += r.price_over_base; s.floor_over_base += r.floor_over_base; s.pn += r.pn;
        s.first += 0;
        R.steel_life += (r.last - std::max(r.first, 1) + 1); ++R.steel_life_n;
    }
    std::printf("  steel orders this seed:\n");
    for (int o = 0; o < o_count; ++o)
    {
        if (!n[o]) continue;
        const order_rec& s = by[o];
        std::printf("    %-36s %2d orders (%d closed before t%d) | surplus-ticks %d, floor-bound ticks %d | held %.0f u, sold %.0f (%.0f%%) | cf paying %.0f u (%.0f%% of held), mean gain %.2f/u | price/base %.2f floor/base %.2f\n",
                    k_origin[o], n[o], closed[o], ticks, s.ticks_surplus, s.ticks_floor_bound, s.held, s.sold,
                    s.held > 0 ? 100.0 * s.sold / s.held : 0.0, s.cf_paying, s.held > 0 ? 100.0 * s.cf_paying / s.held : 0.0,
                    s.cf_paying > 0 ? s.cf_net_gain / s.cf_paying : 0.0,
                    s.pn ? s.price_over_base / s.pn : 0.0, s.pn ? s.floor_over_base / s.pn : 0.0);
        order_rec& a = R.steel_by_origin[o];
        a.ticks_surplus += s.ticks_surplus; a.ticks_floor_bound += s.ticks_floor_bound; a.held += s.held; a.sold += s.sold;
        a.cf_paying += s.cf_paying; a.cf_net_gain += s.cf_net_gain; a.price_over_base += s.price_over_base;
        a.floor_over_base += s.floor_over_base; a.pn += s.pn;
        R.steel_orders[o] += n[o]; R.steel_closed[o] += closed[o];
    }
    // longest-standing steel orders
    int shown = 0;
    for (const auto& [id, r] : p.orders)
    {
        if (r.g != k_steel || shown >= 6) continue;
        ++shown;
        std::printf("      #%u corp %u body %u %s: t%d..t%d%s floor %.1f | held %.0f sold %.0f cf %.0f\n", id, unsigned(r.corp),
                    unsigned(r.body), k_origin[r.origin], std::max(r.first, 0), r.last, r.last == ticks ? " (standing)" : "",
                    r.floor, r.held, r.sold, r.cf_paying);
    }
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 100;
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t a = 0;
            while (a <= s.size())
            {
                const std::size_t b = s.find(',', a);
                const std::string tok = s.substr(a, b == std::string::npos ? std::string::npos : b - a);
                if (!tok.empty()) seeds.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (b == std::string::npos) break;
                a = b + 1;
            }
        }
        else { std::fprintf(stderr, "usage: steel_flow_probe [--seeds a,b] [--ticks N]\n"); return 2; }
    }
    std::printf("steel_flow_probe - BL-1229: why existing steel stays home\n");
    run_out R;
    for (const std::uint32_t s : seeds) run_seed(s, ticks, R);

    std::printf("\n================ POOLED over %zu seeds, %d ticks ================\n", seeds.size(), ticks);
    std::printf("\n  ORDER BOOK by good and placer (distinct orders seen in play):\n");
    for (const auto& [g, a] : R.orders_by_good)
    {
        std::printf("  %-18s", gname(g).c_str());
        for (int o = 0; o < o_count; ++o) std::printf("  %s %d", k_origin[o], a[o]);
        std::printf("\n");
    }
    std::printf("\n  STEEL ORDERS by placer:\n");
    for (int o = 0; o < o_count; ++o)
    {
        const order_rec& s = R.steel_by_origin[o];
        if (!R.steel_orders[o]) continue;
        std::printf("    %-36s %3d orders, %d closed | held %.0f u, sold at home %.0f (%.0f%%) | cf paying dest w/ room %.0f u (%.0f%% of held), gain %.2f/u | price/base %.2f floor/base %.2f | floor bound %d of %d surplus-ticks\n",
                    k_origin[o], R.steel_orders[o], R.steel_closed[o], s.held, s.sold, s.held > 0 ? 100.0 * s.sold / s.held : 0.0,
                    s.cf_paying, s.held > 0 ? 100.0 * s.cf_paying / s.held : 0.0, s.cf_paying > 0 ? s.cf_net_gain / s.cf_paying : 0.0,
                    s.pn ? s.price_over_base / s.pn : 0.0, s.pn ? s.floor_over_base / s.pn : 0.0, s.ticks_floor_bound, s.ticks_surplus);
    }
    std::printf("    mean steel order life %.1f ticks (over %d orders)\n", R.steel_life_n ? R.steel_life / R.steel_life_n : 0.0, R.steel_life_n);
    std::printf("    steel surplus (above reservation) in all corp pools, per tick: %.1f u, of it under an order %.1f u (%.0f%%)\n",
                R.steel_pool_reads ? R.steel_pool_total / R.steel_pool_reads : 0.0,
                R.steel_pool_reads ? R.steel_pool_ordered / R.steel_pool_reads : 0.0,
                R.steel_pool_total > 0 ? 100.0 * R.steel_pool_ordered / R.steel_pool_total : 0.0);
    std::printf("    steel convoyed per tick: %.1f u, of it out of an ORDERED pool %.1f u (%.0f%%)\n",
                R.steel_pool_reads ? R.steel_shipped / R.steel_pool_reads : 0.0,
                R.steel_pool_reads ? R.steel_shipped_ordered / R.steel_pool_reads : 0.0,
                R.steel_shipped > 0 ? 100.0 * R.steel_shipped_ordered / R.steel_shipped : 0.0);

    const auto print_pairs = [](const char* lbl, const pair_tally& T) {
        double tu = 0;
        for (int c = 0; c < p_count; ++c) tu += T.units[c];
        std::printf("  %-18s starved %4d need %8.1f |", lbl, T.starved, T.unmet);
        for (int c = 0; c < p_count; ++c) std::printf(" %s %.0f%%", k_pcls[c], tu > 0 ? 100.0 * T.units[c] / tu : 0.0);
        std::printf("  (pooled %.0f of need over off-M stock)\n", tu);
    };
    std::printf("\n  STARVED STEEL USERS, need pooled over the classes of off-M steel surplus on the body (reads 10/25/50/100):\n");
    print_pairs("steel", R.steel);
    const gate_tally& G = R.gate;
    std::printf("\n  GATE (steel pairs %d, surplus-weighted %.0f u): dest cheaper-or-equal %d, dest dearer but within the margin %d\n",
                G.n, G.w, G.dest_cheaper, G.within_margin);
    if (G.w > 0)
        std::printf("    weighted mean: price_src %.2f (%.2fx base), price_dest %.2f (%.2fx base) | dest demand %.1f listed %.1f shelf %.1f hauler_want %.1f\n",
                    G.ps / G.w, G.ps_b / G.w, G.pd / G.w, G.pd_b / G.w, G.dest_demand / G.w, G.dest_supply / G.w,
                    G.dest_shelf / G.w, G.dest_hw / G.w);
    std::printf("    dest price over the ceiling %d, dest with silenced want (hauler_want>0) %d, src over the ceiling %d (of %d pairs)\n",
                G.dest_over_ceiling, G.dest_silenced, G.src_over_ceiling, G.n);
    const room_tally& RT = R.room;
    const int rn = RT.n_pre0 + RT.n_pre_pos;
    std::printf("\n  ROOM (steel pairs past gate+leg %d): absorbable 0 %d | no room POST-pass %d -> PRE-pass %d (room pre-pass %d) | mean absorbable %.1f pending post %.1f pre %.1f\n",
                rn, RT.absorb0, RT.n_post0, RT.n_pre0, RT.n_pre_pos, rn ? RT.absorb / rn : 0.0, rn ? RT.pend_post / rn : 0.0,
                rn ? RT.pend_pre / rn : 0.0);

    std::printf("\n  ALL GOODS (starved users by limiting input; top 12 by need), classes of off-M stock:\n");
    std::vector<std::pair<double, std::size_t>> order;
    for (const auto& [g, T] : R.goods) order.push_back({T.unmet, g});
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    for (std::size_t i = 0; i < order.size() && i < 12; ++i) print_pairs(gname(order[i].second).c_str(), R.goods[order[i].second]);
    return 0;
}
