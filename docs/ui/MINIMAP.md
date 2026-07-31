# Project Io — Minimap

The **minimap** is a fixed inset in the bottom-right corner of the shell. It shows
a **neighbouring canvas at reduced scale** and is the player's "you are here, in
the bigger picture" readout and the control for stepping *out* one zoom level.

Historically the minimap was one half of a binary swap between two canvases
(Solar ⇄ Planetary). Layer 2's introduction of a third canvas — the
**Circumplanetary** view — turns that pair into a three-rung **zoom ladder**, so
the minimap is no longer a simple toggle. This document is the authoritative
spec for the minimap's chrome and navigation; the canvases it frames are
specified in [SOLAR.md](SOLAR.md), [CIRCUMPLANETARY.md](CIRCUMPLANETARY.md) *(to
be written)*, and [PLANETARY.md](PLANETARY.md). See [LAYOUT.md](LAYOUT.md) for
placement in the shell and [CANVASES.md](CANVASES.md) for the shared drawing path.

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
(zoom in). The **Circumplanetary** canvas is the new middle rung: a planet drawn
at the centre with its moons orbiting it and the immediate local space around it
(stations, and later, traffic). A planet with no moons is still a valid
circumplanetary view — it is the natural stepping stone between picking a planet
out of the system and dropping to its surface.

---

## Navigation model — *minimap is context*

The rule is deliberately asymmetric and simple:

- **The minimap always shows the rung one step *out* (zoom-out) from the primary.**
  It is pure context — "where does the thing I'm looking at sit in the larger
  view." It is never the place you drill *into*.
- **You descend by clicking a body in the primary canvas**, not by clicking the
  minimap. A body click navigates the primary *down* one rung.
- **You ascend by clicking the minimap.** A minimap click promotes the zoom-out
  neighbour it is showing to primary.

### Descending (click a body in the primary)

| Primary | Click… | Primary becomes |
|---|---|---|
| Solar | a **planet** | that planet's **Circumplanetary** view |
| Solar | a **moon** | the **parent planet's** Circumplanetary view, with the moon highlighted |
| Circumplanetary | the **planet** or one of its **moons** | that body's **Planetary** surface |
| Planetary | — | (bottom rung; nothing to descend into) |

This generalises the original idea — "click a planet, get its circumplanetary
view; the moon shows its parent's circumplanetary view" — and folds the
old solar→surface jump into a two-step drill (system → local → surface) that
always reads the same way.

### Ascending (click the minimap)

| Primary | Minimap shows | Click minimap → primary becomes |
|---|---|---|
| Planetary | Circumplanetary (parent planet) | Circumplanetary |
| Circumplanetary | Solar | Solar |
| Solar | *(no rung above — see below)* | — |

### The top rung (Solar primary)

Solar has no zoom-out neighbour, so the "context" slot has no canvas to show.
Instead, when Solar is primary the minimap displays the **game name**
(`Project Io` for now) as a branding placeholder: the title bar shows the name
and the inset is a plain dark fill. It is **non-interactive** — there is no rung
to ascend to. The player still descends from here by clicking a body in the
Solar primary.

---

## Minimap chrome

The minimap is framed by its own chrome — a **title bar** above the inset
canvas and a **lens mode bar** below it — so it reads as a deliberate panel
rather than a floating thumbnail. The lens controls briefly lived in a
bottom-left overlay control strip; BL-093 moves them back onto the minimap
itself, this time as a compact glyph bar rather than the old mode-bar dots
(see *Overlay controls* below).

