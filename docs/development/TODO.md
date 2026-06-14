# Project Io — TODO

Parked thoughts, recorded but not yet actioned. These are deliberately deferred
while higher-priority work takes precedence. None of them are committed designs —
they are reminders to revisit.

Difficulty scale: **1** trivial · **2** light work · **3** medium · **4** hard ·
**5** very hard · **6** deferred

## Categories

Every item below sits under exactly one category. The allowed categories are the
**UI** categories — **Canvas**, **Menu**, **Ledger**, **Documentation**,
**Known Bug** — and the **game-system** categories mirroring `docs/SYSTEMS.md`:
**Trade**, **Conflict**, **Budget**, **Resources**, **Supply**,
**Infrastructure**, **Workforce**, **Exploration**, **Environment**, **Research**,
**Policy**, **Diplomacy**. File each new item under its category heading, creating
the heading if it is the first item for that category. Only categories that
currently hold items appear as sections below.

---

## Canvas

- **[1] Circumplanetary max zoom.** Cap the Circumplanetary canvas so its most
  zoomed-in framing shows roughly **0.3 AU** across. The Solar canvas zoom range
  is fine as it currently is. Touches the zoom bound (`zoom_max`, shared with the
  scroll wheel and `draw_scale_zoom_overlay`) in `circumplanetary_canvas.cpp` —
  derive it from the per-anchor scale so the deepest zoom frames ~0.3 AU.

- **[2] Map lens icons.** The overlay (map) lens control strip uses worded buttons
  (Supply / Market / Faction). Switch them to **icons**, using the vector-glyph
  icon helper (`src/ui/icons.hpp`), keeping the lens name in a hover tooltip.
  Touches `draw_overlay_controls` in `src/ui/overlay.cpp`.

- **[6] Informative tooltip / hover-card system.** Deferred. The single most important
  player-communication surface for a grand strategy game. Today there is one
  ad-hoc `ImGui::BeginTooltip` inside the Planetary canvas, plus the lightweight
  `ImGui::SetTooltip` body tooltips on the Solar / Circumplanetary canvases;
  there is no shared rich card. We want a *shared* hover-card primitive with a
  consistent structure — title line (name + type + icon), a short stat block,
  optional sectioned detail, and room for "why" annotations (e.g. how a price or
  yield was derived). It must work for every hoverable thing across all canvases
  and ledgers: bodies, tiles, buildings, markets, and later convoys and routes.
  It can now build on the shared presentation metadata, formatters, and icon
  helpers (`src/ui/presentation.hpp`, `format.hpp`, `icons.hpp`). Decide: a single
  `draw_hover_card(...)` helper vs. per-entity builders; instant vs. delayed
  reveal; how a "rich card" (LAYOUT.md popup elements) differs from the lightweight
  canvas tooltip. Likely earns its own `docs/ui/TOOLTIP.md`. Note the overlap with
  the **Selection info element** (Ledger) — both present per-entity detail; share
  the per-type content builders where it makes sense. See `docs/ui/LAYOUT.md`.

- **[6] Clarify the time control view.** Deferred. The current two-column time
  panel (calendar block + speed controls) is a prototype-grade layout. Revisit it
  later to settle the production design: what the player needs from the clock at a
  glance (date, quarter/economy-tick countdown, speed), whether to surface the
  economy-tick boundary more explicitly, keyboard shortcuts for pause/speed, and
  how the panel relates to the rest of the shell chrome. See `docs/ui/TIME_CONTROLS.md`
  / `docs/ui/LAYOUT.md`.

## Menu

- **[6] Define the menu items from the systems.** Work out the important menu items
  driven by the game systems (`docs/SYSTEMS.md`), then **get feedback on the
  intended order before final implementation.** See `docs/ui/MENU.md`.

## Ledger

- **[2] Tile Ledger default body.** The Tile Ledger should default its selected
  body to the **current view's main body** — the Circumplanetary view's anchor, or
  the Planetary view's body — rather than the lowest id. The existing default
  ordering is otherwise fine. Touches the body-selector default in
  `src/ui/tile_inspector.cpp` (read `ui_state.active_body` /
  `circumplanetary_anchor`).

