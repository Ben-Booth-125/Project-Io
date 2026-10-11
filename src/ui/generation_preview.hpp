#pragma once

#include "world/planetology.hpp"

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// Generation preview pane — the System round's drawing of the system
//
// The New World wizard splits into a control column (stage folds, leans,
// reroll) and a pane. On round 1, System, the pane is this: a stylised drawing
// of the system the current roll produced — star colour and size, orbits, body
// sizes — read straight from planetology_state, so the picture and the charts
// cannot disagree. Nothing here is generated.
//
// Every later round draws the homeworld as the 2D MAP instead (STARTUP.md
// § The Life map; the globe that used to stand here is retired, Ben,
// 2026-10-10): the Life round's map is `ui::draw_lapse_ground`, the pass
// rounds' is `ui::draw_lapse_map`, and they are one map by construction.
// ---------------------------------------------------------------------------

namespace ui {

/// One body as the preview pane needs it — the authored orbit facts the chart
/// source does not carry, plus the chain's output.
struct preview_body
{
    const char*              name            = "";
    float                    orbit_au        = 1.0f;
    float                    mass_earths     = 1.0f;
    float                    parent_orbit_au = 0.0f; ///< >0: a moon; drawn beside its primary.
    bool                     is_home         = false;
    const planetology_state* state           = nullptr;
};

// The wizard's packed homeworld raster: one byte per tile, `substrate << 4 |
// cover` (BL-519). The maps read the substrate alone, for the land mask
// (`history_lapse.cpp`); the format is kept so the raster stays one byte a tile.

/// Pack a tile's two terrain axes into the raster's one byte.
inline uint8_t preview_pack(terrain_substrate sub, terrain_cover cov)
{
    return static_cast<uint8_t>((static_cast<int>(sub) << 4) | static_cast<int>(cov));
}

inline terrain_substrate preview_substrate(uint8_t packed)
{
    return static_cast<terrain_substrate>((packed >> 4) & 0x0F);
}

/// Paint the System round's drawing of the system into the current window's
/// remaining content region. Pure draw-list painting; no widgets, no state.
void draw_system_preview(const preview_body* bodies, std::size_t count,
                         const planetology_params& params);

} // namespace ui
