#include "sim_loop.hpp"
#include <SDL3/SDL.h>

sim_loop::sim_loop(int sim_hz, int econ_per_second)
    : m_step_ms(1000.0 / sim_hz)
    , m_steps_per_econ(sim_hz / econ_per_second)
    , m_last_ms(SDL_GetTicks())
{}

void sim_loop::tick()
{
    const uint64_t now   = SDL_GetTicks();
    const double   delta = static_cast<double>(now - m_last_ms);
    m_last_ms = now;

    m_accum_ms += delta;

    // Clamp to avoid spiral-of-death when a frame hitch causes the accumulator
    // to grow faster than the sim can drain it.
    if (m_accum_ms > m_step_ms * max_catchup_steps)
        m_accum_ms = m_step_ms * max_catchup_steps;

    while (m_accum_ms >= m_step_ms)
    {
        on_sim_step();
        m_accum_ms -= m_step_ms;
    }
}

void sim_loop::on_sim_step()
{
    ++m_sim_tick;
    ++m_step_accum;
    if (m_step_accum >= m_steps_per_econ)
    {
        m_step_accum = 0;
        on_econ_tick();
    }
}

void sim_loop::on_econ_tick()
{
    ++m_econ_tick;
    // Layer 1+: supply/demand resolution, tick-boundary snapshots, and
    // in-flight convoy progress evaluation are added here.
}
