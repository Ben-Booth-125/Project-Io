// ---------------------------------------------------------------------------
// sell_probe — BL-1227 (idle mines), follow-up diagnosis: could they sell elsewhere?
// ---------------------------------------------------------------------------
// QUESTION. idle_mines_probe found mines decommissioned in play overwhelmingly
// "wages tip it" while ANOTHER MARKET ON THE SAME BODY pays >= 1.5x the home
// price for their output. Two candidate causes:
//   (A) the owner's corporation dispatcher does not ship that output to the
//       better market — which rule refuses it;
//   (B) the loss reflex / idle verb values the mine at its HOME price
//       (estimate_building_profit: output x the tile market's price of the
//       primary), ignoring what dispatch would net.
//
// THE WORLD. Seated exactly as idle_mines_probe seats it: build_app_start_world,
// the 12-tick settle, seat_player_corporation, then PLAY ticks.
//
// A PURE READER. Two reads per tick, nothing written (the router's path caches
// are warmed through a const_cast — a cache fill computes the answer the next
// call would):
//   lap 0 (convoys)   which extraction sites are decommissioned before the
//                     economy step (so a transition can be seen at lap 1);
//                     the convoy id high-water mark.
//   lap 1 (economy+dispatch)  for every live extraction site in a loss streak
//                     (loss_streak >= 1) or decommissioned THIS tick (the idle
//                     verb zeroes the streak), the dispatcher's rules for the
//                     owner's (corp, pool key, primary) pool re-applied at the
//                     state dispatch_convoys saw: last clear's prices (no clear
//                     has run), the order floor (dispatch_source_price), the
//                     processor reservation, this tick's arrivals, the leg, and
//                     the destination's room. Room is read PRE-PASS: the
//                     post-pass dispatch_pending less this tick's convoys bound
//                     for the destination, plus this tick's convoys out of the
//                     destination's own pools (their excess sat there pre-pass);
//                     "room seen" adds back only the convoys committed BEFORE
//                     this pool in the dispatcher's walk (lower corp id, or the
//                     same corp and a lower pool key).
//   after the tick    the site's row: output (report), maintenance and wages
//                     (estimate_building_profit — the reflex's own figures),
//                     revenue at the home price the reflex read (lap-1 price),
//                     and revenue at the dispatcher's net.
//
// THE BEST MARKET is the highest posted price for the primary among the
// markets on the site's body (absolute price, as dispatch compares). The
// dispatcher's CLASS for shipping the pool to it:
//   nopool     the pool holds none of the good
//   reserved   no surplus above the processor reservation (+ arrivals)
//   home_best  the home market IS the best on the body
//   gate       price_best <= price_src x (1 + margin)
//   noroute    no viable leg
//   costly     price_best - haul - price_src <= margin x price_src
//   sent       units shipped to the best market this tick
//   outranked  passed, room, but shipped to another (better-net) market
//   noroom     passed, no room as seen (another market may have taken it)
//   lp         passed, room, nothing sent, corp solvent (the passive-LP cap)
//   solv       passed, room, nothing sent, corp balance < the leg's cost
//
// REVENUE AT THE DISPATCHER'S NET:
//   potential  output x max(price_src, the best net over every destination
//              that clears the dispatcher's gate and margin) — no room limit
//   realised   output x the pool's blended unit price this tick: units shipped
//              at (price_d - haul), the rest of the pool at home
//
// CAUSE POOL per decommission event (window = the classified rows among the
// last 8, the decommission tick included — the reflex judged that tick's row):
//   A  the dispatcher did not ship it: shipped < 50% of the pool's pre-pass
//      surplus over the window (attributed to the modal class to the best market)
//   B  the reflex's price mismatches: mean net at the home price < 0, but
//      mean net at the ROOM-LIMITED price >= 0 — every gate-passing candidate's
//      room (as seen) filled in net order from the pool's surplus, the rest at
//      home (multi-destination: an upper bound on the one-destination pass).
//      The flip at the POTENTIAL net (no room limit) is counted beside it.
// Revenue throughout follows the reflex's own convention: the site's whole
// output_quantity (the co-extraction basket) x the primary's unit price.
//   both / neither
// and for the best market: route class (leg viable, mode), short (last clear's
// demand > supply), deep (pre-pass absorbable at landed >= the pool's surplus).
//
// Usage: build_gen/verify/sell_probe.exe [--seeds a,b] [--ticks N] [--examples N]
// Build:  ./tools/verify/build_lua_harness.sh sell_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/budget_system.hpp"
#include "world/building_profit.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {

std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

enum dcls : int { k_nopool = 0, k_reserved, k_homebest, k_gate, k_noroute, k_costly, k_sent, k_outranked, k_noroom,
                  k_lp, k_solv, k_dcount };
const char* k_dname[k_dcount] = {"nopool", "reserved", "home_best", "gate", "noroute", "costly", "sent",
                                 "outranked", "noroom", "lp", "solv"};

enum pc : int { p_nopool = 0, p_reserved, p_nocand, p_sent, p_noroom, p_lp, p_solv, p_count };
const char* k_pname[p_count] = {"nopool", "reserved", "no candidate", "sent", "noroom (all)", "lp", "solv"};

enum cause : int { c_A = 0, c_B, c_both, c_neither, c_count };
const char* k_cname[c_count] = {"(A) dispatcher did not ship", "(B) reflex price mismatch only", "both A and B",
                                "neither (ships, still loses)"};

const char* mode_name(convoy_mode m)
{
    switch (m)
    {
    case convoy_mode::land: return "land";
    case convoy_mode::sea: return "sea";
    case convoy_mode::air: return "air";
    case convoy_mode::space: return "space";
    }
    return "?";
}

/// The lap-1 dispatch read for one (corp, pool key, good).
struct disp
{
    int cls = k_nopool;
    float home = 0, src = 0;          ///< home posted price; dispatcher's source price (floor-aware)
    bool floor_binds = false;
    entity_id best = null_entity;     ///< highest posted price on the body
    float best_px = 0;
    float pool_pre = 0, surplus = 0;  ///< pool qty pre-pass (post + shipped), surplus above reservation+arrivals
    float shipped = 0, to_best = 0;   ///< units out of this pool this tick
    float ship_net_sum = 0;           ///< sum over shipped units of (price_d - haul)
    entity_id ship_dest = null_entity;
    bool leg_ok = false; convoy_mode mode = convoy_mode::land; float haul = 0; float net_best = 0;
    float pot_net = 0;                ///< best net over every gate-passing destination (0 if none)
    entity_id pot_dest = null_entity;
    float absorb = 0, room_pre = 0, room_seen = 0;
    float best_demand = 0, best_supply = 0, best_inv = 0;
    float blended = 0;                ///< realised blended unit price of the pool
    int pcls = 0;                     ///< the dispatcher's outcome over its whole candidate list
    int ncand_body = 0, ncand_off = 0;
    int cand_demand = 0, cand_room = 0;   ///< candidates with last-clear demand > 0; with room as seen
    float room_fill = 0, blended_room = 0; ///< units placeable in candidates' room; the pool's unit price then
};

struct row
{
    int tick = 0;
    bool has_disp = false, has_rep = false;
    disp d;
    float output = 0, maint = 0, wages = 0;
    int streak = 0;
    float rev_home() const { return output * d.home; }
    float rev_pot() const { return output * std::max(d.home, d.pot_net); }
    float rev_real() const { return output * d.blended; }
    float rev_room() const { return output * d.blended_room; }
};

struct site
{
    entity_id bid = null_entity, corp = null_entity, key = null_entity, home = null_entity, body = null_entity;
    std::size_t g = 0;
    bool dec0 = false;
    std::deque<row> hist;
    row pending; bool pending_set = false;
};

struct event_rec
{
    std::uint32_t seed = 0;
    entity_id bid = null_entity;
    int tick = 0, by = 0; // 0 reflex, 1 verb
    std::size_t g = 0;
    bool owner_player = false;
    std::vector<row> win;
    // derived
    int cause = c_neither, modal = k_nopool, pmodal = p_nopool;
    int reality = 0; // 0 untraded (D = S = 0 at best), 1 short (D > S), 2 not short (D <= S, traded)
    double pot_net = 0, ship_frac = 0;
    double out = 0, home = 0, best = 0, rev_home = 0, rev_pot = 0, rev_real = 0, rev_room = 0, maint = 0, wages = 0;
    double room_fill = 0; bool cand_dem_any = false, cand_room_any = false, B_pot = false;
    double surplus = 0, shipped = 0, to_best = 0, absorb = 0, room_pre = 0, room_seen = 0, dem = 0, sup = 0;
    bool far15 = false, short_ = false, deep = false, leg_ok = false, home_is_best = false;
    convoy_mode mode = convoy_mode::land;
    bool keymismatch = false;
};

