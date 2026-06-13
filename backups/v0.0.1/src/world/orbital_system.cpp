#include "orbital_system.hpp"

#include <cmath>

namespace {

constexpr double two_pi = 6.283185307179586;

// Angular velocity of a 1 AU orbit: one revolution per Earth year. All other
// orbits scale from this by Kepler's third law.
constexpr double earth_days_per_year   = 365.25;
constexpr double omega_at_one_au_per_day = two_pi / earth_days_per_year;

} // namespace

void advance_orbits(world& w, double delta_days)
{
    if (delta_days <= 0.0)
        return;

    for (auto& [id, body] : w.bodies)
    {
        if (body.orbital_angular_velocity_rad_per_day == 0.0f)
            continue;

        double angle = body.orbital_angle_rad
                     + body.orbital_angular_velocity_rad_per_day * delta_days;

        // Wrap into [0, 2*pi) to keep the value bounded over long sessions.
        angle = std::fmod(angle, two_pi);
        if (angle < 0.0)
            angle += two_pi;

        body.orbital_angle_rad = static_cast<float>(angle);
    }
}

float kepler_angular_velocity(float orbital_radius_au)
{
    if (orbital_radius_au <= 0.0f)
        return 0.0f;

    // omega = omega_1AU * a^(-3/2).
    const double omega = omega_at_one_au_per_day
                       * std::pow(static_cast<double>(orbital_radius_au), -1.5);
    return static_cast<float>(omega);
}
