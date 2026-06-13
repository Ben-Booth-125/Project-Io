#pragma once

#include "world.hpp"

/// Construct and return a world populated with two authored bodies.
///
/// Bodies:
///   Kepler — temperate rocky planet, 4×4 tile grid, iron-ore and silicate
///            deposits, two surface installations.
///   Vesta  — icy outer moon, 3×3 tile grid, ice and rare-metal deposits,
///            one surface installation.
///
/// All values are hand-authored for prototype testing. Replace this function
/// with a data-driven loader when scripted body definitions are added.
///
/// @return A fully populated world ready to drive the simulation.
world make_hard_coded_world();
