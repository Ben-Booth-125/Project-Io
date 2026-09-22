#pragma once

#include "world/components.hpp" // terrain_substrate / terrain_cover / terrain_landform, cover_fraction

#include <cstdint>

// ---------------------------------------------------------------------------
// Terrain palette — the PURE half of the terrain colour source of truth.
//
// Extracted from hex_render.cpp (BL-732) so the ground bake (ground_bake.cpp)
// and the headless bake harness can read the same palette without pulling in
// ImGui. hex_render's ImU32 functions are thin delegates onto these; the maths
// here is byte-for-byte the maths that lived there, because the visual goldens
// compare pixels and the extraction must not move a single channel.
//
// Colour encoding: the same 32-bit ABGR layout ImGui's default IM_COL32 packs
// (R in the low byte, A in the high byte), so hex_render can cast the values
// straight across. Nothing in this header includes imgui.h — that is the point.
// ---------------------------------------------------------------------------

namespace ui::palette {

using colour32 = std::uint32_t;

constexpr int k_r_shift = 0;
constexpr int k_g_shift = 8;
constexpr int k_b_shift = 16;
constexpr int k_a_shift = 24;

constexpr colour32 col32(int r, int g, int b, int a = 255)
{
    return (static_cast<colour32>(r & 0xFF) << k_r_shift)
         | (static_cast<colour32>(g & 0xFF) << k_g_shift)
         | (static_cast<colour32>(b & 0xFF) << k_b_shift)
         | (static_cast<colour32>(a & 0xFF) << k_a_shift);
}

constexpr int col_r(colour32 c) { return (c >> k_r_shift) & 0xFF; }
constexpr int col_g(colour32 c) { return (c >> k_g_shift) & 0xFF; }
constexpr int col_b(colour32 c) { return (c >> k_b_shift) & 0xFF; }
constexpr int col_a(colour32 c) { return (c >> k_a_shift) & 0xFF; }

/// Channel-truncating blend — hex_render's `blend`, moved. Alpha keeps a's.
colour32 blend(colour32 a, colour32 b, float t);

/// Channel-rounding blend — hex_render's `blend_round`, moved. The four
/// calibrated cover endpoints land a channel short under truncation, which is
/// why this variant exists (BL-519); every other path stays on `blend`.
colour32 blend_round(colour32 a, colour32 b, float t);

/// The substrate's own colour, no cover blended in.
colour32 substrate_colour(terrain_substrate sub);

/// The colour a cover tends toward at FULL density (endpoints, not samples).
colour32 cover_endpoint(terrain_cover c);

/// Identity fill colour for a tile's terrain — substrate blended toward the
/// cover endpoint by the tile's density. The single colour source of truth;
/// ui::terrain_colour delegates here.
colour32 tile_colour(terrain_substrate sub, terrain_cover cov, std::uint8_t density);

/// Signed relief ordinal, plains = 0 (mountain highest, canyon lowest).
float relief_amount(terrain_landform lf);

/// Composite the landform relief shade over @p base — warm highlight up, cool
/// shadow down. ui::landform_relief delegates here.
colour32 landform_relief(colour32 base, terrain_landform lf);

} // namespace ui::palette
