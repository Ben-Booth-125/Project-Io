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
#include "empire_tree_data.hpp" // BL-912 — the empire tree's generated node table
#include "exploration_tree_data.hpp" // BL-930 — the exploration tree's generated node table
#include "industry_tree_data.hpp" // BL-1038 — the industry tree's generated node table

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
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

// Forward declarations for BL-931's resume pointers below — all four are
// defined later in this header (`polity`, `grudge`, `contact`,
// `history_corridor`); a pointer needs no more than the name at this point,
// and `history_sim_params` stays a plain, allocation-free, trivially-copyable
// struct (its own long-standing rule, see the comment on the struct itself)
// because these are raw non-owning pointers, never containers by value.
struct polity;
struct grudge;
struct contact;
struct history_corridor;
struct dated_object;    // BL-1036's resume pointers, on the same footing.
struct universal_creed; // (`civilisation` is complete already: creeds.hpp.)

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
    /// Weight on what stands on the target BEYOND bare endowment — centres,
    /// urban population, works, and the seat premium (BL-924). See
    /// `campaign_prize_q`. Zero on any region nothing has settled yet, so
    /// every harness fixture that opens synthetic/undeveloped regions is
    /// untouched regardless of this weight's value.
    int w_prize = 300;
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
    /// BL-933 — a bound clause broken before its term ran out. Sized between
    /// a border raid and ground actually taken: a treaty broken cost the
    /// wronged party nothing physical, but the record is a clearer promise
    /// kept-or-not than either.
    int grudge_treaty_broken_q  = 250;

    /// Per-mille of the standing score shed PER YEAR. A rate, so the stepped
    /// clock multiplies it by the step.
    ///
    /// WITHOUT DECAY A 4000-YEAR RUN REACHES THE EPOCH WITH EVERY PAIR
    /// MAXIMALLY AGGRIEVED, which carries no information at all — every nation
    /// would seed identical sentiment and the whole record would be a constant.
    /// 3/1000 is a half-life near 230 years: a single old wrong is gone by the
    /// epoch, a running feud is not.
    ///
    /// THAT HALF-LIFE HOLDS ONLY ABOVE THE TRUNCATION LINE (BL-842). Below
    /// score 1000/(rate x step) the proportional decrement rounds to zero, so
    /// `decay_grudges` sheds one unit a round instead: small grudges fade
    /// linearly, faster than the exponential, and faster on a finer clock.
    /// history_sim_harness BL842 prints the measured half-lives per step.
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
    ///
    /// RETIRED BY BL-922 (Ben, 2026-09-11: the capital is the strategic
    /// headquarters). `campaign_supply` no longer reads this field: the
    /// staging-hub distance term it scaled was the reason the reach gate
    /// refused nothing (BL-905 -- an ordinary neighbour-adjacent march barely
    /// decayed the currency, and the capital's reach was a footnote to it).
    /// The march is now priced as the capital's reach to the hub over held
    /// ground plus one `edge_step` onto the target, all in the
    /// `terrain_reach_cost_q` currency. The field stays so older harness
    /// fixtures and `--set` lines still parse; setting it does nothing.
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
    /// BL-920 — ORGANISE, the fifth verb: growth over UNORGANISED culture
    /// ground the polity's network can reach (CIVILISATION.md sec "A city
    /// state spawns where a region's population is above a threshold"). Same
    /// shared currency as the four above.
    int organise_threshold_q = 40;

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

    /// BL-912 — THE EMPIRE TREE'S RESEARCH FRACTION. Research is a FLOW
    /// derived from the industry slice, never a stockpile (TREES.md sec
    /// State): this per-mille share of a polity's summed
    /// `region_industry_output` over its held ground is what the Invest
    /// verb spends on the tree's currently-targeted node, each round,
    /// scaled further by the spire ring held and by contact degree. A
    /// sweep dial per TREES.md sec Open questions ("the research
    /// fraction... to be set against the sizing rule rather than picked");
    /// 60 (6%) is a first guess sized so the Empire tree's ~48 nodes are
    /// reachable, not exhausted, by a leading polity over 400 BCE-1200 CE.
    int empire_research_fraction_q = 60;

    // -----------------------------------------------------------------------
    // BL-1038 — THE INDUSTRY TREE, INVESTED BEHIND A SWITCH.
    // -----------------------------------------------------------------------
    //
    // TREES.md sec Milestones: the Industry tree opens at its root to EVERY
    // living polity at 1660 (Ben, 2026-09-18) — no rim gate, unlike the
    // Exploration tree's entry on the Empire rim. OFF BY DEFAULT, and it must
    // stay off on every shipped path until the one re-bless (BL-1044): Industry
    // modifiers sum into the SHARED `polity::tree_mod_q`, which feeds defence,
    // cohesion, the materials pull-forward and every tree's research rate, so
    // a node bought here moves the whole world. With the switch off no code
    // path below reads or writes an Industry field, and `industry_mask` stays
    // 0, so its fold in `apply_tree_effects` adds nothing.

    /// The switch. Read only by the Invest verb's Industry block.
    bool industry_tree_enabled = false;

    /// The calendar year the Industry tree opens (the Digitisation span's
    /// own start, 1660). A round at or after it invests; one before it does
    /// not, so a run with the switch on that stops at or before this year is
    /// the switch-off run bit for bit (exploration_sim_harness pins that).
    int64_t industry_open_year = 1660;

    /// THE INDUSTRY RESEARCH RATE (TREES.md sec State: "the Industry tree's
    /// rate reads urban mass instead of the industry slice", PROPOSED
    /// 2026-09-18, not overturned). Per year, before the `research` modifier:
    ///
    ///     M      = urban_population summed over the polity's
    ///              `industry_research_top_k` largest held regions
    ///     Mc     = min(M, industry_urban_mass_cap)            CLAMPED FIRST
    ///     rate_q = Mc * isqrt64(Mc) / isqrt64(reference)
    ///              * industry_research_fraction_q / 1000
    ///
    /// i.e. `fraction x Mc x sqrt(Mc / reference)`: linear-equivalent at the
    /// reference mass, x2.83 for twice the mass, x0.35 for half. Superlinear so
    /// innovation gathers where people do; a RATE, never a scorer term — the
    /// scorer may not carry anything that grows with the polity's size
    /// (TREES.md sec The scorer). The cap is what keeps the transform from
    /// running away (a clamped sum cannot overflow the product, and one giant
    /// city cannot finish the tree in a decade). FIRST-CUT MAGNITUDES — the
    /// sweep re-prices them against the sizing rule (TREES.md sec Sizes: the
    /// leading polity finishes just before 1960, the median reaches halfway).
    ///
    /// MEASURED ONCE (exploration_sweep --through 1960 --industry-open 1660,
    /// the 16 library seeds, 2026-09-18). The top-3 urban mass of a living
    /// polity at 1660 has per-seed medians 69k-278k (median of medians ~214k)
    /// and maxima 499k-654k. The first guess (reference 100k, fraction 60)
    /// did not bind: every polity above ~200k earned a whole node per Invest
    /// round, so the spread was set by how often Invest won, not by urban
    /// mass. The reference is therefore the measured median mass, rounded,
    /// and the fraction sized so the cap-mass leader earns ~25k a 4-year
    /// round (a ring-2 major a round, a ring-3 milestone in three) and so
    /// needs most of the span's rounds to finish — 6 per mille of urban
    /// mass a year, which is the same order per head as the empire's 60
    /// per mille of the industry slice. The cap sits at the measured
    /// maxima, so it binds on the few largest realms only.
    int     industry_research_fraction_q   = 6;
    int64_t industry_urban_mass_reference  = 200000;
    int64_t industry_urban_mass_cap        = 600000;

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

    /// Supply cost per 100 tiles of TERRAIN-WEIGHTED reach from the capital
    /// over HELD ground (BL-922): `network_supply_q = 1000 - reach x this /
    /// 100`. Mountains cost roughly twice what plains cost, using the same
    /// landform ratios logistics.cpp already defines for the 1960 era; a
    /// road discounts the edge (BL-837) and a town relays (BL-887).
    ///
    /// 4000 IS MEASURED, NOT CHOSEN (BL-922, history_sweep 4 seeds at the
    /// 0 CE epoch, supply priced over the uncapped held-ground index). The
    /// old 10 lost 0.1 per plains tile, so the longest connected path on the
    /// map read ~970 and the floors (40/60/80) were reachable only by ground
    /// in another connected component -- the reach gate refused nothing
    /// (BL-905). Swept 10 / 1000 / 2000 / 3000 / 4000 / 5000: the gate first
    /// refuses at 3000 (median 1,593 of ~940,000 contacts, 0.17%); at 4000 it
    /// refuses 12,129 of ~784,000 (1.5%), the 30+-tile band reads 360-480
    /// against 880 beside the capital, and a few CONNECTED regions sit under
    /// the campaign floor; at 5000 it refuses 8.7% and battles fall to a
    /// third of the 10 figure. 4000 is the smallest swept value at which
    /// the gate refuses a non-trivial share. Re-tune from the sweep's
    /// BL-922 block, never by re-guessing.
    int terrain_reach_cost_q = 4000;

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
    /// busiest lines. NO TIER EARNED BY TRAFFIC BEYOND THIS ONE — Highway-
    /// grade promotion wants a built work at both ends
    /// (`road_generation.cpp::ancient_tier`), which is campaign-era-only
    /// bookkeeping this pass does not carry. BL-940 adds a THIRD rung below
    /// (`road_tier3_uses`), but it is bought, never walked into existence —
    /// see that field's own comment.
    int road_tier2_uses = 12;

    // --- BL-940: the road ladder's third rung, bought with capital --------
    // EXPLORATION.md sec Goods move as throughput: "The road ladder gets its
    // third rung here... bought with capital rather than earned by traffic"
    // (tree node EX-WY-1a, Post Roads).

    /// THE THRESHOLD ORDINARY TRAFFIC IS NOT MEANT TO REACH. `road_tier_for_
    /// uses` still reads a plain use count, so a purchase and enough
    /// centuries of ordinary walking are not formally distinguishable in the
    /// data — this is set well above what `road_tier2_uses` traffic could
    /// plausibly accumulate across one 460-year span, so in PRACTICE the rung
    /// is reached only by `try_build_post_road`'s direct set. A FIRST CUT,
    /// not a measurement: raise it further if a sweep ever shows ordinary
    /// traffic crossing it unpurchased.
    int road_tier3_uses = 200;

    /// Treasury spent (BL-932's `region::treasury`, NEVER `material_stock`)
    /// to promote one corridor from Road (tier 2) to Post Road (tier 3), once
    /// the spending polity holds EX-WY-1a. MEASURED (BL-949, exploration_sweep
    /// 16 seeds, 2026-09-14, after a resumed span began seeding its live
    /// corridor counts): seeds with a post road / total bought at 3,000 15/69,
    /// 100,000 15/69, 1,000,000 15/67, 3,000,000 15/61, 10,000,000 15/46,
    /// 30,000,000 12/28, 100,000,000 9/15, 300,000,000 3/3. Up to 10M the
    /// treasury gates nothing -- the purchase is decided by holding EX-WY-1a
    /// and an internal Road, and the price is noise against capital
    /// treasuries in the tens of millions. 30M is the first swept value at
    /// which the bill refuses a real share of would-be builders while a clear
    /// majority of worlds still buy one (median living capital treasury at
    /// the close is ~1-8M, so only a rich seat affords it). Read only when
    /// `exploration_upkeep_enabled` is set (BL-931's own default-off
    /// discipline), so the Empire span and every fixture that never opts in
    /// is untouched regardless of this field's value; zero disables the
    /// purchase even where upkeep runs.
    int64_t post_road_treasury_cost = 30000000;

    /// BL-942 — TWO WAYS TO BE STRONG. Per-mille weight applied to the creed-
    /// derived lean (`consolidator_lean_q`/`expansion_lean_q`, history_sim.cpp)
    /// when it boosts `choose_exploration_node`'s `throughput_bound` term (for
    /// a consolidator) or its `coastal_holdings`/`ground_port`/`subject_held`
    /// terms (for an expansionist) — EXPLORATION.md sec Two ways to be strong:
    /// "neither is a flag, neither is chosen." A WEIGHTING of existing scorer
    /// terms, not a new one; 0 makes every polity's tree choice creed-blind,
    /// which is the falsifiable case the doc's own open question names. FIRST
    /// CUT, UNMEASURED.
    int exploration_creed_lean_weight_q = 400;

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

    // --- REACH PROPAGATES THROUGH A NETWORK OF CENTRES (BL-887) -----------
    //
    // Ben, 2026-09-10, watching a run: *"Reach should be propagated by chains
    // of population centres. This is a logistics question no? We can treat
    // logistic points as a side effect of larger population centres too."*
    //
    // WHAT IS STRUCTURALLY WRONG WITH ONE DIJKSTRA FROM ONE CAPITAL. Reach
    // today radiates from a single seat, so a shrinking polity's last holdout
    // can sit permanently outside its own CONTRACTING radius -- it is too far
    // to reach, so it starves, so the realm contracts further, so it is
    // further outside still. `sustainable_campaign_floor_q`'s own comment
    // records that tension, and records that recalibrating the floor
    // (250 -> 80 -> 20) did not move it -- because it is not a tuning
    // problem: a radius has ONE origin, and the map has many towns on it.
    //
    // WHAT THIS REPLACES IT WITH: A RELAY, WHICH IS WHAT A CHAIN IS. A region
    // carrying `centres >= centre_reach_min_centres` refunds part of the cost
    // that reached it before relaxing its neighbours, so reach hops town to
    // town along the roads `rebuild_reach` already discounts rather than
    // decaying monotonically from the seat. A distant province with a real
    // town on it stays reachable in a way a bare frontier march does not --
    // which is CIVILISATION.md § Centres are derived by supply and governance
    // read in the other direction, and it is the ../economy/LOGISTICS.md
    // § Logistic Points shape: *"cities generate it"*, a node-generated rate
    // rather than a per-actor haul allowance, with a bigger node generating
    // more. LOGISTICS.md's rule 2 ("adopt the node half; refuse the link
    // half") is why this is a NODE discount and never a second distance
    // budget laid over `road_traversal_multiplier`.
    //
    // THE FEEDBACK LOOP IS THE HAZARD, AND IT IS DAMPED BY CONSTRUCTION.
    // Centres are grown BY supply (`sustainable_settlement_floor_q`, BL-872)
    // and would now also GENERATE it, which is a loop that could run away or
    // oscillate. Four things stop it, and none of them is a magic number:
    //
    //   1. THE REBATE IS A FRACTION OF COST ALREADY ACCRUED, never a credit.
    //      A centre nothing has reached has accrued nothing and refunds
    //      nothing, so a centre CANNOT BOOTSTRAP ITS OWN REACH FROM NOTHING.
    //      The capital itself stands at cost 0, so its own rebate is 0.
    //   2. THE FRACTION IS CAPPED STRICTLY BELOW 1 (`..._rebate_cap_q` < 1000).
    //      Cost along a chain of k centres therefore decays geometrically and
    //      is bounded below by zero rather than turning negative, so no edge
    //      is ever traversed for free and no cycle can pump cost downward.
    //      This is also what makes the relaxation below TERMINATE.
    //   3. THE READING IS ONE DECISION ROUND STALE. Centres are promoted in
    //      the demography loop off LAST round's `network_supply_q`; a rebuild
    //      reads the centre map as it already stands and never re-enters it.
    //      There is no recursion inside a rebuild.
    //   4. THE OUTPUT IS CLAMPED. `network_supply_q` is clamped to [0, 1000],
    //      so the loop's gain saturates: past full supply, more centres buy a
    //      region nothing at all and the two floors stop moving.
    //
    // DEFAULTS OFF. The single-capital path stays live and stays the default
    // so no existing fixture changes meaning; `era_minus_one.cpp` switches it
    // on for generation's own round, and `history_sweep`'s `--set` carries
    // every field below so the two models can be A/B'd on one build.
    bool centre_chain_reach = false;

    /// Per-mille of accrued cost a SINGLE centre refunds. The LP analogy's
    /// "bigger node, more throughput": the fraction scales with how many
    /// centres stand in the region, up to `centre_reach_rebate_cap_q`.
    /// A placeholder magnitude on the same footing as the w_* weights --
    /// the SHAPE is the design, `history_sweep` tunes the number.
    int centre_reach_rebate_q = 150;

    /// Hard ceiling on the combined rebate, per mille. MUST STAY BELOW 1000,
    /// and is what makes damping point 2 above true rather than hoped for: at
    /// 1000 a large enough town would carry reach onward for free and the
    /// network would extend without bound.
    int centre_reach_rebate_cap_q = 500;

    /// Centres a region must carry before it relays at all. At 1, every
    /// region with any town on it is a node of the network, which is the
    /// reading CIVILISATION.md § The road is the empire's skeleton uses (a
    /// network "whose nodes are seats and whose edges are roads"). Raise it
    /// to make the network sparser -- only real cities relay.
    int centre_reach_min_centres = 1;

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

    /// SETTLE IS RE-SETTLEMENT IN THE EMPIRES ROUND (BL-894; Ben, 2026-09-11:
    /// "For our stage, land should already be settled. Settle in our stage is
    /// just a consequence of when polities decide to raze settlements and
    /// destroy cultures."). When true, the Settle verb may only fire from a
    /// held region that carries `centres_razed > 0` -- ground somebody has
    /// already harmed. No razing, no settling.
    ///
    /// WHY THIS RATHER THAN A WEIGHT. BL-889 measured Campaign in the running
    /// 5,204 rounds per world and winning 61 of them -- 1.2%. Nothing blocks
    /// war: the reach gate denies nothing, the water gate accounts for 2% of
    /// contacts, and 90% of conquered ground is taken once and kept. Peaceful
    /// expansion is simply always available and always cheaper, because Settle
    /// MANUFACTURES new ground on population pressure alone and never asks
    /// whether there is anywhere to go. Raising `w_campaign` or lowering
    /// `campaign_threshold_q` would be a term inside the actor -- forbidden by
    /// the standing rule, and the shape Ben declined for the reach gate
    /// (NR-823). This changes what is LEGAL, not what is preferred, and leaves
    /// the scorer honest.
    ///
    /// FALSE BY DEFAULT, DELIBERATELY. The struct default spans a 4000 BCE ->
    /// 0 CE arc that INCLUDES the migration era, and `COLONISATION.md` owns a
    /// diffusion that has to fill the map. Generation's own Empires round sets
    /// it true (`era_minus_one_sim_params`); harnesses measuring the old arc
    /// keep the old rule, so no existing fixture changes meaning under it.
    ///
    /// THE FROZEN REGION COUNT IS THE POINT, NOT A COST. The Settle block's
    /// own comment justifies the verb by "without this verb a 2000-year run has
    /// a frozen region count" — a MIGRATION-era argument running inside an
    /// EMPIRE-era round. A settled map with a fixed region count is this
    /// phase's premise (`CIVILISATION.md` § The arc the phase must produce).
    bool settle_requires_razed_ground = false;

    // --- BL-920: the opening seeds culture ground, not polities -----------
    //
    // CIVILISATION.md sec "A city state spawns where a region's population
    // is above a threshold" (Ben, 2026-09-11, ruling on NR-835): "City states
    // spawn in provinces above a threshold population... 300,000 heads,
    // provisional." A region above this line IS a city state -- at the
    // opening and whenever it crosses the line later; everything else is
    // unorganised ground of its culture (`region::nation == -1`), never a
    // free holding of the plurality polity as the old one-per-culture seed
    // read it.
    //
    // FALSE BY DEFAULT, DELIBERATELY -- the same idiom
    // `settle_requires_razed_ground` beside it uses. Dozens of
    // `history_sim_harness` fixtures (`two_polity_world`,
    // `one_polity_two_regions`, `road_chain_world`...) build a synthetic
    // world whose whole premise is "one polity per founding culture, holding
    // every region of it, from year zero" -- BL-826's seed, which this item
    // supersedes for GENERATION's OWN round but must not silently rewrite
    // underneath every harness that isolates a DIFFERENT mechanism (BL-837
    // garrison maintenance, BL-922 supply pricing, the road discount...) on
    // top of that premise. Generation's own round sets this true
    // (`era_minus_one_sim_params`); every existing fixture keeps the old
    // seed, unchanged in meaning, by never setting it.
    bool city_states_by_population_threshold = false;

    int64_t city_state_population_threshold = 300000;

    /// ORGANISE is priced by KINSHIP (`culture_opposition_q`, 0 = own people,
    /// 1000 = fully opposed): own culture cheapest, kin dearer, and a people
    /// at or above this bar cannot be organised at all -- only conquered
    /// (CIVILISATION.md sec "The unit is the city state"). A placeholder on
    /// the same footing as the w_* weights beside it -- the SHAPE is the
    /// ruling, history_sweep tunes the number.
    int organise_opposition_bar_q = 700;

    /// Materials the polity's capital pays to organise one region of its own
    /// culture (op_q == 0); a kin people costs proportionally more, up to
    /// double at the opposition bar itself. Same footing as
    /// `road_build_material_cost` beside it in `era_minus_one_sim_params`.
    int64_t organise_material_cost = 400;

    /// Growth over unorganised ground is gated exactly like holding it
    /// (`sustainable_settlement_floor_q`) -- BL-922's "the network must
    /// supply the ground" reach test, asked of the CANDIDATE region rather
    /// than of ground already held.
    int organise_reach_floor_q = 40;

    /// AMPHIBIOUS CAPTURE BY WEIGHT (BL-893; Ben, 2026-09-10: "units can move
    /// into and capture coast tiles if the adjoined land province has a higher
    /// population count than the defender"). When true, the BL-778 water gate
    /// gains a fourth escape: a wet contact is legal if the attacker's STAGING
    /// region carries strictly more population than the target.
    ///
    /// ADMISSIBLE UNDER THE STANDING RULE because population in adjacent ground
    /// is a FORCE WITH A VISIBLE CAUSE ON THE MAP -- the player can see it, and
    /// it is symmetric: the same rule lets a heavy neighbour take from you. It
    /// is not a term inside an actor and does not scale with rank.
    ///
    /// THE CROSSING STILL STARVES, and that is not an oversight. `forages` is
    /// `dry || shore`, so a force that crosses on weight alone feeds on nothing
    /// -- exactly as one carried by ships does. Ships get a force there; they do
    /// not feed it, and neither does numbers. Without that, coastal ground would
    /// be CHEAPER to take than inland ground, which inverts the intent.
    ///
    /// STRICTLY GREATER, so a tie fails, matching how ties resolve elsewhere.
    ///
    /// REGION, NOT PROVINCE, and this is a KNOWN DIVERGENCE from Ben's wording.
    /// He said "province"; the sim indexes regions and has no province grain at
    /// this point in the pass. Raise it if the coarser unit is wanted.
    ///
    /// False by default; generation's Empires round sets it true, so no existing
    /// fixture changes meaning.
    bool amphibious_weight_crossing = false;

    /// SEA LEGS: THE REDUCED RATION A CROSSING LANDS ON (BL-899; Ben,
    /// 2026-09-11 — docs/lore/CREEDS.md § Sea legs). Per-mille of the supply an
    /// ordinary foraging force would have drawn, awarded at FULL sea legs; the
    /// ration actually paid is this scaled by the staging region's own
    /// `sea_legs_q`, so a creed with deep sea legs lands nearly fed and one
    /// with shallow sea legs lands hungry but alive.
    ///
    /// WHY A SCALED RATION AND NOT FORAGE. BL-893 opened the water gate on
    /// weight and changed no outcome, because `forages` is `dry || shore` and a
    /// weight crossing is neither -- so every crossing fought at zero supply and
    /// lost the verb contest. Restoring full forage is the wrong repair: it
    /// makes coastal ground CHEAPER to take than inland ground, inverting the
    /// intent (see `amphibious_weight_crossing` above, which says the same). A
    /// flat allowance is wrong for a different reason -- it is a switch where
    /// the design asks for a spectrum a player can read.
    ///
    /// NOT A TERM INSIDE AN ACTOR. This is read at the SUPPLY calculation, by
    /// both the scorer and execute, off the ground and the people standing on
    /// it. Nothing is added to a campaign's score.
    ///
    /// Zero by default -- the whole mechanism is off, and no existing fixture
    /// changes meaning. Generation's Empires round sets it
    /// (`era_minus_one.cpp`).
    int sea_legs_ration_q = 0;

    /// The sea legs a staging region's people must carry before the ration is
    /// paid at all, 0-1000 (BL-899).
    ///
    /// "NONE STILL STARVES" is the design's third rung and this is what makes
    /// it true rather than asymptotic. Without a floor, a landlocked people
    /// with a token storm god would land on a thin-but-nonzero ration, and the
    /// ordinary case -- a creed with no tradition of the water trying a
    /// crossing -- would stop being refused-or-beaten exactly as it is today.
    int sea_legs_floor_q = 0;

    /// The `region::port_q` a staging region must reach for its people to use
    /// the sea legs they earned, 0-1000 (BL-899).
    ///
    /// THE POLITY HOLDS IT, BUT ONLY FROM A PORT (CREEDS.md § Sea legs). A
    /// realm that inherits a seafaring people and holds no water cannot use
    /// what it inherited, and that is the second of the two conditions the
    /// design requires at the crossing -- the first being that the people on
    /// the staging ground carry the tradition in the first place.
    ///
    /// This is NOT the naval test and does not replace it: a crossing still
    /// requires `can_field_naval` at the polity's military band, because the
    /// two answer different questions (can this realm put hulls on the water /
    /// can these people feed a force once it lands). Keeping them independent
    /// is what stops an ancient people out-raiding a realm that has actually
    /// reached the naval rung.
    int sea_legs_port_q = 0;

    /// TRADE INCOME FROM THE NETWORK (BL-895; Ben, 2026-09-11: "we also need a
    /// simple cost for war, and this cost can be sourced by rich trade").
    /// Materials yielded per YEAR, per held region, per DISTINCT KIND OF
    /// UNLIKE GROUND its realm reaches (BL-1021). Zero disables it.
    ///
    /// THE BASE IS KINDS REACHED, NOT PAIRS ROADED (BL-1021; Ben, 2026-09-16,
    /// NR-827 option 2). BL-895 first paid this per walked corridor joining
    /// unlike ground, and measured it at 0.25% of production: a per-link
    /// income is bounded by a region's ADJACENCY, so it could never grow with a
    /// realm the way industry does. Now every held region the realm's network
    /// reaches (`network_supply_q` above `sustainable_settlement_floor_q`, the
    /// floor that already means "a network that can carry ordinary trade")
    /// trades with every KIND of ground the realm reaches that is unlike its
    /// own. A KIND is `region_trade_class` in history_sim.cpp -- what the
    /// ground is best at: farm, ore or port. An amicable neighbour's reached
    /// ground joined by a walked corridor (BL-925) is ground the realm reaches.
    /// The magnitude was NOT moved by the re-base (40 in era_minus_one.cpp);
    /// only what it multiplies changed.
    ///
    /// NO MARKET, AND THAT IS THE WHOLE CONSTRAINT. `CIVILISATION.md` sec
    /// Materials are spent when something happens keeps this phase free of an
    /// order book, a firm, a building stack and a price, and Ben re-affirmed
    /// that exclusion on 2026-09-11 when offered the chance to reopen it. So
    /// the income is a property of the NETWORK rather than of a market: only
    /// DIFFERENCE is read -- that two places hold unlike things -- never a
    /// price for either, and never a quantity that behaves like one.
    ///
    /// WHAT IT BUYS, and each is a job nothing else does:
    ///   - roads become worth building for a reason other than reach, so BL-837
    ///     stops being the only argument for a network;
    ///   - war becomes affordable to the rich and unaffordable to the poor,
    ///     which is asymmetry in campaigning WITHOUT a term inside any actor;
    ///   - network failure becomes EXPENSIVE, which is what gives BL-896's
    ///     collapse-by-network-failure its teeth: a realm whose roads fail
    ///     loses its income before it loses its ground.
    ///
    /// PAID TO THE REGION'S OWN SEAT, like industry. A realm that reaches one
    /// kind of ground earns nothing however large it is -- only DIFFERENCE is
    /// read, never a quantity -- and a region the network cannot reach neither
    /// earns nor lends its kind to anyone else. Roads pay through REACH
    /// (`rebuild_reach` discounts walked corridors), not per edge.
    int trade_income_per_class = 0;

    /// TRADE CROSSES A BORDER, BETWEEN AMICABLE NEIGHBOURS (BL-925; Ben,
    /// 2026-09-11: "amicable neighbours trading"). Before this a realm's own
    /// borders were the whole world trade could see -- BL-895's income never
    /// crossed a border, so "amicable neighbours trading" had no meaning in
    /// the sim (NR-827: trade at 0.25% of production).
    ///
    /// TWO POLITIES ARE AMICABLE when their cultures are KIN -- opposition
    /// below `organise_opposition_bar_q`, the SAME bar Organise already
    /// refuses at -- AND NEITHER HOLDS A BITING GRUDGE against the other.
    /// A walked corridor joining UNLIKE ground across an amicable border
    /// opens a link. Since BL-1021 the link pays nothing per edge: it makes
    /// the far side's reached ground a KIND each realm reaches, so both seats
    /// are paid through `trade_income_per_class` -- the same income extended
    /// across a new kind of edge, not a second economy. No market, no price:
    /// the no-market ruling (`trade_income_per_class`'s own comment) stands.
    ///
    /// A grudge raised on EITHER side closes the link until it decays below
    /// this bar again -- read both directions of `grudge_between`, since a
    /// one-sided wrong is enough to sour a border. Zero disables the grudge
    /// gate entirely (kinship alone decides). PLACEHOLDER, same footing as
    /// the `w_*` weights: half of `grudge_ground_taken` (300), so one ground
    /// exchange bites but the noise floor near `grudge_floor` (4) never does.
    int trade_grudge_bar_q = 150;

    /// THE STANDING ARMY EATS, EVERY YEAR (BL-895 sink 1; Ben, 2026-09-11).
    /// Materials charged per YEAR per 1000 heads standing in `army_stock`,
    /// drawn from the region's seat. Zero disables it.
    ///
    /// WHY IT HAD TO EXIST. Before it, campaigns were the ONLY thing materials
    /// were ever spent on -- one site in the whole sim, spending in the
    /// HUNDREDS against hundreds of millions produced. An income nothing
    /// competes for cannot make war affordable to the rich and unaffordable to
    /// the poor however large it is, so the trade income of BL-895's first half
    /// could not gate anything. A standing army is now a PERMANENT claim on
    /// production rather than a free accumulation, which makes a large one a
    /// decision.
    ///
    /// IT IS ALSO THE STRANGLING CHANNEL. A realm whose seat cannot pay does
    /// not simply run a deficit -- the unpaid share of its garrison goes home
    /// (`unpaid_army_disband_q`), so a realm cut off from its income loses its
    /// army without losing a battle. That is the second job BL-896 needs.
    int army_upkeep_per_1000_heads = 0;

    /// Per-mille of the UNPAID share of a garrison that goes home this year.
    /// The heads return to `manpower_stock`, exactly as `muster_garrison`'s
    /// over-target disband does and for the same reason: a discharged soldier
    /// was never subtracted from `population`. Zero means an unpaid army
    /// stands anyway, which makes `army_upkeep_per_1000_heads` a pure drain.
    int unpaid_army_disband_q = 0;

    /// ROADS COST MATERIALS TO BUILD (BL-895 sink 2; Ben, 2026-09-11).
    /// Charged to the acting polity's seat each time a walked corridor would
    /// cross a TIER boundary (0 -> Track, Track -> Road). Zero disables it,
    /// which is the historical behaviour: a road was a free side effect of
    /// walking a corridor often enough.
    ///
    /// IT CLOSES A LOOP. Trade pays for the network and the network carries
    /// trade, which turns "the wall moves when you win" from a geographic
    /// statement into an economic one: a realm that cannot afford roads cannot
    /// extend its reach however much ground it takes.
    ///
    /// A REFUSED PROMOTION IS NOT A LOST WALK. The use count is held one short
    /// of the threshold rather than discarded, so the corridor is promoted the
    /// next time it is walked WITH the materials standing. The road is delayed
    /// by poverty, never forbidden by it.
    int road_build_material_cost = 0;

    /// GROUND THE REALM CANNOT REACH SECEDES (BL-896; Ben, 2026-09-11, ruling
    /// from four candidates -- succession, exhaustion and external shock were
    /// all offered and none was chosen). A held region whose
    /// `region::network_supply_q` has fallen to or below this floor is ground
    /// the seat can no longer rule. Zero disables it.
    ///
    /// IT DOES NOT FALL TO WHOEVER CAN REACH IT, and that choice is the arc's
    /// rather than a detail: ground lost to a neighbour CONCENTRATES the map
    /// and works against everything this phase is for, while ground that
    /// secedes becomes a SUCCESSOR -- a new polity holding real ground,
    /// carrying its own people's culture. That is what `../generation/
    /// CIVILISATION.md` sec What the dark age must leave asks the phase to hand
    /// forward: nations of unequal strength, some able to colonise and some
    /// only to be colonised.
    ///
    /// MECHANICAL, NEVER SCHEDULED. No collapse fires on a date or a counter.
    /// A realm fragments because a specific region's supply fell under a floor,
    /// for reasons a player could read off the map -- distance, terrain, a road
    /// never built, a war that emptied the ground between.
    ///
    /// A SEPARATE FLOOR FROM THE OTHER TWO, and deliberately so. It is the
    /// same 0-1000 reach currency `sustainable_garrison_floor_q` (an army on
    /// this ground starts to waste) and `sustainable_settlement_floor_q` (its
    /// towns stop growing) are read in, but it answers a third question: can
    /// the SEAT still rule here. Callers site it where the arc they want puts
    /// it; nothing in this file orders the three.
    int secession_supply_floor_q = 0;

    /// UNUSED BY THE FRAGMENTATION LOGIC SINCE BL-923 (Ben, 2026-09-11, ruling
    /// NR-837). The build-time call this dial encoded -- a contiguous
    /// cut-off block of at least this many regions leaves together -- is
    /// reversed: the unit is now the CITY STATE, one per cut-off seat (with
    /// its hinterland), and a seatless cut-off region joins the nearest
    /// cut-off seat or stands alone, down to size one. Left in place only so
    /// existing callers (`era_minus_one.cpp`, `history_sweep.cpp`'s CLI) keep
    /// compiling; setting it no longer changes fragmentation behaviour.
    int secession_min_regions = 2;

    // --- BL-897: A CREED THAT SPANS CULTURES ------------------------------
    //
    // `../../docs/lore/CREEDS.md` § The four calls, settled (Ben, 2026-09-11).
    // Every dial below defaults to ZERO/off, and the first one is the master
    // switch: with `universal_creed_humbled_cohesion_q` at 0 no creed is ever
    // coined, no polity adopts, no ground holds, and every existing fixture
    // means exactly what it meant before.
    //
    // IT ARISES FROM HUMILIATION AND FROM DENSITY, NEVER FROM A ROLL OR A DATE.
    // Both causes are read off scalars this sim already computes every decision
    // round, which is the standing constraint on the whole phase — a creed that
    // fired on a year, or on a die, would be a term inside an actor rather than
    // a force with a visible cause.

    /// HUMILIATION, read as cohesion. A realm may coin a universal creed only
    /// while `polity::cohesion_q` stands at or below this. Zero disables the
    /// WHOLE mechanism (the master switch).
    ///
    /// THE OBVIOUS READING RUNS THE OTHER WAY and it is the wrong one. A
    /// triumphant empire declaring its god universal is the intuitive picture;
    /// Ben ruled against it, because a creed that is FOR EVERYONE answers a
    /// question the victorious do not have. So the input is the realm's own
    /// battering: cohesion falls on every defeat and on every secession through
    /// `cohesion_loss_on_defeat_q`, which means BL-896's fragmentation is what
    /// feeds this — no new event, and nothing scheduled.
    int universal_creed_humbled_cohesion_q = 0;

    /// DENSITY, read as TRADE LINKS. The realm must hold at least this many
    /// unlike-ground pairs joined by a walked corridor — the pairs BL-895 first
    /// paid on. BL-1021 moved the INCOME to kinds of ground reached; this gate
    /// still counts corridors, because what it reads is CONTACT, not income.
    ///
    /// AN ANSWER FOR EVERYONE NEEDS EVERYONE TO BE IN CONTACT. The network is
    /// what carries a creed, so the density reading is the network measured
    /// exactly where it already pays: a walked corridor between two places that
    /// hold different things is contact between two peoples who need each
    /// other, which is the only kind of contact this phase models. Counting the
    /// links rather than the materials keeps the gate free of the income
    /// magnitude, which is a balance number and moves.
    ///
    /// A WORLD OF INTACT, ISOLATED REALMS PRODUCES NO UNIVERSALISING CREED, and
    /// CREEDS.md calls that "a legitimate world rather than a failed one". This
    /// gate is where that outcome comes from, and it is REPORTED, never gated.
    int universal_creed_min_trade_links = 0;

    /// The mean `region::network_supply_q` across the realm's held ground must
    /// also reach this. The link count says the network is BUSY; this says it
    /// still REACHES — a realm whose corridors carry trade between two clumps
    /// it can no longer supply is not one place, and a creed that spreads along
    /// contact has nothing to travel. Zero means the reach half is not asked.
    int universal_creed_network_floor_q = 0;

    /// HOW LONG A PEOPLE CARRIES BOTH before the pair resolves, in years
    /// (advanced by `step_years`, like `region::mix_years`).
    ///
    /// CONVERSION IS NOT A FLIP. A people carries its pantheon AND the creed
    /// for a span, and only then does the pair settle — the pantheon fades to
    /// residue, or it REASSERTS and the people falls back out. Zero collapses
    /// the span to a single round, which is the model Ben explicitly rejected;
    /// a caller that wants the mechanism at all should site this above zero.
    int universal_creed_hold_years = 0;

    /// WHICH WAY THE PAIR SETTLES, as a floor on the ground's own binding.
    /// Binding is `region::network_supply_q` less
    /// `universal_creed_alien_penalty_q` where the ground's plurality culture
    /// is not the realm's own. At or above this floor the pantheon fades to
    /// residue (CONVERTED); below it the pantheon reasserts and the people
    /// falls out (REASSERTED).
    ///
    /// NOT A ROLL, AND THE TWO TERMS ARE THE FAULT LINE ITSELF. Distance is one
    /// half — far ground converts less — and DIFFERENCE is the other, which is
    /// the axis CIVILISATION.md § Culture relations could not supply, because
    /// kinship there is computed from descent alone. An empire's near, kindred
    /// ground converts and its far, alien ground refuses, so the split, when a
    /// later slice draws it, falls where a player could have seen it coming.
    int universal_creed_convert_supply_q = 0;

    /// How much binding a people loses for not being its realm's own culture.
    /// Zero makes conversion purely a question of distance.
    int universal_creed_alien_penalty_q = 0;

    /// BL-944 — THE SCHISM VERB. A realm's REASSERTED ground (`region::
    /// creed_hold == 3`) that is also ALIEN to the realm's own culture — its
    /// `creed_residue_culture` differs from `polity::culture` — is exactly
    /// CREEDS.md's fault line: an institution the realm adopted standing over
    /// a people who answered with their own pantheon instead. This many held
    /// regions of the SAME residue culture reasserting is what it takes for
    /// that people to break away as their own polity, culture-grouped rather
    /// than reach-grouped so the break can never be the same event as
    /// BL-896/BL-923's network-failure secession. Zero disables the verb
    /// entirely (reassertion still happens; it just never fractures a realm).
    int schism_min_regions = 0;

    /// A CREED'S APPETITE FOR WAR, LEANING THE CAMPAIGN SCORE (BL-868;
    /// CIVILISATION.md sec Armies come from creeds, and only some peoples raise
    /// them). Per-mille pull, applied proportionally and SYMMETRICALLY around a
    /// neutral 500: a polity whose founding culture's `aggression_q` is 1000
    /// scores a campaign `w_aggr_q` per-mille higher, one at 0 scores it that
    /// much lower, and one at 500 is unmoved. Zero disables it.
    ///
    /// THE INPUT IS ALREADY EARNED, WHICH IS WHAT MAKES THIS ADMISSIBLE.
    /// `culture::aggression_q` derives from the pantheon's war god
    /// (`../lore/CREEDS.md`), and COLONISATION.md sec Culture arrives by route
    /// made the distribution of pantheons a RECORD OF ROUTES. So which peoples
    /// turn warlike is downstream of where their ancestors walked. This is not
    /// a dial on an actor; it is a consequence of that actor's history, read at
    /// the one place the decision is made.
    ///
    /// SAME IDIOM AS `w_cult` AND `w_dist`: a proportional lean on a value that
    /// already exists, never a new term added beside the score.
    ///
    /// NOT A NEW AI GRANT. A polity choosing to campaign is the Era -1 sim's
    /// existing scored-utility verb set, covered by the dated register in
    /// `../ai/AI_OPPONENT.md` sec 11 and bound by the same constraints: pure,
    /// seeded, deterministic, replayable, only legal verbs, never a planner.
    int w_aggr_q = 0;

    /// FEAR OF BEING NEXT, LEANING THE CAMPAIGN SCORE (BL-838; AI_OPPONENT.md
    /// sec 11, the grant "The Era -1 scorer may read the GRUDGE LEDGER, scoped
    /// to fear of annihilation", Ben, 2026-09-11). Per-mille pull. Zero
    /// disables it, and zero is the struct default, so no existing fixture
    /// changes meaning.
    ///
    /// THE QUESTION IT ASKS IS "WILL OTHERS ATTACK ME FOR FEAR OF BEING WIPED
    /// OUT NEXT". When polity D scores a campaign against polity A, it reads
    /// the grudges held AGAINST A BY THIRD PARTIES OF D'S OWN PEOPLE -- what A
    /// has demonstrably done to peoples like D -- and leans the prize upward in
    /// proportion. A becomes a target because of its BEHAVIOUR.
    ///
    /// THE THREE EXCLUSIONS ARE THE GRANT, NOT AN IMPLEMENTATION CHOICE, and
    /// each of them is load-bearing:
    ///   - D NEVER READS ITS OWN GRUDGE against A. That is the revenge term
    ///     BL-827 declined -- a term inside the actor rather than a fact about
    ///     the world -- so `from == D` is skipped explicitly in `fear_of_next_q`.
    ///   - NOTHING HERE READS SIZE OR RANK. "The largest polity" is not the
    ///     trigger; "the polity that has been doing this to people like me" is.
    ///     A quiet reimplementation as a size coefficient would pass every
    ///     obvious check and violate the exact rule this grant exists to
    ///     satisfy, which is why the check is behavioural: a large but PEACEFUL
    ///     polity must attract no lean, a smaller AGGRESSIVE one must.
    ///   - NO TREATIES, NEGOTIATION OR ALLIANCE OBJECTS. This is a fear
    ///     response inside a generation sim, not a diplomacy layer, and
    ///     BL-827's two unbuilt grudge kinds stay unbuilt.
    ///
    /// ONE-SIDED, WHERE `w_aggr_q` IS SYMMETRIC, and that is deliberate rather
    /// than an omission. `aggression_q` has a genuine neutral at 500 with a
    /// meaningful half below it, so leaning both ways says something true. A
    /// grudge ledger's neutral is ZERO -- the absence of a record -- and there
    /// is no "less than no wrongs done". Leaning a blameless target DOWN would
    /// be a peace bonus nobody granted, and it would make the peaceful-polity
    /// half of the check pass for the wrong reason. So a clean target is
    /// UNCHANGED from baseline, which is what "attracts no coalition" means.
    ///
    /// SAME IDIOM AS `w_cult`, `w_dist` AND `w_aggr_q`: a proportional lean on
    /// a value that already exists, never a new term added beside the score.
    int w_fear_q = 0;

    /// A WANT POINTS A CAMPAIGN OUTWARD (BL-953; EXPLORATION.md sec A want
    /// points a campaign outward). Per-mille pull on the campaign prize by the
    /// DECIDER's own want for the target region's dominant good
    /// (`polity_good_want_q`): `value += value * w_want_q * want_q / 10^6`,
    /// applied once, outside the season loop, only to a positive prize -- a
    /// want RANKS winnable campaigns, it never makes an unwinnable one
    /// attractive. The same want ranks which native a sea-legged power
    /// subjects first (stable tie-break on the lower id).
    ///
    /// Read only while `exploration_upkeep_enabled` is set (the scarcity
    /// signal it reads exists only then). ZERO IS THE STRUCT DEFAULT AND THE
    /// IDENTITY: at 0 neither the scorer nor subjection moves, so the Empires
    /// span is byte-identical; only `exploration_sim_params` sets it.
    ///
    /// NOT A NEW AI GRANT: it leans the existing campaign/subjection verbs'
    /// scored utility with a richer input, same idiom as `w_fear_q`/`w_aggr_q`.
    int w_want_q = 0;

    /// The grudge total, summed across D's aggrieved kin, that counts as FULL
    /// fear -- the denominator that turns an unbounded ledger sum into the
    /// 0-1000 currency every other lean in this file speaks.
    ///
    /// A PLACEHOLDER MAGNITUDE on the same footing as the `w_*` weights and
    /// `grudge_ground_taken` beside it. 2000 is roughly two sacked seats, or
    /// seven taken regions, held recently enough to have survived
    /// `grudge_decay_per_year_q` -- i.e. a polity in the middle of a career of
    /// conquest against this people, not one that fought a war once. Raising it
    /// makes fear rarer and later; lowering it makes every border raid read as
    /// an existential threat.
    int fear_reference = 2000;

    // --- THE HISTORICAL TURBULENCE LEAN (BL-839) ---------------------------
    //
    // Ben, 2026-09-08: "we are really looking to encourage historical
    // turbulence as a parameter -- so that players can roll for worlds which
    // have fewer countries at this step, or more countries at this step."
    //
    // IT TUNES FORCES AND TARGETS NO COUNT, AND THAT IS THE WHOLE OF ITS
    // DESIGN, not a caveat on it. `../generation/GENERATION_STRATEGY.md`
    // sec Asymmetry is the deliverable and sec The asymmetry is POLITICAL both
    // settle the same rule -- generation is answerable for the DISTRIBUTION of
    // an outcome across a spread of worlds and for nothing at all about any
    // single one -- and the 2026-07-30 emergent-nation-count ruling and
    // BL-224's non-hegemony invariant say the same thing from the other side.
    // So there is deliberately NOTHING here that counts polities, and nothing
    // downstream may acquire a post-hoc correction that does: "fewer countries"
    // is the SHAPE the player is buying odds on, never a quota the sim is told
    // to hit. A calm world that happens to fragment is a correct calm world.
    //
    // IT LEANS THREE FORCES THAT ALREADY EXIST, AND ADDS NO FOURTH. Each was
    // built by a separate item for its own reasons; what this adds is one
    // player-facing axis that moves all three the same way, because they are
    // the three that decide whether conquest COMPOUNDS:
    //
    //   1. THE SPREAD OF CULTURE AGGRESSION (`turbulence_aggression_spread_q`,
    //      applied wherever `culture::aggression_q` enters the sim). A SPREAD,
    //      not a dial -- widening it does not make every people warlike, it
    //      makes the warlike ones more so and the placid ones more placid.
    //      Two archetypes fall out of the same engine at the wide end, and
    //      they are different SHAPES rather than two ends of one axis: a
    //      people that EXPANDS AND INCORPORATES, whose neighbours are placid
    //      enough to be absorbed and kept, and a world that CYCLES, where
    //      several warlike peoples unify, stabilise, fragment and re-unify.
    //      Which one a given world shows is decided by where its cultures
    //      actually fell, which is a fact about its own migration
    //      (`../generation/COLONISATION.md` sec Culture arrives by route) --
    //      so it is a consequence, not a setting.
    //   2. HOW SHARPLY NEIGHBOURS COALESCE AGAINST A RISER
    //      (`turbulence_fear_q`, scaling `w_fear_q`). Strictly a scale on the
    //      existing lean, so it inherits BL-838's grant WHOLE and widens it
    //      nowhere: still third-party grudges only, still never size or rank,
    //      still never the scoring polity's own grudge. A scale of zero is the
    //      force switched off, which is the struct default's meaning too.
    //   3. HOW FAST REACH DECAYS WITH DISTANCE (`turbulence_reach_cost_q`,
    //      scaling `terrain_reach_cost_q`). BL-316's burden-of-breadth term:
    //      dear reach means a conquest held far from the seat feeds itself
    //      badly, so an empire that overruns its supply cannot keep what it
    //      took. Cheap reach means it can.
    //
    // WHY ALL THREE AND NOT ONE. Turbulence is not a quantity of war; a world
    // can run hundreds of conquests and end the shape it started
    // (GENERATION_STRATEGY.md sec The asymmetry is POLITICAL, on conquest that
    // does not COMPOUND). Wide aggression supplies the attempts, fear supplies
    // the counter-pressure that turns a rise into a peak, and dear reach
    // supplies the fall. One alone moves volume; the three together move the
    // SHAPE of the arc, which is what the player is actually choosing between.
    //
    // ORDINARY IS ZERO AND ZERO IS THE STRUCT DEFAULT, so every fixture in the
    // repo keeps its meaning and no existing measurement is re-based by this
    // item landing. The three magnitudes below are placeholders on the same
    // footing as the `w_*` weights beside them -- `history_sweep --set` tunes
    // them, and the DONE-WHEN is that the three settings visibly move the
    // spread of outcomes, never that they hit a number.

    /// -1 calm, 0 ordinary, +1 turbulent. Deliberately a three-valued axis and
    /// not a continuous slider: the player has to be able to tell what they
    /// rolled, and a raw per-mille dial on a force nobody can see is a control
    /// that cannot be read back off the world it made
    /// (`../ui/STARTUP.md` sec Rounds 4, 5 and 6 -- "you set conditions, you do
    /// not steer"). Values outside -1..+1 are clamped where they are read.
    int turbulence_lean = 0;

    /// Per-mille WIDENING of each culture's distance from the 500 neutral, at
    /// |lean| == 1. At 400: a turbulent world multiplies the deviation by 1.4
    /// and a calm one by 0.6, so a culture already at the neutral is untouched
    /// by either -- which is the property that makes this a spread rather than
    /// a dial, and the reason the no-creeds fallback (a flat 500) is
    /// identically unmoved at every setting.
    int turbulence_aggression_spread_q = 400;

    /// Per-mille scale on `w_fear_q` at |lean| == 1. 500: turbulent x1.5, calm
    /// x0.5. Scaling rather than replacing matters -- a run with `w_fear_q`
    /// at 0 (every isolating fixture, and the struct default) stays at 0 under
    /// every setting, so the lean cannot switch a force ON that its own run
    /// had deliberately switched off.
    int turbulence_fear_q = 500;

    /// Per-mille scale on `terrain_reach_cost_q` at |lean| == 1. 500:
    /// turbulent x1.5, calm x0.5. Same scale-don't-replace property as
    /// `turbulence_fear_q`, and the reason this leans the COST rather than the
    /// BL-837 reach gate's floors: the floors are a wall with a road through
    /// it, and moving a wall by a player setting is a much larger claim than
    /// making the ground dearer to cross.
    int turbulence_reach_cost_q = 500;

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

    // --- BL-929: SUPPLY SITES BOUGHT FROM THE STOCKPILE ---------------------
    //
    // CIVILISATION.md § What materials are FOR; Ben, 2026-09-11: "a polity
    // should be able to upgrade supply sites spending our stockpiled
    // 'industry points'". `build_work` already lets a region's reach relief
    // rise as the incidental yield of `work_score_q`'s argmax, and a
    // corridor already promotes a tier as the incidental yield of enough
    // campaigns having walked it — BL-757 measured the first winning ZERO
    // times across sixteen real generated worlds, and the second is
    // triggered by USE, never by CHOICE. This verb is the deliberate
    // spend neither of those is: a region's own relief or one incoming
    // corridor's tier, bought outright from the capital's `material_stock`,
    // offered wherever a held region's `network_supply_q` sits at or below
    // `sustainable_settlement_floor_q` -- the SAME floor growth and
    // secession already read, so "low" here means exactly what "too far
    // out to grow" and "too far out to rule" already mean elsewhere in this
    // file.
    //
    // Zero cost disables the WHOLE verb -- no candidate is ever offered --
    // which keeps every existing fixture and golden unmoved, exactly as
    // `road_build_material_cost == 0` disables road promotion's own cost.

    /// Materials the capital spends on ONE supply-site upgrade, region or
    /// corridor. The same price for both kinds: the choice between them is
    /// which one buys more `network_supply_q`, not which one is cheaper.
    int64_t supply_upgrade_material_cost = 0;

    /// How far ONE region purchase raises `region::work_reach_mod`, still
    /// clamped at `work_reach_relief_cap_q` by the same read every other
    /// caller of that field already clamps against -- a region already at
    /// the ceiling has nothing left to buy, so the scorer skips it.
    int supply_upgrade_reach_gain_q = 0;

    /// Minimum in the shared currency, like the other four verb thresholds.
    int supply_upgrade_threshold_q = 0;

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

    // -----------------------------------------------------------------------
    // BL-931 — RESUMING A SECOND SPAN ON THE SAME ENGINE (the Exploration
    // phase over the Empire phase's close), rather than seeding a fresh
    // opening.
    // -----------------------------------------------------------------------
    //
    // "The same engine" (EXPLORATION.md sec The engine is shared) means one
    // `run_history_sim`, called a second time, over the polities the first
    // call left alive — not a second simulator and not a fork of this
    // function's opening. `nullptr` (every field's default) is exactly
    // TODAY'S BEHAVIOUR: no existing caller sets any of these, so no existing
    // run's opening or record changes by one bit.

    /// When non-null, `run_history_sim` skips its own opening entirely --
    /// the population/army seeding loop, the polity-construction branch
    /// (BL-826 or BL-920's), the great-power seed, and the seat-placement
    /// pass -- and starts `out.polities` as a COPY of these instead. The
    /// caller owns the matching region state: resuming means `ss.regions` is
    /// already the closing region set (population, army_stock, ownership,
    /// `is_seat`/`seat_region`, all live) -- typically
    /// `pass_one_output::regions`, copied in by the caller before this call,
    /// paired with `pass_one_output::polities` here. `army_stock` therefore
    /// CARRIES rather than being re-seeded at the new span's target
    /// (EXPLORATION.md sec Force persists now: "`army_stock` CARRIES across
    /// the handoff").
    const std::vector<polity>* resume_polities = nullptr;

    /// Grudges/contacts/corridors carried from the prior span's close, folded
    /// into this run's own tables at the top rather than starting empty. Each
    /// is independent of `resume_polities` in principle but is only ever set
    /// alongside it in practice (BL-931's one caller sets all four together).
    const std::vector<grudge>*           resume_grudges   = nullptr;
    const std::vector<contact>*          resume_contacts  = nullptr;
    const std::vector<history_corridor>* resume_corridors = nullptr;

    // -----------------------------------------------------------------------
    // BL-1036 — A SPAN RESUMED FROM A HANDOFF STRUCT LOSES NOTHING THE STRUCT
    // CARRIES (DIGITISATION.md, "The span is its own call, resumed from
    // `exploration_output`").
    // -----------------------------------------------------------------------
    //
    // Three more tables cross beside BL-931's four. Each is null by default,
    // and the Exploration caller leaves all three null, so every existing run
    // -- the shipped Exploration span included -- opens exactly as before.

    /// Treaty clauses and tribute standing at the prior span's close
    /// (`exploration_output::dated_objects`), copied into this run's table at
    /// the top. TRADE FLOWS NEED NO POINTER OF THEIR OWN: round 1's upkeep
    /// rebuilds `trade_flows` from these objects before anything reads a flow
    /// (the upkeep call precedes treaty formation's `pair_trade_value_q`).
    /// Null here re-forms every treaty against an EMPTY table, which inflates
    /// each pair's trade value (it subtracts only OTHER partners' flows),
    /// binds pairs the history never bound, and expires the re-formed set
    /// together one term later. The Exploration caller passes null because
    /// the Empires span forms no treaty to carry.
    const std::vector<dated_object>* resume_dated_objects = nullptr;

    /// The civilisation and universal-creed records that the carried
    /// `region::civilisation`, `region::universal_creed` and
    /// `polity::universal_creed` indices point into. Copied at the top, so a
    /// record coined in this span takes the NEXT free index instead of
    /// reusing 0, and a pair the prior span already settled is found rather
    /// than recorded twice.
    ///
    /// THE EXPLORATION CALLER LEAVES BOTH NULL, AND THAT IS A KNOWN GAP, NOT
    /// A CHOICE: its 1200 resume still restarts both tables at 0.
    /// `pass_one_output` does not carry them, and carrying them there would
    /// drop a twice-recorded civilisation's line from the world log -- a
    /// digest mover, so it rides a re-bless rather than this item.
    const std::vector<civilisation>*    resume_civilisations    = nullptr;
    const std::vector<universal_creed>* resume_universal_creeds = nullptr;

    /// BL-1037 -- A RESUMED CORRIDOR REOPENS AT THE RUNG IT WAS BOUGHT TO.
    ///
    /// Off (the default): a resumed span seeds each corridor's live count from
    /// the record's `uses` -- its WALKS. That is wrong in both directions. A
    /// rung BOUGHT outright (a supply-site purchase, or a post road) sets the
    /// live count straight to its threshold and records ONE walk, so it
    /// reopens at the rung its walks earn -- a bought Track or Road demoted at
    /// 1200, a post road demoted at 1660 and then buyable a second time. And a
    /// walk REFUSED for want of materials is held one short live but counted in
    /// the record, so the corridor reopens promoted without being paid for.
    ///
    /// On: each corridor's live count is seeded so it stands at EXACTLY the
    /// record's `tier` (the rung the prior span's live count stood at when it
    /// closed): `uses` clamped into that rung's band, [threshold(tier),
    /// threshold(tier + 1) - 1], open above for a post road. Both directions
    /// close, and the rung above a post road is priced from where the road
    /// really stands.
    ///
    /// It moves every resumed span, Exploration's included, so it stays off
    /// until the sprint 45 re-bless turns it on (BL-1044).
    bool resume_seeds_corridor_tier = false;

    /// THE TWO 1200 ANCHORS, EXPLICIT (DIGITISATION.md: "Consolidation and the
    /// near-home cutoff stay anchored at 1200"). Before this item both were
    /// read off `start_year`, which is right only for the span that opens at
    /// 1200: a span opening at 1660 would sweep every seat's stores into the
    /// capital a second time, and would call every pair met after 1200 near
    /// home.
    ///
    /// `consolidation_year` -- the ONE decision round on which every held
    /// seat's `material_stock` folds into its capital's treasury
    /// (EXPLORATION.md sec Capital arrives). A span that does not open on it
    /// never consolidates, which is exactly "once".
    ///
    /// `near_home_cutoff_year` -- a pair whose recorded first contact is
    /// strictly before this year is NEAR HOME (BL-941): the Alarm read, the
    /// treaty value's near-home term and its break re-score all key on it.
    ///
    /// INT64_MIN (the default) is "never" for both: no round consolidates and
    /// no pair is near home. Every DECISION read of either sits in the upkeep
    /// step and its treaty block, which run only under
    /// `exploration_upkeep_enabled`; the one other read is a trace-only
    /// diagnostic (BL-950's campaign class). The Exploration caller sets
    /// both to its own start year (`empires_stop_year`), so it is
    /// byte-identical to the `start_year` reads these replace.
    int64_t consolidation_year    = INT64_MIN;
    int64_t near_home_cutoff_year = INT64_MIN;

    // --- BL-1036: the resume-fidelity instrumentation -----------------------
    //
    // Both default off and neither is set by any generation caller. They exist
    // so `digitisation_sim_harness --fidelity` can PROVE a resume loses
    // nothing, rather than assert it: one reads the run, the other lets the
    // harness hand a resume the continued run's own live network.

    /// When a run reaches the TOP of this year -- before the year's first act
    /// -- it copies its whole working state into `history_sim_state::capture`
    /// (see `history_sim_capture`). On a resumed run, `capture_year ==
    /// start_year` is the OPENING state; on a continued run it is the state
    /// that year's round opens on. READ BY NOTHING in the sim: a captured and
    /// an uncaptured run agree in every other output. INT64_MIN = never.
    int64_t capture_year = INT64_MIN;

    /// FIDELITY ORACLE ONLY. On a resumed run, seed the live road counts from
    /// these rows' `uses` verbatim -- a row of zero uses still creates the
    /// edge, because an edge's PRESENCE is itself read ("a walked corridor")
    /// -- instead of from `resume_corridors`. The harness passes a continued
    /// run's `history_sim_capture::live_roads`, which is how it separates what
    /// the handoff's corridor record changes from what the resume itself
    /// loses. Null (every real caller) seeds from `resume_corridors`.
    const std::vector<history_corridor>* resume_live_roads = nullptr;

    // -----------------------------------------------------------------------
    // BL-931 — THE ROUND-LEVEL UPKEEP STEP.
    // -----------------------------------------------------------------------
    //
    // EXPLORATION.md sec The engine is shared names two honest additions the
    // shared engine does not already have. This is the first: "the treasury
    // earns, then pays its stocks, then invests" every round. OFF by default,
    // so the Empire span (every caller before this item) is unaffected bit
    // for bit. There is no treasury yet (BL-932) -- the hook fires every
    // decision round and calls `run_exploration_upkeep`, which is a
    // documented no-op until that item gives it something to earn, pay or
    // invest. Landing the STEP now, empty, is what makes BL-932 a one-line
    // change at that hook rather than a new call site threaded through the
    // round loop from scratch.
    bool exploration_upkeep_enabled = false;

    // --- BL-932: what earns the treasury -----------------------------------
    // EXPLORATION.md sec Capital arrives names four sources; subject tribute
    // (BL-933/934) is not built yet and contributes 0. THE ARITHMETIC IS A
    // MEASUREMENT, NOT A GUESS (the doc's own words) — these three are a
    // FIRST CUT, sized to produce a visible spread rather than tuned against
    // a sweep. Re-tune from `history_sweep`/`exploration_sweep`, not by
    // re-guessing a round number.

    /// Per-mille of a polity's mean held-ground endowment
    /// (`(farm_q+ore_q+energy_q+port_q)/4`), earned into the capital's
    /// `region::treasury` EVERY DECISION ROUND (a rate, scaled by the step —
    /// see § The stepped decision clock). A polity sitting on rich, wide
    /// ground earns faster than one on poor or narrow ground BY CONSTRUCTION.
    int treasury_endowment_income_q = 40;

    /// Capital earned per decision round PER INHERITED CORRIDOR touching held
    /// ground (`history_sim_state::supply_corridors`, seeded from
    /// `resume_corridors` at the span's open and grown by ordinary use
    /// thereafter) — the network term BL-937's reading binds treasury spread
    /// to. A rate, scaled by the step.
    int treasury_corridor_income_q = 6;

    /// BL-954 -- A MARKET EARNS BY WHAT FLOWS THROUGH IT, NOT BY STANDING
    /// (EXPLORATION.md sec Capital arrives). There is no flat per-market
    /// term: every `trade_flow` credits BOTH the seller's and the buyer's
    /// capital `volume_q * this * step_years / 1000` (a rate, scaled by the
    /// step). A market with no trade crossing it earns nothing from trade.
    /// MEASURED (exploration_sweep, 3 seeds): at 100 the spread's summed
    /// trade income per round (~36.5k volume x 2 ends x 100 x 4 / 1000 ~ 29k)
    /// matches the summed flat market income it replaced (~139 market seats
    /// x 50 x 4 ~ 28k), so removing the allowance redistributes capital to
    /// busy lines rather than draining it. 20 was also measured (~5.7k per
    /// round); neither moved reading 8, which consolidation dominates.
    int treasury_trade_income_q = 100;

    /// BL-954 -- per-mille of the TRADE VALUE a pair's trade-access clause
    /// WOULD open (the marginal volume, both directions, every good --
    /// `pair_trade_value_q`) added to `treaty_value_q`, near home and far
    /// alike (EXPLORATION.md sec Trade is a want met by throughput: "only
    /// trade can make a stranger worth a promise"). FIRST CUT, measured
    /// against 0 (exploration_sweep, 3 seeds): at 100 a few more pairs bind
    /// (1371 vs 1365 formed) and one seed's span battles fall 53 -> 9; at 0
    /// that seed matches the pre-trade baseline. The effect is real, not
    /// cosmetic -- tune from the sweep, never by re-guessing.
    int treaty_trade_weight_q = 100;

    // --- BL-933: treaties ---------------------------------------------------
    // EXPLORATION.md sec Diplomacy becomes real.

    /// The shared term every clause bound in one round's treaty formation
    /// gets. "A term of years is what makes it lengthy rather than a stance."
    int64_t treaty_term_years = 80;

    /// THE FORMATION THRESHOLD, EXPLICITLY OPEN (EXPLORATION.md sec Open
    /// questions: "too low and the map freezes; too high and the displacement
    /// never happens"). Both parties must independently score the binding at
    /// or above this before it forms. FIRST CUT, UNMEASURED — lean on this
    /// wave's own sweep (reading 4, treaty depth), never re-guess a round
    /// number (NR item owed: report this as unmeasured).
    int treaty_formation_threshold_q = 400;

    /// Per-mille penalty subtracted from `treaty_value_q` for every treaty the
    /// OTHER party has broken (`polity::treaties_broken`) — "the cost lands on
    /// every other party's willingness to bind with the defector." First cut,
    /// same discipline as the threshold above.
    int treaty_defector_distrust_q = 300;

    /// Per-mille of a subject's CAPITAL TREASURY BALANCE remitted to its
    /// overlord every decision round while a `tribute` clause binds (BL-934's
    /// hook, landed here so BL-933 need not be revisited when that item gives
    /// it a payer). A stock tax rather than an income-share tax — simpler,
    /// still real, still a cost the subject can be starved out of.
    int treaty_tribute_rate_q = 150;

    // --- BL-941: the arms race is deterrence ---------------------------------
    // EXPLORATION.md sec The arms race reinforces peace near home and
    // conflict far from it. Reuses ERAS.md sec The two scalars' Ceiling/Alarm
    // pair at POLITY grain rather than inventing a third quantity: the
    // existing `decider_aggression_q` cost term in `treaty_value_q` already
    // plays Ceiling's part (how much restraint a doctrine carries); these two
    // are what add Alarm.

    /// Reference scale for `visible_capability_q` — a polity's capital
    /// `army_stock` plus its own `navy_stock`, divided by this, is what a
    /// NEIGHBOUR reads (0-1000). Same "plain integer over a reference
    /// constant" shape `fear_reference` already uses. FIRST CUT, UNMEASURED.
    int64_t visible_capability_reference = 5000;

    /// Per-mille of the OTHER side's visible capability added to
    /// `treaty_value_q`, NEAR HOME ONLY — "a neighbour that reads high
    /// visible capability should be more likely to form/maintain a
    /// non-aggression treaty with that polity."
    ///
    /// MEASURED WITH `treaty_far_penalty_q` (BL-950, exploration_sweep 16
    /// seeds, traced, 2026-09-14; displacement median / battle-rate median /
    /// summed neighbour and frontier rates, per century). At 400/350: 0.06 /
    /// 98.0 / 1339 / 366. Alarm alone makes BOTH halves fall -- 700/350 1.34 /
    /// 18.7 / 180 / 296; 1000/350 1.00 / 14.4 / 117 / 291; 550/350 0.50 /
    /// 35.2 / 297 / 331 -- which is the failure the doc names. With the far
    /// penalty at 700: 500 0.84 / 44.4 / 391 / 466; 525 1.33 / 43.5 / 312 /
    /// 492; 550 1.33 / 43.5 / 314 / 473; 575 1.34 / 44.8 / 298 / 536; 600
    /// 1.59 / 35.0 / 245 / 499; 1000 1.72 / 18.7 / 117 / 463.
    ///
    /// 525, AUTHORISED (Ben, 2026-09-14, NR-867): 575 had the most frontier war
    /// in the 525-575 plateau but left four of sixteen seeds under five battles
    /// a century; 525 halves that for ~9% less frontier war. A frozen map is
    /// the failure EXPLORATION.md names, so the gentler end of the plateau wins.
    int deterrence_alarm_weight_q = 525;

    /// Flat penalty on `treaty_value_q` for a pair that met only DURING this
    /// span (a frontier contact, `contact::first.year >= start_year`) — the
    /// other half of the same mechanism: a fleet pointed at ground with no
    /// visible defender meets no deterrent, so a frontier pair should not
    /// bind a non-aggression clause as readily as a long-known neighbour.
    /// Named directly by NR-851: without this every contacted pair, near or
    /// far, scored identically and a funded port's cheap crossing got
    /// treatied over before it was ever used.
    ///
    /// MEASURED (BL-950, see `deterrence_alarm_weight_q` for the joint
    /// table): at alarm 400, 350 -> 700 lifts the summed frontier rate 366 ->
    /// 490 with neighbour war unmoved (1339 -> 1363), displacement 0.06 ->
    /// 0.19. 700 and 1000 are byte-identical on 16 seeds: at 700 no frontier
    /// pair clears the formation bar except on the trade a clause would open
    /// (`treaty_trade_weight_q`), so the penalty is saturated there. It is the
    /// half of the mechanism that GROWS frontier war; the alarm weight is the
    /// half that quiets neighbours.
    int treaty_far_penalty_q = 700;

    // --- BL-934: colonies ----------------------------------------------------
    // EXPLORATION.md sec A colony is a subject, and it wants things of its own.

    /// The arriving power's capital treasury must clear the native's own by
    /// at least this many units before subjection is scored as viable — the
    /// economic half of "technologically dominant" (treasury already folds
    /// endowment, network and market together). FIRST CUT, UNMEASURED.
    int64_t subjection_treasury_margin_q = 200;

    /// Chebyshev capital-to-capital distance beyond which even a sea-legs
    /// power will not attempt subjection this round — a first-cut reach bound
    /// standing in for a real overseas logistics model (§ Force persists now
    /// owns that model's actual mechanism; this is a gate, not a cost).
    int subjection_reach_q = 400;

    /// Chebyshev capital-to-capital distance beyond which a subject refuses
    /// tribute renewal outright, reading the SAME "outran its network" idea
    /// the Empire phase's own secession check reads for a land empire.
    int subject_secession_distance_q = 500;

    /// Cohesion floor: a subject at or below this refuses renewal regardless
    /// of distance or reachability — the same `cohesion_floor_q`-shaped idea
    /// `secession_supply_floor_q` already applies to a land empire's own
    /// provinces, read here for an overseas one.
    int subject_secession_cohesion_q = 260;

    // --- BL-935: ports, navies and standing armies ---------------------------
    // EXPLORATION.md sec Force persists now, and persistence has a bill. Every
    // cost below is spent from the CAPITAL'S `region::treasury`
    // (`region::material_stock` never — same discipline `post_road_treasury_
    // cost` already holds itself to). Same SHAPE as `try_build_post_road`:
    // an all-or-nothing spend per round rather than a fractional trickle, so
    // a purchase always moves the stock by exactly one step or not at all.

    /// Treasury cost to raise `region::port_stock_q` by `port_build_step_q`,
    /// one decision round's worth. Zero disables ports outright.
    int64_t port_build_cost_q = 400;
    int     port_build_step_q = 120; ///< Per-mille gain per funded round.
    /// Per-mille of `port_stock_q` LOST per YEAR when the round's build was
    /// refused for want of treasury — "silts toward nothing if underfunded."
    int     port_decay_per_mille_year_q = 40;

    /// Treasury cost to grow `polity::navy_stock` by `navy_build_step_q`, one
    /// decision round's worth. Gated on the CAPITAL'S OWN `port_stock_q`
    /// clearing `navy_min_port_stock_q` — a fleet is staged from a port, not
    /// conjured beside a bare coastline.
    int64_t navy_build_cost_q      = 600;
    int64_t navy_build_step_q      = 400;
    int     navy_min_port_stock_q  = 200;
    /// Per-mille of the UNPAID share of `navy_stock` lost per YEAR (BL-972).
    /// "A fleet is a running cost, not a purchase": the cost is
    /// `navy_upkeep_per_1000_units_year_q` below, and this decay is what
    /// happens to the hulls the purse could not pay for this round. A fleet
    /// whose bill is met in full does not decay.
    int     navy_decay_per_mille_year_q = 30;

    /// Treasury cost to add `standing_army_build_step_q` heads to the
    /// capital's `region::army_stock`, on top of whatever muster alone holds
    /// there, one decision round's worth. BL-972: the heads are a LEVY drawn
    /// from the seat's `manpower_stock` (`standing_army_levy_per_mille_q`),
    /// never conjured — a paid soldier is a civilian who left the pool.
    int64_t standing_army_build_cost_q = 500;
    int64_t standing_army_build_step_q = 300;
    /// Per-mille of the UNPAID share of the PAID standing heads
    /// (`region::standing_army`) that go home per YEAR (BL-972; the rate is
    /// BL-955's). "Falls back toward what muster alone provides": the men
    /// leave `army_stock` and return to the `manpower_stock` of the ground
    /// they stand on, capped at its ceiling exactly as `muster_garrison`'s
    /// own disband is. A realm whose bill is met in full loses none.
    int     standing_army_decay_per_mille_year_q = 60;

    // --- BL-972: force persists, and persistence has a BILL -----------------
    // EXPLORATION.md sec Force persists now: "a polity that over-builds is
    // poorer every round afterwards ... a cost in the world rather than a
    // handicap in the scorer." Every round, after EARN and before the one
    // scored purchase, each living polity is billed for the paid heads
    // standing anywhere in its realm and for its fleet, from the CAPITAL'S
    // `region::treasury`. The army is billed first (the garrison at home
    // before the hulls), then the navy. A bill the purse cannot meet in full
    // is paid for as many heads/units as it covers, and the UNPAID share
    // decays at the two rates above — treasury 0 is exactly BL-955's decay.
    //
    // DEFAULTS: the raising price, per year. A paid step buys 300 heads for
    // 500 (1667 per 1000) and 400 hull-units for 600 (1500 per 1000); a
    // standing soldier's pay over a year is of the order of what it cost to
    // raise him (real history supplies the mechanism: pay, not kit, is the
    // running cost of a standing force). MEASURED, not targeted — see the
    // BL-972 sweep report for what these defaults do on 16 seeds. Zero
    // disables a bill, and a stock with no bill never decays.
    int64_t standing_army_upkeep_per_1000_heads_year_q = 1667;
    int64_t navy_upkeep_per_1000_units_year_q          = 1500;
    /// The LEVY BOUND: a paid army step may draw at most this per-mille of
    /// the seat's banked `manpower_stock` in one round, and the step is
    /// all-or-nothing (same shape as every other purchase here), so the army
    /// option is not eligible when the pool cannot lend a whole step. This is
    /// the physical limit that replaces the scorer's old per-region cap: the
    /// muster and the paid levy now draw on the SAME pool of eligible
    /// civilians (GENERATION_STRATEGY.md sec Population is civilian).
    int     standing_army_levy_per_mille_q = 500;

    // --- BL-955: spend is ALLOCATED, not bought whenever affordable ----------
    // EXPLORATION.md sec Force persists now ("Spend is ALLOCATED"). Once a
    // round's EARN is in, each living polity makes ONE scored choice among
    // {port step, navy step, standing-army step, hold}
    // (`score_exploration_spend` / `choose_exploration_spend`). Every score is
    // on a 0-1000 integer scale; the leans enter as per-mille RANKS over the
    // round's living, cultured polities (`exploration_lean_ranks`), never as
    // the raw leans, whose scales are incommensurable (NR-864). The decays
    // above are untouched: only the purchases are chosen. FIRST CUT; see the
    // BL-955 sweep report for the measurement these defaults stand on.

    /// Weight of the EXPANSION rank in the port and navy scores.
    int     spend_w_expansion_q         = 600;
    /// Weight of the across-water want (0-1000) in the port and navy scores.
    int     spend_w_water_want_q        = 400;
    /// Weight of the CONSOLIDATOR rank in the standing-army score.
    int     spend_w_consolidator_q      = 600;
    /// Weight of the near-home Alarm (0-1000) in the standing-army score.
    int     spend_w_alarm_q             = 400;
    /// The flat value of keeping the purse — hold's floor.
    int     spend_hold_base_q           = 250;
    /// Weight of the CONSOLIDATOR rank in hold (weighted below the army's).
    int     spend_w_hold_consolidator_q = 200;
    // BL-972 REMOVED the two saturation caps BL-955 first cut carried here
    // (`navy_saturation_per_region` 400, `army_saturation_per_region` 300:
    // a score of 0 past N units/heads per held region). They were a handicap
    // in the scorer standing in for a cost in the world; the per-head bill
    // above (`standing_army_upkeep_per_1000_heads_year_q`,
    // `navy_upkeep_per_1000_units_year_q`) and the levy bound
    // (`standing_army_levy_per_mille_q`) are the forces that now limit a
    // stock, and both are visible in the world rather than inside the actor.
};

