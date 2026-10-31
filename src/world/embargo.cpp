#include "embargo.hpp"

#include "sentiment_emitters.hpp" // queue_conduct (embargo_imposed)
#include "world.hpp"

bool embargoes(const world& w, entity_id author, entity_id target)
{
    return w.nation_embargoes.count({ author, target }) != 0;
}

embargo_result set_embargo(world& w, entity_id author, entity_id target, bool on)
{
    if (author == null_entity || target == null_entity || author == target
        || w.nations.find(author) == w.nations.end() || w.nations.find(target) == w.nations.end())
        return embargo_result::rejected_invalid;
    const std::pair<entity_id, entity_id> key{ author, target };
    const bool held = w.nation_embargoes.count(key) != 0;
    if (on == held)
        return embargo_result::rejected_state;
    // Validation is complete; everything below is unconditional.
    if (on)
    {
        w.nation_embargoes.insert(key);
        // RELATIONS.md § Who emits: the embargoed -> the author, Access -, Trust -.
        queue_conduct(w, target, author, sentiment_factor_kind::embargo_imposed);
    }
    else
    {
        w.nation_embargoes.erase(key);
    }
    return embargo_result::applied;
}

entity_id embargo_market_nation(const world& w, entity_id market)
{
    const auto m = w.markets.find(market);
    if (m == w.markets.end() || m->second.centre_tile == null_entity)
        return null_entity;
    const auto n = w.tile_to_nation.find(m->second.centre_tile);
    return n == w.tile_to_nation.end() ? null_entity : n->second;
}

bool embargo_refuses(const world& w, entity_id owner, entity_id market)
{
    if (w.nation_embargoes.empty())
        return false;
    const entity_id shipper = owner_home_nation(w, owner);
    if (shipper == null_entity)
        return false;
    const entity_id author = embargo_market_nation(w, market);
    if (author == null_entity || author == shipper)
        return false;
    return embargoes(w, author, shipper);
}
