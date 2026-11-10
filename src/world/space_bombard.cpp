#include "space_bombard.hpp"

#include "air_combat.hpp"        // condition_bites: what a strike can wear down
#include "components.hpp"
#include "nation_tech.hpp"        // nation_has_mechanic
#include "sentiment_emitters.hpp" // queue_conduct: force_used
#include "space_battle.hpp"       // space_hostile, space_battle_params
#include "space_convoy.hpp"       // prune_space_hauls
#include "space_tech_keys.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <map>
#include <vector>

// BL-1599 — see space_bombard.hpp for the design. This is the arithmetic.

namespace {

bool in_transit(const space_unit& u) { return u.state == space_unit_state::in_transit; }

bool is_nation(const world& w, entity_id id) { return id != null_entity && w.nations.count(id) != 0; }

/// The nation whose ground @p market's centre stands on, or null.
entity_id market_holder(const world& w, entity_id market)
{
    const auto m = w.markets.find(market);
    if (m == w.markets.end() || m->second.centre_tile == null_entity)
        return null_entity;
    const auto n = w.tile_to_nation.find(m->second.centre_tile);
    return n == w.tile_to_nation.end() ? null_entity : n->second;
}

entity_id tile_body(const world& w, entity_id tile)
{
    const auto t = w.tiles.find(tile);
    return t == w.tiles.end() ? null_entity : t->second.body;
}

/// The catalogue index of the orbital-missile armament (the munition), or -1.
int missile_item()
{
    const auto& cat = space_item_catalogue();
    for (std::size_t i = 0; i < cat.size(); ++i)
        if (std::strcmp(cat[i].id, "armament.missile") == 0)
            return static_cast<int>(i);
    return -1;
}

/// A building a strike may land on: standing, completed, not decommissioned, and of a
/// type its condition bites (`condition_bites`, the strategic mission's own target
/// set) - a strike that could change nothing is not offered.
bool strikeable(const building_component& b)
{
    return b.ticks_remaining <= 0 && !b.decommissioned && b.condition_permille > 0 && condition_bites(b.type);
}

int hull_rank(space_hull h)
{
    switch (h)
    {
    case space_hull::frigate: return 0; // escorts
    case space_hull::cruiser: return 1; // capital ships
    default:                  return 2; // unarmed hulls
    }
}

float finite_nonneg(float v) { return std::isfinite(v) ? std::max(0.0f, v) : 0.0f; }

} // namespace

bool space_blockaded(const world& w, entity_id nation, entity_id body)
{
    if (w.space_units.empty() || !is_nation(w, nation) || body == null_entity)
        return false;
    const orbital_node low{body, orbital_node_kind::low_orbit};
    bool hostile = false;
    for (const auto& [id, u] : w.space_units) // ascending
    {
        if (u.node != low || in_transit(u) || !(space_unit_firepower(u) > 0.0f))
            continue;
        if (u.owner == nation)
            return false; // the holder is armed there: contested, not blockaded
        hostile = hostile || space_hostile(w, nation, u.owner);
    }
    return hostile;
}

bool space_market_blockaded(const world& w, entity_id market)
{
    if (w.space_units.empty())
        return false;
    const entity_id holder = market_holder(w, market);
    return holder != null_entity && space_blockaded(w, holder, market_body(w, market));
}

bool space_trade_blockaded(const world& w, entity_id trader, entity_id src_market, entity_id dest_market)
{
    if (w.space_units.empty())
        return false;
    return space_blockaded(w, trader, market_body(w, src_market)) || space_market_blockaded(w, src_market)
        || space_market_blockaded(w, dest_market);
}

bool space_unit_can_bombard(const space_unit& u)
{
    static const int missile = missile_item();
    if (missile < 0)
        return false;
    const std::size_t a = static_cast<std::size_t>(space_unit_slot::armament);
    return space_hull_has_slot(u.hull, space_unit_slot::armament) && u.design[a] == static_cast<uint8_t>(missile)
        && space_unit_firepower(u) > 0.0f;
}

entity_id space_bombard_target_body(const world& w, const space_bombard_order& o)
{
    if (o.kind == space_bombard_target::building)
    {
        const auto bit = w.buildings.find(static_cast<entity_id>(o.target));
        return bit == w.buildings.end() ? null_entity : tile_body(w, bit->second.tile);
    }
    if (o.kind == space_bombard_target::province)
    {
        const province* pr = w.provinces.find(o.target);
        return pr == nullptr ? null_entity : pr->body;
    }
    return null_entity;
}

