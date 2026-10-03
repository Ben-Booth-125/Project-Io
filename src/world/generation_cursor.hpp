#pragma once

// ---------------------------------------------------------------------------
// THE GENERATION CURSOR (BL-1084; docs/ui/STARTUP.md § The world cache)
// ---------------------------------------------------------------------------
//
// `make_hard_coded_world` is the composition of six resumable stage functions
// over ONE cursor, and this header is that cursor. A stage reads and writes
// only the cursor and its sinks, so a caller may stop after any stage, hold the cursor, and
// resume it later -- or copy it and resume the copy -- and the world that comes
// out is the world the single call builds. The single call is literally
// `begin_generation` + `run_generation_to(tail)`; it is not a second path.
//
// THE STAGES, ONE PER WIZARD ROUND BOUNDARY (generation_stage, hard_coded_world.hpp):
//
//   life_gate          the star, the system, the homeworld's tiles WITH THEIR
//                      DEPOSITS, its rivers (progress bumps 0-5). The Life
//                      round's world. Helios, Cinder and Kepler exist here;
//                      Selene and Pallas do not (they are built in the tail).
//   culture            the ladder, the creeds, the migration (bumps 6-7).
//   empires            the Empires span, 400 BCE -> 1200 (bump 8).
//   exploration        the exploration age, 1200 -> 1660 (re-captions bump 8).
//   industrialisation  1660 -> 1960 (re-captions bump 8).
//   tail               the history's close (the capital shells, population
//                      centres and their names), then borders, roads,
//                      companies and finishing (bumps 9-12).
//
// A span that does not run (the era switched off, Exploration opted out, the
// Industrialisation switch off) still ADVANCES the cursor past its stage: the
// stage ran and found nothing to do, exactly as the single call's gates skip it.
//
// WHY THE HISTORY'S CLOSE IS THE TAIL'S FIRST ACT, NOT THE LAST SPAN'S. The
// close -- the BL-910 capital market shells, the population centres, the nation
// seeding and the city names -- runs once, after whichever span ran LAST. Were
// it the end of the Empires stage, a cursor held at the Empires boundary would
// already carry 1200's cities, and resuming it into Exploration would build a
// different world from the single call. So every span boundary is PRE-close,
// and the close is where the world stops being resumable.
//
// COPYING A CURSOR IS A FAITHFUL COPY OF EVERY VALUE IT HOLDS. The world copies
// in iteration order (BL-1034, faithful_unordered_map.hpp); every other member
// is a value type with no unordered store and no pointer into itself or the
// world -- the span resume pointers are set inside the stage that reads them,
// from the cursor's own handoff structs, never stored. The FOUR I/O BINDINGS
// below are non-owning pointers and a copy SHARES them: a caller resuming a
// copy rebinds `report` (to a copy of the report), `fixture` and `progress` to
// its own, or two cursors will write into one record. `works` is a read-only
// table and may be shared. tools/verify/world_cursor_equivalence.cpp is the
// proof: every stage on a copy of its predecessor's cursor, compared with the
// single call at every boundary on the sixteen curated seeds.
//
// THE CLOCKS ARE REPORTED ONLY. The `clocks` block (BL-754, BL-1072) is the
// per-step wall clock and the loading bar's step bookkeeping. No branch, seed,
// hash or stored field reads it, so a copied cursor's clocks are harmless.

#include "body_names.hpp"
#include "creeds.hpp"
#include "era_timelapse.hpp"
#include "grudge_sentiment.hpp"
#include "hard_coded_world.hpp"
#include "history_ladder.hpp"
#include "history_sim.hpp"
#include "nation_generation.hpp"
#include "planetology.hpp"
#include "settlement.hpp"
#include "sim_terrain_build.hpp"
#include "world.hpp"
#include "world_gen_config.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

struct generation_cursor
{
    // --- What the build was asked for (values) --------------------------------
    world_params     params{};
    world_gen_config gen_cfg{};

    // --- Where the build reports: NON-OWNING, SHARED BY A COPY ---------------
    // Rebind `report`, `progress` and `fixture` on a copy that is resumed (see
    // the header). Null is always legal and costs nothing, as on the single call.
    generation_report*     report   = nullptr;
    generation_progress*   progress = nullptr;
    const works_registry*  works    = nullptr; ///< Read-only; a copy may share it.
    era_minus_one_fixture* fixture  = nullptr;

    /// The last stage run. `run_generation_to` advances it one stage at a time.
    generation_stage reached = generation_stage::none;

    // --- The world and what the later stages read -----------------------------
    world w;

    // life_gate
    float             deposit_scalar = 1.0f; ///< The abundance tier's multiplier.
    body_naming       naming;                ///< The system's catalogue (BL-257).
    resolved_world    rw;                    ///< The preferences, resolved.
    entity_id         kepler = null_entity;  ///< The homeworld.
    planetology_state kepler_pl;
    std::vector<entity_id> kepler_tiles;     ///< Raster order.

