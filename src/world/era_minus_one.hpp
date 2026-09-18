#pragma once

// ---------------------------------------------------------------------------
// era_minus_one — ONE construction of the Era -1 invocation (BL-462).
// ---------------------------------------------------------------------------
//
// THE DEFECT THIS FILE EXISTS TO CLOSE. `history_sim_params` defaults to a
// 4000 BCE -> 0 CE span across a six-band clock (136 decision rounds).
// Generation overrides all of it — 400 BCE -> 0 CE on ONE four-year band, 100
// rounds — and it did so at a call site no other caller could see. So every
// harness took the struct default, and BL-462's finding is the consequence:
// **no check in the project measured the run that actually generates a world.**
// It is the third recorded instance of this repo's "a check that looks like
// coverage and is not" failure mode, and the largest, because it hit every
// Era -1 check at once.
//
// The span and the clock were only two of six divergences. The others are
// quieter and each one is enough on its own to make the measurement describe a
// different world:
//
//   3. THE SEED. Generation folds `params.seed ^ 0x415C1E17u`; a harness that
//      passes the bare world seed simulates a different era of the same map.
//   4. THE CREEDS. `run_history_sim` reads `cs->cultures[c].aggression_q` to
//      set every polity's aggression. A null creed pointer flattens all of
//      them to a neutral 500 — which is to say it deletes the one input that
//      makes polities differ in how readily they go to war, in a harness whose
//      subject is why they do not.
//   5. THE WORKS TABLE. `history_sim.cpp` gates `build_work` on
//      `works != nullptr && works->size() > 0`, so a null registry runs a
//      FOUR-verb contest where generation runs a five-verb one.
//   6. THE SETTLEMENT. `run_history_sim` mutates its `settlement_state&` in
//      place, and `generation_report::body_entry::settlement` is the state
//      AFTER the sim (and after `derive_national_character`). Re-running the
//      sim on it starts the era from its own ending.
//
// WHY A FIXTURE RATHER THAN A SECOND DERIVATION. The obvious repair is to copy
// generation's settings into each harness. That is the defect again, deferred:
// two places constructing the same invocation independently is exactly how the
// first four divergences arrived. So the parameter derivations live here and
// are CALLED by generation, and everything generation cannot re-derive — the
// pre-sim settlement, the creeds, the terrain, the works pointer — is CAPTURED
// at generation's own call site into `era_minus_one_fixture`. A harness then
// re-runs the era by handing those values straight back, constructing nothing
// of its own, so there is no second construction left to drift.
//
// Capture is opt-in and costs nothing when unrequested: `make_hard_coded_world`
// fills the fixture only when it is handed one.
//
// A SEVENTH CALLER EXISTED AND NOBODY COUNTED IT (NR-732/NR-733, 2026-08-30).
// This file's scope was HARNESSES; the History ledger's Ages view re-ran the era
// too, and diverged on all six axes above at once. It is gone — generation now
// RECORDS the ownership history (`generation_report::body_entry::
// prehistory_timelapse`) and the view replays that, so there is no second
// invocation left to drift. The lesson is about scope rather than that view: a
// file built to stop a class of defect stopped it for the callers its author had
// in mind, and nothing made the omission visible. There is still no check that
// says "every caller of run_history_sim derives its params here".

#include "creeds.hpp"
#include "hard_coded_world.hpp"
#include "history_sim.hpp"
#include "settlement.hpp"
#include "sim_terrain_build.hpp"
#include "world.hpp"

#include <cstdint>

class works_registry;

/// Does generation run the Era -1 sim for these params, as far as the params
/// alone can say?
///
/// THE EPOCH NO LONGER DECIDES THIS (BL-747). It used to: an `epoch_year`
/// at or above 1700 skipped the pass outright, on the reasoning that the
/// settlement pass had already pre-computed that history and the era had
/// nothing left to simulate. The two-span design replaces that — the epoch
/// now decides only whether the run has a SECOND span, not whether it runs at
/// all, so a 1960 arc plays the same engine across an ancient span and then
/// an industrial one (docs/lore/HISTORY.md § The epoch and the run).
///
/// What remains is the SCOPE KNOB: `prehistory_years == 0` skips the pass, and
/// that is how the harnesses that do not test the era avoid paying its cost
/// (world_params::prehistory_years). Not a tuning dial.
///
/// The real gate has a third clause — `!settlement.regions.empty()` — which is
/// not a question about params, so it stays at the call site. Ask
/// `era_minus_one_fixture::ran` for the answer that includes it.
bool era_minus_one_enabled(const world_params& params);

