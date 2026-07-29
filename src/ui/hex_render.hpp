#pragma once

#include "world/entity.hpp"
#include "world/components.hpp" // terrain_composition

#include <imgui.h>

// ---------------------------------------------------------------------------
// Shared hex-surface rendering primitives.
//
// The terrain palette and hex geometry the Planetary canvas draws with, lifted
// out of body_surface_canvas.cpp so a second surface — the Selection band's
// zoomed tile-neighbourhood view (BL-194) — renders identical hexes rather than
// a second palette that drifts from the canvas. The canvas includes this header
// and no longer defines its own copies.
// ---------------------------------------------------------------------------

struct world;

namespace ui {

/// Identity fill colour for a tile's composition — the single source of truth for
/// surface tinting (landform is conveyed by glyphs, not hue).
ImU32 terrain_colour(terrain_composition t);

/// Fills `out[6]` with the screen-space vertices of a pointy-top hexagon centred
/// at (cx, cy) with circumradius r.
void hex_vertices(ImVec2 out[6], float cx, float cy, float r);

/// World-space centre of a hex at (col, row) in odd-r offset coordinates, relative
/// to the grid top-left, at the given circumradius.
ImVec2 hex_local_centre(int col, int row, float hex_size);

/// Render a zoomed hex **neighbourhood** around @p centre_tile into [@p origin,
/// @p origin+@p size], clipped to that rect: terrain-filled hexes for the tile and
/// its neighbours out to @p radius rings, a marker on built tiles, and a highlight
/// ring on the centre tile. The view is centred on @p centre_tile and scaled so
/// the (2·radius+1)-wide block fills the box — the card's "show the tile you
/// clicked, zoomed" surface. Columns wrap horizontally (the grid wraps); rows clamp.
///
/// Takes a mutable world only to build the per-body raster index on first use
/// (body_tile_grid — a deterministic derived cache); it does not mutate simulation
/// state. A no-op when @p centre_tile is not a live tile.
///
/// @param dl          Draw list to render into (clip is pushed/popped internally).
/// @param w           World (mutable for the lazy grid-index cache only).
/// @param centre_tile The tile to centre the view on and highlight.
/// @param origin      Top-left of the render rect, screen px.
/// @param size        Width/height of the render rect, screen px.
/// @param radius      Rings of neighbours to show around the centre tile.
void draw_tile_neighbourhood(ImDrawList* dl, world& w, entity_id centre_tile,
                             ImVec2 origin, ImVec2 size, int radius);

} // namespace ui
