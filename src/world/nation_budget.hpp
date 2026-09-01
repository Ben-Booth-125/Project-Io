#pragma once

#include "entity.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

struct world; // forward-declared; nation_budget.cpp reads it.

// ---------------------------------------------------------------------------
// The national budget (BL-537) — the spend side of nation_component::treasury
// ---------------------------------------------------------------------------
// Until this, a nation's treasury had only ever RISEN. Two flows credit it —
// BL-480's extraction levy and the D4 import tariff — and nothing whatsoever
// debited it, which is why NR-398 called it "a scoreboard with no game
// attached". This is the debit half.
//
// THE OBJECT IS WEIGHTS, NOT AMOUNTS (Ben, 2026-08-22). A nation states what it
// CARES ABOUT and the amounts follow from what it HOLDS. That is the whole
// reason the model is a weight vector rather than a line-item budget: a poor
// nation and a rich one of the same character then behave recognisably alike,
// differing in scale and not in kind, and no authored number has to be re-tuned
// when a treasury grows. It also means the authoring side (a nation scorer,
// BL-542) never has to know how rich it is to state a preference.
//
// THREE RULES KEEP IT HONEST. Each is a requirement row, not a comment:
//
//  1. EVERY CREDIT OUT IS A DIRECT TRANSFER TO A NAMED CORPORATION. Ben's
//     ruling: "Payment to corps is direct and not on the market." A budget
//     line does not bid, does not clear, and never touches `clear_markets` —
//     it debits the treasury and credits a corporation balance in the SAME
//     float, the same tick. This is the same discipline BL-480 applied to the
//     levy, and for the same reason: a leak here would be the BL-392 class of
//     silent value destruction, one grain up.
//
//     WHAT "CONSERVED" MEANS HERE, EXACTLY (narrowed 2026-08-23, Sprint N1).
//     The pass debits precisely what it credits: `nation_budget_result::paid`,
//     `national_budget_tick::total_transferred` and the treasury's delta are
//     ONE accumulation, not three that agree approximately, and that identity
//     is bit-exact for any weights however awkward. What is not guaranteed is
//     the world's total credit afterwards, because `balance += amount` rounds
//     once the destination has outgrown the credit — the same rounding
//     `apply_budget` has always carried. So: bit-exact when each credit is
//     representable at its destination, and otherwise off by the destinations'
//     own rounding and nothing else. The harness proves the distinction rather
//     than assuming it, by running one span whose balances are swept (residual
//     exactly zero) against an identical span whose balances accumulate.
//
//  2. NATIONS SAVE. `reserve_fraction` is withheld from the tick's spendable
//     total, and an UNDERSPENT LINE ACCUMULATES rather than evaporating —
//     because the only credits that leave the treasury are the ones actually
//     paid to a corporation. A line whose share exceeds its demand simply
//     leaves the balance where it was, so next tick every line's share is
//     larger. Deliberately NOT a per-line carry bucket: a weight is a claim on
//     THIS tick's spendable, not a pot, and a bucket would make a line's
//     spending power depend on history the player cannot see.
//
//  3. BOUNDED BY THE TREASURY. A nation cannot allocate what it does not hold.
//     Where a line's demand exceeds its share the line is PARTIALLY FILLED and
//     says so (`budget_line_result::fill_fraction` < 1) — never overdrawn, and
//     never silently dropped. The bound is a RUNNING TOTAL of what has been
//     paid, never a decremented remainder: a remainder drops the low bits of
//     every pay it subtracts, so it under-counts, and a nation facing enough
//     claims pays past its allotment while its bound says there is room. That
//     shipped, and it drove solvent treasuries negative — which is not cosmetic,
//     because the `treasury > 0` gate then locks the nation out of spending on
//     every following tick. See `run_national_budget`'s § The bound.
//
//  3a. EARMARKED CLAIMS ARE WHOLE OR NOTHING (Ben, 2026-08-23, NR-568). A claim
//     that names a `subject` is not a request for credits; it is a request that
//     ONE NAMED THING be paid for — today, the survey of a body, which
//     `dispatch_survey` debits at its full cost in the same tick the credit
//     lands (Sprint N3 T6). Rule 3's pro-rata partial fill would leave such a
//     corp holding a fraction of a survey: a credit that dispatches nothing,
//     which is exactly the fungible top-up Ben ruled out. So an earmarked claim
//     is paid in FULL when the line's remaining share and the nation's
//     remaining headroom both cover it, and otherwise SKIPPED — pay 0, the
//     share it would have taken left in the treasury (rule 2 already says where
//     that goes), the skip counted on the line (`budget_line_result::skipped`)
//     and the line flagged `rationed`. Unearmarked claims keep rule 3's partial
//     fill unchanged. The walk order is unchanged too: a skipped earmarked claim
//     changes which later claims are reached only through the share it leaves.
//
// DETERMINISM. Once per economy tick, over nations in ASCENDING ID, over lines
// in the fixed authored enum order, over each line's claims in ascending
// (corp id, arrival index). `w.nations` and `w.corporations` are unordered_maps
// and float addition is not associative, so every accumulation order here is
// pinned. No RNG, no wall clock, no iteration whose result depends on hash
// layout.
// ---------------------------------------------------------------------------

