// BL-1431 — missiles, interceptors and tactical nuclear warheads. See missiles.hpp.

#include "missiles.hpp"

#include "air_combat.hpp"     // air_tile_distance, air_province_distance, wing_class, wing_stats, condition_bites
#include "battle_system.hpp"  // units_in_live_battles
#include "construction.hpp"   // construction_capex (the scorer's building value)
#include "digitisation_tree_data.hpp" // io::dg::find_node
#include "economy_system.hpp" // corp_is_mobilised
#include "nation_tech.hpp"    // nation_holds
#include "province.hpp"       // province_holder_for
#include "recipe_registry.hpp"
#include "sea_combat.hpp"     // ship_class_of
#include "sentiment.hpp"
#include "sentiment_emitters.hpp" // queue_conduct, note_battle_losses, sentiment_conduct_live
#include "stance.hpp"         // is_hostile
#include "unit_designs.hpp"   // unit_land_class
#include "world.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

namespace {

const missile_params& mp(const recipe_registry& reg) { return reg.military().missile; }

bool fireable(uint32_t kind) { return kind < static_cast<uint32_t>(missile_fireable_count); }

std::size_t col(missile_kind k) { return static_cast<std::size_t>(k); }

bool is_live_nation(const world& w, entity_id n)
{
    const auto it = w.nations.find(n);
    return it != w.nations.end() && !it->second.ended;
}

/// Unit ids ascending (w.units is not an ordered map).
std::vector<entity_id> sorted_unit_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.units.size());
    for (const auto& kv : w.units)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool unit_ready(const unit_component& u)
{
    return u.count > 0 && u.refit_days <= 0 && u.landing_ship == null_entity && u.position != null_entity;
}

bool ship_ready(const sea_ship& s)
{
    return s.hp_ppm > 0 && s.muster_days <= 0 && !s.refitting && s.tile != null_entity;
}

/// The completed building @p bid a nation @p nation has declared hostile owns, whose
/// condition matters (`condition_bites`), or null.
entity_id hostile_target_owner(const world& w, entity_id nation, entity_id bid)
{
    const auto bit = w.buildings.find(bid);
    if (bit == w.buildings.end() || bit->second.ticks_remaining > 0 || !condition_bites(bit->second.type))
        return null_entity;
    const entity_id owner = owner_nation_of(w, bid);
    if (owner == null_entity || owner == nation || !is_hostile(w, nation, owner))
        return null_entity;
    return owner;
}

/// Milli-missiles of a @p salvo that get through @p use interceptors, each stopping
/// @p eff per-mille of one missile, `per` to a missile.
int64_t landed_milli(int salvo, int use, int per, int eff)
{
    per = std::max(1, per);
    const int64_t survive1 = std::clamp(1000 - eff, 0, 1000);
    const auto survive = [&](int k) {
        int64_t s = 1000;
        for (int i = 0; i < k; ++i)
            s = s * survive1 / 1000;
        return s;
    };
    const int full = use / per;
    const int part = use % per;
    const int open = salvo - full - (part > 0 ? 1 : 0);
    return int64_t{std::max(0, open)} * 1000 + int64_t{full} * survive(per) + (part > 0 ? survive(part) : 0);
}

} // namespace

// ---------------------------------------------------------------------------
// Launchers
// ---------------------------------------------------------------------------

missile_launcher_type missile_launcher_of(const world& w, entity_id launcher)
{
    if (launcher == null_entity)
        return missile_launcher_type::none;
    if (w.units.find(launcher) != w.units.end())
        return missile_launcher_type::land_unit;
    if (w.ships.find(launcher) != w.ships.end())
        return missile_launcher_type::ship;
    if (w.wings.find(launcher) != w.wings.end())
        return missile_launcher_type::wing;
    return missile_launcher_type::none;
}

