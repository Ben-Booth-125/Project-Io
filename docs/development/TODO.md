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
  Current state: the placeholder belt asteroids (Vesta, Ceres, Pallas) and all
  other backdrop bodies were removed during Layer 2 finalisation — the world now
  holds only Helios, Cinder, Kepler, and Selene. There is no belt or ring yet.
  Decide how the notable asteroids relate to the ring (embedded markers? separate
  bodies drawn over the band?) when implementing. See `docs/ui/CANVASES.md`.

## UI building blocks (decide before Layer 3)

Layer 2 deliberately kept the UI thin, but Layers 3–6 (extraction → market →
supply → budget) all lean on the same handful of rendering primitives. These are
the pieces we simplified that are *certainly* needed later; the goal is a
versatile basic version of each now — the **building block**, not the detail — so
later layers extend it rather than inventing one each. None are committed
designs; record, then decide.

- **[6] Informative tooltip / hover-card system.** Deferred. The single most important
  player-communication surface for a grand strategy game. Today there is one
  ad-hoc `ImGui::BeginTooltip` inside the Planetary canvas; everything else has
  none. We want a *shared* hover-card primitive with a consistent structure —
  title line (name + type + icon), a short stat block, optional sectioned detail,
  and room for "why" annotations (e.g. how a price or yield was derived) so we
  can explain mechanics from the player's perspective as we revise them. It must
  work for every hoverable thing across all canvases and ledgers: bodies (Solar /
  Circumplanetary), tiles (Planetary), buildings, markets, and later convoys and
  routes. Decide: a single `draw_hover_card(...)` helper vs. per-entity builders;
  instant vs. delayed reveal; how a "rich card" (LAYOUT.md popup elements) differs
  from the lightweight canvas tooltip. Likely earns its own `docs/ui/TOOLTIP.md`
  once the shape is settled. See `docs/ui/LAYOUT.md` (UI popup elements).

- **[3] Centralised presentation metadata (resource identity + semantic palette).**
  `resource_labels[]` is duplicated in `tile_inspector.cpp` and
  `body_surface_canvas.cpp`, and carries only a name. Establish one source of
  truth for each resource — display name, short label/abbreviation, colour, and
  (eventually) icon — used everywhere a resource appears (header strip, market
  ledger, tile deposits, tooltips, future overlays). Alongside it, a small
  **semantic palette**: positive/negative (profit/loss, surplus/deficit),
  selection/hover/pinned, and reserved faction colour slots (data model already
  allows multi-faction). Keeps the data-dense UI visually consistent and makes a
  later restyle a one-file change. See `docs/ui/HEADER.md`.

- **[2] Shared value & date formatting helpers.** Layers 3–6 are almost entirely
  changing numbers, currently printed raw (`%.1f`, `%llu`). Provide small
  formatters for: credits/currency, large-magnitude abbreviation (1.2k / 3.4M),
  signed and colour-coded deltas (income vs. expense, price moves), rates
  (per-tick / per-day), and percentages. Include the deferred **date/quarter
  formatting** (the tick readout still shows raw Day/Econ counts). One legible,
  reused number path before the numbers start moving. See `docs/ui/LAYOUT.md`
  (time column) and `docs/ui/HEADER.md`.

- **[4] Canvas overlay layer + mode switching.** Economic/military data (supply
  routes, convoy paths/progress, faction presence) is deferred to later layers as
  *overlays* on the canvases, and the minimap **mode bar** is already reserved
  chrome for selecting them — but no overlay mechanism exists. Decide the building
  block now: an overlay draw pass on top of each canvas, an overlay-mode value in
  `ui_state`, and how the mode bar toggles it. Layer 5 (supply routing) is the
  first hard requirement — convoy lines between bodies on the Solar canvas with a
  progress marker. See `docs/ui/CANVASES.md` (What is deferred) and `MINIMAP.md`.

- **[3] Icon/glyph + font atlas strategy.** A grand-strategy UI is icon-heavy
  (resource strip "icon + quantity", building-type markers, unit/convoy markers),
  and the canvases currently use only the default ImGui bitmap font. Decide an
  approach — an icon font (Font Awesome / Kenney) merged into the atlas, or vector
  glyphs via the draw list — and load the atlas with **oversampling / sub-pixel
  positioning** while we are at it, which also fixes the body-label shimmer bug
  below. Tackling the font atlas once covers both. See the shimmer item under
  *Known bugs*.

- **[2] Shared selection / hover / pinned highlight convention.** Each canvas
  hand-draws its own selection (white hex outline, body ring). As more selectable
  entity types appear (buildings, convoys, routes) and the Explorer adds *pinned*
  items, settle one visual language for selected vs. hovered vs. pinned and a
  small shared helper, so highlights read the same everywhere. See
  `docs/ui/EXPLORER.md` and `CANVASES.md`.

- **[2] "Focus on entity" view-navigation helper.** The Explorer's "jump straight
  to them" and any future notification/alert needs one call that focuses an
  entity: set `active_body`, choose the correct rung (`primary_level`), and centre
  that rung's pan/zoom on the target. A reusable building block rather than
  ad-hoc state pokes. See `docs/ui/EXPLORER.md`.

- **[3] Render-time interpolation for fractional-progress entities.**
  TECH_FOUNDATIONS specifies the render loop interpolates visual state between
  simulation states; orbits currently advance directly by elapsed days, which is
  fine. But Layer 5 convoys carry a fractional progress that completes only at the
  economy-tick boundary — their position on the Solar canvas must interpolate
  smoothly between ticks rather than jumping. Decide the render-side read path for
  fractional progress before convoys exist. See `docs/tech/TECH_FOUNDATIONS.md`
  (in-flight actions at tick boundaries).

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
