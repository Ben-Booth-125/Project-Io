# Project Io — Minimap

The **minimap** is a fixed inset in the bottom-right corner of the shell. It shows
a **neighbouring canvas at reduced scale** and is the player's "you are here, in
the bigger picture" readout and the control for stepping *out* one zoom level.

The three canvases — Solar, **Circumplanetary**, Planetary — form a three-rung
**zoom ladder**, so the minimap is not a toggle between two views but the rung
above the one being played. This document is the authoritative spec for the
minimap's chrome and navigation; the canvases it frames are specified in
[SOLAR.md](SOLAR.md), [CIRCUMPLANETARY.md](CIRCUMPLANETARY.md), and
[PLANETARY.md](PLANETARY.md). See [LAYOUT.md](LAYOUT.md) for placement in the
shell and [CANVASES.md](CANVASES.md) for the shared drawing path.

---

## The zoom ladder

Three canvases form a single vertical ladder from widest to narrowest view:

```
   ┌─────────────────────────┐
   │  Solar          (system)│   the star at centre, every body orbiting it
   ├─────────────────────────┤
   │  Circumplanetary (local)│   one planet at centre, its moons / local space
   ├─────────────────────────┤
   │  Planetary    (surface) │   one body's hex tile grid
   └─────────────────────────┘
        zoom out ↑   ↓ zoom in
```

Each rung has at most one rung **above** it (zoom out) and one **below** it
(zoom in). The **Circumplanetary** canvas is the middle rung: a planet drawn at
the centre with its moons orbiting it and the immediate local space around it
(stations, traffic). A planet with no moons is still a valid circumplanetary
view — it is the natural stepping stone between picking a planet out of the
system and dropping to its surface.

---

## Navigation model — *minimap is context*

The rule is deliberately asymmetric and simple:

- **The minimap always shows the rung one step *out* (zoom-out) from the primary.**
  It is pure context — "where does the thing I'm looking at sit in the larger
  view." It is never the place you drill *into*.
- **You descend by double-clicking a body in the primary canvas**, not by
  clicking the minimap. A body double-click navigates the primary *down* one
  rung (single-click selects — SELECTION.md owns the click model).
- **You ascend by clicking the minimap.** A minimap click promotes the zoom-out
  neighbour it is showing to primary.

### Descending (double-click a body in the primary)

| Primary | Double-click… | Primary becomes |
|---|---|---|
| Solar | a **planet** | that planet's **Circumplanetary** view |
| Solar | a **moon** | the **parent planet's** Circumplanetary view, with the moon highlighted |
| Circumplanetary | the **planet** or one of its **moons** | that body's **Planetary** surface |
| Planetary | — | (bottom rung; nothing to descend into) |

A solar→surface jump is always a two-step drill (system → local → surface) that
reads the same way every time.

### Ascending (click the minimap)

| Primary | Minimap shows | Click minimap → primary becomes |
|---|---|---|
| Planetary | Circumplanetary (parent planet) | Circumplanetary |
| Circumplanetary | Solar | Solar |
| Solar | *(no rung above — see below)* | — |

### The top rung (Solar primary) — **the ladder wraps**

Solar has no zoom-out neighbour, so the "context" slot has no canvas to show.
It shows **the galaxy, seen from the surface of the homeworld** — titled
**Galaxy**. Rather than invent a further rung, the ladder **loops back to where
the player is standing**: zoom all the way out and you are looking up from the
ground again.

That is deliberate, and it is what keeps this panel a *minimap* rather than a
fourth canvas. There is nothing to navigate here and no rung to ascend to; it
exists to be looked at. The player still descends from here by double-clicking a
body in the Solar primary — the panel itself stays **non-interactive**.

What it draws (`src/ui/star_map_view.cpp`, over the authored table in
`src/world/star_map.hpp`):

- the **galactic band** as an arc across the sky, thickening and brightening
  toward the core — not a level strip, which reads as a UI element rather than a
  sky;
- the **galactic core** as a bright bulge with the supermassive black hole
  (`Ith-Karan`) dark at its heart. From the ground the centre of the galaxy is a
  *direction*, not a destination, which is why it appears here as a glow rather
  than as a top-down map of itself;
- six named **constellations** with joining lines, plus a faint field biased
  toward the band;
- **deep-sky objects** — nebulae, globular clusters, a satellite galaxy, a
  quasar, and the three supernova remnants the body's biography names.

