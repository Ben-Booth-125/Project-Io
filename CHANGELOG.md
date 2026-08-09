# Changelog

All notable changes to Project Io are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/) (pre-1.0: minor/patch are advisory while
the prototype is in flux).

Each released version corresponds to an annotated git tag (`vX.Y.Z`) — the tag is the
authoritative version-history record. A local-only snapshot of `src/` is also kept under
`backups/vX.Y.Z/` as a convenience rollback point (gitignored, not the record). See
`docs/development/DEVELOPMENT_PRACTICES.md` § Cutting a release.

## [Unreleased]

_Nothing yet._

## [0.1.2] — 2026-08-09

**Buildings rework — remoteness stops being free.** The minor that the simulated-play arc was
waiting on. Siting a building is now a decision with a trade-off rather than a lookup of the
richest tile on the map, and construction reads as a process on the canvas instead of a silent
timer.

> **Cut order.** This tag lands before `v0.1.1`, whose word-interface theme is cut separately
> from earlier work. Pre-1.0, minor numbering is advisory (see the header note); each tag
> documents its own theme rather than a strict chronology.

### Added
- **A logistical maximum range on placement** (BL-323) — the rule that had no code at all. A site
  must sit within a bounded logistics cost of a supply anchor (city, port, or inland logistics
  hub), computed as a multi-source Dijkstra reach field over the terrain-weighted cost function
  logistics already provided, cached per body and invalidated on every mutation that can move an
  anchor. The budget is authored in `economy.lua`, not hard-coded: at the shipped value, 77.4% of
  Kepler's land is placeable, no land tile is unreachable, and all 20 generation-placed buildings
  already sit inside it — so generation and the rule agree without retuning either.
- **A first-anchor bootstrap** — a body with no anchor refuses everything *except* an anchor, so
  working a virgin moon begins by planting a logistics hub. Deliberate mechanic, not a side effect.
- **Site-dependent build time** (BL-323) — `build_duration_ticks` becomes a per-type *base*,
  scaled by landform, by distance from the nearest supply anchor, and discounted where a tile
  already carries an established stack. A remote mountain site is now a considered commitment.
- **Construction as a visible process** (BL-323, BL-327) — a dedicated under-construction glyph on
  the Planetary canvas replaces the old dimmed-marker treatment, with remaining ticks on the
  hover card.
- **Eleven more extraction targets** (BL-323) — `k_extractable` widens from 4 to 15, covering every
  resource tile generation already deposits but no building could reach, plus the Smelter's
  iron/nickel → steel recipe.
- **Pre-commitment supply warning** (BL-328) — the construction ledger says a build will starve
  *before* you commit to it, rather than after it stalls.
- **Expandable building groups** in the construction ledger, two-tier alphabetically sorted
  (BL-326).

### Changed
- **Clicking a building fills the Selection element again** (BL-330) — it had been skipping
  straight to management, contradicting the settled single-click-selects / double-click-navigates
  click model.
- Placement surfaces stop offering tiles the gate will refuse — the rule is applied wherever a
  tile is *offered*, not only where a build is committed, so an offered-then-rejected tile no
  longer reads as a broken build.

### Removed
- **The corp-building reach circle** (BL-329) — the fog and the new reach rule now carry that
  information between them.

### Verification
- `buildings_rework_harness` 12/12 PASS; `logistics_reach_harness` 26/26 PASS.

### Known gaps
- The **processing half** of the roster (Chemical Plant, Electronics Lab, Fabricator, Assembly
  Plant, most Refinery outputs) is **not** in this cut. It needs new `resource_type` values with
  market, price and serialisation wiring — a save-format change rather than Lua authoring — and is
  filed as BL-340 (processing-chain roster) so closing the rework did not silently drop it.

## [0.1.0] — 2026-08-03

**The prototype cut.** The economy loop, validated and playable end-to-end — construction,
population-grounded workforce, spatial price divergence via convoys, gated discovery, scoped
competitor intelligence, a legible budget, and a green, performance/data-growth-audited build.
This is the milestone `ROADMAP.md` names as the v0.1.0 done-definition; everything past it
(laws, techs, military, politics, the AI opponent) is the v0.1.x → v0.3.0 arc, not this cut.

### Added
- **Roads & planetary logistics** — a generated per-nation road lattice (three tiers), cities as
  free logistics hubs, and the inland logistics-hub building, landed ahead of its originally
  planned v0.1.1 slot.
