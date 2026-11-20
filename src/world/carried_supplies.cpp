#include "carried_supplies.hpp"

#include "world.hpp"
#include "battle_system.hpp"   // BL-1455: unit_terrain_effects - .wear, x the equipment drain
#include "recipe_registry.hpp" // military_capability_params
#include "unit_roster.hpp"     // BL-1678: the energy gate that marks a fuel carrier
#include "unit_designs.hpp"    // BL-1678: kit_goods, unit_land_class - what a man's equipment is

#include <cmath>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

int clamp_pm(int v) { return std::clamp(v, 0, 1000); }

/// Sub-per-mille units: one per-mille of stock is k_supply_unit of these. Each stock's
/// remainder (`unit_component::*_remainder`) is kept in them, in [0, k_supply_unit).
constexpr int64_t k_supply_unit = k_supply_remainder_unit;

/// One stock's day (Ben, 2026-10-29): drain @p spend, deliver
/// share x (spend + restock) where restock = @p full_rate x @p restock_pm / 1000,
/// clamped to [0, 1000], with the sub-per-mille part carried in @p rem — so a cover
/// that exactly meets the need neither gains nor loses, whatever the rounding.
/// Integer throughout. A full stock under any cover at or past break-even is a no-op.
/// @p spend_milli is the day's spend in thousandths of a per-mille (so a terrain wear
/// multiplier can scale it without rounding the drain to whole per-mille).
int64_t stock_day(int& stock, int32_t& rem, int64_t spend_milli, int full_rate, int restock_pm, int share)
{
    spend_milli = std::max<int64_t>(0, spend_milli);
    const int64_t s        = clamp_pm(share);
    const int64_t need_u   = spend_milli
                           + static_cast<int64_t>(std::max(0, full_rate)) * std::max(0, restock_pm);
    const int64_t delivery = s * need_u; // in k_supply_unit units
    int64_t v = static_cast<int64_t>(clamp_pm(stock)) * k_supply_unit
              + std::clamp<int64_t>(rem, 0, k_supply_unit - 1)
              - spend_milli * 1000 + delivery;
    // BL-1678: what the delivery actually put into the stock - all of it, less what ran
    // past full - so the account bills goods that arrived, never goods that spilled.
    const int64_t used = delivery - std::max<int64_t>(0, v - 1000 * k_supply_unit);
    v = std::clamp<int64_t>(v, 0, 1000 * k_supply_unit);
    stock = static_cast<int>(v / k_supply_unit);
    rem   = static_cast<int32_t>(v % k_supply_unit);
    return used;
}

/// BL-1678: add today's delivery beyond the camp drain to a stock's account, in thousandths of
/// a per-mille (`unit_component::*_owed`). Both in `stock_day`'s units; a quiet day at full is
/// exactly zero, so it never moves the field.
void owe(int32_t& owed, int64_t used_u, int64_t camp_milli)
{
    const int64_t d = (used_u - camp_milli * 1000) / 1000; // toward zero
    owed = static_cast<int32_t>(std::clamp<int64_t>(int64_t{ owed } + d, -2000000000LL, 2000000000LL));
}

