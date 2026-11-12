// BL-1643 — artillery fires shells. See artillery.hpp.

#include "artillery.hpp"

#include "air_combat.hpp"         // air_tile_distance
#include "battle_system.hpp"      // units_in_live_battles
#include "campaign_battle.hpp"    // campaign_battle_params: the land round's hit conversion
#include "economy_system.hpp"     // march_days_per_tile
#include "recipe_registry.hpp"
#include "sentiment_emitters.hpp" // note_battle_losses
#include "stance.hpp"             // is_hostile
#include "unit_designs.hpp"       // unit_land_class, artillery_kit_range, artillery_kit_traits
#include "unit_roster.hpp"        // unit_land_stats
#include "world.hpp"

#include <algorithm>
#include <vector>

namespace {

std::vector<entity_id> sorted_unit_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.units.size());
    for (const auto& kv : w.units)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool on_the_ground(const unit_component& u)
{
    return u.count > 0 && u.position != null_entity && u.landing_ship == null_entity;
}

} // namespace

int artillery_range_tiles(const recipe_registry& reg, const unit_component& u)
{
    const land_class c = unit_land_class(u);
    if (c != land_class::artillery)
        return 0;
    // Never past sight: a battery fires only at what its side's units can see.
    const int range = reg.military().artillery.base_range + artillery_kit_range(c, u.kit);
    return std::clamp(range, 0, std::max(0, reg.military().missile.sight_tiles));
}

uint8_t artillery_traits(const unit_component& u)
{
    return artillery_kit_traits(unit_land_class(u), u.kit);
}

bool artillery_ready(const recipe_registry& reg, const unit_component& u, entity_id uid,
                     const std::set<entity_id>& fighting)
{
    return artillery_range_tiles(reg, u) > 0 && on_the_ground(u) && u.order.dest == null_entity
        && !unit_refitting(u) && fighting.count(uid) == 0;
}

entity_id artillery_target(const world& w, const recipe_registry& reg, entity_id uid,
                           const std::set<entity_id>& fighting)
{
    const auto bit = w.units.find(uid);
    if (bit == w.units.end())
        return null_entity;
    const unit_component& b = bit->second;
    const int range = artillery_range_tiles(reg, b);
    if (range <= 0 || b.position == null_entity)
        return null_entity;
    const bool cb = (artillery_traits(b) & k_art_counter_battery) != 0;

    // (counter-battery miss, not in battle, distance, id): the lowest wins.
    struct rank { int cb_miss; int calm; int d; entity_id id; };
    bool found = false;
    rank best{};
    for (const auto& [tid, t] : w.units) // the min of a total order: walk order cannot reach it
    {
        if (tid == uid || !on_the_ground(t) || t.owner == b.owner || !is_hostile(w, b.owner, t.owner))
            continue;
        const int d = air_tile_distance(w, b.position, t.position);
        if (d < 0 || d > range)
            continue;
        const rank r{ (cb && unit_land_class(t) == land_class::artillery) ? 0 : 1,
                      fighting.count(tid) != 0 ? 0 : 1, d, tid };
        const auto less = [](const rank& a, const rank& c) {
            if (a.cb_miss != c.cb_miss) return a.cb_miss < c.cb_miss;
            if (a.calm != c.calm) return a.calm < c.calm;
            if (a.d != c.d) return a.d < c.d;
            return a.id < c.id;
        };
        if (!found || less(r, best))
        {
            best  = r;
            found = true;
        }
    }
    return found ? best.id : null_entity;
}

