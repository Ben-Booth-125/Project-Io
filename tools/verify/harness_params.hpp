// ---------------------------------------------------------------------------
// Shared harness world_params helper
// ---------------------------------------------------------------------------
// Wiring the Era -1 year-tick sim into generation (2026-08-12) added ~23 s to
// EVERY world any caller builds, and most harnesses build two or more. Harnesses
// that audit tiles, roads, corporations, economy arithmetic or determinism do
// not test the pre-epoch era at all, and were paying its whole cost — several
// past their ctest timeouts, which is what stopped the gate being trustworthy.
//
// `no_prehistory()` says, in one greppable word, "this harness does not test the
// era". It is a SCOPE declaration, not a speed hack: the harnesses that DO test
// the era (era_world_harness, history_sim_harness, history_sweep,
// stepped_clock_harness) must not use it.
//
// Determinism is untouched — prehistory_years is part of world_params, so the
// same params still produce the same world.
#pragma once

#include "world/hard_coded_world.hpp"

/// The caller's params with the pre-epoch year-tick sim switched off.
inline world_params no_prehistory(world_params p = {})
{
    p.prehistory_years = 0;
    return p;
}
