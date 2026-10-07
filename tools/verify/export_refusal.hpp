// ---------------------------------------------------------------------------
// export_refusal.hpp — the market-export dispatcher's refusal classifier, as a
// READER (BL-1223, gate logistics row; first written inside water_pair_probe for
// BL-1203). Header-only; shared by water_pair_probe and market_viability so the
// two tools can never disagree about why a surplus shelf did not ship to a
// market (BL-1165's haulage_measure lesson: one reader, not two copies).
// ---------------------------------------------------------------------------
// THE QUESTION. For a good G, a surplus market s and a dry market d: which rule
// of export_market_shelves (supply_system.cpp) refuses the pair? The classes,
// FIRST RULE FAILED, in the dispatcher's own order:
//   body     d is on another body (a market has no space lane)
//   gate     price_d <= price_s x (1 + dispatch_margin)
//   noroute  price_market_export_leg is not viable; sub-reason (noroute_why):
//            no path at all and < 2 active ports | < 2 active ports | no port the
//            source reaches overland | no port that reaches d overland | both
//            ends reach a port but no sea leg joins them
//   costly   routed (land or sea), but price_d - haul - price_s does not clear
//            the margin
//   noroom   dispatch_absorbable at the landed cost, less pending and d's shelf,
//            is <= 0
//   room     the rule would send (held back by the one-destination-per-pass
//            rule or the passive-LP cap, or sent elsewhere this tick)
// "BEST" class over every source: room > noroom > costly > gate > noroute > body
// (the enum's numeric order; higher is closer to sending).
//
// THE SETS (water_pair_probe's definitions, now for any good):
//   surplus source  base_price > 0, market_shelf_surplus >= 1 unit, and an
//                   anchored centre tile (an unanchored market cannot export)
//   consuming       base_price > 0 and the households bid the good
//   dry             consuming and the shelf holds < 1 unit
//
// A PURE READER. Every world call here is a const read, except the router's
// path caches (intra_body_path / intra_body_leg_path / body_active_port_tiles /
// price_market_export_leg take a world&), warmed through the caller's
// const_cast: a cache fill computes the answer the next call would compute; it
// never changes one, and those caches never evict. The reader is called at a
// fixed world state (an after_lap hook), so the per-(s, d) leg memo inside a
// `context` is exact: price_market_export_leg at qty 1 does not depend on G.
// No wall clock, no randomness; every iteration is over sorted ids.
// ---------------------------------------------------------------------------
#pragma once

#include "world/components.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace export_refusal {

enum cls : int { c_body = 0, c_noroute, c_gate, c_costly, c_noroom, c_room, c_count };
inline const char* const k_cls[c_count] = {"body", "noroute", "gate", "costly", "noroom", "room"};

enum noroute_why : int
{
    nr_none = -1,
    nr_nopath = 0,   ///< no overland path and fewer than 2 active ports
    nr_ports_lt2,    ///< fewer than 2 active ports on the body
    nr_no_port_src,  ///< no port the source reaches overland
    nr_no_port_dst,  ///< no port that reaches the destination overland
    nr_no_sea,       ///< both ends reach a port, no sea leg joins them
    nr_count
};

/// One probe's read state: the world at a fixed instant plus the dispatcher's
/// sorted id lists and memos. Build one per probe tick; never keep it across a
/// tick (the world moves).
struct context
{
    world&                       w;
    const recipe_registry&       reg;
    float                        margin;
    logistics_nodes              nodes;
    std::vector<entity_id>       corp_ids; ///< ascending (the dispatcher's order)
    std::vector<entity_id>       mids;     ///< every market, ascending
    reservation_memo             memo;
    std::map<std::pair<entity_id, entity_id>, convoy_leg> legs;    ///< (s, d) at qty 1
    std::map<std::pair<entity_id, entity_id>, int>        nr_why;  ///< (s, d) noroute sub-reason

    context(world& w_, const recipe_registry& reg_)
        : w(w_), reg(reg_), margin(reg_.dispatch_margin()), nodes(collect_logistics_nodes(w_))
    {
        for (const auto& kv : w.corporations) corp_ids.push_back(kv.first);
        for (const auto& kv : w.markets) mids.push_back(kv.first);
        std::sort(corp_ids.begin(), corp_ids.end());
        std::sort(mids.begin(), mids.end());
    }

    const convoy_leg& leg(entity_id s, entity_id d)
    {
        const auto key = std::make_pair(s, d);
        auto it = legs.find(key);
        if (it == legs.end())
            it = legs.emplace(key, price_market_export_leg(w, reg, nodes, s, d, 1.0f)).first;
        return it->second;
    }

