#pragma once

// ---------------------------------------------------------------------------
// landscape_search — BL-770 slice 3. Where candidate landscapes COME FROM.
//
// `landscape_score.hpp` answers "how good is this landscape". This file answers
// the half nothing owned: which landscapes get scored at all.
//
// THE SHAPE (Ben, 2026-09-06; GENERATION_STRATEGY.md § How a candidate is
// PRODUCED — greedy refinement). Start from one seed candidate, score it,
// perturb the incumbent, re-score, keep the better — for a FIXED NUMBER OF
// ROUNDS, never until convergence. A round proposes one perturbation, scores
// it, and the argmax over {incumbent, proposal} becomes the next incumbent.
//
// ONE AXIS: PLACEMENT, the seed the candidate's holdings are staked from (Ben,
// 2026-11-12; GENERATION_STRATEGY.md § "The search walks placement alone, over
// fewer rounds"). The walk once proposed along three — roster, placement and
// road tier — and two retired: a charter budget sets the roster on every
// shipped world, so that axis had nothing to choose, and the road tier rarely
// won a round while costing a third of each. A candidate's `corporation_count`
// and `road_tier` stay at the seed candidate's values for the whole walk. The
// objective is unchanged, all five terms: the fifth (reach quality) still
// separates placement-only candidates, because the ports and inland hubs a
// candidate's charter web lays from its placement seed are supply anchors that
// reach cost runs to (landscape_score.hpp).
//
// A WORLD WITH NO CHARTER BUDGET RUNS ZERO ROUNDS. No budget, a refused spend
// and a budget that opens no specialist are all laid by the no-budget branch,
// which charters nothing and reads no placement seed (BL-1404), so every
// proposal would score exactly as the seed candidate and could never be taken.
// The search scores the seed candidate alone there: the same winner, score and
// world the full walk would return, without the evaluations that cannot move it.
//
// THE DRAW TAGS DID NOT MOVE. `landscape_axis` keeps its three slots and
// `landscape_axis_count` its value of 3, because the perturbation stream is
// keyed by (round, axis) as `round * 3 + axis`: holding the stride holds the
// placement draw, so a placement proposal is exactly the one the three-axis
// walk drew in the same round from the same incumbent. Roster and road tier
// are slots in that tag space, never proposed.
//
// FIXED ROUNDS IS A DETERMINISM REQUIREMENT, NOT A BUDGET ONE. A convergence
// test makes the amount of work depend on the landscape, so two worlds run the
// search for different lengths and the threshold becomes a hidden tuning knob.
// A fixed round count makes the search a pure function of
// (base world, seed, round count) with a cost known before it starts.
//
// WHAT KEEPS IT DETERMINISTIC, and every clause is load-bearing:
//   * Perturbations are drawn from a seeded `checkpoint_rng` sub-stream keyed by
//     (round, axis) — the project's existing splitmix64 mechanism, not a second
//     one. Keying rather than sequencing means a proposal's draw does not depend
//     on whether a sibling proposal was drawn first, which is what lets a round's
//     proposals be scored in any order (or on any number of threads) and still
//     be the same proposals.
//   * A round's proposals are scored INDEPENDENTLY — each on its own copy of the
//     base world — and never in completion order. `score_landscape` MEMOISES the
//     per-body reach field into the world it scores (see its header note), so
//     sharing one `world` across threads would be a data race as well as an
//     ordering hazard. Each proposal owns its world.
//   * The argmax runs over a TOTAL order with an EXPLICIT tie-break
//     (`compare_landscape`), so an exact tie resolves the same way on every
//     machine and at every thread count.
//   * The incumbent is replaced only on a STRICT improvement, so a tie leaves
//     the incumbent standing rather than churning between equals.
//
// GREEDY IS CHOSEN KNOWING WHAT IT COSTS. It finds a local optimum, which is
// acceptable precisely because the objective is viable-but-uneven rather than
// maximal — a search grinding toward the single best landscape would be
// re-introducing the "most profitable" reading Ben's point 4 rejects. The walk
// also yields a PATH: `landscape_search_result::path` records what each round
// proposed, what it scored and whether it was taken, so a landscape is
// inspectable rather than being one draw among hundreds.
//
// PARALLELISM IS AN OPTIMISATION, and with one live axis it has nothing to
// spread: a round holds one proposal, so at most one scoring thread runs
// whatever `thread_count` says. The constraint it imposed stays — the winner
// never varies with `thread_count`, which the harness PROVES — and the threaded
// path is kept, generic over a round's proposals, so a round with more than one
// inherits it. Each scoring thread holds its own copy of the base world.
//
// NOTHING HERE IS PERSISTENT. The search runs at generation time and its output
// is a world, not a field — so it adds nothing to the flat-binary save path.
// ---------------------------------------------------------------------------

