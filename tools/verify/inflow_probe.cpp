// ---------------------------------------------------------------------------
// inflow_probe — BL-1217 (inputs reach processors), wave 2 diagnosis probe
// ---------------------------------------------------------------------------
// QUESTION. Input-starved processors want ~3x what flows onto their markets.
// For each one's SCARCEST input (the report's limiting_input): where is that
// good made on the body, how far is it, how much is made against how much the
// body's processors want, and why does it not arrive?
//
// THE WORLD. Seated exactly as market_viability seats it: build_app_start_world,
// the 12-tick settle (run_settle_tick, econ steps 0..11, day 0, spectating),
// seat_player_corporation, then PLAY ticks as run_app_live_window steps them.
//
// A PURE READER. At play ticks 10/25/50 it reads three after-lap states:
//   lap 0 (convoys)          P0: every corp pool; extraction tiles' reserves
//   lap 1 (economy+dispatch) P1: pools after production, in-step draws and the
//                            corporations' + markets' dispatch; tile reserves
//                            (the per-good extraction is the reserve delta);
//                            the convoys dispatched this tick; and the
//                            dispatcher's rules re-applied (below) at the very
//                            state dispatch_convoys saw (prices are last
//                            clear's until clear_markets runs)
//   lap 2 (clear_markets)    P2: pools after the clear (auto-surplus sold)
// and the tick's economy report. Nothing the simulation reads is written; the
// router's path caches are warmed through a const_cast (a cache fill computes
// the answer the next call would — export_refusal.hpp's argument).
//
// PRODUCTION per (corp pool, good) this tick:
//   extraction  the tile's resource_remaining delta (co-extraction basket),
//               split among the tile's deposit sites by report output; a
//               depositless site (well / wharf) is its output, all target
//   processing  the report's output_quantity split by the recipe's output
//               shares (exact unless the BL-708 store ceiling clamped one)
//
// FATE of a pool's production (pro-rata over the pool's flows this tick:
// avail = P0 + prod = consumed in-step + shipped + sold at the clear + held):
//   consumed  P0 + prod - shipped - P1   (own processors, construction, upkeep)
//   shipped   corp convoys from the pool this tick, to M or elsewhere
//   sold      P1 - P2                    (auto-surplus at the home clear)
//   held      P2                         (under the processor reservation)
// then the MARKET shelf's own export this tick (export_market_shelves) re-labels
// sold-at-home units at X as shipped (to M or elsewhere), and exports off M's
// own shelf as shipped away from M.
//
// THE POOL, per starved processor at market M with input g, unmet U = full-run
// need (it ran nothing), on body B:
//   (1) deficit   share d = max(0, 1 - prod_B(g) / want_B(g)), want_B = the
//                 full-run need of EVERY live processor on B with input g
//   the rest U x (1 - d) split by the fate of B's production of g w.r.t. M:
//   (0) at M      made in a pool AT M (consumed / sold / held there) — it
//                 reached M's market and was contended away; not a logistics miss
//   (2) local     consumed in-step or held under reservation at X != M
//   (3) elsewhere shipped by its corp (or exported off a shelf) to a market != M
//   (4) refused   sold at home at X != M — the corp dispatcher did not send it
//                 to M; split by the dispatcher's rule for (corp pool X -> M):
//                 ordered | reserved | gate | noroute | costly | noroom |
//                 outranked (passed every rule, another destination netted
//                 more) | lp/solv (passed, room, nothing sent: LP cap/solvency)
//   (5) to M      shipped toward M this tick (corp convoy or shelf export)
// A processor's PRIMARY cause (count pool): (1) if prod_B < want_B, else the
// largest of the rest.
//
// Usage (repo root): build_gen/verify/inflow_probe.exe [--seeds a,b] [--ticks N] [--samples N]
// Build:  ./tools/verify/build_lua_harness.sh inflow_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "export_refusal.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
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
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {

constexpr int k_sel_ticks[] = {10, 25, 50};

using arr = std::array<float, resource_count>;
using pool_key = std::pair<entity_id, entity_id>; // (corp, key)
using pool_map = std::map<pool_key, arr>;

std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

// --- the corporation-side refusal classes (dispatch_convoys' rules) --------
enum ccls : int { k_ordered = 0, k_reserved, k_gate, k_noroute, k_costly, k_noroom, k_outranked, k_lpsolv, k_sent, k_ccount };
const char* k_ccls[k_ccount] = {"ordered", "reserved", "gate", "noroute", "costly", "noroom", "outranked", "lp/solv", "sent"};

// --- shelf-side (export_market_shelves) class for sold-at-home units -------
// export_refusal::cls 0..5, plus 6 = X has no shelf surplus (local draws and
// households ate the shelf), 7 = X is not a market (body-level pool)
constexpr int k_s_nosurplus = export_refusal::c_count, k_s_nomkt = k_s_nosurplus + 1, k_scount = k_s_nomkt + 1;
const char* k_scls[k_scount] = {"body", "noroute", "gate", "costly", "noroom", "room", "noshelfsurplus", "nomarket"};

// --- the pool categories ----------------------------------------------------
enum cat : int { c_atM = 0, c_deficit, c_local, c_else, c_refused, c_toM, c_count };
const char* k_cat[c_count] = {"(0) at M", "(1) body deficit", "(2) consumed/held upstream",
                              "(3) shipped elsewhere", "(4) refused (sold at home)", "(5) to M"};

struct ship_rec { entity_id corp, src, dest; std::size_t g; float qty; };

struct leg_info { bool viable = false; float cost = 0; convoy_mode mode = convoy_mode::land; int ticks = 0; };

/// One sel tick's reads.
struct tick_read
{
    pool_map p0, p1, p2;
    std::map<entity_id, arr> tile0, tile1;            ///< extraction tiles' reserves
    std::vector<ship_rec> ships;                     ///< convoys dispatched this tick
    /// (corp, X, M, g) -> corp-side class (only pools holding g at dispatch)
    std::map<std::tuple<entity_id, entity_id, entity_id, std::size_t>, int> corp_cls;
    /// (X, M, g) -> shelf-side class
    std::map<std::tuple<entity_id, entity_id, std::size_t>, int> shelf_cls;
    std::map<std::pair<entity_id, std::size_t>, float> price_at;    ///< (M, g) dispatch price
    std::map<std::pair<entity_id, std::size_t>, float> inflight_to; ///< (M, g) cargo in flight at lap 1
};

struct probe
{
    int lap_conv = -1, lap_econ = -1, lap_clear = -1;
    bool armed = false;
    tick_read* cur = nullptr;
    const recipe_registry* reg = nullptr;
    std::uint32_t max_id = 0;
};

pool_map copy_pools(const world& w)
{
    pool_map m;
    for (const auto& [k, p] : w.corp_market_pools) m.emplace(k, p.quantities);
    return m;
}

std::map<entity_id, arr> copy_tiles(const world& w)
{
    std::map<entity_id, arr> t;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::extraction_site) continue;
        const auto ti = w.tiles.find(b.tile);
        if (ti != w.tiles.end()) t.emplace(b.tile, ti->second.resource_remaining);
    }
    return t;
}

