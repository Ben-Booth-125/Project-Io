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
//   * RESEARCH, at all, and this one is a correctness rule rather than a
//     scheduling one. Research points accrue from population under BL-822, so a
//     research column would show a correlation it never measured — on the very
//     board Ben is using to judge whether the research levers work.
//
// WHAT IT DRAWS FROM THE EVENT LAYER (BL-916). The record carries the named
// moments — foundings, seats falling, realms ending, secessions, re-seatings,
// roads, civilisations, creeds, and in round 3 the migration's culture splits —
// and this surface shows them THREE ways: a ticker of the last few at or before
// the playhead, phrased as prose with the region's generated name; a marker
// pulsing at the event's region on the map for about one screen-second of
// playback; and the arc readout, which counts realms ended off the events
// rather than inferring a death from a polity missing in the next sample. The
// board's Population and Might columns come from the sample series the record
// already carried (BL-817).
//
// FLEET AND CARAVAN EXEMPLARS (BL-943, EXPLORATION.md sec Goods move as
// throughput: "the visual is a filter on that number, not a second
// simulation"). A `road_promoted` or
// `trade_link_opened` event IS a corridor's throughput crossing a threshold —
// that is the whole reason the road ladder promotes on usage — so drawing an
// exemplar there is reading an existing filter, never a second one. One sail
// or one caravan per crossing, fading over the same marker window the event
// ring does; never a mark per unit of cargo and never an animation of
// continuous traffic. Sea vs. land is read off the corridor's own baked
// `over_water`; the road rung (Track/Road/Post Road) is read off the event's
// own tier and drawn as size, colour and ring count so the three rungs never
// look alike. The one-time treasury consolidation (BL-932, "material becomes
// capital" at 1200 CE) draws as a separate gold burst at every capital, gated
// on the record actually reaching past 1200 CE — which, until the Exploration
// span's own tap is wired into `prehistory_timelapse` (still `tap=nullptr` in
// `hard_coded_world.cpp` as of BL-943), it never does, so this beat is
// correctly and silently inert today rather than firing on the Empire era's
// own close.
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

/// A bridge glyph: a promoted corridor's straight line crosses a river edge
/// here, in TILE units. Belongs to the `lapse_road_seg` whose line produced it.
struct lapse_bridge
{
    float col = 0.0f;
    float row = 0.0f;
};

/// ONE PROMOTED ROAD CORRIDOR (BL-917), baked once when the record lands —
/// straight-line anchor to anchor, in TILE units, the pane scale applied at
/// draw time like every other baked layer here.
///
/// Built from `lapse_event_kind::road_promoted` events ALONE: a corridor the
/// era only ever walked once carries no such event and never gets a
/// `lapse_road_seg`, so it is never drawn (CIVILISATION.md's "sparse road
/// network" — the settle tree is dense and mostly one-use, and drawing it
/// would be plaid). The GEOMETRY is fixed at bake time; the TIER a corridor
/// draws at is a function of the playhead year, so it is read at draw time
/// off the two year fields below rather than baked in.
struct lapse_road_seg
{
    uint16_t region_a = 0;
    uint16_t region_b = 0;
    float c0 = 0.0f, r0 = 0.0f; ///< Region A's anchor tile centre.
    float c1 = 0.0f, r1 = 0.0f; ///< Region B's anchor tile centre.

    /// The year this corridor first crossed into Track — always set, since a
    /// segment with no promotion event is never created. `INT32_MAX` never
    /// appears here; it is the sentinel for `year_road`/`year_post_road`
    /// below, which are legitimately unset on a corridor that never reached
    /// that rung.
    int32_t year_track = 0;
    int32_t year_road      = 0x7FFFFFFF; ///< The year it reached Road, or unset.
    int32_t year_post_road = 0x7FFFFFFF; ///< BL-940/BL-943: the year it reached Post Road, or unset.

    /// BL-943 — this corridor's straight line samples as majority water
    /// against the terrain band baked in `finish_history_lapse`. A road
    /// corridor is land by construction (the settle tree it is promoted off
    /// never crosses open water), but this is measured rather than assumed,
    /// so the fleet/caravan exemplar always reads the same fact a bridge
    /// glyph would.
    bool over_water = false;