// ---------------------------------------------------------------------------
// BL-931 — OBJECTS WITH A TERM.
// ---------------------------------------------------------------------------
//
// EXPLORATION.md sec The engine is shared's second honest addition: "treaties
// expire, which nothing in the Empire phase does." This is the MECHANISM —
// a dated object that ticks down and is removed once its term ends — with no
// treaty semantics yet (a treaty's clauses are BL-933's). `kind`/`a`/`b` are
// deliberately opaque integers here: this file interprets none of them, so a
// later item can give `kind` a meaning (non-aggression, trade access, ...)
// without this struct or its expiry pass changing shape.
struct dated_object
{
    int64_t expires_year = 0; ///< The object is gone once the round reaches this year.
    int32_t kind = 0;         ///< Opaque to this file; a future item's clause kind.
    int32_t a = -1;           ///< Opaque to this file; typically a polity id.
    int32_t b = -1;           ///< Opaque to this file; typically a polity id.
};

/// Removes every `dated_object` whose term has ended AT OR BEFORE @p year,
/// in place. Stable relative order preserved (`std::remove_if` + `erase`),
/// so two callers walking the same vector before and after a tie-breaking
/// year see the same survivors in the same order. Pure and seedless: a
/// deterministic consequence of the year and the terms already recorded,
/// never a roll.
void expire_dated_objects(std::vector<dated_object>& objects, int64_t year);

