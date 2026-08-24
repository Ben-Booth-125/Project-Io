#pragma once

#include "entity.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Sentiment (BL-545) — the relational SUBSTRATE
// ---------------------------------------------------------------------------
// ONE directed, continuous, DERIVED value from an OBSERVER to a SUBJECT, where
// an actor is a corporation or a nation. Settled 2026-08-22 (Ben, on NR-520)
// after three designs were found to be converging independently on the same
// quantity: CONCEPT.md's never-built sentiment, BL-540's nation->corp
// Access/Trust, and `world::corp_reputation` (BL-350). Rather than a fourth
// relational read, this is the substrate the other three become reads OF.
//
// The layering, which is the whole design in one line:
//
//   SENTIMENT is DERIVED and CONTINUOUS.  STANCE is DECLARED and DISCRETE.
//
// ---------------------------------------------------------------------------
// THE INVARIANT — the thing this file exists to not break
// ---------------------------------------------------------------------------
// Ben ruled on 2026-08-17 (NR-302, and again in the BL-450 grant) that hostility
// is A DECLARED STATE A CORP OPTS INTO, and that a rival "may score and declare
// it, never acquire it ambiently". That ruling STANDS. This substrate must not
// quietly overturn it by the back door of a threshold:
//
//   SENTIMENT INFORMS A DECLARATION; IT NEVER MAKES ONE.
//   NO THRESHOLD ANYWHERE MAY FLIP A STANCE TABLE BY ITSELF.
//
// That is enforced STRUCTURALLY here, not by discipline: this translation unit
// is never handed a `world&`. It operates on a `sentiment_table` and nothing
// else, so it has no reach to `corp_hostile_pairs`, `corp_friend_pairs` or
// `corp_friend_offers` — it cannot see them, so it cannot move them. A comment
// promising restraint would have been the weaker version of the same claim; the
// signature is the claim. (`sentiment_harness` R1 asserts it anyway, because a
// structural argument that nobody checks is one refactor from being false.)
//
// The one direction that IS legal is the reverse: an observed DECLARATION is a
// perfectly good input to sentiment (`sentiment_factor_kind::hostility_declared`
// below). Declarations move sentiment; sentiment never moves declarations.
//
// ---------------------------------------------------------------------------
// Four properties, all load-bearing
// ---------------------------------------------------------------------------
//   1. DIRECTED. A->B is not B->A, and the key is ordered (observer, subject).
//      Every existing use needs this: reputation is keyed (buyer, supplier), a
//      nation reads a corp and never the reverse, and a grudge is asymmetric by
//      nature. Symmetry, where a consumer wants it, is a read that asks twice.
//
//   2. TWO DIMENSIONS — ACCESS and TRUST — AT EVERY GRAIN (Ben, NR-522: not
//      one, not per-grain). *A substrate whose shape varies by grain is not a
//      substrate.* They move independently and decay independently: a
//      counterparty can be perfectly trusted and still barred, or welcomed
//      everywhere and believed by nobody.
//
//   3. ONE TABLE, EVERY GRAIN. corp->corp, nation->corp, nation->nation and
//      corp->nation are the SAME table, because entity ids are globally unique
//      and the substrate has no reason to care which kind of actor it is looking
//      at. This is property 2's structural half: the shape cannot vary by grain
//      when there is only one shape. Validating that an id names a real actor
//      belongs to the EMITTER, which knows what it observed; the substrate
//      stores what it is told (minus the two rejections below).
//
//   4. CONTINUOUS. A float per dimension. Bands are a PRESENTATION choice
//      (`band_of` below), never the storage — nothing in `world/*` may branch on
//      a band, and certainly not to declare anything (see THE INVARIANT).
//
// ---------------------------------------------------------------------------
// Decay has NO FLOOR
// ---------------------------------------------------------------------------
// A value driven to any extreme returns toward neutral within the authored
// window, and THERE IS NO VALUE IT CANNOT LEAVE. This is what dissolves BL-391's
// reputation deadlock (a permanent floor a counterparty could never climb back
// out of) BY CONSTRUCTION rather than by a special case: a quantity that decays
// has no permanent floor to be stuck at, so the deadlock has nowhere to live.
//
// `neutral_epsilon` is the opposite of a floor and must not be misread as one:
// multiplicative decay approaches zero asymptotically and would leave a
// vanishing residue forever, so a value inside the epsilon snaps to EXACTLY
// neutral and its row is erased. The epsilon makes neutral REACHABLE.
//
// ---------------------------------------------------------------------------
// Factors are AUTHORED DATA, not code branches
// ---------------------------------------------------------------------------
// Ben, 2026-08-22: "Certain actions might make a rival wary, such as buying a
// background firm that they wanted. There are tonnes of realistic ways to affect
// sentiment." So the application is a data-driven FOLD over an authored table —
// `sentiment_params::factors`, indexed BY the kind, with no per-factor branch
// anywhere in this file. Re-weighting a factor, or admitting one into play at
// all, is a row in scripts/economy.lua and nothing else.
//
// Naming a genuinely new KIND of observable conduct still costs an enumerator,
// exactly as `condition_subject` and `modifier_subject` do, and for the same
// reason: a closed vocabulary is what keeps the effect data, deterministic,
// serialisable and legible in a ledger line. That is the house rule from
// META_LAYER.md — a new rule family costs an enumerator, not a subsystem — and
// `sentiment_factor_kind` is APPEND-ONLY once serialised (BL-455's rule): append
// a value, never insert, renumber or remove one.
//
// ---------------------------------------------------------------------------
// Determinism (BL-158's rule, taken verbatim)
// ---------------------------------------------------------------------------
// At 43 nations plus ~88 corporations an N x N table is thousands of ordered
// pairs — trivial to store, which is exactly what makes the walk rule
// load-bearing rather than pedantic:
//
//   - The table is a `std::map`, so every walk over it is a SORTED WALK over
//     (observer, subject). Never an unordered container, never a hash order.
//   - The EVENT FOLD IS ORDER-INDEPENDENT BY CONSTRUCTION: a batch is
//     canonicalised (sorted by observer, subject, kind, magnitude) before it is
//     folded, so an emitter that gathered its events from an unordered container
//     cannot leak that container's order into the result. Float addition does
//     not commute; rather than make every future emitter responsible for that,
//     the substrate takes the responsibility once.
//   - A NEUTRAL ROW IS NEVER STORED. Absent == neutral, so a zero-weighted
//     factor cannot leave a trace, and the inert case is empty rather than full
//     of zeroes.
// ---------------------------------------------------------------------------

