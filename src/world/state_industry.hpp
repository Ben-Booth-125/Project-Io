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
#include "world/campaign_settle.hpp" // k_campaign_settle_ticks: the reserve counts the settle (BL-1386)

#include <array>
#include <cstdint>
#include <map>
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
    /// The founding's world-history entry (`world::history_log` index), so the
    /// first-tick reserve can say so when it leaves the price short.
    std::size_t history_index = static_cast<std::size_t>(-1);
};

/// A coastal seat's founded port, as paid (parallel to the plant above).
struct state_port_founding
{
    entity_id nation   = null_entity;
    entity_id building = null_entity;
    float     price    = 0.0f;
    float     paid     = 0.0f;
    std::size_t history_index = static_cast<std::size_t>(-1);
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
    /// Sprint 53 (Ben, 2026-10-14): ports a major with a coastal capital took
    /// as state industry at the fold, and majors whose coastal seat found no
    /// ground for one.
    int ports_founded = 0, ports_unplaced = 0;
    std::vector<entity_id> port_buildings; ///< the ports founded, in nation order
    std::vector<state_port_founding> ports; ///< the same ports with their price and payment
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
/// inputs at a full run, and its upkeep goods basket, every good at its market's
/// POSTED price (the price every shelf draw this tick is billed at). A site
/// under construction is bounded by its opex alone: its credits were paid at
/// placement and its materials are unpaid (Ben, 2026-10-26). Never
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

/// THE FIRST TICK'S STATE BILL — what the funding gate commits @p nation to
/// when it funds the whole of its state industry: every state building's
/// `state_building_cost_bound`, plus its track's upkeep, each with the gate's
/// headroom. Read-only; the same terms `plan_state_funding` sums.
float state_first_tick_bill(const world& w, const recipe_registry& reg, entity_id nation);

/// The state buildings the funding gate would IDLE if it ran on @p w now —
/// `plan_state_funding`'s building walk, read-only (no ledger, no flag
/// written). For a reading that must count only what will run.
std::vector<entity_id> state_gate_would_idle(const world& w, const recipe_registry& reg);

/// BL-1382 economy follow-up (Ben, 2026-10-19; NATION_GENERATION.md § Pass 7:
/// "No major leaves generation on a zero treasury").
///
/// THE FOUNDING PAYS FROM WHAT THE STATE DOES NOT NEED TO RUN. The fold's
/// foundings (the plant, the coastal port) are paid from the treasury, and a
/// treasury short of the price "pays what it holds and the shortfall is
/// reported, never minted". Ben's ruling narrows what it holds: a founding
/// leaves the settle and A YEAR of the state bill in the chest, counted from
/// the opening (Ben, 2026-10-19: twelve settle ticks plus four,
/// `k_state_founding_reserve_ticks`), so the foundings may not take the
/// treasury below sixteen times the first tick's state bill
/// (`state_first_tick_bill`) — its hubs among it, which is why this runs after
/// the Logistic Point seeding, once every state building stands. Where they
/// did, the foundings' payments are reduced — the port's (paid last) first,
/// then the plant's — by as much as the bill needs and never more than was
/// paid; the reduction is shortfall, reported and written into the founding's
/// history entry exactly as a short treasury's always was. Nothing is minted:
/// the treasury never exceeds the chest it held before the foundings, and a
/// major whose history left it no chest at all (nothing paid) is untouched.
/// The reserve a founding leaves in the chest, COUNTED FROM THE OPENING
/// (NATION_GENERATION.md § Pass 7, Ben 2026-10-19): the settle the finish runs
/// before the player's first tick (`k_campaign_settle_ticks`, campaign_settle.hpp
/// -- read from there, never a second literal) plus a year, four quarterly
/// ticks of the state bill. Twelve plus four: sixteen ticks.
inline constexpr int k_state_founding_year_ticks     = 4;
inline constexpr int k_state_founding_reserve_ticks  = k_campaign_settle_ticks + k_state_founding_year_ticks;

struct state_reserve_report
{
    int   nations_reserved = 0;  ///< majors whose foundings were reduced
    int   nations_short    = 0;  ///< of them, still under the year's bill (their chest is smaller)
    float returned         = 0.0f; ///< payment reduced, summed (credits)
    int   minted           = 0;    ///< BL-1386 review: nations left above the chest they held before the foundings (0 by construction)
};
state_reserve_report reserve_state_first_tick(world& w, const recipe_registry& reg,
                                              state_handover_report& handover);

/// Print the reserve's one `[state_reserve]` log line.
void print_state_reserve(const state_reserve_report& r);

/// BL-1398 (Ben, 2026-10-22; NATION_GENERATION.md § Pass 7, "No capital starts
/// dark"): THE FIRMS ON A NATION'S GROUND FUND ITS OPENING NODE BILL. Every nation
/// holding network nodes (hubs, stations, ports - a minor's too, NATIONS.md § State
/// industry) whose treasury is short of `k_state_founding_reserve_ticks` of its
/// NODES' share of the bill - which the funding gate pays first - is topped up to it by
/// the corporations holding buildings on its ground. Each
/// firm pays its share by the buildings it holds there, and never more than its
/// balance; what one cannot pay is asked again of the others. A transfer, never a
/// mint: what a treasury gains, balances lose. A nation no firm stands on, or whose
/// firms cannot pay, stays short and is reported. Runs at the fold after the
/// founding reserve (`reserve_state_first_tick`) and before the reading at the gate.
struct state_node_fund_report
{
    int   nations_short         = 0;    ///< nations short of the opening node bill
    int   nations_funded        = 0;    ///< of them, paid anything by firms
    int   nations_still_short   = 0;    ///< of them, still short after the firms paid
    int   nations_without_firms = 0;    ///< of them, no corporation holds a building on their ground
    float paid                  = 0.0f; ///< credits moved from firms to treasuries
    int   minted                = 0;    ///< transfers that left more than they moved (0 by construction)
};
state_node_fund_report fund_state_nodes_from_firms(world& w, const recipe_registry& reg);

/// Print the step's one `[node_fund]` log line.
void print_state_node_fund(const state_node_fund_report& r);

/// BL-1402 -- what the fold to ground did.
struct fold_report
{
    int   corporations       = 0;    ///< corporations walked (ascending id)
    int   buildings_folded   = 0;    ///< buildings that joined their ground nation's state_assets
    int   on_unowned         = 0;    ///< buildings on ground no nation owns: demolished (Own ground)
    int   rehomed            = 0;    ///< nation buildings off their owner's ground, passed to the ground's nation
    int   firm_folded        = 0;    ///< of buildings_folded, those a corporation held (all of them)
    int   firm_on_unowned    = 0;    ///< of on_unowned, those a corporation held
    int   routes_merged      = 0;    ///< duplicate (body pair, nation) routes merged after re-owning
    float reserve_dropped    = 0.0f; ///< trade reserves not carried (0 on a generated start; see the body)
    int   units_reowned      = 0;    ///< units passed to their corporation's home nation
    int   rail_reowned       = 0;    ///< track tiles passed to their ground nation
    int   trades_reowned     = 0;    ///< standing trades passed to the owner's home nation
    int   convoys_reowned    = 0;    ///< convoys in flight passed likewise
    int   routes_reowned     = 0;    ///< trade routes passed likewise
    int   contracts_cleared  = 0;    ///< procurement quotes and contracts retired (CONTRACTS.md)
    float balance_moved      = 0.0f; ///< credits moved from balances into home treasuries
    float debt_forgiven      = 0.0f; ///< negative balances not carried: a nation never runs into debt
    float balance_stranded   = 0.0f; ///< credits of corporations with no home nation (moved nowhere)
    /// BL-1407: generation creates no corporation; the fold CLOSES the charters
    /// (`world::gen_charters`). Their buildings are already their ground nation's
    /// — the close only orders each nation's list as the old fold appended it,
    /// demolishes a charter building on ground no nation owns, and credits each
    /// charter's capital to its home treasury (counted in `balance_moved` etc.).
    int   charters_closed    = 0;    ///< charter records closed (ascending id)
    int   charter_unowned    = 0;    ///< of on_unowned, those a charter placed
};

/// BL-1402 THE FOLD TO GROUND (Ben, 2026-10-23; NATIONS.md § What a nation owns,
/// § Own ground). Every corporation's every building leaves its `assets` and joins
/// the `state_assets` of the nation whose ground it stands on, walked in (corp
/// id, asset order) -- the order the funding gate later spends in. A building on
/// ground no nation owns is demolished (a nation builds only on its own ground).
/// Each corporation's positive balance moves into its HOME nation's treasury (a
/// transfer; a negative one is forgiven and reported, since a nation never runs
/// into debt), its trade reserve with it. Its units, standing trades, convoys in
/// flight and trade routes pass to its home nation; its track to the track's ground
/// nation. Its HQ and border are cleared. Procurement quotes and contracts are
/// cleared (named-supplier procurement is retired). The corporation records stay,
/// empty -- the seat's handle until BL-1407 deletes the type. Idempotent: a second
/// fold moves nothing. Deterministic: sorted walks only. Runs at the end of the
/// campaign finish, after the rail handover and the Logistic Point seeding (both
/// may stand corporation-owned nodes) and before the opening reserve.
fold_report fold_to_ground(world& w);

/// BL-1407 — A PRE-v56 STREAM'S FIRM, read into scratch by the save loader and
/// folded away (`fold_legacy_firms`); never world state. The fields the fold
/// reads, and nothing else of the retired corporation record.
struct legacy_firm
{
    entity_id              home_nation   = null_entity;
    float                  balance       = 0.0f;
    float                  trade_reserve = 0.0f;
    bool                   is_background = false;
    std::vector<entity_id> assets;        ///< its buildings, in its own stored order
};

/// BL-1407: the fold's rules over @p firms (ascending id): each firm's buildings
/// to the nation whose ground they stand on (ground no nation owns: demolished),
/// its positive balance and its trade reserve to its home treasury, its units,
/// track, trades, convoys and routes to its home nation; then every nation
/// building off its own ground re-homed, duplicate routes merged and the
/// procurement records cleared. With @p firms empty only the ground and
/// procurement rules run. Counts into @p out. Deterministic: sorted walks only.
void fold_firms(world& w, const std::map<entity_id, legacy_firm>& firms, fold_report& out);

/// BL-1407 — THE v54/v55 LOADER'S FOLD (R2): `fold_firms`, then the seat
/// (`player_entity` naming a firm becomes its home nation) and every store still
/// keyed by a firm id (stance pairs, embargo conditions, workforce overrides,
/// sentiment pairs) dropped. The caller discards @p firms afterwards.
fold_report fold_legacy_firms(world& w, const std::map<entity_id, legacy_firm>& firms);

/// Print the step's one `[fold]` log line.
void print_fold(const fold_report& r);


/// BL-1255 -- what the rail handover did at the fold.
struct rail_handover_report
{
    int links           = 0; ///< generated links read (world::gen_rail_links)
    int links_laid      = 0; ///< market-city pairs laid as track (spanning-tree edges)
    int links_merged    = 0; ///< lines lying inside one market's catchment (no track)
    int links_broken    = 0; ///< market cities a line could not reach over land (a strait)
    int track_tiles     = 0; ///< track tiles on the world
    int to_nations      = 0; ///< tiles handed to a major as state industry
    int to_firms        = 0; ///< tiles handed to a minor's background firm
    int along_the_line  = 0; ///< of them, owned by the owner nearest along the line (not the ground's)
    int unowned         = 0; ///< tiles no owner could reach (no owner on the body at all)
    int stations        = 0; ///< stations stood up
    int station_nations = 0; ///< of them, a nation's (BL-1404: the ground's, a minor's too)
    std::vector<std::pair<entity_id, int>> per_owner_tiles; ///< ascending owner id
};

/// RAIL OWNERSHIP (BL-1255; LOGISTICS.md sec 4c). Every track tile's owner is
/// what its GROUND names — the major holding it (state industry), or the minor
/// holding it's lowest-id background firm — else (ground no nation holds, or a
/// minor with no background firm; Ben 2026-10-14, "no track is ownerless") the
/// owner NEAREST ALONG THE LINE: a breadth-first walk over track from every
/// named tile, seeded in ascending tile id. A line touching no named ground
/// takes the owner whose ground lies nearest its tile. Re-read every tick by
/// the funding gate, so OWNERSHIP FOLLOWS TERRITORY when ground changes hands.
/// @return how many tiles changed owner (a change clears the rail network).
int assign_rail_owners(world& w);

/// THE RAIL HANDOVER (BL-1255; INDUSTRIALISATION.md sec Who lays rail: "the
/// track and stations pass to the nation at the fold as state industry (a
/// major's) or to background firms (a minor's)"). First the Industrialisation
/// span's links (`world::gen_rail_links`), joined into lines, are LAID as the
/// spanning tree of the CITIES holding the market centres that serve the
/// line's seats (a rail must reach markets); a city across water is left
/// unjoined (the line breaks at the strait). Then every tile is
/// owned (`assign_rail_owners`), and a STATION stands, complete, on every city
/// tile touching track ("a station at each city a line serves"), ascending tile
/// id, owned by the track it touches. Generation holdings stand complete and
/// unpaid. Idempotent: a second call lays nothing and stands no second station.
rail_handover_report hand_over_rail(world& w);

/// Print the rail handover's one `[rail]` log line.
void print_rail_handover(const rail_handover_report& r);
