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
- **`resource_chain_harness`** — the processing-chain roster (BL-340): a hand-built
  three-extractor (silica/copper_ore/rare_earth_ore) + seven-processor chain run
  80 ticks, asserting every one of the seven new goods (silicon, refined_copper,
  ree_alloy, machinery, alloys, electronics, spacecraft_components) is produced
  at least once and never sits pegged at the `[0.25×, 4×]` band ceiling for the
  whole run. Links the same TUs as `econ_harness`.
- **`military_capability_harness`** — capability points (BL-332): a completed
  `military_base` credits its owning corp's `military_points` and a completed
  `research_institute` credits `science`, at the authored per-tick rate,
  symmetric across player and non-player corps; a building still under
  construction (`ticks_remaining > 0`) or decommissioned accumulates nothing.
  Links the same TUs as `econ_harness`.
- **`procurement_harness`** — the procurement/contract seam (BL-350): one contract
  quoted, accepted, paced and delivered end to end with the treasury debited in
  the split shape (a deposit at `accept_quote`, the remainder paced across
  `lead_time_ticks`), plus one decline observed for each of `request_quote`'s
  four refusal conditions (no capacity, no input access, embargo via
  `condition_set`, reputation floor), plus `cancel_contract` forfeiting the
  deposit and moving reputation down. Links `world.cpp`, `economy_system.cpp`,
  `market_clearing.cpp`, `budget_system.cpp`, `corp_command.cpp`,
  `construction.cpp`, `placement_rules.cpp`, `condition_set.cpp`,
  `survey_system.cpp`, `logistics.cpp`, `unit_roster.cpp`.
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
- **`chain_depth`** — The growth-spine metric (BL-428): how far down the production graph a corp can
  reach, computed off the recipe graph rather than authored. **Mixed fixture strategy, on purpose** —
  D1–D5 are hand-built graphs, because they ask whether the metric computes what it claims and must
  not drift with the economy; D6 loads the real scripts, because it asks what the *shipped* graph's
  depth actually is. Asserts raws are depth 0 and **max-within-a-recipe** (a depth-1 input mixed with
  a raw gives 2, not 3); **min-across-recipes** (adding a shallower alternate route *lowers* a good's
  depth — the rule BL-430's alternate methods will lean on, pinned before it is needed); that cycles
  and orphaned inputs read as **unreachable (-1)** rather than looping or fabricating a number, and
  that a route bottoming out in raws resolves them; order-independence; and against the shipped
  recipes, that every produced good has a well-defined depth and that BL-433's era masking never
  *adds* depth. Prints the real ceilings — **industrial 4, ancient 1** as of 2026-08-15, the latter
  being the measurement that says the ancient roster has no chain yet (BL-429).

  **G1-G4 cover the GATE** (BL-428 slice 2, 2026-08-16) — the half that makes depth the growth track
  rather than a readout: a recipe's required depth is its deepest input's (derived, never authored);
  a corp's reached depth is produced-once-ever and **monotonic** (the property the gate rests on,
  since a placement that was legal must not become illegal); **no ancient recipe is stranded** — the
  ladder is climbed from a fresh corp to a fixed point and anything still shut out is named, which is
  BL-432's "an unplaceable building is the roster's orphan" assertion; and the required-depth vector
  is identical across two loads and independent of insertion order. Prints the real ladder height —
  **ancient climbs to depth 3** as of 2026-08-16, so the gate has genuine rungs rather than passing
  vacuously on a flat roster.

  **R1-R2 are BL-432's roster invariants** (landed 2026-08-16), completing the item — its third
  assertion (every building's minimum depth reachable) is G3 above.

  **R1 — no orphan resources, EITHER direction.** Every `resource_type` must be obtainable (a recipe
  produces it, a deposit yields it, or it is an endemic good — the second deposit route, off
  `planetology::endemics` in `tile_generation.cpp`, which is *not* in `k_extractable` and whose
  omission makes all four endemics read as orphans) and wanted (a recipe consumes it, or a named
  actor does via an **explicit** exemption table, so an orphan cannot hide as an assumed terminal).
  **This row ships RED on eight resources and that is the check working** — five of them
  (grain, fodder, salt, transport_capacity, bullion) are orphaned in both directions and are exactly
  the BL-286 "behaviour unfiled" values BL-432 was filed over; three more (platinum_group_metals,
  regolith, machinery) are obtainable but unconsumed. See NR-257 for the content decision.

  **R2 — no dominant production method, between recipes a corp can actually choose between.** The
  grouping is the whole content of this row. `recipe_switch_harness`'s old R1 grouped by (primary
  output, era) and reported four dominated pairs (NR-243); measured against the axis `recipes.lua`
  already states in its own comments, three of those have **disjoint raws** — supply routes, where
  deposit access rather than price decides which you run — and the fourth differs by a placement
  precondition. All four were grouping artefacts, and no recipe magnitude was changed (NR-258). Every
  sibling pair is now bucketed as a supply route, an explicitly-exempted precondition pair, or a
  genuine interchangeable method, and **only the third is price-compared**; bucketing every pair is
  what stops a pair escaping by being unclassifiable, and the counts print on every run. Current
  reading: **4 sibling pairs — 3 supply routes, 1 precondition, 0 interchangeable methods**, so the
  dominance half currently guards an empty set and will only bite once BL-430's own alternates are
  authored. The duplicate in `recipe_switch_harness` was **deleted**, not left as a second answer.

  **Run it with the unmasked band.** Both R rows call `set_era(era_band::any)`. `industrial` is a
  band like any other and hides the entire ancient roster — measured mid-build, that misreported
  charcoal, iron_blooms, timber, clay and peat as orphans and hid two of the four sibling pairs.
