#include "interp.hpp"

#include "core/sim_loop.hpp"

#include <cmath>

namespace ui {

float econ_tick_alpha(double elapsed_days)
{
    constexpr double quarter_days = static_cast<double>(sim_loop::econ_tick_days);
    if (quarter_days <= 0.0)
        return 0.0f;
    return static_cast<float>(std::fmod(elapsed_days, quarter_days) / quarter_days);
}

float day_alpha(double elapsed_days)
{
    return static_cast<float>(elapsed_days - std::floor(elapsed_days));
}

} // namespace ui
