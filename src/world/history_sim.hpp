#pragma once

// ---------------------------------------------------------------------------
// The Era -1 history sim — the year-tick loop and the campaign scorer.
// BL-277 (Era -1 military strategy) + BL-271's first running slice.
// Authority: docs/lore/HISTORY.md; design prose in backlog.json BL-277.
// ---------------------------------------------------------------------------
//
// WHAT THIS IS. `run_settlement` (BL-218) simulates settlement ->
// industrialisation -> war ONCE, at generation time, then freezes the result.
// Below the industrial era it generates the world AT its stop year and leaves
// later regions unfounded, with the comment "the year-tick sim founds them".
// This file is that sim: a YEAR tick from a start year to a stop year in which
// polities settle, campaign, invest and consolidate, so wars and expansion
// RECUR rather than firing once as checkpoints.
//
// THE SCORER'S SHAPE mirrors the corp-AI stage-A layer (corp_ai.hpp): a pure
// function over a BOUNDED candidate set, integer-scored, argmax above a
// threshold. That is the sanctioned deterministic-agency idiom in this
// codebase, and reusing it is deliberate — the transfer contract in BL-271
// says what graduates to the 1960 era is the ARCHITECTURE, never the
// constants, so the architecture had better be the one already in use.
//
// INTEGER FIXED-POINT THROUGHOUT. Every rate and weight is a `_q` quantity in
// thousandths (1000 = 1.0). No floats anywhere in the decision path, and no
// transcendentals — the standing determinism rule (io-standing-rules
// § Determinism & data model) applies to `world/*` without exception, and this
// loop feeds region ownership, which every later pass reads.
//
// NO RNG. Scoring is pure. `seed` below does not drive a generator; it
// deterministically perturbs each polity's weights so polities do not all
// behave identically, and it breaks ties. Same inputs -> same history, which
// is what the harness's determinism assertion checks and what makes the
// checkpoint/branch model (BL-217) meaningful.
//
// SEASON IS AN AXIS OF THE ACTION, NOT A PHASE OF THE CLOCK (Ben, 2026-08-04,
// amending BL-271's original "season flag on the tick"). `combat.cpp`'s
// `season_attrition_multiplier` returns 1500 for winter and 1000 for every
// other season — the engine distinguishes two seasons, not four, so a
// four-tick clock would pay 4x the ticks to express a distinction it cannot
// make. The tick is therefore a plain year, and "campaign in winter" is an
// extra scored CANDIDATE: it carries the engine's 1.5x attrition unchanged
// plus a caller-side defender-readiness penalty applied through
// `army_stack_entry::type_power_mod`. combat.{hpp,cpp} is NOT modified.
//
// TERRITORY MOVES AT PROVINCE GRANULARITY, NEVER TILE. A won campaign
// reassigns `region::nation` and nothing else. That keeps the per-year
// ownership ring small enough to be the History Log time-lapse substrate
// (~82 regions x 2000 years x 2 bytes), which a tile-granular border model
// would not be.

#include "combat.hpp"
#include "creeds.hpp"
#include "settlement.hpp"
#include "works_roster.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Terrain view
// ---------------------------------------------------------------------------

/// A narrow, read-only terrain lookup in raster order (row * gw + col), the
/// only thing this sim needs the tile map for: pricing a battle through
/// `resolve_battle`'s terrain scalars.
///
/// Deliberately NOT `const world&` + tile_ids the way `run_settlement` takes
/// them. The sim's whole decision path is region-level, so binding it to the
/// ECS would buy nothing and cost headless testability. Both pointers may be
/// null, in which case every battle is fought on the neutral default terrain —
/// legal, and what the harness's synthetic cases use.
struct sim_terrain_view
{
    /// The three tile axes (BL-519), as parallel raster arrays. `cover` and
    /// `density` may be null independently of `substrate` — a caller that only
    /// has geology still gets defensible ground, just bare ground.
    const std::vector<terrain_substrate>* substrate = nullptr;
    const std::vector<terrain_cover>*     cover     = nullptr;
    const std::vector<std::uint8_t>*      density   = nullptr;
    const std::vector<terrain_landform>*  landform  = nullptr;
};

