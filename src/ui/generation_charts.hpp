#pragma once

#include "world/history_log.hpp"
#include "world/planetology.hpp"

#include <cstddef>
#include <string>

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

// ---------------------------------------------------------------------------
// At-a-glance heuristics (BL-209 / BL-211)
//
// A dated biography is the right surface for reading ONE world carefully. It is
// the wrong surface for judging a seed in two seconds, or for telling two seeds
// apart — which is what tuning the generator actually asks of a reader who is not
// a chemist. These three are that summary.
//
// All are pure functions of a planetology_state, same as the charts.
// ---------------------------------------------------------------------------

/// The chain spine — one compact row of gate outcomes, S5a..S5g then S6a..S6c,
/// as pass / marginal / fail / not-reached glyphs.
///
/// This row IS the causal chain: it says where the world stopped without needing
/// any of the chemistry to be understood. Outcomes are recovered from the
/// molecular trace (each event names its own gate), so the spine cannot disagree
/// with what was generated.
///
/// It also names any gate that came out MARGINAL. That is PLANETOLOGY.md's own
/// anti-rigging device — surfacing the margin is what makes a world that barely
/// passed read as lucky rather than as scripted, and "one roll from a Silent
/// Eden" is exactly the kind of thing that makes a seed interesting.
void draw_chain_spine(const planetology_state& st);

/// What this world does NOT have, as one line.
///
/// PLANETOLOGY.md § Presentation argues this harder than anything else: "print
/// the denial list, not just the grant... 'no subduction, therefore no porphyry
/// copper' is more useful than any positive number — it tells the player where to
/// go instead." For a reader judging a seed, absence is decisive, because absence
/// is what constrains play.
void draw_denial_list(const planetology_state& st);

/// One trace detail entry's citation payload, rendered for a human: outcome,
/// yield and venue.
///
/// Decodes the integer citation pairs (domain 0x01, chemistry) through the
/// compiled-in tables, so displayed names can never drift from the ids that were
/// actually generated — the invariant BL-209's id/table split exists to protect.
std::string citation_summary(const hist::entry& e);

} // namespace ui