// ---------------------------------------------------------------------------
// BL-954 — TRADE IS A WANT MET BY THROUGHPUT (EXPLORATION.md sec Trade is a
// want met by throughput). Declared here, ahead of the upkeep step that
// rebuilds them; the functions that size a flow sit beside the scarcity
// signal further down, once `region`/`polity` are in scope.
// ---------------------------------------------------------------------------

/// ONE NUMBER PER SELLER, BUYER AND GOOD — not a cargo, not a route, not a
/// price. A fact in the same family as `grudge`/`contact`: a named, directed
/// pair plus what joined them. `good` is a `scarcity_good_index` (farm=0,
/// ore=1, energy=2, port=3). `volume_q` is always > 0 in a stored entry.
/// Rebuilt every decision round of the Exploration span; the vector is
/// sorted ascending by (seller, buyer, good).
struct trade_flow
{
    uint16_t seller   = 0;
    uint16_t buyer    = 0;
    uint8_t  good     = 0;
    int32_t  volume_q = 0;
};

/// THE UPKEEP STEP ITSELF (BL-931/BL-932), called once per decision round
/// when `history_sim_params::exploration_upkeep_enabled` is set. "Earn, then
/// pay stocks, then invest" (EXPLORATION.md sec The engine is shared) — this
/// item builds EARN: every living polity's capital seat (`region::treasury`)
/// draws income from its held ground's endowment, the inherited corridor
/// network, and the trade flowing through its market (BL-954; `history_sim_params::treasury_*_income_q`),
/// and — ONCE, on the round at @p year == @p params.start_year, the phase's
/// visible opening act — the seat's accumulated `material_stock` is folded
/// into it (EXPLORATION.md sec Capital arrives: "material becomes capital").
/// Also refreshes every market's scarcity signal (`refresh_market_scarcity`,
/// BL-939) — the demand half runs on the same round-level cadence the
/// treasury's own earn does, for the same reason: both are facts that go
/// stale the moment ground changes hands. PAY (ports/navies/standing armies
/// decaying, BL-933's stocks) and INVEST beyond the ordinary verb are still
/// owed to a later item.
/// BL-935 — PAY then INVEST for ports, navies and standing armies (out
/// parameters rather than a returned struct, so the one call site can
/// accumulate straight into `history_sim_state`'s own counters without a
/// second copy): treasury actually spent building each stock, this call.
struct exploration_upkeep_spend
{
    int64_t ports           = 0;
    int64_t navies          = 0;
    int64_t standing_armies = 0;
    /// BL-955: stock STEPS bought this call (one per polity per call at most,
    /// across all three) — the observable for "a fully funded polity still
    /// builds at most one stock per round".
    int64_t port_steps      = 0;
    int64_t navy_steps      = 0;
    int64_t army_steps      = 0;
    /// BL-955: polity indices whose navy decayed to zero this call.
    std::vector<uint16_t> navies_lapsed;
    /// BL-972: the BILL. Treasury actually paid this call for paid standing
    /// heads and for hulls, and how many living polities' bills went short
    /// (paid for fewer heads/units than stand), for each stock.
    int64_t army_upkeep     = 0;
    int64_t navy_upkeep     = 0;
    int64_t army_unpaid     = 0; ///< polities whose army bill was not met in full
    int64_t navy_unpaid     = 0; ///< polities whose navy bill was not met in full
    /// BL-972: the LEVY. Heads drawn from a seat's `manpower_stock` by an
    /// army step this call, and heads returned to a region's pool by the
    /// unpaid decay this call.
    int64_t levy_raised     = 0;
    int64_t levy_returned   = 0;
};

