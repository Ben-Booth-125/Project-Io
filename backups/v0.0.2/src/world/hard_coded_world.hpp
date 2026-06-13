#pragma once

#include "world.hpp"

/// Construct and return a world populated with the prototype's authored bodies.
///
/// Bodies (all orbiting the star Helios):
///   Cinder — hot inner planet, 180×84 procedural tile grid.
///   Kepler — temperate rocky planet, 180×84 procedural tile grid; the
///            corporation's home body, with two surface installations and a
///            market.
///   Selene — Kepler's moon, 90×42 procedural tile grid.
///
/// Grids follow a ~9:5 width:height ratio (see PLANETARY.md). All values are
/// hand-authored for prototype testing. Replace this function with a
/// data-driven loader when scripted body definitions are added.
///
/// @return A fully populated world ready to drive the simulation.
world make_hard_coded_world();