struct probe
{
    int lap_conv = -1, lap_econ = -1;
    int tick = 0;
    std::uint32_t seed = 0;
    std::uint32_t max_id = 0;
    const recipe_registry* reg = nullptr;
    std::map<entity_id, site> sites;
    std::vector<event_rec>* events = nullptr;
    long classified = 0;
};

std::map<entity_id, entity_id> owner_map(const world& w)
{
    std::map<entity_id, entity_id> o;
    for (const auto& [cid, cc] : w.corporations) for (const entity_id a : cc.assets) o.emplace(a, cid);
    return o;
}

/// The dispatcher's rules for (corp, key, g), re-applied after the pass.
disp classify(world& w, const recipe_registry& reg, const logistics_nodes& nodes, const order_floor_map& floors,
              const std::vector<entity_id>& corp_ids, reservation_memo& memo, const std::vector<convoy_component>& fresh,
              entity_id corp, entity_id key, std::size_t g)
{
    disp d;
    const float margin = reg.dispatch_margin();
    const entity_id body = pool_key_body(w, key);
    d.home = dispatch_home_price(w, key, g);
    d.src = dispatch_source_price(w, floors, corp, key, g);
    d.floor_binds = d.src > d.home;
    // best posted price on the body
    std::vector<entity_id> mids;
    for (const auto& [mid, m] : w.markets) mids.push_back(mid);
    std::sort(mids.begin(), mids.end());
    for (const entity_id mid : mids)
    {
        const market_component& m = w.markets.at(mid);
        if (m.body != body) continue;
        const float px = dispatch_market_price(m, g);
        if (px > d.best_px) { d.best_px = px; d.best = mid; }
    }
    // shipped this tick from this pool
    for (const convoy_component& c : fresh)
    {
        if (c.corp != corp || c.source_market != key || static_cast<std::size_t>(c.cargo_resource) != g) continue;
        d.shipped += c.cargo_qty;
        if (c.dest_market == d.best) d.to_best += c.cargo_qty;
        d.ship_dest = c.dest_market;
    }
    const float post = w.pool_at(corp, key).quantities[g];
    d.pool_pre = post + d.shipped;
    auto rit = memo.find({corp, key});
    if (rit == memo.end()) rit = memo.emplace(std::make_pair(corp, key), processor_reservation(w, reg, corp, key)).first;
    d.surplus = d.pool_pre - rit->second[g] - dispatch_arrived(w, corp, key, g);
    const float gate = d.src + margin * d.src;
    // room at a destination, pre-pass and as this pool saw it in the walk
    struct roomr { float absorb, pre, seen; };
    const auto room_at = [&](entity_id dest, float landed) {
        roomr rr{dispatch_absorbable(w, reg, dest, g, landed), 0.0f, 0.0f};
        if (!(rr.absorb > 0.0f)) return rr;
        if (!std::isfinite(rr.absorb)) { rr.pre = rr.seen = 1e30f; return rr; }
        float pend = dispatch_pending(w, reg, dest, g, corp_ids, memo);
        float before = 0.0f;
        for (const convoy_component& c : fresh)
        {
            if (static_cast<std::size_t>(c.cargo_resource) != g) continue;
            if (c.dest_market == dest)
            {
                pend -= c.cargo_qty;
                if (c.corp < corp || (c.corp == corp && c.source_market < key)) before += c.cargo_qty;
            }
            if (c.source_market == dest) pend += c.cargo_qty; // that excess sat at the destination pre-pass
        }
        pend = std::max(0.0f, pend);
        rr.pre = std::max(0.0f, rr.absorb - pend);
        rr.seen = std::max(0.0f, rr.absorb - pend - before);
        return rr;
    };
    // the potential: the dispatcher's candidate set (gate, viable leg, margin), argmax net
    const float probe_qty = d.surplus > 0.0f ? std::min(d.surplus, 1.0f) : 1.0f;
    std::vector<std::tuple<float, entity_id, float>> cands; // net, dest, haul
    for (const entity_id mid : mids)
    {
        if (mid == key) continue;
        const float pd = dispatch_market_price(w.markets.at(mid), g);
        if (!(pd > gate)) continue;
        const convoy_leg leg = price_convoy_leg(w, reg, nodes, corp, key, mid, g, probe_qty,
                                                reg.logistics_cost(convoy_mode::space));
        if (!leg.viable) continue;
        const float net = pd - leg.cost / probe_qty;
        if (!(net - d.src > margin * d.src)) continue;
        cands.emplace_back(net, mid, leg.cost / probe_qty);
        if (w.markets.at(mid).body == body) ++d.ncand_body; else ++d.ncand_off;
    }
    std::sort(cands.begin(), cands.end(), [](const auto& a, const auto& b) {
        return std::get<0>(a) != std::get<0>(b) ? std::get<0>(a) > std::get<0>(b) : std::get<1>(a) < std::get<1>(b);
    });
    if (!cands.empty()) { d.pot_net = std::get<0>(cands[0]); d.pot_dest = std::get<1>(cands[0]); }
    // room-limited: fill every candidate's room (as seen) in net order with the
    // surplus; the rest of the pool sells at home. Multi-destination, so an upper
    // bound on what the one-destination pass could realise under its own rules.
    {
        float remaining = std::max(0.0f, d.surplus), fill_rev = 0.0f;
        for (const auto& c : cands)
        {
            const market_component& cm = w.markets.at(std::get<1>(c));
            if (cm.demand[g] > 0.0f) ++d.cand_demand;
            const roomr rr = room_at(std::get<1>(c), d.src + std::get<2>(c));
            if (rr.seen > 0.0f) ++d.cand_room;
            const float take = std::min(remaining, rr.seen);
            if (take > 0.0f) { fill_rev += take * std::get<0>(c); remaining -= take; }
        }
        d.room_fill = std::max(0.0f, d.surplus) - remaining;
        d.blended_room = d.pool_pre > 0.0f ? (fill_rev + (d.pool_pre - d.room_fill) * d.home) / d.pool_pre : d.home;
    }
    // the dispatcher's own outcome for the pool (over its whole candidate list)
    if (!(d.pool_pre > 0.0f)) d.pcls = p_nopool;
    else if (!(d.surplus > 0.0f)) d.pcls = p_reserved;
    else if (cands.empty()) d.pcls = p_nocand;
    else if (d.shipped > 0.0f) d.pcls = p_sent;
    else
    {
        d.pcls = p_noroom;
        for (const auto& c : cands)
        {
            const roomr rr = room_at(std::get<1>(c), d.src + std::get<2>(c));
            if (rr.seen > 0.0f)
            {
                const float bal = w.corporations.count(corp) ? w.corporations.at(corp).balance : 0.0f;
                d.pcls = bal < std::get<2>(c) * std::min(d.surplus, rr.seen) ? p_solv : p_lp;
                break;
            }
        }
    }
    // realised blended price: shipped units at their net, the rest at home
    for (const convoy_component& c : fresh)
    {
        if (c.corp != corp || c.source_market != key || static_cast<std::size_t>(c.cargo_resource) != g) continue;
        const float pd = dispatch_market_price(w.markets.at(c.dest_market), g);
        const float net = c.cargo_qty > 0.0f ? pd - c.cost_paid / c.cargo_qty : pd;
        d.ship_net_sum += c.cargo_qty * net;
    }
    d.blended = d.pool_pre > 0.0f ? (d.ship_net_sum + (d.pool_pre - d.shipped) * d.home) / d.pool_pre : d.home;
    // best market: route and room, read whatever the class
    if (d.best != null_entity && d.best != key)
    {
        const market_component& bm = w.markets.at(d.best);
        d.best_demand = bm.demand[g]; d.best_supply = bm.supply[g]; d.best_inv = bm.inventory[g];
        const convoy_leg leg = price_convoy_leg(w, reg, nodes, corp, key, d.best, g, probe_qty,
                                                reg.logistics_cost(convoy_mode::space));
        d.leg_ok = leg.viable; d.mode = leg.mode;
        d.haul = leg.viable ? leg.cost / probe_qty : 0.0f;
        d.net_best = d.best_px - d.haul;
        const roomr rr = room_at(d.best, d.src + d.haul);
        d.absorb = rr.absorb; d.room_pre = rr.pre; d.room_seen = rr.seen;
    }
    // the class
    if (!(d.pool_pre > 0.0f)) d.cls = k_nopool;
    else if (!(d.surplus > 0.0f)) d.cls = k_reserved;
    else if (d.best == null_entity || d.best == key) d.cls = k_homebest;
    else if (!(d.best_px > gate)) d.cls = k_gate;
    else if (!d.leg_ok) d.cls = k_noroute;
    else if (!(d.net_best - d.src > margin * d.src)) d.cls = k_costly;
    else if (d.to_best > 0.0f) d.cls = k_sent;
    else if (d.shipped > 0.0f) d.cls = d.room_seen > 0.0f ? k_outranked : k_noroom;
    else if (!(d.room_seen > 0.0f)) d.cls = k_noroom;
    else
    {
        const float bal = w.corporations.count(corp) ? w.corporations.at(corp).balance : 0.0f;
        d.cls = bal < d.haul * std::min(d.surplus, d.room_seen) ? k_solv : k_lp;
    }
    return d;
}

