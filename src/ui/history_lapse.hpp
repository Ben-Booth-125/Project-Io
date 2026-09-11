#pragma once

#include "world/components.hpp"    // terrain_landform, for the packed terrain raster (BL-915)
#include "world/era_timelapse.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// history_lapse — round 4's time-lapse, as a view over the recorded era
// ---------------------------------------------------------------------------
//
// WHAT THIS IS FOR. The New World wizard's round 4 (STARTUP.md § Rounds 4 and 5)
// replaces the globe with a 2D MAP and plays the Era -1 ownership record across
// it, with an ordered top-16 board beside it. Ben, 2026-09-08, at the live app:
// *a globe shows a world; a map shows a FRONTIER*, and the frontier is the whole
// subject of the round — a border that stalls at a strait reads as a stall on a
// map and as foreshortening on a sphere.
//
// A REPLAY, NEVER A RE-RUN. Everything drawn here is folded out of
// `era_timelapse` — the change list generation already emits — through
// `owner_slice_at`. There is no sim on this side of the seam, which is the same
// ruling the History ledger's Ages view was rebuilt on (NR-733, 2026-08-30): a
// second caller of the Era -1 invocation is a second invocation, and it drifts.
// This file adds NO caller; the record it draws comes out of generation's own
// `generation_report`.
//
// WHAT IT DELIBERATELY DOES NOT DRAW.
//   * CULTURE MIXES (BL-826). They exist and they are not drawn: two overlapping
//     colour meanings on one map is how a legible surface becomes a plaid.
//   * POPULATION and MILITARY columns. They need BL-817's per-polity sample
//     series, which the record does not carry yet — the seam for them is marked
//     in `draw_lapse_scoreboard`.
//   * RESEARCH, at all, and this one is a correctness rule rather than a
//     scheduling one. Research points accrue from population under BL-822, so a
//     research column would show a correlation it never measured — on the very
//     board Ben is using to judge whether the research levers work.
//
// DRAW COST. ImGui carries 16-bit draw indices, which is why the wizard's globe
// is 48 meridian slices rather than ~7,500 projected hexes. The map is drawn at
// TILE resolution but RUN-MERGED along each row: a political map is long runs of
// one colour, so a 261x121 raster collapses to on the order of a thousand rects.
// The region granularity of the record is what makes that possible — there is no
// per-tile ownership anywhere in this file, only a tile-to-region assignment
// computed once.
// ---------------------------------------------------------------------------

namespace ui {

// ---------------------------------------------------------------------------
// The terrain the map is drawn on (BL-915)
// ---------------------------------------------------------------------------
//
// THE GROUND, NOT ONLY THE FILL (STARTUP.md § The pass rounds draw the ground,
// not only the fill; Ben, 2026-09-11). Rivers and relief are drawn under the
// culture or polity fill and the fill is a translucent TINT over them, because a
// frontier is legible only against the terrain it crosses — a border that stops
// at a river reads as a river border, and on a flat colour it read as nothing.
//
// The wizard's surface build packs one uint16 per tile: the landform in the low
// four bits, the six river-edge bits above it, and the six downstream bits above
// those. Only facts the tile pass already decided; nothing is re-derived here.

/// Pack a tile's landform and river edges for the lapse map's terrain base.
inline uint16_t pack_lapse_terrain(terrain_landform lf, uint8_t river_edges,
                                   uint8_t river_downstream)
{
    return static_cast<uint16_t>((static_cast<unsigned>(lf) & 0x0Fu)
                                 | ((river_edges      & 0x3Fu) << 4)
                                 | ((river_downstream & 0x3Fu) << 10));
}
inline terrain_landform lapse_landform(uint16_t t)
{
    return static_cast<terrain_landform>(t & 0x0Fu);
}
inline uint8_t lapse_river_edges(uint16_t t)      { return static_cast<uint8_t>((t >> 4)  & 0x3Fu); }
inline uint8_t lapse_river_downstream(uint16_t t) { return static_cast<uint8_t>((t >> 10) & 0x3Fu); }

/// What the wizard's async surface build hands back: the packed axes the globe
/// samples (`preview_pack`) and the packed terrain the lapse maps draw.
struct wizard_surface
{
    std::vector<uint8_t>  comp;
    std::vector<uint16_t> terrain;
};

/// One run-merged rect of the baked terrain base, in TILE units — the pane
/// scale is applied at draw time, so the bake survives a pane resize.
struct lapse_base_run
{
    uint16_t row;
    uint16_t c0;
    uint16_t c1;   ///< Exclusive.
    uint8_t  kind; ///< 0 flat land, 1 valley, 2 highland, 3 mountain, 4 canyon/rift.
};

/// One river segment, tile centre to downstream tile centre, in TILE units.
struct lapse_river_seg
{
    int16_t c0, r0, c1, r1;
};

/// One relief edge: a run along the NORTH or SOUTH edge of a row where the
/// landform band steps down to the neighbouring row — the lit and shadowed
/// rims of a range. Edges, not rows: shading every mountain row reads as
/// scanlines, shading where the ground drops reads as relief.
struct lapse_relief_seg
{
    uint16_t row;
    uint16_t c0;
    uint16_t c1;  ///< Exclusive.
    uint8_t  lit; ///< 1 = the north rim (lit), 0 = the south rim (shadow).
};

/// The recorded era, plus the derived fields the map and the board need.
///
/// Lifted whole out of `generation_report` on the worker that produced it (see
/// `app::launch_wizard_history_run`), so the draw thread holds a value and never
/// reaches into a report another thread might be writing.
struct history_lapse
{
    /// The record itself — the replay substrate, and the only thing here that
    /// generation actually emits.
    era_timelapse lapse;

