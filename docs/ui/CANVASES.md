# Project Io — Primary Canvases

Two canvases form the main spatial UI: the **Solar screen** and the **Planetary screen**. Both are always rendered. One occupies the primary viewport; the other is inset as a minimap. Clicking the minimap swaps which is primary.

Detailed specifications for each screen are in their own documents:

- [SOLAR.md](SOLAR.md) — Solar System Canvas: visual design, coordinate mapping, orbital motion, interaction, and implementation.
- [PLANETARY.md](PLANETARY.md) — Body Surface (Planetary) Canvas: tile grid, terrain colours, building markers, interaction, and implementation. This is the primary focus of Layer 2.

---

## Primary / Minimap layout

The game window is divided into two regions:

- **Primary region** — the majority of the window. The active canvas fills this space.
- **Minimap region** — a fixed inset in the bottom-right corner. The inactive canvas renders here at reduced scale.

**Default state:** Solar screen is primary; Planetary screen is the minimap.

**Swap mechanic:** clicking anywhere inside the minimap region swaps primary and minimap. The two canvases exchange roles; selection state is unchanged. There is no animation in the prototype.

**Clicking a body in the Solar screen** (whether it is primary or minimap) selects that body and makes the Planetary screen the primary view — unconditionally. The natural workflow — click a body in the solar system, arrive at its surface — is a single action regardless of which canvas was primary at the time. Selecting a *different* body while the surface is already primary simply re-targets it and leaves the surface in front.

The minimap renders the same drawing code as the primary canvas, parameterised by the available region size. No separate minimap drawing path. Scale, cell size, and orbit radius are all derived from the canvas size at draw time. Fixed-pixel element sizes (star/body radii, building markers) scale by `min(size.x, size.y) / 720` with small floors so they stay visible at minimap scale; labels and title text are suppressed below ~320 px on the shorter edge to avoid clutter.

**Minimap region size.** `mm_w = max(240, 0.20 × min(window width, height))`; `mm_h = mm_w × 0.75`, preserving the 240×180 (4:3) ratio of the default. Anchored bottom-right with an 8 px margin.

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
