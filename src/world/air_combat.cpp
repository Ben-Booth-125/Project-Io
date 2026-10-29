#include "air_combat.hpp"

#include "battle_system.hpp"   // units_in_live_battles
#include "campaign_battle.hpp" // campaign_battle_params: the land round's hit conversion
#include "economy_system.hpp"  // draw_goods_at_tile, step_unit_supply_toward, economy_report
#include "province.hpp"        // province_holder_for
#include "recipe_registry.hpp"
#include "stance.hpp"          // is_hostile
#include "unit_designs.hpp"
#include "world.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

// ---------------------------------------------------------------------------
// The air roster. One row per class; numbers are the class's (Lua). The gate is
// the modern land rows' binary shelf test: steel-or-ore and fuel access, which is
// what airframes and their fuel are made of.
// ---------------------------------------------------------------------------

const std::vector<air_roster_row>& air_roster_table()
{
    static const std::vector<air_roster_row> rows = {
        {"Interceptor Wing",           air_class::fighter,    {600, 0, 0, 600}},
        {"Strike Wing",                air_class::multirole,  {600, 0, 0, 600}},
        {"Strategic Bomber Wing",      air_class::bomber,     {700, 0, 0, 700}},
        {"Attack Helicopter Wing",     air_class::helicopter, {400, 0, 0, 500}},
        {"Strike Drone Wing",          air_class::drone,      {300, 0, 0, 400}},
        {"Airborne Early Warning Wing", air_class::recon,     {500, 0, 0, 600}},
    };
    return rows;
}

std::vector<uint16_t> available_air_rows(const world& w, entity_id nation)
{
    std::vector<uint16_t> out;
    if (w.nations.find(nation) == w.nations.end())
        return out;
    const campaign_roster_gate_input g = campaign_gate_input(w, nation);
    const std::vector<air_roster_row>& t = air_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        const roster_gate& r = t[i].gate;
        if (g.ore_q >= r.ore_q && g.farm_q >= r.farm_q && g.port_q >= r.port_q
            && g.energy_q >= r.energy_q)
            out.push_back(static_cast<uint16_t>(i));
    }
    return out;
}

// ---------------------------------------------------------------------------
// A wing's numbers
// ---------------------------------------------------------------------------

air_class wing_class(const air_wing& wg)
{
    const std::vector<air_roster_row>& t = air_roster_table();
    return wg.type < t.size() ? t[wg.type].cls : air_class::fighter;
}

air_wing_stats wing_stats(const recipe_registry& reg, const air_wing& wg)
{
    const air_class c = wing_class(wg);
    const air_class_stats& base = reg.military().air.cls[static_cast<std::size_t>(c)];
    const air_combat_stats k = air_kit_delta(c, wg.kit);
    air_wing_stats s;
    s.hp        = std::max(1, base.hp + k.hp);
    s.air       = std::max(0, base.air + k.air);
    s.superiority_permille = base.superiority_permille;
    s.soft      = std::max(0, base.soft + k.soft);
    s.hard      = std::max(0, base.hard + k.hard);
    s.strategic = std::max(0, base.strategic + k.strategic);
    s.recon     = std::max(0, base.recon + k.recon);
    s.range     = std::max(0, base.range + k.range);
    s.ad_vulnerability_permille = base.ad_vulnerability_permille;
    s.has_morale = base.has_morale;
    return s;
}

int wing_strength_permille(const recipe_registry& reg, const air_wing& wg)
{
    const int64_t hp = std::clamp<int64_t>(wg.hp_ppm / 1000, 0, 1000);
    const int64_t morale = wing_stats(reg, wg).has_morale
                               ? std::clamp<int64_t>(wg.morale_permille, 0, 1000) : 1000;
    const int64_t supply = std::clamp<int64_t>(wg.supply_factor_permille, 0, 1000);
    return static_cast<int>(hp * morale / 1000 * supply / 1000);
}

bool air_class_flies(const recipe_registry& reg, air_class c, air_mission m)
{
    if (m == air_mission::none)
        return true;
    if (static_cast<int>(c) >= air_class_count || static_cast<int>(m) >= air_mission_count)
        return false;
    return (reg.military().air.cls[static_cast<std::size_t>(c)].missions & air_mission_bit(m)) != 0;
}

