#include "body_surface_canvas.hpp"
#include "text_fit.hpp"

#include "entity_summary.hpp"
#include "highlight.hpp"
#include "hover_card.hpp"
#include "hover_content.hpp"
#include "hex_render.hpp"
#include "icons.hpp"
#include "tile_inspector.hpp"   // history_view_tectonics (BL-660 routing)
#include "market_ledger.hpp" // market_city_name (Market lens catchment key, BL-015)
#include "nav_pane.hpp"
#include "presentation.hpp"
#include "shell_metrics.hpp"     // right chrome column + minimap rect (BL-533 legend home)
#include "ground_bake.hpp"      // k_tilt_sy: the one camera angle (BL-1246)
#include "world/battle_system.hpp" // first_battle_in (BL-469 battle rung)
#include "world/hex_neighbors.hpp"   // canonical odd-r neighbour offsets (BL-363)
#include "world/logistics.hpp"       // intra_body_path (convoy vision beam, BL-152)
#include "world/market_clearing.hpp" // market_for_tile (Scarcity catchment, prices)
#include "world/placement_rules.hpp"
#include "world/survey_system.hpp"   // survey_tile_visible (region mask, BL-067)
#include "world/workforce.hpp"       // workforce_efficiency (Population lens, BL-069)

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <SDL3/SDL_stdinc.h> // SDL_getenv: the pass meter's off-by-default flag

namespace ui {

namespace {

// ---------------------------------------------------------------------------
// The vector-layer pass meter (off by default; PLANETARY.md § Draw-loop cost
// model). IO_CANVAS_PASS_LOG=1 charges every map pass its emitted vertices and
// its CPU time, and prints the per-frame mean every 120 frames:
//   CANVAS_PASS draw_r 6.3 lens 0 frames 120 vtx 12345 | fill 1.20ms 4000v | ...
// A running cursor, not nested scopes: `mark(p)` charges everything since the
// previous mark to pass p, so instrumenting a section is one line at its end.
// ---------------------------------------------------------------------------
struct canvas_pass_meter
{
    enum pass : int { setup, ground, shade, band_depth, tile_misc, fill, band_wash,
                      lens_marks, roads, lanes, border_rule, player_ring, markers,
                      outline, post, count };
    static constexpr const char* k_names[count] = {
        "setup", "ground", "shade", "bdepth", "tile", "fill", "bwash", "lensmk",
        "roads", "lanes", "brule", "pring", "markers", "outline", "post" };

    bool        on = false;
    double      ms[count]  = {};
    long long   vtx[count] = {};
    long long   vtx_total  = 0;
    int         frames     = 0;
    float       last_r     = -1.0f;
    int         last_lens  = -1;
    std::chrono::steady_clock::time_point t;
    int         v = 0;

    /// The top of the canvas draw: everything up to the first mark is `setup`.
    void start(ImDrawList* dl)
    {
        if (!on) return;
        t = std::chrono::steady_clock::now();
        v = dl->VtxBuffer.Size;
    }
    /// Once the zoom and lens are known: a new (radius, lens) starts a new tally.
    void begin(float draw_r, int lens)
    {
        if (!on) return;
        if (std::fabs(draw_r - last_r) > 0.25f || lens != last_lens)
        {
            const auto keep_t = t;
            const int  keep_v = v;
            flush();
            t = keep_t;
            v = keep_v;
            last_r = draw_r;
            last_lens = lens;
        }
    }
    void mark(int p, ImDrawList* dl)
    {
        if (!on) return;
        const auto now = std::chrono::steady_clock::now();
        ms[p]  += std::chrono::duration<double, std::milli>(now - t).count();
        vtx[p] += dl->VtxBuffer.Size - v;
        t = now;
        v = dl->VtxBuffer.Size;
    }
    void end(ImDrawList* dl, int v_start)
    {
        if (!on) return;
        vtx_total += dl->VtxBuffer.Size - v_start;
        if (++frames >= 120)
            flush();
    }
    void flush()
    {
        if (frames > 0)
        {
            const double f = static_cast<double>(frames);
            double tot = 0.0;
            for (double x : ms) tot += x;
            std::printf("CANVAS_PASS draw_r %.1f lens %d frames %d vtx %lld cpu %.2fms",
                        last_r, last_lens, frames,
                        static_cast<long long>(static_cast<double>(vtx_total) / f), tot / f);
            for (int p = 0; p < count; ++p)
                if (ms[p] / f >= 0.005 || vtx[p] > 0)
                    std::printf(" | %s %.2fms %lldv", k_names[p], ms[p] / f,
                                static_cast<long long>(static_cast<double>(vtx[p]) / f));
            std::printf("\n");
            std::fflush(stdout);
        }
        *this = canvas_pass_meter{ on };
        last_r = -1.0f;
    }
};

canvas_pass_meter& pass_meter()
{
    static canvas_pass_meter m = [] {
        canvas_pass_meter pm;
        const char* e = SDL_getenv("IO_CANVAS_PASS_LOG");
        pm.on = e && *e && e[0] != '0';
        return pm;
    }();
    return m;
}

/// The construction-suitability wash's terrain test: tile-level `can_place`,
/// plus the world-level terrain gate of the two deposit-agnostic extraction
/// routes, which `can_place` passes through for `can_place_in_world` to
/// enforce — the Well on fresh water (BL-1198) and the Fishing Wharf on the
/// coast (BL-168). Without it the wash lit every land tile for a water or
/// agricultural_produce target. Tech, reach and the province ceiling stay out:
/// the wash shows ground, as it always has.
bool suitability_placeable(const world& w, entity_id id, const tile_component& tile,
                           building_type bt, resource_type target)
{
    if (!placement_rules::can_place(tile, bt, target))
        return false;
    if (bt != building_type::extraction_site
        || tile.resource_deposit[static_cast<std::size_t>(target)] > 0.0f)
        return true;
    if (target == resource_type::water)
        return placement_rules::is_fresh_water_adjacent(w, id);
    if (target == resource_type::agricultural_produce)
        return placement_rules::is_coastal(w, id);
    return true;
}

constexpr float kSqrt3 = 1.7320508f;
constexpr float kPi    = 3.14159265f;

// Zoom is normalised to grid height: at zoom = 1 the grid fills kFitMargin of the
// canvas height. The zoom floor must therefore be derived from these constants,
// not guessed — at the minimum the viewport spans kMinZoomHeadroom of the grid
// height (the full grid plus ~20% headroom), i.e. zoom = 1 / (headroom * margin).
constexpr float kFitMargin       = 0.95f; // grid fills 95% of the canvas height at zoom = 1
constexpr float kMinZoomHeadroom = 1.2f;  // the old floor's viewport spanned ~120% of the grid height
// Zoom-out cap (Ben, 2026-08-12): the full-grid view lags on the 3x map — 45,240
// hexes in one draw pass — so the widest view is restricted to 0.7x the old
// maximum extent (extent scales as 1/zoom, hence the division below). The
// default framing (4/3, view_nav.cpp) sits above this floor, so only the last
// stretch of zoom-out is lost, not any framing the app itself sets.
constexpr float kMaxViewScale    = 0.7f;
constexpr float kMaxZoom         = 20.0f;
constexpr float kMinZoom         = 1.0f / (kMaxViewScale * kMinZoomHeadroom * kFitMargin); // ~1.253

/// Rungs on the stepped x2 zoom ladder (Ben, 2026-09-01): kMinZoom * 2^k for
/// k = 0..k_zoom_rungs-1. kMinZoom * 16 ≈ 20.05, so the top rung clamps to
/// kMaxZoom and the ladder spans exactly the old continuous range in 5 steps.
constexpr int k_zoom_rungs = 5;

// terrain_colour, hex_vertices, and hex_local_centre now live in ui/hex_render.hpp
// (shared with the Selection band's zoomed tile-neighbourhood view, BL-194) so both
// surfaces draw from one terrain palette and one hex geometry rather than diverging.

/// Per-channel linear blend of two opaque colours: result = a·(1−t) + b·t, alpha
/// forced opaque. Used by the Resource lens to composite a deposit hue over terrain
/// at a magnitude-driven opacity, and by the Market lens's diverging wash.
ImU32 lerp_colour(ImU32 a, ImU32 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    auto ch = [](ImU32 c, int shift) -> float { return static_cast<float>((c >> shift) & 0xFFu); };
    const int r = static_cast<int>(ch(a, IM_COL32_R_SHIFT) * u + ch(b, IM_COL32_R_SHIFT) * t);
    const int g = static_cast<int>(ch(a, IM_COL32_G_SHIFT) * u + ch(b, IM_COL32_G_SHIFT) * t);
    const int bl= static_cast<int>(ch(a, IM_COL32_B_SHIFT) * u + ch(b, IM_COL32_B_SHIFT) * t);
    return IM_COL32(r, g, bl, 255);
}

// ---------------------------------------------------------------------------
// BL-511 — the province as the rendered and selected unit
// ---------------------------------------------------------------------------
// Geometry is still per hex. What changed is that a hex's fill is blended into
// its province's, so the seams inside a province disappear and the four-tile
// cell reads as one soft shape that still carries its real terrain mixture.
// The two tables below are the whole geometric argument, so they live together.
//
// Vertex i sits at angle 30 + 60i (hex_vertices), and hex_neighbors' side order
// is 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE — i.e. sides at 0, 300, 240, 180, 120, 60
// degrees with y down. Each corner therefore falls exactly between two sides,
// and each side spans exactly two corners.

/// The two sides sharing hex corner i. Read by the blend: a corner takes the
/// mean of this tile and its corner-sharing same-province neighbours.
constexpr int k_corner_sides[6][2] = { {0,5}, {5,4}, {4,3}, {3,2}, {2,1}, {1,0} };

/// The two corners spanning hex side s. Read by the province-edge pass: an edge
/// toward a different province is stroked between these two vertices.
constexpr int k_side_verts[6][2] = { {5,0}, {4,5}, {3,4}, {2,3}, {1,2}, {0,1} };

/// How far a blended corner travels from the tile's OWN fill toward the mean of
/// its blending neighbours. `1.0` is the flat mean — the full-strength blend;
/// `0.0` is no blend at all, every hex flat.
///
/// NAMED FOR LAND, NOT FOR PROVINCES, and the name is the point. BL-511 blended
/// a hex into its PROVINCE, stopping at the cell boundary. BL-514 removed that
/// stop (Ben, 2026-08-22: *"blur should cross province borders"*), so what is
/// dialled here is a **land-wide continuous field** — the corner mean below has
/// no province-match term and must not get one back. `ns.blend` carries the only
/// exclusions that remain, and they are not about provinces.
///
/// BL-597, Ben 2026-08-24: *"Just reduce the amount of smearing so it looks less
/// blurred."* The blend STAYS — it is doing something wanted (land reads as one
/// continuous field rather than a wireframe of seams) and was simply doing too
/// much of it. So this is a MAGNITUDE, not a mechanism change: none of the three
/// offered redesigns (drop it, confine it to the seam, invert it) was taken, and
/// a constant is far cheaper to tune by eye than a pass is to replace.
///
/// Read the chain and it is not a reversal of the original design: BL-511 blended
/// inside a province, BL-514 widened it across the whole landmass, and this is
/// Ben seeing the result of THAT widening at working zoom and pulling it back.
///
/// The value is Ben's eye in the live app, not a derived bound — 0.35 is the
/// starting point the ruling names. `1.0` must reproduce the pre-BL-597 render
/// byte for byte, which is the guard this constant is checked against
/// (`scripts/verify/zoom_ladder.lua`).
constexpr float k_land_blend_strength = 0.35f;

// ---------------------------------------------------------------------------
// BL-601 — the national border band (always-on chrome, no longer a lens)
// ---------------------------------------------------------------------------
// Ben, 2026-08-24: "National borders should not diffuse together, instead they
// should borders extending their colour inwards. With this, we can drop the
// nation lens."
//
// A nation reads as a BORDERED REGION, not a tinted field. Its identity colour
// lives at the boundary and falls off inwards over a few tiles, which is what
// makes an always-on read affordable at all: the middle of a territory stays
// free for terrain, texture and whatever lens is active, which a full-territory
// tint cannot do. Roads set the precedent — drawn always, not behind a lens.
//
// TWO NEIGHBOURS MEETING MUST NEVER BLEND INTO A THIRD COLOUR. That is exactly
// the objection PLANETARY.md's categorical refusal raised against blending the
// old Country lens (overruled by BL-532, and reinstated here in a shape that
// does not need the refusal): the band is composited PER TILE, after the
// province blend has already run, and each tile takes only its OWN nation's
// colour. Nothing in this pass ever averages two nations' hues.

/// Depth of the inward band, in tiles. Depth 0 is a tile touching a foreign
/// owner — another nation, or unclaimed ground, so a coastline is a border too.
///
/// ONE TILE (Ben, 2026-09-01, judging the baked ground): the three-ring falloff
/// was tuned against flat saturated hexes; over the muted painterly bake the
/// band inverted its contrast relationship with the ground and became the
/// loudest mark on the map. "A 1 tile glow, rather than the current 2/3 tile
/// glow" — the frontier ring alone, in a muted colour (below).
constexpr int k_border_band_tiles = 1;

/// Wash opacity by depth — the single frontier ring.
constexpr float k_border_band_alpha[k_border_band_tiles] = { 0.35f };

/// How far the band's colour is pulled toward its own luma before drawing —
/// the "muted colour palette" half of the same 2026-09-01 ruling. Applied to
/// the wash AND the stroke; the border-corridor hover label keeps the full
/// identity colour, because a label must be read, not weighed.
constexpr float k_border_mute = 0.55f;

/// Mute a nation identity colour for the band: desaturate toward its own luma
/// by k_border_mute and sit it down slightly, so the ring reads as a claim on
/// the ground rather than as chrome over it.
inline ImU32 muted_nation_colour(ImU32 nc)
{
    const float r = static_cast<float>((nc >> IM_COL32_R_SHIFT) & 0xFFu);
    const float g = static_cast<float>((nc >> IM_COL32_G_SHIFT) & 0xFFu);
    const float b = static_cast<float>((nc >> IM_COL32_B_SHIFT) & 0xFFu);
    const float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    auto ch = [&](float v) {
        return static_cast<int>((v + (luma - v) * k_border_mute) * 0.92f);
    };
    return IM_COL32(ch(r), ch(g), ch(b), 255);
}

/// Resource lens ground (Ben, 2026-10-04): a WHITE WASH that keeps a ghost of the
/// terrain. The tile is desaturated toward its own luma, then pulled most of the way to
/// a pale wash colour (warm off-white on land, pale blue-grey on water so the coastline
/// survives). The deposits, drawn solid with a dark rim, are then the only saturated
/// and the only dark-edged thing on the map.
constexpr float k_resource_ground_desat = 0.80f;
constexpr float k_resource_ground_wash  = 0.72f;

inline ImU32 washed_resource_ground(ImU32 c, bool water)
{
    const float r = static_cast<float>((c >> IM_COL32_R_SHIFT) & 0xFFu);
    const float g = static_cast<float>((c >> IM_COL32_G_SHIFT) & 0xFFu);
    const float b = static_cast<float>((c >> IM_COL32_B_SHIFT) & 0xFFu);
    const float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    const float wr = water ? 206.0f : 244.0f;
    const float wg = water ? 216.0f : 242.0f;
    const float wb = water ? 230.0f : 236.0f;
    auto ch = [&](float v, float w) {
        const float d = v + (luma - v) * k_resource_ground_desat;
        return static_cast<int>(d + (w - d) * k_resource_ground_wash);
    };
    return IM_COL32(ch(r, wr), ch(g, wg), ch(b, wb), 255);
}

// --- A lens washes the rendered ground (BL-1250; LENSES.md § A lens washes the
// rendered ground) ---------------------------------------------------------------
// Under a lens the baked ground stays; each lens's per-tile answer is a translucent
// wash over it in the tile's own geometry. ONE strength per lens family, set by eye
// against the lens_modes / owner_multi_select captures:
//
//   CATEGORICAL (Corporation, Company, Market, Continent, Resource deposits) — a
//   hue says WHICH, and a hue reads at a lower alpha than a value does, because the
//   eye separates hues even over textured ground; the structures, forms and rivers
//   beneath must still read through, so the wash stays under half.
//   SEQUENTIAL (Population, Throughput, Scarcity, Industry) — a ramp says HOW MUCH,
//   and a value read needs more of the ramp colour over the ground's own light and
//   dark: a little stronger. Where a sequential lens's old blend already scaled with
//   its value (Scarcity, Industry), the value scales this alpha the same way.
constexpr float k_lens_wash_categorical = 0.46f;
constexpr float k_lens_wash_sequential  = 0.54f;
/// The Resource lens's no-deposit ground: its ruled white wash (Ben, 2026-10-04,
/// "the white wash should still exist") over the bake, paler than the fill path's
/// 72% so the ground's forms survive under it while the deposits stay the only
/// saturated thing.
constexpr float k_resource_ground_wash_alpha = 0.50f;

/// @p c's RGB at alpha @p a (0..1).
inline ImU32 with_alpha(ImU32 c, float a)
{
    const int ai = static_cast<int>(std::lround(std::clamp(a, 0.0f, 1.0f) * 255.0f));
    return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(ai) << IM_COL32_A_SHIFT);
}

// --- The close-zoom seam (BL-1251 part 5; RENDERING.md § The grid rule) ---------
// At the two closest rungs only, a faint line between neighbouring tiles: a
// low-alpha BLACK stroke, so it is the ground under it darkened, never a colour of
// its own. Keyed on the drawn radius: 40 px sits between rung 2 (~27 px) and rung 3
// (~55 px) at the reference 1720x1080 window.
constexpr float k_seam_min_drawn_r = 40.0f;
constexpr float k_seam_alpha       = 0.16f;

/// Scale applied to the whole treatment — wash AND stroke — where the frontier
/// faces UNCLAIMED ground rather than another nation (Ben, 2026-08-24: "reduce
/// the border band on edges facing unclaimed ground").
///
/// Both kinds of edge are still borders — a coastline is where a nation stops —
/// but they are not the same claim, and drawing them at the same weight made the
/// band read as heavy on exactly the nations that have the most of it. A country
/// of small islands is nearly all frontier, so at full strength almost none of
/// its land showed plain terrain: the treatment that was meant to be an edge
/// effect became a tint again for the shapes least able to afford it.
///
/// A tile touching BOTH a foreign nation and unclaimed ground counts as
/// political — the stronger claim wins, so a coastal frontier between two
/// countries does not quietly fade.
constexpr float k_border_unclaimed_scale = 0.40f;

/// The boundary stroke is INSET toward the drawing tile's own centre by this
/// fraction of the circumradius, rather than laid along the shared edge. That is
/// what keeps two neighbours' colours apart: each nation paints a rule just
/// inside its own side of the frontier, so a border reads as two parallel
/// coloured lines with the seam between them — never one line of a mixed hue.
constexpr float k_border_stroke_inset = 0.18f;
constexpr float k_border_stroke_px    = 2.2f;

/// Hit corridor half-width, in screen pixels, around each drawn boundary
/// segment. The drawn stroke is a line and a line is not clickable at play zoom
/// (Ben's ruling on the nation-ledger route), so the band carries a real hit
/// width that is independent of how thick it is drawn.
///
/// CAPPED AS A FRACTION OF THE HEX as well, because a fixed pixel width is only
/// thin at close zoom. A frontier tile can face several foreign neighbours at
/// once, so its corridors ring most of its rim; at play zoom that still leaves
/// the middle of the hex selecting the tile, but a 7 px corridor on a 17 px
/// inradius would swallow the tile whole. The effective width is therefore
/// min(k_border_hit_px, draw_r * k_border_hit_frac) — 7 px wherever a hex is
/// large enough to spare it, and proportional below that.
constexpr float k_border_hit_px   = 7.0f;
constexpr float k_border_hit_frac = 0.18f;

/// Per-tile shade, computed one pass ahead of the draw loop so a tile's
/// neighbours' colours are in hand when its corners are blended.
struct tile_shade
{
    ImU32    fill     = 0;      ///< The tile's final composited colour.
    uint32_t province = 0;      ///< Owning province id; 0 = ocean / unpartitioned.
    bool     blend    = false;  ///< Whether this tile joins its province's blend.
    bool     province_known = false; ///< `province` was looked up (the bake skips it).
};

/// Per-channel mean of @p n opaque colours. The blend's only arithmetic: a
/// corner shared by k same-province tiles takes the mean of their fills, which
/// is what makes the gradient express the MIXTURE rather than a winner.
ImU32 mean_colour(const ImU32* c, int n)
{
    if (n <= 0) return IM_COL32(0, 0, 0, 255);
    int r = 0, g = 0, b = 0;
    for (int i = 0; i < n; ++i)
    {
        r += static_cast<int>((c[i] >> IM_COL32_R_SHIFT) & 0xFFu);
        g += static_cast<int>((c[i] >> IM_COL32_G_SHIFT) & 0xFFu);
        b += static_cast<int>((c[i] >> IM_COL32_B_SHIFT) & 0xFFu);
    }
    return IM_COL32(r / n, g / n, b / n, 255);
}

/// Does @p m's field blend across a province, or must it stay crisp per tile?
///
/// THE PER-LENS REDUCTION IS A DECISION, NOT A DEFAULT (BL-511, requirement R3).
/// The full table with the reasoning for each mode is in PLANETARY.md § Province
/// grain; this is its executable half. Two lenses blend because their field is a
/// continuous property of the ground itself. Every other mode refuses, and the
/// refusals are the interesting half:
///
///   - CATEGORICAL fields (country, continent) must not blend: the mean of two
///     nation colours is a third nation's colour, and the mean of two plate
///     colours is a plate that does not exist. A province straddling a border is
///     a real fact the lens exists to show.
///   - CATCHMENT fields (market, scarcity) are already coarser than a province;
///     blending would soften the catchment boundary the lens is about.
///   - SPARSE fields (corporation, industry) are attributes of one
///     building on one tile. Smearing a point value over the empty ground beside
///     it is exactly the "one tile's value standing for the whole province"
///     defect — so these reduce per PROVINCE instead (see the uniform pass), not
///     per vertex.
///   - Population draws a per-tile DOT, not a fill, so there is no fill to blend.
///   - Reach and Supply-routes are body-level and paint no tile fill at all.
bool lens_blend_mode(overlay_mode m)
{
    // BL-532 (Ben, 2026-08-22): "lenses should not revert to render each tile in
    // the previous mode, we can keep the province view."
    //
    // This OVERRULES the categorical and catchment refusals documented above,
    // knowingly — Ben was asked whether Country and Continent should fill
    // uniformly per province (inventing no colours) or blend across province
    // vertices like everything else, and chose to blend. The objection is not
    // withdrawn, only outranked: the mean of two nation colours IS a third
    // nation colour, so Country can tint a province toward a neighbour it does
    // not belong to. If that reads wrong in play it goes back to Ben (BL-532),
    // rather than being quietly reinstated here.
    //
    // The SPARSE lenses are deliberately still absent, and that is not a partial
    // application of the ruling. Industry, Corporation and Production already
    // reduce per province and fill it flat (see the uniform pass), so they
    // ALREADY keep the province view that the ruling asks for — vertex-blending
    // them on top would spread one works' value onto the empty ground beside it,
    // which is the one defect the province grain exists to remove. Population,
    // Opportunity, Reach and Supply-routes paint no tile fill to blend at all.
    switch (m)
    {
        case overlay_mode::none:
        case overlay_mode::resource:
        case overlay_mode::continent:
        case overlay_mode::market:
        case overlay_mode::scarcity:  return true;
        default:                      return false;
    }
}

/// Emit @p verts as a 6-triangle fan with per-corner colours — the blend's draw
/// call. ImDrawList's AddConvexPolyFilled is flat-colour only, so the fan is
/// written through the Prim* API. 7 vertices / 18 indices against the ~10 an
/// anti-aliased 6-gon costs, so this is not a regression on the measured
/// vertex budget (BL-269).
void prim_blended_hex(ImDrawList* dl, const ImVec2 verts[6], ImVec2 centre,
                      ImU32 centre_col, const ImU32 corner_col[6])
{
    // The white-pixel UV via the PUBLIC accessor: ImDrawList::_Data points at
    // ImDrawListSharedData, which is only a forward declaration outside
    // imgui_internal.h, so reading TexUvWhitePixel off it does not compile here.
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    dl->PrimReserve(18, 7);
    const unsigned int base = dl->_VtxCurrentIdx;
    dl->PrimWriteVtx(centre, uv, centre_col);
    for (int i = 0; i < 6; ++i)
        dl->PrimWriteVtx(verts[i], uv, corner_col[i]);
    for (int i = 0; i < 6; ++i)
    {
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(base));
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1 + i));
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1 + ((i + 1) % 6)));
    }
}

/// The intra-body reach fog's wash (BL-151/152/154): dim a colour toward the fog dark
/// by how little the player sees the tile — `vision` 1 = fully lit, 0 = unreached.
/// Alpha is preserved (lerp_colour forces opaque, which would change how a translucent
/// road span reads). Every layer the fog covers goes through this one function — the
/// lens fill and, since BL-185, the road spans — so the fog reads as a single wash
/// rather than a dark ground with brightly-lit roads laid over it.
ImU32 fog_dim(ImU32 c, float vision)
{
    if (vision >= 1.0f)
        return c;
    const ImU32 alpha = (c >> IM_COL32_A_SHIFT) & 0xFFu;
    const ImU32 dim   = lerp_colour(c, IM_COL32(8, 10, 16, 255), 0.5f * (1.0f - vision));
    return (dim & ~(0xFFu << IM_COL32_A_SHIFT)) | (alpha << IM_COL32_A_SHIFT);
}

/// ROUTE STROKE WIDTHS, one named constant per tier (RENDERING.md § Roads and sea
/// lanes; Ben, 2026-10-03: "make them thinner"). Each is the stroke width as a fraction
/// of the drawn hex circumradius, which is floored at 10 px before it is applied so
/// the tiers stay apart on the whole-grid view. Half the widths the straight-segment
/// roads used (0.12 / 0.18 / 0.24); the 1 : 1.5 : 2 ladder between the tiers is kept,
/// so Track, Road and Highway still read as three weights.
constexpr float k_road_width_track   = 0.06f;
constexpr float k_road_width_road    = 0.09f;
constexpr float k_road_width_highway = 0.12f;
/// A sea lane's one rung (LOGISTICS.md § 4b): a soft band a little under a Highway,
/// told apart from the road ladder by its sea blue rather than by weight.
constexpr float k_lane_width         = 0.10f;
/// Below this DRAWN hex radius a road or lane curve takes the wide-rung LOD: one
/// stroke per curve, two segments a half (chords at the coarse fill), no apex
/// joint (RENDERING.md § Roads and sea lanes; the 60 fps budget, Ben 2026-10-09).
/// 20 px sits between the second rung (~13 px) and the third (~27 px): the third
/// rung and up draw the curve as they always have.
constexpr float k_route_lod_radius_px = 20.0f;

/// Building silhouette radius as a fraction of the hex circumradius — since BL-1241
/// (structures baked) used ONLY by the construction ghost, the one place a building
/// glyph still draws on this canvas (an armed placement is chrome, not ground).
/// The silhouette scales to the hex rather than being a small pin on it; the value leaves the
/// lower-right corner free for the owner emblem tag and keeps the widest glyph (the
/// square) inside the hexagon's inradius. Shared with the construction ghost so the
/// armed preview matches what actually lands.
///
/// BL-596 removed the *plate* this silhouette used to stand on — a built tile no
/// longer swaps its hex out for an owner-tinted fill. The glyph now draws over live
/// ground, so its legibility rests on its own dark outline against a live background,
/// which is what the icon vocabulary is for (ICONS.md § Shared conventions).
constexpr float kBuiltSilhouetteScale = 0.48f;

/// Shared red→yellow→green ramp for every "red to green" lens (Opportunity,
/// Population/workforce, Production). `t` in [0, 1]: 0 = red (low), 0.5 = yellow
/// (mid), 1 = green (high). Routing all these lenses through one helper keeps a
/// mid value reading **yellow** rather than the muddy brown a direct red→green
/// blend (or the former grey neutral) produces. Ben's directive 2026-07-10.
ImU32 ryg_colour(float t)
{
    constexpr ImU32 red    = IM_COL32(216, 100,  96, 255);
    constexpr ImU32 yellow = IM_COL32(228, 200,  84, 255);
    constexpr ImU32 green  = IM_COL32(110, 200, 120, 255);
    t = std::clamp(t, 0.0f, 1.0f);
    return t < 0.5f ? lerp_colour(red,    yellow, t * 2.0f)
                    : lerp_colour(yellow, green,  (t - 0.5f) * 2.0f);
}

/// Paint the lens chrome region's panel — fill, border, and the input blocker — and
/// hand back its rect. Shared by BOTH key families (BL-602), which is the whole point:
/// there is one region, so there is one function that opens it.
///
/// The BLOCKER is a full-region empty ImGui window. The panel paints through a draw
/// list, which draws pixels and nothing else — it registers no window, so
/// `io.WantCaptureMouse` stays false over it, and `app.cpp` derives the canvas's
/// `primary_input` from exactly that flag. Without it, a press on the legend ALSO
/// lands on the canvas behind it: the measured symptom was clicking the legend header
/// toggling the legend AND selecting whatever tile sat underneath.
///
/// @param want_h Height the caller's content needs; the region caps it (shell_metrics).
/// @param id     Distinct blocker-window id, so two keys never collide on one name.
ui::shell_rect open_lens_chrome(ImDrawList* dl, const ui_state& state, float want_h,
                                const char* id)
{
    const ui::shell_rect r =
        ui::lens_chrome_rect(ImGui::GetIO().DisplaySize, state.time_panel_h, want_h);
    const ImVec2 p0 = { r.x, r.y };
    const ImVec2 p1 = { r.x + r.w, r.y + r.h };
    // OPAQUE. The region is chrome sitting on the minimap's edge, not an overlay on
    // the map, and its neighbour (the minimap box) is opaque too — a translucent fill
    // let terrain hexes read faintly through the panel, which is a milder version of
    // exactly the complaint this region exists to answer (NR-601). Nothing underneath
    // it is worth seeing; the key is.
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 255), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    ImGui::SetNextWindowPos(p0, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ r.w, r.h }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags bflags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav   | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin(id, nullptr, bflags);
    ImGui::End();
    ImGui::PopStyleVar();
    return r;
}

/// Shared chrome for a FIXED-HEIGHT lens key — the gradient bars (Resource, Scarcity,
/// Population, Industry) and the Continent swatch list. Opens the one lens chrome
/// region at exactly the height the caller measured and returns the inner top-left and
/// content width via @p out_x / @p out_y / @p out_w.
///
/// These keys have nothing to overflow, so they draw OPEN. The collapse affordance
/// belongs only to the count-driven keys, because a list that grows with the world is
/// the only thing it was ever for (Ben, 2026-08-22, NR-503).
///
/// Drawn on the BACKGROUND list like every other key. The Continent key used to take
/// the foreground list with an opaque fill (BL-376) because at the old flush-left
/// anchor it sat inside the always-open Selection band; the region no longer overlaps
/// any window, so the z-order patch has nothing left to work around and one draw path
/// serves all of them.
void begin_lens_key(ImDrawList* dl, const ui_state& state,
                    float body_h, float pad, float& out_x, float& out_y, float& out_w)
{
    const ui::shell_rect r = open_lens_chrome(dl, state, body_h, "##lens_key_blocker");
    out_x = r.x + pad;
    out_y = r.y + pad * 0.5f;
    out_w = r.w - 2.0f * pad;
}

/// --- Resource lens: the toggled set and the pie split (Ben, 2026-10-04) -------
/// The lens shows a SET of extractable resources at once. A tile carrying one of
/// them fills flat with its colour; a tile carrying several splits into equal pie
/// wedges from the hex centre, in toggle order. The cap is six — one per hex edge.

bool lens_resource_on(const ui_state& state, resource_type r)
{
    return std::find(state.lens_resources.begin(), state.lens_resources.end(), r)
           != state.lens_resources.end();
}

/// Toggle @p r in the Resource lens set. Adding past the cap is refused, not
/// wrapped: the checklist greys the unchecked rows when the set is full.
void toggle_lens_resource(ui_state& state, resource_type r)
{
    auto& v = state.lens_resources;
    const auto it = std::find(v.begin(), v.end(), r);
    if (it != v.end())
        v.erase(it);
    else if (static_cast<int>(v.size()) < ui_state::k_lens_resource_cap)
        v.push_back(r);
}

/// The toggled resources this tile carries (deposit > 0), in toggle order.
int lens_resources_on_tile(const ui_state& state, const tile_component& tile,
                           resource_type out[ui_state::k_lens_resource_cap])
{
    int n = 0;
    for (const resource_type r : state.lens_resources)
    {
        const std::size_t i = static_cast<std::size_t>(r);
        if (i < std::size(tile.resource_deposit) && tile.resource_deposit[i] > 0.0f
            && n < ui_state::k_lens_resource_cap)
            out[n++] = r;
    }
    return n;
}

/// A point on the hex perimeter at parameter @p t in [0, 6]: vertex i (in
/// hex_vertices order) sits at t = i, and t walks the edges between them.
ImVec2 hex_perimeter_point(ImVec2 c, float r, float t)
{
    const int   i  = static_cast<int>(std::floor(t)) % 6;
    const float f  = t - std::floor(t);
    const float a0 = kPi / 6.0f + kPi / 3.0f * static_cast<float>(i);
    const float a1 = a0 + kPi / 3.0f;
    return { c.x + r * ((1.0f - f) * std::cos(a0) + f * std::cos(a1)),
             c.y + r * ((1.0f - f) * std::sin(a0) + f * std::sin(a1)) };
}

/// Which of @p n equal perimeter wedges the offset @p d (pointer minus hex centre)
/// falls in — the inverse of the split draw, so a press lands on the wedge it sees.
int lens_wedge_at(ImVec2 d, int n)
{
    if (n <= 1)
        return 0;
    float th = std::atan2(d.y, d.x) - kPi / 6.0f;
    while (th < 0.0f) th += 2.0f * kPi;
    const int i = std::min(5, static_cast<int>(th / (kPi / 3.0f)));
    const float a0 = kPi / 6.0f + kPi / 3.0f * static_cast<float>(i);
    const ImVec2 a{ std::cos(a0), std::sin(a0) };
    const ImVec2 b{ std::cos(a0 + kPi / 3.0f), std::sin(a0 + kPi / 3.0f) };
    const auto cross = [](ImVec2 p, ImVec2 q) { return p.x * q.y - p.y * q.x; };
    const float den = cross(d, { b.x - a.x, b.y - a.y });
    const float s   = (std::fabs(den) > 1e-6f) ? std::clamp(-cross(d, a) / den, 0.0f, 1.0f) : 0.0f;
    const float t   = static_cast<float>(i) + s;
    return std::min(n - 1, static_cast<int>(t * static_cast<float>(n) / 6.0f));
}

/// Draw @p n pie wedges over a hex of circumradius @p r, wedge k in @p cols[k].
/// Each wedge spans 6/n of the perimeter; with n >= 2 that is at most half the
/// hex, so every wedge is convex at the centre and one filled polygon.
void draw_lens_wedges(ImDrawList* dl, ImVec2 c, float r, const ImU32* cols, int n)
{
    for (int k = 0; k < n; ++k)
    {
        const float t0 = 6.0f * static_cast<float>(k)     / static_cast<float>(n);
        const float t1 = 6.0f * static_cast<float>(k + 1) / static_cast<float>(n);
        ImVec2 pts[10];
        int    m = 0;
        pts[m++] = c;
        pts[m++] = hex_perimeter_point(c, r, t0);
        for (int v = static_cast<int>(std::floor(t0)) + 1; static_cast<float>(v) < t1; ++v)
            pts[m++] = hex_perimeter_point(c, r, static_cast<float>(v));
        pts[m++] = hex_perimeter_point(c, r, t1);
        dl->AddConvexPolyFilled(pts, m, cols[k]);
    }
}

/// The lens-local good selector for the Market and Scarcity lenses (BL-134): both
/// pick "which good" from the same `lens_resource` field, so one combo serves them.
/// The Resource lens left this combo for its own toggled checklist (Ben, 2026-10-04).
/// Now lives at the top of the on-canvas legend (moved off the minimap strip,
/// which the former popup button docked in) — a real scrollable ImGui::BeginCombo,
/// hosted in a small borderless window since the legend itself paints on the
/// background draw list rather than a live ImGui window.
constexpr float kLensComboH = 22.0f;