/// One directed sentiment: the two dimensions, at every grain (property 2).
/// Default-constructs NEUTRAL, which is also what an absent row reads as.
struct sentiment_value
{
    /// Will the observer LET the subject operate — market access, territory,
    /// a licence, a berth. BL-540's nation->corp gate reads this one.
    float access = 0.0f;

    /// Will the observer BELIEVE the subject — a promise kept, a contract
    /// delivered, a price honoured. `corp_reputation` (BL-350) migrates onto
    /// this one (BL-546).
    float trust = 0.0f;

    /// True iff both dimensions are exactly neutral. The predicate the "never
    /// store a neutral row" rule is written in terms of.
    bool neutral() const { return access == 0.0f && trust == 0.0f; }
};

/// Bit-exact equality, deliberately not an epsilon compare: this is the
/// predicate the determinism and inertness checks are stated in.
inline bool operator==(const sentiment_value& a, const sentiment_value& b)
{
    return a.access == b.access && a.trust == b.trust;
}
inline bool operator!=(const sentiment_value& a, const sentiment_value& b) { return !(a == b); }

/// The closed vocabulary of OBSERVABLE CONDUCT a sentiment can be derived from.
/// Each names a thing that happens in the world and that an observer could
/// plausibly have seen; the WEIGHT it carries is authored (see
/// `sentiment_params::factors`), so a row here is a name, not a number.
///
/// APPEND-ONLY once serialised (`sentiment_version`): append a value, never
/// insert, renumber or remove one — the `condition_subject` rule (BL-455).
///
/// NOTHING EMITS THESE YET. They are vocabulary, on the exact precedent
/// `modifier_subject` set (a military subject shipped before anything read it):
/// a shape is only proven by an instance, and the instances are BL-546
/// (reputation migration), BL-540 (nation->corp access) and BL-541.
enum class sentiment_factor_kind : uint8_t
{
    /// A procurement contract delivered as promised (BL-350). The migration
    /// target for `corp_reputation`'s positive half.
    contract_completed = 0,

    /// A procurement contract cancelled by the counterparty (BL-350). The
    /// migration target for `corp_reputation`'s negative half — and the
    /// deadlock BL-391 could not escape, which decay now dissolves.
    contract_cancelled = 1,

    /// Goods actually moved between the pair — a lane run, an order filled.
    /// Commerce as its own slow warming, independent of any contract.
    trade_conducted = 2,

    /// The observer wanted a firm and the subject took it (BL-524's syndicate
    /// tier). Ben's own worked example of a factor: "buying a background firm
    /// that they wanted".
    equity_taken = 3,

