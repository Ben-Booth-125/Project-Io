# Project Io — Solar Screen

> **Settles:** what the system-wide rung shows and how a body's orbit maps to a
> screen position · what the star is as an entity, how it is drawn, and why it is
> the one body a press cannot descend into · how the asteroid belt reads against
> the bodies within it · which reference a distance on this rung is measured from ·
> what this rung's default framing is.
> **Not here:** the ladder, the click model, the descend and ascend rules, the
> shared view controls and the shared state (CANVASES) · which lenses draw at which
> rung (LENSES) · the rung below (CIRCUMPLANETARY) · what the inset frames
> (MINIMAP).
> **Confused with:** CANVASES.md, CIRCUMPLANETARY.md, MINIMAP.md, LENSES.md.

The Solar screen is the top-down 2D view of the solar system — the **top rung**
of the canvas ladder. See [CANVASES.md](CANVASES.md) for layout rules shared
across the three canvases (the zoom ladder, context minimap, region sizing, the
shared view controls, shared selection state, implementation approach).

---

## What the user sees

The star sits at the centre. Each body orbits it at a position derived from `orbital_radius_au` and `orbital_angle_rad`. Orbital rings mark each body's distance from the star. Bodies are labelled.

**Reference distance is rung-relative (2026-06-15).** On the Solar rung the distance reference is the **star — 0 AU at the centre**, so a body's surfaced distance is its distance from the star. This is the Solar-rung case of the shared rung-relative rule; on the Circumplanetary rung the reference is the parent body instead (see CIRCUMPLANETARY.md). The rung-aware read lives in the canvas-aware `draw_body_summary(const world&, const ui_state&, entity_id)` overload in `entity_summary.cpp`, which switches on `ui_state::primary_level`; the single-argument overload keeps the plain star-referenced orbit line.

The **star is a body entity** (`body_type::star`) at the system centre
(`orbital_radius_au = 0`, no parent, stationary) — not a hard-coded circle. It
carries a `name`, which the canvas labels and which the minimap shows as its
title when the Solar screen is the minimap (see `MINIMAP.md`). The star is drawn
through the same body-draw pass as every other body, with a star style (large,
yellow). It has no Circumplanetary view — the one body on this rung a press
cannot descend into (§ Interaction).

This canvas communicates:

- Which bodies exist and where they are in the system
- Which body is currently selected (Planetary screen target)
- Body type at a glance via colour

Economic and military data (supply routes, faction presence, convoy paths) are overlays on this canvas.

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
| Body label | Body name, drawn just below the body circle. Colour: white. **Planets and asteroids are labelled permanently; moons are labelled only while hovered**, to keep the crowded inner system readable. The label tracks the live body position every frame; it stays crisp because the UI font atlas is loaded with horizontal oversampling (see `src/ui/fonts.hpp`). |
| Selection / hover ring | Ring drawn around a body via the shared highlight convention (`src/ui/highlight.hpp`): white for the selected body, light blue for the hovered body; the amber `pinned` tier exists in the convention but no surface sets it (`EXPLORER.md`). 3 px outside the body radius, constant pixel size. When markers overlap the cursor, **only one** body highlights — the nearest centre wins, with body id breaking exact ties (arbitrary but stable); a hit-test pass resolves the single hovered body before drawing. |
| Hover tooltip | Body name, type string, orbital radius in AU. Shown while mouse is over a body circle (the single resolved body). |
| Survey badge | Per-body survey-status glyph at the body's upper-right (the survey system, BL-067). **Unsurveyed (`hidden`)**: a dimmed `?` (`icons::unknown`). **In progress (`in_transit` / `scanning`)**: a magnifying glass (`icons::survey_badge`) in cyan, with a `k∕N` revealed-region count drawn beside it while scanning. **Surveyed** (the home planet, the star, or a completed survey): no badge. Tracks the live body position every frame. This is the **geographic** fog badge — see [`DISCOVERY.md`](DISCOVERY.md). |
| Activity badge | Per-body commercial-activity glyph at the body's **lower-left** — the **activity** fog (the commercial sphere, BL-089), deliberately offset from the survey badge so the two fogs read apart. A concentric pulse (`icons::activity`) coloured by tier: `known` (`palette::activity_known`), `known_stale` (greyed), `visible` (`palette::activity_visible`). **Unknown** bodies and the **home body** (which carries its own presence halo) show no badge. Derived from `body_activity_visibility` (routes + live convoys + ownership + tick). See [`DISCOVERY.md`](DISCOVERY.md). |
| Home halo | An always-on player-identity ring around `home_body` (player presence, BL-085), drawn behind the body — a soft player-blue glow + ring, distinct from the survey/activity badges and the selection highlight. |
| Trade corridors | The player's persistent trade routes (BL-088, persistent trade routes) drawn as lit lanes between endpoint bodies (primary view only): fresh routes glow (`palette::activity_corridor`), stale routes fade to grey. Commercial reach made visible; see [`DISCOVERY.md`](DISCOVERY.md). |
| Convoys | The Supply lens draws a line per live convoy between its endpoint bodies (`w.convoys`, `supply_system.cpp`), primary view only. Inter-body traffic is all this rung carries — the radius-2 convoy **vision beam** is an intra-body layer and belongs to the surface ([`DISCOVERY.md`](DISCOVERY.md)). |
| Scale bar + zoom slider | The shared bottom-centre overlay (primary view only; CANVASES.md § Shared view controls). Its scale bar reports the **AU** it spans at the current zoom — this rung's unit. |

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
`body_type::asteroid` body entities at radii inside the belt. They are drawn
*over* the band in the normal body pass, so they remain individually hoverable,
labelled, and selectable (double-clicking one descends to its Circumplanetary
view). They carry small tile grids so their surfaces are explorable like the
planets. This is the chosen relationship between the notable asteroids and the
ring: **separate bodies drawn over the band**, not markers embedded in it.