void draw_lens_resource_combo(ui_state& state, ImVec2 pos, float w)
{
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, kLensComboH}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("##lens_key_resource_combo", nullptr, flags);
    ImGui::SetNextItemWidth(w);
    if (ImGui::BeginCombo("##lens_key_resource", presentation_of(state.lens_resource).name))
    {
        for (std::size_t i = 0; i < resource_count; ++i)
        {
            const resource_type r = static_cast<resource_type>(i);
            if (ImGui::Selectable(presentation_of(r).name, r == state.lens_resource))
                state.lens_resource = r;
        }
        ImGui::EndCombo();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

/// Marker glyph for a count-driven lens-key row: a filled swatch (Country / Market),
/// a small dot (Reach), or a thickness bar (Supply). Lets one scrollable renderer
/// serve every list-shaped legend.
enum class key_marker { swatch, dot, bar };

/// One row of a count-driven lens key: a marker + a label, each with its own colour
/// (swatch legends label in neutral grey; the connection legends colour the label by
/// recency tier). `bar_frac` is the [0,1] fill only key_marker::bar reads.
struct key_row
{
    ImU32       marker_colour;
    ImU32       label_colour;
    std::string label;
    key_marker  marker;
    float       bar_frac = 0.0f;
};

/// Shared chrome for a count-driven, potentially-overflowing lens key (BL-163/164):
/// the Country, Market, Reach and Supply-routes legends, whose row list grows with the
/// world. A header bar over a bounded, smoothly-scrolling body of rows, plus an
/// optional good-selector combo (Market). Must run inside the ImGui frame -- it opens
/// child windows, like draw_lens_resource_combo. @p combo_state != nullptr draws the
/// resource selector.
///
/// It lives in the ONE lens chrome region (BL-602, ui/shell_metrics.hpp), the same
/// region the fixed-height gradient keys use: bottom edge on the minimap's top edge,
/// right edge on the screen edge, growing upward and ceilinged below the time panel.
///
/// THE HEADER SITS AT THE FOOT OF THE BOX, and that is deliberate. The region is
/// bottom-anchored, so a header drawn at the top would travel with the box: opening
/// the list moved the control ~320 px up the screen and a second press at the same
/// point landed on the canvas instead of closing it -- the toggle worked exactly once.
/// Drawn at the foot, the header stays on the minimap's edge open or shut, so
/// open/close is one repeatable press in one place, and the list reads as a drawer
/// sliding up out of the minimap header. Above it sits the combo, and above that the
/// rows.
///
/// Collapsed by default (Ben, 2026-08-22, NR-503), so a lens switch never throws a
/// forty-row list over the column. Long labels WRAP rather than widening the box --
/// the width is the chrome column's ("keep names shorter, and use text wrapping").
void draw_scroll_list_key(const char* id, const char* header,
                          const std::vector<key_row>& rows, const char* empty_note,
                          ui_state* combo_state, ui_state& state)
{
    const float pad      = 8.0f;
    const float line_h   = ImGui::GetTextLineHeight();
    const float swatch   = line_h;
    const float row_h    = line_h + 2.0f;              // matches the legacy per-row advance
    const float header_h = line_h + 4.0f;
    const float combo_h  = combo_state ? (kLensComboH + 4.0f) : 0.0f;
    const float bar_max  = 40.0f;                      // key_marker::bar full length

    const ImVec2 disp   = ImGui::GetIO().DisplaySize;
    const float  box_w2 = ui::minimap_rect(disp).w;
    const float  body_w = box_w2 - 2.0f * pad;

    // A row may WRAP, so its height is measured rather than assumed.
    const float label_w = std::max(16.0f, body_w - (swatch + 4.0f));
    float content_h = 0.0f;
    for (const key_row& r : rows)
        content_h += std::max(row_h,
                              ImGui::CalcTextSize(r.label.c_str(), nullptr, false, label_w).y + 2.0f);
    if (rows.empty()) content_h = row_h;

    // Ask for chrome + whatever the rows want; the region caps it against the time
    // panel, so an expanded forty-nation list stops below the clock rather than over it.
    const float chrome = pad + combo_h + header_h + pad;
    const float want_h = chrome + (state.lens_key_open ? content_h : 0.0f);

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ui::shell_rect r = open_lens_chrome(dl, state, want_h, "##lens_key_blocker");

    const float x = r.x + pad;
    // Laid out from the FOOT upward: header on the minimap edge, combo above it, rows
    // above that. The rows take whatever the region actually granted, which is how the
    // cap reaches them without a second clamp.
    const float header_y = (r.y + r.h) - pad * 0.5f - header_h;
    const float combo_y  = header_y - combo_h;
    const float rows_top = r.y + pad * 0.5f;
    const float body_h   = std::max(0.0f, combo_y - rows_top);

    if (combo_state)
        draw_lens_resource_combo(*combo_state, {x, combo_y}, body_w);

    // Header row: a caret plus the title, and the whole bar is the toggle. The count
    // rides in the header so a collapsed legend still says how much it is hiding --
    // otherwise "Countries" gives the player no reason to open it. The caret points
    // the way the press will move the body: "^" opens upward, "v" closes it back down.
    {
        char title[96];
        std::snprintf(title, sizeof title, "%s  %s  (%d)",
                      state.lens_key_open ? "v" : "^", header,
                      static_cast<int>(rows.size()));
        dl->AddText({x, header_y}, IM_COL32(235, 235, 235, 255), title); // fit-exempt: header bar spans the chrome column

        // The hit rect spans the header band down to the box foot, not just the text
        // line: a press anywhere on the bar should work, and a narrow blind target is
        // one a script or a player misses.
        const float hit_top = header_y - pad * 0.5f;
        const float hit_h   = (r.y + r.h) - hit_top;
        ImGui::SetNextWindowPos({ r.x, hit_top }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ box_w2, hit_h }, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        constexpr ImGuiWindowFlags hflags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav   | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
        ImGui::Begin("##lens_key_toggle", nullptr, hflags);
        // Toggle rule (standing rules § Toggle rule): the control expresses an active
        // state, so pressing it while open closes it.
        if (ImGui::InvisibleButton("##lens_key_hit", { box_w2, hit_h }))
            state.lens_key_open = !state.lens_key_open;
        ImGui::End();
        ImGui::PopStyleVar();
    }

    if (!state.lens_key_open || body_h <= 0.0f)
        return;

    if (rows.empty())
    {
        dl->AddText({x, rows_top}, IM_COL32(170, 175, 185, 255), empty_note); // fit-exempt: legend box sized to its measured entries (container 2)
        return;
    }

    // Scrollable body: a borderless child overlaying the row area (the combo pattern),
    // so overflow scrolls with a clean scrollbar rather than overrunning the region.
    ImGui::SetNextWindowPos({ x, rows_top }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ body_w, body_h }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags wflags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav   | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin(id, nullptr, wflags);
    ImGui::BeginChild("##rows", { body_w, body_h }, false, ImGuiWindowFlags_NoBackground);
    ImDrawList* wdl = ImGui::GetWindowDrawList();
    for (const key_row& kr : rows)
    {
        const ImVec2 c = ImGui::GetCursorScreenPos();
        switch (kr.marker)
        {
            case key_marker::swatch:
                wdl->AddRectFilled(c, { c.x + swatch, c.y + swatch }, kr.marker_colour);
                break;
            case key_marker::dot:
                wdl->AddCircleFilled({ c.x + 4.0f, c.y + line_h * 0.5f }, 3.5f, kr.marker_colour);
                break;
            case key_marker::bar:
            {
                const float th = 3.0f + (bar_max - 3.0f) * std::clamp(kr.bar_frac, 0.0f, 1.0f);
                wdl->AddRectFilled({ c.x, c.y + line_h * 0.5f - 2.0f },
                                   { c.x + th, c.y + line_h * 0.5f + 2.0f }, kr.marker_colour);
                break;
            }
        }
        // The box does not widen to the longest name -- it is the width of the chrome
        // column -- so a long name WRAPS. AddText with a wrap_width takes the same path
        // ImGui uses for wrapped text, so the row grows by whatever it actually needed
        // and the Dummy below reserves exactly that.
        const float text_x = (kr.marker == key_marker::bar) ? (bar_max + 6.0f) : (swatch + 4.0f);
        const float wrap_w = std::max(16.0f, body_w - text_x);
        wdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), { c.x + text_x, c.y },
                     kr.label_colour, kr.label.c_str(), nullptr, wrap_w); // fit-exempt: wrapped to the column
        const float this_h = std::max(row_h,
                                      ImGui::CalcTextSize(kr.label.c_str(), nullptr, false, wrap_w).y + 2.0f);
        ImGui::Dummy({ body_w, this_h });
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
}

/// Legend for the Resource lens (Ben, 2026-10-04): a search box over a checklist of
/// the extractable resources present on the active body. A checked row shows its
/// identity swatch and is drawn on the map; the set holds at most six, so once full
/// the unchecked rows grey out. A good that cannot be extracted (a manufactured good,
/// computers) is never on a body's surface, so it is not offered at all.
void draw_resource_key(const world& w, ui_state& state)
{
    // The goods present on this body, in k_extractable order. Recomputed only when
    // the body changes: deposits are generation data and never move in play.
    // Keyed on the world and its tile count too: a new game can reuse a body id.
    static const world*               s_world = nullptr;
    static std::size_t                s_tiles = 0;
    static entity_id                  s_body  = null_entity;
    static std::vector<resource_type> s_present;
    if (s_body != state.active_body || s_world != &w || s_tiles != w.tiles.size())
    {
        s_world = &w;
        s_tiles = w.tiles.size();
        s_body  = state.active_body;
        s_present.clear();
        std::array<bool, resource_count> seen{};
        for (const auto& [tid, t] : w.tiles)
        {
            if (t.body != state.active_body)
                continue;
            for (std::size_t i = 0; i < resource_count; ++i)
                if (t.resource_deposit[i] > 0.0f)
                    seen[i] = true;
        }
        for (const resource_type r : placement_rules::k_extractable)
            if (seen[static_cast<std::size_t>(r)])
                s_present.push_back(r);
    }

    // Case-insensitive substring filter on the display name.
    const auto matches = [&](resource_type r) {
        const char* q = state.lens_resource_filter;
        if (q[0] == '\0')
            return true;
        std::string name   = presentation_of(r).name;
        std::string needle = q;
        for (char& ch : name)   ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        for (char& ch : needle) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return name.find(needle) != std::string::npos;
    };
    std::vector<resource_type> rows;
    for (const resource_type r : s_present)
        if (matches(r))
            rows.push_back(r);

    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float row_h  = ImGui::GetFrameHeightWithSpacing();
    const float head_h = line_h + 4.0f;
    const float note_h = line_h + 2.0f;
    const float want_h = pad + head_h + kLensComboH + 4.0f
                       + static_cast<float>(std::max<std::size_t>(rows.size(), 1)) * row_h
                       + note_h + pad;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ui::shell_rect r = open_lens_chrome(dl, state, want_h, "##lens_key_blocker");
    const float x  = r.x + pad;
    const float bw = r.w - 2.0f * pad;
    float       y  = r.y + pad * 0.5f;

    char head[64];
    std::snprintf(head, sizeof head, "Resource deposits  (%d/%d)",
                  static_cast<int>(state.lens_resources.size()), ui_state::k_lens_resource_cap);
    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), head); // fit-exempt: legend box sized to its measured entries (container 2)
    y += head_h;

    const float note_y = r.y + r.h - pad * 0.5f - note_h;
    const float list_h = std::max(0.0f, note_y - y);

    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({bw, list_h}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav    | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("##lens_key_resource_list", nullptr, flags);
    ImGui::SetNextItemWidth(bw);
    ImGui::InputTextWithHint("##lens_resource_search", "Search resources",
                             state.lens_resource_filter, sizeof state.lens_resource_filter);
    ImGui::Dummy({bw, 2.0f});
    ImGui::BeginChild("##rows", {bw, std::max(0.0f, list_h - kLensComboH - 4.0f)}, false,
                      ImGuiWindowFlags_NoBackground);
    const bool full = static_cast<int>(state.lens_resources.size()) >= ui_state::k_lens_resource_cap;
    if (rows.empty())
        ImGui::TextDisabled("%s", s_present.empty() ? "No deposits on this body" : "No match");
    for (const resource_type res : rows)
    {
        const bool on      = lens_resource_on(state, res);
        const bool blocked = full && !on;
        ImGui::PushID(static_cast<int>(res));
        ImGui::BeginDisabled(blocked);
        bool v = on;
        if (ImGui::Checkbox("##on", &v))
            toggle_lens_resource(state, res);
        ImGui::EndDisabled();
        ImGui::SameLine();
        const ImVec2 c   = ImGui::GetCursorScreenPos();
        ImDrawList*  wdl = ImGui::GetWindowDrawList();
        const float  sy  = c.y + (ImGui::GetFrameHeight() - 10.0f) * 0.5f;
        if (on)
            wdl->AddRectFilled({c.x, sy}, {c.x + 10.0f, sy + 10.0f}, presentation_of(res).colour);
        else
            wdl->AddRect({c.x, sy}, {c.x + 10.0f, sy + 10.0f}, IM_COL32(110, 115, 125, 255));
        ImGui::Dummy({14.0f, ImGui::GetFrameHeight()});
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        if (blocked)
            ImGui::TextDisabled("%s", presentation_of(res).name);
        else
            ImGui::TextUnformatted(presentation_of(res).name);
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();

    dl->AddText({x, note_y}, IM_COL32(170, 175, 185, 255), // fit-exempt: legend box sized to its measured entries (container 2)
                full ? "Full: six at most" : "Split tile = several here");
}

/// Key for the Corporation and Company lenses (BL-1240, owner multi-select;
/// LENSES.md § Corporation lens). The Resource lens's checklist shape: a header
/// with the picked count, a search box, and one row per owner of the lens's kind
/// holding ground on the active body — identity swatch filled when picked, empty
/// when not. A checkbox press toggles the owner in or out of the lens's own set
/// (`ui_state::lens_corps` / `lens_companies`). An owner with nothing on this body
/// earns no row; its pick, if any, stands and is counted in the header.
void draw_owner_key(const world& w, ui_state& state,
                    const std::unordered_map<entity_id, entity_id>& tile_to_corp)
{
    const bool background = (state.overlay == overlay_mode::company);

    // Owners of the admitted kind with ground here, with their tile count.
    // tile_to_corp is already scoped to the active body.
    std::unordered_map<entity_id, int> held;
    for (const auto& [tile, corp] : tile_to_corp)
    {
        const auto cit = w.corporations.find(corp);
        if (cit != w.corporations.end() && cit->second.is_background == background)
            ++held[corp];
    }
    std::vector<entity_id> present;
    present.reserve(held.size());
    for (const auto& [corp, n] : held)
        present.push_back(corp);
    // The player first, then by name, then by id — never the map's walk order.
    std::sort(present.begin(), present.end(), [&](entity_id a, entity_id b) {
        const bool pa = (a == w.player_entity), pb = (b == w.player_entity);
        if (pa != pb)
            return pa;
        const std::string& na = w.corporations.at(a).name;
        const std::string& nb = w.corporations.at(b).name;
        return na != nb ? na < nb : a < b;
    });

    const auto matches = [&](entity_id corp) {
        const char* q = state.lens_owner_filter(background);
        if (q[0] == '\0')
            return true;
        std::string name   = w.corporations.at(corp).name;
        std::string needle = q;
        for (char& ch : name)   ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        for (char& ch : needle) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return name.find(needle) != std::string::npos;
    };
    std::vector<entity_id> rows;
    for (const entity_id corp : present)
        if (matches(corp))
            rows.push_back(corp);

    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float row_h  = ImGui::GetFrameHeightWithSpacing();
    const float head_h = line_h + 4.0f;
    const float note_h = line_h + 2.0f;
    const float want_h = pad + head_h + kLensComboH + 4.0f
                       + static_cast<float>(std::max<std::size_t>(rows.size(), 1)) * row_h
                       + note_h + pad;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ui::shell_rect r = open_lens_chrome(dl, state, want_h, "##lens_key_blocker");
    const float x  = r.x + pad;
    const float bw = r.w - 2.0f * pad;
    float       y  = r.y + pad * 0.5f;

    char head[64];
    std::snprintf(head, sizeof head, "%s  (%d picked)",
                  background ? "Companies" : "Corporations",
                  static_cast<int>(state.lens_owner_set(background).size()));
    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), head); // fit-exempt: legend box sized to its measured entries (container 2)
    y += head_h;

    const float note_y = r.y + r.h - pad * 0.5f - note_h;
    const float list_h = std::max(0.0f, note_y - y);

    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({bw, list_h}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav    | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("##lens_key_owner_list", nullptr, flags);
    ImGui::SetNextItemWidth(bw);
    ImGui::InputTextWithHint(background ? "##lens_company_search" : "##lens_corp_search",
                             background ? "Search companies" : "Search corporations",
                             state.lens_owner_filter(background),
                             sizeof state.lens_corps_filter); // both filters are char[32]
    ImGui::Dummy({bw, 2.0f});
    ImGui::BeginChild("##rows", {bw, std::max(0.0f, list_h - kLensComboH - 4.0f)}, false,
                      ImGuiWindowFlags_NoBackground);
    if (rows.empty())
        ImGui::TextDisabled("%s", present.empty()
                                      ? (background ? "No companies on this body"
                                                    : "No corporations on this body")
                                      : "No match");
    for (const entity_id corp : rows)
    {
        const bool on = state.lens_owner_picked(corp, background);
        ImGui::PushID(static_cast<int>(corp));
        bool v = on;
        if (ImGui::Checkbox("##on", &v))
            state.toggle_lens_owner(corp, background);
        ImGui::SameLine();
        const ImVec2 c   = ImGui::GetCursorScreenPos();
        ImDrawList*  wdl = ImGui::GetWindowDrawList();
        const float  sy  = c.y + (ImGui::GetFrameHeight() - 10.0f) * 0.5f;
        const ImU32  col = palette::corp_identity_colour(corp, w.player_entity);
        if (on)
            wdl->AddRectFilled({c.x, sy}, {c.x + 10.0f, sy + 10.0f}, col);
        else
            wdl->AddRect({c.x, sy}, {c.x + 10.0f, sy + 10.0f}, col);
        ImGui::Dummy({14.0f, ImGui::GetFrameHeight()});
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        const std::string& name = w.corporations.at(corp).name;
        if (on)
            ImGui::TextUnformatted(name.c_str());
        else
            ImGui::TextDisabled("%s", name.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();

    dl->AddText({x, note_y}, IM_COL32(170, 175, 185, 255), // fit-exempt: legend box sized to its measured entries (container 2)
                "Grey = unpicked owner. Shift-click to pick");
}

/// On-canvas legend for the Market lens: a diverging cheap↔dear gradient bar plus
/// the selected good's name and its current price ratio (or an "untraded" note when
/// the body's market has no entry for it). The one lens chrome region, like every key.
// BL-015: market lens is now a catchment-boundary tint (one colour per market).
// The key shows a colour swatch per market labelled with its city name (matching the
// ledger / selection / CSV — never a bare ordinal, which the player never sees elsewhere).
void draw_market_key(const world& w, ui_state& state,
                     const std::unordered_map<entity_id, ImU32>& catchment_colours)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float swatch = line_h;
    // Stable legend order — the source map iterates arbitrarily; sort by market id so the
    // swatch list does not reshuffle frame to frame (colours stay keyed to their market).
    std::vector<std::pair<entity_id, ImU32>> entries(catchment_colours.begin(),
                                                     catchment_colours.end());
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Size the box to the widest label so long city names are not clipped. One
    // market_city_name call per market: the row keeps the string the width pass
    // measured (it used to be built twice per market per frame — BL-362).
    float label_w = ui::fit_width("Market catchments");
    std::vector<key_row> rows;
    rows.reserve(entries.size());
    for (const auto& [mid, col] : entries)
    {
        std::string name = market_city_name(w, mid);
        label_w = std::max(label_w, swatch + 4.0f + ui::fit_width(name.c_str()));
        rows.push_back({ col, IM_COL32(220, 220, 220, 255), std::move(name),
                         key_marker::swatch, 0.0f });
    }


    draw_scroll_list_key("##lens_key_market",
                         "Market catchments", rows, "No markets", &state, state);
}

// The Country lens's per-nation key (BL-133, draw_country_key) RETIRED with the
// lens (BL-601). A scroll-list of every nation on the body was the legend for a
// territory-wide tint — it answered "which colour is whose?" for a wash that no
// longer exists. The band answers it in place: a nation's colour sits on its own
// border, and hovering that border names the nation outright, so the read is at
// the thing rather than in a list beside it.

/// Legend for the Population lens: a low→high habitability gradient bar (dark substrate
/// → liveable green), in the one lens chrome region like every key.
void draw_population_key(ImDrawList* dl, const ui_state& state)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, state, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Workforce efficiency"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 4.0f;

    // Red→green rank bar (BL-135): mirrors the per-tile value-mark dot the lens
    // now draws instead of a full-tile tint — low efficiency reads red, full
    // labour reads green.
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = static_cast<float>(i) / segs;
        const ImU32 c  = ryg_colour(t0);
        dl->AddRectFilled({ x + bar_w * t0, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low"); // fit-exempt: legend box sized to its measured entries (container 2)
    const ImVec2 hts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - hts.x, y}, IM_COL32(170, 175, 185, 255), "high"); // fit-exempt: legend box sized to its measured entries (container 2)
}

/// Colour for tectonic plate @p index (Continent lens, BL-226). A dedicated
/// palette rather than palette::nation_colour, because plates are substrate and
/// not identity — but a *categorical* palette all the same, so the first draft's
/// near-monochrome mineral tones are deliberately abandoned: they blended into
/// one grey wash and the lens could not do the one job it has (tell one plate
/// from the next). These keep the earthy cast that separates them from the
/// nation wheel while carrying real hue AND lightness separation, alternating
/// light/dark so adjacent slots differ even in greyscale. Ten slots covers
/// run_continents' clamp of 4..10 mobile plates; the modulo is belt-and-braces.
ImU32 plate_colour(int index)
{
    static constexpr ImU32 table[10] = {
        IM_COL32(196, 152,  92, 255), // sandstone (light warm)
        IM_COL32( 62,  88, 110, 255), // deep slate (dark cool)
        IM_COL32(182, 108,  86, 255), // clay red (light warm)
        IM_COL32( 70, 110,  92, 255), // dark serpentine (dark cool)
        IM_COL32(206, 190, 128, 255), // pale marl (very light)
        IM_COL32( 96,  76, 116, 255), // dark porphyry (dark violet)
        IM_COL32(150, 176, 106, 255), // olivine (light green)
        IM_COL32( 58,  76,  84, 255), // basalt (very dark)
        IM_COL32(206, 146, 160, 255), // rose quartz (light pink)
        IM_COL32(104,  92,  60, 255), // dark ochre
    };
    return table[static_cast<std::size_t>(((index % 10) + 10) % 10)];
}

/// The Continent lens key. Unlike the gradient keys, this legend has no scale to
/// explain — the tint is categorical — so it explains the one thing that is not
/// self-evident: that the BRIGHT tiles are plate boundaries, and why they matter.
///
/// BL-376: the key keeps this position but is drawn on the FOREGROUND list by its
/// caller, so it floats over the always-open Selection band instead of being buried
/// by it. Purely a draw-order fix — nothing here moves. Because the band, not the
/// canvas, is now what sits underneath, the panel takes an opaque fill: the 210-alpha
/// default let the band's own background bleed through and muddy the plate swatches,
/// which are the one thing this key exists to show.
void draw_continent_key(ImDrawList* dl, const ui_state& state, const continent_state* plates)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float sw     = 11.0f; // swatch edge

    const float body_h = pad + line_h + 6.0f + sw + 4.0f + sw + 4.0f + line_h + pad;
    float x, y, inner_w;
    begin_lens_key(dl, state, body_h, pad, x, y, inner_w);
    (void)inner_w;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Tectonic plates"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 6.0f;

    if (!plates)
    {
        dl->AddText({x, y}, IM_COL32(170, 170, 180, 255), "No plate record"); // fit-exempt: legend box sized to its measured entries (container 2)
        y += line_h + 4.0f;
        dl->AddText({x, y}, IM_COL32(150, 150, 160, 255), "for this body."); // fit-exempt: legend box sized to its measured entries (container 2)
        return;
    }

    const int n = static_cast<int>(plates->plates.size());
    if (n <= 1)
    {
        dl->AddText({x, y}, IM_COL32(170, 170, 180, 255), "Stagnant lid:"); // fit-exempt: legend box sized to its measured entries (container 2)
        y += line_h + 4.0f;
        dl->AddText({x, y}, IM_COL32(150, 150, 160, 255), "one immobile plate."); // fit-exempt: legend box sized to its measured entries (container 2)
        return;
    }

    // A row of plate swatches — the count is the readable fact, not the identity
    // of any one plate, so they run as an unlabelled strip.
    float sx = x;
    for (int i = 0; i < n; ++i)
    {
        dl->AddRectFilled({ sx, y }, { sx + sw, y + sw }, plate_colour(i));
        sx += sw + 3.0f;
    }
    y += sw + 4.0f;

    // The boundary swatch carries the same white lift the map applies, so the
    // legend shows the actual treatment rather than describing it.
    dl->AddRectFilled({ x, y }, { x + sw, y + sw },
                      lerp_colour(plate_colour(0), IM_COL32(255, 255, 245, 255), 0.45f));
    dl->AddText({ x + sw + 6.0f, y - 1.0f }, IM_COL32(220, 220, 228, 255), "pale = boundary"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += sw + 4.0f;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d plates - uplift & rift", n);
    dl->AddText({x, y}, IM_COL32(170, 170, 180, 255), buf); // fit-exempt: legend box sized to its measured entries (container 2)
}

/// On-canvas legend for the Industry lens (BL-084; re-pointed BL-373): a low→high
/// amber gradient bar mapping the tile tint, so the field reads as "where the
/// industry I did not build is densest". Same placement as the others. (This
/// comment previously drifted onto plate_colour, which it does not describe.)
void draw_industry_key(ImDrawList* dl, const ui_state& state)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, state, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Background industry"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 4.0f;

    // Gradient mirrors the tile tint lerp (terrain hue -> industrial amber at
    // 0.15 + 0.6t), anchored on a neutral dark base so the bar reads standalone.
    constexpr ImU32 ind  = IM_COL32(210, 150, 70, 255); // industrial amber (tile pass)
    constexpr int   segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = static_cast<float>(i) / segs;
        const ImU32 c  = lerp_colour(IM_COL32(40, 40, 48, 255), ind, 0.15f + 0.6f * t0);
        dl->AddRectFilled({ x + bar_w * t0, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low"); // fit-exempt: legend box sized to its measured entries (container 2)
    const ImVec2 hts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - hts.x, y}, IM_COL32(170, 175, 185, 255), "high"); // fit-exempt: legend box sized to its measured entries (container 2)
}

/// The logistics hue this canvas already speaks: the Supply lens's convoy glyph
/// colour, reused so throughput and the convoys it admits read as one vocabulary
/// rather than two unrelated blues.
constexpr ImU32 k_throughput_hue = IM_COL32(80, 200, 255, 255);

/// The Throughput lens's FIELD ramp (BL-606): deep navy where throughput is
/// furthest away, the logistics cyan at an anchor. Sequential, not diverging —
/// capacity has a single good direction. @p t is 1 at an anchor and 0 at the
/// body's furthest reachable ground (or anywhere unreachable).
ImU32 throughput_field_colour(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return lerp_colour(IM_COL32(16, 24, 42, 255), k_throughput_hue, t);
}

/// The Throughput lens's ANCHOR ramp: a distinctly HOTTER, near-white cyan over
/// an anchor's share of the body's largest pool. Deliberately a different ramp
/// from the field's — an anchor mark drawn in the field's own colours vanishes
/// into it, which is exactly how the first cut's marks disappeared.
ImU32 throughput_anchor_colour(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return lerp_colour(IM_COL32(70, 150, 190, 255), IM_COL32(240, 252, 255, 255), t);
}

/// On-canvas legend for the Throughput lens (BL-606, LOGISTICS.md § Logistic
/// Points). A fixed-height GRADIENT-BAR key, so it takes the begin_lens_key
/// chrome flush-left of the minimap — but on ImGui's FOREGROUND draw list with
/// an OPAQUE fill, the way draw_continent_key does (BL-376). The six other
/// gradient keys still paint on the background list and are buried by the
/// always-open Selection band; reproducing that here would ship a key nobody
/// can read, so this one follows the fixed precedent rather than the majority.
///
/// It reports the two facts the ramp alone cannot: how many anchors generate on
/// this body, and how much they generate in total this tick. Degrades honestly —
/// a body with no anchor, or an authored rate of zero, says so instead of
/// drawing a scale over nothing.
void draw_throughput_key(ImDrawList* dl, const ui_state& state)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float mark_d = 11.0f; // anchor swatch diameter
    const bool  have   = !state.lp_anchors.empty() && state.lp_anchor_max > 0.0f;
    const float body_h = have
        ? pad + line_h + 4.0f + bar_h + 2.0f + line_h + 6.0f
              + std::max(line_h, mark_d) + 4.0f + line_h + pad
        : pad + line_h + 6.0f + line_h + 4.0f + line_h + pad;

    float x, y, bar_w;
    begin_lens_key(dl, state, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Throughput (active LP)"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + (have ? 4.0f : 6.0f);

    if (!have)
    {
        dl->AddText({x, y}, IM_COL32(170, 170, 180, 255), "No anchor generates"); // fit-exempt: legend box sized to its measured entries (container 2)
        y += line_h + 4.0f;
        dl->AddText({x, y}, IM_COL32(150, 150, 160, 255), "active LP on this body."); // fit-exempt: legend box sized to its measured entries (container 2)
        return;
    }

    // Bar 1 — the FIELD ramp: how far this ground is from the nearest generator.
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = static_cast<float>(i) / segs;
        dl->AddRectFilled({ x + bar_w * t0, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h },
                          throughput_field_colour(t0));
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "far"); // fit-exempt: legend box sized to its measured entries (container 2)
    const ImVec2 nts = ImGui::CalcTextSize("at anchor");
    dl->AddText({x + bar_w - nts.x, y}, IM_COL32(170, 175, 185, 255), "at anchor"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 6.0f;

    // Row 2 — the anchor MARK, carrying the LP quantity itself. Two ramps in one
    // key because the lens genuinely shows two things: where capacity is, and how
    // far the ground is from it. The mark is drawn here exactly as the map draws
    // it (ring, dark backing included) so the legend shows the treatment, not a
    // description of it.
    const float mr = mark_d * 0.5f;
    dl->AddCircle({x + mr + 1.0f, y + mr}, mr, IM_COL32(10, 18, 30, 220), 18, 4.0f);
    dl->AddCircle({x + mr + 1.0f, y + mr}, mr, throughput_anchor_colour(1.0f), 18, 2.0f);
    char per[48];
    std::snprintf(per, sizeof(per), "anchor: %.0f LP/tick", state.lp_anchor_max);
    dl->AddText({x + mark_d + 8.0f, y + (mark_d - line_h) * 0.5f}, // fit-exempt: legend box sized to its measured entries (container 2)
                IM_COL32(235, 235, 235, 255), per);
    y += std::max(line_h, mark_d) + 4.0f;

    char totals[64];
    std::snprintf(totals, sizeof(totals), "%d anchors - %.0f LP/tick total",
                  static_cast<int>(state.lp_anchors.size()), state.lp_anchor_total);
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), totals); // fit-exempt: legend box sized to its measured entries (container 2)
}

/// Legend for the Scarcity lens: an abundant→scarce gradient bar (no tint
/// → hot) plus the selected resource's name and swatch. Same placement as the others.
void draw_scarcity_key(ImDrawList* dl, ui_state& state)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + kLensComboH + 4.0f
                       + line_h + 4.0f + bar_h + 2.0f + line_h + 4.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, state, body_h, pad, x, y, bar_w);

    draw_lens_resource_combo(state, {x, y}, bar_w);
    y += kLensComboH + 4.0f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Market scarcity"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 4.0f;

    // Met (substrate, no tint) → scarce (hot). Mirrors the per-market composite.
    constexpr ImU32 substrate = IM_COL32(40, 40, 48, 255);
    constexpr ImU32 hot       = IM_COL32(220, 70, 55, 255);
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = static_cast<float>(i) / segs;
        const ImU32 c  = lerp_colour(substrate, hot, t0);
        dl->AddRectFilled({ x + bar_w * t0, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "met"); // fit-exempt: legend box sized to its measured entries (container 2)
    const ImVec2 sts = ImGui::CalcTextSize("scarce");
    dl->AddText({x + bar_w - sts.x, y}, IM_COL32(170, 175, 185, 255), "scarce"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 4.0f;

    dl->AddRectFilled({x, y + 2.0f}, {x + 10.0f, y + 12.0f},
                      presentation_of(state.lens_resource).colour);
    dl->AddText({x + 14.0f, y}, IM_COL32(235, 235, 235, 255), // fit-exempt: legend box sized to its measured entries (container 2)
                presentation_of(state.lens_resource).name);
}

/// Recency-tier colour shared by the Reach and Supply-routes keys, mirroring the
/// activity-fog convention (DISCOVERY.md, BL-089): fresh reads green, gone-cold
/// reads greyed. (No `visible` case here — a trade_route-only reach has no live
/// convoy/ownership signal to promote it past `known`.)
ImU32 reach_tier_colour(activity_vis tier)
{
    return (tier == activity_vis::known) ? palette::activity_known : palette::activity_stale;
}

/// A body this canvas's active body is connected to via a player `trade_route`,
/// tiered by recency. Shared shape for the Reach lens's pre-pass and key.
struct reach_link { entity_id other_body; activity_vis tier; };

/// One aggregated player trade lane touching the active body. Shared shape for
/// the Supply-routes lens's pre-pass and key.
struct supply_edge { entity_id other_body; int convoy_count; activity_vis tier; };

/// On-canvas legend for the Reach lens (BL-011): this canvas only ever shows the
/// active body's tile grid (never other bodies), so the "highlight the connected
/// bodies" the design calls for reads here as a list of the active body's own
/// connections (name + recency tier) — the body-marker glow the design describes
/// belongs on the Solar canvas, out of this lens work's file scope.
void draw_reach_key(const world& w, const std::vector<reach_link>& links, ui_state& state)
{
    std::vector<key_row> rows;
    rows.reserve(links.size());
    for (const reach_link& link : links)
    {
        const auto  it   = w.bodies.find(link.other_body);
        const char* name = (it != w.bodies.end()) ? it->second.name.c_str() : "unknown body";
        const ImU32 c    = reach_tier_colour(link.tier);
        rows.push_back({ c, c, name, key_marker::dot, 0.0f });
    }
    draw_scroll_list_key("##lens_key_reach",
                         "Reach (your trade network)", rows,
                         "no routes from this body", nullptr, state);
}

/// On-canvas legend for the Supply-routes lens (BL-014): one row per aggregated
/// lane touching the active body — a thickness bar log-scaled from convoy_count
/// stands in for the edge-thickness encoding the design specifies for the
/// (out-of-scope-here) Solar-canvas graph rendering, and colour is the shared
/// recency tier.
void draw_supply_routes_key(const world& w, const std::vector<supply_edge>& edges,
                            ui_state& state)
{
    std::vector<key_row> rows;
    rows.reserve(edges.size());
    for (const supply_edge& edge : edges)
    {
        const auto  it   = w.bodies.find(edge.other_body);
        const char* name = (it != w.bodies.end()) ? it->second.name.c_str() : "unknown body";
        const ImU32 c    = reach_tier_colour(edge.tier);
        // Log-scaled thickness (BL-014, settled): a bare completion reads as a thin
        // sliver; heavy repeat traffic saturates toward the bar's full length rather
        // than dominating the row linearly. Expressed as a [0,1] fraction of bar_max
        // (37 = bar_max 40 − base 3) so the shared renderer paints the bar.
        const float frac = std::clamp(6.0f * std::log(1.0f + static_cast<float>(edge.convoy_count)) / 37.0f,
                                      0.0f, 1.0f);
        rows.push_back({ c, c, name, key_marker::bar, frac });
    }
    draw_scroll_list_key("##lens_key_supply",
                         "Supply routes", rows, "no lanes from this body", nullptr, state);
}

/// --- Trade-flow lens (BL-1222; LENSES.md § Trade-flow lens) -------------------
/// The refusal classes' on-screen names, indexed by `trade_refusal` up to `room`:
/// the CORPORATION dispatcher's rules, worst first (LENSES.md § Trade-flow lens).
/// The market-viability skill's logistics row classifies MARKET SHELF exports, a
/// different dispatcher with its own order and its own labels; these are not those.
constexpr int k_trade_refusal_count = 8;
constexpr const char* k_trade_refusal_name[k_trade_refusal_count] = {
    "no lane", "price gate", "no route", "costly",
    "no propellant", "no room", "no funds", "room",
};
static_assert(static_cast<int>(trade_refusal::room) + 1 == k_trade_refusal_count,
              "one name per refusal class");
static_assert(std::size(palette::trade_refusal_colour) == k_trade_refusal_count,
              "one colour per refusal class");

/// Arrow stroke width for a flow of @p rate units/tick against the body's
/// heaviest flow @p max_rate: 1.5 px for a trickle, 7 px at the heaviest.
float trade_flow_width(float rate, float max_rate)
{
    const float t = (max_rate > 0.0f) ? std::clamp(rate / max_rate, 0.0f, 1.0f) : 0.0f;
    return 1.5f + 5.5f * t;
}

/// The Trade-flow lens key: the class ramp (colour -> class, worst first) and the
/// flow-width scale in units per tick. A fixed-height key, so it takes the
/// begin_lens_key chrome and draws open.
void draw_trade_flow_key(ImDrawList* dl, const ui_state& state, float max_rate, int flows,
                         int markers)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float row_h  = line_h + 2.0f;
    const float sw     = 10.0f;

    const float body_h = pad + line_h + 4.0f + line_h + 2.0f
                       + static_cast<float>(k_trade_refusal_count) * row_h + 6.0f
                       + line_h + 2.0f + 3.0f * row_h + pad;
    float x, y, inner_w;
    begin_lens_key(dl, state, body_h, pad, x, y, inner_w);
    (void)inner_w;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Trade flow (your surplus)"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 4.0f;

    char buf[96];
    std::snprintf(buf, sizeof buf, "Not sent, best reason (%d)", markers);
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), buf); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 2.0f;
    for (int i = 0; i < k_trade_refusal_count; ++i)
    {
        const ImVec2 c{ x + sw * 0.5f + 1.0f, y + line_h * 0.5f };
        dl->AddCircleFilled(c, sw * 0.5f + 1.5f, IM_COL32(10, 12, 18, 230), 12);
        dl->AddCircleFilled(c, sw * 0.5f, palette::trade_refusal_colour[i], 12);
        dl->AddText({x + sw + 8.0f, y}, IM_COL32(220, 220, 228, 255), k_trade_refusal_name[i]); // fit-exempt: legend box sized to its measured entries (container 2)
        y += row_h;
    }
    y += 6.0f;

    std::snprintf(buf, sizeof buf, "Shipped, units/tick (%d)", flows);
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), buf); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 2.0f;
    const float fracs[3] = { 1.0f, 0.5f, 0.1f };
    for (const float f : fracs)
    {
        const float wpx = trade_flow_width(f * max_rate, max_rate);
        const float ly  = y + line_h * 0.5f;
        dl->AddLine({x, ly}, {x + 36.0f, ly}, palette::trade_flow_arrow, wpx);
        if (max_rate > 0.0f)
            std::snprintf(buf, sizeof buf, "%.3g", f * max_rate); // 3 significant: a trickle reads, not 0.0
        else
            std::snprintf(buf, sizeof buf, "%s", "no shipments");
        dl->AddText({x + 44.0f, y}, IM_COL32(220, 220, 228, 255), buf); // fit-exempt: legend box sized to its measured entries (container 2)
        y += row_h;
        if (!(max_rate > 0.0f))
            break;
    }
}

