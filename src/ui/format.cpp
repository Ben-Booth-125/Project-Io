#include "format.hpp"

#include "core/sim_loop.hpp"

#include <array>
#include <cmath>
#include <cstdio>

namespace ui::fmt {

namespace {

// Calendar constants. The first three mirror sim_loop's tentative calendar so
// there is one source of truth for month/quarter lengths; quarters_per_year
// completes the calendar (sim_loop defines no year) and is documented as the
// natural 4-quarter year.
constexpr int days_per_month     = sim_loop::days_per_month;   // 30
constexpr int months_per_quarter = sim_loop::months_per_econ;  // 3
constexpr int quarters_per_year  = 4;
constexpr int months_per_year    = months_per_quarter * quarters_per_year; // 12
constexpr int days_per_year      = days_per_month * months_per_year;       // 360

// Magnitude abbreviation thresholds, ascending. Each suffix applies once the
// magnitude reaches its divisor.
struct magnitude_suffix
{
    double      divisor;
    const char* suffix;
};
constexpr std::array<magnitude_suffix, 4> magnitude_suffixes = {{
    { 1e12, "T" },
    { 1e9,  "B" },
    { 1e6,  "M" },
    { 1e3,  "k" },
}};

} // namespace

sign sign_of(double value, double epsilon)
{
    if (value > epsilon)  return sign::positive;
    if (value < -epsilon) return sign::negative;
    return sign::zero;
}

std::string abbreviate(double value)
{
    char buf[32];

    const double magnitude = std::fabs(value);
    for (const magnitude_suffix& m : magnitude_suffixes)
    {
        if (magnitude >= m.divisor)
        {
            std::snprintf(buf, sizeof(buf), "%.1f%s", value / m.divisor, m.suffix);
            return buf;
        }
    }

    // Below 1000: print as a whole number — fractional units are noise at this
    // scale and the abbreviated forms above already carry the only decimal.
    std::snprintf(buf, sizeof(buf), "%.0f", value);
    return buf;
}

std::string credits(double amount)
{
    return "Cr " + abbreviate(amount);
}

std::string signed_delta(double value)
{
    switch (sign_of(value))
    {
        case sign::zero:     return "±0"; // ±0 — matches the header placeholder style
        case sign::positive: return "+" + abbreviate(value);
        case sign::negative: return abbreviate(value); // abbreviate keeps the '-'
    }
    return abbreviate(value);
}

std::string rate(double value, const char* per_unit)
{
    return signed_delta(value) + " / " + (per_unit ? per_unit : "");
}

std::string percent(double fraction, int decimals)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f%%", decimals, fraction * 100.0);
    return buf;
}

calendar_date date_from_day(uint64_t day_tick)
{
    const uint64_t day_of_year = day_tick % static_cast<uint64_t>(days_per_year);
    const int      month_index = static_cast<int>(day_of_year) / days_per_month; // 0-11

    calendar_date d;
    d.year    = static_cast<int>(day_tick / static_cast<uint64_t>(days_per_year)) + 1;
    d.month   = month_index + 1;
    d.day     = static_cast<int>(day_of_year % static_cast<uint64_t>(days_per_month)) + 1;
    d.quarter = month_index / months_per_quarter + 1;
    return d;
}

std::string short_date(uint64_t day_tick)
{
    const calendar_date d = date_from_day(day_tick);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Y%d M%02d D%02d", d.year, d.month, d.day);
    return buf;
}

} // namespace ui::fmt
