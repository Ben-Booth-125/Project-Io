#include "unit_roster.hpp"

#include "recipe_registry.hpp" // era_band — BL-461
#include "settlement.hpp"
#include "world.hpp"

#include <algorithm>

roster_band campaign_roster_band_for(era_band band)
{
    switch (band)
    {
        case era_band::ancient: return roster_band::classical;
        // Industrial, and the unset `any` default: the campaign's top band,
        // which is the campaign-only `modern` (BL-1422). Cumulative, so the
        // Era -1 rows stay available beside it.
        default:                return roster_band::modern;
    }
}

// ---------------------------------------------------------------------------
// The roster table (BL-274). Rows are authored, availability is derived.
//
// Names are generic mechanism names throughout — io-standing-rules § Terms &
// docs. "Stirrup Horse" describes what the unit is; it is not a people, a
// place, or an Earth army. The sim renders these through Kepler's own cultures.
// ---------------------------------------------------------------------------

namespace
{

// Gates are thresholds on the region endowment windows the settlement pass
// already surveyed, so nothing here needs a new generation input.
//
// BL-779 GAVE THE THREE NAVAL ROWS A REAL power_mod. All three were authored at
// 0 because the class scored zero regardless (combat.cpp), so the column was
// meaningless for them. They now sit on the SAME per-band ladder their
// contemporaries do rather than on a scale invented for ships: Coastal Galley 0
// beside Levy Spear and Light Horse, Broadside Ship 230 beside Line Infantry's
// 240 and Dragoon's 210, Ironclad 400 beside Breech Artillery's 400.
//
// The last column is the BL-778 traversal mask. It is omitted on every land row,
// which takes `k_domains_land` from the member's default — the exception is the
// thing worth writing down, and there are exactly three of them.
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
    {"Coastal Galley",  roster_band::classical, unit_class::naval,    {  0,   0, 500,   0},   0,  80, k_domains_naval},

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
    {"Broadside Ship",  roster_band::gunpowder, unit_class::naval,    {550,   0, 600, 400}, 230, 110, k_domains_naval},

    // --- Industrial (ladder T5-T6): rifle, ironclad, then armour ----------
    {"Rifle Regiment",  roster_band::industrial, unit_class::infantry,{650,   0,   0, 550}, 380, 340},
    {"Breech Artillery",roster_band::industrial, unit_class::siege,   {700,   0,   0, 600}, 400, 170},
    {"Ironclad",        roster_band::industrial, unit_class::naval,   {700,   0, 650, 600}, 400, 120, k_domains_naval},
    {"Mechanised Column", roster_band::industrial, unit_class::cavalry,{750,  0,   0, 700}, 420, 160},

    // --- Modern (CAMPAIGN ONLY, BL-1422): one row per land class -----------
    // APPENDED, never interleaved: `unit_component::type` is a saved index into
    // this table. No ladder capacity maps to `modern` (roster_band_for_capacity
    // stops at industrial), so the Era -1 sim never fields one of these.
    //
    // Each row DECLARES its land class (the last column); the class carries the
    // role (MILITARY.md § Land — the classes). power_mod carries the generation
    // and stays modest — one step above the industrial rows (380-420), spread
    // only 440-500 so the class, not the modifier, decides the fight:
    // unit_land_stats scales the class stats by 1000 + power_mod.
    //
    // Gates: ore for steel-heavy kit, energy for fuelled vehicles. Infantry is
    // ungated — the floor a modern state can always raise, as Levy Spear is the
    // floor of the ancient bands. The campaign gate is presence-based (0/1000),
    // so the thresholds are ordinal here; the ordering still says what each needs.
    //
    // `cls` is the nearest Era -1 class, read by what still keys on it (the march
    // points per class, the stack adapter): foot and trucks march as infantry,
    // tracked and wheeled armour as cavalry, guns as siege, air defence as ranged.
    {"Rifle Infantry",       roster_band::modern, unit_class::infantry, {  0, 0, 0,   0}, 440, 340, k_domains_land, land_class::infantry},
    {"Motor Rifles",         roster_band::modern, unit_class::infantry, {300, 0, 0, 500}, 450, 200, k_domains_land, land_class::motorised},
    {"Mechanised Infantry",  roster_band::modern, unit_class::cavalry,  {600, 0, 0, 600}, 470, 160, k_domains_land, land_class::mechanised},
    {"Medium Armour",        roster_band::modern, unit_class::cavalry,  {800, 0, 0, 700}, 500, 120, k_domains_land, land_class::armour},
    {"Light Armour",         roster_band::modern, unit_class::cavalry,  {500, 0, 0, 600}, 460, 100, k_domains_land, land_class::light_armour},
    {"Field Gun Battery",    roster_band::modern, unit_class::siege,    {700, 0, 0, 400}, 480, 150, k_domains_land, land_class::artillery},
    {"Air Defence Battery",  roster_band::modern, unit_class::ranged,   {600, 0, 0, 400}, 470,  60, k_domains_land, land_class::air_defence},
};

