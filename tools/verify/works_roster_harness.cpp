// ---------------------------------------------------------------------------
// Headless Era -1 works-table harness (BL-321; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises the works LOGIC in src/world/works_roster.cpp — the gate rule, the
// availability derivation, and effect accumulation. Everything here is a pure
// function of a hand-built region and a hand-built registry: no world
// generation, and no Lua.
//
// WHY THE TABLE ITSELF IS NOT ASSERTED HERE. The works data moved to
// scripts/works.lua (Ben, 2026-08-07, NR-081), so this harness cannot see it
// without linking the Lua-bound TU and losing its headless property. The data
// invariants that used to live here — no duplicate names, every row has an
// effect, a reach work exists and one of them is reachable on poor ground —
// moved INTO works_registry::load_from_lua, which throws on violation. They now
// run on every startup rather than only when someone remembers to run a
// harness, which is a stronger guarantee, not a weaker one. This file covers
// what that check cannot: that the derivation over a valid table is correct.
//
//   R1  work_gate_met is a pure AND over the five axes, and an absent gate
//       (zero) never rejects.
//   R2  Availability is DERIVED from the ground: a region failing a gate is
//       not offered that row, and two regions with different endowments at
//       the same band are offered different sets.
//   R3  Bands are CUMULATIVE — a later band offers everything an earlier one
//       does; nothing un-invents a granary.
//   R4  available() is deterministic and order-stable, cheapest gate first.
//   R5  total_effect sums per-mille effects additively and SKIPS unknown ids
//       rather than reading a neighbouring row.
//   R6  id_of/row_at round-trip a row by name, and reject the unknown.
//
// The process exits non-zero if any assertion FAILs.

#include "world/works_roster.hpp"
#include "world/settlement.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

work_row make_row(const char* name, roster_band band, work_gate gate, work_effect eff, int weight = 100)
{
    work_row r;
    r.name = name; r.band = band; r.gate = gate; r.effect = eff; r.weight = weight;
    return r;
}

/// A registry shaped like works.lua — an ungated reach row, endowment-gated rows
/// on each axis, and rows in every band. Deliberately hand-built rather than
/// loaded, so this harness stays Lua-free.
works_registry make_registry()
{
    works_registry reg;
    // name                band                     gate{ore,farm,port,energy,pop}  effect{cap,man,reach,def,ind}
    reg.add_row(make_row("Way Station",   roster_band::classical,  {   0,   0,   0,   0,  3000}, {   0,   0, 120,   0,   0}));
    reg.add_row(make_row("Granary",       roster_band::classical,  {   0, 300,   0,   0,  4000}, { 180,  40,   0,   0,   0}));
    reg.add_row(make_row("Ore Pits",      roster_band::classical,  { 400,   0,   0,   0,  5000}, {   0,  60,   0,   0, 120}));
    reg.add_row(make_row("Harbour Mole",  roster_band::classical,  {   0,   0, 400,   0,  5000}, { 120,   0, 200,   0,  40}));
    reg.add_row(make_row("Stone Fortress",roster_band::medieval,   { 350,   0,   0,   0, 15000}, {   0,  80,   0, 480,   0}));
    reg.add_row(make_row("Bastion Fort",  roster_band::gunpowder,  { 500,   0,   0, 350, 25000}, {   0, 100,   0, 640,   0}));
    reg.add_row(make_row("Rail Head",     roster_band::industrial, { 550,   0,   0, 550, 45000}, { 220,  80, 700,   0, 380}));
    return reg;
}

region rich_region()
{
    region p;
    p.ore_q = p.farm_q = p.port_q = p.energy_q = 1000;
    p.population = 1'000'000;
    return p;
}

region bare_region()
{
    region p;
    p.ore_q = p.farm_q = p.port_q = p.energy_q = 0;
    p.population = 1'000'000;
    return p;
}

std::vector<std::string> names_of(const std::vector<const work_row*>& rows)
{
    std::vector<std::string> out;
    out.reserve(rows.size());
    for (const work_row* r : rows) out.push_back(r->name);
    return out;
}

} // namespace

