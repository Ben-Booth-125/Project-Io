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
> implementation; the Corporation section was the first fully settled. The
> remaining four sections (Supply, Market, Faction, Resource) are **settled by
> the lens-design Brief** to the same depth — per-lens specification, rung
> applicability, glyph, legend, and interaction notes. Where a lens depends on
> data or geometry not yet generated (Supply routes, Market prices), the spec
> is the *target*; the build is gated on that dependency and noted per lens.

---

## Rung applicability

Which lenses are meaningful on each rung of the ladder. "—" = no representation
intended; "✓" = built and active; "(later)" = a representation is specified here
but its build is gated on a dependency (named in the lens section).

| Lens | Solar | Circumplanetary | Planetary |
|---|---|---|---|
| Supply | (later) inter-body route lines | (later) per-body throughput badge | (later) per-tile route + stockpile |
| Market | — | ✓ per-body price strip | ✓ per-body price wash |
| Faction | — | — | ✓ tile tint + owner borders |
| **Corporation** | — | — | **✓ tile tint + player border** |
| **Resource** | — | — | **✓ deposit-density tint + gradient key** |

**Resource** is **built** (2026-06-16): unlike Supply and Market it had **no data
dependency** (tile `resource_deposit` is already generated), so it landed directly as a
Planetary render pass. See its section for the full spec.

Interaction notes shared by all lenses: lenses are **Planetary-first** in this
prototype, single-select (one `overlay_mode` at a time), and do not yet propagate
to the minimap. Selecting the already-active lens clears back to
`overlay_mode::none` (`toggle_overlay`). Coarser Solar/Circumplanetary
representations, where specified, are additive render passes guarded behind the
same `overlay_mode` — they do not change the Planetary behaviour.

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

## Faction lens

**Intent.** Read the map as a *political landscape*: which nation holds which
tile, and where the borders between them fall. This is the national counterpart
to the Corporation lens — territory, not corporate holdings.

**Ownership definition.** A tile's owner is its nation entity id, derived from the
`world` tile→nation ownership map (stored off-tile so the nation and tile-tuning
groups stay disjoint; see [NATION_GENERATION.md](../generation/NATION_GENERATION.md)).
Unclaimed tiles have no owner.

**Rung.** Planetary only. No Solar or Circumplanetary representation is intended:
nations are sub-body political units and have no coherent expression on the
inter-body rungs. The render pass is guarded behind `overlay_mode::faction` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).

**Colour.**
- **Claimed tiles** are tinted their owning nation's identity colour
  (`palette::nation_colour`, keyed by nation entity id), a direct replacement of
  the terrain hue.
- **Borders:** a dark stroke is drawn on every hex edge shared with a neighbour of
  a *different* owner — including the claimed/unclaimed boundary — so contiguous
  territory reads as a filled region with a hard outline.
- **Unclaimed tiles** keep their plain terrain hue with no tint.

**Glyph.** A downward-pointing shield silhouette with a dark outline
(`icons::faction`). Distinct from the corporation seal-square and the resource
strata.

**Legend.** Named by the strip glyph highlight and its hover tooltip
(`overlay_mode_name` → "Faction territory"); no on-canvas colour key yet. A
per-nation colour key is a follow-up shared with the Corporation lens (both want
the same identity-swatch list).

**Interaction notes.** Planetary-only, single-select. Borders are recomputed at
draw time from the neighbour ownership comparison; no border data is persisted.

## Supply lens *(target spec — gated on Layer 5 route geometry)*

**Intent.** Read the map as a *logistics network*: where goods move, along which
routes, and where throughput concentrates. The economic counterpart to Faction's
political read.

**Rung applicability.** Supply is the one lens with a meaningful representation on
*every* rung, because logistics span the zoom ladder:
- **Solar** — **inter-body route lines** between bodies that trade, weighted by
  throughput (line thickness/opacity).
- **Circumplanetary** — a **per-body throughput badge** summarising goods in/out
  for the focused body.
- **Planetary** — **per-tile** route segments and the producing/consuming tile's
  stockpile state.

**Colour.** Route lines and badges use a single neutral logistics hue
(`palette` entry, not per-faction) modulated by throughput; the lens is about flow
volume, not ownership. Tiles are not tinted — supply annotates, it does not
re-skin terrain.

**Glyph.** Two parallel horizontal lines — a route/convoy shorthand
(`icons::supply`). Distinct from the market vertical bars.

