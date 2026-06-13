# Project Io — Planetary Screen (Layer 2)

The Planetary screen is the tile-grid view of the selected body's surface. It is the primary focus of Layer 2. See [CANVASES.md](CANVASES.md) for layout rules shared with the Solar screen (primary/minimap swap, region sizing, shared selection state, implementation approach).

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

**Target size:** ~40 rows × 180 columns per body (approximately Earth-scale). The 40×180 grids are not yet instantiated in the hard-coded world — existing bodies (Kepler, Cinder, Selene and others) use small placeholder grids. The grid sizes for Kepler, Cinder, and Selene should be expanded to 40×180 when procedural generation is introduced.

**Horizontal wrap:** column indices wrap at the grid boundary so the east edge connects to the west edge, forming a cylinder. This is a generation and rendering constraint; the canvas does not currently visualise the seam.

---

## Terrain types

| Terrain | Colour | Notes |
|---|---|---|
| Barren | `(170, 145, 100)` sandy tan | Flat, easy to build, low extraction cost |
| Rocky | `(112, 105, 95)` warm grey | Moderate yield, higher build cost |
| Icy | `(160, 200, 220)` pale blue | High ice deposit, low habitability |
| Volcanic | `(135, 55, 28)` dark red-brown | High hazard, elevated rare metals |
| Water | `(40, 80, 160)` deep blue | Ocean/sea; no land resources, high habitability |

**Water coverage:** approximately 60% of a body's tiles should be water, placed as one contiguous region (no isolated lakes in the prototype). The generation algorithm floods outward from a seed set of tiles using BFS, stopping at the 60% threshold. The remaining 40% forms one or more land masses; a single continent is preferred for prototype readability.

---

## Visual elements

| Element | Description |
|---|---|
| Background | Dark: `(18, 18, 24)` |
| Tile | Filled hexagon. Colour from terrain table above. A 1 px gap between hexes lets the background show through as a border — achieved by drawing each hex at `circumradius - 1 px` rather than adding explicit borders. |
| Building marker | Small filled square (centred on tile, ~18% of hex circumradius) in white. A visual distinction between building types can be introduced later; a uniform marker is sufficient for Layer 2. |
| Selection indicator | 2 px white hex outline around the selected tile (if any). |
| Hover tooltip | Tile coordinates `[col, row]`, terrain name, hazard, habitability, and all four resource deposit values. Suppress zero deposits. |
| Body label | Canvas title bar shows the selected body name, type, and grid dimensions. Suppressed when the canvas is the minimap (shorter edge < 320 px). |

---

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
- **Left-click** a tile: set `active_tile` in selection state.
- **Click anywhere in the minimap** (when Planetary screen is minimap): swap to primary. If the click also landed on a tile, that tile becomes `active_tile` in the same action.
- **Middle mouse button drag** (primary view only): pan.
- **Scroll wheel** (primary view only): zoom, anchored at the cursor position.
- The minimap always renders the default framing (zoom=1, no pan).

---

## What is deferred

| Item | Deferred to |
|---|---|
| Stockpile / output readout on tiles | Layer 3 (extraction) |
| Resource deposit overlay / lens mode | Deferred indefinitely — terrain colour is sufficient for prototype |
| Tile inspector ledger redesign (exploration system) | Post-prototype |
| Horizontal wrap rendering (seam visualisation) | Post-prototype |
| Water tile placement in hard-coded world bodies | When grids expand to 40×180 |
