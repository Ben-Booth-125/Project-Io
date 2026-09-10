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
#include "era_timelapse.hpp"  // owner_change / owner_none / era_timelapse
#include "creeds.hpp"
#include "settlement.hpp"
#include "works_roster.hpp"

#include <array>
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

    /// Non-zero where `tile_component::river_edges` is non-zero (BL-853). A
    /// river is an EDGE on the tile, not a tile property of its own, so this
    /// is a one-byte-per-tile derived flag rather than a fifth terrain axis.
    /// May be null independently of the rest, in which case a caller (e.g.
    /// `run_colonisation`) treats every tile as riverless.
    const std::vector<std::uint8_t>*      river     = nullptr;
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

    // --- The two spans (BL-747) -------------------------------------------
    //
    // ONE engine, TWO spans: an ancient span to a boundary year, then an
    // industrial span from the boundary to the epoch with the higher roster
    // bands unlocked. Expressed as params on the SINGLE existing invocation
    // rather than as a second call to `run_history_sim` — see
    // era_minus_one.hpp for the six axes a second caller drifts on.

    /// The year the INDUSTRIAL span begins. Before it a polity may not reach
    /// past `span1_band_ceiling`; at or after it the ladder is unrestricted.
    /// The default is INT64_MIN — no year is before it, so the whole run is
    /// one unrestricted span and the struct default is exactly today's
    /// behaviour. A caller that sets neither field cannot change a world.
    int64_t boundary_year = INT64_MIN;

    /// The highest roster band reachable before `boundary_year`. `industrial`
    /// (the default) is no restriction at all, so the ceiling is inert twice
    /// over on a single-span run: no year is before the boundary, AND the
    /// clamp is the identity.
    roster_band span1_band_ceiling = roster_band::industrial;

    // --- Objective selection (BL-277 Q1) ----------------------------------
    int w_farm = 300; ///< Weight on a target region's farm endowment.
    int w_ore  = 250; ///< Weight on its ore endowment.
    int w_port = 200; ///< Weight on its port endowment.
    int w_ring = 400; ///< Weight on ENCLOSED-SEA RING CLOSURE — see `ring_closure_q`.
    int w_dist = 120; ///< Penalty per tile of supply distance from the capital.
    int w_def  = 500; ///< Penalty on the defender's fielded power.
    /// Penalty for taking ground of a foreign culture. BL-826 made it a
    /// question about the SHARES rather than an equality test — see
    /// `culture_shares` — so the discount fades as ground stops being foreign.
    int w_cult = 150;

    // --- Assimilation (BL-826) --------------------------------------------
    //
    // THE FIRST ANTI-HEGEMONY LEVER MADE REAL. Before this, a conquest replaced
    // the region's culture at the instant the border moved, so `w_cult` charged
    // a conqueror once and then never again: the second region of a foreign
    // people cost the same as the first, and a realm digested a continent for
    // free. Now foreign ground shifts toward its holder SLOWLY, so a conquest
    // is paid for over centuries — a wide realm carries a long tail of ground
    // that is still charging its owner the foreign-ground discount.
    //
    // A RATE, so the stepped clock multiplies it by the step (see § The stepped
    // decision clock). Proportional to the foreign remainder, so it never
    // completes: a conquered people is never arithmetically erased.
    //
    // 2/1000 a year leaves roughly 45% of a conquest still foreign after four
    // centuries and roughly 82% after one, which is the "long-held is digested,
    // recent is not" separation the harness binds to.
    int assimilation_per_year_q = 2;

    // --- Grudges (BL-827) --------------------------------------------------
    //
    // A grudge is a DIRECTED, SPARSE, DECAYING integer from one polity to
    // another, raised by NAMED EVENTS carrying a place and a date. It is an
    // INPUT TO SENTIMENT at world setup (docs/politics/RELATIONS.md § What each
    // quantity was before — "Era -1 grudges, with nowhere to live | seeded
    // nation->nation sentiment"), NOT a fifth quantity beside sentiment,
    // stance, reputation and standing. Nothing in this sim reads a grudge to
    // make a decision; it is a record, and the moment it became an input to the
    // scorer it would be an agent term rather than an in-world force.
    //
    // Magnitudes are PLACEHOLDERS on the same footing as the `w_*` weights.
    int grudge_ground_taken     = 300; ///< A region changed hands.
    int grudge_seat_sacked      = 700; ///< And it was the loser's capital.
    int grudge_border_raided    = 60;  ///< A battle that transferred nothing.
    int grudge_realm_ended      = 900; ///< The kin of an extinguished realm.

    /// Per-mille of the standing score shed PER YEAR. A rate, so the stepped
    /// clock multiplies it by the step.
    ///
    /// WITHOUT DECAY A 4000-YEAR RUN REACHES THE EPOCH WITH EVERY PAIR
    /// MAXIMALLY AGGRIEVED, which carries no information at all — every nation
    /// would seed identical sentiment and the whole record would be a constant.
    /// 3/1000 is a half-life near 230 years: a single old wrong is gone by the
    /// epoch, a running feud is not.
    int grudge_decay_per_year_q = 3;

    /// Ceiling on a single pair's score, so a millennium of border war does not
    /// run away past every other pair in the world.
    int grudge_cap = 10000;

    /// Scores at or below this are dropped from the sparse table entirely.
    /// Sparse is the point: most pairs never meet.
    int grudge_floor = 4;

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

    // --- The army pool (BL-835) -------------------------------------------
    //
    // ARMIES ARE DISTINCT FROM POPULATION (Ben, 2026-09-08). These three
    // per-mille dials are the whole of what the sim says about how militarised
    // an ancient polity is; the mechanics they drive are `muster_garrison` in
    // settlement.cpp, which never touches a civilian headcount.

    /// Per-mille of a region's recruitable manpower ceiling that stands as its
    /// GARRISON — the army the ground keeps under arms, and the force both
    /// sides field in a campaign over it.
    ///
    /// THE VALUE IS 400 BECAUSE THAT IS WHAT THE OLD MODEL COMMITTED. This
    /// field was `levy_fraction_q`, "the fraction of a region's banked manpower
    /// a campaign may raise", read straight out of `manpower_stock` at the
    /// moment of battle by both attacker and defender. Keeping the number puts
    /// the armies of the new model in the same headcount range as the armies
    /// of the old one, so what moved in the measurements below is the MODEL and
    /// not a silent recalibration riding along with it.
    int garrison_fraction_q = 400;

    /// Per-mille of the shortfall a region closes toward `garrison_fraction_q`
    /// each year. Matched to `demog_manpower_recover_q` (250) deliberately: an
    /// army destroyed is rebuilt at the same pace the pool behind it refills,
    /// so the two stages compound into a recovery measured in decades rather
    /// than in years. That is what makes losing an army expensive.
    int garrison_muster_q = 250;

    /// Per-mille of an OVER-strength garrison discharged per year — the case
    /// where the ground can no longer feed the host standing on it, after a
    /// plague or after a victorious army parks itself on a poor frontier.
    int garrison_disband_q = 300;

    /// THE EMERGENCY LEVY, per-mille: how much of its garrison shortfall a
    /// province under attack calls up in the year of the attack, over and above
    /// the peacetime muster. Read by BOTH the scorer's defence estimate and the
    /// battle itself.
    ///
    /// THE VALUE IS MEASURED, AND BOTH EXTREMES WERE MEASURED FIRST, because
    /// this dial turns out to decide whether the sim has wars at all. At 0 the
    /// defender rebuilds at the peacetime quarter-a-year while the attacker's
    /// survivors march home intact, so the second battle on any frontier is a
    /// walkover: the two-polity fixture fell in 2 battles and four of eight real
    /// seeds had every battle end in a conquest. At 1000 the province refills to
    /// full strength before every engagement and NEVER exhausts — the pool it
    /// draws from is larger than the garrison it fills — so the same fixture
    /// ground out 534 battles and took nothing at all.
    ///
    /// 500 is the half-measure and it is also the honest model: the near
    /// hinterland reaches the muster field within a campaign season and the far
    /// hinterland does not.
    ///
    /// AND IT SITS IN A BASIN, NOT ON A POINT — swept 2026-09-09 because Ben
    /// asked whether the value was calibrated or merely fitted, which is the
    /// right question to ask of any number derived from a fixture. Running
    /// `history_sim_harness` across the range, the criterion that moves is
    /// `S1a` ("terrain CHANGES the history — the terrain view is no longer
    /// inert"), and it is a good criterion: at either extreme terrain stops
    /// mattering to how the history comes out.
    ///
    ///     0    S1a FAILS      (defence never rebuilds; the frontier is a walkover)
    ///     250  passes, 4 fail (the four known failures, unchanged)
    ///     375  passes, 4 fail
    ///     500  passes, 4 fail (this value)
    ///     625  passes, 4 fail
    ///     750  S1a FAILS      (defence outruns any concentration an attacker can bring)
    ///     1000 S1a FAILS
    ///
    /// So anything from 250 to 625 behaves identically on every assertion in
    /// the harness and only the ends break. 500 is near the centre of that
    /// basin, which is what makes it a calibration rather than a fit — and it
    /// is also why re-deriving it is not worth anyone's time unless the basin
    /// itself moves. If a change makes this dial suddenly sharp, that is the
    /// signal something else has gone wrong.
    int defence_levy_q = 500;

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
    ///
    /// RE-PRICED BY MEASUREMENT (BL-767, history_sweep 8 seeds at --epoch 1960,
    /// 2026-09-06). At 90 over 12 years a capacity band was worth ~75 a year to
    /// a polity holding forty regions, against Settle at ~190 and Campaign at
    /// ~140 in the same currency — so Invest lost every round it was offered
    /// past band 2, and every world in the spread topped out at materials band
    /// 2 against an Industrial rung of 5. The pair says a permanent capacity
    /// band pays back over FIVE years rather than twelve, which is the honest
    /// reading of a benefit that never expires; measured, it puts three worlds
    /// in eight over the rung with the first furnace spread from 1560 to 1804.
    int invest_yield_q        = 260;
    int invest_amortise_years = 5;

    /// Accumulated progress one rung of the capacity ladder costs, MULTIPLIED
    /// by the band already held — so band 4 costs four times what band 1 did
    /// and capacity never runs away (ANCIENT_TECH_LADDER § diffusion).
    ///
    /// PROMOTED FROM A LITERAL (BL-767). It was `4000 * q.capacity[d]` inline
    /// in the Invest execution, which made the single most load-bearing
    /// quantity in the tech ladder the one thing in this file that could not be
    /// tuned as data. The value is unchanged; only its address is.
    int capacity_band_cost = 4000;

    /// WHERE A POLITY'S INVESTMENT GOES (BL-767). The Invest verb raises ONE
    /// domain a round, and these two weights decide which.
    ///
    /// THE DEFECT THEY EXIST TO CLOSE. The choice used to be "whichever domain
    /// sits at the lowest band", full stop — which levels all seven domains in
    /// LOCKSTEP and makes `capacity[]` a flat line rather than the PROFILE the
    /// ladder's own § Shape asks for. It also puts the Industrial rung out of
    /// arithmetic reach: crossing it in materials means dragging all seven
    /// domains to band 5, roughly seven times the investment of the one rung
    /// that matters, and measured (history_sweep, 4 seeds at --epoch 1960,
    /// 2026-09-06) every world in the spread topped out at materials band 2.
    /// No weight anywhere else could move that, because the ceiling was in the
    /// selection rule rather than in the price.
    ///
    /// `invest_level_pull_q` is the old rule as a force: how strongly a domain
    /// being BEHIND pulls investment toward it, per band of arrears.
    /// `invest_ground_pull_q` is the new one: how strongly the polity's own
    /// GROUND pulls, scored off the mean endowment of what it holds — farm to
    /// agriculture, ore to materials, energy to energy, port to transport, and
    /// nothing to institutions, military or medicine, which no window measures.
    ///
    /// The claim is HISTORY.md's Stage 4 hook read one stage earlier:
    /// endowment, not virtue. A people sitting on ore climbs the materials
    /// ladder because of the ore, and the furnace that eventually lights over
    /// it is the same fact read twice.
    ///
    /// AT `invest_ground_pull_q = 0` THE OLD RULE IS EXACTLY RECOVERED, which
    /// is what makes this a dial rather than a rewrite.
    ///
    /// THE DEFAULTS ARE MEASURED, NOT GUESSED. At `level_pull` 1000 a single
    /// band of arrears outweighs any ground signal (endowment windows top out
    /// at 1000, so the ground term cannot reach 1000 x 900/1000), and the run
    /// levels in lockstep exactly as it did — which is what the first trial
    /// measured. At 200/1000 the ground can outweigh roughly two bands of
    /// arrears at a mean endowment of 400, so a polity specialises where its
    /// ground argues and still catches a domain up once it falls far enough
    /// behind. Military, which no window measures, therefore advances on
    /// arrears alone and still reaches the medieval roster.
    int invest_level_pull_q  = 200;
    /// DEFAULT 0, AND THAT IS A MEASUREMENT OVERTURNING THE ITEM'S OWN
    /// DIAGNOSIS (cold review, 2026-09-06). BL-767 shipped this at 1000 on the
    /// reading that "the ceiling was in the selection rule rather than in the
    /// price". Swept over 16 seeds at epoch 1960, the opposite is true:
    ///
    ///     ground_pull     0    100   200   400   700  1000  3000  10000
    ///     worlds ind.   14/16   -     -     -     -   8/16    -     1/16
    ///
    /// monotone in the WRONG direction, with the shipped value among the worst.
    /// The PRICE change alone (invest_yield_q 260, invest_amortise_years 5) is
    /// what achieves R1 — 14 of 16 worlds industrialise with the selection rule
    /// left exactly as it was. The ground pull then re-breaks it.
    ///
    /// The dial is KEPT rather than deleted, because it is a real force and it
    /// may earn its keep once a capped domain stops being a sink (fixed
    /// separately) and once BL-757's zero-works finding moves. But it ships
    /// INERT, at the value that measures best, and the claim that 0 recovers the
    /// old argmin exactly was verified from the code rather than assumed.
    int invest_ground_pull_q = 0;
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
    /// CANDIDATE neighbours; `max_neighbour_degree` below decides which of
    /// them actually link.
    int neighbour_radius = 9;

    /// BL-855: THE HARD CAP THAT MAKES THE NEIGHBOUR GRAPH O(N) AGAIN. A radius
    /// alone is not a bound on degree — the map does not grow, but the regions
    /// filling it do, so a fixed-radius disc holds more and more of them as a
    /// run goes on. BL-844 gave `rebuild_reach` a heap (O(E log V) instead of
    /// O(N^2)), but E itself was densifying: measured at 0.017 -> 0.079 ms per
    /// rebuild from 533 to 1,174 regions (seed 0), fitted exponent ~2 across
    /// seeds — the heap fix could not reach a problem that lives in E, only the
    /// one that lived in the scan.
    ///
    /// A region links to at most this many of its nearest (by
    /// `region_distance`, ties broken on the lower region index — the same
    /// determinism discipline `rebuild_reach`'s heap comparator already uses)
    /// same-radius candidates, and only among candidates that themselves still
    /// have a free slot. THE VALUE IS MEASURED ON THIS BUILD TREE, not guessed:
    /// with degree left uncapped, mean degree at `neighbour_radius = 9` runs
    /// ~7-9 through the early game and climbs past 20 by 4,000 years as the map
    /// fills in (history_span_cost). 10 sits at the top of the early-game band,
    /// so the cap costs the sim almost nothing while the map is sparse — where
    /// most of a run's battles land — and only prunes the DENSIFYING interior
    /// that the old radius rule let through unbounded.
    ///
    /// A DETERMINISM-DIGEST CHANGE, NOT A REFACTOR (BL-855): capping degree
    /// changes which regions the sim considers adjacent, which changes which
    /// campaigns are even candidates. `world_determinism`'s digests move and
    /// must be re-blessed deliberately, not by reflex.
    int max_neighbour_degree = 10;

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

    // --- Ancient roads and the reach GATE (BL-837) -------------------------
    //
    // CIVILISATION.md § The road is the empire's skeleton, and reach GATES
    // conquest — Ben's elicitation ruling (2026-09-09): "REACH GATES A
    // CAMPAIGN; IT DOES NOT MERELY PRICE IT." Everything above this block
    // (terrain_reach_cost_q, the burden of breadth) still PRICES distance —
    // it makes a far campaign worse, never impossible. That is precisely what
    // the ruling declines: a rich enough polity could always buy past
    // geography. These four fields are what makes reach a WALL rather than a
    // toll, and what lets a road move the wall.
    //
    // THE NETWORK ALREADY EXISTS, ONE STEP REMOVED FROM THE SIM. BL-768's
    // `supply_corridors` already record every region-to-region line a
    // campaign or a Settle actually walked, deduplicated with a use count —
    // but only as a POST-SIM artefact `road_generation.cpp` stamps onto the
    // campaign-era tile grid. The sim itself never read its own corridors
    // back. These fields close that loop: a LIVE copy of the same use count,
    // read by `rebuild_reach` while the sim is still running, so a heavily
    // walked line is cheaper for the REST OF THE RUN, not just for the
    // finished world. Nodes are seats, edges are these corridors — exactly
    // the network CIVILISATION.md's outcome brief names, now load-bearing
    // rather than decorative.

    /// Corridor uses (BL-768's own count, read live) before a line is
    /// considered a Track (tier 1) at all. Below this a corridor is a route
    /// that has been walked, not one worth widening — mirrors
    /// `road_generation.cpp`'s measured `kAncientRoadUses` (4: one journey is
    /// a founding party that never returns, four is repeat traffic).
    int road_tier1_uses = 4;
    /// Uses before a Track becomes a Road (tier 2), the ancient network's
    /// busiest lines. No tier 3 in the sim itself — Highway-grade promotion
    /// wants a built work at both ends (`road_generation.cpp::ancient_tier`),
    /// which is campaign-era-only bookkeeping this pass does not carry.
    int road_tier2_uses = 12;

    /// SUSTAINABLE-REACH FLOOR FOR LAUNCHING A CAMPAIGN, in the same 0..1000
    /// supply currency `campaign_supply` already prices. At or below this,
    /// the target is not scored at all — GATED, not merely priced down — so
    /// a rich polity cannot outbid geography for a target its roads have not
    /// earned it. Deliberately ABOVE zero: at literally zero supply the
    /// existing arithmetic already zeroes the odds term, so a floor of zero
    /// would be a gate in name only. NOT A MEASUREMENT — exactly the
    /// magnitude `history_sweep` is meant to tune.
    ///
    /// FIRST CUT WAS 250, AND B384c EXPOSED A DEEPER PROBLEM THAN A BAD
    /// NUMBER (2026-09-10). `history_sim_harness`'s B384c ("a polity
    /// somewhere in the set loses its last region", BL-308's death-spiral
    /// invariant) passes on main today with 6/8 seeds eliminating a polity.
    /// At floor=250 it failed on all 8. Lowering to 80, then to 20 — TWO
    /// FULL ORDERS OF MAGNITUDE APART — produced IDENTICAL per-seed battle,
    /// conquest and elimination counts (0/8 both times), despite battle
    /// volume comparable to or higher than the no-gate baseline. That rules
    /// out a calibration fix: the floor's exact value is not what is
    /// suppressing eliminations.
    ///
    /// THE LIKELY MECHANISM, and it follows directly from Ben's own ruling
    /// that reach GATES rather than prices: a polity reduced toward its last
    /// region is exactly the case where pure pricing used to still let a
    /// low-probability killing blow occasionally land. A hard gate removes
    /// that long tail entirely — if a shrinking polity's last holdout ever
    /// sits beyond every neighbour's CURRENT road reach (and a shrinking
    /// realm is also a realm whose roads are contracting), it can become
    /// permanently unconquerable rather than merely unlikely to fall. That
    /// is a genuine tension with B384c's invariant, not a bug in this field.
    ///
    /// 80 IS WHERE THIS SHIPS, PENDING BEN'S CALL. It is the value BL837a's
    /// synthetic far-target case was calibrated against and is defensible on
    /// its own terms; B384c is left failing and UNRESOLVED rather than
    /// forced green by further lowering a floor already shown not to move
    /// it. See the sprint 38 delivery note for the options put to Ben.
    ///
    /// `campaign_supply` stacks several decay terms (distance, terrain, the
    /// burden of breadth) that a genuinely overextended ORDINARY target
    /// still drives below this floor, so BL837a's synthetic far-target case
    /// stays refused. Re-tune from here with `history_sweep`, not by
    /// re-guessing a round number.
    int sustainable_campaign_floor_q = 80;

    /// SUSTAINABLE-REACH FLOOR FOR A STANDING GARRISON. Lower than the
    /// campaign floor on purpose: an army already standing on ground it holds
    /// can live thinner than one being asked to march onto new ground, so a
    /// region only starves once reach has eaten essentially ALL of its
    /// terrain-priced supply. At or below this, the garrison cannot be
    /// maintained and attrites (see `unsustained_army_attrition_q`) — the
    /// standing-army half of "reach gates", CIVILISATION.md's sharpened
    /// question B: not "can I take this" but "can I KEEP AN ARMY THERE".
    int sustainable_garrison_floor_q = 0;

    /// SUSTAINABLE-REACH FLOOR FOR A REGION'S CENTRES TO GROW (BL-872,
    /// CIVILISATION.md § Centres are derived by supply and governance). The
    /// SAME 0-1000 currency as the two floors above, read from
    /// `region::network_supply_q` rather than re-derived, and deliberately a
    /// third named floor rather than reusing either: growing a town is
    /// neither "launch a campaign" (`sustainable_campaign_floor_q`, 80) nor
    /// merely "keep an army fed at home" (`sustainable_garrison_floor_q`,
    /// 0) — it needs a network that can carry ordinary trade, which this
    /// project's design places above bare survival and at or below what a
    /// march demands. AT OR BELOW this, `advance_region_urban` FREEZES
    /// `region::centres`: no new centre stands up on ground the network can
    /// no longer feed or govern, but nothing already standing is razed —
    /// see the field comment on `region::network_supply_q` and
    /// `settlement.cpp`'s `promote_centres` for why freeze, not raze, is
    /// this item's answer to its own open question.
    int sustainable_settlement_floor_q = 40;

    /// Per-mille of `army_stock` lost per YEAR to a garrison standing beyond
    /// `sustainable_garrison_floor_q`. An army beyond sustainable reach cannot
    /// be maintained — this is what makes that literally true rather than a
    /// sentence in a doc: it starves at home the same way a campaign starves
    /// crossing open water (MILITARY_HISTORY.md § Forage), on the same
    /// per-mille scale, just priced by distance instead of by domain.
    int unsustained_army_attrition_q = 300;

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

    /// Severity of the sack a conquered region suffers, per-mille.
    ///
    /// BL-835 — THIS IS NOW AN URBAN QUANTITY ONLY. It used to be subtracted
    /// from `region::population` as well, and that was the mechanism that
    /// emptied the ground: 258 conquests of one region left it with no people,
    /// therefore no manpower, therefore no defence, therefore the best target
    /// on the map for the rest of the run. Under Ben's civilian-population
    /// ruling the countryside headcount does not move for war at all, so the
    /// sack now falls only where a sack falls — on the walls, through
    /// `sack_region_urban`, which razes centres and records every one it took.
    ///
    /// The collapse path survives, in the shape that was always the legible
    /// one: a razed city that regrows and still says it was razed. What is
    /// gone is war as a demographic event.
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
    ///
    /// 4 -> 2 (BL-767), and it is a CONSEQUENCE rather than a finding of its
    /// own. Re-pricing Invest to make the capacity ladder climbable made it
    /// ~7x stronger in the shared currency, and this file's own § Magnitudes
    /// note says the works weights exist to put the verb "in the same band as
    /// the other verbs" — so leaving this at 4 dropped `build_work` out of the
    /// contest entirely, taking three green harness checks (history_sweep W5 /
    /// W5c / W5d) red with it. Halving the horizon puts it back in band.
    ///
    /// DELIBERATELY PARTIAL: a 2x correction against a 7x move. It restores
    /// the checks and does NOT pretend to fix BL-757, whose zero-works finding
    /// is a SCALE MISMATCH rather than a weight — Invest's score is
    /// proportional to the whole empire's holdings while a work's local term is
    /// proportional to ONE region, so the gap widens with every region a polity
    /// takes. Measured under this change: real worlds still raise ZERO works
    /// (history_sweep, 8 seeds at --epoch 1960, 2026-09-06), and no value of
    /// this dial changes that.
    /// 4 -> 2 -> 1, and every step was a CONSEQUENCE of re-pricing Invest
    /// rather than a finding about works. BL-767 dropped it to 2 as a 2x
    /// correction against a ~7x Invest re-pricing; excluding capped domains from
    /// Invest selection (a correctness fix, not a tuning one) made Invest
    /// productive in rounds where it previously bought nothing, which squeezed
    /// build_work out of the contest again - 18 works raised fell to 6, and the
    /// capacity effect on population vanished entirely. 1 restores it (30 works,
    /// population 2660758 against 2297778 without) with industrialisation
    /// unchanged at 14/16.
    ///
    /// THIS IS THE FLOOR, AND THAT IS THE POINT WORTH CARRYING FORWARD. A work
    /// cannot amortise over less than one year, so if Invest becomes any more
    /// productive there is no headroom left in this dial and build_work leaves
    /// the contest for good. That is BL-757's structural finding arriving at its
    /// limit: Invest's score scales with the WHOLE EMPIRE while a work's local
    /// term scales with ONE REGION, so the gap widens with every conquest and no
    /// dial closes it. It still does not fix BL-757 - real generated worlds
    /// raise ZERO works at 16 seeds even here.
    int work_amortise_years = 1;

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

    // --- Materials and labour (BL-867) --------------------------------------
    // CIVILISATION.md § Materials are spent when something happens. Industry
    // accumulates at every seat for free (`region_industry_output`, no dial
    // here — it is a pure consequence of population/army/ore); these two are
    // the only knobs this item needs, because they are the only place an
    // ACTION prices itself against the stock rather than deriving it.

    /// Material units a Campaign spends per head of the army it raises,
    /// drawn from the acting polity's CAPITAL seat and clamped at what is
    /// actually standing there — an action consumes stock, it never goes
    /// into debt for one. Small by design: a campaign should visibly cost
    /// something without making the seat's stock the thing that decides
    /// whether a campaign is legal at all (reach does that — CIVILISATION.md
    /// § The road is the empire's skeleton — and gating on stock too would be
    /// a second, uncoordinated gate on the same verb).
    int campaign_material_cost_per_head_q = 4;

    /// How hard a materials SHORTFALL tells on the army that marches anyway,
    /// per-mille of the readiness channel `def_ready`/works/cohesion already
    /// share (roster_stack turns it into an additive offset on
    /// `type_power_mod`). At the default this bounds the whole effect to the
    /// same band those three sit in — "tilts a fight, never decides one" —
    /// so an empty seat fields a visibly weaker army rather than an
    /// impossible one. Zero would make the cost above purely cosmetic
    /// bookkeeping with no consequence a battle could show.
    int material_shortfall_penalty_q = 120;

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

    // --- The playback record (BL-817) --------------------------------------
    //
    // ON BY DEFAULT, unlike `trace_battles`, because generation is the consumer
    // rather than a harness — the wizard's round 4 replays this, and a record
    // that had to be asked for would be missing on every real world.
    //
    // The suppression switch exists for exactly one purpose: so the harness can
    // run the SAME seed twice, once recorded and once not, and assert every
    // other output agrees bit-for-bit. That is the check that keeps a watchable
    // sim and a deterministic one from becoming two different sims, and it is
    // the reason this is a param rather than a compile-time constant.
    bool record_playback = true;

    /// Minimum years between recorded steps. The record samples on a DECISION
    /// ROUND once this many years have passed since the last sample, so it is
    /// never finer than the sim's own decisions and never coarser than the
    /// decision band by more than one round.
    ///
    /// 20 puts a 4000-year run at ~200 steps. A per-YEAR record would be 4000,
    /// and the per-step cost is a full pass over the polity table — which is
    /// the trap `owner_changes` already avoided once by not being a grid.
    /// Values below 1 are treated as 1.
    int record_interval_years = 20;
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
/// "This polity never crossed the Industrial rung." A sentinel outside every
/// arc this sim runs, because 0 is a legitimate crossing year on the ancient
/// arc (4000 BCE -> 0 CE).
inline constexpr int64_t k_never_industrialised = INT64_MIN;


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

    /// BL-748 — THE YEAR THIS POLITY CROSSED THE INDUSTRIAL RUNG, or 0 for
    /// never. The sim's industrial clock is the capacity ladder, and the rung
    /// is `roster_band_for_capacity(capacity[materials]) == industrial` under
    /// the span ceiling in force — the SAME derivation the works table reads,
    /// so a polity cannot light a furnace at a band it could not build at.
    ///
    /// Materials, not military, for the reason HISTORY.md § The works roster
    /// gives: a Blast Works turns over with metallurgy, not with the column
    /// whose rows turn over at a roster boundary.
    ///
    /// This is the polity half of Stage 4. The REGION half is
    /// `region::industrial_lag_years` — how long that particular ground takes
    /// once its owner can pay for a furnace at all — and a region lights at
    /// `industrial_year + lag`, if that year falls before the epoch.
    /// `k_never_industrialised`, NOT 0, and the distinction is not pedantry:
    /// 0 CE is a real calendar year on every arc this sim runs, and the ancient
    /// arc runs 4000 BCE -> 0 CE. Sentinel-as-zero meant three things at once.
    /// A polity that crossed the rung in year 0 wrote the sentinel, so it was
    /// re-detected on every later round and `polities_industrialised`
    /// double-counted it. And on the default single-span arc `sim_band_ceiling`
    /// is inert, so a median of four polities per world "crossed" between 2000
    /// and 3100 BCE - garbage that no consumer could tell from a real date, two
    /// lines beneath a summary correctly printing "no polity reached the rung".
    int64_t industrial_year = k_never_industrialised;

    /// BL-750 — TARIFF POSTURE, AS A DERIVED SCALAR, 0-1000.
    ///
    /// How hard this polity protects what it has. Ben ruled the DERIVED form on
    /// 2026-09-06 (NATIONS.md sec 4 Tariffs): a polity does not spend a round
    /// choosing protectionism, so this is not a scored verb and nothing in the
    /// decision loop reads it. It is computed ONCE, after the run, from facts
    /// the run already accumulated, and read at the handoff by
    /// `derive_national_protection` -> `seed_national_tariffs`.
    ///
    /// TWO TERMS, MULTIPLIED, and the product is the point:
    ///   - HOW MUCH OF THE FIELD IS AHEAD of it (the share of surviving
    ///     polities that lit a furnace strictly before it did), and
    ///   - HOW FAR BEHIND it is (its own lag from the world's first furnace,
    ///     as a share of the span from that furnace to the epoch).
    /// Either alone reads flat. Rank alone is uniform by construction — the
    /// last polity in a twelve-way field always scores 1000 whether it lit two
    /// years late or never. Lag alone makes every non-industrialiser max out,
    /// so a world where one polity of twelve industrialises tariffs eleven
    /// nations identically. The product says "behind, AND far behind".
    ///
    /// A WORLD WHERE NOBODY LIT SCORES ZERO FOR EVERYONE, and it falls out
    /// rather than being special-cased: with no furnace, nobody is strictly
    /// ahead of anybody, the share term is zero for every polity, and the
    /// product collapses. That is the honest reading — protection is a response
    /// to an industrial competitor, and a world without one has nothing to
    /// protect against. A world with no tariff is a legitimate outcome
    /// (GENERATION_STRATEGY.md sec Asymmetry is the deliverable) and this is
    /// where it comes from.
    ///
    /// THE COLONY TERM IS OWED, NOT FORGOTTEN. Ben's ruling names two movers:
    /// industrialisation timing and whether the polity holds colonies, "since a
    /// metropole protects its ties". The second has NO INPUT in this codebase —
    /// BL-749 (sea-leg campaign) is what gives a polity ground across water,
    /// and it has not landed. Rather than invent a proxy for a colony (a
    /// far-flung holding is a large empire, not an overseas one), the term is
    /// left out and recorded as owed. It is an addend on this scalar when
    /// BL-749 lands, not a restructure.
    int protection_q = 0;

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