**The sky is fixed; the world is not.** Nothing in the table reads a seed. Every
campaign generates a different world around a different star, but the galaxy
those systems sit in is the same galaxy — it is the one fixture a player carries
between campaigns. Names are invented per the standing rule (real history is a
mechanism reference, never a name source), so there is no Orion and no
Betelgeuse.

**It turns.** One revolution per year against the campaign date, so the
constellations up tonight are not the ones up in six months. The projection
wraps in x for that reason; segments crossing the seam are skipped rather than
smeared across the panel.

---

## Minimap chrome

The minimap is framed by its own chrome — a **title bar** above the inset
canvas and a **lens mode bar** below it — so it reads as a deliberate panel
rather than a floating thumbnail. The minimap box is three tiers: **title bar**
(top) → **inset canvas** (middle) → **lens mode bar** (bottom). Immediately above
the box, sharing its width, sits the **lens chrome region** (below) — the
minimap's header in Ben's sense: not a fourth tier of the box, but the strip the
box's top edge anchors.

```
┌─────────────────────────┐
│  Kepler        (title)  │   ← title bar: the viewed body, or the star name
├─────────────────────────┤
│                         │
│   [ inset canvas ]      │   ← the zoom-out neighbour, drawn at reduced scale
│                         │
├─────────────────────────┤
│ [Co][Ctr][Rs][Mk][Pop][Cn]         │ ← lens mode bar: 6 glyph buttons
└─────────────────────────┘
```

### Title bar

A single line naming **what the minimap is currently showing** (the zoom-out
neighbour), which depends on the primary rung:

- Primary **Circumplanetary** → minimap shows **Solar** → title is the **star
  name** (see *Star as an entity* below); the system has one star, so this
  doubles as the system name.
- Primary **Planetary** → minimap shows **Circumplanetary** → title is the
  **planet name** (the circumplanetary anchor, e.g. `Kepler`).
- Primary **Solar** → minimap shows the sky → title is **`Galaxy`**.

The title bar is part of the minimap chrome, not the in-canvas title, so it is
**always shown** — including below the ~320 px threshold at which the *in-canvas*
title/labels are suppressed for clutter. The two should not both draw; when the
minimap chrome owns the title, the inset canvas suppresses its own.

### Overlay controls — the lens mode bar

The lens toggles live on the minimap (BL-093, Selection element redesign + lens
strip on the minimap) as a **lens mode bar** running along the bottom of the
minimap box, under the inset canvas.

The bar is a single row of **6 lens glyphs**: **Corp, Country, Resource,
Market, Population, Continent** — single-select with a null state (clicking the
active glyph clears the lens). This is a curated subset of the full lens family in
[LENSES.md](LENSES.md); **Scarcity** and **Industry** are keyboard-cycle only,
joining **Supply**, **Reach** and **Supply-routes** off the strip — the off-strip
status is purely a width call, not a data gate. The **resource/good selector** —
needed by the Resource, Market and Scarcity lenses — lives in the lens chrome
region, not on this bar, so the bar carries glyphs only.

The row is sized by the roster, not by a written count: `draw_overlay_controls`
deduces the array extent, so a lens leaving the bar re-numbers the rest with
nothing to keep in step. The bar was eight wide before Opportunity and Production
were retired (LENSES.md § Rung applicability).

The bar is `draw_overlay_controls(ui, x, top_y, w)` in `src/ui/overlay.hpp`,
called from the minimap block in `src/core/app.cpp` rather than from a
standalone window. The lens *rendering* lives inside each canvas's own draw
pass, keyed by `ui_state::overlay` (`body_surface_canvas.cpp` and friends) —
`overlay.cpp`'s `draw_canvas_overlay` hook renders nothing and is an unused
extension point. The active lens is named by the bar's glyph highlight +
tooltip, not an on-canvas chip. (The zoom-ladder navigation lives in the
body / minimap clicks described above, **not** in these controls.) See
[SELECTION.md](SELECTION.md) for the paired Selection element design.

### Lens chrome (the minimap header, top right)

The minimap's **header** is the home for everything a lens puts on screen beside
the canvas: the shared **resource/good selector** for the Resource / Market /
Scarcity lenses (`draw_lens_resource_combo`) and whichever **key** the active lens
draws. Ben, 2026-08-24: *"This selection element for lenses should always fit in
the header for the minimap, at the top right corner."* One region serves the whole
roster, since a lens draws at most one key.

