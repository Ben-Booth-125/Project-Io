# Project Io — Primary Canvases

Three canvases form the main spatial UI, arranged as a single **zoom ladder**
from the widest view to the narrowest:

```
   Solar          (system)   the star at centre, every body orbiting it
     │  ↑ zoom out / ↓ zoom in
   Circumplanetary (local)   one planet at centre, its moons / local space
     │
   Planetary    (surface)    one body's hex tile grid
```

All three are always available; exactly one is **primary** (fills the window)
and one **neighbouring** canvas is shown in the minimap inset. Detailed
specifications for each rung are in their own documents:

- [SOLAR.md](SOLAR.md) — Solar System Canvas: visual design, coordinate mapping, orbital motion, interaction.
- [CIRCUMPLANETARY.md](CIRCUMPLANETARY.md) — Circumplanetary Canvas: a planet and its moons / local space; the middle rung.
- [PLANETARY.md](PLANETARY.md) — Body Surface (Planetary) Canvas: tile grid, terrain colours, building markers, interaction.
- [MINIMAP.md](MINIMAP.md) — the inset's chrome (title bar), what it shows at each rung, and the navigation model.

The Planetary canvas additionally draws two always-on chrome layers, neither lens-gated: **civic
chrome** — tiered population-centre conurbation markers, sized by scale and labelled at City+
(BL-083; model in [`../economy/POPULATION.md`](../economy/POPULATION.md), glyph in
[`ICONS.md`](ICONS.md)) — and **player-presence chrome** — a home-cluster ring + HQ star on
`home_body` (BL-085), echoed by a home halo around the body on the Solar rung. Full detail in
[PLANETARY.md](PLANETARY.md) and [SOLAR.md](SOLAR.md).

---

## Primary / minimap layout

The game window is divided into two regions:

- **Primary region** — the majority of the window. The active canvas fills this space.
- **Minimap region** — a fixed inset in the bottom-right corner, framed by its own chrome (a title bar above). The neighbouring canvas renders in the inset beneath it. (The overlay-lens toggles that once sat in a mode bar below the inset now live in a bottom-left overlay control strip.)

**Default state.** The game opens on the **corporation's home planet** (Kepler in
the hard-coded world): the Planetary screen is primary with the home body
selected, and the minimap shows that planet's Circumplanetary view. The opening
rung is the surface, not the system — the player starts looking at home.

---

## Navigation — the zoom ladder

The minimap is **context**: it always shows the rung one step *out* (zoom-out)
from the primary, never the rung you drill into. Movement along the ladder has
two clear directions:

> **Click-model change (Selection info element).** A *single* click now
> **selects** the clicked entity (filling the Selection info element, no view
> change); **descend/navigate moves to a double-click**. The "click to descend"
> wording below describes the navigate gesture, now bound to double-click. The
> minimap ascend gesture stays a single click (the minimap has no selection).
> See [SELECTION.md](SELECTION.md).

- **Descend (zoom in) by double-clicking a body in the primary canvas.** This is
  the load-bearing navigation interaction.
  - Solar primary, click a **planet** → its **Circumplanetary** view becomes primary.
  - Solar primary, click a **moon** → the **parent planet's** Circumplanetary view becomes primary, with the moon selected.
  - Circumplanetary primary, click the **planet or a moon** → that body's **Planetary** surface becomes primary.
  - Planetary is the bottom rung; tile clicks select a tile, they do not descend.
- **Ascend (zoom out) by clicking the minimap.** A minimap click promotes the
  zoom-out neighbour it is showing to primary (Planetary→Circumplanetary,
  Circumplanetary→Solar).

**Navigating** to a *different* body (double-click / 'go to') re-targets the
lower rungs (`active_body`) without forcing the primary to change rung except on
an explicit descend. **Selecting** a body (single-click) is independent: it fills
the Selection info element but changes neither the Active anchor nor the framing.

### Keyboard navigation

A keyboard-only "limited access" surface drives the same canvas actions as the
mouse, useful to real players and to the visual-verification harness alike. Every
binding resolves to a `canvas_command` (`src/ui/canvas_command.hpp`) applied via
`apply_canvas_command`; `app::process_events` owns the key→command map, and the
verify API's `verify.command(name)` routes through the *same* dispatch, so a
verification script reads as the player's key sequence. Bindings are ignored while
ImGui is capturing the keyboard (a text field has focus).

| Key | Command | Action |
|---|---|---|
| `Enter` | `descend` | Descend one rung (Solar → Circumplanetary → Planetary). |
| `Backspace` | `ascend` | Ascend one rung (Planetary → Circumplanetary → Solar). |
| `]` | `body_next` | Anchor the next body (by id) and re-frame it at the current rung. |
| `[` | `body_prev` | Anchor the previous body. |
| `←` `→` `↑` `↓` | `pan_left/right/up/down` | Pan the current rung's view by one step. |
| `=` / `+` | `zoom_in` | Zoom the current rung in. |
| `-` | `zoom_out` | Zoom the current rung out. |
| `L` | `lens_next` | Cycle the overlay lens forward. |
| `Shift`+`L` | `lens_prev` | Cycle the overlay lens backward. |
| `0` | `lens_clear` | Clear the overlay lens. |
| `F12` | *(capture)* | Save a screenshot. Capture is an app concern (it needs the renderer), not a `canvas_command`. |