// `owner_change` and `owner_none` live in era_timelapse.hpp (included above):
// the recorded time-lapse crosses into `generation_report`, and
// hard_coded_world.hpp refuses to include THIS header. Moved rather than
// copied — a duplicated wire type is the drift BL-462 exists to prevent.

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

/// ONE DECISION ROUND IN WHICH CAMPAIGN CLEARED ITS THRESHOLD — Sprint 28's
/// fork, recorded whether Campaign then won the round or lost it.
///
/// WHY THIS EXISTS, and why it is not a mean. `battle_trace` explains a battle
/// that HAPPENED, so it is mute about the two worlds in eight that fight no war
/// in an entire era. `campaign_contacts`/`campaign_scored`/`campaign_chosen`
/// narrow those to "the scorer sees war and prefers something else every time"
/// and then stop, because a candidate that was DISCARDED BELOW THE THRESHOLD
/// and one that CLEARED IT AND LOST THE ARGMAX are indistinguishable in a
/// `chosen == 0` count — and they are different defects with different fixes:
///   - never cleared  -> the campaign score itself is too small (a term, a
///     weight, or a prize that does not survive its own discounts);
///   - cleared and lost -> Campaign is competing against verbs it cannot beat,
///     which is a VERB-COMPETITION question and not a combat one.
///
/// And when it is the second, the MARGIN separates two further bugs that a mean
/// cannot: a margin of 3 is a weighting nudge, a margin of 3000 is BL-318
/// incommensurability — two scores authored on different scales. This file has
/// been bitten by that twice (`w_cult` as a flat 150; `w_dist` flat against a
/// tripled map), so the distribution is recorded per round and summarised by
/// the harness rather than folded into an average here.
///
/// PURE OBSERVATION, exactly as `battle_trace` is. Populated only under
/// `history_sim_params::trace_battles`; no field is read to make a decision and
/// none feeds a draw, so a traced run and an untraced run are identical in
/// every other output.
struct verb_contest_trace
{
    int32_t  year   = 0;
    uint16_t polity = 0;
    /// Best Campaign candidate score this round, among those that cleared
    /// `campaign_threshold_q`. Includes the tie-break salt, because that is the
    /// value the argmax actually compared.
    int      campaign_score = 0;
    /// The score the round was decided on — `best_score` after every verb has
    /// been offered. Equals `campaign_score` exactly when Campaign won.
    int      winner_score = 0;
    /// The verb that won. Never `none`: a cleared Campaign always beats the
    /// initial `best_score` of 0, so this record cannot exist on a round that
    /// fell through to the Consolidate default.
    sim_verb winner = sim_verb::none;
};

