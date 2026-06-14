# Project Io — TODO

Parked thoughts and **described additions** — recorded but not yet actioned, and
not yet committed designs. Each item is a *description of intent*: a change to
make, a feature to build, or a doc to write, with enough context and file
pointers to pick it up later. Items are deliberately deferred while
higher-priority work takes precedence.

## TODO vs. TASKS

This file (TODO.md) holds **described intent**. [`TASKS.md`](TASKS.md) holds the
**active, prioritised, actionable worklist** — the concrete, file-scoped
breakdown we execute against (in the style of the A–F items the Selection info
element was split into).

The workflow is one-directional. When we decide to act on a TODO item, we
**promote** it into TASKS.md by breaking it into ordered, individually-scopable
tasks. Promotion is where we do the planning a TODO entry deliberately omits:

- **Decompose** the intent into the smallest independently-buildable steps
  (foundation first, then dependants).
- **Scope each step to its files**, so collisions between steps are visible.
- **Mark dependencies and parallelisation** — which steps are independent roots
  that can run concurrently (potentially as parallel sub-agents on disjoint
  files), and which must wait. Steps that edit the same files stay sequential.

A TODO item stays here (updated to note what has been done) even after parts of
it are promoted; TASKS.md is the transient execution list, cleared as tasks
complete. See [`TASKS.md`](TASKS.md) for the task format.

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

- **[4] Render the political layer (nations + corporations).** The v0.0.3 world
  now *generates* nations (`world.nations`, `world.tile_to_nation`) and
  corporations (`world.corporations`, with placed `building` assets), but **nothing
  draws them** — the data is invisible in the app. Wire it into the existing
  **Faction lens** on the Planetary canvas: tint/outline each tile by its owning
  nation (stable per-nation colour keyed off the nation entity id), draw nation
  borders where `tile_to_nation` changes between neighbours, and mark corporation
  starting assets (their buildings) with a per-corporation glyph/marker, the player
  corp distinguished. Make the per-nation and per-corporation summaries feed the
  **Selection info element** (the shared per-entity content builders in
  `entity_summary.{hpp,cpp}`) so a nation/corp can be inspected once it is
  click-selectable. Decide the colour-assignment scheme (hash vs. palette-cycle),
  whether borders draw on the base map or only under the Faction lens, and how the
  legend reads. Touches `src/ui/overlay.cpp` (Faction lens path), the Planetary
  canvas tile draw, `presentation.{hpp,cpp}` (nation/corp colour + name helpers),
  and `entity_summary.{hpp,cpp}`. See `docs/ui/CANVASES.md`,
  `docs/ui/SELECTION.md`, and the **Canvas hit-testing** follow-up under Ledger
  (nations/corps are not yet canvas-selectable). Likely needs the **hover-card /
  tooltip** work to land for rich faction read-outs.

- **[2] Default view should surface the generated world.** Decide and set the
  **opening canvas / lens** so a fresh campaign immediately shows the populated
  world rather than a bare map — e.g. open on Kepler's Planetary surface with the
  Faction lens active (now that there is a political layer to show), or the
  Circumplanetary view of the home body. Depends on the political-layer render
  above being available. Touches the initial `ui_state` (active canvas / anchor /
  lens) set at startup — likely in `src/core/app.cpp` or wherever `ui_state` is
  seeded. Confirm the intended first-frame framing with a quick check before
  finalising.

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

### Selection info element

**Implemented** (the original A–D breakdown): the pinned, polymorphic Selection
info element, the single-click-selects / double-click-navigates click model, the
shared per-entity content builders, and the panel itself. Design in
`docs/ui/SELECTION.md`; the three interaction states in `docs/glossary.md`; build
notes in the 2026-06-14 DEVLOG entry. Files: `src/ui/selection.{hpp,cpp}`,
`entity_summary.{hpp,cpp}`, `selection_panel.{hpp,cpp}`, the three canvas click
handlers, and `ui_state.hpp`.

Remaining / follow-up intent (promote to TASKS.md when actioned):

- **[3] 'Go to' should land on the planetary tile view.** Today the panel's
  'go to' (and double-click) routes a **body** through `focus_on_entity` →
  `focus_on_body`, which frames the **circumplanetary** view. Change it so 'go
  to' for a body always **descends to that body's Planetary (tile) surface** —
  the most informative rung — i.e. route bodies through `focus_on_surface` rather
  than `focus_on_body`. For a **tile** selection, 'go to' should **do nothing for
  now** (the surface is already shown; pan-to-tile is out of scope). Touches the
  go-to dispatch (`src/ui/view_nav.cpp` and/or `selection_panel.cpp`) and the
  per-kind 'go to' table in `SELECTION.md`.

- **[3] 'Go to' is unreliable — appears to only work for Kepler.** Observed: the
  'go to' button navigates correctly for the home planet (Kepler) but seems to do
  nothing (or the wrong thing) for other bodies. Likely entangled with the
  target-rung change above — a circumplanetary landing on a body with little
  authored local detail can read as "nothing happened". Confirm whether this is a
  genuine id/lookup failure or just an unhelpful landing rung; reproduce per body
  and fix alongside the planetary-target change. Also filed under **Known Bug**.

