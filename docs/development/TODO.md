# Project Io — TODO

Parked thoughts, recorded but not yet actioned. These are deliberately deferred
while canvas work takes priority. None of them are committed designs — they are
reminders to revisit.

Difficulty scale: **1** trivial · **2** light work · **3** medium · **4** hard ·
**5** very hard · **6** deferred

---

## Canvas

- **[4] Asteroid belt as a textured ring, not bodies.** There is a single asteroid
  belt. Render it as a thick, somewhat translucent textured ring (a band, not a
  set of orbiting body dots). Within the belt sit ~3 *notable* asteroids that
  remain individually selectable bodies; the belt itself is **not** a body.
  Current state: the three asteroids (Vesta, Ceres, Pallas) exist as ordinary
  asteroid bodies and there is no ring — this is a placeholder. Decide how the
  notable asteroids relate to the ring (embedded markers? separate bodies drawn
  over the band?) when implementing. See `docs/ui/CANVASES.md`.

## Known bugs

- **[6] Body labels shimmer while moving (font sub-pixel rendering).** On the Solar
  System Canvas, planet/asteroid labels shimmer slightly as the body moves. Root
  cause: the default ImGui font is a bitmap atlas with **no sub-pixel
  positioning**, so glyphs are only crisp at integer pixel coordinates — the
  anti-aliased body dot reads smooth at any fraction, but text at a fractional
  coordinate softens/shimmers as the fraction changes each frame. Currently the
  label is drawn at its live fractional position (fluid motion, but shimmers).
  Rounding to whole pixels removes the shimmer but makes the label step 1px at a
  time. The durable fix is to enable **font oversampling / sub-pixel rendering**
  (fluid *and* crisp) — load the font with oversampling, or render labels via a
  higher-quality text path. Deferred. See `docs/ui/CANVASES.md`.

## Menus

- **[6] Define the menu items from the systems.** Work out the important menu items
  driven by the game systems (`docs/SYSTEMS.md`), then **get feedback on the
  intended order before final implementation.** See `docs/ui/MENU.md`.
