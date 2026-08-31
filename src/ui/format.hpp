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
/// a 360-day year. The 12 thirty-day months map to Jan–Dec, and the campaign
/// epoch is `world_params::epoch_year` — day 0 falls on the first day of it.
struct calendar_date
{
    int year;    ///< Calendar year; the campaign epoch is whatever the live world was generated at (see campaign_epoch_year()).
    int month;   ///< 1-12 (Jan–Dec).
    int day;     ///< 1-30.
    int quarter; ///< 1-4 (the in-year quarter, derived from the month).
};

/// The calendar year day 0 falls in — i.e. the year the campaign opens on
/// January 1st of.
///
/// THIS IS A VALUE, NOT A CONSTANT (BL-705). It was `constexpr int = 1960` up
/// to 2026-08-31, which was simply wrong: `world_params::epoch_year` has
/// defaulted to 0 since the ancient refocus (NR-177), and no UI site read it —
/// so a 0 CE campaign rendered 1960-based dates on every clock in the shell.
/// Both starts are supported (`docs/economy/ERAS.md` § Where the ladder starts),
/// so a constant is wrong for one of them whichever value it holds.
///
/// The app publishes the live world's epoch through `set_campaign_epoch_year`
/// at the two points `app::m_active_world_params` is assigned — after
/// `setup_world` (a new campaign) and after a save loads. Display only: it
/// never enters `world`, never reaches the serialisation seam, and no
/// simulation value is derived from it, so it carries no determinism weight.
///
/// Before the first world exists the value is `world_params{}.epoch_year`, so a
/// menu-only session reads the same year a default campaign would.
int campaign_epoch_year();

/// Publish the live world's `world_params::epoch_year` to the tick calendar.
/// Called by the app; nothing else should.
void set_campaign_epoch_year(int year);

/// Render a day-of-month as an English ordinal string ("1st", "2nd", "3rd",
/// "4th", ..., "21st", "22nd", "23rd", "24th", ...), following the standard
/// suffix rule (the 11th/12th/13th exception applies regardless of tens digit).
///
/// @param day Day-of-month (any positive value; not range-checked against 30).
/// @return    Ordinal string, e.g. "1st".
std::string ordinal_day(int day);

/// Three-letter English month abbreviation for a 1-12 month index ("Jan".."Dec").
/// Returns "?" for an out-of-range month so a bad value is visible rather than
/// reading as a silent wrong month. Used by the compact `Jan 01` calendar line.
///
/// @param month 1-based month (1 == January).
/// @return      Static, null-terminated month abbreviation.
const char* month_abbrev(int month);

/// Decompose an absolute in-game day count (sim_loop::day_tick) into a calendar
/// date. Day 0 is January 1st of `campaign_epoch_year()`, Q1.
///
/// @param day_tick In-game days elapsed since the campaign start.
/// @return         The corresponding calendar date.
calendar_date date_from_day(uint64_t day_tick);

/// Fraction (0..1) of the way through the current in-year quarter, for a quarter
/// progress bar. Day 0 of a quarter reads 0.0; the value advances one quarter-day
/// step per day and reaches just under 1.0 on the quarter's final day.
///
/// @param day_tick In-game days elapsed since the campaign start.
/// @return         Progress through the quarter, clamped to [0, 1).
float quarter_progress(uint64_t day_tick);

/// The labour-contention read for a (corp, body) workforce scalar — the single
/// phrasing every surface uses, so the same ceiling seen from a ledger,
/// the Selection element and the hover card cannot drift in either its number or
/// its words (BL-179).
///
/// `scalar` is `economy_report::workforce_contention`: `min(1, supply/demand)`
/// for the corp's labour demand on that body. At or above 1.0 the corp's demand
/// fits the body's pool and the function returns an empty string — there is no
/// shortfall to report, and printing "100%" would invite the player to hunt for
/// a problem that is not there.
///
/// @param scalar Workforce contention scalar for the (corp, body).
/// @return       e.g. "84% (labour short)", or "" when labour is uncontended.
std::string labour_contention(float scalar);

} // namespace ui::fmt