void after_lap(const world& cw, int lap, void* ctx)
{
    auto* p = static_cast<probe*>(ctx);
    if (lap == p->lap_conv)
    {
        for (const auto& [bid, b] : cw.buildings)
        {
            if (b.type != building_type::extraction_site) continue;
            auto it = p->sites.find(bid);
            if (it == p->sites.end()) continue;
            it->second.dec0 = b.decommissioned;
        }
        std::uint32_t mx = 0;
        for (const convoy_component& c : cw.convoys) mx = std::max(mx, c.id);
        p->max_id = std::max(p->max_id, mx);
        return;
    }
    if (lap != p->lap_econ) return;
    world& w = const_cast<world&>(cw);
    const recipe_registry& reg = *p->reg;
    std::vector<convoy_component> fresh;
    for (const convoy_component& c : w.convoys) if (c.id > p->max_id) fresh.push_back(c);
    const logistics_nodes nodes = collect_logistics_nodes(w);
    const order_floor_map floors = collect_order_floors(w);
    std::vector<entity_id> corp_ids;
    for (const auto& kv : w.corporations) corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    reservation_memo memo;
    std::map<std::tuple<entity_id, entity_id, std::size_t>, disp> cache;
    const auto owners = owner_map(w);
    for (auto& [bid, s] : p->sites)
    {
        s.pending_set = false;
        const auto bi = w.buildings.find(bid);
        if (bi == w.buildings.end()) continue;
        const building_component& b = bi->second;
        if (b.ticks_remaining > 0) continue;
        const bool dec_now = !s.dec0 && b.decommissioned;
        if (b.decommissioned && !dec_now) continue;
        if (!(b.loss_streak >= 1) && !dec_now) continue;
        const auto oi = owners.find(bid);
        if (oi == owners.end()) continue;
        s.corp = oi->second;
        s.key = pool_key_for_tile(w, b.tile);
        s.home = market_for_tile(w, b.tile);
        s.g = static_cast<std::size_t>(b.target_resource);
        s.body = pool_key_body(w, s.key);
        const auto k = std::make_tuple(s.corp, s.key, s.g);
        auto ci = cache.find(k);
        if (ci == cache.end())
        {
            ci = cache.emplace(k, classify(w, reg, nodes, floors, corp_ids, memo, fresh, s.corp, s.key, s.g)).first;
            ++p->classified;
        }
        s.pending.tick = p->tick;
        s.pending.has_disp = true;
        s.pending.d = ci->second;
        s.pending.streak = b.loss_streak;
        s.pending_set = true;
    }
}

