#pragma once
// ---------------------------------------------------------------------------
// charter_price.hpp -- THE PRICE DIVISOR (BL-1064), in a header of its own.
// ---------------------------------------------------------------------------
//
// One constant, two readers. `stockpile_budget.hpp` derives a charter's price
// from the 1960 stockpile over it at the close (BL-1064), and the
// Industrialisation span reads the SAME divisor every year to price the
// RUNNING stock of the same trade reach for its works-chartered notes (BL-1099,
// BL-1168, `history_sim.cpp`), so
// a crossing in 1780 is read against the price the close's own price grows
// into. It sits here rather than in `stockpile_budget.hpp` because that header
// pulls `world.hpp` and the carve index, which the sim must not include; and
// it is ONE definition rather than a hoisted copy, because a divisor that
// differs between the span and the close is exactly the inconsistency NR-907
// ruled out.

#include <cstdint>

/// The divisor a stockpile is split by into the price of ONE firm charter (Ben,
/// 2026-09-21, NR-907: "a charter's price is the world's whole industry
/// stockpile divided by a constant"; INDUSTRIALISATION.md § 1). WHOSE stockpile
/// was narrowed (Ben, 2026-10-03, BL-1168): the stock within the centre's TRADE
/// REACH — its landmass (`stockpile_budget.hpp` `charter_price_reach`; the
/// world's where a centre has none, the dearest price). The span's running
/// price below is read on the SAME reach — each region's landmass's running
/// stock (`history_sim.cpp`, the works notes) — so the span and the close price
/// one stock by one divisor.
///
/// TWO KNOBS, TWO JOBS (Ben, 2026-09-21, NR-908; INDUSTRIALISATION.md § 1).
/// A centre affords a specialist when its points cover
/// `k_stockpile_specialist_firm_charters` / this of the stock it is priced by,
/// so the SEAT MENU turns on the ratio of the two alone, and the specialist's
/// price in firm charters is the knob set against it. THIS divisor sets how
/// many firm charters a stock buys — its density, and so its tick — and is the
/// knob set against LIVE-PLAY COST.
///
/// PINNED AT 650 (Ben, 2026-09-21, NR-910; the number read on the shipped world,
/// 2026-09-22, NR-914). PROVENANCE, READ UNDER THE WORLD'S PRICE AND THE
/// OVERTURNED PIN OF TWO CHARTERS A SPECIALIST: the rule was the divisor at
/// which the median library world opens the seat-menu anchor's nine seats with
/// the specialist at two firm charters. 580 was its reading with BL-1037's
/// corridor tier off; on the shipped world the seat curve
/// (`stockpile_budget_check --seat-curve`, 16 library seeds, m = 2) ran median
/// 7.5 / 7.5 / 8 / 8.5 / 8.5 / 9 / 13.5 at d = 600 / 610 / 620 / 630 / 640 /
/// 650 / 660, the library spread 4 to 98 seats at 650. Live-play cost: the
/// divisor alone sets the tick (BL-1043 stage 2: x0.43 / x0.91 / x1.70 the
/// legacy world at 325 / 650 / 1300, world price). Under the reach price at 44
/// charters, 650 reads a median x1.12 the legacy tick (live_tick_cost_probe
/// --quick, 16 library seeds, x0.77 to x1.61 per seed, noisy).
///
/// THE SEAT MENU NO LONGER TURNS ON THIS (Ben, 2026-10-03, option a, BL-1168
/// folding BL-1151): under the reach price the anchor is carried by the
/// charters — 44 a specialist, overturning NR-910's two
/// (`k_stockpile_specialist_firm_charters`) — because the divisor that would
/// reach it alone (~36 at two charters) starves the web; 650 stays the
/// live-play-cost pin.
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
