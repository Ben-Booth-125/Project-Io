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
  in-year quarter the campaign is, labelled with the percentage
  (`ui::fmt::quarter_progress`). The economy resolves on the quarter boundary, so
  this doubles as a countdown to the next economy tick.

The calendar completes sim_loop's tentative constants (30-day months, 3-month
quarters) with a 4-quarter, 360-day year. The 12 thirty-day months map to
Jan–Dec, and the campaign epoch is set so day 0 falls on `Jan 01 1960`
(`ui::fmt::campaign_epoch_year`).

## Speed controls (right column, 75%)

A raw `Sim` counter with the current multiplier, then the controls:

- **`II`** — pause.
- **`1`–`5`** — set the speed multiplier. The active speed is highlighted.

## The three-layer clock

The clock has three layers — **sim tick → day → economy tick** — paced so that:

- 1× ≈ 6 s/day
- 3× ≈ 2 s/day

The economy resolves **quarterly** (every economy tick). Supply/demand and market calculations are tick-gated to this layer (see `CONCEPT.md` — tick-gated economy), keeping simulation cost bounded while real-time play continues between updates.

These constants are deliberately tentative and authored in `src/core/sim_loop.hpp`.

## Open questions

- Whether pause and speed gain keyboard shortcuts.
- Surfacing the economy-tick countdown (how long until the next quarter resolves).

## Related

- `src/core/sim_loop.hpp` — authoritative tick model and pacing constants.
- `LAYOUT.md` — placement in the shell.
- `CONCEPT.md` — tick-gated economy.