/// Does this epoch carry an INDUSTRIAL span? Above 1700 the sim plays the
/// run-up to an industrial start, so the boundary sits `industrial_years`
/// before the epoch and the higher bands unlock there. At an ancient epoch
/// there is no second span and the derivation below leaves every new field
/// at its inert default.
bool era_minus_one_has_industrial_span(const world_params& params);

/// The `history_sim_params` generation runs the era on.
///
/// 400 years at 4 years a tick is Ben's figure (2026-08-12), re-affirmed on
/// 2026-08-18 as ruling 8 of NR-331: the docs were wrong and the code was
/// right, keep 400. NR-334's open half — whether the HARNESS follows generation
/// down or generation follows the struct default up — is answered by this
/// function existing: generation is unchanged and the harnesses follow it.
/// Every field this does not touch keeps its `history_sim_params` default,
/// which is deliberate: the scorer weights and thresholds are shared, and only
/// the span and the clock were ever overridden.
history_sim_params era_minus_one_sim_params(const world_params& params);

/// The seed generation hands the era. A per-pass fold off the master seed, in
/// the same shape as every other pass in `make_hard_coded_world`.
uint32_t era_minus_one_sim_seed(const world_params& params);

// ---------------------------------------------------------------------------
// BL-931 — the Exploration span's own derivations, on the same footing as the
// three above. A SECOND CALL to `run_history_sim`, over the same engine, with
// `history_sim_params::resume_polities`/`resume_grudges`/`resume_contacts`/
// `resume_corridors` set by the caller from the closing `pass_one_output` —
// this file derives the SPAN and the SEED only, exactly as it does for the
// Empires round; the resume pointers are per-call state this file has no
// business holding.
// ---------------------------------------------------------------------------

/// Does generation run the Exploration span for these params? Requires the
/// Empires round itself to be enabled and single-span
/// (`!era_minus_one_has_industrial_span`) — see `world_params::
/// exploration_sim_enabled`'s comment for why a two-span epoch is out of
/// scope here — plus the opt-in flag itself.
bool exploration_sim_enabled(const world_params& params);

/// The `history_sim_params` the Exploration span runs on: `start_year =
/// params.empires_stop_year` (1200 by default, wherever the Empires round
/// actually closed), `stop_year = params.exploration_stop_year` (1660 by
/// default). A single tick band at 4 years, the same cadence the Empires
/// round's own single-span closes on. `exploration_upkeep_enabled` is set —
/// this is the one caller that wants the (still-empty) upkeep hook running.
/// The resume pointers are NOT set here; the caller fills them in from its
/// own `pass_one_output` immediately before calling `run_history_sim`.
history_sim_params exploration_sim_params(const world_params& params);

/// The seed generation hands the Exploration span. Folded off the same
/// master seed with its own constant, so it is neither the Empires round's
/// seed nor a caller-invented one.
uint32_t exploration_sim_seed(const world_params& params);

// ---------------------------------------------------------------------------
// BL-1040 — the Digitisation span's own derivations, on the same footing as
// Exploration's: this file derives the SPAN and the SEED; the resume pointers
// (every table of the closing `exploration_output`) are per-call state the
// caller sets immediately before `run_history_sim`.
//
// THERE IS NO `digitisation_span_enabled(params)` PREDICATE HERE, AND THAT IS
// THE POINT. The span runs if and only if Exploration ran (Ben, 2026-09-18),
// so the call site nests it inside the block that ran Exploration and gates it
// on `world_params::digitisation_span_enabled` alone. A params-only predicate
// would be a second reading of Exploration's own gate -- one more place for
// the two to drift, and one more place an epoch test could creep back in.
// ---------------------------------------------------------------------------

