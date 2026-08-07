// ---------------------------------------------------------------------------
// Headless Era -1 works-table harness (BL-321 S1; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises src/world/works_roster.cpp — the pre-history building list. The
// table is authored and availability is derived, so everything here is a pure
// function of a hand-built province: no world generation involved.
//
//   R1  Every row carries a generic mechanism name, a band, and at least one
//       non-zero effect field — no row exists that does nothing.
//   R2  Availability is DERIVED from the ground: a province that fails a gate
//       is not offered that row, and two provinces at the same band with
//       different endowments are offered different sets (asymmetry is the point).
//   R3  Bands are CUMULATIVE — an industrial-band province is still offered
//       classical works; nothing un-invents a granary.
//   R4  available_works is deterministic and order-stable: repeated calls on
//       the same province return an identical sequence.
//   R5  total_work_effect sums per-mille effects additively and SKIPS unknown
//       ids rather than reading a neighbouring row.
//   R6  work_id round-trips a row back to its index, and rejects a foreign
//       pointer with -1.
//   R7  Every reach-bearing row is reachable by some province — the reach_mod
//       field is the counter-move to BL-316's burden of breadth, so a reach
//       work no ground can ever gate would be dead weight.
//
// The process exits non-zero if any assertion FAILs.

#include "world/works_roster.hpp"
#include "world/settlement.hpp"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

/// A province with everything dialled up — passes every gate in the table.
province rich_province()
{
    province p;
    p.ore_q = p.farm_q = p.port_q = p.energy_q = 1000;
    p.population = 1'000'000;
    return p;
}

/// A province with nothing but people — passes only the ungated rows.
province bare_province()
{
    province p;
    p.ore_q = p.farm_q = p.port_q = p.energy_q = 0;
    p.population = 1'000'000;
    return p;
}

std::vector<std::string> names_of(const std::vector<const work_row*>& rows)
{
    std::vector<std::string> out;
    out.reserve(rows.size());
    for (const work_row* r : rows) out.emplace_back(r->name);
    return out;
}

bool has_effect(const work_effect& e)
{
    return e.capacity_mod || e.manpower_mod || e.reach_mod
        || e.defence_mod  || e.industrial_mod;
}

} // namespace

