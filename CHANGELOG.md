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

[Unreleased]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.5...HEAD
[0.0.5]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/Ben-Booth-125/Project-Io/releases/tag/v0.0.1