// ---------------------------------------------------------------------------
// Grudges (BL-827)
// ---------------------------------------------------------------------------

/// WHAT was done. Every kind here is an event this sim actually resolves —
/// nothing is invented to fill the enum.
///
/// TWO OF THE ITEM'S NAMED KINDS ARE OWED, NOT FORGOTTEN, and they are recorded
/// as owed for the same reason `polity::protection_q` records its colony term:
/// "a union refused" and "an ally abandoned" have NO INPUT in this codebase.
/// Era -1 has no unions and no alliances — a polity has holdings, a doctrine
/// and neighbours, and nothing else. Proxying them from adjacency would invent
/// a diplomatic layer inside a data-model change, which is exactly the scope
/// growth the standing rules forbid. They are two more rows on this enum and
/// two more `raise_grudge` calls once a diplomacy layer exists.
enum class grudge_kind : uint8_t
{
    ground_taken = 0,  ///< A region changed hands.
    seat_sacked,       ///< And it was the loser's capital.
    border_raided,     ///< A battle was fought that transferred nothing.
    realm_ended,       ///< An extinguished realm's kin resent its killer.
};
inline constexpr int grudge_kind_count = 4;

/// Contributing events kept per pair. Fixed and small: the rest falls into the
/// scalar, which is the sparse/bounded half of the design.
inline constexpr int grudge_events_kept = 3;