int main()
{
    const std::vector<work_row>& table = works_table();

    // --- R1: every row is well-formed and does something -------------------
    {
        bool named = true, effective = true, unique = true;
        std::set<std::string> seen;
        for (const work_row& r : table)
        {
            if (r.name == nullptr || std::strlen(r.name) == 0) named = false;
            if (!has_effect(r.effect))                          effective = false;
            if (!seen.insert(r.name).second)                    unique = false;
        }
        check(!table.empty(), "R1 the works table is non-empty");
        check(named,     "R1 every row carries a name");
        check(unique,    "R1 row names are unique");
        check(effective, "R1 every row has at least one non-zero effect field");
    }

    // --- R2: availability is derived from the ground -----------------------
    {
        const province rich = rich_province();
        const province bare = bare_province();

        const auto rich_rows = available_works(rich, roster_band::industrial);
        const auto bare_rows = available_works(bare, roster_band::industrial);

        check(rich_rows.size() == table.size(),
              "R2 a province meeting every gate is offered the whole table");
        check(bare_rows.size() < rich_rows.size(),
              "R2 a province with no endowment is offered strictly fewer works");
        check(!bare_rows.empty(),
              "R2 an endowment-poor province is still offered the ungated works");

        // Asymmetry: ore country and port country diverge at the same band.
        province ore = bare_province();  ore.ore_q  = 1000;
        province port = bare_province(); port.port_q = 1000;
        const auto ore_names  = names_of(available_works(ore,  roster_band::medieval));
        const auto port_names = names_of(available_works(port, roster_band::medieval));
        check(ore_names != port_names,
              "R2 two provinces at the same band with different ground field different works");

        // A population floor is a real gate, not decoration.
        province depopulated = rich_province();
        depopulated.population = 0;
        check(available_works(depopulated, roster_band::industrial).empty(),
              "R2 a province with no population is offered nothing");
    }

    // --- R3: bands are cumulative ------------------------------------------
    {
        const province rich = rich_province();
        const auto classical  = available_works(rich, roster_band::classical);
        const auto industrial = available_works(rich, roster_band::industrial);

        check(classical.size() < industrial.size(),
              "R3 a later band offers strictly more rows than an earlier one");

        bool classical_survives = false;
        for (const work_row* r : industrial)
            if (r->band == roster_band::classical) { classical_survives = true; break; }
        check(classical_survives,
              "R3 an industrial-band province is still offered classical works");

        bool no_future_rows = true;
        for (const work_row* r : classical)
            if (static_cast<int>(r->band) > static_cast<int>(roster_band::classical))
                no_future_rows = false;
        check(no_future_rows,
              "R3 a classical-band province is offered no later-band work");
    }

    // --- R4: deterministic, order-stable -----------------------------------
    {
        const province rich = rich_province();
        const auto a = names_of(available_works(rich, roster_band::gunpowder));
        const auto b = names_of(available_works(rich, roster_band::gunpowder));
        check(a == b, "R4 repeated calls return an identical sequence");
    }

    // --- R5: effects sum additively; unknown ids are skipped ---------------
    {
        const work_effect none = total_work_effect({});
        check(!has_effect(none), "R5 an empty build list yields a zero effect");

        const work_effect one  = total_work_effect({0});
        const work_effect twice = total_work_effect({0, 0});
        check(twice.capacity_mod   == one.capacity_mod   * 2 &&
              twice.manpower_mod   == one.manpower_mod   * 2 &&
              twice.reach_mod      == one.reach_mod      * 2 &&
              twice.defence_mod    == one.defence_mod    * 2 &&
              twice.industrial_mod == one.industrial_mod * 2,
              "R5 effects are additive in per-mille");

        const work_effect with_junk = total_work_effect({0, 250});
        check(with_junk.capacity_mod   == one.capacity_mod &&
              with_junk.reach_mod      == one.reach_mod &&
              with_junk.industrial_mod == one.industrial_mod,
              "R5 an out-of-range id is skipped, not clamped onto a real row");
    }

    // --- R6: work_id round-trips -------------------------------------------
    {
        bool round_trips = true;
        for (std::size_t i = 0; i < table.size(); ++i)
            if (work_id(&table[i]) != static_cast<int>(i)) round_trips = false;
        check(round_trips, "R6 work_id returns each row's own index");
        check(work_id(nullptr) == -1, "R6 work_id rejects null with -1");

        const work_row foreign = {"Not In Table", roster_band::classical, {}, {1, 0, 0, 0, 0}, 1};
        check(work_id(&foreign) == -1, "R6 work_id rejects a foreign pointer with -1");
    }

    // --- R7: every reach work is reachable by some ground ------------------
    // reach_mod is the field BL-316's burden of breadth is answered with. A row
    // carrying reach that no province could ever gate would be dead weight.
    {
        const province rich = rich_province();
        const auto all = available_works(rich, roster_band::industrial);

        std::set<std::string> offered;
        for (const work_row* r : all) offered.insert(r->name);

        bool all_reachable = true;
        int reach_rows = 0;
        for (const work_row& r : table)
            if (r.effect.reach_mod != 0)
            {
                ++reach_rows;
                if (offered.find(r.name) == offered.end()) all_reachable = false;
            }
        check(reach_rows > 0,      "R7 the table carries reach-bearing works at all");
        check(all_reachable,       "R7 every reach-bearing work is gateable by some ground");

        // And at least one is available to a bare province — a polity with poor
        // ground must still have SOME way to buy reach, or breadth is a ceiling.
        bool bare_has_reach = false;
        for (const work_row* r : available_works(bare_province(), roster_band::classical))
            if (r->effect.reach_mod != 0) { bare_has_reach = true; break; }
        check(bare_has_reach,
              "R7 an endowment-poor province can still raise a reach work");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
