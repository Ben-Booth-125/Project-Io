// BL-1737 (army groups; docs/military/MILITARY.md § Army groups). See army_groups.hpp.

#include "army_groups.hpp"

#include "world.hpp"

#include <algorithm>
#include <set>

std::string army_group_name(uint32_t ordinal)
{
    if (ordinal == 0)
        return "Army";
    const uint32_t tens = ordinal % 100;
    const char*    sfx  = "th";
    if (tens < 11 || tens > 13)
    {
        switch (ordinal % 10)
        {
            case 1: sfx = "st"; break;
            case 2: sfx = "nd"; break;
            case 3: sfx = "rd"; break;
            default: break;
        }
    }
    return std::to_string(ordinal) + sfx + " Army";
}

std::string army_group_name(const world& w, entity_id group)
{
    const auto it = w.army_groups.find(group);
    return it == w.army_groups.end() ? std::string{} : army_group_name(it->second.ordinal);
}

std::vector<entity_id> army_group_members(const world& w, entity_id group)
{
    std::vector<entity_id> out;
    if (group == null_entity)
        return out;
    for (const auto& [uid, u] : w.units)
        if (u.army_group == group)
            out.push_back(uid);
    std::sort(out.begin(), out.end()); // `units` is unordered
    return out;
}

bool army_group_has_member(const world& w, entity_id group)
{
    if (group == null_entity)
        return false;
    for (const auto& [uid, u] : w.units)
        if (u.army_group == group)
            return true;
    for (const auto& [sid, sh] : w.ships)
        for (const auto& [uid, u] : sh.aboard)
            if (u.army_group == group)
                return true;
    return false;
}

std::vector<entity_id> army_groups_of(const world& w, entity_id owner)
{
    std::vector<std::pair<uint32_t, entity_id>> rows;
    for (const auto& [gid, g] : w.army_groups)
        if (g.owner == owner)
            rows.emplace_back(g.ordinal, gid);
    std::sort(rows.begin(), rows.end());
    std::vector<entity_id> out;
    out.reserve(rows.size());
    for (const auto& r : rows)
        out.push_back(r.second);
    return out;
}

uint32_t lowest_free_army_ordinal(const world& w, entity_id owner)
{
    std::set<uint32_t> used;
    for (const auto& [gid, g] : w.army_groups)
        if (g.owner == owner)
            used.insert(g.ordinal);
    uint32_t n = 1;
    while (used.count(n) != 0)
        ++n;
    return n;
}

corp_command_result set_army_group_check(const world& w, entity_id actor, entity_id unit,
                                         entity_id group, uint32_t order)
{
    // The seat's alone (AI_OPPONENT.md § 11: no grant lets a rival group its units).
    if (actor == null_entity || actor != player_nation(w))
        return corp_command_result::rejected_not_owner;
    if (order > 1u || (order == 1u && group != null_entity))
        return corp_command_result::rejected_invalid;
    const auto uit = w.units.find(unit);
    if (uit == w.units.end())
        return corp_command_result::rejected_invalid;
    const unit_component& u = uit->second;
    if (u.owner != actor)
        return corp_command_result::rejected_not_owner;
    if (group != null_entity)
    {
        const auto git = w.army_groups.find(group);
        if (git == w.army_groups.end())
            return corp_command_result::rejected_invalid;
        if (git->second.owner != actor)
            return corp_command_result::rejected_not_owner;
        if (u.army_group == group)
            return corp_command_result::rejected_state; // already in it
        return corp_command_result::applied;
    }
    if (order == 0u)
        return u.army_group == null_entity ? corp_command_result::rejected_state // in none
                                           : corp_command_result::applied;
    // A new group. Its own old group, if this unit is its last, is gone before the
    // new one is counted, so a lone unit can always move to a new group.
    std::size_t held = 0;
    for (const auto& [gid, g] : w.army_groups)
        if (g.owner == actor)
            ++held;
    if (u.army_group != null_entity && held > 0)
    {
        bool others = false;
        for (const auto& [oid, o] : w.units)
            if (oid != unit && o.army_group == u.army_group)
            {
                others = true;
                break;
            }
        if (!others)
            for (const auto& [sid, sh] : w.ships)
                for (const auto& [oid, o] : sh.aboard)
                    if (o.army_group == u.army_group)
                        others = true;
        if (!others)
            --held;
    }
    if (held >= k_army_groups_max || w.next_entity_id() == null_entity) // cap, or the id pool spent
        return corp_command_result::rejected_state;
    return corp_command_result::applied;
}

corp_command_result apply_set_army_group(world& w, entity_id actor, entity_id unit,
                                         entity_id group, uint32_t order,
                                         entity_id* out_group)
{
    if (out_group != nullptr)
        *out_group = null_entity;
    const corp_command_result r = set_army_group_check(w, actor, unit, group, order);
    if (r != corp_command_result::applied)
        return r;
    unit_component& u = w.units.at(unit);
    if (group != null_entity || order == 0u)
    {
        u.army_group = group; // join `group`, or leave (null)
        prune_army_groups(w);
        return corp_command_result::applied;
    }
    // A new group: leave the old first (and let it end if this unit was its last),
    // so the ordinal is the lowest free once it has.
    u.army_group = null_entity;
    prune_army_groups(w);
    const entity_id id = w.create_entity(); // not null: the check refused a spent pool
    army_group_component g;
    g.owner   = actor;
    g.ordinal = lowest_free_army_ordinal(w, actor);
    w.army_groups.emplace(id, g);
    u.army_group = id;
    if (out_group != nullptr)
        *out_group = id;
    return corp_command_result::applied;
}

std::size_t prune_army_groups(world& w)
{
    // A unit naming a group that is gone, or another owner's, leaves it.
    const auto fix = [&](unit_component& u) {
        if (u.army_group == null_entity)
            return;
        const auto git = w.army_groups.find(u.army_group);
        if (git == w.army_groups.end() || git->second.owner != u.owner)
            u.army_group = null_entity;
    };
    for (auto& [uid, u] : w.units)
        fix(u);
    for (auto& [sid, sh] : w.ships)
        for (auto& [uid, u] : sh.aboard)
            fix(u);
    std::set<entity_id> named;
    for (const auto& [uid, u] : w.units)
        if (u.army_group != null_entity)
            named.insert(u.army_group);
    for (const auto& [sid, sh] : w.ships)
        for (const auto& [uid, u] : sh.aboard)
            if (u.army_group != null_entity)
                named.insert(u.army_group);
    std::size_t removed = 0;
    for (auto it = w.army_groups.begin(); it != w.army_groups.end();)
    {
        if (named.count(it->first) == 0)
        {
            it = w.army_groups.erase(it);
            ++removed;
        }
        else
            ++it;
    }
    return removed;
}