/// BL-362: bumps whenever the logistics caches were cleared since the last look.
/// invalidate_logistics_caches fires on every build/demolish/completion/road event,
/// so a shrink of world.astar_cost_cache is the one cheap signal that covers them
/// all; growth (ordinary path queries filling the cache) never bumps.
std::uint32_t logistics_generation(const world& w)
{
    static std::size_t   last_size = 0;
    static std::uint32_t gen       = 0;
    const std::size_t size = w.astar_cost_cache.size();
    if (size < last_size) ++gen;
    last_size = size;
    return gen;
}

/// BL-362 rebuild stamp for the per-frame derived views (vision model, marker
/// maps): their inputs move only on a body switch, an econ/day tick, a building
/// or convoy change, or a logistics invalidation — never mid-frame.
struct body_frame_stamp
{
    entity_id     body      = null_entity;
    int           day_tick  = -1;
    std::size_t   buildings = 0;
    std::size_t   convoys   = 0;
    std::size_t   units     = 0; // BL-575: invalidates the unit-marker groups on hire/disband; a march ORDER doesn't move a unit until a tick advances, which day_tick already catches.
    std::uint32_t logi_gen  = ~0u; // default never matches a live stamp
    bool operator==(const body_frame_stamp&) const = default;
};

body_frame_stamp make_body_frame_stamp(const world& w, entity_id body)
{
    return { body, w.current_day_tick, w.buildings.size(), w.convoys.size(),
             w.units.size(), logistics_generation(w) };
}

/// Nearest-wins marker hit resolution shared by hover and click (BL-031/BL-059):
/// unit outranks building outranks market_centre within the glyph radii; the
/// tile fallback stays with the callers. Unit was added above building by
/// BL-575, matching the repeat-click cycle's own order (Battle -> Soldier ->
/// Building -> Province — SELECTION.md § Tile repeat-click selection cycle):
/// a unit standing on a built tile must be reachable on the FIRST click, not
/// only after cycling past the building. Extracted so the two paths cannot
/// drift (BL-362).
entity_id resolve_marker_hit(const std::vector<marker_hit_zone>& zones, float mx, float my)
{
    float     best_unit_d2 = std::numeric_limits<float>::max();
    float     best_bld_d2  = std::numeric_limits<float>::max();
    float     best_mkt_d2  = std::numeric_limits<float>::max();
    entity_id unit = null_entity;
    entity_id bld  = null_entity;
    entity_id mkt  = null_entity;
    for (const marker_hit_zone& hz : zones)
    {
        const float dx = mx - hz.centre.x;
        const float dy = my - hz.centre.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 > hz.radius * hz.radius)
            continue;
        if (hz.kind == marker_hit_zone::kind::unit && d2 < best_unit_d2)
        {
            best_unit_d2 = d2;
            unit         = hz.id;
        }
        else if (hz.kind == marker_hit_zone::kind::building && d2 < best_bld_d2)
        {
            best_bld_d2 = d2;
            bld         = hz.id;
        }
        else if (hz.kind == marker_hit_zone::kind::market_centre && d2 < best_mkt_d2)
        {
            best_mkt_d2 = d2;
            mkt         = hz.id;
        }
    }
    return unit != null_entity ? unit : (bld != null_entity ? bld : mkt);
}

/// Nearest-wins STRUCTURE hit resolution (BL-601) — resolve_marker_hit's
/// region-grain sibling, and deliberately the general case rather than a nation
/// lookup: a boundary segment carries its own kind and its own corridor width,
/// so a plate rim or a catchment edge joins by producing zones, not by editing
/// this. BL-603 generalises the pattern.
///
/// A point-to-SEGMENT distance, not point-to-point: the target is a line, and a
/// line is only clickable if it is given a corridor. `half_width` is that
/// corridor, and it has nothing to do with how thick the border is drawn.
///
/// @param zones      This frame's boundary segments.
/// @param mx,my      Cursor position, screen px.
/// @param out_kind   Optional; receives the winning zone's kind.
/// @return           The closest structure whose corridor contains the cursor,
///                   or null_entity.
/// BL-603 — which STRUCTURE is this tile part of, as the ACTIVE LENS defines it?
///
/// The area counterpart to `resolve_structure_hit`, which resolves a structure by
/// its boundary. A catchment has no boundary the player aims at — they aim at
/// ground inside it — so this asks the tile instead.
///
/// LENSES.md's routing table has said since 2026-06-15 that "the active lens does
/// not only re-skin the canvas: it DEFINES what the pointer resolves to". This is
/// that rule at the grain the lens actually DRAWS. A lens whose subject is the
/// tile itself (Population, Industry) returns none and keeps tile grain — the
/// selection grain follows the drawing, which is the whole rule.
///
/// A structure USED TO have to be an entity here, because that is what a selection
/// is, and the Resource lens's deposit and the Continent lens's plate are regions
/// with no entity id — so both were absent rather than half-supported, on the
/// grounds that highlighting a region the player then cannot select promises a
/// pivot that does not arrive.
///
/// BOTH NOW RESOLVE (Ben, 2026-08-28, choosing option A over giving them real
/// entity ids in world/). They travel in `ui_state::selected_deposit_resource`
/// and `selected_plate` rather than in `selected_entity`, exactly as the battle
/// triple (BL-469) and `selected_province` (BL-511) do — the sanctioned shape in
/// this codebase for a selection whose subject is not an entity. `out_id` carries
/// a synthetic key for them (index + 1), which no caller may treat as an entity:
/// every comparison pairs it with the kind, and the click path dispatches on the
/// kind before it ever reaches `selected_entity`.
///
/// @param tile_to_corp This frame's tile→owning-corp index, already built by the
///                     caller for the Corporation lens's own tint.
structure_kind lens_structure_of_tile(const world& w, const ui_state& state,
                                      entity_id tile,
                                      const std::unordered_map<entity_id, entity_id>& tile_to_corp,
                                      entity_id* out_id,
                                      const continent_state* plates = nullptr,
                                      int grid_w = 0,
                                      const ImVec2* pointer = nullptr)
{
    if (out_id != nullptr)
        *out_id = null_entity;
    if (tile == null_entity)
        return structure_kind::none;

    switch (state.overlay)
    {
    case overlay_mode::market:
    case overlay_mode::scarcity:
    {
        // Both lenses draw the catchment: Market tints it, Scarcity blocks it.
        // Same structure, so the same pivot — and `market_for_tile` is the very
        // function the two fills already key on, so the highlight can never
        // disagree with the wash it sits under.
        const entity_id m = market_for_tile(w, tile);
        if (m == null_entity)
            return structure_kind::none;
        if (out_id != nullptr)
            *out_id = m;
        return structure_kind::market;
    }
    case overlay_mode::resource:
    {
        // The deposit as DRAWN: every tile whose selected-resource deposit is
        // non-zero. Not a flood fill — the lens itself does not do one ("any tile
        // carrying the resource is part of the deposit", LENSES.md), and the
        // selection grain must follow the drawing or the highlight would disagree
        // with the wash under it. So the whole resource on this body is one
        // structure, and its key is the resource index.
        //
        // A SPLIT tile belongs to several deposits (Ben, 2026-10-04: a toggled set,
        // pie wedges). The press resolves to the wedge under @p pointer (offset from
        // the hex centre); with no pointer it takes the first in toggle order.
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end())
            return structure_kind::none;
        resource_type on_tile[ui_state::k_lens_resource_cap];
        const int n = lens_resources_on_tile(state, tit->second, on_tile);
        if (n == 0)
            return structure_kind::none;
        const int k = (pointer != nullptr) ? lens_wedge_at(*pointer, n) : 0;
        if (out_id != nullptr)
            *out_id = static_cast<entity_id>(on_tile[k]) + 1u; // synthetic; +1 keeps 0 = none
        return structure_kind::deposit;
    }
    case overlay_mode::continent:
    {
        // The plate as drawn: the Voronoi region of `plate_id`. A stagnant-lid
        // body has one plate owning everything, which is a legitimate answer
        // rather than a degenerate one — the key is still stable.
        if (plates == nullptr || plates->plate_id.empty() || grid_w <= 0)
            return structure_kind::none;
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end())
            return structure_kind::none;
        const std::size_t idx =
            static_cast<std::size_t>(tit->second.grid_y) * static_cast<std::size_t>(grid_w)
            + static_cast<std::size_t>(tit->second.grid_x);
        if (idx >= plates->plate_id.size())
            return structure_kind::none;
        const int pid = plates->plate_id[idx];
        if (pid < 0)
            return structure_kind::none;
        if (out_id != nullptr)
            *out_id = static_cast<entity_id>(pid) + 1u; // synthetic; +1 keeps 0 = none
        return structure_kind::plate;
    }
    case overlay_mode::corporation:
    case overlay_mode::company:
    {
        // THE OWNER'S TILE GROUP — every tile it holds on this body, lit and
        // selected as one (Ben, 2026-08-28: "hovering one tile displays an
        // outline around all company buildings for that corporation/company").
        //
        // THIS CASE WAS DELIBERATELY ABSENT UNTIL BL-665, and the reason it was
        // absent is worth keeping because it was MEASURED, not reasoned: the
        // structure is exactly the set of tiles carrying that owner's buildings,
        // so every tile in it also carries a MARKER — and a marker outranked a
        // structure, so an area pivot here could never fire. The first draft of
        // this switch included it and the check reported `tile` on empty ground
        // and `building` on owned ground, never `corporation`. The lens resolved
        // through the marker instead, as a hand-wired special case in the click
        // handler.
        //
        // BL-664 removed the marker's precedence under a lens, which dissolves
        // the objection: the area resolver is now reachable on owned ground
        // whether or not a glyph sits on it, and the special case is deleted.
        //
        // Keyed to the TINT (compute_tile_fill), because the selection grain
        // follows the drawing: Corporation admits corporations proper — the
        // player and its rivals — and Company admits BL-365 background firms,
        // neither showing the other's. A tile no firm of the admitted kind holds
        // answers none, and is inert.
        const auto oit = tile_to_corp.find(tile);
        if (oit == tile_to_corp.end())
            return structure_kind::none;
        const auto cit = w.corporations.find(oit->second);
        if (cit == w.corporations.end())
            return structure_kind::none;
        const bool want_background = (state.overlay == overlay_mode::company);
        if (cit->second.is_background != want_background)
            return structure_kind::none;
        if (out_id != nullptr)
            *out_id = oit->second;
        // Distinct KINDS for the two, not one kind and a lookup at the routing
        // site: they are different words since the 2026-08-28 split and Ben
        // ruled they reach different destinations, so the difference belongs in
        // the answer rather than being re-derived by every caller.
        return want_background ? structure_kind::company : structure_kind::corporation;
    }
    default:
        return structure_kind::none;
    }
}

entity_id resolve_structure_hit(const std::vector<structure_hit_zone>& zones,
                                float mx, float my,
                                structure_kind* out_kind = nullptr)
{
    float     best_d2 = std::numeric_limits<float>::max();
    entity_id best    = null_entity;
    for (const structure_hit_zone& hz : zones)
    {
        const float ax = hz.b.x - hz.a.x;
        const float ay = hz.b.y - hz.a.y;
        const float len2 = ax * ax + ay * ay;
        float t = 0.0f;
        if (len2 > 0.0f)
            t = std::clamp(((mx - hz.a.x) * ax + (my - hz.a.y) * ay) / len2, 0.0f, 1.0f);
        const float dx = mx - (hz.a.x + ax * t);
        const float dy = my - (hz.a.y + ay * t);
        const float d2 = dx * dx + dy * dy;
        if (d2 > hz.half_width * hz.half_width || d2 >= best_d2)
            continue;
        best_d2 = d2;
        best    = hz.id;
        if (out_kind != nullptr)
            *out_kind = hz.kind;
    }
    return best;
}

} // namespace

void update_body_throughput(world& w, ui_state& state, const recipe_registry& reg)
{
    state.lp_anchors.clear();
    state.lp_anchor_total = 0.0f;
    state.lp_anchor_max   = 0.0f;
    state.lp_reach_max    = 0.0f;
    state.lp_reach_p90    = 0.0f;

    // Off by default in every sense: no lens, no work. The lens is the only reader,
    // and the pools must not be built when nobody is looking at them — LP is a rate,
    // and a rate nobody consumes is exactly the write-only accumulator LOGISTICS.md
    // rule 3 deleted military_points for.
    if (state.overlay != overlay_mode::throughput || state.active_body == null_entity)
        return;

    // Fresh every call, never cached and never stored on `world` — the contract
    // active_lp_anchor_pools states in its own doc comment. `world&` is needed
    // only because it may populate the RASTER INDEX cache (body_tile_grid); it
    // runs no Dijkstra and no A*, so the cost of a miss here is one grid build,
    // not a reach-field rebuild. The reach field the tile pass reads is warmed
    // separately by app.cpp, one call before the draw.
    const std::unordered_map<entity_id, float> pools =
        active_lp_anchor_pools(w, state.active_body, reg.military().active_lp_per_anchor_tick);

    state.lp_anchors.reserve(pools.size());
    for (const auto& [tile, lp] : pools)
        state.lp_anchors.push_back({ tile, lp });

    // SORT BEFORE REDUCING. `pools` is an unordered_map, so both the legend's
    // total and the draw's lower_bound would otherwise depend on hash layout —
    // float addition is not associative, and a legend whose number moves with
    // the allocator is not a legend.
    std::sort(state.lp_anchors.begin(), state.lp_anchors.end(),
              [](const ui_state::lp_anchor& a, const ui_state::lp_anchor& b) {
                  return a.tile < b.tile;
              });
    for (const ui_state::lp_anchor& a : state.lp_anchors)
    {
        state.lp_anchor_total += a.lp;
        state.lp_anchor_max = std::max(state.lp_anchor_max, a.lp);
    }

    // The field ramp's denominator. `body_reach_field` is a CACHE HIT here — the
    // caller warms it for this same body one line earlier — so this is a vector
    // read, not a Dijkstra, and the const draw below still triggers nothing.
    // Max over a vector is order-independent, so it is deterministic where a
    // float sum over a hash map would not be.
    const std::vector<float>& reach = body_reach_field(w, state.active_body);
    std::vector<float> finite;
    finite.reserve(reach.size());
    for (const float c : reach)
        if (c >= 0.0f && !std::isinf(c))
        {
            state.lp_reach_max = std::max(state.lp_reach_max, c);
            finite.push_back(c);
        }

    // The 90th percentile, which is what the field ramp actually divides by.
    // nth_element rather than a full sort: this runs once per lens-active frame
    // over ~31k values, and the ramp needs one order statistic, not an ordering.
    // Deterministic despite nth_element's unspecified partial order, because only
    // the value AT the position is read — and equal values are equal.
    if (!finite.empty())
    {
        const std::size_t k = (finite.size() * 9) / 10;
        std::nth_element(finite.begin(), finite.begin() + static_cast<std::ptrdiff_t>(k),
                         finite.end());
        state.lp_reach_p90 = finite[k];
    }
}

void update_body_vision(world& w, ui_state& state, double now_days)
{
    state.sim_now_days = now_days;

    // BL-362: the flood fills, market_for_tile scans and A* corridor walks below
    // depend only on the stamp's inputs — skip the rebuild while none has moved.
    static body_frame_stamp s_vision_stamp;
    const body_frame_stamp stamp = make_body_frame_stamp(w, state.active_body);
    if (stamp == s_vision_stamp)
        return;
    s_vision_stamp = stamp;

    state.permanent_vision.clear();
    state.convoy_beams.clear();

    const entity_id body = state.active_body;
    if (body == null_entity) return;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end()) return;
    const int gw = std::max(1, bit->second.grid_width);
    const int gh = std::max(1, bit->second.grid_height);
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (static_cast<int>(grid.size()) < gw * gh) return;

    // Flood `radius` hops out from `seeds` into `out` (odd-r neighbours, column wrap).
    auto flood = [&](const std::vector<entity_id>& seeds, int radius,
                     std::unordered_set<entity_id>& out) {
        std::vector<entity_id> frontier;
        for (const entity_id s : seeds)
            if (s != null_entity && out.insert(s).second) frontier.push_back(s);
        for (int r = 0; r < radius && !frontier.empty(); ++r)
        {
            std::vector<entity_id> next;
            for (const entity_id tid : frontier)
            {
                const auto tit = w.tiles.find(tid);
                if (tit == w.tiles.end()) continue;
                const tile_component& t = tit->second;
                const int (*off)[2] = hex_neighbors::offsets(t.grid_y);
                for (int k = 0; k < 6; ++k)
                {
                    const int nrow = t.grid_y + off[k][1];
                    if (nrow < 0 || nrow >= gh) continue;
                    int ncol = (t.grid_x + off[k][0]) % gw;
                    if (ncol < 0) ncol += gw;
                    const entity_id nb = grid[static_cast<std::size_t>(nrow) * gw + ncol];
                    if (nb != null_entity && out.insert(nb).second) next.push_back(nb);
                }
            }
            frontier.swap(next);
        }
    };

    // The player's building tiles on this body + the markets whose catchment they sit
    // in — the markets the corp operates in (stable; buildings don't move). The centre
    // of operation is the lowest-id player building tile on the body (mirrors
    // market_clearing's file-local representative_tile, which is not header-visible).
    std::vector<entity_id> player_building_tiles;
    std::unordered_set<entity_id> operated_markets;
    entity_id centre         = null_entity;
    entity_id centre_building = null_entity;
    const auto pit = w.corporations.find(w.player_entity);
    if (pit != w.corporations.end())
    {
        for (const entity_id bid : pit->second.assets)
        {
            const auto bld = w.buildings.find(bid);
            if (bld == w.buildings.end()) continue;
            const entity_id t = bld->second.tile;
            const auto tit = w.tiles.find(t);
            if (tit == w.tiles.end() || tit->second.body != body) continue;
            player_building_tiles.push_back(t);
            const entity_id mid = market_for_tile(w, t);
            if (mid != null_entity) operated_markets.insert(mid);
            if (centre_building == null_entity || bid < centre_building)
            {
                centre_building = bid;
                centre          = t;
            }
        }
    }

    // Layer 1 (permanent): radius-2 pockets around the player's own installations —
    // your buildings are always visible.
    flood(player_building_tiles, 2, state.permanent_vision);

    // Layer 2 (permanent): 3-wide corridors from the corp centre of operation to each
    // market centre it operates in. A 3-wide corridor is the path tiles flooded one hop
    // to each side.
    if (centre != null_entity)
    {
        for (const entity_id mid : operated_markets)
        {
            const auto mk = w.markets.find(mid);
            if (mk == w.markets.end()) continue;
            const entity_id mc = mk->second.centre_tile;
            if (mc == null_entity) continue;
            const logistics_path& lp = intra_body_path(w, body, centre, mc);
            if (!lp.reachable || lp.tiles.empty()) continue;
            flood(lp.tiles, 1, state.permanent_vision);
        }
    }

    // Layer 3 (moving): the tile path + progress/speed of each live player intra-body
    // convoy, oriented src→dst. The renderer interpolates a head along it and trails a
    // dimming tail one econ tick's travel behind.
    for (const auto& cv : w.convoys)
    {
        if (cv.corp != w.player_entity) continue;
        // BL-458: the endpoint resolution, the A* lookup and the lo->hi orientation
        // flip used to live inline here. They are now convoy_route_tiles
        // (world/logistics.hpp), because interdiction has to ask the SAME question
        // ("which tile is this convoy on?") and a second private copy of the
        // orientation rule would be a silent, unrenderable divergence.
        convoy_route route = convoy_route_tiles(w, cv);
        if (route.body != body || route.tiles.empty()) continue;
        state.convoy_beams.push_back(
            { std::move(route.tiles), std::clamp(cv.progress, 0.0f, 1.0f), std::max(cv.speed, 0.0f) });
    }
}

