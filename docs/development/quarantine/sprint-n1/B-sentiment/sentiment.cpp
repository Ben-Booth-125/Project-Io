#include "sentiment.hpp"

#include <algorithm>
#include <cmath>
#include <istream>
#include <iterator>
#include <ostream>

// NOTE THE INCLUDES: there is no "world.hpp" here, and there must never be one.
// THE INVARIANT (sentiment.hpp) is enforced by this file's inability to see a
// stance table, not by its restraint in front of one.

namespace {

// Deliberately a local copy of order_book.cpp's / procurement.cpp's binary
// primitives — see order_book.cpp's own comment on why this is not hoisted into
// a shared header yet.

void write_u32(std::ostream& out, uint32_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

void write_f32(std::ostream& out, float v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

bool read_u32(std::istream& in, uint32_t& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

bool read_f32(std::istream& in, float& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

/// One dimension, one tick of decay, always TOWARD zero from whichever side it
/// is on. A rate of zero leaves the dimension COMPLETELY untouched — not even
/// the epsilon snap runs — so "no decay authored" means "no arithmetic", which
/// is what the inertness rule needs.
///
/// A rate above 1 is clamped rather than trusted: `v - v*1.5` crosses zero and
/// would flip the sentiment's SIGN, turning a nonsense authored number into a
/// silently inverted relationship.
float decayed(float v, float rate, float epsilon)
{
    if (rate <= 0.0f)
        return v;

    const float r = (rate > 1.0f) ? 1.0f : rate;
    v -= v * r;

    // Reach neutral EXACTLY rather than asymptotically. This is why the epsilon
    // is the opposite of a floor: without it the last vanishing fraction would
    // never leave, and the row would never be erased.
    const float e = (epsilon > 0.0f) ? epsilon : 0.0f;
    if (v > -e && v < e)
        return 0.0f;
    return v;
}

/// Symmetric saturation. `limit <= 0` disables it entirely, so an unauthored
/// (zero) limit means "unbounded" rather than "everything is zero" — the
/// surprising reading is the one that silently deletes state.
float saturated(float v, float limit)
{
    if (limit <= 0.0f)
        return v;
    if (v > limit)
        return limit;
    if (v < -limit)
        return -limit;
    return v;
}

/// Canonical batch order: (observer, subject, kind, magnitude). Total over
/// every batch this file will fold, because non-finite magnitudes are filtered
/// out before the sort — a NaN here would break the strict weak ordering the
/// sort requires, not merely produce an odd result.
bool event_order_less(const sentiment_event& a, const sentiment_event& b)
{
    if (a.observer != b.observer)
        return a.observer < b.observer;
    if (a.subject != b.subject)
        return a.subject < b.subject;
    if (a.kind != b.kind)
        return a.kind < b.kind;
    return a.magnitude < b.magnitude;
}

/// An event the substrate will act on at all. Every rejection below mutates
/// NOTHING and is silent: a null id, a self pair (an actor has no sentiment
/// about itself, exactly as a corp cannot stance itself — stance.cpp's
/// `valid_pair`), an out-of-range kind, or a non-finite magnitude.
bool well_formed(const sentiment_event& e)
{
    if (e.observer == null_entity || e.subject == null_entity)
        return false;
    if (e.observer == e.subject)
        return false;
    if (static_cast<std::size_t>(e.kind) >= sentiment_factor_count)
        return false;
    if (!std::isfinite(e.magnitude))
        return false;
    return true;
}

} // namespace

sentiment_value sentiment_toward(const sentiment_table& t, entity_id observer, entity_id subject)
{
    const auto it = t.pairs.find({observer, subject});
    return (it != t.pairs.end()) ? it->second : sentiment_value{};
}

void decay_sentiment(sentiment_table& t, const sentiment_params& p)
{
    // Nothing decays: not one line of arithmetic runs, and no row is visited.
    if (p.access_decay_per_tick <= 0.0f && p.trust_decay_per_tick <= 0.0f)
        return;

    // A sorted walk over (observer, subject) — `pairs` is a std::map, so this
    // is the BL-158 rule satisfied by the container choice rather than by a
    // sort here (and the erase-during-walk idiom below relies on it).
    for (auto it = t.pairs.begin(); it != t.pairs.end();)
    {
        sentiment_value& v = it->second;
        v.access = decayed(v.access, p.access_decay_per_tick, p.neutral_epsilon);
        v.trust  = decayed(v.trust, p.trust_decay_per_tick, p.neutral_epsilon);

        // Absent == neutral, always: a row that has decayed all the way home is
        // erased rather than kept as a row of zeroes, so the table stays sparse
        // and a fully-forgotten pair is indistinguishable from one that never
        // interacted. That indistinguishability IS the no-floor property.
        it = v.neutral() ? t.pairs.erase(it) : std::next(it);
    }
}

void apply_sentiment_events(sentiment_table& t, const sentiment_params& p,
                            const std::vector<sentiment_event>& events)
{
    // No authored weight can move anything, so no event can either.
    if (events.empty() || p.factors_inert())
        return;

    // Canonicalise before folding. Two callers holding the same events in
    // different orders must produce bit-identical tables, and float addition
    // does not commute — so the substrate takes that responsibility once here
    // rather than levying it on every future emitter.
    std::vector<sentiment_event> batch;
    batch.reserve(events.size());
    for (const sentiment_event& e : events)
        if (well_formed(e))
            batch.push_back(e);
    std::stable_sort(batch.begin(), batch.end(), event_order_less);

    for (const sentiment_event& e : batch)
    {
        // THE FOLD. One array index and no branch on which factor this is —
        // adding a factor is a row in `p.factors` (authored in economy.lua),
        // never an `if` here.
        const sentiment_factor& f = p.factors[static_cast<std::size_t>(e.kind)];
        const float delta_access = f.access * e.magnitude;
        const float delta_trust  = f.trust * e.magnitude;

        // A zero-weighted factor leaves NO TRACE: it must not conjure a row of
        // zeroes, which would be a state change (and a serialised one) caused by
        // an authored nothing.
        if (delta_access == 0.0f && delta_trust == 0.0f)
            continue;

        const std::pair<entity_id, entity_id> key{e.observer, e.subject};
        sentiment_value&                      v = t.pairs[key];
        v.access = saturated(v.access + delta_access, p.limit);
        v.trust  = saturated(v.trust + delta_trust, p.limit);

        // An event that cancels a row exactly back to neutral erases it, for
        // decay's reason: absent == neutral, with no second spelling.
        if (v.neutral())
            t.pairs.erase(key);
    }
}

void run_sentiment_step(sentiment_table& t, const sentiment_params& p,
                        const std::vector<sentiment_event>& events)
{
    // The loud statement of the inertness rule. Both halves self-gate as well,
    // so this line is redundant on purpose: it is the one place a reader can see
    // that an unauthored substrate is a no-op in full.
    if (p.inert())
        return;

    // Decay first, then this tick's events — see run_sentiment_step's contract
    // in sentiment.hpp. A thing that just happened is worth what it was authored
    // to be worth, not that minus one tick of forgetting.
    decay_sentiment(t, p);
    apply_sentiment_events(t, p, events);
}

sentiment_band band_of(float value, const sentiment_params& p)
{
    // Presentation only. Returns a word; decides nothing.
    const float limit = (p.limit > 0.0f) ? p.limit : 1.0f;
    const float near_edge = p.band_edge_near * limit;
    const float far_edge  = p.band_edge_far * limit;

    if (value <= -far_edge)
        return sentiment_band::adverse;
    if (value <= -near_edge)
        return sentiment_band::wary;
    if (value >= far_edge)
        return sentiment_band::trusted;
    if (value >= near_edge)
        return sentiment_band::favourable;
    return sentiment_band::neutral;
}

void write_sentiment(const sentiment_table& t, std::ostream& out)
{
    write_u32(out, sentiment_magic);
    write_u32(out, sentiment_version);

    write_u32(out, static_cast<uint32_t>(t.pairs.size()));
    for (const auto& [key, value] : t.pairs)
    {
        write_u32(out, key.first);  // observer
        write_u32(out, key.second); // subject
        write_f32(out, value.access);
        write_f32(out, value.trust);
    }
}

bool read_sentiment(sentiment_table& t, std::istream& in)
{
    uint32_t magic = 0;
    if (!read_u32(in, magic) || magic != sentiment_magic)
        return false;

    uint32_t version = 0;
    if (!read_u32(in, version) || version != sentiment_version)
        return false;

    uint32_t count = 0;
    if (!read_u32(in, count) || count > sentiment_max_records)
        return false;

    // Parse into a local and commit only on success — a half-loaded relational
    // table is worse than none, and the caller's table must survive a rejected
    // stream untouched (the reject-mutates-nothing rule, stance.hpp's too).
    std::map<std::pair<entity_id, entity_id>, sentiment_value> parsed;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t observer = 0, subject = 0;
        if (!read_u32(in, observer) || !read_u32(in, subject))
            return false;

        sentiment_value v;
        if (!read_f32(in, v.access) || !read_f32(in, v.trust))
            return false;

        // The same well-formedness the fold enforces, applied to the stream:
        // a null id, a self pair or a non-finite value cannot have been written
        // by `write_sentiment`, so the stream is corrupt and is rejected rather
        // than reinterpreted.
        if (observer == null_entity || subject == null_entity || observer == subject)
            return false;
        if (!std::isfinite(v.access) || !std::isfinite(v.trust))
            return false;
        // A stored neutral row cannot exist either (absent == neutral), so one
        // in the stream is corruption by the same argument.
        if (v.neutral())
            return false;

        parsed[{observer, subject}] = v;
    }

    t.pairs = std::move(parsed);
    return true;
}
