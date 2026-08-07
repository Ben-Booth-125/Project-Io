#include "works_roster.hpp"

#include "settlement.hpp"

#include <algorithm>

// ---------------------------------------------------------------------------
// The works table (BL-321). Rows are authored, availability is derived.
//
// Names are generic mechanism nouns throughout — io-standing-rules § Terms &
// docs. "Way Station" describes what the work is; it is not a people, a place,
// or an Earth institution, and none of these may read as recognisably Latin.
// The sim renders them through Kepler's own cultures.
//
// Effect magnitudes are authored by judgement, in per-mille, and are meant to
// be calibrated against BL-275's sweep once BL-316's terrain-weighted reach
// exists to discount. Until then `reach_mod` has nothing to act on — which is
// why this file is S1 and the wiring is S4.
// ---------------------------------------------------------------------------

namespace
{

// Gates are thresholds on the province endowment windows the settlement pass
// already surveyed, so nothing here needs a new generation input. Population
// floors are absolute headcounts against province::population.
const std::vector<work_row> g_table = {
    // --- Classical (ladder T1) --------------------------------------------
    // The floor works: earthworks, stores, a wall. Nothing here needs metal it
    // cannot dig locally, which is why the classical band gates lightly.
    {"Granary",        roster_band::classical,
     /*gate*/ {   0, 300,   0,   0,  4000},
     /*eff */ { 180,  40,   0,   0,   0}, 220},

    {"Channel Works",  roster_band::classical,
     /*gate*/ {   0, 450,   0,   0,  6000},
     /*eff */ { 240,   0,  40,   0,  30}, 190},

    {"Ore Pits",       roster_band::classical,
     /*gate*/ { 400,   0,   0,   0,  5000},
     /*eff */ {   0,  60,   0,   0, 120}, 170},

    {"Wall Circuit",   roster_band::classical,
     /*gate*/ {   0,   0,   0,   0,  8000},
     /*eff */ {  40,   0,   0, 250,   0}, 200},

    // The first reach work, and the cheapest. A polity that cannot yet afford
    // stone still wants the road stage between its provinces.
    {"Way Station",    roster_band::classical,
     /*gate*/ {   0,   0,   0,   0,  3000},
     /*eff */ {   0,   0, 120,   0,   0}, 240},

    {"Harbour Mole",   roster_band::classical,
     /*gate*/ {   0,   0, 400,   0,  5000},
     /*eff */ { 120,   0, 200,   0,  40}, 150},

    // --- Medieval (ladder T2-T3) ------------------------------------------
    {"Water Mill",     roster_band::medieval,
     /*gate*/ {   0, 400,   0,   0, 12000},
     /*eff */ { 220,   0,   0,   0, 140}, 180},

    {"Stone Fortress",  roster_band::medieval,
     /*gate*/ { 350,   0,   0,   0, 15000},
     /*eff */ {   0,  80,   0, 480,   0}, 210},

    {"Guild Quarter",  roster_band::medieval,
     /*gate*/ {   0,   0,   0,   0, 20000},
     /*eff */ { 160,   0,  60,   0, 220}, 170},

    {"Deepwater Wharf", roster_band::medieval,
     /*gate*/ {   0,   0, 550,   0, 14000},
     /*eff */ { 140,   0, 300,   0, 120}, 140},

    {"Span Bridge",    roster_band::medieval,
     /*gate*/ { 300,   0,   0,   0, 10000},
     /*eff */ {   0,   0, 260,  60,   0}, 190},

    // --- Gunpowder (ladder T4) --------------------------------------------
    // Energy stands in for saltpetre/sulphur here for the same reason it does
    // in unit_roster.cpp: there is no signal of its own. Named, not faked.
    {"Bastion Fort",   roster_band::gunpowder,
     /*gate*/ { 500,   0,   0, 350, 25000},
     /*eff */ {   0, 100,   0, 640,   0}, 200},

    {"Powder Mill",    roster_band::gunpowder,
     /*gate*/ {   0,   0,   0, 450, 22000},
     /*eff */ {   0, 180,   0,  80, 160}, 160},

    {"Counting House", roster_band::gunpowder,
     /*gate*/ {   0,   0,   0,   0, 30000},
     /*eff */ { 200,   0, 100,   0, 300}, 180},

    {"Cut Canal",      roster_band::gunpowder,
     /*gate*/ {   0, 300,   0,   0, 28000},
     /*eff */ { 120,   0, 420,   0, 180}, 170},

    {"Naval Yard",     roster_band::gunpowder,
     /*gate*/ {   0,   0, 600, 300, 26000},
     /*eff */ { 100,  60, 340,   0, 200}, 130},

    // --- Industrial (ladder T5-T6) ----------------------------------------
    // These rows are the ANCESTORS of components.hpp's building_type: a
    // province that lit Blast Works is where the 1960 smelter sits and why its
    // substrate_density is high. Recording that correspondence is in scope;
    // consuming it in campaign generation is a follow-on, not a slice here.
    {"Blast Works",    roster_band::industrial,
     /*gate*/ { 600,   0,   0, 500, 40000},
     /*eff */ { 180, 140,   0,   0, 520}, 240},

    {"Rail Head",      roster_band::industrial,
     /*gate*/ { 550,   0,   0, 550, 45000},
     /*eff */ { 220,  80, 700,   0, 380}, 260},

    {"Signal Line",    roster_band::industrial,
     /*gate*/ { 450,   0,   0, 400, 35000},
     /*eff */ {   0,   0, 380,  80, 220}, 190},

    {"Arsenal",        roster_band::industrial,
     /*gate*/ { 650,   0,   0, 600, 50000},
     /*eff */ {   0, 320,   0, 260, 180}, 180},

    {"Coaling Station", roster_band::industrial,
     /*gate*/ {   0,   0, 650, 550, 38000},
     /*eff */ {  80,   0, 480,   0, 200}, 140},
};

bool gate_met(const work_gate& g, const province& p)
{
    if (g.ore_q      > 0 && p.ore_q      < g.ore_q)      return false;
    if (g.farm_q     > 0 && p.farm_q     < g.farm_q)     return false;
    if (g.port_q     > 0 && p.port_q     < g.port_q)     return false;
    if (g.energy_q   > 0 && p.energy_q   < g.energy_q)   return false;
    if (g.population > 0 && p.population < g.population) return false;
    return true;
}

/// How demanding a gate is, as one number — used only to order the available
/// set so the cheapest thing a polity could raise reads first. Not a cost.
int gate_burden(const work_gate& g)
{
    return g.ore_q + g.farm_q + g.port_q + g.energy_q
         + static_cast<int>(g.population / 100);
}

} // namespace