void judge(event_rec& e)
{
    int n = 0;
    std::array<int, k_dcount> cnt{}; std::array<int, p_count> pcnt{};
    double net_home = 0, net_pot = 0;
    for (const row& r : e.win)
    {
        if (!r.has_disp || !r.has_rep) continue;
        ++n;
        ++cnt[r.d.cls]; ++pcnt[r.d.pcls]; e.pot_net += r.d.pot_net;
        e.out += r.output; e.home += r.d.home; e.best += r.d.best_px;
        e.rev_home += r.rev_home(); e.rev_pot += r.rev_pot(); e.rev_real += r.rev_real(); e.rev_room += r.rev_room();
        e.room_fill += r.d.room_fill; e.cand_dem_any |= r.d.cand_demand > 0; e.cand_room_any |= r.d.cand_room > 0;
        e.maint += r.maint; e.wages += r.wages;
        e.surplus += std::max(0.0f, r.d.surplus); e.shipped += r.d.shipped; e.to_best += r.d.to_best;
        e.absorb += std::isfinite(r.d.absorb) ? std::min(r.d.absorb, 1e6f) : 1e6f;
        e.room_pre += std::min(r.d.room_pre, 1e6f); e.room_seen += std::min(r.d.room_seen, 1e6f);
        e.dem += r.d.best_demand; e.sup += r.d.best_supply;
    }
    if (!n) { e.cause = -1; return; }
    const double N = n;
    e.out /= N; e.home /= N; e.best /= N; e.rev_home /= N; e.rev_pot /= N; e.rev_real /= N; e.rev_room /= N; e.room_fill /= N; e.maint /= N; e.wages /= N;
    e.surplus /= N; e.shipped /= N; e.to_best /= N; e.absorb /= N; e.room_pre /= N; e.room_seen /= N; e.dem /= N; e.sup /= N;
    e.modal = int(std::max_element(cnt.begin(), cnt.end()) - cnt.begin()); e.pot_net /= N;
    e.pmodal = int(std::max_element(pcnt.begin(), pcnt.end()) - pcnt.begin());
    e.ship_frac = e.surplus > 0 ? e.shipped / e.surplus : 0;
    e.reality = (e.dem <= 0 && e.sup <= 0) ? 0 : (e.dem > e.sup ? 1 : 2);
    const row& last = e.win.back();
    e.leg_ok = last.d.leg_ok; e.mode = last.d.mode; e.home_is_best = last.d.best == null_entity || last.d.cls == k_homebest;
    e.far15 = e.home > 0 && e.best >= 1.5 * e.home;
    e.short_ = e.dem > e.sup;
    e.deep = e.absorb >= e.surplus && e.surplus > 0;
    net_home = e.rev_home - e.maint - e.wages;
    net_pot = e.rev_pot - e.maint - e.wages;
    const double net_room = e.rev_room - e.maint - e.wages;
    e.B_pot = net_home < 0 && net_pot >= 0;
    const bool A = e.surplus > 0 ? e.shipped < 0.5 * e.surplus : true;
    // B on the ROOM-LIMITED price: what the dispatcher's own rules could place
    const bool B = net_home < 0 && net_room >= 0;
    e.cause = A && B ? c_both : A ? c_A : B ? c_B : c_neither;
}