---

## Interaction

The click model, the descend and ascend rules, the shared view controls and input
precedence are settled once for the whole ladder in [CANVASES.md](CANVASES.md)
§ Navigation — the zoom ladder; `solar_system_canvas.cpp` is this rung's half of
them. What is particular to this rung:

- **Hover** a body circle: tooltip with the body's name, type, and orbital radius
  in AU from the star (§ Visual elements).
- **The star is the one body a press cannot descend into.** It has no
  Circumplanetary view, so a double-click on it changes nothing. It still
  *selects* — the star is a selectable entity like any other body here.
- **Default framing** is the auto-fit that shows every body at zoom 1 with no pan,
  scaled off `max_radius_au` (§ Coordinate mapping), which takes the belt's outer
  radius into account so the whole band fits. Positions and orbital rings scale
  with zoom. This rung's pan/zoom lives in `solar_zoom` and `solar_pan_x/y`
  (`ui_state`).

---

## Orbital motion

Bodies orbit continuously. Each `body_component` carries an `orbital_angular_velocity_rad_per_day`; `advance_orbits` (see `src/world/orbital_system.hpp`) advances `orbital_angle_rad` each frame by the in-game days elapsed, freezing while the simulation is paused. Star-orbiting bodies derive a plausible speed from their radius via Kepler's third law (`kepler_angular_velocity`), so inner bodies sweep faster than outer ones; moons author their own speed.

### `body_component` orbital fields

- `orbital_angle_rad` (float) — current angular position; the authored value is the phase at world construction, advanced over time by orbital motion. y is negated at draw so angle 0 points right and increases CCW.
- `orbital_angular_velocity_rad_per_day` (float) — angular speed; 0 = stationary.
- `parent` (entity_id) — the body this one orbits; `null_entity` means it orbits the star directly. **Moons set `parent` to their planet** and are composed at draw time (`parent position + own orbit`) so they track the planet as it moves. `orbital_radius_au` and `orbital_angle_rad` are then relative to the parent. Moon orbital radii are *not* true scale — real moon distances would render on top of the planet — they use a small visible offset.

---

## What is deferred on this rung

**Lenses are not a Solar question.** [LENSES.md](LENSES.md) § Rung applicability
holds the whole Lens × rung table, this rung's column included, and it is not
restated here — a column copied into a rung doc drifts from the table it was
copied out of.

**Faction colour coding on bodies** is post-prototype: it waits on diplomacy, and
until nations declare toward one another there is nothing for a body's colour to
report.
