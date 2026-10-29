#include "carried_supplies.hpp"

#include "world.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

int clamp_pm(int v) { return std::clamp(v, 0, 1000); }

/// One stock's day: drain @p spend, deliver @p share x @p spend / 1000 (floor) — the
/// share of today's need — and, at FULL cover only, restock one @p full_rate on top;
/// clamped to [0, 1000]. Integer. Full cover on a full stock is a no-op; full cover on
/// a short stock refills it a full day's rate at a time; short cover never catches up.
int stock_day(int stock, int spend, int full_rate, int share)
{
    spend = std::max(0, spend);
    const int s = clamp_pm(share);
    const int64_t refill = static_cast<int64_t>(spend) * s / 1000
                         + (s >= 1000 ? std::max(0, full_rate) : 0);
    return clamp_pm(static_cast<int>(static_cast<int64_t>(clamp_pm(stock)) - std::max(0, spend) + refill));
}

/// The three stocks after one day for @p u, written into the out-params; no starvation.
void stocks_after_day(const unit_component& u, const carried_supply_params& p, bool fought,
                      int& food, int& ammo, int& equip)
{
    const int share      = u.supply_factor_permille;
    const int food_rate  = p.food_per_day;
    const int ammo_rate  = p.ammo_per_battle_day;
    const int equip_rate = p.equipment_attrition_per_day + p.equipment_per_battle_day;
    food  = stock_day(u.food_permille, food_rate, food_rate, share);
    ammo  = stock_day(u.ammo_permille, fought ? ammo_rate : 0, ammo_rate, share);
    equip = stock_day(u.equipment_permille,
                      p.equipment_attrition_per_day + (fought ? p.equipment_per_battle_day : 0),
                      equip_rate, share);
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
                                              const std::set<entity_id>& fought)
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

        int food = 0, ammo = 0, equip = 0;
        stocks_after_day(u, p, fought.count(uid) != 0, food, ammo, equip);
        u.food_permille      = food;
        u.ammo_permille      = ammo;
        u.equipment_permille = equip;

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
        else
        {
            u.starvation_carry = 0; // food returned: the loss stops
        }
        if (unit_hungry(u))
            ++out.hungry;

        u.readiness_permille = std::min(clamp_pm(u.readiness_permille), carried_readiness_cap(u));
    }
    return out;
}

bool carried_supplies_pending(const world& w, const carried_supply_params& p)
{
    for (const auto& [uid, u] : w.units) // a pure any-of: walk order cannot reach it
    {
        if (u.count <= 0)
            continue;
        if (unit_starving(u))
            return true;
        int food = 0, ammo = 0, equip = 0;
        stocks_after_day(u, p, false, food, ammo, equip);
        if (food != u.food_permille || ammo != u.ammo_permille || equip != u.equipment_permille)
            return true;
    }
    return false;
}
