# Project Io — Known Bugs

Known defects and rough edges in the prototype. **This is not a backlog file** — a known bug is a
*reported defect with a settled (or owed) fix*, not a unit of design intent, so it lives here
rather than in the backlog. Each entry records the symptom, the confirmed root cause
(where known), and the settled fix or the work still owed. When a fix lands, the entry moves to
the DEVLOG and is removed here.

> Relocated from `BACKLOG.md` § Known Bug (2026-06-15; the file was then named `OPENS.md`) — known
> bugs are defect tracking, not design items. The two entries below carry the fix designs settled in
> the 2026-06-15 design pass.

---

## Frame stutter / performance + hardware limits unconfigured

**Symptom.** The app stutters intermittently. May be benign for now, but the cause is not
diagnosed and there is no frame-pacing or hardware-limit configuration (vsync / target frame-rate
/ present mode, and the per-frame draw budget for the dense tile grids — Kepler is 180×84 = 15,120
tiles redrawn each frame, plus the upcoming per-tile lens tint/border passes). Worth headroom
before Layer 4's denser UI piles on.

**Possible benign cause.** It may simply be **display resolution / pixel density** — not enough
pixels on screen for the dense grid — rather than a pacing or draw-call defect. The measurement
below should distinguish a genuine frame-time problem from a perceived-sharpness one.

**Baseline (2026-06-14).** vsync is on (`SDL_SetRenderVSync(m_renderer, 1)`, `app.cpp:77`), with no
frame cap and no per-frame timing readout.

**Settled fix — the measurement instrument first.** The blocker was that classifying the stutter
needs frame-time data over a *live* present loop, which the headless harness cannot observe. The
settled next action is to build that instrument: an always-running **frame-time HUD** in `app.cpp`
— a capped ring buffer of the last N inter-frame `dt` values (already available from the
fixed-timestep loop), drawn as a small overlay reading **last / avg / max ms + a 1% spike count +
a frame-time sparkline**, toggled by a debug key. With the HUD, the classification (present-driven
vsync/composition vs. CPU draw-call volume vs. per-frame allocation churn) becomes *runnable* by
reading it while panning the dense Kepler grid. Once classified, the fix (frame cap / present-mode
change / cached tile geometry / dirty-rect) is a follow-on entry. Files: `src/core/app.{hpp,cpp}`.
v0.0.8 hardening, not on the Layer-4 critical path.

## Body labels move in steps, not smoothly

**Symptom.** Body labels visibly advance only every few ticks while the body dot glides smoothly.
Same code path on the Solar and Circumplanetary canvases.

**Root cause (confirmed 2026-06-14).** The label position derives from the live float `pos` every
frame (`solar_system_canvas.cpp:218–224`, no rounding). The stepping is `ImDrawList::AddText`
snapping glyphs to the integer pixel grid while the dot (`AddCircleFilled`) is sub-pixel
anti-aliased — the glyph-placement-quantisation path, not stale position. The font-oversampling
pass (`src/ui/fonts.hpp`) improved glyph crispness but did not fix this motion artefact.

**Settled fix — accept and document, with one cheap mitigation.** Glyph-grid snapping is ImGui's
standard raster behaviour; true sub-pixel text is disproportionate for a prototype. The actual
eyesore is the *relative* lag between dot and label (they desync), not that the label steps. So:
(a) round **both** the dot centre and the label origin to the same integer pixel each frame so dot
and label *step together*, removing the visible desync; (b) accept the residual whole-label step
as a documented prototype limitation (recorded in `SOLAR.md`). **Verification** is a one-off live
visual check — the temporal artefact is invisible to the headless capture harness. Files:
`src/ui/solar_system_canvas.cpp` (and the Circumplanetary canvas).

---

## Related

- `docs/development/DEVLOG.md` — where a fixed bug's record lands.
- `docs/development/BACKLOG.md` — design intent (items; metadata in `backlog.json`), distinct from defect tracking here.