    /// The subject DECLARED hostility toward the observer (BL-448). Note the
    /// direction: a declaration is an INPUT to sentiment. Sentiment is never an
    /// input to a declaration — see THE INVARIANT at the top of this file.
    hostility_declared = 4,

    /// A friendship offer from the subject was accepted (BL-448). The symmetric
    /// declaration's asymmetric echo: both parties observe it, each in their own
    /// row.
    friendship_accepted = 5,

    /// The observer was charged under a law the subject authored (BL-480's
    /// extraction levy, BL-343's tariff). The corp->nation grain's bread and
    /// butter.
    levy_charged = 6,

    /// The subject refused to deal with the observer at all — the embargo
    /// predicate (`world::corp_embargo_conditions`, BL-350 Q2) firing.
    embargo_imposed = 7,

    /// The subject spent to move a nation against the observer (BL-539). The
    /// political grain, granted 2026-08-22 on BL-450's terms.
    lobbied_against = 8,

    /// Force was used against the observer's interest (BL-467's campaign
    /// battles). The sharpest input, and still only an input.
    force_used = 9,

    /// A mercenary contract's predicate came up false at its deadline (a
    /// "take"), or on the tick it first read false (a "hold") — CONTRACTS.md
    /// § Q2 (BL-573). The observer is the CLIENT nation, the subject the
    /// CONTRACTOR corp — the same direction `contract_completed`/
    /// `contract_cancelled` already read. Authored MORE NEGATIVE than
    /// `contract_cancelled` (scripts/economy.lua): "you fought, you were
    /// beaten" costs more standing than an honest early withdrawal, and
    /// CONTRACTS.md names failure as the hardest reputation move a mercenary
    /// can make.
    contract_failed = 10,
};

/// Number of rows in `sentiment_factor_kind`. Grows when a value is appended;
/// it is the array bound the fold indexes with, so the two cannot drift apart.
inline constexpr std::size_t sentiment_factor_count = 11;

/// The AUTHORING NAME of each kind, INDEXED BY the kind — the key
/// `scripts/economy.lua`'s `sentiment.factors` table uses for that row.
///
/// An array rather than a switch, so the Lua loader is a LOOP over the roster
/// and not ten branches. That is the rule the fold obeys on the C++ side,
/// applied to the authoring side: naming a factor costs an enumerator and a
/// string, never an `if`. Append here in lockstep with the enum — the harness
/// checks that no name is empty and that none repeats.
inline constexpr const char* sentiment_factor_names[sentiment_factor_count] = {
    "contract_completed",
    "contract_cancelled",
    "trade_conducted",
    "equity_taken",
    "hostility_declared",
    "friendship_accepted",
    "levy_charged",
    "embargo_imposed",
    "lobbied_against",
    "force_used",
    "contract_failed",
};

/// The AUTHORED weight of one kind of conduct: how far a single occurrence moves
/// each dimension. Both default to zero, which is what makes an unauthored
/// factor inert rather than guessed — a factor that has not been given a number
/// does nothing at all.
struct sentiment_factor
{
    float access = 0.0f; ///< Delta applied to `sentiment_value::access` per occurrence.
    float trust  = 0.0f; ///< Delta applied to `sentiment_value::trust` per occurrence.
};

/// Every tunable the substrate has, authored in scripts/economy.lua under the
/// top-level `sentiment` table (the `procurement_params` precedent). Every field
/// defaults so that the WHOLE SUBSTRATE IS INERT: zero weights, zero decay. A
/// build that never authors this table is bit-identical to one without it.
struct sentiment_params
{
    /// The factor table, INDEXED BY `sentiment_factor_kind`. This array is the
    /// data-driven fold: `factors[size_t(kind)]` and no branch. A new factor is
    /// a row here, not an `if` in sentiment.cpp.
    std::array<sentiment_factor, sentiment_factor_count> factors = {};

    /// Fraction of the REMAINING DISTANCE TO NEUTRAL each dimension sheds per
    /// tick, always toward zero from whichever side it is on. Independent per
    /// dimension (property 2): access may be slow to forgive while trust is
    /// quick, or the reverse.
    ///
    /// Authored as a rate rather than a half-life on purpose. A half-life is the
    /// legible way to THINK about it — rate = 1 - 2^(-1 / half_life_ticks), so a
    /// 180-tick half-life is ~0.00384 (a tick is a quarter) — but converting it
    /// would put a `std::pow` in the engine, and pow is not guaranteed
    /// identically rounded across libms. The value that crosses the save seam
    /// and the tick is the RATE. The authored value (scripts/economy.lua,
    /// NR-568) is a NINE-tick half-life: 1 - 2^(-1/9) = 0.074125.
    ///
    /// Zero = this dimension does not decay AND IS NOT TOUCHED AT ALL (not even
    /// by the epsilon snap). Values above 1 are clamped to 1 (full decay in one
    /// tick) so a nonsense authored rate can never flip a sentiment's sign —
    /// an in-process backstop only: the Lua loader (recipe_registry.cpp,
    /// `read_unit_rate`) REJECTS an authored rate outside [0, 1] or non-finite
    /// at load, naming the key, so an authored value never reaches the clamp.
    float access_decay_per_tick = 0.0f;
    float trust_decay_per_tick  = 0.0f;

