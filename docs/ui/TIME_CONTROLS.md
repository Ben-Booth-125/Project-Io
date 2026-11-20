# Project Io — Time Controls

> **Settles:** which questions the time panel answers about where the campaign
> stands in its calendar and how fast it is moving · which rows it stacks and what
> each reads · what shape the speed choices take · which clocks are layered and
> which of them the player is actually steering.
> **Not here:** the pacing constants (`src/core/sim_loop.hpp`) · what resolves on a
> month boundary (economy/*) · placement (LAYOUT) · the session menu the panel
> hosts (MENU).
> **Confused with:** HEADER.md, LAYOUT.md, MENU.md.

The **time panel** is a single panel in the top-right corner of the shell, the
same width as the minimap so the right edge stays aligned (`tick_w = mm_w`). It
is **four stacked full-width rows** (BL-138, compact time panel; legibility pass
BL-178, revised on Ben's 2026-07-10 and 2026-07-15 reviews). See `LAYOUT.md`
for placement; the draw is `ui::draw_time_panel` in `src/ui/time_panel.{hpp,cpp}`
(the `##time_panel` block, which also hosts the gear that opens the escape menu), and the
authoritative pacing constants are in `src/core/sim_loop.hpp`.

---

## Layout — four stacked rows

Panel height is content-derived, not a minimap fraction. Top to bottom:

1. **Year + date/month** — one left-aligned line at the base font size:
   `1960   Jan 1st [Q1]` (`ui::fmt::date_from_day`). Year and date share one row
   (Ben's 2026-07-15 review), giving the height to a taller speed row.
2. **Month-progress bar** — full width, text-height, with a **centred overlay
   naming the distance to the next resolution**: `58 d to Q2` (`%d d to Q%d`).
   The overlay exists because a bare fill fails to convey "how close am I to the
   economy resolving", which is the fact the bar carries; it overturns the
   2026-06-15 [C2] no-overlay rule. A tooltip explains what resolves on the
   boundary (prices clear, production banks, budget settles).
3. **Speed row** — full width, double frame height, aligned with the bar above:
   a **pause slot** plus tier buttons **Roman I–V**. When running, the pause slot
   carries a **drawn filled square** glyph (deliberately not `||`, which reads as
   the numeral II beside the tiers); when paused it flips to `>` (play). The
   active tier is highlighted; pause toggles, restoring the previous speed.
4. **Rate line** — an always-visible dimmed line naming the **active** tier's
   multiplier — `5×` — or `Paused`. It carries the multiplier alone: the real-time
   cost of a month was an explainer the player does not need (Ben, 2026-11-20).

**Per-button rate tooltips.** Each tier button names its multiplier and hotkey on
hover — `Speed III — 5× (3)` — and nothing more (Ben, 2026-11-20). The pause slot's tooltip is `Pause (Space)` / `Resume (Space)`.

The calendar completes sim_loop's constants (30-day months, one month per economy tick)
with a 12-month, 360-day year. The 12 thirty-day months map to Jan–Dec, and the
campaign epoch is set so day 0 falls on `Jan 1st 1960`
(`ui::fmt::campaign_epoch_year`).

## Opening state — paused (Ben, 2026-11-19)

**Every arrival in the game opens paused** — a new game and a loaded save alike. The player lands
on a stopped clock and reads the opening before anything moves; the first press of play (the play
slot, a tier, or `Space`) starts it. A new game's first `Space` resumes at the tier
`scripts/init.lua`'s `default_speed` names; a load's resumes at the speed the save was made at.

## Speed curve (settled 2026-06-15, [C2]; III and IV raised, Ben 2026-11-19; III to 5×, Ben 2026-11-20)

The five speed buttons map **non-linearly**. Buttons 1–2 are genuine slow-motion (for watching
detail); 3 is ordinary play; 4–5 fast-forward (for skipping quiet months to the next economy tick):

| Button | Multiplier | Real time per day | Real time per month | Role |
|--------|-----------|------|------|------|
| 1 | 0.25× | 24 s | 12 min | Slow-motion (detail) |
| 2 | 0.5× | 12 s | 6 min | Slow-motion |
| 3 | 5× | 1.2 s | 36 s | Ordinary play |
| 4 | 10× | 0.6 s | 18 s | Fast-forward |
| 5 | 16× | 0.375 s | ~11 s | Fast-forward to the next econ tick |

The multiplier scales `seconds_per_day_1x` (6 s; 12 sim ticks a day, 30 days a month). Tier V
bounds the sim's real per-tick load, so no lower tier may exceed it. The lever is the
speed→multiplier mapping in `sim_loop.{hpp,cpp}` (`max_speed`, the `step_ms` divisor) and the
button labels in the time panel.

## The three-layer clock

The clock has three layers — **sim tick → day → economy tick**. The economy resolves
**monthly** (every economy tick). Supply/demand and market calculations are tick-gated to this
layer (see `CONCEPT.md` — tick-gated economy), keeping simulation cost bounded while real-time
play continues between updates.

These constants are authored in `src/core/sim_loop.hpp`; the speed-multiplier curve above is the
settled mapping.

## What the panel answers

The panel is the production clock view: where-we-are (row 1), when-the-economy-next-resolves
(row 2's countdown overlay, inside the bar), and how-fast (rows 3–4). The absolute next-month
date alongside the relative count is not shown; there is no demand for it.

**Keyboard shortcuts.** `Space` toggles pause/resume; `1`–`5` set the speed multiplier,
mirroring the on-screen buttons. These route through the shared canvas-command vocabulary
(`canvas_command`) so they read as key bindings, consistent with the canvas keyboard model
(CANVASES.md § Keyboard).

## The escape menu (Ben, 2026-11-05)

The session gear sits **to the right of the time controls**, inside the right end of this panel,
because the panel is flush to the screen edge. It opens the full-window escape menu and pauses the
game (`MENU.md`).

## Related

- `src/core/sim_loop.hpp` — authoritative tick model and pacing constants.
- `LAYOUT.md` — placement in the shell.
- `CONCEPT.md` — tick-gated economy.
