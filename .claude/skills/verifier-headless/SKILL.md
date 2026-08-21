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
- **`haulage_measure`** — what it COSTS to serve a neighbouring market, and whether
  anyone does (BL-442 step 2). The companion to `price_band_harness`: that one guards
  that the band is read from one place, this one says what its **ceiling should be**.
  Ben's requirement makes the ceiling derived — scarcity must clear `base + haulage`
  for a nearby market — so this measures the per-unit haulage `dispatch_convoys`
  actually debits (`logistics_cost(mode) × terrain-weighted A* cost` between market
  centre tiles) over the real generated world, reports its distribution against the
  authored `base_price` table, and prints the `ceil_mult > 1 + haul/base` that
  follows. **Report-only by design**: its one assertion is a vacuity guard that it
  measured a world with markets in it, because a pass/fail bar on a number the
  harness exists to *discover* would be a guess wearing a test's clothes. Its second
  section runs the real tick loop and counts convoys split into intra-body
  market-to-market / inter-body space lane / BL-088 persistent `trade_routes` — the
  check that a band change actually moved trade rather than only moving prices.
  **Re-run it whenever the logistics cost table, the map scale, or `base_price`
  changes**, and re-derive the ceiling in `scripts/economy.lua` from what it prints.
  Live-Lua, hand-declared in `CMakeLists.txt`, runs from the repo root.