void after_tick(world& w, const recipe_registry& reg, const settle_tick_result& res, probe& p, int tick)
{
    const economy_report& rep = res.report;
    for (auto& [bid, s] : p.sites)
    {
        if (!s.pending_set) continue;
        s.pending_set = false;
        row r = s.pending;
        const auto ri = rep.building_row.find(bid);
        if (ri != rep.building_row.end() && ri->second < rep.buildings.size())
        {
            r.has_rep = true;
            r.output = rep.buildings[ri->second].output_quantity;
            const building_profit bp = estimate_building_profit(w, reg, rep, bid);
            r.maint = bp.maintenance; r.wages = bp.wages;
            // the decommission tick: the reflex judged the RUNNING opex, read now
            // after the flag flipped (idle floor, no wages) — carry the prior row's
            const auto bj = w.buildings.find(bid);
            if (bj != w.buildings.end() && bj->second.decommissioned && !s.hist.empty() && s.hist.back().has_rep)
            {
                r.maint = s.hist.back().maint; r.wages = s.hist.back().wages;
            }
        }
        s.hist.push_back(r);
        while (s.hist.size() > 8) s.hist.pop_front();
        const auto bi = w.buildings.find(bid);
        const bool dec_now = bi != w.buildings.end() && bi->second.decommissioned && !s.dec0;
        if (dec_now && tick >= 1)
        {
            event_rec e;
            e.seed = p.seed; e.bid = bid; e.tick = tick; e.g = s.g;
            e.by = bi->second.loss_streak >= 8 ? 0 : 1;
            e.owner_player = w.corporations.count(s.corp) && w.corporations.at(s.corp).is_player;
            e.keymismatch = s.key != s.home;
            // the window: rows within the last 8 ticks
            for (const row& h : s.hist) if (h.tick > tick - 8) e.win.push_back(h);
            judge(e);
            p.events->push_back(std::move(e));
        }
        if (dec_now) s.hist.clear();
    }
}

void scan(const world& w, probe& p)
{
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::extraction_site && !p.sites.count(bid))
        {
            site s; s.bid = bid; s.dec0 = b.decommissioned;
            p.sites.emplace(bid, s);
        }
}

