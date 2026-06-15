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
> lands when the [C2] time-speed Brief is promoted. Remove once reviewed. See OPENS § Canvas.

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

## Production clock view (settled 2026-06-15, [F3])

The prototype two-column layout is provisional; the **production design** is settled to what the
player actually needs from the clock at a glance, in priority order:

1. **Where we are** — year + quarter (the strategic unit), then month + day (the fine unit).
2. **When the economy next resolves** — the econ tick is the load-bearing beat, so it is surfaced
   **explicitly**, not left implicit in the progress bar. The calendar block carries a **days-until
   -next-quarter countdown** (e.g. `Q2 in 47d`) beside the quarter-progress fill, so the player
   reads both the analogue fill and the digital count. The progress bar keeps its clean overlay
   -less fill ([C2]); the countdown is the worded companion.
3. **How fast** — the current speed (the 1–5 curve above) and pause state.

**Keyboard shortcuts (settled).** `Space` toggles pause/resume; `1`–`5` set the speed multiplier,
mirroring the on-screen buttons. These route through the shared canvas-command vocabulary
(`canvas_command`) so they read as key bindings, consistent with the canvas keyboard model
(CANVASES.md § Keyboard).

The panel's relationship to the rest of the shell chrome is unchanged — it stays the top-right
clock aligned to the minimap width (LAYOUT.md). The two-column split is an implementation
detail free to change when this is built; the *content* above is the settled requirement.

## Open questions

- Production polish only: exact countdown phrasing and whether to show the absolute next-quarter
  date alongside the relative count.

## Related

- `src/core/sim_loop.hpp` — authoritative tick model and pacing constants.
- `LAYOUT.md` — placement in the shell.
- `CONCEPT.md` — tick-gated economy.
