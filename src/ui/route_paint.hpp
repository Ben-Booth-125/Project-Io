#pragma once

#include "world/entity.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Route paint (BL-1253, roads painted) — the ROADS AND SEA LANES pass of the
// ground bake. docs/ui/RENDERING.md § Roads and sea lanes is the authority.
//
// Roads (`road_level`) and sea lanes (`lane_level`) are painted INTO the
// master, not stroked over it: a route along its own tiles on the
// four-cardinal grid, a junction paired into through-curves most-opposite
// first, an end or a three-way junction's odd branch a spoke, a lane's rungs
// skipped, one width per tier. BL-1261 (roads as tile sets): each piece runs
// between its edges' hashed CROSSING POINTS (route_crossing), a curve that
// finds its way inside the tile — to the lower ground, around the road plan's
// cluster — and carries its terrain's TREATMENT (hillside cut, forest
// corridor, embankment, verge, street) in the ground beside it. Each tier has
// a SURFACE, read by colour and value at a thread's width
// (BL-1257): Track pale packed dirt, Road paler gravel with a faint verge,
// Highway a slightly darker asphalt between pale shoulders (a centre line
// only where it can resolve), Rail a
// ballast bed with sleepers and twin rails (a tier value nothing in the world
// sets yet: the world has no rail rung), and a sea lane as a faint broken
// wake on the water.
//
// THE ROAD / STRUCTURE AGREEMENT. One function decides how a road meets a
// built tile: `tile_road_plan` (below). The route derivation bends a
// through-road around the cluster with it and ends an arriving road at the
// forecourt it names; the installation pass (structure_stamps.cpp build_tile)
// places the cluster, its pad and its forecourt apron with the SAME plan. So
// the two passes cannot disagree — there is one source of the layout, and
// each pass draws its own half of it. The route paint itself runs between the
// installation pass's ground parts (pads, fields, aprons, a town's paving)
// and its shadows and standing structures: a road lies on the yard it
// arrives at and on a town's paving (its street), structures' shadows fall
// across it, and no roof is ever under it.
//
// Pure and SDL/ImGui/Lua-free, like the rest of the bake, so the headless
// harness (tools/verify/ground_bake_check.cpp) compiles it.
// ---------------------------------------------------------------------------

struct world;