// ---------------------------------------------------------------------------
// The stepped decision clock (Ben, 2026-08-12)
// ---------------------------------------------------------------------------

/// One rung of the stepped clock: this band covers years **below**
/// `until_year`, and inside it polities take a decision round every
/// `step_years` years.
///
/// THE STEP GATES DECISIONS, NOT TIME. Demography still advances one real year
/// at a time in every band — population does not stop growing between a
/// polity's choices, and making it do so would have compressed 4000 years of
/// growth into 136 years of it. What the band changes is how often the scorer
/// runs, which is both the expensive part of the loop and the part that
/// deserves to be coarse in deep prehistory and fine near the epoch.
///
/// The consequence for the tunables below: a quantity that is a **rate** (per
/// year) must be multiplied by the step when it is applied, and a quantity that
/// is an **event** (a battle is lost, a region changes hands) must not.
/// `run_history_sim` scales exactly three rates — tech progress, cohesion
/// recovery and contest decay — and nothing else.
struct sim_tick_band
{
    int64_t until_year = 0; ///< This band covers years < until_year.
    int     step_years = 1; ///< Years between decision rounds inside it.
};

/// Ceiling on the band table. Fixed-size rather than a vector so
/// `history_sim_params` stays trivially copyable and allocation-free.
inline constexpr int sim_tick_band_max = 8;

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------

/// Scorer weights and loop bounds. Defaults are PLACEHOLDERS, not tuned
/// values: BL-277 records that the `w_*` weights are BL-275 sweep outputs
/// rather than authored constants. They are set here to plausible magnitudes
/// so the loop runs and the harness can bind assertions to its behaviour.
struct history_sim_params
{
    /// THE RUN IS 4000 BCE -> 0 CE (Ben, 2026-08-12), not 0 CE -> 1960.
    /// The campaign epoch moved to 0 CE with the ancient refocus (NR-177), so
    /// this loop's job changed from "run the run-up to an industrial start" to
    /// "run the run-up to an ancient one" — and 4000 BCE is where a settled,
    /// agrarian world with something to fight over begins.
    int64_t start_year = -4000; ///< Calendar year the run begins (4000 BCE).
    int64_t stop_year  = 0;     ///< Calendar year the run ends (the campaign epoch).

    /// The stepped decision clock, coarsest first. Bands must be sorted
    /// ascending by `until_year`; a year at or past the last band's
    /// `until_year` falls back to the last band's step.
    ///
    /// The default ladder is Ben's (2026-08-12): 100 -> 50 -> 20 -> 10 -> 5 -> 1,
    /// with boundaries chosen so resolution concentrates near the epoch. It
    /// costs **136 decision rounds** across 4000 years against 4000 on a flat
    /// tick — cheaper AND finer where it matters, which is the whole point.
    sim_tick_band tick_bands[sim_tick_band_max] = {
        {-2000, 100}, {-1000, 50}, {-400, 20}, {-100, 10}, {-20, 5}, {0, 1}
    };
    int tick_band_count = 6; ///< Live entries in `tick_bands`.

    // --- Objective selection (BL-277 Q1) ----------------------------------
    int w_farm = 300; ///< Weight on a target region's farm endowment.
    int w_ore  = 250; ///< Weight on its ore endowment.
    int w_port = 200; ///< Weight on its port endowment.
    int w_ring = 400; ///< Weight on ENCLOSED-SEA RING CLOSURE — see `ring_closure_q`.
    int w_dist = 120; ///< Penalty per tile of supply distance from the capital.
    int w_def  = 500; ///< Penalty on the defender's fielded power.
    int w_cult = 150; ///< Penalty for taking ground of a foreign culture.