void draw_body_surface_canvas(const world& w, ui_state& state, const recipe_registry& reg,
                              const economy_report& report, const generation_report& gen,
                              ImVec2 origin, ImVec2 size,
                              bool input_enabled)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    pass_meter().start(dl);

    dl->AddRectFilled(origin, origin + size, IM_COL32(18, 18, 24, 255));

    // The surface canvas is the bottom rung of the ladder, so it is only ever
    // drawn as the primary view — never the minimap.
    const float min_dim    = std::min(size.x, size.y);
    const bool  draw_title = min_dim > 320.0f;

    auto body_it = w.bodies.find(state.active_body);
    if (body_it == w.bodies.end())
    {
        if (draw_title)
        {
            const char* msg = "No body selected";
            const ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText(origin + (size - ts) * 0.5f, IM_COL32(150, 150, 150, 255), msg); // fit-exempt: legend box sized to its measured entries (container 2)
        }
        return;
    }

    const body_component& body = body_it->second;

    // BL-408 spectator god view: one draw-time flag for every survey-mask read
    // in this pass. Only ever true under spectate — the pair is the gate — so a
    // played session evaluates a single && and renders byte-identical to before.
    const bool god_view_lift = state.spectating && state.god_view;

    float title_h = 0.0f;
    if (draw_title)
    {
        title_h = ImGui::GetTextLineHeightWithSpacing();
        // Survey status suffix (BL-067): a surveyed body shows none; others read
        // their phase so the locked surface is explained in the header.
        char survey_note[48] = "";
        switch (body.survey.phase)
        {
            case survey_phase::hidden:
                std::snprintf(survey_note, sizeof survey_note, "  -  UNSURVEYED"); break;
            case survey_phase::in_transit:
                std::snprintf(survey_note, sizeof survey_note, "  -  Survey en route"); break;
            case survey_phase::scanning:
                std::snprintf(survey_note, sizeof survey_note, "  -  Surveying %d/%d",
                    body.survey.regions_done, body.survey.regions_total); break;
            case survey_phase::surveyed: break;
        }
        char title[176];
        std::snprintf(title, sizeof(title), "%s  -  %s  (%dx%d)%s",
            body.name.c_str(), body_type_name(body.type), body.grid_width, body.grid_height, survey_note);
        dl->AddText(origin + ImVec2{4.0f, 2.0f}, IM_COL32(235, 235, 235, 255), title); // fit-exempt: legend box sized to its measured entries (container 2)
    }

    const int gw = std::max(1, body.grid_width);
    const int gh = std::max(1, body.grid_height);

    // Grid occupies the region below the title bar.
    const ImVec2 grid_area_origin = origin + ImVec2{0.0f, title_h};
    const ImVec2 grid_area_size   = { size.x, size.y - title_h };
    const ImVec2 canvas_centre    = grid_area_origin + grid_area_size * 0.5f;

    // --- Hex size at zoom=1 ---
    // zoom=1 is defined as "the full grid height fills the canvas height." The
    // grid width will typically exceed the canvas at this scale — the player pans
    // horizontally. This definition makes zoom=4/3 mean exactly "3/4 of the grid
    // height is visible" regardless of canvas size or grid aspect ratio, so the
    // same zoom value reads correctly on both the primary view and the minimap.
    const float fit_by_y = grid_area_size.y / (1.5f * static_cast<float>(gh) + 0.5f);
    const float hex_size = fit_by_y * kFitMargin;

    // Grid centre in local (unzoomed) world space — used to centre the grid
    // on the canvas at zoom=1 with no pan.
    const float col_step = kSqrt3 * hex_size;
    const float row_step = 1.5f * hex_size;
    const float grid_cx  = (static_cast<float>(gw) - 0.5f) * col_step * 0.5f;
    const float grid_cy  = static_cast<float>(gh - 1) * row_step * 0.5f;

    // --- View transform (pan/zoom) ---
    // The surface canvas is always primary, so pan and zoom always apply.
    const float  zoom        = std::clamp(state.planetary_zoom, kMinZoom, kMaxZoom);

    // Pending centre request (verify.center_tile): now that the exact grid
    // transform is in hand, set the pan that places the requested tile's local
    // centre on the canvas centre — pan = (grid_centre − tile_local) · zoom. This
    // is the one place the centring math lives; Lua callers never replicate it.
    if (state.planetary_center_pending)
    {
        const ImVec2 lc = hex_local_centre(state.planetary_center_col,
                                           state.planetary_center_row, hex_size);
        state.planetary_pan_x = (grid_cx - lc.x) * zoom;
        state.planetary_pan_y = (grid_cy - lc.y) * zoom;
        state.planetary_center_pending = false;
        // Publish where that tile just landed on screen (BL-521). The transform is
        // only fully known here, so this is the one honest source for "the screen
        // point that is tile (col,row)" — the verify harness's click injection
        // reads it instead of re-deriving title_h / hex_size / pan in Lua.
        state.planetary_center_screen = canvas_centre;
    }

    const ImVec2 view_origin = ImVec2{ canvas_centre.x + state.planetary_pan_x,
                                       canvas_centre.y + state.planetary_pan_y };

    // Local world space → screen space.
    auto to_screen = [&](ImVec2 lp) -> ImVec2 {
        return {
            view_origin.x + (lp.x - grid_cx) * zoom,
            view_origin.y + (lp.y - grid_cy) * zoom,
        };
    };

    // Rebuild per-frame marker hit-zone list (BL-059). Cleared here so every draw
    // call starts fresh; the tile loop and the market-centre pass below fill it.
    state.marker_hit_zones.clear();
    // Same contract at STRUCTURE grain (BL-601): the national border pass in the
    // tile loop fills this with one corridor segment per drawn frontier edge.
    state.structure_hit_zones.clear();

    // Selection reconciliation. `selected_entity` has many writers (ledger rows,
    // the corporation list, "inspect the thing I just built"); this canvas owns
    // the two fields that hang off it.
    //
    // BL-598: `selected_province` is no longer a rival selection to be CLEARED
    // here — it is the province of whatever tile is selected, so this arm
    // RE-DERIVES it. That is what lets a tile selected from somewhere other than
    // the canvas (a ledger row, the verify harness) still get its province
    // outline, without adding a set-the-mirror duty to every selecting surface.
    // `province_of` returns 0 for anything that is not a tile, so a building or
    // unit selection lands on 0 without a kind test.
    if (state.selected_entity != state.province_sync_entity)
    {
        state.selected_province    = w.provinces.province_of(state.selected_entity);
        // BL-469: the battle selection is a THIRD channel into the same one
        // element, so it clears on the same reconciliation. Without this arm a
        // battle card would survive a click on a building and the Selection
        // element would have two things to draw — the exact failure the
        // province_sync_entity witness exists to prevent, one selection kind
        // later.
        state.clear_battle_selection();
                state.clear_lens_region_selection();
        state.province_sync_entity = state.selected_entity;
    }

    // Clip to the grid area so hexes don't overdraw the title bar or the solar canvas.
    dl->PushClipRect(grid_area_origin, grid_area_origin + grid_area_size, true);

    // Tiles that carry a building, mapped to their type so the marker pass can
    // draw the type-specific glyph. Also track the building entity per tile for
    // hit-zone registration (BL-059), and the corp owning each built tile (marker
    // colour + Corporation lens). BL-362: all three are cached behind the shared
    // stamp — the building/asset sets only move on the events the stamp tracks —
    // and scoped to the active body (queries only ever ask about its tiles).
    static body_frame_stamp s_marker_stamp;
    static std::unordered_map<entity_id, building_type> built_tiles;
    static std::unordered_map<entity_id, entity_id>     tile_to_bld;  // tile_id → building entity
    static std::unordered_map<entity_id, entity_id>     tile_to_corp; // tile_id → owning corp
    // BL-367: how many buildings stand on the tile — the "+N" badge count, and
    // the signal that a marker click should land on the TILE (grouped stack
    // list) rather than assuming the whole hex is one installation.
    static std::unordered_map<entity_id, int>            tile_bld_count;
    // Unit groups per province (BL-575) — see the pre-pass below. Keyed on the
    // SAME stamp as the maps above, since a hire/disband changes w.units.size()
    // and a march order's actual tile move only happens on a tick (already
    // caught by day_tick).
    static std::unordered_map<uint32_t, std::vector<unit_marker_summary>> province_units;
    // BL-1145 (a province's readers take its anchor, not its lowest tile): the
    // tile each (province, owner) unit group DRAWS on — its lowest-id unit's own
    // tile, so the marker sits where the army stands — and the tile each
    // province's battle glyph draws on, its ANCHOR (`province_anchor_tile`; the
    // lowest-id tile only for a province with no centre). Keyed on the same
    // stamp: a centre changes only on a tick, which day_tick catches.
    static std::unordered_map<entity_id, std::vector<unit_marker_summary>> tile_units;
    static std::unordered_map<uint32_t, entity_id> province_mark_tile;
    const body_frame_stamp marker_stamp = make_body_frame_stamp(w, state.active_body);
    if (!(marker_stamp == s_marker_stamp))
    {
        s_marker_stamp = marker_stamp;
        built_tiles.clear();
        tile_to_bld.clear();
        tile_to_corp.clear();
        tile_bld_count.clear();
        for (const auto& [bld_id, bld] : w.buildings)
        {
            auto tile_it = w.tiles.find(bld.tile);
            if (tile_it != w.tiles.end() && tile_it->second.body == state.active_body)
            {
                ++tile_bld_count[bld.tile];
                // Lowest building id wins the tile. w.buildings is an unordered_map, so a
                // plain last-writer-wins assignment would let its iteration order pick the
                // representative — fine while a tile holds one building, not once they
                // stack. Lowest-id is stable across frames and across runs.
                const auto prev = tile_to_bld.find(bld.tile);
                if (prev != tile_to_bld.end() && prev->second <= bld_id)
                    continue;
                built_tiles[bld.tile] = bld.type;
                tile_to_bld[bld.tile] = bld_id;
            }
        }
        for (const auto& [corp_id, corp] : w.corporations)
        {
            for (entity_id bld_id : corp.assets)
            {
                const auto bld_it = w.buildings.find(bld_id);
                if (bld_it == w.buildings.end())
                    continue;
                const auto tile_it = w.tiles.find(bld_it->second.tile);
                if (tile_it != w.tiles.end() && tile_it->second.body == state.active_body)
                    tile_to_corp[bld_it->second.tile] = corp_id;
            }
        }

        // Unit groups per province (BL-575). A unit's command grain is the
        // province (BL-511's march_unit retarget), so units sharing a province
        // AND an owner draw as one marker with a count badge, exactly as the
        // "+N" building-stack badge above groups several buildings on one tile.
        // Iterated in ascending unit-id order so `sample_unit` (the group's
        // click/hover target) is deterministically the LOWEST id in the group,
        // the same "lowest id wins" precedent tile_to_bld uses just above.
        province_units.clear();
        {
            std::vector<entity_id> unit_ids;
            unit_ids.reserve(w.units.size());
            for (const auto& [uid, uc] : w.units)
                unit_ids.push_back(uid);
            std::sort(unit_ids.begin(), unit_ids.end());
            for (const entity_id uid : unit_ids)
            {
                const auto& uc = w.units.at(uid);
                const auto utile_it = w.tiles.find(uc.position);
                if (utile_it == w.tiles.end() || utile_it->second.body != state.active_body)
                    continue;
                const uint32_t upid = w.provinces.province_of(uc.position);
                if (upid == 0)
                    continue;
                std::vector<unit_marker_summary>& groups = province_units[upid];
                auto grp_it = std::find_if(groups.begin(), groups.end(),
                    [&](const unit_marker_summary& g) { return g.owner == uc.owner; });
                if (grp_it == groups.end())
                {
                    // BL-575 stub: `committed` stays false — no writer sets it
                    // true until BL-573 (wave 4) adds the real per-unit flag.
                    groups.push_back({ uc.owner, uid, 1, /*committed=*/false });
                }
                else
                {
                    ++grp_it->count;
                }
            }
        }

        // Each group draws on its sample (lowest-id) unit's own tile. A tile
        // lies in exactly one province, so every group landing on a tile comes
        // from ONE province's vector, in that vector's own (ascending-unit-id)
        // order — the unordered walk of `province_units` cannot reorder them.
        tile_units.clear();
        for (const auto& [upid, groups] : province_units)
            for (const unit_marker_summary& g : groups)
            {
                const auto uit = w.units.find(g.sample_unit);
                if (uit != w.units.end())
                    tile_units[uit->second.position].push_back(g);
            }

        // The battle glyph's tile per province of this body: its anchor.
        province_mark_tile.clear();
        {
            const std::map<uint32_t, province_anchor> anchors = province_anchors(w);
            for (const province& pv : w.provinces.provinces)
                if (pv.body == state.active_body)
                    province_mark_tile[pv.id] = province_anchor_tile(anchors, pv);
        }
    }

    // Spatial index (BL-268): the per-body raster logistics already caches on
    // world.body_tile_index (grid_y*gw + grid_x -> tile entity, null_entity for an
    // absent cell). app::render ensures it is built for the active body before this
    // draw, so the const world& here just reads it — replacing the per-frame
    // unordered_map rebuild that scanned every body's tiles each frame. An absent
    // entry (a tileless body) yields an empty raster, which draws nothing — loud
    // in a golden, never a silent slow path.
    static const std::vector<entity_id> no_raster;
    const auto raster_it = w.body_tile_index.find(state.active_body);
    const std::vector<entity_id>& raster =
        raster_it != w.body_tile_index.end() ? raster_it->second : no_raster;
    const bool raster_ok = raster.size() == static_cast<std::size_t>(gw) * gh;

    // Grid-coordinate lookup into the raster; bounds-checked so callers can pass
    // an out-of-range row (rows do not wrap) and get null_entity, exactly as the
    // old map's find-miss did.
    auto tile_at_rc = [&](int col, int row) -> entity_id {
        if (!raster_ok || col < 0 || col >= gw || row < 0 || row >= gh)
            return null_entity;
        return raster[static_cast<std::size_t>(row) * gw + col];
    };

    // Nation owner of a tile, or null_entity when the tile is absent from
    // tile_to_nation (unclaimed). Used by the border pass to compare adjacent
    // owners; "unclaimed" is treated as its own distinct owner.
    auto nation_of = [&](entity_id tile_id) -> entity_id {
        const auto it = w.tile_to_nation.find(tile_id);
        return it != w.tile_to_nation.end() ? it->second : null_entity;
    };

    // Identity colour for a corporation: delegates to the shared palette source of
    // truth (BL-090) so the marker pass, the Corporation lens, the identity card,
    // the Selection header, and the corp emblem tags all agree.
    auto corp_identity = [&](entity_id corp_id) -> ImU32 {
        return palette::corp_identity_colour(corp_id, w.player_entity);
    };

    // Resource lens (BL-019): always single-resource. The lens fills the whole
    // contiguous deposit of the selected resource as a flat, uniform colour — the
    // *shape* of the deposit, not a magnitude gradient. A tile is part of the
    // deposit when it carries any of the resource (deposit > 0); contiguous tiles
    // form one blob (8-connected), but since the fill is uniform the per-tile
    // threshold is visually identical, so no flood-fill grouping is needed. No
    // normalisation maxima — the only state the pass needs is the selected index.

    // Market lens pre-pass (BL-015): assign each market on the active body a
    // distinct catchment colour (using corp palette slots, cycling if there are
    // more markets than slots). Tiles are tinted by their catchment market.
    // The old price-wash readout is relocated to the Market Ledger.
    std::unordered_map<entity_id, ImU32> market_catchment_colour;
    if (state.overlay == overlay_mode::market)
    {
        int slot = 0;
        for (const auto& [mid, mk] : w.markets)
        {
            if (mk.body != state.active_body)
                continue;
            market_catchment_colour[mid] =
                palette::corp_colour(slot % palette::corp_slot_count);
            ++slot;
        }
    }

    // Placement-suitability pre-pass (BL-010). A surface tied to *construction mode*,
    // not bare selection: it activates only while a build is armed
    // (state.construction.active) and is keyed to the armed building type/target, so a
    // plain inspection click never re-skins the map (it used to fire on any tile
    // selection, reading as a spurious lens change). Tints every other tile by how
    // well the *armed* building would do there.
    const bool          suitability_active = state.construction.active;
    const building_type suitability_btype  = state.construction.type;
    const resource_type suitability_target = state.construction.target;
    // "Affine" (thrives-here) applies to extraction only: a tile whose own richest
    // extractable resource is the armed target reads as optimal. Other building types
    // carry no terrain-affinity signal — valid-but-not-affine tiles stay uncoloured.
    const bool          suitability_affine_kind =
        (suitability_btype == building_type::extraction_site);

    // Scarcity lens pre-pass (BL-018): a market-level field, not a per-tile one.
    // Scarcity is driven by **supply shortfall** — how much demand outran supply
    // last tick for the selected good, per market (independent of price). Each
    // market's catchment tiles read as one chunky block (market_for_tile). We
    // collect per-market shortfall and the body-max for normalisation; a tile then
    // looks up its market's shortfall and tints uniformly across the catchment.
    std::unordered_map<entity_id, float> scar_shortfall; // market id → shortfall of sel
    float scar_max_shortfall = 0.0f;
    if (state.overlay == overlay_mode::scarcity)
    {
        const std::size_t sel = static_cast<std::size_t>(state.lens_resource);
        for (const auto& [mid, mk] : w.markets)
        {
            if (mk.body != state.active_body)
                continue;
            const float shortfall = std::max(0.0f, mk.demand[sel] - mk.supply[sel]);
            scar_shortfall[mid] = shortfall;
            scar_max_shortfall  = std::max(scar_max_shortfall, shortfall);
        }
    }

    // Supply lens pre-pass: check whether the active body has any player convoys
    // (source or destination). Used inside the tile loop to gate the per-tile glyph.
    // w.convoys is populated by dispatch_convoys (supply_system.cpp) each tick.
    bool supply_active = false;
    if (state.overlay == overlay_mode::supply)
    {
        for (const auto& cv : w.convoys)
        {
            if (cv.corp != w.player_entity)
                continue;
            entity_id src_body = null_entity;
            entity_id dst_body = null_entity;
            const auto smk = w.markets.find(cv.source_market);
            const auto dmk = w.markets.find(cv.dest_market);
            if (smk != w.markets.end()) src_body = smk->second.body;
            if (dmk != w.markets.end()) dst_body = dmk->second.body;
            if (src_body == state.active_body || dst_body == state.active_body)
            {
                supply_active = true;
                break;
            }
        }
    }

    // Continent lens pre-pass (BL-226): the Continents/Drift pass (BL-210) decided
    // where the land ended up, then folded its verdict into Pass 1's heightmap and
    // vanished — by the time a tile exists, the plate that raised it is unreadable
    // from the terrain. The generation report retains the plate field precisely so
    // this lens can put it back on the map. Matched by ENTITY ID (BL-257) — body
    // names are generated and display-only, so the old name match was a display
    // string standing in for an identity.
    const continent_state* plates = nullptr;
    if (state.overlay == overlay_mode::continent)
    {
        for (const auto& be : gen.bodies)
            if (be.id == state.active_body) { plates = &be.continents; break; }
        // A world generated before the field was retained (or a body absent from
        // the report) leaves this null — the lens then draws plain terrain and
        // says so in its key, rather than inventing plates.
        if (plates && plates->plate_id.size() !=
                static_cast<std::size_t>(body.grid_width) * static_cast<std::size_t>(body.grid_height))
            plates = nullptr;
    }

    // Industry lens pre-pass (BL-084; re-pointed BL-373). The lens used to tint by
    // tile.substrate_density — an abstract occupation scalar left over from the
    // nation-substrate injection. BL-365 replaced that substrate with REAL background
    // corporations that own real buildings, so the lens now reads the thing its name
    // always promised: the background firms' plant standing on each tile. The question
    // it answers is "where is the industry I did not build?" — deliberately distinct
    // from the Production lens, which ranks output intensity INCLUDING the player's own
    // holdings. substrate_density survives untouched; it just stops feeding this lens.
    //
    // Per tile: Σ over background-owned buildings of (0.5 + 0.5 × output share). The
    // 0.5 floor keeps an idle or still-building background plant visible — its presence
    // is the fact the lens reports — while output separates a token works from a real
    // one, and two buildings on a tile stack.
    std::unordered_map<entity_id, float> industry_field;
    float industry_max = 0.0f;
    if (state.overlay == overlay_mode::industry)
    {
        const auto background_owned = [&w](entity_id corp) {
            const auto cit = w.corporations.find(corp);
            return cit != w.corporations.end() && cit->second.is_background;
        };

        // Body-local output ceiling, so the share term is relative to this body's own
        // background industry rather than a cross-body absolute.
        float max_out = 0.0f;
        for (const building_report& br : report.buildings)
            if (br.body == state.active_body && background_owned(br.corp))
                max_out = std::max(max_out, br.output_quantity);

        for (const building_report& br : report.buildings)
        {
            if (br.body != state.active_body || !background_owned(br.corp))
                continue;
            const auto bld_it = w.buildings.find(br.building);
            if (bld_it == w.buildings.end())
                continue;
            const float share = (max_out > 0.0f)
                                    ? std::clamp(br.output_quantity / max_out, 0.0f, 1.0f)
                                    : 0.0f;
            float& thr = industry_field[bld_it->second.tile];
            thr += 0.5f + 0.5f * share;
            industry_max = std::max(industry_max, thr);
        }

        // BL-511's per-province reduction for this lens: a SUM, rendered
        // uniformly over the province. Industry is the one sparse field whose
        // question — "how much plant I did not build stands here?" — is genuinely
        // about the locality rather than about the individual works, and density
        // is additive, so summing the member tiles and filling the whole province
        // flat is the honest answer. It is also the reason this lens is NOT in
        // lens_blend_mode: blending would spread one works' amber over the empty
        // ground beside it, which reads as industry that is not there.
        if (!industry_field.empty())
        {
            std::unordered_map<uint32_t, float> prov_total;
            for (const auto& [tid, v] : industry_field)
                if (const uint32_t pid = w.provinces.province_of(tid); pid != 0)
                    prov_total[pid] += v;

            // Driven from the totals, not from the whole partition: only the
            // provinces that actually hold background plant are touched, so this
            // costs the number of industrial localities on one body rather than
            // the world's ~8,300 provinces per frame.
            for (const auto& [pid, total] : prov_total)
            {
                const province* pv = w.provinces.find(pid);
                if (!pv)
                    continue;
                for (entity_id tid : pv->tiles)
                    industry_field[tid] = total;
                industry_max = std::max(industry_max, total);
            }
        }
    }

    // Reach lens pre-pass (BL-011): `trade_route` is body-level, not per-tile, and
    // this canvas only ever shows the *active* body's tile grid (never other
    // bodies), so the "body-to-body highlight" the design calls for reads here as
    // a per-body connectivity readout for the active body — every other endpoint
    // it is connected to, tiered by recency. Player's own routes only, per the
    // competitor-visibility rule (rival lanes stay private). No falloff/distance
    // model: trade_route carries none to derive one from.
    std::vector<reach_link> reach_links;
    if (state.overlay == overlay_mode::reach)
    {
        for (const trade_route& route : w.trade_routes)
        {
            if (route.corp != w.player_entity)
                continue;
            entity_id other = null_entity;
            if (route.body_a == state.active_body)      other = route.body_b;
            else if (route.body_b == state.active_body) other = route.body_a;
            else                                        continue;
            const bool fresh = (w.current_day_tick - route.last_tick) <= route_fresh_ticks_default;
            reach_links.push_back({other, fresh ? activity_vis::known : activity_vis::known_stale});
        }
    }

    // Supply-routes lens pre-pass (BL-014): one aggregated edge per unordered
    // (body_a, body_b) pair touching the active body, built from `w.trade_routes`
    // at render time (not per-frame convoy positions). `trade_route` is already
    // upserted per (pair, corp), so filtering to the player's corp yields at most
    // one entry per pair directly — no further aggregation needed. Thickness is
    // log-scaled convoy_count; colour/alpha is the same recency tier as Reach.
    std::vector<supply_edge> supply_edges;
    if (state.overlay == overlay_mode::supply_routes)
    {
        for (const trade_route& route : w.trade_routes)
        {
            if (route.corp != w.player_entity)
                continue;
            entity_id other = null_entity;
            if (route.body_a == state.active_body)      other = route.body_b;
            else if (route.body_b == state.active_body) other = route.body_a;
            else                                        continue;
            const bool fresh = (w.current_day_tick - route.last_tick) <= route_fresh_ticks_default;
            supply_edges.push_back({other, route.convoy_count,
                                    fresh ? activity_vis::known : activity_vis::known_stale});
        }
    }

    // Intra-body vision — the moving convoy beam (BL-152/154). Permanent vision
    // (state.permanent_vision: radius-2 pockets around the player's buildings + 3-wide
    // corridors from the corp centre of operation to each market centre it operates in)
    // is precomputed each frame in update_body_vision. Here we add the *moving* beam:
    // for each live player convoy we interpolate a HEAD along its path by the fraction
    // through the current econ tick — so it glides smoothly between quarterly steps —
    // and trail a dimming TAIL one econ tick's travel behind it. beam_intensity holds
    // each lit tile's brightness (0..1); the tile loop blends it with permanent vision
    // to size the fog wash. Survey fog (BL-067) still owns unrevealed tiles.
    std::unordered_map<entity_id, float> beam_intensity;
    {
        // Fraction through the current econ tick (90 days); mirrors sim_loop::econ_tick_days.
        const float frac = static_cast<float>(
            std::fmod(std::max(0.0, state.sim_now_days), 90.0) / 90.0);
        constexpr int beam_radius = 2; // Ben's spec: a radius-2 beam of vision.

        // Flood radius-2 around a path tile, marking each at `inten` (keeping the max).
        auto light = [&](entity_id centre_tile, float inten) {
            if (centre_tile == null_entity) return;
            std::unordered_set<entity_id> seen;
            std::vector<entity_id> frontier{ centre_tile };
            seen.insert(centre_tile);
            { float& v = beam_intensity[centre_tile]; if (inten > v) v = inten; }
            for (int r = 0; r < beam_radius && !frontier.empty(); ++r)
            {
                std::vector<entity_id> next;
                for (const entity_id tid : frontier)
                {
                    const auto tit = w.tiles.find(tid);
                    if (tit == w.tiles.end()) continue;
                    const tile_component& t = tit->second;
                    const int (*off)[2] = hex_neighbors::offsets(t.grid_y);
                    for (int k = 0; k < 6; ++k)
                    {
                        const int nrow = t.grid_y + off[k][1];
                        if (nrow < 0 || nrow >= gh) continue;
                        int ncol = (t.grid_x + off[k][0]) % gw;
                        if (ncol < 0) ncol += gw;
                        const entity_id nb = tile_at_rc(ncol, nrow);
                        if (nb == null_entity) continue;
                        if (seen.insert(nb).second)
                        {
                            next.push_back(nb);
                            float& v = beam_intensity[nb]; if (inten > v) v = inten;
                        }
                    }
                }
                frontier.swap(next);
            }
        };

        for (const auto& cb : state.convoy_beams)
        {
            const int n = static_cast<int>(cb.path.size());
            if (n == 0) continue;
            // Head glides: last econ-step progress + this tick's fraction of a step.
            const float p    = std::clamp(cb.progress + cb.speed * frac, 0.0f, 1.0f);
            const int   head = std::clamp(static_cast<int>(std::lround(p * (n - 1))), 0, n - 1);
            // Tail = one econ tick's travel in tiles (>=1), dimming to 0 at its far end.
            const int   tail = std::max(1, static_cast<int>(std::lround(cb.speed * (n - 1))));
            for (int i = head; i >= 0 && i >= head - tail; --i)
            {
                const float inten = 1.0f - static_cast<float>(head - i) / static_cast<float>(tail);
                if (inten > 0.0f) light(cb.path[static_cast<std::size_t>(i)], inten);
            }
        }
    }

    // A tile's vision scalar (0..1): 1 inside the permanent layers (building pockets +
    // the corp-centre->market corridors), else the moving beam's intensity. Hoisted out
    // of the tile loop's fill block for BL-185, because the road pass now needs the same
    // number for a *neighbour* tile — the fog must size identically wherever it is read.
    //
    // Held as a dense raster, built from the two sparse layers ONCE a frame, for
    // the passes that read it per tile and per neighbour (the 60 fps budget,
    // PLANETARY.md § Draw-loop cost model): ~30k tiles x two set lookups became
    // one array read. 1 inside permanent vision, else the beam.
    //
    // The Resource lens reads SURVEY knowledge, not activity: a deposit is known
    // wherever the ground is surveyed, so the activity fog is lifted under it and
    // the washed-clear map reads clear (Ben, 2026-10-04). The survey mask still
    // owns unsurveyed tiles.
    //
    // The building and ownership indices get the same treatment as a bit mask:
    // both maps are small, so a miss on every other tile was the whole cost.
    static std::vector<float>   vision_raster;
    static std::vector<uint8_t> tile_flags; // bit 0 built, bit 1 owned
    vision_raster.assign(raster_ok ? raster.size() : 0u, 0.0f);
    tile_flags.assign(raster_ok ? raster.size() : 0u, 0u);
    const auto raster_index_of = [&](entity_id tid) -> int {
        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end() || it->second.body != state.active_body)
            return -1;
        const int x = it->second.grid_x, y = it->second.grid_y;
        return (x >= 0 && x < gw && y >= 0 && y < gh) ? y * gw + x : -1;
    };
    if (raster_ok)
    {
        for (const entity_id tid : state.permanent_vision)
            if (const int ri = raster_index_of(tid); ri >= 0)
                vision_raster[static_cast<std::size_t>(ri)] = 1.0f;
        for (const auto& [tid, inten] : beam_intensity)
            if (const int ri = raster_index_of(tid); ri >= 0)
            {
                float& v = vision_raster[static_cast<std::size_t>(ri)];
                v = std::max(v, inten);
            }
        for (const auto& [tid, bt] : built_tiles)
            if (const int ri = raster_index_of(tid); ri >= 0)
                tile_flags[static_cast<std::size_t>(ri)] |= 1u;
        for (const auto& [tid, corp] : tile_to_corp)
            if (const int ri = raster_index_of(tid); ri >= 0)
                tile_flags[static_cast<std::size_t>(ri)] |= 2u;
    }
    const auto vision_rc = [&](int col, int row) -> float {
        if (state.overlay == overlay_mode::resource)
            return 1.0f;
        if (!raster_ok || col < 0 || col >= gw || row < 0 || row >= gh)
            return 0.0f;
        return vision_raster[static_cast<std::size_t>(row) * gw + col];
    };

    const ImVec2 mouse = state.mouse.active
                         ? ImVec2{state.mouse.x, state.mouse.y}
                         : ImVec2{-1.0f, -1.0f}; // off-screen sentinel suppresses hover

    // --- The camera: ONE angle (RENDERING.md § One angle; BL-1246) ----------
    // The land is viewed at 22.5° at every rung and under every lens (Ben,
    // 2026-10-09 — one angle is what makes one master possible, and toggling
    // a lens never moves the map). The canvas keeps drawing entirely in FLAT
    // ground space; one vertex-range squash over the map passes is the camera
    // (the lens legend's vertices are excluded from it), and this inverse
    // maps the real cursor into that flat space so every hit test works
    // unchanged.
    const float  tilt_sy    = planetary_tilt_sy(zoom);
    const float  tilt_pivot = canvas_centre.y;
    const ImVec2 mouse_g    = { mouse.x,
                                tilt_pivot + (mouse.y - tilt_pivot) / tilt_sy };

    // Hover resolves to a single tile copy. Adjacent hexes' circular hit-tests
    // overlap, and the cylinder draws several wrap copies of each tile, so more
    // than one (tile, copy) can satisfy the hover test at once. The nearest hex
    // centre to the cursor wins; its outline is drawn after the grid pass so only
    // one tile highlights (see the highlight tie convention in highlight.hpp).
    entity_id hovered_tile     = null_entity;
    bool      hovered_selected = false;
    bool      have_hover       = false;
    float     best_d2          = std::numeric_limits<float>::max();
    ImVec2    hover_verts[6]   = {};
    ImVec2    hover_offset     = {};   // cursor minus hovered hex centre (Resource lens wedge)

    // Slightly shrink the drawn hex so the background shows through as a border.
    // Use the full circumradius for hit-testing so small hexes stay clickable.
    const float draw_r = hex_size * zoom - 1.0f;
    const float hit_r  = hex_size * zoom;

    // --- Fill level-of-detail (BL-269) ------------------------------------------
    // MEASURED 2026-08-09, pan_perf on the real renderer, Kepler (15,120 tiles):
    //
    //   zoom            vertices   submit ms
    //   0.9 whole grid   157,084      14.25
    //   1.1                6,172       0.47
    //   3.0               23,620       1.95
    //
    // A 25x vertex cliff at the whole-grid view, and submit alone blows the 8 ms
    // frame budget there. Panning is NOT the cost -- the static and panning phases
    // measured identically (157,084 vs 158,407) -- which is why this is a draw-cost
    // fix and not an input fix. Panning only makes the hitches visible, because a
    // frame that takes 3x as long applies 3x the accumulated MouseDelta at once:
    // the right destination by a jumpy route.
    //
    // The cause is `AddConvexPolyFilled(verts, 6, ...)`: with anti-aliasing on, a
    // 6-gon costs ~10 vertices once the AA fringe is counted, times every tile.
    // `AddRectFilled` with no rounding costs 4 and emits no fringe.
    //
    // The threshold is DERIVED, not picked. A hexagon differs from its inscribed
    // rect by the corner cut, `draw_r * (1 - sqrt(3)/2)` = `draw_r * 0.134`. That
    // difference stops being drawable when it falls under one pixel, i.e. at
    // draw_r < 1 / 0.134 = 7.46 px. Round down to 7: at that radius the corner cut
    // is 0.94 px, and the per-tile landform icons (drawn at 0.42 * draw_r, so ~3 px)
    // are already unreadable.
    //
    // The first attempt used 5 px and did not fire at all — at a 1080-tall window
    // the whole-grid view sits at draw_r ~5.1-5.7, just the wrong side of it. Worth
    // recording, because it is the argument for deriving the bound from the geometry
    // rather than eyeballing a constant that happens to work at one window size.
    //
    // Terrain colour, relief shading, the survey mask and the fog wash are all
    // UNAFFECTED — they are colour, not geometry — so the analytic read the
    // whole-grid view exists for is exactly as legible.
    constexpr float k_lod_radius_px = 7.0f;
    const bool      coarse_fill     = draw_r <= k_lod_radius_px;

    // --- Terrain texture strength (BL-520) --------------------------------------
    // Texture gets its OWN level-of-detail bound, and a stricter one than the fill
    // LOD above. The coarse-fill threshold asks "is the corner cut still drawable"
    // and answers 7 px. A texture asks a harder question: a *sampled pattern* at
    // hex scale is not merely invisible when it is too small, it is MOIRE — the
    // marks of adjacent tiles beat against the pixel grid and the map crawls. So
    // the pass is off entirely below 14 px of drawn circumradius and ramps to full
    // strength at 22 px (texture_lod_scale, hex_render.hpp, where the 14 is
    // derived from the 0.20*r mark size needing ~2 px of extent).
    //
    // LENS DECISION (BL-520 open question 1): texture SURVIVES every lens, at
    // 0.45 strength. Two reasons it survives rather than being replaced. First,
    // the precedent is already set one channel over — BL-231's landform relief is
    // composited AFTER the lens tint on exactly the argument that terrain facts
    // stay true under an overlay, and "this ground is closed-canopy forest" is the
    // same class of fact as "this ground is a mountain". Second, replacing it would
    // make every lens a different map rather than the same map read differently,
    // which is the property the lens bar depends on.
    //
    // It is ATTENUATED rather than left at full because a lens fill is a
    // categorical claim and must stay the loudest thing on the tile; and because
    // the marks derive their ink from the lens fill itself (texture_ink), so at
    // 0.45 they read as shading on the lens colour instead of as dirt over it —
    // which was the exact failure mode the open question named.
    constexpr float k_texture_lens_strength = 0.45f;
    const float     texture_strength =
        texture_lod_scale(draw_r)
        * (state.overlay == overlay_mode::none ? 1.0f : k_texture_lens_strength);

    // --- Infinite horizontal scroll ---
    // The grid is a cylinder: column gw wraps onto column 0. In screen space the
    // grid repeats every `period_px = gw * col_step * zoom`. Each tile is drawn
    // (and hit-tested) at every integer wrap offset k whose copy falls within the
    // canvas, so panning past either edge continues seamlessly from the far side.
    const float period_px = static_cast<float>(gw) * col_step * zoom;
    const float visible_left  = grid_area_origin.x - hit_r;
    const float visible_right = grid_area_origin.x + grid_area_size.x + hit_r;

    // Draw the active body's tiles in a deterministic order — row-major over the
    // raster. This IS the old sorted-by-id order (tile generation creates each
    // body's tiles rows-outer with sequential create_entity ids), so overlapping
    // antialiased hex edges and markers land exactly as before and no golden
    // moves. BL-268 replaces the old per-frame collect-and-sort with the cached
    // raster, and culls to the visible ROW band before any per-tile work: rows do
    // not wrap, so the band falls straight out of the clip rect, mirroring the
    // ±hit_r margin the horizontal wrap-window below already uses. Column
    // visibility is the wrap-window test itself, hoisted to the top of the loop
    // body — an off-screen tile costs one bounds test and one multiply-compare.
    const float row_pitch = 1.5f * hex_size; // hex_local_centre's vertical pitch
    // The clip edges map into GROUND space first (the camera squash shows a
    // taller ground band than the screen band when tilted — BL-737).
    const float clip_top_g = tilt_pivot + (grid_area_origin.y - hit_r - tilt_pivot) / tilt_sy;
    const float clip_bot_g = tilt_pivot
        + (grid_area_origin.y + grid_area_size.y + hit_r - tilt_pivot) / tilt_sy;
    const int row_lo = raster_ok ? std::max(
        0, static_cast<int>(std::floor(
               ((clip_top_g - view_origin.y) / zoom + grid_cy) / row_pitch)))
        : 0;
    const int row_hi = raster_ok ? std::min(
        gh - 1, static_cast<int>(std::ceil(
               ((clip_bot_g - view_origin.y) / zoom + grid_cy) / row_pitch)))
        : -1;

    // --- BL-732: the baked painterly ground (docs/ui/RENDERING.md) ----------
    // With no lens active, the ground is drawn from the baked chunk textures
    // core/ground_layer publishes into `state.ground` — the far page first
    // (whole body, low-res, always ready after a body switch), the ready
    // full-res chunks over it — and the per-tile loop below then SKIPS its
    // fill and texture emit for surveyed tiles (`on_bake`): the no-grid rule,
    // and the fallback-by-coverage rule, in one flag. Canonical space is the
    // hex grid at circumradius 1, so canonical -> local is a scale by hex_size.
    const ground_view& gview = state.ground;
    // Under EVERY lens too (BL-1250, LENSES.md § A lens washes the rendered
    // ground): the lens is a wash over the bake, not a fill that replaces it.
    const bool ground_on = gview.body == state.active_body
                        && gview.far_ready;
    const auto canon_to_screen = [&](float gx_, float gy_) -> ImVec2 {
        return to_screen({ gx_ * hex_size, gy_ * hex_size });
    };
    // Publish this frame's visible window for NEXT frame's bake pass (one
    // frame of latency; the far page covers it). x in canonical units, wrap
    // handled by ground_layer.
    {
        const float x0c = ((visible_left  - view_origin.x) / zoom + grid_cx) / hex_size;
        const float x1c = ((visible_right - view_origin.x) / zoom + grid_cx) / hex_size;
        state.ground_req.body   = state.active_body;
        state.ground_req.x0     = x0c;
        state.ground_req.x1     = x1c;
        state.ground_req.y0     = 1.5f * static_cast<float>(row_lo) - 1.0f;
        state.ground_req.y1     = 1.5f * static_cast<float>(row_hi) + 1.0f;
        // The DRAWN ground radius, hex_size * zoom — not draw_r, which is a
        // 1 px border-inset for the polygon fills. Passing draw_r let a drawn
        // radius in (T, T+1] pick tier T and draw it magnified (BL-1244).
        state.ground_req.draw_r = hit_r; ///< Picks the master's level (RENDERING.md § Level of detail).
        state.ground_req.valid  = raster_ok;
    }
    // BL-737: everything from here to the camera squash at the end of the map
    // passes draws in FLAT ground space; the squash over this vertex range IS
    // the tilt. (The background rect, title and chrome above this line stay
    // screen-space; so does the lens legend, whose vertex range is cut out.)
    const int v_map_start = dl->VtxBuffer.Size;
    canvas_pass_meter& pm = pass_meter();
    pm.begin(draw_r, static_cast<int>(state.overlay));
    pm.mark(canvas_pass_meter::setup, dl);
    int v_key_start = -1, v_key_end = -1;
    // The camera over [from, to) of this list's vertices.
    const auto squash = [&](int from, int to) {
        if (tilt_sy >= 1.0f || from < 0)
            return;
        ImDrawVert* vtx = dl->VtxBuffer.Data;
        for (int vi = from; vi < to; ++vi)
            vtx[vi].pos.y = tilt_pivot + (vtx[vi].pos.y - tilt_pivot) * tilt_sy;
    };
    // THE GROUND IS EMITTED LAST AND DRAWN FIRST (sprint 51, the 60 fps budget;
    // PLANETARY.md § Draw-loop cost model). The SDL renderer backend converts a
    // draw command's vertex colours from that command's VtxOffset to the END of
    // the list, once per command — and every ground chunk is its own texture,
    // so its own command. Emitted first, ~50 chunk images each re-converted the
    // whole vector layer behind them (~300k vertices): measured, that was most
    // of the canvas's submit time, not the vectors themselves. So the images go
    // into channel 0 of a three-channel split (0 ground, 1 washes, 2 strokes),
    // written AFTER the vector passes on a vertex offset of their own: the draw
    // order is unchanged, the per-command conversion is ~4 vertices.
    //
    // Draw one image quad per wrap copy that intersects the canvas. The
    // image spans exactly one wrap period, so copies abut seamlessly.
    const auto draw_ground_rect = [&](const ground_chunk_view& cv)
    {
        if (!cv.tex)
            return;
        const ImVec2 p0 = canon_to_screen(cv.x0, cv.y0);
        const ImVec2 p1 = canon_to_screen(cv.x1, cv.y1);
        const int k_lo = (period_px > 0.0f)
            ? static_cast<int>(std::floor((visible_left  - p1.x) / period_px)) : 0;
        const int k_hi = (period_px > 0.0f)
            ? static_cast<int>(std::ceil ((visible_right - p0.x) / period_px)) : 0;
        for (int k = k_lo; k <= k_hi; ++k)
        {
            const float off = static_cast<float>(k) * period_px;
            if (p1.x + off < visible_left || p0.x + off > visible_right)
                continue;
            dl->AddImage((ImTextureID)(intptr_t)cv.tex,
                         { p0.x + off, p0.y }, { p1.x + off, p1.y });
        }
    };
    if (ground_on)
    {
        dl->ChannelsSplit(3);
        dl->ChannelsSetCurrent(2);
    }
    // The washes over the bake (player identity, construction suitability, the
    // vision fog) are collected as the tile loop meets them and emitted into
    // channel 1 — over the ground, under every stroke — so a run of equal washes
    // along a row can be ONE quad (see the tile loop's on_bake branch).
    struct ground_wash { ImVec2 c; float r; ImVec2 p0, p1; ImU32 col; bool hex; };
    static std::vector<ground_wash> washes;
    washes.clear();
    // The lens's own washes (BL-1250) are a list of their own, emitted BEFORE the
    // player/suitability/fog washes so the fog dims the lens answer exactly as it
    // dimmed the lens fill — and so each list's equal-colour run joining sees its
    // own previous rect. Tiles never overlap, so splitting the lists moves nothing.
    static std::vector<ground_wash> lens_washes;
    lens_washes.clear();
    int lens_run[4] = { -1, -1, -1, -1 }; // last coarse rect per wrap copy (k & 3)
    // A Resource-lens split tile's wedges over the bake, at the wash strength.
    struct ground_wedges { ImVec2 c; float r; ImU32 cols[ui_state::k_lens_resource_cap]; int n; };
    static std::vector<ground_wedges> lens_wedges;
    lens_wedges.clear();
    // The close-zoom seam (BL-1251 part 5): one segment per shared tile edge.
    static std::vector<ImVec2> seam_segs;
    seam_segs.clear();
    const bool seam_on = hit_r >= k_seam_min_drawn_r;
    const auto emit_ground = [&] {
        if (!ground_on)
            return;
        dl->ChannelsSetCurrent(1);
        // The seam first: over the ground, under every wash and stroke. Thin
        // anti-aliased lines — the texture-AA path, a few vertices a segment.
        for (std::size_t si = 0; si + 1 < seam_segs.size(); si += 2)
            dl->AddLine(seam_segs[si], seam_segs[si + 1],
                        IM_COL32(0, 0, 0, static_cast<int>(k_seam_alpha * 255.0f)), 1.0f);
        // NOT anti-aliased: a wash tiles the plane with its neighbours, and an AA
        // fringe both doubles the vertices and leaves a faint seam where two
        // translucent fringes overlap.
        const ImDrawListFlags saved = dl->Flags;
        dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        const auto emit_list = [&](const std::vector<ground_wash>& list) {
            for (const ground_wash& gw_ : list)
            {
                if (gw_.hex)
                {
                    ImVec2 hv[6];
                    hex_vertices(hv, gw_.c.x, gw_.c.y, gw_.r);
                    dl->AddConvexPolyFilled(hv, 6, gw_.col);
                }
                else
                    dl->AddRectFilled(gw_.p0, gw_.p1, gw_.col);
            }
        };
        emit_list(lens_washes);
        for (const ground_wedges& wg : lens_wedges)
            draw_lens_wedges(dl, wg.c, wg.r, wg.cols, wg.n);
        emit_list(washes);
        dl->Flags = saved;
        pm.mark(canvas_pass_meter::fill, dl);
        dl->ChannelsSetCurrent(0);
        // And start the images on a FRESH vertex offset: below 64k vertices the
        // whole list is one offset block, so being emitted last is not enough —
        // each image command would still re-convert every vertex from 0.
        if (dl->Flags & ImDrawListFlags_AllowVtxOffset)
        {
            dl->_CmdHeader.VtxOffset = static_cast<unsigned int>(dl->VtxBuffer.Size);
            dl->_OnChangedVtxOffset();
        }
        // The far page carries only a body whose master has not yet covered
        // the view (BL-1246): every other frame the level's chunks cover it.
        draw_ground_rect(gview.far);
        for (const ground_chunk_view& cv : gview.chunks)
            draw_ground_rect(cv);
        dl->ChannelsMerge();
    };
    // Every map vertex emitted so far, minus the legend's: called once, at
    // whichever exit the frame takes (the input early-out included — a
    // cursor over a panel must not un-tilt the map).
    // The ground's deferred images are emitted here, so they ride the squash.
    const auto apply_camera = [&] {
        pm.mark(canvas_pass_meter::post, dl);
        emit_ground();
        pm.mark(canvas_pass_meter::ground, dl);
        pm.end(dl, v_map_start);
        const int n = dl->VtxBuffer.Size;
        if (v_key_start < 0)
            squash(v_map_start, n);
        else
        {
            squash(v_map_start, v_key_start);
            squash(v_key_end, n);
        }
    };

    if (ground_on)
    {
        // BL-736: the ground-detail metric — the texel renderer's answer to a
        // polygon count. Rides the existing frame-HUD toggle; texel:pixel is
        // the number that says whether "blurry" is a resolution problem (it
        // is not, when this reads ~0.85-1.0) or a content problem.
        if (state.show_frame_hud)
        {
            char gbuf[96];
            if (gview.level_ppr > 0.0)
                std::snprintf(gbuf, sizeof gbuf,
                              "ground: level %.0f px/r  ·  %.2f texel/px  ·  %d chunks",
                              gview.level_ppr, gview.level_ppr / hit_r, // texels per DRAWN px
                              static_cast<int>(gview.chunks.size()));
            else
                std::snprintf(gbuf, sizeof gbuf,
                              "ground: far page 6 px/r  ·  %.2f texel/px",
                              6.0f / hit_r);
            // Foreground list: HUD text must not ride the tilt camera's squash.
            ImGui::GetForegroundDrawList()->AddText(
                { grid_area_origin.x + 8.0f,
                  grid_area_origin.y + grid_area_size.y - 22.0f },
                IM_COL32(200, 210, 220, 200), gbuf); // fit-exempt: debug HUD line
        }
    }
    pm.mark(canvas_pass_meter::setup, dl);

    // --- BL-511: the province fill cache ------------------------------------
    // The province, not the hex, is the unit this canvas renders and the player
    // selects. Geometry stays per hex — the cull, the LOD and the wrap window are
    // untouched — but a tile's colour is now blended into its province's, so the
    // seams INSIDE a province disappear and the cell reads as one shape carrying
    // its real terrain mixture (Ben's ruling: a blended gradient, explicitly not a
    // dominant composition and not a texture pattern).
    //
    // That needs a tile's NEIGHBOURS' colours at draw time, so the fill derivation
    // is hoisted out of the draw loop into this lambda and run one pass ahead over
    // the visible row band plus a one-row margin. The derivation itself is
    // unchanged, line for line — only where it runs moved.
    // A BL-365 background firm — a "company" rather than a "corporation" since
    // Ben's 2026-08-28 terminology ruling. Looked up rather than cached because
    // the corporation set is small and this runs only under two lenses.
    const auto is_background_firm = [&w](entity_id corp) -> bool
    {
        const auto it = w.corporations.find(corp);
        return it != w.corporations.end() && it->second.is_background;
    };

    // BL-1240: the Corporation lens's picked set defaults to the player, applied
    // once per session (ui_state cannot know the player id when it is built).
    state.seed_lens_corps(w.player_entity);
    // A dissolved owner drops out of both sets rather than lingering in the count.
    state.prune_lens_owners([&w](entity_id id) { return w.corporations.count(id) > 0; });

    // A tile's catchment market, memoised per frame on the raster: the Market and
    // Scarcity fills key on it, and the hovered-catchment wash asks again for the
    // same tile (the 60 fps budget — it is several lookups a call).
    static std::vector<entity_id> market_memo;
    static std::vector<uint8_t>   market_memo_set;
    const bool memo_markets = state.overlay == overlay_mode::market
                           || state.overlay == overlay_mode::scarcity;
    if (memo_markets)
    {
        market_memo.assign(raster_ok ? raster.size() : 0u, null_entity);
        market_memo_set.assign(raster_ok ? raster.size() : 0u, 0u);
    }
    const auto market_of = [&](entity_id id, const tile_component& tile) -> entity_id {
        const int x = tile.grid_x, y = tile.grid_y;
        if (!memo_markets || !raster_ok || x < 0 || x >= gw || y < 0 || y >= gh)
            return market_for_tile(w, id);
        const std::size_t ri = static_cast<std::size_t>(y) * gw + x;
        if (!market_memo_set[ri])
        {
            market_memo[ri]     = market_for_tile(w, id);
            market_memo_set[ri] = 1u;
        }
        return market_memo[ri];
    };

    auto compute_tile_fill = [&](entity_id id, const tile_component& tile) -> ImU32
    {
        // The ownership bit first: the index is small, so a lookup on every
        // unowned tile was all miss.
        const bool   maybe_owned = !raster_ok
            || (tile_flags[static_cast<std::size_t>(tile.grid_y) * gw + tile.grid_x] & 2u) != 0;
        const auto   corp_it    = maybe_owned ? tile_to_corp.find(id) : tile_to_corp.end();
        const bool   has_owner  = corp_it != tile_to_corp.end();
        const ImU32  owner_col  = has_owner ? corp_identity(corp_it->second)
                                            : IM_COL32(255, 255, 255, 255);
        // BL-596: EVERY tile starts from its terrain colour, built or not. A built
        // tile used to be swapped wholesale for an owner-tinted plate; Ben, 2026-08-24:
        // "Remove building background. Buildings should be drawn over the hex, not
        // completely on top." The hex is what carries substrate, cover, ownership and
        // every lens, so occluding it to label it trades away the map. Ownership has
        // not lost a channel — it is still on the silhouette's fill, the corp emblem
        // tag, and (for the player) the persistent footprint outline.
        //
        // The Country lens's whole-tile nation tint ALSO used to land here, inside the
        // blended fill (BL-601 removes the lens). It was the wrong place twice over:
        // it made a nation a field rather than a bordered region, and being part of
        // the blended fill it let two neighbours' hues average into a third nation's
        // colour at a shared corner. The national read is the border band now —
        // always-on chrome, composited per tile AFTER the blend.
        ImU32 fill = terrain_colour(tile.substrate, tile.cover, tile.cover_density);
        // Corporation lens: tint a tile that carries a corporate building with its
        // owning corp's colour (a direct replacement of the terrain hue). Tiles
        // with no corporate building keep their terrain hue — no nation underlay.
        //
        // NARROWED to corporations proper (Ben, 2026-08-28): the player and its
        // rivals, never a BL-365 background firm. The two are different words now
        // (GLOSSARY.md), and a lens that mixed them answered neither question —
        // "who are my rivals here" drowned in the background firms that outnumber
        // them. Those get their own lens immediately below, drawn identically.
        //
        // OWNER MULTI-SELECT (BL-1240): only a PICKED owner takes its colour; an
        // unpicked owner of the admitted kind takes the one owned-grey, so "someone
        // holds this" survives without every rival shouting at once.
        if (state.overlay == overlay_mode::corporation)
        {
            if (has_owner && !is_background_firm(corp_it->second))
                fill = state.lens_owner_picked(corp_it->second, false) ? owner_col
                                                                       : palette::owned_grey;
        }
        // Company lens: the exact mirror — background firms only, same per-corp
        // identity tint and owned-grey, so the two lenses are read the same way and
        // differ only in which population of firms they admit (and in the default).
        else if (state.overlay == overlay_mode::company)
        {
            if (has_owner && is_background_firm(corp_it->second))
                fill = state.lens_owner_picked(corp_it->second, true) ? owner_col
                                                                      : palette::owned_grey;
        }
        // Resource lens (BL-1182): a toggled set of goods. A tile carrying one takes
        // its identity colour SOLID; every other tile takes a white wash that keeps a
        // ghost of its terrain (Ben, 2026-10-04: "the white wash should still exist ...
        // push up the contrast"). A split tile's wedges draw over
        // this in the tile loop; the fill here is its first good, which is also what a
        // coarse-LOD rect shows. Intensity lives in tile detail, not the lens.
        else if (state.overlay == overlay_mode::resource)
        {
            resource_type on_tile[ui_state::k_lens_resource_cap];
            if (lens_resources_on_tile(state, tile, on_tile) > 0)
                fill = presentation_of(on_tile[0]).colour | IM_COL32_A_MASK;
            else
                fill = washed_resource_ground(fill, placement_rules::is_water_tile(tile.substrate));
        }
        // Market lens (BL-015): tint each tile with its catchment market's colour
        // so the boundary between markets reads as a colour boundary. Same visual
        // logic as the Corporation lens. Price readout is in the Market Ledger.
        else if (state.overlay == overlay_mode::market)
        {
            const entity_id mid = market_of(id, tile);
            const auto col_it   = market_catchment_colour.find(mid);
            if (col_it != market_catchment_colour.end())
                fill = lerp_colour(fill, col_it->second, 0.55f);
        }
        // Population (Workforce) lens (BL-135, recut to a HEATMAP 2026-08-28 on
        // Ben's instruction: "rework the workforce efficiency lens to be a
        // heatmap, akin to throughput").
        //
        // It was a per-tile red→green DOT drawn in place of the building glyph,
        // which made it the only lens in the roster that answered by adding a mark
        // rather than by colouring the ground — so it read at one tile at a time
        // and never as a field, which is the whole reason to have a lens. Now it
        // tints like every other value lens and the tile keeps its glyphs.
        //
        // WATER IS LEFT ALONE rather than tinted at its floor value. Workforce
        // efficiency is undefined on ocean, not zero, and painting it the ramp's
        // red end would assert "bad ground here" about ground that is not ground.
        // Terrain hue is the honest answer, and it also keeps the coastline
        // legible, which a wall-to-wall wash destroys.
        else if (state.overlay == overlay_mode::population
                 && !placement_rules::is_water_tile(tile.substrate))
        {
            const float eff = workforce_efficiency(std::clamp(tile.habitability, 0.0f, 1.0f));
            fill = lerp_colour(fill, ryg_colour(eff), 0.72f);
        }
        // Scarcity lens (BL-018): a market-level shortfall field. Every tile in a
        // market's catchment reads as one chunky block tinted by that market's
        // supply shortfall of the selected good (demand outran supply last tick),
        // normalised across the body's markets. Hot where scarce; no tint where met.
        else if (state.overlay == overlay_mode::scarcity)
        {
            const auto sf_it = scar_shortfall.find(market_of(id, tile));
            if (sf_it != scar_shortfall.end() && scar_max_shortfall > 0.0f)
            {
                const float scar = std::clamp(sf_it->second / scar_max_shortfall, 0.0f, 1.0f);
                constexpr ImU32 hot = IM_COL32(220, 70, 55, 255);
                fill = lerp_colour(fill, hot, 0.6f * scar);
            }
        }
        // Industry lens (BL-084/BL-373): tint a tile by the background-firm plant
        // standing on it (pre-pass above), normalised to the body max — brightest amber
        // where the industry the player did not build is densest. Tiles carrying no
        // background building keep their terrain hue.
        else if (state.overlay == overlay_mode::industry)
        {
            const auto it = industry_field.find(id);
            if (it != industry_field.end() && industry_max > 0.0f)
            {
                const float t = std::clamp(it->second / industry_max, 0.0f, 1.0f);
                constexpr ImU32 ind = IM_COL32(210, 150, 70, 255); // industrial amber
                fill = lerp_colour(fill, ind, 0.15f + 0.6f * t);
            }
        }
        // Throughput lens (BL-606, LOGISTICS.md § Logistic Points). "Throughput is
        // a lens, extending Reach. The Reach lens shows a binary field; throughput
        // is that field with a magnitude." The BINARY half is drawn here — served
        // ground where it is near against ground where it is far — and the LP
        // QUANTITY is drawn on the anchors themselves, below, because that is
        // where LP is generated and where LOGISTICS.md constraint 2 insists its
        // spatial locus stays.
        //
        // WHY THE FIELD IS THE REACH COST AND NOT "the LP serving this tile".
        // Measured, not assumed: on the home body every anchor generates the same
        // authored rate (57 anchors x 20 LP) and every tile has a FINITE reach
        // cost, because ocean is crossable at a sea-leg cost. So both the binary
        // "is it reached" and the per-tile "how much LP reaches it" are CONSTANTS
        // over the whole grid — a flat wash carrying no information. What varies,
        // and what the player actually needs, is how far the ground is from the
        // capacity: the reach field's own magnitude, which is precisely the
        // quantity the Reach lens throws away when it uses this field as a
        // yes/no placement predicate.
        //
        // This does NOT assert that LP attenuates with distance — it does not,
        // and pricing it by distance is the thing LOGISTICS.md constraint 3
        // forbids. It asserts only what the field says: this ground is this far
        // from the nearest generator.
        //
        // The read is `tile_reach_cost` — the CONST half of the reach-field pair,
        // and the only one a canvas holding a `const world&` may call. Its
        // three-way contract is honoured exactly: -1 is "not computed", which
        // draws NOTHING rather than inventing an answer (the field is warmed for
        // the active body one call before this draw, in app.cpp, so -1 means the
        // body genuinely has no field, not that this pass arrived early);
        // infinity is "computed and unreachable", which takes the cold end of the
        // ramp; anything finite normalises against the body's own worst case.
        // Nothing here can trigger the Dijkstra.
        else if (state.overlay == overlay_mode::throughput && state.lp_reach_max > 0.0f)
        {
            const float rc = tile_reach_cost(w, id);
            if (rc >= 0.0f)
            {
                // NORMALISED AGAINST THE 90th PERCENTILE, then square-root
                // compressed. Both halves are measured — `throughput_field_census`
                // is the harness, re-run it whenever the anchor set moves.
                //
                // The sqrt was calibrated in Sprint 18 against 57 anchors. The home
                // body now carries 1917, and the census says why that broke the
                // read: median cost 6.75 against a max of 81.58, so HALF the grid
                // sits in the bottom 8% of the range the ramp is spread over.
                //
                // WHAT THE MEASUREMENT OVERTURNED. The obvious fix is a steeper
                // curve, and it does not work — every curve over cost/max rescales
                // the same bunched input. Share of the grid landing in the single
                // most crowded tenth of the ramp: linear 52%, sqrt 32%, quadratic
                // 45%, quartic 35%, 1-d² 75%, cube-root 28%. Quadratic is WORSE
                // than the sqrt it would replace.
                //
                // The problem is the DENOMINATOR, not the curve. p90 is 42.58
                // against a max of 81.58 — the top decile of cost is 48% of the
                // range and holds a tenth of the tiles, so normalising against the
                // max spends half the ramp on ground almost nothing sits on.
                // Normalising against p90 instead and clamping drops the worst
                // bucket to 21%: the same curve, three times the spread.
                //
                // Ground beyond p90 saturates at the cold end rather than being
                // given its own gradient. That is the honest trade and it is the
                // point: the far tenth is all equally out of reach, and the near
                // ground is where a player sites.
                const float denom = (state.lp_reach_p90 > 0.0f) ? state.lp_reach_p90
                                                                : state.lp_reach_max;
                const float t = std::isinf(rc)
                    ? 0.0f
                    : 1.0f - std::sqrt(std::clamp(rc / denom, 0.0f, 1.0f));
                fill = lerp_colour(fill, throughput_field_colour(t), 0.72f);
            }
        }
        // Continent lens (BL-226): tint each tile by the tectonic plate that owns
        // it, and brighten the tiles that touch another plate. The boundary is the
        // point of the lens — a plate interior is just a region, but a boundary is
        // where the mountains, the rifts and the porphyry copper came from, so it
        // gets the emphasis rather than reading as an incidental edge.
        else if (state.overlay == overlay_mode::continent && plates)
        {
            const int gw  = body.grid_width;
            const int gh  = body.grid_height;
            const int idx = tile.grid_x + tile.grid_y * gw;
            const int me  = plates->plate_id[static_cast<std::size_t>(idx)];

            // Columns wrap (the grid is a cylinder, TILE_GENERATION.md); rows do not.
            bool boundary = false;
            const int cols[2] = { (tile.grid_x + 1) % gw, (tile.grid_x + gw - 1) % gw };
            for (const int c : cols)
                if (plates->plate_id[static_cast<std::size_t>(c + tile.grid_y * gw)] != me) boundary = true;
            for (const int dy : { -1, 1 })
            {
                const int ry = tile.grid_y + dy;
                if (ry < 0 || ry >= gh) continue;
                if (plates->plate_id[static_cast<std::size_t>(tile.grid_x + ry * gw)] != me) boundary = true;
            }

            // The plate tint is near-opaque: this lens is about the plate field,
            // not the terrain under it, and a weak blend let the terrain's own
            // hues swamp the categories. The boundary reads on a SEPARATE channel
            // — a lift toward white on top of the plate colour — because "more of
            // the same colour" is not a visible difference, which is exactly how
            // the first draft's boundaries disappeared.
            fill = lerp_colour(fill, plate_colour(me), 0.80f);
            if (boundary)
                fill = lerp_colour(fill, IM_COL32(255, 255, 245, 255), 0.45f);
        }
        // Player-owned tile? Drives the persistent, lens-independent ownership
        // accent: a subtle wash at the plain default plus an outline under every
        // lens (drawn in the border pass), so "these tiles are mine" reads without
        // the player having to pick the Corporation lens.
        const bool is_player_tile = has_owner && corp_it->second == w.player_entity;
        // Plain-default wash: tint the player's own tiles with the player identity
        // colour when no lens is active. Suppressed under any lens — the outline
        // carries identity there, so the wash never fights a lens fill.
        //
        // BL-596 removed the `!built` exemption that used to stand here. It existed
        // because a built tile's owner PLATE already was the player's identity colour;
        // with the plate gone, exempting built tiles would punch a hole in the middle
        // of the player's own footprint — the one tile in a cluster that does not read
        // as theirs would be the one they built on.
        if (is_player_tile && state.overlay == overlay_mode::none)
            fill = lerp_colour(fill, corp_identity(w.player_entity), 0.30f);

        const bool   selected  = (id == state.selected_entity);

        // Placement-suitability overlay (BL-010): active only in construction mode.
        // Invalid tiles (can't build) darken; affine tiles (the armed target is their
        // richest extractable) tint green. Skips the selected tile (already outlined).
        if (suitability_active && !selected)
        {
            const bool placeable = suitability_placeable(
                w, id, tile, suitability_btype, suitability_target);
            if (!placeable)
                fill = lerp_colour(fill, IM_COL32(0, 0, 0, 255), 0.35f);
            else if (suitability_affine_kind)
            {
                bool any_dep = false;
                const resource_type best = placement_rules::richest_extractable(tile, any_dep);
                if (any_dep && best == suitability_target)
                    fill = lerp_colour(fill, IM_COL32(100, 200, 100, 255), 0.24f);
            }
        }

        // Landform relief (BL-231). Composited HERE — after every lens tint and the
        // suitability wash — because composition owns hue and the lenses composite over
        // that hue at 0.6–0.80 alpha; relief folded into the base fill would be buried
        // exactly when a lens is active. Landform drives movement cost (×1.0–×2.0), hazard,
        // habitability and mineral richness, and that cost applies whether or not a lens
        // is on, so this is always-on terrain chrome rather than an overlay_mode.
        //
        // BL-596 removed the `!built` gate here too. It was the plate's argument —
        // shading an identity fill would muddy whose it is — and with the plate gone
        // a built tile's fill IS terrain, so relief applies to it exactly as it does
        // to the ground next door. The landform under a building does not stop being
        // a landform because something was sited on it.
        fill = landform_relief(fill, tile.landform);

        // Survey mask (BL-067): tiles in regions not yet revealed render as a dark
        // locked overlay with no lens detail, borders, markers, or hit-testing — the
        // locked fill is drawn, then the per-copy detail is skipped. A fully surveyed
        // body (home, the star, or a completed survey) reveals everything.
        //
        // BL-408 spectator god view: the mask lifts AT THE DRAW CALL only —
        // `survey_tile_visible` and the survey store are untouched, so nothing the
        // sim or the scorer reads moves. An unsurveyed tile is NOT rendered as
        // ordinary ground: it keeps a heavy wash of the lock colour (the tell), so
        // where the fog still sits — the corps' own blindness — stays legible even
        // while the watcher sees through it. The header's UNSURVEYED/Surveying
        // suffix stays for the same reason.
        const bool surveyed = survey_tile_visible(body.survey, gw, gh, tile.grid_x, tile.grid_y);
        const bool revealed = surveyed || god_view_lift;
        if (!surveyed)
            fill = god_view_lift ? lerp_colour(fill, IM_COL32(12, 14, 20, 255), 0.62f)
                                 : IM_COL32(12, 14, 20, 255);

        // Intra-body vision fog (BL-151/152/154): dim any *revealed* tile by how little
        // the player sees it. Vision = 1 in permanent vision (building pockets + the
        // corp-centre→market corridors, from update_body_vision); otherwise the moving
        // convoy beam's contribution (bright at the head, dimming down the tail). The
        // fog wash scales with (1 − vision), so the surface reads mostly unknown, lit
        // along the player's corridors, with a convoy's beam gliding and trailing over
        // them. Survey mask owns unrevealed tiles, so this skips them. The road pass
        // below reads the same scalar (BL-185) — hence the hoist out of the branch.
        const float vision = vision_rc(tile.grid_x, tile.grid_y);
        if (surveyed) // the survey mask (or its god-view tell) owns unsurveyed tiles
            fill = fog_dim(fill, vision);
        return fill;
    };

    // The lens's answer for one tile ON THE BAKED GROUND (BL-1250; LENSES.md § A
    // lens washes the rendered ground): the colour compute_tile_fill blends in for
    // the active lens, as a translucent wash at its family's strength. Zero = the
    // lens has no answer here; the plain ground shows. The lens SEMANTICS are
    // compute_tile_fill's, line for line — which tiles answer, which colour; only
    // the composite changed (alpha over the bake, not a lerp over a terrain hue).
    // Relief, the survey mask and the fog are not here: the bake carries relief,
    // on_bake is surveyed by definition, and the fog is the wash list after this.
    const auto lens_wash_of = [&](entity_id id, const tile_component& tile) -> ImU32
    {
        const overlay_mode ov = state.overlay;
        if (ov == overlay_mode::corporation || ov == overlay_mode::company)
        {
            const bool maybe_owned = !raster_ok
                || (tile_flags[static_cast<std::size_t>(tile.grid_y) * gw + tile.grid_x] & 2u) != 0;
            const auto corp_it = maybe_owned ? tile_to_corp.find(id) : tile_to_corp.end();
            if (corp_it == tile_to_corp.end())
                return 0u;
            const bool want_bg = (ov == overlay_mode::company);
            if (is_background_firm(corp_it->second) != want_bg)
                return 0u;
            const ImU32 c = state.lens_owner_picked(corp_it->second, want_bg)
                                ? corp_identity(corp_it->second)
                                : palette::owned_grey;
            return with_alpha(c, k_lens_wash_categorical);
        }
        if (ov == overlay_mode::resource)
        {
            resource_type on_tile[ui_state::k_lens_resource_cap];
            if (lens_resources_on_tile(state, tile, on_tile) > 0)
                return with_alpha(presentation_of(on_tile[0]).colour, k_lens_wash_categorical);
            const ImU32 pale = placement_rules::is_water_tile(tile.substrate)
                                   ? IM_COL32(206, 216, 230, 255) : IM_COL32(244, 242, 236, 255);
            return with_alpha(pale, k_resource_ground_wash_alpha);
        }
        if (ov == overlay_mode::market)
        {
            const auto col_it = market_catchment_colour.find(market_of(id, tile));
            return col_it != market_catchment_colour.end()
                       ? with_alpha(col_it->second, k_lens_wash_categorical) : 0u;
        }
        if (ov == overlay_mode::population)
        {
            if (placement_rules::is_water_tile(tile.substrate))
                return 0u;
            const float eff = workforce_efficiency(std::clamp(tile.habitability, 0.0f, 1.0f));
            return with_alpha(ryg_colour(eff), k_lens_wash_sequential);
        }
        if (ov == overlay_mode::scarcity)
        {
            const auto sf_it = scar_shortfall.find(market_of(id, tile));
            if (sf_it == scar_shortfall.end() || scar_max_shortfall <= 0.0f)
                return 0u;
            const float scar = std::clamp(sf_it->second / scar_max_shortfall, 0.0f, 1.0f);
            return scar > 0.0f
                ? with_alpha(IM_COL32(220, 70, 55, 255), k_lens_wash_sequential * scar) : 0u;
        }
        if (ov == overlay_mode::industry)
        {
            const auto it = industry_field.find(id);
            if (it == industry_field.end() || industry_max <= 0.0f)
                return 0u;
            const float t = std::clamp(it->second / industry_max, 0.0f, 1.0f);
            // The fill path's 0.15 + 0.6t ramp, rescaled so its top is the family strength.
            return with_alpha(IM_COL32(210, 150, 70, 255),
                              k_lens_wash_sequential * (0.15f + 0.6f * t) / 0.75f);
        }
        if (ov == overlay_mode::throughput && state.lp_reach_max > 0.0f)
        {
            const float rc = tile_reach_cost(w, id);
            if (rc < 0.0f)
                return 0u;
            const float denom = (state.lp_reach_p90 > 0.0f) ? state.lp_reach_p90
                                                            : state.lp_reach_max;
            const float t = std::isinf(rc)
                ? 0.0f
                : 1.0f - std::sqrt(std::clamp(rc / denom, 0.0f, 1.0f));
            return with_alpha(throughput_field_colour(t), k_lens_wash_sequential);
        }
        if (ov == overlay_mode::continent && plates)
        {
            const int idx = tile.grid_x + tile.grid_y * gw;
            const int me  = plates->plate_id[static_cast<std::size_t>(idx)];
            bool boundary = false;
            const int cols[2] = { (tile.grid_x + 1) % gw, (tile.grid_x + gw - 1) % gw };
            for (const int c : cols)
                if (plates->plate_id[static_cast<std::size_t>(c + tile.grid_y * gw)] != me) boundary = true;
            for (const int dy : { -1, 1 })
            {
                const int ry = tile.grid_y + dy;
                if (ry < 0 || ry >= gh) continue;
                if (plates->plate_id[static_cast<std::size_t>(tile.grid_x + ry * gw)] != me) boundary = true;
            }
            // The boundary lift keeps its own channel: toward white, on the plate hue.
            ImU32 c = plate_colour(me);
            if (boundary)
                c = lerp_colour(c, IM_COL32(255, 255, 245, 255), 0.45f);
            return with_alpha(c, k_lens_wash_categorical);
        }
        return 0u;
    };

    // One-pass-ahead shade cache over the visible band (+1 row each way, so a
    // top/bottom-row tile still finds its corner neighbours). Sized to the whole
    // raster and reused across frames; only the band is written each frame, and
    // only the band is read.
    static std::vector<tile_shade> shade_cache;
    shade_cache.assign(static_cast<std::size_t>(gw) * gh, tile_shade{});
    const int row_cache_lo = std::max(0,      row_lo - 1);
    const int row_cache_hi = std::min(gh - 1, row_hi + 1);

    // COLUMN CULL for the band passes (the 60 fps budget, PLANETARY.md
    // § Draw-loop cost model). The tile loop has always skipped a column with no
    // visible wrap copy, but the passes that run one ahead of it — this shade
    // cache and the border depth — walked every column of the band, and even at
    // the widest rung the body is wider than the canvas. A column is live when
    // either row parity of it has a copy on screen; the mask is then widened by
    // the one-tile neighbourhood those passes read (the blend corners, the
    // frontier test). An unmasked column falls back to the lookup.
    static std::vector<uint8_t> col_live;
    col_live.assign(static_cast<std::size_t>(std::max(gw, 0)), 0u);
    {
        static std::vector<uint8_t> col_seen;
        col_seen.assign(col_live.size(), 0u);
        for (int cc = 0; cc < gw; ++cc)
            for (int parity = 0; parity < 2; ++parity)
            {
                const float sx = to_screen(hex_local_centre(cc, parity, hex_size)).x;
                const int k_lo = (period_px > 0.0f)
                    ? static_cast<int>(std::ceil((visible_left  - sx) / period_px)) : 0;
                const int k_hi = (period_px > 0.0f)
                    ? static_cast<int>(std::floor((visible_right - sx) / period_px)) : 0;
                if (k_lo <= k_hi)
                    col_seen[static_cast<std::size_t>(cc)] = 1u;
            }
        const int margin = std::max(1, k_border_band_tiles);
        for (int cc = 0; cc < gw; ++cc)
        {
            if (!col_seen[static_cast<std::size_t>(cc)])
                continue;
            for (int d = -margin; d <= margin; ++d)
                col_live[static_cast<std::size_t>(((cc + d) % gw + gw) % gw)] = 1u;
        }
    }
    const bool lens_blends = lens_blend_mode(state.overlay);
    // The same band's tile records and nation owners, looked up ONCE a frame:
    // the passes below read a tile's neighbours again and again (the border
    // depth, the border rule, the road and lane chains), and at the widest rung
    // that was ~30k tiles x six hash lookups per pass (the 60 fps budget,
    // PLANETARY.md § Draw-loop cost model). Rows outside the band fall back to
    // the lookup (tile_rc / nation_rc below).
    static std::vector<const tile_component*> tile_cache;
    tile_cache.assign(static_cast<std::size_t>(gw) * gh, nullptr);
    static std::vector<entity_id> nation_cache;
    const bool cache_nations = state.overlay == overlay_mode::none
                            && !state.dbg_hide_border_band;
    if (cache_nations)
        nation_cache.assign(static_cast<std::size_t>(gw) * gh, null_entity);
    for (int cr = row_cache_lo; cr <= row_cache_hi; ++cr)
    for (int cc = 0; cc < gw; ++cc)
    {
        if (!col_live[static_cast<std::size_t>(cc)])
            continue;
        const std::size_t ci = static_cast<std::size_t>(cr) * gw + cc;
        const entity_id cid  = raster_ok ? raster[ci] : null_entity;
        if (cid == null_entity)
            continue;
        const tile_component& ct = w.tiles.at(cid);
        tile_cache[ci] = &ct;
        if (cache_nations)
            nation_cache[ci] = nation_of(cid);
        tile_shade& sh = shade_cache[ci];
        // On the baked ground a surveyed tile's fill is never drawn (the loop's
        // on_bake branch) — skip the derivation, the costliest per-tile work at
        // the wide rungs, and leave the province to be looked up where a pass
        // asks for it. The god view keeps both: there a lifted tile beside a
        // surveyed one blends with its fill.
        if (ground_on && !god_view_lift
            && survey_tile_visible(body.survey, gw, gh, ct.grid_x, ct.grid_y))
        {
            sh.blend = false;
            continue;
        }
        sh.province       = w.provinces.province_of(cid);
        sh.province_known = true;
        sh.fill        = compute_tile_fill(cid, ct);
        // A tile joins the blend only when it is ordinary ground under a lens
        // whose field is continuous. A survey-masked tile is excluded: the lock
        // fill is a statement about knowledge, not terrain, and smearing it would
        // leak the shape of ground the player has not surveyed.
        //
        // A BUILT tile used to be excluded here as well, and BL-596 dropped that.
        // The exclusion's whole argument was the plate — "smearing a corp identity
        // across unbuilt ground would put ownership on land nobody owns" — and the
        // plate is gone. A built tile's fill is now terrain, so blending it is
        // blending terrain. Keeping the exclusion would have left the built hex as
        // the one crisp, 1 px-bordered cell inside an otherwise seamless province:
        // a plate-shaped hole where the plate used to be.
        const bool ct_seen  = survey_tile_visible(body.survey, gw, gh, ct.grid_x, ct.grid_y)
                              || god_view_lift;
        //
        // WATER IS EXCLUDED EXPLICITLY (BL-516). It used to fall out of
        // `province != 0` for free, because the partition covered land only —
        // and that accident is what has been keeping the coastline crisp. Sea
        // tiles carry provinces now, so the exclusion has to be stated. It is
        // stated rather than dropped deliberately: a province never spans land
        // and water, so blending water WOULD still leave the shoreline hard, but
        // it would change how the sea reads, and that is a look nobody has seen
        // yet. This change ships the pre-BL-516 rendering unchanged; softening
        // the sea is a separate, visually-reviewed call.
        sh.blend = lens_blends && ct_seen && sh.province != 0
                   && !is_water(ct.substrate);
    }
    pm.mark(canvas_pass_meter::shade, dl);

    // The band caches with their fallback (a row outside the band, or a lens up).
    const auto in_cache = [&](int row) { return row >= row_cache_lo && row <= row_cache_hi; };
    const auto tile_rc = [&](int col, int row) -> const tile_component* {
        if (!raster_ok || col < 0 || col >= gw || row < 0 || row >= gh)
            return nullptr;
        const std::size_t ci = static_cast<std::size_t>(row) * gw + col;
        if (in_cache(row) && tile_cache[ci])
            return tile_cache[ci];
        const auto it = w.tiles.find(raster[ci]);
        return it != w.tiles.end() ? &it->second : nullptr;
    };
    const auto nation_rc = [&](int col, int row) -> entity_id {
        if (!raster_ok || col < 0 || col >= gw || row < 0 || row >= gh)
            return null_entity;
        const std::size_t ci = static_cast<std::size_t>(row) * gw + col;
        return (cache_nations && in_cache(row) && col_live[static_cast<std::size_t>(col)])
            ? nation_cache[ci] : nation_of(raster[ci]);
    };

    // --- BL-601: the national border band's inward falloff ------------------
    // Per-tile depth from the nation's frontier: 0 = touching a foreign owner
    // (another nation, or unclaimed ground — a coastline is a border too), 1 and
    // 2 = one and two tiles in, 0xFF = beyond the band or claimed by nobody.
    // This is the "colour extending inwards" of Ben's ruling, made a scalar: the
    // wash below reads it straight out of k_border_band_alpha.
    //
    // Computed over the visible band PLUS the band depth in rows each way, so a
    // visible tile's chain back to its own frontier is complete. Depth 0 is
    // marked from `nation_of` directly (a raster index, not the cache), so the
    // top and bottom rows of the computed range are still correct; only the
    // relaxation is short there, and those rows are already off screen.
    //
    // COARSE ZOOM TAKES DEPTH 0 ONLY. At the whole-grid view every one of the
    // ~15k hexes is in range and a three-ring relaxation over all of them is the
    // one place this pass could cost real time. A single ring still draws the
    // political outline, which is the whole read at that zoom.
    static std::vector<uint8_t> border_depth;
    border_depth.assign(static_cast<std::size_t>(gw) * gh, 0xFFu);
    // Which KIND of frontier a banded tile belongs to: 1 where the nation faces
    // another nation, 0 where it faces only unclaimed ground (Ben, 2026-08-24).
    // Carried beside the depth rather than recomputed at the wash, because a
    // depth-1 or depth-2 tile is not on the frontier at all and cannot answer the
    // question by looking at its own neighbours — it inherits the answer from the
    // frontier tile that seeded it.
    static std::vector<uint8_t> border_political;
    border_political.assign(static_cast<std::size_t>(gw) * gh, 0u);
    const int band_depth = coarse_fill ? 1 : k_border_band_tiles;
    const int band_lo    = std::max(0,      row_lo - band_depth);
    const int band_hi    = std::min(gh - 1, row_hi + band_depth);
    // SUPPRESSED UNDER EVERY LENS (Ben, 2026-08-28). The band is always-on chrome
    // on the plain canvas — that is what BL-601 made it, and it is why the default
    // view IS the country view. But a lens is a question about one subject, and a
    // national wash over a corporate or resource read competes with the answer.
    // Ben, on the Corporation lens: "we will not display country borders when
    // displaying that lens. This will bring focus on to corporations" — extended
    // to all lenses on the same instruction.
    //
    // Gated on the COMPUTATION, not just the wash below, so the three-ring
    // relaxation over the visible band costs nothing while a lens is up. Every
    // depth stays 0xFF, and the wash's `depth < band_depth` test then fails on
    // its own without needing a second guard.
    const bool draw_border_band = (state.overlay == overlay_mode::none)
                               && !state.dbg_hide_border_band;
    if (draw_border_band && raster_ok && !w.tile_to_nation.empty())
    {
        for (int cr = band_lo; cr <= band_hi; ++cr)
        for (int cc = 0; cc < gw; ++cc)
        {
            if (!col_live[static_cast<std::size_t>(cc)])
                continue;
            const std::size_t ci = static_cast<std::size_t>(cr) * gw + cc;
            const entity_id cid  = raster[ci];
            if (cid == null_entity)
                continue;
            const entity_id nat = nation_rc(cc, cr);
            if (nat == null_entity)
                continue; // Unclaimed ground has no colour to extend inwards.
            const int (*off)[2] = hex_neighbors::offsets(cr);
            for (int n = 0; n < 6; ++n)
            {
                const int nrow = cr + off[n][1];
                if (nrow < 0 || nrow >= gh)
                    continue; // Off the pole: no neighbour, so not a frontier.
                int ncol = (cc + off[n][0]) % gw;
                if (ncol < 0)
                    ncol += gw;
                const entity_id nb_nat = nation_rc(ncol, nrow);
                if (nb_nat != nat)
                {
                    border_depth[ci] = 0u;
                    // Do NOT break on the first foreign edge: a tile can face
                    // both a neighbour nation and open ground, and the political
                    // claim is the stronger one. Keep looking until a political
                    // edge is found, so a coastal frontier between two countries
                    // is not quietly faded by the sea on its other side.
                    if (nb_nat != null_entity)
                    {
                        border_political[ci] = 1u;
                        break;
                    }
                }
            }
        }
        for (uint8_t d = 1; d < static_cast<uint8_t>(band_depth); ++d)
        {
            for (int cr = band_lo; cr <= band_hi; ++cr)
            for (int cc = 0; cc < gw; ++cc)
            {
                if (!col_live[static_cast<std::size_t>(cc)])
                    continue;
                const std::size_t ci = static_cast<std::size_t>(cr) * gw + cc;
                if (border_depth[ci] != 0xFFu)
                    continue;
                const entity_id cid = raster[ci];
                if (cid == null_entity)
                    continue;
                const entity_id nat = nation_rc(cc, cr);
                if (nat == null_entity)
                    continue;
                const int (*off)[2] = hex_neighbors::offsets(cr);
                for (int n = 0; n < 6; ++n)
                {
                    const int nrow = cr + off[n][1];
                    if (nrow < band_lo || nrow > band_hi)
                        continue;
                    int ncol = (cc + off[n][0]) % gw;
                    if (ncol < 0)
                        ncol += gw;
                    const std::size_t ni = static_cast<std::size_t>(nrow) * gw + ncol;
                    // Same nation only: depth propagates INSIDE a territory. A
                    // neighbour across the frontier is a different nation's
                    // band and must never seed this one's.
                    if (border_depth[ni] == d - 1u && nation_rc(ncol, nrow) == nat)
                    {
                        border_depth[ci]     = d;
                        border_political[ci] = border_political[ni];
                        break;
                    }
                }
            }
        }
    }

    pm.mark(canvas_pass_meter::band_depth, dl);
    // The two provinces the outline can light (BL-511): selected and hovered.
    const province* sel_prov = state.selected_province != 0
                             ? w.provinces.find(state.selected_province) : nullptr;
    const province* hov_prov = state.hovered_province != 0
                             ? w.provinces.find(state.hovered_province) : nullptr;
    for (int t_row = row_lo; t_row <= row_hi; ++t_row)
    for (int t_col = 0; t_col < gw; ++t_col)
    {
        const entity_id id = raster[static_cast<std::size_t>(t_row) * gw + t_col];
        if (id == null_entity)
            continue;
        // The raster is row-major on the grid, so (t_col, t_row) IS the tile's
        // grid position: the cull below needs no tile record.
        const ImVec2 lc   = hex_local_centre(t_col, t_row, hex_size);
        const ImVec2 sc   = to_screen(lc);

        // Column cull: the horizontal wrap-window (computed once here, reused by
        // the draw below). No wrap copy of this tile lands inside the canvas —
        // skip before the built/owner/lens work, which is the expensive part.
        const int k_min = (period_px > 0.0f)
            ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
        const int k_max = (period_px > 0.0f)
            ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;
        if (k_min > k_max)
            continue;
        const tile_component& tile = *tile_rc(t_col, t_row);

        // Does this tile carry a building, and who owns it? Resolved once here so the
        // marker pass below reuses the one lookup. It no longer feeds the FILL — since
        // BL-596 a built tile fills as terrain like any other.
        const uint8_t tflags    = tile_flags[static_cast<std::size_t>(t_row) * gw + t_col];
        const bool   built      = (tflags & 1u) != 0;
        const auto   corp_it    = (tflags & 2u) ? tile_to_corp.find(id) : tile_to_corp.end();
        const bool   has_owner  = corp_it != tile_to_corp.end();
        // (BL-1241: the marker's owner colour and type identity retired with the
        // marker — the structure is baked into the ground, keyed by the bake.)

        // Fill starts as the tile's terrain colour, on EVERY tile (BL-596 retired the
        // built tile's owner plate). Lens tints then composite over it — a lens is a
        // mode the player chose, so suppressing it where the player's own assets sit
        // would blind it where it matters most.
        //
        // Under the Faction lens a tile owned by a nation is tinted that nation's
        // identity colour. The tint is a direct replacement (no blend); unclaimed
        // tiles — absent from tile_to_nation, e.g. ocean — keep their terrain hue so
        // the political map still reads as terrain underneath.
        // Fill comes from the province shade cache (BL-511), computed one pass
        // ahead so a tile's neighbours' colours are in hand for the blend. The
        // three scalars the rest of the loop still needs are recomputed here:
        // they are map lookups, not the colour derivation.
        const bool is_player_tile = has_owner && corp_it->second == w.player_entity;
        const bool selected       = (id == state.selected_entity);
        const bool surveyed = survey_tile_visible(body.survey, gw, gh, tile.grid_x, tile.grid_y);
        const bool revealed = surveyed || god_view_lift;
        const float vision  = vision_rc(t_col, t_row);

        const std::size_t shade_idx = static_cast<std::size_t>(t_row) * gw + t_col;
        const tile_shade& shade     = shade_cache[shade_idx];
        const ImU32       fill      = shade.fill;
        // The province, looked up only when a pass asks (the bake skips it).
        uint32_t prov_id_v     = shade.province;
        bool     prov_id_known = shade.province_known;
        const auto prov_id_of  = [&]() -> uint32_t {
            if (!prov_id_known)
            {
                prov_id_v     = w.provinces.province_of(id);
                prov_id_known = true;
            }
            return prov_id_v;
        };

        // BL-732: this tile's ground is carried by the baked texture drawn
        // before the loop — skip the per-tile fill and texture emit (the
        // no-grid rule), and re-express the washes compute_tile_fill folded
        // into the fill as translucent overlays instead. A survey-masked tile
        // keeps the old path (the lock fill is knowledge, not terrain), which
        // also keeps the god-view tell byte-identical.
        const bool on_bake = ground_on && surveyed;

        // Corner colours for the blend. Each hex corner is shared with two of the
        // six neighbours (k_corner_sides); a corner takes the MEAN of this tile's
        // fill and those of its corner-sharing neighbours that are in the SAME
        // province AND themselves blending. An out-of-province neighbour
        // contributes nothing, so the province boundary keeps its colour step
        // while every seam inside the province vanishes — which is the whole
        // effect. A tile that does not blend (built, masked, or under a lens whose
        // field is categorical or sparse — see PLANETARY.md's reduction table)
        // renders exactly as it did before this item.
        ImU32 corner_col[6] = { fill, fill, fill, fill, fill, fill };
        if (shade.blend && !on_bake) // on the bake the fill is not drawn at all
        {
            for (int v = 0; v < 6; ++v)
            {
                ImU32 acc[3] = { fill, 0u, 0u };
                int   n      = 1;
                for (int s = 0; s < 2; ++s)
                {
                    const int side = k_corner_sides[v][s];
                    const auto nc  = hex_neighbors::neighbour(t_col, t_row, side);
                    const int  ncol = ((nc.gx % gw) + gw) % gw; // cylinder wrap
                    const int  nrow = nc.gy;
                    if (nrow < row_cache_lo || nrow > row_cache_hi)
                        continue;
                    const tile_shade& ns =
                        shade_cache[static_cast<std::size_t>(nrow) * gw + ncol];
                    // BL-514 (Ben, 2026-08-22): the blend CROSSES province
                    // borders. The province-match term that used to stand here
                    // is deliberately gone — land is one continuous field now,
                    // and a province edge is drawn only as the selection outline.
                    // `ns.blend` still carries the two exclusions that remain:
                    // ocean never sets it (it needs province != 0) and built
                    // tiles never set it, so installations and coastline stay
                    // crisp exactly as before.
                    if (!ns.blend || prov_id_of() == 0)
                        continue;
                    acc[n++] = ns.fill;
                }
                // BL-597: the corner does not take the flat mean any more — it
                // travels from the tile's OWN fill toward that mean by
                // k_land_blend_strength. At 1.0 lerp_colour returns the mean
                // unchanged (u = 0, and an 8-bit channel round-trips a float
                // multiply by 1.0 exactly), so the constant's top of range is the
                // pre-BL-597 render byte for byte — the guard the item names.
                corner_col[v] = lerp_colour(fill, mean_colour(acc, n),
                                            k_land_blend_strength);
            }
        }


        pm.mark(canvas_pass_meter::tile_misc, dl);
        // The lens wash (BL-1250), derived once per tile, not once per wrap copy.
        ImU32 lens_wc      = 0u;
        bool  lens_wc_done = false;
        // Wrap copies inside the canvas: k_min/k_max were computed at the top of
        // the loop body (BL-268), where they double as the column cull.
        for (int k = k_min; k <= k_max; ++k)
        {
            const float cx = sc.x + static_cast<float>(k) * period_px;
            const float cy = sc.y;

            // The vertices are still needed below — hover and selection outline this
            // hex, and those read as hexes at any zoom. Computing them is arithmetic;
            // it is EMITTING them as a filled 6-gon that costs.
            ImVec2 verts[6];
            hex_vertices(verts, cx, cy, draw_r);

            // A BLENDING tile is drawn at the FULL circumradius, not at draw_r.
            // draw_r is `hex_size * zoom - 1`, and that 1 px is the whole reason a
            // hex grid reads as a grid — the background showing through as a
            // border. Softening the province means giving that gap up INSIDE the
            // province: adjacent blended hexes share their edges exactly, the
            // corner colours already agree there, and the seam stops existing.
            // Every non-blending tile keeps draw_r and its 1 px border, so a built
            // installation and a categorical lens block stay crisp.
            ImVec2 blend_verts[6];
            if (shade.blend)
                hex_vertices(blend_verts, cx, cy, draw_r + 1.0f);

            // Coarse fill below the LOD threshold (BL-269): a rect instead of a
            // 6-gon, ~4 vertices against ~10 and no AA fringe.
            //
            // SIZED TO THE GRID STEP, not to the hex radius. Rows step by
            // 1.5 * hex_size while a hex is 2 * hex_size tall — consecutive rows
            // OVERLAP. A rect built from the radius is shorter than the row pitch and
            // the terrain renders as horizontal stripes, which is exactly what the
            // first attempt did. Because odd rows are offset by half a column, rects
            // of (col_step x row_step) brick-lay and tile the plane exactly.
            //
            // `hex_size * zoom` is recovered as `draw_r + 1` (draw_r is that minus the
            // 1 px border shrink), and the same 1 px is taken back off each axis so the
            // background still shows through as the grid texture the hexes give.
            if (on_bake)
            {
                // The baked ground already carries terrain, relief and grain.
                // What the fill path also carried — the player-identity wash,
                // the construction-suitability washes, the vision fog — draws
                // as translucent full-radius washes over the bake, at the
                // same strengths compute_tile_fill blends them in at.
                //
                // Collected, not drawn here: they emit into the wash channel
                // once the loop is done (emit_ground). At the coarse fill they
                // are the grid-step rect (the fill LOD), and the FOG — the one
                // wash on nearly every tile — joins equal neighbours in a row
                // into one rect: a few hundred quads for the whole body where
                // it was 15k anti-aliased hexes (PLANETARY.md § Draw-loop cost
                // model, the 60 fps budget).
                const float step = draw_r + 1.0f; // hex_size * zoom
                const float whw  = kSqrt3 * step * 0.5f;
                const float whh  = 0.75f  * step;
                const auto wash = [&](ImU32 c) {
                    if (coarse_fill)
                        washes.push_back({ {}, 0.0f, { cx - whw, cy - whh },
                                           { cx + whw, cy + whh }, c, false });
                    else
                        washes.push_back({ { cx, cy }, step, {}, {}, c, true });
                };
                // The lens's answer (BL-1250): a wash of its own list, joined
                // along a row at the coarse fill exactly as the fog is (a
                // catchment or a plate is long runs of one colour).
                if (state.overlay != overlay_mode::none)
                {
                    resource_type on_tile[ui_state::k_lens_resource_cap];
                    const int n_res = (state.overlay == overlay_mode::resource && !coarse_fill)
                        ? lens_resources_on_tile(state, tile, on_tile) : 0;
                    if (n_res >= 2)
                    {
                        // A split tile: equal wedges, each at the wash strength.
                        ground_wedges wg{ { cx, cy }, step, {}, n_res };
                        for (int q = 0; q < n_res; ++q)
                            wg.cols[q] = with_alpha(presentation_of(on_tile[q]).colour,
                                                    k_lens_wash_categorical);
                        lens_wedges.push_back(wg);
                    }
                    else if (const ImU32 lc = lens_wc_done
                                 ? lens_wc
                                 : (lens_wc_done = true, lens_wc = lens_wash_of(id, tile));
                             (lc & IM_COL32_A_MASK) != 0u)
                    {
                        // The run is per wrap COPY: with two copies on screen the
                        // list's back is the other copy's rect, never a neighbour.
                        int& run = lens_run[static_cast<unsigned>(k) & 3u];
                        ground_wash* last = (run >= 0 && run < static_cast<int>(lens_washes.size()))
                                                ? &lens_washes[static_cast<std::size_t>(run)] : nullptr;
                        if (coarse_fill && last && !last->hex && last->col == lc
                            && last->p0.y == cy - whh
                            && std::fabs(last->p1.x - (cx - whw)) < 0.01f)
                            last->p1.x = cx + whw;
                        else if (coarse_fill)
                        {
                            run = static_cast<int>(lens_washes.size());
                            lens_washes.push_back({ {}, 0.0f, { cx - whw, cy - whh },
                                                    { cx + whw, cy + whh }, lc, false });
                        }
                        else
                            lens_washes.push_back({ { cx, cy }, step, {}, {}, lc, true });
                    }
                }
                // The close-zoom seam: sides 0-2 (E, NE, NW) of every tile on
                // the bake — each shared edge is exactly one tile's side in that
                // set and the other's side + 3, so every edge draws once.
                if (seam_on)
                {
                    ImVec2 sv[6];
                    hex_vertices(sv, cx, cy, step);
                    for (int side = 0; side < 3; ++side)
                    {
                        const auto nc = hex_neighbors::neighbour(t_col, t_row, side);
                        if (nc.gy < 0 || nc.gy >= gh)
                            continue; // off the pole: no neighbour, no seam
                        seam_segs.push_back(sv[k_side_verts[side][0]]);
                        seam_segs.push_back(sv[k_side_verts[side][1]]);
                    }
                }
                if (is_player_tile)
                {
                    const ImU32 pid = corp_identity(w.player_entity);
                    wash((pid & ~IM_COL32_A_MASK) | (ImU32(77) << IM_COL32_A_SHIFT)); // 0.30
                }
                if (suitability_active && !selected)
                {
                    const bool placeable = suitability_placeable(
                        w, id, tile, suitability_btype, suitability_target);
                    if (!placeable)
                        wash(IM_COL32(0, 0, 0, 90)); // 0.35
                    else if (suitability_affine_kind)
                    {
                        bool any_dep = false;
                        const resource_type best =
                            placement_rules::richest_extractable(tile, any_dep);
                        if (any_dep && best == suitability_target)
                            wash(IM_COL32(100, 200, 100, 61)); // 0.24
                    }
                }
                if (vision < 1.0f)
                {
                    // fog_dim's wash (lerp toward (8,10,16) by 0.5*(1-vision)),
                    // as an alpha overlay over the baked ground.
                    const int fa = static_cast<int>(std::lround(127.5f * (1.0f - vision)));
                    if (fa > 0)
                    {
                        const ImU32 fc = IM_COL32(8, 10, 16, fa);
                        // Join the previous rect when it is this row's left
                        // neighbour in the same copy, in the same colour, and
                        // nothing was washed between them (so order holds).
                        ground_wash* last = washes.empty() ? nullptr : &washes.back();
                        if (coarse_fill && last && !last->hex && last->col == fc
                            && last->p0.y == cy - whh
                            && std::fabs(last->p1.x - (cx - whw)) < 0.01f)
                            last->p1.x = cx + whw;
                        else
                            wash(fc);
                    }
                }
            }
            else if (coarse_fill)
            {
                const float step = draw_r + 1.0f; // hex_size * zoom
                const float hw   = kSqrt3 * step * 0.5f - 0.5f;
                const float hh   = 1.5f   * step * 0.5f - 0.5f;
                dl->AddRectFilled({ cx - hw, cy - hh }, { cx + hw, cy + hh }, fill);
            }
            else if (shade.blend)
                prim_blended_hex(dl, blend_verts, { cx, cy }, fill, corner_col);
            else
                dl->AddConvexPolyFilled(verts, 6, fill);

            // Resource lens split (Ben, 2026-10-04): a surveyed tile carrying two or
            // more toggled resources is cut into equal pie wedges from the centre,
            // one per resource in toggle order. Below the coarse LOD the hex is a
            // few pixels across and the rect keeps the first resource's colour.
            // (On the bake the wedges are washes, collected above.)
            if (state.overlay == overlay_mode::resource && surveyed && !coarse_fill && !on_bake)
            {
                resource_type on_tile[ui_state::k_lens_resource_cap];
                const int n = lens_resources_on_tile(state, tile, on_tile);
                if (n >= 2)
                {
                    ImU32 cols[ui_state::k_lens_resource_cap];
                    for (int q = 0; q < n; ++q)
                        cols[q] = presentation_of(on_tile[q]).colour | IM_COL32_A_MASK;
                    draw_lens_wedges(dl, { cx, cy }, shade.blend ? draw_r + 1.0f : draw_r, cols, n);
                }
            }

            // Terrain texture (BL-520). Immediately after the fill and BEFORE the
            // province edge stroke, so the stroke stays the topmost ground mark
            // and a province border is never broken up by a canopy tick.
            //
            // Gated on `revealed`: a cover pattern IS terrain information, and
            // drawing it through the survey mask would leak the shape of ground
            // the player has not paid to survey (DISCOVERY.md's geographic fog).
            // BL-596 dropped the `!built` gate. It said a built hex is not ground —
            // true while the plate stood, false now that the hex under a building is
            // ordinary terrain. Ben's ruling is that terrain, texture and the live
            // lens wash all keep showing under the glyph.
            // Never drawn under `coarse_fill`, which is implied: coarse_fill needs
            // draw_r <= 7 and texture_strength is 0 below draw_r 14.
            if (revealed && texture_strength > 0.0f && !on_bake)
                draw_tile_texture(dl, { cx, cy }, draw_r, tile.grid_x, tile.grid_y,
                                  tile.substrate, tile.cover, tile.cover_density,
                                  fill, texture_strength);

            pm.mark(canvas_pass_meter::fill, dl);
            // Province edge: NOT DRAWN. BL-511 stroked every side facing a
            // different province at alpha 105, and NR-417 called that alpha the
            // one dial to move. Ben moved it to zero (BL-514, 2026-08-22):
            // "blur should cross province borders". Since the fill now blends
            // across the boundary as well, a stroke here would be the only thing
            // left asserting a cell — drawing it at any alpha would defeat the
            // change rather than soften it, so the pass is gone rather than
            // dialled down. The crisp affordance is unchanged and still on
            // demand: the hover/selection outline below.

            // Masked region: locked fill only — no borders, markers, selection, or
            // hit-testing for this copy. This single gate also *is* the rival-marker
            // visibility rule (BL-068): a marker — yours or a rival's — shows iff its
            // tile sits in a survey-revealed region, so no separate per-owner gate is
            // needed on the marker pass below. The competitor information asymmetry
            // lives at read time in the hover card and selection panel, not here.
            if (!revealed)
                continue;

            // National border band — the inward falloff (BL-601). Always-on
            // chrome, drawn under every lens exactly as roads are: the national
            // read is terrain-grade context now, not a mode the player enters.
            //
            // Each tile takes ITS OWN nation's colour at an alpha keyed to its
            // depth from the frontier, composited over the finished fill. That
            // per-tile compositing is the guarantee Ben's ruling asks for: two
            // neighbours meeting draw two different colours side by side and
            // never average into a third nation's hue, because no arithmetic in
            // this pass sees more than one nation.
            //
            // Gated on `revealed` for the same reason the province edge was: a
            // border drawn through the survey mask would leak the political
            // shape of ground the player has not paid to survey.
            if (const uint8_t depth = border_depth[shade_idx]; depth < band_depth)
            {
                const entity_id nat = nation_rc(t_col, t_row);
                if (nat != null_entity)
                {
                    // Muted (2026-09-01): the identity colour desaturated for
                    // the band, so the claim reads without shouting over the
                    // painterly ground.
                    const ImU32 nc = muted_nation_colour(palette::nation_colour(nat));
                    const float scale = border_political[shade_idx]
                                        ? 1.0f : k_border_unclaimed_scale;
                    const int   a  = static_cast<int>(k_border_band_alpha[depth] * scale * 255.0f);
                    const ImU32 wash = IM_COL32((nc >> IM_COL32_R_SHIFT) & 0xFFu,
                                                (nc >> IM_COL32_G_SHIFT) & 0xFFu,
                                                (nc >> IM_COL32_B_SHIFT) & 0xFFu, a);
                    if (coarse_fill)
                    {
                        const float step = draw_r + 1.0f;
                        const float hw   = kSqrt3 * step * 0.5f - 0.5f;
                        const float hh   = 1.5f   * step * 0.5f - 0.5f;
                        dl->AddRectFilled({ cx - hw, cy - hh }, { cx + hw, cy + hh }, wash);
                    }
                    else
                    {
                        dl->AddConvexPolyFilled(verts, 6, wash);
                    }
                }
            }

            pm.mark(canvas_pass_meter::band_wash, dl);
            // Owner multi-select rim (BL-1240): ground held by an UNPICKED owner of
            // the lens's kind takes owned_grey AND a dark inset rim. The grey alone
            // is a value, and some terrain sits at that value (urban, regolith,
            // metallic); the rim is the colour-independent channel that says "held,
            // not picked" whatever the ground beside it. Drawn after the border
            // band so the national wash cannot bury it; skipped under coarse_fill,
            // where a hex is a few pixels and a rim would be the whole tile.
            if (!coarse_fill && has_owner
                && (state.overlay == overlay_mode::corporation
                    || state.overlay == overlay_mode::company))
            {
                const bool want_bg = (state.overlay == overlay_mode::company);
                const auto oit     = w.corporations.find(corp_it->second);
                if (oit != w.corporations.end()
                    && oit->second.is_background == want_bg
                    && !state.lens_owner_picked(corp_it->second, want_bg))
                {
                    const float lw = std::max(1.0f, draw_r * 0.10f);
                    ImVec2 rim[6];
                    hex_vertices(rim, cx, cy, std::max(1.0f, draw_r - lw * 0.5f));
                    dl->AddPolyline(rim, 6, IM_COL32(18, 20, 26, 200),
                                    ImDrawFlags_Closed, lw);
                }
            }

            // Resource lens deposit outline (Ben, 2026-10-04: "push up the contrast").
            // Pale identities (silica, sand, rare earth) sit close to the white wash, so
            // every deposit blob is ringed with a DARK stroke on each side facing ground
            // that carries no toggled good. Colour-independent, and it draws the
            // deposit's SHAPE, which is the lens.
            if (state.overlay == overlay_mode::resource && surveyed && !coarse_fill && raster_ok)
            {
                resource_type tmp[ui_state::k_lens_resource_cap];
                if (lens_resources_on_tile(state, tile, tmp) > 0)
                {
                    const ImVec2* ev = shade.blend ? blend_verts : verts;
                    const float   lw = std::max(1.5f, draw_r * 0.16f);
                    for (int side = 0; side < 6; ++side)
                    {
                        const auto nc = hex_neighbors::neighbour(t_col, t_row, side);
                        bool nb_on = false;
                        if (nc.gy >= 0 && nc.gy < gh)
                        {
                            const int ncol = ((nc.gx % gw) + gw) % gw; // cylinder wrap
                            const entity_id nid =
                                raster[static_cast<std::size_t>(nc.gy) * gw + ncol];
                            if (nid != null_entity)
                            {
                                const auto nit = w.tiles.find(nid);
                                nb_on = nit != w.tiles.end()
                                     && lens_resources_on_tile(state, nit->second, tmp) > 0;
                            }
                        }
                        if (!nb_on)
                            dl->AddLine(ev[k_side_verts[side][0]], ev[k_side_verts[side][1]],
                                        IM_COL32(28, 30, 36, 230), lw);
                    }
                }
            }

            // BL-603: the hovered STRUCTURE lights whole. Ben, 2026-08-24 — "the
            // entire market gets highlighted on mouse over". One frame behind, as
            // the province outline already is (see where it is written, below the
            // loop): the loop cannot know the answer before it has drawn the tile
            // that produces it.
            //
            // A wash rather than an outline, deliberately. Outlining a catchment
            // means walking its boundary every frame to find which edges face out;
            // a wash is per tile and costs one test, and it is the truer read
            // anyway — the claim is "all of THIS is one thing", which is an area
            // statement, not an edge one.
            if (state.hovered_structure != null_entity)
            {
                entity_id      tile_struct = null_entity;
                structure_kind sk          = structure_kind::none;
                if (memo_markets)
                {
                    // lens_structure_of_tile's Market/Scarcity case, on the memo.
                    tile_struct = market_of(id, tile);
                    sk = (tile_struct != null_entity) ? structure_kind::market
                                                      : structure_kind::none;
                }
                else
                    sk = lens_structure_of_tile(w, state, id, tile_to_corp, &tile_struct,
                                                plates, gw);
                // A deposit lights by MEMBERSHIP: a split tile resolves to one wedge,
                // but it belongs to every toggled deposit it carries.
                const bool member =
                    (state.hovered_structure_kind == structure_kind::deposit)
                        ? [&] {
                              const std::size_t ri =
                                  static_cast<std::size_t>(state.hovered_structure - 1u);
                              return ri < std::size(tile.resource_deposit)
                                  && tile.resource_deposit[ri] > 0.0f;
                          }()
                        : (sk == state.hovered_structure_kind
                           && tile_struct == state.hovered_structure);
                if (member)
                {
                    constexpr ImU32 lit = IM_COL32(255, 255, 255, 34);
                    if (on_bake)
                    {
                        // Over the bake it is a wash like the lens's own (BL-1250):
                        // full radius, in the wash channel, so a lit catchment does
                        // not draw a 1 px grid of unlit gaps across the ground.
                        const float step = draw_r + 1.0f;
                        if (coarse_fill)
                            washes.push_back({ {}, 0.0f,
                                               { cx - kSqrt3 * step * 0.5f, cy - 0.75f * step },
                                               { cx + kSqrt3 * step * 0.5f, cy + 0.75f * step },
                                               lit, false });
                        else
                            washes.push_back({ { cx, cy }, step, {}, {}, lit, true });
                    }
                    else if (coarse_fill)
                    {
                        const float step = draw_r + 1.0f;
                        const float hw   = kSqrt3 * step * 0.5f - 0.5f;
                        const float hh   = 1.5f   * step * 0.5f - 0.5f;
                        dl->AddRectFilled({ cx - hw, cy - hh }, { cx + hw, cy + hh }, lit);
                    }
                    else
                    {
                        dl->AddConvexPolyFilled(verts, 6, lit);
                    }
                }
            }

            pm.mark(canvas_pass_meter::lens_marks, dl);
            // ROADS AND SEA LANES AS SMOOTH CURVES (Ben, 2026-10-03: "render [roads] as
            // curves rather than lines, and make them thinner"; "sea lanes should always go
            // over ocean, never over ground"; RENDERING.md § Roads and sea lanes). Both are
            // tile fields on the cardinal grid the traversal walk uses, so both draw the same
            // way: a tile joined to exactly TWO network neighbours draws ONE quadratic from
            // the midpoint of the first shared edge, through its own centre as the control
            // point, to the midpoint of the second. Consecutive tiles' curves meet at those
            // midpoints along the centre-to-centre line, so the run is tangent-continuous —
            // the quadratic B-spline of the tile-centre chain — and each curve stays inside
            // the triangle (midpoint, centre, midpoint), so a lane never leaves the sea tiles
            // it was stamped on. A junction pairs its neighbours into through-curves (most
            // opposite first); an end, or a three-way junction's odd branch, is a straight
            // spoke, so a fork still reads as a fork. The half-edge symmetry of
            // BL-172 is unchanged: the halves still meet at the shared midpoint, and each
            // half takes its own edge's fog (BL-185, max of the pair).
            //
            // At the coarse LOD (draw_r <= 7 px) the curve is invisible at a few pixels a
            // tile, so the quadratic is drawn as its two straight chords.
            // Is (col, row) on the network? Raw field, columns wrapping, rows not — the
            // rung test below reads it, and must read the same from either end.
            const auto on_network = [&](auto level_of, int col, int row) -> bool
            {
                if (row < 0 || row >= gh)
                    return false;
                col %= gw;
                if (col < 0)
                    col += gw;
                const tile_component* at = tile_rc(col, row);
                return at && level_of(*at) != 0;
            };
            // @p skip_rungs: a link between two tiles that BOTH run straight through
            // along the other axis is a RUNG between two parallel runs, not a route —
            // two lanes laid on adjacent rows touch tile to tile, and drawing every
            // touch hatched the sea between them. Symmetric (the same test from either
            // end), so the half-edges still meet. A crossing is kept: the cross run's
            // tile does not run straight through along the other axis. Lanes only; a
            // road lattice's rungs are real roads.
            const auto draw_network = [&](auto level_of, ImU32 col, float thick, bool skip_rungs)
            {
                static const int card_off[4][2] = {{+1, 0}, {-1, 0}, {0, +1}, {0, -1}};
                const auto through = [&](int c, int r, bool horizontal) {
                    return horizontal
                        ? (on_network(level_of, c + 1, r) && on_network(level_of, c - 1, r))
                        : (on_network(level_of, c, r + 1) && on_network(level_of, c, r - 1));
                };
                ImVec2 mids[4];
                float  vis[4];
                int    deg = 0;
                for (int n = 0; n < 4; ++n)
                {
                    const int nrow = tile.grid_y + card_off[n][1];
                    if (nrow < 0 || nrow >= gh)
                        continue;
                    const int raw_col = tile.grid_x + card_off[n][0];
                    int ncol = raw_col % gw;
                    if (ncol < 0)
                        ncol += gw;

                    const entity_id nb_id = tile_at_rc(ncol, nrow);
                    if (nb_id == null_entity)
                        continue;
                    const tile_component* nb_tile = tile_rc(ncol, nrow);
                    if (!nb_tile || level_of(*nb_tile) == 0)
                        continue;
                    if (!(survey_tile_visible(body.survey, gw, gh, ncol, nrow) || god_view_lift))
                        continue; // BL-408: god view draws into the masked region too
                    if (skip_rungs)
                    {
                        const bool other_axis_horizontal = (n >= 2); // a row step's rung runs E-W
                        if (through(tile.grid_x, tile.grid_y, other_axis_horizontal)
                            && through(ncol, nrow, other_axis_horizontal))
                            continue;
                    }

                    ImVec2 nb_sc = to_screen(hex_local_centre(ncol, nrow, hex_size));
                    nb_sc.x += static_cast<float>(k) * period_px;
                    if (raw_col >= gw)      nb_sc.x += period_px; // east across the cylinder seam
                    else if (raw_col < 0)   nb_sc.x -= period_px; // west across the seam

                    mids[deg] = {(cx + nb_sc.x) * 0.5f, (cy + nb_sc.y) * 0.5f};
                    vis[deg]  = std::max(vision, vision_rc(ncol, nrow));
                    ++deg;
                }

                // The junction cap takes the brightest edge meeting at this centre, so it
                // never reads as a dark blot on the end of a lit span.
                float cap_vision = vision;
                for (int i = 0; i < deg; ++i)
                    cap_vision = std::max(cap_vision, vis[i]);

                // One quadratic mids[a] -> (centre) -> mids[b], split at its apex so each
                // half carries its own edge's fog. Apex = (ma + 2c + mb) / 4; each half's
                // control is the midpoint of its end and the centre.
                const auto curve = [&](int a, int b) -> ImVec2
                {
                    const ImVec2 apex = {(mids[a].x + 2.0f * cx + mids[b].x) * 0.25f,
                                         (mids[a].y + 2.0f * cy + mids[b].y) * 0.25f};
                    // THE WIDE-RUNG LOD (drawn radius under k_route_lod_radius_px;
                    // RENDERING.md § Roads and sea lanes). The whole curve is ONE
                    // stroke when both halves fog alike (nearly always), so it
                    // needs no round joint at the apex; each half is tessellated
                    // at two segments, and at the coarse fill (<= 7 px) at one —
                    // its two chords. At these radii the curve spans a dozen
                    // pixels: six segments a half and a joint fan were most of
                    // the road layer's vertices for a shape no one could see.
                    if (draw_r < k_route_lod_radius_px)
                    {
                        const ImU32 ea = fog_dim(col, vis[a]);
                        const ImU32 eb = fog_dim(col, vis[b]);
                        const ImVec2 ctl_a = {(mids[a].x + cx) * 0.5f, (mids[a].y + cy) * 0.5f};
                        const ImVec2 ctl_b = {(mids[b].x + cx) * 0.5f, (mids[b].y + cy) * 0.5f};
                        const int segs = coarse_fill ? 1 : 2;
                        // Continue the open path along one half, to @p to.
                        const auto half = [&](ImVec2 ctl, ImVec2 to) {
                            if (segs == 1)
                                dl->PathLineTo(to);
                            else
                                dl->PathBezierQuadraticCurveTo(ctl, to, segs);
                        };
                        if (ea == eb)
                        {
                            dl->PathLineTo(mids[a]);
                            half(ctl_a, apex);
                            half(ctl_b, mids[b]); // the b half, walked apex-outward
                            dl->PathStroke(ea, ImDrawFlags_None, thick);
                        }
                        else
                        {
                            dl->PathLineTo(mids[a]);
                            half(ctl_a, apex);
                            dl->PathStroke(ea, ImDrawFlags_None, thick);
                            dl->PathLineTo(apex);
                            half(ctl_b, mids[b]);
                            dl->PathStroke(eb, ImDrawFlags_None, thick);
                        }
                        return apex;
                    }
                    for (const int i : {a, b})
                    {
                        const ImU32 ec = fog_dim(col, vis[i]);
                        const ImVec2 ctl = {(mids[i].x + cx) * 0.5f, (mids[i].y + cy) * 0.5f};
                        dl->AddBezierQuadratic(mids[i], ctl, apex, ec, thick, 6);
                    }
                    // A round joint at the apex hides the seam between the two halves.
                    dl->AddCircleFilled(apex, thick * 0.5f, fog_dim(col, std::max(vis[a], vis[b])));
                    return apex;
                };

                // THROUGH-ROUTES. The neighbours pair off, most-opposite first (the pair
                // whose directions from the centre have the most negative dot product),
                // and each pair is one curve — so a junction reads as two crossing runs,
                // not a star of spokes, and a dense city lattice curves as a run does.
                // Deterministic: ties keep the first pair in the fixed E, W, S, N order.
                bool   used[4] = {false, false, false, false};
                ImVec2 hub     = {cx, cy}; // where an odd branch joins: the last curve's apex
                for (int left = deg; left >= 2; left -= 2)
                {
                    int   pa = -1, pb = -1;
                    float best = 2.0f;
                    for (int i = 0; i < deg; ++i)
                    {
                        if (used[i]) continue;
                        for (int j = i + 1; j < deg; ++j)
                        {
                            if (used[j]) continue;
                            const float ax = mids[i].x - cx, ay = mids[i].y - cy;
                            const float bx = mids[j].x - cx, by = mids[j].y - cy;
                            const float la = std::sqrt(ax * ax + ay * ay), lb = std::sqrt(bx * bx + by * by);
                            const float d  = (la > 0.0f && lb > 0.0f) ? (ax * bx + ay * by) / (la * lb) : 1.0f;
                            if (d < best) { best = d; pa = i; pb = j; }
                        }
                    }
                    if (pa < 0) break;
                    used[pa] = used[pb] = true;
                    hub = curve(pa, pb);
                }

                // An end (one neighbour, or none: a lone / just-placed tile), or a
                // three-way junction's odd branch: a straight spoke from the hub — the
                // centre, or the through-curve's apex so the branch meets the curve —
                // to its shared-edge midpoint, with a cap that rounds the join and keeps
                // a lone tile visible. Four neighbours pair off whole and need neither.
                if (deg == 2 || deg == 4)
                    return;
                for (int i = 0; i < deg; ++i)
                    if (!used[i])
                        dl->AddLine(hub, mids[i], fog_dim(col, vis[i]), thick);
                dl->AddCircleFilled(hub, std::max(1.0f, thick * 0.75f),
                                    fog_dim(col, cap_vision));
            };

            // Road network (BL-146/BL-172 generated + BL-147/BL-172 player-placed). Always-on
            // under every lens (roads are terrain, not an overlay), drawn only toward roaded,
            // survey-revealed cardinal neighbours (the 4 directions the intra-body A* actually
            // traverses; ocean is never roaded, so "land" is implicit). Styled by THIS tile's
            // tier — Track(1) thin/dim, Road(2) medium, Highway(3) thick/bright — so a tier
            // change reads as a taper at the midpoint. The width is ONE named constant per
            // tier (k_road_width_*: the stroke as a fraction of the drawn hex radius, which is
            // floored at 10 px so the tiers stay apart on the whole-grid view).
            //
            // BL-185: roads dim with the intra-body reach fog, through the same fog_dim wash
            // the lens fill takes, so an unreached road recedes with the ground under it rather
            // than reading as brightly as one on your own corridor. A road edge spans two tiles,
            // so the pair's vision is combined with MAX — a road is lit if EITHER end is reached.
            // Max is the choice for two reasons: it is SYMMETRIC, so both tiles' halves fog to the
            // same value and the span stays one continuous weight (the BL-172 no-from/to-asymmetry
            // property the geometry already guarantees); and reach is a flood outward from where
            // the player operates, so an edge touching a reached tile is inside that reach — a
            // corridor's roads should not darken one hop early at its rim. Survey (BL-067) still
            // owns genuinely unrevealed tiles; this is only the commercial-reach fog.
            const float route_r = std::max(10.0f, draw_r) * state.dbg_route_width_scale;
            if (tile.road_level > 0)
            {
                ImU32 col; float thick;
                switch (tile.road_level)
                {
                    case 1:  col = IM_COL32(175, 158, 120, 205); thick = route_r * k_road_width_track;   break;
                    case 2:  col = IM_COL32(205, 188, 140, 225); thick = route_r * k_road_width_road;    break;
                    default: col = IM_COL32(225, 205, 150, 238); thick = route_r * k_road_width_highway; break;
                }
                draw_network([](const tile_component& t) { return t.road_level; }, col, thick, false);
            }

            pm.mark(canvas_pass_meter::roads, dl);
            // Sea lanes (BL-1098, LOGISTICS.md § 4b): the stamped `lane_level` field, drawn
            // along the lane's own water tiles — never between its ends — as the same smooth
            // curve, in the soft sea blue the wizard's lapse draws a lane in. Always-on like
            // roads: a lane is part of the network a convoy rides, not an overlay.
            if (tile.lane_level > 0)
                draw_network([](const tile_component& t) { return t.lane_level; },
                             IM_COL32(140, 200, 245, 190), route_r * k_lane_width, true);

            pm.mark(canvas_pass_meter::lanes, dl);
            // Rivers draw nothing here: they bake into the ground as carved courses along
            // river_edges, widening downstream (ground_bake.cpp bake_rivers; RENDERING.md
            // § Mountains, rivers and terrain variety). The width gradient says which way
            // the water flows, so the stroke and its chevrons are retired.

            // National borders - the coloured rule (BL-601). The band's wash
            // above says "this ground is near a frontier"; this pass says WHICH
            // frontier and whose, and it is what carries the hit corridor.
            //
            // ON THE PLAIN CANVAS ONLY (Ben, 2026-08-28). It was "always on,
            // under every lens" from BL-601 until now - the national read became
            // chrome on the same footing as roads and rivers. Ben, reviewing the
            // lens sweep: "All: We can still see nation borders."
            //
            // THIS IS THE SECOND OF TWO NATION-BORDER PASSES and the reason the
            // first suppression looked ineffective: the inward WASH (gated at
            // `draw_border_band` above) says "this ground is near a frontier",
            // while this pass draws the coloured rule that says WHICH frontier and
            // whose. Suppressing only the wash left the rule drawing, so the
            // borders were still plainly there. Both now answer to one flag.
            //
            // The hit corridor goes with it, deliberately: it is built inside this
            // same loop, and a border that is invisible but still clickable is a
            // worse outcome than either state. Under a lens the lens's own subject
            // is what hover and selection pivot to (BL-603).
            //
            // THE STROKE IS INSET, not laid along the shared edge, and that is
            // the whole answer to "borders should not diffuse together". A
            // shared edge can only carry one colour, so two neighbours would
            // fight for it and whichever drew last would win - or, worse, be
            // averaged into a third nation's hue. Inset toward the drawing
            // tile's own centre, each nation paints a rule just inside its own
            // side: the pair reads as two parallel coloured lines with the
            // frontier between them, and no pixel ever belongs to a colour that
            // is neither neighbour's.
            // Only a frontier tile (depth 0: it touches a foreign owner — the
            // depth pass ran this same neighbour test) draws a rule; every other
            // tile skips its six neighbour reads.
            if (draw_border_band && border_depth[shade_idx] == 0u)
            {
                const entity_id own_nation = nation_rc(t_col, t_row);
                if (own_nation != null_entity)
                {
                    // Muted like the wash (2026-09-01) — the pair must read as
                    // one treatment.
                    const ImU32 border_col = muted_nation_colour(palette::nation_colour(own_nation));

                    // Standard odd-r neighbour offsets (col, row deltas; canonical table, BL-363).
                    const int (*off)[2] = hex_neighbors::offsets(tile.grid_y);

                    for (int n = 0; n < 6; ++n)
                    {
                        const int nrow = tile.grid_y + off[n][1];
                        if (nrow < 0 || nrow >= gh)
                            continue; // Off the top/bottom edge: no neighbour tile.

                        // Columns wrap on the horizontal cylinder.
                        int ncol = (tile.grid_x + off[n][0]) % gw;
                        if (ncol < 0)
                            ncol += gw;

                        const entity_id nb_id = tile_at_rc(ncol, nrow);
                        if (nb_id == null_entity)
                            continue;
                        const entity_id nb_nation = nation_rc(ncol, nrow);
                        if (nb_nation == own_nation)
                            continue; // Same owner: interior edge, no border.

                        // An edge facing UNCLAIMED ground is drawn lighter and
                        // thinner than one facing another nation (Ben,
                        // 2026-08-24). The wash carries this per TILE, inherited
                        // inward from the frontier; the stroke can do better,
                        // because it already knows what is on the other side of
                        // each individual edge — so a headland that faces the sea
                        // on three sides and a neighbour on the fourth draws three
                        // light rules and one full one, rather than four of a
                        // single averaged weight.
                        const bool  political  = (nb_nation != null_entity);
                        const float edge_scale = political ? 1.0f : k_border_unclaimed_scale;

                        // The shared edge via the midpoint-perpendicular method:
                        // place the segment at the midpoint of the centre-to-centre
                        // line, perpendicular to it, with length equal to one hex side
                        // (== circumradius draw_r for a regular hexagon). This avoids
                        // mapping neighbour directions to per-vertex pairs, which the
                        // offset-row vertex ordering makes error-prone. The neighbour's
                        // screen centre is taken at the SAME wrap offset k as this tile.
                        const ImVec2 nb_lc = hex_local_centre(ncol, nrow, hex_size);
                        ImVec2 nb_sc = to_screen(nb_lc);
                        nb_sc.x += static_cast<float>(k) * period_px;

                        float dirx = nb_sc.x - cx;
                        float diry = nb_sc.y - cy;
                        const float len = std::sqrt(dirx * dirx + diry * diry);
                        if (len <= 0.0f)
                            continue;
                        dirx /= len;
                        diry /= len;

                        // Pulled back along the centre line by the inset, so the
                        // rule sits inside this tile rather than on the seam.
                        const float inset = draw_r * k_border_stroke_inset;
                        const float mx = (cx + nb_sc.x) * 0.5f - dirx * inset;
                        const float my = (cy + nb_sc.y) * 0.5f - diry * inset;
                        const float px = -diry; // perpendicular to the centre line
                        const float py =  dirx;
                        const float half = draw_r * 0.5f;

                        const ImVec2 e0 { mx - px * half, my - py * half };
                        const ImVec2 e1 { mx + px * half, my + py * half };
                        const ImU32 edge_col =
                            political ? border_col
                                      : IM_COL32((border_col >> IM_COL32_R_SHIFT) & 0xFFu,
                                                 (border_col >> IM_COL32_G_SHIFT) & 0xFFu,
                                                 (border_col >> IM_COL32_B_SHIFT) & 0xFFu,
                                                 static_cast<int>(255.0f * k_border_unclaimed_scale));
                        dl->AddLine(e0, e1, edge_col,
                                    std::max(1.0f, k_border_stroke_px * edge_scale));

                        // The hit corridor (BL-601, and the general structure-grain
                        // case BL-603 builds on). Registered per DRAWN segment, so
                        // it follows the wrap copies and the survey mask for free -
                        // a border the player cannot see is a border they cannot
                        // click. Coarse zoom registers nothing: at draw_r <= 7 px
                        // a tile is barely wider than the corridor, and the whole
                        // canvas would resolve to a nation.
                        //
                        // Zones live in GROUND space (pre-squash), and the click
                        // asks with the ground-space cursor, as hover does. A
                        // single-building tile's hex OUTRANKS this corridor with no
                        // lens (SELECTION.md § Multi-building tiles; BL-1241): the
                        // press resolves its building before the band is asked, so
                        // the band is reached from the unbuilt or stacked side of a
                        // border there — and from either side under a lens.
                        if (!coarse_fill)
                        {
                            structure_hit_zone sz;
                            sz.id         = own_nation;
                            sz.kind       = structure_kind::nation;
                            sz.a          = e0;
                            sz.b          = e1;
                            sz.half_width = std::min(k_border_hit_px,
                                                     draw_r * k_border_hit_frac);
                            state.structure_hit_zones.push_back(sz);
                        }
                    }
                }
            }

            pm.mark(canvas_pass_meter::border_rule, dl);
            // Persistent player footprint: outline the player's own tiles under
            // EVERY lens (and the plain default), so "these are mine" never
            // disappears when a lens is picked. Under the Corporation lens the fill
            // is already the player colour, so the bright selection accent keeps the
            // edge visible; elsewhere the player identity colour reads as ownership
            // against terrain and value fills. Rival tiles carry no ring.
            if (is_player_tile)
            {
                const ImU32 ring = (state.overlay == overlay_mode::corporation)
                    ? palette::selection : corp_identity(w.player_entity);
                dl->AddPolyline(verts, 6, ring, ImDrawFlags_Closed, 2.0f);
            }

            pm.mark(canvas_pass_meter::player_ring, dl);
            if (built)
            {
                // BL-1241 (structures baked): NO building mark draws here. What
                // stands on the tile is a structure baked into the ground art —
                // a cluster of up to three, one per stack (RENDERING.md
                // § Installations; ui/structure_stamps.cpp). The silhouette
                // glyph, the crane, the stacked-tile ring, the corp emblem tag and
                // the "+N" badge all retired from this canvas with it; the count
                // and each stack's state are the Selection element's, ownership
                // the hover card's and the owner lenses'.
                //
                // The HIT ZONE stays exactly as it was: the hex, not the stamp,
                // is what a press lands on (SELECTION.md § Multi-building tiles).
                // `mr` is the legacy marker radius, kept solely as its scale.
                const float mr = std::max(2.0f, draw_r * 0.22f);

                // Register hit zone (BL-059). Only the k==0 copy per tile so
                // wrap copies don't produce duplicate zones; the single zone
                // records the canonical screen position for this frame.
                if (k == 0)
                {
                    const auto bld_it = tile_to_bld.find(id);
                    if (bld_it != tile_to_bld.end())
                    {
                        // BL-367: a tile with more than one building does not
                        // assume the whole hex is one installation — the click
                        // lands on the TILE (grouped stack list) instead of
                        // jumping into whichever building sorts lowest-id.
                        const int count = tile_bld_count.count(id) ? tile_bld_count.at(id) : 1;
                        marker_hit_zone hz;
                        hz.id     = (count > 1) ? id : bld_it->second;
                        hz.kind   = marker_hit_zone::kind::building;
                        hz.centre = {cx, cy};
                        hz.radius = mr * 2.0f;
                        state.marker_hit_zones.push_back(hz);
                    }
                }
            }

            // Landforms draw nothing here: the dramatic set (mountain, canyon, crater,
            // rift) bakes its own relief form into the ground, a bridged run as one
            // form (ground_bake.cpp bake_landforms; RENDERING.md § Mountains, rivers and
            // terrain variety). The hover card still names the landform and its cost.

            // The Workforce (Population) lens's per-tile DOT used to be drawn here
            // (BL-135's value mark). It is gone: the lens tints the tile itself now
            // (see compute_tile_fill), so the mark it drew "instead of the building
            // glyph" has nothing left to stand in for. With it go the two
            // suppressions it needed — the stack ring and the landform glyph both
            // draw under this lens exactly as they do under every other one.

            // Throughput lens (BL-606): the MAGNITUDE half. LP is generated at
            // anchors — cities, built-and-active ports and inland hubs — and
            // nowhere else, so the quantity is drawn where it exists rather than
            // smeared over the tiles it might serve. Shading every tile by "the
            // throughput serving it" would need a per-tile nearest-anchor
            // attribution the engine does not have; deriving one would be the
            // second distance model BL-325 ruling 3 forbids outright.
            //
            // Radius and hue both carry the anchor's share of the body's largest
            // pool, so a thin anchor reads small AND dim.
            //
            // NOT vision-fogged, which is the value-mark / building-glyph
            // convention rather than the road-span one: the survey mask already
            // owns this mark (the `!revealed` continue above is upstream of it),
            // and an anchor is a city or a completed port — as public as the
            // building glyph beside it. It was fogged in the first cut and the
            // mark vanished into the wash, which is how the convention was
            // settled here rather than guessed.
            if (state.overlay == overlay_mode::throughput && !state.lp_anchors.empty())
            {
                const auto ait = std::lower_bound(
                    state.lp_anchors.begin(), state.lp_anchors.end(), id,
                    [](const ui_state::lp_anchor& a, entity_id t) { return a.tile < t; });
                if (ait != state.lp_anchors.end() && ait->tile == id && state.lp_anchor_max > 0.0f)
                {
                    // A uniform authored rate makes every share 1.0, and that is
                    // the honest reading: every anchor generates the same, full
                    // amount. The ramp is here for the moment the rate stops being
                    // uniform, not to manufacture variation that is not there.
                    const float t = std::clamp(ait->lp / state.lp_anchor_max, 0.0f, 1.0f);

                    // A RING, not a disc. Every anchor is a city, a port or a hub,
                    // so the anchor tile ALREADY carries a settlement or building
                    // marker — and those are drawn after this pass, on the same
                    // draw list. A filled disc is simply hidden by them (measured:
                    // the first cut drew correctly at every one of the 57 anchors
                    // and was invisible at all of them). A ring sits outside the
                    // marker and reads as a capacity halo around the generator.
                    const float ar = std::max(3.0f, draw_r * 0.66f);
                    const float th = std::max(1.5f, draw_r * (0.10f + 0.13f * t));
                    dl->AddCircle({cx, cy}, ar, IM_COL32(10, 18, 30, 220), 18, th + 2.0f);
                    dl->AddCircle({cx, cy}, ar, throughput_anchor_colour(t), 18, th);
                }
            }

            // Supply lens: draw a convoy glyph on every tile when the active body
            // has a player convoy passing through it. supply_active is false when
            // no player convoy touches this body.
            if (supply_active)
            {
                constexpr ImU32 supply_col = IM_COL32(80, 200, 255, 200);
                const float gr = std::max(2.0f, draw_r * 0.28f);
                icons::convoy(dl, {cx, cy}, gr, supply_col);
            }

            // BATTLE MARKER (BL-469). Drawn on the ANCHOR tile of a province a
            // fight is in — once per battle, not once per tile, or a large
            // province would read as a dozen battles.
            //
            // It is an alert rather than a label, which is what earns it a place
            // the deleted unit chevron (BL-294) did not have: a battle is
            // time-limited AND reversible by a decision, so a fight the player
            // never notices is a decision they never got to make.
            //
            // VISIBILITY (BL-068): a rival-vs-rival fight shows the marker — WHERE
            // fighting is happening is public, the way a rival's buildings are —
            // and the card withholds the internals. Tinted by whether it is yours,
            // so the two read apart at a glance.
            if (!w.battles.empty() && draw_r >= 6.0f)
            {
                const uint32_t pid = w.provinces.province_of(id);
                if (pid != 0)
                {
                    const auto mt = province_mark_tile.find(pid);
                    if (mt != province_mark_tile.end() && mt->second == id)
                    {
                        if (const active_battle* b = first_battle_in(w, pid))
                        {
                            const bool mine = (b->attacker == w.player_entity
                                            || b->defender == w.player_entity);
                            const ImU32 col = mine ? IM_COL32(255, 120, 90, 235)
                                                   : IM_COL32(190, 170, 160, 190);
                            icons::battle(dl, {cx, cy}, std::max(3.0f, draw_r * 0.42f), col);
                        }
                    }
                }
            }

            // UNIT MARKERS (BL-575). Grouped at the province grain like the
            // battle marker just above: drawn once per (province, owner)
            // GROUP, not once per unit or per tile — a unit's command grain is
            // the province (BL-511's march_unit retarget), so a large
            // province full of units reads as a handful of markers with count
            // badges rather than a scatter. Offset BELOW the hex centre so it
            // never sits directly under the battle glyph, which claims the
            // centre when a fight is live here — the two can both be true at
            // once, since a battle needs forces present.
            //
            // WHERE (BL-1145): on the group's lowest-id unit's OWN tile, not the
            // province's lowest-id tile — in a province of a thousand tiles that
            // tile can lie far from every unit in it, leaving an army on screen
            // with no marker. Groups whose sample units share a tile spread
            // side by side, as before.
            if (draw_r >= 6.0f && !tile_units.empty() && prov_id_of() != 0)
            {
                {
                    const auto pu_it = tile_units.find(id);
                    if (pu_it != tile_units.end() && !pu_it->second.empty())
                    {
                        const std::vector<unit_marker_summary>& groups = pu_it->second;
                        const float ur      = std::max(2.5f, draw_r * 0.30f);
                        const float spacing = ur * 2.4f;
                        const float uy      = cy + draw_r * 0.62f;
                        const float start_x = cx - spacing * (static_cast<float>(groups.size() - 1) * 0.5f);
                        for (std::size_t gi = 0; gi < groups.size(); ++gi)
                        {
                            const unit_marker_summary& grp = groups[gi];
                            const ImVec2 mpos{ start_x + spacing * static_cast<float>(gi), uy };
                            const ImU32  fill = corp_identity(grp.owner);
                            icons::unit_marker(dl, mpos, ur, fill, grp.committed);

                            // "+N" stack badge — the same k/N text-overlay idiom the
                            // building stack badge and the survey badge both use, for
                            // a group of more than one unit entity.
                            if (grp.count > 1)
                            {
                                char nbuf[8];
                                std::snprintf(nbuf, sizeof nbuf, "+%d", grp.count - 1);
                                const ImVec2 bpos{ mpos.x + ur * 0.85f, mpos.y + ur * 0.85f };
                                dl->AddCircleFilled(bpos, 7.0f, IM_COL32(18, 20, 26, 225), 12);
                                dl->AddText({bpos.x - 6.0f, bpos.y - 6.0f},
                                            IM_COL32(230, 230, 235, 235), nbuf); // fit-exempt: on-canvas marker badge, no containing box
                            }

                            // Hit zone (BL-031/BL-059/BL-575): the group's LOWEST-id
                            // unit is the click/hover target — same "lowest id wins"
                            // idiom as the building stack above.
                            marker_hit_zone hz;
                            hz.id     = grp.sample_unit;
                            hz.kind   = marker_hit_zone::kind::unit;
                            hz.centre = mpos;
                            hz.radius = ur * 2.0f;
                            state.marker_hit_zones.push_back(hz);
                        }
                    }
                }
            }

            pm.mark(canvas_pass_meter::markers, dl);
            // Selection outline is drawn on every visible copy of the selected
            // tile; hover is deferred to a single nearest copy, resolved below.
            draw_hex_highlight(dl, verts,
                resolve_highlight(selected, /*hovered=*/false, /*pinned=*/false));

            // Province outline (BL-511). The OUTER boundary of the cell, drawn
            // edge by edge on every side facing a different province, never the
            // interior seams. This is the crisp affordance the faint always-on
            // edge deliberately is not. Hover uses the same shape at the hover
            // colour and yields to selection, per the highlight convention.
            //
            // BL-598: a click no longer selects the province — it selects the
            // TILE, whose element carries the province as a set of sections. So
            // the outline now reads as "the ground your Deposits / Buildings /
            // Population sections are about", drawn around the selected tile's
            // own cell. `selected_province` is the derived mirror that says which.
            // (A province lights only when one is selected or hovered, so the
            // test is membership of THOSE two — a binary search of their sorted
            // tile lists, not a province lookup on every tile.)
            const bool in_sel_prov = sel_prov
                && std::binary_search(sel_prov->tiles.begin(), sel_prov->tiles.end(), id);
            const bool in_hov_prov = !in_sel_prov && hov_prov
                && std::binary_search(hov_prov->tiles.begin(), hov_prov->tiles.end(), id);
            if (revealed && (in_sel_prov || in_hov_prov))
            {
                const uint32_t prov_id = in_sel_prov ? state.selected_province
                                                     : state.hovered_province;
                const bool prov_selected = (prov_id == state.selected_province);
                // Hover only with NO lens, for the reason recorded at the tile
                // ring below (Ben's "stop dual hover", 2026-08-28): under a lens
                // the resolved thing is the lens's structure, and a province edge
                // lighting inside it is a second answer to a question that has one.
                // SELECTION is untouched — `selected_province` is non-zero only on
                // the tile rung, which a lens cannot reach anyway, so it needs no
                // guard of its own.
                const bool prov_hovered  = (prov_id == state.hovered_province)
                                           && state.overlay == overlay_mode::none;
                const highlight ph = resolve_highlight(prov_selected,
                                                       prov_hovered, /*pinned=*/false);
                if (ph != highlight::none)
                {
                    // March-picking mode (BL-575): the hovered province reads as a
                    // destination candidate, not a selection candidate, while a
                    // march is armed — a distinct green rather than the ordinary
                    // hover tint, so the live click-through pass has a visible
                    // "you are picking a destination" cue.
                    const bool march_picking = state.pending_march_unit != null_entity
                                             && prov_hovered && ph != highlight::selected;
                    const ImU32 pc = march_picking ? IM_COL32(90, 230, 120, 235)
                                   : (ph == highlight::selected) ? palette::selection
                                                                 : palette::hover;
                    for (int s = 0; s < 6; ++s)
                    {
                        const auto nc   = hex_neighbors::neighbour(t_col, t_row, s);
                        const int  ncol = ((nc.gx % gw) + gw) % gw;
                        const entity_id nid = tile_at_rc(ncol, nc.gy);
                        if (nid != null_entity && w.provinces.province_of(nid) == prov_id)
                            continue;
                        dl->AddLine(verts[k_side_verts[s][0]], verts[k_side_verts[s][1]],
                                    pc, 2.0f);
                    }
                }
            }

            // Hit-test: distance to hex centre < circumradius (approximate,
            // sufficient for usability). Scoped per wrap copy so the highlight
            // lands on the copy actually under the cursor; nearest centre wins.
            if (input_enabled)
            {
                const float dx = mouse_g.x - cx;
                const float dy = mouse_g.y - cy; // ground-space cursor (BL-737)
                const float d2 = dx * dx + dy * dy;
                const bool in_area = mouse.x >= grid_area_origin.x &&
                                     mouse.x <= grid_area_origin.x + grid_area_size.x &&
                                     mouse.y >= grid_area_origin.y &&
                                     mouse.y <= grid_area_origin.y + grid_area_size.y;
                if (in_area && d2 <= hit_r * hit_r && d2 < best_d2)
                {
                    best_d2          = d2;
                    hovered_tile     = id;
                    hover_offset     = { dx, dy };
                    hovered_selected = selected;
                    have_hover       = true;
                    std::copy(std::begin(verts), std::end(verts), std::begin(hover_verts));
                }
            }
            pm.mark(canvas_pass_meter::outline, dl);
        }
    }

    // Hover outline for the single resolved copy. Skipped when the hovered tile is
    // also the selection — selection outranks hover, and its ring is already drawn.
    //
    // AND SKIPPED UNDER A LENS (Ben, 2026-08-28: "we want to stop dual hover —
    // right now if I hover a market, the province/tile also gets highlighted").
    // Three hover marks could fire on one pointer position: this tile ring, the
    // province edge below, and the lens structure's own wash. Under a lens the
    // structure is the ONLY thing the pointer resolves to (BL-664), so it is the
    // only thing that may light — a tile ring inside a lit catchment says the
    // pointer is on two things at once, and one of them is not selectable.
    //
    // With no lens the tile IS the resolved thing, so the ring is the right mark
    // and is unchanged.
    if (have_hover && !hovered_selected && state.overlay == overlay_mode::none)
        draw_hex_highlight(dl, hover_verts, highlight::hovered);

    // BL-511: the hovered PROVINCE, for the outline the tile loop draws. Written
    // after the loop resolved the hovered tile, so the outline it feeds is one
    // frame behind — the same lag `state.hovered_entity` already carries, and
    // invisible at any frame rate the canvas runs at.
    state.hovered_province = (hovered_tile != null_entity)
                             ? w.provinces.province_of(hovered_tile) : 0u;

    // BL-603: and the hovered STRUCTURE, on the same one-frame lag and for the
    // same reason. Cleared when the pointer leaves the canvas or the lens has no
    // structure, so a stale region cannot stay lit after a lens switch.
    {
        entity_id sid = null_entity;
        const structure_kind sk =
            lens_structure_of_tile(w, state, hovered_tile, tile_to_corp, &sid, plates, gw,
                                   &hover_offset);
        state.hovered_structure      = sid;
        state.hovered_structure_kind = sk;
    }

    // Market-centre markers (BL-059). Draw a circle+cross glyph at each market's
    // centre tile position and register a hit zone for click-selection (BL-031).
    // Drawn after the tile loop so markers sit above all tile chrome. Only markets
    // anchored to the active body (and with a valid centre_tile) are shown.
    {
        constexpr ImU32 mkt_col = IM_COL32(255, 220, 80, 220);
        const float mkt_r = std::max(3.0f, draw_r * 0.28f);

        for (const auto& [mid, mk] : w.markets)
        {
            if (mk.body != state.active_body)
                continue;
            if (mk.centre_tile == null_entity)
                continue;
            const auto ctc_it = w.tiles.find(mk.centre_tile);
            if (ctc_it == w.tiles.end())
                continue;
            const tile_component& ctc = ctc_it->second;
            const ImVec2 lc = hex_local_centre(ctc.grid_x, ctc.grid_y, hex_size);
            const ImVec2 sc = to_screen(lc);

            // Draw on every visible wrap copy.
            const int k_min = (period_px > 0.0f)
                ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
            const int k_max = (period_px > 0.0f)
                ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;
            for (int k = k_min; k <= k_max; ++k)
            {
                const ImVec2 mc = {sc.x + static_cast<float>(k) * period_px, sc.y};
                icons::market_centre(dl, mc, mkt_r, mkt_col);
            }

            // Hit zone on the canonical copy (k==0).
            marker_hit_zone hz;
            hz.id     = mid;
            hz.kind   = marker_hit_zone::kind::market_centre;
            hz.centre = sc;
            hz.radius = mkt_r * 2.0f;
            state.marker_hit_zones.push_back(hz);
        }
    }

    // Population centres (BL-083; BL-1241, structures baked). Every generated
    // settlement is a STRUCTURE baked into the ground art — footprint and height
    // stepping with its scale, a razed centre as a ruin (RENDERING.md
    // § Installations; ui/structure_stamps.cpp) — so the skyline glyph, the ruin
    // glyph and the far-zoom density dot all retired from this canvas. Far-zoom
    // legibility is the art's job now: a scale >= 3 centre bakes as a pale paved
    // patch with a dark core at the far page.
    //
    // What remains here is the NAME: only City+ centres (scale >= 4, not razed)
    // carry a label, placed where the old skyline's edge stood.
    if (!w.population_centres.empty())
    {
        const float visible_top    = grid_area_origin.y - hit_r;
        const float visible_bottom = grid_area_origin.y + grid_area_size.y + hit_r;

        // Sorted gather: the store is an unordered_map, and overlapping labels
        // must overdraw in one deterministic order for the capture harness.
        struct pop_centre { int col; int row; int scale; entity_id centre; };
        std::vector<pop_centre> pcs;
        for (const auto& [pid, pc] : w.population_centres)
        {
            if (pc.scale < 4 || pc.razed)
                continue;
            const auto tit = w.population_centre_tile.find(pid);
            if (tit == w.population_centre_tile.end())
                continue;
            const auto til = w.tiles.find(tit->second);
            if (til == w.tiles.end() || til->second.body != state.active_body)
                continue;
            pcs.push_back({ til->second.grid_x, til->second.grid_y,
                            std::clamp(pc.scale, 1, 5), pid });
        }
        std::sort(pcs.begin(), pcs.end(),
                  [](const pop_centre& a, const pop_centre& b) { return a.centre < b.centre; });

        for (const pop_centre& a : pcs)
        {
            const ImVec2 lc = hex_local_centre(a.col, a.row, hex_size);
            const ImVec2 sc = to_screen(lc);
            if (sc.y < visible_top || sc.y > visible_bottom)
                continue;

            // The label's offset from the centre: the old skyline radius, so
            // names sit where players learned to look for them.
            const float sr = std::max(3.0f, draw_r * (0.30f + 0.11f * static_cast<float>(a.scale)));

            // The PERSISTED name — generated by the seeded tongue system
            // (generate_city_name, BL-290) and re-named to the settling culture's
            // speech in name_population_centres, so the label is deterministic
            // per campaign and speaks the world's own language.
            const auto name_it = w.population_centre_name.find(a.centre);
            if (name_it == w.population_centre_name.end() || name_it->second.empty())
                continue;
            const char* name = name_it->second.c_str();

            const int k_min = (period_px > 0.0f)
                ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
            const int k_max = (period_px > 0.0f)
                ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;
            for (int k = k_min; k <= k_max; ++k)
            {
                const ImVec2 mc = { sc.x + static_cast<float>(k) * period_px, sc.y };
                const ImVec2 tp{ mc.x + sr + 3.0f, mc.y - sr };
                dl->AddText({ tp.x + 1.0f, tp.y + 1.0f }, IM_COL32(20, 22, 28, 200), name); // shadow // fit-exempt: legend box sized to its measured entries (container 2)
                dl->AddText(tp, IM_COL32(236, 230, 214, 255), name); // fit-exempt: legend box sized to its measured entries (container 2)
            }
        }
    }

    // Corporate HQ marker (BL-182 foundation; the border RING retired BL-329,
    // 2026-08-08 — Ben's live critique: "retire the circle around corp
    // buildings. It doesn't show anything informative." The reach fog/lens
    // already shows supply reach properly; this fixed-radius, non-growing ring
    // duplicated that less accurately and added noise). What remains is just
    // the seat marker itself, from the PERSISTED hq_building (designated
    // deterministically at generation) — drawn on the corp's HOME body only
    // (the single-home model; branch offices are deferred with the full
    // BL-182 mechanic).
    auto draw_corp_hq = [&](entity_id corp_id, const corporation_component& cc)
    {
        if (cc.hq_building == null_entity)
            return;
        const auto b = w.buildings.find(cc.hq_building);
        if (b == w.buildings.end())
            return;
        const auto t = w.tiles.find(b->second.tile);
        if (t == w.tiles.end() || t->second.body != state.active_body)
            return;

        const ImU32  accent = corp_identity(corp_id);
        const ImVec2 hq_lc  = hex_local_centre(t->second.grid_x, t->second.grid_y, hex_size);
        const ImVec2 hq_s   = to_screen(hq_lc);
        const float  hq_r   = std::max(4.0f, draw_r * 0.5f);

        const int k_min = (period_px > 0.0f)
            ? static_cast<int>(std::ceil((visible_left  - hq_s.x - hq_r) / period_px)) : 0;
        const int k_max = (period_px > 0.0f)
            ? static_cast<int>(std::floor((visible_right - hq_s.x + hq_r) / period_px)) : 0;
        for (int k = k_min; k <= k_max; ++k)
        {
            const float off = static_cast<float>(k) * period_px;
            icons::hq(dl, { hq_s.x + off, hq_s.y }, hq_r, accent);
        }
    };

    // The player's HQ marker is always-on identity chrome (BL-085 lineage), drawn on
    // the player's home body regardless of the active lens.
    if (state.active_body == w.home_body && w.player_entity != null_entity)
    {
        const auto pc = w.corporations.find(w.player_entity);
        if (pc != w.corporations.end())
            draw_corp_hq(w.player_entity, pc->second);
    }

    // Rival HQ markers (BL-183 lineage): under the Corporation lens, every rival
    // corp's seat reads from the same persisted hq_building via the shared lambda
    // above. The player's own marker is the always-on chrome above (excluded here,
    // no double-draw). Each rival's marker is drawn on its OWN home body
    // (draw_corp_hq gates on the seat's body), so a rival only shows one when its
    // home body is the one on screen.
    //
    // SPLIT WITH THE TINT (Ben, 2026-08-28): the Corporation lens shows rival
    // corporations' seats, the Company lens shows background firms' seats, and
    // neither shows the other's. Before the split this loop drew every non-player
    // corp — background firms included, and they outnumber the rivals — so the
    // marker layer answered "where is everyone" when the lens was asking "where
    // are my rivals". Same population rule as compute_tile_fill, so a seat and
    // its tiles never appear under different lenses.
    if (state.overlay == overlay_mode::corporation || state.overlay == overlay_mode::company)
    {
        const bool want_background = (state.overlay == overlay_mode::company);
        for (const auto& [corp_id, cc] : w.corporations)
        {
            if (corp_id == w.player_entity)
                continue;
            if (cc.is_background != want_background)
                continue;
            // BL-1240: a rival's or firm's seat follows the picked set.
            if (!state.lens_owner_picked(corp_id, want_background))
                continue;
            draw_corp_hq(corp_id, cc);
        }
    }

    // Trade-flow lens (BL-1222; LENSES.md § Trade-flow lens). Reads only the
    // player's own dispatcher record (`w.player_trade_flow`, written by
    // dispatch_convoys for the player corporation alone), so no rival shipment,
    // shelf export or refusal can appear (DISCOVERY.md § Competitor visibility).
    //
    // FLOWS: one arrow per (source market, destination market, good) the player
    // shipped within the trailing window, both ends on this body; width from the
    // window's mean units per tick. REFUSALS: a marker beside each market of this
    // body short of a good the player held in surplus and did not ship there,
    // coloured by the best class over its goods (the closest any good came).
    // The hover reads are resolved here and drawn after the input early-out.
    struct tf_arrow
    {
        entity_id     src, dest;
        std::uint16_t good;
        float         rate;   // mean units per tick over the window
        float         price;  // newest pass's destination price
    };
    struct tf_marker
    {
        entity_id market;
        std::vector<std::pair<std::uint16_t, trade_refusal>> goods; // best class per good
    };
    std::vector<tf_arrow>  tf_arrows;
    std::vector<tf_marker> tf_markers;
    float                  tf_max_rate = 0.0f;
    int                    tf_hover_arrow  = -1;
    int                    tf_hover_marker = -1;
    // THE HELD CORPORATION'S PASSES ONLY. Each pass is tagged with the corp it was
    // taken for; after a seat change the window still holds the corporation left
    // behind (now a rival), and drawing it would show that rival's flows as the
    // player's. So the lens reads nothing until the held corp's first pass.
    std::vector<const trade_flow_pass*> tf_passes;
    if (state.overlay == overlay_mode::trade_flow)
        for (const trade_flow_pass& p : w.player_trade_flow)
            if (p.corp == w.player_entity && p.corp != null_entity)
                tf_passes.push_back(&p);
    if (state.overlay == overlay_mode::trade_flow && !tf_passes.empty())
    {
        const auto on_body = [&](entity_id m) {
            const auto it = w.markets.find(m);
            return it != w.markets.end() && it->second.body == state.active_body
                   && it->second.centre_tile != null_entity
                   && w.tiles.find(it->second.centre_tile) != w.tiles.end();
        };
        const auto centre_of = [&](entity_id m) {
            const tile_component& t = w.tiles.at(w.markets.at(m).centre_tile);
            return to_screen(hex_local_centre(t.grid_x, t.grid_y, hex_size));
        };

        // Aggregate the window, in a std::map so the draw order is fixed.
        const float passes = static_cast<float>(tf_passes.size());
        std::map<std::tuple<entity_id, entity_id, std::uint16_t>, std::pair<float, float>> agg;
        for (const trade_flow_pass* p : tf_passes)
            for (const trade_flow_shipment& s : p->shipments)
            {
                if (!on_body(s.source) || !on_body(s.dest))
                    continue;
                auto& e = agg[{s.source, s.dest, s.good}];
                e.first += s.units;
                e.second = s.price_d; // later passes overwrite: the newest price
            }
        for (const auto& [key, v] : agg)
        {
            const float rate = v.first / passes;
            tf_arrows.push_back({std::get<0>(key), std::get<1>(key), std::get<2>(key), rate,
                                 v.second});
            tf_max_rate = std::max(tf_max_rate, rate);
        }

        // Refusals from the NEWEST matching pass: "why not this pass", not a blend.
        {
            std::map<entity_id, tf_marker> by_market;
            for (const auto& [key, cls] : tf_passes.back()->best)
            {
                if (!on_body(key.first))
                    continue;
                tf_marker& m = by_market[key.first];
                m.market = key.first;
                m.goods.push_back({key.second, cls});
            }
            for (auto& [mid, m] : by_market)
            {
                // A market only carries a marker if some good there was NOT sent.
                const bool refused = std::any_of(m.goods.begin(), m.goods.end(), [](const auto& g) {
                    return g.second != trade_refusal::sent;
                });
                if (!refused)
                    continue;
                std::stable_sort(m.goods.begin(), m.goods.end(), [](const auto& a, const auto& b) {
                    return static_cast<int>(a.second) > static_cast<int>(b.second);
                });
                tf_markers.push_back(std::move(m));
            }
        }

        const float mkt_r   = std::max(3.0f, draw_r * 0.28f);
        const float inset   = mkt_r * 1.8f;
        const auto  seg_d2  = [](ImVec2 p, ImVec2 a, ImVec2 b) {
            const float vx = b.x - a.x, vy = b.y - a.y;
            const float l2 = vx * vx + vy * vy;
            float t = (l2 > 0.0f) ? ((p.x - a.x) * vx + (p.y - a.y) * vy) / l2 : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            const float dx = p.x - (a.x + t * vx), dy = p.y - (a.y + t * vy);
            return dx * dx + dy * dy;
        };
        float best_arrow_d2 = std::numeric_limits<float>::max();

        // Arrows. The destination is taken at the SHORTEST wrap displacement, and
        // the shaft sits a few px right of the centre line so A->B and B->A read
        // as two arrows rather than one double-headed line.
        for (std::size_t i = 0; i < tf_arrows.size(); ++i)
        {
            const tf_arrow& f = tf_arrows[i];
            if (f.src == f.dest)
                continue;
            const ImVec2 a0 = centre_of(f.src);
            ImVec2       b0 = centre_of(f.dest);
            if (period_px > 0.0f)
            {
                float dx = b0.x - a0.x;
                dx -= period_px * std::round(dx / period_px);
                b0.x = a0.x + dx;
            }
            const float vx = b0.x - a0.x, vy = b0.y - a0.y;
            const float len = std::sqrt(vx * vx + vy * vy);
            if (len <= 2.0f * inset)
                continue;
            const float ux = vx / len, uy = vy / len;
            const float nx = -uy, ny = ux;
            const float wpx  = trade_flow_width(f.rate, tf_max_rate);
            const float side = 2.0f + wpx * 0.5f;
            const float head = 5.0f + wpx;

            const float lo = std::min(a0.x, b0.x), hi = std::max(a0.x, b0.x);
            const int k_min = (period_px > 0.0f)
                ? static_cast<int>(std::ceil((visible_left  - hi) / period_px)) : 0;
            const int k_max = (period_px > 0.0f)
                ? static_cast<int>(std::floor((visible_right - lo) / period_px)) : 0;
            for (int k = k_min; k <= k_max; ++k)
            {
                const float  off = static_cast<float>(k) * period_px;
                const ImVec2 a{ a0.x + off + ux * inset + nx * side, a0.y + uy * inset + ny * side };
                const ImVec2 b{ b0.x + off - ux * inset + nx * side, b0.y - uy * inset + ny * side };
                const ImVec2 shaft_end{ b.x - ux * head, b.y - uy * head };
                dl->AddLine(a, shaft_end, IM_COL32(10, 12, 18, 170), wpx + 2.0f);
                dl->AddLine(a, shaft_end, palette::trade_flow_arrow, wpx);
                const float hw = head * 0.6f;
                dl->AddTriangleFilled(b,
                                      { shaft_end.x + nx * hw, shaft_end.y + ny * hw },
                                      { shaft_end.x - nx * hw, shaft_end.y - ny * hw },
                                      palette::trade_flow_arrow);
                const float d2 = seg_d2(mouse_g, a, b);
                const float hit = std::max(5.0f, wpx * 0.5f + 3.0f);
                if (d2 <= hit * hit && d2 < best_arrow_d2)
                {
                    best_arrow_d2  = d2;
                    tf_hover_arrow = static_cast<int>(i);
                }
            }
        }

        // Refusal markers, up-right of the market glyph so the glyph stays legible.
        const float mr = std::max(4.0f, mkt_r * 0.9f);
        for (std::size_t i = 0; i < tf_markers.size(); ++i)
        {
            const tf_marker& m = tf_markers[i];
            int best = -1;
            for (const auto& g : m.goods)
                if (g.second != trade_refusal::sent)
                    best = std::max(best, static_cast<int>(g.second));
            if (best < 0 || best >= k_trade_refusal_count)
                continue;
            const ImVec2 c0 = centre_of(m.market);
            const ImVec2 at0{ c0.x + mkt_r * 1.7f, c0.y - mkt_r * 1.7f };
            const int k_min = (period_px > 0.0f)
                ? static_cast<int>(std::ceil((visible_left  - at0.x) / period_px)) : 0;
            const int k_max = (period_px > 0.0f)
                ? static_cast<int>(std::floor((visible_right - at0.x) / period_px)) : 0;
            for (int k = k_min; k <= k_max; ++k)
            {
                const ImVec2 at{ at0.x + static_cast<float>(k) * period_px, at0.y };
                dl->AddCircleFilled(at, mr + 1.5f, IM_COL32(10, 12, 18, 235), 14);
                dl->AddCircleFilled(at, mr, palette::trade_refusal_colour[best], 14);
                const float dx = mouse_g.x - at.x, dy = mouse_g.y - at.y;
                if (dx * dx + dy * dy <= (mr + 3.0f) * (mr + 3.0f))
                    tf_hover_marker = static_cast<int>(i);
            }
        }
        // A marker is the smaller target, so it wins a tie with an arrow under it.
        if (tf_hover_marker >= 0)
            tf_hover_arrow = -1;
    }
    state.trade_flow_arrows  = static_cast<int>(tf_arrows.size());
    state.trade_flow_markers = static_cast<int>(tf_markers.size());
    state.trade_flow_passes  = static_cast<int>(tf_passes.size());

    // Building-placement ghost preview. When construction mode is active and a tile
    // is hovered, draw a translucent-intent marker of the chosen building type at the
    // hovered copy's centre, tinted green when the placement-rules seam accepts the
    // tile and red when it rejects it. The marker reads placement_rules::can_place
    // and mutates nothing — the click below is what enqueues the build. The
    // hovered copy's centre is the centroid of its six vertices (the wrap copy the
    // cursor actually resolved to); the marker radius mirrors the built-marker pass.
    if (have_hover && state.construction.active)
    {
        float gx = 0.0f, gy = 0.0f;
        for (const ImVec2& v : hover_verts)
        {
            gx += v.x;
            gy += v.y;
        }
        gx /= 6.0f;
        gy /= 6.0f;

        const float mr = std::max(3.0f, draw_r * kBuiltSilhouetteScale);

        // Full world-level check (coastal / launchpad, not just terrain) so the
        // ghost is red — and the reason legible — *before* the click, not after
        // construct_building refuses it (BL-071).
        const placement_rules::placement_result pr = placement_rules::can_place_in_world(
            w, hovered_tile, state.construction.type, state.construction.target,
            state.max_logistics_reach, null_entity,
            reg.placement_gate_for(state.construction.type, state.construction.pending_recipe));
        const ImU32 ghost_col = pr ? palette::positive : palette::negative;

        // BL-429: only the extraction target is known here (armed placement mode
        // carries no recipe field, verify_api.cpp being its one setter today) — a
        // processing ghost falls through to the generic square, same as before
        // this item.
        icons::building(dl, {gx, gy}, mr, state.construction.type, state.construction.target, ghost_col);

        // 'Why not here': the rejection reason follows the cursor while build mode
        // is armed, sat just below the ghost with a dark backdrop for contrast.
        if (!pr)
        {
            const char* why = pr.message();
            const ImVec2 tsz = ImGui::CalcTextSize(why);
            const ImVec2 tp{gx - tsz.x * 0.5f, gy + mr + 3.0f};
            dl->AddRectFilled({tp.x - 3.0f, tp.y - 1.0f},
                              {tp.x + tsz.x + 3.0f, tp.y + tsz.y + 1.0f},
                              IM_COL32(0, 0, 0, 180), 2.0f);
            dl->AddText(tp, palette::negative, why); // fit-exempt: legend box sized to its measured entries (container 2)
        }
    }

    dl->PopClipRect();

    // The active lens's key, in the ONE lens chrome region (BL-602): the minimap's
    // header, top right. Drawn unclipped and BEFORE the input early-out, so it shows in
    // headless captures too.
    //
    // Neither family takes a position argument any more, and that is the change. Every
    // key asks `ui::lens_chrome_rect` where it goes, so there is nothing here for a
    // future edit to leave un-updated — the same rule shell_metrics.hpp exists to
    // enforce on the rest of the shell. The Continent key drops its foreground-list
    // special case with the move (see begin_lens_key).
    //
    // The Country row is absent rather than pending: BL-601 retired that lens in the
    // same sprint, and its key with it.
    //
    // The key is screen chrome on the canvas's own draw list: its vertices
    // are cut out of the camera squash (BL-1246 — lenses ride the one angle
    // now, their legends do not).
    v_key_start = dl->VtxBuffer.Size;
    if (state.overlay == overlay_mode::resource)
        draw_resource_key(w, state);
    else if (state.overlay == overlay_mode::corporation || state.overlay == overlay_mode::company)
        draw_owner_key(w, state, tile_to_corp);
    else if (state.overlay == overlay_mode::market)
        draw_market_key(w, state, market_catchment_colour);
    else if (state.overlay == overlay_mode::population)
        draw_population_key(dl, state);
    else if (state.overlay == overlay_mode::scarcity)
        draw_scarcity_key(dl, state);
    else if (state.overlay == overlay_mode::industry)
        draw_industry_key(dl, state);
    else if (state.overlay == overlay_mode::continent)
        draw_continent_key(dl, state, plates);
    // BL-606's key joins the same one chrome home. It was authored against the
    // old flush-left anchor and needed the FOREGROUND draw list plus an opaque
    // fill to float over the always-open Selection band; Sprint 17b's minimap-
    // header region removes the collision the workaround existed for, so the
    // key draws on the ordinary list like every other legend.
    else if (state.overlay == overlay_mode::throughput)
        draw_throughput_key(dl, state);
    else if (state.overlay == overlay_mode::reach)
        draw_reach_key(w, reach_links, state);
    else if (state.overlay == overlay_mode::supply_routes)
        draw_supply_routes_key(w, supply_edges, state);
    else if (state.overlay == overlay_mode::trade_flow)
        draw_trade_flow_key(dl, state, tf_max_rate, static_cast<int>(tf_arrows.size()),
                            static_cast<int>(tf_markers.size()));
    v_key_end = dl->VtxBuffer.Size;

    if (!input_enabled)
    {
        apply_camera();
        return;
    }

    // Trade-flow lens hover (BL-1222). An immediate read at the cursor, like the
    // border band's below: an arrow or a marker is not an entity, so the
    // entity-keyed glance card has nothing to hold; under this lens the ground
    // carries no structure, so that card never competes for the slot.
    if (tf_hover_arrow >= 0 || tf_hover_marker >= 0)
    {
        std::string text;
        char line[160];
        if (tf_hover_arrow >= 0)
        {
            const tf_arrow& f = tf_arrows[static_cast<std::size_t>(tf_hover_arrow)];
            const char* good = presentation_of(static_cast<resource_type>(f.good)).name;
            std::snprintf(line, sizeof line, "%s  %.3g units/tick\nlanded price %.2f\n%s -> %s",
                          good, f.rate, f.price, market_city_name(w, f.src).c_str(),
                          market_city_name(w, f.dest).c_str());
            text = line;
        }
        else
        {
            const tf_marker& m = tf_markers[static_cast<std::size_t>(tf_hover_marker)];
            text = market_city_name(w, m.market) + " - short; your surplus:";
            for (const auto& [good, cls] : m.goods)
            {
                const char* gname = presentation_of(static_cast<resource_type>(good)).name;
                const char* cname = (cls == trade_refusal::sent)
                                        ? "sent"
                                        : k_trade_refusal_name[static_cast<int>(cls)];
                std::snprintf(line, sizeof line, "\n%s: %s", gname, cname);
                text += line;
            }
        }
        ImDrawList* fdl = ImGui::GetForegroundDrawList();
        const ImVec2 ts  = ImGui::CalcTextSize(text.c_str());
        const float  pad = 5.0f;
        const ImVec2 tl { mouse_g.x + 14.0f, mouse_g.y + 14.0f };
        const ImVec2 br { tl.x + ts.x + pad * 2.0f, tl.y + ts.y + pad * 2.0f };
        const ImU32  edge = (tf_hover_marker >= 0) ? palette::neutral : palette::trade_flow_arrow;
        fdl->AddRectFilled(tl, br, IM_COL32(18, 18, 24, 235), 3.0f);
        fdl->AddRect(tl, br, edge, 3.0f, 0, 1.5f);
        fdl->AddText({ tl.x + pad, tl.y + pad }, IM_COL32(230, 230, 235, 255), text.c_str()); // fit-exempt: the card is sized to its measured text
    }

    // The border band's hover read (BL-601). Ben's ruling asked for a read that
    // NAMES the nation before the click commits - a corridor the player cannot
    // see is a click they cannot predict, and the band's own colour says "a
    // nation" without saying which.
    //
    // Deliberately NOT the glance-then-stick hover card below: that card waits
    // out an appear delay by design, and a target that only announces itself
    // after a dwell fails the "before the click commits" test. This is an
    // immediate label at the cursor, on the foreground list so the always-open
    // Selection band cannot bury it.
    //
    // Written against the general resolver, so a second structure kind names
    // itself here by extending the switch, not by adding a branch.
    {
        structure_kind hk = structure_kind::nation;
        const entity_id hovered_structure =
            resolve_structure_hit(state.structure_hit_zones, mouse_g.x, mouse_g.y, &hk);
        const char* label = nullptr;
        ImU32       label_col = palette::neutral;
        if (hovered_structure != null_entity && hk == structure_kind::nation)
        {
            if (const auto nit = w.nations.find(hovered_structure); nit != w.nations.end())
            {
                label     = nit->second.name.c_str();
                label_col = palette::nation_colour(hovered_structure);
            }
        }
        if (label != nullptr && label[0] != '\0')
        {
            ImDrawList* fdl = ImGui::GetForegroundDrawList();
            const ImVec2 ts  = ImGui::CalcTextSize(label);
            const float  pad = 5.0f;
            // Positioned at the GROUND-space cursor: the camera squash maps it
            // back onto the real one (BL-737).
            const ImVec2 tl { mouse_g.x + 14.0f, mouse_g.y + 14.0f };
            const ImVec2 br { tl.x + ts.x + pad * 2.0f, tl.y + ts.y + pad * 2.0f };
            fdl->AddRectFilled(tl, br, IM_COL32(18, 18, 24, 235), 3.0f);
            fdl->AddRect(tl, br, label_col, 3.0f, 0, 1.5f);
            fdl->AddText({ tl.x + pad, tl.y + pad }, IM_COL32(230, 230, 235, 255), label);
        }
    }

    // Hover-card (BL-060, BL-020). Resolve the hovered entity in marker-priority
    // order (building > market_centre > tile — mirroring click priority). Track
    // stable hover ticks and show the lens-contextual "why not what" card after
    // kHoverAppearDelaySec of rest on the same entity.
    {
        // Resolve the highest-priority entity under the cursor (shared with the
        // click path below — resolve_marker_hit, BL-362).
        const entity_id marker_hover =
            resolve_marker_hit(state.marker_hit_zones, mouse_g.x, mouse_g.y);

        // A built tile resolves to its BUILDING across the whole hex, not just inside
        // the glyph's radius: once a tile carries an installation, the installation is
        // what is there. The tile beneath it is no longer separately reachable.
        entity_id tile_bld = null_entity;
        if (hovered_tile != null_entity)
            if (const auto tb = tile_to_bld.find(hovered_tile); tb != tile_to_bld.end())
                tile_bld = tb->second;

        entity_id hover_eid = null_entity;
        if (state.overlay != overlay_mode::none)
        {
            // BL-664, the hover half of the same rule. Under a lens the pointer
            // resolves to the lens's structure or to nothing, so:
            //
            //  - a MARKER takes no part. The card's subject is the ground, not
            //    the glyph standing on it, because the lens is what the player is
            //    asking about ("markers do not outrank lenses", Ben 2026-08-28).
            //  - where the lens has NO STRUCTURE for this ground there is NO CARD
            //    AT ALL ("for lenses which return none, just don't surface a
            //    hover"). Three lenses — Population, Industry, Throughput — draw a
            //    value field with no structure grain, so the card is absent across
            //    the whole body while one of them is active. That is deliberate: a
            //    value field is something you read.
            //
            // `hovered_structure_kind` was resolved by the tile loop earlier in
            // THIS frame (it is written well above this read), so this costs a
            // read rather than a second resolve and hover cannot disagree with the
            // click about what the ground resolved to.
            //
            // WHAT THE CARD IS ABOUT IS STILL THE TILE, not the structure -- so a
            // press and a hover at the same pixel agree on the KIND and differ on
            // the SUBJECT. Deliberate and bounded: `draw_hover_content` has no
            // branch for a corporation, a company, a catchment or a plate, and
            // writing four is content work rather than the resolution rule this
            // item is about. Recorded as owed rather than done (NR-701).
            hover_eid = (state.hovered_structure_kind != structure_kind::none)
                        ? hovered_tile : null_entity;
        }
        else if (marker_hover != null_entity)
            hover_eid = marker_hover;
        else if (tile_bld != null_entity)
            hover_eid = tile_bld;
        else
            hover_eid = hovered_tile;

        if (hover_eid != state.hovered_entity)
        {
            state.hovered_entity = hover_eid;
            state.hover_seconds  = 0.0f;
        }
        else if (hover_eid != null_entity)
        {
            // Interactive deltas are clamped: one long stall (world gen, alt-tab)
            // would otherwise dump seconds into the accumulator in a single frame
            // and snap a card straight to stuck.
            state.hover_seconds += state.fixed_frame_clock
                                       ? kVerifyFrameSeconds
                                       : std::min(ImGui::GetIO().DeltaTime, kHoverMaxFrameSeconds);
        }

        // Glance-then-stick hover (BL-228/230, retires BL-200's dwell-to-open).
        //
        // Hovering no longer OPENS the Selection band. Opening is the click's job
        // alone — one gesture, one meaning.
        //
        // What hover does now, in two phases: past kHoverAppearDelaySec the card
        // appears as a GLANCE and tracks the live cursor like an ordinary
        // tooltip, so it does not yet own the pointer. Past kHoverStickDelaySec it
        // STICKS — freezes at its current position, stops following the cursor,
        // and stays up until the pointer leaves its bounds. That makes a long
        // line readable (it cannot slide away mid-read) while still letting a
        // player who is only passing through dismiss it by moving on normally.
        if (state.hover_card_entity != null_entity && state.hover_card_stuck)
        {
            // Stuck: dismiss only when the pointer leaves the rect. The pad spans
            // the gap between the anchor and the card drawn above it, so the
            // card does not dismiss itself the frame it freezes.
            const bool inside =
                mouse.x >= state.hover_card_min.x - kHoverCardExitPadPx &&
                mouse.x <= state.hover_card_max.x + kHoverCardExitPadPx &&
                mouse.y >= state.hover_card_min.y - kHoverCardExitPadPx &&
                mouse.y <= state.hover_card_max.y + kHoverCardExitPadPx;

            if (!inside)
            {
                state.hover_card_entity = null_entity;
                state.hover_card_stuck  = false;
            }
        }
        else if (state.hover_card_entity != null_entity)
        {
            // Glancing: the card is not yet stuck, so it lives only as long as
            // the cursor keeps hovering the same entity that summoned it.
            if (hover_eid != state.hover_card_entity)
                state.hover_card_entity = null_entity;
        }

        // Summon a new glance card once the appear delay is met over an entity —
        // but never while one is already up (a glance or stuck card owns the slot
        // until dismissed), and never mid-placement, where a floating card would
        // sit over the ghost.
        if (state.hover_card_entity == null_entity &&
            hover_eid != null_entity &&
            !state.construction.active &&
            state.hover_seconds >= kHoverAppearDelaySec - kHoverThresholdEpsilon)
        {
            state.hover_card_entity = hover_eid;
            state.hover_card_stuck  = false;
        }

        // Promote glance -> stuck once the stick delay elapses, freezing the
        // card at its current (live-cursor) position.
        if (state.hover_card_entity != null_entity &&
            !state.hover_card_stuck &&
            state.hover_seconds >= kHoverStickDelaySec - kHoverThresholdEpsilon)
        {
            state.hover_card_stuck  = true;
            state.hover_card_anchor = { mouse.x, mouse.y };
            // Seed the rect around the anchor so the first frame's hit-test (which
            // runs before the card has been drawn at this position) cannot
            // dismiss it early.
            state.hover_card_min = { mouse.x, mouse.y };
            state.hover_card_max = { mouse.x, mouse.y };
        }

        // While glancing (not yet stuck), the card tracks the live cursor.
        if (state.hover_card_entity != null_entity && !state.hover_card_stuck)
            state.hover_card_anchor = { mouse.x, mouse.y };

        if (state.hover_card_entity != null_entity)
        {
            const entity_id card_eid = state.hover_card_entity;
            draw_hover_card(state.hover_card_anchor, [&]() {
                draw_hover_content(w, state, card_eid, &reg, &report);
            }, &state.hover_card_min, &state.hover_card_max);
        }
    }

    // Click handling. The surface is the bottom rung, so there is nothing to
    // descend into: a single left-click simply selects the hovered tile (null
    // clears the selection on empty space) and fills the Selection info element.
    // No view change; the player ascends via the minimap. See SELECTION.md.
    // Construction mode suppresses selection: in placement mode a left-click is a
    // construction gesture, not a selection one, so it must not retarget the
    // Selection info element.
    // BL-031: marker hit zones take priority over tile selection (unit >
    // building > market_centre; closest-wins tie-break within a kind).
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        // March-picking mode (BL-575) outranks everything below, exactly as
        // construction placement mode does: the unit card's March press ARMED
        // `pending_march_unit`, so this click's whole job is to name a
        // destination province, not to select or build anything.
        // app::render reads both fields once `pending_march_dest_province` is
        // non-zero, dispatches `corp_verb::march_unit`, and clears both — the
        // same deferred-mutation path every other canvas gesture takes (this
        // canvas holds only `const world&`). A click that misses every
        // province (open ocean, off-body) is simply ignored: the mode stays
        // armed for the next try rather than silently cancelling on a stray
        // miss.
        //
        // OWNER MULTI-SELECT (BL-1240): under the Corporation or Company lens, a
        // SHIFT-click on ground an owner of the lens's kind holds toggles that owner
        // in or out of the picked set — the map itself is a picker — and touches no
        // selection. A plain click, or a shift-click on ground no admitted owner
        // holds, takes the ordinary resolution below, unchanged.
        entity_id    shift_pick_owner = null_entity;
        const bool   owner_lens = (state.overlay == overlay_mode::corporation
                                   || state.overlay == overlay_mode::company);
        if (owner_lens && ImGui::GetIO().KeyShift)
        {
            entity_id owner = null_entity;
            const structure_kind k =
                lens_structure_of_tile(w, state, hovered_tile, tile_to_corp, &owner);
            // The border band outranks the ground under every lens (BL-601): a
            // shift-click ON a border still selects the nation. The zones are
            // registered in ground space, so ask with the ground-space cursor,
            // as the hover label does.
            structure_kind band_kind = structure_kind::nation;
            const bool on_band =
                resolve_structure_hit(state.structure_hit_zones, mouse_g.x, mouse_g.y,
                                      &band_kind) != null_entity;
            if (!on_band
                && (k == structure_kind::corporation || k == structure_kind::company))
                shift_pick_owner = owner;
        }

        if (state.pending_march_unit != null_entity)
        {
            const uint32_t march_prov = (hovered_tile != null_entity)
                                        ? w.provinces.province_of(hovered_tile) : 0u;
            if (march_prov != 0)
                state.pending_march_dest_province = march_prov;
        }
        else if (shift_pick_owner != null_entity && !state.construction.active)
        {
            state.toggle_lens_owner(shift_pick_owner,
                                    state.overlay == overlay_mode::company);
        }
        else if (!state.construction.active)
        {
            // WHICH RULE APPLIES IS DECIDED FIRST (BL-664, Ben 2026-08-28),
            // because these are two different resolutions rather than one with an
            // exception. SELECTION.md § A lens collapses selection to ONE TIER is
            // the authority.
            //
            //   NO LENS — the marker/tile stack, most-specific first, with the
            //             four-rung repeat-click cycle over it.
            //   A LENS  — the lens's structure, or NOTHING. "Markers do not
            //             outrank lenses": inside a lens the marker is only
            //             ground that happens to be built on, and the lens is the
            //             question the player is asking. A lens with no answer
            //             for this ground selects nothing at all.
            //
            // THE BORDER BAND IS OUTSIDE THE SPLIT. A nation is reached by
            // clicking its border under EVERY lens (BL-601) because the band is
            // always-on chrome rather than any lens's own structure, so the
            // boundary resolver runs in both branches and outranks both. That is
            // also why the boundary is asked BEFORE the lens's area: a border is
            // a thing the player aimed at, a catchment is ground they happen to
            // be over (the BL-603 ordering, unchanged).
            const bool lensed = (state.overlay != overlay_mode::none);

            // Resolve marker hit zones in priority order (BL-031): building
            // outranks market-centre; both outrank tile. Shared with hover.
            // Suppressed entirely under a lens by the rule above.
            //
            // GROUND-SPACE CURSOR (BL-1241 fix round): the zones are registered
            // in flat ground space and the camera squash maps them onto the
            // screen, so the press asks with `mouse_g` exactly as hover does.
            // Asking with the screen `mouse` missed off-centre presses on every
            // tilted rung — the hover named one thing and the press took another.
            entity_id marker_hit =
                lensed ? null_entity
                       : resolve_marker_hit(state.marker_hit_zones, mouse_g.x, mouse_g.y);

            // A SINGLE BUILDING OWNS ITS WHOLE HEX (SELECTION.md § Multi-building
            // tiles; BL-1241). With the structures baked there is no glyph to aim
            // at — the hex is the press area — so on a tile carrying exactly one
            // building the building is resolved HERE, before the border band's
            // corridor gets a say: a press anywhere on that hex, edge included,
            // lands on the installation. A unit marker still outranks it (a unit
            // standing on a built tile must stay reachable on the first press).
            // A stacked tile is unchanged — it falls through to the tile view —
            // and so is every lensed press (the lens branch never reaches here).
            if (!lensed && hovered_tile != null_entity
                && (marker_hit == null_entity || !w.units.count(marker_hit)))
            {
                const auto cnt_it = tile_bld_count.find(hovered_tile);
                const auto tb     = tile_to_bld.find(hovered_tile);
                if (cnt_it != tile_bld_count.end() && cnt_it->second == 1
                    && tb != tile_to_bld.end())
                    marker_hit = tb->second;
            }

            // STRUCTURE-GRAIN selection (BL-601), between the markers and the
            // tile/province fallback. With no lens a marker is a specific thing
            // the player aimed at and still outranks a boundary; a boundary in
            // turn outranks the ground it runs across, because inside the
            // corridor the border IS what the pointer is on.
            //
            // This is the route the retired Country lens used to own - LENSES.md
            // sent a hovered tile under that lens to its owning nation. Ben's
            // ruling of 2026-08-24 moved it onto the band: "click the border
            // itself". The border is what carries the nation on screen now, so
            // it is the thing that opens it; a rail slot would have put a nation
            // behind a menu while its territory sat under the pointer.
            //
            // Written against the general resolver, not against nations: a plate
            // rim or a catchment edge routes through this same branch once it
            // produces zones, which is what BL-603 generalises.
            structure_kind struct_kind = structure_kind::nation;
            entity_id structure_hit =
                (marker_hit == null_entity)
                    ? resolve_structure_hit(state.structure_hit_zones, mouse_g.x, mouse_g.y,
                                            &struct_kind) // ground space, as hover asks
                    : null_entity;

            // Set when the click resolved to a NON-ENTITY structure (a deposit or
            // a plate). Those take their own selection channels, so both the
            // entity-structure branch and the tile fallback must stand down —
            // without this the fallback would immediately select the tile under
            // the pointer and overwrite the region the player just picked.
            bool non_entity_structure = false;

            // Set when a lens was active and had NOTHING to say about this
            // ground — BL-664's rule 3. Distinct from `structure_hit ==
            // null_entity`, which under no lens simply means "fall through to
            // the tile". Here there is no falling through: the click selects
            // nothing and the band returns to resting.
            bool lens_answered_nothing = false;

            // The hand-wired Corporation-lens pivot that used to sit here — a
            // clicked BUILDING resolving through to its owning corporation — is
            // GONE, folded into `lens_structure_of_tile`'s own corporation case
            // (BL-665). It existed only because a marker outranked a structure,
            // so the lens could not reach its owner from the ground; BL-664
            // removed that precedence and the area resolver answers on owned
            // ground whether or not a glyph sits on it. One resolver, not two.

            // BL-603: the AREA structure, if the boundary one did not answer. The
            // boundary resolver wins where both do, and that ordering is the whole
            // reason a border is still clickable under a lens: a nation's rule is a
            // thing the player AIMED at, a catchment is the ground they happen to be
            // over. Same branch, same mutual exclusion, one resolver later.
            if (lensed && structure_hit == null_entity)
            {
                entity_id area_id = null_entity;
                const structure_kind area_kind =
                    lens_structure_of_tile(w, state, hovered_tile, tile_to_corp, &area_id,
                                           plates, gw, &hover_offset);
                // A DEPOSIT AND A PLATE ARE NOT ENTITIES, so they never reach
                // `structure_hit` — that variable feeds `selected_entity` a few
                // lines down, and a synthetic key landing there would read as a
                // real entity to every surface that later resolves it. They are
                // dispatched here instead, into their own fields (BL-659/BL-660,
                // Ben's option A), and the entity path is skipped entirely.
                if (area_kind == structure_kind::deposit || area_kind == structure_kind::plate)
                {
                    state.clear_lens_region_selection();
                    state.clear_battle_selection();
                    state.selected_entity      = null_entity;
                    state.selected_province    = 0u;
                    state.province_sync_entity = null_entity;
                    state.selection_cycle_tile = null_entity;
                    non_entity_structure = true;

                    if (area_kind == structure_kind::deposit)
                    {
                        // "select a deposit going to the market ledger for this
                        // item" (Ben, 2026-08-28). The resource is the key, and
                        // the Market ledger already follows a selected resource,
                        // so this opens the panel and the existing route aims it.
                        state.selected_deposit_resource = static_cast<int>(area_id) - 1;
                        close_all_panels(state);
                        state.show_market_ledger = true;
                        // ...ON THE PRICES VIEW (BL-659). Opening the ledger on
                        // whichever tab was last used answers a different question
                        // from the one the press asked: a deposit's question is
                        // "what is this worth", and Prices is the view that says
                        // so. The view then highlights the pressed resource's own
                        // sparkline, which is what "aimed at that resource" means
                        // in a view that lists every traded good.
                        state.market_ledger_view = 0;
                    }
                    else
                    {
                        // "click to the relevant history section, describing
                        // collisions and the opposite" (Ben, 2026-08-28). The
                        // History ledger is the destination; aiming it AT the
                        // plate's own collision/rift record is BL-660's second
                        // half and is not built — see PLACEHOLDER below.
                        state.selected_plate = static_cast<int>(area_id) - 1;

                        // Cache what the Selection element will say about it, here
                        // and only here — the band cannot reach the generation
                        // report, and this pass already holds it (BL-671; the
                        // field's own comment carries the reasoning).
                        state.selected_plate_facts = {};
                        if (plates != nullptr && gw > 0)
                        {
                            const int pid = state.selected_plate;
                            const std::size_t n = plates->plate_id.size();
                            const bool have_conv = plates->convergent.size() == n;
                            const bool have_div  = plates->divergent.size() == n;
                            for (std::size_t k = 0; k < n; ++k)
                            {
                                if (plates->plate_id[k] != pid)
                                    continue;
                                ++state.selected_plate_facts.tiles;
                                if (have_conv && plates->convergent[k] != 0u)
                                    ++state.selected_plate_facts.convergent_tiles;
                                if (have_div && plates->divergent[k] != 0u)
                                    ++state.selected_plate_facts.divergent_tiles;
                            }
                            if (pid >= 0 && static_cast<std::size_t>(pid) < plates->plates.size())
                            {
                                const tectonic_plate& tp =
                                    plates->plates[static_cast<std::size_t>(pid)];
                                state.selected_plate_facts.oceanic   = tp.oceanic;
                                state.selected_plate_facts.drift_col = tp.drift_col;
                                state.selected_plate_facts.drift_row = tp.drift_row;
                            }
                        }

                        close_all_panels(state);
                        state.show_tile_ledger = true;
                        // ...ON THE TECTONICS VIEW (BL-660). Opening the History
                        // ledger on whichever view was last used answers a
                        // different question from the one the press asked.
                        state.history_view = history_view_tectonics;
                    }
                }
                else if (area_kind != structure_kind::none && area_id != null_entity)
                {
                    structure_hit = area_id;
                    struct_kind   = area_kind;
                }
                else
                {
                    lens_answered_nothing = true;
                }
            }

            if (non_entity_structure)
            {
                // Already dispatched into its own field above; nothing further.
            }
            else if (lens_answered_nothing)
            {
                // BL-664 rule 3: the active lens has no answer for this ground, so
                // the click does nothing to the canvas and the Selection band
                // CLEARS TO RESTING (Ben, 2026-08-28). It does not fall through to
                // the tile and it does not fall through to a marker.
                //
                // Clearing rather than leaving the previous selection standing is
                // the ruled half and the one worth stating: a band still showing
                // the last thing selected asserts something the player did not
                // just click and cannot connect to the pointer — the failure
                // NR-697 recorded, arriving here by a different route.
                state.selected_entity      = null_entity;
                state.selected_province    = 0u;
                state.province_sync_entity = null_entity;
                state.selection_cycle_tile = null_entity;
                state.clear_battle_selection();
                state.clear_lens_region_selection();
                // AND THE OWNER SURFACES GO WITH IT. A ledger whose whole subject
                // is one named firm cannot outlive the selection that named it:
                // leaving the Company ledger open on firm X after the player has
                // cleared the band is the same failure NR-697 recorded, just in
                // the column instead of the band. The tile build ledger is the
                // precedent -- app.cpp actively closes it once the selection stops
                // being a tile, rather than letting it sit there.
                state.selected_company             = null_entity;
                state.selected_corporation_dossier = null_entity;
                state.show_company_ledger          = false;
            }
            else if (structure_hit != null_entity)
            {
                // A structure is an ENTITY selection, so it takes the same
                // mutual exclusion every marker hit does: the province clears,
                // the battle clears, and the repeat-click cycle's tile anchor is
                // dropped so the next click on the ground starts a fresh cycle
                // rather than resuming one this selection interrupted.
                state.selected_entity      = structure_hit;
                state.selected_province    = 0u;
                state.province_sync_entity = structure_hit;
                state.selection_cycle_tile = null_entity;
                state.clear_battle_selection();
                state.clear_lens_region_selection();

                // "Clicking opens up our market ledger for THAT market" (Ben,
                // 2026-08-24). The ledger already follows the selection - BL-159
                // wired `draw_market_ledger` to jump its body/market combos to a
                // market selected anywhere - so this opens the panel and the
                // existing route aims it. Nothing new is invented to point it.
                if (struct_kind == structure_kind::market)
                {
                    close_all_panels(state);
                    state.show_market_ledger = true;
                }
                else if (struct_kind == structure_kind::corporation)
                {
                    // BL-666. This used to open the BALANCE ledger, which is the
                    // PLAYER'S OWN BOOKS — so clicking a rival's ground showed you
                    // your own accounts (NR-700). Wrong in the way that is hardest
                    // to catch in a capture: not empty, not broken, just not about
                    // the thing that was pressed.
                    //
                    // The corporations table is the surface that IS about a named
                    // firm, and it already highlights its row from the dossier
                    // field, so this aims it rather than inventing a route.
                    //
                    // THE FLAG IS `show_corporations_table`, NOT
                    // `show_corporation_panel`, and the two are named the wrong way
                    // round: `show_corporation_panel` drives
                    // `draw_corporation_dashboard` (nav slot 1, THE PLAYER'S OWN
                    // corporation at a glance), while `show_corporations_table`
                    // drives `draw_corporation_panel` (nav slot 8, the
                    // all-corporations table). The first draft of this line took the
                    // flag whose name matched the function it wanted and reproduced
                    // NR-700 one surface over — a rival's ground opening the
                    // player's own dashboard. Read app.cpp:1981-1989 before touching
                    // either flag.
                    state.selected_corporation_dossier = structure_hit;
                    state.selected_company             = null_entity;
                    close_all_panels(state);
                    state.show_corporations_table = true;
                }
                else if (struct_kind == structure_kind::company)
                {
                    // A DIFFERENT DESTINATION, not the same one with a flag (Ben,
                    // 2026-08-28: "Different types for either, make a placeholder
                    // if needed"). A corporation is a rival the player competes
                    // with; a company is a background firm. Routing both to the
                    // corporations table would re-merge the distinction the
                    // 2026-08-28 terminology split just drew.
                    //
                    // `selected_company` rather than reading `selected_entity`
                    // back: the ledger's subject must survive the player selecting
                    // something else, which a live-selection read would not.
                    state.selected_company             = structure_hit;
                    state.selected_corporation_dossier = null_entity;
                    close_all_panels(state);
                    // THE COMPANY LENS'S DESTINATION, since BL-675. The
                    // placeholder this replaces was BL-666's, and it was always
                    // meant to be replaced: a corporation press asks "how is this
                    // rival doing", a company press asks "can I buy this". So a
                    // background firm lands on the Acquisitions ledger.
                    //
                    // The firm is carried across as the ledger's FOCUS, never a
                    // filter. The buyable field is a mean of 81.6 firms on the
                    // shipped spawn (measured, acquisition_viability § C), and
                    // filtering it to one would answer a question nobody asked.
                    // A clicked firm may well not be in the field at all — most
                    // background firms are `closed` and cannot be priced — so the
                    // highlight can go unmatched, and the ledger says on its own
                    // face how many of the world's firms file. An unmatched
                    // highlight is the honest outcome; inventing a row for an
                    // unpriceable firm would be worse.
                    state.acquisitions_focus_corp  = structure_hit;
                    state.show_acquisitions_ledger = true;
                }
            }
            else
            {

            // Falling through to the tile means the click missed every marker glyph.
            // On a BUILT tile that still selects the building: the whole hex belongs to
            // the installation, so the tile element is unreachable there (Ben's
            // 2026-07-22 call). The tile element is the prospecting view for UNBUILT
            // ground — routing a built tile to it offers a Construct button that the
            // placement rules then refuse, which is the bug this closes.
            // BL-367: that "whole hex is one installation" assumption only holds for
            // exactly one building — once BL-366 lets a tile stack several, the hex
            // stays reachable as the grouped-by-stack tile view instead.
            //
            // BL-511 made the fallback the hovered tile's PROVINCE rather than
            // the tile; BL-598 puts it back on the tile. The province folded
            // into the tile Selection element as a set of accordion sections, so
            // selecting the tile IS selecting the province — with the tile's own
            // deposits, terrain and Construct door still reachable, which the
            // province-only selection cost a press each. `hovered_prov` survives
            // as the MIRROR (`selected_province`) the province outline reads.
            const uint32_t hovered_prov = (hovered_tile != null_entity)
                                          ? w.provinces.province_of(hovered_tile) : 0u;
            entity_id fallback = hovered_tile;
            if (hovered_tile != null_entity)
            {
                const auto tb = tile_to_bld.find(hovered_tile);
                const auto cnt_it = tile_bld_count.find(hovered_tile);
                const int  count = (cnt_it != tile_bld_count.end()) ? cnt_it->second : 0;
                if (tb != tile_to_bld.end() && count <= 1)
                    fallback = tb->second;
            }

            // Placeholder unit-cycle scaffolding: a REPEAT click on the same tile the
            // selection already sits on cycles Soldier -> Building -> Tile -> Soldier
            // (skipping any stage with nothing there), instead of re-resolving the
            // same marker/fallback every click. Deliberately checked BEFORE consulting
            // marker_hit at all: on a built tile the building's marker zone covers
            // nearly the whole hex (BL-367), so gating this on "marker_hit missed"
            // meant the cycle almost never fired there — every click just re-hit the
            // marker and reselected the same building, which read as "selection
            // depends on where I click" rather than as a cycle. Once we already know
            // this is the anchored tile, what to select next is fully determined by
            // the stage table below; marker position no longer matters. A click that
            // lands on a FRESH tile still goes through the marker-hit / fallback
            // precedence untouched, below. Units mostly don't exist yet in the live
            // economy (BL-393); this is built ahead of that on purpose (Ben's direction).
            if (hovered_tile != null_entity && state.selection_cycle_tile == hovered_tile)
            {
                entity_id unit_here = null_entity;
                for (const auto& [uid, uc] : w.units)
                    if (uc.position == hovered_tile) { unit_here = uid; break; } // first match; no ordering guarantee needed here

                entity_id building_here = null_entity;
                if (const auto tb2 = tile_to_bld.find(hovered_tile); tb2 != tile_to_bld.end())
                    building_here = tb2->second;

                // Cycle order, ruled by Ben 2026-08-24 (BL-598): BATTLE > UNIT >
                // BUILDING > TILE. FOUR rungs. The province rung sat between unit
                // and building until that ruling dissolved it: the province folded
                // into the tile Selection element as a set of sections, so the
                // tile rung reaches it and a rung of its own selected the same
                // ground twice.
                //
                // This is the CYCLE order only. The hit-test above is unchanged
                // and still resolves most-specific-first, so a building is still
                // reachable on the FIRST click.
                //
                // Rungs 1..3 are entities, so a null check is their liveness test.
                // Rung 0 is not — a battle has no entity id — so `stage_live`
                // stays, carrying that one rung's test rather than a null check
                // standing in for it.
                //
                // On bare ground with no unit, no building and no battle exactly
                // ONE rung is live and a repeat click re-selects the same tile.
                // That is the honest reading: there is nothing else there.
                const active_battle* battle_here =
                    (hovered_prov != 0) ? first_battle_in(w, hovered_prov) : nullptr;

                const entity_id stages[4] = { null_entity,      // battle rung
                                              unit_here,
                                              building_here,
                                              hovered_tile };
                const bool stage_live[4] = { battle_here != nullptr,
                                             unit_here != null_entity,
                                             building_here != null_entity,
                                             hovered_tile != null_entity };
                int stage = state.selection_cycle_stage;
                for (int i = 0; i < 4; ++i)
                {
                    stage = (stage + 1) % 4;
                    if (stage_live[stage])
                        break;
                }
                state.selection_cycle_stage = stage;
                state.selected_entity       = stages[stage];
                // The mirror: the province of the selection, which is non-zero
                // only on the TILE rung — the one card that has province sections
                // to show, and so the only one whose outline says anything.
                state.selected_province     = (stage == 3) ? hovered_prov : 0u;
                state.province_sync_entity  = state.selected_entity;
                if (stage == 0 && battle_here != nullptr)
                {
                    state.selected_battle_province = battle_here->province;
                    state.selected_battle_attacker = battle_here->attacker;
                    state.selected_battle_defender = battle_here->defender;
                }
                else
                {
                    state.clear_battle_selection();
                state.clear_lens_region_selection();
                }
            }
            else
            {
                state.selected_entity = (marker_hit != null_entity) ? marker_hit : fallback;
                // BL-598: the mirror, not a rival selection. A click that lands
                // on the TILE carries its province (the tile element has the
                // sections that read it); a marker hit does not, because a
                // building or unit card has no province section for the outline
                // to be about.
                state.selected_province =
                    (marker_hit == null_entity) ? hovered_prov : 0u;
                state.province_sync_entity = state.selected_entity;

                // Seed/reset the cycle anchor so a follow-up repeat click on this
                // SAME tile knows where to advance from next time.
                if (hovered_tile != null_entity)
                {
                    entity_id unit_here = null_entity;
                    for (const auto& [uid, uc] : w.units)
                        if (uc.position == hovered_tile) { unit_here = uid; break; }
                    entity_id building_here = null_entity;
                    if (const auto tb2 = tile_to_bld.find(hovered_tile); tb2 != tile_to_bld.end())
                        building_here = tb2->second;

                    state.selection_cycle_tile  = hovered_tile;
                    // Seed the anchor at the rung this click actually landed on,
                    // in the FOUR-rung order above, so the NEXT repeat click
                    // advances from the right place rather than replaying a rung.
                    // These indices must track `stages[4]` exactly: BATTLE 0,
                    // UNIT 1, BUILDING 2, TILE 3. (Getting this wrong is not a
                    // compile error and not a visual one either — it reads as
                    // "the cycle does not fire", which is how NR-504 was found.)
                    // The fallback is the tile itself since BL-598, so a plain
                    // click on ground seeds the TILE rung.
                    state.selection_cycle_stage = (marker_hit != null_entity && unit_here == marker_hit) ? 1
                                                 : (marker_hit != null_entity) ? 2   // a building marker
                                                 : (building_here != null_entity && fallback == building_here) ? 2
                                                                               : 3;  // plain ground = the tile
                }
                else
                {
                    state.selection_cycle_tile = null_entity;
                }
            }
            } // else: the click missed every structure boundary
        }
        else if (hovered_tile != null_entity)
        {
            // Placement mode: queue a construction request on the hovered tile.
            // app::render executes it against the mutable world (this canvas holds
            // only const world&) and clears it. See SELECTION.md / construction.hpp.
            state.construction.pending_tile   = hovered_tile;
            state.construction.pending_type   = state.construction.type;
            state.construction.pending_target = state.construction.target;
        }
    }

    // BL-737: the camera. One squash over every map-space vertex emitted since
    // v_map_start — fills, strokes, ground images, glyphs and labels alike —
    // about the canvas centre. Chrome drawn before the marker, the lens key,
    // ImGui windows (hover card, ledgers) and the foreground list are untouched.
    apply_camera();

    // Pan and zoom. Middle mouse button pans; scroll wheel zooms, anchored at
    // the cursor so the point under the mouse stays fixed.
    {
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            state.planetary_pan_x += io.MouseDelta.x;
            state.planetary_pan_y += io.MouseDelta.y;
        }

        // Keep horizontal pan bounded to one grid period. The grid wraps, so
        // this is visually identical but stops pan_x growing without limit.
        if (period_px > 0.0f)
            state.planetary_pan_x = std::fmod(state.planetary_pan_x, period_px);

        if (io.MouseWheel != 0.0f)
            state.planetary_wheel_accum += io.MouseWheel;
        // STEPPED (Ben, 2026-09-01): one wheel NOTCH = one x2 ladder rung, so
        // each zoom level pairs with a bake tier and the ground stays crisp at
        // every step. Deltas accumulate first — a notched mouse sends ±1 and
        // steps immediately; a precision wheel/trackpad sends fractions that
        // must add up to a notch before a rung fires (review fleet,
        // 2026-09-01). A fast multi-notch burst still steps one rung per
        // frame, deliberately: rungs are x2 apart and skipping two in one
        // frame reads as a teleport. The cursor-anchor math is unchanged.
        if (std::fabs(state.planetary_wheel_accum) >= 0.99f)
        {
            const int dir = state.planetary_wheel_accum > 0.0f ? +1 : -1;
            state.planetary_wheel_accum = 0.0f;
            const float new_zoom = planetary_zoom_stepped(zoom, dir);
            // World point under the cursor, kept fixed across the zoom change —
            // read through the tilt's inverse, re-projected through it (one
            // angle now, so both are the same squash — BL-737, BL-1246).
            const float new_sy = planetary_tilt_sy(new_zoom);
            const ImVec2 wp = { (mouse.x - view_origin.x) / zoom + grid_cx,
                                (mouse_g.y - view_origin.y) / zoom + grid_cy };
            const float target_gy = tilt_pivot + (mouse.y - tilt_pivot) / new_sy;
            state.planetary_pan_x = mouse.x - (wp.x - grid_cx) * new_zoom - canvas_centre.x;
            state.planetary_pan_y = target_gy - (wp.y - grid_cy) * new_zoom - canvas_centre.y;
            state.planetary_zoom  = new_zoom;
        }
    }
}

float planetary_zoom_stepped(float current, int direction)
{
    // A zoom that sits between rungs (verify's free-form set_zoom, an old
    // save's value) steps to the next rung IN THE DIRECTION OF TRAVEL, never
    // rounds first — stepping down from 3.0 lands on 2.5, not 1.25.
    constexpr float eps = 0.01f;
    const float k_exact = std::log2(std::max(current, kMinZoom) / kMinZoom);
    const int   k = direction > 0
        ? static_cast<int>(std::floor(k_exact + eps)) + 1
        : static_cast<int>(std::ceil (k_exact - eps)) - 1;
    const int   kc = std::clamp(k, 0, k_zoom_rungs - 1);
    return std::min(kMinZoom * static_cast<float>(1 << kc), kMaxZoom);
}

float planetary_tilt_sy(float zoom)
{
    // ONE angle at every rung (RENDERING.md § One angle, Ben 2026-10-09; the
    // stepped 22.5°/45° tilt of 2026-09-02 is retired): cos(22.5°).
    (void)zoom;
    return static_cast<float>(ground::k_tilt_sy);
}

} // namespace ui
