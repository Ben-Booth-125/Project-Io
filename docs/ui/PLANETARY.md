# Project Io — Planetary Screen

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
> grid, the building/settlement canvas glyphs).

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
  channels** — a subtle relief tint (`ui::landform_relief`) plus stroke-only glyphs for
  the dramatic set, inked by luminance (`ui::contrast_ink`) — never in the hue. The full
  render spec is [CANVASES.md](CANVASES.md) § Terrain channels, shared
  with the Selection band's neighbourhood view via `hex_render`.

Substrate and cover **share** the hex's hue: `ui::terrain_colour` (`src/ui/hex_render.{hpp,cpp}`)
is the single colour source of truth, and it blends the substrate's own colour toward a
per-cover endpoint by that tile's density. Sharing one channel is what makes the texture pass
below necessary rather than decorative — two different tiles can arrive at the same green.

Ocean and landmass are derived from the **Continents/Drift tectonic-plate pass**
(`docs/generation/CONTINENTS.md`; the Continent lens renders the plates). Ocean fraction is
an outcome of the plate pass and the body's hydrological state, not a flood-fill target.

---

## Visual elements

| Element | Description |
|---|---|
| Background | Dark: `(18, 18, 24)` |
| Tile | Filled hexagon. Colour from `ui::terrain_colour` (substrate + cover hue), composited with the landform relief tint (§ Terrain types above). A 1 px gap between hexes lets the background show through as a border — achieved by drawing each hex at `circumradius - 1 px` rather than adding explicit borders. |
| Building marker | A vector glyph centred on the tile at 48 % of the hex circumradius, drawn by `ui::icons::building` over **live ground** — the hex keeps its terrain hue, its texture, its relief and whatever the active lens washes over it. The silhouette encodes the type and the fill encodes the owner; a stacked tile adds the segmented ring. Full spec: [§ Building markers](#building-markers) below. |
| Road network | **Always-on** (like terrain, not a lens): the generated road lattice plus player-placed roads render as **continuous, symmetric spans**. Each roaded tile draws its **own half** of every shared road edge — from its centre to the midpoint of the centre-to-neighbour line — toward each roaded, survey-revealed cardinal neighbour; the two tiles' halves meet at the edge midpoint, so a road spans the pair identically whichever tile is "from" (no from/to asymmetry), and a small centre cap rounds junctions and keeps a lone / just-placed road visible. Cylinder-seam edges shift one period to stay short; drawn only toward survey-revealed neighbours, so roads don't leak past the survey fog. Styled by the drawing tile's **tier** — **Track** (`road_level` 1) thin/dim, **Road** (2) medium, **Highway** (3) thick/bright — so a tier change reads as a taper at the midpoint. Spans **dim with the commercial-reach fog**, through the same wash the lens fill takes; a road edge is fogged by the **max** of its two tiles' vision (see [DISCOVERY.md](DISCOVERY.md)). The tier ladder has **no on-canvas key** — it is named contextually in the Selection panel instead (below). |
| Road-tier legend | **Contextual, not chrome** (Ben's call, 2026-08-09). The three tiers render by line weight and brightness alone, and roads are always-on terrain rather than a lens, so the per-lens legend drawer cannot carry them. Instead, selecting a roaded tile names its tier beside the coordinates in the Selection panel header — `Tile [x, y] · Highway` — with a hover tooltip giving the thin→thick ladder. A roadless tile shows nothing; no persistent chip is added anywhere. |
| Selection / hover indicator | Hex outline drawn through the shared highlight convention (`src/ui/highlight.hpp`): white for the selected tile, light blue for the hovered tile (per wrap copy), amber for pinned. Precedence is selected > pinned > hovered. |
| Hover card | The shared glance-then-stick hover card ([TOOLTIP.md](TOOLTIP.md)), content **lens-keyed** (`src/ui/hover_content.cpp`). A tile's default variant: `substrate · landform` header (plains unnamed), habitability, and the landform's movement-cost multiplier when not plains. Under the Resource lens: the selected resource's deposit richness; under Population: habitability + workforce cap. Buildings and market centres carry their own variants (rival buildings show type + owner only — the competitor-visibility rule, [DISCOVERY.md](DISCOVERY.md)). |
| Body label | Canvas title bar shows the selected body name, type, and grid dimensions. As the Planetary screen is always primary (full size), the title is always shown. A **survey-status suffix** follows it: `UNSURVEYED`, `Survey en route`, or `Surveying k/N` — nothing once surveyed. |
| Survey region mask | On a body whose survey is incomplete, tiles in **unrevealed regions** render as a flat dark "locked" fill `(12, 14, 20)` with no lens tint, borders, markers, selection outline, or hit-testing; revealed regions render normally. Regions reveal in deterministic raster (row-major) order as the survey scans ([DISCOVERY.md](DISCOVERY.md)). A fully surveyed body (the home planet, or a completed survey) shows everything. |
| Settlement markers | Always-on civic chrome, not lens-gated: **every** generated population centre draws, and its **form follows the zoom** — the LOD ladder (BL-625, settlement tier glyphs). Far zoom (hex radius ≤ 7 px, the canvas's coarse-fill pivot): only scale ≥ 3 centres carry the tier skyline (`ui::icons::settlement`); everything smaller is a dim civic **density dot**, so a settled region reads as settled without glyph soup. Mid zoom (7–14 px): towns (scale 2) join the skylines. Close zoom (≥ 14 px, the texture pivot): every centre is a skyline, and **razed** centres (BL-624) surface as the ruin mark (`ui::icons::settlement_razed`) — a ruin is a tile-scale fact. Only **City+** centres (scale ≥ 4) carry a name label. Colour is **civic-neutral** (`palette::settlement`) under every lens — tier is carried by the glyph, and ownership is carried by the national border band, not by a settlement's colour. |
| Home-cluster ring + HQ star | Always-on player-presence chrome on `home_body` only: a translucent ring (player-identity colour) encloses the player's holdings cluster on that body ("my region"), and an `ui::icons::hq` star marks the building nearest the cluster centroid ("my origin"). Composes with, does not duplicate, the per-tile ownership outline. |
| National border band | **Always-on** political chrome (like roads, not a lens): a nation's identity colour sits at its frontier and falls off inwards over three tiles, and clicking the band selects the nation. See § The national border band below. |
| Rivers | Directed river lines drawn along tile edges with downstream chevrons, so a basin reads as flowing rather than as a static blue band. Terrain drawing, not a lens; always on. |

---

## Building markers

### The glyph draws over the hex, never on a plate of its own

**A building marker paints no background.** The silhouette is drawn straight onto the
tile's own fill, so terrain hue, substrate grain, cover pattern, landform relief and
whatever the active lens washes over them all keep rendering underneath and around it.
Ben, 2026-08-24: *"Remove building background. Buildings should be drawn over the hex,
not completely on top."* (BL-596, buildings over the hex.)

The argument is that the hex is the thing carrying substrate, cover, ownership and every
lens; occluding it in order to label it trades away the map to annotate it. A built tile
is therefore an ordinary tile in every render pass — it takes the province blend, the
texture pass and the relief tint exactly as the unbuilt ground beside it does.

**Legibility rests on the glyph, not on a backing.** The silhouette is drawn at 48 % of
the hex circumradius in a pale, owner-tinted fill, carrying the filled family's dark
outline ([ICONS.md](ICONS.md) § Shared conventions). The pair is self-balancing across the
terrain palette's full range: over near-white ice the dark outline holds the shape, over
dark forest the pale fill does. When a glyph is illegible over some terrain, the fix
belongs in the **glyph** — its weight, or an outline/halo on the stroke itself — never in
a reinstated plate.

**Ownership keeps three channels and loses none.** The silhouette's *fill* is the owning
corporation's identity colour lightened toward white; a small `corp_emblem` tag sits in
the hex's lower-right; and the player's own tiles carry the persistent footprint outline
on the rim under every lens. The player-identity wash on the plain default applies to a
built tile like any other tile of theirs, so a cluster reads as one footprint rather than
as a ring of owned ground around an unowned hole.

### A stacked tile: the segmented ring

A tile carries as many buildings as its richness allows, so "how does one hex say it holds
more than one thing?" is a real question. Ben's answer, 2026-08-24: **the segmented ring**
— one arc per building **kind** laid around the inside of the hex rim, with the dominant
kind's glyph in the centre. Chosen over a glyph cluster (which becomes soup past three) and
over primary-plus-count (which is always legible but never says *which*): the ring is the
only one of the three that scales with the richness-derived stack cap and still names its
contents.

**A kind is a `building_type`, not a named building.** Two extraction sites working
different deposits are one kind standing twice. Each kind's arc takes its colour from
`palette::building_kind_colour` — a hand-picked, hue-separated set rather than a hash,
because the roster is small and closed and the one place these colours are read is exactly
where two adjacent hues would be indistinguishable.

**Three marks, three questions.** They compose rather than duplicate:

| Mark | Answers |
|---|---|
| The **ring** | *Which kinds stand here?* |
| The **centre glyph** | *Which of them leads?* — the dominant (lowest-id) building's silhouette |
| The **`+N` badge** | *How many buildings in total?* |

**Read clockwise from the top.** The first segment sits at 12 o'clock and is the dominant
kind — the one the centre glyph depicts. The remainder follow in ascending `building_type`
order, so the ring is stable frame to frame and identical across runs. A single-kind tile
draws **no ring at all**: its centre glyph already describes it fully, and a ring on every
built tile in the world would be chrome rather than information.

**The rim is the borders' territory, and a ring placed there must not read as one.** Three
passes claim it: the player's own footprint outline (the loudest), the nation-border
segments under the Country lens, and the province edge stroke. Every one of them is
**hexagonal, continuous and thin**, and four properties separate the ring from all three:

| Property | The ring | The border passes |
|---|---|---|
| **Shape** | A circle — curved everywhere | Hexagonal: straight sides, hard corners |
| **Radius** | Inset to 0.76 r, inside both the edges (0.866 r at their midpoints) and the vertices (1.0 r) | On the rim itself |
| **Continuity** | **Broken** — a gap between every segment | Continuous; a border is never dashed |
| **Weight** | `max(2, 0.14 r)`, plus its own dark under-stroke | 1.5–2 px flat |

Shape is the load-bearing one: a curve among hexagons cannot be read as a boundary of a
hexagonal cell. The gap is the second — no border pass is ever dashed.

### The ring's level of detail — it degrades, it does not vanish

The ring draws only above **`draw_r > 10 px`**, its own bound and a stricter one than the
`7 px` coarse-fill threshold (§ Fill level-of-detail at far zoom), for the same reason the
texture pass carries its own stricter bound: the two ask different questions. Coarse fill
asks whether the corner cut is still drawable; the ring asks whether one *segment* is still
a segment, and a segment shrunk to the length of its own gap reads as a dotted circle rather
than as a count.

**The bound is derived.** A segment's drawn arc is `2π × 0.76 × draw_r / kinds × (1 − 0.20)`.
At the practical worst case — the full placeable roster, six kinds on one tile — that is
`0.637 × draw_r`, and a stroke needs about 6 px of run before it reads as an arc rather than
a blob: `0.637 × draw_r ≥ 6` gives `draw_r ≥ 9.4`, rounded up to 10.

**Below the bound the tile falls back to the dominant kind's glyph alone.** That is the
whole point of stating a bound rather than letting the arcs shrink: never an empty hex,
and never a ring drawn at a size where its arcs have merged. Because 10 > 7, the ring is
already gone by the time the fill goes coarse, so there is no band in which a rim is being
segmented that the fill is no longer drawing.

The ring is suppressed under the **Population** and **Opportunity** lenses, alongside the
silhouette it surrounds: those lenses replace a tile's installation read with a per-tile
value mark, and a ring with no centre glyph would be a ring with nothing to be dominant.

**Check:** `scripts/verify/stacked_tile_ring.lua` (`verifier-visual`), which *stages* a
multi-kind stack rather than hunting for one — a generated world does not reliably produce
a tile carrying several kinds, and a capture of a state the script could not produce proves
nothing.

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

**Country has no row because it is not a lens.** The national read is the border band below —
always-on chrome, composited per tile *after* the blend has run. That siting is what retires the
question the row used to answer: a nation's colour never enters the blended fill, so the mean of two
nation colours — a third nation's colour — cannot be reached.

### Selection and hover at province grain

- A click that hits no marker glyph selects the **province** (`ui_state::selected_province`). The
  marker precedence above it (building > market_centre > unit) is untouched.
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
stays plain. That is what makes the read affordable **always-on**, under every lens and on the plain
canvas — a full-territory tint would own the ground the terrain, the texture and the active lens
need, and a band does not. Roads are the precedent: drawn always, because they are context rather
than a mode the player enters.

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
| 0 (on the frontier) | 0.50 | The edge itself, under the coloured rule |
| 1 | 0.26 | The colour reaching inwards |
| 2 | 0.11 | The last trace before plain ground |
| 3+ | none | Terrain, texture and the active lens, untouched |

`k_border_band_tiles` = 3. At the **coarse-fill LOD** (`draw_r ≤ 7 px`) the band collapses to
depth 0 alone: at the whole-grid view a single ring still draws the political outline, which is the
whole read at that zoom, and relaxing three rings over every hex on the body is the one place the
pass could cost real frame time.

The band is gated on `revealed`, like the survey mask itself: a border drawn through the survey
mask would leak the political shape of ground the player has not paid to survey
([DISCOVERY.md](DISCOVERY.md)).

### Clicking the border selects the nation

**The band is a selection target, and it is the route the Country lens used to own.** With the lens
retired ([LENSES.md](LENSES.md) § Structure-grain selection), the border is what carries a nation
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
- **Priority.** Markers first (building > market centre > unit), then the boundary corridor, then
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

- **Terrain channels** — substrate/cover hue + landform relief tint and glyph/spans.
  Spec: [CANVASES.md](CANVASES.md) § Terrain channels.
- **Lens tints** — the lenses keyed on `ui_state::overlay`
  ([LENSES.md](LENSES.md)); relief composites *after* the lens tint so landform
  survives a saturated overlay.
- **Built-tile installations** — building markers (the silhouette, the stacked-tile
  ring, the corp emblem tag and the `+N` count badge), road spans, settlement
  markers (the BL-625 LOD ladder), the home-cluster ring + HQ star. Drawn *over* the hex, which
  keeps rendering underneath ([§ Building markers](#building-markers)).
- **Corporate HQ markers** — per-corp seat markers (`draw_corp_hq`; see LENSES.md).
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

Budget reference (pan_perf, 1720×1080, 60 Hz vsync): play-zoom pan costs ~5.0 ms
work/frame in Release and ~6.7 ms in Debug. The heavy case is the whole-grid view
(all 15,120 hexes genuinely visible, ~155k vertices), which is
vertex-emission-bound and is what the fill LOD below exists for.

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
- **Single-click** the surface: markers are hit-tested first, in the order **building → market → unit** (`body_surface_canvas.cpp`), so buildings, markets and units stay independently selectable. A click that misses every marker but lands in a **national border corridor** selects that nation (§ The national border band). Otherwise it selects the **province** (§ Province grain above) rather than the tile; the tile is one press away in the province card. Clicks do not change the view rung — the Planetary screen is the bottom of the ladder.
- **Ascend:** clicking the minimap (which shows the Circumplanetary view) promotes it to primary.
- **Middle mouse button drag:** pan. Horizontal panning is unbounded — the grid is a cylinder, so panning past the east or west edge wraps seamlessly to the opposite side. Each tile is drawn (and hit-tested) at every horizontal offset that falls within the canvas, so there is no visible seam and the column under the cursor is always correct.
- **Scroll wheel:** zoom, anchored at the cursor position.

The wrap seam has **no marker**: the wrap is seamless by construction and a seam indicator would
draw attention to a boundary that does not exist for the player.
