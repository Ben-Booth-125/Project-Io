#pragma once

#include "world/planetology.hpp"

#include <cstddef>

// ---------------------------------------------------------------------------
// Generation stage charts — the chain's visual half, drawn from one place
//
// These plots were born inside app.cpp's staged generation wizard
// (draw_generation_screen) and were therefore thrown away the moment the world
// finished generating: the richest visuals in the game were visible only on the
// screen the player clicks through once. This module is the extraction that
// gives them a second home — the History ledger (BL-211) redraws exactly the
// same charts from the persisted generation_report, stage by stage.
//
// The charts are a pure function of the per-body planetology_state, so both
// callers hand in the same shape and get the same picture by construction
// rather than by imitation. Nothing here reads app or world state.
//
// Design authority: docs/generation/GENERATION_LEDGER.md, backlog.json BL-211.
// ---------------------------------------------------------------------------

namespace ui {

/// One body's contribution to a stage chart.
struct generation_chart_body
{
    const char*              name    = "";      ///< Column label and legend text.
    const planetology_state* state   = nullptr; ///< The chain's output for this body.

    /// The same body with the industrial drawdown dialled to zero — the "formed"
    /// reference the S9 Spend chart's hollow columns need. Optional: when null,
    /// Spend charts what is left and says so rather than inventing a before.
    const planetology_state* undrawn = nullptr;
};

/// Everything a stage chart draws from: the bodies, and which one is home.
///
/// Single-body charts (endowment, arable share, the iron/coal trade) are always
/// about the homeworld — it is the world the player inherits, and the only body
/// S9 runs on at all.
struct generation_chart_source
{
    const generation_chart_body* bodies = nullptr;
    std::size_t                  count  = 0;
    std::size_t                  home   = 0; ///< Index into `bodies`. Clamped by the drawer.
};

/// One round of the chain: a thematic run of contiguous stages, charted together.
///
/// The wizard paces the player through these one at a time; the History ledger
/// uses the same three as its tab strip, so a player who read the chain at
/// generation finds it filed the way they learned it.
struct chain_round
{
    const char* name;     ///< The tab / header label.
    const char* question; ///< What the round settles, in one line.
    chain_stage first;    ///< Inclusive.
    chain_stage last;     ///< Inclusive.
};

inline constexpr int chain_round_count = 3;

/// The A / B / C grouping — the chain's own shape. Caller clamps the index.
const chain_round& chain_round_at(int r);

/// The paragraph shown at each stage: the actual chemistry, in the plainest
/// words that stay true. A player should leave knowing WHY the gates sit where
/// they do, not just which bar moved.
const char* stage_explainer(chain_stage s);

/// Draw one chain stage's charts into the current ImGui window.
///
/// Lays out two charts per row when the region is wide enough and one when it is
/// not, so the same call reads in the wizard's 1180px panel and in the shell
/// column's ~330px fold-out. Emits, in order: the stage explainer, which bodies
/// died at this gate, then the stage's plots.
///
/// @param src     Bodies to chart. Drawing is a no-op when empty.
/// @param s       Which stage. `chain_stage::count` and unknown values draw nothing.
/// @param heading When true, prefixes a `SeparatorText` naming the stage and the
///                question it answers — the wizard wants it, the ledger does not
///                (its collapsing header already carries the name).
void draw_stage_charts(const generation_chart_source& src, chain_stage s, bool heading);

} // namespace ui
