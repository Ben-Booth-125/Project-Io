#pragma once

#include "entity.hpp"

#include <cstdint>

// ---------------------------------------------------------------------------
// Orbital node identity (BL-1580) — the one shared name for "where in space"
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § The orbital graph. Space is a graph of named
// nodes; an asset sits AT a node. This header is only the node's NAME, shared
// by the graph (orbital_graph.*), the slot store, stations and coverage, so
// those can be written apart. The graph itself — links, thrust cost, live
// positions — is orbital_graph.hpp.
//
// THE KEY. A node is (body, kind):
//   surface, low_orbit, high_orbit   the body's own three nodes.
//   l1, l2, l4, l5                   the Lagrange points of the pair whose
//                                    SMALLER member is `body` — the pair
//                                    (body's parent, body), or (star, body)
//                                    for a body that orbits the star directly.
// Each body has exactly one parent, so keying an L-point on the smaller body
// names every pair once and needs no second id. The star carries no node.
//
// SAVED: both fields are written wherever a node is stored. Append-only enum.
// ---------------------------------------------------------------------------

enum class orbital_node_kind : uint8_t
{
    surface    = 0,
    low_orbit  = 1,
    high_orbit = 2,
    l1         = 3,
    l2         = 4,
    l4         = 5,
    l5         = 6,
};
inline constexpr int orbital_node_kind_count = 7;

struct orbital_node
{
    entity_id         body = null_entity;
    orbital_node_kind kind = orbital_node_kind::low_orbit;

    friend bool operator==(const orbital_node& a, const orbital_node& b)
    {
        return a.body == b.body && a.kind == b.kind;
    }
    friend bool operator!=(const orbital_node& a, const orbital_node& b) { return !(a == b); }
    /// Total order (body, then kind) so nodes key std::map deterministically.
    friend bool operator<(const orbital_node& a, const orbital_node& b)
    {
        return a.body != b.body ? a.body < b.body
                                : static_cast<uint8_t>(a.kind) < static_cast<uint8_t>(b.kind);
    }
};

/// Is @p k a Lagrange point?
inline bool orbital_node_is_lagrange(orbital_node_kind k)
{
    return k == orbital_node_kind::l1 || k == orbital_node_kind::l2 ||
           k == orbital_node_kind::l4 || k == orbital_node_kind::l5;
}

/// May an orbital asset occupy @p k? Every node but the surface.
inline bool orbital_node_is_orbital(orbital_node_kind k)
{
    return k != orbital_node_kind::surface;
}
