---
name: verifier-headless
description: Run a Project Io headless logic-verification harness. Compiles and runs a tools/verify/<name>.cpp harness over the SDL/Lua-free world/* logic and reports its PASS/FAIL assertions. Use to verify pure simulation/generation logic (economy arithmetic, tile generation, placement audits) without launching the GUI. Authorising a new check = adding a tools/verify/*.cpp harness and naming it here.
---

# verifier-headless

Compiles and runs a headless C++ harness over the `world/*` logic and reports its
assertions. The `world/*` translation units depend only on the standard library
(no SDL / ImGui / Lua), so a harness links without the GUI stack — the fast path
for verifying pure simulation and generation logic. Counterpart to
`verifier-visual` (which covers the rendered `visual` class). Design context:
`docs/development/DEVELOPMENT_PRACTICES.md` § Testing; memory `reference-headless-build`.

## Argument

The harness to run, as a `tools/verify/<name>.cpp` path or just its name
(e.g. `econ_harness`, `world_audit`). If omitted, list the harnesses in
`tools/verify/` (each `*.cpp`) and ask which to run. Exact build/run commands are
in `tools/verify/README.md`.

## Available harnesses

- **`econ_harness`** — Layer 3 economy arithmetic (production → market clearing →
  budget) against a hand-built world + registry. Links `world.cpp`,
  `economy_system.cpp`, `market_clearing.cpp`, `budget_system.cpp`.
- **`econ_stability`** — runs the economy loop (production → market clearing →
  budget) over 100 ticks on a small fixed world and asserts multi-tick stability:
  prices stay in the `[0.25×, 4×]` band, no NaN/Inf, deposit reserves decrease
  monotonically, balances stay bounded. Links the same TUs as `econ_harness`
  (`world.cpp`, `economy_system.cpp`, `market_clearing.cpp`, `budget_system.cpp`).
  **Also the econ-tick scaling instrument (BL-250, v0.1.0 quality audit):** prints
  per-tick mean/p95/max, asserts the prototype tick is well under the 1 ms ROADMAP
  target (R5), and runs a six-rung bodies × corps sweep asserting growth is no worse
  than `size^1.5` (R6). Growth is read off the **min** per-tick time — preemption can
  only ever *add* time, so the fastest tick is the cleanest estimate and the shape
  survives a loaded machine. Reading the sweep: the exponent sitting above 1.0 is
  `run_corp_strategic_step`'s O(corps × tiles) candidate scan (BL-253), not a
  regression.
- **`data_creep_harness`** — data-creep instrument (BL-251, v0.1.0 quality audit).
  Runs the **real** generated world (`make_hard_coded_world`) for 1500 ticks, sampling
  ~30 counters (entities, every component store, pools, markets, convoys, trade routes,
  AI decision ring) plus process RSS at five marks, and asserts each **plateaus**
  between the mid and final sample rather than climbing. When one climbs it *names the
  structure* — that naming is the instrument. Links the world superset (as `world_audit`).
  **Read its COVERAGE section, not just the verdict:** counters never exercised by the
  rollout are declared **vacuous** rather than reported as passing. The convoy /
  trade-route / glimpse plateaus **were** exactly that until BL-254 (2026-08-01) drove
  convoy traffic through the rollout; they now bind, and R4 fails the run outright if
  those three structures are ever un-exercised again, so the vacuous green pass cannot
  come back silently. All three plateau: convoys hold steady and drain (1500 seeded,
  1496 credited and retired), trade routes saturate at their structural bound of 18 by
  tick 36 and stay flat for the remaining 1464, glimpse stamps are overwritten in place.
  A `STILL UNEXERCISED` block names what the rollout genuinely does not touch —
  `dispatch_convoys` auto-dispatch never fires, so what is under test is the credit /
  upsert / retire path, not the dispatch decision. It also reports a **second** cause of
  the original blind spot, independent of the launchpad gate: the generated world seeds
  every market on the single tiled body, so it holds no inter-body market pair and cannot
  record a trade route at all without the stub markets the harness authors pre-run.
  Slowest harness in the tier at ~7 s.
- **`font_glyph_harness`** — font glyph-range guard (BL-234). The one harness here that
  links **ImGui** rather than `world/*`: the defect it guards lives in the `ImFontConfig`
  handed to `AddFontFromFileTTF` in `src/ui/fonts.cpp`. It still links no SDL and no Lua —
  ImGui builds a font atlas on the CPU with no renderer, window or backend — so it stays
  in the fast tier (~10 ms). Builds the atlas through the real `ui::load_ui_font` and
  probes each codepoint the UI's string literals use, via `FindGlyphNoFallback`
  (`FindGlyph` substitutes the fallback glyph and would pass vacuously for *every*
  codepoint). Two Latin-1 controls are probed alongside: if either fails, the harness is
  broken rather than the font range. Hand-declared in `CMakeLists.txt` above the glob so
  it links `src/ui/fonts.cpp` instead of the world superset.
- **`world_audit`** — builds the hard-coded world and audits Kepler biome balance
  (forest + wetland fraction), extraction-asset placement, deposit-reserve seeding,
  the reusable `placement_rules::can_place` seam (placed assets pass it; ocean /
  zero-deposit tiles are rejected), and the **generated corp starting stockpile**
  (BL-116: every corp opens non-empty + prototype-scoped on its home body; extraction
  opens richer in raws than trade; stockpiles identical across two generations).
  Links the generation TUs (`tile_generation`, `nation_generation`,
  `corporation_generation`, `population_generation`, `placement_rules`,
  `hard_coded_world`, `orbital_system`, `world`).
- **`supply_advance`** — Supply layer (BL-039 / BL-038 / BL-045): `advance_convoys` progress
  and arrival (R1), `recipe_registry` logistics-cost accessors (R4), `dispatch_convoys`
  gate check + balance debit + source-pool debit (R4–R6), and `credit_arrived_convoys`
  pool + market-supply injection (R7). Links `world.cpp`, `supply_system.cpp`.
- **`construction_harness`** — Layer 4 player construction (`construct_building`):
  `placement_rules::can_place` validation, build-cost spend from the corp balance,
  building/stockpile authoring and asset attachment, default-recipe seeding, and the
  insufficient-funds / unknown-corp / unknown-tile guards. Links `world.cpp`,
  `construction.cpp`, `placement_rules.cpp` (hand-builds a `recipe_registry`).
- **`survey_harness`** — Survey system (BL-067): cost/duration formulas vs size×distance
  (R2), deterministic raster region partition + reveal order (R3), home starts surveyed
  (R4), concurrent surveys advance independently (R5), dispatch guards + upfront debit
  (R6). Links `world.cpp`, `survey_system.cpp`. CMake target `survey_harness`.
- **`visibility_harness`** — Visibility model (BL-068): the read-side ownership accessors
  — `owner_corp_of` resolves a building to its owning corporation (null when unowned) and
  `is_player_owned` is the single uniform rival branch point. Links `world.cpp` only.
  CMake target `visibility_harness`.
- **`workforce_harness`** — Population legibility (BL-069): regression guard asserting
  `workforce_efficiency` (world/workforce.hpp) reproduces the prior inline economy_system
  habitability→workforce curve bit-identically across [0,1], plus the named cliff/floor/
  ceiling anchors. Header-only (no link sources). CMake target `workforce_harness`.
- **`trade_routes_harness`** — Persistent trade routes (BL-088): a completed convoy
  upserts exactly one route with the correct unordered body-pair + `last_tick`; a repeat
  lane bumps `convoy_count` without duplicating; an intra-body convoy records nothing;
  `body_of_market` resolves. Links `world.cpp`, `supply_system.cpp`. CMake target
  `trade_routes_harness`.
- **`commercial_fog_harness`** — Commercial-sphere activity fog (BL-089):
  `body_activity_visibility` returns the right tier for each of {no route, fresh route,
  stale route, active lane, presence}; `home_body` starts visible; activity is independent
  of survey phase (surveyed-but-unrouted stays `unknown`; unsurveyed-but-routed is
  `known`). Links `world.cpp`, `supply_system.cpp`. CMake target `commercial_fog_harness`.
- **`determinism_harness`** — Determinism guard (BL-106): generates `make_hard_coded_world()`
  **twice** and asserts 23 field-identity checks (well-known entities + asteroid belt, every
  component-store size + sorted entity-id key set, the `tile_to_nation` and
  `population_centre_tile` mappings, and the `corp_body_pools` keys). Catches any clock/rand leak
  or unordered_map iteration-order dependence in world generation — the standing determinism
  invariant. Links the generation TU superset (as `world_audit`). CMake target
  `determinism_harness`.
- **`chemistry_tables_harness`** — Molecular vocabulary (BL-209): the species/reaction
  dictionary the seven-gate abiogenesis chain is written against. `molecular_event` is
  exactly 8 bytes (R12a — it is a save-format record, so a silent size change is a
  compatibility break); **no orphan ids** (R12b — every process names a gate and resolves
  every species it references; this is the key invariant, since ids and display names are
  decoupled by design and nothing else catches table drift); names never ids (R12c); the
  RNA half-life curve is monotone, clamped and table-driven (R12d — PLANETOLOGY.md bans
  exp/log/pow in a gate path); and the S5e survival floor cuts through the Lost City band,
  45–90 °C (R12e). Links `chemistry_tables.cpp` only. CMake target
  `chemistry_tables_harness`.
- **`population_demand_harness`** — BL-190 population-demand ordering fix (2026-07-31):
  `inject_population_demand` routes each centre's agricultural_produce demand (1 × scale)
  to its catchment market (R1); the demand survives `clear_markets`' per-tick reset into
  the cleared state price resolution reads (R2); stale demand written before clearing is
  erased while the injection still lands (R3 — the ordering contract that was previously
  broken: the econ-step stub was zeroed the same tick, never priced). Links the SDL/Lua-free
  world superset (glob minus `recipe_registry`/`tech_tree`). CMake target
  `population_demand_harness`.
- **`ai_skill_harness`** — AI skill-regression instrument (BL-204,
  docs/ai/AI_OPPONENT.md § 3): freezes a 5-seed benchmark set (`world_params.seed`
  0-4, spanning the generator's body/terrain/market diversity), runs 300 ticks of
  the real bot-vs-bot economy loop per seed (BL-202's strategic tier already
  commands every non-player corp), and asserts four metrics per seed against
  disposable golden bands — net-worth curve (final + minimum), solvency (ticks
  any AI corp balance sampled below zero), survival (fraction of AI corps still
  fielding an active building), and action counts by `corp_verb` (the thrash
  detector). Also proves `world::state_hash` (the BL-204 tick-boundary FNV-1a
  checksum) is identical across two same-seed runs and differs across two
  different seeds — the harness's own determinism primitive, and the future
  multiplayer lockstep desync detector's first exercise. Hand-builds a
  `recipe_registry` (mirrors `scripts/economy.lua` / `recipes.lua`); links the
  generation TU superset (as `world_audit`/`corp_terrain_matrix`) plus
  `corp_ai.cpp`/`corp_command.cpp`/`construction.cpp`/`survey_system.cpp`/
  `supply_system.cpp`/`building_profit.cpp`. CMake target `ai_skill_harness`
  (picked up by the generic glob below — no CMakeLists entry needed).
- **`continents_harness`** — Continents/Drift (BL-210 first slice): the plate-drift
  sibling pass. Determinism (R1 — same seed identical, different seed different);
  mobile-lid plate count lands in [4,10] (R2); the stagnant-lid special case is one
  immobile plate with zero height bias everywhere (R3); convergent AND divergent
  boundaries both fire across a seed spread, so the dot-product classifier isn't
  sign-biased (R4); every emitted history line names its consequence, per
  PLANETOLOGY.md's presentation rule (R5). Links `planetology.cpp` (reads
  `mobile_lid`/`theta`) + `continents.cpp`. CMake target `continents_harness`.

- **`era_world_harness`** — Era −1 antiquity start (BL-271 first slice): generates the
  canonical world with `world_params::epoch_year = 0` and asserts the stop holds (every
  province founded by year 0, no furnace lit, median industrial year 0 — R1), demography is
  seeded within `(0, carrying_capacity]` with manpower under its ceiling while the default
  1960 world stays unseeded (R2), the 0 CE world is multipolar (R3), two epoch-0 generations
  produce byte-identical province tables (R4), and the default 1960 arc is untouched —
  still industrialises, still ruptures, at least as many provinces (R5). Also **prints the
  0 CE dossier** (provinces by population, nations by tiles) — the instrument for eyeballing
  an antiquity world. Links the generation TU superset (as `world_audit`); CMake target
  `era_world_harness` via the generic glob.
- **`order_book_harness`** — The order book in world state (BL-293): the item-spanning check
  that a corp places, holds and removes a standing sell order entirely through
  `apply_corp_command` with no UI (R0), that the economy tick sells against the book with
  nobody passing it in (R1), that `write_order_book`/`read_order_book` round-trip the book
  **in its stored order** and refuse a wrong-magic / wrong-version / truncated stream without
  writing anything partial (R2), that every rejection reason of the three new verbs is
  distinguishable and mutates nothing (R3), that `state_hash` is identical across two same-seed
  runs *with order traffic* and MOVES when an order changes — including when the same orders
  sit in a different sequence, since price-time priority makes the book's order state (R4), and
  that the rival-corp scorer reaches the trade verb conservatively: never below the rarity
  floor, never under the hold threshold, never a duplicate, never on the player's corp (R5).
  Links the world superset; CMake target `order_book_harness` via the generic glob. 43
  assertions, ~instant.

## Running the whole suite (CTest — BL-104)

As of BL-104 every `tools/verify/*.cpp` is a registered CTest test, so the whole logic tier runs
with one command instead of per-harness `cl` lines:

```
ctest --test-dir build --output-on-failure          # all harnesses
ctest --test-dir build -R determinism_harness        # one, by name
```

`check.bat` wraps `cmake --build build` + `ctest`. A single harness still builds standalone with
`cmake --build build --target <name>` (no SDL/Lua needed). Use the per-harness `cl` recipe below
only when building outside the CMake tree.

## Procedure

1. **Compile** from the repo root, after sourcing the VS `vcvars64`
   (`C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`
   — verified 2026-07-28; the old 2022 BuildTools path in this file was stale. From Git Bash
   the quoting fails, so write a one-off `.bat` that `call`s vcvars then `cl`, and run that),
   with the harness's own source list (see `tools/verify/README.md`), e.g.:
   ```
   cl /nologo /std:c++20 /EHsc /I src tools\verify\econ_harness.cpp ^
      src\world\world.cpp src\world\economy_system.cpp ^
      src\world\market_clearing.cpp src\world\budget_system.cpp ^
      /Fo:build_gen\verify\econ_harness\ /Fe:build_gen\verify\econ_harness.exe
   ```
   Keep harnesses **outside `src/`** — CMake `GLOB_RECURSE`s `src/*.cpp` into the
   real build.

   **Build output goes under `build_gen\verify\` — never `%TEMP%`, never the repo root.**
   Three rules, all load-bearing:
   - Always pass **both** `/Fe:` (exe) and `/Fo:` (objects, trailing `\` required).
     `cl` defaults both to the *current directory*, so omitting them scatters
     `.obj` files across the tree.
   - Use the harness's **full name** from **Available harnesses** above. No
     abbreviations — a stray `hlh.exe` or `ct.exe` is unidentifiable weeks later
     and reads as malware to a virus scanner.
   - `%TEMP%` is banned as an output target. It is user-writable staging that AV
     tools watch closely, so an unsigned exe there is exactly the shape of a
     dropper — and excluding `%TEMP%` from a scanner to quieten that would blind
     it to real threats. `build_gen/` gives Norton et al. one narrow, stable
     exclusion path instead.

   Nothing here dirties `git status`: `.gitignore` already covers `*.exe`, `*.obj`,
   `*.pdb`, `build/` and `.claude/worktrees/`. Keeping artifacts in-tree costs
   nothing and buys a scannable, self-describing location.
2. **Run** the produced exe. From PowerShell a bare name fails (`9009`) when cwd
   isn't on PATH — invoke as `& ".\build_gen\verify\econ_harness.exe"` or with an
   absolute path. Each harness prints `PASS` / `FAIL` lines and exits non-zero on
   any failure.
3. **Report** the assertion results against the requirement being checked; cite the
   harness name and the failing lines if any.

## Notes

- Deterministic: the harnesses build a fixed world (hand-authored or
  `make_hard_coded_world`), so results are reproducible.
- Keep `recipe_registry.hpp` **pure data** (sol2 only in its `.cpp`) so economy
  logic stays harness-buildable; a harness hand-builds a `recipe_registry` rather
  than loading Lua. The Lua-loading + GUI path is covered by `verifier-visual`.
- To author a new check: add `tools/verify/<name>.cpp` with `check(...)`-style
  assertions, record its build line in `tools/verify/README.md`, name it under
  **Available harnesses** above, then run it through this skill — that is how a
  check becomes a permanent, reusable asset.