/// ONE NAMED CAUSE — what, where, when, and how much it was worth when it
/// happened. A number a player cannot ask about is a modifier, not a story.
struct grudge_event
{
    int32_t     year      = 0;
    uint16_t    region    = 0xFFFFu; ///< `owner_none` where the event has no place.
    grudge_kind kind      = grudge_kind::ground_taken;
    int32_t     magnitude = 0;       ///< Score added at the time, before decay.
};

/// A directed pair. DIRECTED because resentment is not symmetric and that
/// asymmetry is the flavour: the realm that lost the province and the realm
/// that took it do not feel the same way about each other.
struct grudge
{
    uint16_t from = 0; ///< The aggrieved polity.
    uint16_t to   = 0; ///< The polity it resents.
    /// The standing, decayed score. What seeds sentiment at world setup.
    int32_t  score = 0;
    /// The highest the score ever reached. Kept because decay makes the epoch
    /// figure alone misleading — a pair that fought for six centuries and then
    /// held a quiet millennium reads as nothing without it.
    int32_t  peak = 0;
    /// Every event ever raised on this pair, including the ones that fell out
    /// of `events`. The denominator `events` is a top-N of.
    int32_t  event_count = 0;
    /// The largest contributing events, sorted descending by magnitude, ties on
    /// the later year, then the lower kind. Bounded by construction.
    int32_t  events_kept = 0;
    grudge_event events[grudge_events_kept]{};
};