entity_id space_bombard_pick(const world& w, entity_id nation, const space_bombard_order& o)
{
    if (o.kind == space_bombard_target::building)
    {
        const entity_id bid = static_cast<entity_id>(o.target);
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end() || !strikeable(bit->second))
            return null_entity;
        const entity_id owner = owner_nation_of(w, bid);
        return (is_nation(w, owner) && space_hostile(w, nation, owner)) ? bid : null_entity;
    }
    if (o.kind != space_bombard_target::province || w.provinces.find(o.target) == nullptr)
        return null_entity;
    // The hostile building in best condition, ties to the lowest id (the strategic
    // mission's rule): a campaign wears a province down evenly.
    std::vector<entity_id> bids;
    for (const auto& [bid, b] : w.buildings)
        if (strikeable(b) && w.provinces.province_of(b.tile) == o.target)
            bids.push_back(bid);
    std::sort(bids.begin(), bids.end());
    entity_id best = null_entity;
    int best_c = -1;
    for (const entity_id bid : bids)
    {
        const entity_id owner = owner_nation_of(w, bid);
        if (!is_nation(w, owner) || !space_hostile(w, nation, owner))
            continue;
        const int c = w.buildings.at(bid).condition_permille;
        if (c > best_c)
        {
            best_c = c;
            best = bid;
        }
    }
    return best;
}

space_unit_result order_space_bombard(world& w, entity_id nation, entity_id unit,
                                      space_bombard_target kind, uint32_t target, bool clear)
{
    const auto uit = w.space_units.find(unit);
    if (uit == w.space_units.end())
        return space_unit_result::rejected_unknown_unit;
    const space_unit& u = uit->second;
    if (u.owner != nation || !is_nation(w, nation))
        return space_unit_result::rejected_not_owner;
    if (clear)
    {
        const auto oit = w.space_bombard_orders.find(unit);
        if (oit == w.space_bombard_orders.end())
            return space_unit_result::rejected_invalid;
        w.space_bombard_orders.erase(oit);
        return space_unit_result::ok;
    }
    // The raw kind is validated before anything reads it as an enum value.
    if (static_cast<uint8_t>(kind) > static_cast<uint8_t>(space_bombard_target::province) || target == 0)
        return space_unit_result::rejected_invalid;
    const space_bombard_order o{kind, target};
    const entity_id body = space_bombard_target_body(w, o);
    if (body == null_entity)
        return space_unit_result::rejected_invalid;
    if (!nation_has_mechanic(w, nation, space_keys::orbital_bombardment))
        return space_unit_result::rejected_tech_locked;
    if (!space_unit_can_bombard(u))
        return space_unit_result::rejected_not_armed;
    if (in_transit(u))
        return space_unit_result::rejected_busy;
    if (u.node.kind != orbital_node_kind::low_orbit || u.node.body != body)
        return space_unit_result::rejected_wrong_node;
    if (space_bombard_pick(w, nation, o) == null_entity)
        return space_unit_result::rejected_target;
    w.space_bombard_orders[unit] = o;
    return space_unit_result::ok;
}

void prune_space_bombard_orders(world& w)
{
    for (auto it = w.space_bombard_orders.begin(); it != w.space_bombard_orders.end();)
        it = (w.space_units.count(it->first) == 0) ? w.space_bombard_orders.erase(it) : std::next(it);
}

