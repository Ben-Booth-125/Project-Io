# Project Io — Solar Screen

The Solar screen is the top-down 2D view of the solar system — the **top rung**
of the canvas ladder. See [CANVASES.md](CANVASES.md) for layout rules shared
across the three canvases (the zoom ladder, context minimap, region sizing,
shared selection state, implementation approach).

---

## What the user sees

The star sits at the centre. Each body orbits it at a position derived from `orbital_radius_au` and `orbital_angle_rad`. Orbital rings mark each body's distance from the star. Bodies are labelled.

**Reference distance is rung-relative (2026-06-15).** On the Solar rung the distance reference is the **star — 0 AU at the centre** (as today), so a body's surfaced distance is its distance from the star. This is the Solar-rung case of the shared rung-relative rule; on the Circumplanetary rung the reference is the parent body instead (see CIRCUMPLANETARY.md). The body stat block (`draw_body_summary`) reads the reference from the current rung rather than hard-coding the star.

The **star is a body entity** (`body_type::star`) at the system centre
(`orbital_radius_au = 0`, no parent, stationary) — not a hard-coded circle. It
carries a `name`, which the canvas labels and which the minimap shows as its
title when the Solar screen is the minimap (see `MINIMAP.md`). The star is drawn
through the same body-draw pass as every other body, with a star style (large,
yellow). It has no Circumplanetary view, so clicking it does nothing.

At Layer 2, this canvas communicates:

- Which bodies exist and where they are in the system
- Which body is currently selected (Planetary screen target)
- Body type at a glance via colour

Economic and military data (supply routes, faction presence, convoy paths) are added in later layers as overlays on this canvas.

---

## Visual elements

| Element | Description |
|---|---|
| Background | Near-black: `(8, 10, 20)` |
| Star | Filled circle at the system centre, drawn from the star body entity. Radius: ~18 px at full size (1.5× the planet reference). Colour: `(255, 220, 80)`. Labelled with the star name. |
| Orbital rings | Thin circle at each body's `orbital_radius_au` distance. Colour: `(38, 42, 52)` — barely visible, structural only. |
| Asteroid belt | A thick, translucent textured **band** between two orbital radii (`world.belt`), drawn over the orbital rings and under the bodies. Rendered as a translucent annulus (a thick ring stroke) with a deterministic scatter of dusty specks for texture. The belt is **not a body**; the notable asteroids within it are separate body entities drawn over the band. See *Asteroid belt* below. |
| Body (planet) | Filled circle, radius 7 px. Colour: `(80, 120, 180)` blue-grey. |
| Body (moon) | Filled circle, radius 5 px. Colour: `(148, 145, 140)` grey. |
| Body (asteroid) | Filled circle, radius 4 px. Colour: `(140, 110, 80)` brown. |
| Body (station) | Filled circle, radius 4 px. Colour: `(80, 180, 160)` teal. |
| Body label | Body name, drawn just below the body circle. Colour: white. **Planets and asteroids are labelled permanently; moons are labelled only while hovered**, to keep the crowded inner system readable. The label tracks the live body position every frame; it stays crisp because the UI font atlas is loaded with horizontal oversampling (the former sub-pixel shimmer is fixed — see `src/ui/fonts.hpp`). |
| Selection / hover ring | Ring drawn around a body via the shared highlight convention (`src/ui/highlight.hpp`): white for the selected body, light blue for the hovered body, amber for a pinned body (pinning not yet wired). 3 px outside the body radius, constant pixel size. When markers overlap the cursor, **only one** body highlights — the nearest centre wins, with body id breaking exact ties (arbitrary but stable); a hit-test pass resolves the single hovered body before drawing. |
| Hover tooltip | Body name, type string, orbital radius in AU. Shown while mouse is over a body circle (the single resolved body). |
| Survey badge | Per-body survey-status glyph at the body's upper-right (survey system, BL-067). **Unsurveyed (`hidden`)**: a dimmed `?` (`icons::unknown`). **In progress (`in_transit` / `scanning`)**: a magnifying glass (`icons::survey_badge`) in cyan, with a `k∕N` revealed-region count drawn beside it while scanning. **Surveyed** (the home planet, the star, or a completed survey): no badge. Tracks the live body position every frame. |
| Scale bar + zoom slider | Bottom-centre overlay (primary view only): a fixed-width scale bar reporting the AU it spans at the current zoom, and a logarithmic zoom slider where **right = zoomed in, left = zoomed out**. Factored into the shared `ui::draw_scale_zoom_overlay` (`src/ui/canvas_scale.hpp`), used by the Circumplanetary canvas too. |

