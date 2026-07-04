#include "world.hpp"

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

} // namespace

activity_vis body_activity_visibility(const world& w, entity_id body, int now_tick,
                                      int route_fresh_ticks)
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
        return activity_vis::unknown;

    return (now_tick - freshest <= route_fresh_ticks) ? activity_vis::known
                                                      : activity_vis::known_stale;
}
