#pragma once

#include <cstdint>
#include <string>

namespace ui::fmt {

/// Sign classification for a signed value. Lets a caller colour a delta from the
/// semantic palette (see ui/presentation.hpp, palette::value_colour) without
/// re-deriving its direction, and keeps the colour decision out of the string
/// formatter.
enum class sign
{
    negative,
    zero,
    positive,
};

/// Classify a value's sign. Values within @p epsilon of zero read as zero so a
/// rounding residue does not render as a spurious gain or loss.
///
/// @param value   The signed value to classify.
/// @param epsilon Half-width of the dead-band treated as zero.
/// @return        negative, zero, or positive.
sign sign_of(double value, double epsilon = 1e-6);

/// Abbreviate a magnitude to a compact human form: 1234 -> "1.2k",
/// 3'400'000 -> "3.4M", 5.0e9 -> "5.0B". Values below 1000 print whole. One
/// decimal place is shown above the threshold; a negative value keeps its sign.
///
/// @param value The number to abbreviate.
/// @return      Compact string form.
std::string abbreviate(double value);

/// Format a credit amount with the "Cr" prefix and magnitude abbreviation, e.g.
/// "Cr 1.2k". "Cr" stands in for the credit glyph the default font cannot render
/// (see DEVLOG, header currency placeholder).
///
/// @param amount Credit quantity.
/// @return       Prefixed, abbreviated credit string.
std::string credits(double amount);

/// Format a signed delta with an explicit leading sign and abbreviation:
/// +1.2k / -340 / "±0" at zero. Pair with palette::value_colour(sign_of(value))
/// for the colour-coded income-vs-expense / price-move treatment.
///
/// @param value The signed change.
/// @return      Signed, abbreviated string.
std::string signed_delta(double value);

/// Format a per-period rate as a signed delta with a unit suffix, e.g.
/// "+1.2k / qtr" or "-50 / day". The unit label is caller-supplied so one helper
/// serves day, quarter, and tick rates.
///
/// @param value    The signed amount per period.
/// @param per_unit Period label appended after " / " (e.g. "qtr", "day").
/// @return         Signed rate string.
std::string rate(double value, const char* per_unit);

/// Format a 0..1 fraction as a percentage string ("0.5" -> "50%").
///
/// @param fraction Fractional value (1.0 == 100%).
/// @param decimals Decimal places to show; defaults to whole percent.
/// @return         Percentage string with a trailing '%'.
std::string percent(double fraction, int decimals = 0);

/// A point on the in-game calendar, decomposed from an absolute day count.
///
/// The calendar is the natural completion of sim_loop's tentative constants:
/// 30-day months, 3-month quarters, and (defined here) 4 quarters to a year —
/// a 360-day year. The campaign clock starts at year 1, month 1, day 1.
struct calendar_date
{
    int year;    ///< 1-based campaign year.
    int month;   ///< 1-12.
    int day;     ///< 1-30.
    int quarter; ///< 1-4 (the in-year quarter, derived from the month).
};

/// Decompose an absolute in-game day count (sim_loop::day_tick) into a calendar
/// date. Day 0 is Y1 M1 D1 Q1.
///
/// @param day_tick In-game days elapsed since the campaign start.
/// @return         The corresponding calendar date.
calendar_date date_from_day(uint64_t day_tick);

/// Format a day count as a compact calendar string for the tick readout, e.g.
/// "Y1 M05 D12". The in-year quarter is available separately via
/// date_from_day(...).quarter.
///
/// @param day_tick In-game days elapsed since the campaign start.
/// @return         Short calendar string.
std::string short_date(uint64_t day_tick);

} // namespace ui::fmt
