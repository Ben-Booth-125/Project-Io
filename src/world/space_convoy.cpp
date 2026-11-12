#include "space_convoy.hpp"

#include "components.hpp"
#include "orbital_graph.hpp"
#include "war.hpp"   // at_war
#include "space_battle.hpp"  // BL-1599: open_space_battle
#include "space_bombard.hpp" // BL-1599: blockade
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// BL-1596 — see space_convoy.hpp for the design. This is the arithmetic.

namespace {

constexpr std::size_t PR = static_cast<std::size_t>(resource_type::propellant);

/// The propellant lift of one unit of mass off (or down to) @p body: climb Δv x rate.
float climb_rate(const world& w, const space_unit_params& p, entity_id body)
{
    const double climb = orbital_climb_dv(w, orbital_graph_params{}, body);
    const float rate = std::isfinite(p.launch_propellant_per_dv) ? std::max(0.0f, p.launch_propellant_per_dv) : 0.0f;
    const float r = static_cast<float>(climb * rate);
    return std::isfinite(r) ? std::max(0.0f, r) : std::numeric_limits<float>::infinity();
}

bool carrier(space_hull h) { return h == space_hull::hauler || h == space_hull::lander; }

/// The lowest-id completed, standing, staffed Launchpad on @p body, any owner's.
/// `buildings` is unordered: the minimum id is a property of the ids alone.
entity_id staffed_pad_any(const world& w, entity_id body)
{
    entity_id best = null_entity;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::launchpad || b.ticks_remaining > 0 || b.decommissioned
            || !(b.workforce_assigned > 0.0f))
            continue;
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        if (best == null_entity || bid < best)
            best = bid;
    }
    return best;
}

/// The days a trip takes hop by hop, as `order_space_unit` counts each hop.
float hop_days(const world& w, const std::vector<orbital_node>& nodes)
{
    double days = 0.0;
    for (std::size_t i = 1; i < nodes.size(); ++i)
    {
        const orbital_path_result hop = orbital_path(w, orbital_graph_params{}, nodes[i - 1], nodes[i]);
        if (!hop.ok || !std::isfinite(hop.days))
            return std::numeric_limits<float>::infinity();
        days += std::max(1.0, std::ceil(hop.days));
    }
    return static_cast<float>(days);
}

/// The nation's lowest-id idle, empty, record-free Hauler at @p node.
entity_id idle_hauler_at(const world& w, entity_id nation, orbital_node node)
{
    for (const auto& [id, u] : w.space_units) // ascending
        if (u.owner == nation && u.hull == space_hull::hauler && u.state == space_unit_state::idle
            && u.node == node && !(u.cargo > 0.0f) && w.space_hauls.count(id) == 0
            && evaluate_space_design(u.hull, u.design).valid)
            return id;
    return null_entity;
}

void erase_convoy(world& w, uint32_t id)
{
    if (id == 0)
        return;
    w.convoys.erase(std::remove_if(w.convoys.begin(), w.convoys.end(),
                                   [id](const convoy_component& c) { return c.id == id; }),
                    w.convoys.end());
}

convoy_component* find_convoy(world& w, uint32_t id)
{
    if (id == 0)
        return nullptr;
    for (convoy_component& c : w.convoys)
        if (c.id == id)
            return &c;
    return nullptr;
}

float price_of(const world& w, entity_id market, std::size_t r)
{
    const auto mit = w.markets.find(market);
    if (mit == w.markets.end())
        return 0.0f;
    const float pr = posted_price(mit->second, r);
    return std::isfinite(pr) ? pr : 0.0f;
}

} // namespace

bool space_node_hostile(const world& w, entity_id nation, orbital_node node)
{
    for (const auto& [id, u] : w.space_units) // ascending
        if (u.owner != nation && u.node == node && u.state != space_unit_state::in_transit
            && space_unit_firepower(u) > 0.0f && at_war(w, nation, u.owner))
            return true;
    return false;
}

bool space_haul_eligible(const world& w, entity_id trader, entity_id nation)
{
    return trader != null_entity && trader == nation && w.nations.find(nation) != w.nations.end()
        && space_unit_design_of(w, nation, space_hull::hauler) != nullptr;
}