bool condition_bites(building_type t)
{
    switch (t)
    {
        case building_type::extraction_site:
        case building_type::processing_facility:
        case building_type::port:
        case building_type::inland_logistics_hub:
        case building_type::research_institute:
        case building_type::university:
        case building_type::planetary_marketplace:
        case building_type::railway_station:
            return true;
        default:
            return false;
    }
}

bool air_mission_bombs(air_mission m)
{
    return m == air_mission::tactical || m == air_mission::strategic;
}

// ---------------------------------------------------------------------------
// Range
// ---------------------------------------------------------------------------

namespace {

/// Odd-r offset (col, row) -> cube x; z is the row.
int cube_x(int col, int row)
{
    return col - (row - (row & 1)) / 2;
}

int hex_distance_wrapped(int c0, int r0, int c1, int r1, int width)
{
    int best = std::numeric_limits<int>::max();
    for (const int shift : {0, width, -width})
    {
        if (shift != 0 && width <= 0)
            continue;
        const int x0 = cube_x(c0, r0), z0 = r0;
        const int x1 = cube_x(c1 + shift, r1), z1 = r1;
        const int dx = x1 - x0, dz = z1 - z0, dy = -dx - dz;
        best = std::min(best, std::max({std::abs(dx), std::abs(dy), std::abs(dz)}));
    }
    return best;
}

bool hostile_either(const world& w, entity_id a, entity_id b)
{
    return a != b && a != null_entity && b != null_entity
        && (is_hostile(w, a, b) || is_hostile(w, b, a));
}

} // namespace

int air_tile_distance(const world& w, entity_id a, entity_id b)
{
    const auto ia = w.tiles.find(a);
    const auto ib = w.tiles.find(b);
    if (ia == w.tiles.end() || ib == w.tiles.end() || ia->second.body != ib->second.body)
        return -1;
    int width = 0;
    if (const auto bit = w.bodies.find(ia->second.body); bit != w.bodies.end())
        width = bit->second.grid_width;
    return hex_distance_wrapped(ia->second.grid_x, ia->second.grid_y, ib->second.grid_x,
                                ib->second.grid_y, width);
}

int air_province_distance(const world& w, entity_id from, uint32_t province_id)
{
    const ::province* p = w.provinces.find(province_id);
    if (p == nullptr)
        return -1;
    int best = -1;
    for (const entity_id t : p->tiles)
    {
        const int d = air_tile_distance(w, from, t);
        if (d >= 0 && (best < 0 || d < best))
            best = d;
    }
    return best;
}

entity_id wing_base_tile(const world& w, const air_wing& wg)
{
    const auto bit = w.buildings.find(wg.airbase);
    return bit != w.buildings.end() ? bit->second.tile : null_entity;
}

bool air_province_in_range(const world& w, const recipe_registry& reg, const air_wing& wg,
                           uint32_t province)
{
    const entity_id from = wing_base_tile(w, wg);
    if (from == null_entity)
        return false;
    const int d = air_province_distance(w, from, province);
    return d >= 0 && d <= wing_stats(reg, wg).range;
}

std::vector<uint32_t> air_provinces_in_range(const world& w, const recipe_registry& reg,
                                             entity_id wing)
{
    std::vector<uint32_t> out;
    const auto wit = w.wings.find(wing);
    if (wit == w.wings.end())
        return out;
    for (const province& p : w.provinces.provinces) // ascending id
        if (air_province_in_range(w, reg, wit->second, p.id))
            out.push_back(p.id);
    return out;
}

// ---------------------------------------------------------------------------
// The doors
// ---------------------------------------------------------------------------

entity_id airbase_at(const world& w, entity_id nation, entity_id tile)
{
    const std::vector<entity_id>* assets = assets_of(w, nation);
    if (assets == nullptr)
        return null_entity;
    entity_id best = null_entity;
    for (const entity_id bid : *assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        if (b.tile == tile && b.type == building_type::airbase && b.ticks_remaining <= 0
            && !b.decommissioned && (best == null_entity || bid < best))
            best = bid;
    }
    return best;
}

