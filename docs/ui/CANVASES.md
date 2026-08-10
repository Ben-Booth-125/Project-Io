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
- **Minimap region** — a fixed inset in the bottom-right corner, framed by its own chrome (a title bar above, and the **lens mode bar** along its bottom edge — BL-093 moved the overlay-lens toggles onto the minimap itself; see `MINIMAP.md` / `LENSES.md`). The neighbouring canvas renders in the inset beneath it.

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

The shared struct is `ui_state` in `src/ui/ui_state.hpp` — the code is the
reference; no snippet is mirrored here (an earlier copy drifted badly). The
load-bearing members for the canvases: the `active_body`
navigation anchor, `selected_entity` (the Selection state, SELECTION.md),
`primary_level` (`canvas_level` — which rung fills the window), `overlay`
(`overlay_mode` — **fourteen** values: `none` plus thirteen lenses, LENSES.md;
the early `faction` mode was renamed **`country`**), and per-canvas pan/zoom.
The struct has since grown hover-card, construction, vision, and drill-down
state — see the header itself.

Selection, hover, and pinning are drawn through a shared **highlight convention**
(`src/ui/highlight.hpp`) so they read the same on every canvas: white for the
selected entity, light blue for hover, amber for pinned (pinning not yet wired),
with `selected > pinned > hover` precedence. When several entities satisfy the
same condition at once (overlapping markers under the cursor), each canvas first
resolves a **single** choice — the candidate nearest the cursor, with entity id
breaking exact ties — so a tie highlights one entity, arbitrarily but stably,
rather than several. A reusable **focus helper**
(`src/ui/view_nav.hpp`) jumps the view to any entity — selecting it, choosing the
rung that frames it, and centring that rung — for the opening view (and any future
jump-to affordance; the Explorer that first motivated it is superseded, see
`EXPLORER.md`).

`active_body` drives both lower rungs: the Circumplanetary view centres on
`active_body`'s planet (the body itself if it orbits the star directly, or its
parent if it is a moon — see `circumplanetary_anchor`), and the Planetary view
draws `active_body`. `show_tile_ledger` is shared housekeeping for the left
navigation pane; the canvases do not touch it.

---

## Terrain channels — composition and landform (BL-231)

This section is the **shared-ladder spec** for the landform render — it lives here, not
in PLANETARY.md, because the implementation is `hex_render` and serves two surfaces (the
canvas and the Selection band's neighbourhood view); PLANETARY.md § Layers points back
here (2026-07-31).

A tile's character has two axes ([TILES.md](../economy/TILES.md)), and the Planetary
canvas draws them on **two separate channels**. Both are **always-on chrome**, not an
`overlay_mode`: terrain identity is not something the player opts into, and landform's
movement-cost multiplier applies whether or not a lens is active.

| Axis | Channel | Source |
|---|---|---|
| **Composition** (what it is made of) | **Hue** — the flat hex fill | `ui::terrain_colour` |
| **Landform** (its physical shape) | **Relief tint** + **glyph** | `ui::landform_relief`, `ui::icons::landform` |

**Why two channels rather than one.** Lens tints composite over the terrain hue at
0.6–0.80 alpha, so a second signal carried *in that hue* is obliterated exactly when a
lens is on. This is the rule BL-226 established for the Continent lens's plate
boundaries and it applies here unchanged: the relief is composited **after** every lens
branch, and the glyphs are drawn over the finished fill in a contrasting ink
(`ui::contrast_ink`, picked by the fill's luminance so it reads over the whole palette).

**Why the landform channel splits in two.** The measured mix (`world_audit` § S3) decided
it. Plains and valley alone are ~95 % of land tiles, while every dramatic landform is
≤ 1.5 %:

- **Common ground — relief tint.** Plains is the untouched baseline; elevated ground lifts
  toward a warm highlight and sunken ground toward a cool shadow, on a small signed
  ordinal scale (mountain highest → canyon lowest). Deliberately subtle: it must read as
  light on terrain, never as a change of composition.
- **Dramatic landforms — glyph.** Mountain, canyon, crater and rift each draw a stroke-only
  silhouette ([ICONS.md](ICONS.md) § Landform). These are the ≤ 1.5 % set whose movement cost
  is ×1.3 or worse, so an invisible surprise there is expensive. A glyph on *every* tile
  would be far denser than any other glyph family and would fight the building silhouette
  for the hex centre.

**Contiguous runs are bridged (BL-232).** A run of the same linear landform draws as **one**
spanning marker rather than the same glyph repeated per tile — mountain as a chain of peaks, rift
as one continuous fissure, canyon as paired rims — reusing BL-172's road span/symmetry idiom (each
tile draws its own half of the shared edge, so halves meet at the midpoint with no cross-tile state
and the survey fog clips cleanly). A lone tile keeps its centred glyph, the role the road's centre
cap plays. Crater never spans. Contiguity was measured before the render was built (`world_audit`
§ S4): 71% of mountain and 81% of rift tiles have such a neighbour, so bridging fires on the
majority — while **no** tile in the system has all four, which cancelled the designed
"filled interior" case before it was written.

**The glyphs are named where the player looks.** Every tile hover card states
`composition · landform` and, on the plain canvas, the landform's movement cost. Before BL-232 the
hover named only the composition, so the glyph vocabulary could be learned only by clicking each
tile through to the Selection panel — which is not learning. Plains stays unnamed: it is the
untouched baseline in both channels.

**Suppression rules.** The glyph is skipped on a **built** tile (which already carries an
enlarged silhouette plus a corp emblem tag, and whose cost is already spent — elevation
matters when *siting*) and under the **Population/Opportunity** lenses (which claim the hex
centre for their own value mark, BL-135). The relief tint is likewise skipped on a built
tile, whose hex is swapped wholesale for its owner plate as an identity signal.

Both channels also render in the Selection band's zoomed tile-neighbourhood view, which is
why they live in `hex_render` rather than in the canvas — one implementation, so the two
surfaces cannot drift. Verified by `scripts/verify/landform_relief.lua`.

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

Little, on the lens front: **twelve of the thirteen lenses render today** —
Corporation, Country, Resource, Market, Population, Opportunity, Production,
Scarcity, Industry, Reach, Continent, and Supply-routes all draw real geometry
in the Planetary canvas's draw pass (`body_surface_canvas.cpp`, keyed on
`ui_state::overlay`; catalogued in LENSES.md). Only the original **Supply**
lens remains gated, on Layer-5 supply-route rendering. The lens *controls* are
the eight-glyph mode bar on the minimap (`draw_overlay_controls`,
`src/ui/overlay.cpp`) plus keyboard cycling; no lens is active on load — the
canvas opens unskinned. The vestigial `draw_canvas_overlay` pass in
`overlay.cpp` still renders nothing — the lens geometry grew inside the canvas
draw functions instead, which is where per-tile compositing had to happen.