---

## Coordinate mapping

```
canvas_centre  = top_left + size * 0.5
max_radius_au  = maximum orbital_radius_au across all bodies
scale          = (min(size.x, size.y) * 0.45) / max_radius_au   // 0.45 leaves margin

body_screen_pos.x = canvas_centre.x + cos(orbital_angle_rad) * orbital_radius_au * scale
body_screen_pos.y = canvas_centre.y - sin(orbital_angle_rad) * orbital_radius_au * scale
```

`max_radius_au` also includes `world.belt.outer_radius_au` when a belt is present, so the whole band fits the auto-fit framing.

The y-axis is negated so that angle 0 is to the right and angles increase counter-clockwise, matching the conventional 2D maths orientation.

---

## Asteroid belt

The system has a single asteroid belt, held as system-level data (`world.belt`,
an `asteroid_belt` with `inner_radius_au` / `outer_radius_au`) — **not** a body
and not an entity. The canvas renders it as a thick, somewhat translucent
textured ring: a translucent annulus (a thick circle stroke between the two
radii) overlaid with a deterministic, fixed-seed scatter of dusty specks so it
reads as a dust band rather than a solid disc. Speck positions are in AU space,
so the band pans and zooms with the view, and the fixed seed keeps the pattern
still between frames (no flicker).

Within the band sit one or more **notable asteroids** — ordinary
`body_type::asteroid` body entities (currently just Pallas in the prototype) at
radii inside the belt. They are drawn *over* the band in the normal body pass, so
they remain individually hoverable, labelled, and selectable (clicking one
descends to its Circumplanetary view). They carry small tile grids so their
surfaces are explorable like the planets. This is the chosen relationship between
the notable asteroids and the ring: **separate bodies drawn over the band**, not
markers embedded in it.

---

## Interaction

- **Hover** a body circle: show tooltip.
- **Left-click a body — descend (zoom in).** When the Solar screen is primary, clicking a body sets `active_body` and drills the primary down one rung to that body's **Circumplanetary** view. Clicking a **planet** opens the planet's view; clicking a **moon** opens its **parent planet's** view with the moon selected. Clicking the **star** does nothing (it has no Circumplanetary view).
- **Click the Solar minimap — ascend.** When the Solar screen is the minimap (i.e. the Circumplanetary screen is primary), any click promotes the Solar screen back to primary.
- Input is only processed for the canvas the mouse is over; an ImGui panel under the cursor takes precedence over the canvases.
- **Pan and zoom (primary view only).** Scroll wheel zooms, anchored at the cursor so the point under the mouse stays fixed; the middle mouse button pans. A bottom-centre **zoom slider** sets the same factor — dragging **right zooms in**, left zooms out — sharing its bounds with the wheel. Positions and orbital rings scale with zoom, but element sizes (body/star radii, labels, selection outlines) stay the same pixel size. The default framing (zoom 1, no pan) is the auto-fit that shows all bodies. The **minimap always renders the default framing** — pan/zoom apply only when the canvas holds the primary slot. View state (`solar_zoom`, `solar_pan_x/y`) lives in `ui_state`.

---

## Orbital motion

Bodies orbit continuously. Each `body_component` carries an `orbital_angular_velocity_rad_per_day`; `advance_orbits` (see `src/world/orbital_system.hpp`) advances `orbital_angle_rad` each frame by the in-game days elapsed, freezing while the simulation is paused. Star-orbiting bodies derive a plausible speed from their radius via Kepler's third law (`kepler_angular_velocity`), so inner bodies sweep faster than outer ones; moons author their own speed.

### `body_component` orbital fields

- `orbital_angle_rad` (float) — current angular position; the authored value is the phase at world construction, advanced over time by orbital motion. y is negated at draw so angle 0 points right and increases CCW.
- `orbital_angular_velocity_rad_per_day` (float) — angular speed; 0 = stationary.
- `parent` (entity_id) — the body this one orbits; `null_entity` means it orbits the star directly. **Moons set `parent` to their planet** and are composed at draw time (`parent position + own orbit`) so they track the planet as it moves. `orbital_radius_au` and `orbital_angle_rad` are then relative to the parent. Moon orbital radii are *not* true scale — real moon distances would render on top of the planet — they use a small visible offset.

---

## What is deferred

| Item | Deferred to |
|---|---|
| Supply route lines | Layer 5 (supply routing) |
| Market price indicators on bodies | Layer 4 (market) |
| Faction colour coding on bodies | Post-prototype (diplomacy) |
| Convoy entities in transit | Layer 5 |
