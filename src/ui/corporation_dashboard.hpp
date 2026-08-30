#pragma once

#include "charts.hpp" // stack_segment — the Balance card's two columns
#include "ui_state.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstddef>

// ---------------------------------------------------------------------------
// The Corporation ledger (BL-248, cut to one card by BL-691) — nav slot 1
//
// THE QUESTION IS "HOW WELL AM I DOING?", and that is why this surface holds
// one card rather than four. It shipped with four roll-ups — Production, Trade,
// Workforce, Finance — and three of them were answered better somewhere else:
// Production and Workforce by the Construction ledger's Buildings tab, which
// now holds the estate AND the per-building levers, and Trade by the Market and
// Convoys ledgers. What was left that only this surface can answer is how well
// the corporation is doing, and the honest answer to that today is money.
//
// So: ONE card, **Balance**, condensed to a two-column chart — earnings against
// stacked expenses. It is a sub-header inside the Corporation ledger, not a
// competing surface name (Ben, 2026-08-29). The overlap with the Budget ledger
// is explicitly accepted rather than tolerated, with Budget to be revisited.
//
// The chart itself is `ui::charts::draw_stacked_columns`, shared with the
// building card's Revenue/Expenses graph at the building grain. The corp's
// expense set is wider — inputs bought, maintenance, wages, interest, levies and
// force upkeep — and interest is charged only while the corp is in debt, so this
// stack has a VARIABLE segment count where the building's is fixed at three.
// That difference lives in the shared drawer, not in a fork of it.
//
// The card shows its chart AT REST: with one card there is nothing to accordion,
// so `disclosure_controls` is passed `in_place = false` and only the full-canvas
// `›` is drawn. The resting column used to be four verdict lines over ~700 px of
// empty column at 1920x1080; the chart is budgeted off the host's own remaining
// height, so that space is now the answer rather than the complaint.
//
// Every figure here is derived from the live world and the last economy report.
// ---------------------------------------------------------------------------

namespace ui {

/// What the Corporation ledger reports, derived once per frame.
///
/// Trimmed to the Balance card's own inputs: with Production, Trade and
/// Workforce gone, the per-building and per-lane walks that fed them went with
/// them rather than being left to run each frame for nothing.
struct corp_rollups
{
    float       balance = 0.0f;
    corp_budget budget{};   ///< Empty under the headless harnesses.

    /// True when the last economy report carried no budget breakdown — the card
    /// says so rather than charting zeroes as though they were measured.
    bool budget_measured = false;
};

/// The Balance card's two columns: what came in, and what went out.
///
/// Built once from the quarter's `corp_budget` and shared by the drawer and the
/// verify seam, so a check asserts the numbers that are actually charted rather
/// than a parallel sum of its own.
///
/// A ZERO FLOW IS ABSENT from the stack, which is what makes `expense_count`
/// meaningful: a solvent corp's stack is one segment shorter than an indebted
/// one's, because interest is charged only below zero
/// (`docs/ui/ledgers/balance.md` § Data sources records that the row count
/// changes across the solvency boundary).
struct balance_columns
{
    /// Income, plus national subsidies when a nation paid any this quarter.
    charts::stack_segment earnings[2]{};
    std::size_t           earning_count = 0;

    /// Every outflow `corp_budget::net()` subtracts, each its own segment.
    charts::stack_segment expenses[6]{};
    std::size_t           expense_count = 0;

    /// Sum of the drawn expense segments — the quarter's real expenditure.
    float expense_total() const
    {
        float t = 0.0f;
        for (std::size_t i = 0; i < expense_count; ++i)
            t += expenses[i].value;
        return t;
    }
};

/// Build the Balance card's columns from a measured quarter. Pure.
balance_columns build_balance_columns(const corp_budget& b);

/// How many cards the ledger draws. One; exposed so a check can assert that
/// rather than trusting a comment.
std::size_t corp_card_count();

/// Derive the ledger's figures for @p corp. Pure: reads, allocates, returns;
/// touches neither the world nor the UI state. Exposed so a headless harness can
/// check the arithmetic without an ImGui context.
corp_rollups derive_corp_rollups(const world& w, const recipe_registry& reg,
                                 const economy_report& report, entity_id corp);

/// Draw the Corporation ledger into the shell fold-out column, plus the
/// full-canvas takeover when one is open. The card draws its chart in both
/// hosts, from one builder, so the two cannot show different things.
///
/// @param w      World (read-only — the ledger reports, it does not operate).
/// @param reg    Loaded registry.
/// @param report Last economy step; supplies the budget flows.
/// @param s      UI state — the fold target.
/// @param open   The nav slot's flag; the ledger draws nothing while false.
void draw_corporation_dashboard(const world& w, const recipe_registry& reg,
                                const economy_report& report,
                                ui_state& s, bool& open);

} // namespace ui