space_haul_quote quote_space_haul(const world& w, const space_unit_params& p, entity_id nation,
                                  entity_id src_market, entity_id dest_market)
{
    space_haul_quote q;
    const auto sit = w.markets.find(src_market);
    const auto dit = w.markets.find(dest_market);
    const auto nit = w.nations.find(nation);
    if (sit == w.markets.end() || dit == w.markets.end() || nit == w.nations.end())
        return q;
    const entity_id src_body = sit->second.body, dest_body = dit->second.body;
    if (src_body == null_entity || dest_body == null_entity || src_body == dest_body)
        return q;
    q.from = {src_body, orbital_node_kind::low_orbit};
    q.to   = {dest_body, orbital_node_kind::low_orbit};
    if (!orbital_node_valid(w, q.from) || !orbital_node_valid(w, q.to))
        return q;
    q.pad = space_staffed_pad_on(w, nation, src_body);
    if (q.pad == null_entity || staffed_pad_any(w, dest_body) == null_entity)
        return q;
    // BL-1599 (SPACE_FLEETS.md § Blockade): a blockaded body's space trade stops.
    if (space_trade_blockaded(w, nation, src_market, dest_market))
        return q;
    const entity_id pad_market = space_market_on_body(w, w.buildings.at(q.pad).tile, src_body);
    if (pad_market == null_entity)
        return q;
    q.src_market  = src_market;
    q.dest_market = dest_market;

    const orbital_path_result out  = orbital_path(w, orbital_graph_params{}, q.from, q.to);
    const orbital_path_result back = orbital_path(w, orbital_graph_params{}, q.to, q.from);
    if (!out.ok || !back.ok || !(out.dv > 0.0) || !std::isfinite(out.dv) || !std::isfinite(back.dv))
        return q;

    // The hull: an idle one at the source's low orbit, else a fresh raise.
    q.unit = idle_hauler_at(w, nation, q.from);
    space_design_stats st;
    q.fixed.fill(0.0f);
    if (q.unit != null_entity)
    {
        const space_unit& u = w.space_units.at(q.unit);
        st = evaluate_space_design(u.hull, u.design);
        q.fixed[PR] = std::max(0.0f, st.tank - u.propellant); // the refuel over the pad
    }
    else
    {
        if (space_raise_ready(w, nation, space_hull::hauler) != space_unit_result::ok)
            return q;
        st = evaluate_space_design(space_hull::hauler, *space_unit_design_of(w, nation, space_hull::hauler));
        if (nit->second.astronauts < st.crew)
            return q;
        q.fixed = st.goods;
        q.fixed[PR] += st.tank + climb_rate(w, p, src_body) * (st.dry_mass + st.tank);
    }
    if (!st.valid || !(st.cargo > 0.0f) || !(st.tank > 0.0f))
        return q;

    bool any_fixed = false;
    for (const float v : q.fixed)
        any_fixed = any_fixed || v > 0.0f;
    if (any_fixed && !space_quote_lot(w, p, pad_market, q.fixed, &q.fixed_credits))
        return q;

    q.lift_per_unit   = climb_rate(w, p, src_body);
    q.drop_per_unit   = climb_rate(w, p, dest_body);
    q.src_prop_price  = price_of(w, pad_market, PR);
    q.dest_prop_price = price_of(w, dest_market, PR);
    if (!std::isfinite(q.lift_per_unit) || !std::isfinite(q.drop_per_unit))
        return q;
    if ((q.lift_per_unit > 0.0f && !(q.src_prop_price > 0.0f)) || (q.drop_per_unit > 0.0f && !(q.dest_prop_price > 0.0f)))
        return q;

    // THE CAP: the laden outbound burn and the empty return both fit one full tank,
    // with a thousandth held back so the hop-by-hop burns (equal in exact
    // arithmetic) never fall a float short.
    //   return  R = dry (e^(D2/ve) - 1)
    //   outbound (dry + c + P)(1 - e^(-D1/ve)) <= (P - R) x 0.999
    const double ve  = st.exhaust;
    const double dry = st.dry_mass;
    const double P   = st.tank;
    const double R   = dry * (std::exp(back.dv / ve) - 1.0);
    const double f   = 1.0 - std::exp(-out.dv / ve);
    const double c   = ((P - R) * 0.999) / f - dry - P;
    if (!std::isfinite(c) || !(P > R))
        return q;
    q.cap = static_cast<float>(std::min<double>(st.cargo, c));
    if (!(q.cap > 1.0e-3f))
    {
        q.cap = 0.0f;
        return q;
    }
    q.days = 1.0f + hop_days(w, out.nodes); // the day it waits at the pad, then the hops
    if (!std::isfinite(q.days) || !std::isfinite(q.cost_for(q.cap)))
        return q;
    q.viable = true;
    return q;
}

