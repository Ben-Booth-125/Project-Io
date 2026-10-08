#pragma once

// ---------------------------------------------------------------------------
// building_state — THE running-state classification of one building (BL-1239,
// tile production section).
// ---------------------------------------------------------------------------
// SELECTION.md § The tile element's layout (the Production section) and
// § The building element's layout (the Status page): a building is Running,
// Understaffed, Idle or Mothballed, or Under construction with its ETA, and an
// idle or understaffed one SAYS WHY — labour short, or an input short, naming
// the input and its cause.
//
// ONE FUNCTION, THREE READERS. The building hover card (hover_content.cpp), the
// tile element's Production section and the building card's Status page
// (selection_panel.cpp) all classify through `classify_building_running` and word
// it through `running_state_text`, so no two surfaces can disagree about whether
// a building runs or why it does not. The classification used to live inline in
// `hover_building_detail` as a workforce-fraction heuristic; it now reads the
// economy tick's own verdict (`economy_report`'s per-building row: idle,
// exhausted, run fraction, limiting input) where one exists, and falls back to
// that heuristic only where no tick has reported the building yet.
//
// READ-ONLY over world state. Nothing here is a simulation rule: the input-short
// CAUSE re-reads the same two public facts the production tick tested (the
// market's shelf, and the fair-price ceiling `shelf_admits`), it does not decide
// anything the tick did not.

#include "world/components.hpp"
#include "world/entity.hpp"

#include <imgui.h>
#include <string>

struct world;
struct economy_report;
class recipe_registry;

namespace ui {

/// The five running states, in the words SELECTION.md uses.
enum class building_run_kind : uint8_t
{
    running,
    understaffed,
    idle,
    mothballed,
    under_construction,
};

/// Why an idle or understaffed building is not running full.
enum class building_run_reason : uint8_t
{
    none,          ///< Running full (or under construction / mothballed — no reason owed).
    labour_short,  ///< Too little workforce reaches it.
    input_short,   ///< A processor's input is short; `input` names it.
    no_deposit,    ///< An extraction site on ground with none of its target.
    deposit_spent, ///< An extraction site whose reserve is exhausted.
    no_method,     ///< A processing facility with no recipe assigned.
};

/// Why an input is short, where the world exposes it cheaply.
enum class input_short_cause : uint8_t
{
    unknown,          ///< Not derivable from the shelf and the ceiling alone.
    none_on_shelf,    ///< The market's shelf holds none of it.
    over_ceiling,     ///< Posted over the fair-price ceiling, so the draw did not buy.
};

struct building_running_state
{
    building_run_kind   kind   = building_run_kind::running;
    building_run_reason reason = building_run_reason::none;
    resource_type       input  = resource_type::iron_ore; ///< Meaningful for input_short only.
    input_short_cause   input_cause = input_short_cause::unknown;
    int                 eta_ticks   = 0;     ///< Under construction: ticks remaining.
    bool                reported    = false; ///< An economy tick has a row for this building.
    float               output      = 0.0f;  ///< Units credited this tick (reported only).
};

/// Classify building @p id's running state.
///
/// @param w       Read-only world state.
/// @param reg     Recipe registry — the fair-price ceiling multiple; may be null
///                (the input cause then reads only the shelf).
/// @param report  This tick's economy report; may be null (the hover path before
///                a tick, a hand-built world) — the classification then falls
///                back to the building's workforce fraction.
/// @param id      The building.
/// @param b       Its component.
building_running_state classify_building_running(const world& w, const recipe_registry* reg,
                                                 const economy_report* report, entity_id id,
                                                 const building_component& b);

/// The state word alone: "Running", "Understaffed", "Idle", "Mothballed",
/// "Under construction".
const char* running_state_word(building_run_kind k);

/// The full line, in the Production section's words — "Running",
/// "Idle — input short: Steel, over the fair-price ceiling",
/// "Under construction — 4 ticks".
std::string running_state_text(const building_running_state& s);

/// Just the reason, or "" when none is owed — for a table cell beside the word.
std::string running_state_reason(const building_running_state& s);

/// Text colour for the state: running reads positive, idle negative,
/// understaffed / under construction amber, mothballed muted.
ImU32 running_state_colour(building_run_kind k);

} // namespace ui
