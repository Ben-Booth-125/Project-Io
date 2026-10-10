# Project Io — Planetary Screen

> **Settles:** what the surface rung communicates above the ground · where a building's
> facts are read now that the canvas draws no marker over it · at what grain the surface is drawn and
> selected · how a national border reads without two neighbours blending into a
> third, and what pressing it selects · which channels carry composition and
> landform, and which tiles suppress them · which layers draw in what order and what
> degrades at far zoom · how a press and a hover land on the hex grid.
> **Not here:** how the ground itself is rendered (RENDERING) · the ladder and the
> shared state (CANVASES) · what an overlay shows (LENSES) · what a click then
> offers (SELECTION).
> **Confused with:** RENDERING.md, LENSES.md, CANVASES.md.

The Planetary screen is the tile-grid view of the selected body's surface — the **bottom rung** of the canvas ladder, and the rung play opens on (the corporation's home planet — the app itself opens on the main menu first, see [STARTUP.md](STARTUP.md)). See [CANVASES.md](CANVASES.md) for layout rules shared across the three canvases (the zoom ladder, context minimap, region sizing, shared selection state, implementation approach).

Because it is the bottom rung, the Planetary screen is **only ever primary** — it is never shown in the minimap. Reaching it is a descend click on the Circumplanetary screen; leaving it is a click on the minimap (which shows the Circumplanetary view) to ascend.

---

## What the user sees

A hex tile grid for the selected body. Each tile is a coloured hexagon. Terrain determines colour. Buildings are marked with an overlay symbol on their tile. Hovering a tile shows its data. Pan and zoom let the player navigate large bodies.

This canvas communicates:

- The terrain profile of the selected body
- Where buildings, roads, settlements and rivers are
- Per-tile resource and environment data on hover
- Whatever the active lens overlays ([LENSES.md](LENSES.md))

---

## Tile grid

> **The ground's render mechanism is owned by [RENDERING.md](RENDERING.md)** — baked
> painterly chunks, the C-F art direction, the no-grid rule, installations as rendered
> geometry, animation and LOD. This doc owns the analytic channels above the ground;
> which of them survive, retire or restyle over painterly ground is owned by BL-734
> (ground/chrome layer contract), and until that settles every channel below keeps its
> current spec — except where RENDERING.md's rulings already retire it (the on-ground
> grid, the building/settlement canvas glyphs, the landform glyphs, the river strokes).

**Shape:** Pointy-top hexagons in odd-r offset coordinates. Odd rows are shifted right by half a column. Grid axes: columns (x) run left-to-right, rows (y) run top-to-bottom.

**Target size and aspect ratio:** A body's grid is roughly **9 columns wide for every 5 rows tall** — the height is a little *under* half the width. The reasoning is geometric: the grid width spans the body's full circumference (both hemispheres), so a pole-to-pole height would be half the width; truncating the non-traversable polar caps brings it a little under half. The two planets are standardised to **180 × 84** (columns × rows); **Selene**, as a moon, uses **90 × 42** (the same ratio at half scale). The prototype world's surface bodies are **Cinder, Kepler, Selene, and Pallas** (Pallas, a notable belt asteroid, carries a small grid); **Helios** is the system star and has no surface.

