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
    if (a.magnitude != b.magnitude)
        return a.magnitude < b.magnitude;
    return a.alarm_scale < b.alarm_scale;
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
    if (!std::isfinite(e.magnitude) || !std::isfinite(e.alarm_scale))
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
    decay_sentiment(t, p, nullptr);
}

void decay_sentiment(sentiment_table& t, const sentiment_params& p,
                     const std::map<entity_id, float>* alarm_decay_scale)
{
    // Nothing decays: not one line of arithmetic runs, and no row is visited.
    if (p.access_decay_per_tick <= 0.0f && p.trust_decay_per_tick <= 0.0f
        && p.alarm_decay_per_tick <= 0.0f)
        return;
    const bool scaled = alarm_decay_scale != nullptr && !alarm_decay_scale->empty();

    // BL-1470: the contributions map walks in LOCKSTEP — the same key order, so
    // one forward iterator follows the rows. An entry whose row is missing is an
    // orphan (a caller erased the row directly) and goes as it is passed.
    auto cit = t.contributions.begin();

    // A sorted walk over (observer, subject) — `pairs` is a std::map, so this
    // is the BL-158 rule satisfied by the container choice rather than by a
    // sort here (and the erase-during-walk idiom below relies on it).
    for (auto it = t.pairs.begin(); it != t.pairs.end();)
    {
        while (cit != t.contributions.end() && cit->first < it->first)
            cit = t.contributions.erase(cit);
        std::vector<sentiment_contribution>* shares =
            (cit != t.contributions.end() && cit->first == it->first) ? &cit->second : nullptr;
        sentiment_value& v = it->second;
        v.access = decayed(v.access, p.access_decay_per_tick, p.neutral_epsilon);
        v.trust  = decayed(v.trust, p.trust_decay_per_tick, p.neutral_epsilon);
        // BL-1432: Alarm's own rate, times the SUBJECT's scale where one is
        // given (a held "decays N% faster" node). `decayed` clamps above 1.
        float alarm_rate = p.alarm_decay_per_tick;
        if (scaled && alarm_rate > 0.0f)
        {
            const auto s = alarm_decay_scale->find(it->first.second);
            if (s != alarm_decay_scale->end() && std::isfinite(s->second) && s->second > 0.0f)
                alarm_rate *= s->second;
        }
        v.alarm  = decayed(v.alarm, alarm_rate, p.neutral_epsilon);

        // Absent == neutral, always: a row that has decayed all the way home is
        // erased rather than kept as a row of zeroes, so the table stays sparse
        // and a fully-forgotten pair is indistinguishable from one that never
        // interacted. That indistinguishability IS the no-floor property.
        // BL-1470: each share decays exactly as its row does — the same three
        // rates (Alarm's scaled for this subject), the same snap — and a share
        // home on all three dimensions is dropped.
        if (shares != nullptr)
        {
            if (v.neutral())
                shares->clear();
            for (auto s = shares->begin(); s != shares->end();)
            {
                s->access = decayed(s->access, p.access_decay_per_tick, p.neutral_epsilon);
                s->trust  = decayed(s->trust, p.trust_decay_per_tick, p.neutral_epsilon);
                s->alarm  = decayed(s->alarm, alarm_rate, p.neutral_epsilon);
                s = s->zero() ? shares->erase(s) : std::next(s);
            }
            cit = shares->empty() ? t.contributions.erase(cit) : std::next(cit);
        }

        it = v.neutral() ? t.pairs.erase(it) : std::next(it);
    }
    // Entries past the last row are orphans too.
    while (cit != t.contributions.end())
        cit = t.contributions.erase(cit);
}

const sentiment_contribution* sentiment_contribution_of(const sentiment_table& t,
                                                        entity_id observer, entity_id subject,
                                                        sentiment_factor_kind kind)
{
    const std::pair<entity_id, entity_id> key{observer, subject};
    if (t.pairs.find(key) == t.pairs.end())
        return nullptr;
    const auto cit = t.contributions.find(key);
    if (cit == t.contributions.end())
        return nullptr;
    for (const sentiment_contribution& c : cit->second)
        if (c.kind == kind)
            return &c;
    return nullptr;
}

const sentiment_contribution* sentiment_largest_contributor(const sentiment_table& t,
                                                            entity_id observer, entity_id subject,
                                                            sentiment_dimension dim)
{
    const std::pair<entity_id, entity_id> key{observer, subject};
    const auto row = t.pairs.find(key);
    if (row == t.pairs.end())
        return nullptr;
    const auto cit = t.contributions.find(key);
    if (cit == t.contributions.end())
        return nullptr;

    const auto pick = [dim](const auto& x) {
        return dim == sentiment_dimension::access ? x.access
             : dim == sentiment_dimension::trust  ? x.trust
                                                  : x.alarm;
    };
    const float value = pick(row->second);
    if (value == 0.0f)
        return nullptr;

    // Ascending kind order, strict `>`: a tie keeps the lower kind (the stable key).
    const sentiment_contribution* best = nullptr;
    float best_mag = 0.0f;
    for (const sentiment_contribution& c : cit->second)
    {
        const float share = pick(c);
        if (share == 0.0f || (share > 0.0f) != (value > 0.0f))
            continue;
        const float mag = std::fabs(share);
        if (mag > best_mag)
        {
            best     = &c;
            best_mag = mag;
        }
    }
    return best;
}

