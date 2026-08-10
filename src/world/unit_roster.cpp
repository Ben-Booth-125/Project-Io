#include "unit_roster.hpp"

#include "settlement.hpp"
#include "world.hpp"

#include <algorithm>

// ---------------------------------------------------------------------------
// The roster table (BL-274). Rows are authored, availability is derived.
//
// Names are generic mechanism names throughout — io-standing-rules § Terms &
// docs. "Stirrup Horse" describes what the unit is; it is not a people, a
// place, or an Earth army. The sim renders these through Kepler's own cultures.
// ---------------------------------------------------------------------------

namespace
{

// Gates are thresholds on the province endowment windows the settlement pass
// already surveyed, so nothing here needs a new generation input.
const std::vector<roster_row> g_table = {
    // --- Classical (ladder T1): massed iron infantry, siegecraft -----------
    {"Levy Spear",      roster_band::classical, unit_class::infantry, {  0,   0,   0,   0},   0, 380},
    {"Iron Foot",       roster_band::classical, unit_class::infantry, {450,   0,   0,   0},  70, 260},
    {"Skirmisher",      roster_band::classical, unit_class::ranged,   {  0,   0,   0,   0},   0, 200},
    {"Massed Bow",      roster_band::classical, unit_class::ranged,   {300,   0,   0,   0},  50, 160},
    // Light horse only at this band. The stirrup is a MEDIEVAL unlock (the
    // T1/T2 boundary settled 2026-08-04), so the classical band gets scouts and
    // flankers, never the heavy charge.
    {"Light Horse",     roster_band::classical, unit_class::cavalry,  {  0, 500,   0,   0},   0, 150},
    {"Siege Train",     roster_band::classical, unit_class::siege,    {350,   0,   0,   0},  40,  90},
    {"Coastal Galley",  roster_band::classical, unit_class::naval,    {  0,   0, 500,   0},   0,  80},

    // --- Medieval (ladder T2-T3): the stirrup, the fortress, plate ---------
    {"Stirrup Horse",   roster_band::medieval,  unit_class::cavalry,  {400, 550,   0,   0}, 150, 210},
    {"Plated Foot",     roster_band::medieval,  unit_class::infantry, {600,   0,   0,   0}, 130, 220},
    {"Crossbow Corps",  roster_band::medieval,  unit_class::ranged,   {450,   0,   0,   0}, 110, 170},
    {"Counterweight Engine", roster_band::medieval, unit_class::siege,{500,   0,   0,   0}, 120, 100},

    // --- Gunpowder (ladder T4): line, artillery fortress, broadside -------
    // Energy stands in for saltpetre/sulphur, which has no signal of its own.
    {"Line Infantry",   roster_band::gunpowder, unit_class::infantry, {550,   0,   0, 400}, 240, 300},
    {"Field Artillery", roster_band::gunpowder, unit_class::siege,    {600,   0,   0, 450}, 260, 150},
    {"Dragoon",         roster_band::gunpowder, unit_class::cavalry,  {500, 500,   0, 400}, 210, 140},
    {"Broadside Ship",  roster_band::gunpowder, unit_class::naval,    {550,   0, 600, 400},   0, 110},

    // --- Industrial (ladder T5-T6): rifle, ironclad, then armour ----------
    {"Rifle Regiment",  roster_band::industrial, unit_class::infantry,{650,   0,   0, 550}, 380, 340},
    {"Breech Artillery",roster_band::industrial, unit_class::siege,   {700,   0,   0, 600}, 400, 170},
    {"Ironclad",        roster_band::industrial, unit_class::naval,   {700,   0, 650, 600},   0, 120},
    {"Mechanised Column", roster_band::industrial, unit_class::cavalry,{750,  0,   0, 700}, 420, 160},
};

/// The shared primitive both gate paths (province, campaign) funnel through —
/// a threshold check against four 0-1000 axis values, whatever supplied them.
bool gate_met(const roster_gate& g, int ore_q, int farm_q, int port_q, int energy_q)
{
    if (g.ore_q    > 0 && ore_q    < g.ore_q)    return false;
    if (g.farm_q   > 0 && farm_q   < g.farm_q)   return false;
    if (g.port_q   > 0 && port_q   < g.port_q)   return false;
    if (g.energy_q > 0 && energy_q < g.energy_q) return false;
    return true;
}

bool gate_met(const roster_gate& g, const province& p)
{
    return gate_met(g, p.ore_q, p.farm_q, p.port_q, p.energy_q);
}

} // namespace

const std::vector<roster_row>& unit_roster_table() { return g_table; }