- **[2] Non-spatial 'go to' routing.** For nation / corporation selections (no
  canvas of their own), 'go to' should open the relevant ledger rather than
  navigate a canvas. The dispatch seam exists (`draw_selection_panel` →
  `focus_on_entity`); add the branches once those entity kinds and their ledgers
  exist. Not actionable until then.

- **[2] Canvas hit-testing for buildings / units / markets.** Only bodies and
  tiles are hit-tested on the canvases today; the other kinds are selectable only
  as Tile Ledger rows. Add canvas hit-testing so they can be single-click-selected
  directly (the panel already renders all five kinds). Depends on those entities
  being drawn as selectable canvas markers first.

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

This category covers the whole **world-generation** layer — terrain, nations, and
corporations — the spine of v0.0.3. The terrain pass is built; the political and
economic generation layers are net-new and described below. Design authority lives
in `docs/generation/{TILE,NATION,CORPORATION}_GENERATION.md`.

### Tile generation (terrain)

The two-axis terrain model and the six-pass procedural generation are
**implemented** (`src/world/tile_generation.{hpp,cpp}`, driven by per-body
`body_profile`s in `hard_coded_world.cpp`; design authority in
`docs/generation/TILE_GENERATION.md`). What remains is tuning and refinement, not
new structure. The three knobs below had an **initial v0.0.3 tuning pass applied**
(Selene icy 52%→33%; mountain/rift rings 2→3, crater 1→2, `scale_to_area` ref
1800→1200; Kepler Pass 2 `bias_amp` 0.15→0.07). They remain live tuning levers —
the values moved the right direction but still want eyeball tuning (Kepler forest
~0.9% / wetland ~0.5% are improved but modest):

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

### Nation generation

**Implemented** (v0.0.3). The five-pass pipeline now runs on Kepler at world
construction: `src/world/nation_generation.{hpp,cpp}` (`generate_nations`), with
`nation_component` + the `ideology`/`expansionism`/`economic_focus` enums in
`components.hpp` and the `nations` / `tile_to_nation` stores in `world.hpp`.
Verified: 10 nations, no ocean claimed, no empty nations, varied territory. Design
authority: `docs/generation/NATION_GENERATION.md`. The original task breakdown is
kept below for reference; remaining open item:

- **[2] Orphan-island assignment (refinement).** The cardinal-adjacency Voronoi
  BFS cannot cross water, so landmasses disconnected from every seed stay
  unclaimed (~708 / 6048 Kepler land tiles ≈ 12%). Defensible as "unclaimed
  islands", but if full land coverage is wanted, add a post-pass assigning each
  orphan island component to the nearest nation across water. `nation_generation.cpp`.

- **[3] Nation data model.** Add a `nation_component` (name, owned-tile set,
  derived resource-profile descriptor, political character) and a world store +
  `null`-able tile→nation back-reference. Foundation for every pass below. Touches
  `src/world/components.hpp`, `src/world/world.{hpp,cpp}`,
  `src/world/tile_generation.hpp` (tile ownership field).

- **[3] Pass 1 — seed placement.** Place `nation_count` seeds on landmass tiles,
  preferring habitable compositions (grassland/forest/wetland) and enforcing a
  minimum tile separation. Seeded RNG from a campaign seed. New
  `src/world/nation_generation.{hpp,cpp}`.

- **[4] Pass 2 — territory expansion (Voronoi BFS).** Grow each seed by weighted
  Voronoi BFS: never claim ocean, decay expansion probability with distance, treat
  high-`H` tiles (mountains/highlands) as soft barriers that drain the expansion
  budget so ranges fall near borders. Run until all claimable land is assigned.
  `src/world/nation_generation.cpp`.

- **[3] Pass 3 — resource profile derivation.** Sum each nation's tile deposit
  profiles (weighted by composition) into a read-only abundance descriptor
  (`iron_ore_abundance`, `agricultural_abundance`, …) used by diplomacy init and
  corporation generation. `src/world/nation_generation.cpp`.

- **[3] Pass 4 — political character assignment.** Draw `ideology`,
  `expansionism`, `economic_focus` per the NATION_GENERATION.md attribute table
  from the seeded pass. (Sentiment-graph seeding is deferred with the diplomacy
  system; only store the attributes now.) `src/world/nation_generation.cpp`,
  `src/world/components.hpp`.

- **[2] Pass 5 — naming.** Generate names from a seeded culture-flavoured template
  bank (structural form + phoneme table); no authored name list.
  `src/world/nation_generation.cpp`.

- **[2] Campaign hook.** Run the nation pipeline for Kepler at world construction
  (8–12, tunable), after tile generation. `src/world/hard_coded_world.cpp`.