    /// Magnitude below which a DECAYING dimension snaps to exactly neutral.
    /// THIS IS NOT A FLOOR — it is the opposite of one (see the header comment):
    /// multiplicative decay only approaches zero, so without it a driven value
    /// would leave a vanishing residue forever and its row would never be
    /// erased. Applied only to a dimension that is actually decaying.
    float neutral_epsilon = 1e-4f;

    /// Symmetric SATURATION bound on each dimension, applied when an event
    /// accumulates. Not a floor either, and not in tension with "no value it
    /// cannot leave": decay always runs toward zero, so a saturated value
    /// returns exactly like any other. It exists so a long run of one factor
    /// cannot drive the number somewhere a band read stops being legible.
    /// Zero or negative disables saturation entirely.
    float limit = 100.0f;

    /// Band edges as a FRACTION of `limit`, for `band_of` — presentation only.
    float band_edge_near = 0.10f; ///< |v| below this reads neutral.
    float band_edge_far  = 0.50f; ///< |v| at or above this reads adverse/trusted.

    /// True iff nothing here can move anything: every factor weight zero and
    /// neither dimension decaying. The loud statement of the inertness rule —
    /// when this holds, `run_sentiment_step` does literally nothing.
    bool inert() const
    {
        if (access_decay_per_tick > 0.0f || trust_decay_per_tick > 0.0f)
            return false;
        return factors_inert();
    }

    /// True iff every authored factor weight is exactly zero, so no event can
    /// move anything. (Decay may still be authored; `inert` is the conjunction.)
    bool factors_inert() const
    {
        for (const sentiment_factor& f : factors)
            if (f.access != 0.0f || f.trust != 0.0f)
                return false;
        return true;
    }
};

/// One occurrence of observable conduct, ready to be folded. `magnitude` scales
/// the authored row — 1.0 is "one occurrence", and a caller with a natural size
/// (credits levied, units lost) passes it here rather than authoring a second
/// factor kind for "a big one".
///
/// A batch of these is CANONICALISED BEFORE IT IS FOLDED, so the order an
/// emitter happened to gather them in is not observable in the result.
struct sentiment_event
{
    entity_id             observer  = null_entity; ///< Who formed the opinion.
    entity_id             subject   = null_entity; ///< Who it is about.
    sentiment_factor_kind kind      = sentiment_factor_kind::contract_completed;
    float                 magnitude = 1.0f;
};

/// THE TABLE. Sparse and directed: keyed (observer, subject), and an ABSENT ROW
/// IS NEUTRAL. A `std::map` so every walk is a sorted walk over ids (BL-158),
/// which is the same reason `corp_body_pools` and `corp_reputation` are maps.
///
/// A struct rather than a bare typedef so it can carry a serialiser and any
/// later derived index without changing every signature that names it.
///
/// Held OFF `world` by this header deliberately, on `stance.hpp`'s precedent:
/// the world declares the field, this file owns the meaning. One line joins
/// them — see the patch in BL-545's report.
struct sentiment_table
{
    std::map<std::pair<entity_id, entity_id>, sentiment_value> pairs;
};

/// What @p observer feels about @p subject. Neutral for an absent row, a self
/// pair, or a null id — the single read point, so no consumer invents its own
/// default. Pure; never inserts.
///
/// @param t        The table.
/// @param observer Who is doing the feeling.
/// @param subject  Who it is about.
/// @return         The stored value, or neutral.
sentiment_value sentiment_toward(const sentiment_table& t, entity_id observer, entity_id subject);

/// Decay every stored row one tick toward neutral, per dimension, and ERASE any
/// row that has reached exact neutral. A sorted walk over (observer, subject).
///
/// Returns immediately, touching nothing, when neither dimension decays — so a
/// world with no authored decay pays nothing and drifts nowhere.
///
/// @param t The table, mutated in place.
/// @param p Authored decay rates and epsilon.
void decay_sentiment(sentiment_table& t, const sentiment_params& p);