struct history_sim_state
{
    std::vector<polity> polities;

    /// WHY A WORLD NEVER FIGHTS (BL-384's real question), counted only under
    /// `params.trace_battles`.
    ///
    /// A world with zero battles has zero `battle_trace`s, so the per-battle
    /// record is silent about exactly the case that needs explaining. These
    /// three separate the possibilities, which need different fixes:
    ///   - `campaign_contacts` 0  -> the polity never shares a border with a
    ///     foreign owner at all. The gap is in expansion and adjacency; scoring
    ///     is irrelevant because nothing is ever a candidate.
    ///   - contacts > 0, `campaign_scored` 0 -> candidates exist and are
    ///     discarded before scoring.
    ///   - scored > 0 but `campaign_chosen` 0 -> the scorer sees war and prefers
    ///     something else every time. That is a threshold/weighting question.
    int64_t campaign_contacts = 0; ///< (own region, foreign-owned neighbour) pairs examined.
    int64_t campaign_scored   = 0; ///< Candidates that reached the score comparison.
    int64_t campaign_chosen   = 0; ///< Rounds where Campaign won the verb choice.

    /// THE FORK THE THREE ABOVE STOP ONE STEP SHORT OF (Sprint 28 lane A).
    /// `campaign_scored > 0, campaign_chosen == 0` still has two readings, and
    /// they need opposite fixes — see `verb_contest_trace`. These split it:
    ///   - `campaign_cleared` 0 -> the score NEVER clears its own threshold.
    ///     Verb competition is irrelevant; nothing was ever in the running.
    ///   - `campaign_cleared` > 0 with `campaign_cleared_lost` accounting for
    ///     every cleared round -> Campaign is in the running every time and
    ///     loses the argmax every time. That is a verb-competition question,
    ///     and `verb_contests` names the winner and the margin.
    ///
    /// GRAINS DIFFER, deliberately, and the names say which: `campaign_cleared`
    /// is CANDIDATE-grain (a polity examines many targets x 2 seasons in one
    /// round), the other two are ROUND-grain. Mixing them would make the ratio
    /// between them meaningless.
    ///
    /// The identity the harness binds to:
    ///     campaign_cleared_rounds == campaign_chosen + campaign_cleared_lost
    /// — every round in which Campaign was in the running either won it or lost
    /// it, with no third outcome.
    int64_t campaign_cleared        = 0; ///< CANDIDATES clearing `campaign_threshold_q`.
    int64_t campaign_cleared_rounds = 0; ///< ROUNDS with at least one such candidate.
    int64_t campaign_cleared_lost   = 0; ///< Of those rounds, the ones another verb won.

