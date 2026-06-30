#pragma once

/// @file
/// Single source of truth for the habitability → workforce-efficiency curve
/// (BL-069). The economy system applies this multiplier to a body's effective
/// workforce, and the Population lens / Selection panel / hover read the *same*
/// helper so the surfaced legibility cannot drift from the simulation by a
/// duplicated constant.
///
/// Full labour at/above habitability 0.6; below that it ramps linearly down to
/// 0.5× at habitability 0 — the "efficiency cliff" the player reasons about when
/// siting. Pure and header-only: no stored state, no tick, no determinism impact.

/// Habitability → workforce-efficiency multiplier.
///
/// @param habitability Tile/body habitability in [0, 1].
/// @return             Labour multiplier in [0.5, 1.0]; 1.0 at/above 0.6.
inline float workforce_efficiency(float habitability)
{
    return habitability >= 0.6f ? 1.0f : 0.5f + (habitability / 0.6f) * 0.5f;
}
