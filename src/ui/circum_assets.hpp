#pragma once

#include "circum_nodes.hpp"
#include "ui_state.hpp"

#include "world/world.hpp"

#include <imgui.h>

#include <vector>

// ---------------------------------------------------------------------------
// What sits at the Circumplanetary rung's orbital nodes (BL-1590 part 2) —
// docs/ui/CIRCUMPLANETARY.md § Orbital nodes and what sits at them.
// ---------------------------------------------------------------------------
// Satellites and stations drawn at the node they occupy, in their owner
// nation's identity colour: a satellite as a small round plate carrying its
// role's mark, a station as a larger square-cored mark sized by its grid
// (icons::satellite_marker / icons::station_marker). One in transit draws at
// its DESTINATION at half strength: it already holds that node's slot.
//
// WHERE AT A NODE. Around a ring, the marks walk down the ring's side from
// its equator — the low orbit's right side, the high orbit's left, mirroring
// the pip rows on its top. Under a diamond, in a row centred below it.
//
// IN TRANSIT (BL-1614). An asset in transit still draws its ghost at the
// DESTINATION (it already holds that slot), and — where the world recorded where
// the transit left from (`space_asset::transit_from`, `depart_tick`) and both
// ends are on this rung — a dashed segment from the origin to the ghost, with a
// solid marker at the share of the way travelled. A ring node's end sits on its
// ring, facing the other end; a surface's end is its body's centre. An origin
// off this rung, or a save from before the field, draws the ghost alone.
//
// Immediate-mode: laid out from the live world each frame and never kept.
// ---------------------------------------------------------------------------

namespace ui {

/// One satellite or station as laid out this frame.
struct circum_asset_mark
{
    entity_id id      = null_entity;  ///< The asset's id; a station's is its core's.
    entity_id owner   = null_entity;  ///< A nation.
    bool      station = false;
    space_satellite_role role = space_satellite_role::none; ///< Satellites only.
    bool      in_transit = false;
    bool      storm      = false;     ///< BL-1609: a dark satellite / a stormed station (the bolt draws).
    int       node_index = -1;        ///< Into the node marks it was laid against.
    ImVec2    pos {0.0f, 0.0f};
    float     r = 0.0f;               ///< Drawn half-extent, px.
};

/// Lay out every satellite and station that sits at (or travels to) one of
/// @p nodes. Lost assets draw nothing. Ascending id within a node, stations
/// first.
std::vector<circum_asset_mark> layout_circum_assets(const world& w,
                                                    const std::vector<circum_node_mark>& nodes);

/// One transit segment as laid out this frame (BL-1614).
struct circum_transit_mark
{
    entity_id id    = null_entity;   ///< The asset (a station's core id is its station's).
    entity_id owner = null_entity;
    ImVec2    from {0.0f, 0.0f};     ///< The origin's end.
    ImVec2    to   {0.0f, 0.0f};     ///< The destination's end (the ghost mark).
    float     share = 0.0f;          ///< Of the way travelled, in [0, 1].
    ImVec2    marker {0.0f, 0.0f};   ///< from + (to − from) × share.
};

/// The share of an econ-tick transit travelled: quarters elapsed since
/// @p depart_tick plus @p quarter_frac (the day within the quarter, [0, 1)),
/// over the quarters from departure to arrival; clamped to [0, 1]. A zero-length
/// transit reads 1. Pure.
float space_transit_share(int depart_tick, int arrival_tick, int econ_tick, float quarter_frac);

/// The quarter's elapsed fraction the transit share reads off the live day tick.
float circum_quarter_fraction(const world& w);

/// Where node @p n meets a segment on this rung, aimed toward @p toward: a ring
/// node's point on its ring, a diamond's centre, a surface's body centre. False
/// when @p n is not on this rung.
bool circum_node_endpoint(const std::vector<circum_node_mark>& nodes, orbital_node n, ImVec2 toward,
                          ImVec2* out);

/// Lay out a segment for every in-transit satellite and station core in
/// @p marks whose origin is recorded and on this rung.
std::vector<circum_transit_mark> layout_circum_transits(const world& w,
                                                        const std::vector<circum_node_mark>& nodes,
                                                        const std::vector<circum_asset_mark>& marks);

/// THE TRANSIT IDIOM (BL-1614, shared with space units and the Solar rung): a
/// dashed run from @p from to @p to at half strength of @p colour, and a solid
/// dot at @p share of the way. Constant pixel size.
void draw_space_transit(ImDrawList* dl, ImVec2 from, ImVec2 to, float share, ImU32 colour);

/// Draw the segments. Under the asset marks.
void draw_circum_transits(ImDrawList* dl, const std::vector<circum_transit_mark>& segs);

/// Draw the marks; @p hovered and @p selected (ids) ring theirs.
void draw_circum_assets(ImDrawList* dl, const std::vector<circum_asset_mark>& marks,
                        entity_id hovered, entity_id selected);

/// The mark under @p mouse, or −1. Nearest centre wins; then the lower index.
int hit_circum_asset(const std::vector<circum_asset_mark>& marks, ImVec2 mouse);

/// "Observation satellite", "Station". Empty for an id that is neither.
std::string space_asset_title(const world& w, entity_id id);

/// THE SHARED CONTENT BUILDER for a satellite or station (SELECTION.md § the
/// shared content builders): owner, role or modules and crew, node, state.
/// The hover card and the Selection band's card both draw it.
void draw_space_asset_summary(const world& w, entity_id id);

/// The hover card: the summary, through the shared hover-card primitive.
void draw_circum_asset_card(const world& w, entity_id id, ImVec2 mouse);

/// The Selection band's card for a selected satellite or station. Returns
/// false (and draws nothing) when @p ui.selected_entity is neither.
bool draw_space_asset_selection(const world& w, const ui_state& ui);

} // namespace ui
