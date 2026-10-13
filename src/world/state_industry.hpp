#pragma once
// ---------------------------------------------------------------------------
// state_industry — buildings a major nation owns outright (BL-1296)
// ---------------------------------------------------------------------------
// docs/politics/NATIONS.md § State industry (Ben, 2026-10-11/13) owns the
// design; GLOSSARY.md § State industry / Private industry owns the words. This
// file holds the three pieces of it that are not an owner-kind branch inside an
// existing pass:
//
//   1. THE SECTOR MAPPING — which buildings are strategic, in ONE place
//      (`strategic_sector_of`), and which of those the state runs in this cut
//      (`state_runs`).
//   2. THE HANDOVER at the fold (`hand_over_state_industry`): the strategic
//      buildings background firms placed on a major's ground pass to that
//      nation; the firms keep everything else; a champion keeps all its own.
//   3. THE FUNDING GATE (`plan_state_funding`): each tick, before anything is
//      charged, a state building whose costs the treasury cannot cover IDLES
//      for the tick. The state never runs into debt.
//
// The money itself is not here: a state building's sales land in the treasury
// through clear_markets' landings (paid to any owner with an asset list) and its
// costs are settled by apply_budget's state pass (budget_system.cpp) — market
// flows, NATIONS.md § 2's second exception, never transfers.

#include "entity.hpp"
#include "components.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

struct world;
class recipe_registry;

/// The strategic sectors (NATIONS.md § State industry: "energy, steel, arms,
/// transport infrastructure, and space"). `none` is private industry.
enum class strategic_sector : std::uint8_t
{
    none      = 0,
    energy    = 1,
    steel     = 2,
    arms      = 3,
    transport = 4,
    space     = 5,
};

/// Display / log name of a sector ("energy", ...; "private" for none).
const char* strategic_sector_name(strategic_sector s);

/// THE SECTOR MAPPING — the one place "strategic" is defined. By building TYPE
/// for the infrastructure the type itself names, by RECIPE OUTPUT for a
/// processing facility (a recipe IS the named building, PRODUCTION.md):
///
///   transport   type port, inland_logistics_hub (rail and stations join when
///               they exist as buildings — LOGISTICS.md § 4c)
///   space       type launchpad; a processor making spacecraft_components or
///               propellant
///   energy      a processor making power or refined_fuel
///   steel       a processor making steel
///   arms        a processor making ordnance
///
/// Everything else — every extraction site, every other recipe, every other
/// type — is private (`none`). A processor with no recipe is private.
strategic_sector strategic_sector_of(const recipe_registry& reg, building_type type,
                                     std::uint16_t recipe);

/// As above, for a standing building.
strategic_sector strategic_sector_of(const recipe_registry& reg, const building_component& b);

/// Whether the state RUNS a strategic building of this shape in this cut — the
/// set the handover passes and the state build verb admits. True for a
/// strategic PROCESSING facility. False for the strategic infrastructure types
/// (port, inland logistics hub, launchpad): those carry corporation-only
/// machinery (a port makes its owner's trade points; a launchpad gates its
/// owner's space convoys; a hub anchors its owner's reach), and the state owns
/// no trades or convoys, so handing one over would silently switch it off. They
/// stay with their firms until that machinery has a nation owner (reported as
/// `held_back` by the handover). False for anything private.
bool state_runs(const recipe_registry& reg, building_type type, std::uint16_t recipe);

/// What the handover did, for the log and the harness.
struct state_handover_report
{
    int majors         = 0; ///< nations holding a champion when the handover ran
    int passed         = 0; ///< buildings that passed to a nation
    int held_back      = 0; ///< strategic buildings a background firm kept (`state_runs` false)
    int champion_kept  = 0; ///< strategic buildings on a major's ground a champion kept
    int firms_emptied  = 0; ///< background firms left holding no building at all
    std::array<int, 6> by_sector{}; ///< passed, by strategic_sector
    /// Per major, ascending nation id: (nation, buildings it now owns).
    std::vector<std::pair<entity_id, int>> per_nation;
};

/// THE HANDOVER (NATIONS.md § State industry, "Handed over at the fold", Ben
/// 2026-10-13). For every building a BACKGROUND firm owns that stands on a
/// MAJOR nation's ground (`tile_to_nation`) and that the state runs
/// (`state_runs`), the building leaves the firm's `assets` and joins that
/// nation's `state_assets`. A champion's buildings never pass, whatever their
/// sector; a minor's ground hands nothing over. A firm whose HQ passed is
/// re-seated on its lowest-id remaining building on the same body (or none).
///
/// Deterministic: firms in ascending id, each firm's assets in stored order;
/// a nation's `state_assets` therefore lists its buildings in (firm id, stored
/// order). Idempotent: a second call finds nothing left to pass. Mutates only
/// ownership lists and HQ pointers — no money moves, no building changes.
state_handover_report hand_over_state_industry(world& w, const recipe_registry& reg);

/// Print the handover's one `[state_industry]` log line.
void print_state_handover(const state_handover_report& r);

/// The most @p b can cost its owner this tick, in credits: maintenance and
/// wages at full labour (`compute_building_opex` at grant 1), its recipe's
/// inputs at a full run, its upkeep goods basket, and — under construction —
/// a full-rate tick of flat cost and materials, every good at its market's
/// POSTED price (the price every shelf draw this tick is billed at). Never
/// below what the tick actually charges: the gate funds against it.
float state_building_cost_bound(const world& w, const recipe_registry& reg,
                                const building_component& b);

/// THE FUNDING GATE. Each nation holding state industry, ascending id, walks
/// its `state_assets` in stored order: a building whose cost bound fits in what
/// is left of the treasury is FUNDED (its bound comes off what is left); one
/// that does not fit IDLES this tick (`building_component::state_idle`). Opens
/// this tick's `world::state_ledger` rows. Called at the top of
/// `run_economy_step`, before anything this tick charges an owner.
void plan_state_funding(world& w, const recipe_registry& reg);

/// What @p nation's treasury may spend on a command right now: the treasury
/// less what this tick's funded state buildings are committed to. Never
/// negative.
float state_uncommitted_treasury(const world& w, entity_id nation);
