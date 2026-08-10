#pragma once

// ---------------------------------------------------------------------------
// Canonical odd-r hex neighbour offsets (BL-363)
// ---------------------------------------------------------------------------
// The single tabulation of the odd-r offset-grid neighbour deltas (pointy-top
// hexes) shared by every consumer — placement, river tracing, and the canvas
// renderers. Side ordering: 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE. This table was
// previously pasted per file; one divergent digit is a silent geometry bug,
// so it lives here once. Lives world-side because world/ cannot include ui/.
//
// No wrap or clamp here — callers own the east-west cylinder wrap and the
// top/bottom row bounds, which differ per consumer.

namespace hex_neighbors {

inline constexpr int even_off[6][2] =
    {{+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1}};
inline constexpr int odd_off[6][2] =
    {{+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1}};

/// The offset row for a tile at grid row @p gy.
inline constexpr auto& offsets(int gy) { return (gy & 1) ? odd_off : even_off; }

/// Unwrapped grid coordinate of neighbour @p side (0..5) of (@p gx, @p gy).
struct coord { int gx; int gy; };
inline constexpr coord neighbour(int gx, int gy, int side)
{
    return { gx + offsets(gy)[side][0], gy + offsets(gy)[side][1] };
}

} // namespace hex_neighbors