bool live_processor(const building_component& b) { return !b.decommissioned && b.ticks_remaining <= 0; }

/// The corporation dispatcher's rules (dispatch_convoys) for one pool and one
/// destination, read after the dispatch at the state it saw.
void classify_lap1(world& w, const recipe_registry& reg, tick_read& t)
{
    const float margin = reg.dispatch_margin();
    const logistics_nodes nodes = collect_logistics_nodes(w);
    std::vector<entity_id> corp_ids;
    for (const auto& kv : w.corporations) corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    reservation_memo memo;

    std::set<std::tuple<entity_id, entity_id, std::size_t>> ordered;
    for (const sell_order& o : w.sell_orders) ordered.insert({o.corp, o.body, static_cast<std::size_t>(o.resource)});

    // destinations: markets hosting a live processor with input g, by body
    std::map<std::pair<entity_id, std::size_t>, std::set<entity_id>> dests; // (body, g) -> M
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility || !live_processor(b)) continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (!rc) continue;
        const entity_id m = market_for_tile(w, b.tile);
        const auto mi = w.markets.find(m);
        if (mi == w.markets.end()) continue;
        for (std::size_t g = 0; g < resource_count; ++g)
            if (rc->inputs[g] > 0.0f && !reg.grid_goods().grid(g))
            {
                dests[{mi->second.body, g}].insert(m);
                t.price_at[{m, g}] = dispatch_market_price(mi->second, g);
            }
    }
    for (const convoy_component& c : w.convoys)
        if (!c.arrived) t.inflight_to[{c.dest_market, static_cast<std::size_t>(c.cargo_resource)}] += c.cargo_qty;

    // shipped this tick per (corp, src, g) -> dest qty
    std::map<std::tuple<entity_id, entity_id, std::size_t>, std::map<entity_id, float>> shipped;
    for (const ship_rec& s : t.ships)
        if (s.corp != null_entity) shipped[{s.corp, s.src, s.g}][s.dest] += s.qty;

    std::map<std::pair<entity_id, std::size_t>, float> pend_memo;
    const auto pending = [&](entity_id M, std::size_t g) {
        const auto k = std::make_pair(M, g);
        auto it = pend_memo.find(k);
        if (it == pend_memo.end())
            it = pend_memo.emplace(k, dispatch_pending(w, reg, M, g, corp_ids, memo)).first;
        return it->second;
    };
    std::map<std::tuple<entity_id, entity_id, entity_id>, convoy_leg> legs;

    for (const auto& [pk, q1] : t.p1)
    {
        const entity_id corp = pk.first, X = pk.second;
        if (!w.corporations.count(corp)) continue;
        const entity_id body = pool_key_body(w, X);
        if (body == null_entity) continue;
        bool any = false;
        for (std::size_t g = 0; g < resource_count && !any; ++g) any = q1[g] > 0.0f;
        const auto sh_any = [&](std::size_t g) { return shipped.count({corp, X, g}) != 0; };
        if (!any)
        {
            bool s = false;
            for (std::size_t g = 0; g < resource_count && !s; ++g) s = sh_any(g);
            if (!s) continue;
        }
        auto rit = memo.find({corp, X});
        if (rit == memo.end()) rit = memo.emplace(std::make_pair(corp, X), processor_reservation(w, reg, corp, X)).first;
        const arr reserve = rit->second;
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            if (reg.grid_goods().grid(g)) continue;
            const auto di = dests.find({body, g});
            if (di == dests.end()) continue;
            float ship_tot = 0.0f;
            const std::map<entity_id, float>* sm = nullptr;
            if (auto s = shipped.find({corp, X, g}); s != shipped.end())
            {
                sm = &s->second;
                for (const auto& kv : s->second) ship_tot += kv.second;
            }
            if (!(q1[g] > 0.0f) && !(ship_tot > 0.0f)) continue;
            const float surplus = q1[g] + ship_tot - reserve[g] - dispatch_arrived(w, corp, X, g);
            const float price_src = dispatch_home_price(w, X, g);
            for (const entity_id M : di->second)
            {
                if (M == X) continue;
                int c;
                if (ordered.count({corp, body, g})) c = k_ordered;
                else if (!(surplus > 0.0f)) c = k_reserved;
                else
                {
                    const float price_d = dispatch_market_price(w.markets.at(M), g);
                    if (!(price_d > price_src + margin * price_src)) c = k_gate;
                    else
                    {
                        const auto lk = std::make_tuple(corp, X, M);
                        auto li = legs.find(lk);
                        if (li == legs.end())
                            li = legs.emplace(lk, price_convoy_leg(w, reg, nodes, corp, X, M, g, 1.0f,
                                                                    reg.logistics_cost(convoy_mode::space))).first;
                        const convoy_leg& leg = li->second;
                        if (!leg.viable) c = k_noroute;
                        else if (!(price_d - leg.cost - price_src > margin * price_src)) c = k_costly;
                        else
                        {
                            const bool to_m = sm && sm->count(M);
                            const bool else_ = sm && (sm->size() > (to_m ? 1u : 0u));
                            const float absorb = dispatch_absorbable(w, reg, M, g, price_src + leg.cost);
                            const bool room = absorb > 0.0f && absorb - pending(M, g) > 0.0f;
                            if (to_m) c = k_sent;
                            else if (else_) c = room ? k_outranked : k_noroom;
                            else c = room ? k_lpsolv : k_noroom;
                        }
                    }
                }
                t.corp_cls[{corp, X, M, g}] = c;
            }
        }
    }

    // shelf side: (X market, M, g) for every market X on the body
    export_refusal::context x(w, reg);
    for (const auto& [bg, Ms] : dests)
        for (const entity_id X : x.mids)
        {
            const market_component& xm = w.markets.at(X);
            if (xm.body != bg.first) continue;
            const std::size_t g = bg.second;
            const bool src = market_shelf_surplus(w, X, g) > 0.0f && xm.centre_tile != null_entity
                          && xm.base_price[g] > 0.0f;
            for (const entity_id M : Ms)
            {
                if (M == X) continue;
                t.shelf_cls[{X, M, g}] = src ? export_refusal::classify_pair(x, X, M, g).c : k_s_nosurplus;
            }
        }
}

