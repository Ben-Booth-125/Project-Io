# Project Io — Solar Screen

The Solar screen is the top-down 2D view of the solar system. See [CANVASES.md](CANVASES.md) for layout rules shared with the Planetary screen (primary/minimap swap, region sizing, shared selection state, implementation approach).

---

## What the user sees

The star sits at the centre. Each body orbits it at a position derived from `orbital_radius_au` and `orbital_angle_rad`. Orbital rings mark each body's distance from the star. Bodies are labelled.

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
| Star | Filled circle at the system centre. Radius: ~18 px at full size (1.5× the planet reference). Colour: `(255, 220, 80)`. No label needed. |
| Orbital rings | Thin circle at each body's `orbital_radius_au` distance. Colour: `(38, 42, 52)` — barely visible, structural only. |
| Body (planet) | Filled circle, radius 7 px. Colour: `(80, 120, 180)` blue-grey. |
| Body (moon) | Filled circle, radius 5 px. Colour: `(148, 145, 140)` grey. |
| Body (asteroid) | Filled circle, radius 4 px. Colour: `(140, 110, 80)` brown. |
| Body (station) | Filled circle, radius 4 px. Colour: `(80, 180, 160)` teal. |
| Body label | Body name in small ImGui default font, drawn just below the body circle. Colour: white. **Planets and asteroids are labelled permanently; moons are labelled only while hovered**, to keep the crowded inner system readable. The label tracks the live body position every frame. Known issue: it shimmers slightly while moving because the default ImGui font is a bitmap atlas with no sub-pixel positioning — see the *Known bugs* note in `docs/development/TODO.md`. |
| Selection indicator | Unfilled circle (outline only) drawn around the selected body, 3 px larger than the body radius. Colour: white. |
| Hover tooltip | Body name, type string, orbital radius in AU. Shown while mouse is over a body circle. |

---

## Coordinate mapping

```
canvas_centre  = top_left + size * 0.5
max_radius_au  = maximum orbital_radius_au across all bodies
scale          = (min(size.x, size.y) * 0.45) / max_radius_au   // 0.45 leaves margin

body_screen_pos.x = canvas_centre.x + cos(orbital_angle_rad) * orbital_radius_au * scale
body_screen_pos.y = canvas_centre.y - sin(orbital_angle_rad) * orbital_radius_au * scale
```

The y-axis is negated so that angle 0 is to the right and angles increase counter-clockwise, matching the conventional 2D maths orientation.

---

## Interaction

- **Hover** a body circle: show tooltip.
- **Left-click** a body circle: set `active_body`. The Planetary screen minimap updates immediately to show the selected body's surface. The canvas does **not** switch to planetary view — the Solar screen stays primary. The player navigates to the Planetary view by clicking the minimap.
- **Click empty space in the minimap** (when Solar screen is minimap): swap to primary.
- Input is only processed for the canvas the mouse is over; an ImGui panel under the cursor takes precedence over both canvases.
- **Pan and zoom (primary view only).** Scroll wheel zooms, anchored at the cursor so the point under the mouse stays fixed; the middle mouse button pans. Positions and orbital rings scale with zoom, but element sizes (body/star radii, labels, selection outlines) stay the same pixel size. The default framing (zoom 1, no pan) is the auto-fit that shows all bodies. The **minimap always renders the default framing** — pan/zoom apply only when the canvas holds the primary slot. View state (`solar_zoom`, `solar_pan_x/y`) lives in `ui_state`.

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
