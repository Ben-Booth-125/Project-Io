#pragma once
#include "entity.hpp"
#include "nation_ai.hpp"

struct world;
struct economy_report;
class recipe_registry;

// ---------------------------------------------------------------------------
// The nation step (Sprint N3, 2026-08-23) — where a nation ACTS each tick
// ---------------------------------------------------------------------------
// Sprint N1 landed the national budget (BL-537) and Sprint N2 the nation scorer
// (BL-542), and nothing in the tick called either: a nation authored no weights
// and spent nothing, which is the `military_points` defect with a new name. This
// is the caller. It runs AFTER `apply_budget` — the treasury holds this tick's
// levy (BL-480) and tariff (D4) only then; running it inside `run_economy_step`
// would spend last quarter's revenue — and BEFORE `advance_tech_gates`, whose
// `surplus` gate reads the corp balance the subsidy just moved.
//
// FOUR MOVES, IN ORDER, AND EACH IS SOMEBODY ELSE'S MACHINERY:
//
//  1. SCORE. `score_national_budgets` returns the weight vector for exactly the
//     nations DUE on `econ_tick` under the staggered cadence (index over the
//     SORTED nation set — a nation admitted with the highest id shifts nobody's
//     slot). Each returned slot OVERWRITES one entry of `w.nation_budgets`; the
//     rest keep last quarter's weights. The map's emptiness is the inertness
//     proof: no nation scored, nothing spent.
//  2. GATHER. This tick's claims come off `economy_report::budget_claims`,
//     emitted earlier in the same tick by corp_ai's cash gate — a rival that
//     wanted to survey a body and could not afford to asks its home nation.
//     Nothing here invents a claim.
//  3. SPEND. `run_national_budget` — the pure pass over (weights, claims). Every
//     credit out is a direct transfer to a named corporation; the treasury is
//     never overdrawn; an earmarked claim is paid whole or not at all.
//  4. DISPATCH THE EARMARK (Ben, 2026-08-23, NR-568). A paid `public_exploration`
//     transfer names a body. The corp was credited exactly `survey_cost(body)`
//     and `dispatch_survey` debits exactly that, the same tick — so the credit
//     never sits on the balance for the corp's next evaluation to spend on a
//     build. Conservation holds by construction: credit in == cost out. The
//     dispatch is attempted in transfer order (sorted corp, nation, line), and a
//     dispatch that cannot proceed (the body was surveyed by someone else this
//     very tick, say) is RECORDED on the report as a failed earmark and its
//     credit CLAWED BACK to the treasury — a nation does not pay for a survey
//     that did not start.
//
// An UNEARMARKED transfer (none exists yet; BL-538's other lines will make
// them) is folded onto `report.budgets[corp].subsidies` so the corp's `net()`
// explains the credit. An earmark is NOT: it leaves the balance where it found
// it, so it lives on `report.earmarks` instead — the line BL-555 ("who is
// paying me, on what") renders.
//
// CADENCE KEY. `econ_tick` is `world::current_econ_tick` — the quarter counter,
// never the day tick (BL-568: the day tick is 90n in the app and only ever
// rotates half the slots).
//
// NO HUMAN SUBJECT. This pass runs regardless of `corp_ai_params::spectating`.
// A subsidy landing on the player's corp is a transfer TO the player, not an
// action ON the player's corp, so the standing prohibition has nothing to
// protect here (NR-569d). (The player corp is never a CLAIMANT this cut — corp_ai
// never scores it — so in a played session it is never funded either.)
//
// Deterministic: sorted walks throughout; the only floats that move are the
// treasury, the corp balance and the survey cost, each in fixed order.

/// What the earmark dispatch did for one paid transfer — reported, not stored.
struct earmark_result
{
    entity_id corp    = null_entity;
    entity_id nation  = null_entity;
    entity_id subject = null_entity; ///< The body the survey was of.
    float     credits = 0.0f;        ///< The transfer that funded it.
    bool      dispatched = false;    ///< False: credit clawed back to the treasury.
};

/// Run the nation step for one economy tick. Mutates nation treasuries, corp
/// balances, the survey store (via `dispatch_survey`) and `w.nation_budgets`;
/// nothing else. Appends to `report.national_budget`, `report.nation_scores`,
/// `report.earmarks` and `report.budgets[corp].subsidies`.
void run_nation_step(world& w, const recipe_registry& reg, economy_report& report,
                     int econ_tick);

// ---------------------------------------------------------------------------
// BL-571 — garrison upkeep, the `military_research` line's first consumer
// ---------------------------------------------------------------------------
// "Upkeep is a budget claim, not the corp vector" (MILITARY.md § Nation
// garrisons) — but a `budget_claim` (nation_budget.hpp) is structurally a
// request that a NAMED CORPORATION be paid; a garrison has no corp on the
// other end, so it cannot be one. This pass instead RECOMPUTES the same
// share formula `run_national_budget` uses (spendable = treasury x
// (1 - reserve); share(L) = spendable x weight(L) / sum(weights)) for the
// `military_research` line alone, and spends it as a direct expenditure —
// debited from the treasury, credited to nobody, exactly the credit-wage
// precedent `budget_system.cpp`'s corp `upkeep` term already sets (soldiers'
// wages leave the modelled economy; they are not a transfer between two
// entities the world tracks).
//
// The BILL reuses the SAME authored table a corp unit's credit wage does
// (`recipe_registry::military().upkeep`, unit_roster.hpp's
// `resolve_unit_upkeep(u, p).credits`) — "there is no second mechanism"
// (MILITARY.md): one upkeep table, two payers. Every rate defaults to zero,
// so this pass is INERT at the shipped rates, matching the corp path.
//
// SHORTFALL reads exactly like `run_unit_upkeep`'s own decay rule: unmet
// (paid < bill) decays every one of that nation's garrison units'
// `supply_factor_permille` by `supply_decay_permille`; met (bill fully paid,
// or no bill at all) recovers by `supply_recovery_permille`. One bill per
// nation, not one draw per unit — a garrison's upkeep is a single credit
// figure, not a per-unit goods draw, so there is nothing to distribute
// per-unit the way `distribute_losses` has to.
//
// Runs from `run_nation_step`, after `run_national_budget` (step 3) — the
// SAME treasury and the SAME `w.nation_budgets` weights a claim on any other
// line would read this tick, just without going through the claim/transfer
// machinery a corp-payee line needs.
//
// DETERMINISTIC: nations walked in ascending id (`std::map`, built from the
// unordered `w.units`/`w.nations` and sorted first); each nation's garrison
// unit ids ascending.
void run_nation_garrison_upkeep(world& w, const recipe_registry& reg);