    // --- Force commitment (BL-277 Q2) -------------------------------------
    /// Power lost per tile between the STAGING HOLDING and the objective, in
    /// per-mille of full supply. Local cost only — bounded by
    /// `neighbour_radius`, so it does not scale with the map.
    ///
    /// THIS TERM ALONE DOES NOT STALL A FRONTIER, and the harness records it
    /// rather than asserting it away: with the score's distance preference
    /// zeroed, a region 34 tiles out was still taken despite every campaign
    /// arriving under-supplied. The reason is upstream in combat.cpp — supply
    /// only mitigates ATTRITION, and attrition is terrain x season, so on plains
    /// the entire span from fully supplied to totally cut off is 10% of combat
    /// power. That is noise.
    ///
    /// The stall is therefore the THREE terms together, which is what BL-316
    /// added: this local decay, `terrain_reach_cost_q` (so the 10% span is only
    /// the plains case — a mountain battle carries up to 900/1000 attrition and
    /// supply matters enormously), and `holdings_burden_q` (so breadth costs
    /// something at all). See `campaign_supply` in the .cpp, which is the one
    /// place all three are priced.
    int supply_decay_per_tile_q = 28;

    /// Fraction of a region's banked manpower a campaign may raise.
    int levy_fraction_q = 400;

    // --- Season as an action axis -----------------------------------------
    /// Caller-side readiness penalty applied to a WINTER defender's power,
    /// per-mille. The attacker's own winter cost comes from the engine's
    /// attrition multiplier, which this file does not touch.
    int winter_readiness_penalty_q = 250;
    /// Extra score a winter campaign must clear to be chosen over waiting for
    /// summer — surprise is worth something, but not unconditionally.
    int winter_score_premium_q = 90;

    // --- Thresholds -------------------------------------------------------
    //
    // THE VERBS ARE SCORED ON THEIR OWN SCALES AND COMPARED ON A SHARED ONE
    // (BL-309). Each verb produces a raw score, a threshold it must clear, and
    // a ceiling that raw score is measured against; the argmax then runs over
    // the NORMALISED margin `(raw - threshold) / (ceiling - threshold)`.
    //
    // The first cut compared raw scores directly and it did not work. Invest's
    // raw score was population/4000 clamped to 1000, and region population is
    // hard-capped near 505,000 and only grows — so a few mature regions pinned
    // Invest at 1000 forever, while Campaign could not arithmetically exceed
    // ~750 and Settle ~600. The result was a simulation whose last ownership
    // change happened at median year 458 of a 0-1960 run: three quarters of
    // every run was inert, and every distribution the sweep reported described
    // a ~460-year world.
    //
    // Capping Invest would have hidden that rather than fixed it. The error was
    // comparing incommensurable quantities, so the fix is to make them
    // commensurable.
    // All four are now MINIMA IN THE SHARED CURRENCY — expected annual gain in
    // endowment value held — rather than cut-offs on four private scales.
    int campaign_threshold_q  = 40;
    int settle_threshold_q    = 60;
    int invest_threshold_q    = 50;
    int consolidate_threshold_q = 30;

    /// Share of a target's endowment value a conqueror expects to keep.
    int campaign_gain_q = 700;
    /// Cost charged per per-mille of missing supply, in the shared currency.
    int campaign_supply_cost_q = 260;
    /// Capacity yield as a fraction of holdings value, and its payback years.
    int invest_yield_q        = 90;
    int invest_amortise_years = 12;
    /// Divisor turning holdings-value-at-risk into a comparable annual figure.
    int consolidate_divisor = 24;

    // THE NORMALISED-MARGIN CUT WAS ATTEMPTED AND REVERTED, 2026-08-04, and the
    // reason is worth keeping: dividing each verb by its own range structurally
    // favours the verb with the NARROWEST range. Settle's usable span is 220
    // wide and Campaign's ~540, so equal relative desirability gave Settle the
    // larger margin — the run went from Invest-dominated to Settle-dominated
    // (82 -> 1532 regions, 1450 foundings, conquests ZERO). Same failure,
    // different verb. Rescaling incommensurable numbers cannot make them
    // commensurable.
    //
    // WHAT LANDED INSTEAD (BL-318): the verbs are scored on one scale BY
    // CONSTRUCTION. `region_value_q` in the .cpp is the common unit — a
    // region is worth the mean of its three endowment windows — and every
    // verb answers the same question in it: what is this worth to me this year,
    // in regions-worth-of-endowment. Campaign values the ground taken times
    // the odds of taking it, less supply; Settle values the daughter region
    // times the pressure driving it; Invest values holdings times yield over the
    // payback period; Consolidate values holdings at risk times the cohesion
    // shortfall. The argmax is then an honest comparison rather than a
    // coincidence of scales.
    //
    // THE COROLLARY, and the trap this item kept falling into: a term that is
    // NOT in the currency must not be SUBTRACTED from something that is. Both
    // `w_cult` and `w_dist` were flat subtractions from a region value of only
    // ~200-300, so each acted as a veto rather than a preference — measured,
    // w_cult 150 gave 0 battles and w_cult 0 gave 266. Both are now
    // PROPORTIONAL (a per-mille discount on the prize), which says the same
    // thing at any region value and at any map size. Supply is priced ONCE,
    // by a single lambda both the scorer and the executed battle call, for the
    // same reason — see `campaign_supply` in the .cpp.

