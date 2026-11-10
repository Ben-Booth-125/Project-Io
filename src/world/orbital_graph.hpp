#pragma once

#include "orbital_node.hpp"
#include "world.hpp"

#include <vector>

// ---------------------------------------------------------------------------
// The orbital graph (BL-1580) — docs/economy/SPACE_ASSETS.md § The orbital
// graph and § Orbital slots.
// ---------------------------------------------------------------------------
// Space is a graph of named nodes (orbital_node.hpp). This file derives the
// graph from the bodies: which nodes exist, where each sits, which links join
// them and what each link costs in thrust (abstract Δv units) and days.
//
// PURE AND NEVER SAVED. Everything here is a function of `world::bodies` and
// the params below; nothing is cached on the world, so there is nothing to
// serialise. Every container is ordered (bodies are walked in ascending id),
// and the path search breaks ties on node order, so two calls on the same
// world are bit-identical.
//
// THE PAIRS. Every non-star body B is the SMALLER member of exactly one pair:
// (B's parent, B) when B has a parent body, else (star, B). The pair's four
// Lagrange points are keyed on B. A parent body is the LARGER member of every
// pair whose smaller member orbits it. The star carries no node, so a
// star–planet pair's L-points are reached only from the planet's side (and
// from other star pairs, the interplanetary link).
//
// COSTS ARE DERIVED, NEVER AUTHORED PER LINK. A body's CLIMB (surface ↔ low
// orbit) is `climb_mass_coeff × mass_earths^climb_mass_exponent` plus an
// atmosphere term by `body_component::atmosphere`; every other link is a
// fraction of a climb, save the interplanetary link, which grows with the
// difference in orbital radius. Travel days are `days_per_dv × cost` per link
// kind, so a longer link adds more days.
// ---------------------------------------------------------------------------

/// The dials. Defaults are a first in-band calibration, chosen so the home
/// world (1 Earth mass, moderate air) climbs ~10 Δv and the ratios echo real
/// Δv budgets (a Luna-mass airless moon climbs ~1.7).
struct orbital_graph_params
{
    // --- Surface ↔ low orbit: the climb. ---
    double climb_mass_coeff     = 8.0;  ///< Δv for one Earth mass before the atmosphere term.
    double climb_mass_exponent  = 0.35; ///< Orbital speed ~ M^0.35 for rocky bodies (R ~ M^0.27).
    double atmosphere_none_dv   = 0.0;  ///< Added to the climb by atmosphere class.
    double atmosphere_thin_dv   = 0.5;
    double atmosphere_moderate_dv = 2.0;
    double atmosphere_thick_dv  = 4.0;

    // --- Low ↔ high orbit. ---
    double low_high_fraction    = 0.4;  ///< Of the body's own climb.

    // --- High orbit ↔ a pair's L-points. ---
    double high_lagrange_fraction = 0.25; ///< Of the climb of the body whose high orbit it is.

    // --- L-point ↔ L-point within one pair. ---
    double lagrange_hop_fraction  = 0.1;  ///< Of the climb of the pair's largest NODE-BEARING member.

    // --- Star-pair L1/L2 ↔ another star-pair L1/L2 (interplanetary). ---
    double interplanetary_base_dv   = 1.0; ///< Fixed part of any transfer.
    double interplanetary_dv_per_au = 4.0; ///< Grows with |Δ orbital radius|.

    // --- L1/L2 placement: a fixed fraction of the pair separation. ---
    double lagrange_l12_offset_fraction = 0.15; ///< L1 inside, L2 beyond the smaller body.

    // --- Travel days per Δv, one dial per link kind. ---
    double climb_days_per_dv          = 0.1;
    double low_high_days_per_dv       = 0.25;
    double high_lagrange_days_per_dv  = 1.0;
    double lagrange_hop_days_per_dv   = 2.0;
    double interplanetary_days_per_dv = 25.0;
};

enum class orbital_link_kind : uint8_t
{
    climb             = 0, ///< surface ↔ low orbit
    low_high          = 1, ///< low orbit ↔ high orbit
    high_lagrange     = 2, ///< a high orbit ↔ an L-point of a pair the body belongs to
    lagrange_hop      = 3, ///< L-point ↔ L-point within one pair
    interplanetary    = 4, ///< star-pair L1/L2 ↔ another star-pair L1/L2
};

/// One undirected link. Stored with `a < b`.
struct orbital_link
{
    orbital_node      a;
    orbital_node      b;
    orbital_link_kind kind = orbital_link_kind::climb;
    double            dv   = 0.0; ///< Thrust cost, abstract Δv units.
    double            days = 0.0; ///< Travel time, in-game days.
};

/// A node's system position in AU, x right, y UP (math convention; the
/// canvases negate y for the screen). The star sits at the origin.
struct orbital_position
{
    double x_au = 0.0;
    double y_au = 0.0;
};

struct orbital_path_result
{
    bool                      ok   = false; ///< False: an invalid end, or no route.
    double                    dv   = 0.0;   ///< Sum of the links' Δv.
    double                    days = 0.0;   ///< Sum of the links' days.
    std::vector<orbital_node> nodes;        ///< from … to inclusive; empty on failure.
};

/// Does @p n name a node of @p w? Its body exists and is not the star, and its
/// kind is in range.
bool orbital_node_valid(const world& w, orbital_node n);

/// Every node of @p w, ascending (body, kind): seven per non-star body.
std::vector<orbital_node> orbital_nodes(const world& w);

/// Every link of @p w, ascending (a, b), each with a < b.
std::vector<orbital_link> orbital_links(const world& w, const orbital_graph_params& p = {});

/// The climb Δv off @p body's surface to its low orbit. 0 for a star or a
/// missing body.
double orbital_climb_dv(const world& w, const orbital_graph_params& p, entity_id body);

/// @p n's position in AU from the live `orbital_angle_rad`s, composed as the
/// canvases compose moons: the parent's position plus the body's own orbit.
/// L1/L2 sit on the parent→body line at ∓ `lagrange_l12_offset_fraction` of
/// the separation; L4/L5 sit at ±60° on the body's orbit, L4 ahead in the
/// direction of motion. An invalid node returns the origin.
orbital_position orbital_node_position(const world& w, orbital_node n,
                                       const orbital_graph_params& p = {});

/// The cheapest path from @p from to @p to by Δv (Dijkstra; ties broken on
/// node order, so the result is deterministic). from == to is a valid empty
/// trip: ok, zero cost, one node.
orbital_path_result orbital_path(const world& w, const orbital_graph_params& p,
                                 orbital_node from, orbital_node to);

/// The node's slot count per SPACE_ASSETS.md § Orbital slots. Surface and an
/// invalid node hold 0. A station body's high orbit holds the small-body 2.
int orbital_slot_capacity(const world& w, orbital_node n);