/// The `history_sim_params` the Digitisation span runs on. STARTS FROM
/// `exploration_sim_params` (struct defaults plus Exploration's overrides),
/// NEVER the Empires derivation -- copying the wrong base silently changes the
/// verb set (supply upgrades, universal creeds, army upkeep). Then only:
///   - the span: `start_year = params.exploration_stop_year` (1660, wherever
///     Exploration closed), `stop_year = params.digitisation_stop_year`
///     (1960), one band at Exploration's 4-year cadence (NR-888): 75 rounds;
///   - the Industry tree ON from the span's own open (BL-1038, TREES.md sec
///     Milestones), and on in this span only.
/// Both 1200 anchors (`consolidation_year`, `near_home_cutoff_year`) are
/// Exploration's, unchanged: consolidation happens once and a pair met after
/// 1200 stays far however late a span opens (Ben, 2026-09-18). BL-1037's
/// `resume_seeds_corridor_tier` keeps its default (off); BL-1044 turns it on
/// with the re-bless.
history_sim_params digitisation_sim_params(const world_params& params);

/// The seed generation hands the Digitisation span: its own constant, own
/// additive fold, so polity temperaments re-roll at 1660 as they did at 1200
/// (DIGITISATION.md, PROPOSED 2026-09-18, not overturned).
uint32_t digitisation_sim_seed(const world_params& params);

/// EXACTLY what generation handed `run_history_sim`, captured at its own call
/// site — the arguments, and the three counts the run produced.
///
/// THE ACCEPTANCE TEST THIS TYPE EXISTS FOR: a harness that re-runs the sim
/// from these arguments must reproduce `battles` / `conquests` / `foundings`
/// bit-for-bit. If it does, the harness is measuring generation's own era. If
/// it does not, it is measuring something else and no other number it prints
/// means anything — which is the whole of BL-462 stated as a check.
///
/// Not part of `generation_report`, and deliberately so. The report is
/// serialised in full by `src/core/save_game.cpp`, so a field here would be a
/// save-format change; and what a harness needs is the sim's INPUT, which the
/// report cannot hold — its `settlement` is the state after the sim has already
/// mutated it. This is a generation-time diagnostic capture with no save-seam
/// presence at all.
struct era_minus_one_fixture
{
    /// True when generation actually ran the era — all three gate clauses.
    /// False leaves every field below at its default, and a harness that
    /// reports a number off an unran fixture is reporting a default.
    bool ran = false;

    entity_id body = null_entity; ///< The body the era was simulated on.
    int       gw   = 0;           ///< Grid width the sim ran at.
    int       gh   = 0;           ///< Grid height the sim ran at.

    /// The settlement as the sim RECEIVED it, before it mutated it in place.
    /// Nothing else records this: the report's copy is post-sim.
    settlement_state settlement;

    /// The creeds as passed. Generation always passes a real one; the sim reads
    /// per-culture `aggression_q` off it, and nothing else.
    creed_state creeds;

    /// The terrain arrays as passed. Captured rather than re-derived because a
    /// re-derivation is a second construction, which is the defect this file
    /// closes — even though `build_sim_terrain` happens to be stable across the
    /// passes that run after the era.
    sim_terrain_arrays terrain;

    history_sim_params params;      ///< The span and clock the era ran on.
    uint32_t           seed = 0;    ///< The folded seed the era ran on.

    /// The works table as passed — the POINTER, not a copy, so a re-run cannot
    /// diverge from generation whatever the caller supplied. Null when the
    /// caller supplied none, in which case `build_work` was not in the contest
    /// and the run scored four verbs rather than five.
    const works_registry* works = nullptr;

    /// What generation's own run produced. The other half of the acceptance
    /// test: these are the values that reach
    /// `generation_report::prehistory_battles` and its two siblings.
    int64_t battles   = 0;
    int64_t conquests = 0;
    int64_t foundings = 0;
    int64_t years     = 0;

