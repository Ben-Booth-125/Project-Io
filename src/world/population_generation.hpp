#pragma once

#include "world.hpp"

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
void generate_population_centres(world& w, entity_id body_id, unsigned seed);

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
