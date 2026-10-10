#pragma once

#include "structure_stamps.hpp" // BL-1241: the installation pass and its stamp seam
#include "route_paint.hpp"      // BL-1253: the roads and sea lanes pass
#include "world/entity.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Ground bake (BL-732) — the PURE half of the baked-chunk ground renderer.
//
// Composes the Planetary canvas's painterly ground (docs/ui/RENDERING.md) into
// RGBA8 pixel buffers, CPU-side: each land tile's own ground, blending into a
// neighbour only in a narrow band at the shared edge, with border sets between
// different terrain families (BL-1251; no cell boundary drawn), hillshade from the BL-517 height
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
    /// The base hillshade's sign. +1 is the shading as baked, which lights the
    /// SE-facing slopes — the opposite of the landform pass and the tree and
    /// structure shadows (NR-1006 (e)); -1 lights the NW-facing slopes, as the
    /// NW light every other pass uses. Flipped by Ben, 2026-10-10: one light for
    /// the ground, landforms, trees and structures.
    float hillshade_sign  = -1.0f;
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
    float edge_ink        = 0.0f;   ///< Darkening where two cover classes meet (>= 20 px/r). 0 since BL-1251:
                                    ///< a line between terrains is an outline; the border sets carry the edge.
    float shore_ink       = 0.18f;  ///< Darkening on the land|water boundary: the waterline (0.42 before BL-1251's shore shelf).
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
    // Tiles hold their own ground (BL-1251, RENDERING.md § Tiles hold their
    // own ground; Ben 2026-10-09: "there's still quite a blur over each tile").
    // A land tile's MATERIAL — colour, per-tile tone, landform accent, variant
    // vector, family pattern — is its own, undiluted, across its body; it
    // blends into a neighbour only within `edge_band` canonical units of the
    // shared edge (0.13 = 15% of the hex's 0.866 inradius, each side). The
    // material is resolved at a lightly frayed point (`material_fray`, the
    // amplitude of two small periodic warp octaves) so a tile's body sits on its
    // hex — where the close-zoom seam is drawn — while the land|water class
    // keeps the full warp (organic coasts). Terrain SHAPE (height, slope,
    // the oblique lift) keeps the wide smooth interpolation: relief is
    // continuous ground, not a material. Water keeps the wide blend too.
    float edge_band         = 0.13f;
    float material_fray     = 0.07f; ///< Fray of the material point, canonical units (0 = hex-true edges).
    // Border sets: the natural transition baked along every edge between two
    // DIFFERENT terrain families (forest fringe, scrub fringe, scree lip,
    // field edge, reed fringe, drift lip, snow drift, shore shelf). 0 = none
    // (the A/B lever).
    float border_strength   = 1.0f;
    // Family patterns and grain (furrows, tussocks, scree, ripples). 0 = none.
    float pattern_strength  = 1.0f;
    // Crisper texture inside a tile (BL-1254, RENDERING.md § Tiles hold their
    // own ground; Ben 2026-10-09: the ground read softer than the buildings).
    // Moves the base ground's energy from soft blotches to crisp marks: the
    // broad grain octave hands over to a firmer fine grain and a fleck grain,
    // patch edges firm, the relief gradient resolves its folds as creases
    // (a plain's soft swells ease), and relief creases and the family
    // patterns carry a light contact ink.
    // Nominal-keyed: full at the 96 px master, nothing at the far page. The
    // per-tile tone (the honeycomb) is untouched. 0 = the pre-BL-1254 look.
    float crisp             = 1.0f;
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
    // Roads and sea lanes (BL-1253, roads painted; route_paint.hpp): the
    // route pass's strength. 0 = the pass off (the A/B lever; the roaded
    // cluster layout and the trees' road clearance stay, they are geometry).
    float route_strength  = 1.0f;
    // The lock fast path (BL-1246): a window wholly inside survey-masked
    // ground is filled with the lock colour directly. Byte-identical to the
    // full bake (ground_bake_check P23); false = always resolve per pixel
    // (the harness's comparison lever).
    bool  fast_lock       = true;
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
    // Border sets (BL-1251). Per tile, per hex side (6 bytes per tile): the
    // transition set baked along that side's edge (low 7 bits; 0 = none) and,
    // in bit 7, whether this tile is the set's RECEIVING side (the open ground
    // a fringe steps into, the soil a scree lip spills onto). A pure function
    // of class and family, derived once; terrain_hash covers both inputs.
    std::vector<std::uint8_t> bset;
    /// Bit k: a RECEIVING side of border set k lies on this tile or a
    /// neighbour (the scatter pass's cull).
    std::vector<std::uint8_t> near_border;
    /// Per tile, the cultivated pattern's furrow direction (cos, sin): a pure
    /// function of the grid position, so it never moves with terrain.
    std::vector<float> furrow_cs;
    /// Bit k: points of scatter set k can stand on this tile; and per side,
    /// the same for the neighbour across it.
    std::vector<std::uint8_t> border_pts, nb_border_pts;
    /// 1: land with a water neighbour, or water with a land one (the shore
    /// shelf's cull).
    std::vector<std::uint8_t> coastal;
    // Roads and sea lanes (BL-1253; route_paint.hpp). Masked and void tiles
    // carry none, so no route is baked from unsurveyed ground. The derived
    // arrays are a pure function of road, lane, class and installations
    // (rederive_routes).
    std::vector<std::uint8_t> road;          ///< Route tier per tile (k_route_*): revealed land only.
    std::vector<std::uint8_t> lane;          ///< Sea-lane tier per tile: revealed water only.
    std::vector<std::uint8_t> route_links;   ///< Bits 0-3: road links E, W, S, N; bits 4-7: lane links (rungs skipped).
    std::vector<std::uint8_t> near_route;    ///< Bit 0: a road on this tile or a neighbour; bit 1: a lane (the pass's cull).
    std::vector<std::int32_t> route_first;   ///< Per tile: its first piece in route_pieces.
    std::vector<std::uint8_t> route_count;   ///< Per tile: its piece count.
    std::vector<route_piece>  route_pieces;
};

