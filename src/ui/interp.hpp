#pragma once

#include <imgui.h>

namespace ui {

/// Linear interpolation of a scalar. t is not clamped — callers pass a fraction
/// in [0, 1].
///
/// @param a Start value (t = 0).
/// @param b End value (t = 1).
/// @param t Blend fraction.
/// @return  a + (b - a) * t.
inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/// Linear interpolation of a 2-D point, component-wise.
///
/// @param a Start point (t = 0).
/// @param b End point (t = 1).
/// @param t Blend fraction.
/// @return  The blended point.
inline ImVec2 lerp(ImVec2 a, ImVec2 b, float t)
{
    return { lerp(a.x, b.x, t), lerp(a.y, b.y, t) };
}

/// The decided render-side read path for fractional-progress entities.
///
/// TECH_FOUNDATIONS specifies the render loop shows visual state interpolated
/// between simulation states. Orbits do not need this — they read the
/// continuous elapsed-days clock directly. But an entity whose progress only
/// advances at a discrete tick boundary (Layer 5 convoys: a fractional progress
/// along a route that the economy tick resolves) would sit still and then jump
/// at each boundary. To glide instead, the owner keeps the value from the start
/// of the current step (`previous`) and the latest resolved value (`current`),
/// and the canvas reads `at(alpha)` where alpha is the fraction of the way
/// through the current step (see econ_tick_alpha / day_alpha).
///
/// Usage sketch (Layer 5):
///   interpolated<float> progress;            // owned by the convoy
///   progress.advance(new_progress);          // at each economy tick
///   const float p   = progress.at(econ_tick_alpha(sim.elapsed_days()));
///   const ImVec2 at = lerp(route_start, route_end, p);  // smooth screen pos
template <typename T>
struct interpolated
{
    T previous{}; ///< Value at the start of the current step.
    T current{};  ///< Latest resolved simulation value (end of the current step).

    /// Record a freshly resolved value, demoting the old current to previous.
    /// Call once per simulation step (e.g. per economy tick for convoys).
    ///
    /// @param next The newly resolved value.
    void advance(const T& next)
    {
        previous = current;
        current  = next;
    }

    /// The visually blended value at fraction @p t through the current step.
    ///
    /// @param t Fraction in [0, 1] through the step (see econ_tick_alpha).
    /// @return  lerp(previous, current, t).
    T at(float t) const
    {
        return lerp(previous, current, t);
    }
};

/// Fraction in [0, 1) through the current economy tick, from the continuous
/// elapsed-days clock (sim_loop::elapsed_days). Blend a convoy's previous/
/// current progress by this alpha so it moves smoothly across the quarter
/// rather than jumping when the economy resolves.
///
/// @param elapsed_days Continuous in-game days since campaign start.
/// @return             Fractional position within the current economy tick.
float econ_tick_alpha(double elapsed_days);

/// Fraction in [0, 1) through the current in-game day, from the continuous
/// elapsed-days clock. For entities that resolve per day rather than per
/// economy tick.
///
/// @param elapsed_days Continuous in-game days since campaign start.
/// @return             Fractional position within the current day.
float day_alpha(double elapsed_days);

} // namespace ui
