#include "era_minus_one.hpp"

// ---------------------------------------------------------------------------
// The Era -1 invocation's parameter derivations (BL-462). See the header for
// why they live here rather than at `make_hard_coded_world`'s call site.
// ---------------------------------------------------------------------------

bool era_minus_one_enabled(const world_params& params)
{
    // An antiquity start generates the world BEFORE the 0-1960 story happens,
    // so the year-tick sim is what produces the run-up. Above 1700 the
    // settlement pass has already pre-computed that history and the era has
    // nothing left to simulate.
    //
    // `prehistory_years == 0` is a SCOPE KNOB, not a tuning dial: it skips the
    // pass outright, which is how the harnesses that do not test the era avoid
    // paying its cost (world_params::prehistory_years).
    return params.epoch_year < 1700 && params.prehistory_years > 0;
}

history_sim_params era_minus_one_sim_params(const world_params& params)
{
    history_sim_params hp;
    hp.start_year      = params.epoch_year - params.prehistory_years;
    hp.stop_year       = params.epoch_year;
    hp.tick_bands[0]   = {params.epoch_year, 4};
    hp.tick_band_count = 1;

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
    return params.seed ^ 0x415C1E17u;
}