const std::vector<work_row>& works_table() { return g_table; }

std::vector<const work_row*> available_works(const province& p, roster_band band)
{
    std::vector<const work_row*> out;
    for (const work_row& r : g_table)
    {
        // Cumulative, exactly as the unit roster's bands are.
        if (static_cast<int>(r.band) > static_cast<int>(band)) continue;
        if (!gate_met(r.gate, p)) continue;
        out.push_back(&r);
    }
    // Stable order, cheapest gate first. std::stable_sort so equal burdens keep
    // table order and the result stays a pure function of the inputs — this
    // runs inside a deterministic generation pass.
    std::stable_sort(out.begin(), out.end(),
                     [](const work_row* a, const work_row* b)
                     { return gate_burden(a->gate) < gate_burden(b->gate); });
    return out;
}

int work_id(const work_row* row)
{
    if (row == nullptr)              return -1;
    if (row < g_table.data())        return -1;
    const std::ptrdiff_t i = row - g_table.data();
    if (i >= static_cast<std::ptrdiff_t>(g_table.size())) return -1;
    return static_cast<int>(i);
}

work_effect total_work_effect(const std::vector<uint8_t>& built)
{
    work_effect t;
    for (const uint8_t id : built)
    {
        // Unknown ids are SKIPPED, not clamped: a record written against an
        // older, shorter table shrinks gracefully rather than reading a
        // neighbouring row's effects as if they were its own.
        if (id >= g_table.size()) continue;
        const work_effect& e = g_table[id].effect;
        t.capacity_mod   += e.capacity_mod;
        t.manpower_mod   += e.manpower_mod;
        t.reach_mod      += e.reach_mod;
        t.defence_mod    += e.defence_mod;
        t.industrial_mod += e.industrial_mod;
    }
    return t;
}