entity_id launch_space_haul(world& w, const space_unit_params& p, const space_haul_quote& q,
                            entity_id nation, std::size_t ri, float send, float* out_credits)
{
    if (out_credits != nullptr)
        *out_credits = 0.0f;
    if (!q.viable || ri >= resource_count || !std::isfinite(send) || !(send > 0.0f) || send > q.cap
        || w.nations.find(nation) == w.nations.end() || w.buildings.find(q.pad) == w.buildings.end())
        return null_entity;
    const entity_id src_body = w.markets.at(q.src_market).body;
    const entity_id pad_market = space_market_on_body(w, w.buildings.at(q.pad).tile, src_body);
    if (pad_market == null_entity)
        return null_entity;
    if (q.unit != null_entity)
    {
        const auto uit = w.space_units.find(q.unit);
        if (uit == w.space_units.end() || uit->second.state != space_unit_state::idle || uit->second.node != q.from
            || uit->second.cargo > 0.0f || w.space_hauls.count(q.unit) != 0)
            return null_entity;
    }
    // The whole space lot on the pad's shelf: the raise or refuel, and the lift.
    std::array<float, resource_count> lot = q.fixed;
    std::array<float, resource_count> lift{};
    lift[PR] = q.lift_per_unit * send;
    lot[PR] += lift[PR];
    float credits = 0.0f;
    if (!space_quote_lot(w, p, pad_market, lot, &credits))
        return null_entity;
    // The cargo comes off the trade's shelf; where that is the pad's, the two must fit together.
    if (pad_market == q.src_market && !(w.markets.at(pad_market).inventory[ri] >= lot[ri] + send))
        return null_entity;
    if (!(w.nations.at(nation).treasury >= credits))
        return null_entity;

    // Every check passed: write.
    entity_id unit = q.unit;
    float spent = 0.0f;
    if (unit == null_entity)
    {
        const space_unit_op op = raise_space_unit_at_pad(w, p, nation, space_hull::hauler, src_body);
        if (op.result != space_unit_result::ok)
            return null_entity; // a raise refuses whole: nothing written
        unit = op.unit;
        spent += op.credits;
    }
    else if (q.fixed[PR] > 0.0f)
    {
        const space_unit_op op = refuel_space_unit(w, p, station_params{}, nation, unit);
        if (op.result != space_unit_result::ok)
            return null_entity; // refuses whole
        spent += op.credits;
    }
    if (lift[PR] > 0.0f)
    {
        float lift_credits = 0.0f;
        space_quote_lot(w, p, pad_market, lift, &lift_credits);
        space_take_lot(w, nation, pad_market, lift, lift_credits);
        spent += lift_credits;
    }
    if (out_credits != nullptr)
        *out_credits = spent;
    return unit;
}

void attach_space_haul(world& w, const space_haul_quote& q, entity_id unit, entity_id nation,
                       std::size_t ri, float send, uint32_t convoy_id)
{
    space_unit& u = w.space_units.at(unit);
    u.cargo = send;
    space_haul h;
    h.owner       = nation;
    h.phase       = space_haul_phase::outbound;
    h.cargo_res   = static_cast<resource_type>(ri);
    h.src_market  = q.src_market;
    h.convoy      = convoy_id;
    h.dest_market = q.dest_market;
    h.target      = q.to;
    h.home        = q.from;
    w.space_hauls[unit] = h;
}

