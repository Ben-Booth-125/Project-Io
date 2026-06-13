#pragma once

#include <cstdint>

/// Fixed-timestep simulation loop with a layered economy tick.
///
/// tick() is called once per rendered frame. Internally it drains an
/// accumulator at the sim step rate. Every N sim steps one economy tick fires.
/// The render layer reads sim_tick() / econ_tick() but never writes to them.
class sim_loop
{
public:
    /// @param sim_hz         Simulation steps per real second. Default: 20.
    /// @param econ_per_second Economy ticks per real second. Default: 1.
    explicit sim_loop(int sim_hz = 20, int econ_per_second = 1);

    /// Advance simulation by wall-clock delta. Call exactly once per rendered frame.
    void tick();

    /// Total simulation steps completed since construction.
    uint64_t sim_tick()  const { return m_sim_tick; }

    /// Total economy ticks completed since construction.
    uint64_t econ_tick() const { return m_econ_tick; }

private:
    void on_sim_step();
    void on_econ_tick();

    double   m_step_ms;           // milliseconds per sim step
    int      m_steps_per_econ;    // sim steps between economy ticks
    int      m_step_accum = 0;    // steps accumulated since last economy tick

    static constexpr int max_catchup_steps = 8;

    double   m_accum_ms  = 0.0;
    uint64_t m_last_ms   = 0;
    uint64_t m_sim_tick  = 0;
    uint64_t m_econ_tick = 0;
};