/// The shared primitive both gate paths (region, campaign) funnel through —
/// a threshold check against four 0-1000 axis values, whatever supplied them.
bool gate_met(const roster_gate& g, int ore_q, int farm_q, int port_q, int energy_q)
{
    if (g.ore_q    > 0 && ore_q    < g.ore_q)    return false;
    if (g.farm_q   > 0 && farm_q   < g.farm_q)   return false;
    if (g.port_q   > 0 && port_q   < g.port_q)   return false;
    if (g.energy_q > 0 && energy_q < g.energy_q) return false;
    return true;
}

bool gate_met(const roster_gate& g, const region& p)
{
    return gate_met(g, p.ore_q, p.farm_q, p.port_q, p.energy_q);
}

} // namespace

const std::vector<roster_row>& unit_roster_table() { return g_table; }

bool row_can_traverse(const roster_row& r, traversal_domain d, bool owned_by_mover)
{
    if ((r.domains & domain_bit(d)) == 0)
        return false;

    // The owned-coastal-water asymmetry (BL-778), expressed once. A row that
    // also holds LAND is a land force wading its own shore, and may do so only
    // where its polity owns that water. A row without the land bit is at home
    // on water and asks nobody's permission.
    if (d == traversal_domain::coastal_water
     && (r.domains & domain_bit(traversal_domain::land)) != 0)
        return owned_by_mover;

    return true;
}

roster_band roster_band_for_capacity(int military_capacity)
{
    // The ladder's six capacity bands collapse onto the roster's four exactly
    // as ANCIENT_TECH_LADDER.md § Shape sets out: T1 | T2-T3 | T4 | T5-T6.
    if (military_capacity <= 1) return roster_band::classical;
    if (military_capacity <= 3) return roster_band::medieval;
    if (military_capacity <= 4) return roster_band::gunpowder;
    return roster_band::industrial;
}

std::vector<const roster_row*> available_rows(const region& p, roster_band band)
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

/// Does @p owner hold a port anywhere. BL-1402 (NATIONS.md § What a nation owns):
/// owner-generic — a corporation's `assets` or a nation's `state_assets`
/// (`assets_of`), so the naval gate serves a nation hiring as itself.
bool corp_owns_port(const world& w, entity_id corp)
{
    const std::vector<entity_id>* assets = assets_of(w, corp);
    if (assets == nullptr) return false;
    for (const entity_id asset : *assets)
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
    // BL-1265 (MARKETS.md § The shelf economy): corporations hold no stockpiles,
    // so the gate reads the stock on the shelves of the markets the corp sits
    // in — what a hire there could buy (`corp_shelf_stock`).
    // BL-1402: `corp_shelf_stock` / `buy_from_corp_shelves` (market_clearing.cpp,
    // another lane's file) still read a CORPORATION's markets only, so a nation
    // reads 0 here and its resource-gated rows stay closed — consistent with the
    // debit, which would buy nothing for it. Owed with that file's port.
    return corp_shelf_stock(w, corp, static_cast<std::size_t>(res));
}

