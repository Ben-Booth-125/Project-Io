#pragma once

#include "tile_generation.hpp"
#include "world.hpp"

// ---------------------------------------------------------------------------
// River generation (BL-170 — surface hydrology, downhill trace)
// ---------------------------------------------------------------------------
// A river is an EDGE feature: it borders exactly two tiles, never occupies one
// (no new landform/composition value). Each traversed edge sets one bit — on
// BOTH tiles that share it — in tile_component.river_edges (components.hpp),
// one bit per hex-neighbour direction. A river mouth (the last land tile before
// the sea) legitimately sets a bit on the bordering ocean tile too — a river
// borders whatever it borders, land or sea.
//
// Run AFTER generate_body_tiles, as a separate pass (mirrors generate_roads,
// road_generation.cpp), over the SAME heightmap the ocean-banding pass (Pass 2,
// tile_generation.cpp) reads: generate_body_tiles must be called with a non-null
// generation_record so its captured Pass-1 height field survives to feed this
// pass. Pure function of that height field and the generated tile compositions
// — every tie-break is explicit (lowest hex-direction index, then lowest tile
// id), so no RNG is needed and the stamped river_edges bits are reproducible.
//
// Algorithm: pick high-ground land tiles as sources (height above a threshold,
// spaced apart so rivers do not all originate from one ridge), then trace
// steepest-descent over the 6 hex neighbours (river_generation.cpp's own
// cube-direction hex_neighbours — reciprocal by construction, unlike
// tile_generation.cpp's odd-r offset table, which the bit encoding needs and
// the cluster-growth BFS there does not) until reaching an ocean tile, a local
// basin (no lower neighbour), or an already-traced tile (rivers may merge,
// never loop). Direction (upstream/downstream) is NOT stored — logistics/
// consumers derive it by comparing the two edge tiles' height (from the same
// record) at cost-computation time; see docs/generation/TILE_GENERATION.md
// § Rivers.
void generate_rivers(world& w, entity_id body, const generation_record& record);
