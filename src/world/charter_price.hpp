#pragma once
// ---------------------------------------------------------------------------
// charter_price.hpp -- THE PRICE DIVISOR (BL-1064), in a header of its own.
// ---------------------------------------------------------------------------
//
// One constant, two readers. `stockpile_budget.hpp` derives a charter's price
// from the 1960 stockpile over it at the close (BL-1064), and the
// Industrialisation span reads the SAME divisor every year to price the
// RUNNING stock for its works-chartered notes (BL-1099, `history_sim.cpp`), so
// a crossing in 1780 is read against the price the close's own price grows
// into. It sits here rather than in `stockpile_budget.hpp` because that header
// pulls `world.hpp` and the carve index, which the sim must not include; and
// it is ONE definition rather than a hoisted copy, because a divisor that
// differs between the span and the close is exactly the inconsistency NR-907
// ruled out.

#include <cstdint>

/// The divisor a world's whole stockpile is split by into the price of ONE firm
/// charter (Ben, 2026-09-21, NR-907: "a charter's price is the world's whole
/// industry stockpile divided by a constant"; INDUSTRIALISATION.md § 1).
///
/// TWO KNOBS, TWO JOBS (Ben, 2026-09-21, NR-908; INDUSTRIALISATION.md § 1).
/// A centre affords a specialist when its points cover
/// `k_stockpile_specialist_firm_charters` / this of the world's stock, so the
/// SEAT MENU turns on the ratio of the two alone, and the specialist's price in
/// firm charters is the knob set against it. THIS divisor sets how many firm
/// charters a world's stock buys — its density, and so its tick — and is the
/// knob set against LIVE-PLAY COST.
///
/// PINNED AT 650. THE RULE is Ben's (2026-09-21, NR-910): with the specialist at
/// two firm charters (`stockpile_budget.hpp`), the divisor at which the median
/// library world opens the seat-menu anchor's nine seats. THE NUMBER is that
/// rule read on the SHIPPED world (Ben, 2026-09-22, NR-914): 580 was its
/// reading on a seat curve taken with BL-1037's corridor tier off, and turning
/// the tier on moved every stockpile (on the shipped world 580:2 opens a median
/// 6.5). The shipped seat curve (`stockpile_budget_check --seat-curve`, 16
/// library seeds, m = 2, centres affording a specialist): median 7.5 / 7.5 / 8
/// / 8.5 / 8.5 / 9 / 13.5 at d = 600 / 610 / 620 / 630 / 640 / 650 / 660 — 650
/// is the first divisor at nine — and no library world opens none anywhere on
/// it. A LARGER ratio d/m is a CHEAPER seat and MORE seats, and the step at 660
/// is steep (seed 12 goes 4 -> 52): worlds with many near-equal cities cross
/// the price together. The spread is ACCEPTED, not capped (NR-910) — 4 to 98
/// seats across the library at 650:2: the anchor is a median. Live-play cost:
/// the divisor alone sets the tick (BL-1043 stage 2: x0.43 / x0.91 / x1.70 the
/// legacy world at 325 / 650 / 1300), so 650 runs near x0.91 the legacy tick.
inline constexpr std::int64_t k_stockpile_price_divisor = 650;
static_assert(k_stockpile_price_divisor > 0, "the price divisor must be > 0");

/// The RUNNING charter price of a stock of @p total points: the whole stock
/// over the divisor, floored, and never below one point (the same arithmetic
/// `build_stockpile_budget` fixes the close's price by, so the span and the
/// close price one rule). A negative total is not a stock and prices 1.
constexpr std::int64_t charter_running_price(std::int64_t total)
{
    return total <= 0 ? 1 : (total / k_stockpile_price_divisor < 1 ? 1
                                                                    : total / k_stockpile_price_divisor);
}
