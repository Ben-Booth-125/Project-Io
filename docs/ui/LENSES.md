# Project Io — Map Lenses

The **lens system** is the set of data overlays the player toggles over the
canvases from the bottom overlay control strip
([`overlay.cpp`](../../src/ui/overlay.cpp), `draw_overlay_controls`). One lens is
active at a time — the active mode is a single `overlay_mode` enum value
([`ui_state.hpp`](../../src/ui/ui_state.hpp)); `overlay_mode::none` is the plain
canvas. Each lens has one distinct vector glyph in the strip
(see [ICONS.md](ICONS.md) § Map-lens glyphs) and re-skins or annotates the canvas
when active.

This document is the *design* authority for what each lens shows, which rung of
the [canvas zoom ladder](CANVASES.md) it applies to, and how its legend reads.
Glyph shapes live in [ICONS.md](ICONS.md); identity colours live in
[`presentation.hpp`](../../src/ui/presentation.hpp) (the `palette` namespace).

> **Status.** This doc was created alongside the **Corporation lens**
> implementation; the Corporation section is fully settled. The other four
> sections are **stubs** — they record the current behaviour and the intended
> direction, but the full per-lens specification (legend format, multi-rung
> representations) is promoted from the lens-design Brief separately.

---

## Rung applicability

Which lenses are meaningful on each rung of the ladder. "—" = no representation
yet; "✓" = active; "(later)" = a coarser representation is intended but unbuilt.

| Lens | Solar | Circumplanetary | Planetary |
|---|---|---|---|
| Supply | (later) route lines | (later) | (later) per-tile |
| Market | (later) | (later) price summary | (later) per-tile |
| Faction | — | — | ✓ tile tint + borders |
| **Corporation** | — | — | **✓ tile tint + player border** |
| Resource | — | — | (later) deposit density |

Interaction notes shared by all lenses: lenses are **Planetary-first** in this
prototype, single-select (one `overlay_mode` at a time), and do not yet propagate
to the minimap. Selecting the already-active lens clears back to
`overlay_mode::none` (`toggle_overlay`).

---

## Corporation lens

**Intent.** Read the map as a *corporate landscape*: where corporations operate,
who owns what, and how the player's footprint sits against rivals. National
territory is the Faction lens's job — this lens is about **corporate-owned tiles**
first; nation context is deliberately absent.

**Ownership definition (settled).** A *corporate-owned tile* is any tile on which
a corporation holds a building. The mapping is derived at draw time from
`w.corporations[].assets` → `building_component.tile` (a building id resolves to
its tile). **There is no influence radius and no nation-domination heuristic in
this iteration** — only the literal building tile counts as owned. A tile with no
corporate building is *unowned*.

**Rung.** Planetary only. No Solar or Circumplanetary representation — the render
pass is guarded entirely behind `overlay_mode::corporation` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp) and changes
nothing on the other two canvases.

**Colour.**
- **Owned tiles** are tinted their owning corporation's identity colour (a direct
  replacement of the terrain hue, matching the Faction lens's tint convention).
- The **player corporation** (`w.player_entity`) uses
  `presentation::faction_colour(0)` and additionally gets a thin border in that
  same colour so the player's holdings read at a glance against rivals.
- **Rival corporations** use the per-corp hashed faction slot already used for the
  building markers (a multiplicative hash kept off slot 0 so a rival never
  collides with the player's colour).
- **Unowned tiles** render in their plain terrain colour with **no tint** — there
  is no nation underlay in this lens.

**Glyph.** A filled square with a centred inner dot — a "seal" silhouette
(`icons::corporation`; see [ICONS.md](ICONS.md)). Distinct from the
extraction-site filled diamond, the processing-facility plain filled square, and
the port/unit filled triangle.

**Legend.** As with the other lenses today, the active lens is named only by the
strip glyph highlight and its hover tooltip (`overlay_mode_name` →
"Corporation ownership"); no on-canvas colour key yet. A per-corp colour key is a
follow-up shared with the Faction lens.

---

## Faction lens *(stub — current behaviour)*

Tints each tile by its owning nation's identity colour
(`palette::nation_colour`, keyed by nation entity id) and draws dark borders on
every hex edge shared with a neighbour of a different owner (including the
claimed/unclaimed boundary). Unclaimed tiles keep their terrain hue. Planetary
only today. Full spec (legend, coarser Solar/Circumplanetary representations)
deferred. See `body_surface_canvas.cpp` (the faction branch) and
[NATION_GENERATION.md](../generation/NATION_GENERATION.md).

## Supply lens *(stub — not yet rendering)*

Intended to show supply routes / convoy paths: route lines on Solar, price/flow
summaries at Circumplanetary, per-tile supply detail at Planetary. No on-canvas
geometry yet (Layer 5). Glyph: two parallel horizontal lines.

## Market lens *(stub — not yet rendering)*

Intended to show market / price state: price summaries at Circumplanetary,
per-tile market detail at Planetary. Blocked on the economy tick and per-market
price resolution (not yet implemented). Glyph: three ascending bars.

## Resource lens *(stub — proposed, no glyph yet)*

Proposed deposit-density lens: colour tiles by their highest-value deposit, or by
a player-selected resource, with a gradient legend. Data already present (tile
`resource_deposit`); needs a glyph and a render pass. Planetary-first. Not yet
implemented.