The region takes the minimap's x and width, so the two read as one stack of chrome
flush to the right screen edge. Its **bottom edge is the minimap's top edge** and
it grows **upward** into the column's otherwise-unused space, stopping one margin
below the time panel. `ui::lens_chrome_rect` (`shell_metrics.hpp`) owns that
algebra; nothing here re-derives it, and no legend is handed a position.

Growing upward from a fixed bottom is what keeps a collapsible key's header — and
therefore its toggle — sitting on the minimap's edge whether the list is open or
shut. [LENSES.md](LENSES.md) § Legend placement owns the rest: which keys collapse,
which draw open, and why the z-order patch that used to prop one of them up is no
longer needed.

```
┌───────────────────────┐
│  (rows / gradient bar)  │   ← the active lens's key, growing upward
│  [ good selector ]      │   ← shared combo, when the lens takes one
│  ^ Countries  (44)      │   ← header bar: the toggle, fixed on the edge below
├───────────────────────┤
│  Kepler        (title)  │   ← the minimap box begins here
```

---

## Star as an entity

The Solar title needs a star name, so the **star is a real body entity**:

- `body_type::star` is a `body_type` enumerator (`src/world/components.hpp`).
- `make_hard_coded_world` creates a star entity at the system centre
  (`orbital_radius_au = 0`, `parent = null_entity`, stationary), with a `name`;
  `world.star_body` points at it.
- The solar canvas reads the star entity for its name and position instead of
  hard-coding the centre, and exposes the name to the minimap title.

Making the star an entity (rather than a loose `world.star_name` string) keeps it
uniform with every other body, lets it carry a title and be selectable. Its name
is in the same invented register as every other body name.

---

## Sizing and placement

Unchanged from `CANVASES.md` (authoritative there), with the chrome accounted for:

- `mm_w = max(336, 0.28 × min(window width, height))`; `mm_h = mm_w × 0.75` (4:3)
  governs the minimap box; the title bar and lens mode bar each take a
  fixed-height strip (top and bottom respectively) and the inset canvas fills
  the remaining height between them.
- Anchored bottom-right with an 8 px margin.
- In-canvas labels/title suppressed below ~320 px on the shorter edge; the chrome
  title bar and lens mode bar are exempt (always shown).
- **Coupling:** the bottom strip's `selection_band_height` is **derived from the
  minimap height** — `minimap_height + chrome_margin` in `foldout_column.cpp` —
  so the Selection band and comms dock top-align with the minimap by
  construction. A minimap sizing change ripples into the whole bottom strip; do
  not break this derivation by hard-coding either side.

---

## Selection / view state

A three-rung ladder needs a level, not a binary `surface_is_primary` flag.
`ui_state` carries:

```cpp
enum class canvas_level { solar, circumplanetary, planetary };

canvas_level primary_level = canvas_level::solar; // which rung fills the window
// active_body drives the lower rungs:
//   circumplanetary → active_body's planet (active_body itself if it is a planet,
//                      else active_body.parent if it is a moon — circumplanetary_anchor)
//   planetary       → active_body
```

The minimap's level is derived as one rung *out* from `primary_level` (with the
Solar top rung showing the galaxy instead of a canvas). Pan/zoom view fields
stay per-canvas (`solar_*`, `circum_*`, `planetary_*`) and apply only when that
canvas holds the primary slot; the minimap always renders the default framing.

---

## Open questions

- **Lens bar width ceiling.** Eight glyphs fitted the bar without widening the
  minimap or adding a second row, so the six it now carries have room to spare. A
  *ninth* on-screen lens reopens the question.
- **Circumplanetary framing** for a planet with many vs. zero moons — how much
  local space to show, and at what scale, is for `CIRCUMPLANETARY.md`.
- **Overlays.** Does the minimap mirror supply routes / units on the canvases,
  and is that what the lens mode bar selects?

---

## Related

- `CANVASES.md` — the ladder overview, shared drawing path, sizing, and the navigation model.
- `SOLAR.md`, `CIRCUMPLANETARY.md`, `PLANETARY.md` — the three rungs.
- `LAYOUT.md` — placement in the shell.
- `LENSES.md` — the full lens family, each mode's surface and key; the minimap
  bar surfaces a curated subset of it.
- `SELECTION.md` — the Selection element (BL-093).
