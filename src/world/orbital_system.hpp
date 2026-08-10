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

/// A body's orbital angle as a pure function of the day tick.
///
/// Reconstructs epoch phase + angular velocity × elapsed days, wrapped into
/// [0, 2*pi), reading only authored fields — never the frame-advanced
/// `orbital_angle_rad`, whose value depends on wall-clock frame timing. This is
/// the angle the econ tick must use (convoy sourcing/pricing, supply_system.cpp)
/// so two runs of the same seed agree regardless of frame rate; the render path
/// keeps its smooth wall-clock angles.
///
/// @param body     Body whose position is queried.
/// @param day_tick Current sim day tick (one day per tick).
/// @return Orbital angle in radians, wrapped into [0, 2*pi).
float orbital_angle_at_tick(const body_component& body, int day_tick);
