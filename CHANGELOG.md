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

## [0.1.9] — 2026-08-09

**Shell & legibility — the standing UI set, finished.** Eight items closing the follow-through that
had been accumulating since v0.1.1: the always-on canvas layers become decodable, stacking becomes
a decision rather than a dominant strategy, screen geometry gets one owner, and disclosure stops
being a single all-or-nothing control.

### Added
- **A contextual road-tier legend** (BL-184). Three tiers had been drawn by line weight and
  brightness alone with no key anywhere. Selecting or hovering a road-carrying tile now names its
  tier, with the thin→thick ladder in a tooltip. Contextual rather than a persistent chip: roads
  render always-on like terrain, not as a lens, so the per-lens legend drawer could never have
  carried them.
- **Building stacks are a real trade-off** (BL-193). Site *k* on a tile now produces `0.8^(k-1)` of
  a lone site's rate — 2 sites yield 1.8×, 5 yield 3.36×, never 5× — while every site draws real
  reserve, so a full stack drains the deposit faster than its output multiple. Depletion tapers
  against the stack's **combined** nominal, so members exhaust together instead of desynchronising.
  The construction ledger states where a site sits in its stack and what that costs it.
- **The UI justification store** (BL-260) — `docs/ui/question_log.json`, 16 surfaces each declaring
  the question it answers, why it earns its space, and the backlog item that demanded it. A two-way
  provenance index: surface → item, and item → surface.
- **The Economy panel has a door** (BL-292). It was drawn every frame while nothing in the nav rail
  could open it — reachable only by the verify harness. It now holds rail slot 3 and has an
  `ACTIONS.json` entry, so a human reading the rail and an agent reading the dictionary can both
  find it.

### Changed
- **Roads dim with the commercial-reach fog** (BL-185). A road on an unreached tile had rendered
  identically to one on a tile the player actively operates. Both the lens fill and the road spans
  now pass through one `fog_dim`, so the fog is literally a single wash. Per-edge vision takes the
  **max** of the two tiles: a span is drawn as two halves meeting at a shared midpoint, and max is
  the only cheap combiner under which both halves agree.
- **Disclosure is two controls** (BL-265) — *expand in place*, or *take the canvas*. Full screen now
  bounds itself to the canvas rather than the window, so the header, clock, comms dock, Selection
  band and minimap survive it. A full-screened accordion shows **all** of itself, scrolled — the
  whole round of chain stages, all four dashboard roll-ups. Controls are right-aligned in one
  column everywhere, and every glyph is **drawn** through the draw list rather than typed, which is
  the missing-codepoint defect BL-234 already fixed once.
- **Screen geometry has one owner** (BL-216). `shell_metrics.{hpp,cpp}` now owns the shell's
  composed rect algebra, and the five places in `app.cpp` that each re-derived the right chrome
  column's left edge by hand were migrated onto it.
- **The comms channel strip became a selector.** A wrapping tab chain spends vertical rows, and the
  docked comms panel is 260 px tall; a combo holds any number of channels in one row.

### Removed
- **The History ledger's Tiles view** (BL-281) — a current-state readout inside a ledger about the
  past. Renaming it would have fixed the label and kept the defect. Its content already had homes:
  buildings on the canvas and in the Selection element, market data in the market surfaces. Story,
  Chain and Ages remain.

### Known gaps
- **The econ tick roughly doubled** (BL-347, priority A). Measured against the parent commit on the
  same machine: the largest sweep rung's `min` went 0.958 ms → 2.045 ms, with the cost present at
  every rung including the smallest. **Prototype scale is unaffected** — 0.20 ms mean, 5× headroom —
  so this is lost growth headroom rather than a live frame-rate problem.
- `building_profit` still estimates a lone site, so a stacked building over-reports revenue and
  remaining life (BL-346). Not display-only: the loss-streak reflex and the workforce solver both
  act on that number.
- The building-selection element still does not follow the tile element's three-column format
  (BL-229) — deliberately deferred, as the item reserves that layout for Ben to design.
- 13 of the justification store's 16 entries are `drafted`, awaiting Ben's wording. Writing the
  pair *is* the design check, so they are drafted rather than shipped as settled.

## [0.1.8] — 2026-08-09

**Build health — the gate stops lying.** No player-facing content: this is the minor where the
project's own tooling stopped costing time on every session that tripped it. The headline number
is that `ctest` reported **ten failures of which exactly one was a failing assertion**, and the
other nine were the suite misreporting itself.

> **Cut order.** Numbered v0.1.8 to avoid renumbering the design-forward stubs at v0.1.3–v0.1.6,
> but cut ahead of them deliberately — number is not sequence in this band.

### Changed
- **Three test tiers instead of one flat 60-second bound** (BL-288). Four harnesses pass but
  exceed it (`earthlike_lean_trace` 121 s, `notable_worlds` 105 s, `mediterranean_sweep` 87 s,
  `earthlike_tile_census` 58 s) and were being reported as failures; the long tier rises to 240 s
  and covers them. Two open-ended research sweeps get a `sweep` label, no timeout, and are out of
  the routine gate — they answer design questions over many worlds and will outgrow any bound.
  Two harnesses that assert *absolute* wall-clock times get a `bench` label, so a failure reads as
  "re-run on an idle machine" rather than as a regression. The gate is now
  `ctest --test-dir build_linux -LE sweep --output-on-failure`.
