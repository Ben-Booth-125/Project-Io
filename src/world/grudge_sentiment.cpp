#include "grudge_sentiment.hpp"

#include <cstdio>

// ---------------------------------------------------------------------------
// The Era -1 grudge record's one consumer (BL-898). See the header for why the
// obvious fix — letting the sim's scorer read a grudge — is the forbidden one.
// ---------------------------------------------------------------------------

namespace
{

/// The substrate parameters this seeding folds through. Built here rather than
/// taken from `recipe_registry::sentiment()` for one blunt reason: generation
/// runs with NO LUA STATE — `make_hard_coded_world` has never seen a registry
/// and giving it one to author a single factor row would drag the whole economy
/// loader into world generation.
///
/// So the weights are `grudge_sentiment_params`' own, and they are handed to
/// the substrate through its normal front door rather than written into the map
/// directly. That is the whole point of routing through
/// `apply_sentiment_events`: the seeding inherits the substrate's batch
/// canonicalisation, its saturation bound, its null/self-pair rejection and its
/// "a neutral row is never stored" rule for free, instead of becoming a second
/// write path that has to remember all four.
///
/// DECAY IS ZERO HERE, and deliberately: this call does not advance a tick, it
/// seeds a starting condition. The world's own authored decay
/// (`economy.sentiment`) takes over from the first tick like it does for every
/// other row.
sentiment_params fold_params(const grudge_sentiment_params& p)
{
    sentiment_params sp;
    sp.access_decay_per_tick = 0.0f;
    sp.trust_decay_per_tick  = 0.0f;
    sp.factors[static_cast<std::size_t>(sentiment_factor_kind::historical_grudge)] =
        sentiment_factor{ p.full_access, p.full_trust };
    return sp;
}

/// The successor nation of @p polity, or `null_entity` where it has none.
/// Bounds-checked against the table rather than assumed: the table is sized by
/// the polity count the fold saw, and a grudge naming a polity outside it is a
/// handoff defect the caller wants counted, not a read past the end.
entity_id successor_of(const std::vector<entity_id>& polity_nation, int32_t polity)
{
    if (polity < 0) return null_entity;
    if (static_cast<std::size_t>(polity) >= polity_nation.size()) return null_entity;
    return polity_nation[static_cast<std::size_t>(polity)];
}

} // namespace

grudge_sentiment_report seed_grudge_sentiment(
    sentiment_table&                    t,
    const std::vector<grudge>&          grudges,
    const std::vector<entity_id>&       polity_nation,
    const grudge_sentiment_params&      p,
    std::vector<grudge_sentiment_seed>* out_seeds)
{
    grudge_sentiment_report rep{};
    if (p.inert() || grudges.empty() || polity_nation.empty())
        return rep;

    // Reserved once: one event per grudge is the ceiling, and the batch is
    // small (a sparse table over ~40 polities).
    std::vector<sentiment_event> batch;
    batch.reserve(grudges.size());

    // A SORTED WALK BY CONSTRUCTION. `history_sim_state::grudges` is sorted
    // ascending by (from, to) and searched by binary search precisely so its
    // order is a property of the integers in it; walking it in place preserves
    // that property rather than re-deriving it. (`apply_sentiment_events`
    // canonicalises the batch again before folding, so even a caller that
    // handed us an unsorted vector could not leak its order into the result —
    // but the belt is the substrate's and the braces are ours.)
    for (const grudge& g : grudges)
    {
        const int32_t from = static_cast<int32_t>(g.from);
        const int32_t to   = static_cast<int32_t>(g.to);

        if (g.score <= p.min_score) { ++rep.below_floor; continue; }

        if (static_cast<std::size_t>(from) >= polity_nation.size()
            || static_cast<std::size_t>(to) >= polity_nation.size())
        {
            ++rep.out_of_range;
            continue;
        }

        const entity_id observer = successor_of(polity_nation, from);
        const entity_id subject  = successor_of(polity_nation, to);

        // A QUARREL WITH NO HEIR IS OVER — dropped, never redirected. See the
        // header: attaching it to whoever now holds the ground would be id
        // arithmetic standing in for a successor concept this project does not
        // have.
        if (observer == null_entity || subject == null_entity) { ++rep.no_successor; continue; }

        // Both polities folded into ONE nation. A nation has no sentiment about
        // itself, and this is the ordinary case rather than a defect: the fold
        // in `nation_generation.cpp` collapses an empire's regions into one
        // realm, and two of its constituent polities may well have quarrelled.
        if (observer == subject) { ++rep.self_pair; continue; }

        // The conversion: a fraction of a FULL grudge, clamped. Integer
        // numerator and denominator, one divide, no transcendentals.
        const int32_t capped = g.score > p.score_full ? p.score_full : g.score;
        const float   frac   = static_cast<float>(capped) / static_cast<float>(p.score_full);

        batch.push_back(sentiment_event{ observer, subject,
                                         sentiment_factor_kind::historical_grudge, frac });
        ++rep.rows_seeded;

        if (out_seeds != nullptr)
        {
            grudge_sentiment_seed s;
            s.observer    = observer;
            s.subject     = subject;
            s.from_polity = from;
            s.to_polity   = to;
            s.score       = g.score;
            s.peak        = g.peak;
            s.event_count = g.event_count;
            s.access      = p.full_access * frac;
            s.trust       = p.full_trust  * frac;
            // `grudge::events` is sorted descending by magnitude with a total
            // tie-break, so the largest cause is [0] and the choice is as
            // deterministic as the table.
            if (g.events_kept > 0)
            {
                s.has_cause = true;
                s.cause     = g.events[0];
            }
            out_seeds->push_back(s);
        }
    }

    if (!batch.empty())
        apply_sentiment_events(t, fold_params(p), batch);

    return rep;
}

std::string grudge_sentiment_line(const grudge_sentiment_seed& s, const settlement_state& ss)
{
    // The nation ids are raw entity ids here rather than names: this file never
    // includes `world.hpp` (see the header), so it cannot resolve a name and
    // will not pretend to. The CAUSE is the half that carries the story, and
    // `grudge_event_line` already renders it with its place and its date.
    std::string line = "nation " + std::to_string(static_cast<unsigned long long>(s.observer))
                     + " resents nation " + std::to_string(static_cast<unsigned long long>(s.subject))
                     + " (polity " + std::to_string(static_cast<long long>(s.from_polity))
                     + " -> " + std::to_string(static_cast<long long>(s.to_polity))
                     + ", score " + std::to_string(static_cast<long long>(s.score))
                     + " over " + std::to_string(static_cast<long long>(s.event_count))
                     + " events): access ";

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f trust %.2f", static_cast<double>(s.access),
                  static_cast<double>(s.trust));
    line += buf;

    if (s.has_cause)
        line += " — worst: " + grudge_event_line(s.cause, ss);

    return line;
}
