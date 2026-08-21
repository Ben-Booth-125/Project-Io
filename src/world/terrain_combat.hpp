#pragma once

#include "components.hpp"

// ---------------------------------------------------------------------------
// Terrain combat value (BL-233).
//
// How ground behaves under military pressure, as pure functions of a tile's
// character — the same shape as `landform_logistics_cost` in logistics.hpp, and
// for the same reasons: no stored field, no component change, nothing on the
// serialisation seam, and deterministic by construction.
//
// SCOPE. This adds NO combat resolution, units, unit stats, fortifications or
// AI — all of those remain excluded by TECH_FOUNDATIONS § Prototype scope. What
// it provides is the terrain property SYSTEMS.md § Environment already assigns
// ("environment sets the local cost of construction, the difficulty of military
// operations, and extraction yields"), at a usable resolution. Today the sole
// consumer is generation-time: the history ladder's Stage 2, which prices
// conquest. A later combat layer reads these same two functions rather than
// inventing a third answer to the same question.
//
// WHY TWO FUNCTIONS AND NOT ONE. The predecessor (`is_barrier`, a bool in
// history_ladder.cpp) conflated three mechanically different things:
//
//   1. Defensive ground — mountain, canyon, rift. Hard to attack ACROSS;
//      multiplies whoever holds it.
//   2. Attritional ground — barren, icy, regolith. Perfectly crossable, but
//      starves an army: no forage, no water, no shelter. It costs the ATTACKER
//      without helping any defender.
//   3. Open water — a movement MODE, not a magnitude, and the thing that makes
//      EXIT cheap rather than conquest dear.
//
// Merging them is why a mountain and a barren plain scored identically while a
// forest scored nothing. (1) and (2) are the two functions below; (3) stays an
// explicit `substrate == ocean` branch at the call site, exactly as logistics
// already branches land/sea, because a mode is not a number.
// ---------------------------------------------------------------------------

/// How well this ground holds against an attacker, 0..1000 (0 = open ground).
///
/// Landform dominates — the shape of the ground is what a defender uses — with
/// cover and substrate modulating for going. Ocean returns 0: water is a mode
/// change, and the caller must branch on it rather than read a defence value.
int terrain_defence(terrain_substrate sub, terrain_cover cov, std::uint8_t density,
                    terrain_landform lf);

/// What crossing this ground costs an army in supply, 0..1000, independently of
/// any defender. The substrate dominates here (what the ground itself denies an
/// army), the cover relieves it (forage, fuel, shelter), and landform adds the
/// climb. Ocean returns 0 for the same reason as above.
int terrain_attrition(terrain_substrate sub, terrain_cover cov, std::uint8_t density,
                      terrain_landform lf);

/// The combined "this tile resists an army" quantity, 0..1000 — the graded
/// replacement for the old `is_barrier` bool. Weighted toward defence, since a
/// defensible frontier shapes a border more strongly than a merely expensive
/// one, but attrition is what stops an army crossing an ice cap that no one is
/// defending at all. Ocean returns 0; callers count water separately.
int terrain_resistance(terrain_substrate sub, terrain_cover cov, std::uint8_t density,
                       terrain_landform lf);