- **`next_id.js` fails loudly instead of silently defending nothing** (BL-322). It reported zero
  refs because `execSync` runs through `dash`, which aborts on the unquoted `(` in
  `--format=%(refname)` before `git` ever runs — so the tool worked on the Windows box where it
  was written and failed silently everywhere else. It was issuing ids **25 below the true
  ceiling**, which is the mechanical account of how BL-326..BL-333 each landed twice. Now uses
  `execFileSync` with argv arrays, cross-checks with `git show-ref`, and exits non-zero when it
  cannot prove it ran. Refs scanned: 0 → 53.
- **GCC goldens re-blessed** (BL-285) — the one real defect in the ten. Stale since 2026-08-01;
  the divergence is downward and explained by the v0.1.2 reach rule (siting is now bounded) plus
  unit hiring (a new cash outflow), and that reasoning is recorded in the harness rather than
  assumed. The MSVC set is now stale for the same reason and is flagged in place.
- **Ladder lines carry a `ladder_rung` tag** (BL-285), so the history-ladder check filters
  structurally instead of pattern-matching biography prose — rewording a line can no longer
  silently change what the check asserts over.

### Added
- **Seeded FetchContent cache** (BL-302) — per-dependency `FETCHCONTENT_SOURCE_DIR_<dep>` from a
  shared `_deps_cache`, so fresh worktrees configure offline. The item's own preferred option, a
  shared `FETCHCONTENT_BASE_DIR`, was tested and **hard-fails** across build trees on a
  generator-locked subbuild cache. Every configure now states its mode, and a deliberate from-cold
  check is documented.

### Fixed
- `world_audit` was **never broken** (BL-291) — it exits non-zero on 1 assertion of 26. The
  measured landform census stands, and `TILES.md`'s last "needs re-measuring" note is re-measured:
  Kepler's 0.0% valley is a real wet-body property, not a stale artifact.

### Known gaps
- The from-cold check has **not** been run on Windows, where the original schannel TLS revocation
  failure actually occurs — it does not reproduce on Linux, so the fix is untested against its own
  symptom. Filed as BL-341.
- The gate's one remaining failure is `world_audit`'s biome-balance assertion (forest+wetland
  2.41% against a 3% target). That is a world-generation finding carried by BL-338, not an
  instrument defect — the gate reporting it is the gate working.

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

## [0.1.1] — 2026-08-09

**The word interface.** The route from documentation to a text-driven player, and the groundwork
the v0.2.0 AI opponent is built on: an agent can now read the world state, look up what every
control *means*, and drive the game over a socket — with no HTTP client, no API key and no cloud
dependency anywhere in the engine. Ships alongside the shell work that ran in the same band.

> **Cut order.** Tagged after `v0.1.2`, whose buildings theme was finished first. Pre-1.0 numbering
> is advisory (see the header note); entries are ordered by version, not by tag date.

### Added — the word interface (the theme)
- **Blackboard export** (BL-206) — world state an agent reads through a stable export instead of
  scraping the UI.
- **The action dictionary** (BL-270) — all 115 controls as `{press, typed args, preconditions,
  expected output, reason to select}`, across five families. The gameplay family is *transcribed*
  from `corp_command.hpp` rather than authored beside it, so the seam cannot drift from its own
  description. `ACTIONS.json` is canonical; `ACTIONS.md` and a compact `ACTIONS_INDEX.json` are
  generated.
- **The Io MCP server** (BL-278) — `ProjectIo --serve` plus `tools/mcp/`, making both of the above
  drivable by any agent runtime. Touches no simulation code.

### Added — shell, canvases & legibility
- **The sticky detail card family** (BL-194–BL-198) — card frame, the Selection element moved
  wholesale into it, recursive bounded drill-down, a titled dual-axis chart container, and the
  resource time-series store behind it. **Drill-through** becomes one shared progressive-disclosure
  idiom (BL-214), and every chart declares the question it answers (BL-247).
- **Corporation dashboard** (BL-248) as roll-up cards.
- **Commercial-activity fog** (BL-150–BL-154) — the unlit map as a dim shadow, intra-body reach on
  the Planetary canvas, and convoy vision beams with moving head/tail corridors.
- **Radial tech-tree viewer** (BL-310, F9) — the Era 0/1 quest trees as a constellation.
- **New World wizard** split to 1/3 controls + 2/3 preview, with a real-tile globe (BL-319).
- **Mediterranean rift sea** (BL-276) — a continental rift basin floods into an enclosed sea.

### Changed
- **Hover behaviour** — a card freezes until the cursor leaves its bounds, retiring the dwell
  timer (BL-228), and hover settles into glance-then-stick (BL-230).
- **Comms dock** moves bottom-left at the Selection band height (BL-227); the **minimap** wraps
  flush to the canvas edges (BL-312); the **time controls** reflow into the header's own height
  (BL-313).
- **GPU utilisation and multicore threading** for the growing generation/render load (BL-267).

### Known gaps
- **The write leg is partial.** `place_sell_order`, `remove_sell_order` and `set_workforce_auto`
  appear in the dictionary but have no `corp_verb`, so an agent can read the order book and be told
  what the press means without being able to place a standing sell order. The cause is structural
  rather than three missing verbs: sell orders live in UI state, the world holds no order book to
  mutate, and no serialisation path touches them. `ACTIONS.json`'s note states the narrowing
  explicitly. Carried by BL-293, moved to v0.2.0.
- 22 further items that had been retrofitted onto this minor after its theme completed were
  **re-homed, not cancelled** — into v0.1.8 (build health), v0.1.9 (shell & legibility) and
  v0.1.10 (generation & content).

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

[Unreleased]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.9...HEAD
[0.1.9]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.8...v0.1.9
[0.1.8]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.1...v0.1.8
[0.1.2]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.0...v0.1.2
[0.1.1]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.2...v0.1.1
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
