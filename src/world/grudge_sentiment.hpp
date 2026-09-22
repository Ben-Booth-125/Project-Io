#pragma once

// ---------------------------------------------------------------------------
// grudge_sentiment — the Era -1 grudge record's ONE consumer (BL-898)
// ---------------------------------------------------------------------------
//
// WHAT WAS WRONG. `pass_one_output::grudges` was CARRIED across the pass 1 ->
// pass 2 handoff and read by nobody: `grudge_between` had exactly one caller in
// the tree (the handoff harness) and `hard_coded_world.cpp` never touched the
// table at all. A field that is retained but unread is indistinguishable from
// one that was never retained, except that it looks finished.
//
// THE FIX THAT WAS FORBIDDEN, named here so nobody re-derives it. BL-898's
// first text said a grudge "must measurably change who a polity campaigns
// against". That is precisely what BL-827 ruled out, and the ruling is written
// at the field itself (`history_sim_params`, § Grudges): "Nothing in this sim
// reads a grudge to make a decision; it is a record, and the moment it became
// an input to the scorer it would be an agent term rather than an in-world
// force." The Era -1 scorer therefore reads NOTHING new, and this file is on
// the far side of the sim from it — it runs once the sim has stopped.
//
// THE ADMISSIBLE FIX, and it was already specified. `docs/politics/
// RELATIONS.md` § What each quantity was before names the consumer outright:
//
//     Era -1 grudges, with nowhere to live  ->  SEEDED nation->nation sentiment
//
// So a grudge stays a RECORD inside the sim and becomes an INPUT where
// sentiment is already a first-class quantity with its own authority doc. The
// colonial era `docs/generation/CIVILISATION.md` § What the dark age must leave
// asks for then opens with actors who already dislike the right neighbours —
// "who colonises whom is not a fresh roll, it is the last quarrel continued by
// other means" — for reasons that happened at a named place on a named date.
//
// WHY A FILE OF ITS OWN rather than a function inside `sentiment.cpp` or
// `history_sim.cpp`. Both were refused, each for a rule the other does not have:
//
//   - `sentiment.cpp` is the SUBSTRATE and is deliberately ignorant of every
//     grain it serves (see THE INVARIANT in sentiment.hpp — it is never handed
//     a `world&`, and it has no reason to know what a polity is). Making it
//     include `history_sim.hpp` would invert the layering: the substrate would
//     depend on one generation pass.
//   - `history_sim.cpp` is the file BL-898's "done when" says must gain NO new
//     read of a grudge. A seeding function living there would satisfy the
//     letter and lose the point.
//
// So this is a JOIN: it depends on both and neither depends on it, and it is
// the only translation unit in the tree that knows a grudge becomes a
// sentiment. It never includes `world.hpp` — its caller resolves nation ids and
// hands them in, which is also what makes it testable on a synthetic table.
//
// DETERMINISM. The grudge table is sorted ascending by (from, to) and searched
// by binary search precisely so its order is a property of the integers in it
// (`history_sim_state::grudges`). This file preserves that: it walks the vector
// in its given order, builds one `sentiment_event` per admitted row, and hands
// the batch to `apply_sentiment_events`, which canonicalises before it folds.
// No RNG, no map iteration, no float accumulation outside the substrate's own.
// ---------------------------------------------------------------------------

#include "entity.hpp"
#include "history_sim.hpp"
#include "sentiment.hpp"

#include <string>
#include <vector>

/// Every tunable the seeding has, in one place. Defaults are the SHIPPED
/// values: unlike `sentiment_params::factors` — whose rows default to zero
/// because each is a seat waiting for its emitter (`sentiment.hpp` § Factors
/// are AUTHORED DATA) — this conversion has exactly one emitter, it is this
/// file, and it runs at generation where no Lua state exists to author from.
/// A zero default here would ship the defect BL-898 was filed about: a record
/// carried across the handoff that still moves nothing.
///
/// NO EXISTING FIXTURE CHANGES MEANING. Nothing read a grudge before this item,
/// so every value below is new surface: a world generated with an empty grudge
/// table, or with `seed_grudge_sentiment` never called, is bit-identical to the
/// build before it.
struct grudge_sentiment_params
{
    /// The grudge score that counts as a FULL grudge — the denominator the
    /// conversion is a fraction of. Must be `history_sim_params::grudge_cap`,
    /// and `make_hard_coded_world` passes that field rather than trusting this
    /// default, so the two cannot drift. Kept as a parameter rather than read
    /// from a `history_sim_params&` so a harness can state the scale it means
    /// without also constructing a sim.
    int32_t score_full = 10000;

    /// Scores at or below this seed NOTHING. Sparse is the point, and it is the
    /// same argument `history_sim_params::grudge_floor` makes one layer down:
    /// most pairs never meet, and a pair whose only quarrel was a single raid
    /// four thousand years ago should arrive at the campaign indistinguishable
    /// from a pair that never met. Set well above `grudge_floor` (4) on
    /// purpose — the sim's floor exists to keep the TABLE sparse, this one
    /// exists to keep the SENTIMENT legible, and they are not the same
    /// threshold.
    int32_t min_score = 250;

