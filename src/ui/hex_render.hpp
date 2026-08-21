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

// ---------------------------------------------------------------------------
// Terrain texture (BL-520)
// ---------------------------------------------------------------------------
// Two passes over one tile, deliberately split along the axis split BL-519 made
// available: a **substrate grain** (the ground's own material, drawn faint and
// small, so it survives the BL-511 province blend averaging across the province
// rather than fighting it) and a **cover pattern** (per tile, emphatic, scaled by
// `cover_density` — a forest edge IS information, a rock-to-rock seam is not).
//
// PROCEDURAL, not an atlas: this project ships no art assets, and a hatch/stipple
// drawn from `ImDrawList` primitives is the same hand-drawn vector idiom
// `docs/ui/ICONS.md` already establishes for the glyph vocabulary.
//
// Mark placement is hashed from the tile's GRID coordinates, never from screen
// position, so a tile's texture is identical every frame and across every wrap
// copy — panning does not make the ground crawl.

/// Alpha ramp for the texture pass by drawn hex circumradius, in screen px.
///
/// Its own threshold, ABOVE BL-269's coarse-fill bound (7 px) and above the
/// province-edge stroke that shares it. Derived: a cover mark is drawn at
/// `0.20 * r`, and a mark needs ~2 px of extent before it is a shape rather than
/// a stipple of aliasing — `2 / 0.20 = 10 px`, plus headroom for the several
/// marks a dense tile draws without them merging, gives a **14 px floor**. Below
/// that the pass emits nothing at all. It then ramps linearly to full strength at
/// **22 px** instead of popping in, because a texture that appears between one
/// zoom notch and the next reads as a rendering fault.
///
/// @return 0 at r <= 14, 0.5 at r = 18, 1 at r >= 22.
float texture_lod_scale(float draw_radius_px);

/// Draw the substrate grain and cover pattern for one hex, centred at @p centre
/// with drawn circumradius @p r.
///
/// @p fill is the colour the tile was actually filled with — lens tint, relief,
/// fog and all. Every mark is derived from it rather than from a fixed palette
/// entry, which is what lets texture survive an overlay: the mark moves with
/// whatever colour the tile ended up as, so it reads as depth in that colour
/// instead of as dirt over it.
///
/// @p strength scales the whole pass (LOD ramp x any lens attenuation). At 0 the
/// call returns immediately. Caller is responsible for skipping built and
/// survey-masked tiles — this draws terrain, and neither of those IS terrain.
void draw_tile_texture(ImDrawList* dl, ImVec2 centre, float r, int grid_x, int grid_y,
                       terrain_substrate sub, terrain_cover cov,
                       std::uint8_t density, ImU32 fill, float strength);

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
