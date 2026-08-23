// ---------------------------------------------------------------------------
// Headless sentiment-substrate harness (BL-545; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// BL-545 makes sentiment the SUBSTRATE: one directed, continuous, derived value
// from an observer to a subject, two dimensions (Access and Trust) at every
// grain. This harness checks src/world/sentiment.{hpp,cpp} against the
// requirement group "sentiment-substrate":
//
//   R1  THE STANCE INVARIANT. Driving sentiment to any value, in either
//       direction, on any pair, mutates NO stance table — not
//       `corp_hostile_pairs`, not `corp_friend_pairs`, not `corp_friend_offers`.
//       Sentiment informs a declaration; it never makes one (Ben, 2026-08-17,
//       reaffirmed 2026-08-22). The row the whole item rests on.
//   R2  DIRECTEDNESS. A->B and B->A move independently, on either dimension.
//   R3  DECAY HAS NO FLOOR. A value driven to either extreme returns to EXACT
//       neutral within the authored window, and its row is erased — so there is
//       no value it cannot leave. This dissolves BL-391's reputation deadlock by
//       construction rather than by a special case. R3h-R3m (Sprint N3, NR-568)
//       pin the AUTHORED rate: a -2 cancellation halves in nine ticks, is erased
//       inside ~130, and a pair with no event never grows a row.
//   R4  TWO DIMENSIONS stored and moving independently, at every grain
//       (corp->corp, nation->corp, nation->nation).
//   R5  INERTNESS. All factor weights and decay at zero => nothing is stored and
//       the world is bit-identical to the pre-item build.
//   R6  DETERMINISM. Two runs of one stream produce identical tables — and so
//       does the same stream in a different order, since the fold canonicalises
//       its batch before folding it.
//   S   SERIALISATION round-trips bit-exactly; a bad stream is rejected leaving
//       the table untouched.
//   W   WELL-FORMEDNESS. Null ids, self pairs, out-of-range kinds and non-finite
//       magnitudes are rejected, mutating nothing.
//
// R1's strongest form is structural and cannot be printed: sentiment.cpp is
// never handed a `world&`, so it has no reach to a stance table at all. The
// check below asserts it anyway — a structural argument nobody checks is one
// refactor away from being false.
//
// The process exits non-zero if any assertion FAILs.
// ---------------------------------------------------------------------------

#include "world/sentiment.hpp"
#include "world/stance.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool same(float a, float b) { return a == b; } // bit-identical, deliberately not epsilon

/// Field-for-field, in sorted key order. The predicate R5 and R6 are stated in.
bool tables_identical(const sentiment_table& a, const sentiment_table& b)
{
    if (a.pairs.size() != b.pairs.size())
        return false;
    auto ia = a.pairs.begin();
    auto ib = b.pairs.begin();
    for (; ia != a.pairs.end(); ++ia, ++ib)
        if (ia->first != ib->first || ia->second != ib->second)
            return false;
    return true;
}

entity_id add_corp(world& w, const char* name)
{
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name    = name;
    cc.balance = 10000.0f;
    w.corporations[c] = cc;
    return c;
}

entity_id add_nation(world& w, const char* name)
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.name       = name;
    w.nations[n]  = nc;
    return n;
}

/// Authored weights used by most rows below. Deliberately small integers so
/// every sum is exact in float and the assertions can be bit-exact.
///   contract_completed  = pure ACCESS  (+5)
///   trade_conducted     = pure TRUST   (+3)
///   contract_cancelled  = pure ACCESS  (-5)
///   force_used          = BOTH         (-7 access, -11 trust)
sentiment_params authored_params()
{
    sentiment_params p;
    p.factors[static_cast<std::size_t>(sentiment_factor_kind::contract_completed)] = {5.0f, 0.0f};
    p.factors[static_cast<std::size_t>(sentiment_factor_kind::trade_conducted)]    = {0.0f, 3.0f};
    p.factors[static_cast<std::size_t>(sentiment_factor_kind::contract_cancelled)] = {-5.0f, 0.0f};
    p.factors[static_cast<std::size_t>(sentiment_factor_kind::force_used)]         = {-7.0f, -11.0f};
    return p;
}

sentiment_event ev(entity_id observer, entity_id subject, sentiment_factor_kind k, float mag = 1.0f)
{
    sentiment_event e;
    e.observer  = observer;
    e.subject   = subject;
    e.kind      = k;
    e.magnitude = mag;
    return e;
}