/// The border sets (BL-1251; RENDERING.md § Tiles hold their own ground) —
/// the values of bake_source::bset's low bits. The pairing table lives
/// beside derive_border_sets in ground_bake.cpp.
enum bset_id : std::uint8_t
{
    bs_none = 0, bs_forest, bs_scrub, bs_scree, bs_field, bs_reed, bs_drift, bs_snow,
    bs_count
};
inline constexpr std::uint8_t k_bs_recv = 0x80u; ///< bset byte: this tile is the receiving side.
inline constexpr std::uint8_t k_bs_mask = 0x7Fu;

/// Variants per terrain family (BL-1243). Four: with six neighbours a proper
/// colouring needs at least four indices to leave a hash-chosen preference
/// any freedom (three would force a fixed tiling), and the tables to author
/// grow linearly past four.
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

/// Re-derive the border sets (BL-1251) after a caller edits a source's class
/// or family arrays in place (the harness builds neighbour variations so).
void rederive_border_sets(bake_source& s);

/// Bake pixels [px0, px0+pw) x [py0, py0+ph) of @p g into @p out (pw*ph RGBA8,
/// ABGR u32, row-major). Pixels outside the grid's vertical extent bake
/// transparent (the canvas background shows through). Pure and deterministic:
/// same source + params + window -> byte-identical output.
void bake_region(const bake_source& src, const geometry& g, const bake_params& p,
                 int px0, int py0, int pw, int ph, std::uint32_t* out);

/// THE ONE CAMERA ANGLE (RENDERING.md § One angle, Ben 2026-10-09): the
/// planetary ground is viewed at 22.5 degrees at every rung and under every
/// lens — the canvas squashes y by this, and every bake is oblique by it.
inline constexpr double k_tilt_sy = 0.92387953251128674; ///< cos(22.5 deg)

/// THE ONE MASTER (RENDERING.md § Level of detail, Ben 2026-10-09): every
/// body's ground is baked ONCE, whole-body, at k_master_ppr px per hex at the
/// one angle, in 512 px chunks; every other zoom is a box-downsample of it.
/// Level 0 is the master; level k is the master halved k times — 96, 48, 24,
/// 12 and 6 px per hex. The master's W and H are multiples of
/// k_master_align (= 2^(k_level_count - 1)), so every level is a whole number
/// of pixels, every master chunk halves exactly at every level, and a level's
/// wrap period is still exactly its width.
inline constexpr double k_master_ppr   = 96.0;
inline constexpr int    k_level_count  = 5;
inline constexpr int    k_master_align = 1 << (k_level_count - 1);
inline constexpr double k_level_ppr[k_level_count] = { 96.0, 48.0, 24.0, 12.0, 6.0 };
/// The fallback page a not-yet-baked body is drawn from: a direct whole-body
/// bake at 6 px per hex (one job), shown until the master's levels cover the
/// view. Same angle as the master.
inline constexpr double k_far_ppr = 6.0;
/// Pixel side of one chunk, at every level.
inline constexpr int    k_chunk_px = 512;