**Horizontal wrap:** column indices wrap at the grid boundary so the east edge connects to the west edge, forming a cylinder. Generation wraps neighbours across this seam, and the Planetary canvas renders the wrap as a seamless infinite side-scroll: panning past either edge continues into tiles drawn from the opposite side (see [Interaction](#interaction)).

---

## Terrain types

A tile's character has **three axes** ([TILES.md](../economy/TILES.md)):

- **Substrate** — what the ground is made of, and never transformed. Ten values
  (`terrain_substrate`, `src/world/components.hpp`): barren, rocky, sedimentary,
  volcanic, metallic, regolith, icy, and three water substrates — ocean (open sea),
  coast (a sea tile with a land neighbour) and lake (water that does not reach the sea).
- **Cover** — what sits on it, and `none` is a first-class answer. Ten values
  (`terrain_cover`): none, grass, scrub, forest, marsh, snow, dunes, ash, salt, urban,
  each graded by `cover_density` (0–255; 0 iff cover is `none`).
- **Landform** — the tile's physical shape. Seven values (`terrain_landform`): plains,
  highland, mountain, canyon, valley, crater, rift. Landform renders on its **own
  channels** — a subtle relief tint (`ui::landform_relief`) plus baked relief forms for
  the dramatic set (§ Terrain channels) — never in the hue.

Substrate and cover **share** the hex's hue: `ui::terrain_colour` (`src/ui/hex_render.{hpp,cpp}`)
is the single colour source of truth, and it blends the substrate's own colour toward a
per-cover endpoint by that tile's density. Sharing one channel is what makes the texture pass
below necessary rather than decorative — two different tiles can arrive at the same green.

Ocean and landmass are derived from the **Continents/Drift tectonic-plate pass**
(`docs/generation/CONTINENTS.md`; the Continent lens renders the plates). Ocean fraction is
an outcome of the plate pass and the body's hydrological state, not a flood-fill target.

### Terrain channels — composition and landform

The three axes render on **two channels** (BL-231, landform channels; BL-232,
bridged runs). Both are **always-on chrome**, not an `overlay_mode`: terrain
identity is not something the player opts into, and landform's
movement-cost multiplier applies whether or not a lens is active.

| Axis | Channel | Source |
|---|---|---|
| **Composition** (substrate + cover) | **Hue** — the flat hex fill | `ui::terrain_colour` |
| **Landform** (its physical shape) | **Relief tint** + **baked landform relief** | `ui::landform_relief`; the bake's relief pass ([RENDERING.md](RENDERING.md) § Mountains, rivers and terrain variety) |

**Why two channels rather than one.** Lens tints composite over the terrain hue at
0.6–0.80 alpha, so a second signal carried *in that hue* is obliterated exactly when a
lens is on. This is the rule the Continent lens's plate boundaries established and it
applies here unchanged: the relief is composited **after** every lens branch, and the
baked relief forms are shape and light, not hue, so a lens wash tints them without erasing them.

**Why the landform channel splits in two.** The measured mix (`world_audit` § S3) decided
it. Plains and valley alone are ~95 % of land tiles, while every dramatic landform is
≤ 1.5 %:

- **Common ground — relief tint.** Plains is the untouched baseline; elevated ground lifts
  toward a warm highlight and sunken ground toward a cool shadow, on a small signed
  ordinal scale (mountain highest → canyon lowest). Deliberately subtle: it must read as
  light on terrain, never as a change of composition.
- **Dramatic landforms — baked relief, no glyph** (Ben, 2026-10-08, the sprint 51 form).
  Mountain, canyon, crater and rift are the ≤ 1.5 % set whose movement cost is ×1.3 or worse,
  so an invisible surprise there is expensive — which is why they once carried a stroke-only
  glyph. The glyph retires from the canvas; the obligation does not. Each dramatic landform
  bakes its own relief form into the ground — massif and ridge, canyon cut, crater bowl, rift
  fissure ([RENDERING.md](RENDERING.md) § Mountains, rivers and terrain variety) — authored to
  read at every rung, and the hover card keeps naming the landform and its movement cost.

**Contiguous runs are bridged.** A run of the same linear landform bakes as **one** continuous
form rather than the same form repeated per tile — mountain as a range, rift as one continuous
fissure, canyon as one cut between paired rims — each tile baking its own half of the shared edge,
so halves meet at the midpoint with no cross-tile state and the survey mask clips cleanly. A lone
tile keeps a centred form. Crater never spans. Contiguity was measured before the render was designed
(`world_audit` § S4): 71% of mountain and 81% of rift tiles have such a neighbour, so bridging
fires on the majority — while **no** tile in the system has all four neighbours, which is why there
is no "filled interior" case.

**The landforms are named where the player looks.** Every tile hover card states
`composition · landform` and, on the plain canvas, the landform's movement cost — a relief
form learnable only by clicking each tile through to the Selection panel is not learnable.
Plains stays unnamed: it is the untouched baseline in both channels.

**Suppression rules.** The baked landform relief is ground, so it is never suppressed: a
structure stands on it (RENDERING.md § Installations) and a lens washes over it like any other
ground. The relief tint composites after every lens branch, as above.

Both channels also render in the Selection band's zoomed tile-neighbourhood view, which is
why they live in `hex_render` rather than in the canvas — one implementation, so the two
surfaces cannot drift. Verified by `scripts/verify/landform_relief.lua`.

---

## Visual elements

| Element | Description |
|---|---|
| Background | Dark: `(18, 18, 24)` |
| Tile | Filled hexagon. Colour from `ui::terrain_colour` (substrate + cover hue), composited with the landform relief tint (§ Terrain types above). A 1 px gap between hexes lets the background show through as a border — achieved by drawing each hex at `circumradius - 1 px` rather than adding explicit borders. |
| Buildings | **No marker.** A building is a structure baked into the ground art (RENDERING.md § Installations) — a cluster of up to three on a stacked tile. Type, count, owner and running state are read from the hover card, the Selection element and the ownership lenses: [§ Building markers](#building-markers) below. |
| Road network | **Always-on** (like terrain, not a lens): the generated road lattice plus player-placed roads render as **continuous, symmetric spans**. Each roaded tile draws its **own half** of every shared road edge — from its centre to the midpoint of the centre-to-neighbour line — toward each roaded, survey-revealed cardinal neighbour; the two tiles' halves meet at the edge midpoint, so a road spans the pair identically whichever tile is "from" (no from/to asymmetry), and a small centre cap rounds junctions and keeps a lone / just-placed road visible. Cylinder-seam edges shift one period to stay short; drawn only toward survey-revealed neighbours, so roads don't leak past the survey fog. Styled by the drawing tile's **tier** — **Track** (`road_level` 1) thin/dim, **Road** (2) medium, **Highway** (3) thick/bright — so a tier change reads as a taper at the midpoint. Spans **dim with the commercial-reach fog**, through the same wash the lens fill takes; a road edge is fogged by the **max** of its two tiles' vision (see [DISCOVERY.md](DISCOVERY.md)). The tier ladder has **no on-canvas key** — it is named contextually in the Selection panel instead (below). Below a 20 px drawn radius each curve is one stroke with no apex joint (RENDERING.md § Roads and sea lanes — the 60 fps budget). **Painted from the third rung up:** at a drawn radius of 20 px and more, a road or lane over baked ground is painted into the ground with a surface per tier (RENDERING.md § Roads and sea lanes) and this drawn network does not draw; it stays, thin, at the two widest rungs, and at every rung on a tile still showing the vector fallback. |
| Road-tier legend | **Contextual, not chrome** (Ben's call, 2026-08-09). The three tiers render by line weight and brightness alone, and roads are always-on terrain rather than a lens, so the per-lens legend drawer cannot carry them. Instead, selecting a roaded tile names its tier beside the coordinates in the Selection panel header — `Tile [x, y] · Highway` — with a hover tooltip giving the thin→thick ladder. A roadless tile shows nothing; no persistent chip is added anywhere. |
| Selection / hover indicator | Hex outline drawn through the shared highlight convention (`src/ui/highlight.hpp`): white for the selected tile, light blue for the hovered tile (per wrap copy), amber for pinned. Precedence is selected > pinned > hovered. |
| Hover card | The shared glance-then-stick hover card ([TOOLTIP.md](TOOLTIP.md)), content **lens-keyed** (`src/ui/hover_content.cpp`). A tile's default variant: `substrate · landform` header (plains unnamed), habitability, and the landform's movement-cost multiplier when not plains. Under the Resource lens: the selected resource's deposit richness; under Population: habitability + workforce cap. Buildings carry their own variant (rival buildings show type + owner only — the competitor-visibility rule, [DISCOVERY.md](DISCOVERY.md)). A market centre has no hover target of its own on this canvas: its tile hovers as any tile does. |
| Body label | Canvas title bar shows the selected body name, type, and grid dimensions. As the Planetary screen is always primary (full size), the title is always shown. A **survey-status suffix** follows it: `UNSURVEYED`, `Survey en route`, or `Surveying k/N` — nothing once surveyed. |
| Survey region mask | On a body whose survey is incomplete, tiles in **unrevealed regions** render as a flat dark "locked" fill `(12, 14, 20)` with no lens tint, borders, markers, selection outline, or hit-testing; revealed regions render normally. Regions reveal in deterministic raster (row-major) order as the survey scans ([DISCOVERY.md](DISCOVERY.md)). A fully surveyed body (the home planet, or a completed survey) shows everything. |
| Settlements | Always-on, not lens-gated: **every** generated population centre is a **settlement structure** baked into the ground (RENDERING.md § Installations) whose footprint and height grow with its scale, and a **razed** centre (BL-624) bakes as a ruin — a ruin is a tile-scale fact. The skyline and ruin glyphs retire from this canvas (Ben, 2026-10-08). Far-zoom legibility is the art's job: a settled region must read as settled at the far page without a glyph. Only **City+** centres (scale ≥ 4) carry a name label. Ownership is never carried by a settlement's colour — tier is carried by the structure. On the plain canvas ownership is read from the national border band; under a lens the band is suppressed, so ownership is not on the canvas at all and is read from the Selection panel. |
| Home-cluster ring | Always-on player-presence chrome: the player's holdings read as "my region" by a ring in the player-identity colour traced round **each tile the player holds** (under every lens; the selection accent under the Corporation lens, where the fill is already the player colour), over a light player-identity wash on the plain canvas. Rival tiles carry no ring. It is drawn per tile, not as one circle round the cluster — the fixed-radius cluster circle retired with BL-329 (corp-reach circle). **No HQ star** — the `ui::icons::hq` origin mark is not drawn on this canvas, for the player or any corporation, plain or under a lens (Ben, 2026-10-10: "We should remove the HQ glyph and market center glyphs"). |
| Market centres | **No marker and no hit zone** (Ben, 2026-10-10: "We should remove the HQ glyph and market center glyphs"). A market is selected from the map through the **Market lens** — a press on its catchment selects it ([SELECTION.md](SELECTION.md) § A lens collapses selection to ONE TIER) — or from the Market ledger. With no lens, a press on a market's centre tile resolves as any other tile or building press. |
| National border band | **Plain-canvas** political chrome, **suppressed while any lens is up** (Ben, 2026-08-28, reaffirmed 2026-09-07): a nation's identity colour sits at its frontier and falls off inwards over three tiles, and clicking the band selects the nation. Unlike roads, it is not always-on — a lens asks one question, and a national wash competes with the answer. See § The national border band below. |
| Rivers | **Baked into the ground** as carved, curved water courses that widen downstream (RENDERING.md § Mountains, rivers and terrain variety). No canvas stroke and no chevron: the width gradient carries the flow direction. Terrain, not a lens; always on. |

---

## Building markers

### A building is a structure in the ground, not a mark over it

**No building marker draws on the Planetary canvas** (Ben, 2026-10-08, the sprint 51 form). What
stands on a tile is a structure stamped into the ground art by the bake's installation pass —
[RENDERING.md](RENDERING.md) § Installations. The silhouette glyph, the segmented stacked-tile
ring, the `+N` count badge and the corp emblem tag all retire from this canvas; their glyphs
survive on the surfaces that still draw them (panels, ledgers, chrome — [ICONS.md](ICONS.md)).

The trade is deliberate. A glyph layer over painterly ground made every built tile read as a
label stuck on a picture, and it carried four answers at once on 48 % of a hex. The art now
answers *that something stands here and roughly what*; every finer question moves to a surface
built to answer it:

| Question the old marks answered | Where it is answered now |
|---|---|
| *What kind of building is this?* (the silhouette) | The structure itself, authored per type; the hover card names it |
| *Which kinds stand here, and which leads?* (the ring, the centre glyph) | A **cluster** of up to three structures, one per stack ([RENDERING.md](RENDERING.md) § Installations), and the tile Selection element's **Production** section ([SELECTION.md](SELECTION.md) § The tile element's layout) |
| *How many in total?* (the `+N` badge) | The Production section, one row per stack with its count |
| *Whose is it?* (the owner-tinted fill, the emblem tag) | The **Corporation** and **Company** lenses' owner multi-select ([LENSES.md](LENSES.md)), the hover card, and — for the player alone — the always-on footprint outline |
| *Is it running?* | The Selection element only. **The art is static**: no smoke, light or state cue distinguishes a running plant from an idle one |

**Ownership is harder to read on the plain canvas, and that is accepted.** The player's own
footprint outline stays always-on (it is the one ownership signal the plain canvas keeps), so
"where am I" survives; "where is everyone else" is a lens question, which is what a lens is for.

**Hit-testing is unchanged.** A built tile still resolves to its building across the whole hex
when it carries exactly one, and to the tile when it carries more (SELECTION.md § Multi-building
tiles). The structure is what is drawn; the hex is still what is pressed.

---

## Province grain — the rendered and selected unit

**The province, not the hex, is what this canvas renders and what a click selects.** A province is
the grown partition cell of [PROVINCES.md](../generation/PROVINCES.md) (`src/world/province.hpp`).
The canvas changes no sizing, partitioning or id layout; it consumes the partition. Building
placement is tile-keyed and the tile does not retire — deposits, terrain, buildings and richness
all remain tile-keyed. Ben, 2026-08-21: tiles *"are just going to be rendered differently, but
still instrumental unit values."*

### The blend

Geometry is per hex — the row-band cull, the wrap window and the fill LOD are the same whether a
hex blends or not. What the blend changes is a hex's **colour**, by one mechanism:

**It is a land-wide field, not a per-province one.** The blend began province-scoped, stopping at
the cell boundary; the stop was removed (Ben, 2026-08-22: *"blur should cross province borders"*),
so a corner now averages with every blending neighbour regardless of which cell it belongs to. The
constant below is named `k_land_blend_strength` for exactly that reason, and the corner mean has no
province-match term to reinstate.

- Each hex is drawn as a **6-triangle fan with per-corner colours** (`prim_blended_hex`), not as a
  flat `AddConvexPolyFilled`. The centre vertex takes the tile's own composited colour.
- A corner takes the **mean of the tile and the blending neighbours sharing that corner**, then
  travels back toward the tile's own fill by `1 − k_land_blend_strength`. Each corner falls between
  exactly two of the six sides, tabulated once in `k_corner_sides`.
- **The strength is a dial, and it is deliberately not at full.** `1.0` is the flat mean — the
  maximum blend; `0.0` is no blend, every hex flat. At full strength the land reads as a *blur*
  rather than as ground (Ben, 2026-08-24: *"just reduce the amount of smearing so it looks less
  blurred"*), so the blend is dialled back rather than replaced: it is doing something wanted, only
  too much of it. The shipped value is **0.35**, and the value's authority is Ben's eye in the live
  app, not a derived bound. Below full strength two adjacent hexes' shared corners no longer agree
  exactly, so a faint colour step returns at every seam — that step *is* the crispness, and it is
  a step in hue, never the dark 1 px gridline (which stays given up, below).
- A blending hex is drawn at the **full circumradius**, not at `draw_r`. That `-1 px` is the whole
  reason a hex grid reads as a grid — the background showing through as a border. Across blending
  land the gap is given up: adjacent hexes share edges exactly, so the dark seam stops existing and
  what separates two tiles is the colour step alone.
- **Water and installations are what the blend stops at, not province lines.** Ocean carries no
  blend (it needs a province id, which sea tiles resolve to 0), so a coastline stays hard; a built
  tile is excluded for its own reason (below). Those two exclusions are the whole boundary set.

The result is a gradient across the land's **real tile mixture** — explicitly not a dominant
composition (which would discard the axis mixture [TILES.md](../economy/TILES.md) exists to
express) and not a texture pattern. The Selection element's mixture bar is the blend's legend: it
un-blends the same colours so "what did that gradient just average?" is answerable at a glance
(see [SELECTION.md](SELECTION.md) § The province element).

**Two classes of tile are excluded from the blend and keep their crisp hex and 1 px border:**

| Excluded | Why |
|---|---|
| A **survey-masked** tile | The lock fill is a statement about *knowledge*, not terrain. The national border band is gated on `revealed` for the same reason — a boundary drawn through the mask would leak the political shape of unsurveyed ground. |
| Any tile under a **non-blending lens** | See the reduction table below. |

### Per-lens province reduction

**Every overlay mode keys on tile fields, so each needs a stated per-province answer.** A lens that
silently showed one tile's value for a whole province would be a defect. The reductions are decided
lens by lens; `lens_blend_mode` in `body_surface_canvas.cpp` is this table's executable half.

| Lens | Field grain | Province reduction | Why |
|---|---|---|---|
| **none** (terrain) | per tile, continuous | **Blend** (vertex mean) | The terrain mixture *is* the thing being rendered. |
| **Resource** | per tile, presence of one good | **Blend** (vertex mean) | Deposit extent is a real spatial field; the blend renders the deposit's soft edge, which is exactly what the lens is about — the *shape* of the deposit. |
| **Continent** | per tile, categorical (plate) | **Refusal — stays flat per tile** | Same argument: the mean of two plate colours is a plate that does not exist, and the boundary emphasis is the lens's whole point. |
| **Market** | per catchment | **No reduction needed** | A catchment already covers whole provinces. Blending would soften the catchment boundary the lens exists to show. |
| **Scarcity** | per catchment | **No reduction needed** | As Market — the value is already constant across every province in the catchment. |
| **Corporation** | sparse, per built tile | **Refusal — stays flat per tile** | Ownership is a property of a building on a tile, and built tiles are outside the blend by construction. |
| **Production** | sparse, per producing tile | **Refusal — stays flat per tile** | The question is "where is output concentrated?" and the building's tile *is* the answer. A province-uniform block would claim the whole province produces. |
| **Industry** | sparse, per tile with background plant | **Province SUM, filled uniformly** | The one sparse field whose question — "how much plant I did not build stands here?" — is genuinely about the locality. Density is additive, so the member tiles are summed and the whole province fills flat. Blending would spread one works' amber over empty ground beside it, reading as industry that is not there. |
| **Population** | per-tile **dot mark**, no fill | **N/A — mark, not fill** | Nothing to blend; the dot stays per tile. |
| **Opportunity** | per-catchment **dot mark**, no fill | **N/A — mark, not fill** | As Population. |
| **Reach** | body-level | **N/A — paints no tile fill** | The readout is per connected body, not per tile. |
| **Supply-routes** | body-level edges | **N/A — paints no tile fill** | Aggregated body-pair edges. |
| **Supply** | per-tile convoy glyph, no fill | **N/A — glyph, not fill** | As Population. |

Only **Industry** carries a genuinely computed per-province reduction; the rest are blends, refusals
with a reason, or lenses that paint no fill. That is deliberate: the province is the *selection*
grain under every lens, but it is the *render* grain only where the field is continuous.

**Country has no row because it is not a lens.** The national read is the border band below, which draws on the plain canvas only —
always-on chrome, composited per tile *after* the blend has run. That siting is what retires the
question the row used to answer: a nation's colour never enters the blended fill, so the mean of two
nation colours — a third nation's colour — cannot be reached.

### Selection and hover at province grain

- A click that hits no marker glyph selects the **province** (`ui_state::selected_province`). The
  marker precedence above it (unit > building) is untouched.
- The **selection outline traces the province's outer boundary** — every side facing a different
  province, never the interior seams. Hover uses the same shape at the hover colour and yields to
  selection, per the highlight convention (`highlight.hpp`).
- **There is no always-on province edge.** It was a faint 1 px stroke on every side facing another
  province; Ben took its alpha to zero (2026-08-22, the same ruling that let the blend cross province
  borders), so the pass is gone rather than dialled down — at any alpha it would have been the one
  thing still asserting a cell, which defeats the change instead of softening it. **A province's only
  visible boundary is the on-demand selection/hover outline**, which is the crisp affordance the
  faint stroke was trying to be.
- An ocean or unpartitioned tile has no province and still selects as a tile.

Full selection semantics — the mutual exclusion with `selected_entity`, the reconciliation rule, and
the card's contents — are in [SELECTION.md](SELECTION.md) § The province element.

---

## The national border band

**A nation reads as a bordered region, not as a tinted field.** Its identity colour
(`palette::nation_colour`) lives at the boundary and falls off inwards; the middle of a territory
stays plain. That is what makes the read affordable at all — a full-territory tint would own the
ground the terrain and the texture need, and a band does not.

**The band draws on the plain canvas only, and is suppressed while any lens is up** (Ben,
2026-08-28, reaffirmed 2026-09-07). Affordability is why the band is a band; it is not a licence to
draw it under a lens. A lens asks one question, and nation ownership is a second political answer
competing with it — so nation context is absent from a lens *by construction*, not merely absent
from its fill. Roads are therefore **not** the precedent: a road is terrain a lens reads over.

A consequence worth stating, because it removes a question rather than answering it: the band's
click corridor cannot contend with a lens's own structure for a press, because the two are never on
screen together. Under a lens, a click that misses every marker falls through to the province as it
always did.

Ben, 2026-08-24: *"National borders should not diffuse together, instead they should borders
extending their colour inwards. With this, we can drop the nation lens."*

### Two neighbours must never blend into a third colour

This is the binding constraint, not a nicety, and it decides both halves of the pass:

- **The wash is composited per tile, after the land blend.** A tile takes only *its own*
  nation's colour, at an alpha keyed to its depth from the frontier. No arithmetic in the pass ever
  sees two nations, so no averaged hue can be produced.
- **The boundary stroke is inset, not laid on the shared edge.** A shared edge can carry one colour
  only; two neighbours would fight for it and the later draw would win. Pulled back toward the
  drawing tile's own centre by `k_border_stroke_inset`, each nation paints a rule just *inside* its
  own side, so a frontier reads as two parallel coloured lines with the seam between them.

### The falloff

Depth is the tile's distance, in tiles, from its nation's frontier — depth 0 being a tile that
touches a foreign owner. **Unclaimed ground is its own owner**, so a coastline is a border: a
nation's shore carries the band too. Unclaimed tiles draw no band; they have no colour to extend.

**But a shore is not the same claim as a neighbour, and it is not drawn at the same weight.** An
edge facing unclaimed ground takes `k_border_unclaimed_scale` — wash *and* stroke, colour and
thickness — where an edge facing another nation takes full strength. Ben, 2026-08-24: *"reduce the
border band on edges facing unclaimed ground."*

The reason is a shape problem rather than a taste one. A country of small islands is nearly *all*
frontier, so at one uniform weight the treatment meant to be an edge effect became a tint again —
and it did so on precisely the nations least able to spare the ground. Two rules keep the reduction
honest:

- **Political wins where both meet.** A tile touching a foreign nation *and* open ground counts as
  political, so a coastal frontier between two countries does not quietly fade into its own sea.
- **The stroke resolves per edge, the wash per tile.** The stroke already knows what lies across
  each individual edge, so a headland facing water on three sides and a neighbour on the fourth
  draws three light rules and one full one. The wash cannot: a depth-1 tile is not on the frontier
  and has no neighbour to ask, so it *inherits* its kind from the frontier tile that seeded it.

| Depth | Wash opacity | Reads as |
|---|---|---|
| 0 (on the frontier) | 0.35 | The single frontier ring, under the coloured rule |
| 1+ | none | Terrain, texture and the active lens, untouched |

`k_border_band_tiles` = 1, and the band colour is **muted** — the nation identity colour
pulled toward its own luma by `k_border_mute` (0.55) and sat down slightly, wash and stroke
alike (the border-corridor hover label keeps the full identity colour: a label must be read,
not weighed). Ben, 2026-09-01, judging the first baked painterly ground: the three-ring
falloff and full-strength colour were tuned against flat saturated hexes, and over the muted
C-F bake the band inverted its contrast relationship with the ground — it became the loudest
mark on the map. *"Nation borders are way too strong. We should use a muted colour palette,
and we should also make it a 1 tile glow."* The inward-falloff mechanism above remains the
design (the depth relaxation still runs, and widening the ring is one constant) — what the
ruling sets is its extent and its volume.

The band is gated on `revealed`, like the survey mask itself: a border drawn through the survey
mask would leak the political shape of ground the player has not paid to survey
([DISCOVERY.md](DISCOVERY.md)).

### Clicking the border selects the nation

**The band is a selection target, and it is the route the Country lens used to own.** With the lens
retired ([LENSES.md](LENSES.md) § The Country lens has retired — national borders are chrome), the
border is what carries a nation
on screen — so the border is what opens it. Ben's ruling of 2026-08-24, on where the nation ledger
is reached: *"click the border itself."*

- **A real hit width.** The drawn stroke is a line, and a line is not clickable at play zoom, so
  each drawn segment registers a **corridor** of half-width `k_border_hit_px` (7 px) that is
  independent of the stroke's thickness. Registered per *drawn* segment, so the corridor follows the
  wrap copies and the survey mask for free.
- **A hover read that names the nation before the click commits.** Hovering the corridor shows an
  immediate label at the cursor, in the nation's colour. Immediate, deliberately: the shared
  glance-then-stick hover card ([TOOLTIP.md](TOOLTIP.md)) waits out an appear delay by design, and a
  target that only announces itself after a dwell cannot be predicted before the press.
- **Priority.** Markers first (unit > building), then the boundary corridor, then
  the tile/province fallback. A marker is a specific thing the player aimed at and outranks a
  region; inside the corridor, the border *is* what the pointer is on.
- **Structure grain, not a nation special case.** The corridor is a `structure_hit_zone`
  (`ui_state.hpp`) carrying its own kind and width, resolved by a general nearest-segment walk. A
  plate rim or a market-catchment edge joins by producing zones — the resolver and the click path
  do not change.

---

## Layers — what draws on this canvas

Beyond the base grid and the chrome in the table above, the draw pass
(`body_surface_canvas.cpp`) composites, in broad order:

- **Terrain channels** — substrate/cover hue + landform relief tint; the dramatic landforms are baked relief.
  Spec: § Terrain channels — composition and landform, above.
- **Lens tints** — the lenses keyed on `ui_state::overlay`
  ([LENSES.md](LENSES.md)); relief composites *after* the lens tint so landform
  survives a saturated overlay.
- **Built-tile chrome** — road spans (at the two widest rungs; painted into the ground
  above them), the home-cluster ring. Buildings and
  settlements are **not** a layer here: they are baked into the ground
  ([§ Building markers](#building-markers); RENDERING.md § Installations).
- **No HQ or market-centre markers** — both retired from this canvas (Ben, 2026-10-10: "We should remove the HQ glyph and market center glyphs").
- **Activity fog + convoy beams** — the intra-body vision layers
  (`permanent_vision`, `convoy_beams` in `ui_state`) and the
  survey region mask. Model authority: [DISCOVERY.md](DISCOVERY.md).
- **Hover/selection chrome** — the highlight convention, the glance-then-stick
  hover card ([TOOLTIP.md](TOOLTIP.md)), and the construction placement ghost.

### Draw-loop cost model

The per-tile loop is **culled and cached**, not all-tiles-per-frame:

- The spatial index is the per-body raster logistics caches on
  `world.body_tile_index` (`body_tile_grid`); `app::render` ensures it
  for the active body, and the canvas — holding `const world&` — only reads it.
  Nothing per-frame rebuilds a tile map or sorts a draw list.
- Iteration is **row-major over the raster**, which *is* sorted-by-id
  order (tile generation creates each body's tiles rows-outer with sequential
  ids) — so draw order is stable and every golden depends on it.
- **Row band cull:** rows don't wrap, so the visible row range falls straight
  out of the clip rect (± the hex circumradius margin). **Column cull:** the
  horizontal wrap-window (`k_min`/`k_max`) is computed at the top of the loop
  body — a tile with no visible wrap copy costs one multiply-compare, before
  any built/owner/lens work.

- **The band passes cull columns too.** The shade cache and the border depth run one
  pass ahead of the loop; they walk only the columns with a visible wrap copy, widened
  by the one-tile neighbourhood they read (blend corners, the frontier test). Even the
  widest rung shows little more than half the body's width.
- **Per-tile reads are dense, once a frame.** The band's tile records and nation owners,
  the vision scalar (permanent vision, then the beam) and a built/owned bit are laid
  into raster-indexed arrays at the top of the frame; the passes that read a tile and its
  neighbours read those, not the hash maps. The Market and Scarcity lenses memoise a
  tile's catchment market the same way, so the fill and the hovered-catchment wash ask
  once. Values are identical; only where they are read from moved.

**The 60 fps budget (TECH_FOUNDATIONS.md § Target hardware; Ben, 2026-10-09).** The
canvas must hold 60 fps at every rung, plain and under a lens. Measured
(`scripts/verify/canvas_vector_perf.lua`, Release, 1720×1080, home body, 300 frames
of sustained pan per rung; median work ms per frame — build + submit, present excluded):

| Rung (drawn radius) | Plain, before | Plain, after | Market lens, before | Market lens, after |
|---|---|---|---|---|
| 0 (~6 px) | 155.5 | 11.5 | 21.4 | 12.4–15.3 |
| 1 (~13 px) | 52.4 | 7.3 | 14.2 | 7.7 |
| 2 (~27 px) | 17.0 | 4.1 | 10.6 | 5.0 |
| 3 (~55 px) | 4.6 | 1.5 | 3.8 | 1.6 |
| 4 (~110 px) | 2.4 | 1.2 | 2.7 | 1.2 |

Rung 0 under a lens is the tightest case: the lens fill is derived per visible tile
every frame (no bake carries it), and its p95 sits near the budget on a loaded machine.

**What the cost was.** Not the ground, and mostly not vertex count either. The SDL
renderer backend converts each draw command's vertex colours from that command's
vertex offset to the END of the list — once per command — and every ground chunk is a
texture of its own, so a command of its own. With the ~50 chunk images emitted first,
each re-converted the whole vector layer behind them: at rung 0 that was ~45 ms of
submit on its own. So the chunk images are emitted **last, on a vertex offset of their
own, into the first channel of a three-way split** (ground, washes, strokes): drawn
first, converted for their own four vertices. Draw order is unchanged.

**What the vector layer gave up at the wide rungs**, each keyed on the drawn radius:

- **Washes over the bake are one channel, not anti-aliased.** The player-identity,
  construction-suitability and vision-fog washes on baked ground are collected through
  the tile loop and emitted beneath every stroke. They tile the plane with their
  neighbours, so an AA fringe bought nothing but doubled vertices and a faint seam where
  two translucent fringes overlapped. At the coarse fill (`draw_r ≤ 7 px`) a wash is the
  grid-step rect, and the fog — the one wash on nearly every tile — joins equal
  neighbours along a row into one rect: ~500 vertices for the whole body where there
  were ~175k. A road or rule now sits over its neighbour's wash rather than under it;
  the difference is the width of a fringe.
- **Roads and lanes below 20 px take the wide-rung curve LOD** — one stroke per curve,
  two segments a half, no apex joint (RENDERING.md § Roads and sea lanes).
- **Only frontier tiles draw the border rule** — the depth pass already ran the same
  neighbour test, so every interior tile skips six reads. Nothing that drew stops drawing.

**The static strokes are cached, not rebuilt (Ben, 2026-10-10: 60 fps at every zoom).**
The border band's wash, the drawn road and lane network, the border rule and the
Throughput anchor rings do not move while the player pans, so they are not re-tessellated
every frame. They are built per **bucket** — sixteen columns of one row — at the view
origin, stored as finished vertices, and re-emitted each frame with the pan added, for
each wrap copy and only for the bucket's tiles whose centres are on screen. A bucket is
**content-addressed**: its key folds in every input its strokes read (each tile's road and
lane levels, survey bit, vision and nation over the bucket widened by two columns and two
rows — the lane rung test reads two steps out — and each own tile's band depth, frontier
kind and anchor share), so it rebuilds exactly when something it draws from moves, and
never on a pan. Whatever moves the world — a tick, a road laid while paused, a scripted
survey, a different world — the hash sees it on the next frame; nothing is trusted to a
change counter. A zoom step, a lens switch or a change of draw flags starts the cache over.
The cache is off where its draw order would be wrong: without the baked ground (the vector
fallback fills each tile over its neighbours' strokes) and under the god view.

- **Draw order.** The cached strokes are emitted before the tile loop, so a tile's own marks
  — the player ring, the owner rim, the Resource outline, markers, highlights — now sit over
  a neighbour's road or rule rather than under it. At rungs 0-1 the captures differ from the
  uncached ones in under 0.13% of canvas pixels by more than 8/255 (the junction caps of a
  few road forks, whose degenerate spoke turns on a sub-pixel rounding difference); at rungs
  2-4 they are identical.
- **A tile record is a cache miss.** At the widest rung the band holds ~30k tiles, and a
  read of a tile's record is a hash-map node somewhere in memory: the passes read it once, in
  the band pass, and lay what they need into raster-indexed arrays there (the road and lane
  levels beside the tile and nation already laid). Everything after reads the arrays — the
  Throughput wash reads the reach field by raster position, the tile loop fetches a record
  only for a pass that asks (a lens, construction, a hovered structure), and the unit and
  battle markers, and the anchor ring, test a raster bit before their lookup.

Measured (Release, 1720×1080, home body, sustained pan, an idle machine, the play ground
path — `IO_GROUND_BENCH=1`, below). Canvas CPU is the pass meter's per-frame total, its own
cost included; work is the frame CSV's build + submit, the better of two runs' medians:

| Rung | Canvas CPU, plain | Corporation | Throughput | Frame work, plain | Corporation | Throughput |
|---|---|---|---|---|---|---|
| 0 (~6 px) | 9.0 → 5.8 ms | 7.9 → 6.0 | 8.7 → 5.8 | 10.4 → 5.7 ms | 7.5 → 4.6 | 14.7 → 7.7 |
| 1 (~13 px) | 3.7 → 2.4 | 3.5 → 2.2 | 4.2 → 2.3 | 5.5 → 3.6 | 4.4 → 3.3 | 8.4 → 4.8 |
| 2 (~27 px) | 1.1 → 0.9 | 1.1 → 0.8 | 1.4 → 0.9 | 1.9 → 1.7 | 1.6 → 1.2 | 3.5 → 3.0 |
| 3 (~55 px) | 0.6 → 0.5 | 0.5 → 0.5 | 0.6 → 0.5 | 1.2 → 1.1 | 1.2 → 0.9 | 1.9 → 1.8 |
| 4 (~110 px) | 0.3 → 0.4 | 0.3 → 0.3 | 0.4 → 0.3 | 1.1 → 0.9 | 1.0 → 0.7 | 1.5 → 1.4 |

Vertex counts are unchanged at every rung, and every rung holds the 16.7 ms frame. Under
heavy load (a game and a parallel compile on the same machine) rung 0 is the one still at
the edge: 13.3 ms of work plain against 16.4 before, 12.9 under Throughput against 15.9.

**Frame timings need the live ground path.** Under `--verify` the ground layer re-snapshots
the world into its bake source every frame (`ground_layer::tick`, the `--verify` path, so a
capture sees that frame's world) — ~15 ms of every `build_ms` sample that play never pays.
Read frame totals with `IO_GROUND_BENCH=1`, which runs `--verify` on the play path; the pass
meter's canvas split is unaffected either way.

**No layer is dropped at any rung.** Terrain, relief, survey mask, fog, roads, lanes,
the border band and rule, markers and labels all draw at every rung they drew at
before (roads and lanes are drawn at the two widest rungs and painted into the ground
from the third up — one layer, two media); the captures at rungs 0 and 1 differ from the old ones in under 2.5% of pixels
by more than 8/255, and in under 0.1% by more than 32/255.

`IO_CANVAS_PASS_LOG=1` prints, every 120 frames, each map pass's vertices and CPU time
per frame (`CANVAS_PASS ...`); the meter costs ~0.4 ms per pass at rung 0, so read it for
the split, and `frame_csv` for the totals. Its `static` pass is the stroke cache (hash and
re-emit), and `static builds` counts the buckets rebuilt in the tally — zero on a still
view, a column of buckets per block a pan uncovers.

### Fill level-of-detail at far zoom

Below **`draw_r ≤ 7 px`** (`k_lod_radius_px`) the terrain fill is an `AddRectFilled` instead of a
6-gon: ~4 vertices against ~10 once anti-aliasing's fringe is counted, and no
fringe to rasterise.

**The bound is derived, not chosen.** A hexagon differs from its inscribed rect by
the corner cut, `draw_r × (1 − √3⁄2)` = `draw_r × 0.134`, which falls under one
pixel at `draw_r < 7.46`. At the 7 px bound the cut is 0.94 px, and the per-tile
landform icons (drawn at `0.42 × draw_r`) are already unreadable.

The rect is sized to the **grid step**, not the hex radius. Rows step by
`1.5 × hex_size` while a hex is `2 × hex_size` tall, so consecutive rows overlap; a
radius-sized rect is shorter than the row pitch and the terrain renders as
horizontal stripes. At `col_step × row_step` the rects brick-lay — odd rows are
already offset half a column — and tile the plane exactly.

| whole-grid view | hex fill | rect fill |
|---|---|---|
| vertices | 157,084 | **63,004** |
| submit | 14.25 ms | **4.25 ms** |

Zoomed-in phases are unchanged to the vertex (6,172 at z1.1; 23,620 at z3), so the
LOD provably does not fire where detail is readable. **Terrain colour, relief
shading, the survey mask and the fog wash are untouched** — they are colour, not
geometry — so the analytic read this view exists for is unchanged. The visible
difference is that the far-zoom grid texture is brick-laid rather than hex-dotted.

> **Panning is not the cost, and the measurement is the reason to say so.** The
> static and panning phases at the same zoom measure identically (157,084 vs
> 158,407 vertices). Pan is `pan += io.MouseDelta` — 1:1 with the cursor and
> correct — so a frame that takes 3× as long applies 3× the accumulated delta at
> once: the right destination by a jumpy route. "Sharp jumps while panning" is a
> frame-cost symptom, and input-side damping would add latency without touching it.

`build_ms` (~9 ms, ImGui draw-list construction) is the next thing
to look at if this view is short of budget on low-spec hardware.

### Terrain texture — substrate grain and cover pattern

Two procedural passes over each hex, drawn from `ImDrawList` primitives —
**no atlas, no art assets**, the same hand-drawn vector idiom
[ICONS.md](ICONS.md) establishes for the glyph vocabulary. Implementation:
`ui::draw_tile_texture` / `ui::texture_lod_scale` (`src/ui/hex_render.{hpp,cpp}`),
called from the Planetary fill loop and from the Selection band's neighbourhood view
so the two surfaces cannot drift.

**The split is the whole design, and it follows the axis split.** The substrate is the
axis the blend averages across the land; the cover is per tile
and must read as per tile. So:

| Pass | Keyed on | Character | Why |
|---|---|---|---|
| **Substrate grain** | `terrain_substrate` | 2–5 tiny marks, alpha 0.14–0.30 | Material, not a boundary. A rock-to-rock seam is not information, so the grain must not draw one — it stays quiet enough that the blend still reads as one continuous field. |
| **Cover pattern** | `terrain_cover` × `cover_density` | 1–5 marks, alpha 0.35–0.80 | A forest edge **is** information. The pattern asserts where one tile ends. |

Grain kinds by substrate: a **dot stipple** (barren, regolith), **bedding strokes**
(sedimentary, icy) and **angular fracture chips** (rocky, volcanic, metallic — metallic
in pale ink rather than dark, so it reads as specular rather than as dirt).
**Water draws nothing**: a flat sea is a correct reading of open water, and any mark on
it would be read as animated water, which the texture pass explicitly excludes.

Cover marks: a **canopy tick** (forest), the same silhouette at ~60 % (scrub — so a
forest line *grades* instead of snapping to an edge), a **three-blade tuft** (grass),
**stacked level dashes** (marsh), a **stipple** (snow and ash — both are *fall*, not a
growth form), a **crossed crust tick** (salt), a **windward crest** (dunes) and a
**block plan** (urban — built form, not growth).

**Density drives both channels**, which is the argument for the pattern reading it at
all: `cover_density` already means biotic yield to the economy, so a sparse wood both
*looks* thin and *cuts* thin off one scalar.

```
f     = cover_density / 255
marks = clamp(1 + round(f × 4), 1, 5)
alpha = (0.35 + 0.45 × f) × strength
```

| cover @ density | f | marks | alpha (full strength) |
|---|---|---|---|
| scrub @ 75 | 0.294 | 2 | 0.482 (123/255) |
| grass @ 150 | 0.588 | 3 | 0.615 (157/255) |
| forest @ 205 | 0.804 | 4 | 0.712 (182/255) |
| any @ 255 | 1.000 | 5 | 0.800 (204/255) |

(The four densities are the axis model's calibration points — the ones at which the
split model reproduces each single-axis composition's colour exactly; [TILES.md](../economy/TILES.md).)

Mark placement is **hashed from the tile's grid coordinate**, never from screen
position: the grid is a cylinder that draws several wrap copies of one tile, and the
canvas pans continuously, so a screen-space hash would make the ground crawl and would
disagree between two copies of the same tile.

#### Texture and the lenses — texture survives, attenuated

**Texture survives every lens at `0.45` strength** (`k_texture_lens_strength`). Two reasons
it survives rather than being replaced. The precedent is one channel over — the landform
relief is composited *after* the lens tint on the argument that terrain facts stay true
under an overlay, and "this ground is closed-canopy forest" is the same class of fact as
"this ground is a mountain". And replacing it would make each lens a *different map*
rather than the same map read differently, which is the property the lens bar depends on.

It is attenuated rather than left at full because a lens fill is a **categorical claim**
and must stay the loudest thing on the tile. The mechanism that keeps it from reading as
dirt is not the attenuation, though — it is that **each mark's ink is derived from the
tile's own drawn fill**, pushed 55 % toward a per-cover target. Under a saturated lens a
mark is that lens's colour darkened, so it reads as shading *on* the block. The same
derivation makes the fog wash and the survey dim free: as the ground darkens, so does
its grain.

> The 0.45 strength is a decision taken on Ben's behalf, not a ruling — see
> `NEEDS_REVIEW.json`. The frame that falsifies it is a saturated-lens rung of the check below.

#### Texture level-of-detail — its own, stricter bound

Texture is gated on **`draw_r > 14 px`**, ramping to full strength at **22 px**
(`texture_lod_scale`). This is deliberately **stricter than the 7 px coarse-fill
threshold above**, and for a different reason. The fill LOD asks *is the corner cut
still drawable*. A sampled pattern asks a harder question: at hex scale it is not
merely invisible when too small, it is **moiré** — adjacent tiles' marks beat against
the pixel grid and the map crawls under a pan.

**The bound is derived.** A cover mark is drawn at `0.20 × draw_r` and needs ~2 px of
extent before it is a shape rather than a stipple of aliasing: `2 / 0.20 = 10 px`, plus
headroom for the five marks a closed canopy draws without them merging → **14 px**. It
then ramps linearly rather than popping in, because a texture that appears between one
zoom notch and the next reads as a rendering fault. So `texture_lod_scale` is 0 at
`r ≤ 14`, 0.25 at 16, 0.5 at 18, and 1 at `r ≥ 22` — and the whole-grid view
(`draw_r ≈ 5–7`) carries no texture at all.

One further gate, in the canvas call site: texture is skipped on **survey-masked** tiles
(a cover pattern is terrain information, and drawing it through the mask would leak the
shape of unsurveyed ground — [DISCOVERY.md](DISCOVERY.md)). A **built** tile is *not*
excluded — its ground is ordinary ground and its texture keeps drawing under the
silhouette ([§ Building markers](#building-markers)). Texture is drawn after the fill and
**before** the national border band, so a boundary is never broken up by a canopy
tick.

**Check:** `scripts/verify/tile_texture.lua` (`verifier-visual`).

## Cell sizing and coordinate mapping

```
// Fit the full grid at zoom=1.
// For a pointy-top grid of (gw columns × gh rows) in odd-r offset:
//   total visual width  = sqrt(3) * hex_size * (gw + 0.5)
//   total visual height = hex_size * (1.5 * gh + 0.5)
hex_size = min(canvas_w / (sqrt(3) * (gw + 0.5)),
               canvas_h / (1.5 * gh + 0.5)) * 0.95   // 5% margin

// World-space centre of hex at (col, row) — odd-r offset:
col_step = sqrt(3) * hex_size
row_step = 1.5 * hex_size
local_cx = col_step * col + (row is odd ? col_step * 0.5 : 0)
local_cy = row_step * row

// Grid is centred at world origin:
grid_cx = (gw - 0.5) * col_step / 2
grid_cy = (gh - 1)   * row_step  / 2

// Screen position (with pan/zoom applied):
screen = view_origin + (local - grid_centre) * zoom

// Pointy-top hex vertices (circumradius r, centre c):
for i in 0..5:
    angle = π/6 + π/3 * i      // 30°, 90°, 150°, 210°, 270°, 330°
    vertex[i] = c + r * (cos(angle), sin(angle))
```

---

## Interaction

- **Hover** a tile: show the hover card. Hit-tested by distance to hex centre (< circumradius).
- **Single-click** the surface: markers are hit-tested first, **unit before building** (`resolve_marker_hit`, `body_surface_canvas.cpp`), so a unit standing on a built tile stays reachable on the first press. A market is not a marker here: it is selected through the Market lens's catchment or from the Market ledger (Ben, 2026-10-10: "We should remove the HQ glyph and market center glyphs"). On the plain canvas, a click that misses every marker but lands in a **national border corridor** selects that nation (§ The national border band); under a lens the corridor does not exist, because it is built in the same pass as the stroke. Otherwise it selects the **province** (§ Province grain above) rather than the tile; the tile is one press away in the province card. Clicks do not change the view rung — the Planetary screen is the bottom of the ladder.
- **Ascend:** clicking the minimap (which shows the Circumplanetary view) promotes it to primary.
- **Middle mouse button drag:** pan. Horizontal panning is unbounded — the grid is a cylinder, so panning past the east or west edge wraps seamlessly to the opposite side. Each tile is drawn (and hit-tested) at every horizontal offset that falls within the canvas, so there is no visible seam and the column under the cursor is always correct.
- **Scroll wheel:** zoom, anchored at the cursor position — **stepped**, one ×2 ladder
  rung per notch (Ben, 2026-09-01; [RENDERING.md](RENDERING.md) § Level of detail owns the
  ladder: one master image per body, every rung a downsample of it). The `=`/`-` keys step
  the same ladder. The land is viewed at **one oblique angle, 22.5°, at every rung and under
  every lens** (Ben, 2026-10-09 — RENDERING.md § One angle); interaction is unchanged under
  the tilt, with hit-testing through the camera's inverse.

The wrap seam has **no marker**: the wrap is seamless by construction and a seam indicator would
draw attention to a boundary that does not exist for the player.