void after_lap(const world& cw, int lap, void* ctx)
{
    auto* p = static_cast<probe*>(ctx);
    if (!p->armed) return;
    tick_read& t = *p->cur;
    if (lap == p->lap_conv) { t.p0 = copy_pools(cw); t.tile0 = copy_tiles(cw); return; }
    if (lap == p->lap_econ)
    {
        t.p1 = copy_pools(cw);
        t.tile1 = copy_tiles(cw);
        for (const convoy_component& c : cw.convoys)
        {
            if (c.id <= p->max_id) continue;
            t.ships.push_back({c.corp, c.source_market, c.dest_market, static_cast<std::size_t>(c.cargo_resource), c.cargo_qty});
        }
        classify_lap1(const_cast<world&>(cw), *p->reg, t);
        return;
    }
    if (lap == p->lap_clear) t.p2 = copy_pools(cw);
}

// --- tallies ----------------------------------------------------------------
struct tally
{
    int starved = 0;
    std::array<double, c_count> units{};
    std::array<double, k_ccount> refused_by{};   ///< (4) units by corp-side class
    std::array<double, k_scount> refused_shelf{}; ///< (4) units by shelf-side class
    std::array<int, c_count> primary{};
    double unmet = 0;
    double atM_sold = 0; ///< of (0): sold onto M's shelf at the clear (the rest: the producer's own draws / held)
    void add(const tally& o)
    {
        starved += o.starved; unmet += o.unmet; atM_sold += o.atM_sold;
        for (int i = 0; i < c_count; ++i) { units[i] += o.units[i]; primary[i] += o.primary[i]; }
        for (int i = 0; i < k_ccount; ++i) refused_by[i] += o.refused_by[i];
        for (int i = 0; i < k_scount; ++i) refused_shelf[i] += o.refused_shelf[i];
    }
};

