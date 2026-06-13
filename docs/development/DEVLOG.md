# Project Io — Development Log

Entries are newest-first. Each entry covers one development session and records what was built, what in-session decisions were made, and what was left open. Decisions that affect the whole project permanently belong in TECH_FOUNDATIONS or a dedicated ADR; this log is for session-scoped choices and progress notes.

---

## 2026-06-13 — Canvas zoom ladder + Circumplanetary canvas

**Status:** Complete. The two-canvas binary swap is replaced by a three-rung zoom ladder (Solar → Circumplanetary → Planetary) with click-to-descend / minimap-to-ascend navigation. Builds and runs; the game opens on the home planet's surface.

### What was built

**Circumplanetary canvas (new middle rung).**
`src/ui/circumplanetary_canvas.{hpp,cpp}` — a top-down view of a single planet (the *anchor*) and its moons: the anchor at centre (enlarged), an orbital ring and dot per moon, selection outline on `active_body`, hover tooltips, and primary-only pan/zoom (`circum_zoom`, `circum_pan_x/y` in `ui_state`). The free function `circumplanetary_anchor(world, active_body)` resolves the anchor — the body itself if it orbits the star, or its parent planet if it is a moon — and is shared with `app::render()` for the minimap title.

**The zoom ladder replaces the binary swap.**
`ui_state::surface_is_primary` (bool) became `ui_state::primary_level` (`enum class canvas_level { solar, circumplanetary, planetary }`). The minimap is now pure **context**: it shows the rung one step *out* from the primary. Navigation:
- **Descend** by clicking a body in the primary canvas (Solar→Circumplanetary, Circumplanetary→Planetary). Clicking a moon on the Solar canvas opens its parent's circumplanetary view with the moon selected.
- **Ascend** by clicking the minimap.

The solar and circumplanetary canvases gained an explicit `bool is_minimap` parameter (their click handling differs between primary and minimap). The body-surface canvas is now only ever primary — its minimap branch and `surface_is_primary` writes were removed.

**Star as a body entity.**
`body_type::star` added to `components.hpp`. `make_hard_coded_world` creates **Helios** at the system centre (radius 0, stationary) and stores it in `world.star_body`. The solar canvas now draws the star through the normal body pass (new star style: 18 px, yellow) instead of a hard-coded circle, labels it, and excludes it from descend clicks (it has no circumplanetary view). Zero-radius bodies are skipped in the orbital-ring pass.

**Home planet start.**
`world.home_body` added and set to **Kepler**. `app::run()` opens with `active_body = home_body` and `primary_level = planetary` — the game starts on the home planet's surface, with Kepler's circumplanetary view in the minimap.

**Minimap chrome.**
`app::render()` now draws a title bar above the inset and a placeholder mode bar (three dim dots) below it, with the inset canvas between. The title names what the minimap shows: the **star name** (primary Circumplanetary), the **planet name** (primary Planetary), or the **game name `Project Io`** at the top rung, where the inset is a dark branding fill (no canvas, non-interactive).

**Docs reconciled to the ladder.**
`CANVASES.md` rewritten (binary swap → three-rung ladder, context minimap, new `ui_state`/signatures). New `CIRCUMPLANETARY.md`. `SOLAR.md` (body click descends; star is an entity), `PLANETARY.md` (surface is always primary), `LAYOUT.md` (canvas area / minimap / companion list), `MINIMAP.md` (top rung = game name), and `CLAUDE.md` (CANVASES entry) updated.

### In-session decisions

**Minimap is context-only; descend via the primary, not the minimap.**
Chosen over an up/down tabbed minimap. The minimap always shows the zoom-out neighbour; the player descends by clicking a body in the primary canvas. This keeps the bottom mode bar free for future overlay modes rather than spending it on level navigation.

**Star as an entity, not a `world.star_name` string.**
Keeps the star uniform with every other body (name, position, style, future selectability) at the cost of a new enum value and a skip in the ring pass. Name "Helios" is a placeholder, consistent with the original body names.

**Open:** the mode bar has no function yet (placeholder); the ladder navigation was verified by screenshot (opens on Kepler surface, minimap shows Kepler + Selene), not yet by interactive click-through of every rung.

