# Project Io — Circumplanetary Screen

The Circumplanetary screen is the **middle rung** of the canvas ladder: a
top-down view of a single planet and the space immediately around it — its
moons, and (later) stations and local traffic. It sits between the Solar screen
(the whole system) and the Planetary screen (one body's surface). See
[CANVASES.md](CANVASES.md) for the layout rules shared across the three canvases.

---

## What the user sees

The **anchor** planet sits at the centre. Its moons orbit it at positions
derived from their `orbital_radius_au` and live `orbital_angle_rad`, with an
orbital ring per moon. The anchor and its moons are labelled. The selected body
(`active_body`) carries a selection outline — so when the player descended from
the Solar screen by clicking a moon, that moon is highlighted here.

A planet with **no moons** is still a valid circumplanetary view: the planet sits
alone at the centre. The rung is the deliberate stepping stone between picking a
planet out of the system and dropping to its surface, so it always exists for any
descendable body.

At Layer 2, this canvas communicates:

- Which planet is in focus and what moons it has
- Which body is currently selected (the Planetary screen target)
- Body type at a glance via colour

Stations, local traffic, and orbital infrastructure are added in later layers as
overlays.

---

## The anchor

The view centres on the **circumplanetary anchor** resolved from `active_body`:

- If `active_body` orbits the star directly (a planet or belt asteroid), the
  anchor is that body.
- If `active_body` is a **moon**, the anchor is its **parent** planet, and the
  moon is shown highlighted in orbit.

This is the free function `circumplanetary_anchor(const world&, entity_id)`,
shared with `app::render()` (which uses it to title the minimap with the anchor's
name). If `active_body` is unknown, the canvas shows a "No body selected" notice.

---

## Visual elements

| Element | Description |
|---|---|
| Background | Near-black, a touch warmer than the Solar screen: `(10, 12, 22)` |
| Anchor planet | Filled circle at the centre, drawn with the body style for its type, enlarged relative to its solar-view size so the local view reads as "zoomed in". |
| Moon | Filled circle at its orbital position. Colour `(148, 145, 140)` grey (the shared moon style). |
| Orbital rings | Thin circle at each moon's `orbital_radius_au` from the anchor. Colour `(38, 42, 52)` — structural only. |
| Body label | Anchor and moon names in the small default font, below each circle. White. |
| Selection indicator | Unfilled circle around `active_body`, 3 px larger than its radius. White. When a moon overlaps the anchor under the cursor, **only one** highlights — the nearest centre wins (anchor on an exact tie); a hit-test pass resolves the single hovered body before drawing, matching the Solar canvas. |
| Hover tooltip | Body name, type string, and orbital radius (from the anchor for moons). |
| Scale bar + zoom slider | Bottom-centre overlay (primary view only), identical to the Solar canvas's — the shared `ui::draw_scale_zoom_overlay` (`src/ui/canvas_scale.hpp`). The zoom slider runs **right = zoomed in, left = zoomed out**. |

Moon orbital radii are **not** true scale — real moon distances would render on
top of the planet — they use a visible offset, consistent with how moons are
drawn on the Solar screen.

---

## Coordinate mapping

```
canvas_centre  = top_left + size * 0.5          // the anchor sits here
max_moon_au    = maximum moon orbital_radius_au about the anchor (>= a small floor)
scale          = (min(size.x, size.y) * 0.40) / max_moon_au   // 0.40 leaves margin

moon_screen_pos.x = canvas_centre.x + cos(orbital_angle_rad) * orbital_radius_au * scale
moon_screen_pos.y = canvas_centre.y - sin(orbital_angle_rad) * orbital_radius_au * scale
```

The y-axis is negated so angle 0 is to the right and angles increase
counter-clockwise, matching the Solar screen's orientation. Pan and zoom apply on
top of this when the canvas is primary; the minimap always shows the default
framing.

---

## Interaction

- **Hover** a body circle: show tooltip.
- **Left-click a body — descend (zoom in).** When the Circumplanetary screen is primary, clicking the anchor planet or one of its moons sets `active_body` and drills the primary down to that body's **Planetary** surface.
- **Click the Circumplanetary minimap — ascend.** When the Circumplanetary screen is the minimap (i.e. the Planetary screen is primary), any click promotes it to primary.
- **Pan and zoom (primary view only).** Middle mouse button pans; scroll wheel zooms, anchored at the cursor. A bottom-centre **scale bar + zoom slider** (shared with the Solar canvas) sets the same factor — dragging **right zooms in**. Element sizes stay fixed; only the framing scales. View state (`circum_zoom`, `circum_pan_x/y`) lives in `ui_state`. The minimap always renders the default framing.
- Input is only processed for the canvas the mouse is over; an ImGui panel under the cursor takes precedence.

---

## What is deferred

| Item | Deferred to |
|---|---|
| Stations and orbital infrastructure | Later layers |
| Local traffic / convoys in transit | Layer 5 (supply routing) |
| True-scale or selectable orbit framing | Post-prototype |
| Market / faction overlays | Layer 4+ |