    /// Decisiveness (0-1000) a victory must reach before territory changes
    /// hands. Below it the battle is a raid: losses and contest, no transfer.
    int transfer_decisiveness_q = 300;

    /// Adjacency radius in tiles — two regions closer than this are
    /// neighbours, and only neighbours are campaign candidates.
    int neighbour_radius = 9;

    // --- Logistics (BL-314) -----------------------------------------------
    //
    // Ben, 2026-08-04: "it should definitely be tangibly harder to supply more
    // regions, and fight further away battles." Before this item neither was
    // true. Supply was straight-line Chebyshev distance feeding a term whose
    // whole dynamic range was 10% of combat power, and breadth cost nothing at
    // all — holding 500 regions cost exactly what holding 5 cost.

    /// Extra supply cost per tile of TERRAIN-WEIGHTED reach, on top of the
    /// per-tile decay. Mountains cost roughly twice what plains cost, using the
    /// same landform ratios logistics.cpp already defines for the 1960 era.
    int terrain_reach_cost_q = 10;

    /// THE BURDEN OF BREADTH. Supply lost per region held beyond
    /// `free_holdings`, in per-mille. An empire spread thin supplies every
    /// campaign worse, so expansion eventually pays for itself in reach — the
    /// arithmetic stall BL-277 Q2 claimed but never had.
    int holdings_burden_q = 4;
    /// Holdings a polity may carry before the burden begins to bite.
    int free_holdings = 40;
    /// Floor the burden may drag supply to, so a large empire is hampered
    /// rather than instantly unable to fight anywhere.
    int holdings_burden_floor_q = 250;

    /// Supply below which an arriving force counts as STALLED. This is what
    /// `history_sim_state::stalled_campaigns` measures — a campaign that was
    /// launched but arrived too thin to win, which is the frontier stalling.
    int stalled_supply_q = 300;

    /// Population pressure (population * 1000 / carrying capacity) above which
    /// a polity will consider founding a new region.
    int settle_pressure_q = 780;

    // --- Defeat compounds (BL-308) ----------------------------------------
    /// Cohesion lost when a region is taken from this polity.
    int cohesion_loss_on_defeat_q = 110;
    /// Cohesion regained per year of Consolidate — deliberately slower than the
    /// loss, so recovery costs several quiet years and a two-front collapse is
    /// not simply walked back.
    int cohesion_recovery_q = 14;
    /// Floor cohesion can fall to. Above zero so a dying polity still fights,
    /// badly, rather than becoming a free region with a flag on it.
    int cohesion_floor_q = 180;
    /// Cohesion below which a polity stops founding new regions. A state
    /// fighting for its life does not colonise, and letting it do so was the
    /// main reason losers regrew faster than they were conquered.
    int settle_cohesion_gate_q = 620;

    /// Fraction of a conquered region's population lost in the taking.
    /// The collapse path the first sweep had none of: population rose to
    /// carrying capacity by ~1300 CE and never fell again in any world.
    int sack_population_loss_q = 220;

    /// How much a region's accumulated `contest_q` lowers the decisiveness a
    /// victory needs before territory changes hands. A frontier ground down
    /// over centuries should eventually give, which a flat threshold never let
    /// it do — battles outnumbered conquests by up to 1000:1.
    int contest_transfer_relief_q = 400;