bool missile_launcher_carries(const world& w, entity_id launcher, missile_kind k)
{
    const bool s = k == missile_kind::short_range;
    const bool l = k == missile_kind::long_range;
    const bool n = k == missile_kind::tactical_nuclear;
    switch (missile_launcher_of(w, launcher))
    {
        case missile_launcher_type::land_unit:
            return unit_land_class(w.units.at(launcher)) == land_class::artillery && (s || n);
        case missile_launcher_type::ship:
            switch (ship_class_of(w.ships.at(launcher)))
            {
                case ship_class::destroyer: return s;
                case ship_class::capital:   return s || l;
                case ship_class::submarine: return l || n;
                default:                    return false;
            }
        case missile_launcher_type::wing:
            switch (wing_class(w.wings.at(launcher)))
            {
                case air_class::multirole: return s;
                case air_class::bomber:    return l || n;
                default:                   return false;
            }
        default:
            return false;
    }
}

entity_id missile_launch_tile(const world& w, const recipe_registry& reg, entity_id launcher,
                              missile_kind k, int* reach_out)
{
    if (reach_out != nullptr)
        *reach_out = -1;
    if (!fireable(static_cast<uint32_t>(k)))
        return null_entity;
    const int range = mp(reg).kind[col(k)].range;
    entity_id from = null_entity;
    int reach = range;
    switch (missile_launcher_of(w, launcher))
    {
        case missile_launcher_type::land_unit:
        {
            const unit_component& u = w.units.at(launcher);
            if (unit_ready(u))
                from = u.position;
            break;
        }
        case missile_launcher_type::ship:
        {
            const sea_ship& s = w.ships.at(launcher);
            if (ship_ready(s))
                from = s.tile;
            break;
        }
        case missile_launcher_type::wing:
        {
            const air_wing& wg = w.wings.at(launcher);
            if (wg.muster_days > 0 || wg.hp_ppm <= 0)
                break;
            const auto bit = w.buildings.find(wg.airbase);
            if (bit == w.buildings.end() || bit->second.type != building_type::airbase
                || bit->second.ticks_remaining > 0 || bit->second.decommissioned
                || owner_nation_of(w, wg.airbase) != wg.owner)
                break;
            from  = bit->second.tile;
            reach = range + std::max(0, wing_stats(reg, wg).range); // it flies out, then releases
            break;
        }
        default:
            break;
    }
    if (from != null_entity && reach_out != nullptr)
        *reach_out = reach;
    return from;
}

// ---------------------------------------------------------------------------
// Sight
// ---------------------------------------------------------------------------

bool missile_province_sighted(const world& w, const recipe_registry& reg, entity_id nation,
                              uint32_t province_id)
{
    const province* p = w.provinces.find(province_id);
    if (p == nullptr || !is_live_nation(w, nation))
        return false;
    if (province_holder_for(w, province_id) == nation)
        return true;
    for (const entity_id t : p->tiles)
        if (const auto it = w.tile_to_nation.find(t); it != w.tile_to_nation.end() && it->second == nation)
            return true;
    const int sight = mp(reg).sight_tiles;
    for (const auto& [uid, u] : w.units) // an any-test: its answer cannot depend on order
    {
        if (u.owner != nation || u.count <= 0 || u.position == null_entity)
            continue;
        const int d = air_province_distance(w, u.position, province_id);
        if (d >= 0 && d <= sight)
            return true;
    }
    for (const auto& [sid, s] : w.ships)
    {
        if (s.owner != nation || s.hp_ppm <= 0 || s.muster_days > 0 || s.tile == null_entity)
            continue;
        const int d = air_province_distance(w, s.tile, province_id);
        if (d >= 0 && d <= sight)
            return true;
    }
    for (const auto& [wid, wg] : w.wings)
        if (wg.owner == nation && air_wing_flies(w, wg)
            && std::binary_search(wg.provinces.begin(), wg.provinces.end(), province_id))
            return true;
    for (auto it = w.air_revealed.lower_bound({nation, null_entity});
         it != w.air_revealed.end() && it->first == nation; ++it)
        if (const auto uit = w.units.find(it->second);
            uit != w.units.end() && w.provinces.province_of(uit->second.position) == province_id)
            return true;
    for (auto it = w.sea_sightings.lower_bound({nation, null_entity});
         it != w.sea_sightings.end() && it->first == nation; ++it)
        if (const auto sit = w.ships.find(it->second);
            sit != w.ships.end() && w.provinces.province_of(sit->second.tile) == province_id)
            return true;
    return false;
}

