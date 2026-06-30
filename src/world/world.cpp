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