struct history_sim_state;       // defined further down
struct culture_good_preference; // defined further down

/// BL-955 — WHAT THE ALLOCATION READS beyond the region/polity tables. Every
/// pointer may be null, and a null reads as "no signal" rather than an error:
///   - @c state   : its `contacts` give the near-home Alarm (contacts whose
///                  first year predates `params.start_year`, read through
///                  `deterrence_alarm_q`). Its `polities` MUST be the same
///                  vector the upkeep call mutates (as `run_history_sim`
///                  passes it); the Alarm is read in a pre-pass before any
///                  stock moves, so the read never sees this round's buys.
///   - @c creeds  : the cultures the leans are ranked from. Null ranks every
///                  polity 0 on both leans.
///   - @c prefs   : the round's live culture preference, weighting the
///                  across-water want (`polity_good_want_q`). Null or empty
///                  weights every good at 0 preference.
struct exploration_spend_context
{
    const history_sim_state*                    state  = nullptr;
    const creed_state*                          creeds = nullptr;
    const std::vector<culture_good_preference>* prefs  = nullptr;
};

///
/// BL-954 — THE ROUND'S ORDER IS: refresh the raw signal, compute every
/// trade flow a bound `trade_access` clause in @p treaties opens (written,
/// sorted, into @p flows_out), relieve each buyer's signal by its inbound
/// volume, EARN (endowment, corridor touch, and each flow crediting BOTH
/// capitals — no flat market income), then pay and invest. @p treaties null
/// means no clause binds anyone, so no flow forms; @p flows_out null discards
/// the flows after they have relieved and earned.
///
/// BL-955 — PAY and INVEST are split: every DECAY runs exactly as BL-935 set
/// it (a port silts on any round it is not built, a navy decays every round,
/// a standing army falls back on any round it is not funded), but the
/// PURCHASE is one scored choice per polity per round
/// (`score_exploration_spend`/`choose_exploration_spend`), fed from
/// @p spend_ctx. A null @p spend_ctx still runs the choice, on no leans, no
/// Alarm and unweighted wants.
void run_exploration_upkeep(std::vector<region>&                 regions,
                            std::vector<polity>&                 polities,
                            const std::vector<history_corridor>& corridors,
                            const history_sim_params&             params,
                            int64_t                                year,
                            int                                    step_years,
                            exploration_upkeep_spend*              spend = nullptr,
                            const std::vector<dated_object>*       treaties = nullptr,
                            std::vector<trade_flow>*               flows_out = nullptr,
                            const exploration_spend_context*       spend_ctx = nullptr);

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
// ---------------------------------------------------------------------------
// BL-897 — A CREED THAT SPANS CULTURES
// ---------------------------------------------------------------------------

/// ONE UNIVERSALISING CREED, COINED NEW AND BELONGING TO NOBODY
/// (`../../docs/lore/CREEDS.md` § The four calls, settled; Ben, 2026-09-11).
///
/// Every other creed in this codebase is LOCAL BY CONSTRUCTION — a pantheon
/// belongs to a cradle culture and travels only as that people travels, which
/// is what made the distribution of pantheons a record of routes
/// (`../../docs/generation/COLONISATION.md` § Culture arrives by route). This
/// record inverts exactly that property, and the inversion is why it is a
/// separate type rather than another `creed`: it has NO CULTURE FIELD, because
/// it has no cradle culture, and that absence is load-bearing rather than
/// unfinished. A creed risen *from* a pantheon would still be that people's
/// creed wearing a larger name, and every other people would read its spread as
/// that people's spread. Belonging to nobody is what lets it bind peoples who
/// are not kin, and what makes the pantheons it subsumes RESIDUE UNDERNEATH
/// (`region::creed_residue_culture`) rather than ancestors above.
///
/// ITS NAME IS COINED FROM A PHONOLOGY OF ITS OWN. The standing rule is
/// unconditional — real history is a mechanism reference, never a name source —
/// and it bites hardest here, because the mechanism this models has an obvious
/// proper noun attached to it in every reader's head. So `speech` is a fresh
/// `roll_tongue` seeded off the run seed, the founding realm and the year: not
/// drawn from any culture's inventory (which would give it a cradle by the back
/// door) and not drawn from any list (which would break the rule outright).
struct universal_creed
{
    std::string name;   ///< Coined from `speech`, which belongs to no people.
    tongue      speech; ///< Its own phonology, rolled for it and used by nothing else.