// ---------------------------------------------------------------------------
// Stocks
// ---------------------------------------------------------------------------

int32_t missile_stock(const world& w, entity_id nation, missile_kind k)
{
    if (!fireable(static_cast<uint32_t>(k)))
        return 0;
    const auto it = w.missile_magazines.find(nation);
    return it == w.missile_magazines.end() ? 0 : it->second[col(k)];
}

std::array<float, resource_count> missile_goods(const recipe_registry& reg, missile_kind k, int count,
                                                bool interceptors)
{
    std::array<float, resource_count> out{};
    if (!interceptors && !fireable(static_cast<uint32_t>(k)))
        return out;
    const std::array<float, resource_count>& one =
        interceptors ? mp(reg).interceptor_goods : mp(reg).kind[col(k)].goods;
    for (std::size_t r = 0; r < resource_count; ++r)
        out[r] = one[r] * static_cast<float>(count);
    return out;
}

int missile_nuclear_node()
{
    static constexpr int node = io::dg::find_node("DG-PR-EN-B2");
    return node;
}

corp_command_result stock_missiles_check(const world& w, const recipe_registry& reg, entity_id nation,
                                         uint32_t kind, uint32_t count)
{
    if (!is_live_nation(w, nation) || !fireable(kind))
        return corp_command_result::rejected_invalid;
    if (count < 1 || count > static_cast<uint32_t>(std::max(1, mp(reg).stock_per_verb)))
        return corp_command_result::rejected_invalid;
    const missile_kind k = static_cast<missile_kind>(kind);
    if (k == missile_kind::tactical_nuclear && !nation_holds(w, nation, missile_nuclear_node()))
        return corp_command_result::rejected_tech_locked;
    if (int64_t{missile_stock(w, nation, k)} + count > k_missile_magazine_cap)
        return corp_command_result::rejected_state;
    return corp_command_result::applied;
}

corp_command_result stock_interceptors_check(const world& w, const recipe_registry& reg, entity_id nation,
                                             entity_id unit, uint32_t count)
{
    const auto uit = w.units.find(unit);
    if (uit == w.units.end())
        return corp_command_result::rejected_invalid;
    const unit_component& u = uit->second;
    if (u.owner != nation)
        return corp_command_result::rejected_not_owner;
    if (unit_land_class(u) != land_class::air_defence || u.count <= 0)
        return corp_command_result::rejected_invalid;
    if (count < 1 || count > static_cast<uint32_t>(std::max(1, mp(reg).stock_per_verb)))
        return corp_command_result::rejected_invalid;
    const auto sit = w.interceptor_stocks.find(unit);
    const int64_t have = sit == w.interceptor_stocks.end() ? 0 : sit->second;
    if (have + count > k_interceptor_cap)
        return corp_command_result::rejected_state;
    return corp_command_result::applied;
}

// ---------------------------------------------------------------------------
// The strike
// ---------------------------------------------------------------------------