/// The lines a nation can spend on. Five are Ben's (2026-08-22); four more were
/// proposed alongside them and accepted; `space_programme` is Ben's, 2026-08-26
/// (BL-644). The SPEND MECHANICS are generic over this enum — what a line
/// actually BUYS is BL-538's, not this item's, so a line no consumer has yet
/// produces no claim and is simply skipped.
///
/// APPEND-ONLY once serialised: a weight vector is indexed by these values, so
/// inserting or renumbering one would silently re-point every authored weight.
/// Add at the end, bump `priority_count` — and the weight record is
/// fixed-width on the save seam (world_save.cpp § w_nation_budget), so the
/// bump is a `world_save_version` bump too.
enum class budget_priority : uint8_t
{
    logistics_maintenance = 0, ///< Keeping the road/hub network standing (LOGISTICS.md).
    schooling             = 1, ///< Population capability; no consumer yet.
    military_research     = 2, ///< Force-side research; no consumer yet.
    academic_research     = 3, ///< The civil tech ladder (`science` is reached, not spent).
    public_exploration    = 4, ///< State-funded survey (DISCOVERY.md's geographic fog).
    contracted_force      = 5, ///< Buying force it does not raise (CONTRACTS.md, BL-377).
    strategic_reserve     = 6, ///< Buying goods to HOLD. Distinct from `reserve_fraction`,
                               ///< which withholds CREDITS: this line spends them.
    public_works          = 7, ///< Works a corporation builds and the nation pays for.
    charters              = 8, ///< Paying a corporation to exist somewhere it otherwise would not.
    space_programme       = 9, ///< Government satellite launches — spacecraft_components and
                               ///< propellant, bought procurement-style and CONSUMED
                               ///< (space_programme.hpp is the consumer; BL-644).
};

/// Number of enumerators in `budget_priority`. Every weight vector is this long.
inline constexpr std::size_t priority_count = 10;

/// Whether a line's claims name a SUBJECT — the one thing the credit is
/// earmarked to pay for (rule 3a). `public_exploration`'s subject is the BODY
/// to survey (NR-568); `contracted_force`'s is the PROVINCE a mercenary_offer
/// targets (BL-572, CONTRACTS.md § Where offers come from) — an offer's
/// escrow is a request that one named province's work be paid for, not a
/// fungible credit, exactly the earmark shape rule 3a already generalises.
/// `space_programme`'s is the BODY the purchased goods stand on (BL-644):
/// state demand arrives in LUMPS (NATIONS.md), and the earmark's whole-or-
/// nothing fill is precisely what a lump is — a fraction of a launch lot
/// dispatches nothing, the same argument NR-568 made for a fraction of a
/// survey.
/// A claim on such a line with a null subject is rejected whole at gather; a
/// claim on any other line has its subject IGNORED (nulled at gather, so
/// "earmarked" and "has a subject" stay one predicate downstream).
///
/// `derive_contract_offers` (nation_step.cpp) does not itself route through
/// `run_national_budget`'s claim/gather machinery — like `military_research`'s
/// garrison-upkeep consumer, a mercenary offer has no corp on the other end
/// to be a `budget_claim`'s payee, so it recomputes the line's share directly
/// (see `run_nation_garrison_upkeep`'s own comment for why). This predicate is
/// still extended here — truthfully naming what the line's spend IS earmarked
/// against — for BL-573's `accept_offer`, the consumer that turns an offer
/// into a corp claimant and is the first thing that could reach `subject_exists`.
inline constexpr bool line_takes_subject(budget_priority line)
{
    return line == budget_priority::public_exploration
        || line == budget_priority::contracted_force
        || line == budget_priority::space_programme;
}