    int64_t founded_year = 0;  ///< Calendar year it was first coined.
    int     origin_polity = -1; ///< The humbled realm it arose in — a FACT, not an owner.
    int     origin_region = -1; ///< That realm's seat at the time.
};

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
    ///
    /// WRITTEN ON THE TWO-SPAN ARC ONLY (BL-976). The single-span arc runs no
    /// industrial span, so the sim leaves this at zero there and the
    /// derivation is Digitisation's (DIGITISATION.md § The boundary);
    /// `seed_national_tariffs` reads it on both arcs as the enactment seam.
    int protection_q = 0;

    /// True for a seeded great power (BL-299). Majors start with more ground
    /// and an opposed strategic creed; the periphery stays alive as actors.
    bool major = false;

    /// BL-926 — THE LINEAGE HOOK. The polity this one BROKE AWAY FROM, as an
    /// index into `history_sim_state::polities`, or -1 where the sim records
    /// no parent (every seeded polity; every polity born by any route other
    /// than the BL-896 secession block, which is the only site that writes
    /// it today; BL-916 names a successor "X, broken from Y" off it). A RECORD, never a decision input: nothing in the loop reads
    /// it. NOT SERIALISED — `polity` does not cross the save seam (only
    /// `polity_sample` does, era_timelapse.hpp), so no flat-binary path is
    /// owed. The sweep's per-polity table prints '-' when this is -1.
    int parent = -1;

    /// BL-897 — THE INSTITUTION, as distinct from what its peoples believe.
    /// Index into `history_sim_state::universal_creeds`, or -1.
    ///
    /// TWO GRAINS, EXPECTED TO DISAGREE (CREEDS.md § The four calls, settled).
    /// A realm adopts the creed as an INSTITUTION — that is what makes it able
    /// to hold ground its own culture never walked — while the peoples under it
    /// convert at their own pace or refuse (`region::creed_hold`). The gap
    /// between this field and those is the fault line: an empire whose
    /// institution is universal and whose subject peoples are not is exactly
    /// the empire that splits over faith rather than over distance.
    int universal_creed = -1;

    /// Calendar year this realm adopted, or 0 for never. Kept so a later slice
    /// can ask how long an institution has stood against its own ground.
    int64_t creed_adopted_year = 0;

    bool alive = true; ///< False once the polity holds no regions.

    // -----------------------------------------------------------------------
    // BL-912 — THE EMPIRE TREE, held as real per-polity state.
    // -----------------------------------------------------------------------
    //
    // trees/TREES.md sec State: "one 64-bit mask per tree plus one accumulated
    // progress integer for the node being invested in." This is that state,
    // added alongside the capacity ladder above rather than in place of it —
    // TREES.md sec "The sim reads nodes, not bands" demotes the band to a
    // DERIVED reading kept where a consumer still wants a scalar (the works
    // gate, the unit roster boundary); nothing here retires `capacity`.
    //
    // `empire_mask` bit i is set iff this polity holds `empire_tree::nodes[i]`
    // (docs/generation/trees/empire_tree.json, via the generated
    // `empire_tree_data.hpp` — see `tools/session/gen_empire_tree_table.js`).
    // A closed fork side (`node::excludes`) never sets its bit once the other
    // side is held; see `empire_node_available` in history_sim.cpp.
    uint64_t empire_mask = 0;

    /// Index into `empire_tree::nodes`, or -1 when nothing is targeted. The
    /// Invest verb (BL-912) picks this via the scorer and keeps investing in
    /// it, round over round, until it is bought or goes unavailable (a rival
    /// branch closed under it, e.g. after a fork).
    int16_t empire_investing = -1;

    /// Progress accumulated toward `empire_investing`'s cost. Reset to 0 when
    /// the node is bought or abandoned. Same accumulated-progress currency the
    /// domain ladder's `progress_q` uses; the unit is `capacity_band_cost`-
    /// scaled, per node kind and ring (TREES.md sec Nodes: "cost is base x ring").
    int32_t empire_progress_q = 0;

    // -----------------------------------------------------------------------
    // BL-930 — THE EXPLORATION TREE, held alongside the empire tree's state.
    // -----------------------------------------------------------------------
    //
    // Exactly the empire tree's shape (BL-912, above), copied rather than
    // templated: `trees/TREES.md` sec State fixes "one 64-bit mask per tree
    // plus one accumulated progress integer for the node being invested in"
    // as the per-tree shape every tree gets, and a polity holds one of these
    // triples per tree it can invest in. The Exploration tree opens behind
    // the empire tree's rim (`polity_holds_empire_rim`), so a polity that
    // never reaches that rim simply never sets a bit here.
    //
    // `exploration_mask` bit i is set iff this polity holds
    // `exploration_tree::nodes[i]` (docs/generation/trees/exploration_tree.json,
    // via the generated `exploration_tree_data.hpp` — see
    // `tools/session/gen_empire_tree_table.js exploration`). A closed fork
    // side (`node::excludes`) never sets its bit once the other side is held;
    // see `exploration_node_available` in history_sim.cpp.
    uint64_t exploration_mask = 0;

    /// Index into `exploration_tree::nodes`, or -1 when nothing is targeted.
    /// Same Invest verb, same round, as `empire_investing`.
    int16_t exploration_investing = -1;

    /// Progress accumulated toward `exploration_investing`'s cost. Same
    /// currency as `empire_progress_q`.
    int32_t exploration_progress_q = 0;

    // -----------------------------------------------------------------------
    // BL-1038 — THE INDUSTRY TREE, the third triple, same shape again.
    // -----------------------------------------------------------------------
    //
    // `industry_mask` bit i is set iff this polity holds
    // `industry_tree::nodes[i]` (docs/generation/trees/industry_tree.json via
    // the generated `industry_tree_data.hpp`). Written ONLY by the Invest
    // verb's Industry block, which runs only while
    // `history_sim_params::industry_tree_enabled` is set and the round's year
    // has reached `industry_open_year` — so on every shipped path all three
    // stay at their defaults and the fold reads a zero mask. NOT SERIALISED,
    // same footing as the other two triples: `polity` does not cross the save
    // seam.
    uint64_t industry_mask = 0;

    /// Index into `industry_tree::nodes`, or -1 when nothing is targeted.
    int16_t industry_investing = -1;

    /// Progress toward `industry_investing`'s cost. Same currency as
    /// `empire_progress_q`, earned at the Industry rate (urban mass).
    int32_t industry_progress_q = 0;

    /// Has this polity EVER, on an Industry-investing round, held ground with
    /// a fuel seam that clears the gate's bar? Written only by the Industry
    /// block; read by nothing in the sim — the sweep's "never passes the fuel
    /// gate" share reads it, which is why it is a fact on the polity rather
    /// than a closing-map reconstruction (ground changes hands).
    bool industry_fuel_seen = false;

    // -----------------------------------------------------------------------
    // BL-934 — THE OVERLORD LINK. A colony is a LIVE POLITY with one field
    // pointing at somebody else, never a region annotation and never a new
    // actor class (EXPLORATION.md sec A colony is a subject).
    // -----------------------------------------------------------------------

    /// Index into `history_sim_state::polities`, or -1 for no subjection.
    /// DELIBERATELY NOT `parent` (BL-926, lineage: who this polity broke away
    /// FROM). Subjection is a LIVE relation that begins, ends and transfers;
    /// `parent` never changes once written and this field does, on both ends,
    /// which is exactly the distinction the design draws. NOT SERIALISED,
    /// same footing as `parent` — this struct does not cross the save seam.
    int32_t overlord = -1;

    /// 0 = trade province (a foothold; the native polity survives beside it),
    /// 1 = subjected polity (the native realm itself, brought under the link,
    /// whole), -1 = not a subject (`overlord < 0`). Derived once, at the round
    /// `overlord` is first set, from whether the native seat itself carries a
    /// port endowment (`region::port_q`) — a coastal seat is a foothold an
    /// arriving sea power plants beside; an interior seat has no coast to
    /// plant one on, so the whole realm is what changes hands. A DERIVED
    /// READING, not a decision the loop makes twice — see run_history_sim's
    /// subjection block for where it is set and why this is the honest proxy
    /// available without a second, region-spawning placement pass (§ Where
    /// subjects come from names both paths; this data model does not yet
    /// carry a trade seat as a distinct region — a scope note, not a design
    /// claim).
    int8_t subject_kind = -1;

    /// BL-935 — THE NAVY, NEW AND ZERO EVERYWHERE AT 1200 CE (EXPLORATION.md
    /// sec Force persists now: unlike `army_stock`, no polity inherits a
    /// fleet). A headcount-like scalar, built from a funded port's treasury
    /// spend and decaying every round regardless — "a fleet is a running
    /// cost, not a purchase." NOT SERIALISED, same footing as `overlord`.
    int64_t navy_stock = 0;

    /// BL-933 — HOW MANY TREATIES THIS POLITY HAS BROKEN, ever. The smallest
    /// quantity that makes "the cost lands on every OTHER party's willingness
    /// to bind with the defector" (EXPLORATION.md sec Diplomacy becomes real)
    /// readable by a third party without a second ledger: `treaty_value_q`
    /// reads the COUNTERPART's copy of this field, never the decider's own.
    int32_t treaties_broken = 0;

    // -----------------------------------------------------------------------
    // BL-973 — THE TREE EFFECT SURFACE, folded from the masks above.
    // -----------------------------------------------------------------------
    //
    // DERIVED, NEVER AUTHORED: `apply_tree_effects` rewrites both fields
    // from `empire_mask`, `exploration_mask` and (BL-1038) `industry_mask` —
    // zero on every shipped path — at the top of every round
    // and again the instant a node is bought, walking the generated
    // `effects[]` tables (tree_effect.hpp is the vocabulary). Nothing else
    // writes them, and nothing in the sim names a node — a reader asks for
    // a TERM or a KEY, never for "node 11". NOT SERIALISED, same footing as
    // the masks: recomputable from them, and this struct does not cross the
    // save seam.

    /// Per-term sum of held `modifier` effects' per-mille, every invested
    /// tree, indexed by `io::tree_modifier_term`. A term with no reader in the
    /// sim is still summed here (the surface is generic); which terms are
    /// read is `tree_effect_reader_of`'s to say.
    int32_t tree_mod_q[io::tree_modifier_term_count] = {};

    /// Bit `k` set iff some held node carries an effect with
    /// `key == io::tree_effect_key(k)`. The identity reads (sea legs, post
    /// roads) test this and nothing else.
    uint32_t tree_keys = 0;
};

// ---------------------------------------------------------------------------
// The empire tree (BL-912) — availability, the scorer, and the rim read
// ---------------------------------------------------------------------------

/// True iff `node_idx` is a legal Invest target for a polity holding `mask`:
/// not already held, its fork partner (if any) not held, its ring unlocked
/// (the milestone one ring down is held, or it sits at ring 1), and rule 2's
/// OR-availability satisfied (a linked neighbour is held, or it has none —
/// the spire roots). A milestone additionally needs its `requires` AND set
/// and, if present, one side of its `requires_fork` pair.
bool empire_node_available(uint64_t mask, int node_idx);

/// The scorer (TREES.md sec The scorer): a seeded, deterministic argmax over
/// every available node, integer throughout, tie-broken on the lower node
/// index (the tree's own fixed authored order). Returns -1 when nothing is
/// available (tree exhausted, or every open branch closed under a fork).
///
/// Terms read the polity's own state and its held ground — never a rank,
/// never anything that grows with the polity's size alone (TREES.md sec
/// "every term is an in-world quantity with a visible cause"). Argument to
/// the Invest verb the AI grant register already covers
/// (docs/ai/AI_OPPONENT.md sec 11); this chooses a node, not a new verb.
int choose_empire_node(uint64_t mask, int cohesion_q, int stores_low_q,
                        int reach_bound_q, int manpower_bound_q, int food_bound_q,
                        int ground_ore_q, int ground_farm_q, int ground_fuel_q,
                        int ground_port_q, int surplus_q);

/// THE RIM (BL-912/BL-907): has this polity crossed EM-SP-4m, "The
/// Enforceable Promise"? A per-polity BOOLEAN, read straight off the mask —
/// the closure contract's "explorer set" reading, per the item's own naming.
inline bool polity_holds_empire_rim(const polity& q)
{
    return (q.empire_mask & (1ULL << io::empire_tree::rim_node_index)) != 0;
}

// ---------------------------------------------------------------------------
// The exploration tree (BL-930) — availability, the scorer, and the rim read
// ---------------------------------------------------------------------------
//
// EXACT PRECEDENT: `empire_node_available`/`choose_empire_node` above,
// re-read against `io::exploration_tree` instead of `io::empire_tree`. The
// five rules (`TREES.md` sec The five rules) are the tree grammar, not an
// empire-specific mechanic, so the availability test is identical in shape.

/// True iff `node_idx` is a legal Invest target for a polity holding
/// `mask` in the EXPLORATION tree — same five-rule test
/// `empire_node_available` runs, over `io::exploration_tree::nodes`.
bool exploration_node_available(uint64_t mask, int node_idx);

/// The scorer (TREES.md sec The scorer — one shape, four trees), read against
/// `io::exploration_tree`. Four of the terms this tree adds
/// (`EXPLORATION_TREE.md` sec The scorer: `purse_low`, `wants_unmet`,
/// `throughput_bound`, `subject_held`) read quantities this item does not
/// build — the capital treasury (BL-932), the scarcity signal (BL-939),
/// corridor throughput (BL-940), and the overlord/subject link (BL-933/934)
/// respectively. `purse_low_q`, `wants_unmet_q` and `throughput_bound_q` are
/// now real, threaded-through parameters (BL-932/939/940); `subject_held`
/// alone is still STUBBED AT A PINNED NEUTRAL VALUE inside the function body,
/// exactly as `choose_empire_node` stubs `threatened`/`plague_struck`/
/// `many_peoples` at 0 — a 0 term never wins the argmax on its own account,
/// which is honest rather than wrong, and a harness pins the remaining
/// stub's value so BL-933/934 landing shows as a diff.
/// BL-942 -- `consolidator_lean_q`/`expansion_lean_q` (0-1000, see the two
/// free functions of the same name below) weight the terms EXPLORATION.md
/// sec Two ways to be strong names for each strategy; `creed_lean_weight_q`
/// scales how hard that weighting bites, 0 reducing it to a no-op so every
/// pre-existing call site (default arguments below) is unchanged.
int choose_exploration_node(uint64_t mask, int stores_low_q, int reach_bound_q,
                             int ground_port_q, int ground_farm_q, int surplus_q,
                             int purse_low_q, int wants_unmet_q, int throughput_bound_q,
                             int subject_held_q = 0, int consolidator_lean_q = 0,
                             int expansion_lean_q = 0, int creed_lean_weight_q = 0);

/// BL-942 -- CONSOLIDATOR LEAN, 0-1000: "high dominion with no sea legs is
/// the continental consolidator" (EXPLORATION.md sec Two ways to be strong).
/// Reads the culture's war god (`pantheon[1]`, the same slot
/// `culture_opposition_q` reads) `dominion` and its OWN `sea_legs_q`
/// (creeds.hpp), never rolled. 0 for a culture with no war-god slot.
int consolidator_lean_q(const culture& c);

/// BL-942 -- EXPANSION LEAN, 0-1000: "deep sea legs with a universalising
/// creed is the coloniser." Reads `sea_legs_q` and the war god's `zeal` (the
/// same temperament axis `culture_opposition_q` reads), never rolled.
int expansion_lean_q(const culture& c);

/// THE RIM (BL-930): has this polity crossed EX-SP-3m, "The Long Reckoning"?
/// A per-polity boolean, read straight off the mask — this is the fact
/// `EXPLORATION_TREE.md` sec What the tree hands the Industry tree calls
/// "entry timing": a polity holding this at 1660 starts the Industry tree at
/// its root.
inline bool polity_holds_exploration_rim(const polity& q)
{
    return (q.exploration_mask & (1ULL << io::exploration_tree::rim_node_index)) != 0;
}

// ---------------------------------------------------------------------------
// The industry tree (BL-1038) — availability, the scorer, the rim, the rate
// ---------------------------------------------------------------------------
//
// Same five-rule availability as the other two trees, over
// `io::industry_tree`. What differs is ALL in the Industry tree's own reading,
// and none of it touches the empire or exploration scorers:
//
//   - ENTRY. Every living polity invests from `industry_open_year`; there is
//     no rim gate (TREES.md sec Milestones, the Industry exception).
//   - THE FUEL GATE READS A SEAM (INDUSTRY_TREE.md sec Aims, PROPOSED
//     2026-09-18): `fuel` passes when ANY held region's `energy_q` clears
//     `industry_fuel_seam_bar_q`, not the held mean. The empire scorer keeps
//     its mean reading; the exploration scorer keeps its open fuel gate.
//   - THE RATE READS URBAN MASS (TREES.md sec State), never the industry
//     slice, and never the scorer (`industry_research_per_year_q`).

/// The bar a held region's `energy_q` must clear for the Industry tree's
/// `fuel` gate. The same 250/1000 placeholder bar the empire scorer's gates
/// use (TREES.md sec Effects: "magnitudes are authored by judgement") —
/// only WHAT it is compared against differs: one region, not the mean.
inline constexpr int industry_fuel_seam_bar_q = 250;

/// How many held regions the Industry rate reads (`industry_urban_mass`).
///
/// THREE, AND WHY. The doc's reading is "the population of the polity's
/// largest held centres" — concentration, not breadth. k = 1 makes the rate
/// hostage to one city (a sacked capital zeroes a century's research) and
/// cannot tell a one-city realm from a realm of great cities; k = all held
/// regions makes the rate the realm's total urban headcount, i.e. its SIZE,
/// which is breadth wearing a density label. Three is a seat and its two
/// largest rivals: a small realm (most living polities hold a handful of
/// regions) sums nearly all of its urban ground, and a large one stops
/// earning research for breadth past its third city — it earns it by making
/// those cities denser. A named constant rather than a param because it is
/// the definition of the reading, not a magnitude the sweep re-prices.
inline constexpr int industry_research_top_k = 3;

/// Integer square root: the largest r with r*r <= v, for v >= 0 (0 for any
/// v <= 0). Exact over the whole int64_t range — Newton's iteration from a
/// power-of-two overestimate, no floating point, so it is bit-identical on
/// every machine. src/world had none before BL-1038.
int64_t isqrt64(int64_t v);

/// The Industry rate's input: `urban_population` summed over the
/// `industry_research_top_k` largest of @p held's regions. UNCLAMPED — the
/// clamp belongs to the transform (`industry_research_per_year_q`). The sum of
/// the k largest values is the same whichever of several equal values is
/// picked, so the reading is independent of `held`'s order.
int64_t industry_urban_mass(const std::vector<region>& regions, const std::vector<int>& held);

/// Research earned per year toward the Industry node being invested in:
/// clamp @p urban_mass to `industry_urban_mass_cap` FIRST, then the integer
/// superlinear transform `Mc * isqrt64(Mc) / isqrt64(reference)`, times
/// `industry_research_fraction_q / 1000`, times the polity's `research`
/// modifier `(1000 + clamp(mod, 0, 4000)) / 1000` exactly as the other two
/// trees apply it. Never negative.
int64_t industry_research_per_year_q(int64_t urban_mass, int research_mod_q,
                                     const history_sim_params& params);

/// True iff `node_idx` is a legal Invest target for a polity holding `mask`
/// in the INDUSTRY tree — the same five-rule test `empire_node_available`
/// runs, over `io::industry_tree::nodes`.
bool industry_node_available(uint64_t mask, int node_idx);

/// Everything the Industry scorer reads, as one value, so a harness can set
/// a reading by name rather than by position in a 20-argument call. Every
/// field is 0-1000 unless said otherwise; the Invest block fills it.
struct industry_scorer_reading
{
    // --- The shared core, derived exactly as the empire scorer's inputs are
    int reach_bound_q    = 0;
    int manpower_bound_q = 0;
    int food_bound_q     = 0;
    int stores_low_q     = 0;
    int cohesion_q       = 1000; ///< `cohesion_low` reads 1000 - this.
    int surplus_q        = 0;
    int ground_ore_q     = 0;    ///< held MEAN — the `ore_q` gate reads it, as the empire's does.
    int ground_farm_q    = 0;    ///< held mean — `arable`, `ground_farm`.
    int ground_port_q    = 0;    ///< held mean — `coastal`, `coastal_holdings`, `ground_port`.

    /// THE SEAM: the MAX `energy_q` over held ground. The `fuel` gate
    /// (`>= industry_fuel_seam_bar_q`) and the `ground_fuel` term both read
    /// it — "coal seams under held ground" is a claim about a region.
    int fuel_seam_q      = 0;

    // --- Terms this phase adds (INDUSTRY_TREE.md sec The scorer)
    int threatened_q     = 0; ///< the heaviest grudge a living polity holds against this one
    int fuel_bound_q     = 0; ///< the seat market's unmet energy want
    int labour_bound_q   = 0; ///< share of the held non-subsistence surplus standing under arms
    int credit_bound_q   = 0; ///< how far the seat's purse falls short of one road's price
    int colonial_reach_q = 0; ///< 1000 iff a held region lies across a sea leg from the seat
    int many_peoples_q   = 0; ///< share of held regions whose plurality culture is not the polity's

    /// `known` is PER NODE (TREES.md sec The scorer: "a neighbour already
    /// holds it"): the OR of the Industry masks of every living polity this
    /// one has met. A node whose bit is set here reads `known` at 1000.
    uint64_t known_mask  = 0;
};

/// Every scorer term's value for one polity, indexed by the generated
/// `io::industry_tree::scorer_term` (never positionally — see the guard in
/// history_sim.cpp). `known` is 0 here because it is per node;
/// `choose_industry_node` reads it off `known_mask`. THREE TERMS ARE PINNED
/// AT 0, named so their landing shows as a diff in exploration_sim_harness:
/// `ground_forest` (no forest reading in the sim — INDUSTRY_TREE.md sec Open
/// questions), `tariff_pressure` (no price at a market before the campaign)
/// and `plague_struck` (the history sim runs no plague; the empire scorer
/// pins it too).
void industry_term_values(uint64_t mask, const industry_scorer_reading& r,
                          int (&out)[io::industry_tree::term_count]);