void run_space_strike_day(world& w, const space_battle_params& bp, space_strike_day_report* out)
{
    space_strike_day_report r;
    if (w.space_units.empty())
    {
        w.space_bombard_orders.clear(); // no unit, no order (already true; kept exact)
        if (out != nullptr)
            *out = r;
        return;
    }

    // 1. THE STRIKES, fixed at the day's start (fire is simultaneous, as a battle's):
    // ascending unit id, each order checked whole; a failing one is dropped.
    struct strike
    {
        entity_id unit = null_entity, nation = null_entity, building = null_entity;
        int       loss = 0;
    };
    std::vector<strike> strikes;
    const float per_fp = finite_nonneg(bp.bombard_condition_per_firepower);
    for (auto it = w.space_bombard_orders.begin(); it != w.space_bombard_orders.end();)
    {
        const auto uit = w.space_units.find(it->first);
        bool keep = uit != w.space_units.end();
        entity_id pick = null_entity;
        if (keep)
        {
            const space_unit& u = uit->second;
            const entity_id body = space_bombard_target_body(w, it->second);
            keep = is_nation(w, u.owner) && !in_transit(u) && u.node.kind == orbital_node_kind::low_orbit
                && body != null_entity && u.node.body == body && space_unit_can_bombard(u)
                && nation_has_mechanic(w, u.owner, space_keys::orbital_bombardment);
            if (keep)
            {
                pick = space_bombard_pick(w, u.owner, it->second);
                keep = pick != null_entity;
            }
        }
        if (!keep)
        {
            ++r.orders_dropped;
            it = w.space_bombard_orders.erase(it);
            continue;
        }
        const space_unit& u = uit->second;
        const float raw = space_unit_firepower(u) * per_fp;
        strike s;
        s.unit     = it->first;
        s.nation   = u.owner;
        s.building = pick;
        s.loss     = std::isfinite(raw) ? static_cast<int>(std::min(1000.0f, std::max(0.0f, std::floor(raw)))) : 0;
        strikes.push_back(s);
        ++it;
    }

    // 2. THE BATTERIES, ascending building id, each at its condition at the day's start.
    std::vector<entity_id> batteries;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::orbital_defence_battery && b.ticks_remaining <= 0 && !b.decommissioned
            && b.condition_permille > 0)
            batteries.push_back(bid);
    std::sort(batteries.begin(), batteries.end());
    const float bfire = finite_nonneg(bp.battery_firepower);
    std::vector<std::pair<entity_id, int>> fire; // (battery, condition at day start)
    for (const entity_id bid : batteries)
        fire.emplace_back(bid, w.buildings.at(bid).condition_permille);
    bool lost_any = false;
    for (const auto& [bid, cond] : fire)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const entity_id owner = owner_nation_of(w, bid);
        const entity_id body  = tile_body(w, bit->second.tile);
        if (!is_nation(w, owner) || body == null_entity)
            continue;
        const orbital_node low{body, orbital_node_kind::low_orbit};
        // The targets, in the battle's order: escorts, capital ships, unarmed hulls.
        std::vector<std::pair<int, entity_id>> ts;
        for (const auto& [id, u] : w.space_units) // ascending
            if (u.node == low && !in_transit(u) && space_hostile(w, owner, u.owner))
                ts.emplace_back(hull_rank(u.hull), id);
        if (ts.empty())
            continue;
        std::stable_sort(ts.begin(), ts.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
        float pool = bfire * static_cast<float>(std::clamp(cond, 0, 1000)) / 1000.0f;
        if (!(pool > 0.0f))
            continue;
        ++r.battery_shots;
        float mass = 0.0f;
        for (const auto& [rank, id] : ts)
        {
            if (!(pool > 0.0f))
                break;
            space_unit& u = w.space_units.at(id);
            const float armour = std::max(0.0f, space_unit_armour(u));
            if (pool <= armour)
                break; // absorbed whole: the fire stops on this target
            pool -= armour;
            const float hit = std::min(pool, u.hull_points);
            pool -= hit;
            r.battery_landed += hit;
            u.hull_points = std::max(0.0f, u.hull_points - hit);
            if (u.hull_points > 0.0f)
                break; // the target stands; the pool is spent on it
            const space_design_stats st = evaluate_space_design(u.hull, u.design);
            mass += st.dry_mass + std::max(0.0f, u.cargo);
            ++r.units_lost;
            r.crew_lost += std::max(0, u.crew);
            w.space_units.erase(id);
            lost_any = true;
        }
        if (mass > 0.0f)
            r.mass_destroyed.emplace_back(low, mass);
    }

    // 3. THE STRIKES LAND, ascending unit id: condition off, force_used observed.
    const float force_k = finite_nonneg(bp.bombard_force_per_permille);
    for (const strike& s : strikes)
    {
        const auto bit = w.buildings.find(s.building);
        if (bit == w.buildings.end())
            continue;
        building_component& b = bit->second;
        const int lost = std::min(s.loss, std::max(0, b.condition_permille));
        if (lost <= 0)
            continue;
        b.condition_permille -= lost;
        r.condition_lost += lost;
        ++r.strikes;
        // RELATIONS.md § 6: bombardment raises Alarm as force used does — the struck
        // building's owner observes the bomber.
        const entity_id owner = owner_nation_of(w, s.building);
        if (is_nation(w, owner) && owner != s.nation && force_k > 0.0f)
            queue_conduct(w, owner, s.nation, sentiment_factor_kind::force_used,
                          static_cast<float>(lost) * force_k);
    }

    if (lost_any)
    {
        prune_space_hauls(w);
        prune_space_bombard_orders(w);
    }
    if (out != nullptr)
        *out = r;
}