#include "charter_budget.hpp"
#include "landscape_score.hpp"

#include <cstdint>
#include <string>
#include <vector>

class recipe_registry;
struct generation_progress; // hard_coded_world.hpp; the search only writes its inner bar

/// The axis SLOTS of the (round, axis) draw tag. Only PLACEMENT is proposed
/// (Ben, 2026-11-12); roster and road tier are retired axes whose slots are
/// kept so the tag stride, and with it every placement draw, did not move.
enum class landscape_axis : int
{
    roster    = 0,   ///< retired: never proposed
    placement = 1,   ///< the one live axis
    road_tier = 2,   ///< retired: never proposed
};
/// The TAG STRIDE (slots per round), not the number of axes a round proposes.
constexpr int landscape_axis_count = 3;
/// Proposals per round: placement alone.
constexpr int landscape_live_axis_count = 1;

const char* landscape_axis_name(landscape_axis a);

/// One candidate landscape. Ben's point 3 — generation runs ONCE and only
/// candidates repeat. The search varies `placement_seed` alone; the other two
/// fields are held at the seed candidate's values through the whole walk, and
/// are kept because `apply_landscape_candidate` still reads them.
struct landscape_candidate
{
    /// Corporations a LEGACY apply (the 4-argument overload) lays. Not a search
    /// axis (retired 2026-11-12): a budget world's roster is the budget's, and a
    /// no-budget world charters nothing (BL-1404).
    int corporation_count = 8;
    /// PLACEMENT, the one search axis: the seed the holdings are staked from.
    std::uint32_t placement_seed = 0xC0FFEEu;
    /// Every tile that already carries a road is developed to at least this
    /// tier (1 = Track, 2 = Road, 3 = Highway; purely additive, never a
    /// downgrade). Not a search axis (retired 2026-11-12): every candidate keeps
    /// the seed's tier, and the campaign's seed tier is 1, a no-op on the field.
    std::uint8_t road_tier = 1;
};

/// Total-order key for two candidates that score EXACTLY alike. Used only to
/// order proposals against each other; never to unseat an incumbent (see
/// `compare_landscape`).
bool candidate_key_less(const landscape_candidate& a, const landscape_candidate& b);

struct landscape_search_params
{
    /// FIXED. Not a convergence budget — see the header. The campaign's count
    /// is set in `campaign_search_params`, read off the placement curve.
    int rounds = 6;

    /// Seeds the perturbation stream. The search is a pure function of
    /// (base world, this seed, rounds).
    std::uint32_t seed = 0x5EA12C00u;

    /// The seed candidate the walk starts from.
    landscape_candidate start{};

    /// Whether applying a candidate REPLACES the specialist roster — strips the
    /// one world-gen laid (`remove_specialist_roster`) and lays the candidate's
    /// own (`generate_corporations`, from `world::gen_settlement`) — as well as
    /// laying the background firms.
    ///
    /// TRUE AT THE LIVE SEAM (BL-977). It was false there because
    /// `generate_corporations` appends and had already run inside
    /// `make_hard_coded_world`, which made the roster axis a no-op that still
    /// cost a third of every round. The removal is the inverse that makes the
    /// axis real. False is kept for an instrument that wants to hold the
    /// world-gen roster fixed.
    bool regenerate_specialists = true;