corp_command_result hire_wing_check(const world& w, entity_id nation, entity_id tile,
                                    uint16_t row, entity_id* base_out)
{
    if (row >= air_roster_table().size())
        return corp_command_result::rejected_invalid;
    const std::vector<uint16_t> ok = available_air_rows(w, nation);
    if (std::find(ok.begin(), ok.end(), row) == ok.end())
        return corp_command_result::rejected_invalid;
    if (w.tiles.find(tile) == w.tiles.end())
        return corp_command_result::rejected_invalid;
    const entity_id base = airbase_at(w, nation, tile);
    if (base == null_entity)
        return corp_command_result::rejected_placement;
    if (base_out != nullptr)
        *base_out = base;
    return corp_command_result::applied;
}

corp_command_result air_mission_check(const world& w, const recipe_registry& reg,
                                      entity_id nation, entity_id wing, uint32_t mission,
                                      const uint32_t* provinces, std::size_t n,
                                      std::vector<uint32_t>* out)
{
    const auto wit = w.wings.find(wing);
    if (wit == w.wings.end())
        return corp_command_result::rejected_invalid;
    const air_wing& wg = wit->second;
    if (wg.owner != nation)
        return corp_command_result::rejected_not_owner;
    if (mission >= static_cast<uint32_t>(air_mission_count))
        return corp_command_result::rejected_invalid;
    const air_mission m = static_cast<air_mission>(mission);
    if (!air_class_flies(reg, wing_class(wg), m))
        return corp_command_result::rejected_invalid;

    std::vector<uint32_t> set;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (provinces[i] == no_province)
            continue; // an unused slot of the fixed-size field
        if (w.provinces.find(provinces[i]) == nullptr)
            return corp_command_result::rejected_invalid;
        set.push_back(provinces[i]);
    }
    std::sort(set.begin(), set.end());
    if (std::adjacent_find(set.begin(), set.end()) != set.end())
        return corp_command_result::rejected_invalid; // a province named twice
    if (set.size() > k_air_mission_provinces_max)
        return corp_command_result::rejected_invalid;
    if ((m == air_mission::none) != set.empty())
        return corp_command_result::rejected_invalid;

    for (const uint32_t p : set)
        if (!air_province_in_range(w, reg, wg, p))
            return corp_command_result::rejected_placement;

    // STRATEGIC bombing only over a province held by a nation the owner is hostile
    // to (directed: the owner's own declaration — MILITARY.md § Air, and the rival
    // grant's "only a nation the rival is already hostile to"). TACTICAL bombing may
    // fly over ANY province — the owner's own, a third nation's, the sea — because
    // its hostility rule is on the TARGET: run_air_day hits only units of a nation
    // the owner is hostile to (cold review, 2026-10-29: close air support on home soil).
    if (m == air_mission::strategic)
        for (const uint32_t p : set)
        {
            const entity_id holder = province_holder_for(w, p);
            if (holder == null_entity || holder == nation || !is_hostile(w, nation, holder))
                return corp_command_result::rejected_state;
        }

    if (wg.mission == m && wg.provinces == set)
        return corp_command_result::rejected_state; // no change
    if (out != nullptr)
        *out = std::move(set);
    return corp_command_result::applied;
}

corp_command_result air_design_check(const world& w, entity_id nation, uint32_t cls,
                                     uint16_t slot, uint16_t item)
{
    if (w.nations.find(nation) == w.nations.end() || nation != player_nation(w))
        return corp_command_result::rejected_not_owner;
    if (cls >= static_cast<uint32_t>(air_class_count) || slot >= k_air_kit_slot_count)
        return corp_command_result::rejected_invalid;
    const air_class c = static_cast<air_class>(cls);
    const std::vector<air_kit_item>& cat = air_kit_catalogue();
    if (item >= cat.size() || cat[item].cls != c || cat[item].slot != slot)
        return corp_command_result::rejected_invalid;
    if (!air_kit_item_researched(w, nation, item))
        return corp_command_result::rejected_tech_locked;
    if (air_design_fitted(w, nation, c, slot) == item)
        return corp_command_result::rejected_state;
    return corp_command_result::applied;
}