namespace {

/// What every strike check on @p nation's side reads off the world, gathered ONCE per pass
/// (BL-1431 review): hostile units and hostile buildings by province, ascending id, and a
/// memo of which provinces it has sighted. A check over many launchers, kinds and targets
/// shares one scan, so the cost is one walk of units and buildings, not one per candidate.
/// Every list is ascending, so a check through a scan answers exactly as one without.
struct missile_scan
{
    entity_id nation = null_entity;
    std::map<uint32_t, std::vector<entity_id>> hostile_units;     ///< province -> ascending ids
    std::map<uint32_t, std::vector<entity_id>> hostile_buildings; ///< province -> ascending ids
    std::map<uint32_t, bool> sighted;                             ///< memo of missile_province_sighted
};

missile_scan scan_for(const world& w, entity_id nation)
{
    missile_scan sc;
    sc.nation = nation;
    for (const entity_id uid : sorted_unit_ids(w))
    {
        const unit_component& u = w.units.at(uid);
        if (u.count > 0 && u.owner != nation && is_hostile(w, nation, u.owner))
            sc.hostile_units[w.provinces.province_of(u.position)].push_back(uid);
    }
    std::vector<entity_id> bids;
    for (const auto& kv : w.buildings)
        if (hostile_target_owner(w, nation, kv.first) != null_entity)
            bids.push_back(kv.first);
    std::sort(bids.begin(), bids.end());
    for (const entity_id bid : bids)
        sc.hostile_buildings[w.provinces.province_of(w.buildings.at(bid).tile)].push_back(bid);
    return sc;
}

const std::vector<entity_id>& in_province(const std::map<uint32_t, std::vector<entity_id>>& m, uint32_t p)
{
    static const std::vector<entity_id> none;
    const auto it = m.find(p);
    return it == m.end() ? none : it->second;
}

bool scan_sighted(const world& w, const recipe_registry& reg, missile_scan& sc, uint32_t p)
{
    const auto it = sc.sighted.find(p);
    if (it != sc.sighted.end())
        return it->second;
    const bool s = missile_province_sighted(w, reg, sc.nation, p);
    sc.sighted.emplace(p, s);
    return s;
}

corp_command_result fire_check_scanned(const world& w, const recipe_registry& reg, entity_id nation,
                                       entity_id launcher, uint32_t kind, uint32_t salvo,
                                       uint32_t province_id, entity_id building,
                                       missile_strike_plan* plan_out, missile_scan& sc)
{
    const missile_launcher_type lt = missile_launcher_of(w, launcher);
    if (lt == missile_launcher_type::none)
        return corp_command_result::rejected_invalid;
    const entity_id owner = lt == missile_launcher_type::land_unit ? w.units.at(launcher).owner
                          : lt == missile_launcher_type::ship      ? w.ships.at(launcher).owner
                                                                   : w.wings.at(launcher).owner;
    if (owner != nation || !is_live_nation(w, nation) || sc.nation != nation)
        return corp_command_result::rejected_not_owner;
    // NO VERB FIRES A STRATEGIC WEAPON (MILITARY.md § Missiles): kind 3 and anything
    // past it is refused here, before any other leg is read.
    if (!fireable(kind))
        return corp_command_result::rejected_invalid;
    const missile_kind k = static_cast<missile_kind>(kind);
    if (!missile_launcher_carries(w, launcher, k))
        return corp_command_result::rejected_invalid;
    const missile_params& P = mp(reg);
    if (salvo < 1 || salvo > static_cast<uint32_t>(std::max(1, P.salvo_max)))
        return corp_command_result::rejected_invalid;
    int reach = -1;
    const entity_id from = missile_launch_tile(w, reg, launcher, k, &reach);
    if (from == null_entity)
        return corp_command_result::rejected_state;
    const province* prov = w.provinces.find(province_id);
    if (prov == nullptr)
        return corp_command_result::rejected_invalid;
    const missile_kind_stats& ks = P.kind[col(k)];

    // The target, and whose it is.
    entity_id struck = null_entity;
    const std::vector<entity_id>& hostile_units = in_province(sc.hostile_units, province_id);
    const std::vector<entity_id>& hostile_bids  = in_province(sc.hostile_buildings, province_id);
    if (building != null_entity)
    {
        const auto bit = w.buildings.find(building);
        if (bit == w.buildings.end() || bit->second.ticks_remaining > 0 || !condition_bites(bit->second.type)
            || w.provinces.province_of(bit->second.tile) != province_id)
            return corp_command_result::rejected_invalid;
        struck = hostile_target_owner(w, nation, building);
        if (struck == null_entity)
            return corp_command_result::rejected_state;
    }
    else
    {
        // A province strike needs something to strike (BL-1431 review): a conventional one a
        // hostile unit standing there, an area one a hostile unit or a hostile building. A
        // hostile holder over empty ground is not a target; the salvo is not wasted silently.
        if (hostile_units.empty() && (!ks.area || hostile_bids.empty()))
            return corp_command_result::rejected_state;
        const entity_id holder = province_holder_for(w, province_id);
        if (holder != null_entity && holder != nation && is_hostile(w, nation, holder))
            struck = holder;
        else if (!hostile_units.empty())
            struck = w.units.at(hostile_units.front()).owner;
        else
            struck = hostile_target_owner(w, nation, hostile_bids.front());
    }

    const int dist = building != null_entity ? air_tile_distance(w, from, w.buildings.at(building).tile)
                                             : air_province_distance(w, from, province_id);
    if (dist < 0 || dist > reach)
        return corp_command_result::rejected_placement;
    if (!scan_sighted(w, reg, sc, province_id))
        return corp_command_result::rejected_state;
    if (missile_stock(w, nation, k) < static_cast<int32_t>(salvo))
        return corp_command_result::rejected_funds;
    if (plan_out == nullptr)
        return corp_command_result::applied;

    // --- The plan --------------------------------------------------------------
    missile_strike_plan plan;
    plan.kind   = k;
    plan.salvo  = static_cast<int>(salvo);
    plan.struck = struck;

    // Interceptors: every air defence unit in the province hostile to the firer, either
    // way, ascending id (the stock map's order), until the salvo's doctrine is met. Only a
    // unit standing on the ground draws: one aboard a ship is out of `world::units`, and one
    // in the surf (`landing_ship`) fires nothing, as neither may launch (BL-1431 review).
    int need = static_cast<int>(salvo) * std::max(1, P.interceptors_per_missile);
    for (const auto& [uid, have] : w.interceptor_stocks)
    {
        if (need <= 0)
            break;
        if (have <= 0)
            continue;
        const auto uit = w.units.find(uid);
        if (uit == w.units.end())
            continue;
        const unit_component& u = uit->second;
        if (u.count <= 0 || u.landing_ship != null_entity || u.position == null_entity
            || w.provinces.province_of(u.position) != province_id || u.owner == nation
            || !(is_hostile(w, nation, u.owner) || is_hostile(w, u.owner, nation)))
            continue;
        const int take = std::min(need, static_cast<int>(have));
        plan.interceptor_draws.push_back({uid, take});
        plan.interceptors += take;
        need -= take;
    }
    plan.landed_milli = landed_milli(plan.salvo, plan.interceptors, P.interceptors_per_missile,
                                     ks.intercept_permille);

    const auto unit_hit = [&](entity_id uid) {
        const unit_component& u = w.units.at(uid);
        const int64_t men_pm = std::min<int64_t>(1000, int64_t{ks.unit_men_permille} * plan.landed_milli / 1000);
        const int64_t rd_pm  = std::min<int64_t>(1000, int64_t{ks.unit_readiness_permille} * plan.landed_milli / 1000);
        const int32_t men = static_cast<int32_t>(int64_t{u.count} * men_pm / 1000);
        const int32_t rd  = static_cast<int32_t>(std::min<int64_t>(u.readiness_permille, rd_pm));
        if (men > 0 || rd > 0)
        {
            plan.unit_hits.push_back({uid, men, rd});
            plan.men += men;
        }
    };
    const auto building_hit = [&](entity_id bid) {
        const int32_t cond = w.buildings.at(bid).condition_permille;
        const int64_t loss = std::min<int64_t>(cond, int64_t{ks.building_condition_permille} * plan.landed_milli / 1000);
        if (loss > 0)
        {
            plan.building_hits.push_back({bid, static_cast<int32_t>(loss)});
            plan.condition += static_cast<int>(loss);
        }
    };

    if (ks.area)
    {
        for (const entity_id uid : hostile_units)
            unit_hit(uid);
        for (const entity_id bid : hostile_bids)
            building_hit(bid);
    }
    else if (building != null_entity)
        building_hit(building);
    else if (!hostile_units.empty())
    {
        // A unit in battle first, as tactical bombing (MILITARY.md § Air).
        const std::set<entity_id> fighting = units_in_live_battles(w);
        entity_id target = hostile_units.front();
        for (const entity_id uid : hostile_units)
            if (fighting.count(uid) != 0)
            {
                target = uid;
                break;
            }
        unit_hit(target);
    }
    *plan_out = std::move(plan);
    return corp_command_result::applied;
}

/// Whether a launcher at @p from with @p reach could touch target (@p p, @p bid) at all: the
/// check's own range leg, tested before the full check is built.
bool in_reach(const world& w, entity_id from, int reach, uint32_t p, entity_id bid)
{
    const int dist = bid != null_entity ? air_tile_distance(w, from, w.buildings.at(bid).tile)
                                        : air_province_distance(w, from, p);
    return dist >= 0 && dist <= reach;
}

/// The (province, building or null) candidates a scan offers, in the published order:
/// provinces with a hostile unit ascending, then hostile buildings by ascending id.
std::vector<std::pair<uint32_t, entity_id>> scan_candidates(const missile_scan& sc)
{
    std::vector<std::pair<uint32_t, entity_id>> out;
    for (const auto& [p, ids] : sc.hostile_units)
        if (p != 0 && !ids.empty())
            out.push_back({p, null_entity});
    std::vector<std::pair<entity_id, uint32_t>> bids;
    for (const auto& [p, ids] : sc.hostile_buildings)
        if (p != 0)
            for (const entity_id b : ids)
                bids.push_back({b, p});
    std::sort(bids.begin(), bids.end());
    for (const auto& [b, p] : bids)
        out.push_back({p, b});
    return out;
}

} // namespace