/// The master geometry for a body grid: oblique at k_tilt_sy, ~k_master_ppr
/// px per hex, W and H rounded to multiples of k_master_align.
geometry make_master_geometry(int gw, int gh);

/// Level @p level (0 = the master) of @p master: s, W and H halved @p level
/// times; y_min, lift and tilt unchanged — the same image, smaller.
geometry level_geometry(const geometry& master, int level);

/// The level a drawn hex radius reads (RENDERING.md § Level of detail): the
/// COARSEST level whose px per hex is at or above @p draw_r, so it is drawn
/// minified by at most 2:1; above the master's 96 px the master is drawn
/// magnified (the top rung's accepted ~1.15x at the reference window).
int choose_level(double draw_r);

/// Box-downsample @p src (@p sw x @p sh, both even) to half size into @p dst
/// (sw/2 x sh/2), alpha-weighted so a transparent margin never darkens an
/// edge. Pure: the mip chain is built with it chunk by chunk, and because a
/// master chunk's sides are multiples of k_master_align the chunkwise chain
/// equals the whole-image one byte for byte.
void downsample_half(const std::uint32_t* src, int sw, int sh, std::uint32_t* dst);

/// Content hash of everything bake_region reads for the given pixel window
/// (tile fields + mask state of the tiles overlapping it, plus a margin ring).
/// The ground_layer cache re-bakes a chunk when this moves.
std::uint64_t region_hash(const bake_source& src, const geometry& g,
                          int px0, int py0, int pw, int ph);

/// The TERRAIN half of region_hash (BL-1246, the partial re-bake): every tile
/// field the bake reads, without the installations. region_hash ==
/// combine_region_hash(terrain_hash, installation_hash) for the same window.
std::uint64_t terrain_hash(const bake_source& src, const geometry& g,
                           int px0, int py0, int pw, int ph);
std::uint64_t combine_region_hash(std::uint64_t terrain, std::uint64_t installations);

/// A pixel rectangle, absolute bake pixels.
struct pixel_rect { int x0 = 0, y0 = 0, w = 0, h = 0; };

/// THE PARTIAL RE-BAKE WINDOW RULE (BL-1246, RENDERING.md § Chunks, cache and
/// invalidation — "a building's change re-bakes a window around it, not its
/// whole chunk", Ben 2026-10-09). Window [px0, px0+pw) x [py0, py0+ph) was
/// baked against @p old_src and must now show @p new_src. When the two differ
/// in INSTALLATIONS ONLY over the window (terrain_hash unchanged), fills @p out
/// with the rectangles whose re-bake against @p new_src, blitted over the old
/// pixels, gives the whole window's new bake byte for byte, and returns true:
///   - each changed tile's reach — every pixel its structures touch before
///     and after the change (installation_tile_bounds: the pass rasterised in
///     bounds mode) and, on a forest or scrub tile, the trees its cleared
///     ground removes or restores — plus k_patch_pad pixels for the post
///     passes that read across a pixel (unsharp +-2, anti-aliased rims),
///   - clipped to the window and aligned outward to k_patch_align pixels (so
///     every mip level's piece of a patch is whole pixels and derives from the
///     patch alone),
///   - overlapping or touching rectangles merged into their bounding box.
/// Returns false — re-bake the window whole — when the terrain moved, the
/// grids differ, or the rectangles cover more than @p max_fraction of it. An
/// empty @p out with true means nothing the window shows changed.
inline constexpr int k_patch_align = k_master_align;
inline constexpr int k_patch_pad   = 4;
bool installation_patch_rects(const bake_source& old_src, const bake_source& new_src,
                              const geometry& g, const bake_params& p,
                              int px0, int py0, int pw, int ph,
                              std::vector<pixel_rect>& out, double max_fraction = 0.4);

/// The mip pieces of a @p w x @p h master block (both multiples of
/// k_master_align): out[l] = the block halved l times, l = 1 .. k_level_count-1
/// (out[0] untouched). A whole chunk's chain and a patch's are built alike.
void derive_mip_pieces(const std::uint32_t* px, int w, int h,
                       std::vector<std::uint32_t> (&out)[k_level_count]);

} // namespace ui::ground