campaign_roster_gate_input campaign_gate_input(const world& w, entity_id corp)
{
    campaign_roster_gate_input g;

    // BL-498: the resource axes walk the SHARED table (unit_roster.hpp
    // § hire_axis_table), which the hire debit reads too. Adding a resource to
    // an axis now moves the gate and the cost together, by construction.
    for (std::size_t i = 0; i < hire_axis_count; ++i)
    {
        const hire_axis axis = static_cast<hire_axis>(i);
        bool            held = false;
        for (const resource_type r : hire_axis_resources(axis))
            if (corp_stockpile_total(w, corp, r) > 0.0f) { held = true; break; }
        gate_axis_q(g, axis) = held ? 1000 : 0;
    }

    // port_q is NOT on that table: a building, not a resource — does the corp
    // hold a port anywhere. It gates a hire but never costs one.
    g.port_q = corp_owns_port(w, corp) ? 1000 : 0;
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
                                           const region& p,
                                           roster_band     band,
                                           int             readiness_q,
                                           bool            allow_naval)
{
    std::vector<army_stack_entry> stack;
    if (manpower <= 0) return stack;

    std::vector<const roster_row*> rows = available_rows(p, band);

    // NR-794: a campaign whose objective is inland fields NO SHIPS. Dropped
    // here, BEFORE the weighting below, so the remaining land rows divide the
    // whole manpower — a filtered fleet must not leave a hole in the army.
    //
    // WHY THIS EXISTS. The class scored zero until BL-779, so composing every
    // available row cost nothing and read as honest: a coastal polity simply
    // carried an inert galley entry. Giving naval real power turned that into a
    // COMBAT BONUS FOR OWNING A PORT, applied in landlocked battles, with no
    // cause anyone could point at in the world — and it was not rare: 84% of
    // battles carried a naval contingent, against the 18% that actually reached
    // over water. Rare is the design (MILITARY_HISTORY.md § Naval), and 84% is
    // not rare.
    if (!allow_naval)
        rows.erase(std::remove_if(rows.begin(), rows.end(),
                                  [](const roster_row* r) {
                                      return r->cls == unit_class::naval;
                                  }),
                   rows.end());

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

// ===========================================================================
// BL-454 / BL-459 — upkeep resolution and derived strength
// ===========================================================================

namespace
{

/// The row @p roster_type names, or nullptr when the index is out of range.
/// An unknown index is a data problem; every caller here degrades to the
/// neutral row rather than making the unit free or infinitely strong.
const roster_row* row_or_null(uint16_t roster_type)
{
    const std::vector<roster_row>& t = unit_roster_table();
    const std::size_t idx = static_cast<std::size_t>(roster_type);
    return (idx < t.size()) ? &t[idx] : nullptr;
}

} // namespace

unit_upkeep_draw resolve_unit_upkeep(const unit_component& u, const unit_upkeep_params& p)
{
    unit_upkeep_draw d;
    const int heads = (u.count > 0) ? u.count : 0;
    if (heads == 0)
        return d;

    const roster_row* row  = row_or_null(u.type);
    const int         powr = row ? row->power_mod : 0;
    const float       n    = static_cast<float>(heads);

    d.credits = n * (p.credits_per_head
                     + p.credits_per_head_per_power * static_cast<float>(powr));

    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float per_head = p.goods_per_head[r];
        if (per_head <= 0.0f)
            continue;
        d.goods[r]  = n * per_head;
        d.any_goods = true;
    }
    return d;
}

int roster_type_quality_permille(uint16_t roster_type)
{
    const roster_row* row = row_or_null(roster_type);
    // 1000 + power_mod: neutral for a levy, 1.38x for a rifle regiment. Floored
    // at 1 so a hypothetical row with power_mod <= -1000 cannot produce zero or
    // negative strength from a positive headcount.
    const int q = 1000 + (row ? row->power_mod : 0);
    return (q < 1) ? 1 : q;
}

int64_t unit_strength(const world& w, const unit_component& u)
{
    (void)w; // Reserved — see the declaration's @param note.
    if (u.count <= 0)
        return 0;

    const int64_t heads   = static_cast<int64_t>(u.count);
    const int64_t quality = static_cast<int64_t>(roster_type_quality_permille(u.type));
    const int64_t supply  = std::clamp<int64_t>(u.supply_factor_permille, 0, 1000);

    // count x (quality/1000) x (supply/1000) x 100, evaluated as ONE integer
    // expression so there is no rounding step in the middle. At quality 1000 and
    // supply 1000 this is exactly count * 100 (the documented scale anchor).
    return heads * quality * supply / 10000;
}

army_stack_entry unit_to_stack_entry(const world& w, const unit_component& u)
{
    (void)w; // Reserved, for the same reason unit_strength takes it.

    const roster_row* row = row_or_null(u.type);

    army_stack_entry e;
    e.type_id = u.type;                     // Opaque to combat.cpp by contract.
    e.cls     = row ? row->cls : unit_class::infantry;

    // Supply lands in the COUNT — the heads that can actually be put in the
    // field — and quality in type_power_mod, which is where the resolver's
    // matchup already expects it. Putting quality in both would square it
    // against unit_strength.
    const int64_t supply = std::clamp<int64_t>(u.supply_factor_permille, 0, 1000);
    const int64_t heads  = (u.count > 0) ? static_cast<int64_t>(u.count) : 0;
    e.count   = static_cast<int>(heads * supply / 1000);
    e.type_power_mod = row ? row->power_mod : 0;
    return e;
}

// ---------------------------------------------------------------------------
// Land classes (BL-1421)
// ---------------------------------------------------------------------------

