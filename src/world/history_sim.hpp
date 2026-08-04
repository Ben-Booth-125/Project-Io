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
// later provinces unfounded, with the comment "the year-tick sim founds them".
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
// loop feeds province ownership, which every later pass reads.
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
// reassigns `province::nation` and nothing else. That keeps the per-year
// ownership ring small enough to be the History Log time-lapse substrate
// (~82 provinces x 2000 years x 2 bytes), which a tile-granular border model
// would not be.

#include "combat.hpp"
#include "creeds.hpp"
#include "settlement.hpp"

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
/// them. The sim's whole decision path is province-level, so binding it to the
/// ECS would buy nothing and cost headless testability. Both pointers may be
/// null, in which case every battle is fought on the neutral default terrain —
/// legal, and what the harness's synthetic cases use.
struct sim_terrain_view
{
    const std::vector<terrain_composition>* composition = nullptr;
    const std::vector<terrain_landform>*    landform    = nullptr;
};

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------

/// Scorer weights and loop bounds. Defaults are PLACEHOLDERS, not tuned
/// values: BL-277 records that the `w_*` weights are BL-275 sweep outputs
/// rather than authored constants. They are set here to plausible magnitudes
/// so the loop runs and the harness can bind assertions to its behaviour.
struct history_sim_params
{
    int64_t start_year = 0;    ///< Calendar year the run begins (the antiquity stop).
    int64_t stop_year  = 1960; ///< Calendar year the run ends (the campaign epoch).

    // --- Objective selection (BL-277 Q1) ----------------------------------
    int w_farm = 300; ///< Weight on a target province's farm endowment.
    int w_ore  = 250; ///< Weight on its ore endowment.
    int w_port = 200; ///< Weight on its port endowment.
    int w_ring = 400; ///< Weight on ENCLOSED-SEA RING CLOSURE — see `ring_closure_q`.
    int w_dist = 120; ///< Penalty per tile of supply distance from the capital.
    int w_def  = 500; ///< Penalty on the defender's fielded power.
    int w_cult = 150; ///< Penalty for taking ground of a foreign culture.

    // --- Force commitment (BL-277 Q2) -------------------------------------
    /// Power lost per tile of distance between capital and objective, in
    /// per-mille of full supply. THE STALL MECHANISM: reach far enough and the
    /// arriving force is below the defender's, so the frontier stops on
    /// arithmetic rather than an explicit cap (BL-224 non-hegemony, emergent).
    int supply_decay_per_tile_q = 28;

    /// Fraction of a province's banked manpower a campaign may raise.
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
    int campaign_threshold_q  = 220; ///< Minimum score for a campaign to be launched.
    int settle_threshold_q    = 380; ///< Minimum score for founding a new province.
    int invest_threshold_q    = 160; ///< Minimum score for a capacity investment.

    /// Decisiveness (0-1000) a victory must reach before territory changes
    /// hands. Below it the battle is a raid: losses and contest, no transfer.
    int transfer_decisiveness_q = 300;

    /// Adjacency radius in tiles — two provinces closer than this are
    /// neighbours, and only neighbours are campaign candidates.
    int neighbour_radius = 9;

    /// Population pressure (population * 1000 / carrying capacity) above which
    /// a polity will consider founding a new province.
    int settle_pressure_q = 780;
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
/// nations — `run_settlement` leaves `province::nation` at -1 until the
/// political pass runs, and a pre-national ladder (BL-221) is exactly a world
/// whose actors are peoples. The sim writes `province::nation` as it goes, so
/// by the stop year the political map is this loop's output.
struct polity
{
    int id      = -1; ///< Index into `history_sim_state::polities`.
    int culture = -1; ///< Index into `creed_state::cultures`.
    int capital = -1; ///< Province index; the origin supply decays from.

    /// Doctrine lean, 0-1000, from the culture's `aggression_q` (BL-277 Q5).
    int aggression_q = 0;

    /// Capacity band 1-6 per domain, indexed by `sim_domain`. Everyone starts
    /// at band 1 (classical) — the roster grouping in ANCIENT_TECH_LADDER.md
    /// § Shape puts a 0 CE start in the classical band alone.
    int capacity[sim_domain_count] = {1, 1, 1, 1, 1, 1, 1};

    /// Accumulated investment per domain; crossing a threshold raises the band.
    int progress_q[sim_domain_count] = {0, 0, 0, 0, 0, 0, 0};

    bool alive = true; ///< False once the polity holds no provinces.
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
};

// ---------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------

/// One ownership change: province `province` came under polity `owner` in
/// year `year`. `owner_none` records a province leaving all ownership.
///
/// This is the whole time-lapse format. It is a change LIST rather than a
/// per-year grid because ownership is overwhelmingly static — most years, on
/// most provinces, nothing happens, and a dense grid pays for all of it.
struct owner_change
{
    int32_t  year     = 0;
    uint16_t province = 0;
    uint16_t owner    = 0;
};

/// The run's output. `owner_changes` is the time-lapse substrate; everything
/// else is either the political result or a counter the harness binds to.
struct history_sim_state
{
    std::vector<polity> polities;

    /// The History Log time-lapse substrate (Ben, 2026-08-04), DELTA-ENCODED:
    /// one record per ownership CHANGE, not per province per year.
    ///
    /// A dense year-major grid was the obvious first cut and it is the wrong
    /// one: the Settle verb grows the province count during a run (82 -> ~480
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
    int      province_stride = 0; ///< Final province count (slice width for replay).
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
    int64_t stalled_campaigns = 0; ///< Scored but never launched: supply decay bit.
};

/// Sentinel for "no polity owns this province in this year slice".
inline constexpr uint16_t owner_none = 0xFFFFu;

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

/// Run the year-tick history sim over @p ss, mutating it in place: province
/// ownership, population, manpower and culture all move, and new provinces are
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
history_sim_state run_history_sim(settlement_state&         ss,
                                  const creed_state*        cs,
                                  const sim_terrain_view&   terrain,
                                  int                       gw,
                                  int                       gh,
                                  const history_sim_params& params,
                                  uint32_t                  seed);

/// Tile distance between two provinces on the cylinder — column difference
/// wraps, row difference does not. Exposed because the harness asserts the
/// supply-decay stall against it, and a test that recomputed distance its own
/// way would be testing its own arithmetic rather than the sim's.
int province_distance(const province& a, const province& b, int gw);

/// Bytes the time-lapse substrate occupies — the quantity the requirement
/// bounds, and the reason the encoding is a change list rather than a grid.
inline int64_t owner_ring_bytes(const history_sim_state& s)
{
    return static_cast<int64_t>(s.owner_changes.size()) * static_cast<int64_t>(sizeof(owner_change));
}

/// Materialise the ownership map as it stood at the END of @p year — the
/// time-lapse read. Returns `province_stride` entries, `owner_none` where the
/// province did not exist yet or was unowned.
///
/// Linear in the change list, which is the point: a whole 2000-year playback
/// is one forward walk, and a seek to a single year costs no more than the
/// changes preceding it.
std::vector<uint16_t> owner_slice_at(const history_sim_state& s, int64_t year);