// ---------------------------------------------------------------------------
// Superiority
// ---------------------------------------------------------------------------

namespace {

/// The share of @p nation's power @p own against every hostile nation's in
/// @p province, read off @p sup (keyed (province, nation)).
int share_in(const world& w, const std::map<std::pair<uint32_t, entity_id>, int64_t>& sup,
             uint32_t province, entity_id nation)
{
    int64_t own = 0, hostile = 0;
    for (auto it = sup.lower_bound({province, null_entity});
         it != sup.end() && it->first.first == province; ++it)
    {
        if (it->first.second == nation)
            own += it->second;
        else if (hostile_either(w, nation, it->first.second))
            hostile += it->second;
    }
    if (own + hostile <= 0)
        return 1000;
    return static_cast<int>(own * 1000 / (own + hostile));
}

} // namespace

int air_share_permille(const world& w, uint32_t province, entity_id nation)
{
    if (w.air_superiority.empty())
        return 1000;
    std::map<std::pair<uint32_t, entity_id>, int64_t> sup;
    for (auto it = w.air_superiority.lower_bound({province, null_entity});
         it != w.air_superiority.end() && it->first.first == province; ++it)
        sup.emplace(it->first, it->second);
    return share_in(w, sup, province, nation);
}

int air_march_cost_permille(const world& w, const recipe_registry& reg, entity_id mover,
                            entity_id tile)
{
    if (w.air_superiority.empty())
        return 1000;
    const uint32_t p = w.provinces.province_of(tile);
    if (p == 0)
        return 1000;
    const int share = air_share_permille(w, p, mover);
    return 1000 + reg.military().air.march_slow_permille * (1000 - share) / 1000;
}

int air_defence_kit_bonus_permille(const unit_design& kit)
{
    static constexpr int launcher[] = {0, 300, 600, 900};
    static constexpr int fire[]     = {0, 200, 400, 500};
    const int a = kit.fit[0] < 4 ? launcher[kit.fit[0]] : 0;
    const int b = kit.fit[1] < 4 ? fire[kit.fit[1]] : 0;
    return a + b;
}

// ---------------------------------------------------------------------------
// The day
// ---------------------------------------------------------------------------

namespace {

struct sortie_wing
{
    entity_id      id = null_entity;
    entity_id      owner = null_entity;
    air_mission    mission = air_mission::none;
    air_wing_stats st;
    int64_t        strength = 0; ///< per-mille, at the day's start
    int64_t        n = 1;        ///< provinces it divides the day across
    std::vector<uint32_t> provinces;
};

/// May @p wg fly today? Raised, alive, assigned, with a standing, completed,
/// working airbase of its owner's.
bool wing_flies(const world& w, const air_wing& wg)
{
    if (wg.muster_days > 0 || wg.hp_ppm <= 0 || wg.mission == air_mission::none
        || wg.provinces.empty() || w.nations.find(wg.owner) == w.nations.end())
        return false;
    const auto bit = w.buildings.find(wg.airbase);
    return bit != w.buildings.end() && bit->second.type == building_type::airbase
        && bit->second.ticks_remaining <= 0 && !bit->second.decommissioned;
}

/// A unit's strength, per-mille: men over establishment x supply x morale.
int64_t unit_strength_permille(const unit_component& u)
{
    const int est = std::max(1, unit_establishment(u));
    const int64_t men = std::clamp<int64_t>(int64_t{u.count} * 1000 / est, 0, 1000);
    return men * std::clamp<int64_t>(u.supply_factor_permille, 0, 1000) / 1000
               * std::clamp<int64_t>(u.morale_permille, 0, 1000) / 1000;
}

} // namespace