corp_command_result fire_missile_check(const world& w, const recipe_registry& reg, entity_id nation,
                                       entity_id launcher, uint32_t kind, uint32_t salvo,
                                       uint32_t province_id, entity_id building,
                                       missile_strike_plan* plan_out)
{
    missile_scan sc = scan_for(w, nation);
    return fire_check_scanned(w, reg, nation, launcher, kind, salvo, province_id, building, plan_out, sc);
}

std::vector<missile_target> missile_targets(const world& w, const recipe_registry& reg, entity_id nation,
                                            entity_id launcher, missile_kind k)
{
    std::vector<missile_target> out;
    int reach = -1;
    if (!fireable(static_cast<uint32_t>(k)) || !missile_launcher_carries(w, launcher, k))
        return out;
    const entity_id from = missile_launch_tile(w, reg, launcher, k, &reach);
    if (from == null_entity)
        return out;
    missile_scan sc = scan_for(w, nation);
    for (const auto& [p, b] : scan_candidates(sc))
    {
        if (!in_reach(w, from, reach, p, b))
            continue;
        const corp_command_result r =
            fire_check_scanned(w, reg, nation, launcher, static_cast<uint32_t>(k), 1, p, b, nullptr, sc);
        if (r == corp_command_result::applied || r == corp_command_result::rejected_funds)
            out.push_back({p, b});
    }
    return out;
}