**Legend.** Strip glyph highlight + tooltip ("Supply routes"). A throughput
scale-key (line weight → goods/tick) is part of the build when routes render.

**Interaction notes.** Propagates across all three rungs (the exception to the
Planetary-first default). **Gated on Layer 5**: no convoy/route geometry is
generated yet, so nothing renders today. No interim Planetary-only stub is built —
the lens waits for its data.

## Market lens *(built 2026-06-16 — price resolution landed in v0.0.4)*

**Intent.** Read the map as a *price surface*: where a good is dear or cheap, and
how scarcity varies across markets. The complement to Supply — Supply shows flow,
Market shows the prices that drive it.

**Rung applicability.**
- **Solar** — none; prices are per-body-market and have no inter-body surface.
- **Circumplanetary** — a **per-body price strip** for the anchor body's market (a
  compact good→price list, the selected good highlighted).
- **Planetary** — a **body-wide price wash** for the player-selected good (dear =
  warm, cheap = cool). Markets are per-**body** (`market_component`), not per-tile, so
  the tint is uniform across the body — a property of the body's market, not of each
  tile.

**Colour.** A diverging warm↔cool gradient keyed to **price relative to the good's
own base (floor) price** (`price / base_price`); neutral mid-tone at the floor ratio
1.0, trending cool below and warm above, across the market's `[0.25×, 4×]` clamp band.
*(Refinement from the original "relative to the body mean": a basket mean across
resources of very different base prices is not meaningful — the per-good floor ratio is
the scarcity signal.)* As with Resource, the player picks which good the surface shows
(a good selector shared with the Resource lens's resource picker — one `lens_resource`).

**Glyph.** Three ascending bars — a price-chart silhouette (`icons::market`).
Distinct from the supply horizontal pair and the resource horizontal strata
(market bars are vertical and outlined).

**Legend.** Strip glyph highlight + tooltip ("Market prices") plus a diverging
gradient key (cheap↔dear) and the selected good's name when the per-tile surface
is active.

**Interaction notes.** Single-select. Price resolution landed in v0.0.4, so the lens
is built: the Circumplanetary strip and the Planetary wash share the resolved
`market_component.price`.

**Implemented 2026-06-16** (Market lens render Brief; design confirmed in the batch-close Q&A).
Planetary wash in `body_surface_canvas.cpp` (`diverging_colour`, composited over terrain at ~0.55
alpha); the per-body strip in **`circumplanetary_canvas.cpp`** — the Circumplanetary rung, *not*
`solar_system_canvas.cpp` as the Session-2 handoff's file list said (Solar has no market surface).
The good-selector is the shared combo from the Resource lens (`overlay.cpp`, bound to
`ui_state.lens_resource`). On-canvas keys/strip are inset past the nav rail (`nav_pane_width`) and
vertically centred. Verified by `scripts/verify/market_lens.lua`, which runs `verify.econ_step(12)`
to diverge prices from base before capture (and a new `verify.show_panel("economy", false)` hook to
clear the panel `econ_step` opens), against blessed goldens.

**Toward per-tile variation.** The wash is body-wide because the present world seeds **one market
per body**. The data model now supports **multiple tile-centred markets per body** — each market
carries a `centre_tile`, and a tile clears against the nearest centre's market (its *catchment*,
`market_for_tile` in `src/world/market_clearing.cpp`); landed 2026-06-16. When generation seeds
genuinely multiple centres (from capitals / population centres — owed, follows the population
layer, OPENS § Trade), this lens should tint **each market's catchment** rather than the whole
body, regaining a spatial per-market surface.

## Per-lens selection validity & routing (settled 2026-06-15, [F4])

The active lens does not only re-skin the canvas — it also **defines what the pointer resolves
to**. Each lens answers "what is the meaningful target under this pointer?" differently, so the
same position resolves to a different entity (and routes to a different ledger) per lens. The
Selection element walks the kind stack (SELECTION.md) and returns the first entity this lens calls
valid:

| Lens | Valid target under the pointer | Routes selection to |
|---|---|---|
| **none** | the lowest drawn entity (marker, else tile) | Tile Ledger |
| **Corporation** | the **owning corporation** of the tile/building | Balance Ledger |
| **Faction** | the **owning nation** of the tile | Nation ledger |
| **Resource** | the tile's **deposit** profile | Tile Ledger (deposit detail) |
| **Market** | the body's **market** / the listing under the pointer | Market Ledger |
| **Supply** *(Layer 5)* | the **route segment / stockpile** under the pointer | (Supply surface) |

