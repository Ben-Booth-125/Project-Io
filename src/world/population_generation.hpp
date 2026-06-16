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
