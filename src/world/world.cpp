#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

entity_id world::create_entity()
{
    return m_next_id++;
}

entity_id owner_corp_of(const world& w, entity_id building)
{
    if (building == null_entity)
        return null_entity;
    for (const auto& [corp_id, cc] : w.corporations)
        for (const entity_id a : cc.assets)
            if (a == building)
                return corp_id;
    return null_entity;
}

bool is_player_owned(const world& w, entity_id building)
{
    const entity_id corp = owner_corp_of(w, building);
    if (corp == null_entity)
        return false;
    const auto it = w.corporations.find(corp);
    return it != w.corporations.end() && it->second.is_player;
}

entity_id body_of_market(const world& w, entity_id market)
{
    if (market == null_entity)
        return null_entity;
    const auto it = w.markets.find(market);
    return it != w.markets.end() ? it->second.body : null_entity;
}

namespace {

/// True if the player corporation owns a building on `body`.
bool player_present_on(const world& w, entity_id body)
{
    const auto cit = w.corporations.find(w.player_entity);
    if (cit == w.corporations.end())
        return false;
    for (const entity_id bld : cit->second.assets)
    {
        const auto bit = w.buildings.find(bld);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            return true;
    }
    return false;
}

/// True if a live player convoy is in transit on a lane touching `body`.
bool player_lane_active_on(const world& w, entity_id body)
{
    for (const auto& cv : w.convoys)
    {
        if (cv.corp != w.player_entity)
            continue;
        if (body_of_market(w, cv.source_market) == body ||
            body_of_market(w, cv.dest_market) == body)
            return true;
    }
    return false;
}

/// Flat orbital-plane position (AU) of a body — the same projection the supply system
/// uses for lane distance (r*cos(theta), r*sin(theta)); not recursed through parents.
std::pair<float, float> body_pos(const world& w, entity_id id)
{
    const auto it = w.bodies.find(id);
    if (it == w.bodies.end())
        return {0.0f, 0.0f};
    const body_component& bc = it->second;
    return { bc.orbital_radius_au * std::cos(bc.orbital_angle_rad),
             bc.orbital_radius_au * std::sin(bc.orbital_angle_rad) };
}

} // namespace

activity_vis body_activity_visibility(const world& w, entity_id body, int now_tick,
                                      int route_fresh_ticks, int glimpse_fresh_ticks)
{
    if (body == null_entity)
        return activity_vis::unknown;

    // Visible: the player's own presence or a live lane — the current, direct feed.
    if (body == w.home_body || player_present_on(w, body) || player_lane_active_on(w, body))
        return activity_vis::visible;

    // Otherwise the freshest player route touching this body decides known vs stale.
    bool  have_route = false;
    int   freshest   = 0;
    for (const auto& r : w.trade_routes)
    {
        if (r.corp != w.player_entity)
            continue;
        if (r.body_a != body && r.body_b != body)
            continue;
        if (!have_route || r.last_tick > freshest)
            freshest = r.last_tick;
        have_route = true;
    }

    if (!have_route)
    {
        // No route of its own — but a proximity glimpse (BL-099) may light it faintly. A
        // glimpse never rises above known_stale (a peek, not a data feed) and decays once
        // its freshness window passes.
        const auto git = w.body_last_glimpse_tick.find(body);
        if (git != w.body_last_glimpse_tick.end() && now_tick - git->second <= glimpse_fresh_ticks)
            return activity_vis::known_stale;
        return activity_vis::unknown;
    }

    return (now_tick - freshest <= route_fresh_ticks) ? activity_vis::known
                                                      : activity_vis::known_stale;
}

float body_closest_approach_au(const world& w, entity_id body, entity_id lane_a, entity_id lane_b)
{
    constexpr float far_away = std::numeric_limits<float>::max();
    if (body == null_entity || body == lane_a || body == lane_b)
        return far_away;
    if (w.bodies.find(body)   == w.bodies.end() ||
        w.bodies.find(lane_a) == w.bodies.end() ||
        w.bodies.find(lane_b) == w.bodies.end())
        return far_away;

    const auto [px, py] = body_pos(w, body);
    const auto [ax, ay] = body_pos(w, lane_a);
    const auto [bx, by] = body_pos(w, lane_b);

    const float abx  = bx - ax;
    const float aby  = by - ay;
    const float len2 = abx * abx + aby * aby;

    // Project the body onto the segment, clamped to its endpoints; a degenerate segment
    // (coincident endpoints) collapses to the point distance.
    float t = 0.0f;
    if (len2 > 0.0f)
    {
        t = ((px - ax) * abx + (py - ay) * aby) / len2;
        t = std::clamp(t, 0.0f, 1.0f);
    }
    const float cx = ax + t * abx;
    const float cy = ay + t * aby;
    const float dx = px - cx;
    const float dy = py - cy;
    return std::sqrt(dx * dx + dy * dy);
}

void record_proximity_glimpses(world& w, entity_id lane_a, entity_id lane_b, int tick, float radius_au)
{
    if (lane_a == null_entity || lane_b == null_entity || lane_a == lane_b)
        return;

    for (const auto& entry : w.bodies)
    {
        const entity_id id = entry.first;
        if (id == lane_a || id == lane_b || id == w.star_body)
            continue;
        if (body_closest_approach_au(w, id, lane_a, lane_b) <= radius_au)
            w.body_last_glimpse_tick[id] = tick; // latest glimpse wins; tick is monotonic
    }
}
