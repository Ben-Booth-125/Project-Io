# Project Io — Circumplanetary Screen

> **Settles:** what the middle rung shows and which body it anchors on · how the
> anchor is resolved when the selection is a moon rather than a planet · whether a
> moonless planet still has this view · how local positions map to the screen ·
> which reference a distance on this rung is measured from · what this rung's
> default framing is.
> **Not here:** the ladder, the click model, the descend and ascend rules, the
> shared view controls and the shared state (CANVASES) · which lenses draw at which
> rung (LENSES) · the rungs either side (SOLAR, PLANETARY) · the inset's chrome
> (MINIMAP).
> **Confused with:** SOLAR.md, CANVASES.md, MINIMAP.md, LENSES.md.

The Circumplanetary screen is the **middle rung** of the canvas ladder: a
top-down view of a single planet and the space immediately around it — its
moons, and stations and local traffic as overlays. It sits between the Solar
screen (the whole system) and the Planetary screen (one body's surface). See
[CANVASES.md](CANVASES.md) for the layout rules, view controls and click model
shared across the three canvases.

---

## What the user sees

The **anchor** planet sits at the centre. Its moons orbit it at positions
derived from their `orbital_radius_au` and live `orbital_angle_rad`, with an
orbital ring per moon. The anchor and its moons are labelled. The **selected**
body carries the highlight — `selected_entity`, not the `active_body` anchor, so
a moon the player picked out on the rung above stays marked here while the view
frames its parent.

A planet with **no moons** is still a valid circumplanetary view: the planet sits
alone at the centre. The rung is the deliberate stepping stone between picking a
planet out of the system and dropping to its surface, so it always exists for any
descendable body.

This canvas communicates:

- Which planet is in focus and what moons it has
- Which body is currently selected (the Planetary screen target)
- Body type at a glance via colour

Stations, local traffic, and orbital infrastructure are overlays on this base.

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
| Selection indicator | Unfilled circle around `selected_entity`, 3 px larger than its radius, through the shared highlight convention (`src/ui/highlight.hpp`). White. When a moon overlaps the anchor under the cursor, **only one** highlights — the nearest centre wins (anchor on an exact tie); a hit-test pass resolves the single hovered body before drawing, matching the Solar canvas. |
| Hover tooltip | Body name, type string, and orbital radius (from the anchor for moons). |
| Scale bar + zoom slider | The shared bottom-centre overlay (primary view only; CANVASES.md § Shared view controls), the same helper the Solar canvas uses. |

Moon orbital radii are **not** true scale — real moon distances would render on
top of the planet — they use a visible offset, consistent with how moons are
drawn on the Solar screen.

**Reference distance is rung-relative.** On the Circumplanetary rung the distance
reference is the **parent body — 0 AU at the parent** — so a moon's surfaced distance is its
distance *from its parent*, the meaningful figure when the view is framed on the parent. This
holds for the hover tooltip and for the **body stat block**: the canvas-aware
`draw_body_summary(const world&, const ui_state&, entity_id)` overload in `entity_summary.cpp`
switches on `ui_state::primary_level` and prints `Dist from <parent>` on this rung (the
single-argument overload keeps the star-referenced line). See SOLAR.md for the Solar-rung
reference.

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
counter-clockwise, matching the Solar screen's orientation. Pan and zoom compose
on top of this framing (CANVASES.md § Shared view controls).

---

## Interaction

The click model, the descend and ascend rules, the shared view controls and input
precedence are settled once for the whole ladder in [CANVASES.md](CANVASES.md)
§ Navigation — the zoom ladder; `circumplanetary_canvas.cpp` is this rung's half
of them. What is particular to this rung:

- **Hover** a body circle: tooltip with the body's name, type, and — for a moon —
  its orbital radius measured from the anchor, not from the star (§ Visual
  elements).
- **A press with no anchor has nothing to act on.** When `active_body` is unknown
  the canvas shows a "No body selected" notice instead of a view (§ The anchor).
- **Default framing** fits the anchor's moons — `max_moon_au` about the anchor
  with a small floor, so a **moonless** planet still frames sensibly rather than
  dividing by nothing (§ Coordinate mapping). This rung's pan/zoom lives in
  `circum_zoom` and `circum_pan_x/y` (`ui_state`).

---

## What is deferred on this rung

**Lenses are not a Circumplanetary question.** [LENSES.md](LENSES.md) § Rung
applicability holds the whole Lens × rung table, this rung's column included, and
it is not restated here — a column copied into a rung doc drifts from the table it
was copied out of.

A **true-scale or selectable orbit framing** stays out: moon radii here use a
visible offset rather than their real distances (§ Visual elements), and the
orbital nodes below are named places, not measured ones.

---

## Orbital nodes and what sits at them (Ben, 2026-11-10)

The rung draws the anchor's part of the orbital graph
(`../economy/SPACE_ASSETS.md` § The orbital graph):

| Element | Description |
|---|---|
| Low and high orbit | Two thin rings around the anchor and around each moon, inside the moon-orbit spacing, the high ring wider. Structural colour, a touch brighter than the moon-orbit rings. |
| Lagrange points | A small diamond at each L1, L2, L4 and L5 of every planet–moon pair shown, placed from the pair's live angles, so it moves with the moon. Labelled on hover. |
| Slot occupancy | Beside each node, a row of small pips — one per slot, filled for a held slot in its holder's identity colour, hollow for a free one. |
| Satellites | A small glyph at its node, in its owner's colour, with its role's icon. |
| Stations | A larger glyph at its node, in its owner's colour, sized by its grid. |
| In transit | A short dashed segment along the link the asset travels, with a marker at its share of the way. |
| Fleets | One mark per nation per node: a pointed hull in the owner's colour carrying the lead unit's hull mark, the fleet's size as a badge. On a ring's upper side, or above a diamond. In transit, on its segment at its share of the way. |
| Debris | A small cluster of shards beside a node whose debris level is above the floor, larger for more; the level is on the node's hover card. |
| Solar storm | A bolt at the upper right of a satellite the month's storm darkens, or a station whose modules it stops (`../EVENTS.md` § Solar storms). |

**Hover** a node: its name, its free slots, and the thrust cost and days from the player's home
surface. **Hover** a satellite or station: its owner, class or role, and state. **Press** a satellite
or station: it becomes `selected_entity`, and the Selection band shows its card; double-press on a
station opens the Space ledger's designer on it. A node is not a selectable entity.

**Space units** draw at nodes and in transit in the same idiom (`../military/SPACE_FLEETS.md`).
**Hover** a fleet: the lead unit's card — hull, owner, node or transit, Δv budget, propellant, hull
points. **Press** it: the lead unit becomes `selected_entity`, and the Selection band carries its
orders (`SELECTION.md` § The space-unit element). A fleet wins the pointer over a satellite or
station, which wins over a body.