- **`price_band_harness`** — the price band as data (BL-442 step 1). The band
  `[0.25×, 4×]` around `base_price` is read by **two** call sites — `resolve_price`
  (`market_clearing.cpp`, the real clearing clamp) and `wf_target_price`
  (`economy_system.cpp`, the BL-181 workforce solver's forward price estimate) —
  and used to be two hand-synchronised `constexpr` copies, the second commented
  "mirror market_clearing.cpp price band" with nothing enforcing the mirror. BL-442
  authors it once in `scripts/economy.lua` (`economy.price_band`), routed through
  `recipe_registry::price_band()`; this is the guard that the routing stays real.
  **Every behavioural row is differential** — it sets a non-default band and asserts
  the site *moves* — because a guard that exercised only one site would have passed
  before the change and would pass again the day someone reintroduces a local copy.
  Rows: P1 the defaults are bit-exactly 0.25/4.0 (so every hand-built-registry
  harness is unchanged); P2 the shipped Lua authors the band AND a probe script's
  distinctive override is read back (proving the loader does not silently fall
  through to the defaults); P3/P4 the clearing price honours a raised floor and a
  lowered ceiling; P5 the solver's chosen workforce target moves with the band —
  **the row that fails if `economy_system.cpp` reacquires a private copy**, verified
  by negative control; P6 the settled price tracks the registry ceiling
  monotonically across a four-rung sweep. Live-Lua (for P2 only) — links
  `recipe_registry.cpp` + `lua_state.cpp` + `lua54` on top of `io_world_obj`, and is
  hand-declared in `CMakeLists.txt`, so use the CMake target, not a `cl` recipe.
  Note P5's direction is the opposite of the naive guess and correctly so: a LOW
  ceiling flattens the price across the candidate range, making revenue linear and
  the optimum bang-bang (200); a HIGH ceiling restores the price response and an
  interior optimum appears (100).
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


- **`substrate_census`** — How civilised is the generated world? (NEW-10, Sprint B1, 2026-08-18.)
  Reports over N seeds (default 3), measured **after** `generate_background_firms` and at the
  shipped 400-year prehistory: tiles by composition and landform, population centres and centres
  per nation, corporations and their focus split, buildings by type and per nation, road tiles by
  tier plus how many nations have **no** network, markets plus how many nations contain none, and
  the share of land within N tiles of any building, settlement or road.

  **A report, not a gate** — per BL-224's discipline it measures across seeds and asserts no
  per-world density threshold. S1–S3 pass. A fourth row, *every nation holds at least one
  population centre*, was written, measured and **fails at 64%**; it is deliberately non-gating and
  is the acceptance test for BL-463 (settlement count is seed-invariant).

- **`battle_engagement_harness`** — The engagement trigger and the per-tick battle step (BL-467,
  Sprint C3, 2026-08-21). **45 checks** — 26 for the trigger and the step, plus B12b–B14f for the two surfaces (BL-468/BL-469, same day). **B1 is the row that could not have been written before the
  item**: it stands two hostile forces in one province and runs `run_economy_step` — the REAL tick
  entry point, never a resolver call — then asserts men died. B1c asserts the negative that makes
  it mean something: two NEUTRAL corps sharing a province do not fight, so the trigger is stance,
  not proximity.

  The rest guard the ways a trigger silently goes wrong: order-independent discovery from unordered
  containers (B2), one battle per corp pair per province under mutual hostility (B3), the seed
  folding the province with role order preserved (B4), EXACT loss conservation — men removed equals
  losses reported (B5), in-memory replay determinism including `state_hash` (B6), the terrain pair
  as the defender's argmax with ties by ascending tile id (B7), a rejected withdrawal leaving the
  world byte-identical (B8), and `march_unit` refused for a unit in contact (B9).

  **B12b–B14f cover the surfaces** (BL-468 dispatches, BL-469 card), and they exist because
  `src/core/battle_dispatch_text.cpp` was deliberately made its own translation unit so it COULD be
  linked headlessly — its natural home, `session_history.cpp`, drags in `<SDL3/SDL.h>`. Link it
  explicitly alongside the world objects:

  ```bash
  g++ -std=c++20 -O1 -Isrc -Itools/verify tools/verify/battle_engagement_harness.cpp \
      src/core/battle_dispatch_text.cpp build_gen/obj/*.o -o build_gen/verify/battle_engagement_harness
  ```

  B12b asserts the quoted withdrawal price equals the charged one, in all three terms — the card
  renders `quote_withdrawal()` rather than recomputing it, and this is the row that keeps that true.
  B13a–B13e reach every one of the six `battle_phase` values including both terminal ones. B14a–B14f
  cover the phrase banks: no bank empty, no `%`-token unsubstituted, a missing corp degrading to a
  noun rather than an empty string, replay-identity, and — the row that proves the seed fold is live
  rather than degenerate — wording actually varying across one fight (10 dispatches, 6 distinct
  lines).

- **`interdiction_harness`** — Can a supply line be cut? (BL-458, Sprint C3, 2026-08-21.) The
  item-spanning check for the mechanic that makes a convoy a military object: a hostile unit
  standing on the tile a convoy's head occupies takes the cargo. **R4 is the load-bearing row** —
  conservation: captured quantity in equals quantity credited, EXACTLY, and the destroyed fallback
  (an interceptor holding no pools) credits nothing and mints nothing. No path creates goods.

  R1 guards the defect the item predicted it would ship: `convoy_tile_at` must own the path
  ORIENTATION rule, because `intra_body_path` caches on a canonicalised key and the renderer
  conditionally reverses it. Get that wrong and the convoy's head lands at the wrong end of the
  lane half the time — invisible on screen (the beam looks right either way) and fatal to
  interdiction. R2 asserts stance is the predicate and the ONLY predicate (a neutral unit on the
  same tile does not intercept); R5 that two runs of one seed cut the same convoys on the same
  ticks, field for field; R6 that an unwarred world is untouched and that interdiction declares
  hostility on nobody's behalf.

  **R7 is the NR-407 row (added 2026-08-21, Lane C).** The mechanic worked from the day it landed
  but shipped SILENT: `credit_arrived_convoys` called `(void)intercept_convoys(...)` and dropped
  the records, while erasing the cut convoy in the same call — so the interception existed for one
  statement and then did not exist at all, and no comms line or canvas mark could read it. R7
  drives the real shared seam (`credit_arrived_convoys`, which app/main/every harness go through)
  with its `out_cuts` sink and asserts the record names interceptor, victim, tile, body, cargo and
  outcome; that a quiet tick reports nothing; that the sink APPENDS across ticks; and that the
  two-arg form every existing caller uses still cuts and still credits. An interception is an
  EVENT, not state — nothing is stored on `world`, serialised, or folded into `state_hash`.

- **`tile_axes_harness`** — The substrate/cover terrain split (BL-519, 2026-08-21). 13 checks over
  the two-axis replacement for `terrain_composition`: the density invariant (density is 0 **iff**
  cover is `none`), `cover_fraction` monotonicity, `is_biotic_cover` membership, and — the rows that
  make the 522-reference refactor falsifiable — that `terrain_combat`'s split defence/attrition
  reproduce all 11 pre-split compositions EXACTLY. Pair it with the 120-seed census when touching
  Pass 4: the split was held RNG-identical draw-for-draw, so any census movement is a real
  regression rather than expected drift.

- **`unit_upkeep_rates`** — What turning the upkeep rates off zero would actually cost (Sprint C3's
  rider, 2026-08-21). **A REPORT, and deliberately almost assertion-free.** BL-454 landed every
  rate at zero so pricing could be "tuned by playtest against a measured baseline rather than
  guessed"; this is that baseline. It sweeps five candidate rate sets and prints the wage bill, the
  ordnance draw, ticks-to-collapse unsupplied and ticks-to-recover — the last being what BL-458
  (supply lines cannot be cut) is really waiting on, since it decides whether cutting a lane is a
  threat or a gesture.

  Its four assertions are invariants, never tuning: the SHIPPED rates are still zero (it measures,
  it does not change), a zero rate still costs nothing, and the draw is linear in heads on both the
  credits and the goods half. **Picking a rate stays Ben's** — the harness prints numbers and says
  so in its own closing line.

  **Live-Lua, and this is the same trap `haulage_measure` documents above.** On the generic glob
  target it links `io_world_obj` only, no sol2 — so `economy.lua`'s demand baskets read all-zero,
  `generate_background_firms` places **zero firms**, and the census describes a world nobody plays.
  Declared explicitly in `CMakeLists.txt` with the script-rooted and sweep lists. That this is now
  the *second* harness to hit it is recorded as NR-338, with the open question of which other
  generic-target harnesses touch firms.

  Registered as a **`sweep`** (needs `IO_RUN_SWEEPS=1`): ~1.0 s/world at `/O2` but ~62 s/world in
  Debug, so the three-seed default is ~186 s in a Debug tree — inside 25% of the long tier, the
  flaky-by-luck margin BL-288 re-tiered the suite to remove. `substrate_census.exe [seeds]`.

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
