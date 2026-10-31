#include "sentiment_emitters.hpp"

#include "fronts.hpp"
#include "modifier_set.hpp"
#include "nation_tech.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <algorithm>
#include <iterator>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

// BL-1458 / BL-1432. See sentiment_emitters.hpp for the emitter map. Every walk
// below that feeds an event is over an ORDERED container or a sorted id list;
// the fold canonicalises anyway, but a sum feeding one event's magnitude must
// not depend on a hash order either, and integer sums are the only sums taken
// over an unordered walk here.

namespace {

bool is_nation(const world& w, entity_id id)
{
    return id != null_entity && w.nations.count(id) != 0;
}

/// Every nation, ascending id.
std::vector<entity_id> nation_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.nations.size());
    for (const auto& [nid, nc] : w.nations)
        ids.push_back(nid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

/// The nation whose ground market @p market's centre stands on, or null.
entity_id market_nation(const world& w, entity_id market)
{
    const auto m = w.markets.find(market);
    if (m == w.markets.end() || m->second.centre_tile == null_entity)
        return null_entity;
    const auto n = w.tile_to_nation.find(m->second.centre_tile);
    return n == w.tile_to_nation.end() ? null_entity : n->second;
}

} // namespace

bool sentiment_conduct_live(const world& w)
{
    // The settle (campaign_settle.cpp) runs the economy step for its ticks with
    // `settle_build_gate` up. Nobody is seated and nothing the settle does is
    // conduct a nation chose, so it writes no sentiment: the opening world holds
    // generation's grudges and nothing else.
    return !w.settle_build_gate;
}

void queue_conduct(world& w, entity_id observer, entity_id subject,
                   sentiment_factor_kind kind, float magnitude)
{
    if (!sentiment_conduct_live(w))
        return;
    w.sentiment_pending.push_back(sentiment_event{ observer, subject, kind, magnitude });
}

void note_trade_landed(world& w, entity_id owner, entity_id dest_market, float value)
{
    if (!sentiment_conduct_live(w))
        return;
    if (!std::isfinite(value) || !(value > 0.0f))
        return;
    const entity_id shipper  = owner_home_nation(w, owner);
    const entity_id receiver = market_nation(w, dest_market);
    if (!is_nation(w, shipper) || !is_nation(w, receiver) || shipper == receiver)
        return; // domestic, or no nation on one end: not trade BETWEEN nations
    w.sentiment_trade_landed[{ shipper, receiver }] += value;
}

void note_battle_losses(world& w, entity_id attacker, entity_id defender,
                        int attacker_lost, int defender_lost)
{
    if (!is_nation(w, attacker) || !is_nation(w, defender) || attacker == defender)
        return;
    // RELATIONS.md § Who emits: "the side that lost strength -> the other".
    if (attacker_lost > 0)
        queue_conduct(w, attacker, defender, sentiment_factor_kind::force_used,
                      static_cast<float>(attacker_lost));
    if (defender_lost > 0)
        queue_conduct(w, defender, attacker, sentiment_factor_kind::force_used,
                      static_cast<float>(defender_lost));
}

void emit_alarm_capability(world& w, const recipe_registry& reg)
{
    if (!sentiment_conduct_live(w))
        return;
    const sentiment_params& sp = reg.sentiment();
    const std::vector<entity_id> nations = nation_ids(w);
    if (nations.size() < 2)
        return;

    const sentiment_factor& tech_f = sp.factors[static_cast<std::size_t>(sentiment_factor_kind::technology_visible)];
    const sentiment_factor& fac_f  = sp.factors[static_cast<std::size_t>(sentiment_factor_kind::facility_visible)];
    const sentiment_factor& bord_f = sp.factors[static_cast<std::size_t>(sentiment_factor_kind::border_force)];
    const bool tech_on = tech_f.access != 0.0f || tech_f.trust != 0.0f || tech_f.alarm != 0.0f;
    const bool fac_on  = (fac_f.access != 0.0f || fac_f.trust != 0.0f || fac_f.alarm != 0.0f)
                      && sp.alarm_large_facility_cost > 0.0f;
    const bool bord_on = bord_f.access != 0.0f || bord_f.trust != 0.0f || bord_f.alarm != 0.0f;

    // --- Visible capability: the public tell, and the large facilities ------
    // Seen by EVERY other nation: a tell is public by definition, and a large
    // single site is the thing that cannot be hidden.
    for (const entity_id subject : nations)
    {
        if (tech_on)
        {
            const float tell = nation_scalar(w, subject, modifier_subject::alarm_tell, effect_target{}, 0.0f);
            if (std::isfinite(tell) && tell > 0.0f)
                for (const entity_id observer : nations)
                    if (observer != subject)
                        queue_conduct(w, observer, subject, sentiment_factor_kind::technology_visible, tell);
        }
        if (fac_on)
        {
            int large = 0;
            if (const std::vector<entity_id>* assets = assets_of(std::as_const(w), subject))
                for (const entity_id bid : *assets)
                {
                    const auto b = w.buildings.find(bid);
                    if (b == w.buildings.end() || b->second.decommissioned || b->second.ticks_remaining > 0)
                        continue;
                    if (reg.economics(b->second.type).build_cost >= sp.alarm_large_facility_cost)
                        ++large;
                }
            if (large > 0)
                for (const entity_id observer : nations)
                    if (observer != subject)
                        queue_conduct(w, observer, subject, sentiment_factor_kind::facility_visible,
                                      static_cast<float>(large));
        }
    }

    // --- Standing force on the observer's border -----------------------------
    // RELATIONS.md section 6: the men the SUBJECT stands where the OBSERVER can
    // see them as a threat - on the border tiles of a front the subject shares
    // with the observer, AND on any tile of the observer's own ground (an army
    // that has crossed is more alarming, not less). Magnitude: thousands of men.
    // The front cache is current: the caller re-anchored it.
    if (bord_on && !w.units.empty())
    {
        // (tile, owner) -> men. An integer sum, so the unordered walk's order
        // cannot reach the result.
        std::map<std::pair<entity_id, entity_id>, int64_t> men_at;
        for (const auto& [uid, u] : w.units)
            if (u.count > 0 && u.position != null_entity && is_nation(w, u.owner))
                men_at[{ u.position, u.owner }] += u.count;
        // (observer, subject) -> men, ascending.
        std::map<std::pair<entity_id, entity_id>, int64_t> facing;
        // Inside the observer's ground: the tile's nation is the observer.
        for (const auto& [key, men] : men_at)
        {
            const auto n = w.tile_to_nation.find(key.first);
            if (n == w.tile_to_nation.end() || n->second == key.second || !is_nation(w, n->second))
                continue;
            facing[{ n->second, key.second }] += men;
        }
        // On the subject's own border span toward the observer (std::map: ascending).
        // A span tile is the subject's own, so it is never counted twice above.
        for (const auto& [subject, fronts] : w.fronts_cache.by_nation)
            for (const front& f : fronts)
            {
                if (!is_nation(w, f.neighbour) || f.neighbour == subject)
                    continue;
                int64_t men = 0;
                for (const entity_id tile : f.span)
                {
                    const auto t = w.tile_to_nation.find(tile);
                    if (t != w.tile_to_nation.end() && t->second == f.neighbour)
                        continue; // the observer's own tile: counted above
                    if (const auto it = men_at.find({ tile, subject }); it != men_at.end())
                        men += it->second;
                }
                if (men > 0)
                    facing[{ f.neighbour, subject }] += men;
            }
        for (const auto& [pair, men] : facing)
            queue_conduct(w, pair.first, pair.second, sentiment_factor_kind::border_force,
                          static_cast<float>(men) / 1000.0f);
    }
}