### What the minimap shows at each rung

| Primary | Minimap (zoom-out neighbour) | Minimap title |
|---|---|---|
| Solar | *no rung above* — game branding | the game name (`Project Io`) |
| Circumplanetary | Solar | the **star name** |
| Planetary | Circumplanetary (parent planet) | the **planet name** |

The minimap title (chrome) is described in `MINIMAP.md`. The top rung (Solar) has
no zoom-out neighbour, so its minimap is a non-interactive branding placeholder
showing the game name rather than a canvas.

---

## Shared selection / view state

A small struct in `src/ui/ui_state.hpp`, held by `app`:

```cpp
enum class canvas_level { solar, circumplanetary, planetary };
enum class overlay_mode { none, supply, market, faction };

struct ui_state
{
    entity_id    active_body   = null_entity;            // drives the lower rungs
    entity_id    active_tile   = null_entity;            // set by tile click; consumed by later layers
    canvas_level primary_level = canvas_level::solar;    // which rung fills the window
    overlay_mode overlay       = overlay_mode::none;     // active overlay lens; toggled by the bottom overlay control strip (defaults to none — the plain canvas)

    bool show_tile_ledger = false;                       // owned by the nav pane, not the canvases

    // per-canvas pan/zoom (primary only; the minimap always shows default framing)
    float solar_zoom, solar_pan_x, solar_pan_y;
    float circum_zoom, circum_pan_x, circum_pan_y;
    float planetary_zoom, planetary_pan_x, planetary_pan_y;
};
```

Selection, hover, and pinning are drawn through a shared **highlight convention**
(`src/ui/highlight.hpp`) so they read the same on every canvas: white for the
selected entity, light blue for hover, amber for pinned (pinning not yet wired),
with `selected > pinned > hover` precedence. When several entities satisfy the
same condition at once (overlapping markers under the cursor), each canvas first
resolves a **single** choice — the candidate nearest the cursor, with entity id
breaking exact ties — so a tie highlights one entity, arbitrarily but stably,
rather than several. A reusable **focus helper**
(`src/ui/view_nav.hpp`) jumps the view to any entity — selecting it, choosing the
rung that frames it, and centring that rung — for the opening view and the
Explorer's "jump to".

`active_body` drives both lower rungs: the Circumplanetary view centres on
`active_body`'s planet (the body itself if it orbits the star directly, or its
parent if it is a moon — see `circumplanetary_anchor`), and the Planetary view
draws `active_body`. `show_tile_ledger` is shared housekeeping for the left
navigation pane; the canvases do not touch it.

---

## Implementation approach

Each canvas is a free function. The minimap-role canvases take an explicit
`is_minimap` flag (their click handling differs between primary and minimap);
the surface canvas is only ever primary.

```cpp
// solar_system_canvas.hpp / .cpp
void draw_solar_system_canvas(const world& w, ui_state& ui, ImVec2 origin, ImVec2 size, bool input_enabled, bool is_minimap);

// circumplanetary_canvas.hpp / .cpp
void draw_circumplanetary_canvas(const world& w, ui_state& ui, ImVec2 origin, ImVec2 size, bool input_enabled, bool is_minimap);
entity_id circumplanetary_anchor(const world& w, entity_id active_body);

// body_surface_canvas.hpp / .cpp  (always primary)
void draw_body_surface_canvas(const world& w, ui_state& ui, ImVec2 origin, ImVec2 size, bool input_enabled);
```

`app::render()` switches on `primary_level`: it draws the primary canvas
full-window, then — for the lower two rungs — draws the zoom-out neighbour into
the minimap inset, and finally the minimap chrome (title bar). Scale,
cell size, orbit radius, and element sizes are all derived from the region
`size` at draw time, so the same function renders correctly at both primary and
minimap dimensions. Fixed-pixel element sizes scale by `min(size.x, size.y) / 720`
with small floors; in-canvas labels are suppressed below ~320 px on the shorter
edge, while the minimap's chrome title bar is always shown.

**`input_enabled`** exists because the primary canvas fills the whole window
*behind* the minimap. A click in the overlapping bottom-right corner would
otherwise be handled twice. `render()` enables input for exactly one canvas per
frame — the minimap if the mouse is over the inset, the primary otherwise — and
only when an ImGui panel isn't capturing the mouse (`WantCaptureMouse`). Each
function still draws unconditionally; it just skips hover/click handling when
`input_enabled` is false.

---

## What is deferred

Economic and military data (supply routes, faction presence, convoy paths) are
added in later layers as overlay **lenses** on these canvases. The overlay
*mechanism* now exists as a building block — an overlay draw pass over each
canvas (`src/ui/overlay.hpp`), an `overlay_mode` value in `ui_state`, and a
bottom-left **overlay control strip** (`draw_overlay_controls`) wired to toggle
it (a default lens is active on load). Until those layers supply data, the draw
pass renders nothing — the active lens is named by the control strip, not an
on-canvas chip; the lens geometry (Layer 5 supply routes first) hangs off the
same pass.
