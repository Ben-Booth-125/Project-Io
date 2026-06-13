# Project Io — Time Controls

The **time column** is a two-panel stack in the top-right corner of the shell: the **system tick** readout above the **speed controls**. Both are the same width as the minimap so the right edge stays aligned. See `LAYOUT.md` for placement.

This document records the current implemented behaviour and is to be expanded. The authoritative pacing constants live in `src/core/sim_loop.hpp`.

---

## System tick — readout (top)

A permanent, non-interactive player-facing clock:

- **Day** — in-game days elapsed.
- **Econ** — economy ticks (quarters) elapsed.

No date formatting yet — the readout shows raw Day/Econ counts.

## Speed controls (below)

The time controls and a raw `Sim` counter with the current multiplier:

- **`II`** — pause.
- **`1`–`5`** — set the speed multiplier. The active speed is highlighted.

## The three-layer clock

The clock has three layers — **sim tick → day → economy tick** — paced so that:

- 1× ≈ 6 s/day
- 3× ≈ 2 s/day

The economy resolves **quarterly** (every economy tick). Supply/demand and market calculations are tick-gated to this layer (see `CONCEPT.md` — tick-gated economy), keeping simulation cost bounded while real-time play continues between updates.

These constants are deliberately tentative and authored in `src/core/sim_loop.hpp`.

## Open questions

- Calendar/date formatting for the Day readout (currently raw counts).
- Whether pause and speed gain keyboard shortcuts.
- Surfacing the economy-tick countdown (how long until the next quarter resolves).

## Related

- `src/core/sim_loop.hpp` — authoritative tick model and pacing constants.
- `LAYOUT.md` — placement in the shell.
- `CONCEPT.md` — tick-gated economy.