---

## 2026-06-13 — TODO triage and UI shell polish

**Status:** Complete. Cleared the difficulty-1/2 TODO items; the asteroid-belt ring (difficulty 4) and the two deferred items (label shimmer, menu definition) remain.

### What was built

**TODO difficulty ratings.**
`docs/development/TODO.md` was annotated with a 1–6 difficulty scale (1 trivial → 5 very hard, 6 deferred). The four easiest items were implemented this session and removed from the list.

**Pause as a toggle.**
The time-controls pause button (`App.cpp`) now toggles. `app` gained `m_prev_speed`: pressing pause stores the current speed and sets speed 0; pressing it again restores the stored speed. Speed buttons 1–5 also update `m_prev_speed` so a later pause/unpause round-trips correctly. The button label flips to `>` while paused and `II` while running, so it reflects its toggle state.

**Quarter label.**
The system-tick readout was relabelled: economy ticks now read `Quarter N` (previously `Econ`, briefly `Q`), aligning with the quarterly econ-tick model in `sim_loop.hpp`.

**Header resource strip simplified.**
`header_panel.cpp` dropped the four named placeholder resources (Ore/Metal/Fuel/Goods) for a single `STOCKPILE 0` aggregate placeholder, per the prototype's "deliberately scarce" header intent in HEADER.md.

**Default solar view ≈ 5 AU + scale bar and zoom slider.**
`app::run()` sets the initial `solar_zoom` so the opening view spans roughly 5 AU (computed from the outermost orbit). The scroll-wheel zoom-out is capped at 50 AU (min zoom derived from `max_radius_au / 50`).

A bottom-centre overlay on the primary solar canvas (`solar_system_canvas.cpp`) replaces the earlier `[-] X.X AU [+]` text row:
- A **fixed-width scale bar** (8% of canvas width) with end ticks. Its label shows the spanned distance dynamically to two decimals (`%.2f AU`) at the current zoom.
- A **logarithmic zoom slider** offset to the right of the bar, ranging 0.5 AU (zoomed in) to 50 AU (zoomed out), with no value text — the bar already reports distance.

The overlay is a borderless, fill-free, padding-free ImGui window anchored so the scale bar is screen-centred and the slider sits to its right.

### In-session decisions

**Scale overlay drawn before the `input_enabled` early-out.**
The scale/slider block sits before the canvas's `if (!input_enabled) return;`. Drawing it after would cause a one-frame flicker loop: hovering the slider sets `WantCaptureMouse`, which disables canvas input the next frame, which would skip the draw. Placing it earlier and building it as a real ImGui window keeps it persistent while still handling its own input.

**Fixed-width bar with dynamic distance, not a round-number bar.**
An earlier pass picked the largest "nice" AU span (0.1/0.2/0.5/1/…) that fit within 8% of the width. Changed to a fixed 8%-width bar whose AU value floats to two decimals — simpler and reads as a steady on-screen ruler whose label changes with zoom.

---

## 2026-06-13 — Layer 2: planetary view, hex tiles, procedural terrain

**Status:** Complete. Horizontal wrap rendering, pan/zoom min/max enforcement, and grid size expansion via procedural generation are the main open items.

### What was built

**Doc restructure — CANVASES.md → SOLAR.md + PLANETARY.md.**
`docs/ui/CANVASES.md` was refactored from a monolithic canvas spec into a thin overview document. Per-canvas detail moved to two new files: `docs/ui/SOLAR.md` (Solar System Canvas) and `docs/ui/PLANETARY.md` (Body Surface / Planetary Canvas). CANVASES.md retains shared concerns: primary/minimap layout, region sizing, selection state struct, and the `input_enabled` dispatch model.

**Hex tiles on the planetary canvas.**
`body_surface_canvas.cpp` was rewritten from a rectangular grid to a **pointy-top hexagonal grid** using odd-r offset coordinates. Tile centres are computed via `hex_local_centre(col, row, hex_size)`. Drawing uses `ImDrawList::AddConvexPolyFilled` for filled hexes and `AddPolyline` for the selection outline. Hit-testing uses distance-to-centre (< circumradius), which is approximate but sufficient for usability. A clip rect prevents hexes bleeding over the title bar or into the solar canvas.

