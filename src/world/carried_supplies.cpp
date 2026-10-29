#include "carried_supplies.hpp"

#include "world.hpp"
#include "battle_system.hpp"   // BL-1455: unit_terrain_effects - .wear, x the equipment drain
#include "recipe_registry.hpp" // military_capability_params

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
void stock_day(int& stock, int32_t& rem, int64_t spend_milli, int full_rate, int restock_pm, int share)
{
    spend_milli = std::max<int64_t>(0, spend_milli);
    const int64_t s        = clamp_pm(share);
    const int64_t need_u   = spend_milli
                           + static_cast<int64_t>(std::max(0, full_rate)) * std::max(0, restock_pm);
    const int64_t delivery = s * need_u; // in k_supply_unit units
    int64_t v = static_cast<int64_t>(clamp_pm(stock)) * k_supply_unit
              + std::clamp<int64_t>(rem, 0, k_supply_unit - 1)
              - spend_milli * 1000 + delivery;
    v = std::clamp<int64_t>(v, 0, 1000 * k_supply_unit);
    stock = static_cast<int>(v / k_supply_unit);
    rem   = static_cast<int32_t>(v % k_supply_unit);
}

/// The three stocks after one day for @p u, written into @p out; no starvation. @p wear
/// multiplies the equipment drain (BL-1455's terrain hook; 1 = neutral).
void stocks_after_day(const unit_component& u, const carried_supply_params& p, bool fought,
                      unit_component& out, float wear = 1.0f)
{
    const int share      = u.lp_share_permille; // only the road gap starves (Ben, 2026-10-29)
    const int ammo_rate  = p.ammo_per_battle_day;
    const int equip_rate = p.equipment_attrition_per_day + p.equipment_per_battle_day;
    out.food_permille      = u.food_permille;      out.food_remainder      = u.food_remainder;
    out.ammo_permille      = u.ammo_permille;      out.ammo_remainder      = u.ammo_remainder;
    out.equipment_permille = u.equipment_permille; out.equipment_remainder = u.equipment_remainder;
    stock_day(out.food_permille, out.food_remainder, int64_t{p.food_per_day} * 1000, p.food_per_day,
              p.restock_permille, share);
    stock_day(out.ammo_permille, out.ammo_remainder, int64_t{fought ? ammo_rate : 0} * 1000, ammo_rate,
              p.restock_permille, share);
    // BL-1455: the ground wears kit - the day's equipment drain x the tile's wear for the
    // unit's class (`unit_terrain_effects(...).wear`), rounded to a thousandth of a per-mille.
    const int64_t equip_base = p.equipment_attrition_per_day + (fought ? p.equipment_per_battle_day : 0);
    const double  w = (std::isfinite(wear) && wear > 0.0f) ? static_cast<double>(wear) : 1.0;
    stock_day(out.equipment_permille, out.equipment_remainder,
              static_cast<int64_t>(std::llround(static_cast<double>(equip_base) * 1000.0 * w)),
              equip_rate, p.restock_permille, share);
}

bool stocks_equal(const unit_component& a, const unit_component& b)
{
    return a.food_permille == b.food_permille && a.food_remainder == b.food_remainder
        && a.ammo_permille == b.ammo_permille && a.ammo_remainder == b.ammo_remainder
        && a.equipment_permille == b.equipment_permille
        && a.equipment_remainder == b.equipment_remainder;
}

} // namespace

int carried_combat_permille(const unit_component& u)
{
    const int64_t ammo  = std::max(k_ammo_floor_permille, clamp_pm(u.ammo_permille));
    const int64_t equip = clamp_pm(u.equipment_permille);
    return static_cast<int>(ammo * equip / 1000);
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