- **[4] Selection info element (a ledger).** A closable panel (fits the ledger
  category — open/close like the Tile Ledger) docked **above the lens/zoom
  controls**, showing details of the **current selection**. It is **polymorphic
  by selection type**: a tile shows tile data, a body shows body data, a unit
  shows unit data, a building/market each their own — different content per kind
  of selected entity. It carries a **'go to' button next to the close button**
  that focuses the selection (reuse `ui::focus_on_entity`, `src/ui/view_nav.hpp`).
  Pair this with a **navigation change**: **single-click selects** (populates this
  panel) and **double-click navigates** (descends/focuses the rung), where the
  'go to' button has the same effect as a double-click. This revises the canvas
  click handlers (currently a single click descends). Needs its own doc
  (`docs/ui/SELECTION.md`) and an entry in `LAYOUT.md`.

- **[4] Generation Ledger.** Design a ledger that explains *why* a tile generated
  as it did, for tuning and analysis of the procedural pass. The data seam already
  exists: `generate_body_tiles()` takes an optional `generation_record*`
  (`src/world/tile_generation.hpp`) that captures per-pass intermediates —
  heightmap, moisture, latitude bands, ocean threshold — today thrown away on the
  common path. **Design is deliberately unstarted.** Decide: what the ledger
  presents (per-tile derivation breadcrumb: height/moisture/band → composition →
  landform → deposits; and per-body summaries like composition/landform histograms
  and the ocean threshold); whether history is *persisted* per body or
  *regenerated on demand* (generation is deterministic, so regeneration is cheap
  and avoids storing a record per tile); and how it surfaces (a dedicated Ledger
  window, or an overlay lens over the Planetary canvas showing the heightmap /
  moisture / band fields). Likely earns `docs/generation/GENERATION_LEDGER.md`.
  Note the overlap with the hover-card / Selection info work — the per-tile
  derivation is a natural section of a tile's rich card. See
  `docs/generation/TILE_GENERATION.md` (§ Generation history hook).

## Environment

The two-axis terrain model and the six-pass procedural generation are
**implemented** (`src/world/tile_generation.{hpp,cpp}`, driven by per-body
`body_profile`s in `hard_coded_world.cpp`; design authority in
`docs/generation/TILE_GENERATION.md`). What remains is tuning and refinement, not
new structure:

- **[2] Landform prominence.** Mountain/rift/crater clusters are small and sparse
  by the doc's tight ring transitions, even after the area-scaling of seed counts.
  Dial cluster radius and/or seed density up if more prominent ranges are wanted —
  the levers are `shape_of()` (ring count + decay) and `scale_to_area()` in
  `tile_generation.cpp`.

- **[2] Kepler biome balance.** The equatorial ocean bias (`bias_amp = 0.15` in
  Pass 2) drowns most tropical/subtropical land, so forest and wetland are sparse
  on the home planet (~1% / ~0%). Lower the bias to widen the habitable belt, or
  decouple the volcanic/forest belts from the wettest equatorial rows.

- **[1] Selene ice cap size.** Selene reads as ~52% icy because the `cold` polar
  band spans the outer 50% of rows (per the Pass 3 table) and every polar row ices
  over on a polar-frozen body. Narrow the `cold` polar band, or add a tighter
  polar-cap override distinct from the climate band, if a smaller cap is wanted.

- **[4] Tile generation refinements (deferred).** The larger production passes
  noted in `TILE_GENERATION.md` § Deferred: solar-parameter derivation from orbital
  mechanics, smooth (noise-blended) band transitions, tectonic plate-driven
  landforms, full deposit authoring for the non-prototype resources, and coastline
  refinement (enclosed seas, archipelagos, lakes).

## Known Bug

- **[4] Body labels move in steps, not smoothly.** Re-logged. The font-oversampling
  pass (`src/ui/fonts.hpp`) improved glyph crispness but did **not** fix the motion
  artefact: body labels visibly advance only every few ticks while the body dot
  glides. The symptom is temporal (stepped position over time), not purely the
  sub-pixel rasterisation originally diagnosed. The label and the dot are drawn
  from the same per-frame `pos`, so the stepping must enter via the text path
  itself (glyph placement quantisation) or the way the label position is read —
  compare the two paths on the Solar / Circumplanetary canvases. See `SOLAR.md`.