namespace ui::ground {

struct geometry;
struct bake_params;
struct bake_source;
struct pixel_rect;

/// Route tiers as the bake reads them (bake_source::road): the road ladder's
/// rungs 1-3 (road_level: Track, Road, Highway) and the RAIL rung, which the
/// world does not yet lay — the surface is ready for it (INDUSTRIALISATION.md,
/// the rail sink), exercised only by the harness.
inline constexpr std::uint8_t k_route_track   = 1;
inline constexpr std::uint8_t k_route_road    = 2;
inline constexpr std::uint8_t k_route_highway = 3;
inline constexpr std::uint8_t k_route_rail    = 4;

/// Painted widths, the WHOLE surfaced width as a fraction of the hex
/// circumradius (RENDERING.md § Roads and sea lanes; Ben, 2026-10-10, BL-1257:
/// thin pale threads, about 2-3% of a hex): Track 0.015, Road 0.022, Highway
/// 0.03, the sea lane 0.025; the rail bed 0.10. On the ground a road is a
/// detail of the land — the network as logistics is read through the
/// Throughput lens, which the canvas draws at the old 1 : 1.5 : 2 weights.
/// The margins (worn edge, verge, shoulder, the wake's spread) lie outside the
/// surfaced width.
inline constexpr float k_route_width[5] = { 0.0f, 0.015f, 0.022f, 0.03f, 0.10f };
inline constexpr float k_lane_paint_width = 0.025f;

/// One painted piece of a tile's route: a through-curve, a spoke (one free
/// end, at x[0]; the far end lies on a tile edge), a lone tile's yard, or a
/// forecourt spur, sampled as a polyline in TILE-RELATIVE canonical units (the
/// tile centre is the origin).
///
/// BL-1261 (roads as tile sets): a curve or spoke ENDS at its edge's CROSSING
/// POINT (route_crossing), heading straight across the edge, so the
/// neighbour's piece leaves the same point in the same direction; between its
/// ends it is a cubic bowed by a search that keeps it to the lower ground and
/// clear of the tile's road-plan cluster.
struct route_piece
{
    static constexpr int k_pts = 13; ///< 12 segments: a bend of radius 0.3 is < 0.01 off its polyline.
    enum kind_t : std::uint8_t { curve = 0, spoke = 1, yard = 2, spur = 3 };
    /// flags: the road's treatment that the TILE decides (the rest — forest
    /// corridor, hillside cut — the route pass reads per pixel from the cover
    /// and the slope under it).
    enum flag_t : std::uint8_t { f_street = 1, f_wet = 2, f_wood = 4 };
    float x[k_pts] = {}, y[k_pts] = {};
    float cum[k_pts] = {};      ///< Cumulative arclength at each point.
    float split = 0.0f;         ///< Arclength where the two halves meet (a curve's apex); spokes: 0.
    float bx0 = 0, by0 = 0, bx1 = 0, by1 = 0; ///< Polyline bounding box, tile-relative.
    std::uint8_t tier = 0;      ///< 1-4 road ladder; 0x80 | lane tier for a sea lane.
    std::uint8_t kind = curve;
    std::uint8_t flags = 0;
    std::uint8_t pad_ = 0;
};

/// BL-1261: where a route crosses tile edge (link @p n of tile (@p c, @p r);
/// links E, W, S, N = 0-3), TILE-RELATIVE to (c, r), and the edge's outward
/// unit normal. A hash of the edge — keyed on its WEST / NORTH tile's wrapped
/// column and row and its axis, so both tiles, and both sides of the cylinder
/// seam, compute one point — places it within the inner 70% of the edge
/// (k_cross_band either side of the midpoint). @p net separates networks that
/// can share an edge: 0 the road ladder, 1 rail (a rail point stands half the
/// band from the road's, so the two never meet), 2 a sea lane.
inline constexpr double k_cross_band = 0.35;
void route_crossing(int gw, int c, int r, int n, int net, double& x, double& y, double& nx, double& ny);

/// BL-1261: the painted reach of a piece past its surfaced half-width — the
/// tier's own margin and its terrain treatment (verge, hillside cut, forest
/// corridor, embankment; RENDERING.md § Roads and sea lanes).
double route_piece_out(const route_piece& pc);

/// How a road meets a BUILT tile — the roaded variant of its forms (RENDERING.md
/// § Roads and sea lanes: "a road meets a built tile through the building's
/// set"). Only a tile standing works (stack structures, no settlement) with at
/// least one road link is roaded: its cluster steps toward the tile's FREE side
/// (the direction farthest, by angle, from every road link's edge midpoint;
/// north preferred on a tie, so the road runs past the cluster's front),
/// shrinks by `scale`, and
/// stands its forecourt / yard / loading apron at (apx, apy) facing the road. A
/// through-road bows away from the cluster (route derivation); a road that
/// ends here ends at the apron. A town's tile is not roaded: its road runs
/// through as a street, and its blocks keep off it.
struct road_plan
{
    bool   roaded = false;
    double kx = 0, ky = 0;   ///< Cluster offset from the tile centre (canonical).
    double scale = 1.0;      ///< Cluster scale (slots, pad).
    double fx = 0, fy = -1;  ///< Unit free direction (toward the cluster).
    double radius = 0;       ///< The scaled cluster's keep-out radius around (kx, ky).
    double apx = 0, apy = 0; ///< The forecourt apron's centre (tile-relative).
};

/// THE ONE PLAN, read by both passes. Pure function of the tile's road links
/// (bake_source::route_links) and what it stands (stack count, settlement).
road_plan tile_road_plan(const bake_source& src, std::size_t i);

/// Derive links, the plans' pieces and the cull from bake_source::road / lane
/// (prepare_source reads road_level / lane_level of the REVEALED tiles in its
/// one tile pass — a masked tile carries no route, so none leaks through the
/// survey mask — and calls this after extract_installations). A caller that
/// edits road / lane, the classes or the installations in place calls it again.
void rederive_routes(bake_source& s);

/// The route pass over a window (absolute bake pixels px0, py0; pw x ph),
/// honouring @p tag exactly (never paints lock fill or the margin). @p cover:
/// 100 = water (a lane's wake paints only there), 0 = untouchable, else land
/// (a road paints only there). @p ground_lift: per pixel, the oblique
/// displacement (canonical) at which the ground under it was resolved, so a
/// road rides the lifted terrain.
void paint_routes(const bake_source& src, const geometry& g, const bake_params& p,
                  int px0, int py0, int pw, int ph, std::uint32_t* out,
                  const std::uint8_t* tag, const std::uint8_t* cover, const float* ground_lift);

/// Clearance (canonical) from point (x, y) — ABSOLUTE canonical ground
/// coordinates, x unwrapped — to the nearest ROAD's painted reach (surface
/// plus verges / shoulders / ditches): negative inside it, +1e9 with no road
/// near. The tree stamp and a town's blocks keep off the road with it.
double route_clearance(const bake_source& src, double x, double y);

/// The route half of the installation hash: road and lane tiers of every tile
/// whose route can change a pixel of the window (its own paint, a bend or
/// pairing it decides in a neighbour, the trees and the roaded cluster it
/// moves), folded into installation_hash. 0 when the source has no route.
std::uint64_t route_hash(const bake_source& src, const geometry& g,
                         int px0, int py0, int pw, int ph);

/// BL-1253, the partial re-bake: append to @p out the pixel boxes (absolute
/// bake pixels, UNALIGNED and unclipped; x beside the window) of everything a
/// ROUTE change between @p a and @p b repaints near the window: the pieces of
/// every tile whose pieces or plan differ (before and after), the trees they
/// clear or restore, and a re-planned tile's installation reach. Returns -1
/// when the grids differ, else the number of boxes appended.
int route_patch_boxes(const bake_source& a, const bake_source& b, const geometry& g,
                      const bake_params& p, int px0, int py0, int pw, int ph,
                      std::vector<pixel_rect>& out);

} // namespace ui::ground
