// army_groups_harness.cpp — BL-1737 (army groups; docs/military/MILITARY.md § Army groups,
// Ben 2026-11-22).
//
// Headless check of the one verb, `set_army_group`, and the operations the card builds
// from it, on a generated world:
//
//   G1  names: the ordinal suffixes (1st, 2nd, 3rd, 4th, 11th..13th, 21st, 101st, 111th).
//   G2  form: a unit into a new group, two more into it; ordinal 1, three members.
//   G3  the seat's alone: a rival actor, the seat on a rival's unit, the seat into a
//       rival's group, each refused and the world byte-identical.
//   G4  the verb's own refusals: order above 1, order 1 with a group named, leaving
//       while in none, joining the group it is in, an unknown unit or group.
//   G5  one group at most: joining another group moves the unit out of the first.
//   G6  merge: every unit of group B into group A; B ends; A holds them all.
//   G7  split off: picked units into a new group; ordinal the lowest free; the rest stay.
//   G8  a unit dying: erased from the world, the group drops it; its last unit gone,
//       the group ends on the prune and its ordinal is free again.
//   G9  a unit disbanded through the seam leaves its group, and the day's prune ends a
//       group with no unit left.
//   G10 save/load: groups and memberships round-trip byte-identical, hash equal.
//   G11 determinism: the same sequence on two copies gives byte-identical streams.
//
// Build & run:  node tools/verify/build_harness.js army_groups_harness --run

#include "world/army_groups.hpp"
#include "world/corp_command.hpp"
#include "world/hard_coded_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

std::string to_bytes(const world& w)
{
    std::ostringstream out(std::ios::binary);
    write_world_snapshot(w, out);
    return out.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream in(b, std::ios::binary);
    return read_world_snapshot(w, in);
}

corp_command_result send(world& w, const recipe_registry& reg, entity_id actor, entity_id unit,
                         entity_id group, uint32_t order, entity_id* out = nullptr)
{
    corp_command c;
    c.corp         = actor;
    c.as_nation    = actor;
    c.verb         = corp_verb::set_army_group;
    c.subject      = unit;
    c.counterparty = group;
    c.order        = order;
    return apply_corp_command(w, reg, c, out);
}

/// Form a group of @p units through the seam: the first into a new group, the rest into it.
entity_id form(world& w, const recipe_registry& reg, entity_id actor, const std::vector<entity_id>& units)
{
    entity_id g = null_entity;
    if (units.empty() || send(w, reg, actor, units[0], null_entity, 1, &g) != corp_command_result::applied)
        return null_entity;
    for (std::size_t i = 1; i < units.size(); ++i)
        if (send(w, reg, actor, units[i], g, 0) != corp_command_result::applied)
            return null_entity;
    return g;
}

} // namespace