air_day_tick run_air_day(world& w, const recipe_registry& reg, economy_report& report)
{
    air_day_tick out;
    const air_params& ap = reg.military().air;

    // --- 0. Musters tick ---------------------------------------------------
    for (auto& [wid, wg] : w.wings)
        if (wg.muster_days > 0)
            --wg.muster_days;

    // --- 1. Who flies, from where, with what --------------------------------
    std::vector<sortie_wing> flying;
    for (const auto& [wid, wg] : w.wings) // ascending id
    {
        if (!wing_flies(w, wg))
            continue;
        sortie_wing s;
        s.id       = wid;
        s.owner    = wg.owner;
        s.mission  = wg.mission;
        s.st       = wing_stats(reg, wg);
        s.strength = wing_strength_permille(reg, wg);
        for (const uint32_t p : wg.provinces)
            if (w.provinces.find(p) != nullptr) // a province merged away flies nothing
                s.provinces.push_back(p);
        if (s.provinces.empty())
            continue;
        s.n = static_cast<int64_t>(s.provinces.size());
        flying.push_back(std::move(s));
    }

    // province -> the sorties over it (indices into `flying`, ascending wing id)
    std::map<uint32_t, std::vector<std::size_t>> over;
    std::map<std::pair<uint32_t, entity_id>, int64_t> sup;
    std::map<std::pair<uint32_t, entity_id>, int64_t> recon;
    for (std::size_t i = 0; i < flying.size(); ++i)
    {
        const sortie_wing& s = flying[i];
        for (const uint32_t p : s.provinces)
        {
            over[p].push_back(i);
            ++out.sorties;
            if (s.mission == air_mission::superiority)
            {
                const int64_t pw = int64_t{s.st.air} * s.st.superiority_permille / 1000
                                   * s.strength / 1000 / s.n;
                if (pw > 0)
                    sup[{p, s.owner}] += pw;
            }
            else if (s.mission == air_mission::recon)
            {
                const int64_t rp = int64_t{s.st.recon} * s.strength / 1000 / s.n;
                if (rp > 0)
                    recon[{p, s.owner}] += rp;
            }
        }
    }

    // The day's superiority, kept for the march and the surfaces.
    w.air_superiority.clear();
    for (const auto& [key, v] : sup)
        if (v > 0)
            w.air_superiority.emplace(key, static_cast<int32_t>(
                std::min<int64_t>(v, std::numeric_limits<int32_t>::max())));

    // --- 2. Fire on wings, from the day's starting strength ------------------
    std::vector<int64_t> dmg(flying.size(), 0); // milli-HP

    // Plane against plane: each nation's superiority power over a province is
    // spread evenly over the hostile wings flying there, each target taking its
    // share for the part of its day it spends there.
    for (const auto& [key, power] : sup)
    {
        const auto oit = over.find(key.first);
        if (oit == over.end())
            continue;
        std::vector<std::size_t> targets;
        for (const std::size_t i : oit->second)
            if (hostile_either(w, key.second, flying[i].owner))
                targets.push_back(i);
        if (targets.empty())
            continue;
        const int64_t each = power / static_cast<int64_t>(targets.size());
        for (const std::size_t i : targets)
            dmg[i] += each / flying[i].n;
    }

    // Ground air defence shoots every hostile mission flown over its province.
    std::vector<entity_id> uids;
    if (!over.empty())
    {
        uids.reserve(w.units.size());
        for (const auto& kv : w.units)
            uids.push_back(kv.first);
        std::sort(uids.begin(), uids.end());
    }
    for (const entity_id uid : uids)
    {
        const unit_component& u = w.units.at(uid);
        if (u.count <= 0 || unit_land_class(u) != land_class::air_defence)
            continue;
        const auto oit = over.find(w.provinces.province_of(u.position));
        if (oit == over.end())
            continue;
        const int64_t fire = int64_t{ap.ad_attack} * unit_strength_permille(u) / 1000
                             * (1000 + air_defence_kit_bonus_permille(u.kit)) / 1000;
        for (const std::size_t i : oit->second)
        {
            if (!hostile_either(w, u.owner, flying[i].owner))
                continue;
            dmg[i] += fire * flying[i].st.ad_vulnerability_permille / 1000 / flying[i].n;
            ++out.ad_shots;
        }
    }

    // --- 3. Bombing ---------------------------------------------------------
    const campaign_battle_params land{}; // the land round's hit conversion
    std::set<entity_id> fighting;
    std::map<uint32_t, std::vector<entity_id>> units_in;     // province -> unit ids ascending
    std::map<uint32_t, std::vector<entity_id>> buildings_in; // province -> building ids ascending
    std::map<entity_id, entity_id> building_nation;
    bool any_tac = false, any_str = false;
    for (const sortie_wing& s : flying)
    {
        any_tac = any_tac || s.mission == air_mission::tactical;
        any_str = any_str || s.mission == air_mission::strategic;
    }
    if (any_tac || !over.empty())
    {
        for (const entity_id uid : uids)
        {
            const unit_component& u = w.units.at(uid);
            if (u.count > 0)
                units_in[w.provinces.province_of(u.position)].push_back(uid);
        }
    }
    if (any_tac)
        fighting = units_in_live_battles(w);
    if (any_str)
    {
        std::vector<entity_id> nids;
        for (const auto& kv : w.nations)
            nids.push_back(kv.first);
        std::sort(nids.begin(), nids.end());
        for (const entity_id nid : nids)
            for (const entity_id bid : w.nations.at(nid).state_assets)
                building_nation.emplace(bid, nid);
        std::vector<entity_id> bids;
        for (const auto& kv : w.buildings)
            bids.push_back(kv.first);
        std::sort(bids.begin(), bids.end());
        for (const entity_id bid : bids)
        {
            const building_component& b = w.buildings.at(bid);
            if (b.ticks_remaining <= 0 && condition_bites(b.type))
                buildings_in[w.provinces.province_of(b.tile)].push_back(bid);
        }
    }

    for (const sortie_wing& s : flying)
    {
        if (!air_mission_bombs(s.mission))
            continue;
        for (const uint32_t p : s.provinces)
        {
            const int share = share_in(w, sup, p, s.owner);
            const int64_t scale = 1000 - int64_t{ap.bombing_penalty_permille} * (1000 - share) / 1000;
            int64_t bonus = 0;
            if (const auto rit = recon.find({p, s.owner}); rit != recon.end())
                bonus = std::min<int64_t>(rit->second, ap.recon_cap_permille);
            // per-mille of a full day's attack this sortie lands
            const int64_t factor = s.strength * scale / 1000 * (1000 + bonus) / 1000 / s.n;
            if (factor <= 0)
                continue;

            if (s.mission == air_mission::tactical)
            {
                // Units in battle in the province first, then any enemy unit there.
                entity_id target = null_entity;
                const auto uit = units_in.find(p);
                if (uit != units_in.end())
                {
                    for (const entity_id uid : uit->second)
                    {
                        const unit_component& u = w.units.at(uid);
                        if (u.count > 0 && is_hostile(w, s.owner, u.owner) && fighting.count(uid) != 0)
                        { target = uid; break; }
                    }
                    if (target == null_entity)
                        for (const entity_id uid : uit->second)
                        {
                            const unit_component& u = w.units.at(uid);
                            if (u.count > 0 && is_hostile(w, s.owner, u.owner))
                            { target = uid; break; }
                        }
                }
                if (target == null_entity)
                    continue;
                unit_component& u = w.units.at(target);
                const int64_t hardness = std::clamp(unit_land_stats(u).hardness, 0, 1000);
                // The land arithmetic, with no terrain divisor: from above.
                const int64_t raw = (int64_t{s.st.soft} * (1000 - hardness)
                                     + int64_t{s.st.hard} * hardness) / 1000;
                const int64_t hits = raw * factor / 1000;
                if (hits <= 0)
                    continue;
                ++out.tactical_hits;
                std::pair<int32_t, int32_t>& bank = w.air_hit_bank[target];
                const int64_t hp_part = hits * land.hp_share_permille / 1000;
                int64_t hp_bank = bank.first + hp_part;
                int64_t rd_bank = bank.second + (hits - hp_part);
                const int64_t men = std::min<int64_t>(hp_bank / std::max(1, land.hp_cost), u.count);
                hp_bank -= men * std::max(1, land.hp_cost);
                const int before = u.count;
                u.count -= static_cast<int>(men);
                out.men_killed += static_cast<int>(men);
                // readiness: readiness_cost milli-hits per man spend that man's whole readiness
                const int64_t per_permille = int64_t{std::max(1, land.readiness_cost)} * std::max(1, before);
                const int64_t rd_loss = rd_bank * 1000 / per_permille;
                if (rd_loss > 0)
                {
                    u.readiness_permille = static_cast<int32_t>(
                        std::max<int64_t>(0, u.readiness_permille - rd_loss));
                    rd_bank -= rd_loss * per_permille / 1000;
                }
                bank.first  = static_cast<int32_t>(std::clamp<int64_t>(hp_bank, 0, 1 << 30));
                bank.second = static_cast<int32_t>(std::clamp<int64_t>(rd_bank, 0, 1 << 30));
            }
            else // strategic
            {
                const auto bit = buildings_in.find(p);
                if (bit == buildings_in.end())
                    continue;
                // The hostile building in best condition, ties to the lowest id: a
                // campaign wears a province's economy down evenly.
                entity_id target = null_entity;
                int best = -1;
                for (const entity_id bid : bit->second)
                {
                    const auto nit = building_nation.find(bid);
                    if (nit == building_nation.end() || !is_hostile(w, s.owner, nit->second))
                        continue;
                    const int c = w.buildings.at(bid).condition_permille;
                    if (c > best)
                    { best = c; target = bid; }
                }
                if (target == null_entity || best <= 0)
                    continue;
                const int64_t loss = int64_t{s.st.strategic} * factor / 1000;
                if (loss <= 0)
                    continue;
                building_component& b = w.buildings.at(target);
                const int lost = static_cast<int>(std::min<int64_t>(loss, b.condition_permille));
                b.condition_permille -= lost;
                out.condition_lost += lost;
                ++out.strategic_hits;
            }
        }
    }

    // --- 4. The wings take the day's fire ------------------------------------
    for (std::size_t i = 0; i < flying.size(); ++i)
    {
        if (dmg[i] <= 0)
            continue;
        air_wing& wg = w.wings.at(flying[i].id);
        // milli-HP over the class's HP, as parts per million of full
        const int64_t ppm = dmg[i] * 1000 / std::max(1, flying[i].st.hp);
        wg.hp_ppm = static_cast<int32_t>(std::max<int64_t>(0, wg.hp_ppm - ppm));
        ++out.wings_hit;
    }

    // --- 5. Repair: a damaged building regains condition by drawing goods -----
    {
        std::vector<entity_id> damaged;
        for (const auto& [bid, b] : w.buildings)
            if (b.condition_permille < 1000)
                damaged.push_back(bid);
        std::sort(damaged.begin(), damaged.end());
        for (const entity_id bid : damaged)
        {
            building_component& b = w.buildings.at(bid);
            const int want = std::min(ap.repair_permille_per_day, 1000 - b.condition_permille);
            if (want <= 0)
                continue;
            const entity_id owner = owner_nation_of(w, bid);
            if (owner == null_entity)
                continue; // nobody to repair it
            std::array<float, resource_count> need{};
            for (std::size_t r = 0; r < resource_count; ++r)
                need[r] = ap.repair_goods_per_permille[r] * static_cast<float>(want);
            const int share = draw_goods_at_tile(w, reg, report, owner, b.tile, need);
            const int got = want * share / 1000;
            b.condition_permille = std::min(1000, b.condition_permille + got);
            out.condition_repaired += got;
        }
    }

    // --- 6. Reveal ------------------------------------------------------------
    // A nation reveals a hostile nation's units in a province when its share there
    // against them is above the line, or when it flies recon over it.
    w.air_revealed.clear();
    for (const auto& [p, idx] : over)
    {
        const auto uit = units_in.find(p);
        if (uit == units_in.end())
            continue;
        std::set<entity_id> viewers;
        for (const std::size_t i : idx)
            viewers.insert(flying[i].owner);
        for (const entity_id viewer : viewers)
        {
            const bool recon_here = recon.count({p, viewer}) != 0;
            const bool sup_here   = sup.count({p, viewer}) != 0;
            if (!recon_here && !sup_here)
                continue;
            for (const entity_id uid : uit->second)
            {
                const entity_id t = w.units.at(uid).owner;
                if (!hostile_either(w, viewer, t))
                    continue;
                if (recon_here || share_in(w, sup, p, viewer) > ap.reveal_share_permille)
                    w.air_revealed.insert({viewer, uid});
            }
        }
    }
    out.revealed = static_cast<int>(w.air_revealed.size());
    return out;
}