- **[6] Deferred — nation behaviour & production passes.** Per NATION_GENERATION.md
  § Open items: the nation *system* (tax, licences, war, infrastructure), the
  sentiment graph, historical fragmentation (exclaves/disputed zones), and
  non-Kepler jurisdiction. Out of prototype scope.

### Corporation generation

**Implemented** (v0.0.3). The five-pass pipeline runs after nation generation at
world construction: `src/world/corporation_generation.{hpp,cpp}`
(`generate_corporations`), with `industrial_focus` + `corporation_component` in
`components.hpp` and the `corporations` store in `world.hpp`. It now sets
`world.player_entity` to the flagged corp (previously an unset id; the Kepler
player unit's `owner` now resolves to it). Verified: 8 corporations, exactly one
player, all with home nations, all placing a collision-free starting asset, diverse
focus + capital. Design authority: `docs/generation/CORPORATION_GENERATION.md`. The
original task breakdown is kept below for reference.

- **[3] Corporation data model.** Add a `corporation_component` (name, home-nation
  id, `industrial_focus`, starting capital, `is_player`) and a world store; reuse
  `world.player_entity` for the flagged corp. Foundation for the passes below.
  Touches `src/world/components.hpp`, `src/world/world.{hpp,cpp}`. Depends on the
  nation data model.

- **[3] Pass 1 — nation assignment.** Pick each corp's home nation weighted by the
  nation's `economic_focus` and current corp distribution, with a balancing factor
  so no nation hosts all corps. New `src/world/corporation_generation.{hpp,cpp}`.

- **[2] Pass 2 — industrial focus assignment.** Draw `industrial_focus`
  (extraction/processing/trade) with probability shaped by the home nation's
  `economic_focus` and the running distribution. `src/world/corporation_generation.cpp`.

- **[4] Pass 3 — starting asset placement.** Place opening buildings on tiles in
  the home nation's territory per focus (high-deposit tiles for extraction,
  high-connectivity for trade), collision-checked so no two corps share a tile.
  Reuses the building/stockpile components. `src/world/corporation_generation.cpp`.

- **[2] Pass 4 — financial profile.** Assign starting capital from a seeded range
  with a `wealth_variance` spread; processing/trade focus gets a slight premium.
  `src/world/corporation_generation.cpp`.

- **[2] Pass 5 — naming.** Generate corporate names from a template bank
  (structural forms + seeded identifiers / home-territory references).
  `src/world/corporation_generation.cpp`.

- **[2] Player flag & campaign hook.** Flag one generated corp `is_player` (fixed
  assignment for now) and run the pipeline at world construction after nation
  generation. `src/world/corporation_generation.cpp`, `src/world/hard_coded_world.cpp`.

- **[6] Deferred — corporation selection screen & behaviour.** Per
  CORPORATION_GENERATION.md § Open items: the analytical corp-selection/re-roll
  flow, franchising, nation-seeded privatisation, automated tax, Era-based
  sovereignty, and diplomatic posture. Out of prototype scope.

## Known Bug

- **[4] Frame stutter / performance + hardware limits unconfigured.** The app
  already **stutters intermittently**. This may be benign for now, but the cause is
  not yet diagnosed and there is no frame-pacing or hardware-limit configuration in
  place (vsync / target frame-rate / present mode, and the per-frame draw budget for
  the dense tile grids — Kepler is 180×84 = 15,120 tiles redrawn each frame, plus
  the upcoming per-tile Faction-lens tint/border pass). First **measure** before
  optimising: is the stutter GPU present-driven (vsync/composition), CPU draw-call
  volume (immediate-mode tile loop), or allocation churn per frame? Then settle the
  hardware-limit config (vsync on/off, frame cap, whether to cache static tile
  geometry / dirty-rect the canvas). **Important context:** the **market and pricing
  logic is not implemented at all** yet — once the economy tick and per-market price
  resolution land, the per-frame and per-tick cost profile changes materially, so
  treat any optimisation now as provisional and re-measure after the economy is in.
  Don't over-fit the frame loop to today's (logic-light) workload. Likely touches
  the render/present setup (SDL3 swap / vsync) and the canvas tile-draw loops.

- **[3] Selection 'go to' only works for Kepler.** The Selection info element's
  'go to' button navigates for the home planet but appears inert for other
  bodies. Detailed under **Ledger → Selection info element** (it is fixed
  alongside the planetary-target change). Logged here for triage visibility.

- **[4] Body labels move in steps, not smoothly.** Re-logged. The font-oversampling
  pass (`src/ui/fonts.hpp`) improved glyph crispness but did **not** fix the motion
  artefact: body labels visibly advance only every few ticks while the body dot
  glides. The symptom is temporal (stepped position over time), not purely the
  sub-pixel rasterisation originally diagnosed. The label and the dot are drawn
  from the same per-frame `pos`, so the stepping must enter via the text path
  itself (glyph placement quantisation) or the way the label position is read —
  compare the two paths on the Solar / Circumplanetary canvases. See `SOLAR.md`.
