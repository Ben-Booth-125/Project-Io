# Project Io — Development Log

Entries are newest-first. Each entry covers one development session and records what was built, what in-session decisions were made, and what was left open. Decisions that affect the whole project permanently belong in TECH_FOUNDATIONS or a dedicated ADR; this log is for session-scoped choices and progress notes.

Entries that correspond to a tagged snapshot in `backups/` carry an explicit **version** marker in their heading (e.g. *version 0.0.2*) and a **Backup** line naming the snapshot path. These are the rollback points: to revert, restore the named `backups/vX.Y.Z/` tree over `src/`.

---

## 2026-06-14 — Visual-verification harness (Phase 1) + Corporation lens closed

**Status:** Complete — visual-harness V1–V6 met; corporation-lens R2–R6 re-verified
and the cancelled group closed (all 9 rows met). Full app builds clean (Debug, exit
0); `--verify` runs headless and exits 0.

### What was built

Phase 1 of the automated visual-verification harness — the tool that makes the
`visual` requirement class runnable without a human at the screen (it had been the
blocker that cancelled the corporation-lens group earlier this session).

- **PNG writer** (`src/core/png_writer.{hpp,cpp}`): dependency-free
  `write_png_rgba()` — stored-DEFLATE zlib + CRC32/adler32. Chosen over vendoring
  stb_image_write (no fetch) and over keeping BMP (the Read tool reads PNG, not BMP,
  so Claude can inspect captures directly).
- **Capture → PNG** (`src/core/app.cpp`): `save_screenshot()` now converts the
  `SDL_RenderReadPixels` surface to RGBA32 and writes PNG; supports a named capture
  (F12 keeps the timestamped path).
- **`--verify <script>` mode** (`src/core/app.{hpp,cpp}`, `src/main.cpp`):
  `run_verify()` sets up a deterministic session (fixed window, seeded
  `make_hard_coded_world`, sim paused), binds a `verify` Lua table
  (`goto_surface`, `set_overlay`, `set_zoom`, `set_pan`, `add_pan`, `capture`,
  `log_buildings`) wired straight to `ui_state` (direct-state driver), runs the
  script, exits. `setup_world()` extracted from `run()` so both share one start state.
- **Corporation-lens verify script** (`scripts/verify/corporation_lens.lua`):
  captures the home surface under none/faction/corporation and zooms onto the player
  building (22,82) and a rival (42,63).
- **Docs** (`DEVELOPMENT_PRACTICES.md` § Visual verification, `req/REQUIREMENTS.md`
  policy): the harness is now the standard `visual` verification method.

### Verification result (corporation lens R2–R6)

Ran `ProjectIo --verify`; inspected the PNGs via the Read tool. Confirmed: the
corporation lens button (square+dot glyph) is present and active in the strip
(R2/R3); the player tile tints `faction_colour(0)` blue and a rival tile tints a
distinct hashed colour (coral) — R4 and R5; surrounding non-corporate tiles stay
terrain-coloured (R6). The cancelled corporation-lens group is now closed.

### In-session decisions