air_day_tick run_air_missions(world& w, const recipe_registry& reg, economy_report& report,
                              int days)
{
    air_day_tick sum;
    // THE FAST PATH (cold review): no wing flying, none finishing its muster inside
    // the span, and no building below full condition — then every day of the span
    // would fly nothing, repair nothing and leave an empty superiority map and reveal
    // set, so the span is applied at once: musters run down, the maps clear.
    {
        bool busy = false;
        for (const auto& [wid, wg] : w.wings)
            busy = busy || wing_flies(w, wg) || (wg.muster_days > 0 && wg.muster_days <= days);
        for (auto it = w.buildings.begin(); !busy && it != w.buildings.end(); ++it)
            busy = it->second.condition_permille < 1000;
        if (!busy)
        {
            for (auto& [wid, wg] : w.wings)
                if (wg.muster_days > 0)
                    wg.muster_days = std::max(0, wg.muster_days - std::max(0, days));
            w.air_superiority.clear();
            w.air_revealed.clear();
            return sum;
        }
    }
    for (int d = 0; d < days; ++d)
    {
        const air_day_tick t = run_air_day(w, reg, report);
        sum.sorties += t.sorties;
        sum.wings_hit += t.wings_hit;
        sum.ad_shots += t.ad_shots;
        sum.tactical_hits += t.tactical_hits;
        sum.men_killed += t.men_killed;
        sum.strategic_hits += t.strategic_hits;
        sum.condition_lost += t.condition_lost;
        sum.condition_repaired += t.condition_repaired;
        sum.revealed = t.revealed;
    }
    return sum;
}