    /// Where this corridor's straight line crosses a river edge — a pure
    /// geometric fact, computed once against `history_lapse::river_segs`.
    std::vector<lapse_bridge> bridges;
};

/// ONE OPEN SPAN, on a cross-border trade corridor between amicable seats
/// (BL-925). Unlike a road, which only ever ratchets UP a tier, this link can
/// close (a grudge) and reopen (it decays), so a corridor is baked as a list
/// of [open, close) year spans rather than a single promotion year.
struct lapse_trade_span
{
    int32_t year_open  = 0;
    int32_t year_close = 0x7FFFFFFF; ///< Unset — still open at the record's end.
};

/// ONE CROSS-BORDER TRADE CORRIDOR (BL-925), baked once when the record
/// lands — the same anchor-to-anchor geometry `lapse_road_seg` uses, built
/// from `lapse_event_kind::trade_link_opened` / `trade_link_closed` instead
/// of `road_promoted`. A pair that never traded amicably gets no segment
/// here, same "never walked, never drawn" idiom as the road network.
struct lapse_trade_seg
{
    uint16_t region_a = 0;
    uint16_t region_b = 0;
    float c0 = 0.0f, r0 = 0.0f; ///< Region A's anchor tile centre.
    float c1 = 0.0f, r1 = 0.0f; ///< Region B's anchor tile centre.

    /// BL-943 — see `lapse_road_seg::over_water`; the same measured fact,
    /// baked once. A cross-border trade corridor is the one link here that
    /// can legitimately cross a strait, which is exactly why the fleet/
    /// caravan exemplar reads this per corridor rather than assuming land.
    bool over_water = false;

    std::vector<lapse_trade_span> spans; ///< Ascending by `year_open`.
};

/// ONE SEA LANE (BL-1097), baked once when the record lands from
/// `lapse_event_kind::sea_lane_opened` — the same anchor-to-anchor geometry
/// `lapse_road_seg` uses, on the water. A lane is the water analogue of a
/// promoted road, not of a trade link: it only ever opens (traffic earned the
/// tier, and the record carries no decay), so it holds a single open year
/// and draws from that frame to the round's end. Distinct from a colonial
/// TIE, which would be drawn only while an overlord link stands; the lane is
/// what a colony leaves behind when the metropole falls (EXPLORATION.md sec
/// The colonial tie is a sea lane). A leg that never earned the tier gets no
/// segment here — the "never walked, never drawn" idiom of the road network.
struct lapse_lane_seg
{
    uint16_t region_a = 0;
    uint16_t region_b = 0;
    float c0 = 0.0f, r0 = 0.0f; ///< Region A's anchor tile centre.
    float c1 = 0.0f, r1 = 0.0f; ///< Region B's anchor tile centre.
    int32_t year_open = 0;      ///< The year the leg's uses crossed the lane tier.
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

    // --- The lineage palette (BL-919), derived ONCE at record time ---------
    //
    // On the Culture round the record's `owner` IS a culture index — the
    // migration record is folded from `region::culture.plurality()` — so the
    // map's colour is the culture's, and hundreds of cultures under twelve
    // identity colours would be plaid. These three vectors are indexed by
    // culture and are EMPTY on the Empires round, whose owners are polities;
    // `draw_lapse_map` falls back to the polity palette when they are empty.
    // Filled by `build_lineage_palette` from the culture tree alone: a pure
    // function of parent indices, no hashing, no per-frame work.

    /// Culture -> its root cradle culture (the hue FAMILY). A sibling surface
    /// tinting the Empires round's base by family reads this, not the colour.
    std::vector<int32_t> culture_family;

    /// Culture -> its own position on the wheel, 0-1.
    std::vector<float> culture_hue;

