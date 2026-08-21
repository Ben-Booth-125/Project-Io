#pragma once

#include "world/entity.hpp"
#include "world/components.hpp" // terrain_substrate / terrain_cover / terrain_landform

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

/// Identity fill colour for a tile's terrain — the single source of truth for
/// surface tinting. Composition owns **hue**; landform is a separate channel
/// (`landform_relief` below, plus the `ui::icons::landform` glyphs), because lens
/// tints composite over this hue and would obliterate any second signal carried in it.
ImU32 terrain_colour(terrain_substrate sub, terrain_cover cov, std::uint8_t density);

/// The substrate's own colour, with no cover blended in. Exposed because the
/// lenses want to tint the GROUND (which the province blend already averages)
/// while the cover reads per-tile — the resolution BL-520's texturing turns on.
ImU32 substrate_colour(terrain_substrate sub);

/// Composite a **relief** shade for @p lf over @p base — the landform channel's
/// half that covers the common ground (BL-231).
///
/// Landform drives movement cost (×1.0–×2.0 via landform_logistics_cost), hazard,
/// habitability and mineral richness,
/// but never reached the screen: `terrain_colour` keys on substrate and cover alone. Measurement
/// (world_audit § S3) settled the shape of the fix — the mix is ~95 % plains + valley with
/// every dramatic landform ≤ 1.5 %, so there is no continuous gradient worth contouring.
/// Elevated ground therefore lifts toward a warm sunlit highlight and sunken ground sinks
/// toward a cool shadow, on a small signed ordinal scale (mountain highest, canyon lowest,
/// plains untouched at zero).
///
/// The amounts are deliberately **subtle**: this must read as light falling on terrain,
/// never as a change of terrain (the hue is the identity channel and stays intact).
/// Callers composite this **after** any lens tint, so the relief survives a saturated
/// overlay rather than being buried under it.
ImU32 landform_relief(ImU32 base, terrain_landform lf);

/// Pick an ink that reads against @p bg — dark on a light background, light on a dark
/// one, by perceived luminance. The terrain palette spans near-white ice to dark forest
/// and any lens may composite over it, so a fixed glyph colour would vanish somewhere;
/// this keeps the landform glyphs legible across the whole range.
ImU32 contrast_ink(ImU32 bg);

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
