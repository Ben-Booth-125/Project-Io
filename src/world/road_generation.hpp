#pragma once

#include "era_timelapse.hpp" // history_corridor / history_road_node (BL-768)
#include "world.hpp"

#include <vector>

// The loading screen's progress sink (BL-1072), passed by pointer only.
struct generation_progress;

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
//   0. Local streets  — every centre's own tile gets at least a TRACK (Sprint B2). A nation
//      with a single centre owns no inter-city edge, and used to end generation with no road
//      at all; a settlement has streets regardless, and this is what puts such a nation on
//      the lattice for the border links and for the player's place_road to extend.
//   1. Weighted graph  — terrain-weighted A* cost (intra_body_path) between each
//      TOWN-AND-UP pair (scale >= 2; BL-620 — at demography density all-centre
//      pairs were the generation cost wall), one-off at generation.
//   2. Backbone        — Kruskal MST over that graph, tie-broken by
//      (cost, lo-tile-id, hi-tile-id), plus relative-neighbour redundancy edges
//      for realistic loops.
//   3. Three-tier      — tier per edge from the two towns' scales (BL-172):
//      HIGHWAY (road_level 3) between two major centres (scale >= 3, City /
//      Metropolis); ROAD (2) when at least one endpoint is Town+ (scale >= 2);
//      TRACK (1) otherwise.
//   4. Rasterise       — each edge is stamped along its A* path, taking the max
//      road_level on overlap and skipping ocean tiles (roads are a land feature);
//      the path already respects the east-west cylinder wrap. An edge whose route crosses
//      OPEN ocean (a water run longer than a strait) is not stamped at all — that is a sea
//      route, and stamping it would scatter road fragments on distant shores.
//   5. Village spurs   — each village (scale 1) lays one TRACK to its nearest
//      already-roaded same-nation tile, chosen from a grid-distance-prefiltered
//      candidate set (BL-620): spur tracks, not lattice membership.
// Then, across nations: one TRACK border link between the nearest centre pair of
// each territorially-adjacent nation pair, so the continent-wide lattice connects.
// Territorial adjacency tolerates a short unowned gap (a strait or an unclaimed margin),
// so a coastal or island nation is reachable rather than silently left off the lattice.
//
// road_level lowers a tile's A* traversal cost via road_traversal_multiplier
// (logistics.cpp), so the follow-on dispatch path needs no change — it simply
// finds the roaded corridors cheaper. Tuning (Ben, 2026-07-11): Track=1, Road=2,
// Highway=3; major-centre threshold scale>=3, Town+ threshold scale>=2.
//
// @param progress  Optional loading-screen sink (BL-1072): the village spur walk,
//                  ~90% of this pass, is reported through `report_sub`. Write-only;
//                  null (every caller but generation) publishes nothing.
void generate_roads(world& w, entity_id body, generation_progress* progress = nullptr);

// ---------------------------------------------------------------------------
// Ancient roads, STAMPED FROM the history (BL-768)
// ---------------------------------------------------------------------------
//
// Ben, the 2026-09-03 eight-phase reorder, point 4: *"We should also be laying
// simple roads to supply provinces."* This is that, and the emphasis is on
// FROM: the roads are not laid inside the Era -1 sim — a feasibility pass came
// back blocked on three independent structural grounds (era_timelapse.hpp
// § The ancient road record) — they are stamped afterwards from what the sim
// RECORDED walking. Each corridor is a campaign's staging-to-objective supply
// line or a founding party's parent-to-daughter route, so the network's shape
// is the history's own trunk routes rather than a plausible-looking lattice.
//
// AN ERA-APPROPRIATE TIER RULE, WHICH THIS ITEM OWED. `generate_roads`' gates
// read a nation's qualification PERCENTILE (LOGISTICS.md § Roads), which is a
// 1960-era field derived from industrialisation timing; an ancient road cannot
// borrow it, and an antiquity world has no spread in it to read anyway. The
// ancient rule keys on the two things the history does produce:
//
//   TRAFFIC       — how many times the corridor was actually used. A line an
//                   empire supplied four campaigns and a dozen foundings along
//                   is a Road; one walked once is a Track.
//   WORKS         — whether BOTH ends raised something reach-bearing. A work
//                   promotes the corridor one rung, and it is the ONLY route to
//                   a Highway before the industrial era, so an ancient trunk
//                   highway means traffic AND the stations to carry it.
//
// That keeps LOGISTICS.md's antiquity shape intact — "Roads on every Town+
// backbone, Highways nowhere" for a world that built nothing — while giving the
// works roster a payoff that persists onto the campaign map, which is what
// BL-757's zero-works finding left it without.
//
// PURELY ADDITIVE, and deliberately run AFTER `generate_roads`. Stamping takes
// the max on overlap, so no modern road is ever downgraded and the ancient
// network appears exactly where the nation lattice did not already reach or
// reached lower. Running it BEFORE was the other option and is rejected here:
// the modern pass decides its MST and its tiers on terrain-weighted A* costs,
// so pre-stamped ancient roads would silently re-route the whole national
// lattice — a far larger change than this item's aim, and one that would move
// every road-shaped measurement at once for a reason unrelated to the history.
//
// Deterministic: `corridors` arrives sorted by (a, b), the tier rule is pure
// integer arithmetic over the corridor's own fields, and the stamp takes the max
// per tile — so neither the walk order nor the overlap order can vary the field.
// Roads are a land feature here exactly as in `generate_roads`: water tiles are
// skipped and a corridor whose route crosses open ocean is not stamped at all.
//
// @param nodes      Indexed by region — where each region stood, and its
//                   accumulated works reach. A corridor naming an index past
//                   the end of this array is skipped.
// @param corridors  `history_sim_state::supply_corridors`. Empty (a world with
//                   no Era -1 pass) makes the whole call a no-op.
void stamp_history_roads(world& w, entity_id body,
                         const std::vector<history_road_node>& nodes,
                         const std::vector<history_corridor>&  corridors,
                         generation_progress* progress = nullptr); // BL-1072: per corridor