    // --- Works (BL-321) ---------------------------------------------------
    //
    // THE COUNTER-MOVE TO THE BURDEN OF BREADTH. BL-314 charges a polity supply
    // for every region past `free_holdings`, and before this item there was
    // nothing to buy with: the only answer to the charge was to stop expanding,
    // which makes the frontier stall a CEILING. Works make it a DECISION —
    // spend a round on a Way Station and the same breadth costs less.
    //
    // The verb is priced the way the other four are: not in a treasury Era -1
    // does not have, but in the ROUND it consumes. Building competes directly
    // with raising an army because both spend the polity's one action, which is
    // the whole point of putting it on the shared scale rather than beside it.
    //
    // EVERY WEIGHT BELOW IS A PLACEHOLDER, like the `w_*` weights above and for
    // the same reason: the magnitudes are authored by judgement to put works in
    // the same band as the other verbs, and calibrating them is BL-275's sweep,
    // not this file's.

    /// Per-mille value of each effect axis when scoring a candidate work. Reach
    /// is weighted highest because it is the axis with an emergent consequence
    /// (the stall) rather than merely a local one.
    int w_work_capacity   = 900;
    int w_work_manpower   = 700;
    int w_work_reach      = 1200;
    int w_work_defence    = 600;
    int w_work_industrial = 500;

    /// Payback horizon turning a work's permanent benefit into the annual
    /// figure the shared currency is denominated in.
    int work_amortise_years = 4;

    /// Minimum in the shared currency, like the four thresholds above.
    /// Deliberately LOW: a Way Station on poor ground is a marginal choice and
    /// should be able to win a quiet round, which is exactly the round a young
    /// polity has and a fighting one does not.
    int work_threshold_q = 8;

    /// Ceiling on the reach discount a single supply hub may apply to the
    /// terrain-weighted cost, and on the relief a polity's mean reach may apply
    /// to the burden of breadth. Below 1000 so no amount of building makes
    /// distance or breadth free — works buy a discount, never an exemption,
    /// which is what keeps BL-224's non-hegemony emergent.
    int work_reach_relief_cap_q = 800;

    /// How many held regions a polity considers building on per round. Two:
    /// its capital, and one rotated deterministically through its holdings.
    /// Scoring every holding would be O(held x rows) inside a pass already
    /// costing ~23 s of a ~25 s world; rotating spreads works across the empire
    /// over a run without paying for a full scan every round.
    int work_candidate_regions = 2;

    // --- Great-power seed (BL-299) ----------------------------------------
    /// Seed two opposed majors: one preserving, one expansionist. Off by
    /// default so the ordinary sweep measures an unseeded world.
    bool seed_great_powers = false;
    /// Aggression assigned to the expansionist and preserving majors.
    int major_expansionist_aggression_q = 880;
    int major_preserving_aggression_q   = 260;

    // --- Instrumentation (BL-384) -----------------------------------------
    /// Record one `battle_trace` per battle into `history_sim_state`. OFF by
    /// default, and it changes nothing when on: no decision reads a trace field
    /// and no trace field feeds a draw, so a traced run and an untraced run
    /// agree in every other output. The harness asserts that rather than
    /// trusting this sentence.
    bool trace_battles = false;
};

// ---------------------------------------------------------------------------
// Actors
// ---------------------------------------------------------------------------

/// The ladder's seven domains, in `ancient_tech_ladder.json` order. A polity's
/// tech state is a per-domain CAPACITY BAND, not a scalar, so it reads as a
/// profile — the ladder's own § Shape requirement.
enum class sim_domain : uint8_t
{
    agriculture = 0,
    materials,
    energy,
    transport,
    institutions,
    military,
    medicine,
};

inline constexpr int sim_domain_count = 7;

/// One governing entity. At the antiquity start these are CULTURES, not
/// nations — `run_settlement` leaves `region::nation` at -1 until the
/// political pass runs, and a pre-national ladder (BL-221) is exactly a world
/// whose actors are peoples. The sim writes `region::nation` as it goes, so
/// by the stop year the political map is this loop's output.
struct polity
{
    int id      = -1; ///< Index into `history_sim_state::polities`.
    int culture = -1; ///< Index into `creed_state::cultures`.
    int capital = -1; ///< Region index; the origin supply decays from.

    /// Doctrine lean, 0-1000, from the culture's `aggression_q` (BL-277 Q5).
    int aggression_q = 0;