- **Continent lens** (first cut) and the **Continents/Drift** generation pass feeding plate-biased
  heightmaps into tile generation, with `plate_id` retained for the lens.
- **Terrain landform render and spanning markers**, plus **terrain combat modifiers** replacing the
  flat `is_barrier` field with a graded, weighted terrain cost (water weight 750, swept to hold the
  political map at 30 Kepler nations).
- **Tile construction ledger** — a world-layer cost estimator with per-recipe rows, surviving a
  build across selection changes.
- **Quality-audit instrumentation** (no new systems, three new harnesses): a **frame-budget**
  reporter (F11, off by default — build/submit/present/other breakdown, avg/max/1%-low against the
  8 ms / 16.7 ms targets); an **econ-tick scaling** sweep (0.0018 ms/tick mean at prototype scale,
  555× headroom, growth-shape asserted at size^1.28 against a size^1.5 tolerance across a six-rung
  bodies × corps sweep); and a **data-creep** harness (~30 counters + RSS over 1500 ticks of the
  real generated world, asserting a plateau rather than reporting a snapshot).
- **Convoy data-creep coverage** — convoy dispatch now runs through the audit rollout, so the
  convoy / trade-route / glimpse plateaus bind instead of passing vacuously.

### Changed
- **Font glyph range** widened — 43 sites across 7 codepoints were missing, not the 26/2 originally
  filed.
- **Headless golden bands pinned per toolchain** (`ai_skill_harness`) rather than widened to cover
  both — a shared band spanning MSVC and GCC's divergent net-worth outputs would span ~±100% and
  catch nothing.