/// A wide, deterministic event stream over `actors` — every ordered pair, four
/// kinds, magnitudes from a fixed arithmetic sequence. No RNG anywhere.
std::vector<sentiment_event> wide_stream(const std::vector<entity_id>& actors)
{
    static const sentiment_factor_kind kinds[4] = {
        sentiment_factor_kind::contract_completed,
        sentiment_factor_kind::trade_conducted,
        sentiment_factor_kind::contract_cancelled,
        sentiment_factor_kind::force_used,
    };

    std::vector<sentiment_event> s;
    int i = 0;
    for (const entity_id a : actors)
        for (const entity_id b : actors)
        {
            if (a == b)
                continue;
            const float mag = 0.5f + 0.25f * static_cast<float>(i % 7);
            s.push_back(ev(a, b, kinds[i % 4], mag));
            ++i;
        }
    return s;
}

/// A stream with MANY events on ONE key — which is what `wide_stream` does not
/// have, and why an order-independence row built on it proves nothing: with a
/// single event per ordered pair, nothing ever accumulates, so the canonicalising
/// sort in `apply_sentiment_events` is inert and its removal is invisible.
/// (Sprint N1 adversarial pass, 2026-08-22: `std::stable_sort` was deleted from
/// `sentiment.cpp` and all 39 rows still passed.)
///
/// Every event on a key differs in (kind, magnitude) — including two that share a
/// kind, so the comparator's magnitude tiebreak is exercised — which makes the
/// canonical result well defined. And the shape is chosen so the fold is
/// genuinely order-SENSITIVE: the positive events drive the pair to `limit`, and
/// SATURATION makes what happens next depend on what came before it. Fold
/// +15 then -2 and the row sits at 98; fold -2 then +15 and it sits at 100. That
/// is a structural difference, not a low bit, which is what a row guarding a
/// canonicalising sort needs — a low bit could be lost to a compiler's licence,
/// two whole points cannot.
///
/// Verified by mutation, 2026-08-23: deleting `std::stable_sort` from
/// `sentiment.cpp` moves this stream's table by 2.0 and fails R6d-R6f. On
/// `wide_stream` it moves it by nothing at all.
std::vector<sentiment_event> dense_stream(const std::vector<entity_id>& actors)
{
    std::vector<sentiment_event> s;
    for (const entity_id a : actors)
        for (const entity_id b : actors)
        {
            if (a == b)
                continue;
            // Canonical order is (kind, magnitude), so these fold as listed:
            // complete 0.5, complete 3.0, trade 4.0, cancel 0.4, force 0.15.
            // Any other arrival order that reaches the fold un-canonicalised
            // lands somewhere else.
            s.push_back(ev(a, b, sentiment_factor_kind::contract_cancelled, 0.4f));
            s.push_back(ev(a, b, sentiment_factor_kind::contract_completed, 3.0f));
            s.push_back(ev(a, b, sentiment_factor_kind::force_used,         0.15f));
            s.push_back(ev(a, b, sentiment_factor_kind::contract_completed, 0.5f));
            s.push_back(ev(a, b, sentiment_factor_kind::trade_conducted,    4.0f));
        }
    return s;
}

} // namespace

