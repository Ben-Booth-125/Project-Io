# Project Io — Primary Canvases

Two canvases form the main spatial UI: the **Solar System Canvas** and the **Body Surface Canvas**. They are both always rendered. One occupies the primary viewport; the other is inset as a minimap. Clicking the minimap swaps which is primary.

This document is the detailed specification for Layer 2. It covers visual design, interaction, and implementation approach. Per-system overlay details (market data, supply routes, unit indicators) are deferred to their respective system documents and will be added to these canvases as later layers are built.

---

## Primary / Minimap layout

The game window is divided into two regions:

- **Primary region** — the majority of the window. The active canvas fills this space.
- **Minimap region** — a fixed inset in the bottom-right corner. The inactive canvas renders here at reduced scale.

**Default state:** Solar System Canvas is primary; Body Surface Canvas is the minimap.

**Swap mechanic:** clicking anywhere inside the minimap region swaps primary and minimap. The two canvases exchange roles; selection state is unchanged. There is no animation in the prototype.

**Clicking a body in the Solar System Canvas** (whether it is primary or minimap) selects that body and makes the Body Surface Canvas the primary view — unconditionally. The natural workflow — click a body in the solar system, arrive at its surface — is a single action regardless of which canvas was primary at the time. (Selecting a *different* body while the surface is already primary simply re-targets it and leaves the surface in front.)

The minimap renders the same drawing code as the primary canvas, parameterised by the available region size. No separate minimap drawing path. Scale, cell size, and orbit radius are all derived from the canvas size at draw time. Fixed-pixel element sizes (star/body radii, building markers) scale by `min(size.x, size.y) / 720` with small floors so they stay visible at minimap scale, and labels/title text are suppressed below ~320 px on the shorter edge to avoid clutter.

**Minimap region size.** `mm_w = max(240, 0.20 × min(window width, height))`; `mm_h = mm_w × 0.75`, preserving the 240×180 (4:3) ratio of the default. Anchored bottom-right with an 8 px margin.

---

## Solar System Canvas

### What the user sees

A 2D top-down view of the solar system. The star sits at the centre. Each body orbits it at a position derived from `orbital_radius_au` and `orbital_angle_rad`. Orbital rings mark each body's distance from the star. Bodies are labelled.

At Layer 2, this canvas communicates:

- Which bodies exist and where they are in the system
- Which body is currently selected (Surface Canvas target)
- Body type at a glance via colour

Economic and military data (supply routes, faction presence, convoy paths) are added in later layers as overlays on this canvas.

### Visual elements

| Element | Description |
|---|---|
| Background | Near-black: `(8, 10, 20)` |
| Star | Filled circle at canvas centre. Radius: ~12 px at full size. Colour: `(255, 220, 80)`. No label needed. |
| Orbital rings | Thin circle at each body's `orbital_radius_au` distance. Colour: `(38, 42, 52)` — barely visible, structural only. |
| Body (planet) | Filled circle, radius 7 px. Colour: `(80, 120, 180)` blue-grey. |
| Body (moon) | Filled circle, radius 5 px. Colour: `(148, 145, 140)` grey. |
| Body (asteroid) | Filled circle, radius 4 px. Colour: `(140, 110, 80)` brown. |
| Body (station) | Filled circle, radius 4 px. Colour: `(80, 180, 160)` teal. |
| Body label | Body name in small ImGui default font, drawn just below the body circle. Colour: white. |
| Selection indicator | Unfilled circle (outline only) drawn around the selected body, 3 px larger than the body radius. Colour: white. |
| Hover tooltip | Body name, type string, orbital radius in AU. Shown while mouse is over a body circle. |

### Coordinate mapping

```
canvas_centre  = top_left + size * 0.5
max_radius_au  = maximum orbital_radius_au across all bodies
scale          = (min(size.x, size.y) * 0.45) / max_radius_au   // 0.45 leaves margin

body_screen_pos.x = canvas_centre.x + cos(orbital_angle_rad) * orbital_radius_au * scale
body_screen_pos.y = canvas_centre.y - sin(orbital_angle_rad) * orbital_radius_au * scale
```

The y-axis is negated so that angle 0 is to the right and angles increase counter-clockwise, matching the conventional 2D maths orientation.

### Interaction

- **Hover** a body circle: show tooltip.
- **Left-click** a body circle: set `active_body` and make the Body Surface Canvas primary (`surface_is_primary = true`). Unconditional — clicking a body always brings its surface forward.
- **Click empty space in the minimap** (when Solar System is minimap): swap the Solar System Canvas to primary.
- Input is only processed for the canvas the mouse is over (see Implementation approach); an ImGui panel under the cursor takes precedence over both canvases.
- No pan or zoom in the prototype. Auto-fit always shows all bodies.

### `body_component` addition

