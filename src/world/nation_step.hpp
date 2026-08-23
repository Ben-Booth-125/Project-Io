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