- **Visual goldens declared Windows-authoritative**; the Linux visual-verify job is advisory
  (inspect by eye, don't golden-diff).
- **The full 229-golden visual suite re-blessed** after two days of silent staleness from the
  terrain re-price above — the diffs were world content (terrain colour, generated corp/nation
  names, balances), not a UI regression. `DEVELOPMENT_PRACTICES.md` now names the missing
  discipline: a `src/world/` change that reshapes generated output re-blesses the visual suite in
  the same commit, mirroring the existing shared-chrome rule.

### Fixed
- Windows headless-harness gate — the absolute 1 ms tick bound at the largest swept rung was a
  Debug-build artefact (R5 still passes with 19× headroom in Debug; every growth-shape assertion
  holds either way), not a regression.

### Verified
- Headless harness suite green on Linux (Release) and Windows (Debug) at the same commit for the
  first time (42/42 — `mediterranean_sweep`'s 60s CTest timeout is a harness-runtime artefact,
  confirmed passing standalone; not a v0.1.0 item).
- Visual golden suite re-established against current world content (229 goldens re-blessed).
- Windows build green (residual warnings are third-party sol2/ImGui headers, not project code).

## [0.0.9] — 2026-07-05

Budget clarity + polish — the remaining legibility rough edges cleared before the v0.1.0 quality
audit. (The budget-clarity strands that originally named this minor shipped early, folded into
v0.0.8.)

### Added
- **In-app system menu** — a corner gear (hamburger) button opens a session-control popup:
  Pause/Resume (one shared flag with the Space hotkey) and Exit Game (inline "Really quit?" confirm,
  since there is no save). Esc toggles the same popup.
- **Corp emblem system** — the geometric corporation emblem promoted to a shared `ui::icons` glyph
  family (shape + identity colour, both a pure function of the corp id), rendered for the player and
  rivals wherever a corp is identified: the profile card, the Selection header, on-canvas
  building/HQ owner tags, and the rival hover card. Card, markers, and canvas tint now share one
  colour source of truth.
- **Commercial-fog hover line** — the Solar-canvas body hover tooltip now carries a short activity
  read keyed on the commercial-sphere fog (the first of the two BL-089 deferrals).

### Changed
- **Economy-panel legibility** — the Corporation-balances table widened to show full corp names and
  full balances (no single-character cells); the Workforce, Stockpile-pools, and Markets tables
  un-cramped the same way; the redundant per-building table removed (per-building profitability
  lives in the Corp Dashboard).

### Fixed
- **Construction panel overlap** — the Construction panel no longer occludes the bottom-left
  Selection element during placement, so the build front door and the Thrives/Valid affordance
  readout stay visible while a build is armed.

### Deferred
- **Proximity-glimpse peek** — the third commercial-fog illumination geometry (a faint "peek" from
  routing past a frontier body) held back: it needs a save-seam field or orbital back-computation,
  disproportionate for a polish minor. Re-assess at the v0.1.0 boundary.

## [0.0.8] — 2026-07-04

Discovery & intelligence — the economy gains information asymmetry, and the human / industry / player
layers become legible on the map.

### Added
- **Survey system** — bodies start unsurveyed; a player-dispatched survey (credit cost + N ticks)
  reveals the tile map and deposit bands. Home begins surveyed. Surfaced on the Solar/Planetary
  canvases and the Selection panel.
- **Competitor-visibility model** — rival buildings are visible on-canvas; their production and
  stockpiles are private; market prices and supply/demand aggregates are public (the intelligence channel).
- **Population legibility** — the Population lens re-keyed to workforce efficiency; the Selection panel
  and hover read scale / local habitability / workforce cap.
- **Population-centre markers** — the generated settlements drawn as clustered conurbations with tiered
  skyline markers (City+ labelled), civic-neutral by default and host-nation-tinted under the Country lens.
- **Player presence** — always-on home-cluster ring + an HQ star on the origin building + a home halo
  on the home body in the Solar canvas.
- **Industry-density lens** — the already-live nation-owned substrate rendered as a resource-weighted
  throughput field (occupation × terrain richness), distinct from the population read.
- **Ambient opportunity read** — the Opportunity margin surface confirmed glanceable at rest with its
  on-canvas key.
- **Persistent trade routes** — durable body-pair lanes recorded from convoy traffic (the substrate the
  fog reads).
- **Commercial-sphere fog** — a body-level activity fog (unknown / known / stale / visible) lit by the
  player's own trade and independent of the survey fog: Solar activity badges + lit corridors and a
  coarse market-pulse section in the Selection panel.
- **Budget cluster** (shipped early from the v0.0.9 theme) — itemised cashflow breakdown, debt-interest
  visibility, per-building profitability, and a display-options window.

### Changed
- Verification tooling: added the `trade_routes` and `commercial_fog` headless harnesses and new visual
  golden scripts (population markers, player presence, industry lens, ambient opportunity, commercial fog).

> **Note:** v0.0.6 and v0.0.7 were developed and merged as batches but never formally cut; their tags
> were reconstructed retroactively at the theme-boundary commits (`v0.0.6` → `ab7e28f`,
> `v0.0.7` → `61d9946`). v0.0.8 is a normal cut.

## [0.0.7] — 2026-06-30

Interactive & legible — the economy becomes something the player *drives*, not just observes.

### Added
- **Player construction** — armed build mode with placement validation (terrain/deposit affinity,
  slot rules), a resource build-cost model, and the construction panel as the build front door.
- **Recipe & workforce control** — per-building recipe selection and workforce allocation, with the
  contention scalar feeding production and wages.
- **Population centres** — static MVP placement plus the dynamic habitability→workforce feedback
  loop that grounds labour supply per body.
- **Selectable entity markers** — buildings and market centres are hit-testable on-canvas with
  lens-contextual hover cards; the pinned **Selection panel** shows polymorphic per-entity detail.
- **Placement-suitability surface** — armed build mode tints affinity tiles so siting is legible.
- **Lens-driven selection** — the Selection panel content follows the active map lens.
- **Full hotkey system** — unified keyboard control with an F1 cheat-sheet overlay.
- **Trend plots** — balance/market trend graphs in the Economy panel and Market Ledger.
- **App-driven mouse** — deterministic pointer input for reproducible verify captures.

### Changed
- **Cross-platform build** — Linux is the primary target with Windows CI; source-portability fixes
  and a GCC-14 / sol2 v3.5.0 regression guard.

## [0.0.6] — 2026-06-17

Improved core-loop — the market gains depth, spatial reach, and a read surface.

### Added
- **Matched price-time order book** — `clear_markets` restructured around an order book so bids and
  asks match by price then time rather than a single clearing pass.
- **Saturated substrate** — nation-owned background supply and demand that fills the world so the
  player trades into a live economy rather than a vacuum.
- **Supply convoys** — inter-body convoy routing with logistics costs, driving spatial **price
  convergence** between bodies.
- **Ledger family** — the Market, Balance, and Construction ledgers plus the Corp Overview dashboard,
  wired into the nav pane.
- **Market-centre seeding** — market centres seeded from population centres; multi-market dashboard.
- **Warm-start surface** — pre-game economy warm-up so a campaign opens on a settled market.

### Changed
- Backlog/process refit: JSON backlog (`backlog.json`) with schema v2 (`waits_on` → `requires`),
  the `verifier-review` skill, and the native-only git-write standing rule (BL-058).

## [0.0.5] — 2026-06-16

Layer 4 foundations, the map-lens family, and golden-image visual verification.

### Added
- **Placement-rules seam** — a reusable `src/world/placement_rules.*` validity check, shared by
  world generation and (future) player construction so both honour one set of rules.
- **Economy-stability harness** — `tools/verify/econ_stability.cpp`, a multi-tick stability run
  over the SDL/Lua-free economy (insurance before UI piles onto the loop).
- **Workforce pool (step 1)** — a per-`(corp, body)` labour pool with a proportional contention
  scalar feeding both production and wages (`world::workforce_supply`,
  `economy_report::workforce_contention`, `building_report::effective_workforce`).
- **Map-lens family** — Resource, Market, Population, and Scarcity overlay modes (`overlay_mode`)
  with per-body magnitude normalisation, on-canvas keys, a shared resource selector, and the
  Circumplanetary per-body market price strip.
- **Tile-centred markets** — multiple markets per body, each centred on a tile, generalising the
  single per-body exchange so price can vary within a body.
- **Player sell orders** — standing sell-orders with floor prices (`ui_state.sell_orders`) — the
  first half of the preferential-purchasing model.
- **Layer 4 UI groundwork** — the construction-panel scaffold, the Tile Selection element as the
  build front door, the informative hover-card/tooltip design, and uniform `ledger_chrome`
  ledger-window chrome.
- **Golden-image verification** — a PNG reader and per-pixel diff with a `--bless` workflow in the
  `ProjectIo --verify` harness, upgrading visual checks from eyeball-only to automated pass/fail.

### Changed
- **World generation** — orphan-island assignment (0 unclaimed land tiles, was ~12%), lean
  focus-shaped corporation starting holdings, and a corporation-generation realism pass
  (pre-game profit modelling).
- **Resource realism** — refreshed resource set and pricing.

### Docs & process
- Renamed the TODO backlog to **OPENS** with the Brief design-state model; added the **ROADMAP**
  milestone map and near-term publish plan. Defined **Batch Publish**, brief-spanning
  requirements, naive progress markers, and Brief timestamping/precedence. Completed the v0.1.0
  design pass (supply, infrastructure, inter-body markets, lens system) and migrated the
  requirements record to `requirements.json`.

## [0.0.4] — 2026-06-15

Layer 3 economy and the visual-verification toolchain.

### Added
- **Economy systems** — per-body market clearing, price resolution from local supply/demand,
  deposit depletion, corporate balance and operating costs, and a recipe registry
  (`src/world/{market_clearing,economy_system,budget_system,recipe_registry}.*`).
- **Economy observability** — an economy ledger panel and a player-balance header
  (`src/ui/{economy_panel,header_panel}.*`), with pre-game economy ticks.
- **Visual-verification harness** — `ProjectIo --verify <script>` headless capture mode and
  the `scripts/verify/*` checks (corporation lens, economy panel, header, selection go-to).
- **Headless logic harnesses** — `tools/verify/{econ_harness,world_audit}.cpp`.
- **Tooling/skills** — `verifier-visual`, `verifier-headless`, `commit`, `scoped-commit`
  skills and the `.claude/settings.json` allow rules.
- **Canvas command layer** — `src/ui/canvas_command.*`.

### Changed
- Repo hygiene: stopped tracking `backups/` (now gitignored only); set up the GitHub remote.

## [0.0.3] — 2026-06-14

Environment — the world-generation spine.

### Added
- Nation generation (Voronoi BFS territory) and corporation generation (nation assignment,
  starting-asset placement, financial profile).
- Two-axis terrain model (composition × landform) and tile-deposit tuning.

## [0.0.2] — 2026-06-13

Layer 2 finalisation.

### Added / Changed
- Standardised body grids, infinite side-scroll, and a zoom floor across the canvases.

## [0.0.1]

Initial prototype snapshot — application shell, canvases, and the hard-coded world.

[Unreleased]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.2...HEAD
[0.1.2]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.0...v0.1.2
[0.1.0]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.9...v0.1.0
[0.0.9]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.8...v0.0.9
[0.0.8]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.7...v0.0.8
[0.0.7]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.6...v0.0.7
[0.0.6]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.5...v0.0.6
[0.0.5]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/Ben-Booth-125/Project-Io/releases/tag/v0.0.1