    /// Capacity band 1-6 per domain, indexed by `sim_domain`. Everyone starts
    /// at band 1 (classical) — the roster grouping in ANCIENT_TECH_LADDER.md
    /// § Shape puts a 0 CE start in the classical band alone.
    int capacity[sim_domain_count] = {1, 1, 1, 1, 1, 1, 1};

    /// Accumulated investment per domain; crossing a threshold raises the band.
    int progress_q[sim_domain_count] = {0, 0, 0, 0, 0, 0, 0};

    /// COHESION, 0-1000 (BL-308). The polity-level term that makes defeat
    /// compound instead of merely accumulating.
    ///
    /// Without it the first sweep produced elimination in 0 of 12 worlds:
    /// losing a region cost a polity that region's manpower and nothing
    /// else, so its remaining regions defended exactly as well as before and
    /// no defeat ever led to another. Cohesion falls when ground is lost and
    /// multiplies the power of every stack the polity fields, so a losing
    /// polity gets easier to beat — which is what a death spiral is.
    ///
    /// HOW IT APPLIES: cohesion scales the readiness handed to the roster,
    /// which lands as an ADDITIVE per-mille offset on each unit's
    /// `type_power_mod` (unit_roster.cpp). It is not a multiplier on final
    /// stack power, and the first cut's comment claiming it was is corrected
    /// here (BL-312) — at the floor it is worth roughly -82 against row values
    /// of 90..380, so it tilts a fight rather than deciding one.
    ///
    /// It recovers on Consolidate, which is a SCORED candidate worth more the
    /// further cohesion has fallen (BL-309) — so the spiral is escapable by a
    /// polity that stops fighting and holds what it has. Under the first cut's
    /// raw-score comparison Consolidate was never chosen after ~year 176 and
    /// this escape did not exist.
    int cohesion_q = 1000;

    /// True for a seeded great power (BL-299). Majors start with more ground
    /// and an opposed strategic creed; the periphery stays alive as actors.
    bool major = false;

    bool alive = true; ///< False once the polity holds no regions.
};

/// What the scorer chose for one polity in one year — kept for the harness and
/// for the History Log, so a run can be read back as decisions rather than as
/// an opaque end state.
enum class sim_verb : uint8_t
{
    none = 0,
    settle,
    campaign,
    invest,
    consolidate,
    build_work, ///< Raise an Era -1 work on a held region (BL-321).
};

// ---------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------

/// One ownership change: region `region` came under polity `owner` in
/// year `year`.
///
/// `owner_none` is reserved for a region leaving all ownership, but NOTHING
/// EMITS IT TODAY: once settled or conquered a region always has an owner,
/// and no code path resets one to unowned. The value is kept because
/// `owner_slice_at` needs it for regions that do not exist yet in an early
/// year, and because depopulation-to-abandonment is a plausible later mechanic
/// — but a reader should not infer from the sentinel that abandonment exists
/// (BL-312).
///
/// This is the whole time-lapse format. It is a change LIST rather than a
/// per-year grid because ownership is overwhelmingly static — most years, on
/// most regions, nothing happens, and a dense grid pays for all of it.
struct owner_change
{
    int32_t  year     = 0;
    uint16_t region = 0;
    uint16_t owner    = 0;
};

/// The run's output. `owner_changes` is the time-lapse substrate; everything
/// else is either the political result or a counter the harness binds to.
/// ONE BATTLE, AS THE SCORER SAW IT AND AS IT ACTUALLY WENT (BL-384).
///
/// The gap this closes: a full 0->1960 run reports 267 battles and ZERO
/// conquests, deterministically, and no counter in this file can say WHY. The
/// item's own design forbids guessing — "confirm the mechanism before changing
/// a constant" — and the hypothesis it names is specific and testable: the
/// scorer estimates the odds from levy x supply x cohesion against the
/// defender's RAW manpower, with no terrain term and no works term, while
/// `resolve_battle` applies both to the defender only. If that is right, the
/// estimate is systematically optimistic by roughly the defence factor, and the
/// fix is to give the scorer the terms the resolver already uses. If it is not
/// right, tuning combat constants would be tuning the wrong thing.
///
/// PURE OBSERVATION. Populated only when `history_sim_params::trace_battles` is
/// set; nothing here is read to make a decision, and no field feeds an RNG
/// draw. A traced run and an untraced run are byte-identical in every other
/// output, which the harness asserts rather than assumes.
struct battle_trace
{
    int32_t  year        = 0;
    uint16_t attacker    = 0;  ///< Polity id.
    uint16_t defender    = 0;  ///< Polity id, or 0 where the region is unowned.
    uint16_t region      = 0;  ///< The target region index.

