#include "sim_loop.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

sim_loop::sim_loop()
    : m_last_ms(SDL_GetTicks())
{}

double sim_loop::speed_multiplier(int s)
{
    // Non-linear curve: 0.25× / 0.5× / 1× / 4× / 16×
    static constexpr double table[max_speed + 1] = { 0.0, 0.25, 0.5, 1.0, 4.0, 16.0 };
    if (s <= 0 || s > max_speed) return 0.0;
    return table[s];
}

void sim_loop::set_speed(int speed)
{
    m_speed = (speed <= 0) ? 0 : std::min(speed, max_speed);
}

void sim_loop::tick()
{
    const uint64_t now   = SDL_GetTicks();
    const double   delta = static_cast<double>(now - m_last_ms);
    m_last_ms = now;

    // Paused: consume elapsed wall-clock but advance nothing, and drop any
    // backlog so unpausing does not fast-forward.
    if (m_speed == 0)
    {
        m_accum_ms = 0.0;
        return;
    }

    // Real milliseconds per sim step at the current speed. Derived so that one
    // day (sim_ticks_per_day steps) takes seconds_per_day_1x / multiplier seconds.
    const double mult    = speed_multiplier(m_speed);
    const double step_ms = (seconds_per_day_1x * 1000.0)
                         / (sim_ticks_per_day * mult);

    // Continuous day counter for smooth visual motion. One day passes every
    // seconds_per_day_1x / multiplier real seconds, so days advance by delta scaled
    // accordingly. Decoupled from the discrete sim-step accumulator below.
    m_elapsed_days += (delta * mult) / (seconds_per_day_1x * 1000.0);

    m_accum_ms += delta;

    // Clamp to avoid a spiral of death when a frame hitch lets the accumulator
    // grow faster than the sim can drain it.
    if (m_accum_ms > step_ms * max_catchup_steps)
        m_accum_ms = step_ms * max_catchup_steps;

    while (m_accum_ms >= step_ms)
    {
        on_sim_step();
        m_accum_ms -= step_ms;
    }
}

void sim_loop::advance_days(int days)
{
    if (days <= 0)
        return;
    // Step the discrete counters through on_sim_step so the day/econ boundary
    // logic stays single-sourced; move the continuous day counter in lockstep
    // so orbital motion and the survey clock see the same elapsed time.
    for (int i = 0; i < days * sim_ticks_per_day; ++i)
        on_sim_step();
    m_elapsed_days += static_cast<double>(days);
}

void sim_loop::on_sim_step()
{
    ++m_sim_tick;

    // Day boundary: every sim_ticks_per_day steps.
    if (m_sim_tick % sim_ticks_per_day == 0)
    {
        ++m_day_tick;

        // Economy boundary: every econ_tick_days days.
        // Layer 1+: supply/demand resolution, tick-boundary snapshots, and
        // in-flight convoy progress evaluation hang off this boundary.
        if (m_day_tick % econ_tick_days == 0)
            ++m_econ_tick;
    }
}