    // --- What the round before this one left (Ben, 2026-09-16) -------------
    //
    // CONTINUITY IS THE POINT: "after the culture round we should still render
    // its output on the time-lapse for empires, and have it fade out, rather
    // than just disappearing as soon as Next is pressed." The rounds are one
    // continuous history — the Empires span opens ON the migration's ground,
    // and BL-920 made that literal: at 400 BCE almost nothing is organised and
    // the map is culture ground waiting for city states. Drawing that ground
    // as empty threw away the one frame that says the two rounds are the same
    // world.
    //
    // So a round may carry its predecessor's LAST frame as a colour per
    // region, and paints it under ground nobody holds yet, fading out over the
    // opening stretch of its own span. Colour rather than owner indices
    // because the palettes differ (a culture's lineage hue against a polity's
    // identity slot) and a stale index into the wrong palette would be a
    // quietly wrong colour rather than an obviously missing one. Empty on the
    // Culture round, which has nothing behind it.
    std::vector<uint32_t> carry_colour;

    /// Culture -> generations below its root, 0 for a cradle culture.
    std::vector<int32_t> culture_depth;

    /// Culture -> the colour it is painted in (`palette::lineage_colour` of the
    /// two above). ImU32 layout, kept as uint32_t so this header stays off imgui.
    std::vector<uint32_t> culture_colour;

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

    /// The promoted road network (BL-917), baked from `lapse.events` once at
    /// record time — see `lapse_road_seg`. Empty on the Culture round, whose
    /// record carries no `road_promoted` events.
    std::vector<lapse_road_seg> road_segs;

    /// Amicable cross-border trade corridors (BL-925), baked from
    /// `lapse.events` once at record time — see `lapse_trade_seg`. Empty on
    /// the Culture round, whose record carries no `trade_link_opened` /
    /// `trade_link_closed` events.
    std::vector<lapse_trade_seg> trade_segs;

    /// Sea lanes (BL-1097), baked from `lapse.events` once at record time —
    /// see `lapse_lane_seg`. Empty on every round whose record carries no
    /// `sea_lane_opened` event (the Culture round, and any span in which no
    /// leg's traffic reached the tier).
    std::vector<lapse_lane_seg> lane_segs;

    // --- The industry layer (BL-1080), baked once at record time ------------
    //
    // ROUND 6's STORY IS INDUSTRY, and until this layer the round drew only
    // borders and seats. Two facts the Industrialisation span actually
    // computes, both carried by the record: a region's furnace crossing
    // (`lapse_event_kind::furnace_lit`, with the crossings the span inherited
    // noted at their own earlier years) and each polity's industry points
    // (`polity_sample::industry_points`). Railways are NOT drawn: the span
    // lays no rail of its own (INDUSTRIALISATION.md's rail sink is not a pass
    // that runs), so a rail layer would be invented. Empty / false on the
    // three earlier rounds, whose records carry neither.

    /// Region -> the calendar year it crossed the furnace, or INT32_MAX for
    /// never. The map marks a region from this year on.
    std::vector<int32_t> region_lit_year;

    /// True when any sample carries industry points — the board's industry
    /// column is drawn only then, so the three earlier rounds keep their board.
    bool industry_recorded = false;

    /// THE INDUSTRY HEAT's scale (BL-1080, map layer ruled by Ben 2026-09-24):
    /// the highest industry DENSITY any sample in the record carries — a
    /// polity's `industry_points` over the regions it holds at that step. The
    /// map heats each realm's ground by its density over this peak, so the
    /// heat is one scale across the whole span and visibly grows toward it.
    /// Zero on a record with no points (the three earlier rounds): no heat.
    double industry_density_peak = 0.0;

    // --- The hard border (BL-1090; Ben, 2026-09-24, rulings R9) ------------
    //
    // A REALM WHOSE SHARE OF THE WORLD'S PEOPLE stands above a fixed threshold
    // draws HARD: 2 px of dark and a 1 px inner stroke in its own colour on
    // every edge of its ground, and its board row is bold (STARTUP.md
    // § Identity across the rounds). The share is the board's own rank
    // column -- the polity's sampled population over every living polity's at
    // the recorded step -- so the bold row and the heavy border are one fact.
    // HYSTERESIS across steps keeps a realm sitting on the line from
    // flickering: it turns hard at `lapse_hard_on_q` and soft only under
    // `lapse_hard_off_q`. Derived once per record by `lapse_hard_walk`, a
    // forward walk over the steps; a per-step bitmap so a frame reads one
    // byte per owner and re-derives nothing.

