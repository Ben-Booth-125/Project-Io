#include "space_battle.hpp"

#include "space_units.hpp"
#include "stance.hpp"   // is_hostile
#include "stations.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <set>

// BL-1597 — see space_battle.hpp for the design. This is the arithmetic.

namespace {

bool in_transit(const space_unit& u) { return u.state == space_unit_state::in_transit; }

/// A station whose core is in place (arrived, not lost).
bool core_in_place(const world& w, const space_station& st)
{
    const auto cit = w.space_assets.find(st.core);
    return cit != w.space_assets.end() && cit->second.state != space_asset_state::lost
        && cit->second.state != space_asset_state::in_transit;
}

/// The working Defence modules of @p station, ascending asset id.
std::vector<entity_id> working_defence(const world& w, const station_params& stp, entity_id station)
{
    std::vector<entity_id> out;
    const station_budget b = evaluate_station_budget(w, stp, station, w.current_econ_tick);
    for (const entity_id e : b.working) // ascending
    {
        const auto ait = w.space_assets.find(e);
        if (ait != w.space_assets.end()
            && ait->second.module == static_cast<uint8_t>(station_module_kind::defence))
            out.push_back(e);
    }
    return out;
}

/// The stations of @p nation at @p node with a core in place, ascending id.
std::vector<entity_id> stations_at(const world& w, entity_id nation, orbital_node node)
{
    std::vector<entity_id> out;
    for (const auto& [sid, st] : w.stations) // ascending
        if (st.owner == nation && st.node == node && core_in_place(w, st))
            out.push_back(sid);
    return out;
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

/// One target in a side's order.
struct target
{
    entity_id id     = null_entity;
    bool      module = false; ///< A Defence module (else a unit).
    float     armour = 0.0f;
    float     hp     = 0.0f;  ///< Hull points left at the day's start.
    float     taken  = 0.0f;  ///< Landed today.
};

/// A side's targets, in the order the other side's fire walks them.
std::vector<target> targets_of(const world& w, const station_params& stp, const space_battle_params& bp,
                               const space_battle& b, entity_id nation)
{
    std::vector<std::pair<int, entity_id>> units;
    for (const auto& [id, u] : w.space_units) // ascending
        if (u.owner == nation && u.node == b.node && !in_transit(u))
            units.emplace_back(hull_rank(u.hull), id);
    std::stable_sort(units.begin(), units.end(),
                     [](const auto& x, const auto& y) { return x.first < y.first; });
    std::vector<target> out;
    for (const auto& [rank, id] : units)
    {
        const space_unit& u = w.space_units.at(id);
        target t;
        t.id     = id;
        t.armour = std::max(0.0f, space_unit_armour(u));
        t.hp     = u.hull_points;
        out.push_back(t);
    }
    const float max_hp = std::max(0.0f, bp.defence_hull_points);
    for (const entity_id sid : stations_at(w, nation, b.node))
        for (const entity_id mid : working_defence(w, stp, sid))
        {
            float dmg = 0.0f;
            const auto it = std::lower_bound(b.module_damage.begin(), b.module_damage.end(), mid,
                                             [](const auto& e, entity_id k) { return e.first < k; });
            if (it != b.module_damage.end() && it->first == mid)
                dmg = it->second;
            target t;
            t.id     = mid;
            t.module = true;
            t.armour = std::max(0.0f, bp.defence_armour);
            t.hp     = std::max(0.0f, max_hp - dmg);
            out.push_back(t);
        }
    return out;
}

/// A side's pool: its units' firepower and its working Defence modules' fire.
float pool_of(const world& w, const station_params& stp, const space_battle_params& bp, const space_battle& b,
              entity_id nation)
{
    float pool = 0.0f;
    for (const auto& [id, u] : w.space_units)
        if (u.owner == nation && u.node == b.node && !in_transit(u))
            pool += std::max(0.0f, space_unit_firepower(u));
    const float per = std::max(0.0f, bp.defence_firepower);
    for (const entity_id sid : stations_at(w, nation, b.node))
        pool += per * static_cast<float>(working_defence(w, stp, sid).size());
    return pool;
}

/// Walk @p pool down @p ts: armour first, then hull points. Returns what landed.
float land_fire(float pool, std::vector<target>& ts)
{
    float landed = 0.0f;
    for (target& t : ts)
    {
        if (!(pool > 0.0f))
            break;
        if (!(t.hp > 0.0f))
            continue; // already at zero (a module that had nothing left)
        if (pool <= t.armour)
            break; // absorbed whole: the fire stops on this target
        pool -= t.armour;
        const float hit = std::min(pool, t.hp);
        t.taken += hit;
        landed  += hit;
        pool    -= hit;
        if (hit < t.hp)
            break; // the target stands; the pool is spent on it
    }
    return landed;
}

void trim_history(world& w, const space_battle_params& bp)
{
    const int32_t keep = std::clamp(bp.report_history, 0, k_space_battle_history_max);
    std::size_t closed = 0;
    for (const auto& [id, b] : w.space_battles)
        if (b.state != space_battle_state::open)
            ++closed;
    for (auto it = w.space_battles.begin(); it != w.space_battles.end() && closed > static_cast<std::size_t>(keep);)
    {
        if (it->second.state != space_battle_state::open)
        {
            it = w.space_battles.erase(it); // oldest closed first: ids ascend with time
            --closed;
            continue;
        }
        ++it;
    }
}

/// BL-1598: is the newest record of the pair (@p a < @p c) at @p node a standoff?
bool latest_is_standoff(const world& w, orbital_node node, entity_id a, entity_id c)
{
    for (auto it = w.space_battles.rbegin(); it != w.space_battles.rend(); ++it) // newest first
    {
        const space_battle& b = it->second;
        if (b.node == node && b.sides[0].nation == a && b.sides[1].nation == c)
            return b.state == space_battle_state::standoff;
    }
    return false;
}

} // namespace

bool space_fire_could_land(const world& w, const station_params& stp, const space_battle_params& bp,
                           orbital_node node, entity_id a, entity_id b)
{
    space_battle t;
    t.node = node;
    t.sides[0].nation = a;
    t.sides[1].nation = b;
    std::vector<target> ta = targets_of(w, stp, bp, t, a);
    std::vector<target> tb = targets_of(w, stp, bp, t, b);
    return land_fire(pool_of(w, stp, bp, t, a), tb) > 0.0f || land_fire(pool_of(w, stp, bp, t, b), ta) > 0.0f;
}

bool space_hostile(const world& w, entity_id a, entity_id b)
{
    return a != b && a != null_entity && b != null_entity && (is_hostile(w, a, b) || is_hostile(w, b, a));
}

bool space_armed_at(const world& w, const station_params& stp, entity_id nation, orbital_node node)
{
    if (!space_armed_units_at(w, nation, node).empty())
        return true;
    for (const entity_id sid : stations_at(w, nation, node))
        if (!working_defence(w, stp, sid).empty())
            return true;
    return false;
}

const space_battle* space_battle_engaging(const world& w, entity_id nation, orbital_node node)
{
    for (const auto& [id, b] : w.space_battles) // ascending
        if (b.state == space_battle_state::open && b.node == node
            && (b.sides[0].nation == nation || b.sides[1].nation == nation))
            return &b;
    return nullptr;
}

bool space_unit_engaged(const world& w, entity_id unit)
{
    const auto uit = w.space_units.find(unit);
    if (uit == w.space_units.end() || in_transit(uit->second))
        return false;
    return space_battle_engaging(w, uit->second.owner, uit->second.node) != nullptr;
}

void record_space_withdrawal(world& w, const std::vector<entity_id>& ids, orbital_node dest)
{
    for (const entity_id id : ids)
    {
        const auto uit = w.space_units.find(id);
        if (uit == w.space_units.end() || in_transit(uit->second))
            continue;
        // Every open battle the unit is engaged in carries the record (a node may
        // hold more than one: a nation hostile to two others there).
        for (auto& [bid, b] : w.space_battles)
        {
            if (b.state != space_battle_state::open || b.node != uit->second.node
                || (b.sides[0].nation != uit->second.owner && b.sides[1].nation != uit->second.owner))
                continue;
            auto it = std::lower_bound(b.withdrawing.begin(), b.withdrawing.end(), id,
                                       [](const space_battle_withdrawal& e, entity_id k) { return e.unit < k; });
            if (it != b.withdrawing.end() && it->unit == id)
                it->dest = dest;
            else
                b.withdrawing.insert(it, space_battle_withdrawal{id, dest});
        }
    }
}

void run_space_battle_day(world& w, const space_unit_params& p, const station_params& stp,
                          const space_battle_params& bp, space_battle_day_report* out)
{
    (void)p;
    space_battle_day_report r;
    bool any_open = false;
    for (const auto& [id, b] : w.space_battles)
        any_open = any_open || b.state == space_battle_state::open;
    if (w.space_units.empty() && w.stations.empty() && !any_open)
    {
        if (out != nullptr)
            *out = r;
        return;
    }

    const int32_t stall = std::clamp(bp.stall_days, 0, k_space_battle_stall_max);
    const bool stall_on = stall > 0;

    // 1. OPEN. The nations present at each node (units not in transit, stations with
    // a core in place), node then nation ascending; every hostile, both-armed pair
    // with no open battle there opens one.
    std::map<orbital_node, std::set<entity_id>> present;
    for (const auto& [id, u] : w.space_units)
        if (!in_transit(u))
            present[u.node].insert(u.owner);
    for (const auto& [sid, st] : w.stations)
        if (core_in_place(w, st))
            present[st.node].insert(st.owner);
    for (const auto& [node, nations] : present)
    {
        if (nations.size() < 2)
            continue;
        std::vector<entity_id> armed;
        for (const entity_id n : nations)
            if (space_armed_at(w, stp, n, node))
                armed.push_back(n);
        for (std::size_t i = 0; i < armed.size(); ++i)
            for (std::size_t j = i + 1; j < armed.size(); ++j)
            {
                const entity_id a = armed[i], c = armed[j];
                if (!space_hostile(w, a, c))
                    continue;
                bool exists = false;
                for (const auto& [id, b] : w.space_battles)
                    exists = exists || (b.state == space_battle_state::open && b.node == node
                                        && b.sides[0].nation == a && b.sides[1].nation == c);
                if (exists)
                    continue;
                // BL-1598: a standoff holds until one side's fire could land.
                if (stall_on && latest_is_standoff(w, node, a, c) && !space_fire_could_land(w, stp, bp, node, a, c))
                    continue;
                space_battle b;
                b.id = w.next_space_battle_id++;
                b.node = node;
                b.sides[0].nation = a;
                b.sides[1].nation = c;
                b.opened_tick = w.current_econ_tick;
                w.space_battles.emplace(b.id, b);
                ++r.opened;
            }
    }

    // 2. FIGHT every open battle, ascending id.
    std::vector<uint32_t> ids;
    for (const auto& [id, b] : w.space_battles)
        if (b.state == space_battle_state::open)
            ids.push_back(id);
    for (const uint32_t bid : ids)
    {
        space_battle& b = w.space_battles.at(bid);
        const entity_id n0 = b.sides[0].nation, n1 = b.sides[1].nation;

        // The recorded withdrawals fly (after the day's fire, or at once on peace).
        const auto depart = [&](std::array<bool, 2>& left) {
            const std::vector<space_battle_withdrawal> leaving = std::move(b.withdrawing);
            b.withdrawing.clear();
            for (const space_battle_withdrawal& e : leaving)
            {
                const auto uit = w.space_units.find(e.unit);
                if (uit == w.space_units.end() || in_transit(uit->second) || uit->second.node != b.node)
                    continue; // gone, or already moved by another battle's record
                const entity_id owner = uit->second.owner;
                if (depart_space_units(w, {e.unit}, b.node, e.dest).result == space_unit_result::ok)
                {
                    ++r.departed;
                    left[owner == n0 ? 0 : 1] = true;
                }
                else
                    ++r.departures_failed;
            }
        };

        // Peace (or a vanished nation) closes it before any fire; a recorded
        // withdrawal still flies, now.
        if (w.nations.count(n0) == 0 || w.nations.count(n1) == 0 || !space_hostile(w, n0, n1))
        {
            std::array<bool, 2> left{false, false};
            depart(left);
            b.state = space_battle_state::peace;
            b.module_damage.clear();
            b.quiet_days = 0;
            ++r.closed;
            continue;
        }
        ++b.days;
        ++r.fought;

        // A damaged module removed or lost since (refit-free removal, upkeep decay)
        // leaves the record.
        b.module_damage.erase(std::remove_if(b.module_damage.begin(), b.module_damage.end(),
                                             [&w](const std::pair<entity_id, float>& e) {
                                                 const auto ait = w.space_assets.find(e.first);
                                                 return ait == w.space_assets.end()
                                                     || ait->second.state == space_asset_state::lost;
                                             }),
                              b.module_damage.end());

        // FIRE: both pools fixed first, then landed on the other's targets.
        const float pool0 = pool_of(w, stp, bp, b, n0);
        const float pool1 = pool_of(w, stp, bp, b, n1);
        std::array<std::vector<target>, 2> ts{targets_of(w, stp, bp, b, n0), targets_of(w, stp, bp, b, n1)};
        const float landed_on1 = land_fire(pool0, ts[1]);
        const float landed_on0 = land_fire(pool1, ts[0]);
        b.sides[0].damage_landed += landed_on1;
        b.sides[1].damage_landed += landed_on0;
        r.landed += landed_on0 + landed_on1;
        // BL-1598: the stall's count — a day that landed nothing on either side.
        if (landed_on0 > 0.0f || landed_on1 > 0.0f)
            b.quiet_days = 0;
        else if (b.quiet_days < k_space_battle_stall_max)
            ++b.quiet_days;

        // LOSSES, side 0 then side 1, in target order.
        float mass = 0.0f;
        for (int s = 0; s < 2; ++s)
        {
            space_battle_side& side = b.sides[static_cast<std::size_t>(s)];
            for (const target& t : ts[static_cast<std::size_t>(s)])
            {
                if (!(t.taken > 0.0f))
                    continue;
                if (!t.module)
                {
                    space_unit& u = w.space_units.at(t.id);
                    u.hull_points = std::max(0.0f, u.hull_points - t.taken);
                    if (u.hull_points > 0.0f)
                        continue;
                    const space_design_stats st = evaluate_space_design(u.hull, u.design);
                    mass += st.dry_mass + std::max(0.0f, u.cargo);
                    ++side.units_lost;
                    side.crew_lost += std::max(0, u.crew);
                    ++r.units_lost;
                    r.crew_lost += std::max(0, u.crew);
                    w.space_units.erase(t.id);
                    continue;
                }
                auto it = std::lower_bound(b.module_damage.begin(), b.module_damage.end(), t.id,
                                           [](const auto& e, entity_id k) { return e.first < k; });
                const bool had = it != b.module_damage.end() && it->first == t.id;
                const float total = (had ? it->second : 0.0f) + t.taken;
                if (t.taken < t.hp)
                {
                    if (had)
                        it->second = total;
                    else
                        b.module_damage.insert(it, {t.id, total});
                    continue;
                }
                if (had)
                    b.module_damage.erase(it);
                space_asset& a = w.space_assets.at(t.id);
                ++side.modules_lost;
                side.crew_lost += std::max(0, a.crew);
                ++r.modules_lost;
                r.crew_lost += std::max(0, a.crew);
                mass += std::max(0.0f, bp.defence_module_mass);
                lose_station_asset(a);
            }
        }
        // The dead leave the withdrawal list.
        b.withdrawing.erase(std::remove_if(b.withdrawing.begin(), b.withdrawing.end(),
                                           [&w](const space_battle_withdrawal& e) {
                                               return w.space_units.count(e.unit) == 0;
                                           }),
                            b.withdrawing.end());

        // STATIONS FALL: a side with no armed unit and no working Defence, the
        // other still armed here.
        const auto units_armed = [&](entity_id n) { return !space_armed_units_at(w, n, b.node).empty(); };
        for (int s = 0; s < 2; ++s)
        {
            const entity_id me = b.sides[static_cast<std::size_t>(s)].nation;
            const entity_id foe = b.sides[static_cast<std::size_t>(1 - s)].nation;
            if (units_armed(me) || !space_armed_at(w, stp, foe, b.node))
                continue;
            for (const entity_id sid : stations_at(w, me, b.node))
            {
                if (!working_defence(w, stp, sid).empty())
                    continue;
                const space_station& st = w.stations.at(sid);
                space_battle_side& side = b.sides[static_cast<std::size_t>(s)];
                int crew = 0;
                for (const entity_id e : st.slots) // the core and every part still aboard
                    if (e != null_entity)
                        if (const auto ait = w.space_assets.find(e);
                            ait != w.space_assets.end() && ait->second.state != space_asset_state::lost)
                            crew += std::max(0, ait->second.crew);
                lose_station_asset(w.space_assets.at(st.core));
                ++side.stations_lost;
                side.crew_lost += crew;
                ++r.stations_lost;
                r.crew_lost += crew;
                mass += std::max(0.0f, bp.station_core_mass);
            }
        }

        // WITHDRAWAL: the survivors leave after the day's fire, ascending unit id.
        std::array<bool, 2> left{false, false};
        depart(left);

        b.mass_destroyed += mass;
        if (mass > 0.0f)
            r.mass_destroyed.emplace_back(b.node, mass);

        // THE END.
        const bool armed0 = space_armed_at(w, stp, n0, b.node);
        const bool armed1 = space_armed_at(w, stp, n1, b.node);
        if (armed0 && armed1)
        {
            // BL-1598: THE STALL — both still armed, and `stall` quiet days in a row.
            if (stall_on && b.quiet_days >= stall)
            {
                b.state = space_battle_state::standoff;
                b.withdrawing.clear();
                b.module_damage.clear();
                b.quiet_days = 0;
                ++r.closed;
                ++r.standoffs;
            }
            continue;
        }
        if (!armed0)
            b.sides[0].end = left[0] ? space_battle_end::withdrew : space_battle_end::destroyed;
        if (!armed1)
            b.sides[1].end = left[1] ? space_battle_end::withdrew : space_battle_end::destroyed;
        b.state = space_battle_state::decided;
        b.withdrawing.clear();
        b.module_damage.clear();
        b.quiet_days = 0;
        ++r.closed;
    }
    if (r.closed > 0)
        trim_history(w, bp);
    if (out != nullptr)
        *out = r;
}
