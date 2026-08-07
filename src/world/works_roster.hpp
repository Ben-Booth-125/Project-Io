#pragma once

// ---------------------------------------------------------------------------
// Era -1 works (BL-321) — the pre-history building list, as an authored TABLE.
// Authority: docs/lore/HISTORY.md.
// ---------------------------------------------------------------------------
//
// The noun axis of the Era -1 sim had exactly one half built: unit_roster.hpp
// says what a polity can FIELD. This says what it can BUILD. Same shape, same
// rules — an authored table, gated on the province endowment windows the
// settlement pass already surveyed, availability DERIVED from the ground with
// no research, no unlock events and no player choice.
//
// THIS IS NOT `building_component`. components.hpp's building is the campaign
// era's: an entity on a tile carrying a recipe index, a workforce target and
// auto-solver flag, credit and per-resource build cost, per-tick maintenance
// and wages, decommission state and construction ticks. Era -1 has no credits,
// no market, no recipe registry and no economy tick — it has a year loop over
// provinces. Sharing that struct would drag the whole campaign economy into the
// generation layer to satisfy fields nothing sets. A work is a small id held by
// the PROVINCE, and its effects are per-mille deltas on quantities the sim
// already computes.
//
// AUTHORED IN C++, NOT LUA — deliberately, and against the letter of
// TECH_FOUNDATIONS.md's scripting boundary (which names "building definitions"
// as a Lua concern, and which the campaign era honours: building_economics from
// scripts/economy.lua, recipes from scripts/recipes.lua). The reason is the
// headless build. recipe_registry.cpp is the one TU in the project that pulls
// sol2/Lua; every other world/*.cpp stays Lua-free so the verify harnesses
// compile with a bare C++20 compiler, and the Era -1 sim is verified only by
// those harnesses. A Lua works table would either drag Lua into the headless
// layer or need a second loader for the same data. unit_roster.cpp set this
// precedent in this layer already. Recorded as a decision-taken in
// NEEDS_REVIEW.json (NR-081) rather than silently.
//
// BANDS are unit_roster.hpp's `roster_band`, reused rather than duplicated: the
// two tables turn over at the same boundaries because they are gated on the
// same industrial history. Bands are CUMULATIVE — nothing un-invents a granary.

#include "unit_roster.hpp"

#include <cstdint>
#include <vector>

struct province;

/// What a work needs from the ground before a polity can raise it. The four
/// endowment axes are thresholds on the province windows (0-1000); zero means
/// "no gate". `population` is an absolute headcount floor — a granary wants
/// people before it wants grain.
struct work_gate
{
    int     ore_q      = 0;
    int     farm_q     = 0;
    int     port_q     = 0;
    int     energy_q   = 0;
    int64_t population = 0;
};

/// What a work DOES, as per-mille deltas on quantities the sim already has. No
/// new simulation concepts: every field here names something that exists.
struct work_effect
{
    /// Demographic carrying capacity — BL-273's logistic ceiling for this province.
    int capacity_mod = 0;
    /// The manpower ceiling: what this province can raise.
    int manpower_mod = 0;
    /// Discount on the terrain-weighted supply cost through/from this province.
    /// THE LOAD-BEARING FIELD. BL-316 charges a polity for breadth; without a
    /// work to buy, the only answer to that charge is to stop expanding. This
    /// is the counter-move that makes the frontier stall a decision.
    int reach_mod = 0;
    /// Modifier this province contributes when DEFENDING, resolved through
    /// combat.cpp's existing path. Never a new combat concept.
    int defence_mod = 0;
    /// Pull-forward on the Stage 4 furnace date.
    int industrial_mod = 0;
};

/// One buildable work.
struct work_row
{
    const char* name;   ///< Generic mechanism noun. Never an Earth proper noun.
    roster_band band;
    work_gate   gate;
    work_effect effect;

    /// Relative weight when a polity scores which work to raise next. Not a
    /// count and not a cost — the pull this row exerts among the available set.
    int weight;
};

/// The whole table, in band order. Exposed so a harness can assert over it
/// rather than re-deriving what it thinks the table says.
const std::vector<work_row>& works_table();

/// The works @p p's ground and @p band make available, cheapest gate first.
/// Two provinces at the same date offer different works because their ground
/// differs — the same asymmetry the unit roster exists to make visible.
///
/// Bands are cumulative: a gunpowder-band province may still raise a Granary.
std::vector<const work_row*> available_works(const province& p, roster_band band);

/// Index of @p row in `works_table()`, i.e. the id a province stores. Returns
/// -1 for a row that is not from the table.
int work_id(const work_row* row);

/// Sum the effects of the works @p built (ids into `works_table()`). Unknown
/// ids are skipped rather than clamped, so a save from an older table shrinks
/// gracefully instead of reading garbage. Effects are additive in per-mille;
/// the sim decides how to apply each field.
work_effect total_work_effect(const std::vector<uint8_t>& built);