std::map<entity_id, float> alarm_decay_scales(const world& w)
{
    std::map<entity_id, float> out;
    for (const auto& [nid, st] : w.nation_tech) // only nations holding research can scale
    {
        const float s = nation_scalar(w, nid, modifier_subject::alarm_decay, effect_target{}, 1.0f);
        if (s != 1.0f && std::isfinite(s) && s > 0.0f)
            out[nid] = s;
    }
    return out;
}

std::size_t fold_pending_sentiment(world& w, const recipe_registry& reg)
{
    if (!sentiment_conduct_live(w))
    {
        // Nothing is gathered under the settle, but say so structurally too.
        w.sentiment_pending.clear();
        w.sentiment_trade_landed.clear();
        return 0;
    }
    std::vector<sentiment_event> batch = std::move(w.sentiment_pending);
    w.sentiment_pending.clear();

    // The trade flow: one trade_conducted per party per pair, magnitude the
    // value landed this tick (std::map: ascending pairs).
    std::set<std::pair<entity_id, entity_id>> trading;
    for (const auto& [pair, value] : w.sentiment_trade_landed)
    {
        if (!std::isfinite(value) || !(value > 0.0f))
            continue;
        trading.insert(pair);
        batch.push_back(sentiment_event{ pair.first, pair.second, sentiment_factor_kind::trade_conducted, value });
        batch.push_back(sentiment_event{ pair.second, pair.first, sentiment_factor_kind::trade_conducted, value });
    }
    w.sentiment_trade_landed.clear();

    // Severed trade, through its PERSISTENCE WINDOW (sentiment_trade_streak):
    // a pair is ESTABLISHED after `sentiment_trade_window` consecutive folds with
    // cargo landed, and SEVERED after as many consecutive folds with none - so a
    // convoy that misses a tick severs nothing. It fires once and disarms. The
    // receiver is the one left short; it observes the shipper.
    for (const auto& pair : trading)
        w.sentiment_trade_streaks.emplace(pair, sentiment_trade_streak{});
    for (auto it = w.sentiment_trade_streaks.begin(); it != w.sentiment_trade_streaks.end();)
    {
        sentiment_trade_streak& st = it->second;
        if (trading.count(it->first) != 0)
        {
            st.idle = 0;
            if (st.traded < sentiment_trade_window)
                ++st.traded;
            if (st.traded >= sentiment_trade_window)
                st.armed = true;
        }
        else
        {
            st.traded = 0;
            if (st.idle < sentiment_trade_window)
                ++st.idle;
            if (st.armed && st.idle >= sentiment_trade_window)
            {
                st.armed = false;
                if (is_nation(w, it->first.first) && is_nation(w, it->first.second))
                    batch.push_back(sentiment_event{ it->first.second, it->first.first,
                                                     sentiment_factor_kind::trade_severed, 1.0f });
            }
        }
        // Nothing to remember: not established and not trading now.
        it = (!st.armed && st.traded == 0) ? w.sentiment_trade_streaks.erase(it) : std::next(it);
    }

    if (batch.empty())
        return 0;

    // The subject's "Alarm toward you" scale, once per subject.
    std::map<entity_id, float> scale;
    for (sentiment_event& e : batch)
    {
        auto it = scale.find(e.subject);
        if (it == scale.end())
        {
            float s = 1.0f;
            if (is_nation(w, e.subject))
                s = nation_scalar(w, e.subject, modifier_subject::alarm_toward, effect_target{}, 1.0f);
            if (!std::isfinite(s) || s < 0.0f)
                s = 1.0f;
            it = scale.emplace(e.subject, s).first;
        }
        e.alarm_scale = it->second;
    }

    apply_sentiment_events(w.sentiment, reg.sentiment(), batch);
    return batch.size();
}
