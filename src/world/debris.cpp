#include "debris.hpp"

#include "orbital_graph.hpp"
#include "space_battle.hpp"
#include "space_convoy.hpp" // prune_space_hauls
#include "space_units.hpp"
#include "stations.hpp"     // lose_station_asset
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <map>

// BL-1598 — see debris.hpp for the design. This is the arithmetic.

namespace {

bool finite_pos(float v) { return std::isfinite(v) && v > 0.0f; }

/// A unit's wreck: its design's dry mass plus its cargo.
float unit_mass(const space_unit& u)
{
    return std::max(0.0f, evaluate_space_design(u.hull, u.design).dry_mass) + std::max(0.0f, u.cargo);
}

/// The mass a lost asset of @p a's class leaves; 0 for a class that leaves none.
float asset_mass(const space_debris_params& dp, const space_asset& a)
{
    switch (a.cls)
    {
    case space_asset_class::satellite:      return dp.satellite_mass;
    case space_asset_class::station_core:   return dp.station_core_mass;
    case space_asset_class::station_module: return dp.station_module_mass;
    default:                                return 0.0f;
    }
}

bool leaves_debris(const space_asset& a)
{
    return (a.cls == space_asset_class::satellite || a.cls == space_asset_class::station_core
            || a.cls == space_asset_class::station_module)
        && orbital_node_is_orbital(a.node.kind);
}

bool in_place(const space_asset& a)
{
    return a.state == space_asset_state::active || a.state == space_asset_state::decaying;
}

/// Erase the levels at or below the floor.
void sweep_floor(world& w)
{
    for (auto it = w.space_debris.begin(); it != w.space_debris.end();)
        it = (it->second <= k_space_debris_floor) ? w.space_debris.erase(it) : std::next(it);
}

} // namespace

float space_debris_at(const world& w, orbital_node node)
{
    const auto it = w.space_debris.find(node);
    return it == w.space_debris.end() ? 0.0f : it->second;
}

float add_space_debris(world& w, const space_debris_params& dp, orbital_node node, float mass)
{
    if (!orbital_node_is_orbital(node.kind) || !finite_pos(mass) || !finite_pos(dp.debris_per_mass))
        return 0.0f;
    const float add = mass * dp.debris_per_mass;
    if (!finite_pos(add))
        return 0.0f;
    float& level = w.space_debris[node];
    const float before = level;
    level = std::min(k_space_debris_max, level + add);
    if (level <= k_space_debris_floor)
    {
        w.space_debris.erase(node);
        return 0.0f;
    }
    return level - before;
}

void run_space_debris_day(world& w, const space_debris_params& dp, const space_unit_day_report& units,
                          const space_battle_day_report& battles, space_debris_day_report* out)
{
    space_debris_day_report r;
    if (w.space_debris.empty() && battles.mass_destroyed.empty())
    {
        if (out != nullptr)
            *out = r;
        return;
    }

    // 1. The battle day's losses, in its own (battle id) order.
    for (const auto& [node, mass] : battles.mass_destroyed)
        r.deposited += add_space_debris(w, dp, node, mass);

    const float per = std::isfinite(dp.unit_damage_per_debris) ? std::max(0.0f, dp.unit_damage_per_debris) : 0.0f;
    if (per > 0.0f && !w.space_debris.empty())
    {
        // The levels the day's damage reads, fixed before any unit dies.
        const std::map<orbital_node, float> level = w.space_debris;
        const auto at = [&level](orbital_node n) {
            const auto it = level.find(n);
            return it == level.end() ? 0.0f : it->second;
        };
        std::vector<std::pair<orbital_node, float>> wrecks; // (node, mass), unit id order

        // 2. PASSED THROUGH: an arrival's path, its two ends excluded, in path order.
        std::map<entity_id, orbital_node> origin(units.arrivals.begin(), units.arrivals.end());
        // 3. SITTING: every unit idle or docked at an orbital node. Ascending id, so a
        // unit's pass-through and its sitting charge land together.
        for (auto it = w.space_units.begin(); it != w.space_units.end();)
        {
            space_unit& u = it->second;
            orbital_node died_at = u.node;
            float dmg = 0.0f;
            bool  dead = false;
            const auto take = [&](orbital_node n) {
                const float d = per * at(n);
                if (!(d > 0.0f) || dead)
                    return;
                const float hit = std::min(d, u.hull_points);
                u.hull_points -= hit;
                dmg += hit;
                if (!(u.hull_points > 0.0f))
                {
                    u.hull_points = 0.0f;
                    dead = true;
                    died_at = n;
                }
            };
            if (const auto oit = origin.find(it->first); oit != origin.end())
            {
                const orbital_path_result path = orbital_path(w, orbital_graph_params{}, oit->second, u.node);
                if (path.ok && path.nodes.size() > 2)
                    for (std::size_t k = 1; k + 1 < path.nodes.size(); ++k)
                    {
                        if (at(path.nodes[k]) > 0.0f)
                            ++r.passed;
                        take(path.nodes[k]);
                    }
            }
            if ((u.state == space_unit_state::idle || u.state == space_unit_state::docked)
                && orbital_node_is_orbital(u.node.kind))
                take(u.node);
            r.damage += dmg;
            if (!dead)
            {
                ++it;
                continue;
            }
            wrecks.emplace_back(died_at, unit_mass(u));
            ++r.units_lost;
            r.crew_lost += std::max(0, u.crew);
            it = w.space_units.erase(it);
        }
        if (r.units_lost > 0)
            prune_space_hauls(w); // a hauler lost takes its cargo (and any trade riding it) with it
        // 4. Their wrecks, after the pass.
        for (const auto& [node, mass] : wrecks)
            r.deposited += add_space_debris(w, dp, node, mass);
    }
    if (out != nullptr)
        *out = r;
}