void prune_sentiment_contributions(sentiment_table& t)
{
    for (auto cit = t.contributions.begin(); cit != t.contributions.end();)
        cit = t.pairs.count(cit->first) == 0 ? t.contributions.erase(cit) : std::next(cit);
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
        // BL-1432: Alarm folds by the same weight-times-magnitude, and the
        // subject's "Alarm toward you" scale shrinks (or grows) only the Alarm
        // an occurrence RAISES — relief is never discounted by the holder's tech.
        float delta_alarm = f.alarm * e.magnitude;
        if (delta_alarm > 0.0f)
            delta_alarm *= e.alarm_scale;

        // A zero-weighted factor leaves NO TRACE: it must not conjure a row of
        // zeroes, which would be a state change (and a serialised one) caused by
        // an authored nothing.
        if (delta_access == 0.0f && delta_trust == 0.0f && delta_alarm == 0.0f)
            continue;

        const std::pair<entity_id, entity_id> key{e.observer, e.subject};
        sentiment_value&                      v = t.pairs[key];
        v.access = saturated(v.access + delta_access, p.limit);
        v.trust  = saturated(v.trust + delta_trust, p.limit);
        // ALARM IS ONE-SIDED (Ben, 2026-11-03, NR-1115; RELATIONS.md § 6): its floor
        // is 0. Relief cancels Alarm already raised and never takes it past neutral,
        // so trade cannot bank a reserve against a later threat. Reassurance is what
        // Trust measures. A stream from before the ruling may carry a negative
        // Alarm; it decays toward 0 as before, and the next fold floors it.
        v.alarm  = std::max(0.0f, saturated(v.alarm + delta_alarm, p.limit));

        // An event that cancels a row exactly back to neutral erases it, for
        // decay's reason: absent == neutral, with no second spelling.
        if (v.neutral())
        {
            t.pairs.erase(key);
            t.contributions.erase(key); // BL-1470: the shares go with the row
            continue;
        }

        // BL-1470: this kind's share of the row, before saturation and the
        // Alarm floor. The batch is canonical, so the shares are as replayable
        // as the row; the vector stays ascending by kind.
        std::vector<sentiment_contribution>& shares = t.contributions[key];
        auto s = std::lower_bound(shares.begin(), shares.end(), e.kind,
                                  [](const sentiment_contribution& c, sentiment_factor_kind k) {
                                      return c.kind < k;
                                  });
        if (s == shares.end() || s->kind != e.kind)
        {
            sentiment_contribution fresh;
            fresh.kind = e.kind;
            s = shares.insert(s, fresh);
        }
        s->access += delta_access;
        s->trust  += delta_trust;
        s->alarm  += delta_alarm;
        if (s->zero())
            shares.erase(s);
        if (shares.empty())
            t.contributions.erase(key);
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
        write_f32(out, value.alarm); // BL-1432 (version 2)
    }

    // BL-1470 (version 3): the shares of rows that still stand, then the seeds.
    write_sentiment_provenance(t, out);
}

void write_sentiment_provenance(const sentiment_table& t, std::ostream& out)
{
    uint32_t live = 0;
    for (const auto& [key, shares] : t.contributions)
        if (!shares.empty() && t.pairs.count(key) != 0)
            ++live;
    write_u32(out, live);
    for (const auto& [key, shares] : t.contributions)
    {
        if (shares.empty() || t.pairs.count(key) == 0)
            continue;
        write_u32(out, key.first);
        write_u32(out, key.second);
        write_u32(out, static_cast<uint32_t>(shares.size()));
        for (const sentiment_contribution& c : shares)
        {
            const uint8_t k = static_cast<uint8_t>(c.kind);
            out.write(reinterpret_cast<const char*>(&k), 1);
            write_f32(out, c.access);
            write_f32(out, c.trust);
            write_f32(out, c.alarm);
        }
    }

    write_u32(out, static_cast<uint32_t>(t.seeds.size()));
    for (const auto& [key, s] : t.seeds)
    {
        write_u32(out, key.first);
        write_u32(out, key.second);
        write_u32(out, static_cast<uint32_t>(s.year));
        out.write(reinterpret_cast<const char*>(&s.cause_kind), 1);
        write_u32(out, static_cast<uint32_t>(s.score));
        const std::size_t n = std::min<std::size_t>(s.region.size(), sentiment_seed_region_max);
        write_u32(out, static_cast<uint32_t>(n));
        out.write(s.region.data(), static_cast<std::streamsize>(n));
    }
}