    // --- The generation budget (BL-754) -----------------------------------
    //
    // WHY THE TIMINGS LIVE HERE AND NOT ON `generation_report`. The report is
    // serialised in full by src/core/save_game.cpp, so a field on it is a
    // SAVE-FORMAT change — and a wall clock is the worst possible thing to put
    // through a save: it differs every run on the same machine and differs
    // again on another, so a saved world would carry a value no two loads
    // agree on. This fixture has no save-seam presence at all (see the type
    // comment above), which makes it the only surface in the Era -1 path where
    // a measurement can sit without becoming world state.
    //
    // NOTHING BELOW MAY EVER ENTER A DIGEST OR HASH. These are milliseconds of
    // wall clock; folding one into `state_hash` would make generation
    // non-deterministic by construction, which is the standing rule this whole
    // layer is built around. They are REPORTED, never asserted and never
    // compared — a budget is read by a human, not by a check.
    //
    // Zero when the caller asked for no fixture, and zero for any pass that
    // did not run.

    /// Wall clock of `make_hard_coded_world` end to end, in milliseconds.
    int64_t ms_world_total = 0;

    /// Wall clock of the Era -1 year-tick sim alone — the pass BL-754 exists
    /// to price.
    ///
    /// DO NOT QUOTE A FIGURE HERE WITHOUT ITS BUILD TYPE. An earlier draft of
    /// this comment said "~6.4 s of an ~11.2 s single-span build", which was a
    /// misreading of a harness line (6.45 s was the whole world build, and no
    /// build ever produced 11.2 s). Measured 2026-09-03: the Release harness
    /// path (`build_lua_harness.bat` -> `build_gen/verify/`) prices the 400-year
    /// ancient span at ~323 ms of an ~8.2 s world, while a Debug `build/` prices
    /// the same pass at ~26 s of ~73 s. The pass's SHARE of the build differs by
    /// roughly 9x between them, so any ratio taken from this field is a fact
    /// about one build type and must say which.
    int64_t ms_era = 0;

    /// Wall clock of the settlement pass that precedes the era, and of
    /// everything before it (planetology, continents, tiles, provinces). The
    /// three plus `ms_era` account for `ms_world_total` up to the passes that
    /// run after the era (nations, corporations, roads), which fall into
    /// `ms_after_era`.
    int64_t ms_before_settlement = 0;
    int64_t ms_settlement        = 0;
    int64_t ms_after_era         = 0;

    // --- BL-937: the Exploration span's own capture -----------------------
    //
    // SAME DISCIPLINE AS THE EMPIRES CAPTURE ABOVE: a harness needs the span's
    // real input and output, and re-deriving either here would be the second
    // construction this file exists to prevent. Populated only when
    // `exploration_sim_enabled(params)` held AND the Empires round actually
    // produced a living polity to hand it (hard_coded_world.cpp's own gate) —
    // `exploration_ran` says which; every field below is the struct default
    // otherwise.

    /// True when generation actually ran the Exploration span this call.
    bool exploration_ran = false;

    history_sim_params exploration_params; ///< The span/clock the Exploration round ran on.
    uint32_t           exploration_seed = 0;

    /// The directed contact table AS THE SPAN OPENED — `pass_one_output::
    /// contacts` at 1200 CE, i.e. before a single Exploration-round campaign
    /// ran. This is the "already met by 1200" baseline BL-937's displacement
    /// reading classifies a battle's pair against: a pair present here is a
    /// LONG-CONTACTED NEIGHBOUR; a pair absent here but present in
    /// `exploration_state.contacts` met for the first time DURING this span,
    /// i.e. is a NEWLY-CONTACTED, frontier pair.
    std::vector<contact> pre_exploration_contacts;

    /// The Exploration span's own full output, EXACTLY as generation's own
    /// (untraced — `exploration_params.trace_battles` is false in the real
    /// run) call produced it — polities, the closing contact table,
    /// battles/conquests/foundings, all of it. The ground-truth counts a
    /// harness's own re-run (below) must reproduce bit for bit.
    history_sim_state exploration_state;