```
┌─────────────────────────┐
│  Kepler        (title)  │   ← title bar: the viewed body, or the star name
├─────────────────────────┤
│                         │
│   [ inset canvas ]      │   ← the zoom-out neighbour, drawn at reduced scale
│                         │
├─────────────────────────┤
│ [Co][Ctr][Rs][Mk][Pop][Op][Pr][Cn] │ ← lens mode bar: 8 glyph buttons
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
- Primary **Solar** → minimap shows branding → title is the **game name**
  (`Project Io`).

The title bar is part of the minimap chrome, not the in-canvas title, so it is
**always shown** — including below the ~320 px threshold at which the *in-canvas*
title/labels are suppressed for clutter. The two should not both draw; when the
minimap chrome owns the title, the inset canvas suppresses its own.

### Overlay controls (back on the minimap — BL-093)

The overlay-lens toggles briefly moved to a bottom-left overlay control strip;
BL-093 (Selection element redesign + lens strip relocated to the minimap)
brings them back onto the minimap, this time as a **lens mode bar** running
along the bottom of the minimap box, under the inset canvas. This reverses the
earlier "moved off the minimap" note below — the minimap box is now three
tiers: **title bar** (top) → **inset canvas** (middle) → **lens mode bar**
(bottom).

The bar is a single row of **8 lens glyphs**: **Corp, Country, Resource,
Market, Population, Opportunity, Production, Continent** — single-select with a
null state (clicking the active glyph clears the lens). The eighth glyph is
BL-226 (continent lens, 2026-07-30), the first addition to BL-093's row of
seven. This is a curated subset of the full lens family in
[LENSES.md](LENSES.md); **Scarcity** and **Industry** are keyboard-cycle only,
joining **Supply**, **Reach** and **Supply-routes** off the strip (Layer 5 has
since shipped — the off-strip status is now purely a width call, not a data
gate). The **resource/good selector** — needed by the Resource, Market and
Scarcity lenses — was briefly a popup button on this bar; **BL-134**
(2026-07-09) moved it into the **on-canvas lens legend** (see below), so the
bar carries glyphs only.

The bar is `draw_overlay_controls(ui, x, top_y, w)` in `src/ui/overlay.hpp`,
called from the minimap block in `src/core/app.cpp` rather than from a
standalone bottom-left window. The lens *rendering* lives inside each canvas's
own draw pass, keyed by `ui_state::overlay` (`body_surface_canvas.cpp` and
friends) — `overlay.cpp`'s `draw_canvas_overlay` hook itself still renders
nothing and remains an unused extension point (corrected 2026-07-31; the old
"renders nothing until later layers" reading is now false of the lenses, true
only of that hook). The active lens is named by the bar's glyph highlight +
tooltip, not an on-canvas chip. (The zoom-ladder navigation lives in the
body / minimap clicks described above, **not** in these controls.) See
[SELECTION.md](SELECTION.md) for the paired change to the Selection element
that shipped alongside this relocation in BL-093.

### Lens legend (folds out from the minimap's left edge)

The on-canvas **lens key** — the swatch legend for the field lenses (Resource,
Market, Production, Opportunity, Population, Scarcity, Industry) — no longer sits
against the canvas *left* edge. It now anchors **flush-left of the minimap**: its
right edge meets the minimap's left edge and it is vertically centred on the
minimap, so it reads as a drawer folding out from the minimap's left side. `app.cpp`
passes the render pass a `lens_key_anchor` derived from the minimap rect. This also
clears the widened Selection / ledger fold-out column the legend would otherwise
have overlapped. Since BL-134 (2026-07-09) the legend also hosts the shared
**resource/good selector** for the Resource / Market / Scarcity lenses
(`draw_lens_resource_combo`, `body_surface_canvas.cpp`) — it is no longer on the
lens bar.

---

## Star as an entity

The Solar title needs a star name, and the star currently has none — the solar
canvas draws it as a hard-coded yellow circle at the centre with no backing data.

The chosen approach is to make the **star a real body entity**:

- Add `body_type::star` to the `body_type` enum (`src/world/components.hpp`).
- Create a star entity in `make_hard_coded_world` at the system centre
  (`orbital_radius_au = 0`, `parent = null_entity`, stationary), with a `name`.
- The solar canvas reads the star entity for its name and position instead of
  hard-coding the centre, and exposes the name to the minimap title.

Making the star an entity (rather than a loose `world.star_name` string) keeps it
uniform with every other body, lets it carry a title and — later — anchor the
Circumplanetary ladder or become selectable. **The star's name is still TBD**
(the system is "a loose approximation of Sol" with original body names — Cinder,
Kepler, Selene, …; the star wants a name in the same register). This is a small
code/data change that this UI work depends on; it is tracked as a follow-up, not
done in this doc.

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
- **Coupling (2026-07-31 note):** the bottom strip's `selection_band_height` is
  **derived from the minimap height** — `minimap_height + chrome_margin` in
  `foldout_column.cpp` — so the Selection band and comms dock (BL-227) top-align
  with the minimap by construction. A future minimap sizing change ripples into
  the whole bottom strip; do not break this derivation by hard-coding either side.

---

## Selection / view state

A three-rung ladder needs a level, not the old binary `surface_is_primary` flag.
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
Solar top rung showing game branding instead of a canvas). Pan/zoom view fields
stay per-canvas (`solar_*`, `circum_*`, `planetary_*`) and apply only when that
canvas holds the primary slot; the minimap always renders the default framing.

---

## Open questions

- **Lens bar width ceiling — answered (2026-07-30, BL-226).** The eighth glyph
  (Continent) fit the existing bar without widening the minimap or adding a
  second row; goldens re-blessed. A *ninth* on-screen lens would reopen the
  question.
- **Circumplanetary framing** for a planet with many vs. zero moons — how much
  local space to show, and at what scale, is for `CIRCUMPLANETARY.md`.
- **Overlays.** Once supply routes / units land on the canvases, does the minimap
  mirror them, and is that what the lens mode bar selects?

---

## Related

- `CANVASES.md` — the ladder overview, shared drawing path, sizing, and the navigation model.
- `SOLAR.md`, `CIRCUMPLANETARY.md`, `PLANETARY.md` — the three rungs.
- `LAYOUT.md` — placement in the shell.
- `LENSES.md` — the full lens family, each mode's surface and key; the minimap
  bar surfaces an 8-lens curated subset of it (BL-226 added Continent).
- `SELECTION.md` — the Selection element redesign (BL-093) that shipped
  alongside this relocation.