/// The three stocks after one day for @p u, written into @p out; no starvation. @p wear
/// multiplies the equipment drain (BL-1455's terrain hook; 1 = neutral).
void stocks_after_day(const unit_component& u, const carried_supply_params& p, bool fought,
                      unit_component& out, float wear = 1.0f)
{
    // BL-1678 (Ben, 2026-11-19: "the lower of the two"): each stock refills at the LOWER of
    // the road share and the share of its goods the shelf sold last month. The road gap
    // still starves (Ben, 2026-10-29); an unbought ration now starves too.
    const int road       = u.lp_share_permille;
    const auto share_of  = [&](int bought) { return std::min(clamp_pm(road), clamp_pm(bought)); };
    const int ammo_rate  = p.ammo_per_battle_day;
    const int wear_milli = equipment_attrition_milli_per_day(p);
    // The restock is a per-mille of the full daily rate; the wear's sub-per-mille part is
    // rounded up, so a unit that wears at all restocks at a whole rate.
    const int equip_rate = (wear_milli + 999) / 1000 + p.equipment_per_battle_day;
    out.food_permille      = u.food_permille;      out.food_remainder      = u.food_remainder;
    out.ammo_permille      = u.ammo_permille;      out.ammo_remainder      = u.ammo_remainder;
    out.equipment_permille = u.equipment_permille; out.equipment_remainder = u.equipment_remainder;
    out.fuel_permille      = u.fuel_permille;      out.fuel_remainder      = u.fuel_remainder;
    out.food_owed = u.food_owed; out.ammo_owed = u.ammo_owed;
    out.equipment_owed = u.equipment_owed; out.fuel_owed = u.fuel_owed;
    owe(out.food_owed,
        stock_day(out.food_permille, out.food_remainder, int64_t{p.food_per_day} * 1000, p.food_per_day,
                  p.restock_permille, share_of(u.food_bought_permille)),
        int64_t{p.food_per_day} * 1000);
    owe(out.ammo_owed,
        stock_day(out.ammo_permille, out.ammo_remainder, int64_t{fought ? ammo_rate : 0} * 1000, ammo_rate,
                  p.restock_permille, share_of(u.ammo_bought_permille)),
        0);
    // BL-1455: the ground wears kit - the day's equipment drain x the tile's wear for the
    // unit's class (`unit_terrain_effects(...).wear`), rounded to a thousandth of a per-mille.
    const int64_t equip_base_milli = wear_milli + int64_t{fought ? p.equipment_per_battle_day : 0} * 1000;
    const double  w = (std::isfinite(wear) && wear > 0.0f) ? static_cast<double>(wear) : 1.0;
    const int64_t equip_camp_milli = std::llround(static_cast<double>(wear_milli) * w); // the ground's wear is camp
    owe(out.equipment_owed,
        stock_day(out.equipment_permille, out.equipment_remainder,
                  static_cast<int64_t>(std::llround(static_cast<double>(equip_base_milli) * w)),
                  equip_rate, p.restock_permille, share_of(u.equipment_bought_permille)),
        equip_camp_milli);
    // BL-1678: the fuel stock, for a carrier only: camp, plus a day on the march (an order in
    // flight that the march will step: not while fighting, and not on an empty tank, which
    // holds), plus a day fighting.
    if (unit_carries_fuel(u))
    {
        const bool marching  = u.order.dest != null_entity && !fought && u.fuel_permille > 0;
        const int  fuel_rate = p.fuel_per_day + p.fuel_per_march_day + p.fuel_per_battle_day;
        const int  spend     = p.fuel_per_day + (marching ? p.fuel_per_march_day : 0)
                             + (fought ? p.fuel_per_battle_day : 0);
        owe(out.fuel_owed,
            stock_day(out.fuel_permille, out.fuel_remainder, int64_t{spend} * 1000, fuel_rate,
                      p.restock_permille, share_of(u.fuel_bought_permille)),
            int64_t{p.fuel_per_day} * 1000);
    }
}

bool stocks_equal(const unit_component& a, const unit_component& b)
{
    return a.food_permille == b.food_permille && a.food_remainder == b.food_remainder
        && a.ammo_permille == b.ammo_permille && a.ammo_remainder == b.ammo_remainder
        && a.equipment_permille == b.equipment_permille
        && a.equipment_remainder == b.equipment_remainder
        && a.fuel_permille == b.fuel_permille && a.fuel_remainder == b.fuel_remainder
        && a.food_owed == b.food_owed && a.ammo_owed == b.ammo_owed
        && a.equipment_owed == b.equipment_owed && a.fuel_owed == b.fuel_owed;
}

} // namespace

namespace {

/// A day's fall in a food stock at @p share_permille, in `stock_day`'s units (k_supply_unit
/// to the per-mille): today's spend, less what the share delivers of spend + restock. At or
/// below zero the stock does not fall.
int64_t food_drain_per_day(const carried_supply_params& p, int share_permille)
{
    const int64_t spend_milli = int64_t{ std::max(0, p.food_per_day) } * 1000;
    const int64_t need_u      = spend_milli + int64_t{ std::max(0, p.food_per_day) } * std::max(0, p.restock_permille);
    return spend_milli * 1000 - clamp_pm(share_permille) * need_u;
}

/// The first day that ends under one WHOLE per-mille: `unit_starving` reads no remainder, and
/// that day starves, the same order as the day. Requires @p drain > 0.
int64_t food_empty_on(int food_permille, int64_t drain)
{
    const int64_t stock = int64_t{ clamp_pm(food_permille) } * k_supply_unit;
    return stock >= k_supply_unit ? (stock - k_supply_unit) / drain + 1 : 1;
}

} // namespace

