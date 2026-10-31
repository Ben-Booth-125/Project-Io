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
//   E   THE EMITTERS (BL-1458, RELATIONS.md § Who emits each factor): trade_conducted
//       (once a tick per pair, magnitude the value landed), friendship_accepted,
//       force_used (magnitude the men lost) and trade_severed write their stated
//       rows and signs through the one gathered fold; embargo_imposed is
//       emitted by enacting an embargo (BL-1418) and respects the settle gate.
//   A   ALARM (BL-1432, § 6): the third float, folded, decayed at its own rate,
//       erased only with the other two, scaled per subject, saved; THE INVARIANT
//       holds for it.
//   C   ALARM'S CAPABILITY FACTORS (visible technology, large facility, border
//       force) and the Digitisation rows that write the Alarm dimension.
//
// R1's strongest form is structural and cannot be printed: sentiment.cpp is
// never handed a `world&`, so it has no reach to a stance table at all. The
// check below asserts it anyway — a structural argument nobody checks is one
// refactor away from being false.
//
// The process exits non-zero if any assertion FAILs.
// ---------------------------------------------------------------------------

#include "world/digitisation_tree_data.hpp" // BL-1432: the tree's Alarm rows
#include "world/embargo.hpp" // BL-1418: set_embargo, embargo_imposed's emitter
#include "world/fronts.hpp"
#include "world/nation_tech.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/sentiment_emitters.hpp" // BL-1458: the emitters and the fold
#include "world/stance.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdint>
#include <map>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
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
    nation_component cc{};
    cc.name    = name;
    cc.treasury = 10000.0f;
    w.nations[c] = cc;
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

    // --- E: THE EMITTERS (BL-1458; RELATIONS.md § Who emits each factor) -------
    // Each emitter writes its factor on the stated observer -> subject rows with
    // the stated sign per dimension, through the one gathered, canonicalised fold.
    std::printf("\n-- E  every live factor has its emitter (BL-1458) --\n");
    {
        // Weights signed per the § Who emits table; small dyadic values so every
        // sum is exact. The registry carries them, as the economy step's does.
        sentiment_params p;
        auto set = [&p](sentiment_factor_kind k, float a, float t, float al) {
            p.factors[static_cast<std::size_t>(k)] = sentiment_factor{ a, t, al };
        };
        set(sentiment_factor_kind::trade_conducted,     0.25f, 0.25f, -0.25f);
        set(sentiment_factor_kind::friendship_accepted, 4.0f,  4.0f,  -2.0f);
        set(sentiment_factor_kind::embargo_imposed,     -4.0f, -2.0f, 0.0f);
        set(sentiment_factor_kind::force_used,          0.0f,  -0.25f, 0.25f);
        set(sentiment_factor_kind::trade_severed,       0.0f,  0.0f,  2.0f);
        recipe_registry reg;
        reg.set_sentiment(p);

        // E1 trade_conducted: two landings of one pair in one tick fold as ONE
        // event a party, magnitude the value landed; a domestic landing is not
        // trade between nations.
        {
            world w;
            const entity_id A = add_nation(w, "Shipper");
            const entity_id B = add_nation(w, "Receiver");
            const entity_id tile_b = w.create_entity();
            const entity_id tile_a = w.create_entity();
            w.tile_to_nation[tile_b] = B;
            w.tile_to_nation[tile_a] = A;
            const entity_id mb = w.create_entity();
            const entity_id ma = w.create_entity();
            market_component mkb{};
            mkb.centre_tile = tile_b;
            market_component mka{};
            mka.centre_tile = tile_a;
            w.markets[mb] = mkb;
            w.markets[ma] = mka;
            note_trade_landed(w, A, mb, 4.0f);
            note_trade_landed(w, A, mb, 12.0f);
            note_trade_landed(w, A, ma, 99.0f); // domestic: ignored
            check(w.sentiment_trade_landed.size() == 1
                      && w.sentiment_trade_landed.at({ A, B }) == 16.0f,
                  "E1a the trade flow accumulates landed value per (shipper, receiver) "
                  "nation pair, foreign landings only");
            const std::size_t folded = fold_pending_sentiment(w, reg);
            const sentiment_value ab = sentiment_toward(w.sentiment, A, B);
            const sentiment_value ba = sentiment_toward(w.sentiment, B, A);
            check(folded == 2 && ab == sentiment_value{ 4.0f, 4.0f, -4.0f }
                      && ba == sentiment_value{ 4.0f, 4.0f, -4.0f },
                  "E1b trade_conducted folds once a tick per party, magnitude the value "
                  "landed: each -> the other, Access +, Trust +, Alarm -");
            check(w.sentiment_trade_landed.empty() && w.sentiment_trade_streaks.count({ A, B }) == 1,
                  "E1c the fold clears the accumulator and starts the pair's trade streak");
        }

        // E5 trade_severed, through its persistence window: only an ESTABLISHED
        // trade (2 consecutive folds landing) that then lands nothing for 2
        // consecutive folds severs, once; a pair whose convoys alternate ticks
        // severs nothing. Measured on severed alone (no decay, no trade weight),
        // so B's Alarm toward A is 2 x the severings.
        {
            sentiment_params sp;
            sp.factors[static_cast<std::size_t>(sentiment_factor_kind::trade_severed)] =
                sentiment_factor{ 0.0f, 0.0f, 2.0f };
            recipe_registry sreg;
            sreg.set_sentiment(sp);
            world w;
            const entity_id A = add_nation(w, "Shipper");
            const entity_id B = add_nation(w, "Receiver");
            const entity_id tile_b = w.create_entity();
            w.tile_to_nation[tile_b] = B;
            const entity_id mb = w.create_entity();
            market_component mkb{};
            mkb.centre_tile = tile_b;
            w.markets[mb] = mkb;
            const auto run = [&](const char* pattern) { // 'L' lands, '.' does not
                for (const char* c = pattern; *c != '\0'; ++c)
                {
                    if (*c == 'L')
                        note_trade_landed(w, A, mb, 1.0f);
                    fold_pending_sentiment(w, sreg);
                }
                return sentiment_toward(w.sentiment, B, A).alarm;
            };
            const float alternating = run("L.L.L.L.L.L.L.L.");
            check(alternating == 0.0f && w.sentiment.pairs.empty(),
                  "E5a convoys landing on alternate ticks for 16 folds raise NO severed event "
                  "(a missed tick is not a severing)");
            const float one_stop = run("LLL....");
            check(one_stop == 2.0f,
                  "E5b an established trade (3 folds landing) then stopped (4 folds idle) "
                  "severs exactly once: the receiver's Alarm toward the shipper +2");
            const float no_rearm = run("L..L..");
            check(no_rearm == 2.0f,
                  "E5c ...and not again until the trade is re-established for 2 folds");
            const float rearmed = run("LL..");
            check(rearmed == 4.0f && sentiment_toward(w.sentiment, A, B).alarm == 0.0f
                      && w.sentiment_trade_streaks.empty(),
                  "E5d re-established then stopped again severs once more; the shipper "
                  "forms nothing, and a severed pair leaves no streak behind");
        }

        // E2 friendship_accepted: both parties observe, each in its own row.
        {
            world w;
            const entity_id A = add_nation(w, "Offerer");
            const entity_id B = add_nation(w, "Acceptor");
            const auto hostile_before = w.corp_hostile_pairs;
            check(offer_friendship(w, A, B) == stance_result::applied
                      && w.sentiment_pending.empty()
                      && accept_friendship(w, A, B) == stance_result::applied
                      && w.sentiment_pending.size() == 2,
                  "E2a accept_friendship queues its conduct (an offer alone queues none)");
            fold_pending_sentiment(w, reg);
            check(sentiment_toward(w.sentiment, A, B) == sentiment_value{ 4.0f, 4.0f, -2.0f }
                      && sentiment_toward(w.sentiment, B, A) == sentiment_value{ 4.0f, 4.0f, -2.0f }
                      && w.sentiment_pending.empty(),
                  "E2b friendship_accepted: each party -> the other, Access +, Trust +, Alarm -");
            check(w.corp_hostile_pairs == hostile_before && w.corp_friend_pairs.size() == 1,
                  "E2c the fold moved no stance table (the friendship row is accept's own)");
        }

        // E3 force_used: the side that lost strength -> the other, magnitude the
        // men it lost; the side that lost nothing forms nothing.
        {
            world w;
            const entity_id A = add_nation(w, "Attacker");
            const entity_id D = add_nation(w, "Defender");
            note_battle_losses(w, A, D, 8, 0);
            note_battle_losses(w, A, D, 4, 0);
            fold_pending_sentiment(w, reg);
            check(sentiment_toward(w.sentiment, A, D) == sentiment_value{ 0.0f, -3.0f, 3.0f }
                      && w.sentiment.pairs.count({ D, A }) == 0,
                  "E3 force_used: the side that lost men -> the other, magnitude the men "
                  "lost (12 = 8 + 4 over two days): Trust -, Alarm +");
        }

        // E4 embargo_imposed (BL-1418): enacting the embargo is its one call site.
        // The embargoed observes the author, Access -, Trust -, no Alarm; a repeal
        // queues nothing; under the settle the enactment lands but writes no row.
        {
            world w;
            const entity_id A = add_nation(w, "Author");
            const entity_id B = add_nation(w, "Embargoed");
            check(set_embargo(w, A, B, true) == embargo_result::applied
                      && w.sentiment_pending.size() == 1,
                  "E4a enacting an embargo queues its conduct (one event)");
            fold_pending_sentiment(w, reg);
            check(sentiment_toward(w.sentiment, B, A) == sentiment_value{ -4.0f, -2.0f, 0.0f }
                      && w.sentiment.pairs.count({ A, B }) == 0,
                  "E4b embargo_imposed: the embargoed -> the author, Access -, Trust -, Alarm "
                  "untouched; the author forms nothing");
            check(set_embargo(w, A, B, false) == embargo_result::applied
                      && w.sentiment_pending.empty(),
                  "E4c a repeal queues nothing");
            world g;
            const entity_id GA = add_nation(g, "Author");
            const entity_id GB = add_nation(g, "Embargoed");
            g.settle_build_gate = true;
            check(set_embargo(g, GA, GB, true) == embargo_result::applied
                      && embargoes(g, GA, GB) && g.sentiment_pending.empty(),
                  "E4d under the settle the embargo is enacted but gathers no conduct");
        }

        // E6 the fold is order-blind: the same gathered conduct queued in two
        // orders folds to bit-identical tables.
        {
            world w1, w2;
            const entity_id A = add_nation(w1, "A");
            const entity_id B = add_nation(w1, "B");
            add_nation(w2, "A");
            add_nation(w2, "B");
            queue_conduct(w1, A, B, sentiment_factor_kind::force_used, 3.0f);
            queue_conduct(w1, A, B, sentiment_factor_kind::friendship_accepted);
            queue_conduct(w1, B, A, sentiment_factor_kind::force_used, 5.0f);
            queue_conduct(w2, B, A, sentiment_factor_kind::force_used, 5.0f);
            queue_conduct(w2, A, B, sentiment_factor_kind::friendship_accepted);
            queue_conduct(w2, A, B, sentiment_factor_kind::force_used, 3.0f);
            fold_pending_sentiment(w1, reg);
            fold_pending_sentiment(w2, reg);
            check(!w1.sentiment.pairs.empty() && tables_identical(w1.sentiment, w2.sentiment),
                  "E6 gather order is not observable: one canonicalised batch a tick");
        }
    }

    // --- A: ALARM, THE THIRD DIMENSION (BL-1432; RELATIONS.md § 6) -----------
    std::printf("\n-- A  Alarm is a third dimension (BL-1432) --\n");
    {
        sentiment_params p;
        p.factors[static_cast<std::size_t>(sentiment_factor_kind::border_force)] = sentiment_factor{ 0.0f, 0.0f, 8.0f };
        p.factors[static_cast<std::size_t>(sentiment_factor_kind::trade_conducted)] = sentiment_factor{ 1.0f, 0.0f, -1.0f };
        p.alarm_decay_per_tick = 0.074125f; // the authored nine-tick half-life
        const entity_id a = 7, b = 8;

        // A1 folded by its weight, independent of the other two.
        sentiment_table t;
        apply_sentiment_events(t, p, { ev(a, b, sentiment_factor_kind::border_force) });
        check(sentiment_toward(t, a, b) == sentiment_value{ 0.0f, 0.0f, 8.0f } && t.pairs.size() == 1,
              "A1 an Alarm-only factor moves Alarm alone and STORES its row (Access and "
              "Trust both zero is not neutral while Alarm is not)");

        // A2 decays at its own authored rate; Access and Trust rates zero leave
        // them untouched.
        sentiment_table u;
        apply_sentiment_events(u, p, { ev(a, b, sentiment_factor_kind::trade_conducted, 2.0f),
                                       ev(a, b, sentiment_factor_kind::border_force) });
        // 2 x (+1 access, -1 alarm) + 8 alarm = access 2, alarm 6.
        for (int i = 0; i < 9; ++i)
            decay_sentiment(u, p);
        const sentiment_value nine = sentiment_toward(u, a, b);
        check(nine.access == 2.0f && nine.alarm > 2.99f && nine.alarm < 3.01f,
              "A2 Alarm decays at alarm_decay_per_tick (6 halves to ~3 in nine ticks) "
              "while an undecaying Access stays put");

        // A3 erased only when all three are neutral.
        sentiment_table v;
        apply_sentiment_events(v, p, { ev(a, b, sentiment_factor_kind::border_force) });
        int erased_at = -1;
        for (int i = 1; i <= 400 && erased_at < 0; ++i)
        {
            decay_sentiment(v, p);
            if (v.pairs.empty())
                erased_at = i;
        }
        sentiment_table keep;
        apply_sentiment_events(keep, p, { ev(a, b, sentiment_factor_kind::trade_conducted, 1.0f),
                                          ev(a, b, sentiment_factor_kind::border_force) });
        for (int i = 0; i < 400; ++i)
            decay_sentiment(keep, p);
        check(erased_at > 0 && keep.pairs.size() == 1 && keep.pairs.begin()->second.alarm == 0.0f
                  && keep.pairs.begin()->second.access == 1.0f,
              "A3 a row whose Alarm reaches neutral is erased only when Access and Trust "
              "are neutral too");

        // A4 the subject's alarm_scale shrinks only RAISED Alarm; relief is whole.
        sentiment_table s1;
        sentiment_event raise = ev(a, b, sentiment_factor_kind::border_force);
        raise.alarm_scale = 0.5f;
        sentiment_event relieve = ev(a, b, sentiment_factor_kind::trade_conducted, 1.0f);
        relieve.alarm_scale = 0.5f;
        apply_sentiment_events(s1, p, { raise });
        apply_sentiment_events(s1, p, { relieve });
        check(sentiment_toward(s1, a, b) == sentiment_value{ 1.0f, 0.0f, 3.0f },
              "A4 an event's alarm_scale scales the Alarm it raises (8 x 0.5) and not the "
              "Alarm it relieves (-1) nor Access");

        // A5 a per-subject decay scale.
        sentiment_table d1, d2;
        apply_sentiment_events(d1, p, { ev(a, b, sentiment_factor_kind::border_force) });
        apply_sentiment_events(d2, p, { ev(a, b, sentiment_factor_kind::border_force) });
        const std::map<entity_id, float> faster{ { b, 2.0f } };
        const std::map<entity_id, float> other{ { a, 2.0f } };
        decay_sentiment(d1, p, &faster);
        decay_sentiment(d2, p, &other);
        const float plain = 8.0f - 8.0f * 0.074125f;
        check(sentiment_toward(d2, a, b).alarm == plain
                  && sentiment_toward(d1, a, b).alarm < plain,
              "A5 a subject's Alarm decay scale speeds the Alarm held TOWARD it, and only that");

        // A6 the substrate's own stream: v2 carries Alarm, v1 still reads (Alarm 0).
        {
            sentiment_table src;
            src.pairs[{ a, b }] = sentiment_value{ 1.5f, -2.5f, 3.25f };
            src.pairs[{ b, a }] = sentiment_value{ 0.0f, 0.0f, -0.5f };
            std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
            write_sentiment(src, ss);
            sentiment_table back;
            check(read_sentiment(back, ss) && tables_identical(src, back),
                  "A6a the sentiment stream (v2) round-trips Alarm bit-exactly, an "
                  "Alarm-only row included");

            std::stringstream v1(std::ios::in | std::ios::out | std::ios::binary);
            const uint32_t magic = sentiment_magic, ver = sentiment_version_pre_alarm, n = 1;
            const uint32_t o = a, sbj = b;
            const float acc = 1.5f, tr = -2.5f;
            v1.write(reinterpret_cast<const char*>(&magic), 4);
            v1.write(reinterpret_cast<const char*>(&ver), 4);
            v1.write(reinterpret_cast<const char*>(&n), 4);
            v1.write(reinterpret_cast<const char*>(&o), 4);
            v1.write(reinterpret_cast<const char*>(&sbj), 4);
            v1.write(reinterpret_cast<const char*>(&acc), 4);
            v1.write(reinterpret_cast<const char*>(&tr), 4);
            sentiment_table old;
            check(read_sentiment(old, v1) && old.pairs.size() == 1
                      && sentiment_toward(old, a, b) == sentiment_value{ 1.5f, -2.5f, 0.0f },
                  "A6b a version-1 sentiment stream loads with Alarm 0");
        }

        // A7 THE INVARIANT, for Alarm: driving Alarm to its limit moves no stance.
        {
            world w;
            const entity_id x = add_nation(w, "X");
            const entity_id y = add_nation(w, "Y");
            const auto h = w.corp_hostile_pairs;
            const auto f = w.corp_friend_pairs;
            const auto o = w.corp_friend_offers;
            for (int i = 0; i < 50; ++i)
                apply_sentiment_events(w.sentiment, p, { ev(x, y, sentiment_factor_kind::border_force, 10.0f) });
            check(sentiment_toward(w.sentiment, x, y).alarm == p.limit
                      && w.corp_hostile_pairs == h && w.corp_friend_pairs == f
                      && w.corp_friend_offers == o,
              "A7 Alarm saturated at its limit flips no stance table (THE INVARIANT)");
        }
    }

    // --- C: ALARM'S CAPABILITY FACTORS AND THE TREE (BL-1432 R2, R3) ---------
    std::printf("\n-- C  Alarm's capability factors and the tree's Alarm effects --\n");
    {
        sentiment_params p;
        p.factors[static_cast<std::size_t>(sentiment_factor_kind::technology_visible)] = sentiment_factor{ 0.0f, 0.0f, 0.25f };
        p.factors[static_cast<std::size_t>(sentiment_factor_kind::facility_visible)]   = sentiment_factor{ 0.0f, 0.0f, 1.0f };
        p.factors[static_cast<std::size_t>(sentiment_factor_kind::border_force)]       = sentiment_factor{ 0.0f, 0.0f, 0.001f };
        p.factors[static_cast<std::size_t>(sentiment_factor_kind::force_used)]         = sentiment_factor{ 0.0f, 0.0f, 1.0f };
        p.alarm_large_facility_cost = 450.0f;
        recipe_registry reg;
        reg.set_sentiment(p);
        building_economics big = reg.economics(building_type::launchpad);
        big.build_cost = 500.0f;
        reg.set_economics(building_type::launchpad, big);
        building_economics small = reg.economics(building_type::port);
        small.build_cost = 150.0f;
        reg.set_economics(building_type::port, small);

        const auto node_of = [](std::string_view id) {
            for (int i = 0; i < io::dg::node_count; ++i)
                if (id == io::dg::nodes[static_cast<std::size_t>(i)].id)
                    return i;
            return -1;
        };

        world w;
        const entity_id S = add_nation(w, "Subject");
        const entity_id O = add_nation(w, "Observer");
        const entity_id T = add_nation(w, "Third");

        // C1 visible technology: S holds Strategic Deterrent (every rival's Alarm
        // toward you +10, `alarm_tell`) — every other nation sees the tell.
        const int deterrent = node_of("DG-ST-AC-Ba");
        w.nation_tech[S].held.set(static_cast<std::size_t>(deterrent));
        emit_alarm_capability(w, reg);
        bool tells = w.sentiment_pending.size() == 2;
        for (const sentiment_event& e : w.sentiment_pending)
            tells = tells && e.subject == S && e.kind == sentiment_factor_kind::technology_visible
                 && e.magnitude == 10.0f && (e.observer == O || e.observer == T);
        check(deterrent >= 0 && tells,
              "C1 a held technology with a public tell (DG-ST-AC-Ba, +10) emits "
              "technology_visible from every other nation, magnitude the tell");
        w.sentiment_pending.clear();
        w.nation_tech[S].held.reset();

        // C2 a large single-site facility: a built launchpad (500 >= 450) counts,
        // a port (150) does not, one under construction does not.
        const auto add_building = [&w](entity_id owner, building_type type, int ticks) {
            const entity_id b = w.create_entity();
            building_component bc{};
            bc.type            = type;
            bc.ticks_remaining = ticks;
            w.buildings[b]     = bc;
            w.nations[owner].state_assets.push_back(b);
        };
        add_building(S, building_type::launchpad, 0);
        add_building(S, building_type::launchpad, 3);
        add_building(S, building_type::port, 0);
        emit_alarm_capability(w, reg);
        bool fac = w.sentiment_pending.size() == 2;
        for (const sentiment_event& e : w.sentiment_pending)
            fac = fac && e.subject == S && e.kind == sentiment_factor_kind::facility_visible
               && e.magnitude == 1.0f;
        check(fac, "C2 a built facility at or above alarm_large_facility_cost emits "
                   "facility_visible (one launchpad; the port and the site under "
                   "construction do not count)");
        w.sentiment_pending.clear();

        // C3 standing force on the observer's border: S's men on the front S
        // shares with O. A unit off the front, and O's own men, do not count.
        {
            const entity_id border = w.create_entity();
            const entity_id inland = w.create_entity();
            front f;
            f.nation = S; f.neighbour = O; f.key = border; f.span = { border };
            w.fronts_cache.built = true;
            w.fronts_cache.territory_generation = w.territory_generation;
            w.fronts_cache.by_nation[S] = { f };
            const auto add_unit = [&w](entity_id owner, entity_id tile, int men) {
                unit_component u{};
                u.owner = owner; u.position = tile; u.count = men;
                w.units[w.create_entity()] = u;
            };
            add_unit(S, border, 600);
            add_unit(S, border, 400);
            add_unit(S, inland, 5000);
            add_unit(O, border, 9000);
        }
        // Drop the facilities for this row so only border force emits.
        w.nations[S].state_assets.clear();
        emit_alarm_capability(w, reg);
        check(w.sentiment_pending.size() == 1 && w.sentiment_pending[0].observer == O
                  && w.sentiment_pending[0].subject == S
                  && w.sentiment_pending[0].kind == sentiment_factor_kind::border_force
                  && w.sentiment_pending[0].magnitude == 1.0f,
              "C3a border force: the observer is alarmed by the men the subject stands on "
              "their shared front (1000, magnitude in thousands), not by men inland or its own");
        w.sentiment_pending.clear();

        // C3b an army that has CROSSED: 2000 of S's men three tiles inside O's
        // ground (a tile O holds, on no front span) counts with the 1000 on the
        // front - force inside the observer's territory alarms it too.
        {
            const entity_id deep = w.create_entity();
            w.tile_to_nation[deep] = O;
            unit_component u{};
            u.owner = S; u.position = deep; u.count = 2000;
            w.units[w.create_entity()] = u;
        }
        emit_alarm_capability(w, reg);
        check(w.sentiment_pending.size() == 1 && w.sentiment_pending[0].observer == O
                  && w.sentiment_pending[0].subject == S
                  && w.sentiment_pending[0].magnitude == 3.0f,
              "C3b ...and by the men it stands INSIDE the observer's ground (2000 three "
              "tiles in + 1000 on the front = 3 thousand)");
        w.sentiment_pending.clear();

        // C4 unrest has no store: its factor emits nothing and is authored zero.
        check(sentiment_factor_names[static_cast<std::size_t>(sentiment_factor_kind::unrest_visible)]
                  == std::string("unrest_visible"),
              "C4 unrest_visible is a named factor with no emitter (unrest has no store)");

        // C5 THE TREE: "-1% Alarm toward you" (DG-ST-AC-T1) scales the Alarm others
        // gain toward the holder; "decays 10% faster" (DG-ST-AC-Q) scales its decay.
        const int corps1 = node_of("DG-ST-AC-T1");
        const int quiet  = node_of("DG-ST-AC-Q");
        w.nation_tech[S].held.set(static_cast<std::size_t>(corps1));
        w.nation_tech[S].held.set(static_cast<std::size_t>(quiet));
        w.units.clear();
        note_battle_losses(w, O, S, 100, 0); // O lost 100 men to S: Alarm toward S
        note_battle_losses(w, O, T, 100, 0); // ...and 100 to T, who holds nothing
        fold_pending_sentiment(w, reg);
        const float toward_s = sentiment_toward(w.sentiment, O, S).alarm;
        const float toward_t = sentiment_toward(w.sentiment, O, T).alarm;
        check(corps1 >= 0 && toward_t == 100.0f && toward_s == 100.0f * 0.99f,
              "C5a a held \"-1% Alarm toward you\" node shrinks the Alarm the holder "
              "raises by 1% (99 vs 100) - the tree writes to the Alarm dimension");
        const std::map<entity_id, float> scales = alarm_decay_scales(w);
        check(quiet >= 0 && scales.size() == 1 && scales.count(S) == 1
                  && scales.at(S) > 1.0999f && scales.at(S) < 1.1001f,
              "C5b a held \"Alarm toward you decays 10% faster\" node resolves to a 1.10 "
              "decay scale for its holder alone");
        bool every_alarm_effect_read = true;
        int wired = 0;
        for (const io::dg::effect& e : io::dg::effects)
        {
            const std::string_view sub = e.subject;
            if (e.kind == io::dg::effect_kind::modifier
                && (sub == "alarm_toward" || sub == "alarm_tell" || sub == "alarm_decay"))
            {
                ++wired;
                every_alarm_effect_read = every_alarm_effect_read
                    && classify_effect(e) == effect_reading::read;
            }
        }
        std::printf("     %d Digitisation effect rows write the Alarm dimension\n", wired);
        check(wired >= 13 && every_alarm_effect_read,
              "C5c every Alarm modifier row in the store classifies as READ (nation_tech's "
              "declaration)");
    }

    // --- G: THE SETTLE WRITES NO CONDUCT (BL-1458 review) ----------------------
    // The settle runs the economy step for its ticks with `settle_build_gate`
    // up. Nothing it does is conduct a nation chose, so every emitter gathers
    // nothing and the fold folds nothing: the opening world carries generation's
    // grudges and no other sentiment.
    std::printf("\n-- G  the settle writes no conduct --\n");
    {
        sentiment_params p;
        for (std::size_t i = 0; i < sentiment_factor_count; ++i)
            p.factors[i] = sentiment_factor{ 1.0f, 1.0f, 1.0f };
        recipe_registry reg;
        reg.set_sentiment(p);
        world w;
        const entity_id A = add_nation(w, "A");
        const entity_id B = add_nation(w, "B");
        const entity_id tile_b = w.create_entity();
        w.tile_to_nation[tile_b] = B;
        const entity_id mb = w.create_entity();
        market_component mkb{};
        mkb.centre_tile = tile_b;
        w.markets[mb] = mkb;
        // A grudge generation seeded: the one thing the opening world holds.
        apply_sentiment_events(w.sentiment, p, { ev(A, B, sentiment_factor_kind::historical_grudge) });
        const sentiment_table seeded = w.sentiment;
        unit_component u{};
        u.owner = A; u.position = tile_b; u.count = 5000; // A's army inside B
        w.units[w.create_entity()] = u;
        for (int i = 0; i < io::dg::node_count; ++i)
            if (std::string_view(io::dg::nodes[static_cast<std::size_t>(i)].id) == "DG-ST-AC-Ba")
                w.nation_tech[A].held.set(static_cast<std::size_t>(i)); // a public tell

        w.settle_build_gate = true; // as run_settle_tick raises it on steps 0..11
        for (int step = 0; step < 12; ++step)
        {
            note_trade_landed(w, A, mb, 50.0f);
            note_battle_losses(w, A, B, 10, 10);
            queue_conduct(w, A, B, sentiment_factor_kind::friendship_accepted);
            emit_alarm_capability(w, reg);
            fold_pending_sentiment(w, reg);
        }
        check(tables_identical(w.sentiment, seeded) && w.sentiment_pending.empty()
                  && w.sentiment_trade_landed.empty() && w.sentiment_trade_streaks.empty(),
              "G1 a settled world holds no conduct rows: twelve settle ticks of trade, "
              "battle, friendship, a tell and an army across the border leave only the "
              "seeded grudge, and nothing pending");
        w.settle_build_gate = false; // the handoff
        emit_alarm_capability(w, reg);
        fold_pending_sentiment(w, reg);
        check(sentiment_toward(w.sentiment, B, A).alarm > 0.0f,
              "G2 ...and from the handoff the same world emits (B is alarmed by A)");
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