artillery_day_tick run_artillery_day(world& w, const recipe_registry& reg, const std::set<entity_id>& fighting)
{
    artillery_day_tick out;
    const artillery_params& ap = reg.military().artillery;
    const campaign_battle_params land{}; // the land round's hit conversion, as tactical bombing's

    // 1. AIM: every ready battery, ascending id, picks its target and its hits against the
    //    world as it stands at the day's start. Nothing lands yet, so no battery's id decides
    //    a duel: two batteries in range of each other each fire their full day.
    struct strike { entity_id battery; entity_id target; entity_id firer; int64_t hits; };
    std::vector<strike> strikes;
    for (const entity_id uid : sorted_unit_ids(w))
    {
        const auto bit = w.units.find(uid);
        if (bit == w.units.end() || !artillery_ready(reg, bit->second, uid, fighting))
            continue;
        const entity_id target = artillery_target(w, reg, uid, fighting);
        if (target == null_entity)
            continue;
        const unit_component& b = bit->second;
        const unit_component& t = w.units.at(target);

        // The battery's day of fire, on the land round's scale: per man, soft and hard
        // against the target's hardness, rounds a day, the round's day scale - then the
        // share a battery standing off puts in, the share that lands, and its readiness.
        const land_combat_stats bs = unit_land_stats(b);
        const int64_t hardness = std::clamp(unit_land_stats(t).hardness, 0, 1000);
        const int64_t raw = (int64_t{bs.soft} * (1000 - hardness) + int64_t{bs.hard} * hardness) / 1000;
        const int landed = (artillery_traits(b) & k_art_precision) != 0 ? ap.precision_landed_permille
                                                                        : ap.landed_permille;
        int64_t hits = raw * int64_t{b.count} * std::max(1, land.rounds_per_day);
        hits = hits * std::max(0, land.day_damage_permille) / 1000;
        hits = hits * std::clamp(ap.strike_share_permille, 0, 1000) / 1000;
        hits = hits * std::clamp(landed, 0, 1000) / 1000;
        hits = hits * std::clamp<int64_t>(b.readiness_permille, 0, 1000) / 1000;
        if (hits > 0)
            strikes.push_back({ uid, target, b.owner, hits });
    }

    // 2. LAND: each strike in battery id order, into men and readiness through the bank
    //    tactical bombing keeps (air_combat.cpp).
    for (const strike& st : strikes)
    {
        unit_component& t = w.units.at(st.target);
        ++out.strikes;
        std::pair<int32_t, int32_t>& bank = w.air_hit_bank[st.target];
        const int64_t hp_part = st.hits * land.hp_share_permille / 1000;
        int64_t hp_bank = bank.first + hp_part;
        int64_t rd_bank = bank.second + (st.hits - hp_part);
        const int64_t men = std::min<int64_t>(hp_bank / std::max(1, land.hp_cost), t.count);
        hp_bank -= men * std::max(1, land.hp_cost);
        const int before = t.count;
        t.count -= static_cast<int>(men);
        out.men_killed += static_cast<int>(men);
        const int64_t per_permille = int64_t{std::max(1, land.readiness_cost)} * std::max(1, before);
        const int64_t rd_loss = rd_bank * 1000 / per_permille;
        if (rd_loss > 0)
        {
            t.readiness_permille = static_cast<int32_t>(std::max<int64_t>(0, t.readiness_permille - rd_loss));
            rd_bank -= rd_loss * per_permille / 1000;
        }
        bank.first  = static_cast<int32_t>(std::clamp<int64_t>(hp_bank, 0, 1 << 30));
        bank.second = static_cast<int32_t>(std::clamp<int64_t>(rd_bank, 0, 1 << 30));
        if (men > 0)
        {
            note_battle_losses(w, st.firer, t.owner, 0, static_cast<int>(men)); // force_used
            out.losses.push_back({ st.firer, t.owner, static_cast<int>(men) });
        }
    }
    return out;
}

bool artillery_fire_pending(const world& w, const recipe_registry& reg)
{
    std::set<entity_id> fighting;
    bool any = false;
    for (const auto& [uid, u] : w.units) // a pure any-of
        if (unit_land_class(u) == land_class::artillery && on_the_ground(u) && u.order.dest == null_entity)
        {
            any = true;
            break;
        }
    if (!any)
        return false;
    fighting = units_in_live_battles(w);
    for (const auto& [uid, u] : w.units) // a pure any-of
        if (artillery_ready(reg, u, uid, fighting) && artillery_target(w, reg, uid, fighting) != null_entity)
            return true;
    return false;
}

float artillery_march_rate(const world& w, const recipe_registry& reg, const unit_component& u, float base)
{
    if (u.front == null_entity || (artillery_traits(u) & k_art_keeps_pace) == 0)
        return base;
    // The force it supports: the front's other units, by their class's pace; the force
    // moves at its slowest. A pure min over a total order: walk order cannot reach it.
    float force = 0.0f;
    bool any = false;
    for (const auto& [oid, o] : w.units)
    {
        if (o.owner != u.owner || o.front != u.front || o.count <= 0
            || unit_land_class(o) == land_class::artillery)
            continue;
        const float days = march_days_per_tile(reg.military(), o.type);
        if (days <= 0.0f)
            continue;
        const float rate = 1.0f / days;
        force = any ? std::min(force, rate) : rate;
        any = true;
    }
    return any ? std::max(base, force) : base;
}