int main()
{
    const works_registry reg = make_registry();

    // --- R1: the gate rule -------------------------------------------------
    {
        const region rich = rich_region();
        const region bare = bare_region();

        check(work_gate_met(work_gate{}, bare),
              "R1 an empty gate accepts any region");
        check(work_gate_met(work_gate{0, 300, 0, 0, 4000}, rich),
              "R1 a met gate accepts");
        check(!work_gate_met(work_gate{0, 300, 0, 0, 4000}, bare),
              "R1 an unmet endowment axis rejects");

        region few;
        few.ore_q = few.farm_q = few.port_q = few.energy_q = 1000;
        few.population = 10;
        check(!work_gate_met(work_gate{0, 0, 0, 0, 4000}, few),
              "R1 an unmet population floor rejects on its own");

        // Every axis is ANDed: one failure is enough.
        region ore_only = bare_region();
        ore_only.ore_q = 1000;
        check(!work_gate_met(work_gate{400, 300, 0, 0, 0}, ore_only),
              "R1 the gate is an AND — meeting one axis is not enough");
    }

    // --- R2: availability is derived from the ground -----------------------
    {
        const auto rich_rows = reg.available(rich_region(), roster_band::industrial);
        const auto bare_rows = reg.available(bare_region(), roster_band::industrial);

        check(rich_rows.size() == reg.size(),
              "R2 a region meeting every gate is offered the whole table");
        check(bare_rows.size() < rich_rows.size(),
              "R2 a region with no endowment is offered strictly fewer works");
        check(!bare_rows.empty(),
              "R2 an endowment-poor region is still offered the ungated works");

        region ore = bare_region();  ore.ore_q  = 1000;
        region port = bare_region(); port.port_q = 1000;
        check(names_of(reg.available(ore,  roster_band::medieval)) !=
              names_of(reg.available(port, roster_band::medieval)),
              "R2 two regions at the same band with different ground field different works");

        region depopulated = rich_region();
        depopulated.population = 0;
        check(reg.available(depopulated, roster_band::industrial).empty(),
              "R2 a region with no population is offered nothing");
    }

    // --- R3: bands are cumulative ------------------------------------------
    {
        const region rich = rich_region();
        const auto classical  = names_of(reg.available(rich, roster_band::classical));
        const auto industrial = names_of(reg.available(rich, roster_band::industrial));

        check(classical.size() < industrial.size(),
              "R3 a later band offers strictly more rows than an earlier one");

        bool earlier_survives = true;
        for (const std::string& n : classical)
        {
            bool found = false;
            for (const std::string& m : industrial) if (m == n) { found = true; break; }
            if (!found) earlier_survives = false;
        }
        check(earlier_survives,
              "R3 every classical row remains offered at the industrial band");

        bool no_future_rows = true;
        for (const work_row* r : reg.available(rich, roster_band::classical))
            if (static_cast<int>(r->band) > static_cast<int>(roster_band::classical))
                no_future_rows = false;
        check(no_future_rows,
              "R3 a classical-band region is offered no later-band work");
    }

    // --- R4: deterministic, order-stable, cheapest gate first --------------
    {
        const region rich = rich_region();
        const auto a = names_of(reg.available(rich, roster_band::gunpowder));
        const auto b = names_of(reg.available(rich, roster_band::gunpowder));
        check(a == b, "R4 repeated calls return an identical sequence");
        check(!a.empty() && a.front() == "Way Station",
              "R4 the cheapest-gated work is offered first");
    }

    // --- R5: effects sum additively; unknown ids are skipped ---------------
    {
        const work_effect none = reg.total_effect({});
        check(none.capacity_mod == 0 && none.reach_mod == 0,
              "R5 an empty build list yields a zero effect");

        const work_effect one   = reg.total_effect({0});
        const work_effect twice = reg.total_effect({0, 0});
        check(twice.reach_mod == one.reach_mod * 2,
              "R5 effects are additive in per-mille");

        const work_effect mixed = reg.total_effect({0, 1});
        check(mixed.reach_mod == 120 && mixed.capacity_mod == 180 && mixed.manpower_mod == 40,
              "R5 distinct rows accumulate field by field");

        const work_effect with_junk = reg.total_effect({0, 250});
        check(with_junk.reach_mod == one.reach_mod && with_junk.capacity_mod == 0,
              "R5 an out-of-range id is skipped, not clamped onto a real row");
    }

    // --- R6: id_of / row_at round-trip -------------------------------------
    {
        bool round_trips = true;
        for (std::size_t i = 0; i < reg.size(); ++i)
        {
            const work_row* r = reg.row_at(i);
            if (r == nullptr || reg.id_of(r->name) != static_cast<int>(i)) round_trips = false;
        }
        check(round_trips, "R6 id_of returns each row's own index");
        check(reg.id_of("No Such Work") == -1, "R6 id_of rejects an unknown name with -1");
        check(reg.row_at(reg.size()) == nullptr, "R6 row_at rejects an out-of-range id with null");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
