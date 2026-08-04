# Project Io — Known Bugs

**Retired as a tracking surface (2026-07-31).** Practice has moved: defects are filed as backlog
items in [`backlog.json`](backlog.json), same as any other work — BL-234 (font glyph range) is the
pattern: a defect found in passing, filed with a design field, sequenced against a version goal.
This file stays only because other docs link it; do not add entries here.

The two entries it carried, disposed of honestly:

- **Frame stutter / performance.** The settled fix — a frame-time HUD in `src/core/app.cpp`
  (last / avg / max ms + 1% lows + sparkline, debug-key toggled) — **was built** as BL-249
  (`ui::draw_frame_budget_hud`, called from `app.cpp`; covered by
  `scripts/verify/frame_budget_hud.lua`). *(Corrected 2026-08-04; the "remains unbuilt" note here
  was already stale when written.)* What is still owed is the **measurement** — the frame-budget
  targets (avg < 8 ms, max < 16.7 ms panning the full Kepler grid) need a human at the keyboard,
  NR-026, open. Performance work at large is now BL-267 (GPU and multicore).
- **Body labels move in steps, not smoothly.** The settled mitigation — round **both** the dot
  centre and the label origin to the same integer pixel so they step together — **never landed**
  (verified 2026-07-31: the label derives from the live float `pos` in
  `src/ui/solar_system_canvas.cpp`, no rounding on either). Still an open backlog candidate; file
  it as an item if the desync starts to grate. The residual whole-label step stays an accepted
  prototype limitation either way (glyph-grid snapping is ImGui's standard raster behaviour).

## Related

- `docs/development/backlog.json` — where defects are filed now.
- `docs/development/DEVLOG.md` — where a fixed defect's record lands.
