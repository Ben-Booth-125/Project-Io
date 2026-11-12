#pragma once

#include "entity.hpp"
#include "orbital_node.hpp"

#include <set>

struct world;

// ---------------------------------------------------------------------------
// Comms coverage (BL-1587) — which orbital nodes a nation's orders reach at once
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Signal delay. Coverage is DERIVED, never
// stored: a pure function of the world's assets and stations.
//
//   * the nation's home body's low and high orbit, always;
//   * every node of a body an ACTIVE Comms satellite of the nation orbits;
//   * a WORKING Comms relay module's station node and every node one link away.
//
// This interface is fixed so callers (the order seam, module effects, the UI)
// can be written against it while BL-1587's lane fills it. The first
// implementation covers the home orbits only.
// ---------------------------------------------------------------------------

/// Every node in @p nation's comms coverage, ascending.
std::set<orbital_node> space_coverage(const world& w, entity_id nation);

/// Is @p node in @p nation's comms coverage?
bool space_node_covered(const world& w, entity_id nation, orbital_node node);
