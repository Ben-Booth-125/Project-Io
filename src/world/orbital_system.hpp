#pragma once

#include "world.hpp"

/// Advance every body's orbital angle by its angular velocity.
///
/// Called once per frame with the in-game days elapsed since the previous call.
/// When the simulation is paused, `delta_days` is zero and nothing moves.
/// Angles are kept wrapped into [0, 2*pi). Parented bodies (moons) advance their
/// own angle independently of their parent; the parent's motion is composed at
/// draw time (see the solar system canvas), so moons track their planet.
///
/// @param w          World whose body_components are advanced.
/// @param delta_days In-game days elapsed since the last call. Negative or zero
///                   values are ignored.
void advance_orbits(world& w, double delta_days);

/// Plausible orbital angular velocity for a body at a given star distance.
///
/// Derived from Kepler's third law (period proportional to radius^1.5), seeded
/// so that a body at 1 AU completes one orbit in an Earth year. Used to author
/// the prototype planets and asteroids so their relative speeds are correct —
/// inner bodies sweep faster than outer ones. Moons set their own value.
///
/// @param orbital_radius_au Distance from the star in AU. Must be positive.
/// @return Angular velocity in radians per in-game day.
float kepler_angular_velocity(float orbital_radius_au);