/// What a nation cares about, and how much of its treasury it refuses to touch.
///
/// `weights` are NORMALISED — they are read as fractions of one tick's spendable
/// total. An authored set that does not sum to 1.0 is normalised on read (once,
/// in one place) rather than rejected, so a scorer can hand over raw scores; a
/// set that sums to zero spends nothing at all, which is exactly the inertness
/// condition R2 asserts.
struct nation_budget
{
    /// Fraction of the tick's spendable total each line may claim, indexed by
    /// `static_cast<std::size_t>(budget_priority)`. All zero by default: a
    /// nation with no authored budget spends NOTHING.
    std::array<float, priority_count> weights{};

    /// Fraction of the treasury withheld from this tick entirely — the savings
    /// dial. Clamped to [0, 1] on read; 1.0 is a nation that spends nothing and
    /// banks everything.
    float reserve_fraction = 0.0f;
};

/// One claim on one nation's budget line: a NAMED corporation asking a NAMED
/// nation for credits, on one line, this tick.
///
/// EVERY FIELD IS VALIDATED AT GATHER TIME, `line` included. `budget_priority`
/// has a `uint8_t` underlying type, so every one of 0..255 is a valid VALUE while
/// only 0..9 is a valid INDEX — a claim carrying anything else is dropped whole,
/// never clamped onto a line nobody asked for. Claims are in-process today, but
/// the standing rule that an AI-facing seam is an untrusted input boundary makes
/// this wire-reachable the moment one arrives over `--serve`.
///
/// Claims are produced upstream (BL-538's per-line consumers) and passed in,
/// exactly as `apply_budget` takes `production` rather than deriving it — the
/// mechanics here stay pure over the claim list and know nothing about what any
/// line means. A line with no claim transfers to nobody and is skipped.
struct budget_claim
{
    entity_id       nation = null_entity; ///< Who is being asked to pay.
    entity_id       corp   = null_entity; ///< Who is paid — always a named corporation.
    budget_priority line   = budget_priority::logistics_maintenance;
    float           amount = 0.0f;        ///< Credits the claim would take if paid in full.

    /// What the credit is EARMARKED for, on a line that takes one
    /// (`line_takes_subject`). For `public_exploration` this is the BODY to
    /// survey, and `amount` is that body's FULL survey cost (NR-568): the
    /// payment is the dispatch of that survey, not a top-up. Validated at
    /// gather like every other field — null on a subject-taking line rejects
    /// the claim whole, and a body `w.bodies` does not hold does the same. On
    /// any other line it is ignored and reported as null.
    entity_id       subject = null_entity;
};

/// What one line did this tick. Reported, not stored — a surface reads it to say
/// "we funded 40 % of what logistics asked for", which is the whole point of
/// rule 3 being a partial fill rather than a drop.
struct budget_line_result
{
    float share  = 0.0f; ///< Credits this line was allotted (spendable x normalised weight).
    float demand = 0.0f; ///< Credits its claims asked for.
    float paid   = 0.0f; ///< Credits actually transferred. Never more than `share`.

    /// `paid / demand`, or 1.0 when nothing was asked for. Below 1.0 means the
    /// line was partially filled — the line SAYS SO rather than dropping a claim.
    float fill_fraction = 1.0f;

    /// True when the line could not meet its demand: either demand exceeded
    /// share and the unearmarked claims were rationed pro rata (rule 3), or an
    /// earmarked claim was skipped whole (rule 3a) — `skipped` says which.
    bool  rationed = false;

    /// Earmarked claims skipped whole under rule 3a this tick — asked for more
    /// than the line's remaining share (or the nation's remaining headroom)
    /// could cover in full, so they were paid nothing rather than a fraction.
    int   skipped  = 0;
};

/// One credit actually moved this tick: the record of WHO paid WHOM, on which
/// line, for what. This is the datum the pass used to drop at the end of its
/// walk — the transfer list existed only to apply the balances — and it is what
/// "who is paying me" (BL-555) renders, and what the earmarked dispatch
/// (Sprint N3 T6) walks to turn a `public_exploration` credit into the survey
/// it was for. One entry per claim paid; a claim paid nothing has no entry.
struct budget_transfer
{
    entity_id       corp    = null_entity; ///< Payee.
    entity_id       nation  = null_entity; ///< Payer.
    budget_priority line    = budget_priority::logistics_maintenance;
    entity_id       subject = null_entity; ///< The claim's earmark (null on a line that takes none).
    float           credits = 0.0f;        ///< What moved — the same float the balance received.