    /// Per-round observation of the verb contest, empty unless
    /// `params.trace_battles`. One entry per round in which Campaign cleared —
    /// the won rounds included, so the margin distribution has a control.
    std::vector<verb_contest_trace> verb_contests;

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

    /// THE PLAYBACK RECORD (BL-817) — the OTHER TWO THIRDS of a time-lapse.
    ///
    /// `owner_changes` above draws the spreading colour and nothing else. These
    /// three carry the leaderboard's series and the culture mix's drift, on the
    /// recorded-step cadence `history_sim_params::record_interval_years` sets.
    /// See era_timelapse.hpp § The playback record for the encoding argument
    /// and for why a sampled record cannot move the run that produced it.
    ///
    /// Empty when `params.record_playback` is false — which is the control arm
    /// of the harness assertion, not a debug mode.
    std::vector<timelapse_step>  steps;
    std::vector<polity_sample>   samples;
    std::vector<culture_change>  culture_changes;

    /// THE ANCIENT ROAD RECORD (BL-768) — every region-to-region corridor the
    /// history actually moved along, deduplicated and counted.
    ///
    /// Two sources, and each is an event the sim already resolves rather than a
    /// new concept: a CAMPAIGN records (staging holding -> objective), the line
    /// an army was actually victualled along and the exact pair `campaign_supply`
    /// prices; a SETTLE records (parent -> daughter), the line a founding party
    /// walked. Consolidate and Invest have no spatial pair and contribute none.
    ///
    /// Sorted ascending by (a, b) with `uses` accumulated, so the stamping pass
    /// is order-independent by construction. Region indices are stable for the
    /// life of a run — Settle only ever appends — so an index recorded in year
    /// -3900 still names the same region at the epoch.
    ///
    /// NOT gated on `trace_battles`: generation is its consumer, not a harness.
    std::vector<history_corridor> supply_corridors;

    /// THE SPARSE, DIRECTED, DECAYING GRUDGE TABLE (BL-827).
    ///
    /// Sorted ascending by (from, to) and searched by binary search, so the
    /// order cannot depend on a container's layout — an unordered map keyed on
    /// a pair would have been the obvious first cut and is exactly the
    /// non-determinism this repo has been bitten by before.
    ///
    /// Sparse because most pairs never meet: an entry exists only where an
    /// event was actually raised, and is dropped once decay takes it under
    /// `history_sim_params::grudge_floor`.
    ///
    /// A DEAD POLITY'S GRUDGES ARE LOST, IN BOTH DIRECTIONS, and that is a
    /// deliberate call rather than a consequence of how ids happen to be
    /// reused. This sim has no successor concept — polity ids are seeded once
    /// from cultures and never re-created — so "the successor inherits" could
    /// only be decided by id arithmetic, which is precisely the accident that
    /// would make the record meaningless. What survives an extinction instead
    /// is a NEW named event: `grudge_kind::realm_ended`, raised from every
    /// surviving polity that shares the dead realm's culture toward its killer.
    /// The dead leave a grudge in their kin, never in an heir.
    std::vector<grudge> grudges;

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