int main()
{
    world w = make_hard_coded_world(no_prehistory());
    const recipe_registry reg;

    // The seat and a rival, each with units.
    std::map<entity_id, std::vector<entity_id>> by_owner;
    for (const auto& [uid, u] : w.units)
        if (w.nations.count(u.owner) != 0)
            by_owner[u.owner].push_back(uid);
    for (auto& [oid, ids] : by_owner)
        std::sort(ids.begin(), ids.end());
    entity_id seat = null_entity, rival = null_entity;
    for (const auto& [oid, ids] : by_owner)
    {
        if (seat == null_entity)
            seat = oid;
        else if (rival == null_entity)
            rival = oid;
    }
    if (seat == null_entity || rival == null_entity)
    {
        std::printf("FAIL the world has no two nations with units\n");
        return 1;
    }
    // A generated garrison is small: raise the seat to six units by copying its first, so
    // every operation below has units to spare. Copies stand where it stands.
    while (by_owner.at(seat).size() < 6)
    {
        const entity_id id = w.create_entity();
        w.units[id] = w.units.at(by_owner.at(seat).front());
        by_owner.at(seat).push_back(id);
    }
    w.player_entity = seat;
    const std::vector<entity_id> mine = by_owner.at(seat);
    const entity_id theirs = by_owner.at(rival).front();
    std::printf("seat %u with %zu units; rival %u\n", static_cast<unsigned>(seat), mine.size(),
                static_cast<unsigned>(rival));

    // G1 — names.
    check(army_group_name(1u) == "1st Army" && army_group_name(2u) == "2nd Army"
              && army_group_name(3u) == "3rd Army" && army_group_name(4u) == "4th Army"
              && army_group_name(11u) == "11th Army" && army_group_name(12u) == "12th Army"
              && army_group_name(13u) == "13th Army" && army_group_name(21u) == "21st Army"
              && army_group_name(22u) == "22nd Army" && army_group_name(101u) == "101st Army"
              && army_group_name(111u) == "111th Army",
          "G1 ordinal names: 1st, 2nd, 3rd, 4th, 11th-13th, 21st, 22nd, 101st, 111th");

    // G2 — form.
    const world before_any = w;
    const entity_id a = form(w, reg, seat, { mine[0], mine[1], mine[2] });
    check(a != null_entity && w.army_groups.size() == 1 && w.army_groups.at(a).ordinal == 1
              && w.army_groups.at(a).owner == seat && army_group_name(w, a) == "1st Army"
              && army_group_members(w, a) == std::vector<entity_id>{ mine[0], mine[1], mine[2] },
          "G2 Form: three units in a new group, the 1st Army, members ascending");

    // G3 — the seat's alone.
    {
        const std::string b0 = to_bytes(w);
        const bool r1 = send(w, reg, rival, theirs, null_entity, 1) == corp_command_result::rejected_not_owner;
        const bool r2 = send(w, reg, seat, theirs, null_entity, 1) == corp_command_result::rejected_not_owner;
        world rw = w;
        const entity_id rg = rw.create_entity();
        rw.army_groups[rg] = army_group_component{ rival, 1 };
        rw.units.at(theirs).army_group = rg;
        const std::string rb = to_bytes(rw);
        const bool r3 = send(rw, reg, seat, mine[3], rg, 0) == corp_command_result::rejected_not_owner;
        check(r1 && r2 && r3 && to_bytes(w) == b0 && to_bytes(rw) == rb,
              "G3 a rival's verb, the seat on a rival's unit and the seat into a rival's group are refused, nothing mutated");
    }

    // G4 — the verb's own refusals.
    {
        const std::string b0 = to_bytes(w);
        const bool i1 = send(w, reg, seat, mine[3], null_entity, 2) == corp_command_result::rejected_invalid;
        const bool i2 = send(w, reg, seat, mine[3], a, 1) == corp_command_result::rejected_invalid;
        const bool s1 = send(w, reg, seat, mine[3], null_entity, 0) == corp_command_result::rejected_state;
        const bool s2 = send(w, reg, seat, mine[0], a, 0) == corp_command_result::rejected_state;
        const bool i3 = send(w, reg, seat, 0x7FFFFFF0u, a, 0) == corp_command_result::rejected_invalid;
        const bool i4 = send(w, reg, seat, mine[3], 0x7FFFFFF0u, 0) == corp_command_result::rejected_invalid;
        check(i1 && i2 && s1 && s2 && i3 && i4 && to_bytes(w) == b0,
              "G4 order 2, order 1 with a group, leaving none, joining its own, an unknown unit or group: refused, nothing mutated");
    }

    // G5 — one group at most.
    const entity_id b = form(w, reg, seat, { mine[3], mine[4] });
    const bool moved = send(w, reg, seat, mine[2], b, 0) == corp_command_result::applied;
    check(b != null_entity && w.army_groups.at(b).ordinal == 2 && moved
              && w.units.at(mine[2]).army_group == b
              && army_group_members(w, a) == std::vector<entity_id>{ mine[0], mine[1] }
              && army_group_members(w, b) == std::vector<entity_id>{ mine[2], mine[3], mine[4] },
          "G5 a unit joining the 2nd Army leaves the 1st: one group at most");

    // G6 — merge B into A.
    for (const entity_id uid : army_group_members(w, b))
        send(w, reg, seat, uid, a, 0);
    check(w.army_groups.count(b) == 0 && w.army_groups.size() == 1
              && army_group_members(w, a)
                     == std::vector<entity_id>{ mine[0], mine[1], mine[2], mine[3], mine[4] },
          "G6 Merge: the 2nd Army's units into the 1st; the 2nd ends");

    // G7 — split off two.
    entity_id c = null_entity;
    const bool split = send(w, reg, seat, mine[1], null_entity, 1, &c) == corp_command_result::applied
        && send(w, reg, seat, mine[3], c, 0) == corp_command_result::applied;
    check(split && c != null_entity && w.army_groups.at(c).ordinal == 2
              && army_group_members(w, c) == std::vector<entity_id>{ mine[1], mine[3] }
              && army_group_members(w, a) == std::vector<entity_id>{ mine[0], mine[2], mine[4] },
          "G7 Split off: two picked units into a new group, the lowest free ordinal (2); the rest stay");
    check(army_groups_of(w, seat) == std::vector<entity_id>{ a, c } && army_groups_of(w, rival).empty(),
          "G7b the seat's groups list in ordinal order; the rival has none");

    // G8 — a unit dying.
    {
        world d = w;
        d.units.erase(mine[1]);
        prune_army_groups(d);
        const bool drop = army_group_members(d, c) == std::vector<entity_id>{ mine[3] } && d.army_groups.count(c) == 1;
        d.units.erase(mine[3]);
        const std::size_t removed = prune_army_groups(d);
        const bool ended = removed == 1 && d.army_groups.count(c) == 0 && d.army_groups.size() == 1;
        entity_id again = null_entity;
        const bool reuse = send(d, reg, seat, mine[5], null_entity, 1, &again) == corp_command_result::applied
            && d.army_groups.at(again).ordinal == 2;
        check(drop && ended && reuse,
              "G8 a dead unit drops off its group; the last one gone, the group ends and its ordinal is free again");
        check(prune_army_groups(d) == 0, "G8b the prune is idempotent");
    }

    // G9 — disband through the seam, then the day's prune.
    {
        world d = w;
        corp_command c2;
        c2.corp = seat; c2.as_nation = seat; c2.verb = corp_verb::disband_unit; c2.subject = mine[1];
        const bool gone1 = apply_corp_command(d, reg, c2) == corp_command_result::applied;
        c2.subject = mine[3];
        const bool gone2 = apply_corp_command(d, reg, c2) == corp_command_result::applied;
        prune_army_groups(d); // the day pass's call (economy_system.cpp, after disband_orphaned_units)
        check(gone1 && gone2 && d.army_groups.count(c) == 0 && d.army_groups.count(a) == 1,
              "G9 both units of the 2nd Army disbanded through the seam: the prune ends it, the 1st stands");
    }

    // G10 — save / load.
    {
        const std::string bytes = to_bytes(w);
        world back;
        const bool ok = from_bytes(bytes, back);
        check(ok && to_bytes(back) == bytes && back.state_hash(1) == w.state_hash(1)
                  && back.army_groups.size() == 2 && back.army_groups.at(a).ordinal == 1
                  && back.army_groups.at(c).ordinal == 2
                  && army_group_members(back, a) == army_group_members(w, a)
                  && army_group_members(back, c) == army_group_members(w, c),
              "G10 groups and memberships round-trip the save byte-identical, hash equal");
        check(w.state_hash(1) != before_any.state_hash(1), "G10b a grouped world hashes apart from the ungrouped one");
    }

    // G11 — determinism.
    {
        world x = before_any, y = before_any;
        for (world* p : { &x, &y })
        {
            const entity_id g1 = form(*p, reg, seat, { mine[0], mine[1], mine[2], mine[3] });
            entity_id g2 = null_entity;
            send(*p, reg, seat, mine[2], null_entity, 1, &g2);
            send(*p, reg, seat, mine[3], g2, 0);
            send(*p, reg, seat, mine[0], null_entity, 0);
            (void)g1;
        }
        check(to_bytes(x) == to_bytes(y), "G11 the same sequence on two copies gives byte-identical streams");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