int main()
{
    std::printf("=== sentiment substrate harness (BL-545) ===\n\n");

    // --- R1: THE STANCE INVARIANT -------------------------------------------
    std::printf("-- R1  sentiment informs a declaration; it never makes one --\n");
    {
        world w;
        const entity_id a = add_corp(w, "Alpha");
        const entity_id b = add_corp(w, "Beta");
        const entity_id c = add_corp(w, "Gamma");
        const entity_id d = add_corp(w, "Delta");

        // A representative stance state: one directed hostility, one accepted
        // (symmetric) friendship, one pending offer. All three tables non-empty,
        // so the comparison below cannot pass vacuously.
        check(declare_hostile(w, a, b) == stance_result::applied &&
              offer_friendship(w, a, c) == stance_result::applied &&
              accept_friendship(w, a, c) == stance_result::applied &&
              offer_friendship(w, b, d) == stance_result::applied,
              "R1a fixture: a real stance state exists to be disturbed");

        const std::set<std::pair<entity_id, entity_id>> hostile_before = w.corp_hostile_pairs;
        const std::set<std::pair<entity_id, entity_id>> friends_before = w.corp_friend_pairs;
        const std::set<std::pair<entity_id, entity_id>> offers_before  = w.corp_friend_offers;

        const sentiment_params drive = authored_params(); // no decay: drive HARD
        sentiment_params forget = authored_params();
        forget.access_decay_per_tick = 0.30f;
        forget.trust_decay_per_tick  = 0.30f;

        sentiment_table t;
        const std::vector<entity_id> actors{a, b, c, d};

        // Drive, in both directions, on every ordered pair: 200 ticks of a wide
        // stream saturates every dimension at +/- limit...
        const std::vector<sentiment_event> stream = wide_stream(actors);
        for (int tick = 0; tick < 200; ++tick)
            run_sentiment_step(t, drive, stream);

        const std::size_t rows_after_drive = t.pairs.size();
        bool positive_extreme = false, negative_extreme = false;
        for (const auto& [key, v] : t.pairs)
        {
            if (v.access >= drive.limit || v.trust >= drive.limit)
                positive_extreme = true;
            if (v.access <= -drive.limit || v.trust <= -drive.limit)
                negative_extreme = true;
        }

        // ...and then 400 ticks of pure decay drives every one of them back.
        for (int tick = 0; tick < 400; ++tick)
            run_sentiment_step(t, forget, {});

        check(rows_after_drive == actors.size() * (actors.size() - 1),
              "R1b the drive moved sentiment on EVERY ordered pair (not a "
              "vacuous check)");
        check(positive_extreme && negative_extreme && t.pairs.empty(),
              "R1c ...to both extremes, and all the way home again");
        check(w.corp_hostile_pairs == hostile_before,
              "R1d corp_hostile_pairs is UNTOUCHED by any sentiment value");
        check(w.corp_friend_pairs == friends_before,
              "R1e corp_friend_pairs is UNTOUCHED by any sentiment value");
        check(w.corp_friend_offers == offers_before,
              "R1f corp_friend_offers is UNTOUCHED by any sentiment value");

        // And the stance verbs still behave normally afterwards — nothing was
        // left in a state only a declaration could have reached.
        check(declare_hostile(w, c, d) == stance_result::applied &&
              is_hostile(w, c, d) && !is_hostile(w, d, c),
              "R1g stance still declares normally afterwards, and only a VERB "
              "moves it");
    }

    // --- R2: DIRECTEDNESS ----------------------------------------------------
    std::printf("\n-- R2  A->B and B->A move independently --\n");
    {
        const sentiment_params p = authored_params();
        sentiment_table t;

        const entity_id a = 11, b = 12;

        apply_sentiment_events(t, p, {ev(a, b, sentiment_factor_kind::contract_completed)});
        check(same(sentiment_toward(t, a, b).access, 5.0f) &&
              sentiment_toward(t, b, a) == sentiment_value{},
              "R2a an event A->B moves A's row and leaves B->A exactly neutral");

        apply_sentiment_events(t, p, {ev(b, a, sentiment_factor_kind::trade_conducted, 2.0f)});
        check(same(sentiment_toward(t, a, b).access, 5.0f) &&
              same(sentiment_toward(t, a, b).trust, 0.0f) &&
              same(sentiment_toward(t, b, a).access, 0.0f) &&
              same(sentiment_toward(t, b, a).trust, 6.0f),
              "R2b the reverse direction moves alone - the two rows disagree, "
              "which is the whole point of a directed value");

        // Independent decay, per direction AND per dimension.
        sentiment_params q = p;
        q.access_decay_per_tick = 1.0f; // full decay in one tick
        q.trust_decay_per_tick  = 0.0f; // trust does not decay at all
        decay_sentiment(t, q);
        check(t.pairs.find({a, b}) == t.pairs.end() &&
              same(sentiment_toward(t, b, a).trust, 6.0f),
              "R2c a decay that empties one direction leaves the other standing");
    }

    // --- R3: DECAY HAS NO FLOOR ---------------------------------------------
    std::printf("\n-- R3  no floor: every extreme returns to exact neutral --\n");
    {
        sentiment_params p = authored_params();
        p.access_decay_per_tick = 0.25f;
        p.trust_decay_per_tick  = 0.25f;

        const entity_id a = 21, b = 22;

        // Drive to the POSITIVE extreme: 100 completions saturate access at
        // +limit; 100 trades saturate trust.
        sentiment_table t;
        for (int i = 0; i < 100; ++i)
            apply_sentiment_events(t, p, {ev(a, b, sentiment_factor_kind::contract_completed),
                                          ev(a, b, sentiment_factor_kind::trade_conducted)});
        check(same(sentiment_toward(t, a, b).access, p.limit) &&
              same(sentiment_toward(t, a, b).trust, p.limit),
              "R3a a value can be driven to the positive extreme");

        // 10 ticks in it is on its way back but NOT home — decay is a slope,
        // not a cliff, and the window is the authored one.
        for (int i = 0; i < 10; ++i)
            decay_sentiment(t, p);
        const sentiment_value midway = sentiment_toward(t, a, b);
        check(midway.access < p.limit && midway.access > 0.0f,
              "R3b ...ten ticks later it is partway home, not snapped");

        // The authored window: rate 0.25 from 100 crosses the epsilon by tick 49.
        for (int i = 0; i < 54; ++i)
            decay_sentiment(t, p);
        check(sentiment_toward(t, a, b) == sentiment_value{},
              "R3c ...and within the authored window it is EXACTLY neutral");
        check(t.pairs.find({a, b}) == t.pairs.end(),
              "R3d ...with its row erased: a fully-forgotten pair is "
              "indistinguishable from one that never interacted");

        // The NEGATIVE extreme returns identically. This is the half BL-391's
        // reputation deadlock lived in — a floor a counterparty could never
        // climb out of. There is no floor to be stuck at.
        sentiment_table u;
        for (int i = 0; i < 100; ++i)
            apply_sentiment_events(u, p, {ev(a, b, sentiment_factor_kind::force_used)});
        check(same(sentiment_toward(u, a, b).access, -p.limit) &&
              same(sentiment_toward(u, a, b).trust, -p.limit),
              "R3e a value can be driven to the negative extreme too");

        for (int i = 0; i < 64; ++i)
            decay_sentiment(u, p);
        check(sentiment_toward(u, a, b) == sentiment_value{} &&
              u.pairs.empty(),
              "R3f ...and returns to EXACT neutral from below - no floor, no "
              "deadlock, no special case (BL-391 dissolved by construction)");

        // Nor can decay overshoot into the opposite sign, however the rate was
        // authored: a nonsense rate is clamped, never trusted.
        sentiment_table v;
        apply_sentiment_events(v, p, {ev(a, b, sentiment_factor_kind::contract_completed)});
        sentiment_params wild = p;
        wild.access_decay_per_tick = 4.0f;
        decay_sentiment(v, wild);
        check(sentiment_toward(v, a, b).access == 0.0f,
              "R3g a rate above 1 lands exactly on neutral - it never flips the "
              "sign of a relationship");
    }

    // --- R3h-R3m: THE AUTHORED RATE (Sprint N3 T10, NR-568) --------------------
    std::printf("\n-- R3  the authored rate: a cancellation is forgotten in nine quarters --\n");
    {
        // Ben's ruling (NR-568, 2026-08-23): a cancellation is forgotten in NINE
        // QUARTERS, i.e. a nine-tick half-life, so scripts/economy.lua authors
        //     trust_decay_per_tick = 1 - 2^(-1/9) = 0.074125   (six sig. figs)
        // This fixture does not load Lua, so the SAME LITERAL is hand-set here;
        // if economy.lua's number moves, this one must move with it.
        //
        // REPRESENTATION. `sentiment_value::trust` is an IEEE-754 binary32 and
        // decay is the multiplicative step `v -= v * r` (sentiment.cpp,
        // `decayed`). Nine of those from -2 give -2 * (1 - r)^9, which is -1.0
        // exactly only if r were the irrational 1 - 2^(-1/9). The authored
        // decimal is six significant figures, so (1 - r)^9 is off by about
        // 9 * |dr| / (1 - r) ~ 3e-6 relative, plus nine float roundings of
        // ~6e-8 each. The tolerance below is 1e-5 — about 80 ulps at 1.0 —
        // which is that decimal rounding and nothing looser; "+-1 fixed-point
        // step" has no meaning for a float, so this is the honest equivalent.
        const float authored_rate = 0.074125f;
        sentiment_params p = authored_params();
        p.trust_decay_per_tick  = authored_rate;
        p.access_decay_per_tick = authored_rate;
        // The fold uses this harness's small-integer weights, where
        // contract_cancelled is pure ACCESS; the ruling is about Trust, so the
        // -2 is driven through the seeded procurement weight instead, exactly
        // as the live loader does (reputation_on_cancel = -2 -> Trust).
        p.factors[static_cast<std::size_t>(sentiment_factor_kind::contract_cancelled)] = {0.0f, -2.0f};

        const entity_id buyer = 31, supplier = 32, bystander = 33;

        sentiment_table t;
        apply_sentiment_events(t, p, {ev(buyer, supplier, sentiment_factor_kind::contract_cancelled)});
        check(same(sentiment_toward(t, buyer, supplier).trust, -2.0f) && t.pairs.size() == 1,
              "R3h one cancellation on a quiet pair reads Trust -2, and creates "
              "exactly one row");

        auto within = [](float v, float target, float tol) {
            const float d = v - target;
            return d <= tol && d >= -tol;
        };

        for (int i = 0; i < 9; ++i)
            decay_sentiment(t, p);
        const float after_nine = sentiment_toward(t, buyer, supplier).trust;
        check(within(after_nine, -1.0f, 1e-5f),
              "R3i ...nine ticks later it has HALVED (-1.0 within 1e-5: the "
              "authored decimal's rounding, binary32 multiplicative decay)");

        for (int i = 0; i < 9; ++i)
            decay_sentiment(t, p);
        check(within(sentiment_toward(t, buyer, supplier).trust, -0.5f, 1e-5f),
              "R3j ...and QUARTERED at eighteen: the half-life is a half-life, "
              "not a one-off");

        // No floor: keep going until the row is gone. Magnitude must fall
        // STRICTLY every tick until the epsilon snap erases it - a tick on
        // which it did not move would be a floor by another name. From -2 at
        // this rate the snap lands at tick 129; 200 is a generous ceiling.
        bool strictly_shrinking = true;
        int  erased_at = -1;
        float prev = sentiment_toward(t, buyer, supplier).trust;
        for (int tick = 19; tick <= 200 && erased_at < 0; ++tick)
        {
            decay_sentiment(t, p);
            const auto row = t.pairs.find({buyer, supplier});
            if (row == t.pairs.end())
            {
                erased_at = tick;
                break;
            }
            const float cur = row->second.trust;
            if (!(cur > prev) || cur >= 0.0f) // toward zero from below, never past it
                strictly_shrinking = false;
            prev = cur;
        }
        check(erased_at > 0 && strictly_shrinking,
              "R3k ...then shrinks strictly every tick until the row is ERASED - "
              "it reaches exact neutral and never sits on a floor");
        check(sentiment_toward(t, buyer, supplier) == sentiment_value{} && t.pairs.empty(),
              "R3l ...leaving the table empty: forgotten is indistinguishable "
              "from never-met");

        // A pair with NO event stays at zero throughout: decay visits stored
        // rows only and never manufactures one, so the bystander's rows are
        // absent before, during and after.
        sentiment_table u;
        apply_sentiment_events(u, p, {ev(buyer, supplier, sentiment_factor_kind::contract_cancelled)});
        bool bystander_quiet = true;
        for (int tick = 0; tick < 200; ++tick)
        {
            decay_sentiment(u, p);
            if (sentiment_toward(u, buyer, bystander) != sentiment_value{} ||
                sentiment_toward(u, bystander, supplier) != sentiment_value{} ||
                u.pairs.count({buyer, bystander}) != 0 || u.pairs.count({bystander, supplier}) != 0)
                bystander_quiet = false;
        }
        check(bystander_quiet,
              "R3m a pair with no event stays at exact zero with no row, before, "
              "during and after the driven pair's whole decay");
    }

    // --- R4: TWO DIMENSIONS, AT EVERY GRAIN ---------------------------------
    std::printf("\n-- R4  Access and Trust, independent, at every grain --\n");
    {
        world w;
        const entity_id corp_a   = add_corp(w, "Corp A");
        const entity_id corp_b   = add_corp(w, "Corp B");
        const entity_id nation_a = add_nation(w, "Nation A");
        const entity_id nation_b = add_nation(w, "Nation B");

        const sentiment_params p = authored_params();
        sentiment_table t;

        // The three grains Ben's ruling names, through ONE table and one code
        // path — a substrate whose shape varies by grain is not a substrate.
        const std::pair<entity_id, entity_id> grains[3] = {
            {corp_a, corp_b},     // corp -> corp
            {nation_a, corp_a},   // nation -> corp
            {nation_a, nation_b}, // nation -> nation
        };

        bool both_dims_ok = true, access_alone_ok = true, trust_alone_ok = true;
        for (const auto& [observer, subject] : grains)
        {
            sentiment_table g;
            apply_sentiment_events(g, p, {ev(observer, subject, sentiment_factor_kind::contract_completed)});
            access_alone_ok = access_alone_ok &&
                same(sentiment_toward(g, observer, subject).access, 5.0f) &&
                same(sentiment_toward(g, observer, subject).trust, 0.0f);

            apply_sentiment_events(g, p, {ev(observer, subject, sentiment_factor_kind::trade_conducted)});
            trust_alone_ok = trust_alone_ok &&
                same(sentiment_toward(g, observer, subject).access, 5.0f) &&
                same(sentiment_toward(g, observer, subject).trust, 3.0f);

            // And a two-dimensional factor moves both at once, by its own weights.
            apply_sentiment_events(g, p, {ev(observer, subject, sentiment_factor_kind::force_used)});
            both_dims_ok = both_dims_ok &&
                same(sentiment_toward(g, observer, subject).access, -2.0f) &&
                same(sentiment_toward(g, observer, subject).trust, -8.0f);
        }
        check(access_alone_ok, "R4a an access-only factor moves access and leaves "
                               "trust exactly neutral, at all three grains");
        check(trust_alone_ok, "R4b a trust-only factor moves trust and leaves "
                              "access untouched, at all three grains");
        check(both_dims_ok, "R4c a two-dimensional factor moves both, by its own "
                            "authored weights, at all three grains");

        // The dimensions also decay independently.
        sentiment_table t2;
        apply_sentiment_events(t2, p, {ev(corp_a, corp_b, sentiment_factor_kind::contract_completed),
                                       ev(corp_a, corp_b, sentiment_factor_kind::trade_conducted)});
        sentiment_params q = p;
        q.access_decay_per_tick = 0.0f; // access never forgets
        q.trust_decay_per_tick  = 0.5f; // trust forgets fast
        for (int i = 0; i < 40; ++i)
            decay_sentiment(t2, q);
        check(same(sentiment_toward(t2, corp_a, corp_b).access, 5.0f) &&
              same(sentiment_toward(t2, corp_a, corp_b).trust, 0.0f) &&
              t2.pairs.find({corp_a, corp_b}) != t2.pairs.end(),
              "R4d the two dimensions decay independently - trust returns home "
              "while access is untouched, and the row survives");
    }

    // --- R5: INERTNESS -------------------------------------------------------
    std::printf("\n-- R5  authored zero => nothing happens at all --\n");
    {
        world w;
        const entity_id a = add_corp(w, "Alpha");
        const entity_id b = add_corp(w, "Beta");
        const entity_id c = add_corp(w, "Gamma");
        const uint64_t hash_before = w.state_hash(0);

        const sentiment_params zero; // every field at its default: fully inert
        check(zero.inert() && zero.factors_inert(),
              "R5a the default-constructed params are inert by their own "
              "predicate");

        sentiment_table t;
        const std::vector<sentiment_event> stream = wide_stream({a, b, c});
        for (int tick = 0; tick < 500; ++tick)
            run_sentiment_step(t, zero, stream);

        check(t.pairs.empty(),
              "R5b 500 ticks of a full event stream at zero weights store "
              "NOTHING - not even a row of zeroes");
        check(w.state_hash(0) == hash_before,
              "R5c ...and the world's state hash is unchanged, because the "
              "substrate is never handed the world at all");

        // A HALF-authored params is still inert in effect: decay with nothing to
        // decay cannot manufacture state.
        sentiment_params decay_only;
        decay_only.access_decay_per_tick = 0.5f;
        decay_only.trust_decay_per_tick  = 0.5f;
        sentiment_table u;
        for (int tick = 0; tick < 100; ++tick)
            run_sentiment_step(u, decay_only, stream);
        check(u.pairs.empty(),
              "R5d authored decay with zero weights still stores nothing");

        // And an authored factor whose weights are zero leaves no trace even
        // while OTHER factors are live — the "never store a neutral row" rule.
        sentiment_params partial;
        partial.factors[static_cast<std::size_t>(sentiment_factor_kind::force_used)] = {-1.0f, 0.0f};
        sentiment_table v;
        apply_sentiment_events(v, partial, {ev(a, b, sentiment_factor_kind::trade_conducted, 99.0f)});
        check(v.pairs.empty(),
              "R5e an unauthored factor leaves no row, even alongside authored "
              "ones");
    }

    // --- R6: DETERMINISM -----------------------------------------------------
    std::printf("\n-- R6  same inputs, identical tables --\n");
    {
        world w;
        const entity_id a = add_corp(w, "Alpha");
        const entity_id b = add_corp(w, "Beta");
        const entity_id c = add_corp(w, "Gamma");
        const entity_id d = add_corp(w, "Delta");

        sentiment_params p = authored_params();
        p.access_decay_per_tick = 0.05f;
        p.trust_decay_per_tick  = 0.11f;

        const std::vector<sentiment_event> stream = wide_stream({a, b, c, d});

        sentiment_table run1, run2;
        for (int tick = 0; tick < 250; ++tick)
        {
            run_sentiment_step(run1, p, stream);
            run_sentiment_step(run2, p, stream);
        }
        check(!run1.pairs.empty(), "R6a the run produced a non-empty table (not "
                                   "a vacuous check)");
        check(tables_identical(run1, run2),
              "R6b two runs of one stream produce bit-identical tables, "
              "field-for-field");

        // The stronger claim: the SAME events in a DIFFERENT order also land
        // bit-identically, because the fold canonicalises its batch. An emitter
        // that gathered from an unordered container cannot leak that order.
        //
        // It has to be tested on a stream with SEVERAL events per key. On one
        // event per key the claim is vacuous — nothing accumulates, so no order
        // can matter and the sort could be deleted without a row noticing. That
        // is exactly what this row used to do.
        const std::vector<sentiment_event> dense = dense_stream({a, b, c, d});
        sentiment_table base;
        for (int tick = 0; tick < 250; ++tick)
            run_sentiment_step(base, p, dense);
        check(!base.pairs.empty() && dense.size() > stream.size(),
              "R6c the order-independence rows run on a stream with SEVERAL "
              "events per key - on one per key the claim is vacuous");

        std::vector<sentiment_event> reversed(dense.rbegin(), dense.rend());
        sentiment_table run3;
        for (int tick = 0; tick < 250; ++tick)
            run_sentiment_step(run3, p, reversed);
        check(tables_identical(base, run3),
              "R6d ...and that stream folded in reverse order lands "
              "bit-identically - order-independent by construction");

        // A rotated batch too, so the reversal is not passing by symmetry.
        std::vector<sentiment_event> rotated(dense.begin() + 1, dense.end());
        rotated.push_back(dense.front());
        sentiment_table run4;
        for (int tick = 0; tick < 250; ++tick)
            run_sentiment_step(run4, p, rotated);
        check(tables_identical(base, run4),
              "R6e ...and a rotated batch, so R6d is not passing by symmetry");

        // And a shuffled batch, deterministically permuted: the two above are
        // structured permutations, this one is not.
        std::vector<sentiment_event> shuffled = dense;
        for (std::size_t i = shuffled.size(); i > 1; --i)
            std::swap(shuffled[i - 1], shuffled[(i * 7919u) % i]);
        bool actually_permuted = false;
        for (std::size_t i = 0; i < dense.size(); ++i)
            if (shuffled[i].observer != dense[i].observer ||
                shuffled[i].subject  != dense[i].subject  ||
                shuffled[i].kind     != dense[i].kind     ||
                shuffled[i].magnitude != dense[i].magnitude)
                actually_permuted = true;
        sentiment_table run5;
        for (int tick = 0; tick < 250; ++tick)
            run_sentiment_step(run5, p, shuffled);
        check(actually_permuted && tables_identical(base, run5),
              "R6f ...and an arbitrary permutation, which is the shape an "
              "unordered_map emitter would actually hand over");
    }

    // --- S: serialisation ----------------------------------------------------
    std::printf("\n-- S  the table joins the flat-binary save seam --\n");
    {
        const sentiment_params p = authored_params();
        sentiment_table t;
        apply_sentiment_events(t, p, {ev(31, 32, sentiment_factor_kind::contract_completed, 1.5f),
                                      ev(32, 31, sentiment_factor_kind::force_used, 0.25f),
                                      ev(33, 31, sentiment_factor_kind::trade_conducted)});

        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        write_sentiment(t, ss);

        sentiment_table loaded;
        check(read_sentiment(loaded, ss) && tables_identical(t, loaded),
              "S1 a written table round-trips bit-exactly");

        // A rejected stream must leave the destination UNTOUCHED — the
        // reject-mutates-nothing rule stance.hpp states for its own verbs.
        sentiment_table keeper = t;
        std::stringstream bad(std::ios::in | std::ios::out | std::ios::binary);
        bad.write("junkjunkjunk", 12);
        check(!read_sentiment(keeper, bad) && tables_identical(keeper, t),
              "S2 a stream with the wrong magic is rejected, leaving the table "
              "untouched");

        // ss was consumed by the read above; write a fresh copy to truncate.
        std::stringstream fresh(std::ios::in | std::ios::out | std::ios::binary);
        write_sentiment(t, fresh);
        const std::string whole = fresh.str();
        std::stringstream truncated(whole.substr(0, whole.size() - 3),
                                    std::ios::in | std::ios::out | std::ios::binary);
        sentiment_table keeper2 = t;
        check(!read_sentiment(keeper2, truncated) && tables_identical(keeper2, t),
              "S3 a truncated stream is rejected, leaving the table untouched");

        std::stringstream empty_stream(std::ios::in | std::ios::out | std::ios::binary);
        sentiment_table empty_table;
        write_sentiment(empty_table, empty_stream);
        sentiment_table empty_loaded;
        check(read_sentiment(empty_loaded, empty_stream) && empty_loaded.pairs.empty(),
              "S4 the inert (empty) table writes and reads as an empty table");
    }

    // --- W: well-formedness --------------------------------------------------
    std::printf("\n-- W  a rejected event mutates nothing --\n");
    {
        const sentiment_params p = authored_params();
        sentiment_table t;

        const float nan_mag = std::numeric_limits<float>::quiet_NaN();
        const float inf_mag = std::numeric_limits<float>::infinity();

        apply_sentiment_events(t, p, {
            ev(null_entity, 42, sentiment_factor_kind::contract_completed),
            ev(42, null_entity, sentiment_factor_kind::contract_completed),
            ev(42, 42, sentiment_factor_kind::contract_completed),
            ev(42, 43, sentiment_factor_kind::contract_completed, nan_mag),
            ev(42, 43, sentiment_factor_kind::contract_completed, inf_mag),
        });
        check(t.pairs.empty(),
              "W1 null ids, a self pair, and non-finite magnitudes are all "
              "rejected - nothing is stored");

        // A good event alongside the bad ones still lands: rejection is
        // per-event, not a poisoned batch.
        apply_sentiment_events(t, p, {
            ev(42, 42, sentiment_factor_kind::contract_completed),
            ev(42, 43, sentiment_factor_kind::contract_completed),
            ev(44, 44, sentiment_factor_kind::force_used, nan_mag),
        });
        check(t.pairs.size() == 1 && same(sentiment_toward(t, 42, 43).access, 5.0f),
              "W2 ...and a well-formed event in the same batch still lands");

        // An event that returns a row exactly to neutral erases it.
        apply_sentiment_events(t, p, {ev(42, 43, sentiment_factor_kind::contract_cancelled)});
        check(t.pairs.empty(),
              "W3 an event that cancels a row back to neutral erases it - "
              "absent == neutral, with no second spelling");
    }

    // --- N: the authoring roster --------------------------------------------
    // The names are what economy.lua keys its factor rows by, so a duplicate or
    // an empty one would silently drop or alias an authored weight. Cheap to
    // check, and the check is what keeps the enum and the roster appending in
    // lockstep.
    std::printf("\n-- N  the factor roster is authorable --\n");
    {
        bool all_named = true;
        for (const char* n : sentiment_factor_names)
            all_named = all_named && n != nullptr && n[0] != '\0';
        check(all_named, "N1 every factor kind has a non-empty authoring name");

        std::set<std::string> unique;
        for (const char* n : sentiment_factor_names)
            unique.insert(n);
        check(unique.size() == sentiment_factor_count,
              "N2 ...and no two kinds share one, so no authored row can alias "
              "another");
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
