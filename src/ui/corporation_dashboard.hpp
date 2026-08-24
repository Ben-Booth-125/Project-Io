#pragma once

#include "ui_state.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// The Corporation dashboard (BL-248) — nav slot 1
//
// MENU.md named this slot in 2026-06-15 and it has stood unbuilt since, with the
// slot drawing an all-corporations balance table instead — a comparison surface,
// not "the player corporation at a glance", and a duplicate of the Economy
// panel's own Corps view.
//
// It is four roll-up cards — Production, Trade, Workforce, Finance — on the fold
// model (BL-214): each rests as ONE verdict line and expands to a full-screen view
// carrying its chart and a per-item drill.
//
// The four drills are deliberately four different SHAPES, not one generic detail
// panel: a building's operating state, a lane's traffic, a building's labour, the
// budget's five flows. That difference is the point — a roll-up whose drill tells
// you nothing the roll-up did not is a roll-up that did not need one.
//
// Every figure here is derived from the live world and the last economy report.
// The exemplar this item was designed against (build_UI_example_3) carried
// invented numbers to prototype the interaction; the interaction transferred, the
// numbers did not.
// ---------------------------------------------------------------------------

namespace ui {

/// A drillable row inside a roll-up: what it is called, the magnitude that ranks
/// it, and the entity the drill resolves against.
struct rollup_item
{
    entity_id   subject = null_entity; ///< Building (Production/Workforce) or body (Trade).
    entity_id   second  = null_entity; ///< Trade only: the lane's other endpoint.
    std::string label;
    float       value   = 0.0f;        ///< The charted magnitude.
    bool        flagged = false;       ///< Idle / suspended / below the labour floor.
};

/// The four roll-ups, derived once per frame from the world and the last report.
struct corp_rollups
{
    // --- Production: what is the corporation actually making? ---
    int   producing = 0;
    int   idle      = 0;
    int   exhausted = 0;
    float output_per_tick = 0.0f;
    std::vector<rollup_item> production; ///< Per producing/idle building, output desc.

    // --- Trade: where is it moving goods, and how much did that earn? ---
    int   routes  = 0;
    int   convoys = 0;
    float bought  = 0.0f; ///< Auto-bought input value this tick (expenditure side).
    float sold    = 0.0f; ///< Market income this tick.
    std::vector<rollup_item> trade;      ///< Per lane, cumulative convoys desc.

    // --- Workforce: is labour the thing holding production back? ---
    float tightest_contention = 1.0f;    ///< The worst (corp, body) labour scalar.
    std::vector<rollup_item> workforce;  ///< Per building, effective labour asc (worst first).

    // --- Finance: is the corporation making money? ---
    float       balance = 0.0f;
    corp_budget budget{};                ///< Empty under the headless harnesses.

    /// True when the last economy report carried no budget breakdown — the Finance
    /// card says so rather than drawing five zeroes as though they were measured.
    bool budget_measured = false;

    // --- Growth track: how far has this corp's production reached? (BL-591) ---
    // Lives on the Production card, per Ben's ruling — a corporation-grain fact
    // (`corp_reached_depth` reads `corporation_component::produced_ever`), not a
    // per-building one, which is why the 2026-08-15 playtest rework was right to
    // cut it from the building card even though the idea itself was not the
    // problem. See PRODUCTION.md § Chain depth — the growth track.
    int reached_depth = 0; ///< 0 for a fresh corp — it has reached the raws and nothing beyond.

    /// The display name of a good at `reached_depth` the corp has actually
    /// produced, or empty for a fresh corp that has produced nothing at all
    /// (`reached_depth` is then trivially 0 with no good to name).
    std::string reached_good;

    /// Display names of the buildings whose `recipe_required_depth` is exactly
    /// `reached_depth + 1` — comma-joined, or empty when nothing in the
    /// era-allowed roster sits at that rung (the corp has already climbed the
    /// era's whole ladder).
    std::string next_rung;

    /// Display names of the goods those next-rung recipes need that the corp has
    /// never produced — comma-joined, or empty when `next_rung` is empty. This is
    /// the line that turns a lock into an instruction: not just "you can't build
    /// this yet" but "build a Charcoal Burner first".
    std::string missing_inputs;
};

/// Derive the four roll-ups for @p corp. Pure: reads, allocates, returns; touches
/// neither the world nor the UI state. Exposed so a headless harness can check the
/// arithmetic without an ImGui context.
corp_rollups derive_corp_rollups(const world& w, const recipe_registry& reg,
                                 const economy_report& report, entity_id corp);

/// Draw the Corporation dashboard into the shell fold-out column, plus the
/// full-canvas takeover when one is open. In the column each card rests as a
/// verdict line and may be expanded in place; the takeover shows all four
/// roll-ups scrolled (BL-265), with `expanded.key` scoping the drill.
///
/// @param w      World (read-only — the dashboard reports, it does not operate).
/// @param reg    Loaded registry; the Production drill prices the building through
///               the shared profitability builder rather than re-deriving it.
/// @param report Last economy step; supplies output, labour and the budget flows.
/// @param s      UI state — the fold target and the roll-up drill.
/// @param open   The nav slot's flag; the dashboard draws nothing while false.
void draw_corporation_dashboard(const world& w, const recipe_registry& reg,
                                const economy_report& report,
                                ui_state& s, bool& open);

} // namespace ui
