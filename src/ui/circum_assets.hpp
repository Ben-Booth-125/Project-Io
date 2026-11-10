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
// IN TRANSIT HAS NO SEGMENT. The rung would draw a dashed run along the link
// with a marker at its share of the way, but an asset in the world carries
// only where it is going and when it arrives: a satellite's move keeps its
// first launch's tick and no origin node, so neither end of the run nor its
// share can be drawn honestly. The ghost at the destination is what the world
// can answer.
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
    int       node_index = -1;        ///< Into the node marks it was laid against.
    ImVec2    pos {0.0f, 0.0f};
    float     r = 0.0f;               ///< Drawn half-extent, px.
};

/// Lay out every satellite and station that sits at (or travels to) one of
/// @p nodes. Lost assets draw nothing. Ascending id within a node, stations
/// first.
std::vector<circum_asset_mark> layout_circum_assets(const world& w,
                                                    const std::vector<circum_node_mark>& nodes);

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