    /// Print one line per round — which axis the round's best proposal came
    /// from, whether it unseated the incumbent, and the round's cost. The
    /// timing is a DIAGNOSTIC read from the steady clock and printed; nothing
    /// in the search reads it back, so the walk stays a pure function of
    /// (base world, seed, rounds).
    bool print_rounds = true;

    /// BL-1085: a loading screen's sink, or null. The search reports its
    /// evaluations done over the evaluations planned on the INNER bar
    /// (`generation_progress::report_sub`) so "Placing buildings" never
    /// looks stopped. WRITE-ONLY, as every progress tap is: nothing in the
    /// walk reads it back, so a watched search is the same walk as an
    /// unwatched one.
    generation_progress* progress = nullptr;

    /// Threads used to score one round's proposals. 1 = serial (the default
    /// here); the campaign passes `k_campaign_search_threads` (2, BL-1136). The
    /// result must not depend on this value; that is the property the harness
    /// asserts.
    int thread_count = 1;

    landscape_score_params score{};

    /// BL-1032 — the per-centre CHARTER BUDGET, or none. NONE BY DEFAULT here;
    /// the shipped new-game path (app::start_new_game_prelude, BL-1042) passes
    /// the world's own stockpile budget (`build_stockpile_budget`), which the
    /// Industrialisation span fills (on by default since BL-1044) and which is EMPTY
    /// on a world the span did not run on — the pre-budget search.
    ///
    /// Null or EMPTY (an all-zero budget is the same state — charter_budget
    /// drops entries <= 0) is the no-budget search: the seed candidate alone,
    /// laid by the no-budget branch, which charters nothing and reads no
    /// placement seed (BL-1404) — so no round is run (see the header).
    ///
    /// NON-EMPTY makes this a BUDGET WORLD: each candidate's corporations are
    /// chartered from the budget (`apply_landscape_candidate`'s budget
    /// overload), so the budget decides the roster (GENERATION_STRATEGY.md
    /// phase 6 point 3) and the placement seed decides where it is staked.
    /// Only a budget world walks. A non-empty budget whose spend params
    /// are refused (`charter_spend_refusal`) is NOT a budget world: the refusal
    /// is checked before anything else, the search runs exactly as with no
    /// budget, prints REFUSED with the reason, and sets
    /// `landscape_search_result::charter_refused`. Nor is a non-empty budget on
    /// which no centre a nation owns affords a specialist (NR-910,
    /// `charter_budget_affords_specialist`): it falls back the same way, prints
    /// that it opens no specialist, and sets `charter_fell_back`.
    ///
    /// Read-only and shared across the scoring threads; the pointee must
    /// outlive the search.
    const charter_budget* budget = nullptr;

    /// How the budget is spent. Read only on a budget world, and COPIED per
    /// evaluation (threads share these params). The prices have no default.
    charter_spend_params spend{};
};

/// One proposal, kept whether or not it won — the path is the point.
struct landscape_search_step
{
    int                 round    = 0;
    landscape_axis      axis     = landscape_axis::roster;
    landscape_candidate proposal{};
    landscape_score     score{};
    bool                accepted = false;   ///< became the incumbent
};

struct landscape_search_result
{
    landscape_candidate seed_candidate{};
    landscape_score     seed_score{};

    landscape_candidate winner{};
    landscape_score     winner_score{};

    std::vector<landscape_search_step> path;
    int evaluations = 0;      ///< score_landscape calls, seed included
    int accepted    = 0;      ///< rounds that produced a strict improvement