struct good_tally
{
    tally t;
    double body_prod = 0, body_want = 0, mkt_demand = 0; ///< summed over distinct (body, tick)
    int bodies_deficit = 0, bodies = 0;
};

struct near_tally
{
    int has_local = 0;                ///< a producer of the input sits at M itself
    std::array<int, 4> nearest_cls{}; ///< nearest NON-local producer: land, sea, space/air, noroute
    double haul_over_price = 0, haul = 0; int haul_n = 0;
    int no_remote = 0;                ///< no producer off M on the body
};

void print_tally(const char* label, const tally& t)
{
    const double U = t.unmet > 0 ? t.unmet : 1.0;
    std::printf("  %s: starved %d, unmet want %.1f u/t\n", label, t.starved, t.unmet);
    std::printf("    units:");
    for (int i = 0; i < c_count; ++i) std::printf("  %s %.0f%%", k_cat[i], 100.0 * t.units[i] / U);
    std::printf("\n    (0) split: sold onto M's shelf %.0f%%, drawn/held by the producer's own corp %.0f%%",
                100.0 * t.atM_sold / U, 100.0 * (t.units[c_atM] - t.atM_sold) / U);
    std::printf("\n    primary (count):");
    for (int i = 0; i < c_count; ++i) std::printf("  %s %d", k_cat[i], t.primary[i]);
    const double R = t.units[c_refused] > 0 ? t.units[c_refused] : 1.0;
    std::printf("\n    (4) by corp rule:");
    for (int i = 0; i < k_ccount; ++i) if (t.refused_by[i] > 0) std::printf("  %s %.0f%%", k_ccls[i], 100.0 * t.refused_by[i] / R);
    std::printf("\n    (4) by shelf-export rule at X:");
    for (int i = 0; i < k_scount; ++i) if (t.refused_shelf[i] > 0) std::printf("  %s %.0f%%", k_scls[i], 100.0 * t.refused_shelf[i] / R);
    std::printf("\n");
}

const char* mode_name(convoy_mode m)
{
    switch (m) { case convoy_mode::land: return "land"; case convoy_mode::sea: return "sea";
                 case convoy_mode::air: return "air"; default: return "space"; }
}

struct run_out
{
    tally all;
    std::map<std::size_t, good_tally> goods;
    near_tally nt;
};