    // culture
    history_ladder_state kepler_hist;
    creed_state          kepler_creeds;
    settlement_state     kepler_settlement;
    nation_params        kepler_np;
    /// The Empires sim's opening year, INT64_MAX where the era will not run.
    int64_t              sim_start              = INT64_MAX;
    /// The Culture round's displayed close (BL-947).
    int64_t              culture_round_end_year = 0;
    /// The migration's own record (BL-1104), folded where a report holds it.
    era_timelapse        migration_lapse;
    /// The Culture stage folded `migration_lapse` (a report was bound). A
    /// build stopped there with no report folds it on demand at its ending,
    /// from the same settlement and creeds -- so no stage reads a stop flag.
    bool                 migration_folded = false;

    // empires / exploration / industrialisation: the records the tail reads,
    // each the LAST span's close, replaced span by span (BL-956, BL-1053).
    // They were locals of the single call "hoisted" out of the history block
    // so the passes after it could read them; the cursor is that hoist.

    /// BL-768 — THE ANCIENT ROAD RECORD. Its two consumers (the road stamp, and
    /// the market carve's trade-concentration term) run in the tail, long after
    /// the spans. The sim's own `history_sim_state` stays local to its stage:
    /// what crosses out of it is the record, not the run.
    std::vector<history_corridor> kepler_corridors;

    /// BL-1098 — THE SEA-LEG RECORD, carried on the corridors' terms: its
    /// consumer (`stamp_sea_lanes`) runs in the tail beside the road stamp.
    /// The last span's close -- Industrialisation's 1960 table when it ran,
    /// else Exploration's 1660 one; empty when neither ran.
    std::vector<sea_leg> kepler_sea_legs;

    /// BL-898 — THE GRUDGE RECORD, carried for the same reason and on the same
    /// terms as the corridors: its consumer (`seed_grudge_sentiment`) runs
    /// after the political map exists. WHY IT IS CARRIED AT ALL: before BL-898
    /// the record crossed the pass 1 -> pass 2 handoff and died there --
    /// `grudge_between` had one caller in the tree (the handoff harness) and
    /// generation never read `pass_one_output::grudges`. Carried is not
    /// consequential.
    std::vector<grudge> kepler_grudges;
    /// The sim's own `grudge_cap`, carried beside the record so the conversion
    /// is a fraction of the scale that actually produced these scores rather
    /// than of a struct default that might have moved.
    int32_t kepler_grudge_cap = grudge_sentiment_params{}.score_full;

    /// BL-975: indexed by POLITY id, the treasury each polity held at the LAST
    /// span's close — `region::treasury` summed over the regions flying its
    /// flag (`polity_treasuries_at_close`): Exploration's 1660 close, or the
    /// Industrialisation span's 1960 one when that span ran (BL-1053). Empty when
    /// neither ran, so a world without them credits nothing and every nation
    /// starts on the floor. The corridor and grudge records above follow the
    /// same rule: each is the last close's, replaced span by span.
    std::vector<int64_t> kepler_polity_treasuries;

    /// BL-1089: indexed by POLITY id, the realm names as the LAST span's close
    /// left them (`polity_names_at_close`), replaced span by span on the same
    /// rule as the chests above, so Pass 5 inherits the name the wizard's last
    /// board printed. Empty when no span ran.
    std::vector<std::string> kepler_polity_names;

    /// BL-1159: indexed by POLITY id, each polity's own industrial crossing year
    /// (`polity::industrial_year`, `k_never_industrialised` for one that never
    /// crossed) at the LAST span's close (`polity_industrial_years_at_close`),
    /// replaced span by span on the same rule as the names above. Read once, by
    /// `derive_national_character`'s qualification axis (POPULATION.md
    /// § Qualification, "Seeded from history"). Empty when no span ran.
    std::vector<int64_t> kepler_polity_industrial_years;

    /// The era ran (the Empires stage's gate): the Exploration span is nested in it.
    bool               era_ran = false;
    /// The sim's terrain view, built once at the Empires stage and read by all three spans.
    sim_terrain_arrays terrain;
    /// The 1200 handoff (BL-911) -- the Exploration span's resume struct.
    pass_one_output    pass_one;
    /// Exploration ran: the Industrialisation span is nested in it.
    bool               exploration_ran = false;
    /// The 1660 handoff (BL-956) -- the Industrialisation span's resume struct.
    exploration_output exploration;
    /// BL-1152 -- the Industrialisation span's fleet ledger at 1960 (the 1660
    /// one rides in `exploration.fleet`). Empty when the span did not run.
    fleet_ledger       fleet_1960;

