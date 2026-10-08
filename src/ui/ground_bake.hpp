#pragma once

#include "structure_stamps.hpp" // BL-1241: the installation pass and its stamp seam
#include "world/entity.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Ground bake (BL-732) — the PURE half of the baked-chunk ground renderer.
//
// Composes the Planetary canvas's painterly ground (docs/ui/RENDERING.md) into
// RGBA8 pixel buffers, CPU-side: continuous base colour interpolated between
// tile centres (no cell boundary drawn), hillshade from the BL-517 height
// field, wrap-periodic grain noise, water shading, and the C-F near-future
// grade as a separable final pass. Dramatic landforms bake their own relief
// forms and rivers bake as carved curved courses (BL-1242).
//
// Deliberately SDL/ImGui/Lua-free so the headless harness
// (tools/verify/ground_bake_check.cpp) can compile it against the world layer
// alone. The SDL texture cache that consumes these buffers is
// src/core/ground_layer.{hpp,cpp}. The bake READS world state and never
// writes it: the world/* determinism rule is untouched by construction.
//
// Coordinate model — "canonical space": hex circumradius = 1, so a pointy-top
// grid has col_step = sqrt(3) and row_step = 1.5, with hex (c, r) centred at
// (sqrt(3) * (c + 0.5 * (r & 1)), 1.5 * r) — hex_local_centre at hex_size 1.
// x wraps at period = gw * sqrt(3) (the cylinder). A bake pixel p maps to
// canonical ((p.x + 0.5) / s, (p.y + 0.5) / s + y_min). One geometry describes
// one whole-body image; chunks are pixel windows into it, so adjacent chunks
// are seamless by construction.
// ---------------------------------------------------------------------------

struct world;
class recipe_registry;

