#pragma once

#include "world/planetology.hpp"

#include <cstddef>

// ---------------------------------------------------------------------------
// Generation preview pane — the wizard's "what we see" half
//
// The New World wizard splits into a control column (stage folds, leans,
// reroll) and this pane: a stylised painting of the world the current roll
// produced, drawn straight from planetology_state. Nothing here is generated —
// it is a pure reading of the same preview the charts plot, so the picture
// and the plots cannot disagree.
//
// Per round the pane shows what the round settles:
//   round 0 — the system: star colour/size, orbits, body sizes.
//   round 1 — the homeworld surface: ocean, atmosphere, ice, life tint.
//   round 2 — the industrial history: the same surface, worked over.
//
// Deliberately stylised, not a tile map: the wizard resolves preferences, and
// a painting that moves when a lean moves is the feedback loop wanted here.
// The real surface appears when "Begin" builds it.
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

/// The homeworld's REAL surface, when the app has one: raster-order
/// terrain_composition values (cast to uint8_t), home_grid_width × height.
/// When `comp` is non-null the globe samples THIS — the actual map "Begin"
/// will hand over — instead of the stylised continent blobs; the blobs remain
/// only as the fallback while the first async build is still in flight.
struct preview_surface_view
{
    int            gw   = 0;
    int            gh   = 0;
    const uint8_t* comp = nullptr;
};

/// Paint the round's preview into the current window's remaining content
/// region. Pure draw-list painting; no widgets, no state.
///
/// @param rot Homeworld rotation in radians — the sampled longitude offset.
///            The caller owns the clock (wall-time in play, 0 under --verify,
///            so golden captures never race an animation).
/// @param surface The real homeworld raster, if built. See preview_surface_view.
void draw_generation_preview(const preview_body* bodies, std::size_t count,
                             const planetology_params& params, int round,
                             float rot = 0.0f,
                             const preview_surface_view& surface = {});

} // namespace ui