    /// Sentiment a FULL grudge (`score_full`) is worth, per dimension, in the
    /// substrate's own units (`sentiment_params::limit` is 100, so these are
    /// read against that). NEGATIVE: a grudge is resentment, and the aggrieved
    /// party is the OBSERVER — `grudge::from` resents `grudge::to`, never the
    /// reverse (`history_sim.hpp` § grudge: "the realm that lost the province
    /// and the realm that took it do not feel the same way about each other").
    ///
    /// ACCESS IS HIT HARDER THAN TRUST, and the split is the design rather than
    /// a taste. `sentiment.hpp` defines Access as "will the observer LET the
    /// subject operate" and Trust as "will the observer BELIEVE the subject".
    /// An inherited historical grudge is overwhelmingly the first: the heirs of
    /// a realm that sacked your capital are not barred because their word is
    /// bad, they are barred because of what was done. Trust carries the
    /// smaller share so the two dimensions are not a single number wearing two
    /// names — they decay independently and a later contract can move Trust
    /// back without reopening the border.
    float full_access = -60.0f;
    float full_trust  = -25.0f;

    /// True iff nothing here can seed anything — the inertness predicate the
    /// substrate's own `sentiment_params::inert` sets the precedent for.
    bool inert() const
    {
        return score_full <= 0 || (full_access == 0.0f && full_trust == 0.0f);
    }
};

/// ONE SEEDED ROW, WITH ITS CAUSE. The return value is the legibility half of
/// BL-898 and is not incidental: the standing rule is that a grudge must act as
/// a force with a VISIBLE CAUSE, never a term inside an actor, so the seeding
/// hands back what it did and why rather than only mutating a table. A caller
/// that wants the number alone can ignore it; a ledger, a harness or a future
/// tooltip has the whole provenance without re-deriving anything.
struct grudge_sentiment_seed
{
    entity_id observer = null_entity; ///< The aggrieved polity's successor nation.
    entity_id subject  = null_entity; ///< The resented polity's successor nation.

    int32_t from_polity = -1; ///< Era -1 polity that held the grudge.
    int32_t to_polity   = -1; ///< Era -1 polity it was held against.

    int32_t score = 0; ///< The standing, decayed grudge score at the epoch.
    int32_t peak  = 0; ///< The highest it ever reached, before decay.
    int32_t event_count = 0; ///< How many named events ever fed it.

    float access = 0.0f; ///< Sentiment delta applied to Access.
    float trust  = 0.0f; ///< Sentiment delta applied to Trust.

    /// The largest contributing event — what, WHERE, WHEN. `grudge::events` is
    /// already sorted descending by magnitude with a total tie-break, so this
    /// is `events[0]` and is as deterministic as the table it came from.
    /// `has_cause` is false only for a grudge carrying no kept event, which
    /// `pass_one_output_valid` already rejects at the handoff.
    bool        has_cause = false;
    grudge_event cause{};
};

/// Why a grudge seeded nothing. Counted rather than silently dropped, because
/// "the record crossed the handoff and died there" is exactly the failure this
/// item exists to close and a silent drop is how it would come back.
struct grudge_sentiment_report
{
    int32_t rows_seeded    = 0; ///< Grudges that became a sentiment row.
    int32_t below_floor    = 0; ///< Score at or under `min_score`.
    int32_t no_successor   = 0; ///< Either polity has no successor nation.
    int32_t self_pair      = 0; ///< Both polities folded into ONE nation.
    int32_t out_of_range   = 0; ///< Polity id outside the successor table.
};

/// Seed nation->nation sentiment from the Era -1 grudge record.
///
/// @param t               The world's sentiment table, mutated in place.
/// @param grudges         `pass_one_output::grudges` / `history_sim_state::grudges`,
///                        sorted ascending by (from, to). Walked in that order.
/// @param polity_nation   Indexed BY Era -1 polity id: the successor nation's
///                        entity id, or `null_entity` where that polity has no
///                        successor. A VECTOR indexed by id, never a map, for
///                        the reason the polity fold gives in
///                        `nation_generation.cpp`: the answer a polity gets
///                        cannot depend on a container's layout.
/// @param p               The conversion.
/// @param out_seeds       Optional: the per-row provenance above, appended in
///                        table order.
/// @return                What was seeded and what was refused.
///
/// A POLITY WITH NO SUCCESSOR IS DROPPED, NEVER REDIRECTED. The mapping is not
/// 1:1 and cannot be made so: `merge_undersized_nations` absorbs realms below
/// the size floor, so an Era -1 polity can end the handoff with no nation of
/// its own. The alternative — attaching its grudges to whichever nation now
/// holds its ground — would be id arithmetic standing in for a successor
/// concept this project deliberately does not have (`history_sim_state::grudges`
/// § A DEAD POLITY'S GRUDGES ARE LOST makes the same call one layer down, for
/// the same reason). A quarrel with no heir is over.
///
/// TWO POLITIES THAT FOLDED INTO ONE NATION seed nothing either: a nation has no
/// sentiment about itself, which is the substrate's own rejection
/// (`apply_sentiment_events`) made explicit here so it is counted rather than
/// swallowed.
///
/// Never overwrites. The fold is additive through `apply_sentiment_events`, so
/// two Era -1 polities whose grudges land on one pair ACCUMULATE (bounded by
/// `sentiment_params::limit`), and any sentiment computed later by conduct moves
/// the same row from wherever generation left it.
grudge_sentiment_report seed_grudge_sentiment(
    sentiment_table&                   t,
    const std::vector<grudge>&         grudges,
    const std::vector<entity_id>&      polity_nation,
    const grudge_sentiment_params&     p,
    std::vector<grudge_sentiment_seed>* out_seeds = nullptr);

/// One line naming a seeded row: who resents whom, how much, and the event that
/// did it. The printable half of "a force with a visible cause" — the same job
/// `grudge_event_line` does for the record, done for its consequence.
///
/// @param s  The seeded row.
/// @param ss The settlement whose `regions` name the place. Region names are
///           read only where `cause.region` is in range.
std::string grudge_sentiment_line(const grudge_sentiment_seed& s, const settlement_state& ss);