    // --- BL-778 / BL-779: what the water model actually produced -----------
    //
    // Three readings, and all three are CALIBRATION, never coverage targets to
    // raise (docs/generation/MILITARY_HISTORY.md § Naval — "rare is the design,
    // not a shortfall"). A sim in which sea battles were routine would be
    // generating a different history.

    /// Campaign candidates REFUSED on traversal legality (BL-778): the line
    /// from the staging holding to the target crosses sea, the polity owns no
    /// shore bridging it, and it can field no naval row to carry the force.
    /// This is the free overseas conquest BL-755 measured, now priced.
    int64_t illegal_campaigns = 0;

    /// Campaigns fought at ZERO supply because the force could not forage
    /// (MILITARY_HISTORY.md § Forage) — it reached ground adjacent to neither
    /// land nor water its own polity holds. A subset of `stalled_campaigns`.
    int64_t starved_campaigns = 0;

    // --- BL-837: ancient roads and the reach GATE ---------------------------

    /// Campaign candidates REFUSED because the target sits beyond
    /// `sustainable_campaign_floor_q` — the GATE, not the price. Distinct
    /// from `illegal_campaigns` (a water-domain refusal): this one fires on
    /// ordinary dry or forage-legal ground that is simply too far, unroaded,
    /// to project force onto. A world with roads and one without should
    /// differ here first, before either differs in battles.
    int64_t reach_denied_campaigns = 0;

    /// Years in which a standing garrison's own ground could not supply it
    /// (`sustainable_garrison_floor_q`) and its `army_stock` attrited as a
    /// result — "an army beyond sustainable reach cannot be maintained",
    /// counted where it happens. Zero on a world where every holding sits
    /// inside its capital's roaded reach.
    int64_t unsustained_attrition_events = 0;

    /// Battles in which EITHER stack committed a naval entry (BL-779).
    ///
    /// READ THIS WITH `sea_leg_battles`, NOT ALONE, and the reason is the
    /// composition model. `roster_stack` composes a stack from EVERY available
    /// row by weight (MILITARY_HISTORY.md § Naval — ships "are rows in the same
    /// roster... composed into the same stack"), so a polity whose ground clears
    /// `port_q` carries a galley contingent into every fight it has, inland ones
    /// included. A high figure here therefore measures HOW COASTAL THE POWERS
    /// ARE, not how often anyone fought at sea.
    int64_t naval_battles = 0;

    /// Battles reached over a SEA LEG — the line from the staging holding to the
    /// objective crossed sea. This is the one that answers "how often does naval
    /// combat actually occur", because it is the only figure that requires water
    /// to have been crossed rather than merely bordered.
    int64_t sea_leg_battles = 0;

    /// Works raised over the run (BL-321). Counted because "did the roster fire
    /// at all" and "did it fire so much nothing else happened" are the two ways
    /// this item fails, and neither is visible in the battle/founding counts.
    int64_t works_raised = 0;

    /// BL-760 (1): works raised and units fielded, SPLIT BY ROSTER BAND and by
    /// which span they happened in (index 0 = the ancient span, 1 = industrial).
    ///
    /// WITHOUT THIS THE TWO-SPAN BAND CEILING HAS NO OBSERVABLE. `works_raised`
    /// is one scalar with no band split, so nothing in the project could tell
    /// `span1_band_ceiling = medieval` from `= industrial`: if no polity reaches
    /// materials capacity 4 before the boundary the clamp never binds, and every
    /// check stays green whether or not the ceiling works at all. A requirement
    /// was marked complete on substituted evidence because of it.
    ///
    /// READ THEM WITH BL-757 IN HAND. That item measured ZERO works raised
    /// across sixteen seeds, because `build_work` never wins the scored contest
    /// — so a band row of zeros here has two possible causes and the counter
    /// alone cannot separate them. The units rows are the ones carrying signal
    /// until that is fixed.
    /// CROSS-TABULATED, not two marginals. [span][band], span 0 = ancient.
    /// Two separate 1-D arrays cannot answer this question and the first cut of
    /// this counter got that wrong: with a medieval span-1 ceiling, span 1
    /// legitimately fields gunpowder, so units_by_band[gunpowder] > 0 and
    /// units_by_span[0] > 0 - and an UNRESTRICTED run where span 0 fields
    /// gunpowder produces the IDENTICAL pair of marginals. The counter built to
    /// see the ceiling was blind to exactly the case it existed for.
    std::array<std::array<int64_t, roster_band_count>, 2> works_by_span_band{};
    std::array<std::array<int64_t, roster_band_count>, 2> units_by_span_band{};

    /// Battles in each century of the run, index 0 = the first hundred years.
    /// The sweep reports war frequency PER CENTURY rather than as a total,
    /// because a world that fought all its wars in one age and a world that
    /// fought steadily for two millennia have the same total and nothing else
    /// in common — the total alone cannot tell them apart.
    std::vector<int32_t> battles_per_century;

    /// BL-748 — how many polities crossed the Industrial rung inside the run,
    /// and how many regions actually lit a furnace before the epoch. The two
    /// differ and the gap is the point: a polity can cross with two years left
    /// and industrialise nothing, and a counter that reported only the second
    /// could not tell that world from one where the rung was never reached.
    int64_t polities_industrialised = 0;
    int64_t regions_industrialised  = 0;

    /// Highest total population the body ever carried, and the year it peaked.
    /// Peak has to be tracked as the run goes: the epoch figure alone cannot
    /// show a world that grew, collapsed and never recovered.
    int64_t peak_population = 0;
    int64_t peak_year       = 0;

    /// BL-867 — total material units ever credited to a seat, and ever spent
    /// launching a Campaign. Neither is a stock (that lives on the regions
    /// themselves); these are RUN TOTALS, the observable that lets a caller
    /// tell "materials exist but nothing spends them" from "the loop is
    /// live" without reading every region's `material_stock` by hand.
    int64_t materials_produced           = 0;
    int64_t materials_spent_on_campaigns = 0;

    /// BL-869 — every civilisation mixing actually grew, in formation order.
    /// A NAMED RECORD each (creeds.hpp), never a creed: `region::civilisation`
    /// indexes into this. Empty on a world where little or no mixing happened
    /// — CIVILISATION.md's "done when": "a world with little mixing produces
    /// few or none".
    std::vector<civilisation> civilisations;

    /// How many civilisation records were ever created (== civilisations.size()
    /// today, but counted alongside the run's other formation-style counters
    /// — `foundings`, `polities_industrialised` — for the same reason: a
    /// harness reads this list, not the vector, when it only wants the count).
    int64_t civilisations_formed = 0;
};

/// Sentinel for "no polity owns this region in this year slice".
// `owner_none` — era_timelapse.hpp.

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
/// REPORT-ONLY wall-clock split of the last `run_history_sim` call (BL-825).
///
/// NOT SIM STATE, AND NEVER READ BY THE SIM. These are nanosecond accumulators
/// written at four sites inside the year loop so a measurement harness can say
/// where the span's cost actually goes. Nothing in `history_sim.cpp` reads them
/// back, nothing branches on them, and they must NEVER enter `state_hash` or any
/// digest — a wall clock differs every run, which is the one thing this layer
/// may not do (see era_minus_one.hpp on why the generation budget lives off the
/// save seam for exactly this reason).
///
/// Process-global and reset at the top of each `run_history_sim`, so a caller
/// reads the split of the run it just made. Not thread-safe, and does not need
/// to be: generation runs the era on one thread.
struct history_sim_profile
{
    int64_t ns_demography = 0; ///< The per-year demography/urban pass.
    int64_t ns_decisions  = 0; ///< The whole polity-decision round (battles and reach INCLUDED).
    int64_t ns_battles    = 0; ///< `resolve_battle` alone, a subset of `ns_decisions`.
    int64_t ns_reach      = 0; ///< `rebuild_reach` (the heapless Dijkstra), also a subset.
    int64_t decision_rounds = 0; ///< Years on which the decision gate opened.
    int64_t reach_rebuilds  = 0; ///< Calls to `rebuild_reach`.
};