- **`player_seed_sweep`** — Which seeds hand the PLAYER a corp worth playing? A live-Lua sweep (real
  `scripts/recipes.lua` + `economy.lua`, real economy ticks) that generates one world per seed and
  reports, per seed, the player corp's buildings by type, its opening and post-warm-start balance,
  and whether it ever dipped negative. `player_seed_sweep.exe [seed_count] [warm_ticks]`.

  **A REPORTING tool, not a gate** — it exits 0 unless generation actually threw (that being
  `seed_sweep_probe`'s job). It deliberately does not filter seeds, resample, or carry a whitelist:
  which of those to do is a design call, and a sweep that silently enforced one would be making that
  call by implication.

  Written 2026-08-16 after a live campaign handed the player a corp with no processing facility, so
  the Method page and the chain-depth ladder had nothing to work with. Its first run is the reason
  BL-435 (starting-corp selection) exists and is worth quoting as the shape of its output: over 24
  seeds, **11 playable, 13 pure-extraction**, and **solvency was not the discriminator at all** —
  every seed ended positive (1.3k–55k cr) and not one dipped. Reach for it whenever a change could
  plausibly move what the opening position looks like.

  **`--guard [seed_count]` is the one ASSERTING mode** (BL-435 task E, 2026-08-16), and the split
  from the reporting default is deliberate: the sweep answers an arguable design question, the
  guard asserts the four properties the starting-corp selection screen needs to hold in *every*
  world, exiting non-zero when one breaks. **G1** every seed offers a choice (≥ 2 specialists);
  **G2** the specialist/background split holds (≤ 12 specialists, none mis-flagged `is_background`);
  **G3** no specialist that cannot produce at all (neither extraction nor processing — Ben's
  dead-start call); **G4** every seed has ≥ 1 specialist with a processor and mean coverage stays
  above the 4.00/8 floor (pre-BL-435 was 2.96/8; measured 6.92/8 over 24 seeds on 2026-08-16).
  Registered as its own ctest, `player_seed_sweep_guard`, at 12 seeds. **G4 is depth, never
  wealth** — BL-436 measured a processing facility as currently earning *less* per tick than the
  extraction site it replaces, so this floor must never be re-read as a profitability gate.
  `player_seed_sweep.exe --guard 24`, or `--roster <seed>` for every corp's opening on one seed.
- **`era_roster`** — The era gate over the authored economy data (BL-433). The **fourth live-Lua
  harness**: it asks what the *authored* `era` tags do, so it loads the real `scripts/economy.lua`
  + `recipes.lua` rather than hand-building a registry — a hand-built fixture could only confirm
  the mask works on data the harness itself wrote, which is how a mis-tagged entry would slip
  through. Asserts **R1** the ancient band drops every `industrial` entry (no Launchpad, none of
  the petroleum/propellant/spacecraft chains) while the industrial band keeps all of them;
  **R2** — the load-bearing row — recipe ids are **absolute and stable across bands**, and a
  recipe filtered *out* of a band still resolves by its stored id, because the filter masks
  rather than removes (a compacting filter would silently repoint every building whose recipe sat
  after a filtered one); **R3** the default band is `any`, so every pre-BL-433 harness sees the
  full roster; **R4** a misspelled era throws at load naming the offending value, rather than
  falling back to `any` and quietly re-admitting a space-era building.

  Carries an **anti-vacuity row**: the ancient roster must be a *proper* subset — neither empty nor
  the whole file — so an all-`any` or all-`industrial` tagging cannot pass. Declared explicitly in
  `CMakeLists.txt` (adds back `recipe_registry.cpp` + `scripting/lua_state.cpp`, the sol2 include
  dir, links `lua54`) and listed in `IO_TEST_SCRIPT_ROOTED_HARNESSES`, since it resolves script
  paths relative to the repo root.
- **`interbody_pull_harness`** — The inter-body demand pull (BL-263) and whether its netting
  means anything (BL-404/BL-406). **One of the three harnesses that needs a live Lua state**
  (with `pregame_balance_harness` and `persona_counsel_harness`): the question it asks is what
  the *authored* economy numbers do, so it loads the real `scripts/economy.lua` + `recipes.lua`
  instead of hand-building a registry — a hand-built fixture is precisely how the defect it
  measures survived a green `market_emergence_harness`. Reports **R1** the shortfall
  subtraction (`home.demand − home.supply`) is a no-op at the injector's read point, because
  `clear_markets` zeroes supply before it and writes supply after it; **R2** what a corrected
  netting would silence, per resource; **R2a** the anti-vacuity guard, plus the finding it
  turned up — the home body carries many carved markets (BL-096) and the pull reads exactly
  one of them, holding ~12% of the body's demand. Deliberately asserts only the *structural*
  half and **prints** the varying half: which market the lowest-id pick lands on is not stable
  across standard libraries, so the same seed gives different supply figures under MSVC and
  g++, and no assertion should pretend either is correct. Declared explicitly in
  `CMakeLists.txt` (adds back `recipe_registry.cpp` + `scripting/lua_state.cpp`, the sol2
  include dir, links `lua54`).

  **Two guards worth copying to any harness that loads a script.** It resolves script paths
  relative to the repo root, so `IO_TEST_SCRIPT_ROOTED_HARNESSES` in `CMakeLists.txt` sets
  `WORKING_DIRECTORY` for it, and it **hard-exits if it cannot open the scripts**. Both exist
  because its own first run measured a default registry with no demand baskets and passed
  every assertion *vacuously* — a clean green result about an economy that was not there. A
  harness that loads data by relative path and does not check the load can only ever report
  that it found nothing wrong.

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

1. **Compile** from the repo root, after sourcing the VS `vcvars64`. From Git Bash the quoting
   fails, so write a one-off `.bat` that `call`s vcvars then builds, and invoke it by **absolute
   path** (`cmd //c "C:\Users\benbo\Project-Io\build_x.bat"` — a bare name is not found).

   **Use the BuildTools 2022 vcvars, pinned to 14.44** (corrected 2026-08-16):

   ```
   call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
   ```

   The VS18 Community path this file recommended between 2026-07-28 and 2026-08-16 **does not work
   against the configured `build/` tree**: it puts the 14.51 STL headers on `INCLUDE` while CMake
   still invokes the 14.44 `cl.exe`, and `yvals_core.h` hard-fails on the first translation unit
   with *"STL1001: Unexpected compiler version, expected MSVC Compiler 19.50 or newer"*. Pinning
   the toolset makes both halves agree. With the environment set, prefer
   `cmake --build build --target <name>` — every harness is already a declared target — and fall
   back to the raw `cl` line only when building outside the CMake tree,
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
  logic stays harness-buildable. **Most** harnesses hand-build a `recipe_registry`
  rather than loading Lua; five do not — `pregame_balance_harness`,
  `persona_counsel_harness`, `interbody_pull_harness`, `era_roster` and `chain_depth`
  link `lua54` and load the real scripts, and are declared explicitly in
  `CMakeLists.txt` above the glob.
  The Lua-loading + **GUI** path is covered by `verifier-visual`.
- **Hand-built or authored — choose by what the check is asking.** A hand-built
  fixture is right when the question is *does this formula compute what it says*:
  it isolates the arithmetic and cannot drift with the economy. It is wrong when
  the question is *what do the shipped numbers actually do*, because it can only
  ever confirm the formula you already wrote down. BL-404 is the worked example:
  `market_emergence_harness` hand-set home demand to 100 against supply 20, called
  the injector directly, and passed for months while the real sequencing made the
  same subtraction a no-op on every tick of every real game.
- **A harness that loads anything must fail loudly when the load fails.** Script
  paths are relative to the repo root; a harness run from the build dir loads
  nothing, measures a default registry, and passes *vacuously* — reporting a clean
  result about a world that was not there. Two guards, both in
  `interbody_pull_harness` and worth copying: `WORKING_DIRECTORY` via
  `IO_TEST_SCRIPT_ROOTED_HARNESSES` in `CMakeLists.txt`, and an explicit
  cannot-open-the-file hard exit. Vacuity is the failure mode this tier is least
  able to see on its own, so it has to be designed against rather than noticed.
- To author a new check: add `tools/verify/<name>.cpp` with `check(...)`-style
  assertions, record its build line in `tools/verify/README.md`, name it under
  **Available harnesses** above, then run it through this skill — that is how a
  check becomes a permanent, reusable asset.