    int grid_w = 0; ///< Homeworld raster width the region positions are in.
    int grid_h = 0;

    // --- The settled regions, flattened -----------------------------------
    // The board and the map need a region's position and its name and nothing
    // else, so `settlement_state` is not carried across the thread boundary.
    std::vector<int32_t>     region_col;
    std::vector<int32_t>     region_row;
    std::vector<std::string> region_name;

    /// Generation's own era counters, for the honest readout under the map.
    int64_t battles   = 0;
    int64_t conquests = 0;
    int64_t foundings = 0;

    // --- Derived on the draw thread by `finish_history_lapse` --------------

    /// Tile -> nearest region index over LAND, or -1 for water and for land no
    /// region reached. This is what turns a change list over regions into a
    /// frontier a player can read: without it the map is a scatter of dots and
    /// a border that stalls at a strait looks like a gap in the dots.
    std::vector<int32_t> tile_region;

    /// Tiles assigned to each region — the AREA "share of land" is measured in.
    /// Region COUNT would be the cheaper metric and a dishonest one: regions
    /// differ in area by several times, so counting them measures the placement
    /// pass rather than the ground anybody holds.
    std::vector<int32_t> region_area;

    /// Total land tiles any region reached — the denominator's ceiling.
    int32_t claimed_land = 0;

    /// Polity index -> the region it FIRST held in the record. The board has no
    /// other name to use: `polity` (history_sim.hpp) carries no name field, and
    /// the seat it rose from is a real, generated, in-world name for it rather
    /// than an invented label. Every such name comes out of the seeded banks in
    /// the founders' own tongue — never an Earth proper noun.
    std::vector<int32_t> polity_seat;

    /// Polity index -> palette slot (`palette::lapse_polity_colour`), from the
    /// greedy colouring over the polity ADJACENCY graph (BL-915): two polities
    /// are adjacent if they ever held neighbouring regions anywhere in the
    /// record, and no two adjacent polities share a slot. Assigned once at
    /// record time by `assign_polity_colours`; the map and the board both read
    /// it, so a row and its territory cannot disagree.
    std::vector<int32_t> polity_slot;

    // --- The baked terrain base (BL-915) -------------------------------------
    // Run-merged ONCE, in tile units, when the record lands. Per frame the map
    // only scales and emits these; nothing under the fill is re-merged.
    std::vector<lapse_base_run>   base_runs;
    std::vector<lapse_river_seg>  river_segs;
    std::vector<lapse_relief_seg> relief_segs;

    /// The map prints its primitive count to stderr ONCE per record, so the
    /// draw-index bound is a measured number in every capture log. Mutable
    /// because the draw takes the record by const reference and this is not
    /// state the map reads.
    mutable bool prim_report_done = false;

    bool empty() const { return lapse.empty(); }