    /// Why price_market_export_leg refused (s, d): water_pair_probe's split.
    int noroute_reason(entity_id s, entity_id d)
    {
        const auto key = std::make_pair(s, d);
        const auto it = nr_why.find(key);
        if (it != nr_why.end()) return it->second;
        const market_component& sm = w.markets.at(s);
        const market_component& dm = w.markets.at(d);
        const entity_id o = sm.centre_tile, dc = dm.centre_tile;
        const logistics_path& p = intra_body_path(w, sm.body, o, dc);
        const std::vector<entity_id> ports = body_active_port_tiles(w, sm.body);
        int why;
        if (!p.reachable && ports.size() < 2) why = nr_nopath;
        else if (ports.size() < 2) why = nr_ports_lt2;
        else
        {
            bool from_s = false, to_d = false;
            for (const entity_id pt : ports)
            {
                if (!from_s && intra_body_leg_path(w, sm.body, o, pt, leg_domain::land).reachable) from_s = true;
                if (!to_d && intra_body_leg_path(w, sm.body, pt, dc, leg_domain::land).reachable) to_d = true;
            }
            why = !from_s ? nr_no_port_src : !to_d ? nr_no_port_dst : nr_no_sea;
        }
        nr_why.emplace(key, why);
        return why;
    }
};

/// The dry-market predicate (water_pair_probe's, any good).
inline bool consuming(const market_component& mc, std::size_t G)
{
    return mc.base_price[G] > 0.0f && mc.household_bid[G] > 0.0f;
}
inline bool dry(const market_component& mc, std::size_t G)
{
    return consuming(mc, G) && mc.inventory[G] < 1.0f;
}

/// The surplus sources and dry markets of good G, ascending by market id.
struct good_markets
{
    std::vector<entity_id> sur, dry;
    long   consuming = 0;
    double surplus_units = 0.0;
};

inline good_markets scan_good(const context& x, std::size_t G)
{
    good_markets out;
    for (const entity_id m : x.mids)
    {
        const market_component& mc = x.w.markets.at(m);
        if (!(mc.base_price[G] > 0.0f)) continue;
        if (mc.household_bid[G] > 0.0f)
        {
            ++out.consuming;
            if (mc.inventory[G] < 1.0f) out.dry.push_back(m);
        }
        const float s = market_shelf_surplus(x.w, m, G);
        if (s >= 1.0f && mc.centre_tile != null_entity)
        {
            out.sur.push_back(m);
            out.surplus_units += s;
        }
    }
    return out;
}

/// One (source, destination) pair's verdict and the numbers behind it.
struct pair_result
{
    int         c = -1;
    int         why = nr_none;          ///< noroute only
    convoy_mode mode = convoy_mode::land;
    float       landed = 0.0f;          ///< costly / noroom / room: price_s + haul
    float       haul = 0.0f;            ///< routed: the leg's cost for one unit
    float       gap = 0.0f;             ///< routed: price_d - price_s
    float       absorb = 0.0f;          ///< noroom / room: dispatch_absorbable(landed)
    float       room = 0.0f;            ///< noroom / room: absorb - pending - shelf
};

/// The dispatcher's rules for one pair, in its own order. s != d.
inline pair_result classify_pair(context& x, entity_id s, entity_id d, std::size_t G)
{
    pair_result r;
    const market_component& sm = x.w.markets.at(s);
    const market_component& dm = x.w.markets.at(d);
    const float p_d = dispatch_market_price(dm, G);
    const float p_s = dispatch_market_price(sm, G);
    const float margin = x.margin;
    if (sm.body != dm.body) { r.c = c_body; return r; }
    if (!(p_d > p_s + margin * p_s)) { r.c = c_gate; return r; }
    const convoy_leg& leg = x.leg(s, d);
    if (!leg.viable)
    {
        r.c = c_noroute;
        r.why = x.noroute_reason(s, d);
        return r;
    }
    r.mode = leg.mode;
    const float haul = leg.cost;
    r.haul = haul;
    r.gap = p_d - p_s;
    r.landed = p_s + haul;
    if (!(p_d - haul - p_s > margin * p_s)) { r.c = c_costly; return r; }
    const float absorb = dispatch_absorbable(x.w, x.reg, d, G, r.landed);
    const float pend = dispatch_pending(x.w, x.reg, d, G, x.corp_ids, x.memo)
                     + std::max(0.0f, dm.inventory[G]);
    r.absorb = absorb;
    r.room = absorb - pend;
    r.c = !(absorb - pend > 0.0f) ? c_noroom : c_room;
    return r;
}

/// A destination's best class over every surplus source of G (-1: no source
/// other than itself), and the cheapest landed cost among its routed-and-priced
/// sources (costly / noroom / room). `on_pair(src, pair_result)` sees each pair
/// in source order.
struct best_result
{
    int   best = -1;
    float best_l = 0.0f;
};

template <class F>
best_result classify_destination(context& x, entity_id d, std::size_t G,
                                 const std::vector<entity_id>& sur, F&& on_pair)
{
    best_result b;
    for (const entity_id s : sur)
    {
        if (s == d) continue;
        const pair_result pr = classify_pair(x, s, d, G);
        on_pair(s, pr);
        const int c = pr.c;
        if (c >= c_costly && (b.best < c_costly || pr.landed < b.best_l)) b.best_l = pr.landed;
        if (b.best < 0 || c > b.best) b.best = c;
    }
    return b;
}

inline best_result classify_destination(context& x, entity_id d, std::size_t G,
                                        const std::vector<entity_id>& sur)
{
    return classify_destination(x, d, G, sur, [](entity_id, const pair_result&) {});
}

} // namespace export_refusal