namespace {

// Per man per round. The ratios carry the design (MILITARY.md § Land — the
// classes): infantry defends best and hits armour least; armour hits hard and is
// hard; artillery hits soft targets hardest and cannot hold; air defence is a
// passenger in a ground fight. The absolute scale is set against
// campaign_battle_params (campaign_battle_harness prints the curve).
constexpr std::array<land_combat_stats, land_class_count> k_land_stats = {{
    //  soft  hard  hardness defence breakthrough
    {   60,   15,     0,      90,     25 }, // infantry
    {   60,   15,   100,      70,     35 }, // motorised
    {   70,   45,   600,      70,     70 }, // mechanised
    {  100,  140,   900,      35,    120 }, // armour
    {   90,   25,   500,      25,     70 }, // light armour
    {  160,   25,   100,      15,     15 }, // artillery
    {   10,   10,   200,      20,      5 }, // air defence
    {    0,    0,     0,       5,      0 }, // none
}};

} // namespace

const land_combat_stats& land_class_stats(land_class c)
{
    const auto i = static_cast<std::size_t>(c);
    return k_land_stats[i < k_land_stats.size() ? i : static_cast<std::size_t>(land_class::none)];
}

land_class land_class_for(const roster_row& r)
{
    // BL-1422: a row that declares its class fights as it (every modern row).
    if (r.land != k_land_class_from_cls)
        return r.land;
    switch (r.cls)
    {
        case unit_class::infantry: return land_class::infantry;
        case unit_class::ranged:   return land_class::infantry;
        case unit_class::cavalry:  return r.band == roster_band::industrial ? land_class::mechanised
                                                                            : land_class::light_armour;
        case unit_class::siege:    return land_class::artillery;
        case unit_class::naval:    return land_class::none;
    }
    return land_class::infantry;
}

int modern_row_for(land_class c)
{
    const std::vector<roster_row>& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (t[i].band == roster_band::modern && land_class_for(t[i]) == c)
            return static_cast<int>(i);
    return -1;
}

uint16_t campaign_opening_unit_row()
{
    const int i = modern_row_for(land_class::infantry);
    return i >= 0 ? static_cast<uint16_t>(i) : uint16_t{0};
}

bool roster_row_obsolete_by_default(const roster_row& r)
{
    return r.band != roster_band::modern;
}

bool roster_row_obsolete(const world& w, entity_id nation, uint16_t row)
{
    const roster_row* r = row_or_null(row);
    if (r == nullptr)
        return true;
    bool obsolete = roster_row_obsolete_by_default(*r);
    if (const auto nit = w.nations.find(nation); nit != w.nations.end())
    {
        const std::vector<uint16_t>& flips = nit->second.roster_obsolete_flips;
        if (std::binary_search(flips.begin(), flips.end(), row))
            obsolete = !obsolete;
    }
    return obsolete;
}

int muster_days_for(uint16_t roster_type)
{
    const roster_row* row = row_or_null(roster_type);
    switch (row ? land_class_for(*row) : land_class::infantry)
    {
        case land_class::infantry:     return 90;
        case land_class::motorised:    return 90;
        case land_class::light_armour: return 90;
        case land_class::artillery:    return 135;
        case land_class::air_defence:  return 135;
        case land_class::mechanised:   return 180;
        case land_class::armour:       return 180;
        case land_class::none:         return 90;
    }
    return 90;
}

land_combat_stats unit_land_stats(const unit_component& u)
{
    const roster_row* row = row_or_null(u.type);
    land_combat_stats s = land_class_stats(row ? land_class_for(*row) : land_class::infantry);
    const int64_t q = roster_type_quality_permille(u.type);
    auto scale = [q](int v) { return static_cast<int>(static_cast<int64_t>(v) * q / 1000); };
    s.soft         = scale(s.soft);
    s.hard         = scale(s.hard);
    s.defence      = scale(s.defence);
    s.breakthrough = scale(s.breakthrough);
    return s;
}

bool refit_row_newer(uint16_t from, uint16_t to)
{
    const roster_row* a = row_or_null(from);
    const roster_row* b = row_or_null(to);
    if (a == nullptr || b == nullptr || from == to)
        return false;
    if (land_class_for(*a) != land_class_for(*b))
        return false;
    const int ba = static_cast<int>(a->band), bb = static_cast<int>(b->band);
    return bb > ba || (bb == ba && b->power_mod > a->power_mod);
}

std::vector<uint16_t> refit_targets(const world& w, entity_id nation, uint16_t from, roster_band band)
{
    std::vector<uint16_t> out;
    const std::vector<roster_row>& t = unit_roster_table();
    const std::vector<const roster_row*> avail = available_rows(w, nation, band);
    for (const roster_row* r : avail)
    {
        const uint16_t idx = static_cast<uint16_t>(r - t.data());
        if (refit_row_newer(from, idx))
            out.push_back(idx);
    }
    return out;
}
