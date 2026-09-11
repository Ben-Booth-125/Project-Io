#include "era_minus_one.hpp"

// ---------------------------------------------------------------------------
// The Era -1 invocation's parameter derivations (BL-462). See the header for
// why they live here rather than at `make_hard_coded_world`'s call site.
// ---------------------------------------------------------------------------

bool era_minus_one_enabled(const world_params& params)
{
    // THE EPOCH CLAUSE IS GONE (BL-747). It read
    // `params.epoch_year < 1700 && params.prehistory_years > 0`, on the
    // reasoning that above 1700 the settlement pass had already pre-computed
    // the history and the era had nothing left to simulate. The two-span
    // design says otherwise: the same engine plays an ancient span and then an
    // industrial one, so a 1960 arc runs the sim too — the epoch decides only
    // whether there IS a second span, which is
    // `era_minus_one_has_industrial_span`'s question, not this one's.
    //
    // `prehistory_years == 0` is a SCOPE KNOB, not a tuning dial: it skips the
    // pass outright, which is how the harnesses that do not test the era avoid
    // paying its cost (world_params::prehistory_years).
    return params.prehistory_years > 0;
}

bool era_minus_one_has_industrial_span(const world_params& params)
{
    // The same 1700 the gate used to turn on, now asking a different question.
    //
    // `industrial_years > 0` is the second clause for the same reason
    // `prehistory_years > 0` is one on the gate above: the field is a SCOPE
    // KNOB, and its own doc-comment promises that zero means no industrial
    // span. Without this clause a zero would instead put the boundary AT the
    // epoch, which leaves the ancient span running the whole way with the
    // medieval ceiling on — the opposite of what the knob says it does.
    return params.epoch_year >= 1700 && params.industrial_years > 0;
}

history_sim_params era_minus_one_sim_params(const world_params& params)
{
    history_sim_params hp;

    // The stop year is the epoch on BOTH shapes; only the start and the
    // interior differ.
    const bool two_span = era_minus_one_has_industrial_span(params);
    hp.stop_year = params.epoch_year;

    // SETTLE IS RE-SETTLEMENT IN GENERATION'S OWN ROUND (BL-894; Ben,
    // 2026-09-11). This is the Empires round -- the land is already settled
    // when it opens, and the migration that filled it ran before this sim
    // starts (`run_settlement`, plus BL-846's founding schedule for anything
    // dated after the start year). A verb that manufactures fresh ground on
    // population pressure alone belongs to that earlier diffusion, not here.
    //
    // Set on the DERIVED params rather than on the struct default, so every
    // harness measuring the old 4000 BCE -> 0 CE arc -- which spans the
    // migration era and therefore still needs pressure-driven settling --
    // keeps the rule it was written against.
    hp.settle_requires_razed_ground = true;

    // BL-893: the water gate's weight escape, on for the same reason -- this is
    // the round the rule was designed for.
    hp.amphibious_weight_crossing = true;

    if (two_span)
    {
        // 1160 -> 1560 -> 1960 at the defaults. The ancient span is
        // `prehistory_years` long and capped at the medieval band; the
        // industrial span is `industrial_years` long with the ladder
        // unrestricted. Both bands tick at 4 years, as the single-span run
        // always has — the clock is not what changes between the spans, the
        // roster ceiling is.
        hp.boundary_year      = params.epoch_year - params.industrial_years;
        hp.start_year         = hp.boundary_year - params.prehistory_years;
        hp.span1_band_ceiling = roster_band::medieval;
        hp.tick_bands[0]      = {hp.boundary_year, 4};
        hp.tick_bands[1]      = {params.epoch_year, 4};
        hp.tick_band_count    = 2;
    }
    else
    {
        // EXACTLY what this function did before BL-747, byte for byte, and
        // that is the point: `boundary_year` and `span1_band_ceiling` are left
        // at their struct defaults (INT64_MIN and `industrial`), so the
        // ceiling is inert twice over — no year is before the boundary, AND
        // the clamp is the identity. An ancient epoch therefore executes the
        // same values through the same code as it did, which is what keeps the
        // 0 CE arc's `state_hash` byte-identical across this change. Adding a
        // candidate to the scorer would move the argmax even where it never
        // wins; adding an inert clamp cannot.
        hp.start_year      = params.epoch_year - params.prehistory_years;
        hp.tick_bands[0]   = {params.epoch_year, 4};
        hp.tick_band_count = 1;
    }

    // NO supply-decay override here, deliberately.
    //
    // An earlier cut of this block derived supply_decay_per_tile_q from the map
    // width (2000/gw), because the authored 28 gave a maximum reach of 36 tiles
    // and produced ZERO battles on a 312-wide map. That was a workaround for
    // measuring supply from the CAPITAL. The sim now supplies a campaign from
    // the staging holding next to the target (history_sim.cpp, `hub_dist`), so
    // reach is bounded by neighbour_radius rather than by empire shape and does
    // not need rescaling when the map does. The authored constant stands.
    return hp;
}

uint32_t era_minus_one_sim_seed(const world_params& params)
{
    // `era_seed` lets the SAME ground be played through twice and come out
    // differently — the wizard's history round rerolls it and nothing else, so
    // the planetology rounds above it are untouched (world_params::era_seed).
    //
    // ADDED, NOT XORed, and multiplied by an odd constant first. XOR would let
    // an era_seed that happened to equal a low bit pattern of the fold collapse
    // two different rolls onto one history; the odd multiplier scatters
    // consecutive roll counters (1, 2, 3...) across the whole word, which is
    // what a reroll counter actually produces.
    //
    // AT era_seed 0 THIS IS THE ORIGINAL EXPRESSION, digit for digit, so every
    // existing world, golden and fixture is unmoved.
    return (params.seed ^ 0x415C1E17u) + params.era_seed * 0x9E3779B9u;
}