void analyse_tick(world& w, const recipe_registry& reg, const economy_report& rep, const tick_read& t,
                  int tick, std::uint32_t seed, int samples, run_out& R, tally& seed_t)
{
    const float base_rate = reg.economics(building_type::processing_facility).base_rate;
    std::map<entity_id, const building_report*> row;
    for (const building_report& br : rep.buildings) row[br.building] = &br;

    // ---- production per (corp pool, good) ----
    pool_map prod;
    std::map<entity_id, std::vector<std::pair<entity_id, float>>> tile_sites; // tile -> (bid, output)
    for (const auto& [bid, br] : row)
    {
        const auto bi = w.buildings.find(bid);
        if (bi == w.buildings.end() || !br->active || !(br->output_quantity > 0.0f)) continue;
        const building_component& b = bi->second;
        const entity_id key = pool_key_for_tile(w, b.tile);
        if (b.type == building_type::extraction_site)
        {
            if (placement_rules::is_depositless_site(w, b.tile, b.target_resource))
                prod[{br->corp, key}][static_cast<std::size_t>(b.target_resource)] += br->output_quantity;
            else
                tile_sites[b.tile].push_back({bid, br->output_quantity});
        }
        else if (b.type == building_type::processing_facility)
        {
            const recipe* rc = reg.get_recipe(b.recipe);
            if (!rc) continue;
            float tot = 0.0f;
            for (std::size_t g = 0; g < resource_count; ++g) tot += rc->outputs[g];
            if (!(tot > 0.0f)) continue;
            for (std::size_t g = 0; g < resource_count; ++g)
                if (rc->outputs[g] > 0.0f) prod[{br->corp, key}][g] += br->output_quantity * rc->outputs[g] / tot;
        }
    }
    for (const auto& [tile, sites] : tile_sites)
    {
        const auto a = t.tile0.find(tile), b = t.tile1.find(tile);
        if (a == t.tile0.end() || b == t.tile1.end()) continue;
        float sum = 0.0f;
        for (const auto& s : sites) sum += s.second;
        if (!(sum > 0.0f)) continue;
        for (const auto& s : sites)
        {
            const building_component& bb = w.buildings.at(s.first);
            const entity_id key = pool_key_for_tile(w, bb.tile);
            arr& out = prod[{row.at(s.first)->corp, key}];
            for (std::size_t g = 0; g < resource_count; ++g)
            {
                const float d = a->second[g] - b->second[g];
                if (d > 0.0f) out[g] += d * s.second / sum;
            }
        }
    }

    // ---- per processor need; body want ----
    struct pinfo { entity_id bid, m, body; std::size_t g; float need; bool starved; };
    std::vector<pinfo> starved;
    std::map<std::pair<entity_id, std::size_t>, float> body_want, body_prod, body_mdem;
    std::vector<entity_id> procs;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility && live_processor(b)) procs.push_back(bid);
    std::sort(procs.begin(), procs.end());
    for (const entity_id bid : procs)
    {
        const building_component& b = w.buildings.at(bid);
        const recipe* rc = reg.get_recipe(b.recipe);
        if (!rc) continue;
        const auto ri = row.find(bid);
        const building_report* br = ri == row.end() ? nullptr : ri->second;
        const float eff = br ? br->effective_workforce : 0.0f;
        const float wt = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
        const float bf = base_rate * eff * wt * building_supply_scalar(b);
        const entity_id m = market_for_tile(w, b.tile);
        entity_id body = br ? br->body : null_entity;
        if (body == null_entity) { const auto mi = w.markets.find(m); if (mi != w.markets.end()) body = mi->second.body; }
        for (std::size_t g = 0; g < resource_count; ++g)
            if (rc->inputs[g] > 0.0f) body_want[{body, g}] += rc->inputs[g] * bf;
        // G1's `input` state (market_viability classify_row)
        if (!br || br->active || !(eff > 0.0f) || building_supply_scalar(b) <= 0.0f || b.workforce_target <= 0.0f
            || !br->has_limiting)
            continue;
        const std::size_t g = static_cast<std::size_t>(br->limiting_input);
        if (g >= resource_count || w.markets.find(m) == w.markets.end() || reg.grid_goods().grid(g)) continue;
        starved.push_back({bid, m, body, g, rc->inputs[g] * bf, true});
    }
    for (const auto& [pk, a] : prod)
    {
        const entity_id body = pool_key_body(w, pk.second);
        for (std::size_t g = 0; g < resource_count; ++g) if (a[g] > 0.0f) body_prod[{body, g}] += a[g];
    }
    for (const auto& [mid, mc] : w.markets)
        for (std::size_t g = 0; g < resource_count; ++g) if (mc.demand[g] > 0.0f) body_mdem[{mc.body, g}] += mc.demand[g];

    // ---- shipments indexed ----
    std::map<std::tuple<entity_id, entity_id, std::size_t>, std::map<entity_id, float>> cship; // (corp, src, g)
    std::map<std::pair<entity_id, std::size_t>, std::map<entity_id, float>> sship;              // (src mkt, g)
    for (const ship_rec& s : t.ships)
    {
        if (s.corp == null_entity) sship[{s.src, s.g}][s.dest] += s.qty;
        else cship[{s.corp, s.src, s.g}][s.dest] += s.qty;
    }
    const auto get = [](const pool_map& m, const pool_key& k, std::size_t g) {
        const auto it = m.find(k);
        return it == m.end() ? 0.0f : it->second[g];
    };

    // ---- the ledger per (M, g) ----
    struct ledger
    {
        std::array<double, c_count> u{};
        std::array<double, k_ccount> by{};
        std::array<double, k_scount> sby{};
        double atM_sold = 0;
        struct prodr { pool_key pk; float q; std::array<double, c_count> f{}; int cc = -1; };
        std::vector<prodr> producers;
    };
    std::map<std::pair<entity_id, std::size_t>, ledger> L;
    for (const pinfo& s : starved)
    {
        const auto key = std::make_pair(s.m, s.g);
        if (L.count(key)) continue;
        ledger& l = L[key];
        const entity_id M = s.m;
        const std::size_t g = s.g;
        std::map<entity_id, double> sold_at; // X -> sold-at-home units (for shelf re-label)
        std::map<entity_id, std::array<double, k_ccount>> sold_by;
        for (const auto& [pk, a] : prod)
        {
            if (!(a[g] > 0.0f) || pool_key_body(w, pk.second) != s.body) continue;
            const entity_id X = pk.second;
            const float q = a[g];
            const float P0 = get(t.p0, pk, g), P1 = get(t.p1, pk, g), P2 = get(t.p2, pk, g);
            float toM = 0.0f, other = 0.0f;
            if (auto it = cship.find({pk.first, X, g}); it != cship.end())
                for (const auto& [d, qq] : it->second) (d == M ? toM : other) += qq;
            const double ship = toM + other;
            const double cons = std::max(0.0, double(P0) + q - ship - P1);
            const double sold = std::max(0.0f, P1 - P2);
            const double held = std::max(0.0f, P2);
            const double tot = cons + ship + sold + held;
            if (!(tot > 0.0)) continue;
            const double k = q / tot;
            ledger::prodr pr; pr.pk = pk; pr.q = q;
            if (X == M)
            {
                pr.f[c_atM] = k * (cons + toM + sold + held);
                pr.f[c_else] = k * other;
                l.atM_sold += k * sold;
            }
            else
            {
                pr.f[c_local] = k * (cons + held);
                pr.f[c_else] = k * other;
                pr.f[c_toM] = k * toM;
                pr.f[c_refused] = k * sold;
                const auto ci = t.corp_cls.find({pk.first, X, M, g});
                pr.cc = ci == t.corp_cls.end() ? k_reserved : ci->second;
                sold_at[X] += k * sold;
                sold_by[X][pr.cc] += k * sold;
            }
            for (int i = 0; i < c_count; ++i) l.u[i] += pr.f[i];
            l.producers.push_back(pr);
        }
        // shelf exports re-label sold-at-home units at X; exports off M's own shelf
        for (const auto& [X, sold] : sold_at)
        {
            double exp_toM = 0, exp_else = 0;
            if (auto it = sship.find({X, g}); it != sship.end())
                for (const auto& [d, qq] : it->second) (d == M ? exp_toM : exp_else) += qq;
            double moved = std::min(sold, exp_toM + exp_else);
            const double fM = (exp_toM + exp_else) > 0 ? exp_toM / (exp_toM + exp_else) : 0.0;
            l.u[c_refused] -= moved;
            l.u[c_toM] += moved * fM;
            l.u[c_else] += moved * (1.0 - fM);
            const double keep = sold > 0 ? (sold - moved) / sold : 0.0;
            for (int c = 0; c < k_ccount; ++c) l.by[c] += sold_by[X][c] * keep;
            const auto si = t.shelf_cls.find({X, M, g});
            const int sc = !w.markets.count(X) ? k_s_nomkt : (si == t.shelf_cls.end() ? k_s_nosurplus : si->second);
            l.sby[sc] += sold - moved;
        }
        if (auto it = sship.find({M, g}); it != sship.end())
        {
            double out = 0;
            for (const auto& [d, qq] : it->second) out += qq;
            const double mv = std::min(out, l.u[c_atM]);
            l.u[c_atM] -= mv; l.u[c_else] += mv;
            l.atM_sold = std::max(0.0, l.atM_sold - mv);
        }
        std::sort(l.producers.begin(), l.producers.end(), [](const ledger::prodr& a, const ledger::prodr& b) {
            return a.pk < b.pk; });
    }

    // ---- per (body, g) good rows (once per tick) ----
    std::set<std::pair<entity_id, std::size_t>> seen_bg;
    tally tick_t;
    int printed = 0;
    std::set<std::pair<entity_id, std::size_t>> sampled;
    logistics_nodes nodes = collect_logistics_nodes(w);
    for (const pinfo& s : starved)
    {
        const auto bg = std::make_pair(s.body, s.g);
        const float bp = body_prod.count(bg) ? body_prod[bg] : 0.0f;
        const float bw = body_want.count(bg) ? body_want[bg] : 0.0f;
        good_tally& gt = R.goods[s.g];
        if (seen_bg.insert(bg).second)
        {
            gt.body_prod += bp; gt.body_want += bw; gt.mkt_demand += body_mdem.count(bg) ? body_mdem[bg] : 0.0f;
            ++gt.bodies; if (bp < bw) ++gt.bodies_deficit;
        }
        const double U = s.need;
        const double d = (bw > 0.0f && bp < bw) ? 1.0 - double(bp) / bw : 0.0;
        const ledger& l = L.at({s.m, s.g});
        double ltot = 0;
        for (int i = 0; i < c_count; ++i) if (i != c_deficit) ltot += std::max(0.0, l.u[i]);
        tally one;
        one.starved = 1; one.unmet = U;
        if (!(ltot > 0.0)) one.units[c_deficit] += U;
        else
        {
            one.units[c_deficit] += U * d;
            const double rest = U * (1.0 - d);
            for (int i = 0; i < c_count; ++i) if (i != c_deficit) one.units[i] += rest * std::max(0.0, l.u[i]) / ltot;
            one.atM_sold += rest * std::max(0.0, l.atM_sold) / ltot;
            double bysum = 0, sbysum = 0;
            for (int c = 0; c < k_ccount; ++c) bysum += l.by[c];
            for (int c = 0; c < k_scount; ++c) sbysum += l.sby[c];
            for (int c = 0; c < k_ccount; ++c) if (bysum > 0) one.refused_by[c] += one.units[c_refused] * l.by[c] / bysum;
            for (int c = 0; c < k_scount; ++c) if (sbysum > 0) one.refused_shelf[c] += one.units[c_refused] * l.sby[c] / sbysum;
        }
        int prim = c_deficit;
        if (!(bp < bw) && ltot > 0.0)
        {
            double best = -1;
            for (int i = 0; i < c_count; ++i) if (i != c_deficit && l.u[i] > best) { best = l.u[i]; prim = i; }
        }
        one.primary[prim] = 1;
        tick_t.add(one);
        gt.t.add(one);

        // nearest producers by route cost (legs are price-independent)
        struct nr { double cost; const ledger::prodr* p; convoy_leg leg; };
        std::vector<nr> near;
        for (const auto& pr : l.producers)
        {
            convoy_leg leg;
            if (pr.pk.second != s.m)
                leg = price_convoy_leg(w, reg, nodes, pr.pk.first, pr.pk.second, s.m, s.g, 1.0f,
                                       reg.logistics_cost(convoy_mode::space));
            const double c = pr.pk.second == s.m ? 0.0 : (leg.viable ? leg.cost : 1e30);
            near.push_back({c, &pr, leg});
        }
        std::stable_sort(near.begin(), near.end(), [](const nr& a, const nr& b) { return a.cost < b.cost; });
        const float pM = t.price_at.count({s.m, s.g}) ? t.price_at.at({s.m, s.g}) : 0.0f;
        double local_q = 0, local_stay = 0;
        std::vector<const nr*> remote;
        for (const nr& n : near)
        {
            if (n.p->pk.second == s.m) { local_q += n.p->q; local_stay += n.p->f[c_atM]; }
            else remote.push_back(&n);
        }
        if (local_q > 0) ++R.nt.has_local;
        if (remote.empty()) ++R.nt.no_remote;
        else
        {
            const nr& n0 = *remote[0];
            const int nc = !n0.leg.viable ? 3 : n0.leg.mode == convoy_mode::land ? 0 : n0.leg.mode == convoy_mode::sea ? 1 : 2;
            ++R.nt.nearest_cls[nc];
            if (n0.leg.viable && pM > 0.0f) { R.nt.haul_over_price += n0.leg.cost / pM; R.nt.haul += n0.leg.cost; ++R.nt.haul_n; }
        }
        if (printed < samples && sampled.insert({s.m, s.g}).second)
        {
            ++printed;
            const auto inf = t.inflight_to.find({s.m, s.g});
            std::printf("    proc %u @mkt %u: %s want %.1f/t price %.1f | body prod %.1f want %.1f | in flight to M %.1f\n",
                        unsigned(s.bid), unsigned(s.m), gname(s.g).c_str(), s.need, pM, bp, bw,
                        inf == t.inflight_to.end() ? 0.0f : inf->second);
            std::printf("      made AT M: %.1f/t (stays at M %.1f)\n", local_q, local_stay);
            for (std::size_t i = 0; i < remote.size() && i < 3; ++i)
            {
                const auto& n = *remote[i];
                const auto& f = n.p->f;
                std::printf("      mkt %u corp %u made %.1f: %s cost %.2f/u (%.0f%% of price, %d t) -> atM %.1f local %.1f else %.1f sold %.1f toM %.1f%s%s\n",
                            unsigned(n.p->pk.second), unsigned(n.p->pk.first), n.p->q,
                            n.p->pk.second == s.m ? "local" : (n.leg.viable ? mode_name(n.leg.mode) : "NOROUTE"),
                            n.leg.viable ? n.leg.cost : 0.0f, (n.leg.viable && pM > 0) ? 100.0 * n.leg.cost / pM : 0.0,
                            n.leg.viable ? n.leg.travel_ticks : 0, f[c_atM], f[c_local], f[c_else], f[c_refused], f[c_toM],
                            n.p->cc >= 0 ? " rule " : "", n.p->cc >= 0 ? k_ccls[n.p->cc] : "");
            }
        }
    }
    char lbl[64];
    std::snprintf(lbl, sizeof lbl, "seed %u t%d", seed, tick);
    print_tally(lbl, tick_t);
    seed_t.add(tick_t);
    R.all.add(tick_t);
}

