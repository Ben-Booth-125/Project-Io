#pragma once

#include <cstdint>

/// Three-layer fixed-timestep simulation clock.
///
/// Tick layers, finest to coarsest:
///   - **sim tick**  smallest step; `sim_ticks_per_day` of them make one day.
///                   Fine enough to interpolate smooth visual motion between.
///   - **day tick**  fires once per in-game day.
///   - **econ tick** fires every `econ_tick_days` — one quarter (three 30-day
///                   months). Supply/demand and prices will resolve here.
///
/// Real-time pacing is a speed multiplier (1x–5x). At 1x a day takes
/// `seconds_per_day_1x` real seconds; higher speeds divide that. Speed 0 pauses
/// the clock.
///
/// These calendar and pacing values are deliberately tentative — they are the
/// single place to retune game time. The render layer reads the tick counters
/// but never writes them.
class sim_loop
{
public:
    // --- tunable calendar / pacing constants ---
    static constexpr int    sim_ticks_per_day  = 12;  ///< Sim steps per in-game day.
    static constexpr int    days_per_month     = 30;  ///< Days per in-game month.
    static constexpr int    months_per_econ    = 3;   ///< Economy resolves quarterly.
    static constexpr int    econ_tick_days     = days_per_month * months_per_econ; ///< 90 days.
    static constexpr double seconds_per_day_1x = 2.0; ///< Real seconds per day at 1x (retuned 3x quicker, 2026-07-15).
    static constexpr int    max_speed          = 5;

    /// Non-linear time multiplier for speed level s (1–5).
    /// Returns the real multiplier applied to the 1× pace reference.
    static double speed_multiplier(int s);

    sim_loop();

    /// Advance by wall-clock delta. Call exactly once per rendered frame.
    void tick();

    /// Set the speed multiplier. 0 pauses; any positive value is clamped to
    /// [1, max_speed].
    void set_speed(int speed);

    /// Advance the clock by exactly @p days in-game days, wall clock be
    /// damned — the counters step through the same on_sim_step path tick()
    /// uses, so every day/econ boundary fires exactly as if the time had
    /// elapsed. Works while paused (that is the point): BL-412's live agent
    /// seam gates the clock, and a TICK request releases one econ tick
    /// (`advance_days(econ_tick_days)`) without touching the speed dial.
    void advance_days(int days);

    int  speed()  const { return m_speed; }
    bool paused() const { return m_speed == 0; }

    /// Total simulation steps completed since construction.
    uint64_t sim_tick()  const { return m_sim_tick; }

    /// In-game days elapsed since construction.
    uint64_t day_tick()  const { return m_day_tick; }

    /// Economy ticks (quarters) elapsed since construction.
    uint64_t econ_tick() const { return m_econ_tick; }

    /// Continuous in-game days elapsed since construction, including the
    /// fractional part of the current day. Advances smoothly with wall-clock
    /// time (scaled by speed) and freezes while paused — drives visual orbital
    /// motion, which wants finer resolution than the discrete day counter.
    double elapsed_days() const { return m_elapsed_days; }

private:
    void on_sim_step();

    int m_speed = 1; ///< 0 = paused, otherwise 1..max_speed.

    static constexpr int max_catchup_steps = 8;

    double   m_accum_ms  = 0.0;
    uint64_t m_last_ms   = 0;
    uint64_t m_sim_tick  = 0;
    uint64_t m_day_tick  = 0;
    uint64_t m_econ_tick = 0;
    double   m_elapsed_days = 0.0; ///< Continuous in-game days; smooth, advanced in tick().
};