    // --- What the SCORER believed, at selection time ---------------------
    int p_win_q          = 0;  ///< Its estimated odds of winning, per-mille.
    int scored_hub       = -1; ///< The staging region it scored the odds against.
    bool winter          = false;

    // --- What the RESOLVER was actually handed ---------------------------
    int      exec_hub       = -1; ///< The staging region execution levied from.
    int64_t  attacker_men   = 0;
    int64_t  defender_men   = 0;
    int      attacker_supply_q = 0;
    int      defender_ready_q  = 0; ///< Winter penalty + works, as passed.
    int      works_defence_q   = 0; ///< The works half of that, alone.
    int      terrain_defence_q = 0; ///< The term the scorer never sees.

    // --- What happened ---------------------------------------------------
    bool attacker_won   = false;
    int  decisiveness   = 0;
    int  transfer_needed = 0;  ///< The bar decisiveness had to clear this time.
    bool conquered      = false;
};

struct history_sim_state
{
    std::vector<polity> polities;

    /// Per-battle observation, empty unless `params.trace_battles`. See
    /// `battle_trace` for why it exists and why it cannot move the run.
    std::vector<battle_trace> battle_traces;

    /// The History Log time-lapse substrate (Ben, 2026-08-04), DELTA-ENCODED:
    /// one record per ownership CHANGE, not per region per year.
    ///
    /// A dense year-major grid was the obvious first cut and it is the wrong
    /// one: the Settle verb grows the region count during a run (82 -> ~480
    /// over 2000 years on Kepler), so a dense ring costs stride x years and
    /// reached ~1.9 MB in practice. Almost every cell in it repeats the cell
    /// above. The change list is ~8 bytes per actual event and lands three
    /// orders of magnitude smaller, which is what makes recording ownership
    /// every year affordable at all.
    ///
    /// Replay is the natural time-lapse read: walk the list in order, applying
    /// each change, and you have the map at any year. `owner_slice_at` does
    /// exactly that for a caller that wants one year materialised.
    std::vector<owner_change> owner_changes;
    int      region_stride = 0; ///< Final region count (slice width for replay).
    int64_t  years           = 0; ///< Years simulated.
    int64_t  start_year      = 0; ///< First simulated year, for replay bounds.

    /// Dated lines for the world history log (BL-208) — foundings, battles,
    /// conquests. The sim narrates through the same `history_event` shape the
    /// generation passes use, so `bridge_generation_history` needs no new case.
    std::vector<history_event> history;

    // --- Counters, for the harness and BL-275's sweep metrics -------------
    int64_t battles     = 0;
    int64_t conquests   = 0;
    int64_t foundings   = 0;
    int64_t winter_campaigns = 0;
    /// Campaigns that reached their objective under `stalled_supply_q` supply
    /// — launched, but arriving too thin for the distance. The supply-decay
    /// stall, counted where it happens rather than after the battle resolved.
    int64_t stalled_campaigns = 0;
    /// Works raised over the run (BL-321). Counted because "did the roster fire
    /// at all" and "did it fire so much nothing else happened" are the two ways
    /// this item fails, and neither is visible in the battle/founding counts.
    int64_t works_raised = 0;

    /// Battles in each century of the run, index 0 = the first hundred years.
    /// The sweep reports war frequency PER CENTURY rather than as a total,
    /// because a world that fought all its wars in one age and a world that
    /// fought steadily for two millennia have the same total and nothing else
    /// in common — the total alone cannot tell them apart.
    std::vector<int32_t> battles_per_century;

    /// Highest total population the body ever carried, and the year it peaked.
    /// Peak has to be tracked as the run goes: the epoch figure alone cannot
    /// show a world that grew, collapsed and never recovered.
    int64_t peak_population = 0;
    int64_t peak_year       = 0;
};

