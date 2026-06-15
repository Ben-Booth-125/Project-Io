# Project Io — Time Controls

The **time panel** is a single panel in the top-right corner of the shell, split
into **two columns** (25% / 75%): a compact **calendar block** on the left and the
**speed controls** on the right. It is the same width as the minimap so the right
edge stays aligned. See `LAYOUT.md` for placement.

This document records the current implemented behaviour and is to be expanded. The authoritative pacing constants live in `src/core/sim_loop.hpp`.

---

## Calendar block (left column, 25%)

A player-facing clock in three stacked rows:

- **Year + quarter** — `1960 Q1` (`calendar_date::year` / `::quarter`).
- **Month + day** — `Jan 01`, a three-letter month abbreviation
  (`ui::fmt::month_abbrev`) and the zero-padded day.
- **Quarter progress** — a progress bar showing how far through the current
  in-year quarter the campaign is. The economy resolves on the quarter boundary, so
  this doubles as a countdown to the next economy tick. **The `xx%` overlay text is
  suppressed (empty overlay string)** so the bar reads as a clean animated fill
  toward the economy-tick boundary (settled 2026-06-15, [C2]).

The calendar completes sim_loop's tentative constants (30-day months, 3-month
quarters) with a 4-quarter, 360-day year. The 12 thirty-day months map to
Jan–Dec, and the campaign epoch is set so day 0 falls on `Jan 01 1960`
(`ui::fmt::campaign_epoch_year`).

## Speed controls (right column, 75%)

A raw `Sim` counter with the current multiplier, then the controls:

- **`II`** — pause.
- **`1`–`5`** — set the speed multiplier. The active speed is highlighted.

## Speed curve (settled 2026-06-15, [C2])

> **⟳ Pending review (2026-06-15) — transient.** Redefined the five-button speed curve
> (non-linear; button 3 is the 1× reference; buttons 1–2 are slower-than-realtime). Implementation
> lands when the [C2] time-speed Brief is promoted. Remove once reviewed. See TODO § Canvas.

The five speed buttons map **non-linearly**, with **button 3 as the 1× normal-play reference**.
Buttons 1–2 are genuine slow-motion (for watching detail) and 4–5 fast-forward aggressively (for
skipping quiet quarters to the next economy tick):

| Button | Multiplier | Role |
|--------|-----------|------|
| 1 | 0.25× | Slow-motion (detail) |
| 2 | 0.5× | Slow-motion |
| 3 | **1×** | Normal-play reference |
| 4 | 4× | Fast-forward |
| 5 | 16× | Fast-forward to the next econ tick |

The lever is the speed→multiplier mapping in `sim_loop.{hpp,cpp}` (`max_speed`, the `step_ms`
divisor) and the button labels in the `app.cpp` time panel. At 1× a day is ≈ 6 s (so 4× ≈ 1.5
s/day, 16× ≈ 0.4 s/day).

## The three-layer clock

The clock has three layers — **sim tick → day → economy tick**. The economy resolves
**quarterly** (every economy tick). Supply/demand and market calculations are tick-gated to this
layer (see `CONCEPT.md` — tick-gated economy), keeping simulation cost bounded while real-time
play continues between updates.

These constants are authored in `src/core/sim_loop.hpp`; the speed-multiplier curve above is the
settled mapping.

## Open questions

- Whether pause and speed gain keyboard shortcuts.
- Surfacing the economy-tick countdown (how long until the next quarter resolves).

## Related

- `src/core/sim_loop.hpp` — authoritative tick model and pacing constants.
- `LAYOUT.md` — placement in the shell.
- `CONCEPT.md` — tick-gated economy.