void print_row(const row& r)
{
    std::printf("       t%-4d streak %d out %6.2f | home %6.2f src %6.2f%s best %6.2f (m%u) | cls %-9s pool %-12s cands %d+%d surplus %7.2f shipped %6.2f to-best %6.2f | leg %s haul %5.2f net-best %6.2f pot-net %6.2f | absorb %8.2f room pre %8.2f seen %8.2f | best D %6.2f S %6.2f | cands w/ demand %d w/ room %d fill %5.2f | rev home %6.2f pot %6.2f room %6.2f real %6.2f | maint %5.2f wages %5.2f\n",
                r.tick, r.streak, r.output, r.d.home, r.d.src, r.d.floor_binds ? "*" : " ", r.d.best_px, unsigned(r.d.best),
                k_dname[r.d.cls], k_pname[r.d.pcls], r.d.ncand_body, r.d.ncand_off, r.d.surplus, r.d.shipped, r.d.to_best, r.d.leg_ok ? mode_name(r.d.mode) : "none",
                r.d.haul, r.d.net_best, r.d.pot_net, std::min(r.d.absorb, 1e6f), std::min(r.d.room_pre, 1e6f),
                std::min(r.d.room_seen, 1e6f), r.d.best_demand, r.d.best_supply, r.d.cand_demand, r.d.cand_room, r.d.room_fill,
                r.rev_home(), r.rev_pot(), r.rev_room(), r.rev_real(),
                r.maint, r.wages);
}

struct pool_tally
{
    long n = 0;
    std::array<long, c_count> cause{};
    std::array<std::array<long, k_dcount>, c_count> modal{}; // by cause
    std::array<long, k_dcount> rows{}; std::array<std::array<long, p_count>, c_count> pmodal{}; std::array<long, 3> reality{}; double potn = 0, sfrac = 0;
    long far = 0, leg_ok = 0, short_ = 0, deep = 0, short_deep = 0, homebest = 0, reflex = 0, verb = 0, mism = 0, player = 0;
    std::map<std::string, long> mode;
    std::map<std::string, long> good;
    double rroom = 0, fill = 0; long cdem = 0, croom = 0, bpot = 0;
    double out = 0, home = 0, best = 0, rh = 0, rp = 0, rr = 0, mt = 0, wg = 0, sur = 0, shp = 0, ab = 0, rpre = 0, dem = 0, sup = 0;
    void add(const event_rec& e)
    {
        ++n; ++cause[e.cause]; ++modal[e.cause][e.modal]; ++pmodal[e.cause][e.pmodal]; ++reality[e.reality]; potn += e.pot_net; sfrac += e.ship_frac;
        for (const row& r : e.win) if (r.has_disp && r.has_rep) ++rows[r.d.cls];
        far += e.far15; leg_ok += e.leg_ok; short_ += e.short_; deep += e.deep; short_deep += e.short_ && e.deep;
        homebest += e.home_is_best; reflex += e.by == 0; verb += e.by == 1; mism += e.keymismatch; player += e.owner_player;
        if (e.leg_ok) ++mode[mode_name(e.mode)];
        ++good[gname(e.g)];
        out += e.out; home += e.home; best += e.best; rh += e.rev_home; rp += e.rev_pot; rr += e.rev_real;
        rroom += e.rev_room; fill += e.room_fill; cdem += e.cand_dem_any; croom += e.cand_room_any; bpot += e.B_pot;
        mt += e.maint; wg += e.wages; sur += e.surplus; shp += e.shipped; ab += e.absorb; rpre += e.room_pre;
        dem += e.dem; sup += e.sup;
    }
    void print(const char* label) const
    {
        std::printf("  %s: %ld events (reflex %ld, verb %ld; seat-owned %ld; pool key != home market %ld)\n", label, n,
                    reflex, verb, player, mism);
        if (!n) return;
        const double N = n;
        std::printf("    window means: out %.2f | home px %.2f best px %.2f | rev at home (the reflex's) %.2f, at dispatcher's potential net %.2f, room-limited %.2f, realised %.2f | maint %.2f wages %.2f\n",
                    out / N, home / N, best / N, rh / N, rp / N, rroom / N, rr / N, mt / N, wg / N);
        std::printf("    units placeable in candidates' room %.2f; events with a gate-passing candidate that has demand %ld, that has room %ld; flip at potential net (no room limit) %ld\n",
                    fill / N, cdem, croom, bpot);
        std::printf("    pool surplus %.2f shipped %.2f | best market: absorbable at landed %.2f, room pre-pass %.2f | best demand %.2f supply %.2f\n",
                    sur / N, shp / N, ab / N, rpre / N, dem / N, sup / N);
        std::printf("    best on body >= 1.5x home %ld; home is the best %ld; leg to best viable %ld", far, homebest, leg_ok);
        for (const auto& [m, c] : mode) std::printf(" [%s %ld]", m.c_str(), c);
        std::printf("; best short (D > S) %ld; deep (absorb >= surplus) %ld; short AND deep %ld\n", short_, deep, short_deep);
        std::printf("    CAUSE:");
        for (int c = 0; c < c_count; ++c) std::printf("  %s %ld", k_cname[c], cause[c]);
        std::printf("\n");
        for (int c = 0; c < c_count; ++c)
        {
            if (!cause[c]) continue;
            std::printf("      %-32s to-best class:", k_cname[c]);
            for (int k = 0; k < k_dcount; ++k) if (modal[c][k]) std::printf("  %s %ld", k_dname[k], modal[c][k]);
            std::printf("  | pool outcome:");
            for (int k = 0; k < p_count; ++k) if (pmodal[c][k]) std::printf("  %s %ld", k_pname[k], pmodal[c][k]);
            std::printf("\n");
        }
        std::printf("    best market's price: untraded (D = S = 0) %ld, short (D > S) %ld, traded not short %ld | mean pot-net %.2f | mean shipped/surplus %.2f\n",
                    reality[0], reality[1], reality[2], potn / N, sfrac / N);
        std::printf("    window rows by class:");
        for (int k = 0; k < k_dcount; ++k) if (rows[k]) std::printf("  %s %ld", k_dname[k], rows[k]);
        std::printf("\n    by good:");
        for (const auto& [g, c] : good) std::printf("  %s %ld", g.c_str(), c);
        std::printf("\n");
    }
};

