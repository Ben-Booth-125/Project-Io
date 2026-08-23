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
/// The real gate has a third clause — `!settlement.regions.empty()` — which is
/// not a question about params, so it stays at the call site. Ask
/// `era_minus_one_fixture::ran` for the answer that includes it.
bool era_minus_one_enabled(const world_params& params);

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
};