/// The Industry tree's endowment gates. The EMPIRE scorer's shape
/// (`ore_q`/`arable`/`grassland` against the held means at the 250 bar,
/// `coastal` on any port) with ONE difference: `fuel` reads the seam.
bool industry_gate_open(io::industry_tree::gate_atom g, const industry_scorer_reading& r);

/// The scorer (TREES.md sec The scorer — one shape, four trees): argmax over
/// every available, gate-open node of `term + kind_bonus - cost`, integer
/// throughout, tie-broken on the lower node index. -1 when nothing is
/// available. The same kind bonus and cost shape the other two trees use.
int choose_industry_node(uint64_t mask, const industry_scorer_reading& r);

/// THE RIM (BL-1038): has this polity crossed IN-SP-3m, "The Renewed Line"?
/// Its `open "campaign tree"` effect is classified `tree_gate` by
/// `tree_effect_reader_of`, but NOTHING in the sim consumes it — the campaign
/// tree lives past the 1960 handoff. The harness says so rather than passing
/// a vacuous check.
inline bool polity_holds_industry_rim(const polity& q)
{
    return (q.industry_mask & (1ULL << io::industry_tree::rim_node_index)) != 0;
}

// ---------------------------------------------------------------------------
// The tree effect surface (BL-973) — one fold, generic readers, an honest
// unread list
// ---------------------------------------------------------------------------
//
// TREES.md sec Effects: "a node whose effect nothing in the sim reads is not
// authored." Before this item the generated tables carried topology only and
// the sim read three nodes by hand (a counted rim index, `index 11` for sea
// legs, a strcmp on "EX-WY-1a"). Now every store effect reaches the sim
// through ONE fold, and what the sim does with each kind is stated here in
// code, so the harness can hold the store to it.

/// Fold `q.empire_mask`, `q.exploration_mask` and `q.industry_mask`
/// (BL-1038; zero unless the Industry switch is on) into `q.tree_mod_q[]` and
/// `q.tree_keys`, walking the three generated `effects[]` tables in their
/// fixed authored order. Pure in the masks; idempotent; cheap (≈230 rows).
void apply_tree_effects(polity& q);

/// Does a held node carry an effect keyed `k`? Reads the folded surface.
inline bool polity_holds_tree_key(const polity& q, io::tree_effect_key k)
{
    return (q.tree_keys & (1u << static_cast<unsigned>(k))) != 0;
}

/// The polity's summed per-mille for one modifier term (0 when nothing held).
inline int tree_mod_q(const polity& q, io::tree_modifier_term t)
{
    const int i = static_cast<int>(t);
    return (i >= 0 && i < io::tree_modifier_term_count) ? q.tree_mod_q[i] : 0;
}

/// BL-934 — THE ASYMMETRY THAT PERMITS SUBJECTION, AS A NODE, NEVER A RANK
/// (EXPLORATION.md sec Where subjects come from: "nothing reads size"): the
/// store effect keyed `sea_legs` ("a crossing to unmet ground no longer
/// requires an adjacent shore"), whichever node carries it.
inline bool polity_holds_exploration_sea_legs(const polity& q)
{
    return polity_holds_tree_key(q, io::tree_effect_key::sea_legs);
}

/// Which sim surface consumes an effect. `unread` is the honest gap: the
/// effect is folded (a modifier still sums into `tree_mod_q`) but nothing
/// downstream reads it yet.
enum class tree_effect_reader : uint8_t
{
    unread = 0,
    ring_gate,        ///< open "ring N": `*_node_available`'s ring lock
    tree_gate,        ///< open "<tree> tree": `polity_holds_*_rim`
    sea_legs_gate,    ///< key sea_legs: the subjection block
    post_roads_gate,  ///< key post_roads: the treasury-bought third road rung
    modifier_defence,     ///< the defender's readiness (campaign pricing and resolution)
    modifier_industrial,  ///< the materials ladder's pull-forward
    modifier_cohesion,    ///< Consolidate's recovery rate
    modifier_research,    ///< every invested tree's research flow
};
tree_effect_reader tree_effect_reader_of(const io::tree_effect& e);

/// THE STATED UNREAD LIST. True for exactly the effects the sim does not
/// consume today: kinds `unlock`, `upgrade`, `retire`, `access`, `reach`,
/// `intel`, `institution`, `doctrine`, `resource` when they carry no key
/// (works and unit rows still gate on the derived band — TREES.md's open
/// question; the rest are prose the sim has no term for), and modifier
/// terms `carrying_capacity`, `manpower`, `stores`, `assimilation`, `plague`,
/// `forage`, `muster_cost` (their consumers read region fields or do not
/// exist in this sim) plus `reach`, which HAS a surface and is withheld on
/// a measured finding (the authored magnitudes collapse the BL-872 distance
/// fixtures — see the holdings-supply site in run_history_sim). A harness
/// asserts every store effect is either read or on this list, and never
/// both, so authoring a new kind or term into a store without a reader
/// fails loudly instead of doing nothing.
bool tree_effect_declared_unread(const io::tree_effect& e);

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
    upgrade_supply, ///< Buy reach directly at a region or a corridor (BL-929).
    organise,   ///< BL-920: grow onto reachable unorganised culture ground.
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

    // --- The route, and what the ration made of it (BL-1022) -------------
    //
    // WHY THESE EXIST. CREEDS.md § Sea legs rests on a claim nobody had
    // measured: a crossing fed near full forage would make coastal ground
    // CHEAPER to take than inland ground. Answering it needs the route a
    // battle was reached by and what it cost both sides, per battle — the
    // counters say how many crossings launched, never what they paid.
    // Same footing as every field above: written once, read by nothing here.
    bool     exec_dry          = false; ///< Staging hub reached the target overland — no sea in the line.
    bool     exec_forages      = false; ///< The force fed normally: dry, or beside a shore its polity holds.
    int      ration_q          = 0;     ///< Sea-legs ration paid on a crossing that could not forage; 0 = starved or foraged.
    int      forage_supply_q   = 0;     ///< What the same march would have drawn foraging (`campaign_supply`).
    int64_t  attacker_lost     = 0;     ///< Heads the attacker lost in this battle.
    int64_t  defender_lost     = 0;     ///< Heads the defender lost in this battle.

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
    treaty_broken,     ///< BL-933: a bound clause was broken before its term ran out.
    faith_sundered,    ///< BL-944: a schism broke away over creed, not reach.
};
inline constexpr int grudge_kind_count = 6;

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

// ---------------------------------------------------------------------------
// Contact (BL-908)
// ---------------------------------------------------------------------------

/// WHAT caused the meeting. Only causes this sim actually resolves — the same
/// discipline `grudge_kind` holds itself to.
enum class contact_kind : uint8_t
{
    campaign  = 0, ///< A campaign crossed onto the other's ground (won or not).
    inherited = 1, ///< Carried forward from a conquered polity's own contacts.
};
inline constexpr int contact_kind_count = 2;

/// The event that FIRST joined the pair. Unlike a grudge, contact does not
/// decay and does not accumulate a score — meeting is a fact, not a magnitude
/// — so one event is the whole of what there is to keep.
struct contact_event
{
    int32_t      year   = 0;
    uint16_t     region = 0xFFFFu; ///< `owner_none` where the event has no place.
    contact_kind kind   = contact_kind::campaign;
};

/// A directed pair, IN THE GRUDGE TABLE'S SHAPE (BL-908): "who has met whom"
/// reads as `pass_one_output::grudges` does, a named pair carrying the event
/// that joined it, rather than a second convention for the same kind of fact.
///
/// Contact is recorded in BOTH directions when it is first established —
/// meeting is mutual even though the record is a directed pair, the same as
/// the table it borrows its shape from stores two independent rows for a
/// mutual fact rather than inventing an undirected edge type.
struct contact
{
    uint16_t      from = 0;
    uint16_t      to   = 0;
    contact_event first;         ///< The event that established the pair.
};

// ---------------------------------------------------------------------------
// Treaties (BL-933) — EXPLORATION.md sec Diplomacy becomes real
// ---------------------------------------------------------------------------
//
// "SAME FAMILY as `grudge` and `contact` — a named pair plus what joined
// them — with two additions: it EXPIRES and it BINDS." A treaty is RECORDED
// as `dated_object` entries in `history_sim_state::dated_objects` — the exact
// seam that struct's own comment names ("this is the seam BL-933's treaty
// objects land in"): `kind` is a `treaty_clause`, `a`/`b` the bound pair
// (canonical `a < b` for the four mutual clauses; `a` = the paying SUBJECT
// and `b` = the OVERLORD for `tribute`, which is directed), `expires_year`
// the shared term. A treaty binding several clauses is several dated_objects
// sharing (a, b, expires_year) — one object per clause rather than a bitmask
// on one, so `expire_dated_objects`'s existing pass needs no change at all to
// expire a treaty clause by clause.
enum class treaty_clause : uint8_t
{
    non_aggression  = 0, ///< Neither campaigns against the other while the term runs.
    trade_access    = 1, ///< One party's market is legible/reachable to the other.
    sphere_of_claim = 2, ///< Non-interference over a native polity's ground, between the two.
    tribute         = 3, ///< A remittance from subject (`a`) to overlord (`b`).
    mutual_defence  = 4, ///< An attack on one draws the other in.
};
inline constexpr int treaty_clause_count = 5;

/// True where a treaty binds @p clause between @p x and @p y (either order
/// for the four mutual clauses; `tribute` reads `a==subject, b==overlord`
/// specifically — pass the subject as @p x for a tribute check). Linear scan
/// over `s.dated_objects`: the table is small (bounded by treaty count x 5
/// clauses, not by region or year count), so a binary search buys nothing a
/// sorted-by-(a,b,kind) discipline would not also have to police on every
/// insert — see `history_sim.cpp` for the read/write sites.
struct history_sim_state; // forward declaration: defined immediately below.
bool has_treaty_clause(const history_sim_state& s, int x, int y, treaty_clause clause);

/// BL-1036 -- ONE RUN'S WHOLE WORKING STATE AT THE TOP OF ONE YEAR, copied
/// when `history_sim_params::capture_year` is reached (`captured` says it
/// was). Instrumentation for the resume-fidelity check: on a resumed run it
/// is the span's OPENING, which must equal the handoff struct it resumed
/// from field for field; on a continued run it is what that year's round
/// opens on, which is what a resume at that year must reproduce.
///
/// READ BY NOTHING IN THE SIM, and empty unless asked for.
struct history_sim_capture
{
    bool    captured = false;
    int64_t year     = 0;

    std::vector<region>           regions;          ///< `ss.regions`, whole.
    std::vector<int>              owner;            ///< The sim's working owner map.
    std::vector<polity>           polities;
    std::vector<grudge>           grudges;
    std::vector<contact>          contacts;
    std::vector<history_corridor> supply_corridors; ///< The RECORD income and trade read.
    std::vector<dated_object>     dated_objects;
    std::vector<trade_flow>       trade_flows;
    std::vector<civilisation>     civilisations;
    std::vector<universal_creed>  universal_creeds;

    /// The LIVE road network reach reads -- every edge in the sim's live
    /// count map as (a, b, uses = the live count, tier = the rung that count
    /// stands at), sorted by (a, b). Not the record above: the record's
    /// `uses` counts walks, the live count is what a purchase or a refused
    /// promotion moved.
    std::vector<history_corridor> live_roads;
};

struct history_sim_state
{
    std::vector<polity> polities;

    /// BL-1036: see `history_sim_capture`. Empty unless the run was asked.
    history_sim_capture capture;

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
    /// BL-950 DIAGNOSTIC, trace only: campaign candidates by the target owner's
    /// contact class -- [0] met before the span, [1] met during it, [2] unmet --
    /// and by gate: [0] examined, [1] treaty-blocked, [2] water-illegal,
    /// [3] reach-denied, [4] season scores clearing the threshold, [5] chosen.
    /// Read by nothing in the sim.
    int64_t campaign_class_trace[3][6] = {};
    /// BL-1018 DIAGNOSTIC, trace only: the RAW visible capability
    /// (`visible_capability_raw`, before `visible_capability_reference` divides
    /// it) of the COUNTERPART in every NEAR-HOME treaty read -- one entry per
    /// side per living long-known pair per decision round, bound or not, read
    /// off the round's opening stocks. Append-only, so a run stopped earlier
    /// holds a prefix of a longer run's vector. The population the alarm
    /// spread and its ceiling share are read from. Read by nothing in the
    /// sim; empty unless `trace_battles`.
    std::vector<int64_t> near_capability_trace;
    /// BL-1019 DIAGNOSTICS, trace only, read by nothing in the sim -- the
    /// first crossing's funnel past the gates `campaign_class_trace` counts.
    ///
    /// New contact PAIRS raised during the run, by `contact_kind`
    /// ([0] campaign -- a crossing onto the other's ground, [1] inherited
    /// from a conquered polity). The span's first-contact count.
    int64_t contacts_raised_trace[2] = {};
    /// Per contact class (as `campaign_class_trace`), over candidates that
    /// passed every gate and were scored: [0] count, and sums of [1] the
    /// ground's worth before odds (after `campaign_gain_q`), [2] `p_win_q`,
    /// [3] supply at the objective, [4] the value the season loop starts
    /// from (every lean and cost applied), [5] the summer defender term,
    /// [6] tiles from the decider's capital, [7] candidates that cannot
    /// forage (a sea leg), [8] the target's `campaign_prize_q` (city and
    /// seat), [9] its ground at the un-jittered farm/ore/port weights,
    /// [10] the value after odds, distance and supply cost (before
    /// foreignness and the leans), [11] the share foreignness leaves
    /// (1000 - the culture discount), [12] candidates an ally's mutual
    /// defence discounted.
    int64_t class_score_trace[3][13] = {};
    /// Of the rounds `unmet_contest_trace[4]` counts, which verb won, indexed
    /// by `sim_verb`.
    int64_t unmet_lost_to_verb_trace[8] = {};
    /// Decision rounds in which an UNMET-owner candidate cleared the
    /// threshold: [0] such rounds, [1] won by an unmet campaign, [2] lost to
    /// a campaign on a pre-span neighbour, [3] lost to a campaign on a pair met
    /// during the span, [4] lost to another verb. And, over the lost rounds,
    /// the summed margin (winning score minus the best unmet score).
    int64_t unmet_contest_trace[5] = {};
    int64_t unmet_contest_margin_sum = 0;
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

    /// THE EVENT LAYER (BL-916) — the named moments, typed, appended at the
    /// sites that already push a prose line into `history`. Ascending by year.
    /// Same discipline as the three above: written by the sim, read by nothing
    /// in it, empty when `params.record_playback` is false. See
    /// era_timelapse.hpp § The event layer for the kinds.
    std::vector<lapse_event>     events;

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

    /// THE SPARSE, DIRECTED CONTACT TABLE (BL-908) — who has met whom, and
    /// what joined them. Sorted ascending by (from, to) and searched by
    /// binary search, same discipline as `grudges` above.
    ///
    /// UNLIKE A GRUDGE, CONTACT SURVIVES A DEATH — see `extinguish_polity`:
    /// a conqueror INHERITS what its victim knew, because the knowledge was
    /// in the seat rather than in the dead ruler. This is the opposite call
    /// from the grudge table's on the same event, and it is deliberate: a
    /// grudge is a feeling a person or lineage holds, which the seat does not
    /// carry forward; contact is a fact about the map, which the seat does.
    std::vector<contact> contacts;

    /// BL-931 — OBJECTS WITH A TERM, ticked down and expired once per decision
    /// round (`expire_dated_objects`). Empty and untouched unless a caller
    /// populates it — nothing in the Empire span's own rules writes here, and
    /// nothing in this item's scope does either; this is the seam BL-933's
    /// treaty objects land in.
    std::vector<dated_object> dated_objects;

    /// BL-954 — THE ROUND'S TRADE (EXPLORATION.md sec Trade is a want met by
    /// throughput). Every flow a bound `trade_access` clause opened on the
    /// most recent decision round, sorted ascending by (seller, buyer, good);
    /// rebuilt, not accumulated, so at the span's close it is the 1660 state.
    /// Empty throughout the Empire span. GENERATION SCRATCH, NOT SAVED, same
    /// footing as `dated_objects` above.
    std::vector<trade_flow> trade_flows;

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
    /// BL-920 -- unorganised ground the ORGANISE verb actually took.
    int64_t organised          = 0;
    /// BL-920 -- regions that crossed `city_state_population_threshold` on
    /// unorganised ground and rose as a new city state, watched across the
    /// span rather than read off only at the opening.
    int64_t city_states_risen  = 0;
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

    /// Campaigns that could NOT forage and were fed anyway, on the reduced
    /// ration their staging region's creed earned (BL-899; CREEDS.md § Sea
    /// legs). Disjoint from `starved_campaigns`: a crossing is one or the
    /// other, and the pair together is every launched crossing.
    ///
    /// REPORT, NEVER A GATE. Its whole job is to answer "did the mechanism fire
    /// at all", which is the question BL-893 could not answer about itself --
    /// it opened legality, changed no outcome, and nothing counted the
    /// difference.
    int64_t sea_legs_fed_campaigns = 0;

    // --- BL-837: ancient roads and the reach GATE ---------------------------

    /// Campaign candidates REFUSED because the target sits beyond
    /// `sustainable_campaign_floor_q` — the GATE, not the price. Distinct
    /// from `illegal_campaigns` (a water-domain refusal): this one fires on
    /// ordinary dry or forage-legal ground that is simply too far, unroaded,
    /// to project force onto. A world with roads and one without should
    /// differ here first, before either differs in battles.
    int64_t reach_denied_campaigns = 0;

    // --- BL-838: fear of being next ----------------------------------------
    //
    // REPORTED, NEVER GATED. Both are bookkeeping: nothing reads them to make
    // a decision, so a run is byte-identical whether they are examined or not.

    /// Campaign CANDIDATES whose score was leaned by `w_fear_q` -- i.e. the
    /// target carried a non-zero grudge total among the decider's own people.
    /// Zero on a world where nobody has yet done anything to anybody, and on
    /// every world at the struct default, where `w_fear_q` is 0.
    int64_t fear_leaned_campaigns = 0;

    /// DISTINCT polities that attracted at least one such lean. Read WITH the
    /// count above and not instead of it: a large figure here with a small one
    /// above is diffuse resentment, the reverse is one realm everybody fears,
    /// and only the second is the coalition this item is about.
    int64_t fear_targets_distinct = 0;

    /// The ids behind `fear_targets_distinct`, kept SORTED and unique so the
    /// count is a property of the integers in it rather than of an insertion
    /// order -- the same discipline `grudges` itself is held to.
    std::vector<uint16_t> fear_targets_seen;

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
    int64_t materials_from_trade = 0; ///< BL-895: of `materials_produced`, the share the network yielded.

    /// BL-1021 -- BL-895's OWN "DONE WHEN", counted where it is decided: does a
    /// polity connected to unlike ground sustain campaigns a disconnected one
    /// cannot? Every array is indexed SIZE BAND x 4 + KINDS: the kinds are how
    /// many DISTINCT KINDS of ground (0..3, `region_trade_class`) the acting
    /// realm reached that year -- 0 or 1 is a realm with no trade income, 2 or
    /// more is one connected to unlike ground -- and the size band is regions
    /// held that year, 1 / 2-3 / 4-7 / 8+ (0..3), because a connected realm is
    /// usually a larger one. Read the same whether `trade_income_per_class` is
    /// on or off, so a zero-income control run classifies realms identically.
    ///   - `realm_years_*`: living polity-years in the bucket (the denominator);
    ///   - `campaigns_*`: campaigns launched, and their material COST against
    ///     what the seat could actually SPEND (a shortfall marches understocked);
    ///   - `upkeep_*`: standing-army upkeep DUE against upkeep PAID (the unpaid
    ///     share walks home -- the strangling channel).
    std::array<int64_t, 16> realm_years_by_trade_bucket{};
    std::array<int64_t, 16> campaigns_by_trade_bucket{};
    std::array<int64_t, 16> campaign_cost_by_trade_bucket{};
    std::array<int64_t, 16> campaign_spent_by_trade_bucket{};
    std::array<int64_t, 16> upkeep_due_by_trade_bucket{};
    std::array<int64_t, 16> upkeep_paid_by_trade_bucket{};

    /// BL-895's TWO SINKS, as run totals on the same footing as
    /// `materials_spent_on_campaigns`. Before they existed a campaign was the
    /// only thing materials were ever spent on, so an income could not gate
    /// anything; these two are what a realm's stock now competes for.
    int64_t materials_spent_on_upkeep = 0; ///< Standing-army upkeep, all regions, all years.
    int64_t materials_spent_on_roads  = 0; ///< Corridor promotions actually paid for.

    /// Heads sent home because the seat could not pay them, and corridor
    /// promotions REFUSED for the same reason. Both are the observable that
    /// separates "the sinks are live" from "the sinks are biting".
    int64_t army_heads_unpaid_disbanded = 0;
    int64_t road_builds_refused         = 0;

    /// BL-929 -- how many supply sites a realm bought outright, of which
    /// kind, and what it cost. `_regions` is a region's own reach relief
    /// bought directly; `_corridors` is one incoming edge's tier bought
    /// outright rather than walked into existence. The split is the
    /// observable that tells "the mechanism never wins the region case" from
    /// "the mechanism never wins the corridor case", the same split
    /// `works_by_span_band` exists to make for `build_work`.
    int64_t supply_sites_upgraded           = 0;
    int64_t supply_sites_upgraded_regions   = 0;
    int64_t supply_sites_upgraded_corridors = 0;
    int64_t materials_spent_on_supply_sites = 0;