roster_band roster_band_for_capacity(int military_capacity)
{
    // The ladder's six capacity bands collapse onto the roster's four exactly
    // as ANCIENT_TECH_LADDER.md § Shape sets out: T1 | T2-T3 | T4 | T5-T6.
    if (military_capacity <= 1) return roster_band::classical;
    if (military_capacity <= 3) return roster_band::medieval;
    if (military_capacity <= 4) return roster_band::gunpowder;
    return roster_band::industrial;
}

std::vector<const roster_row*> available_rows(const province& p, roster_band band)
{
    std::vector<const roster_row*> out;
    for (const roster_row& r : g_table)
    {
        // Bands are CUMULATIVE — nothing un-invents a spear, so an industrial
        // polity still fields levies where its ground cannot pay for better.
        if (static_cast<int>(r.band) > static_cast<int>(band)) continue;
        if (!gate_met(r.gate, p)) continue;
        out.push_back(&r);
    }
    return out;
}

namespace {

bool corp_owns_port(const world& w, entity_id corp)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end()) return false;
    for (const entity_id asset : cit->second.assets)
    {
        const auto bit = w.buildings.find(asset);
        if (bit != w.buildings.end() && bit->second.type == building_type::port)
            return true;
    }
    return false;
}

} // namespace

float corp_stockpile_total(const world& w, entity_id corp, resource_type res)
{
    // Reads the live (corp, body) pool store — the per-building
    // stockpile_component is authored empty and never credited in L3
    // (world.hpp § corp_body_pools). The map is keyed (corp, body), so the
    // corp's pools sit in one contiguous ascending-body run.
    float total = 0.0f;
    for (auto it = w.corp_body_pools.lower_bound({corp, entity_id{0}});
         it != w.corp_body_pools.end() && it->first.first == corp; ++it)
        total += it->second.quantities[static_cast<std::size_t>(res)];
    return total;
}

campaign_roster_gate_input campaign_gate_input(const world& w, entity_id corp)
{
    campaign_roster_gate_input g;
    const auto has = [&](resource_type r) { return corp_stockpile_total(w, corp, r) > 0.0f; };

    // ore_q: metallurgy access — refined steel or either raw ore counts.
    g.ore_q = (has(resource_type::steel) || has(resource_type::iron_ore)
               || has(resource_type::iron_nickel_ore)) ? 1000 : 0;
    // farm_q: PROXY FOR PASTURE, same convention the province table already
    // uses (the roster has no horse/pasture signal at all) — food access stands in.
    g.farm_q = (has(resource_type::food_rations)
                || has(resource_type::agricultural_produce)) ? 1000 : 0;
    // port_q: a building, not a resource — does the corp hold a port anywhere.
    g.port_q = corp_owns_port(w, corp) ? 1000 : 0;
    // energy_q: PROXY FOR SALTPETRE/FUEL, same convention as the province table.
    g.energy_q = (has(resource_type::coal) || has(resource_type::refined_fuel)
                  || has(resource_type::petroleum)) ? 1000 : 0;
    return g;
}

std::vector<const roster_row*> available_rows(const world& w, entity_id corp, roster_band band)
{
    const campaign_roster_gate_input in = campaign_gate_input(w, corp);
    std::vector<const roster_row*> out;
    for (const roster_row& r : g_table)
    {
        if (static_cast<int>(r.band) > static_cast<int>(band)) continue;
        if (!gate_met(r.gate, in.ore_q, in.farm_q, in.port_q, in.energy_q)) continue;
        out.push_back(&r);
    }
    return out;
}

std::vector<army_stack_entry> roster_stack(int64_t         manpower,
                                           const province& p,
                                           roster_band     band,
                                           int             readiness_q)
{
    std::vector<army_stack_entry> stack;
    if (manpower <= 0) return stack;

    const std::vector<const roster_row*> rows = available_rows(p, band);
    if (rows.empty()) return stack;

    // Later bands crowd out earlier ones rather than sitting beside them at
    // equal weight: a rifle-era army is not half levy spears. A row's effective
    // weight falls off with each band it sits below the polity's own.
    int64_t total_weight = 0;
    std::vector<int> weights;
    weights.reserve(rows.size());
    for (const roster_row* r : rows)
    {
        const int gap = static_cast<int>(band) - static_cast<int>(r->band);
        int w = r->weight;
        for (int i = 0; i < gap; ++i) w = (w * 350) / 1000;
        if (w < 1) w = 1;
        weights.push_back(w);
        total_weight += w;
    }
    if (total_weight <= 0) return stack;

    uint16_t tag = 1;
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        army_stack_entry e;
        e.type_id = tag++;                     // Opaque to combat.cpp by contract.
        e.cls     = rows[i]->cls;
        e.count   = static_cast<int>((manpower * weights[i]) / total_weight);
        e.type_power_mod = rows[i]->power_mod + (readiness_q - 1000) / 10;
        if (e.count > 0) stack.push_back(e);
    }
    return stack;
}