space_unit_op load_space_cargo(world& w, const space_unit_params& p, entity_id nation, entity_id unit,
                               std::size_t ri, float qty)
{
    space_unit_op op;
    op.unit = unit;
    if (ri >= resource_count || !std::isfinite(qty) || !(qty > 0.0f))
        return op; // rejected_invalid
    const auto uit = w.space_units.find(unit);
    if (uit == w.space_units.end())
    {
        op.result = space_unit_result::rejected_unknown_unit;
        return op;
    }
    const space_unit& u = uit->second;
    if (u.owner != nation)
    {
        op.result = space_unit_result::rejected_not_owner;
        return op;
    }
    const space_design_stats st = evaluate_space_design(u.hull, u.design);
    const auto hit = w.space_hauls.find(unit);
    if (!carrier(u.hull) || !(st.cargo > 0.0f)
        || (hit != w.space_hauls.end()
            && (hit->second.phase != space_haul_phase::held || hit->second.cargo_res != static_cast<resource_type>(ri)))
        || !(u.cargo + qty <= st.cargo))
    {
        op.result = space_unit_result::rejected_cargo;
        return op;
    }
    if (u.state != space_unit_state::idle)
    {
        op.result = space_unit_result::rejected_busy;
        return op;
    }
    if (u.node.kind != orbital_node_kind::low_orbit)
    {
        op.result = space_unit_result::rejected_wrong_node;
        return op;
    }
    if (space_node_hostile(w, nation, u.node))
    {
        op.result = space_unit_result::rejected_hostile;
        return op;
    }
    const entity_id pad = space_staffed_pad_on(w, nation, u.node.body);
    if (pad == null_entity)
    {
        op.result = space_unit_result::rejected_no_pad;
        return op;
    }
    const entity_id market = space_market_on_body(w, w.buildings.at(pad).tile, u.node.body);
    if (market == null_entity)
    {
        op.result = space_unit_result::rejected_no_market;
        return op;
    }
    // BL-1599 (SPACE_FLEETS.md § Blockade): no load or unload at a blockaded body -
    // blockaded for the acting nation, or for the nation whose ground the market is on.
    if (space_blockaded(w, nation, u.node.body) || space_market_blockaded(w, market))
    {
        op.result = space_unit_result::rejected_blockaded;
        return op;
    }
    const float lift = climb_rate(w, p, u.node.body) * qty;
    if (!std::isfinite(lift))
        return op; // rejected_invalid
    std::array<float, resource_count> lot{};
    lot[ri] += qty;
    lot[PR] += lift;
    float credits = 0.0f;
    if (!space_quote_lot(w, p, market, lot, &credits))
    {
        op.result = space_unit_result::rejected_shelf_short;
        return op;
    }
    if (!(w.nations.at(nation).treasury >= credits))
    {
        op.result = space_unit_result::rejected_insufficient_treasury;
        return op;
    }

    space_take_lot(w, nation, market, lot, credits);
    uit->second.cargo = u.cargo + qty;
    if (hit == w.space_hauls.end())
    {
        space_haul h;
        h.owner      = nation;
        h.phase      = space_haul_phase::held;
        h.cargo_res  = static_cast<resource_type>(ri);
        h.src_market = market;
        h.target = h.home = u.node;
        w.space_hauls.emplace(unit, h);
    }
    op.result     = space_unit_result::ok;
    op.credits    = credits;
    op.propellant = lift;
    op.market     = market;
    return op;
}