    /// True once `finish_history_lapse` has run against a surface.
    bool derived() const { return !tile_region.empty(); }
};

/// Assign every land tile to its nearest region and total the areas.
///
/// @param packed  The wizard's own packed homeworld raster (`ui::preview_pack`),
///                which is the SAME surface "Begin" builds — so the coastline the
///                frontier stalls at is the coastline the campaign has. Pass
///                nullptr / 0 and this is a no-op; the caller retries next frame.
///
/// @param terrain The packed landform + river raster (`pack_lapse_terrain`) the
///                same build produced, or nullptr: the map then draws a flat base
///                and no rivers rather than waiting, because the political
///                record is the round's subject and the ground is its context.
///
/// Cheap and one-shot: a multi-source breadth-first walk over ~31,500 tiles with
/// the columns wrapping and the rows not, exactly as the world's own grid does.
/// Then the terrain bake, the polity adjacency graph and the greedy colouring —
/// all linear in the raster or in the change list.
void finish_history_lapse(history_lapse& h, const uint8_t* packed, std::size_t packed_len,
                          const uint16_t* terrain = nullptr, std::size_t terrain_len = 0);

/// Greedy-colour the polities over their adjacency graph so no two neighbours
/// share a palette slot (BL-915). Called by `finish_history_lapse`; exposed so a
/// sibling item can re-run it with a HUE FAMILY per polity.
///
/// @param hue_family Optional, one entry per polity: polities in the same family
///                   are assigned slots from the same band of the palette where
///                   the adjacency constraint allows. nullptr = no families; the
///                   walk hands out the lowest free slot.
void assign_polity_colours(history_lapse& h, const std::vector<int32_t>* hue_family = nullptr);

/// Paint the political map for one already-materialised ownership slice into the
/// current window's remaining content region.
///
/// TAKES NO MOUSE INPUT, and that is inherited rather than incidental: the globe
/// it replaces takes none by design (STARTUP.md § The globe — and why it does not
/// take input), and BL-829 does not lift the rule. Pure draw-list painting, no
/// widgets, so there is nothing here for a click to land on.
void draw_lapse_map(const history_lapse& h, const std::vector<uint16_t>& slice,
                    int year);

/// The ordered, capped top-16 board.
///
/// ORDERED AND CAPPED IS THE DESIGN, not a display convenience (BL-830): sixteen
/// rows re-ranking as the centuries pass is the surface that shows RISE AND FALL,
/// and an uncapped list of everything would show none of it.
///
/// @param lagged  The same slice taken a few centuries earlier. It is what makes
///                an ENTRY or an EXIT visible: a row that was not on the board
///                then is marked, and the rank delta is drawn against it.
void draw_lapse_scoreboard(const history_lapse& h,
                           const std::vector<uint16_t>& slice,
                           const std::vector<uint16_t>& lagged);

/// BL-891 -- WHAT HAPPENED IN THIS WORLD, read off the record the round already
/// holds. The scoreboard shows a SNAPSHOT that re-ranks as the centuries pass;
/// this answers the different question a player rolling a world actually asks --
/// *did anything happen here* - without them having to watch the whole replay.
///
/// The four quantities are `GENERATION_STRATEGY.md` sec The asymmetry is
/// POLITICAL as well as economic, in the order it states them: polities
/// ELIMINATED, an empire FORMED (the largest share anyone ever reached), empires
/// that then FELL, and how UNEQUAL the survivors are. Definitions are the sweep's
/// own, so the panel and `history_sweep` cannot drift: ROSE means a peak at least
/// double the start and at least three regions more; FELL means ending at or
/// under 60% of that peak.
///
/// DERIVED, NOT STORED. Every number comes from `era_timelapse::samples`, which
/// generation already emits for the replay -- BL-891 adds no field to the record
/// and nothing to the save seam.
struct lapse_arc
{
    int polities        = 0; ///< Distinct polities ever seen holding ground.
    int eliminated      = 0; ///< ...that ended holding none.
    int peak_share_q    = 0; ///< Largest share any one polity ever held, per-mille.
    int rose_and_fell   = 0; ///< ...that doubled and then fell back under 60% of peak.
    int biggest_end_q   = 0; ///< Largest share still held at the end, per-mille.
    int smallest_end    = 0; ///< Regions held by the smallest surviving polity.
};

/// Walks the replay record once. Cheap: linear in `samples`.
lapse_arc summarise_lapse_arc(const history_lapse& h);

/// Renders the summary as prose a player can read at a glance.
void draw_lapse_arc(const history_lapse& h);

/// A signed calendar year as this surface prints it ("400 BCE", "1200 CE").
std::string lapse_year_label(int year);

} // namespace ui