**Water terrain type.**
`terrain_type::water = 4` added to `components.hpp`. Colour: `(40, 80, 160)` deep blue. `terrain_name()` and `terrain_colour()` updated. No deposits, high habitability modifier. Tile data for existing hard-coded bodies is unchanged for now — water placement is deferred until grids expand.

**Pan/zoom on the planetary canvas.**
`ui_state` gained `planetary_zoom`, `planetary_pan_x`, `planetary_pan_y`. Controls match the solar canvas: middle mouse button pans, scroll wheel zooms anchored at the cursor. Both primary view and minimap use the same `planetary_zoom` value so they stay in sync; only the primary applies pan offset.

**Zoom reference frame: fit-by-height.**
The planetary `hex_size` is computed from `fit_by_y` only (canvas height / grid height) rather than `min(fit_by_x, fit_by_y)`. This means zoom=1 is defined as "full grid height fills the canvas," and zoom=4/3 is exactly "3/4 of the grid height visible" — a ratio that holds for any canvas size, including the minimap. Default `planetary_zoom = 4.0f/3.0f`.

**Solar body click no longer switches canvas.**
Previously clicking a body in the solar view set `surface_is_primary = true`. That was changed: a body click now only sets `active_body`. The planetary minimap updates immediately to show the new body; the player navigates to the planetary primary view by clicking the minimap. SOLAR.md updated to document this.

**Procedural tile generation — Kepler, Cinder, Selene.**
`hard_coded_world.cpp` gained a `generate_body_tiles()` function that:
1. Seeds the BFS queue with the **entire centre row** so the ocean grows as a horizontal equatorial band (poles are land).
2. BFS expands with shuffled neighbour order for an irregular coastline, stopping at 60% water coverage. Horizontal wrap is handled in `hex_neighbors()` via column modulo.
3. Land tiles draw terrain from a per-body weighted table (barren/rocky/icy/volcanic). Hazard, habitability, and resource deposits are set by terrain type with mild random jitter.