**Driver = direct state manipulation (owner's call).** The verify API writes
`ui_state` directly rather than injecting keys, so captures are reproducible. Full
keyboard navigation is deferred to Phase 2 (player-facing; TODO § Canvas).

**Pan aiming is empirical.** `set_pan` takes screen pixels; the pan to centre a tile
is `(grid_centre − tile_local) · zoom`, derived/confirmed against a capture rather
than replicating the canvas's title-bar/font-dependent transform in two places.

### Open items / findings

- **Player-tile border is redundant** under the corporation lens — fill and border
  are both `faction_colour(0)`, so the border is invisible. R5 holds via the fill
  colour; logged as a `[1]` TODO § Canvas to recolour the border for contrast.
- A script edited after a build is stale in `build/Debug/scripts` until the next
  build; run `--verify` against the source path (`../../scripts/...`) when iterating.

---

## 2026-06-14 — Corporation lens

**Status:** Code-complete, then **CANCELLED** — 4/9 requirements met (R1, R7, R8, R9);
the 5 visual rows (R2-visual, R3–R6) are `failed` (no `visual` tool; computer-use
declined). Full app builds clean (Debug, exit 0); all changed translation units
compile and link into `ProjectIo.exe`. The code remains in the tree. Per the new
TASKS.md § "Cancelling a task group", the group was cancelled rather than left
half-tracked: requirements marked `failed`, intent merged back into TODO § Canvas
"Corporation lens — verify the landed code", task stubs removed from TASKS.md. The
reusable verification method is TODO § Canvas "Automated visual-verification harness".

### What was built

Promoted and executed the **Corporation lens** group from TASKS.md (TODO § Canvas
[4]). A → {B, C} → D; all run in the main session (the canvas integrator owns the
hotspot file, and B/C are trivial enough not to warrant fan-out).

- **A — `docs/ui/LENSES.md` created.** New design authority for the lens system.
  The Corporation section is fully settled (ownership = a tile carrying a corporate
  building via `w.corporations[].assets` → `building_component.tile`, **no influence
  radius**; Planetary-only; player corp = `faction_colour(0)` + border, rivals =
  per-corp hash; unowned = terrain colour, no nation underlay). The other four lens
  sections are stubs recording current behaviour.
- **B — `icons::corporation` added** (`icons.{hpp,cpp}`): a filled square with a
  centred dark inner dot — a "seal" silhouette distinct from the processing-facility
  plain square, the extraction diamond, and the port/unit triangle.
- **C — `overlay_mode::corporation` added** (`ui_state.hpp`) and wired into the
  overlay strip (`overlay.cpp`): glyph dispatch, `overlay_mode_name`
  ("Corporation ownership"), `overlay_mode_short_name` ("Corp"), and the strip
  `modes` array (now four lenses).
- **D — render pass in `body_surface_canvas.cpp`.** Under `overlay_mode::corporation`:
  owned tiles tint to their corp colour; player-corp tiles additionally get a thin
  `faction_colour(0)` hex outline; unowned tiles stay terrain-coloured. Guarded
  entirely behind the corporation branch — no change on Solar/Circumplanetary.

### In-session decisions

**Extracted a shared `corp_colour` lambda.** The building-marker pass already
inlined the player-vs-rival colour logic (faction slot 0 for the player, a
multiplicative hash kept off slot 0 for rivals). The lens tint must agree with the
markers, so the logic was lifted into one lambda used by both — a single source of
truth rather than a second copy.

### Open items

- Visual confirmation of R2/R3–R6 (glyph reads correctly in the strip; tints,
  player border, and unowned-terrain fallback render as intended) still pending —
  needs an in-GUI run.

---

## 2026-06-14 — Layer 3 foundations: political-layer render, lens icons, Circumplanetary zoom cap

**Status:** Complete (code). Full app builds clean (Debug, exit 0); all six changed
translation units compile and link into `ProjectIo.exe`. Not yet visually run in
the GUI.

### What was built

Promoted the four sub-difficulty-6 Canvas items from TODO.md into TASKS.md and
executed them in four waves, using **parallel sub-agents on disjoint file scopes**
for the substantial branch and the main session for foundations, hotspots, and
integration.

- **Wave 1 (foundations, main session):** Circumplanetary `zoom_max` derived from
  `max_moon_au / 0.15` so the deepest zoom frames ~0.3 AU
  (`circumplanetary_canvas.cpp`); three lens glyphs `supply`/`market`/`faction`
  added to `icons.{hpp,cpp}`; `palette::nation_colour(entity_id)` — a 12-slot hue
  wheel keyed by a Knuth multiplicative hash — added to `presentation.{hpp,cpp}`.
- **Wave 2 (concurrent sub-agents on disjoint files):** nation/corporation stat-block
  builders `draw_nation_summary`/`draw_corporation_summary` (`entity_summary.{hpp,cpp}`)
  ∥ the Faction-lens render in `body_surface_canvas.cpp` (nation tile tint + odd-r
  neighbour borders via midpoint-perpendicular edges + per-corporation building
  marker colours). Map-lens icon buttons wired into `overlay.cpp` inline meanwhile.
- **Wave 3 (main session, hotspot):** `nation`/`corporation` added to
  `selection_kind`; `selection_kind_of`, `selection_kind_name`, and the
  `selection_panel.cpp` title/summary dispatch extended.
- **Wave 4 (main session):** open on the Faction lens by default (`app.cpp`).

This closes Canvas items **[1] Circumplanetary max zoom**, **[2] Map lens icons**,
**[4] Render the political layer**, and **[2] Default view should surface the
generated world** (home-surface open + Faction lens default).

### In-session decisions

- **Nation colours: 12-slot fixed hue wheel + Knuth hash**, distinct from the
  6-slot faction palette — nations tint territory, factions mark ownership.
- **Faction-lens tint is a direct replacement** of the terrain colour (no blend);
  unclaimed tiles (absent from `tile_to_nation`, e.g. ocean) keep terrain hue.
- **Borders draw only under the Faction lens**, via the robust midpoint-perpendicular
  edge method (avoids per-vertex offset-row mapping). Claimed/unclaimed counts as a
  border.
- **Building markers are coloured by owning corporation always-on** (player corp =
  faction slot 0; others a hashed non-zero slot), independent of the active lens.
- **Selection summaries for nation/corp ship now**; canvas hit-testing for them
  remains a separate Ledger follow-up (they are not yet click-selectable).

### Left open

- New TODO item **[3] Design the lens system** (`docs/ui/LENSES.md`): spec the five
  lenses incl. proposed Corporation + Resource lenses, rung applicability, icon
  vocabulary, legend format.
- No legend/colour key for the Faction lens yet (deferred with the lens-design doc).
- Canvas hit-testing for nations/corps (Ledger follow-up); the political-layer
  hover read-out still waits on the deferred hover-card system.

---

## 2026-06-14 — version 0.0.3 — Environment: nation + corporation generation, tile tuning

**Status:** Complete (code). Full app builds clean (Debug, exit 0); the two new
`src/world/*.cpp` translation units compile and link into `ProjectIo.exe`. Logic
verified via a throwaway headless harness (compiled with the `world/*` TUs only,
per `reference_headless_build`); not yet visually run in the GUI.

**Backup:** `backups/v0.0.3/src/` — restore this tree over `src/` to roll back.

### What was built

Closed out the **Environment** category for v0.0.3 — the world-generation spine.
TODO.md's Environment section was broadened from terrain-only to the whole
generation layer (Tile / Nation / Corporation) and the two unitemised generation
docs were itemised, scoped, and promoted to TASKS.md as three groups.

Execution used **parallel sub-agents on disjoint file scopes**, gated by the
real collision map (the passes inside each generator share one `.cpp`, so
within-generator parallelism was rejected; concurrency is cross-group):

- **Wave 1 (concurrent):** Tile tuning (`tile_generation.cpp` only) ∥ Nation
  pipeline (`components.hpp`, `world.{hpp,cpp}`, new `nation_generation.{hpp,cpp}`).
- **Wave 2:** Corporation pipeline (new `corporation_generation.{hpp,cpp}` +
  `components.hpp`/`world.hpp`), gated on the nation component existing.
- **Integration (main session):** all `hard_coded_world.cpp` hooks, both builds,
  and verification. Sub-agents did not build or commit.

**Nation generation** (`generate_nations`): five passes — habitable-preferring
seed placement with min-separation, weighted Voronoi BFS (ocean never claimed,
mountains/highlands as soft cost barriers), resource-profile sum, seeded political
character (`ideology`/`expansionism`/`economic_focus`), procedural phoneme naming.
Stores: `world.nations`, `world.tile_to_nation`. Verified on Kepler: 10 nations,
0 ocean claimed, 0 empty, territory 108–1843 tiles.

**Corporation generation** (`generate_corporations`): five passes — nation
assignment weighted by `economic_focus` + balancing, focus draw biased by home
nation, collision-checked starting-asset placement (focus→building_type, seeded
from existing `w.buildings`), seeded capital with processing/trade premium,
corporate naming. Sets `world.player_entity` to the flagged player corp. Verified:
8 corps, exactly 1 player, all homed, all placed a collision-free asset.

**Tile tuning** (`tile_generation.cpp`): Selene icy 52%→33% (cold band outer
50%→30% of rows); landform prominence (mountain/rift rings 2→3, crater 1→2,
`scale_to_area` ref 1800→1200); Kepler Pass 2 `bias_amp` 0.15→0.07.

### In-session decisions

- **Filed nation/corp generation under Environment** (not Diplomacy), per the
  user — keeps the v0.0.3 world-generation theme coherent; the items note they
  *seed* the deferred Diplomacy/Budget layers.
- **Tile→nation ownership lives in a `world.tile_to_nation` map, not a
  `tile_component` field** — keeps the nation group's file scope disjoint from the
  tile-tuning group so they could run concurrently.
- **Fixed a latent compile trap:** `nation_component`/`corporation_component`
  give members the same name as their enum type; default initialisers must use
  global-scope qualification (`::ideology::mercantile`) or the member shadows the
  type.

### Left open

- **Orphan-island assignment** — the cardinal-adjacency BFS can't cross water, so
  ~708/6048 (~12%) of Kepler land (disconnected islands) stays unclaimed.
  Defensible; a nearest-nation post-pass would close it if full coverage is wanted.
  Logged in TODO.md § Environment → Nation generation.
- **Kepler forest/wetland** still modest (~0.9% / ~0.5%) after the bias cut —
  improved but a candidate for one more eyeball-tuning nudge.

---

## 2026-06-14 — Selection info element + single/double-click model

**Status:** Complete (code). Builds clean (Debug, no warnings). Not yet visually run — the panel is hidden until a selection is made, so a no-input screenshot would not show it.

### What was built

Design first: new **`docs/ui/SELECTION.md`** (the element, the three interaction
states, the click-model change, the polymorphic content/'go-to' table, the
shared-builders abstraction); **glossary** entries for the **Active / Focus /
Selection** states; a **`LAYOUT.md`** region; a **`CANVASES.md`** click-model
call-out; and the doc indexed in `CLAUDE.md`. The original single TODO item was
split into six scoped sub-items (A–F) with a dependency/parallelisation note.

Implemented A–D against that design:

- **A — selection state (`src/ui/selection.{hpp,cpp}`, `ui_state.hpp`).** New
  `ui_state::selected_entity` (and `selection_hidden_for` for the close-button
  hide), kept distinct from the `active_*` navigation anchors. `selection_kind`
  enum + `selection_kind_of(w, id)` resolver probing the world maps in the same
  order as `focus_on_entity`.
- **B — click model (all three canvases).** Single left-click **selects**
  (`selected_entity`, null clears on empty space; no view change); **double**-click
  **navigates** (the former descend/focus). Minimap ascend stays single-click.
  The on-canvas highlight ring now follows `selected_entity` (was `active_body` /
  `active_tile`) so a single click gives immediate feedback.
- **C — shared content builders (`src/ui/entity_summary.{hpp,cpp}`).** Per-kind
  stat blocks (`draw_{body,tile,building,market,unit}_summary`), content-only and
  stale-id tolerant, built on the presentation layer. `body_type_name` /
  `building_type_name` centralised into `presentation.{hpp,cpp}` (de-duplicating
  the copies in the Tile Ledger and — during integration — all three canvases).
  The Tile Ledger reuses the shared names; its multi-tile table stays as-is.
- **D — the panel (`src/ui/selection_panel.{hpp,cpp}`).** Pinned bottom-left
  above the overlay strip, hidden until a valid selection exists. Header: title +
  kind, a **'go to'** button (`focus_on_entity`) and a **close** button (hides
  until the next selection). Dispatches on `selection_kind` to the C builders.
  Wired in `app.cpp`.

### In-session decisions

**Built in dependency order A → {B, C} → D**, with **C run as a parallel
background sub-agent** while A/B were done in the foreground (the two independent
roots: C's builders don't depend on A's state field; only D needs both). The one
predicted collision — C adding `ui::body_type_name` while the canvases (B's
files) kept their own local copies — surfaced as an ambiguous-call build error
and was resolved at integration by deleting the three canvas-local copies and
adding the `presentation.hpp` include. This is exactly the seam the TODO note
flagged; keeping B and C to disjoint files made it a clean, single-point fix.

**Selection ring vs. active anchor.** The canvas highlight was repointed from the
navigation anchor to `selected_entity`. Selecting a moon now rings the moon
rather than the always-anchored planet — the intended selection feedback.

### Open items

- **E — non-spatial 'go to' routing** (nation/corporation → ledger) and **F —
  canvas hit-testing for buildings/units/markets**: left as TODO sub-items.
  E is not actionable until those entity kinds exist; F until those entities are
  drawn as selectable canvas markers. 'Go to' currently routes everything through
  `focus_on_entity` (spatial only).
- **Per-kind title icon** deferred to the hover-card work (shares the builders).
- **Overlay-strip stacking** uses a fixed 40 px offset to sit the panel above the
  lens strip — prototype-grade, like the rest of the shell chrome.

## 2026-06-14 — Two-axis terrain model + six-pass procedural generation

**Status:** Complete (code). Builds clean; validated with a throwaway headless stats harness (since removed).

### What was built

**`docs/generation/TILE_GENERATION.md`** (relocated)
Moved out of `docs/development/` into a new `docs/generation/` area. References updated in `CLAUDE.md` and `docs/economy/TILES.md`. Implementation notes refreshed to point at the new code module and record the deviations below.

**`src/world/components.hpp`** (data model)
`resource_type` expanded from 4 to 19 values: the full Tier-1 raw set (Earth-sourced + space-sourced + ambient) plus the prototype Tier-2 refined goods, ordered by tier per `RESOURCES.md`. `terrain_type` (5-value flat enum) replaced by the two-axis model: `terrain_composition` (11 values) and `terrain_landform` (7 values), both carried on `tile_component` (the old `terrain` field is gone).

**`src/world/tile_generation.{hpp,cpp}`** (new)
The deterministic six-pass pipeline: (1) cylinder-sampled simplex heightmap, X-wrap seamless; (2) latitude-biased ocean threshold; (3) latitude bands + independent moisture map; (4) composition by (band, moisture) for atmospheric bodies, dedicated airless/metallic tables otherwise, with a geology-driven volcanic overlay; (5) BFS landform clusters (mountain/rift/crater) plus a low-ground valley fill; (6) per-tile deposits keyed on (composition, landform) with the ambient every-tile guarantee. Bodies are described by a `body_profile` (temperature/atmosphere/hydrology/geology/water_fraction/bias) — the passes branch only on these, never on body identity. A `body_profile`-driven `generation_record` out-param optionally captures per-pass intermediates (heightmap, moisture, bands, ocean threshold) as the seam for a future generation Ledger.

**`src/world/hard_coded_world.cpp`** (rewired)
Old inline weight-table generator removed. The four prototype bodies now carry authored solar profiles: Cinder (scorching/airless/high-geology), Kepler (temperate/thick/liquid 0.60/moderate), Selene (cold/airless/polar-frozen), Pallas (cold/airless/metallic-bias). Kepler market re-authored against the new resource indices via a small `resource_array` helper.

**UI** — `terrain_colour` (now keyed on composition, 11 cases) updated in `body_surface_canvas.cpp`; `composition_name`/`landform_name` centralised in `presentation.{hpp,cpp}` and used by the canvas tooltip and Tile Ledger (the inspector gained a Landform column); resource presentation table expanded to 19 entries.

### In-session decisions

**Expanded the resource enum now (not deferred).** Faithful Pass 6 needs petroleum, water, agricultural produce, and the ambient resources, which the old 4-value enum lacked. Chosen over an interim mapping to avoid throwaway deposit work; the deposit arrays already span the full enum width so future resource authoring needs no generation change.

**Seed counts scale with grid area (deviation from the doc tables).** The doc's absolute landform seed counts collapse to ~0% coverage on the prototype's 180×84 grids. `scale_to_area()` scales counts up with grid area (never below the authored count, so small bodies are unaffected) to keep feature *density* consistent. Absolute feature prominence/size remains a tuning knob — landform clusters are still intentionally tight per the doc's ring transitions.

**Hazard/habitability derived, not authored.** The design tables don't specify these, but `tile_component` carries them and the inspector shows them, so they are derived from a composition base ceiling modified by landform (mountain/rift raise hazard and cut habitability; valley raises habitability).

### Open items / flagged for tuning

- **Generation Ledger** — filed in TODO.md. The `generation_record` hook exists; the Ledger's persistence model (capture vs. regenerate-on-demand) and UI are unstarted by request.
- **Landform prominence** — features are small/sparse by the doc's cluster rules even after area-scaling. Dial cluster radius / seed density up if more prominent ranges are wanted.
- **Kepler biome balance** — the equatorial ocean bias (`bias_amp = 0.15`) drowns most tropical/subtropical land, so forest/wetland are sparse (~1% / ~0%). Lower the bias to widen the habitable belt.
- **Selene ice fraction** — ~52% icy, because the `cold` polar band spans the outer 50% of rows (doc-consistent) and all polar rows ice over on a polar-frozen body. Narrow the cold polar band or use a tighter polar-cap override if a smaller ice cap is wanted.

---

## 2026-06-14 — Economy design expansion: tiles, population, ambient resources, logistics note

**Status:** Complete (documentation only). No code changes this session.

### What was built

**`docs/economy/TILES.md`** (new)
Two-axis terrain model: `terrain_composition` (barren, rocky, volcanic, icy, tundra, grassland, forest, wetland, ocean, regolith, metallic) × `terrain_landform` (plains, highland, mountain, canyon, valley, crater, rift). Each combination has documented deposit profiles, build cost modifiers, and amenity potential. The current single `terrain_type` enum is redesigned as two separate fields on `tile_component`; existing hard-coded data does not need immediate retrofit but a generation update is filed in `TODO.md`.

**`docs/economy/POPULATION.md`** (new)
Population centres as a formal concept. Scale/agglomeration bonus model (Outpost → Metropolis: +5% to +50% processing throughput, +0% to +20% extraction yield). Land-use state system (undeveloped, extraction, urban, amenity, infrastructure): placing urban or amenity development on a tile permanently sacrifices its extraction potential — the mechanism preventing simultaneous maximisation of raw extraction and finished goods. Population demand basket (food rations, clean water, consumer goods, habitability goods). All deferred from prototype; existing fields (`habitability`, `workforce_assigned`, `market_component.demand`) are already positioned correctly.

**`docs/economy/RESOURCES.md`** (updated)
Added "value tracks" framing alongside tiers (industrial / ambient / habitability). Added Tier 1 ambient resources section: stone, timber, sand, clay, peat — every eligible tile generates a low baseline deposit of at least one ambient resource. Added habitability goods section: clean water, building materials, consumer goods, medical supplies, utilities. Prototype resource count stays at seven; full enum target revised to approximately 35–40 entries.

**`docs/economy/PRODUCTION.md`** (updated)
Extraction building table expanded: Quarry (stone/sand/clay), Lumber Camp (timber), Surface Extractor (regolith/PGMs, Era 1). Added amenity buildings section (Park, Recreation Facility, Cultural Centre) and habitability production buildings (Water Treatment Plant, Construction Yard, Consumer Goods Factory, Pharmaceutical Lab, Power Plant). Added logistics open design note: transport capacity caps supply throughput, not price — oversupplied markets that cannot ship simply stop accumulating rather than crashing in price. Storage Depot added as an infrastructure building placeholder.

**`docs/development/TODO.md`** (updated)
New Environment category with a [4] item covering the full tile generation update: enum rename/expansion, terrain_landform field, revised BFS water logic (variable-width band seeding, polar ice/tundra at poles), per-composition deposit profiles, ambient resource baseline, and landmark landform pass.

### In-session decisions

**Every tile must have at least one deposit.** Ambient resources (stone, timber, sand, clay, peat) fill this role. They have low base prices but ensure no tile is economically inert. Quarry and Lumber Camp extract them.

**Transport capacity constrains throughput, not price.** If a body cannot export its output, production stalls before the price crashes. This is a key design nuance that affects how the market model is implemented at Layer 4 and how ports/warehouses matter at Layer 5. Marked open; to be decided when Layer 5 is designed.

**Population centres are a formal system, not a modifier.** Population has a scale model with named tiers and explicit land-use trade-offs. The "can't fully develop raw extraction and finished goods" constraint is structural — urban tiles are not also extraction tiles.

**Habitability is a separate value track.** Habitability goods and amenity buildings affect workforce efficiency and population growth indirectly, not profit directly. They are worth producing for productive system health, not for market margins.

### Open items

- Tile generation update (filed in TODO.md [4]).
- Full enum expansion for terrain_composition, terrain_landform, and the extended resource list — to be done at the start of Layer 3 implementation.
- Logistics throughput model — open design decision, deferred to Layer 5.
- Population centre implementation — deferred post-prototype.

---

## 2026-06-14 — Layer 3 economy design: resources, production, eras

**Status:** Complete (documentation only). No code changes this session.

### What was built

Three new documents under `docs/economy/` establishing the production system prior to Layer 3 implementation.

**`docs/economy/RESOURCES.md`**
Full resource list: 23 resources across three tiers. Tier 1 — raw materials (11): seven Earth-sourced (iron ore, coal, petroleum, silica, copper ore, rare earth ore, agricultural produce) and four space-sourced (water, iron-nickel ore, platinum group metals, regolith). Tier 2 — refined goods (7): steel, refined fuel, silicon, refined copper, REE alloy, liquid oxygen, food rations. Tier 3 — products (5): machinery, electronics, propellant, alloys, spacecraft components. Regolith is a special non-traded local-use resource. Prototype subset is seven resources (four raw, three refined); all 23 enum values are defined from the start.

**`docs/economy/PRODUCTION.md`**
All building types and recipes. Extraction: Mine (hard minerals), Oil Platform (petroleum), Farm (agricultural produce), Ice Extractor (water from ice — Era 1). Output rate is `deposit × workforce_assigned × (1 − hazard_level)`. Processing: Smelter, Refinery, Chemical Plant, Electronics Lab, Fabricator, Food Processor, Assembly Plant — each supports one or more named recipes. Infrastructure (designed, not yet implemented): Port, Launchpad, Orbital Port, Warehouse. Workforce model documented: `workforce_assigned` is a constant in the prototype; the full policy allocation system is deferred. Layer 3 scope: seven prototype resources, three processing buildings (Smelter/Refinery/Food Processor).

**`docs/economy/ERAS.md`**
Formal era system. Era 0 (Terrestrial) starts at campaign epoch 1 January 1960: heavy industry, no space access. Era 0→1 gate requires Rocketry research + a staffed Launchpad + minimum propellant reserve. Era 1 (Early Space) unlocks all solar system bodies, the Ice Extractor, Assembly Plant, and Orbital Port; the dominant challenge is closing the propellant loop via ISRU. Era 2+ is stubbed.

**Existing documents updated:**
- `CLAUDE.md` — three new economy doc entries added to the reading list.
- `docs/GLOSSARY.md` — Building, Era, ISRU, Recipe, Resource, Stockpile defined.
- `docs/development/INITIAL_INSTRUCTIONS.md` — Layer 3 description expanded to reference `RESOURCES.md` / `PRODUCTION.md` and the seven-resource prototype subset.

### In-session decisions

**Start date 1960 confirmed.** The "early industrial, space locked" era choice validates the existing campaign epoch. 1960 signals post-WWII heavy industry and a nascent-but-real electronics sector; the specific year is fictional in the game's alternate timeline.

**Specialised extraction buildings.** Each extraction type targets a specific resource class (Mine → hard minerals, Oil Platform → petroleum, Farm → agricultural produce, Ice Extractor → water ice). The Mine is the broadest type: its output is determined by the tile's deposits, so the same building type works on both a volcanic Kepler tile (rare earth ore) and a metallic asteroid tile (iron-nickel ore).

**Generic processing via recipes.** Processing buildings are differentiated by their active recipe, not by enum value. A Refinery can produce refined fuel, silicon, refined copper, or REE alloy depending on configuration. This keeps the building type list manageable while supporting the full recipe table.

**Three-tier chain, Tier 3 deferred in prototype.** The full three-tier chain (raw → refined → product) is documented but only Tiers 1 and 2 are implemented in the prototype. Tier-3 product recipes exist in the design but have no authored tile deposits or live buildings until a later pass.

**Eras are a formal system.** "Era" is a first-class game concept with defined gates, not an informal phase description. This affects future tech-tree design, tutorial pacing, and progression gating.

**Food is a resource.** Agricultural produce (Era 0) is extracted by Farm and processed into food rations by the Food Processor. Off-world workforce consumes food rations; a shortage reduces effective workforce at the destination. Food production on Earth is easy; it becomes a critical logistics challenge once off-world colonies exist.

**Propellant is the Era 0→1 gate good.** The Launchpad requires a propellant reserve to operate. Propellant is produced from refined fuel + liquid oxygen. Liquid oxygen requires water (Era 1 input), making the first propellant stockpile the tightest resource bottleneck in early play.

**Prototype enum scope.** All 23 resource types and all building type enum values are defined in code from the start of Layer 3. Unimplemented resources have zero deposits; unimplemented buildings have no authored placements. No retrofit needed when expanding to the full set.

### Open items

- Prototype implementation of Layer 3 (extraction logic, processing logic, ImGui panel showing stockpile changes).
- Lua recipe file structure: decide whether each recipe is a top-level table in `scripts/` or inline in a resource/building definition file.
- Workforce policy allocation (how the player redistributes `workforce_assigned` in real time) — deferred, but the field is in place.
- Deposit depletion model — deferred; the prototype treats deposits as infinite.
- Rocketry tech unlock — designed (ERAS.md), not implemented; requires a skeleton tech system.

---

## 2026-06-14 — Calendar polish + two-column time panel + TODO recategorisation

**Status:** Complete. Builds (Debug, MSVC) and links; default view captured (and a
zoomed crop of the time panel) to confirm the two-column layout renders.

### What was built

**Compact calendar formatters.** Dropped yesterday's `short_date` (`dd/mmm/yyyy`).
The readout is now built from two pieces: `1960 Q1` (year + quarter, formatted
inline from `calendar_date`) and `Jan 01` (`ui::fmt::month_abbrev` + zero-padded
day). The epoch is unchanged (day 0 = Jan 01 1960). A brief intermediate
`long_date` (`January 1st 1960`, with full month names + ordinal) was tried and
then removed when the layout settled on the compact two-line block; only
`month_abbrev` remains.

**Quarter progress bar.** Replaced the `Q1 - Day N` text with an ImGui
`ProgressBar` driven by `ui::fmt::quarter_progress(day)` (0..1 through the 90-day
in-year quarter), labelled with its percentage. Since the economy resolves on the
quarter boundary, the bar doubles as a countdown to the next economy tick.

**Two-column time panel.** The two stacked top-right panels (system tick readout +
speed controls) are merged into one `##time_panel` window, split 25% / 75% via a
two-column stretch table: a left calendar block (`1960 Q1` / `Jan 01` / progress
bar in three rows) and the compressed speed controls on the right (`Sim` counter +
the pause/1–5 buttons). The panel now takes input (it was previously NoInputs);
the explorer's top is keyed off the single panel height (`mm_h * 0.5`).

**TODO recategorisation.** `docs/development/TODO.md` now declares an explicit
category set — UI categories (Canvas, Menu, Ledger, Documentation, Known Bug) plus
game-system categories mirroring `SYSTEMS.md` — and every item sits under exactly
one. Existing items were re-homed (selection-info ledger → Ledger, hover-card →
Canvas, label-stepping → Known Bug, menu-definition → Menu). The per-session
changelog paragraph was dropped (the items stand alone; session history lives
here). Three new items were filed: Circumplanetary 0.3 AU max zoom (Canvas, [1]),
map-lens icons (Canvas, [2]), Tile Ledger default body from the active view
(Ledger, [2]).

### Open items

- The three new TODO items are recorded, not implemented.
- Combined-panel verified on the default (Planetary) view; an interactive pass over
  the quarter bar advancing across an economy tick is still worth a look.

---

## 2026-06-13 — TODO follow-ups: zoom/scale, calendar, highlight ties, overlay controls, icon nav

**Status:** Complete. Builds (Debug, MSVC) and links. Worked through every
`TODO.md` item at difficulty 3 and below — the follow-up revisions raised against
the building-blocks session — leaving the difficulty 4+ items (selection-info
ledger, stepped-label bug) and the deferred (difficulty 6) work.

### What was built

**[3] Zoom slider direction + shared Circumplanetary scale/zoom.** The Solar zoom
slider was reversed (dragging right zoomed *out*). Factored the scale-bar + zoom-
slider block out of `solar_system_canvas.cpp` into a shared
`ui::draw_scale_zoom_overlay` (`src/ui/canvas_scale.{hpp,cpp}`); both the Solar
and Circumplanetary canvases now call it. The slider now drives the zoom factor
directly on a logarithmic track, so **right = zoomed in, left = zoomed out**, and
shares its `[zoom_min, zoom_max]` bounds with the scroll-wheel handler on each
canvas. The Circumplanetary canvas gained the scale bar + zoom slider it lacked.

**[2] Date format & epoch.** `ui::fmt::short_date` switched from `Y1 M05 D12` to a
`dd/mmm/yyyy` form with month abbreviations (`01/Jan/1960`); added a month-abbrev
table + `month_abbrev`, and a `campaign_epoch_year = 1960` so day 0 is `01/Jan/1960`
(`calendar_date::year` is now the calendar year, not a 1-based campaign year). The
in-year quarter readout is unchanged.

**[2] Highlight resolution on ties.** Overlapping markers each drew their own hover
ring (the per-entity hit-test set `this_hovered` independently). Each canvas now
runs a hit-test pass that resolves a **single** hovered entity — nearest centre to
the cursor, entity id breaking exact ties (arbitrary but stable) — before drawing,
so a tie highlights one entity. Solar and Circumplanetary resolve the hovered body
up front; the Planetary canvas defers the hover outline to the single nearest hex
copy (selection still draws on every visible wrap copy). Documented the convention
on `resolve_highlight` (tie resolution is the caller's responsibility).

**[3] Relocate overlay controls + default lens.** Removed the minimap **mode bar**
(the three overlay-mode dots) — the inset now uses the full height under the title.
Added `ui::draw_overlay_controls`, a bottom-left strip of labelled mode buttons
(Supply / Market / Faction) running from the nav-rail edge inward, clear of the
centred scale/zoom control. `ui_state::overlay` now defaults to `supply` rather
than `none`. The on-canvas legend chip was dropped (the strip names the active
lens); `draw_canvas_overlay` is now a no-op extension point for real lens geometry.

**[3] Narrower, icon-based nav rail.** `nav_pane_width` 200 → 56; the pane is now an
icon rail of ten square slots, each a glyph (`src/ui/icons.hpp`) with the menu name
in a hover tooltip instead of a worded label. Added `icons::ledger` (ruled-table
glyph, for the wired Tile Ledger slot) and `icons::placeholder` (hollow square, for
reserved slots). Decoupled the profile from the rail width — added
`profile_panel_width` (200) so the profile and the header stay wide while the rail
is narrow; the header now starts at the profile's right edge, and the Tile Ledger
window spawns clear of the profile/header.

### In-session decisions

**Slider drives zoom directly, not visible-AU.** The old slider edited a derived
"visible AU" value (0.5–50) inverted relative to zoom, which is why it read
backwards. Driving `zoom` directly on a log track makes the direction obvious and
lets the slider and wheel share one `[zoom_min, zoom_max]` range per canvas.

**Profile width decoupled from the nav rail.** The profile previously aligned to
`nav_pane_width`; narrowing the rail to 56 px would have crushed the portrait +
name. Gave the profile its own `profile_panel_width` so the rail can be an icon
column without distorting the identity panel above it.

### Open items

- Per-menu nav icons are placeholders (one ledger glyph + a generic reserved
  glyph) until the menu set is defined (TODO `[6]`).
- Changes verified by build + code review; an interactive click-through / screenshot
  pass of the new slider direction, overlay strip, and icon rail is still worth doing.

---

## 2026-06-13 — Pre-Layer-3 UI building blocks + asteroid belt (label-shimmer fixed)

**Status:** Complete. Builds and runs (verified after each item; solar view
captured to confirm the belt and the new font render). Worked through every
`TODO.md` item below difficulty 6 ahead of Layer 3, leaving only the two
deferred (difficulty 6) items — the hover-card system and the menu definition.

Eight items, each a small focused module under `src/ui/`, built and
build-verified one at a time. Foundational/shared primitives first so later
items reuse them; the meatiest (asteroid belt) last.

### What was built

**[2] Value & date formatting helpers — `src/ui/format.hpp` / `.cpp`.**
`ui::fmt` with `abbreviate` (1.2k / 3.4M / 5.0B), `credits` ("Cr 1.2k"),
`signed_delta` (+/−/"±0"), `rate` ("+1.2k / qtr"), `percent`, `sign_of`, and the
deferred **calendar** (`date_from_day` / `short_date`). The calendar completes
sim_loop's tentative constants (30-day months, 3-month quarters) with a defined
4-quarter / 360-day year. The system-tick readout now shows `Y1 M05 D12` and
`Q2  -  Day N` instead of raw counts, and the header budget/stockpile
placeholders route through the formatters so live numbers drop straight in.

**[3] Presentation metadata — `src/ui/presentation.hpp` / `.cpp`.**
Single source of truth for resource identity (`resource_presentation`: name,
abbreviation, identity colour) replacing the duplicated `resource_labels[]` in
`tile_inspector.cpp` and `body_surface_canvas.cpp`. Plus a **semantic palette**:
positive/negative/neutral (deltas), selection/hover/pinned (interaction), and
six reserved faction colour slots (the data model already permits multi-faction).
`value_colour(...)` maps a signed value to its palette colour. Demonstrated in
the Tile Ledger market table (resource colour swatch; price coloured by its move
vs. base_price).

**[2] Shared selection / hover / pinned highlight convention — `src/ui/highlight.hpp` / `.cpp`.**
`resolve_highlight(selected, hovered, pinned)` (precedence selected > pinned >
hovered) plus `draw_body_highlight` / `draw_hex_highlight` using palette colours.
All three canvases refactored onto it; this **adds a hover ring** (light blue) on
bodies and tiles, previously only a tooltip. Pinning is reserved in the
vocabulary (amber) but not yet driven.

**[2] Focus-on-entity helper — `src/ui/view_nav.hpp` / `.cpp`.**
`focus_on_body` / `focus_on_surface` / `focus_on_tile` / `focus_on_entity` — one
call that selects an entity, chooses the rung that frames it, and centres that
rung. Rung rule: a *body* frames in its orbital/local view (star → Solar,
planet/asteroid/station → its own Circumplanetary, moon → parent's with the moon
selected); something *on* a surface → that body's Planetary surface. The opening
view now goes through `focus_on_surface` rather than poking `ui_state`.

**[3] Render-time interpolation building block — `src/ui/interp.hpp` / `.cpp`.**
The decided read path for fractional-progress entities before convoys exist:
`lerp` (float/ImVec2), an `interpolated<T>` (previous/current with `advance` and
`at(alpha)`), and `econ_tick_alpha` / `day_alpha` from the continuous
`elapsed_days` clock. Layer 5 convoys will hold `interpolated<float> progress`
and read `lerp(route_start, route_end, progress.at(econ_tick_alpha(...)))` to
glide across the quarter instead of jumping at the boundary. No consumer yet
(documented building block).

**[4] Canvas overlay layer + mode switching — `src/ui/overlay.hpp` / `.cpp` + `ui_state`.**
`overlay_mode { none, supply, market, faction }` in `ui_state`; an overlay draw
pass (`draw_canvas_overlay`) over the primary canvas, below the chrome; and the
minimap **mode bar made interactive** — three dots toggle the three lenses
(click the active dot to clear). No overlay data yet, so an active lens draws
only a bottom-left legend chip naming itself — the single extension point where
Layer 5 supply routes etc. will hang their geometry. Mode bar height bumped 10→14
px so the dots are clickable.

**[3] Icon/glyph + font-atlas strategy — `src/ui/fonts.*`, `src/ui/icons.*`.**
Decided with the developer: **vector glyphs via the draw list** (no font asset)
and a **system TTF loaded with oversampling** (`OversampleH=3`, `PixelSnapH=false`,
candidates under `C:/Windows/Fonts`, falling back to an oversampled built-in
font). The oversampled atlas improves glyph crispness. **Note:** this did *not*
resolve the label-motion artefact — the developer reports body labels still
advance in steps every few ticks rather than gliding, so the bug is re-logged in
`TODO.md` (Known bugs) with that sharper symptom; the cause appears temporal, not
purely sub-pixel rasterisation. `ui::icons` draws building
(diamond/square/triangle), resource pip, and unit chevron glyphs; the Planetary
built-tile marker now uses the building-type glyph instead of a uniform white
square.

**[4] Asteroid belt as a textured ring — `world.belt`, `hard_coded_world.cpp`, `solar_system_canvas.cpp`.**
A `world::belt` (`asteroid_belt` { inner/outer radius }) — system-level data,
**not a body**. The Solar canvas renders it as a translucent annulus (thick ring
stroke) with a deterministic fixed-seed scatter of dusty specks (positions in AU
space, so it pans/zooms with the view and holds still between frames). One
**notable asteroid** — Pallas (the prototype keeps a single belt body per the
developer's call) — is an ordinary `body_type::asteroid` entity at a radius
inside the band, drawn *over* it in the normal body pass, so it stays
hoverable/labelled/selectable and carries a small 30×14 tile grid (no water) so
its surface is explorable. The Solar auto-fit extent now includes the belt's
outer radius.

### In-session decisions

- **Font: system TTF + oversampling (developer choice).** Crisp, fluid labels
  with no committed binary asset; Windows font path for now (prototype is
  Windows-only). A bundled `assets/fonts/*.ttf` can be prepended to the candidate
  list later for portability without touching call sites. Default size 16 px.
- **Icons: vector glyphs via the draw list (developer choice).** Crisp at any
  zoom, no atlas/licensing, matches how the canvases already draw.
- **Notable asteroids are separate bodies drawn over the band**, not markers
  embedded in the ring — resolving the open question in the belt TODO. Keeps them
  uniform with every other body (selectable, labelled, own surface).
- **Overlay building block lands now, data later.** The mode bar drives a real
  `ui_state.overlay` + draw pass; with no economic data yet it shows only a
  legend chip. This reserves the mechanism so Layer 5 adds geometry, not plumbing.
- **Highlight precedence selected > pinned > hovered**, single ring per entity, so
  a pinned entity keeps its identity colour under the cursor and selection always
  wins.

### Open items

- **Overlay lenses have no data.** The pass and toggle exist; supply-route
  geometry (Layer 5) is the first real consumer.
- **Pinning is unwired.** The highlight convention and `focus_on_entity` are
  ready, but nothing sets a "pinned" state yet (the Explorer is still a
  placeholder).
- **Interpolation has no consumer** until convoys exist (Layer 5).
- **Font candidate list is Windows-only** — fine for the prototype; revisit for
  cross-platform with a bundled font.
- **Asteroid surfaces** use the generic tile generator at a small grid; proper
  asteroid terrain/deposit authoring is for the extraction layer.

---

## 2026-06-13 — version 0.0.2 — Layer 2 finalisation (standardised body grids, infinite side-scroll, zoom floor)

**Status:** Complete. Builds and runs. **Tagged snapshot.**
**Backup:** `backups/v0.0.2/` (copy of `src/` at this version — the rollback point for v0.0.2; previous snapshot is `backups/v0.0.1/`).

The hard-coded world is pared to three surface bodies on standardised grids, the Planetary canvas scrolls horizontally without bound, and its zoom floor is derived correctly. Layer 2 is considered finalised at this version.

### What was built

**Standardised body grids (~9:5 width:height).**
`make_hard_coded_world` now sizes the two planets at **180 × 84** (columns × rows) and Selene (Kepler's moon) at **90 × 42** — the same ratio at half scale. The height is a little under half the width by design: the width spans the full circumference (both hemispheres) and the height is pole-to-pole with the non-traversable polar caps truncated. The stale generation TODO (which described the rule as unenforced) was replaced with a comment recording the settled ratio.

**Backdrop bodies removed.**
With the canvas perspectives settled, every body except Helios, Cinder, Kepler, and Selene was deleted from the world: Veld, Ochre, Vesta, Ceres, Pallas, Bastion, Forge, Cyra, Halo, Mote. Vesta's hand-authored tiles, extraction site, and market went with it. The now-unused `create_simple_body`, `tile_spec`, and `create_tile` helpers were removed; `hard_coded_world.hpp`'s doc comment was rewritten to list the three surviving surface bodies.

**Infinite horizontal scroll on the Planetary canvas.**
`draw_body_surface_canvas` now draws each tile at every integer wrap offset `k` whose copy falls within the canvas, where the grid repeats every `period_px = gw * col_step * zoom`. The `k`-range is derived per tile from the visible x-extent, so panning past either edge continues seamlessly from the far side with no seam and no special-casing of "three offsets". Hit-testing runs inside the same copy loop, so the hovered/clicked column is always correct regardless of wrap. Horizontal pan is wrapped with `fmod(pan_x, period_px)` each frame to stop `pan_x` drifting without bound — visually identical because the grid is periodic.

**Planetary zoom floor derived from the height-normalised zoom.**
The minimum zoom was a guessed constant (`0.2f`) unrelated to the zoom definition, so it let the grid shrink to ~19% of the canvas height (viewport showing ~525% of the grid). Since zoom is normalised so the grid fills `kFitMargin` (0.95) of the canvas height at zoom 1, the floor is now derived: `kMinZoom = 1 / (kMinZoomHeadroom * kFitMargin) ≈ 0.877`, where `kMinZoomHeadroom = 1.2` means the viewport spans ~120% of the grid height at minimum zoom (full grid + ~20% headroom). Max zoom (`kMaxZoom = 20`) is unchanged; the stored zoom is clamped to `[kMinZoom, kMaxZoom]` each frame as well as in the wheel handler.

### Docs

- `PLANETARY.md` updated: new "Target size and aspect ratio" wording (180×84 / 90×42, the 9:5 rationale, three-body world), the horizontal-wrap and interaction sections describe the seamless side-scroll, and the deferred table now lists only seam *visualisation* (an explicit wrap marker) as post-prototype.
- `TODO.md` gained a **"UI building blocks (decide before Layer 3)"** section capturing the rendering primitives Layers 3–6 will need but Layer 2 simplified: a tooltip/hover-card system (recorded then **deferred to difficulty 6** at the developer's call), centralised resource/palette presentation metadata, shared value/date formatting, a canvas overlay layer + mode-bar wiring, an icon/font-atlas strategy (folds in the label-shimmer fix), a shared selection/hover/pinned highlight convention, a "focus on entity" view-navigation helper, and render-time interpolation for fractional-progress entities (convoys). The stale Vesta/Ceres/Pallas reference in the asteroid-belt item was corrected to note the backdrop bodies are now removed.

### Versioning / backups

- This session is tagged **version 0.0.2**; `src/` is snapshotted to `backups/v0.0.2/` as the rollback point. DEVLOG headings now carry an explicit version marker + Backup line for any tagged snapshot (see the note at the top of this file). Decision on whether to split DEVLOG into per-version files: **kept as a single newest-first file** — version markers make rollback points easy to find by search, and one file preserves chronological review and grep across the whole history. Revisit only if the file becomes unwieldy.

### Open items

- Procedural generation still seeds water from the centre row only; with the taller 84-row grids the polar caps are land by default. Whether the caps should read as ice/barren rather than ordinary land terrain is unaddressed.

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
