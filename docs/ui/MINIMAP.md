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
canvas — so it reads as a deliberate panel rather than a floating thumbnail. (It
previously also carried a **mode bar** below the inset for the overlay-lens
toggles; those controls have moved to a bottom-left overlay control strip — see
*Overlay controls* below — and the inset now uses the full height under the
title.)

```
┌─────────────────────────┐
│  Kepler        (title)  │   ← title bar: the viewed body, or the star name
├─────────────────────────┤
│                         │
│   [ inset canvas ]      │   ← the zoom-out neighbour, drawn at reduced scale
│                         │
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

### Overlay controls (relocated off the minimap)

The overlay-lens toggles used to be a thin **mode bar** along the bottom of the
minimap (three dots, one per mode). They now live in a separate **overlay control
strip** pinned to the bottom-left of the shell, running from the nav-rail edge
inward (clear of the centred scale/zoom control). It holds a labelled button per
mode — Supply / Market / Faction — with the active lens highlighted; clicking the
active button clears the overlay. A **default lens** (supply) is active on load
rather than no overlay. The overlay itself is the building block in
`src/ui/overlay.hpp` (an overlay draw pass over each canvas, keyed by
`ui_state::overlay`); the control strip is `draw_overlay_controls` in the same
header. Until later layers add overlay data the draw pass renders nothing — the
active lens is named by the control strip, not an on-canvas chip. (The zoom-ladder
navigation lives in the body / minimap clicks described above, **not** in these
controls.)

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

- `mm_w = max(240, 0.20 × min(window width, height))`; `mm_h = mm_w × 0.75` (4:3)
  governs the minimap box; the title bar takes a fixed-height strip at the top and
  the inset canvas fills the rest beneath it.
- Anchored bottom-right with an 8 px margin.
- In-canvas labels/title suppressed below ~320 px on the shorter edge; the chrome
  title bar is exempt (always shown).

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

- **Overlay control polish.** The overlay lens now toggles from a labelled
  bottom-left control strip (Supply / Market / Faction), having moved off the
  former minimap mode-bar dots. The button labels may want icons or richer state
  once the lenses draw real data.
- **Circumplanetary framing** for a planet with many vs. zero moons — how much
  local space to show, and at what scale, is for `CIRCUMPLANETARY.md`.
- **Overlays.** Once supply routes / units land on the canvases, does the minimap
  mirror them, and is that what the overlay control strip selects?

---

## Related

- `CANVASES.md` — the ladder overview, shared drawing path, sizing, and the navigation model.
- `SOLAR.md`, `CIRCUMPLANETARY.md`, `PLANETARY.md` — the three rungs.
- `LAYOUT.md` — placement in the shell.
