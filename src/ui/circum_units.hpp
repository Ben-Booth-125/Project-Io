#pragma once

#include "circum_assets.hpp"
#include "circum_nodes.hpp"
#include "ui_state.hpp"

#include "world/world.hpp"

#include <imgui.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

class recipe_registry;

// ---------------------------------------------------------------------------
// Space fleets, debris and storm marks on the near-space rungs (BL-1609) —
// docs/ui/CIRCUMPLANETARY.md § Orbital nodes and what sits at them,
// docs/military/SPACE_FLEETS.md, docs/ui/SELECTION.md § The space-unit element.
// ---------------------------------------------------------------------------
// FLEETS. A fleet is one nation's space units at one node (SPACE_FLEETS.md). The
// rung draws ONE mark per (node, owner) — the lead unit's hull mark (the lowest
// id), the count as a badge — on the ring's upper side (the low orbit's left,
// the high orbit's right; the assets walk down the other side) or above a
// diamond. A unit IN TRANSIT draws in the transit idiom (BL-1614,
// `draw_space_transit`): a dashed run from its origin node to its destination,
// with its own mark at its share of the way (days gone over the path's days).
// Units travelling together — one owner, one origin, one destination, one days
// remaining — are one mark. A landed Lander draws at its body's centre.
//
// DEBRIS. A node holding debris above the floor draws `icons::debris` beside it,
// sized by the level (log scale), its level on the node's hover card.
//
// SOLAR STORMS. A satellite the quarter's storm darkens, or a station whose
// modules it stops, carries `icons::storm_bolt` at its upper right — the asset
// layout marks it (circum_assets.cpp); this file holds the shared test.
//
// Immediate-mode: laid out from the live world each frame and never kept.
// ---------------------------------------------------------------------------

namespace ui {

/// One fleet (or one travelling group) as laid out this frame.
struct circum_fleet_mark
{
    entity_id lead  = null_entity;   ///< The lowest unit id: what a press selects.
    entity_id owner = null_entity;
    int       hull  = 0;             ///< The lead's space_hull.
    int       count = 0;
    bool      in_transit = false;
    bool      landed     = false;
    std::string node;                ///< Where it sits; in transit, its destination.
    ImVec2    pos {0.0f, 0.0f};
    float     r = 0.0f;
    // In transit only: the segment.
    ImVec2    from {0.0f, 0.0f}, to {0.0f, 0.0f};
    float     share = 0.0f;
};

/// One node's debris as laid out this frame.
struct circum_debris_mark
{
    orbital_node node;
    float        level = 0.0f;
    ImVec2       pos {0.0f, 0.0f};
    float        r = 0.0f;
};

/// The share of a space unit's transit travelled: 1 − days_remaining / the
/// path's days (`orbital_path` on the default graph, as the move counts them,
/// at least 1). Clamped to [0, 1]. Pure.
float space_unit_transit_share(const world& w, const space_unit& u);

/// Does the quarter's storm strike @p a — a dark satellite, or a station core
/// in place at a struck node? Pure (the derived storm reads, solar_storm.hpp).
bool space_asset_storm_struck(const world& w, const space_asset& a);

/// Lay out every fleet at, and every transit between, @p nodes.
std::vector<circum_fleet_mark> layout_circum_fleets(const world& w, const std::vector<circum_node_mark>& nodes);

/// The Solar rung's fleets (SOLAR.md § Space fleets): one mark per nation per
/// body at the body's upper-left, and a segment for each move between two
/// bodies. @p bodies maps each drawn body to its screen centre and radius.
std::vector<circum_fleet_mark> layout_solar_fleets(const world& w,
                                                   const std::map<entity_id, std::pair<ImVec2, float>>& bodies);

/// Lay out the debris at @p nodes.
std::vector<circum_debris_mark> layout_circum_debris(const world& w, const std::vector<circum_node_mark>& nodes);

void draw_circum_debris(ImDrawList* dl, const std::vector<circum_debris_mark>& marks);
/// Segments first, then marks; @p hovered and @p selected (unit ids) ring theirs.
void draw_circum_fleets(ImDrawList* dl, const std::vector<circum_fleet_mark>& marks, entity_id hovered,
                        entity_id selected);

/// The fleet mark under @p mouse, or −1. Nearest centre wins.
int hit_circum_fleet(const std::vector<circum_fleet_mark>& marks, ImVec2 mouse);

/// "Frigate", "Hauler" … for a hull value.
const char* space_hull_label(int hull);

/// THE SHARED CONTENT BUILDER for a space unit (SELECTION.md § the shared
/// content builders): hull, owner, state, node, Δv budget, propellant, hull
/// points, cargo, its fleet. The hover card and the Selection band both draw it.
void draw_space_unit_summary(const world& w, entity_id unit);

/// The hover card: the summary through the shared hover-card primitive.
void draw_circum_fleet_card(const world& w, entity_id unit, ImVec2 mouse);

/// The Selection band's card for a selected space unit, with the player's
/// orders (move with its Δv preview, dock, undock, refuel, bombard) queued to
/// `ui.space.pending`. Returns false (and draws nothing) when
/// `ui.selected_entity` is not a space unit.
bool draw_space_unit_selection(const world& w, ui_state& ui);

/// The Military ledger's Design tab, Space theatre: one row per hull, each
/// expanding in place to its slots (a picker per slot) and a Save press that
/// queues `set_unit_design`.
void draw_space_hull_designs(const world& w, ui_state& ui);

} // namespace ui