wing_upkeep_tick run_wing_upkeep(world& w, const recipe_registry& reg, economy_report& report)
{
    wing_upkeep_tick out;
    // Prune what this tick's land passes erased.
    for (auto it = w.air_hit_bank.begin(); it != w.air_hit_bank.end();)
    {
        const auto uit = w.units.find(it->first);
        if (uit == w.units.end() || uit->second.count <= 0)
            it = w.air_hit_bank.erase(it);
        else
            ++it;
    }
    for (auto it = w.air_revealed.begin(); it != w.air_revealed.end();)
    {
        if (w.units.find(it->second) == w.units.end() || w.nations.find(it->first) == w.nations.end())
            it = w.air_revealed.erase(it);
        else
            ++it;
    }
    for (auto it = w.air_superiority.begin(); it != w.air_superiority.end();)
    {
        if (w.nations.find(it->first.second) == w.nations.end())
            it = w.air_superiority.erase(it);
        else
            ++it;
    }
    if (w.wings.empty())
        return out;

    const air_params& ap = reg.military().air;
    const unit_upkeep_params& up = reg.military().upkeep;
    std::vector<entity_id> disband;
    for (auto& [wid, wg] : w.wings) // ascending id: shared shelves, so order is load-bearing
    {
        // The orphan rule (MILITARY.md § Upkeep): owner or airbase erased, or shot down.
        const auto bit = w.buildings.find(wg.airbase);
        if (w.nations.find(wg.owner) == w.nations.end() || bit == w.buildings.end()
            || bit->second.type != building_type::airbase || wg.hp_ppm <= 0)
        {
            disband.push_back(wid);
            continue;
        }
        ++out.wings;
        if (wg.muster_days > 0)
            continue; // still being raised: draws nothing yet
        // Grounded by its base — decommissioned or (rebuilt) still a site — it flies
        // nothing (wing_flies) and draws no fuel either; its supply stands.
        if (bit->second.decommissioned || bit->second.ticks_remaining > 0)
            continue;
        // Supply is drawn where the wing stands — at its airbase.
        const int share = draw_goods_at_tile(w, reg, report, wg.owner, bit->second.tile,
                                             ap.wing_upkeep_goods);
        if (share < 1000)
            ++out.unmet;
        wg.supply_factor_permille = step_unit_supply_toward(wg.supply_factor_permille, share, up);
    }
    for (const entity_id wid : disband)
    {
        w.wings.erase(wid);
        ++out.disbanded;
    }
    return out;
}