    /// Accepted steps per axis slot, indexed by `landscape_axis`. Only the
    /// placement slot can be non-zero since the walk went placement-only
    /// (2026-11-12); the array keeps its shape for the readers that print it.
    int accepted_by_axis[landscape_axis_count] = { 0, 0, 0 };

    /// Wall time per round, milliseconds, DIAGNOSTIC ONLY (see `print_rounds`).
    /// Index 0 is the seed candidate's evaluation.
    std::vector<double> round_ms;

    /// BL-1032. Set when a NON-EMPTY charter budget arrived with spend params
    /// `charter_spend_refusal` refused; `charter_refusal` says why. Such a search
    /// is exactly the no-budget search, so every field above is what it is on a
    /// world with no budget.
    bool        charter_refused = false;
    std::string charter_refusal;

    /// NR-910. Set when a NON-EMPTY budget that was not refused opens no
    /// specialist (`charter_budget_affords_specialist` false on the base world):
    /// the no-specialist world falls back as a refusal does, so the search is
    /// exactly the no-budget search. Never set with `charter_refused`.
    bool        charter_fell_back = false;
};

/// Lay a candidate onto @p w, which must be a copy of the phase-4 base world.
/// Applies the road tier, then (when @p regenerate_specialists) replaces the
/// specialist roster, then lays the background firms, and invalidates the
/// logistics caches every one of those can move. Deterministic in the
/// candidate alone. See `landscape_search_params::regenerate_specialists`.
void apply_landscape_candidate(world& w, const recipe_registry& reg,
                               const landscape_candidate& c,
                               bool regenerate_specialists);

void apply_landscape_candidate(world& w, const recipe_registry& reg,
                               const landscape_candidate& c);

/// BL-1032 — lay a candidate with a CHARTER BUDGET. Branches exactly ONCE:
///
///   * @p budget null or empty -> the 4-argument overload above, verbatim —
///     today's calls in today's order, and @p spend and @p report are not read.
///   * @p budget non-empty but @p spend REFUSED (`charter_spend_refusal`,
///     checked before any mutation) -> the 4-argument overload above, verbatim,
///     exactly as with no budget; @p report receives `charter_refused_report`.
///     No roster is removed and nothing is chartered.
///   * @p budget non-empty and not refused, but NO CENTRE A NATION OWNS AFFORDS
///     A SPECIALIST (`charter_budget_affords_specialist`, NR-910) -> the same:
///     the 4-argument overload verbatim, decided before any mutation; @p report
///     receives `charter_fallback_report`.
///   * otherwise -> the road tier and the recipe pass as above, then
///     `remove_specialist_roster` and `charter_web_from_budget` from the
///     candidate's placement seed and `world::gen_settlement`, in place of
///     `generate_corporations` and `generate_background_firms`. The budget
///     charters the whole web, so @p regenerate_specialists is not read on
///     this branch.
///
/// @p report, when non-null, receives the spend's report on the budget branch,
/// the refusal report on a refused budget and the fallback report on a
/// no-specialist one, and is left untouched on the null-or-empty branch.
void apply_landscape_candidate(world& w, const recipe_registry& reg,
                               const landscape_candidate& c,
                               bool regenerate_specialists,
                               const charter_budget* budget,
                               const charter_spend_params& spend,
                               charter_spend_report* report);

/// The total order the argmax runs on. Returns <0 if @p a sorts BEFORE @p b
/// (i.e. @p a is WORSE), >0 if better, 0 only when every scored term is exactly
/// equal. Score terms only — no candidate identity — so "equal" here is the tie
/// that leaves an incumbent standing.
int compare_landscape(const landscape_score& a, const landscape_score& b);

/// Run the greedy refinement search. @p base must be the phase-4 world BEFORE
/// any candidate roster is layered on; it is copied per proposal and never
/// mutated.
landscape_search_result search_landscape(const world& base, const recipe_registry& reg,
                                         const landscape_search_params& p);
