#pragma once

// ---------------------------------------------------------------------------
// Era -1 works (BL-321) — the pre-history building list.
// Authority: docs/lore/HISTORY.md. Data: scripts/works.lua.
// ---------------------------------------------------------------------------
//
// The noun axis of the Era -1 sim had exactly one half built: unit_roster.hpp
// says what a polity can FIELD. This says what it can BUILD. Availability is
// DERIVED — a row is offered when the province's ground and population clear
// its gate — with no research, no unlock events and no player choice.
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
// AUTHORED IN LUA (scripts/works.lua), per Ben's call 2026-08-07 (NR-081), and
// this header is the seam that keeps that free. It follows recipe_registry's
// shape exactly: THIS HEADER IS PURE DATA — no sol2, no Lua — and every lookup
// below is inline, so the SDL/Lua-free world superset and the headless
// harnesses link without the Lua-bound translation unit. Only
// works_registry.cpp pulls the Lua state.
//
// The consequence for the sim: `history_sim` takes a `works_registry` as a
// PARAMETER rather than calling a global. The app passes the Lua-loaded one; a
// harness hand-builds one. This is the same dependency-injection seam
// `clear_markets` already uses for recipe_registry, and it is what lets the
// table live in Lua without costing the Era -1 sim its headless verification.
//
// BANDS are unit_roster.hpp's `roster_band`, reused rather than duplicated: the
// two tables turn over at the same boundaries because they are gated on the
// same industrial history. Bands are CUMULATIVE — nothing un-invents a granary.

#include "unit_roster.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct province;

// Forward-declared so this header carries no sol2/Lua dependency — the same
// reason recipe_registry.hpp forward-declares it.
class lua_state;

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

/// One buildable work. `name` is a generic mechanism noun, never an Earth
/// proper noun — the loader does not police that (no code can), but works.lua
/// states the rule at the point of authoring.
struct work_row
{
    std::string name;
    roster_band band = roster_band::classical;
    work_gate   gate;
    work_effect effect;

    /// Relative weight when a polity scores which work to raise next. Not a
    /// count and not a cost — the pull this row exerts among the available set.
    int weight = 0;
};

/// The works table, loaded from scripts/works.lua.
///
/// Every accessor is inline and Lua-free; only `load_from_lua` lives in the
/// Lua-bound TU. A harness builds one with `add_row` and never links it.
class works_registry
{
public:
    /// Populate from scripts/works.lua via the embedded Lua state (protected
    /// calls only), VALIDATING as it goes.
    ///
    /// Validation is the loader's job rather than a harness's, because the data
    /// now lives outside the compiler's reach: an unknown band, a row with no
    /// effect at all, a duplicate name, or a reach-bearing row no ground can
    /// gate would otherwise produce a quietly degraded world. Failing at
    /// startup makes a bad edit loud and immediate.
    ///
    /// @throws std::runtime_error on a Lua error or an invalid table.
    void load_from_lua(lua_state& lua);

    /// The whole table, in authored order.
    const std::vector<work_row>& rows() const { return m_rows; }

    std::size_t size() const { return m_rows.size(); }

    /// The row at @p id, or nullptr when out of range.
    const work_row* row_at(std::size_t id) const
    {
        return id < m_rows.size() ? &m_rows[id] : nullptr;
    }

    /// Id of a row by name, or -1. Lets a caller name a work stably rather than
    /// hard-coding an index that authoring order can move.
    int id_of(const std::string& name) const
    {
        for (std::size_t i = 0; i < m_rows.size(); ++i)
            if (m_rows[i].name == name)
                return static_cast<int>(i);
        return -1;
    }

    /// The works @p p's ground and @p band make available, cheapest gate first.
    /// Two provinces at the same date offer different works because their ground
    /// differs — the asymmetry the roster pair exists to make visible.
    ///
    /// Bands are cumulative: a gunpowder-band province may still raise a Granary.
    std::vector<const work_row*> available(const province& p, roster_band band) const;

    /// Sum the effects of the works @p built (ids into `rows()`). Unknown ids
    /// are SKIPPED, not clamped: a record written against an older, shorter
    /// table shrinks gracefully rather than reading a neighbouring row's
    /// effects as if they were its own.
    work_effect total_effect(const std::vector<uint8_t>& built) const
    {
        work_effect t;
        for (const uint8_t id : built)
        {
            if (id >= m_rows.size()) continue;
            const work_effect& e = m_rows[id].effect;
            t.capacity_mod   += e.capacity_mod;
            t.manpower_mod   += e.manpower_mod;
            t.reach_mod      += e.reach_mod;
            t.defence_mod    += e.defence_mod;
            t.industrial_mod += e.industrial_mod;
        }
        return t;
    }

    // --- direct construction for tests (headless harness builds these by hand) ---
    void add_row(const work_row& r) { m_rows.push_back(r); }
    void clear() { m_rows.clear(); }

private:
    std::vector<work_row> m_rows;
};

/// True iff @p p clears @p g. Free function rather than a member so the gate
/// rule can be asserted directly by a harness without a registry.
bool work_gate_met(const work_gate& g, const province& p);
