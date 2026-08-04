# Project Io — Planetary Screen (Layer 2)

The Planetary screen is the tile-grid view of the selected body's surface — the **bottom rung** of the canvas ladder, and the rung play opens on (the corporation's home planet — the app itself opens on the main menu first, see [STARTUP.md](STARTUP.md)). It is the primary focus of Layer 2. See [CANVASES.md](CANVASES.md) for layout rules shared across the three canvases (the zoom ladder, context minimap, region sizing, shared selection state, implementation approach).

Because it is the bottom rung, the Planetary screen is **only ever primary** — it is never shown in the minimap. Reaching it is a descend click on the Circumplanetary screen; leaving it is a click on the minimap (which shows the Circumplanetary view) to ascend.

---

## What the user sees

A hex tile grid for the selected body. Each tile is a coloured hexagon. Terrain type determines colour. Buildings are marked with a simple overlay symbol on their tile. Hovering a tile shows its full data. Pan and zoom let the player navigate large bodies.

At Layer 2, this canvas communicates:

- The terrain profile of the selected body
- Where buildings are placed
- Per-tile resource and environment data on hover

Stockpile readouts, market state, and workforce indicators are added in later layers.

---

## Tile grid

**Shape:** Pointy-top hexagons in odd-r offset coordinates. Odd rows are shifted right by half a column. Grid axes: columns (x) run left-to-right, rows (y) run top-to-bottom.

**Target size and aspect ratio:** A body's grid is roughly **9 columns wide for every 5 rows tall** — the height is a little *under* half the width. The reasoning is geometric: the grid width spans the body's full circumference (both hemispheres), so a pole-to-pole height would be half the width; truncating the non-traversable polar caps brings it a little under half. The two planets are standardised to **180 × 84** (columns × rows); **Selene**, as a moon, uses **90 × 42** (the same ratio at half scale). The prototype world's surface bodies are **Cinder, Kepler, Selene, and Pallas** (Pallas, a notable belt asteroid, carries a small grid); **Helios** is the system star and has no surface. All other backdrop bodies were removed once the canvas perspectives settled.