Three bodies received generated tile grids (replacing the earlier small placeholder grids):
- **Kepler** (Earth analogue): 42 × 174. Barren/rocky dominant, some icy and volcanic. Replaced the hand-authored 4×4 tile table; buildings now attach to the first two land tiles found in raster order.
- **Cinder** (Mercury analogue): 36 × 186. Volcanic dominant (45%), then barren and rocky.
- **Selene** (Kepler's moon): 18 × 92. Barren dominant; smaller grid appropriate for a moon.
- **Vesta** retains its 3×3 hand-authored tiles — small enough to curate manually.

`tile_spec` / `create_tile` helpers are retained for Vesta only. The `generate_body_tiles()` function returns a flat `vector<entity_id>` in raster order (`row*gw+col`) for building placement lookup.

**Two implementation TODOs filed in source.**
- `body_surface_canvas.cpp`: horizontal wrap / infinite scroll — describes the triple-draw approach needed for seamless east/west panning.
- `hard_coded_world.cpp`: generation rules — body ~2× wide as tall (both hemispheres), polar row truncation, zoom min (~12 tiles wide) and zoom max (~12 tiles beyond total grid height).

### In-session decisions

**Pointy-top hexes, odd-r offset.**
Chosen over flat-top because rows read as latitude bands, which aligns naturally with the horizontal ocean / polar land model. Odd rows are shifted right. Column wraps for horizontal continuity; rows do not (poles are boundaries).

**Water is an equatorial band, not a blob.**
Original BFS used a single random interior seed point, producing a roughly radial blob. Changed to seeding the full centre row simultaneously; BFS then expands symmetrically up and down, producing a horizontal ocean with irregular north/south coastlines. This is consistent with the design intent that poles are at the top and bottom of the grid.

**Grid sizes are slightly varied from the 40×180 target.**
The 40×180 ratio is a design guideline, not a precise spec. Sizes were deliberately varied (42×174, 36×186, 18×92) to reflect that real bodies won't be uniform. The aspect ratio (~4:1 for planets, ~5:1 for Selene) is the constraint, not the exact count.

**Zoom reference frame changed from min-fit to height-fit.**
The earlier 4/3 multiplier on the auto-fit zoom had no meaningful effect because the wide planetary grid is always width-constrained. Redefining zoom=1 as "full grid height fills canvas" makes zoom=4/3 a geometrically correct "3/4 height visible" that works at both primary and minimap scale without per-canvas adjustment.

**Both primary and minimap use the same `planetary_zoom`.**
The solar canvas minimap always shows the default (full-system) framing regardless of solar zoom state. For the planetary canvas, the user preference is that primary and minimap are tied — both show the same zoom level. Only pan is suppressed on the minimap (it centres on the grid midpoint).

**Kepler buildings re-attached by raster scan after generation.**
The 4×4 hand-authored Kepler tiles referenced specific entity IDs. After switching to procedural generation the IDs are no longer predictable. Rather than authoring specific target coordinates (which could land on water), buildings are attached to the first two non-water tiles found scanning left-to-right, top-to-bottom. This is an acceptable heuristic for a prototype where building placement logic is deferred.

### Open items

- **Horizontal wrap rendering** — east/west pan currently shows blank space beyond the grid edge. The canvas TODO describes the triple-draw approach.
- **Zoom min/max not enforced** — the generation TODO documents the intended limits (12 tiles wide min, 12 tiles beyond total height max). Currently unclamped beyond a 0.1 floor.
- **Grid sizes for other bodies** — Bastion, Halo, Ochre, Veld, Ceres, Pallas, Forge, Cyra, Mote, Mote retain small placeholder grids (2×2 to 4×4). Expansion deferred until procedural generation is introduced.
- **Water placement in existing small grids** — Vesta and all backdrop bodies have no water tiles. The `water` terrain type exists but is not used in any current tile data.
- **Building placement** — raster-scan heuristic for Kepler is a placeholder. Proper authored placement should be revisited when the extraction layer (Layer 3) designs building site selection.

---

## 2026-06-13 — UI shell placeholders, orbital motion, and canvas pan/zoom

**Status:** Complete. Canvas refinement continues; asteroid belt (as a ring) and the parked UI items remain open — see `docs/development/TODO.md`.

### What was built

- **UI shell docs** — expanded `docs/ui/LAYOUT.md` with profile, header, explorer, minimap, and a UI-popup note, each linking its own spec. New stub specs: `PROFILE.md`, `HEADER.md`, `EXPLORER.md`, `MENU.md`, `MINIMAP.md`, `TIME_CONTROLS.md`. Gated behind LAYOUT.md (not added to the CLAUDE.md authoritative set, by request).
- **Placeholder panels** — `src/ui/profile_panel`, `header_panel`, `explorer_panel`: fixed ImGui panels matching the `nav_pane` style. Profile (top-left, portrait + name placeholder), header (budget + scarce resource strip, zeroed), explorer (empty pin list). `nav_pane` gained a `top_offset` so it sits below the profile. Wired into `app::render()`.
- **Orbital motion** — `body_component` gained `parent`, `orbital_angular_velocity_rad_per_day`. New `src/world/orbital_system`: `advance_orbits` (advances angles by elapsed days, freezes when paused) and `kepler_angular_velocity` (speed from radius via Kepler's third law). `sim_loop` exposes continuous `elapsed_days()`; the app loop advances orbits per-frame.
- **Sol-approximation world** — `hard_coded_world.cpp` rebuilt to ~6 planets (Cinder/Veld/**Kepler**/Ochre/Bastion/Halo, real-AU spacing), 4 parented moons, and 3 belt asteroids (**Vesta** repurposed from moon → asteroid, keeps its tiles/market). Only Kepler and Vesta carry tiles; the rest are backdrop bodies. Default surface selection now prefers a tiled body.
- **Canvas pan/zoom + labelling** — Solar System Canvas gained cursor-anchored scroll zoom and middle-drag pan (primary view only; the minimap stays at default framing). Positions/rings scale with zoom; element sizes do not. Star bumped to 1.5x. Planets/asteroids labelled permanently, moons on hover. View state (`solar_zoom`, `solar_pan_x/y`) lives in `ui_state`. Labels track the live body position; a residual shimmer (bitmap-font sub-pixel artifact) is left unfixed and logged in `TODO.md`.

### In-session decisions

**Moons are parented, not flat orbits.** When asked, chose to add a `parent` field so a moon orbits its planet (composed at draw time) and tracks it as it moves, rather than giving moons their own star orbit (which would drift apart under animation). Moon orbital radii are a small *visible* offset, **not** true scale — real moon distances render on top of the planet.

**Label shimmer is a font-rasterization artifact, left as a known bug.** Bodies move smoothly (continuous `elapsed_days`), but text labels shimmer. Root cause: the default ImGui font is a bitmap atlas with no sub-pixel positioning, so glyphs are crisp only at integer coordinates — an anti-aliased body dot reads smooth at any fraction, but text at the same fractional coordinate shimmers as the fraction changes each frame. Two attempts were explored and reverted: (1) sampling the label position once per sim tick — wrong, it made the label hold still then hop to catch the still-moving body, a positional jump amplified by zoom (the `sim_tick` canvas parameter added for this was removed); (2) rounding the label's screen coordinate to whole pixels — crisp but it made the label step 1px at a time. Final state: the label draws at its live fractional position (fluid motion, residual shimmer). The durable fix is font oversampling / sub-pixel rendering; deferred and logged under *Known bugs* in `TODO.md`.

**Pan/zoom on the solar canvas only.** The Body Surface Canvas keeps "no pan/zoom" (deferred until large procedural bodies exist). Zoom keeps element sizes constant per the request — only framing changes.

**Ledgers start closed (policy).** Codified in `MENU.md` and `LAYOUT.md`: every ledger defaults closed on a fresh session; `show_tile_ledger` now defaults `false`. New ledgers must follow.

**Asteroid belt deferred.** Intended as a single thick, translucent textured *ring* (not orbiting body dots) with ~3 notable asteroids that remain selectable bodies; the belt itself is not a body. Recorded in `TODO.md`; current three asteroids are placeholders.

**Header currency placeholder.** Used `Cr` (credits) rather than a currency glyph — ImGui's default font has no `₡`/symbol coverage beyond ASCII.

---

## 2026-06-13 — Layer 2: Primary canvases

**Status:** Complete. Canvas visual refinement and Layer 3 (extraction and production) are next.

### What was built

- `src/ui/ui_state.hpp` — `ui_state` struct shared by both canvases: `active_body`, `active_tile`, `surface_is_primary`, plus `show_tile_ledger` (owned by the nav pane).
- `src/ui/solar_system_canvas.hpp` / `.cpp` — Top-down system view: star, per-body orbital rings, type-coloured body dots, labels, selection outline, hover tooltip. Draws to the ImGui background draw list. Coordinate mapping per CANVASES.md (y negated, `scale = min_dim·0.45 / max_radius_au`).
- `src/ui/body_surface_canvas.hpp` / `.cpp` — Tile grid for `active_body`: terrain-coloured cells with 1 px gaps, building markers, selection outline, title bar, and a hover tooltip (suppresses zero deposits).
- `src/ui/nav_pane.hpp` / `.cpp` — Left navigation pane: fixed full-height column, ten numbered tab slots, only the **Tile Ledger** wired (parked at slot 8). Exposes `nav_pane_width`.
- `src/world/components.hpp` — `orbital_angle_rad` added to `body_component`; Kepler `1.05`, Vesta `3.93` authored in `hard_coded_world.cpp`.
- `src/ui/tile_inspector` — renamed window to **Tile Ledger**; now takes `bool* p_open` so it fully closes (X button) rather than collapsing. Toggled by the nav tab.
- `src/core/sim_loop` — rebuilt as a **three-layer clock**: sim tick → day tick → econ tick, with a runtime speed multiplier (pause + 1x–5x).
- `src/core/app` — fixed top-right **system tick** readout (Day/Econ) and a **speed-control** panel below it; nav pane and Tile Ledger wired into `render()`; F12 screenshot capture (`save_screenshot` via `SDL_RenderReadPixels` + `SDL_SaveBMP`).
- `tools/capture.ps1` — build → launch → F12 → BMP→PNG dev-loop wrapper.
- `.claude/settings.local.json` — `acceptEdits` default plus an allowlist for the build/screenshot loop (gitignored).

### In-session decisions

**Body click always brings the surface forward.**
CANVASES.md contradicted itself: the layout section states the intent ("click a body, arrive at its surface — a single action") while the interaction bullet made the swap conditional on the Solar System Canvas already being the minimap. Implemented the *intent*: clicking a body sets `active_body` and `surface_is_primary = true` unconditionally. CANVASES.md updated to match.

**`input_enabled` added as a 5th canvas parameter.**
The primary canvas fills the whole window *behind* the bottom-right minimap, so a click in the overlap would otherwise be processed by both canvases. `app::render()` routes input to exactly one canvas (mouse-in-minimap → minimap, else primary), gated by `WantCaptureMouse` so ImGui panels take precedence. Deviates from the 4-arg signature in the spec; documented.

**Canvases drawn to the ImGui background draw list.**
Keeps the debug/overlay windows (nav pane, tick, ledger) on top with no z-order management, and lets manual hit-testing coexist with ImGui. No separate minimap draw path — element sizes scale by `min_dim/720` with floors, and labels/titles are suppressed below ~320 px so the minimap stays readable.

**Minimap / right-column sizing.** `mm_w = max(240, 0.20·min(window w,h))`, `mm_h = mm_w·0.75` (the 240×180 4:3 ratio). The system-tick and speed panels reuse `mm_w` so the right column stays aligned; each is ~⅓ of `mm_h` tall.

**Three-layer tick model with derived pacing.**
`sim_ticks_per_day = 12`, `econ_tick_days = 90` (three 30-day months → quarterly economy resolution). Real-time pacing comes from one constant, `seconds_per_day_1x = 6.0`, so 1x = 6 s/day and **3x ≈ 2 s/day** as requested; 12 sim ticks/day gives 6 steps/sec at 3x — fine-grained enough to interpolate fluid motion later. Speed 0 = paused (drops the accumulator so unpausing doesn't fast-forward). All calendar/pacing values are `static constexpr` tunables — explicitly tentative.

**`init.lua` config repurposed.** The unused `sim_hz` / `econ_per_sec` were retired in favour of `default_speed` (1x–5x), which `run()` reads via `set_speed`. Closes the Layer 0/1 open item about wiring `config` to the loop. The calendar itself now lives in C++.

**Nav pane is a launcher, ledger stays a window.** Tabs toggle panels rather than docking content; the Tile Ledger remains a floating, movable window (kept "as-is") but closable. Slot numbering and the slot-8 placement are temporary — menu layout is deliberately out of scope while canvas work takes priority.

**Screenshot tooling: in-app capture over external screengrab.** F12 dumps the exact composited backbuffer to `build/Debug/screenshots/`. BMP (not PNG) to avoid adding an `SDL_image` dependency; the wrapper converts to PNG via `System.Drawing`. Permissions use a wrapper-script allowlist because shell permission rules are prefix-matched and can't scope by directory.

### Corrections made during session

`tools/capture.ps1` used an em dash in a string literal; Windows PowerShell 5.1 reads BOM-less files as ANSI and the multibyte character broke parsing. Replaced with ASCII.

Nav pane labels were clipped to a single glyph. Cause: `-1.0f` was passed as the `Selectable` width — unlike `Button`, `Selectable` treats a nonzero `size.x` as a *literal* width, producing a near-zero-width box. Fixed by deriving the width from `GetContentRegionAvail().x`. (Widening the pane had no effect until this was found.)

### Open items

- **Canvases render full-window behind the nav pane and top-right panels.** The leftmost sliver of the solar view and the top-right corner are occluded. Clean follow-up: inset the primary canvas to start at `nav_pane_width` and below the tick/speed column. `nav_pane_width` is already exposed for this.
- **Nav slot layout is temporary** — Tile Ledger at slot 8, others empty placeholders. Revisit when the menu set is designed.
- **Calendar values tentative** — no year/month/day date display yet; the tick widget shows raw Day/Econ counts.
- **Lua "alive" indicator dropped** from the fixed tick widget to fit the ~⅓-minimap height; restore with a slightly taller widget if wanted.
- `m_` member prefix still unaddressed in DEVELOPMENT_PRACTICES (carried from Layer 0/1).

---

## 2026-06-13 — Layer 1: ECS data model

**Status:** Complete. Layer 2 (extraction and production) is next.

### What was built

- `src/world/entity.hpp` — `entity_id` typedef (`uint32_t`); `null_entity = 0` sentinel.
- `src/world/components.hpp` — Shared enums (`resource_type`, `terrain_type`, `body_type`, `building_type`) and all six Layer 1 component structs: `tile_component`, `body_component`, `building_component`, `stockpile_component`, `market_component`, `unit_component`. Resource deposits and market arrays are `std::array<float, resource_count>` indexed by `resource_type`.
- `src/world/world.hpp` / `world.cpp` — ECS registry: one `std::unordered_map<entity_id, Component>` per component type, `create_entity()` allocating monotonically increasing IDs.
- `src/world/hard_coded_world.hpp` / `hard_coded_world.cpp` — `make_hard_coded_world()` populating two authored bodies: Kepler (4×4 planet, 1.0 AU, iron/silicate deposits, two buildings) and Vesta (3×3 moon, 5.2 AU, ice/rare-metal deposits, one building). ~200 authored float values across 25 tiles, 2 markets, 3 buildings, 1 unit stub.
- `src/ui/tile_inspector.cpp` — Layer 1 ImGui panel: body selector combo, scrollable tile table (terrain, hazard, habitability, per-resource deposits), buildings list, market supply/demand/price table. Serves as the functional specification for the production tile canvas and market ledger.
- `src/core/app` updated — `world m_world` member added; `make_hard_coded_world()` called at startup; `ui::draw_tile_inspector(m_world)` called each frame.

### In-session decisions

**ECS over OOP for the data model.**
The developer chose ECS explicitly. Entities are plain `uint32_t` IDs; all data lives in per-component maps on the `world` registry. No base classes, no virtual dispatch. Layer 1 has no systems yet — only data.

**`std::unordered_map` for component storage.**
Dense arrays would require a stable maximum entity count upfront. Sparse maps are correct for the prototype's authored, bounded world and keep entity creation trivial. Revisit if component iteration becomes a hot path in later layers.

**Four resource types for prototype scope.**
`iron_ore`, `ice`, `silicates`, `rare_metals` — enough to produce meaningful supply/demand divergence between bodies without expanding the market or extraction logic prematurely.

**`resource_count` constant from enum sentinel.**
`resource_type::count` used as array size via `static_cast<std::size_t>`. Avoids a separate manifest constant; adding a new resource type automatically sizes all arrays correctly.

**`tile_spec` local struct in `hard_coded_world.cpp`.**
A private helper struct used only during world construction — not part of the runtime data model. Keeps the authored values readable as a flat table without polluting `components.hpp`.

**Market prices seeded to `base_price` at init.**
Prices are set equal to `base_price` at construction so the market is in a neutral state before the first economy tick runs price resolution (Layer 3). No placeholder zeroes that would require special-casing.

### Corrections made during session

`SDL3::SDL3main` removed from `target_link_libraries` and `#include <SDL3/SDL_main.h>` removed from `main.cpp`. The SDL_main entry-point shim is only needed for Windows GUI subsystem builds; CMake defaults to the console subsystem, making it redundant. This also resolved the `SDL3::SDL3main` target-not-found error produced by the Visual Studio generator when building against FetchContent SDL3.

`onelua.c` added to the Lua exclusion list in `CMakeLists.txt`. The Lua repository includes this single-file amalgamation which re-includes `lua.c`, causing a duplicate `main` symbol at link time. Excluding it alongside `lua.c` and `luac.c` resolves the error.

### Open items

- `m_` member prefix convention: carried forward from Layer 0, still unaddressed in DEVELOPMENT_PRACTICES. Confirm before Layer 2 adds more types.
- `scripts/init.lua` `config` table not yet wired to `sim_loop` constructor. Still uses hardcoded defaults.
- `unit_component.owner` is `null_entity` — the player corporation entity is not yet defined. Needs a home before Layer 5 (budget) assigns revenue to a faction.

---

## 2026-06-13 — Layer 0: Engine scaffolding

**Status:** Complete. Layer 1 data model begun by end of session.

### What was built

- `CMakeLists.txt` — FetchContent build for SDL3 (`release-3.2.0`), Lua 5.4 (`v5.4.7`), sol2 (`v3.3.0`, header-only), Dear ImGui (`v1.91.6` with SDL3 + SDLRenderer3 backends).
- `src/core/sim_loop` — Fixed-timestep loop at 20 Hz using an SDL `GetTicks` accumulator. Economy tick fires every N sim steps (default: 20, i.e. 1 Hz). Spiral-of-death clamp at 8 steps.
- `src/core/app` — SDL3 window and renderer, ImGui initialised, render loop calling `sim_loop::tick()` each frame.
- `src/scripting/lua_state` — sol2 wrapper; `safe_script_file` used for all file loads per TECH_FOUNDATIONS constraint on unprotected sol2 calls.
- `scripts/init.lua` — Loaded at startup; prints confirmation and defines a `config` table for future use.
- `.gitignore` — Covers build output, CMake artifacts, IDE files, compiled binaries.
- Engine Status ImGui panel — displays live sim tick and econ tick counters to confirm both loops are running.

### In-session decisions

**sol2 integrated as header-only, bypassing its CMake.**
sol2's own `CMakeLists.txt` runs `find_package(Lua)` which conflicts with our FetchContent-built Lua. Using `FetchContent_Populate` and manually adding `${sol2_SOURCE_DIR}/include` to the game target's include dirs avoids the conflict with no functional loss — sol2 is header-only regardless.

**SDL3 linked as shared; DLLs copied post-build on Windows.**
Static SDL3 introduces platform library dependencies (user32, gdi32, etc.) that SDL's CMake handles correctly but which complicate the link on MSVC. Shared + post-build DLL copy via `$<TARGET_RUNTIME_DLLS:ProjectIo>` is the simpler default. Revisit if distribution packaging becomes a concern.

**`SDL3::SDL3main` removed from link.**
The SDL_main redirection was unnecessary for this configuration; removing it resolved a linker issue without changing behaviour.

**`onelua.c` excluded from Lua build.**
The Lua repo includes `onelua.c`, a single-file amalgamation that re-includes `lua.c`. Excluding it alongside `lua.c` and `luac.c` prevents duplicate symbol errors.

**`max_catchup_steps = 8` for accumulator clamp.**
Chosen to allow the sim to catch up after a ~400 ms hitch at 20 Hz without stalling. No empirical basis yet — revisit if the sim loop becomes expensive enough to make 8 steps a meaningful cost.

**`window_w` / `window_h` as compile-time constants in `app.cpp`.**
Not exposed to Lua or config yet. Sufficient for the prototype; move to a config table in `init.lua` if window size needs to vary.

### Corrections made during session

Naming convention violations caught in review: all type names, function names, member variables, and filenames were PascalCase or camelCase on first write. Corrected to `snake_case` throughout per DEVELOPMENT_PRACTICES. Files renamed on disk (two-step rename required for `App` → `app` on Windows NTFS).

Documentation style: public interfaces initially used `//` comments. Corrected to `///` Doxygen with `@param` / `@return` throughout.

### Open items

- Member variable prefix (`m_`) is used throughout but not addressed in DEVELOPMENT_PRACTICES. Confirm whether to keep it or drop it before Layer 1 adds more types.
- `scripts/init.lua` defines a `config` table with `sim_hz` and `econ_per_second`. These are not yet read back by `sim_loop` — the constructor uses hardcoded defaults. Wire this up when the Lua/C++ boundary is exercised further.