Add `orbital_angle_rad` (float) to `body_component`. This is an authored value set during world construction — not procedurally generated. Hard-coded values for the prototype bodies:

- Kepler: `1.05` rad (~60°) — upper-right of the disc
- Vesta: `3.93` rad (~225°) — lower-left of the disc

---

## Body Surface Canvas

### What the user sees

A tile grid for the selected body. Each tile is a coloured rectangle. Terrain type determines colour. Buildings are marked with a simple overlay symbol on their tile. Hovering a tile shows its full data. The grid is always fully visible within the canvas — no pan or camera at this stage.

At Layer 2, this canvas communicates:

- The terrain profile of the selected body
- Where buildings are placed
- Per-tile resource and environment data on hover

Stockpile readouts, market state, and workforce indicators are added in later layers.

### Visual elements

| Element | Description |
|---|---|
| Background | Dark: `(18, 18, 24)` |
| Tile — barren | `(170, 145, 100)` sandy tan |
| Tile — rocky | `(112, 105, 95)` warm grey |
| Tile — icy | `(160, 200, 220)` pale blue |
| Tile — volcanic | `(135, 55, 28)` dark red-brown |
| Tile border | 1 px gap between cells. Background colour shows through — no explicit border draw needed. |
| Building marker | Small filled square (8×8 px, centred on tile) in white. A visual distinction between building types can be introduced later; a uniform marker is sufficient for Layer 2. |
| Selection indicator | 2 px white outline around the selected tile (if any). |
| Hover tooltip | Tile coordinates `[x, y]`, terrain name, hazard, habitability, and all four resource deposit values. Suppress zero deposits. |
| Body label | Canvas title bar shows the selected body name and type. |

### Cell sizing

```
cell_size = min(canvas_size.x / grid_width, canvas_size.y / grid_height)
grid_origin = canvas_top_left + (canvas_size - vec2(grid_width, grid_height) * cell_size) * 0.5
```

The grid is centred in the available canvas area. Cell size scales to use the full shorter dimension. With prototype bodies (3×3 and 4×4 tiles), cells will be large; this is expected and desirable for readability.

### Interaction

- **Hover** a tile: show tooltip.
- **Left-click** a tile: set `active_tile` in selection state.
- **Click anywhere in the minimap** (when Body Surface is minimap): swap to primary. If the click also landed on a tile, that tile becomes `active_tile` in the same action.
- No pan or zoom in the prototype.

---

## Shared selection state

A small struct in `src/ui/ui_state.hpp`, held by `app`:

```cpp
struct ui_state
{
    entity_id active_body = null_entity;  // drives Body Surface Canvas
    entity_id active_tile = null_entity;  // set by tile click; consumed by later layers
    bool      surface_is_primary = false; // false = Solar System is primary

    bool      show_tile_ledger = true;    // owned by the nav pane, not the canvases
};
```

Both canvas drawing functions take `ui_state&` and may write to it (body click, tile click, minimap click). `show_tile_ledger` is shared housekeeping for the left navigation pane and the Tile Ledger window; the canvases do not touch it.

---

## Implementation approach

Each canvas is a free function:

```cpp
// solar_system_canvas.hpp / .cpp
void draw_solar_system_canvas(const world& w, ui_state& ui, ImVec2 origin, ImVec2 size, bool input_enabled);

// body_surface_canvas.hpp / .cpp
void draw_body_surface_canvas(const world& w, ui_state& ui, ImVec2 origin, ImVec2 size, bool input_enabled);
```

`app::render()` computes the primary and minimap regions from the window size (see *Minimap region size* above), then calls both functions with the appropriate `origin` and `size`.

**`input_enabled` exists because the primary canvas fills the whole window *behind* the minimap.** A click in the overlapping bottom-right corner would otherwise be handled by both canvases. `render()` therefore enables input for exactly one canvas per frame — the minimap if the mouse is over the minimap region, the primary otherwise — and only when an ImGui panel isn't capturing the mouse (`WantCaptureMouse`). Each function still draws unconditionally; it just skips hover/click handling when `input_enabled` is false.

Because scale and element sizes are derived from `size` at draw time, the same function renders correctly at both primary and minimap dimensions.

---

## What is deferred

| Item | Deferred to |
|---|---|
| Supply route lines on Solar System Canvas | Layer 5 (supply routing) |
| Market price indicators on bodies | Layer 4 (market) |
| Stockpile / output readout on tiles | Layer 3 (extraction) |
| Faction colour coding on bodies | Post-prototype (diplomacy) |
| Convoy entities in transit on Solar System Canvas | Layer 5 |
| Resource deposit overlay / lens mode | Deferred indefinitely — terrain colour is sufficient for prototype |
| Camera pan and zoom | Deferred until procedural generation produces large bodies |
| Tile inspector ledger redesign (exploration system) | Post-prototype |