/// Fold a batch of events into the table.
///
/// Order-independent by construction: the batch is copied and canonicalised
/// (observer, subject, kind, magnitude) before folding, so two callers that
/// gathered the same events in different orders produce bit-identical tables.
///
/// Rejections mutate nothing and are silent by design — a null id, a self pair
/// (an actor has no sentiment about itself), an out-of-range kind, or a
/// non-finite magnitude. The last is the untrusted-input rule
/// (io-standing-rules § Determinism & data model) applied here rather than at
/// each future emitter: a NaN that reached the table would poison every
/// subsequent compare, and a NaN in the sort comparator would break its strict
/// weak ordering outright.
///
/// A resolved delta of exactly (0, 0) CREATES NO ROW, and an event that returns
/// a row exactly to neutral ERASES it: absent == neutral, always.
///
/// @param t      The table, mutated in place.
/// @param p      Authored factor weights and saturation bound.
/// @param events The batch, in any order.
void apply_sentiment_events(sentiment_table& t, const sentiment_params& p,
                            const std::vector<sentiment_event>& events);

/// One tick of the substrate: DECAY FIRST, then fold this tick's events.
///
/// The order is the contract, not an accident. Decaying first means an event
/// that lands this tick is not shaved by the same tick's decay — a thing that
/// just happened is worth exactly what it was authored to be worth. Exposing
/// the two halves as well is for the harness; production callers should use
/// this, so the ordering cannot be got wrong in two places.
///
/// @param t      The table, mutated in place.
/// @param p      Authored parameters. Fully inert `p` => this call does nothing.
/// @param events This tick's observed conduct, in any order.
void run_sentiment_step(sentiment_table& t, const sentiment_params& p,
                        const std::vector<sentiment_event>& events);

/// Coarse label for one dimension. PRESENTATION ONLY — it returns a WORD, never
/// a decision. Nothing in `world/*` may branch on this, and nothing anywhere may
/// turn it into a declaration (THE INVARIANT). It lives here so surfaces share
/// one set of edges instead of each inventing its own.
///
/// The words deliberately avoid `hostile` and `friendly`: those name STANCE, and
/// a band is not a stance.
enum class sentiment_band : uint8_t
{
    adverse = 0, ///< Strongly negative.
    wary,        ///< Mildly negative.
    neutral,     ///< Within the near edge of zero, or no row at all.
    favourable,  ///< Mildly positive.
    trusted,     ///< Strongly positive.
};

/// Band @p value falls in, using @p p's edges scaled by `limit`. Pure.
sentiment_band band_of(float value, const sentiment_params& p);

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------
// The substrate's leg of the flat-binary save seam, following order_book /
// history_log / procurement exactly: leading magic + version, count-prefixed
// records, REJECTION rather than reinterpretation on anything that does not
// parse, and a sanity ceiling on the declared count before any eager reserve.
//
// Standalone (it takes a `sentiment_table`, not a `world`) for procurement.hpp's
// stated reason and for THE INVARIANT's: nothing in this file may be handed the
// world. The table is a `std::map`, so records are written in sorted
// (observer, subject) order — that is simply the container's own order, not a
// re-sort this function performs.
//
// This closes the hole RELATIONS.md § What is absent named for the relational
// layer ("no serialiser exists"): the new table joins the seam in the same
// change that creates it, per src/world/CLAUDE.md.

/// Leading magic identifying a sentiment stream: the bytes 'I','O','S','N'.
inline constexpr uint32_t sentiment_magic =
    (uint32_t('I')) | (uint32_t('O') << 8) | (uint32_t('S') << 16) | (uint32_t('N') << 24);

/// Format version. Bump on any layout change to the record below — including an
/// appended `sentiment_factor_kind`, IF a kind is ever stored (it is not today:
/// the table stores the derived value, not the history that produced it).
inline constexpr uint32_t sentiment_version = 1;

/// Sanity ceiling on the declared row count, same reasoning as
/// `procurement_max_records`.
inline constexpr uint32_t sentiment_max_records = 1u << 20;

/// Write @p t as magic + version, then the row count, then one record per row
/// (observer, subject, access, trust) in sorted key order. Binary, native byte
/// order — the project's settled flat-binary convention.
void write_sentiment(const sentiment_table& t, std::ostream& out);

/// Read a stream written by `write_sentiment`, REPLACING @p t's contents.
/// On ANY failure — bad magic, wrong version, an absurd count, a truncated
/// record, a self pair or a null id in the stream — @p t IS LEFT UNTOUCHED and
/// the call returns false. Parse into a local, commit only on success: a
/// half-loaded relational table is worse than none.
bool read_sentiment(sentiment_table& t, std::istream& in);