    /// `credits / the claim's amount`: THIS claim's fill, not the line's. An
    /// earmarked transfer is always 1.0 (rule 3a: whole or nothing); an
    /// unearmarked one on a rationed line carries its pro-rata fraction.
    float           fill_fraction = 1.0f;

    /// The LINE's `rationed` flag, copied for context: true means the line this
    /// transfer sits on could not meet its demand, whether or not this claim
    /// itself was cut. Read `fill_fraction` for this claim's own fill.
    bool            rationed = false;
};

/// What one nation did this tick.
struct nation_budget_result
{
    entity_id nation    = null_entity;
    float     treasury  = 0.0f; ///< Balance BEFORE the pass.
    float     spendable = 0.0f; ///< treasury x (1 - reserve_fraction).
    float     paid      = 0.0f; ///< Total transferred out — the treasury's debit.

    /// Per-line detail, indexed by `static_cast<std::size_t>(budget_priority)`.
    std::array<budget_line_result, priority_count> lines{};
};

/// The pass's report. Nations in ascending id — the same order the pass walked.
struct national_budget_tick
{
    std::vector<nation_budget_result> nations;

    /// Sum of every credit transferred, accumulated in the pass's own order.
    /// Equal, bit-for-bit, to the treasury debits — they are the same
    /// accumulation, so the identity holds for any weights. See rule 1 for what
    /// this does and does not say about the world's total credit.
    float total_transferred = 0.0f;

    /// Every credit moved this tick, SORTED (corp, nation, line), arrival order
    /// within equal keys — a payee's view, grouped for "who is paying me". A
    /// RECORD of the credits, not the order they were applied in: the balances
    /// are still credited on the pass's own (corp, nation, amount) sort, which
    /// adding this list did not move.
    ///
    /// The SAME credits `total_transferred` sums, but NOT in the same order:
    /// that total is nested — each nation's pays summed in (line, corp, arrival)
    /// order, then the per-nation totals summed in ascending nation — and float
    /// addition is not associative. A reader wanting the bit-exact identity
    /// re-walks this list in the pass's order (the harness does exactly that);
    /// a flat sum over it agrees only to rounding.
    std::vector<budget_transfer> transfers;
};

/// Run one economy tick of national spending.
///
/// For each nation in ASCENDING ID with a budget and a positive treasury:
///   spendable = treasury x (1 - reserve_fraction)
///   share(L)  = spendable x weight(L) / sum(weights)
///   paid(L)   = min(share(L), demand(L)), rationed pro rata across the line's
///               UNEARMARKED claims when demand exceeds share; an EARMARKED
///               claim (one with a subject) is paid whole or skipped (rule 3a)
/// and every paid credit is debited from the treasury and credited to the named
/// corporation's balance — a direct transfer, never a market order — and
/// recorded on `report->transfers`.
///
/// INERT BY DEFAULT. A nation absent from `budgets`, one whose weights are all
/// zero, one whose reserve is 1.0, one with a non-positive treasury, and one
/// with no claims all spend exactly nothing and touch no float. A world where
/// none of the nations authors a budget is therefore bit-identical to the
/// pre-BL-537 build.
///
/// @param w        World; nation treasuries and corporation balances are mutated,
///                 and nothing else is.
/// @param budgets  Authored weights per nation. Passed in rather than read off
///                 `nation_component` because nothing authors them yet: the
///                 nation scorer that will (BL-542) is unbuilt, and a persistent
///                 field ahead of its author would be state with no writer and a
///                 serialisation seam owed for nothing. See the item report.
/// @param claims   This tick's line claims, from BL-538's per-line consumers —
///                 today `run_corp_strategic_step`'s cash-gated surveys
///                 (`economy_report::budget_claims`). Order-insensitive: the
///                 pass sorts them into its own walk order.
/// @param report   Optional sink for the per-nation / per-line detail and the
///                 transfer list. Null for callers that only need the balances
///                 moved, exactly as `apply_budget`'s `breakdown` is.
void run_national_budget(world& w,
                         const std::map<entity_id, nation_budget>& budgets,
                         const std::vector<budget_claim>& claims,
                         national_budget_tick* report = nullptr);