    /// BL-940 -- corridors promoted to the road ladder's third rung (Post
    /// Road), and the treasury actually spent on them. The observable that
    /// separates "the mechanism never fires" from "no polity ever holds
    /// EX-WY-1a in this seed", the same split `supply_sites_upgraded` makes.
    int64_t post_roads_built           = 0;
    int64_t treasury_spent_on_roads    = 0;
    /// BL-949: post roads bought per polity id (grown on demand, so a polity
    /// past the end bought none). Lets a sweep ask whether the spend tracks
    /// the polities that built rather than only the world total.
    std::vector<int32_t> post_roads_by_polity;

    // --- BL-933/934/935 sweep counters --------------------------------------

    /// Treaties formed / broken, this run. `treaties_broken` here is EVENT
    /// count (how many times a defection happened), never the per-polity
    /// ledger `polity::treaties_broken` reads — same split every other
    /// _count/_regions pair above makes.
    int64_t treaties_formed  = 0;
    int64_t treaties_broken  = 0;
    /// Campaign candidates skipped outright because a non-aggression clause
    /// bound the pair — the direct, countable cause of the displacement
    /// reading's "neighbour-war rate falls" half.
    int64_t treaty_blocked_campaigns = 0;

    int64_t subjections_formed = 0; ///< BL-934: new overlord links this run.
    int64_t subjections_freed  = 0; ///< BL-934: refused-renewal secessions this run.
    int64_t tribute_remitted   = 0; ///< BL-934: total treasury moved subject -> overlord.

    /// BL-935: treasury actually spent building each stock, this run — the
    /// observable that separates "the mechanism never fires" from "no polity
    /// ever affords it in this seed."
    int64_t treasury_spent_on_ports          = 0;
    int64_t treasury_spent_on_navies         = 0;
    int64_t treasury_spent_on_standing_armies = 0;
    /// BL-972: the BILL, summed over every decision round -- treasury paid
    /// for paid standing heads and for hulls, and the polity-rounds on which
    /// each bill went short. The observable for "a cost in the world binds".
    int64_t treasury_spent_on_army_upkeep = 0;
    int64_t treasury_spent_on_navy_upkeep = 0;
    int64_t army_upkeep_unpaid_rounds     = 0;
    int64_t navy_upkeep_unpaid_rounds     = 0;
    /// BL-972: the LEVY -- heads drawn from seats' manpower pools by army
    /// steps, and heads sent home to a pool by the unpaid decay, all rounds.
    int64_t levy_heads_raised   = 0;
    int64_t levy_heads_returned = 0;

    /// BL-955: stock steps bought this run, by kind — with the allocation a
    /// polity buys at most one per round, so these count CHOICES made.
    int64_t port_steps_bought  = 0;
    int64_t navy_steps_bought  = 0;
    int64_t army_steps_bought  = 0;
    /// BL-955: one flag per polity index, set once that polity's navy has
    /// decayed from a standing fleet to zero at least once this run ("at
    /// least one allowed to decay", reading 7). GENERATION SCRATCH, NOT SAVED,
    /// same footing as `trade_flows`.
    std::vector<uint8_t> navy_lapsed;
    /// BL-955: regions found breaking the paid standing army's raw invariant
    /// (`standing_army_invariant_holds`), summed over every decision round's
    /// check and the close. Must be 0. Exploration span only; not saved.
    int64_t standing_army_invariant_violations = 0;

    /// BL-896 -- how many successor realms the dark age produced, and how much
    /// ground walked away with them. The pair is the item's "done when": an
    /// empire that forms and then fragments shows both non-zero, and a world
    /// whose networks always held shows both zero without anything being
    /// broken.
    int64_t secessions      = 0;
    int64_t regions_seceded = 0;
    /// BL-922 -- of `regions_seceded`, how many had a FED region of their
    /// own realm within `neighbour_radius` when they walked. Under the
    /// degree-capped campaign index (BL-855) that was 70-80% of all
    /// secessions -- the graph, not the map, had cut them off -- which is
    /// why supply walks its own uncapped index. What remains is the
    /// residual (a fed neighbour that is itself past the floor's distance,
    /// or a relay that moved this round); a rise here says the supply index
    /// and the map disagree again. Report-only; `history_sweep` prints it.
    int64_t regions_seceded_graph_cut = 0;

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

    /// BL-897 — EVERY UNIVERSALISING CREED THIS RUN COINED, in the order they
    /// arose. A NAMED RECORD each; `region::universal_creed` and
    /// `polity::universal_creed` index into this. Empty on a world of intact,
    /// isolated realms, which CREEDS.md rules a legitimate world.
    std::vector<universal_creed> universal_creeds;

    /// The item's "done when", as four counters that separate the four things
    /// that can go wrong. ZERO ARISEN is a world nothing humbled or nothing
    /// connected. Arisen with ZERO ADOPTED means the creed never left the realm
    /// that coined it — contact is not reaching. Adopted with zero converted
    /// means the institution stands over ground that never took it, which is
    /// the fault line at its widest rather than a bug. And REASSERTED is the
    /// turmoil half: peoples the creed reached and lost.
    int64_t universal_creeds_arisen = 0;
    int64_t polities_adopted_creed  = 0;
    int64_t peoples_converted       = 0;
    int64_t peoples_reasserted      = 0;
    int64_t civilisations_formed = 0;

    /// BL-944 — THE SCHISM: a realm fractures along the gap between its
    /// adopted institution and a REASSERTED people's own pantheon, never
    /// along reach. Counted separately from `secessions`/`regions_seceded`
    /// on purpose — the two are read side by side so a schism can never
    /// silently blend into a network-failure collapse in the reading. A
    /// world where `peoples_reasserted` is non-zero but `schisms` is zero
    /// is a legitimate world: reassertion alone is not yet a large enough
    /// break to fracture a realm.
    int64_t schisms         = 0;
    int64_t regions_sundered = 0;

    // --- BL-926 — THE INSTRUMENT SEES THE ARC. Pure counters, decision-free.
    //
    // Every field below is written beside an event the sim already performs
    // and read by nothing inside the loop. They exist so history_sweep can
    // report the shape of a run rather than its net: `secessions` above counts
    // PIECES and cannot tell one empire shedding six provinces from six
    // empires each losing one, and `foundings` sums two sources with opposite
    // meanings (a schedule playing back vs a polity choosing to settle).

    /// BREAKDOWNS: parent polities that lost at least one block in a round —
    /// the event count, where `secessions` is the piece count.
    int64_t breakdowns = 0;
    /// The size, in regions, of every piece that walked, in the order the
    /// BL-896 block allocated them. Sums to `regions_seceded`.
    std::vector<int32_t> secession_piece_sizes;

    /// `foundings`, split by source. The schedule is the colonisation
    /// diffusion's playback (BL-846, `pending_foundings`); the verb is a
    /// polity's own Settle. `foundings == foundings_scheduled + foundings_settled`.
    int64_t foundings_scheduled = 0;
    int64_t foundings_settled   = 0;
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
                                  const works_registry*     works         = nullptr,
                                  era_lapse_tap*            tap           = nullptr);

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

/// Bytes the EVENT LAYER occupies (BL-916). Disjoint from the two above.
inline int64_t event_record_bytes(const history_sim_state& s)
{
    return static_cast<int64_t>(s.events.size()) * static_cast<int64_t>(sizeof(lapse_event));
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
    t.events          = s.events;
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

/// One round of grudge decay at @p step_years (BL-827; BL-842). Each score
/// sheds `grudge_decay_per_year_q x step` per-mille of itself, never less than
/// ONE unit while it stands above `grudge_floor` -- so no score above the floor
/// is ever stationary across a round -- and rows at or under the floor are
/// erased, which is what keeps the table sparse. The round loop's only decay
/// site; public so the harness asserts the same arithmetic the run performs.
void decay_grudges(std::vector<grudge>& grudges, const history_sim_params& params, int step_years);

/// The @p n strongest pairs, sorted descending by `score`, ties on `peak`, then
/// on (from, to). A TOTAL order with an explicit tie-break, so the listing is
/// identical on every machine — the same discipline the scorer's argmax uses.
std::vector<grudge> top_grudges(const history_sim_state& s, int n);

/// BL-838 -- FEAR OF BEING NEXT, in the 0-1000 currency every lean in this file
/// speaks. "How much have people LIKE `decider` suffered at `target`'s hands",
/// read from the sparse grudge ledger and normalised by
/// `history_sim_params::fear_reference`.
///
/// A FREE FUNCTION RATHER THAN A LAMBDA INSIDE THE SCORER so the rule can be
/// asserted directly. The scope of AI_OPPONENT.md sec 11's 2026-09-11 grant is
/// behavioural, not structural -- a large but PEACEFUL polity must attract no
/// coalition while a smaller AGGRESSIVE one does -- and that is a claim about
/// THIS function, which history_sweep's F-checks put to it on a hand-built
/// ledger rather than inferring it from a run's aggregates.
///
/// Sums the grudges held against `target` by LIVING polities of `decider`'s own
/// culture, EXCLUDING `decider` itself. The exclusion is the grant's, not an
/// optimisation: a polity reading its own ledger to pick a target is the
/// revenge term BL-827 declined. Nothing here reads size, rank, holdings,
/// population or army.
int fear_of_next_q(const history_sim_state& s, const history_sim_params& p,
                   int decider, int target);

/// One line naming a grudge event: what, where, when. The printable half of
/// "it must carry its cause".
std::string grudge_event_line(const grudge_event& e, const settlement_state& ss);

// ---------------------------------------------------------------------------
// Contact reads (BL-908)
// ---------------------------------------------------------------------------

/// True where @p from has met @p to, i.e. an entry exists in the sparse
/// contact table. Binary search over the sorted table, same shape as
/// `grudge_between`.
bool has_contact(const history_sim_state& s, int from, int to);

/// BL-941 — the calendar year @p from and @p to FIRST met, or `INT64_MAX`
/// where the pair has no contact entry. Used to tell a long-known neighbour
/// (contact predates the span) from a frontier pair (met during it) — see
/// `treaty_value_q`'s `near_home` argument. `INT64_MAX` (never "far" nor
/// "near" by accident) rather than a sentinel year, since a pair with no
/// recorded contact should never reach a caller that treats it as either.
int64_t contact_first_year(const history_sim_state& s, int from, int to);

// ---------------------------------------------------------------------------
// Treaty scoring (BL-933) — EXPLORATION.md sec Diplomacy becomes real:
// "nobody negotiates... evaluated against the same seeded world state."
// ---------------------------------------------------------------------------

/// ONE PARTY'S OWN VALUE FOR BINDING WITH ANOTHER, in the same currency as
/// `fear_of_next_q` and for the same reason it is a free function: a claim
/// about a formation threshold has to be put to THIS function directly. Pure,
/// integer, seedless — a deterministic consequence of the two grudge readings
/// between the pair, the counterpart's own defection record, how much freedom
/// the decider's own doctrine prices a binding at, and (BL-941) the
/// counterpart's visible capability, gated on whether the pair sits near home.
///
/// `grudge_against_other_q`/`grudge_from_other_q` are `grudge_between`'s two
/// directions (the decider's grudge against the counterpart, and the
/// counterpart's grudge against the decider) — a biting mutual history lowers
/// the value of promising peace with exactly the realm the promise is least
/// credible toward. `counterpart_treaties_broken` is the counterpart's OWN
/// `polity::treaties_broken` — never the decider's. `decider_aggression_q` is
/// the decider's own doctrine lean (0-1000): a high-aggression culture prices
/// the freedom a non-aggression clause costs it higher, so it takes more
/// peace-value to clear the same threshold. This is the pair's CEILING term
/// (ERAS.md sec The two scalars): restraint priced by the decider's own
/// doctrine, already present before BL-941.
///
/// BL-941 ADDS ALARM. `alarm_from_other_q` is the decider's own reading of the
/// counterpart's VISIBLE capability (`deterrence_alarm_q`, 0-1000) — a fleet
/// or standing army read by a neighbour, whether or not it is ever used.
/// `near_home` is whether the pair's contact predates this span (a long-known
/// neighbour) rather than being met during it (a frontier pair,
/// EXPLORATION.md sec The arms race reinforces peace near home...). Near
/// home, Alarm RAISES the value — visible capability buys quiet. Far from
/// home, Alarm earns nothing and a flat `treaty_far_penalty_q` applies
/// instead — the same purchase meets no deterrent and should not bind as
/// readily, which is what keeps a funded port's cheap crossing from being
/// treatied away before it is ever used (NR-851).
///
/// BL-954 ADDS TRADE. @p trade_value_q is the MARGINAL volume the pair's
/// trade-access clause WOULD open beyond what other partners already carry,
/// both directions, every good (`pair_trade_value_q`), weighted by `treaty_trade_weight_q` per mille and
/// added NEAR HOME AND FAR ALIKE — the one term that can make a distant pair
/// worth a promise. The partner is worth more alive.
int treaty_value_q(const history_sim_params& p,
                    int grudge_against_other_q, int grudge_from_other_q,
                    int counterpart_treaties_broken, int decider_aggression_q,
                    int alarm_from_other_q, bool near_home,
                    int trade_value_q);

/// BL-941 — VISIBLE CAPABILITY, 0-1000. What a neighbour reads of
/// `polity_id`'s capital `region::army_stock` plus its own `polity::navy_stock`,
/// scaled by `history_sim_params::visible_capability_reference` — the same
/// "plain integer over a reference constant" shape `fear_of_next_q` already
/// uses for `fear_reference`. Reads the CAPITAL region only: a standing force
/// is raised and paid for at the seat (EXPLORATION.md sec Force persists
/// now), so the seat is where it stands and where a neighbour would see it.
/// 0 for a dead polity, an out-of-range id, or a capital out of `regions`'s
/// bounds.
int visible_capability_q(const std::vector<region>& regions, const history_sim_state& s,
                          const history_sim_params& p, int polity_id);

/// BL-1018 — the same read UNSCALED, for the trace alone: capital `army_stock`
/// plus `navy_stock`, in heads and hulls, before the reference divides it. 0
/// on the same conditions as `visible_capability_q`. A pure read that no
/// decision calls (`visible_capability_q` keeps its own copy of the
/// arithmetic, so the decision path is untouched); `near_capability_trace`
/// and the sweep's alarm spread read it. The two must agree: if the numerator
/// of `visible_capability_q` changes, this changes with it.
int64_t visible_capability_raw(const std::vector<region>& regions, const history_sim_state& s,
                               int polity_id);

/// BL-941 — ALARM: how threatened `self` feels by `other`'s visible
/// capability, gated on contact (the omniscience guard every want-shaped read
/// in this file applies — a stranger's arsenal is not visible to a polity
/// that has never met it). 0 where the two have not met.
int deterrence_alarm_q(const std::vector<region>& regions, const history_sim_state& s,
                        const history_sim_params& p, int self, int other);

// ---------------------------------------------------------------------------
// BL-955 — spend is ALLOCATED (EXPLORATION.md sec Force persists now)
// ---------------------------------------------------------------------------

/// The lean RANKS the allocation reads (NR-864). Over the LIVING polities whose
/// `culture` indexes @p cs, each one's per-mille rank of its
/// `expansion_lean_q` and of its `consolidator_lean_q`:
///     rank = (count with a STRICTLY lower lean) * 1000 / max(1, n - 1)
/// so ties share a rank, the lowest reads 0 and the highest 1000. Both output
/// vectors are sized to @p polities; a dead or culture-less polity (or every
/// polity, when @p cs is null) reads 0 on both. Pure; computed once per round.
void exploration_lean_ranks(const std::vector<polity>& polities, const creed_state* cs,
                            std::vector<int>& expansion_rank_q,
                            std::vector<int>& consolidator_rank_q);

/// The campaign scorer's estimate of the men a target will field: its
/// `army_stock` plus the defence levy `muster_garrison` (called with
/// `defence_levy_q`) would raise before the fight. The levy reads the ORDINARY
/// men only, as the muster does, so paid standing heads never suppress it.
/// Models the levy half only; the muster's disband of excess ordinary men is
/// not priced, as before BL-955.
int64_t defender_levy_estimate(const region& tgt, const history_sim_params& params);

/// The four options, in TIE-BREAK order: an exact score tie goes to the
/// lower enumerator (hold, then army, then port, then navy).
enum class exploration_spend_option : uint8_t
{
    hold      = 0,
    army_step = 1,
    port_step = 2,
    navy_step = 3,
};

/// Everything one polity's choice reads, already reduced to integers.
struct exploration_spend_facts
{
    int     expansion_rank_q    = 0; ///< `exploration_lean_ranks`, 0-1000.
    int     consolidator_rank_q = 0; ///< `exploration_lean_ranks`, 0-1000.
    int     water_want_q        = 0; ///< across-water want, 0-1000 (see the upkeep).
    int     alarm_q             = 0; ///< max near-home `deterrence_alarm_q`, 0-1000.
    int64_t treasury            = 0; ///< the capital seat's treasury after EARN.
    int     port_window_q       = 0; ///< the seat's `port_q` endowment window.
    int     port_stock_q        = 0; ///< the seat's built port, 0-1000.
    int64_t navy_stock          = 0;
    /// The realm's PAID standing army (every held region's
    /// `standing_army_heads`), >= 0. Read by nothing in the scorer since
    /// BL-972 removed the cap; carried so a caller can print what the bill
    /// stood on.
    int64_t standing_army       = 0;
    /// BL-972: the heads the seat's `manpower_stock` can lend this round
    /// (`manpower_stock * standing_army_levy_per_mille_q / 1000`). The army
    /// step is eligible only when this covers a whole step.
    int64_t levy_room           = 0;
};

/// Each option's score and whether it may be taken at all.
struct exploration_spend_scores
{
    int  hold_q = 0, army_q = 0, port_q = 0, navy_q = 0;
    bool army_eligible = false, port_eligible = false, navy_eligible = false;
};

/// The scores (0-1000 each, integer):
///   outward = (expansion_rank * w_expansion + water_want * w_water_want) / 1000
///   port    = outward * (1000 - port_stock) / 1000
///   navy    = outward
///   army    = (consolidator_rank * w_consolidator + alarm * w_alarm) / 1000
///   hold    = hold_base + consolidator_rank * w_hold_consolidator / 1000
/// No score saturates (BL-972): what limits a stock is its bill, paid before
/// this choice is made, and the levy bound below.
/// Eligibility: port needs a cost > 0 the treasury covers, a port window and
/// port_stock < 1000; navy a cost > 0 the treasury covers and port_stock >=
/// `navy_min_port_stock_q`; army a cost > 0 the treasury covers AND
/// `levy_room >= standing_army_build_step_q`. Hold always.
exploration_spend_scores score_exploration_spend(const history_sim_params&    p,
                                                 const exploration_spend_facts& f);

/// Argmax over a TOTAL order: the higher score wins; an exact tie goes to
/// hold, then army, then port, then navy. Ineligible options never win.
exploration_spend_option choose_exploration_spend(const exploration_spend_scores& s);

// ---------------------------------------------------------------------------
// The directed want table (BL-909)
// ---------------------------------------------------------------------------

/// A directed want, IN THE GRUDGE/CONTACT TABLE'S SHAPE (BL-909): "A wants
/// what B holds" — a named holder and a named good, never a per-polity
/// scarcity list. CIVILISATION.md sec The directed want owns the design.
///
/// NO PRICE, NO MAGNITUDE. The want is a KNOWN ABSENCE (`from`'s ground never
/// reaches this good's world-relative dominance floor) plus a KNOWN HOLDER
/// (`to`'s ground does reach it) — nothing more. Population demand belongs to
/// the economy pass and never leaks back here.
struct want
{
    uint16_t     from = 0;                  ///< The polity that lacks the good.
    uint16_t     to   = 0;                  ///< The polity known to hold it.
    region_class good = region_class::none; ///< Never `none` in a stored entry.

    /// TRUE WHEN `to`'S MARKET ALSO SHOWS THIS GOOD (BL-910), i.e. at least
    /// one of `to`'s market regions is itself dominant in `good` — a market
    /// is a visible condition, a place, so a good present at a region that
    /// also carries a market is doubly legible: geology AND the richer
    /// signal a trading age actually acts on (CIVILISATION.md sec The
    /// directed want). False where the want is known only from B's ground,
    /// with no market region showing it.
    bool via_market = false;
};

/// The directed want table (BL-909), crossed at the pass 1 -> pass 2
/// handoff by `make_pass_one_output`. Sorted ascending by (from, to, good),
/// so the order is a property of the integers rather than of iteration.
/// Defined only over pairs `has_contact` already joins — the omniscience
/// guard CIVILISATION.md sec The directed want is explicit about.
std::vector<want> derive_wants(const std::vector<region>& regions,
                                const std::vector<contact>& contacts,
                                const std::vector<polity>&  polities);

// ---------------------------------------------------------------------------
// Cultural good preference (BL-936) — EXPLORATION.md sec A good acquires a
// cultural preference: "preference attaches to a CULTURE, not a polity."
// ---------------------------------------------------------------------------

/// ONE CULTURE'S WEIGHT ON ONE GOOD IT LACKS, 0-1000. A SEPARATE TABLE FROM
/// `want`, deliberately: `want`'s own doc comment is "NO PRICE, NO
/// MAGNITUDE", so a weight belongs beside it, not inside it. A consumer that
/// wants to rank a polity's own wants reads this table by the polity's
/// FOUNDING culture (`polity::culture`) or by whichever culture holds a
/// region's plurality share, per its own need — this struct does not decide
/// that for it.
struct culture_good_preference
{
    int16_t      culture  = -1;             ///< Index into `creed_state::cultures`.
    region_class good     = region_class::none; ///< Never `none` in a stored entry.

    /// 0-1000 — how strongly this culture prefers this good, derived from how
    /// widely its route has exposed it to holders of a good its own ground
    /// never reaches. NOT a price (Digitisation's job); enough to RANK which
    /// directed want a fleet answers first.
    int16_t weight_q = 0;
};

/// Derives `culture_good_preference` at CULTURE GRAIN — the same two facts
/// `derive_wants` reads (a known absence, a known holder reached by contact),
/// regrouped by `region::culture`'s plurality share (`culture.id[0]`) instead
/// of by `region::nation`, so a preference spreads with people rather than
/// stopping at a border. NEVER ROLLED: a pure, deterministic fold over
/// @p regions/@p contacts/@p polities, no RNG of its own — same discipline
/// `derive_wants` holds itself to. @p culture_count bounds which culture
/// indices are read, same convention `pass_one_output::culture_count` sets.
///
/// A culture with no plurality-held ground and no contacted polity holding a
/// good it lacks contributes nothing for that good — an absence with no
/// route yet is not a preference (EXPLORATION.md sec A good acquires a
/// cultural preference: "derived... from what its route exposed it to").
std::vector<culture_good_preference> derive_culture_preference(
    const std::vector<region>& regions, const std::vector<contact>& contacts,
    const std::vector<polity>& polities, int culture_count);

// ---------------------------------------------------------------------------
// The scarcity signal (BL-939) — EXPLORATION.md sec There is no price here,
// only a scarcity signal.
// ---------------------------------------------------------------------------

/// Index into `region::scarcity_q`/the fixed 4-good order, or -1 for
/// `region_class::none` (never scored). farm=0, ore=1, energy=2, port=3.
int scarcity_good_index(region_class good);

/// Refreshes every market region's `scarcity_q`, in place, for one decision
/// round (BL-939). NO PRICE, NO CLEARING — a market's signal for a good is 0
/// where its OWN ground is dominant in it (nothing to want locally), and
/// otherwise a function of whether its HOLDING POLITY lacks the good
/// anywhere on its ground at all, plus that polity's own population as a
/// demand-pressure term. Called from `run_exploration_upkeep`, never on its
/// own — see that function for when in the round it runs.
void refresh_market_scarcity(std::vector<region>& regions, const std::vector<polity>& polities);

/// Read @p market_region's scarcity signal for @p good, AS VISIBLE TO
/// @p viewer_polity — the omniscience guard every want-shaped read in this
/// file applies (CIVILISATION.md sec The directed want): a polity reads its
/// OWN market's signal unconditionally, a foreign market's once
/// `has_contact` says the pair has met, and 0 otherwise (never a market at
/// all, or a stranger who has not met the holder — EXPLORATION_TREE.md's
/// EX-GD-2a, Quayside Market, is what widens the latter case for a polity
/// that holds it; that node's own consumer is not built by this item).
int market_scarcity_q(const std::vector<region>& regions, const history_sim_state& s,
                       int viewer_polity, int market_region, region_class good);

// ---------------------------------------------------------------------------
// Trade flows (BL-954) — EXPLORATION.md sec Trade is a want met by throughput.
// "A flow needs three things at once, and each is already in the world": a
// WANT (the buyer market's raw signal), a HOLDER (the seller's ground dominant
// in the good), a LINE (held corridors on land, a built port and a navy across
// water). The volume is the smallest of the three.
// ---------------------------------------------------------------------------

/// The per-round reads every flow in a round shares, folded ONCE so sizing a
/// flow costs a lookup rather than a walk of every region and corridor.
/// Built by `build_trade_context` off the same round's regions, polities and
/// corridors; stale the moment ground changes hands, so never kept across a
/// round.
struct trade_context
{
    /// Per polity (indexed by id), per good: per-mille share of the polity's
    /// held regions whose `dominant` is the good. 0 for a polity holding
    /// nothing.
    std::vector<std::array<int32_t, 4>> holding_q;

    /// LAND LINES, one per unordered polity pair a supply corridor joins
    /// (one endpoint held by each): (lo, hi, line_q), sorted by (lo, hi).
    /// `line_q` is the best such corridor's min(`network_supply_q`) over its
    /// two endpoints — each side's own reach from its seat to the shared
    /// border, which is where the goods change hands.
    struct land_line
    {
        uint16_t lo = 0, hi = 0;
        int32_t  line_q = 0;
    };
    std::vector<land_line> land_lines;
};

trade_context build_trade_context(const std::vector<region>&           regions,
                                  const std::vector<polity>&           polities,
                                  const std::vector<history_corridor>& corridors);

/// THE VOLUME ONE DIRECTED (seller, buyer, good) FLOW WOULD CARRY, ignoring
/// the clause gate: min(buyer capital's `scarcity_raw_q[good]`, seller's
/// `holding_q[good]`, line_q), where line_q = max(land, sea); land is the
/// pair's `land_line` (0 without one) and sea is min(seller seat
/// `port_stock_q`, buyer seat `port_stock_q`) while the seller holds a navy
/// (`navy_stock > 0`), else 0. 0 for a dead or out-of-range party, the same
/// polity on both sides, a capital out of range, or @p good outside 0..3.
int trade_flow_volume_q(const trade_context& ctx, const std::vector<region>& regions,
                        const std::vector<polity>& polities,
                        int seller, int buyer, int good);

/// THE MARGINAL TRADE A BINDING WOULD OPEN, ignoring the clause gate —
/// computable before the pair binds, which is what `treaty_value_q` needs.
/// Summed over both directions (seller -> buyer) and all four goods:
/// min(`trade_flow_volume_q`, max(0, buyer's raw want less the volume OTHER
/// sellers already bring it in @p flows), max(0, seller's holding less the
/// volume it already sends OTHER buyers in @p flows)). Flows between @p a
/// and @p b themselves count against neither remainder. @p flows is the
/// current round's (`compute_trade_flows`). Pure.
int pair_trade_value_q(const trade_context& ctx, const std::vector<region>& regions,
                       const std::vector<polity>& polities, int a, int b,
                       const std::vector<trade_flow>& flows);

/// Every flow the bound `trade_access` clauses in @p treaties open this
/// round: per bound pair, both directions, every good with volume > 0.
/// ONLY THE CLAUSE OPENS A FLOW — contact alone never does. One want is
/// shared across a buyer's sellers (spent fattest line first, ties to the
/// lower seller), then one holding across a seller's buyers (fattest flow
/// first, ties to the lower buyer). Sorted ascending by (seller, buyer, good).
/// Pure; reads the RAW signal, never the relieved one.
std::vector<trade_flow> compute_trade_flows(const trade_context&             ctx,
                                            const std::vector<region>&       regions,
                                            const std::vector<polity>&       polities,
                                            const std::vector<dated_object>& treaties);

// ---------------------------------------------------------------------------
// A want points a campaign outward (BL-953) — EXPLORATION.md sec A want
// points a campaign outward.
// ---------------------------------------------------------------------------

/// @p polity_id's WANT for @p good, 0-1000: its OWN capital's `scarcity_q`
/// for the good (0 when the capital has no market, the good has no scarcity
/// index, or the id/capital is out of range), weighted by its people's
/// preference for it — `scarcity * (500 + weight_q / 2) / 1000`. "Its
/// people" is the capital region's plurality culture (the grain
/// `derive_culture_preference` keys on), falling back to the polity's
/// founding culture where the capital carries no share. A good with no
/// preference entry weighs at the 500 floor: an unmet want still counts,
/// preference only sharpens it. @p prefs must be sorted ascending by culture,
/// as `derive_culture_preference` produces it. Pure.
int polity_good_want_q(const std::vector<region>& regions, const std::vector<polity>& polities,
                       const std::vector<culture_good_preference>& prefs,
                       int polity_id, region_class good);

/// The campaign prize @p value leaned by a want: `value + value * w_want_q *
/// want_q / 10^6`, integer arithmetic. A NON-POSITIVE prize is returned
/// unchanged (a want ranks winnable campaigns, never rescues a loss), and the
/// result is never below 0. Pure; the scorer's one call site and the harness
/// both go through this.
int want_leaned_campaign_value(int value, int w_want_q, int want_q);

/// Subjection's pick among ELIGIBLE natives, each given as (native polity id,
/// the arriving power's want for that native capital's dominant good): the
/// highest want wins, ties to the LOWER id; -1 when @p candidates is empty.
/// With every want 0 this is exactly "the lowest eligible id", the id-order
/// walk it replaces. Pure and order-independent in its input.
int choose_subjection_native(const std::vector<std::pair<int, int>>& candidates);

// ---------------------------------------------------------------------------
// The turbulence lean, resolved (BL-839)
// ---------------------------------------------------------------------------
//
// THREE FREE FUNCTIONS RATHER THAN THREE INLINE MULTIPLICATIONS, for the same
// reason `fear_of_next_q` is a free function: the claim the item has to make is
// a claim about these, and a harness can put it to them directly instead of
// inferring it from a run's aggregates. Each is PURE in its arguments, uses
// integer arithmetic only, and returns its input unchanged when
// `turbulence_lean` is 0 -- which is what "the ordinary setting re-bases
// nothing" means, stated as a property rather than as a comment.

/// A culture's `aggression_q`, with the turbulence lean's SPREAD applied.
///
/// Widens or narrows the distance from the 500 neutral, so the neutral itself
/// is a fixed point at every setting and the no-creeds fallback (a flat 500)
/// is identically unmoved. Clamped to 0-1000, which is the currency every
/// consumer of `aggression_q` already speaks.
int leaned_aggression_q(const history_sim_params& p, int culture_aggression_q);

/// `w_fear_q` with the turbulence lean applied. Scales, never replaces: a run
/// whose `w_fear_q` is 0 stays at 0 at every setting.
int leaned_w_fear_q(const history_sim_params& p);

/// `terrain_reach_cost_q` with the turbulence lean applied. Scales, never
/// replaces, and never goes negative.
int leaned_terrain_reach_cost_q(const history_sim_params& p);

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
///                                  + `cultures` (the table itself, BL-969)
///   - works                     -> `region::works_built` and the five
///                                  `work_*_mod` fields, plus `works_by_span_band`
///   - the strain accumulators   -> `region::contest_q` and `polity::cohesion_q`
///   - grudges (BL-827)          -> `grudges`
///   - contact (BL-908)          -> `contacts`
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
    /// bound-check a share without holding a `creed_state`. Always equals
    /// `cultures.size()` on a value the fold produced; the validator checks it.
    int culture_count = 0;

    /// THE CULTURE TABLE ITSELF (BL-969) -- a copy of `creed_state::cultures`
    /// as it stood at the fold. The doc's list names "cultures" as crossing,
    /// and until this field the shares crossed here while the table they
    /// index into (pantheon, tongue, aggression, parentage, coining year)
    /// crossed OUT OF BAND through the live `creed_state` -- so "the struct
    /// is the whole of what crosses" was false for the largest row of the
    /// contract. The sim DOES write that table (a coined civilisation, a
    /// schism), so a copy at the close is a real record, not a duplicate of
    /// the migration's.
    ///
    /// Consumers keep reading `creed_state`; what this field buys is the
    /// CHECK. `pass_one_output_valid` compares it against the live table it
    /// is handed, so the two channels are proven to agree at the fold rather
    /// than assumed to.
    std::vector<culture> cultures;

    /// Works raised, cross-tabulated [span][band] exactly as the sim counts them.
    std::array<std::array<int64_t, roster_band_count>, 2> works_by_span_band{};

    /// The directed grudge table (BL-827) — an input to sentiment at world
    /// setup, never a quantity of its own.
    std::vector<grudge> grudges;

    /// The directed contact table (BL-908) — who has met whom, and what
    /// event joined the pair. Landmass identity is DERIVED from this by a
    /// consumer that walks it against `region::domain`; it is not stored
    /// here as a second table.
    std::vector<contact> contacts;

    /// THE DIRECTED WANT TABLE (BL-909) — a good `from` knows `to` holds and
    /// `from` lacks. Derived from `regions` and `contacts` by `derive_wants`,
    /// never carried as sim state of its own; see `want` for the shape and
    /// CIVILISATION.md sec The directed want for the design.
    std::vector<want> wants;

    /// Which provinces each polity holds, one entry per LIVING polity, sorted
    /// ascending by polity id. The political map as a set rather than as a
    /// field to be re-derived.
    std::vector<polity_holdings> holdings;

    /// The recorded time-lapse and the span it covers.
    era_timelapse timelapse;
    int64_t start_year = 0;
    int64_t stop_year  = 0;

    /// THE SURVIVING NETWORK CROSSES THE HANDOFF, UNEVENLY (BL-911).
    ///
    /// A subset of the sim's own `supply_corridors` record (BL-768) — never a
    /// second, tile-grain route; that seam stays exactly as BL-768 left it. A
    /// corridor survives here when AT LEAST ONE of its two endpoint regions is
    /// held, at `stop_year`, by a polity `alive` in `polities` above. Collapse
    /// is network failure (CIVILISATION.md § How an empire actually falls), so
    /// a segment held by a realm that did not collapse survives; one held only
    /// by ground that fell to nobody living does not. Sorted ascending by
    /// (a, b), same as the source, so the stamping pass stays order-independent.
    ///
    /// UNEVEN BY CONSTRUCTION: this is a filter over ownership, not a share —
    /// a large surviving realm keeps a large slice of what it built, a
    /// fragment keeps little, and a realm that fell keeps none. The spread
    /// this produces across `holdings` is the reading the contract is judged
    /// on (§ The network is the estate, and it crosses), never its total.
    std::vector<history_corridor> surviving_corridors;
};