**Horizontal wrap:** column indices wrap at the grid boundary so the east edge connects to the west edge, forming a cylinder. Generation wraps neighbours across this seam, and the Planetary canvas renders the wrap as a seamless infinite side-scroll: panning past either edge continues into tiles drawn from the opposite side (see [Interaction](#interaction)).

---

## Terrain types

A tile's character has **two axes** (the two-axis model, [TILES.md](../economy/TILES.md)):

- **Composition** — what the tile is made of. **Eleven values** (`terrain_composition`,
  `src/world/components.hpp`): barren, rocky, volcanic, icy, tundra, grassland, forest,
  wetland, ocean, regolith, metallic. Composition owns the hex's **hue** —
  `ui::terrain_colour` (`src/ui/hex_render.{hpp,cpp}`) is the single colour source of
  truth; the palette is not duplicated here (an earlier five-terrain RGB table in this
  section drifted and is superseded).
- **Landform** — the tile's physical shape. Seven values (`terrain_landform`): plains,
  highland, mountain, canyon, valley, crater, rift. Landform renders on its **own
  channels** — a subtle relief tint (`ui::landform_relief`) plus stroke-only glyphs for
  the dramatic set, inked by luminance (`ui::contrast_ink`) — never in the hue. The full
  render spec is [CANVASES.md](CANVASES.md) § Terrain channels (BL-231/BL-232), shared
  with the Selection band's neighbourhood view via `hex_render`.

> **Superseded — water coverage (2026-07-31).** The old rule here — ~60% water flooded
> outward by BFS as one contiguous region, a single preferred continent — is superseded
> by the **Continents/Drift tectonic-plate pass**, which derives ocean and landmass from
> generated plates (see `docs/generation/CONTINENTS.md`; the Continent lens, BL-226,
> renders the plates). Ocean fraction is now an outcome of the plate pass and the body's
> hydrological state, not a flood-fill target.

---

## Visual elements

| Element | Description |
|---|---|
| Background | Dark: `(18, 18, 24)` |
| Tile | Filled hexagon. Colour from `ui::terrain_colour` (composition hue), composited with the landform relief tint (§ Terrain types above). A 1 px gap between hexes lets the background show through as a border — achieved by drawing each hex at `circumradius - 1 px` rather than adding explicit borders. |
| Building marker | A small white vector glyph centred on the tile (~22% of hex circumradius), drawn by `ui::icons::building` with a thin dark outline. The silhouette encodes the type: extraction = ore-chunk, processing = square, port = triangle, inland logistics hub = hexagon (BL-149), other = circle. |
| Road network | **Always-on** (like terrain, not a lens): the generated road lattice (BL-146) plus player-placed roads (BL-147) render as **continuous, symmetric spans** (BL-172). Each roaded tile draws its **own half** of every shared road edge — from its centre to the midpoint of the centre-to-neighbour line — toward each roaded, survey-revealed cardinal neighbour; the two tiles' halves meet at the edge midpoint, so a road spans the pair identically whichever tile is "from" (no from/to asymmetry), and a small centre cap rounds junctions and keeps a lone / just-placed road visible. Cylinder-seam edges shift one period to stay short; drawn only toward survey-revealed neighbours, so roads don't leak past the survey fog. Styled by the drawing tile's **tier** — **Track** (`road_level` 1) thin/dim, **Road** (2) medium, **Highway** (3) thick/bright — so a tier change reads as a taper at the midpoint. |
| Selection / hover indicator | Hex outline drawn through the shared highlight convention (`src/ui/highlight.hpp`): white for the selected tile, light blue for the hovered tile (per wrap copy), amber for pinned (not yet wired). |
| Hover card | The shared glance-then-stick hover card ([TOOLTIP.md](TOOLTIP.md), BL-228/BL-230), content **lens-keyed** (`src/ui/hover_content.cpp`). A tile's default variant: `composition · landform` header (plains unnamed), habitability, and the landform's movement-cost multiplier when not plains (BL-232). Under the Resource lens: the selected resource's deposit richness; under Population: habitability + workforce cap. Buildings and market centres carry their own variants (rival buildings show type + owner only, BL-068). |
| Body label | Canvas title bar shows the selected body name, type, and grid dimensions. As the Planetary screen is always primary (full size), the title is always shown. A **survey-status suffix** follows it (survey system, BL-067): `UNSURVEYED`, `Survey en route`, or `Surveying k/N` — nothing once surveyed. |
| Survey region mask | On a body whose survey is incomplete, tiles in **unrevealed regions** render as a flat dark "locked" fill `(12, 14, 20)` with no lens tint, borders, markers, selection outline, or hit-testing; revealed regions render normally. Regions reveal in deterministic raster (row-major) order as the survey scans (survey system, BL-067). A fully surveyed body (the home planet, or a completed survey) shows everything. |
| Settlement markers | Always-on civic chrome, not lens-gated (BL-083): generated population centres are clustered into **conurbations** and drawn at their highest-scale member with a tier glyph (`ui::icons::settlement`) whose size grows with scale. Only **City+** conurbations (tier ≥ 4) carry a name label, to keep the map legible. Colour is civic-neutral (`palette::settlement`) except under the Country lens, where the host nation's tint applies — tier stays carried by size, keeping colour out of ownership. |
| Home-cluster ring + HQ star | Always-on player-presence chrome on `home_body` only (BL-085/BL-092): a translucent ring (player-identity colour) encloses the player's holdings cluster on that body ("my region"), and an `ui::icons::hq` star marks the building nearest the cluster centroid ("my origin"). Composes with, does not duplicate, the per-tile ownership outline. |

---

## Layers — what draws on this canvas today (2026-07-31)

Beyond the base grid and the chrome in the table above, the draw pass
(`body_surface_canvas.cpp`) composites, in broad order:

- **Terrain channels** — composition hue + landform relief tint and glyph/spans
  (BL-231/BL-232). Spec: [CANVASES.md](CANVASES.md) § Terrain channels.
- **Lens tints** — twelve live lenses keyed on `ui_state::overlay`
  ([LENSES.md](LENSES.md)); relief composites *after* the lens tint so landform
  survives a saturated overlay.
- **Built-tile installations** — building markers (enlarged silhouette + corp
  emblem tag on built tiles), road spans (BL-172), settlement conurbation
  markers, the home-cluster ring + HQ star.
- **Corporate borders** — per-corp seat borders (`draw_corp_border`, BL-201
  foundation).
- **Activity fog + convoy beams** — the intra-body vision layers
  (`permanent_vision`, `convoy_beams` in `ui_state`, BL-151/152/154) and the
  survey region mask (BL-067). Model authority: [DISCOVERY.md](DISCOVERY.md).
- **Hover/selection chrome** — the highlight convention, the glance-then-stick
  hover card ([TOOLTIP.md](TOOLTIP.md)), and the construction placement ghost.

### Draw-loop cost model (BL-268, 2026-08-02)

The per-tile loop is **culled and cached**, not all-tiles-per-frame:

- The spatial index is the per-body raster logistics already caches on
  `world.body_tile_index` (`body_tile_grid`, BL-077); `app::render` ensures it
  for the active body, and the canvas — holding `const world&` — only reads it.
  Nothing per-frame rebuilds a tile map or sorts a draw list.
- Iteration is **row-major over the raster**, which *is* the old sorted-by-id
  order (tile generation creates each body's tiles rows-outer with sequential
  ids) — so draw order, and therefore every golden, is unchanged.
- **Row band cull:** rows don't wrap, so the visible row range falls straight
  out of the clip rect (± the hex circumradius margin). **Column cull:** the
  horizontal wrap-window (`k_min`/`k_max`) is computed at the top of the loop
  body — a tile with no visible wrap copy costs one multiply-compare, before
  any built/owner/lens work.

Measured (pan_perf, 1720×1080, 60 Hz vsync): play-zoom pan went 11.3 → 5.0 ms
work/frame in Release and 41.2 → 6.7 ms in Debug. The remaining heavy case is
the whole-grid view (all 15,120 hexes genuinely visible, ~155k vertices): 12.1 ms
Release — vertex-emission-bound, owned by the zoomed-out LOD/draw-cache follow-on
(BL-269), not by this loop.

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

- **Hover** a tile: show tooltip. Hit-tested by distance to hex centre (< circumradius).
- **Single-click** the surface: set `selected_entity`. Markers are hit-tested first, in the order **building → market → tile** (`body_surface_canvas.cpp`), so buildings, markets and tiles are all independently selectable. Clicks do not change the view rung — the Planetary screen is the bottom of the ladder.
- **Rivers.** Directed river lines are drawn along tile edges with downstream chevrons (BL-170, `body_surface_canvas.cpp`), so a basin reads as flowing rather than as a static blue band. They are terrain drawing, not a lens, and are always on.
- **Ascend:** clicking the minimap (which shows the Circumplanetary view) promotes it to primary.
- **Middle mouse button drag:** pan. Horizontal panning is unbounded — the grid is a cylinder, so panning past the east or west edge wraps seamlessly to the opposite side. Each tile is drawn (and hit-tested) at every horizontal offset that falls within the canvas, so there is no visible seam and the column under the cursor is always correct.
- **Scroll wheel:** zoom, anchored at the cursor position.

---

## What is deferred

| Item | Deferred to |
|---|---|
| Stockpile / output readout on tiles | Layer 3 (extraction) |
| Resource deposit overlay / lens mode | ~~Deferred indefinitely~~ **Shipped 2026-06-17** (Resource lens, BL-019 — LENSES.md § Resource lens) |
| Tile inspector ledger redesign (exploration system) | Post-prototype |
| Seam *visualisation* (an explicit marker showing where the wrap occurs) | Post-prototype — the wrap itself is seamless and needs no marker |