space_loss_watch watch_space_losses(const world& w)
{
    space_loss_watch watch;
    for (const auto& [id, a] : w.space_assets) // ascending
        if (a.state != space_asset_state::lost && leaves_debris(a))
            watch.assets.push_back(id);
    watch.units.reserve(w.space_units.size());
    for (const auto& [id, u] : w.space_units) // ascending
        watch.units.push_back({id, u.node, unit_mass(u)});
    return watch;
}

void run_space_debris_quarter(world& w, const space_debris_params& dp, const space_loss_watch& watch,
                              space_debris_quarter_report* out)
{
    space_debris_quarter_report r;

    // 1. The quarter's losses: watched assets now lost, watched units now gone.
    for (const entity_id id : watch.assets)
    {
        const auto it = w.space_assets.find(id);
        if (it != w.space_assets.end() && it->second.state == space_asset_state::lost)
            r.deposited += add_space_debris(w, dp, it->second.node, asset_mass(dp, it->second));
    }
    for (const space_loss_watch::unit_mark& m : watch.units)
        if (w.space_units.count(m.id) == 0)
            r.deposited += add_space_debris(w, dp, m.node, m.mass);

    // A lost asset carries no wear.
    for (auto& [id, a] : w.space_assets)
        if (a.state == space_asset_state::lost)
            a.wear = 0.0f;

    if (w.space_debris.empty())
    {
        if (out != nullptr)
            *out = r;
        return;
    }

    // 2. WEAR on the levels as they stand; the worn-out leave their mass after the pass.
    const float wpd = std::isfinite(dp.wear_per_debris) ? std::max(0.0f, dp.wear_per_debris) : 0.0f;
    if (wpd > 0.0f)
    {
        const std::map<orbital_node, float> level = w.space_debris;
        std::vector<std::pair<orbital_node, float>> wrecks;
        for (auto& [id, a] : w.space_assets) // ascending
        {
            if ((a.cls != space_asset_class::satellite && a.cls != space_asset_class::station_core) || !in_place(a)
                || !orbital_node_is_orbital(a.node.kind))
                continue;
            const auto lit = level.find(a.node);
            if (lit == level.end())
                continue;
            a.wear += wpd * lit->second;
            if (!std::isfinite(a.wear) || a.wear >= 1.0f)
            {
                a.wear = 0.0f;
                lose_station_asset(a); // the asset-loss shape: the station step takes a core's station
                ++r.worn_out;
                wrecks.emplace_back(a.node, asset_mass(dp, a));
            }
        }
        for (const auto& [node, mass] : wrecks)
            r.cascade += add_space_debris(w, dp, node, mass);
    }

    // 3. TUGS, ascending unit id: idle at an orbital node with a debris arm.
    const float clear = std::isfinite(dp.tug_clear_per_quarter) ? std::max(0.0f, dp.tug_clear_per_quarter) : 0.0f;
    if (clear > 0.0f)
        for (const auto& [id, u] : w.space_units)
        {
            if (u.hull != space_hull::tug || u.state != space_unit_state::idle || !orbital_node_is_orbital(u.node.kind)
                || u.design[static_cast<std::size_t>(space_unit_slot::debris_arm)] == k_no_space_item)
                continue;
            const auto lit = w.space_debris.find(u.node);
            if (lit == w.space_debris.end())
                continue;
            const float take = std::min(clear, lit->second);
            lit->second -= take;
            r.cleared += take;
        }

    // 4. DRAG: low orbit only.
    const float drag = std::isfinite(dp.low_orbit_drag) ? std::clamp(dp.low_orbit_drag, 0.0f, 1.0f) : 0.0f;
    if (drag > 0.0f)
        for (auto& [node, lv] : w.space_debris)
            if (node.kind == orbital_node_kind::low_orbit)
            {
                const float d = lv * drag;
                lv -= d;
                r.dragged += d;
            }

    sweep_floor(w);
    if (out != nullptr)
        *out = r;
}
