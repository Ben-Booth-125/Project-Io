#pragma once

#include "world/orbital_graph.hpp"
#include "world/world.hpp"

#include <imgui.h>

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// The Circumplanetary rung's orbital nodes (BL-1590) — docs/ui/CIRCUMPLANETARY.md
// § Orbital nodes and what sits at them.
// ---------------------------------------------------------------------------
// The anchor's part of the orbital graph drawn on the rung: a low and a high
// orbit ring around the anchor and around each of its moons, a diamond at each
// L1/L2/L4/L5 of every anchor–moon pair, and a row of slot pips beside every
// node. Immediate-mode: the marks are laid out from the live world each frame
// and never kept.
//
// THE FRAME. Moons on this rung sit on a visible-offset scale, not true scale
// (CIRCUMPLANETARY.md § Visual elements). An L-point's graph position is mapped
// into the same frame the moon is drawn in: (node_au − anchor_au) × scale, y
// negated. That mapping is linear, so an L-point lands exactly where the moon's
// own drawing implies (L4/L5 at ±60° on its drawn orbit, L1/L2 on the
// anchor–moon line), and moves with the moon.
//
// Rings are NOT positions in the graph — a low and a high orbit sit at the
// body's own position — so their radii are chosen to read, inside the spacing
// between moon orbits, not measured.
// ---------------------------------------------------------------------------

namespace ui {

/// The rung's view transform, as the canvas computes it.
struct circum_frame
{
    ImVec2 view_origin {0.0f, 0.0f}; ///< Screen position of the anchor.
    float  zoom  = 1.0f;             ///< The rung's live zoom.
    float  scale = 1.0f;             ///< Pixels per AU before zoom (the visible-offset scale).
};

/// One orbital node as laid out this frame.
struct circum_node_mark
{
    orbital_node node;
    ImVec2       pos {0.0f, 0.0f}; ///< Diamond centre, or the ring's centre (its body).
    float        ring_r = 0.0f;    ///< Ring radius in px; 0 for a diamond.
    ImVec2       pip_start {0.0f, 0.0f}; ///< Centre of the first slot pip.
    float        pip_dir = 1.0f;   ///< +1 the row runs right, −1 left.
    int          capacity = 0;     ///< orbital_slot_capacity.
    int          held     = 0;     ///< orbital_slots_held.
};

// Slots of a node held by an asset: the world's `orbital_slots_held`
// (space_assets.hpp, BL-1581), which every pip and the hover card's free count
// read. (The UI stub of the same name is retired: ADL found both.)

/// The node's on-screen name: "Vother L4", "Rivekhaik high orbit".
std::string orbital_node_display_name(const world& w, orbital_node n);

/// Lay out every node of the anchor and its moons. @p body_radius_px gives
/// each body's drawn radius (anchor first, then @p moons in order), so the
/// rings clear the disc.
std::vector<circum_node_mark> layout_circum_nodes(const world& w, entity_id anchor,
                                                  const std::vector<entity_id>& moons,
                                                  const std::vector<float>& body_radius_px,
                                                  const circum_frame& f);

/// The low and high orbit rings. Drawn under the bodies.
void draw_circum_node_rings(ImDrawList* dl, const std::vector<circum_node_mark>& marks);

/// The L-point diamonds and every node's slot pips. Drawn over the rings and
/// the bodies, under the labels. Constant pixel size at any zoom.
void draw_circum_node_marks(ImDrawList* dl, const std::vector<circum_node_mark>& marks);

/// The node under @p mouse, or −1. A diamond or a pip row wins over a ring;
/// among equals the nearest wins, then the lower index.
int hit_circum_node(const std::vector<circum_node_mark>& marks, ImVec2 mouse);

/// The shared hover card for a node: name, free slots, and the thrust cost and
/// days from the player's home surface by `orbital_path`.
void draw_circum_node_card(const world& w, const circum_node_mark& m, ImVec2 mouse);

} // namespace ui
