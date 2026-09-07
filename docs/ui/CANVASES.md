# Project Io — Primary Canvases

> **Settles:** which canvases exist and how the zoom ladder orders them · which
> canvas is primary and which one the inset frames · which press moves between
> rungs · how pan, zoom and minimap framing behave across the rungs · which canvas
> takes a press when two overlap · what view and selection state the three rungs
> share.
> **Not here:** what any single rung draws (SOLAR, CIRCUMPLANETARY, PLANETARY) ·
> the inset's own chrome (MINIMAP) · how the ground is rendered (RENDERING) · what
> the ground is made of and how landform reads (PLANETARY) · what each lens shows
> and which rungs it draws at (LENSES).
> **Confused with:** MINIMAP.md, PLANETARY.md.

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
(BL-083, civic markers; model in [`../economy/POPULATION.md`](../economy/POPULATION.md), glyph in
[`ICONS.md`](ICONS.md)) — and **player-presence chrome** — a home-cluster ring + HQ star on
`home_body` (BL-085, player presence), echoed by a home halo around the body on the Solar rung.
Full detail in [PLANETARY.md](PLANETARY.md) and [SOLAR.md](SOLAR.md).

---

## Primary / minimap layout

The game window is divided into two regions:

- **Primary region** — the majority of the window. The active canvas fills this space.
- **Minimap region** — a fixed inset in the bottom-right corner, framed by its own chrome (a title bar above, and the **lens mode bar** along its bottom edge — the overlay-lens toggles live on the minimap itself; see `MINIMAP.md` / `LENSES.md`). The neighbouring canvas renders in the inset beneath it.

**Default state.** The app opens on the **main menu**, not a canvas: main menu →
New World wizard → "Begin" hands over to `in_game` (the `app_screen` flow —
see [STARTUP.md](STARTUP.md)). The **first in-game view** is then the
**corporation's home planet**: the Planetary screen is primary with the home
body selected, and the minimap shows that planet's Circumplanetary view. The
opening rung is the surface, not the system — the player starts looking at home.

---

## Navigation — the zoom ladder

The minimap is **context**: it always shows the rung one step *out* (zoom-out)
from the primary, never the rung you drill into. Movement along the ladder has
two clear directions:

> **Click model (Selection info element).** A *single* click **selects** the
> clicked entity (filling the Selection info element, no view change);
> **descend/navigate is a double-click**. The minimap ascend gesture is a single
> click (the minimap has no selection). See [SELECTION.md](SELECTION.md).

- **Descend (zoom in) by double-clicking a body in the primary canvas.** This is
  the load-bearing navigation interaction.
  - Solar primary, double-click a **planet** → its **Circumplanetary** view becomes primary.
  - Solar primary, double-click a **moon** → the **parent planet's** Circumplanetary view becomes primary, with the moon selected.
  - Circumplanetary primary, double-click the **planet or a moon** → that body's **Planetary** surface becomes primary.
  - Planetary is the bottom rung; tile clicks select a tile, they do not descend.
  - A solar→surface jump is therefore always a two-step drill (system → local →
    surface) that reads the same way every time.
- **Ascend (zoom out) by clicking the minimap.** A minimap click promotes the
  zoom-out neighbour it is showing to primary (Planetary→Circumplanetary,
  Circumplanetary→Solar).

**Navigating** to a *different* body (double-click / 'go to') re-targets the
lower rungs (`active_body`) without forcing the primary to change rung except on
an explicit descend. **Selecting** a body (single-click) is independent: it fills
the Selection info element but changes neither the Active anchor nor the framing.

### Shared view controls

Pan and zoom belong to the **primary** slot. On the two upper rungs the middle
mouse button pans and the scroll wheel zooms, anchored at the cursor so the point
under it stays fixed, and a bottom-centre **scale bar + zoom slider**
(`ui::draw_scale_zoom_overlay`, `src/ui/canvas_scale.hpp`) sets the same factor —
dragging **right zooms in**, left zooms out. Framing scales; element sizes (body
radii, labels, selection outlines) hold their pixel size.

**A canvas in the minimap slot always renders its default framing.** Pan and zoom
apply only while a canvas is primary, so the inset stays a stable piece of context
rather than a second view the player has to keep. What a rung's default framing
*is*, and which `ui_state` members carry its pan/zoom, is that rung's own business
— see [SOLAR.md](SOLAR.md), [CIRCUMPLANETARY.md](CIRCUMPLANETARY.md) and
[PLANETARY.md](PLANETARY.md).

**Input precedence.** At most one canvas handles input per frame, and the two
regions are not the same shape. The whole minimap **box** blocks the primary
behind it, while only the **inset** canvas inside that box takes minimap input —
so a press lands on the minimap over the inset, on the primary over the rest of
the window, and on neither over the box's own chrome bands. An ImGui panel
capturing the mouse suppresses both. The mechanism is `input_enabled`
(§ Implementation approach).

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
| Solar | *no rung above* — the galaxy seen from the homeworld | `Galaxy` |
| Circumplanetary | Solar | the **star name** |
| Planetary | Circumplanetary (parent planet) | the **planet name** |

The minimap title (chrome) is described in `MINIMAP.md`. The top rung (Solar) has
no zoom-out neighbour, so its minimap is a non-interactive sky view rather than a
canvas (MINIMAP.md § The top rung).

---

## Shared selection / view state

The shared struct is `ui_state` in `src/ui/ui_state.hpp` — the code is the
reference; no snippet is mirrored here. The load-bearing members for the
canvases: the `active_body` navigation anchor, `selected_entity` (the Selection
state, SELECTION.md), `primary_level` (`canvas_level` — which rung fills the
window), `overlay` (`overlay_mode` — the active lens, LENSES.md § Roster), and
per-canvas pan/zoom. The struct also carries hover-card, construction, vision, and
drill-down state — see the header itself.

Selection, hover, and pinning are drawn through a shared **highlight convention**
(`src/ui/highlight.hpp`) so they read the same on every canvas: white for the
selected entity, light blue for hover, amber for pinned (a tier no surface sets —
`EXPLORER.md`), with `selected > pinned > hover` precedence. When several
entities satisfy the same condition at once (overlapping markers under the
cursor), each canvas first resolves a **single** choice — the candidate nearest
the cursor, with entity id breaking exact ties — so a tie highlights one entity,
arbitrarily but stably, rather than several. A reusable **focus helper**
(`src/ui/view_nav.hpp`) jumps the view to any entity — selecting it, choosing the
rung that frames it, and centring that rung — for the opening view and any
jump-to affordance.

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
otherwise be handled twice. `render()` enables input for at most one canvas per
frame: the primary is disabled over the **whole** minimap box, and the minimap is
enabled only over the **inset** canvas within it (the box minus its title bar and
lens mode bar), so the chrome bands enable neither. Both stay disabled while an
ImGui panel is capturing the mouse (`WantCaptureMouse`). Each
function still draws unconditionally; it just skips hover/click handling when
`input_enabled` is false.
