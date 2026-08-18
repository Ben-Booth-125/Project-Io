#include "works_roster.hpp"

#include "settlement.hpp"

#include <algorithm>

// ---------------------------------------------------------------------------
// Works-table LOGIC (BL-321). Lua-free by design — the data lives in
// scripts/works.lua and is loaded by works_registry.cpp, but the gate rule and
// the availability derivation stay here so the SDL/Lua-free world superset and
// the headless harnesses link without the Lua-bound TU.
// ---------------------------------------------------------------------------

namespace
{

/// How demanding a gate is, as one number — used only to order the available
/// set so the cheapest thing a polity could raise reads first. Not a cost.
int gate_burden(const work_gate& g)
{
    return g.ore_q + g.farm_q + g.port_q + g.energy_q
         + static_cast<int>(g.population / 100);
}

} // namespace

bool work_gate_met(const work_gate& g, const region& p)
{
    if (g.ore_q      > 0 && p.ore_q      < g.ore_q)      return false;
    if (g.farm_q     > 0 && p.farm_q     < g.farm_q)     return false;
    if (g.port_q     > 0 && p.port_q     < g.port_q)     return false;
    if (g.energy_q   > 0 && p.energy_q   < g.energy_q)   return false;
    if (g.population > 0 && p.population < g.population) return false;
    return true;
}

std::vector<const work_row*> works_registry::available(const region& p, roster_band band) const
{
    std::vector<const work_row*> out;
    for (const work_row& r : m_rows)
    {
        // Cumulative, exactly as the unit roster's bands are.
        if (static_cast<int>(r.band) > static_cast<int>(band)) continue;
        if (!work_gate_met(r.gate, p)) continue;
        out.push_back(&r);
    }
    // Stable order, cheapest gate first. std::stable_sort so equal burdens keep
    // authored order and the result stays a pure function of the inputs — this
    // runs inside a deterministic generation pass.
    std::stable_sort(out.begin(), out.end(),
                     [](const work_row* a, const work_row* b)
                     { return gate_burden(a->gate) < gate_burden(b->gate); });
    return out;
}

bool apply_work_to_region(region& p, const works_registry& reg, int id)
{
    if (id < 0) return false;
    const std::size_t i = static_cast<std::size_t>(id);
    if (i >= reg.size() || i >= works_mask_bits) return false;

    const uint32_t bit = uint32_t{1} << i;
    if ((p.works_built & bit) != 0) return false; // Already standing.

    const work_row* r = reg.row_at(i);
    if (r == nullptr) return false;

    p.works_built |= bit;
    p.work_capacity_mod   += r->effect.capacity_mod;
    p.work_manpower_mod   += r->effect.manpower_mod;
    p.work_reach_mod      += r->effect.reach_mod;
    p.work_defence_mod    += r->effect.defence_mod;
    p.work_industrial_mod += r->effect.industrial_mod;
    return true;
}
