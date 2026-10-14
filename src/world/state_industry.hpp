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
///   transport   type port, inland_logistics_hub, railway_station (BL-1255,
///               LOGISTICS.md § 4c; the track itself is `world::rail_track`)
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

/// What a major with no strategic plant to inherit FOUNDED at the fold.
struct state_founding
{
    entity_id nation   = null_entity;
    entity_id building = null_entity; ///< null: nowhere to found one
    std::uint16_t recipe = 0;
    bool      chain_feasible = false; ///< its inputs are supplied in reach
    float     price    = 0.0f;        ///< the plant's capex
    float     paid     = 0.0f;        ///< what the treasury paid (<= price)
};

/// What the handover did, for the log and the harness.
struct state_handover_report
{
    int majors         = 0; ///< nations holding a champion when the handover ran
    int passed         = 0; ///< buildings that passed to a nation
    int held_back      = 0; ///< strategic buildings a background firm kept (`state_runs` false)
    int champion_kept  = 0; ///< strategic buildings on a major's ground a champion kept
    int firms_emptied  = 0; ///< background firms left holding no building at all
    std::array<int, 6> by_sector{}; ///< passed, by strategic_sector
    /// BL-1296 fix round (Ben, 2026-10-14): every major left with no state
    /// building after the handover FOUNDS one, ascending nation id.
    std::vector<state_founding> founded;
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
/// order). Idempotent: a second call finds nothing left to pass.
///
/// THEN EVERY MAJOR STILL HOLDING NONE FOUNDS ONE (Ben, 2026-10-14; NATIONS.md
/// § State industry, "Every major founds one"), ascending nation id. The plant
/// is the FIRST strategic recipe of a fixed list — steel, steel_bessemer,
/// refined_fuel, coal_power_plant, oil_power_plant, ordnance,
/// propellant_atmospheric — on the FIRST tile of the nation's own ground, in
/// its territory order (grown out from its seed, so nearest the heartland
/// first), where it can stand (every gate a state build asks, the champion's
/// tree) AND its inputs are supplied in reach (`recipe_inputs_supplied` —
/// chain-feasible). Only where no pair is chain-feasible does the first
/// placeable pair stand instead (`chain_feasible` false, reported). The plant
/// stands COMPLETE, as every generation holding does, and is paid for from
/// the treasury at its capex (`construction_capex`); a treasury short of the
/// price pays what it holds and the shortfall is reported, never minted —
/// the state charter's rule. Recorded in the world history log as a state
/// charter. A second call founds nothing (every major already holds one).
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

/// BL-1296 fix round — A CHANGE TO A FUNDED STATE BUILDING MUST BE FUNDED TOO.
/// The gate funded each state building at the cost bound of its state THEN; a
/// later change this tick (a resume, a raised workforce target, a retool) can
/// raise what the tick will bill. Before such a change is applied, its
/// increase — `state_building_cost_bound(after) - (before)`, with the gate's
/// headroom — must fit in @p nation's uncommitted treasury; when it does, it
/// is COMMITTED (added to this tick's ledger), so a second change cannot spend
/// the same credits. A building the gate idled this tick runs nothing whatever
/// changes, so any change to it passes free; so does a change that lowers the
/// bound. Returns false — and commits nothing — when the treasury cannot carry
/// it: the caller must then refuse the change.
bool state_commit_change(world& w, const recipe_registry& reg, entity_id nation,
                         const building_component& before, const building_component& after);


/// BL-1255 -- what the rail handover did at the fold.
struct rail_handover_report
{
    int track_tiles     = 0; ///< track tiles on the world
    int to_nations      = 0; ///< tiles handed to a major as state industry
    int to_firms        = 0; ///< tiles handed to a minor's background firm
    int unowned         = 0; ///< tiles on no nation's ground, or a minor with no background firm
    int stations        = 0; ///< stations stood up
    int station_nations = 0; ///< of them, a major's
    std::vector<std::pair<entity_id, int>> per_owner_tiles; ///< ascending owner id
};

/// THE RAIL HANDOVER (BL-1255; INDUSTRIALISATION.md sec Who lays rail: "the
/// track and stations pass to the nation at the fold as state industry (a
/// major's) or to background firms (a minor's)"). Every UNOWNED track tile
/// passes to the nation whose ground it stands on when that nation is a major,
/// else to that minor's lowest-id background firm; ground no nation holds, or
/// a minor with no background firm, keeps it unowned (no upkeep; it carries
/// traffic all the same). Then a STATION stands, complete, on every city tile
/// touching track ("a station at each city a line serves"), ascending tile id,
/// owned by the same rule (no owner: no station). Generation holdings stand
/// complete and unpaid, as every generation holding does. Idempotent: owned
/// track and a city already holding a station are skipped.
rail_handover_report hand_over_rail(world& w);

/// Print the rail handover's one `[rail]` log line.
void print_rail_handover(const rail_handover_report& r);