    // tail (the history's close)
    /// The close has run (the tail's first act, or a stopped build's ending).
    /// It runs once; no span runs after it (see `close_stopped_generation`).
    bool                   history_closed = false;
    /// Indexed by region: which POLITY held it at the epoch. Snapshotted before
    /// `derive_national_character` overwrites `region::nation` with the nation
    /// index — the same field, read at the one moment it still names a polity.
    std::vector<int>       kepler_region_polity;
    /// BL-910 capital markets, recorded as they are placed so the pricing pass
    /// after the carve can price them (Ben, 2026-09-23).
    std::vector<entity_id> capital_market_shells;
    /// Parallel to it (BL-1138 review): each shell's REGION ANCHOR tile. A shell on
    /// a water anchor stands on land, but the gravity fold's port gate still binds
    /// it to its own region through this tile.
    std::vector<entity_id> capital_shell_anchor;

    // --- Reported only (BL-754, BL-1072) --------------------------------------
    struct clock_state
    {
        using clock = std::chrono::steady_clock;
        clock::time_point world_begin{};
        clock::time_point settlement_begin{};
        clock::time_point settlement_end{};
        clock::time_point era_end{};
        std::array<int64_t, generation_stage_label_count> step_ms{};
        int               step_label = -1;
        clock::time_point step_begin{};
        /// Bumps made so far, counted whether or not a sink is bound -- the
        /// running count `generation_progress::stage` is published from.
        int               gen_stage = 0;
        /// The sink the last plan was published to (`plan_generation_progress`).
        /// `run_generation_to` plans for a bound sink that differs from it --
        /// a fresh sink, or one rebound on a resumed copy -- and leaves a sink
        /// already planned for alone. Compared, never dereferenced.
        const generation_progress* plan_sink = nullptr;
    } clocks;
};

/// A cursor at `generation_stage::none`, bound to its inputs and sinks. Nothing
/// runs. The arguments are `make_hard_coded_world`'s, with its defaults.
generation_cursor begin_generation(const world_params& params = {},
                                   generation_report* report = nullptr,
                                   const world_gen_config& gen_cfg = {},
                                   generation_progress* progress = nullptr,
                                   const works_registry* works = nullptr,
                                   era_minus_one_fixture* fixture = nullptr);

/// Run every stage after `c.reached` up to and including @p target, in order.
/// A target at or before `c.reached` runs nothing. Stages never run twice.
void run_generation_to(generation_cursor& c, generation_stage target);

/// One call per stage: each runs any earlier stage the cursor has not reached,
/// then its own, and is a no-op on a cursor already past it.
void gen_life_gate(generation_cursor& c);
void gen_culture(generation_cursor& c);
void gen_empires(generation_cursor& c);
void gen_exploration(generation_cursor& c);
void gen_industrialisation(generation_cursor& c);
void gen_tail(generation_cursor& c);

/// WHAT A `stop_after_*` BUILD DOES AT ITS STOP, and nothing else: the ending
/// `make_hard_coded_world` gives a build stopped at `c.reached`.
///   * culture: the migration round's one tap publish, and its record onto the
///     report (the counters, the settlement, `prehistory_timelapse`).
///   * empires / exploration / industrialisation: the history's close (the
///     tail's first act), then the settlement onto the report.
///   * any other stage: nothing.
/// NO LATER SPAN RUNS AFTER THIS on a span stage: the close has built the last
/// span's cities, and a later span would run under them, so a later span stage
/// on a closed cursor advances without running and says so on stderr. The tail
/// may still follow (it does not close twice), and after an industrialisation
/// stop that is exactly the single call's sequence. It is the stop flags'
/// legacy contract ("the world is not usable"), kept byte for byte; a caller
/// that means to resume (the wizard's slots) publishes its round's record
/// without it: `stopped_report`.
void close_stopped_generation(generation_cursor& c);

/// THE REPORT A BUILD STOPPED AT `c.reached` WOULD HAND BACK, as a copy, with
/// the cursor, its world and its sinks untouched (BL-1084). The same report
/// writes `close_stopped_generation` makes (one function stamps both), so the
/// wizard reads each round's record off exactly the report the stop flags
/// always produced -- and the cursor stays resumable. The close is not needed
/// for the report: it reads the settlement and never writes it.
generation_report stopped_report(const generation_cursor& c);

/// Publish to `c.progress` the plan for the stages after `c.reached` up to and
/// including @p target: the weighted total (plus the sink's `weight_after`),
/// the running stage count at the end, and the caption of the first step, with
/// nothing done yet (BL-1084). `run_generation_to` calls it itself for a sink
/// it has not planned for, so each run -- the single call's whole build, one
/// wizard round's stage -- plans exactly the stages it runs, off the params
/// and what has run, never off a stop flag. Call it first only to plan WIDER
/// than one call (round 6's span, tail and finish). A null sink publishes
/// nothing. Reported only: nothing in generation reads it back.
void plan_generation_progress(generation_cursor& c, generation_stage target);