A lens skips kinds it does not validate: under the Corporation lens a hovered *building* resolves
*through* to its owning corporation, not to the building, because the corporation is the lens's
unit of meaning. Under no lens that same building resolves to itself. This is the per-lens
*validity* function the resolution rule consumes; the gating on data/geometry per lens (Market,
Supply) applies here too — a lens whose data does not yet exist contributes no valid target.

## Resource lens *(settled — next to build; no data dependency)*

**Intent.** Read the map as a *deposit-density surface*: where the body's mineral
and material wealth concentrates, so the player can site extraction before any
economy exists. This is the lens that pays off *now* — it needs no simulation,
only the tile generation already in place.

**Data definition (settled).** Every tile carries a `resource_deposit` profile
from generation (see [TILES.md](../economy/TILES.md) and
[TILE_GENERATION.md](../generation/TILE_GENERATION.md)). The lens reads that
profile directly at draw time; **no new data is generated**.

**Two modes (settled).**
- **Highest-value (default).** Each tile is tinted by the **identity colour of its
  single highest-value deposit**, at an **opacity scaled by that deposit's
  magnitude** — so a rich iron tile and a trace iron tile share the iron hue but
  differ in intensity. Tiles with no deposit (below the ambient floor) render plain
  terrain. *(Implementation note: "value" ranks by **deposit richness alone**. The
  spec's richness × **presentation weight** product is deferred — `resource_presentation`
  carries only name/abbrev/colour today, no weight field; adding a 19-entry weight
  table is out of this render Brief's scope. Revisit when a weight lands.)*
- **Single-resource (player-selected).** When the player picks a specific resource
  from the lens's resource selector, every tile is tinted that resource's identity
  colour at an opacity scaled by that resource's deposit magnitude **on that
  tile** (zero where absent → plain terrain). This turns the surface into a
  density heatmap for one good.

**Rung.** Planetary only — deposits are per-tile and have no inter-body surface.
The render pass is guarded behind `overlay_mode::resource` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp), matching the
Corporation/Faction pattern.

**Colour.** Resource identity colours from
[`presentation.hpp`](../../src/ui/presentation.hpp) (`presentation_of(res).colour`),
the same source the resource *pip* uses — so a tile's lens tint matches its pip.
Opacity carries magnitude; hue carries identity. No per-faction colours are
involved.

**Glyph.** Three stacked horizontal strata, widening and deepening in opacity
top-to-bottom — a gradient / deposit-density motif (`icons::resource`, the
`ImU32`-colour overload). Distinct from the supply horizontal *pair* (thin,
full-width, equal), the market *vertical* bars, the faction shield, and the
corporation seal-square; and distinct from the resource *pip* diamond (the
`resource_type` overload) it shares a name with.

**Legend.** Strip glyph highlight + tooltip ("Resource density"). Because this
lens is a true gradient it warrants an on-canvas key the others lack: a **gradient
bar** (sparse → dense) plus, in single-resource mode, the **selected resource's
name and identity swatch**; in highest-value mode, a compact swatch list of the
resources actually present on the body. This is the first lens to specify a
colour key — the per-faction key for Corporation/Faction is still a follow-up.

**Interaction notes.** Planetary-only, single-select. The resource selector is a
lens-local control (proposed: a dropdown in the control strip when the Resource
lens is active, shared in form with the Market good-selector). Default mode
requires no selection. No new data, no tick dependency — buildable immediately.

**Implemented 2026-06-16** (Resource lens render Brief; design confirmed in the batch-close
Q&A). `overlay_mode::resource` Planetary pass in `body_surface_canvas.cpp`: opacity = magnitude
**normalised per body** (against the body's richest deposit, so each body's heatmap auto-scales);
the hue is composited over terrain (`lerp_colour`), not a flat replacement, so density reads. The
lens-local control is a **"Single" mode checkbox + a shared resource combo** bound to
`ui_state.lens_resource` in `overlay.cpp` (the same combo the Market good-selector uses). The
on-canvas key sits at the **left edge, vertically centred, inset past the nav rail**. Verified by
`scripts/verify/resource_lens.lua` against blessed goldens.