void run_seed(std::uint32_t seed, int ticks, std::vector<event_rec>& events)
{
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, wp, *start); }
    catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); return; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    std::printf("\n=== seed %u === (dispatch margin %.3f)\n", seed, reg.dispatch_margin());
    probe p;
    p.reg = &reg; p.seed = seed; p.events = &events;
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
    {
        if (!std::strcmp(k_campaign_settle_lap_names[i], "convoys")) p.lap_conv = i;
        if (!std::strcmp(k_campaign_settle_lap_names[i], "run_economy_step")) p.lap_econ = i;
    }
    settle_tick_hooks hooks;
    hooks.after_lap = after_lap;
    hooks.ctx = &p;
    scan(w, p);
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        p.tick = step - k_campaign_settle_ticks;
        settle_tick_result res = run_settle_tick(w, reg, step, 0, true, &hooks);
        after_tick(w, reg, res, p, p.tick);
        scan(w, p);
    }
    seat_player_corporation(w, seed, start->land.search.winner_score);
    scan(w, p);
    constexpr int k_days = 90;
    for (int k = 1; k <= ticks; ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        p.tick = k;
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, &hooks);
        after_tick(w, reg, res, p, k);
        scan(w, p);
    }
    std::printf("  pools classified (site-ticks deduped per tick): %ld\n", p.classified);
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 200, examples = 2;
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--examples") && i + 1 < argc) examples = std::atoi(argv[++i]);
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
        else { std::fprintf(stderr, "usage: sell_probe [--seeds a,b] [--ticks N] [--examples N]\n"); return 2; }
    }
    std::printf("sell_probe - BL-1227: decommissioned mines that could sell elsewhere on their body\n");
    pool_tally all, far, all_seed;
    for (const std::uint32_t s : seeds)
    {
        std::vector<event_rec> ev;
        run_seed(s, ticks, ev);
        pool_tally t, tf;
        int shown_far = 0;
        for (event_rec& e : ev)
        {
            if (e.cause < 0) continue;
            t.add(e); all.add(e);
            if (e.far15) { tf.add(e); far.add(e); }
        }
        t.print("all decommission events in play");
        tf.print("subset: best on body >= 1.5x home");
        // examples: the first far events of distinct causes
        std::set<int> seen;
        for (const event_rec& e : ev)
        {
            if (e.cause < 0 || !e.far15 || shown_far >= examples || seen.count(e.cause)) continue;
            seen.insert(e.cause); ++shown_far;
            std::printf("  EXAMPLE site %u (%s), decommissioned t%d by %s: %s, modal class %s\n", unsigned(e.bid),
                        gname(e.g).c_str(), e.tick, e.by == 0 ? "loss reflex" : "idle verb", k_cname[e.cause],
                        k_dname[e.modal]);
            for (const row& r : e.win) print_row(r);
        }
    }
    std::printf("\n================ POOLED over %zu seeds, %d ticks ================\n", seeds.size(), ticks);
    all.print("all decommission events in play");
    far.print("subset: best on body >= 1.5x home");
    return 0;
}