bool read_sentiment_provenance(const sentiment_table& t, std::istream& in,
                               std::map<std::pair<entity_id, entity_id>,
                                        std::vector<sentiment_contribution>>& contributions,
                               std::map<std::pair<entity_id, entity_id>, sentiment_seed_record>& seeds)
{
    contributions.clear();
    seeds.clear();

    uint32_t count = 0;
    if (!read_u32(in, count) || count > t.pairs.size())
        return false;
    for (uint32_t i = 0; i < count; ++i)
    {
        std::pair<entity_id, entity_id> key{null_entity, null_entity};
        uint32_t n = 0;
        if (!read_u32(in, key.first) || !read_u32(in, key.second) || !read_u32(in, n))
            return false;
        // Strictly ascending, a row the table holds, 1..count-of-kinds shares.
        if ((!contributions.empty() && !(contributions.rbegin()->first < key))
            || t.pairs.count(key) == 0 || n == 0 || n > sentiment_factor_count)
            return false;
        std::vector<sentiment_contribution> shares;
        shares.reserve(n);
        for (uint32_t j = 0; j < n; ++j)
        {
            uint8_t k = 0;
            sentiment_contribution c;
            in.read(reinterpret_cast<char*>(&k), 1);
            if (!in || !read_f32(in, c.access) || !read_f32(in, c.trust) || !read_f32(in, c.alarm))
                return false;
            if (k >= sentiment_factor_count
                || (!shares.empty() && !(static_cast<uint8_t>(shares.back().kind) < k))
                || !std::isfinite(c.access) || !std::isfinite(c.trust) || !std::isfinite(c.alarm))
                return false;
            c.kind = static_cast<sentiment_factor_kind>(k);
            if (c.zero())
                return false; // a zero share cannot have been written
            shares.push_back(c);
        }
        contributions.emplace(key, std::move(shares));
    }

    uint32_t nseeds = 0;
    if (!read_u32(in, nseeds) || nseeds > sentiment_max_records)
        return false;
    for (uint32_t i = 0; i < nseeds; ++i)
    {
        std::pair<entity_id, entity_id> key{null_entity, null_entity};
        uint32_t year = 0, score = 0, len = 0;
        sentiment_seed_record s;
        if (!read_u32(in, key.first) || !read_u32(in, key.second) || !read_u32(in, year))
            return false;
        in.read(reinterpret_cast<char*>(&s.cause_kind), 1);
        if (!in || !read_u32(in, score) || !read_u32(in, len))
            return false;
        if (key.first == null_entity || key.second == null_entity || key.first == key.second
            || (!seeds.empty() && !(seeds.rbegin()->first < key)) || len > sentiment_seed_region_max)
            return false;
        s.region.resize(len);
        if (len > 0)
        {
            in.read(&s.region[0], static_cast<std::streamsize>(len));
            if (!in)
                return false;
        }
        s.year  = static_cast<int32_t>(year);
        s.score = static_cast<int32_t>(score);
        seeds.emplace(key, std::move(s));
    }
    return true;
}

bool read_sentiment(sentiment_table& t, std::istream& in)
{
    uint32_t magic = 0;
    if (!read_u32(in, magic) || magic != sentiment_magic)
        return false;

    uint32_t version = 0;
    if (!read_u32(in, version)
        || (version != sentiment_version && version != sentiment_version_pre_contributors
            && version != sentiment_version_pre_alarm))
        return false;
    const bool has_alarm = version >= sentiment_version_pre_contributors; // BL-1432: v1 reads Alarm 0

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
        if (has_alarm && !read_f32(in, v.alarm))
            return false;

        // The same well-formedness the fold enforces, applied to the stream:
        // a null id, a self pair or a non-finite value cannot have been written
        // by `write_sentiment`, so the stream is corrupt and is rejected rather
        // than reinterpreted.
        if (observer == null_entity || subject == null_entity || observer == subject)
            return false;
        if (!std::isfinite(v.access) || !std::isfinite(v.trust) || !std::isfinite(v.alarm))
            return false;
        // A stored neutral row cannot exist either (absent == neutral), so one
        // in the stream is corruption by the same argument.
        if (v.neutral())
            return false;

        parsed[{observer, subject}] = v;
    }

    // BL-1470: a version-3 stream carries the provenance; an older one has none.
    sentiment_table staged;
    staged.pairs = std::move(parsed);
    if (version >= sentiment_version
        && !read_sentiment_provenance(staged, in, staged.contributions, staged.seeds))
        return false;

    t = std::move(staged);
    return true;
}
