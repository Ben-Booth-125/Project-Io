#pragma once

#include "world.hpp"

// ---------------------------------------------------------------------------
// Road-network generation (BL-146 — follow-on from the BL-077 logistics core)
// ---------------------------------------------------------------------------
// Builds each nation's intra-body road lattice, run AFTER nation + population
// generation (the pass needs cities to connect). Deterministic from the campaign
// seed — a pure function of the already-generated tiles/nations/centres, with no
// clock or RNG: every collection is sorted by entity id before use and every
// tie-break is explicit, so the stamped road_level field is reproducible.
//
// Per nation, over its population centres (nodes):
//   1. Weighted graph  — terrain-weighted A* cost (intra_body_path) between each
//      centre pair, one-off at generation.
//   2. Backbone        — Kruskal MST over that graph, tie-broken by
//      (cost, lo-tile-id, hi-tile-id), plus relative-neighbour redundancy edges
//      for realistic loops.
//   3. Three-tier      — tier per edge from the two centres' scales (BL-172):
//      HIGHWAY (road_level 3) between two major centres (scale >= 3, City /
//      Metropolis); ROAD (2) when at least one endpoint is Town+ (scale >= 2);
//      TRACK (1) otherwise.
//   4. Rasterise       — each edge is stamped along its A* path, taking the max
//      road_level on overlap and skipping ocean tiles (roads are a land feature);
//      the path already respects the east-west cylinder wrap.
// Then, across nations: one TRACK border link between the nearest centre pair of
// each territorially-adjacent nation pair, so the continent-wide lattice connects.
//
// road_level lowers a tile's A* traversal cost via road_traversal_multiplier
// (logistics.cpp), so the follow-on dispatch path needs no change — it simply
// finds the roaded corridors cheaper. Tuning (Ben, 2026-07-11): Track=1, Road=2,
// Highway=3; major-centre threshold scale>=3, Town+ threshold scale>=2.
void generate_roads(world& w, entity_id body);