/// Sentinel for "no polity owns this region in this year slice".
inline constexpr uint16_t owner_none = 0xFFFFu;

/// Hard cap on the region count, because `owner_change::region` is a
/// uint16_t and `owner_none` claims the top value. The Settle verb grows the
/// region vector without any natural bound, so this is the guard that stops
/// a long run silently wrapping the index and replaying a plausible-but-wrong
/// political map (BL-312).
inline constexpr std::size_t owner_index_limit = 0xFFFEu;

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

/// Run the year-tick history sim over @p ss, mutating it in place: region
/// ownership, population, manpower and culture all move, and new regions are
/// appended as the Settle verb founds them.
///
/// PURE AND DETERMINISTIC in the sense the harness checks: two runs from
/// identical (`ss`, `cs`, `terrain`, `params`, `seed`) inputs produce identical
/// output state and an identical `owner_ring`. There is no RNG, no clock, and
/// nothing read from global state.
///
/// @param ss      Settlement state to advance. Mutated.
/// @param cs      Creeds, for culture aggression -> doctrine (BL-277 Q5). May
///                be null, in which case every polity takes a neutral doctrine.
/// @param terrain Terrain view for battle pricing. Both members may be null.
/// @param gw      Grid width in tiles (columns WRAP — the map is a cylinder).
/// @param gh      Grid height in tiles (rows do not wrap).
/// @param params  Scorer weights and loop bounds.
/// @param seed    Deterministic weight perturbation and tie-break salt.
/// @param year_progress Optional progress sink: each simulated year stores
///                years-elapsed-so-far (1..span) with relaxed ordering, for a
///                loading screen polling from another thread. Null (the default,
///                and every headless caller) costs nothing and changes nothing —
///                the sim never reads it back, so determinism is untouched.
/// @param works   The Era -1 works table (BL-321), or null to run with works
///                DISABLED — no `build_work` candidate is scored and no region
///                gains one. Injected rather than fetched from a global for the
///                reason works_roster.hpp gives: the table is authored in Lua,
///                and a sim that reached for a Lua-loaded global could not be
///                verified by the headless harnesses that are its only
///                verification. The app passes its startup-loaded registry, a
///                harness hand-builds one, and a caller that does not care
///                passes nothing and pays nothing.
history_sim_state run_history_sim(settlement_state&         ss,
                                  const creed_state*        cs,
                                  const sim_terrain_view&   terrain,
                                  int                       gw,
                                  int                       gh,
                                  const history_sim_params& params,
                                  uint32_t                  seed,
                                  std::atomic<int>*         year_progress = nullptr,
                                  const works_registry*     works         = nullptr);

/// Tile distance between two regions on the cylinder — column difference
/// wraps, row difference does not. Exposed because the harness asserts the
/// supply-decay stall against it, and a test that recomputed distance its own
/// way would be testing its own arithmetic rather than the sim's.
int region_distance(const region& a, const region& b, int gw);

/// Years between decision rounds at calendar year @p y, read from @p p's band
/// table. Returns the first band whose `until_year` exceeds @p y, falling back
/// to the last live band; never returns less than 1.
///
/// Exposed because the harness binds to the band schedule directly — a test
/// that recomputed the ladder its own way would be testing its own arithmetic,
/// the same reason `region_distance` is public.
int step_for_year(const history_sim_params& p, int64_t y);

/// Bytes the time-lapse substrate occupies — the quantity the requirement
/// bounds, and the reason the encoding is a change list rather than a grid.
inline int64_t owner_ring_bytes(const history_sim_state& s)
{
    return static_cast<int64_t>(s.owner_changes.size()) * static_cast<int64_t>(sizeof(owner_change));
}

/// Materialise the ownership map as it stood at the END of @p year — the
/// time-lapse read. Returns `region_stride` entries, `owner_none` where the
/// region did not exist yet or was unowned.
///
/// Linear in the change list, which is the point: a whole 2000-year playback
/// is one forward walk, and a seek to a single year costs no more than the
/// changes preceding it.
std::vector<uint16_t> owner_slice_at(const history_sim_state& s, int64_t year);