void apply_missile_strike(world& w, const recipe_registry& reg, entity_id nation, const missile_strike_plan& plan)
{
    // The magazine.
    auto mit = w.missile_magazines.find(nation);
    if (mit != w.missile_magazines.end())
    {
        int32_t& c = mit->second[col(plan.kind)];
        c = std::max(0, c - plan.salvo);
        if (mit->second == std::array<int32_t, 3>{})
            w.missile_magazines.erase(mit);
    }
    // The interceptors.
    for (const auto& [uid, n] : plan.interceptor_draws)
    {
        const auto sit = w.interceptor_stocks.find(uid);
        if (sit == w.interceptor_stocks.end())
            continue;
        sit->second -= n;
        if (sit->second <= 0)
            w.interceptor_stocks.erase(sit);
    }
    // The damage.
    for (const missile_strike_plan::unit_hit& h : plan.unit_hits)
    {
        const auto uit = w.units.find(h.unit);
        if (uit == w.units.end())
            continue;
        unit_component& u = uit->second;
        const int men = std::min(u.count, static_cast<int>(h.men));
        u.count -= men;
        u.readiness_permille = std::max<int32_t>(0, u.readiness_permille - h.readiness);
        if (men > 0)
            note_battle_losses(w, nation, u.owner, 0, men); // force_used: the loser observes the firer
    }
    for (const auto& [bid, loss] : plan.building_hits)
        if (const auto bit = w.buildings.find(bid); bit != w.buildings.end())
            bit->second.condition_permille = std::max<int32_t>(0, bit->second.condition_permille - loss);

    // A tactical warhead's Alarm: every other nation toward the firer, the struck nation
    // counted twice, whether or not it got through (MILITARY.md § Missiles).
    if (plan.kind == missile_kind::tactical_nuclear && sentiment_conduct_live(w))
    {
        const missile_params& P = mp(reg);
        std::vector<entity_id> others;
        for (const auto& [nid, nc] : w.nations)
            if (nid != nation && !nc.ended)
                others.push_back(nid);
        std::sort(others.begin(), others.end());
        for (const entity_id o : others)
            queue_conduct(w, o, nation, sentiment_factor_kind::nuclear_used,
                          static_cast<float>(plan.salvo) * (o == plan.struck ? P.nuclear_struck_magnitude : 1.0f));
    }
}