/// The accumulators above, for the run that just finished.
history_sim_profile& history_sim_last_profile();

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

/// The roster band ceiling in force at year @p y. ONE derivation, read by
/// BOTH roster sites — the works table off materials capacity and the unit
/// table off military — so the two tables cannot drift apart on the span.
inline roster_band sim_band_ceiling(const history_sim_params& p, int64_t y)
{ return y < p.boundary_year ? p.span1_band_ceiling : roster_band::industrial; }

/// Bytes the time-lapse substrate occupies — the quantity the requirement
/// bounds, and the reason the encoding is a change list rather than a grid.
inline int64_t owner_ring_bytes(const history_sim_state& s)
{
    return static_cast<int64_t>(s.owner_changes.size()) * static_cast<int64_t>(sizeof(owner_change));
}

/// Bytes the PLAYBACK record occupies (BL-817) — the steps, the per-polity
/// samples and the culture-share change list. Disjoint from `owner_ring_bytes`,
/// so the two add to the whole time-lapse rather than overlapping.
inline int64_t playback_record_bytes(const history_sim_state& s)
{
    return static_cast<int64_t>(s.steps.size())           * static_cast<int64_t>(sizeof(timelapse_step))
         + static_cast<int64_t>(s.samples.size())         * static_cast<int64_t>(sizeof(polity_sample))
         + static_cast<int64_t>(s.culture_changes.size()) * static_cast<int64_t>(sizeof(culture_change));
}

/// Materialise the ownership map as it stood at the END of @p year — the
/// time-lapse read. Returns `region_stride` entries, `owner_none` where the
/// region did not exist yet or was unowned.
///
/// Linear in the change list, which is the point: a whole 2000-year playback
/// is one forward walk, and a seek to a single year costs no more than the
/// changes preceding it.
/// The sim's output as the RECORDED time-lapse — the one place the live state
/// becomes the thing generation stores and the Ages view replays (NR-733).
/// Everything else in `history_sim_state` (the narration, the counters, the
/// scorer's working set) stays behind; a record is not a snapshot of the run.
inline era_timelapse as_timelapse(const history_sim_state& s)
{
    era_timelapse t;
    t.changes         = s.owner_changes;
    t.steps           = s.steps;
    t.samples         = s.samples;
    t.culture_changes = s.culture_changes;
    t.region_stride = s.region_stride;
    t.start_year    = static_cast<int32_t>(s.start_year);
    t.years         = static_cast<int32_t>(s.years);
    return t;
}

std::vector<uint16_t> owner_slice_at(const history_sim_state& s, int64_t year);

// ---------------------------------------------------------------------------
// Grudge reads (BL-827)
// ---------------------------------------------------------------------------

/// The standing score @p from holds against @p to, or 0 where no entry exists.
/// Binary search over the sorted sparse table.
int grudge_between(const history_sim_state& s, int from, int to);

/// The @p n strongest pairs, sorted descending by `score`, ties on `peak`, then
/// on (from, to). A TOTAL order with an explicit tie-break, so the listing is
/// identical on every machine — the same discipline the scorer's argmax uses.
std::vector<grudge> top_grudges(const history_sim_state& s, int n);

/// One line naming a grudge event: what, where, when. The printable half of
/// "it must carry its cause".
std::string grudge_event_line(const grudge_event& e, const settlement_state& ss);

// ---------------------------------------------------------------------------
// The pass 1 -> pass 2 handoff (BL-828)
// ---------------------------------------------------------------------------

/// The provinces one polity holds at the handoff. `regions` is ascending, so
/// the set is a value rather than a walk order.
struct polity_holdings
{
    int              polity = -1;
    std::vector<int> regions;
};

/// THE WHOLE OF WHAT PASS 1 HANDS FORWARD, AND NOTHING ELSE (BL-828).
///
/// `GENERATION_STRATEGY.md` § What crosses each handoff already names the list
/// — "the region table, cultures, works, the strain accumulators. Nothing is
/// reset." That clause was a promise in prose with nothing enforcing it: every
/// consumer reached into `history_sim_state` and `settlement_state` and took
/// whatever it found, so the list and the code could drift without either
/// noticing. This struct makes the doc's list and the struct's fields the same
/// list, and `pass_one_output_valid` is the check that they still agree.
///
/// A VALUE, not a view. Copies rather than pointers, because a handoff whose
/// members alias the live sim state is not a handoff — a consumer could mutate
/// the producer through it, and "nothing is reset" would stop being checkable.
///
/// WHERE EACH ITEM ON THE DOC'S LIST LIVES, so the mapping is explicit rather
/// than inferred:
///   - the region table          -> `regions`
///   - cultures                  -> `region::culture` (shares) + `culture_count`
///   - works                     -> `region::works_built` and the five
///                                  `work_*_mod` fields, plus `works_by_span_band`
///   - the strain accumulators   -> `region::contest_q` and `polity::cohesion_q`
///   - grudges (BL-827)          -> `grudges`
///   - the provinces each polity holds -> `holdings`
struct pass_one_output
{
    /// The region table, carrying culture shares, works, the urban record and
    /// the per-region strain accumulator (`contest_q`).
    std::vector<region> regions;

    /// The polities, carrying the per-polity strain accumulator (`cohesion_q`),
    /// the capacity ladder and the derived tariff posture.
    std::vector<polity> polities;

    /// How many cultures the shares above index into. Carried so a consumer can
    /// bound-check a share without holding a `creed_state`.
    int culture_count = 0;

    /// Works raised, cross-tabulated [span][band] exactly as the sim counts them.
    std::array<std::array<int64_t, roster_band_count>, 2> works_by_span_band{};

    /// The directed grudge table (BL-827) — an input to sentiment at world
    /// setup, never a quantity of its own.
    std::vector<grudge> grudges;

    /// Which provinces each polity holds, one entry per LIVING polity, sorted
    /// ascending by polity id. The political map as a set rather than as a
    /// field to be re-derived.
    std::vector<polity_holdings> holdings;

    /// The recorded time-lapse and the span it covers.
    era_timelapse timelapse;
    int64_t start_year = 0;
    int64_t stop_year  = 0;
};

/// Fold the live sim state and settlement state into the handoff value.
/// @param culture_count Cultures the shares index into; 0 when unknown.
pass_one_output make_pass_one_output(const settlement_state&  ss,
                                     const history_sim_state& hs,
                                     int                      culture_count);

/// The enforcement half of the struct above. Checks what the doc's clause
/// actually claims: every region's shares sum to exactly 1000 and name only
/// cultures in range; every holding names a living polity and an existing
/// region, with no region held twice; every grudge names polities in range and
/// carries at least one event. Writes the first failure into @p why.
bool pass_one_output_valid(const pass_one_output& o, std::string* why);