namespace ui::ground {

/// Pixel geometry of one whole-body bake target.
///
/// TILT (BL-737, the stepped 2.5D): a tilted geometry bakes the ground for an
/// axonometric camera that will squash y by `tilt_sy` at draw time. The bake
/// stays in GROUND coordinates — the canvas's vertex squash is the camera —
/// but pre-compensates what a quad transform cannot do: the height field
/// displaces content upward by `lift` canonical units per unit height (hills
/// grow silhouettes), and upright features (tree trunks, canopies) bake with
/// their vertical extent stretched by 1/tilt_sy so they stand correctly once
/// squashed. Flat geometry has tilt_sy = 1, lift = 0 and behaves exactly as
/// before.
struct geometry
{
    int    gw = 0, gh = 0;   ///< Grid dimensions.
    double s  = 1.0;         ///< Pixels per canonical unit (the baked hex circumradius, px).
    int    W  = 0, H = 0;    ///< Whole-image pixel size. W spans exactly one wrap period.
    double y_min = 0.0;      ///< Canonical y of pixel row 0 (top margin above row 0's hexes).
    double tilt_sy = 1.0;    ///< cos(tilt) the camera will apply; 1 = flat.
    double lift    = 0.0;    ///< Upward displacement per unit height, canonical units.
    /// Supersample factor this geometry is being baked at (BL-1244): s is the
    /// ACTUAL pixels per hex, s / ss the NOMINAL tier resolution the result
    /// will be downsampled to. Every resolution-keyed character choice (the
    /// close-tier octave, tree stamps, the post passes) keys on the nominal
    /// figure, so supersampling anti-aliases a tier without changing what it
    /// draws. Callers always pass ss = 1; bake_region derives the 2x geometry.
    double ss      = 1.0;
};

/// The NOMINAL bake resolution, px per hex circumradius (s / ss).
inline double nominal_s(const geometry& g) { return g.s / g.ss; }

/// Derive the bake geometry for a body grid at roughly @p target_px_per_r
/// baked pixels per hex circumradius. W is rounded to a whole pixel count and
/// s re-derived from it, so wrap copies of the image abut exactly.
/// @p tilt_sy < 1 makes an oblique geometry (see struct comment): lift is
/// derived as 0.9 * tan(tilt) and the top margin grows to hold displaced
/// peaks and standing trees.
geometry make_geometry(int gw, int gh, double target_px_per_r, double tilt_sy = 1.0);

/// Tuning constants for the bake, C-F defaults (painterly relief + near-future
/// grade). One struct so the look is dialled in one place; the grade sub-block
/// is a separable pass per the round-2 finding (grade over any biome).
struct bake_params
{
    // Spatial character. The domain warp displaces every sample point by a
    // low-frequency noise field before the owning tile is resolved, which is
    // what turns a hex-stepped class boundary (coastline, biome edge) into a
    // natural one — the single dial that most removes "rendered from hexes".
    float blend_radius    = 1.45f;  ///< Interpolation radius, canonical units (> 1).
    float warp_amp        = 0.45f;  ///< Domain-warp displacement, canonical units.
    float warp_cell       = 1.90f;  ///< Domain-warp noise cell size.
    // Painterly relief. The hillshade runs on the interpolated tile height PLUS
    // a fractal detail field whose amplitude grows with the landform bias — a
    // plains stays calm, a range reads craggy.
    float relief_gain     = 9.0f;   ///< Hillshade strength on the combined gradient.
    float altitude_gain   = 0.30f;  ///< Luminance lift with normalised height.
    float landform_accent = 2.2f;   ///< Detail amplitude multiplier from |relief bias|.
    float detail_amp      = 0.30f;  ///< Fractal height-detail base amplitude.
    float detail_cell     = 0.46f;  ///< Fractal detail cell size, canonical units.
    float jitter          = 0.02f;  ///< Per-tile luminance jitter. Small ON PURPOSE — this is the hex-mosaic dial.
    float noise_strength  = 0.05f;  ///< Fine grain amplitude.
    // Close-tier feature stamps (the "individual trees" ruling, Ben
    // 2026-09-01: the closest rungs must render individual trees and sharper
    // hills). Active at bake resolutions >= 40 px/r, i.e. the 48/96 tiers.
    float tree_density    = 1.0f;   ///< Global multiplier on per-tile tree counts.
    float ridge_strength  = 0.75f;  ///< How far mountain detail mixes toward ridged noise.
    float rock_exposure   = 0.5f;   ///< Slope-driven rock colour on steep ground.
    // Edge passes (BL-736, the sharpness ruling: "still a general blur" — the
    // eye reads sharpness from edges, and the interpolated field has none).
    // All post passes run on an internal apron so they cannot seam at a chunk
    // edge, and none touches the lock fill or the transparent margin.
    float edge_ink        = 0.30f;  ///< Darkening where two cover classes meet (>= 20 px/r).
    float shore_ink       = 0.42f;  ///< Darkening on the land|water boundary (stronger).
    float unsharp_amount  = 0.50f;  ///< Unsharp-mask strength at >= 40 px/r, single-sample bake.
    float unsharp_amount_ss = 0.35f; ///< The same pass on a SUPERSAMPLED bake: run once, at nominal
                                    ///< resolution AFTER the downsample, re-tuned against the
                                    ///< anti-aliased result (BL-1244) rather than stacked on it.
    // Supersampling (BL-1244, RENDERING.md § Level of detail): every tier
    // bakes at this multiple of its nominal px per hex and is box-downsampled
    // before upload, so stamp edges, creases and banks are anti-aliased in the
    // bake. 1 = the pre-BL-1244 single-sample bake, byte for byte.
    int   supersample     = 2;
    // Water.
    float water_noise     = 0.03f;
    // Landform relief and carved rivers (BL-1242, RENDERING.md § Mountains,
    // rivers and terrain variety). Each pass reads analytic forms built from
    // the tile skeleton — a dramatic landform's centre plus its half of every
    // shared edge to a same-landform neighbour, a river's quadratic B-spline
    // through its chain — so a run is ONE form and a course is ONE curve.
    // 0 disables a pass (the harness's A/B lever).
    float landform_strength = 1.0f; ///< Landform relief pass (massif, cut, bowl, fissure).
    float river_strength    = 1.0f; ///< River pass (course, bank shelving, wet margin).
    // Terrain variant families (BL-1243, RENDERING.md § Mountains, rivers and
    // terrain variety): every family (a substrate x cover pair, and each
    // dramatic landform's form) carries k_variant_count procedural variants,
    // each tile picking one by its grid coordinates, cross-faded across the
    // tile boundary. 0 = every tile bakes the family's neutral look (the A/B
    // lever: every family at its base look, the cross-fade gather unwidened).
    float variant_strength  = 1.0f;
    // Near-future grade (the separable pass).
    bool  grade_enabled   = true;
    float grade_desat     = 0.34f;  ///< Toward luma.
    float grade_cool[3]   = { 0.93f, 0.97f, 1.05f }; ///< Channel multipliers (r,g,b).
    float grade_lift      = 0.05f;  ///< Haze floor: lift toward the cool haze colour.
    float grade_contrast  = 1.06f;  ///< Mild S-curve about mid-grey.
    // Installations (BL-1241, structures baked): the structure pass and the
    // stamp seam it asks. A null sheet = every key draws its procedural form.
    bool  installations   = true;
    const stamp_sheet* stamps = nullptr;
};

/// Per-tile source fields for one body, extracted once per bake batch so the
/// per-pixel loop touches flat arrays only. Raster order (row * gw + col);
/// absent tiles (never generated) read as class `void_` and bake transparent.
struct bake_source
{
    int gw = 0, gh = 0;
    enum class tile_class : std::uint8_t { void_ = 0, land, water, masked };
    std::vector<std::uint8_t> cls;      ///< tile_class per tile.
    std::vector<std::uint32_t> colour;  ///< palette::tile_colour, ABGR.
    std::vector<float> height;          ///< BL-517 normalised height.
    std::vector<float> grad_x, grad_y;  ///< Height gradient (neighbour differences).
    std::vector<float> relief_bias;     ///< palette::relief_amount, landform accent input.
    std::vector<float> jitter;          ///< Per-tile hash jitter in [-1, 1].
    std::vector<std::uint8_t> cover;    ///< terrain_cover per tile — feeds the close-tier feature stamps.
    std::vector<std::uint8_t> density;  ///< cover_density per tile.
    installation_snapshot     inst;     ///< BL-1241: what stands on each revealed tile.
    // Landform and river features (BL-1242). Masked and void tiles carry none,
    // so no form or course is ever baked from unsurveyed ground.
    std::vector<std::uint8_t> landform;   ///< terrain_landform per tile.
    std::vector<std::uint8_t> lf_links;   ///< Hex sides (bit i = side i) whose neighbour shares this tile's DRAMATIC landform — the bridged run.
    std::vector<std::uint8_t> river_in;   ///< Hex sides a river flows IN across (river_edges & ~river_downstream).
    std::vector<std::uint8_t> river_out;  ///< Hex sides a river flows OUT across (river_edges & river_downstream).
    std::vector<float> river_flow;        ///< Accumulated flow: river tiles draining through this one, itself included (the whole river graph, mask-blind — width is what the visible course shows).
    std::vector<std::uint8_t> near_feature; ///< Bit 0: a river on this tile or a neighbour; bit 1: a dramatic landform likewise. The passes' cull.
    // Terrain variant families (BL-1243). The variant index is a pure function
    // of the grid (a hash-preferred colouring in which no two neighbours
    // share an index), so it never moves with terrain and leaks nothing
    // through the survey mask; the family and the per-tile parameter vector
    // derive from the tile's substrate and cover.
    std::vector<std::uint8_t> family;   ///< Variant family per tile (land only; water/masked/void carry the neutral sentinel).
    std::vector<std::uint8_t> variant;  ///< 0 .. k_variant_count-1 per tile.
    std::vector<float>        vparam;   ///< k_vparam_count floats per tile: the ground variant's character (tone, hue, texture-field mix).
};

/// Variants per terrain family (BL-1243). Four: with six neighbours a proper
/// colouring needs at least four indices to leave a hash-chosen preference
/// any freedom (three would force a fixed tiling), and past four the soft
/// cross-fade (about one tile wide) averages the extra variety away while the
/// tables to author grow linearly.
inline constexpr int k_variant_count = 4;
/// Floats per tile in bake_source::vparam.
inline constexpr int k_vparam_count = 11;

/// Build the source arrays for @p body. Reads tile fields and the survey mask
/// (a masked tile bakes as the lock colour and never leaks terrain into a
/// revealed neighbour — its class excludes it from cross-class interpolation).
/// @p reveal_all lifts the mask (a fully-surveyed read; the spectator god view
/// keeps the mask ON here and lifts it at the draw call instead, as today).
/// @p reg resolves a processing facility's recipe family for its structure
/// stamp (null: the general form).
bake_source prepare_source(const world& w, entity_id body, bool reveal_all = false,
                           const recipe_registry* reg = nullptr);

/// Bake pixels [px0, px0+pw) x [py0, py0+ph) of @p g into @p out (pw*ph RGBA8,
/// ABGR u32, row-major). Pixels outside the grid's vertical extent bake
/// transparent (the canvas background shows through). Pure and deterministic:
/// same source + params + window -> byte-identical output.
void bake_region(const bake_source& src, const geometry& g, const bake_params& p,
                 int px0, int py0, int pw, int ph, std::uint32_t* out);

/// The never-magnify tier chooser (BL-1244, RENDERING.md § Level of detail):
/// the index of the SMALLEST tier whose px per hex is at or above @p draw_r,
/// so the tier is drawn minified (never past 2:1 on a x2 ladder) and never
/// magnified. -1 = the far page, which carries the frame only where it too is
/// drawn at or below 1:1 (@p draw_r <= @p far_ppr). Past the top tier the top
/// tier is returned and magnifies — the ladder's one bound.
/// @p tier_ppr must be ascending.
int choose_tier(double draw_r, double far_ppr, const double* tier_ppr, int n_tiers);

/// The shipped tier ladder (RENDERING.md § Level of detail): the far page and
/// the chunked tiers, px per hex circumradius. One source for ground_layer and
/// the headless chooser rows.
inline constexpr double k_far_ppr = 6.0;
inline constexpr int    k_tier_count = 5;
inline constexpr double k_tier_ladder[k_tier_count] = { 12.0, 24.0, 48.0, 96.0, 192.0 };

/// Content hash of everything bake_region reads for the given pixel window
/// (tile fields + mask state of the tiles overlapping it, plus a margin ring).
/// The ground_layer cache re-bakes a chunk when this moves.
std::uint64_t region_hash(const bake_source& src, const geometry& g,
                          int px0, int py0, int pw, int ph);

} // namespace ui::ground