    /// [step * hard_stride + polity] != 0 when the polity is hard at that
    /// recorded step. Empty on a record with no samples (the Culture round).
    std::vector<uint8_t> polity_hard;
    int32_t hard_stride = 0; ///< The polity count the bitmap is laid out by.

    /// Polity -> hard at the PREDECESSOR round's close (`lapse_hard_at_close`
    /// of the round before), set at the hand-over exactly as `polity_slot` is
    /// inherited. THE FLAG CARRIES BY ID: a realm hard at 1200 on the Empires
    /// round opens the Exploration round hard, and the walk starts from this
    /// state rather than from soft. Empty when nothing came before.
    std::vector<uint8_t> hard_carry;

    /// A CIVILISATION'S DIAMOND STAYS ACROSS THE HAND-OVER (BL-1094): the
    /// (coining realm, coining region) pairs marked at the predecessor round's
    /// close, carried exactly as `hard_carry` is. A resumed span never re-notes
    /// `civilisation_formed`, so without the carry every diamond vanished at the
    /// 1200 and 1660 seams. Drawn ahead of this record's own events.
    struct civ_mark { uint16_t polity; uint16_t region; };
    std::vector<civ_mark> civ_carry;

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

/// Derive the lineage palette (BL-919) from the culture tree.
///
/// THE RULE. Hue comes from the root cradle: the roots are spread evenly around
/// the wheel, so every family owns one wedge of it. A daughter takes its
/// parent's hue shifted by a FIXED step — siblings alternate sides of the parent
/// (+1, -1, +2, -2 steps...) so they are told apart — and the shift is bounded to
/// the family's own wedge, so no lineage ever wanders into a neighbour's hue.
/// Lightness steps down by depth (`palette::lineage_colour`), bounded so a deep
/// lineage stays readable. A family is recognisable at a glance; a member on a
/// second look.
///
/// @param parent  One entry per culture, in `creed_state::cultures` order: the
///                parent's index, or -1 for a cradle culture. A parent index is
///                always lower than its daughter's (creeds.hpp), and the walk is
///                guarded against a malformed tree regardless.
/// @param folded_into  BL-1017: optional, one entry per culture —
///                `culture::folded_into`, -1 for a name that outlived the
///                Colonisation round. A folded culture takes NO wedge and NO
///                sibling step; it wears the colour of the name that absorbed
///                it. So the wheel is spent on the living tree only, and a
///                family's daughters are spaced by the peoples who held ground
///                rather than by every split the migration coined. nullptr =
///                nothing folded.
///
/// Deterministic and pure: a function of @p parent and @p folded_into alone.
/// Call it once, at record time; nothing here belongs in a frame.
void build_lineage_palette(history_lapse& h, const std::vector<int32_t>& parent,
                           const std::vector<int32_t>* folded_into = nullptr);

/// Paint the political map for one already-materialised ownership slice into the
/// current window's remaining content region.
///
/// TAKES NO MOUSE INPUT, and that is inherited rather than incidental: the globe
/// it replaces takes none by design (STARTUP.md § The globe — and why it does not
/// take input), and BL-829 does not lift the rule. Pure draw-list painting, no
/// widgets, so there is nothing here for a click to land on.
void draw_lapse_map(const history_lapse& h, const std::vector<uint16_t>& slice,
                    int year);

/// The previous round's ground, still showing under unclaimed land at @p year:
/// 1 at the round's first year, 0 once the opening stretch has passed. Public
/// so the caller can tell whether it is worth building the carried colours.
float lapse_carry_fade(const history_lapse& h, int year);

/// THE INDUSTRY HEAT of one polity at @p year, in [0, 1] (BL-1080): the square
/// root of its industry points per region held, at the recorded step at or
/// before the year, over `industry_density_peak`. BY POLITY TERRITORY, not by region,
/// because the record samples points per polity only — a per-region series
/// would be new save-format state for a heat the polity series already draws.
/// 0 with no sample, no points, or a record that carries none. Public so the
/// verify API reads the same number the map draws.
float lapse_industry_heat(const history_lapse& h, uint16_t polity, int year);

/// THE HARD-BORDER THRESHOLD, per-mille of the world's people (BL-1090).
/// MEASURED, NOT CHOSEN (Rule 0b): `history_sweep`'s BL-1090 block over the
/// 16 curated seeds, 2026-09-25, every recorded step of the Empires span. The
/// largest realm's share of people PEAKS at 8-17% across the library and
/// stands at 6-15% at 1000 CE and at the close; the living-polity share
/// distribution is 88% under 5%, 11% at 5-10%, under 1% at 10-15%, and 0.1%
/// at 15-20% (94 of 87,900 polity-steps, all on seed 28). So: a 20% line
/// bolds NOTHING on any curated world; 15% bolds one realm on one world
/// (seed 28) and briefly two; 10% bolds on 6 of 16 worlds at 1000 CE (1-6
/// realms, seed 28's 6 the most), 5 at the close, and none on the peaceable
/// city-state worlds (seeds 11, 31, 13, 10, 9 -- 52-133 powers, top share
/// 6-9%). 10% is the line that separates the realms that read as empires
/// from a field that never reaches it.
///
/// HYSTERESIS: at 10% the reading found 35 dip episodes that came back (15
/// of them one step, i.e. twenty years), the deepest single-step dip 39
/// per-mille under the line and the deepest of three steps or fewer 54; a
/// margin of 40 therefore holds every single-step dip observed. On->off
/// transitions at 10%: 69 raw, 36 at a 30 margin, 22 at 50. A realm turns
/// hard at `lapse_hard_on_q` and soft only under `lapse_hard_off_q`.
/// `history_sweep` includes this header and prints what the pinned pair
/// bolds beside its candidate table, so the number here and the number
/// measured cannot drift apart unnoticed.
constexpr int lapse_hard_on_q  = 100;
constexpr int lapse_hard_off_q = 60;

/// Derive `polity_hard` / `hard_stride` from the record's steps: the forward
/// walk with hysteresis, seeded from `hard_carry`. Called by
/// `finish_history_lapse`; public so a caller holding a record that was never
/// finished (a round landed but not yet drawn) can still read its close.
/// Pure: a function of the samples and the carry alone.
void lapse_hard_walk(history_lapse& h);

/// Is @p polity hard at @p year -- the recorded step at or before it, or the
/// carried state before the first step (so a carried realm draws hard from
/// the round's first frame). False on a record with no samples.
bool lapse_polity_hard(const history_lapse& h, uint16_t polity, int year);

/// Polity -> hard at the record's closing step, for the next round's
/// `hard_carry`. Walks the record itself when @p h was never finished.
std::vector<uint8_t> lapse_hard_at_close(const history_lapse& h);

/// The civilisation diamonds standing at a record's close -- its own `civ_carry`
/// plus every `civilisation_formed` it recorded -- for the next round's
/// `civ_carry` (BL-1094). Deduplicated by (polity, region).
std::vector<history_lapse::civ_mark> lapse_civ_marks_at_close(const history_lapse& h);

/// The colour @p owner is drawn in on @p h — a culture's lineage hue on the
/// Culture round, a polity's identity slot elsewhere. Public so one round can
/// hand its final frame to the next as colours rather than as indices into a
/// palette that round does not have (the carry above). ImU32 layout, kept as
/// uint32_t so this header stays off imgui.
uint32_t lapse_owner_colour(const history_lapse& h, uint16_t owner);

/// The ordered, capped top-16 board.
///
/// ORDERED AND CAPPED IS THE DESIGN, not a display convenience (BL-830): sixteen
/// rows re-ranking as the centuries pass is the surface that shows RISE AND FALL,
/// and an uncapped list of everything would show none of it.
///
/// RANKED BY SHARE OF PEOPLE (BL-1000; Ben, 2026-09-15, NR-876): each polity's
/// sampled population over every living polity's at the recorded step at or
/// before the playhead — `history_sweep`'s own arithmetic, so a figure on the
/// board is the figure the sweep prints. Share of land is the second column
/// and the tie-break; where a step has no samples (the opening years; the whole
/// Culture round) every row holds no one and the board orders by land.
///
/// @param lagged       The same slice taken a few centuries earlier. It is what
///                     makes an ENTRY or an EXIT visible: a row that was not on
///                     the board then is marked, and the rank delta is drawn
///                     against it.
/// @param year         The playhead year: the People, Pop and Might columns
///                     read the recorded step AT OR BEFORE it (BL-916 on
///                     BL-817's series), so the board and the map show the
///                     same instant.
/// @param lagged_year  The year @p lagged was taken at, so the lagged board is
///                     ranked by the same rule and a rank delta means a move
///                     on one axis rather than a change of axis.
void draw_lapse_scoreboard(const history_lapse& h,
                           const std::vector<uint16_t>& slice,
                           const std::vector<uint16_t>& lagged,
                           int year, int lagged_year);

/// BL-916 -- one event as a line of prose, with its region's generated name and
/// the year. Never an Earth name: every noun here comes off the region table.
std::string lapse_event_prose(const history_lapse& h, const lapse_event& e);

/// BL-916 -- the ticker: the last `max_rows` narrated events at or before
/// @p year, oldest first, the newest bright. Road promotions are ringed on the
/// map but not narrated (see the .cpp for the measurement). Six rows, so the
/// arc readout under it stays above the column's fold at 1080p.
void draw_lapse_ticker(const history_lapse& h, int year, int max_rows = 6);

/// BL-916 -- how many years an event stays marked on the map after it happens:
/// about one screen-second of playback, derived from the span exactly as the
/// transport's rate is (span / 30 s), so the marker is a property of the record
/// and not of the frame clock. Never below one year.
int lapse_marker_window_years(const history_lapse& h);


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
/// DERIVED, NOT STORED. Every number comes from `era_timelapse::samples` and
/// `era_timelapse::events`, which generation already emits for the replay.
///
/// TWO READINGS WERE WRONG UNDER SAMPLES ALONE (BL-916). The step record samples
/// LIVING polities only, so "ended holding none" was structurally zero on every
/// world — a dead realm is absent from the next step, not present with zeros —
/// and the capture read "0 destroyed" on a world where a dozen powers fell. And
/// peak share divided by the FINAL region stride while the region count grows
/// four to six times inside the run, so an early empire read a quarter of its
/// true share. `eliminated` now counts `realm_ended` events, and the peak share
/// is taken against the regions that EXISTED at the peak's own step.
///
/// THE SHARE THE READOUT PRINTS IS OF PEOPLE (BL-1000; Ben, 2026-09-15, NR-876),
/// the same column the board ranks by: `peak_share_pop_q` is `history_sweep`'s
/// `peak_share_pop_q` arithmetic exactly — a century walk from the record's
/// first year, the largest polity's sampled population over every living
/// polity's at the step at or before each mark — and `end_share_pop_q` is its
/// `top_share_pop_q`, so the panel and the sweep print one figure for one
/// world. The region-share fields stay, as the fallback for a record that
/// carries no samples and for the shape test, whose definition is the sweep's.
struct lapse_arc
{
    int polities         = 0; ///< Distinct polities ever seen holding ground.
    int eliminated       = 0; ///< Realms ended -- the `realm_ended` event count.
    int peak_share_q     = 0; ///< Largest share of REGIONS any one polity ever held, per-mille of the regions live at that step.
    int rose_and_fell    = 0; ///< ...that doubled and then fell back under 60% of peak.
    int biggest_end_q    = 0; ///< Largest share of regions still held at the end, per-mille.
    int smallest_end     = 0; ///< Regions held by the smallest surviving polity.
    int peak_share_pop_q = 0; ///< Largest share of PEOPLE any one polity held at a century mark, per-mille; 0 with no samples.
    int end_share_pop_q  = 0; ///< Largest share of people held at the closing step, per-mille.
};

/// Walks the replay record once. Cheap: linear in `samples`.
lapse_arc summarise_lapse_arc(const history_lapse& h);

/// Renders the summary as prose a player can read at a glance.
void draw_lapse_arc(const history_lapse& h);

/// A signed calendar year as this surface prints it ("400 BCE", "1200 CE").
std::string lapse_year_label(int year);

} // namespace ui