space_unit_op unload_space_cargo(world& w, const space_unit_params& p, entity_id nation, entity_id unit)
{
    space_unit_op op;
    op.unit = unit;
    const auto uit = w.space_units.find(unit);
    if (uit == w.space_units.end())
    {
        op.result = space_unit_result::rejected_unknown_unit;
        return op;
    }
    space_unit& u = uit->second;
    if (u.owner != nation)
    {
        op.result = space_unit_result::rejected_not_owner;
        return op;
    }
    const auto hit = w.space_hauls.find(unit);
    if (hit == w.space_hauls.end() || hit->second.phase != space_haul_phase::held || !(u.cargo > 0.0f))
    {
        op.result = space_unit_result::rejected_cargo; // nothing aboard, or a trade's cargo
        return op;
    }
    if (u.state != space_unit_state::idle)
    {
        op.result = space_unit_result::rejected_busy;
        return op;
    }
    if (u.node.kind != orbital_node_kind::low_orbit)
    {
        op.result = space_unit_result::rejected_wrong_node;
        return op;
    }
    if (space_node_hostile(w, nation, u.node))
    {
        op.result = space_unit_result::rejected_hostile;
        return op;
    }
    const entity_id pad = staffed_pad_any(w, u.node.body);
    if (pad == null_entity)
    {
        op.result = space_unit_result::rejected_no_pad;
        return op;
    }
    const entity_id market = space_market_on_body(w, w.buildings.at(pad).tile, u.node.body);
    if (market == null_entity)
    {
        op.result = space_unit_result::rejected_no_market;
        return op;
    }
    // BL-1599 (SPACE_FLEETS.md § Blockade): no load or unload at a blockaded body -
    // blockaded for the acting nation, or for the nation whose ground the market is on.
    if (space_blockaded(w, nation, u.node.body) || space_market_blockaded(w, market))
    {
        op.result = space_unit_result::rejected_blockaded;
        return op;
    }
    const float drop = climb_rate(w, p, u.node.body) * u.cargo;
    if (!std::isfinite(drop))
        return op; // rejected_invalid
    std::array<float, resource_count> lot{};
    lot[PR] = drop;
    float credits = 0.0f;
    if (drop > 0.0f && !space_quote_lot(w, p, market, lot, &credits))
    {
        op.result = space_unit_result::rejected_shelf_short;
        return op;
    }
    if (!(w.nations.at(nation).treasury >= credits))
    {
        op.result = space_unit_result::rejected_insufficient_treasury;
        return op;
    }

    if (drop > 0.0f)
        space_take_lot(w, nation, market, lot, credits);
    // The cargo lands: an arrived convoy, which the next clear lands and sells.
    convoy_component c;
    c.id             = w.allocate_convoy_id();
    c.source_market  = w.markets.count(hit->second.src_market) != 0 ? hit->second.src_market : market;
    c.dest_market    = market;
    c.mode           = convoy_mode::space;
    c.cargo_resource = hit->second.cargo_res;
    c.cargo_qty      = u.cargo;
    c.progress       = 1.0f;
    c.speed          = 0.0f;
    c.corp           = nation;
    c.arrived        = true;
    c.cost_paid      = credits;
    w.convoys.push_back(c);
    u.cargo = 0.0f;
    w.space_hauls.erase(hit);
    op.result     = space_unit_result::ok;
    op.credits    = credits;
    op.propellant = drop;
    op.market     = market;
    return op;
}

void prune_space_hauls(world& w)
{
    for (auto it = w.space_hauls.begin(); it != w.space_hauls.end();)
    {
        if (w.space_units.count(it->first) != 0)
        {
            ++it;
            continue;
        }
        erase_convoy(w, it->second.convoy); // the cargo goes with its hull
        it = w.space_hauls.erase(it);
    }
}