/// Fold the live sim state and settlement state into the handoff value.
/// @param cs The culture table at the fold, copied into `cultures` and
///           sizing `culture_count`; null leaves both empty/0 ("unknown"),
///           which disables the share range check exactly as a 0 count did.
///           One source for the count and the table, so they cannot disagree.
pass_one_output make_pass_one_output(const settlement_state&  ss,
                                     const history_sim_state& hs,
                                     const creed_state*       cs);

/// The enforcement half of the struct above. Checks what the doc's clause
/// actually claims: every region's shares sum to exactly 1000 and name only
/// cultures in range; every holding names a living polity and an existing
/// region, with no region held twice; every grudge names polities in range and
/// carries at least one event; every contact names polities in range and
/// carries the event that joined it; the culture table's size equals
/// `culture_count`, every parent index is in range and below its child, every
/// coining year is at or before `stop_year` -- and, when @p live is given, the
/// table EQUALS the live `creed_state`'s row for row (BL-969), which is what
/// makes "the struct is the whole of what crosses" a checked claim rather
/// than a sentence. Writes the first failure into @p why.
bool pass_one_output_valid(const pass_one_output& o, std::string* why,
                           const creed_state* live = nullptr);

// ---------------------------------------------------------------------------
// The Exploration -> Digitisation handoff (BL-956)
// ---------------------------------------------------------------------------

/// THE WHOLE OF WHAT THE EXPLORATION SPAN HANDS FORWARD, AND NOTHING ELSE
/// (BL-956). EXPLORATION.md § What this phase hands digitisation names the
/// list and says "The list is a struct, and it has readers before
/// Digitisation exists" — this is that struct, on exactly the footing of
/// `pass_one_output` above: a VALUE (copies, never views of live sim state),
/// folded by `make_exploration_output` and held to its list by
/// `exploration_output_valid`.
///
/// ITS FIRST READER IS WORLD SETUP, not Digitisation: wherever the span ran,
/// sentiment is seeded from `grudges` and roads are stamped from
/// `surviving_corridors` here, so a campaign opening on the 1660 political
/// map does not open on 1200's resentments and 1200's roads.
///
/// WHERE EACH ITEM ON THE DOC'S LIST LIVES, so the mapping is explicit rather
/// than inferred:
///   - Treasuries                -> `region::treasury` on each polity's
///                                  capital seat, in `regions`
///   - Scarcity signals          -> `region::scarcity_q` on each market
///                                  region, in `regions`
///   - Trade flows               -> `trade_flows`
///   - Corridor throughput       -> `surviving_corridors::uses` (traffic, the
///                                  walks) and `surviving_corridors::tier`
///                                  (the road ladder rung, 0-3, the sim's
///                                  own live rung at the close). Read the
///                                  rung off `tier`, NEVER off `uses` against
///                                  `history_sim_params::road_tier{1,2,3}_uses`:
///                                  a bought post road sets the live count to
///                                  `road_tier3_uses` while adding one walk,
///                                  so the two legitimately differ. A resumed
///                                  span seeds its live counts from
///                                  `resume_corridors` (BL-949) -- from `uses`
///                                  by default, from `tier` under
///                                  `resume_seeds_corridor_tier` (BL-1037), the
///                                  only way a BOUGHT rung reopens as bought.
///   - Cultural good preference  -> `culture_preference`
///   - The overlord graph        -> `polity::overlord` / `polity::subject_kind`
///                                  in `polities`; tribute terms in
///                                  `dated_objects` (`treaty_clause::tribute`)
///   - Standing treaties and their remaining terms
///                               -> `dated_objects` (remaining years =
///                                  `expires_year - stop_year`)
///   - Ports, navies, standing armies
///                               -> `region::port_stock_q` / `region::army_stock`
///                                  in `regions`, `polity::navy_stock` in
///                                  `polities`
///   - The contact and want tables -> `contacts`, `wants`
///   - Exploration tree masks    -> `polity::exploration_mask` in `polities`
///   - The grudge table and the surviving network
///                               -> `grudges`, `surviving_corridors`
///   - (the political map, as a set) -> `holdings`
///   - (the culture table, BL-969) -> `cultures`, on the footing
///                                  `pass_one_output::cultures` sets
struct exploration_output
{
    /// The region table at the span's close — carries treasury, scarcity_q,
    /// port_stock_q, army_stock, culture shares and `nation` ownership.
    std::vector<region> regions;

    /// The polities at the span's close — carries navy_stock, overlord,
    /// subject_kind and exploration_mask.
    std::vector<polity> polities;

    /// How many cultures the shares and `culture_preference` index into.
    /// Always equals `cultures.size()` on a folded value; validated.
    int culture_count = 0;

    /// The culture table at the 1660 close (BL-969) -- a copy of
    /// `creed_state::cultures`, on exactly the footing and for exactly the
    /// reason `pass_one_output::cultures` gives: the span writes the table,
    /// so the close is a record, and the validator proves the copy equals the
    /// live table it is handed. Consumers keep reading `creed_state`.
    std::vector<culture> cultures;

    /// Treaty clauses (and tribute) STILL STANDING at `stop_year`: every
    /// object whose term ended at or before `stop_year` is expired out by
    /// `expire_dated_objects`, the same rule the sim's own rounds apply.
    /// Sorted ascending by (a, b, kind, expires_year).
    std::vector<dated_object> dated_objects;

    /// The directed contact table (BL-908), grown across the span.
    std::vector<contact> contacts;

    /// The directed want table (BL-909), re-derived over the 1660 state by
    /// `derive_wants`.
    std::vector<want> wants;

    /// Cultural good preference (BL-936), derived over the 1660 state by
    /// `derive_culture_preference`. Ascending (culture, good index).
    std::vector<culture_good_preference> culture_preference;

    /// The directed grudge table (BL-827), grown across the span.
    std::vector<grudge> grudges;

    /// Which provinces each polity holds at `stop_year`, one entry per polity
    /// holding ground, ascending polity id — same derivation as
    /// `pass_one_output::holdings`.
    std::vector<polity_holdings> holdings;

    /// The span's corridor record filtered over THIS span's dead, by exactly
    /// the rule `pass_one_output::surviving_corridors` applies: a corridor
    /// survives when at least one endpoint region is held, at `stop_year`, by
    /// a polity `alive` in `polities`. Sorted ascending by (a, b).
    std::vector<history_corridor> surviving_corridors;

    /// The span's final decision round's trade flows (BL-954) STILL STANDING
    /// at `stop_year`: kept only where the pair holds a trade_access clause
    /// among `dated_objects` above and both parties are alive in `polities`.
    /// Sorted ascending by (seller, buyer, good).
    std::vector<trade_flow> trade_flows;

    /// BL-1036 -- the civilisation and universal-creed records this span's
    /// run held at its close, in index order: the tables the carried
    /// `region::civilisation`, `region::universal_creed` and
    /// `polity::universal_creed` indices were assigned against during the
    /// span. A resumed span copies both (`history_sim_params::
    /// resume_civilisations` / `resume_universal_creeds`), so its next record
    /// takes the next free index. NOT RANGE-VALIDATED, and that is the known
    /// gap `resume_civilisations` names: the Exploration span itself resumes
    /// at 1200 without the Empires tables, so an index an Empires-era record
    /// was given can sit on a region here with no row to point at.
    std::vector<civilisation>    civilisations;
    std::vector<universal_creed> universal_creeds;

    int64_t start_year = 0;
    int64_t stop_year  = 0;
};

/// Fold the Exploration span's closing sim state and settlement state into
/// the handoff value. @param cs The culture table at the close, copied into
/// `cultures` and sizing `culture_count`; null leaves both empty/0.
exploration_output make_exploration_output(const settlement_state&  ss,
                                           const history_sim_state& hs,
                                           const creed_state*       cs);

/// The enforcement half of `exploration_output`: every table sorted, every id
/// in range, no self-pairs; every holding matches region ownership (and every
/// owned region is held); every surviving corridor has a living holder at one
/// end; every overlord id valid and never self, with `subject_kind` set iff an
/// overlord is; every standing dated object still inside its term; no
/// negative treasury, army or navy stock and every `port_stock_q` on 0-1000;
/// every trade flow sorted, between two distinct living polities, a known
/// good at positive volume, on a pair holding a standing trade_access clause;
/// the culture table sized to `culture_count`, parents in range and below
/// their child, coining years at or before `stop_year`, and -- when @p live
/// is given -- equal row for row to the live `creed_state` (BL-969).
/// Writes the first failure into @p why.
bool exploration_output_valid(const exploration_output& o, std::string* why,
                              const creed_state* live = nullptr);