void run_seed(std::uint32_t seed, int ticks, int samples, run_out& R)
{
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, wp, *start); }
    catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); return; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    std::printf("\n=== seed %u (background_demand.consumes=%s) ===\n", seed,
                reg.background_demand().consumes ? "true" : "false");

    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        run_settle_tick(w, reg, step, 0, true);
    seat_player_corporation(w, seed, start->land.search.winner_score);

    probe p;
    p.reg = &reg;
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
    {
        if (!std::strcmp(k_campaign_settle_lap_names[i], "convoys")) p.lap_conv = i;
        if (!std::strcmp(k_campaign_settle_lap_names[i], "run_economy_step")) p.lap_econ = i;
        if (!std::strcmp(k_campaign_settle_lap_names[i], "clear_markets")) p.lap_clear = i;
    }
    for (const convoy_component& c : w.convoys) p.max_id = std::max(p.max_id, c.id);
    tally seed_t;
    constexpr int k_days = 90;
    for (int k = 1; k <= ticks; ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        const bool sel = std::find(std::begin(k_sel_ticks), std::end(k_sel_ticks), k) != std::end(k_sel_ticks);
        tick_read t;
        p.armed = sel;
        p.cur = &t;
        settle_tick_hooks hooks;
        hooks.after_lap = after_lap;
        hooks.ctx = &p;
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, &hooks);
        if (sel) analyse_tick(w, reg, res.report, t, k, seed, samples, R, seed_t);
        for (const convoy_component& c : w.convoys) p.max_id = std::max(p.max_id, c.id);
        std::fflush(stdout);
    }
    char lbl[64];
    std::snprintf(lbl, sizeof lbl, "SEED %u (t10+t25+t50)", seed);
    print_tally(lbl, seed_t);
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 60, samples = 4;
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(50, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--samples") && i + 1 < argc) samples = std::atoi(argv[++i]);
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
        else { std::fprintf(stderr, "usage: inflow_probe [--seeds a,b] [--ticks N] [--samples N]\n"); return 2; }
    }
    std::printf("inflow_probe - BL-1217 wave 2: where a starved processor's scarcest input is made, and why it does not arrive\n");
    run_out R;
    for (const std::uint32_t s : seeds) run_seed(s, ticks, samples, R);

    std::printf("\n================ POOLED over %zu seeds ================\n", seeds.size());
    print_tally("ALL", R.all);
    std::vector<std::pair<double, std::size_t>> order;
    for (const auto& [g, gt] : R.goods) order.push_back({gt.t.unmet, g});
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    {
        double bp = 0, bw = 0; int nd = 0, nb = 0;
        for (const auto& [g, gt] : R.goods) { bp += gt.body_prod; bw += gt.body_want; nd += gt.bodies_deficit; nb += gt.bodies; }
        std::printf("\n  BODY BALANCE over every (body, starved input, read): production %.1f vs processor want %.1f (%.0f%%); deficit in %d of %d\n",
                    bp, bw, bw > 0 ? 100.0 * bp / bw : 0.0, nd, nb);
    }
    std::printf("\n  BY GOOD (top 8 by unmet want); body sums are over distinct (body, read):\n");
    for (std::size_t i = 0; i < order.size() && i < 8; ++i)
    {
        const good_tally& gt = R.goods[order[i].second];
        std::printf("  %-18s starved %3d unmet %7.1f | body prod %8.1f vs proc want %8.1f (%.0f%%) mkt demand %8.1f | deficit bodies %d/%d\n",
                    gname(order[i].second).c_str(), gt.t.starved, gt.t.unmet, gt.body_prod, gt.body_want,
                    gt.body_want > 0 ? 100.0 * gt.body_prod / gt.body_want : 0.0, gt.mkt_demand, gt.bodies_deficit, gt.bodies);
        print_tally(gname(order[i].second).c_str(), gt.t);
    }
    const near_tally& n = R.nt;
    std::printf("\n  PRODUCERS of the starved input (per starved processor, %d): one AT M %d | nearest OFF-M producer: land %d sea %d space/air %d noroute %d, none on body %d | mean haul %.2f/u = %.1f%% of the price at M (over %d routed)\n",
                R.all.starved, n.has_local, n.nearest_cls[0], n.nearest_cls[1], n.nearest_cls[2], n.nearest_cls[3], n.no_remote,
                n.haul_n ? n.haul / n.haul_n : 0.0, n.haul_n ? 100.0 * n.haul_over_price / n.haul_n : 0.0, n.haul_n);
    return 0;
}