float nuclear_alarm_sum(const world& w, const sentiment_params& sp, entity_id firer, entity_id struck, int salvo,
                        float struck_mag)
{
    // The magnitude rule is apply_missile_strike's.
    const float per = sp.factors[static_cast<std::size_t>(sentiment_factor_kind::nuclear_used)].alarm;
    std::vector<entity_id> others;
    for (const auto& [nid, nc] : w.nations)
        if (nid != firer && !nc.ended)
            others.push_back(nid);
    std::sort(others.begin(), others.end()); // a float sum: fix its order (BL-1431 review)
    float sum = 0.0f;
    for (const entity_id nid : others)
        sum += per * static_cast<float>(salvo) * (nid == struck ? struck_mag : 1.0f);
    return sum;
}

// ---------------------------------------------------------------------------
// The rival pass (AI_OPPONENT.md § 11, "Rival nations fire missiles")
// ---------------------------------------------------------------------------

void score_nation_missiles(world& w, const recipe_registry& reg, entity_id nation, int tick)
{
    // Never the player's nation: no launcher or stock of another nation, the player's included.
    if (nation == null_entity || nation == player_nation(w) || !is_live_nation(w, nation))
        return;
    if (!corp_is_mobilised(w, nation))
        return;
    const missile_params& P = mp(reg);

    const auto issue = [&](corp_command cmd) {
        cmd.tick = tick;
        cmd.corp = nation;
        cmd.as_nation = nation;
        return apply_corp_command(w, reg, cmd) == corp_command_result::applied;
    };

    // --- STOCK: at most one buy of missiles, toward the per-kind target ----------
    for (int ki = 0; ki < missile_fireable_count; ++ki)
    {
        const missile_kind k = static_cast<missile_kind>(ki);
        const int want = P.rival_magazine_target[static_cast<std::size_t>(ki)] - missile_stock(w, nation, k);
        if (want <= 0)
            continue;
        const uint32_t n = static_cast<uint32_t>(std::min(want, std::max(1, P.stock_per_verb)));
        if (stock_missiles_check(w, reg, nation, static_cast<uint32_t>(ki), n) != corp_command_result::applied)
            continue;
        if (kit_goods_price_bound(w, nation, missile_goods(reg, k, static_cast<int>(n))) > 0.25f * purse_room_of(w, nation))
            continue;
        corp_command cmd;
        cmd.verb = corp_verb::stock_missiles;
        cmd.order = static_cast<uint32_t>(ki);
        cmd.unit_type = static_cast<uint16_t>(n);
        if (issue(cmd))
            break;
    }

    // --- STOCK: at most one air defence unit's interceptors ----------------------
    for (const entity_id uid : sorted_unit_ids(w))
    {
        const unit_component& u = w.units.at(uid);
        if (u.owner != nation || u.count <= 0 || unit_land_class(u) != land_class::air_defence)
            continue;
        const auto sit = w.interceptor_stocks.find(uid);
        const int have = sit == w.interceptor_stocks.end() ? 0 : sit->second;
        const int want = P.rival_interceptor_target - have;
        if (want <= 0)
            continue;
        const uint32_t n = static_cast<uint32_t>(std::min(want, std::max(1, P.stock_per_verb)));
        if (stock_interceptors_check(w, reg, nation, uid, n) != corp_command_result::applied)
            continue;
        if (kit_goods_price_bound(w, nation, missile_goods(reg, missile_kind::short_range, static_cast<int>(n), true))
            > 0.25f * purse_room_of(w, nation))
            break;
        corp_command cmd;
        cmd.verb = corp_verb::stock_interceptors;
        cmd.subject = uid;
        cmd.unit_type = static_cast<uint16_t>(n);
        issue(cmd);
        break;
    }

    // --- FIRE: one strike, the best scored ----------------------------------------
    bool any_stock = false;
    for (int ki = 0; ki < missile_fireable_count; ++ki)
        any_stock = any_stock || missile_stock(w, nation, static_cast<missile_kind>(ki)) > 0;
    if (!any_stock)
        return;

    // Targets: provinces with a unit of a nation it has declared hostile, and the hostile
    // buildings whose condition matters, ascending; each kept only when SIGHTED. One scan
    // serves every check below (BL-1431 review): the world does not move inside this pass.
    missile_scan sc = scan_for(w, nation);
    std::vector<std::pair<uint32_t, entity_id>> targets; // (province, building or null)
    for (const auto& t : scan_candidates(sc))
        if (scan_sighted(w, reg, sc, t.first))
            targets.push_back(t);
    if (targets.empty())
        return;

    // Launchers: its own units, ships and wings, ascending id within each.
    std::vector<entity_id> launchers;
    for (const entity_id uid : sorted_unit_ids(w))
        if (w.units.at(uid).owner == nation)
            launchers.push_back(uid);
    for (const auto& [sid, s] : w.ships)
        if (s.owner == nation)
            launchers.push_back(sid);
    for (const auto& [wid, wg] : w.wings)
        if (wg.owner == nation)
            launchers.push_back(wid);

    const sentiment_params& sp = reg.sentiment();
    const float man_value = 20.0f; // credits per man a strike kills: a first cut, logged with BL-1431
    double best = 0.0;
    corp_command best_cmd;
    bool found = false;
    for (const entity_id lid : launchers)
        for (int ki = 0; ki < missile_fireable_count; ++ki)
        {
            const missile_kind k = static_cast<missile_kind>(ki);
            const int32_t stock = missile_stock(w, nation, k);
            if (stock <= 0 || !missile_launcher_carries(w, lid, k))
                continue;
            int reach = -1;
            const entity_id from = missile_launch_tile(w, reg, lid, k, &reach);
            if (from == null_entity)
                continue;
            const int salvo = k == missile_kind::tactical_nuclear ? 1 : std::min<int32_t>(stock, 2);
            for (const auto& [p, bid] : targets)
            {
                if (!in_reach(w, from, reach, p, bid)) // the check's own range leg, first
                    continue;
                missile_strike_plan plan;
                if (fire_check_scanned(w, reg, nation, lid, static_cast<uint32_t>(ki), static_cast<uint32_t>(salvo),
                                       p, bid, &plan, sc) != corp_command_result::applied)
                    continue;
                double value = static_cast<double>(plan.men) * man_value;
                for (const auto& [b, loss] : plan.building_hits)
                {
                    const building_component& bc = w.buildings.at(b);
                    value += static_cast<double>(construction_capex(w, reg, bc.tile, bc.type, bc.target_resource))
                             * static_cast<double>(loss) / 1000.0;
                }
                double cost = static_cast<double>(kit_goods_price_bound(w, nation, missile_goods(reg, k, salvo)))
                              * static_cast<double>(P.rival_value_margin);
                if (k == missile_kind::tactical_nuclear)
                    cost += static_cast<double>(nuclear_alarm_sum(w, sp, nation, plan.struck, salvo,
                                                                  P.nuclear_struck_magnitude))
                            * static_cast<double>(P.rival_alarm_price);
                const double score = value - cost;
                if (score > best)
                {
                    best = score;
                    found = true;
                    best_cmd = corp_command{};
                    best_cmd.verb = corp_verb::fire_missile;
                    best_cmd.subject = lid;
                    best_cmd.order = static_cast<uint32_t>(ki);
                    best_cmd.unit_type = static_cast<uint16_t>(salvo);
                    best_cmd.province = p;
                    best_cmd.counterparty = bid;
                }
            }
        }
    if (found)
        issue(best_cmd);
}
