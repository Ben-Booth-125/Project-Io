#pragma once

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
// DRAW COST. ImGui carries 16-bit draw indices, which is why the wizard's globe
// is 48 meridian slices rather than ~7,500 projected hexes. The map is drawn at
// TILE resolution but RUN-MERGED along each row: a political map is long runs of
// one colour, so a 261x121 raster collapses to on the order of a thousand rects.
// The region granularity of the record is what makes that possible — there is no
// per-tile ownership anywhere in this file, only a tile-to-region assignment
// computed once.
// ---------------------------------------------------------------------------

namespace ui {

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

    /// Culture -> generations below its root, 0 for a cradle culture.
    std::vector<int32_t> culture_depth;

    /// Culture -> the colour it is painted in (`palette::lineage_colour` of the
    /// two above). ImU32 layout, kept as uint32_t so this header stays off imgui.
    std::vector<uint32_t> culture_colour;

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
/// Cheap and one-shot: a multi-source breadth-first walk over ~31,500 tiles with
/// the columns wrapping and the rows not, exactly as the world's own grid does.
void finish_history_lapse(history_lapse& h, const uint8_t* packed, std::size_t packed_len);

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
///
/// Deterministic and pure: a function of @p parent alone. Call it once, at
/// record time; nothing here belongs in a frame.
void build_lineage_palette(history_lapse& h, const std::vector<int32_t>& parent);

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
/// @param year    The playhead year, for the Population and Might columns: they
///                read the recorded step AT OR BEFORE it (BL-916 on BL-817's
///                series), so the board and the map show the same instant.
void draw_lapse_scoreboard(const history_lapse& h,
                           const std::vector<uint16_t>& slice,
                           const std::vector<uint16_t>& lagged,
                           int year);

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
struct lapse_arc
{
    int polities        = 0; ///< Distinct polities ever seen holding ground.
    int eliminated      = 0; ///< Realms ended -- the `realm_ended` event count.
    int peak_share_q    = 0; ///< Largest share any one polity ever held, per-mille of the regions live at that step.
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
