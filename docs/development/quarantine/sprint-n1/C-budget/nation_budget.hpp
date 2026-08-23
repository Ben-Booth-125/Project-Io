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
//     float, the same tick, so the sum is conserved by construction rather
//     than by reconciliation. This is the same discipline BL-480 applied to
//     the levy, and for the same reason: a leak here would be the BL-392 class
//     of silent value destruction, one grain up.
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
//     never silently dropped.
//
// DETERMINISM. Once per economy tick, over nations in ASCENDING ID, over lines
// in the fixed authored enum order, over each line's claims in ascending
// (corp id, arrival index). `w.nations` and `w.corporations` are unordered_maps
// and float addition is not associative, so every accumulation order here is
// pinned. No RNG, no wall clock, no iteration whose result depends on hash
// layout.
// ---------------------------------------------------------------------------

/// The lines a nation can spend on. Five are Ben's (2026-08-22); four more were
/// proposed alongside them and accepted. The SPEND MECHANICS are generic over
/// this enum — what a line actually BUYS is BL-538's, not this item's, so a
/// line no consumer has yet produces no claim and is simply skipped.
///
/// APPEND-ONLY once serialised: a weight vector is indexed by these values, so
/// inserting or renumbering one would silently re-point every authored weight.
/// Add at the end, bump `priority_count`.
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
};

/// Number of enumerators in `budget_priority`. Every weight vector is this long.
inline constexpr std::size_t priority_count = 9;

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

    /// True when demand exceeded share, i.e. the fill was rationed pro rata.
    bool  rationed = false;
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
    /// Equal, bit-for-bit, to the sum of the treasury debits accumulated in that
    /// same order — the conservation property, stated where a caller can check it.
    float total_transferred = 0.0f;
};

/// Run one economy tick of national spending.
///
/// For each nation in ASCENDING ID with a budget and a positive treasury:
///   spendable = treasury x (1 - reserve_fraction)
///   share(L)  = spendable x weight(L) / sum(weights)
///   paid(L)   = min(share(L), demand(L)), rationed pro rata across the line's
///               claims when demand exceeds share
/// and every paid credit is debited from the treasury and credited to the named
/// corporation's balance — a direct transfer, never a market order.
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
/// @param claims   This tick's line claims, from BL-538's per-line consumers.
///                 Order-insensitive: the pass sorts them into its own walk order.
/// @param report   Optional sink for the per-nation / per-line detail. Null for
///                 callers that only need the transfers, exactly as
///                 `apply_budget`'s `breakdown` is.
void run_national_budget(world& w,
                         const std::map<entity_id, nation_budget>& budgets,
                         const std::vector<budget_claim>& claims,
                         national_budget_tick* report = nullptr);
