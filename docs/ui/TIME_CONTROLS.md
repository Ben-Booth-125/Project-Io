# Project Io — Time Controls

The **time panel** is a single panel in the top-right corner of the shell, the
same width as the minimap so the right edge stays aligned (`tick_w = mm_w`). The
old two-column (25%/75%) split is gone — the BL-138 compact redesign (revised on
Ben's 2026-07-10 and 2026-07-15 reviews, legibility pass BL-178) settled it as
**four stacked full-width rows**. See `LAYOUT.md` for placement; the draw lives
in the `##time_panel` block of `src/core/app.cpp`, and the authoritative pacing
constants in `src/core/sim_loop.hpp`.

---

## Layout — four stacked rows (BL-138/BL-178)

Panel height is content-derived (BL-097), not a minimap fraction. Top to bottom:

1. **Year + date/quarter** — one left-aligned line at the base font size:
   `1960   Jan 1st [Q1]` (`ui::fmt::date_from_day`; the year is no longer an
   oversized centred heading — Ben's 2026-07-15 review merged year and date onto
   one row, giving the reclaimed height to a taller speed row).
2. **Quarter-progress bar** — full width, text-height, with a **centred overlay
   naming the distance to the next resolution**: `58 d to Q2`. **BL-178 reversed
   the old no-overlay rule** (settled 2026-06-15, [C2], which suppressed the
   `xx%` text): the bare fill failed to convey "how close am I to the economy
   resolving", which is the fact the bar exists to carry. A tooltip explains
   what resolves on the boundary (prices clear, production banks, budget settles).
3. **Speed row** — full width, double frame height, aligned with the bar above:
   a **pause slot** plus tier buttons **Roman I–V**. When running, the pause slot
   carries a **drawn filled square** glyph (deliberately not `||`, which read as
   the numeral II beside the tiers); when paused it flips to `>` (play). The
   active tier is highlighted; pause toggles, restoring the previous speed.
4. **Rate line** — an always-visible dimmed line naming the **active** tier's
   real rate: `1×  ·  ~3m per quarter`, or `Paused` (BL-178 — a tooltip cannot
   be seen without hovering, so the current speed's meaning is stated on
   screen). Guaranteed-fit: the string is measured and the compact form (rate
   only) is drawn if the long one would not fit.

**Per-button rate tooltips (BL-178).** Each tier button names its real rate on
hover — `Speed III — 1×` plus `~3m per quarter (3)` — derived from
`sim_loop::speed_multiplier` and the sim-loop constants (`speed_rate_label` /
`speed_quarter_label` in `app.cpp`), so the labels can never drift from the
curve. The pause slot's tooltip is `Pause (Space)` / `Resume (Space)`.

The calendar completes sim_loop's constants (30-day months, 3-month quarters)
with a 4-quarter, 360-day year. The 12 thirty-day months map to Jan–Dec, and the
campaign epoch is set so day 0 falls on `Jan 1st 1960`
(`ui::fmt::campaign_epoch_year`).

## Speed curve (settled 2026-06-15, [C2])

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
divisor) and the button labels in the `app.cpp` time panel. At 1× a day is **2 s**
(`seconds_per_day_1x`, retuned 3× quicker 2026-07-15 from the original ≈ 6 s) — so one 90-day
quarter costs ~3 m at 1×, ~45 s at 4×, ~11 s at 16×.

## The three-layer clock

The clock has three layers — **sim tick → day → economy tick**. The economy resolves
**quarterly** (every economy tick). Supply/demand and market calculations are tick-gated to this
layer (see `CONCEPT.md` — tick-gated economy), keeping simulation cost bounded while real-time
play continues between updates.

These constants are authored in `src/core/sim_loop.hpp`; the speed-multiplier curve above is the
settled mapping.

## Production clock view — landed

**Landed (2026-07-31 note).** The [F3] production design (settled 2026-06-15) is now the built
panel above: where-we-are (row 1), when-the-economy-next-resolves (row 2's countdown overlay —
delivered as `58 d to Q2` *inside* the bar via BL-178, rather than the worded companion beside an
overlay-less fill that [F3] pictured), and how-fast (rows 3–4). The provisional two-column split
it was licensed to replace is gone with BL-138.

**Keyboard shortcuts (settled, built).** `Space` toggles pause/resume; `1`–`5` set the speed
multiplier, mirroring the on-screen buttons. These route through the shared canvas-command
vocabulary (`canvas_command`) so they read as key bindings, consistent with the canvas keyboard
model (CANVASES.md § Keyboard).

## Open questions

- ~~Exact countdown phrasing~~ — settled by BL-178 (`%d d to Q%d`, landed 2026-07-30). The
  absolute next-quarter date alongside the relative count remains unshown; no demand for it yet.

## Related

- `src/core/sim_loop.hpp` — authoritative tick model and pacing constants.
- `LAYOUT.md` — placement in the shell.
- `CONCEPT.md` — tick-gated economy.