int carried_hunger_loss_permille(const carried_supply_params& p, int food_permille, int share_permille, int days)
{
    const int64_t drain = food_drain_per_day(p, share_permille);
    if (drain <= 0 || days <= 0)
        return 0;
    const int64_t starving = std::max<int64_t>(0, days - food_empty_on(food_permille, drain) + 1);
    return static_cast<int>(std::min<int64_t>(1000, starving * 1000 / k_starvation_days));
}

int carried_food_after_days(const carried_supply_params& p, int food_permille, int share_permille, int days)
{
    const int64_t drain = food_drain_per_day(p, share_permille);
    const int64_t stock = int64_t{ clamp_pm(food_permille) } * k_supply_unit - drain * std::max(0, days);
    return static_cast<int>(std::clamp<int64_t>(stock, 0, 1000 * k_supply_unit) / k_supply_unit);
}

int carried_days_to_death(const carried_supply_params& p, int food_permille, int share_permille)
{
    const int64_t drain = food_drain_per_day(p, share_permille);
    if (drain <= 0)
        return -1;
    return static_cast<int>(food_empty_on(food_permille, drain) + k_starvation_days - 1);
}

int carried_hunger_price_permille(const carried_supply_params& p, int food_permille, int share_permille)
{
    const int death = carried_days_to_death(p, food_permille, share_permille);
    if (death < 0)
        return 0;
    const int64_t fastest = carried_days_to_death(p, 1000, 0);
    return static_cast<int>(std::clamp<int64_t>(fastest * 1000 / death, 1, 1000));
}

int carried_combat_permille(const unit_component& u)
{
    const int64_t ammo  = std::max(k_ammo_floor_permille, clamp_pm(u.ammo_permille));
    const int64_t equip = clamp_pm(u.equipment_permille);
    // BL-1678 (Ben, 2026-11-19): fuel scales every combat stat as ammunition does, floored
    // the same; a unit that carries none is never scaled, whatever its field holds.
    const int64_t fuel  = unit_carries_fuel(u) ? std::max(k_ammo_floor_permille, clamp_pm(u.fuel_permille)) : 1000;
    return static_cast<int>(ammo * equip / 1000 * fuel / 1000);
}

int equipment_attrition_milli_per_day(const carried_supply_params& p)
{
    const int64_t m = std::max(0, p.equipment_attrition_per_month);
    return static_cast<int>((m * 1000 + k_econ_tick_days / 2) / k_econ_tick_days);
}

bool unit_carries_fuel(const unit_component& u)
{
    // The fuelled vehicles (Ben, 2026-11-19: "energy-gated classes only"): a vehicle class whose
    // row gates on energy. A pre-modern row's energy gate is a stand-in for saltpetre
    // (unit_roster.hpp), and its foot carries no tank.
    const std::vector<roster_row>& table = unit_roster_table();
    if (u.type >= table.size() || gate_axis_q(table[u.type].gate, hire_axis::energy) <= 0)
        return false;
    const land_class c = unit_land_class(u);
    return c == land_class::motorised || c == land_class::mechanised || c == land_class::armour
        || c == land_class::light_armour;
}

bool unit_out_of_fuel(const unit_component& u)
{
    return u.fuel_permille <= 0 && unit_carries_fuel(u);
}

std::array<float, resource_count> carried_stock_goods(const world& w, const carried_supply_params& p,
                                                      const unit_component& u, int food, int ammo,
                                                      int equip, int fuel)
{
    std::array<float, resource_count> out{};
    const float men = static_cast<float>(std::max(0, u.count));
    if (men <= 0.0f)
        return out;
    const auto frac = [](int pm) { return static_cast<float>(std::max(0, pm)) / 1000.0f; };
    out[static_cast<std::size_t>(resource_type::food_rations)] += frac(food) * p.food_fill_rations_per_head * men;
    out[static_cast<std::size_t>(resource_type::ordnance)]     += frac(ammo) * p.ammo_fill_ordnance_per_head * men;
    if (unit_carries_fuel(u))
        out[static_cast<std::size_t>(resource_type::refined_fuel)] += frac(fuel) * p.fuel_fill_refined_fuel_per_head * men;
    if (equip > 0)
    {
        // The man's KIT (Ben, 2026-11-18: "5% of a unit's kit"): his class's equipment goods
        // and his design's, not the gated axes' flat raising costs a hire also pays.
        (void)w;
        const std::array<float, resource_count> kit = kit_goods(unit_land_class(u), u.kit);
        for (std::size_t r = 0; r < resource_count; ++r)
            out[r] += frac(equip) * kit[r] / static_cast<float>(hire_batch_manpower) * men;
    }
    return out;
}

