#pragma once

#include "world.hpp"

/// Land tiles per population centre — the divisor the primary placement pass
/// derives its target count from (BL-463: land area, not grid area).
///
/// MEASURED, not chosen. Exposed here rather than kept file-local in
/// population_generation.cpp so tools/verify/settlement_density.cpp can print
/// the shipped value beside the distribution it produces, and sweep alternatives
/// against it through the optional parameter below. See the long note at the
/// derivation site for how the figure was arrived at.
inline constexpr int k_land_tiles_per_centre = 410;

/// Generates initial population centres for the given body and attaches them
/// to the world as entities with a `population_centre_component` on the chosen
/// tile entity.
///
/// Called from hard_coded_world.cpp after tile generation (wired in the main
/// session). The result is deterministic: the same seed, tile layout, and body
/// always produce identical centre placement.
///
/// @param w        World to populate; receives new population-centre entities.
/// @param body_id  Body whose tiles are candidates for population centre placement.
/// @param seed     Per-body RNG seed; the same seed always produces the same output.
/// @param land_tiles_per_centre  The density divisor. Defaults to the shipped
///        `k_land_tiles_per_centre`, so every production caller is unchanged;
///        the parameter exists so a harness can measure what a DIFFERENT divisor
///        would produce without a recompile. Values <= 0 are treated as the
///        default. Not a world_params field on purpose: it is a measurement
///        affordance, not a save-format commitment.
void generate_population_centres(world& w, entity_id body_id, unsigned seed,
                                 int land_tiles_per_centre = k_land_tiles_per_centre);

/// Founds one population centre in every nation on @p body_id that holds none
/// (BL-463). Runs AFTER generate_nations — that is the whole point of it being a
/// separate pass: the primary placement above runs before borders exist and can
/// only derive a target from land area, so the NATION term of the target is
/// applied here, once there are nations to count.
///
/// Structural, not a tuned top-up: it founds exactly as many centres as there are
/// uncovered nations, at each nation's best habitable-and-rich unoccupied tile,
/// and skips a nation whose whole territory can host nobody. This is what makes
/// substrate_census's F1 row ("every nation holds at least one population
/// centre") true by construction.
///
/// Deterministic: sorted-id nation order, a pure argmax over a sorted tile list,
/// and one RNG stream seeded by @p seed used only for the founding's scale.
///
/// @returns the number of centres founded.
int ensure_national_population_centres(world& w, entity_id body_id, unsigned seed);