    // --- The Exploration span's PRE-SIM inputs, for a harness re-run -------
    //
    // WHY A SECOND CAPTURE RATHER THAN JUST TURNING ON `trace_battles` ABOVE.
    // The Empires capture above never sets `trace_battles` on the run
    // generation itself performs either — the field's own comment states a
    // traced and an untraced run are byte-identical in every other output,
    // but that guarantee is exactly why tracing belongs in a harness's own
    // second, disposable run rather than in the one the player's world is
    // built from: it costs memory (one `battle_trace` per battle) that a
    // shipped generation pass has no reason to carry. So a harness wanting
    // BL-937's per-battle attacker/defender pairs re-invokes `run_history_sim`
    // from these captured PRE-Exploration inputs with `trace_battles` forced
    // on, on the same "capture, do not re-derive" footing as the Empires
    // block: everything below is what generation itself handed the span,
    // copied before the call rather than reconstructed independently.
    settlement_state      pre_exploration_settlement; ///< `kepler_settlement` as the span opened.
    creed_state           pre_exploration_creeds;     ///< `kepler_creeds` as the span opened.
    std::vector<polity>   pre_exploration_polities;   ///< `pass_one_output::polities` at 1200.
    std::vector<grudge>   pre_exploration_grudges;    ///< `pass_one_output::grudges` at 1200.
    std::vector<history_corridor> pre_exploration_corridors; ///< `pass_one_output::surviving_corridors` at 1200.

    // --- BL-956: the Exploration handoff, and what world setup consumed ----

    /// The Exploration -> Digitisation handoff value exactly as generation
    /// folded it (`make_exploration_output`), default-constructed when
    /// `exploration_ran` is false.
    exploration_output exploration_handoff;

    /// The grudge table world setup actually handed `seed_grudge_sentiment`,
    /// and the corridor set it actually handed `stamp_history_roads`,
    /// captured at those two consumption sites (populated whenever a fixture
    /// was asked for, whichever span supplied them). A harness binds these
    /// against `exploration_handoff` to prove the 1660 values were the ones
    /// read, rather than the 1200 ones.
    std::vector<grudge>           setup_grudges;
    std::vector<history_corridor> setup_corridors;

    // --- BL-1040: the Digitisation span's own capture ---------------------
    //
    // Same discipline as the Exploration capture above. Populated only when
    // generation actually ran the span -- `world_params::
    // digitisation_span_enabled` set, Exploration run, and no stop knob that
    // ends generation before it; `digitisation_ran` says which, and every
    // field below is the struct default otherwise. The span's INPUT is
    // `exploration_handoff` above, the value the span resumed from, PLUS the
    // span-open survey (BL-1051), which is why the region table it opened on
    // is captured below rather than re-derived.

    /// True when generation actually ran the Digitisation span this call.
    bool digitisation_ran = false;

    /// BL-1051 — the region table the span actually OPENED ON: the 1660
    /// handoff's regions with `survey_regions_at_span_open` applied, captured
    /// between the survey and the call. It differs from
    /// `exploration_handoff.regions` in `survey_fuel_q` and `survey_forest_q`
    /// and in nothing else, and a harness resuming the span must open on it
    /// (the survey reads tiles, which a fixture does not carry).
    std::vector<region> digitisation_open_regions;

    history_sim_params digitisation_params; ///< The span/clock the span ran on.
    uint32_t           digitisation_seed = 0;

    /// The 1960 close exactly as generation folded it
    /// (`make_digitisation_output`), default-constructed when the span did
    /// not run.
    digitisation_output digitisation_handoff;

    /// The span's own full sim output, as generation's untraced call produced
    /// it -- its counters (battles, subjections formed and freed) count THIS
    /// span only, because a resumed run starts them at zero.
    history_sim_state digitisation_state;

    /// Decision rounds the span ran (`history_sim_profile::decision_rounds`,
    /// read straight after the call): 75 at the defaults, 1660 -> 1956.
    int64_t digitisation_rounds = 0;

    /// Wall clock of the span's `run_history_sim` call alone, in
    /// milliseconds. REPORTED, NEVER ASSERTED, and never folded into a digest
    /// or a branch -- the same rule the BL-754 timings above obey, for the
    /// same reason. Say which build type produced a figure when quoting it.
    int64_t ms_digitisation = 0;
};