void run_space_convoy_day(world& w, const space_unit_params& p, const station_params& stp,
                          space_convoy_day_report* out)
{
    space_convoy_day_report r;
    if (w.space_hauls.empty())
    {
        if (out != nullptr)
            *out = r;
        return;
    }
    std::vector<entity_id> ids;
    ids.reserve(w.space_hauls.size());
    for (const auto& [id, h] : w.space_hauls) // ascending
        ids.push_back(id);

    for (const entity_id id : ids)
    {
        auto hit = w.space_hauls.find(id);
        if (hit == w.space_hauls.end())
            continue;
        space_haul& h = hit->second;
        const auto uit = w.space_units.find(id);
        if (uit == w.space_units.end())
        {
            erase_convoy(w, h.convoy);
            w.space_hauls.erase(hit);
            continue;
        }
        if (h.phase == space_haul_phase::held)
            continue; // hand cargo moves by order only
        space_unit& u = uit->second;
        if (u.state != space_unit_state::idle)
        {
            if (u.state != space_unit_state::in_transit)
                ++r.waiting; // docked or landed by hand: the trip waits on its owner
            continue;
        }

        // 1. INTERDICTION at the node it stands at.
        if (space_node_hostile(w, h.owner, u.node))
        {
            if (!space_armed_units_at(w, h.owner, u.node).empty())
            {
                // Escorted: the meeting OPENS the battle (BL-1599; SPACE_FLEETS.md
                // § Battles) with every nation at war with the owner armed here,
                // ascending, and the Hauler waits at the node until it is decided.
                std::vector<entity_id> foes;
                for (const auto& [oid, o] : w.space_units) // ascending
                    if (o.owner != h.owner && o.node == u.node && o.state != space_unit_state::in_transit
                        && space_unit_firepower(o) > 0.0f && at_war(w, h.owner, o.owner)
                        && std::find(foes.begin(), foes.end(), o.owner) == foes.end())
                        foes.push_back(o.owner);
                std::sort(foes.begin(), foes.end());
                for (const entity_id foe : foes)
                {
                    bool opened = false;
                    (void)open_space_battle(w, h.owner, foe, u.node, &opened);
                    if (opened)
                        ++r.battles;
                }
                ++r.stopped;
                continue;
            }
            ++r.lost;
            r.cargo_lost += u.cargo;
            erase_convoy(w, h.convoy);
            w.space_hauls.erase(hit);
            w.space_units.erase(uit);
            continue;
        }

        // 2. Outbound at the destination: unload.
        if (h.phase == space_haul_phase::outbound)
        {
            convoy_component* c = find_convoy(w, h.convoy);
            if (c == nullptr)
            {
                // The riding cargo is gone (nothing else erases it; robust all the same).
                u.cargo  = 0.0f;
                h.convoy = 0;
                h.phase  = space_haul_phase::returning;
                h.target = h.home;
            }
            else if (u.node == h.target)
            {
                const auto mit = w.markets.find(h.dest_market);
                const entity_id body = u.node.body;
                const float drop = climb_rate(w, p, body) * u.cargo;
                std::array<float, resource_count> lot{};
                lot[PR] = std::isfinite(drop) ? drop : 0.0f;
                float credits = 0.0f;
                // BL-1599: a blockaded destination takes no unload; the Hauler waits.
                const bool can = mit != w.markets.end() && std::isfinite(drop) && staffed_pad_any(w, body) != null_entity
                              && !space_blockaded(w, h.owner, body) && !space_market_blockaded(w, h.dest_market)
                              && (!(drop > 0.0f) || space_quote_lot(w, p, h.dest_market, lot, &credits))
                              && w.nations.count(h.owner) != 0 && w.nations.at(h.owner).treasury >= credits;
                if (!can)
                {
                    ++r.waiting; // the descent cannot be paid today: it waits in orbit
                    continue;
                }
                if (drop > 0.0f)
                    space_take_lot(w, h.owner, h.dest_market, lot, credits);
                c->arrived   = true;
                c->progress  = 1.0f;
                c->cost_paid += credits;
                u.cargo  = 0.0f;
                h.convoy = 0;
                h.phase  = space_haul_phase::returning;
                h.target = h.home;
                ++r.unloaded;
                // Top up where it may (an owner's pad below, or nothing); the return
                // was sized to need none.
                (void)refuel_space_unit(w, p, stp, h.owner, id);
            }
        }

        // 3. Returning at home: the trip ends.
        if (h.phase == space_haul_phase::returning && u.node == h.home)
        {
            ++r.home;
            w.space_hauls.erase(hit);
            continue;
        }

        // 4. The next hop of the cheapest path from where it stands.
        const orbital_path_result path = orbital_path(w, orbital_graph_params{}, u.node, h.target);
        space_unit_op op;
        if (path.ok && path.nodes.size() >= 2)
            op = order_space_unit(w, p, h.owner, id, path.nodes[1]);
        if (op.result == space_unit_result::ok)
        {
            ++r.departed;
            continue;
        }
        // No hop it can pay: the trip ends where it stands. A trade's cargo still
        // aboard becomes the owner's hand cargo (unloadable by order); its convoy,
        // which rode at speed 0, is retired with nothing landed.
        ++r.stranded;
        if (h.phase == space_haul_phase::outbound && u.cargo > 0.0f)
        {
            erase_convoy(w, h.convoy);
            h.convoy      = 0;
            h.dest_market = null_entity;
            h.phase       = space_haul_phase::held;
            h.target = h.home = u.node;
        }
        else
            w.space_hauls.erase(hit);
    }
    if (out != nullptr)
        *out = r;
}