std::array<int, 4> carried_month_drain(const carried_supply_params& p, const unit_component& u)
{
    return { std::max(0, p.food_per_day) * k_econ_tick_days, 0,
             std::max(0, p.equipment_attrition_per_month),
             unit_carries_fuel(u) ? std::max(0, p.fuel_per_day) * k_econ_tick_days : 0 };
}

int carried_attack_permille(const unit_component& u)
{
    const int64_t combat = carried_combat_permille(u);
    return static_cast<int>(combat * carried_recovery_permille(u) / 1000);
}

int carried_recovery_permille(const unit_component& u)
{
    return unit_hungry(u) ? 1000 - k_hunger_penalty_permille : 1000;
}

int carried_readiness_cap(const unit_component& u)
{
    if (!unit_starving(u))
        return 1000;
    const int est = unit_establishment(u);
    if (est <= 0)
        return 0;
    return clamp_pm(static_cast<int>(static_cast<int64_t>(std::max(0, u.count)) * 1000 / est));
}

carried_supplies_day run_carried_supplies_day(world& w, const carried_supply_params& p,
                                              const std::set<entity_id>& fought,
                                              const military_capability_params* mil)
{
    carried_supplies_day out;
    if (w.units.empty())
        return out;

    // Ascending id: each unit's day reads only itself, so the order cannot reach the
    // result, but the walk is sorted on principle (no hash-layout order anywhere).
    std::vector<entity_id> ids;
    ids.reserve(w.units.size());
    for (const auto& kv : w.units)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    for (const entity_id uid : ids)
    {
        unit_component& u = w.units.at(uid);
        if (u.count <= 0)
            continue;

        // A unit at full stock under cover at or past break-even is untouched: the
        // step writes only what changed, so a fed world at peace costs no daily churn.
        unit_component next;
        const float wear = mil != nullptr ? unit_terrain_effects(w, *mil, u).wear : 1.0f;
        stocks_after_day(u, p, fought.count(uid) != 0, next, wear);
        if (!stocks_equal(u, next))
        {
            u.food_permille       = next.food_permille;
            u.food_remainder      = next.food_remainder;
            u.ammo_permille       = next.ammo_permille;
            u.ammo_remainder      = next.ammo_remainder;
            u.equipment_permille  = next.equipment_permille;
            u.equipment_remainder = next.equipment_remainder;
            u.fuel_permille       = next.fuel_permille;
            u.fuel_remainder      = next.fuel_remainder;
            u.food_owed           = next.food_owed;
            u.ammo_owed           = next.ammo_owed;
            u.equipment_owed      = next.equipment_owed;
            u.fuel_owed           = next.fuel_owed;
        }

        if (unit_starving(u))
        {
            // Establishment / 180 men a day, with the remainder carried, so the whole
            // establishment is gone on day 180 exactly — never on day 179.
            // An unrecorded establishment reads as the current count; pin it once the
            // unit starves, or the full mark would shrink with the men it loses.
            if (u.establishment <= 0)
                u.establishment = u.count;
            const int est = unit_establishment(u);
            const int64_t owed = static_cast<int64_t>(std::max(0, u.starvation_carry)) + est;
            const int lost = static_cast<int>(owed / k_starvation_days);
            u.starvation_carry = static_cast<int>(owed % k_starvation_days);
            const int take = std::min(lost, u.count);
            u.count -= take;
            out.starved += take;
            ++out.starving;
        }
        else if (u.starvation_carry != 0)
        {
            u.starvation_carry = 0; // food returned: the loss stops
        }
        if (unit_hungry(u))
            ++out.hungry;

        const int capped = std::min(clamp_pm(u.readiness_permille), carried_readiness_cap(u));
        if (capped != u.readiness_permille)
            u.readiness_permille = capped;
    }
    return out;
}

bool carried_supplies_pending(const world& w, const carried_supply_params& p,
                              const military_capability_params* mil)
{
    for (const auto& [uid, u] : w.units) // a pure any-of: walk order cannot reach it
    {
        if (u.count <= 0)
            continue;
        if (unit_starving(u))
            return true;
        unit_component next;
        const float wear = mil != nullptr ? unit_terrain_effects(w, *mil, u).wear : 1.0f;
        stocks_after_day(u, p, false, next, wear);
        if (!stocks_equal(u, next))
            return true;
    }
    return false;
}
