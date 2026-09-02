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
- **`quarterly_return`** — BL-626, requirement group `quarterly-return` R1–R6: the
  balance sheet `apply_budget` files for every corporation every economy tick. Asserts
  the record is a **retain** and not a second computation — the seven flows are
  bit-identical to `corp_budget`'s, the filed `net` equals the loop's own delta
  EXACTLY (both on an isolated fixture and per-tick on a really generated world), the
  rolling 40-quarter retention drops the oldest first, `book_value` is the registry's
  flat `build_cost` (historical cost — deliberately NOT the press's charge, which adds
  a market-priced materials term), the returns round-trip the save (version named
  symbolically, never as a literal), and the pre-game warm start's 80 ticks produce
  byte-identical records across two runs. Build via `build_harness.js`.
- **`demand_census`** — BL-649, requirement group `demand-census` R1–R4: per resource and per era
  band, the total modelled demand and **which passes inject it**, with every one of MARKETS.md's
  eight channels either represented or explicitly reported ABSENT — an absent channel being the most
  valuable row in the report. It **reports and never asserts a magnitude**, deliberately: the moment
  it asserts a number people tune against the harness instead of reading it, so it fails only on an
  internal inconsistency. Output is byte-identical across runs so a before/after pair reads at a
  glance — run it either side of every Sprint 21 tuning pass and keep the deltas. Build via
  `build_lua_harness.bat` (it needs the real Lua baskets); `--band ancient|industrial|both`,
  `--seed`, `--ticks`, `--fast`.
- **`ownership_class`** — BL-631, requirement group `ownership-class` R1–R6: whether a
  corporation is `publicly_held` / `privately_held` / `closed`, derived from its home
  region's history rather than an authored table. Asserts the derivation is pure and
  draws no randomness (two generations of one seed classify identically), that a
  region-less corp falls back to national character, that the world-level
  reject-and-reroll takes its public floor without ever patching an individual corp,
  and that the field round-trips the save. R2 is a **measured** row: it REPORTS the
  class distribution across a seed sweep rather than asserting it against a number —
  which is how the antiquity finding of NR-659 surfaced. Build via `build_harness.js`.
- **`whole_firm_buyout`** — BL-628, requirement group `whole-firm-buyout` R1–R6: the
  `buy_corporation` verb, the one thing in the codebase that **erases an actor**.
  Asserts the price (`max(0, book_value + k_acquisition_multiple × trailing_net +
  balance)`) against an independent recomputation from the filed record, with the
  young-firm, loss-making (prices BELOW book, unclamped) and zero-floor cases each
  given their own row; that a private, closed, never-filed or player-owned target is
  refused with the world **byte-identical** either side of the command; that holdings,
  pools, balance and returns transfer while open orders are CANCELLED; and — R4, the
  row this harness exists for — that **nothing dangles**, checked by a generic scanner
  that re-walks every corp-referencing container in `world.hpp` rather than mirroring
  the implementation's own list, run on a fixture, on a warm-started generated world,
  and again after four further economy ticks. Row **X** is a *measurement, not an
  assertion of correctness*: it states in credits how far this verb undervalues a
  contract-earning firm, because a filed return cannot see post-`apply_budget` income
  (NR-655). Build via `build_harness.bat`.
- **`building_upkeep`** — BL-641's building pass: a building's upkeep is credits AND
  a goods vector, as a unit's already is (`docs/economy/FINANCE.md` § Upkeep is
  credits AND goods — for buildings too; the Industry channel of `MARKETS.md`
  § Demand channels). Asserts **relations, not magnitudes** — the authored rates are
  a first cut and are deliberately not pinned here. R1: the draw is per building
  type, taken from the owner's pool **on the building's own body**, in **ascending
  building id** — asserted by starving two buildings of one corp on one body of all
  but one draw's worth and checking the LOWER id is the one supplied, which is what
  makes the order load-bearing rather than cosmetic. R2: the shortfall rule is the
  SAME rule an out-of-supply unit takes — an unmet draw subtracts
  `supply_decay_permille` and, after 41 unmet ticks, the building still **exists**,
  is **not decommissioned**, is **not idled** and is still on its owner's books; a
  met draw recovers, ceilinged at 1000. R3: rates are per type and **era-banded** (an
  `any` basket applies in both arcs, a banded one only in its own), and a **zero
  entry is skipped exactly as an absent one** — down to creating no pool that did not
  already exist, which is the property the inertness rests on. R6: two independently
  built worlds run 12 contended ticks to identical supply factors and pools. Build
  via `build_harness.bat`. The two rows it deliberately does NOT carry: bit-identity
  at zero rates is a **byte-compare** of `econ_harness` / `econ_bankruptcy` /
  `econ_stability` across the change, and the Industry channel reading PRESENT is
  `demand_census`, which needs the real Lua.
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
- **`sea_port_gate`** — The sea-mode infrastructure gate (BL-602, SUPPLY.md § Infrastructure
  gates: "Port building at both endpoints"). Before this item `price_convoy_leg`
  (`supply_system.cpp`) picked sea mode off `path.crosses_ocean` alone, with no check that
  either endpoint held a Port — this harness is the gate's acceptance test. R0: both
  endpoints Port-equipped dispatches normally, in sea mode, at the sea unit cost over a
  forced water-crossing fixture (a full vertical ocean band, not a single row — a partial
  band leaves every other row a free land detour and `crosses_ocean` never fires, which is
  how the first draft of this fixture silently tested nothing). R1: missing a Port at
  EITHER endpoint (dest only, source only, neither) is refused through the existing
  `!leg.viable -> rejected_placement` path — the same one "no launchpad", "no reachable
  route" already use — with a full-world-fingerprint check that the rejection mutates
  nothing, matching `convoy_command.cpp`'s R1 discipline. R2: a decommissioned or
  still-under-construction Port does not count as active (mirrors `is_supply_anchor`'s
  built+active test). R3: two runs of the same scripted sequence agree byte-for-byte.
  Links `world.cpp`, `supply_system.cpp`, `corp_command.cpp` (drives the gate through the
  real `dispatch_convoy` corp_verb, not a direct call to `price_convoy_leg`).
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
- **`endemic_demand_harness`** — BL-647 endemic luxury demand (2026-09-01): the Endemic trade
  channel end to end. `inject_endemic_demand` pulls a wealth-scaled, nation-flavoured luxury
  basket (tobacco/spices/coffee/furs) into nation-anchored markets — wealth gates the pull
  exactly (E1: treasury + positive domiciled balances × `wealth_scale` × basket; a broke nation
  injects nothing; a debtor corp contributes zero); character asymmetry is real (E2: two nations
  crave measurably different baskets off the pure seeded preference hash); two same-fixture runs
  deposit `==`-identical floats (E3); all four luxuries pass `is_extractable` + `can_place` on a
  deposit tile while a bare tile still refuses (E4 — the BL-586 slice-2 gap closed); the shared
  tranche survives both era bands and a banded row masks (E5); on the generated world the demand
  survives `clear_markets` into priced state (E6). Every row mutation-proved red at authoring.
  Links the world superset. Build via `node tools/verify/build_harness.js endemic_demand_harness`.
- **`campaign_lapse`** — BL-723, the spectated-campaign measurement instrument (2026-09-01): one
  spectated campaign under one parameter set becomes per-tick CSVs
  (`build_gen/verify/lapse/<tag>/` — corps, markets, world, manifest) for the sweep battery
  BL-724…BL-729. Valued production is NR-774's GDP definition; parameter overrides take the
  `--reach` pattern (post-load, manifest-echoed). `--t0` is the validity battery — A/A
  byte-identity, differential knob proof, zero-observation-fails, wall-clock ceiling — every row
  mutation-proved red at authoring. Lua-linked class: build via
  `cmd //c tools\verify\build_lua_harness.bat campaign_lapse`. The visual half is
  `scripts/verify/campaign_lapse.lua` (capture-only, no goldens).
- **`firm_exit_harness`** — BL-743 firm exit (2026-09-01): the insolvency wind-up. F1/F1b the
  trigger fires and the estate liquidates (building demolished, unit disbanded, pool lands WHOLE
  in market inventory — the conservation law); F2 the player is exempt absolutely; F3 a short
  streak or one solvent quarter survives; F4 the re-walk (no store the dissolution table names
  still holds the erased id); F5 inert defaults touch nothing; F6 determinism. F1b and F2
  mutation-proved red at authoring. Fixture-only; build via
  `cmd //c tools\verify\build_lua_harness.bat firm_exit_harness`.
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
- **`stratum_gate_harness`** — Stratum placement gates (BL-615, POPULATION.md § Strata gate
  buildings): the `placement_gate` axis through `can_place_in_world`, driven by authored data on
  the building definition (per-type in `building_economics`, per-recipe radius for the heavy
  processor class), never a building-name switch. Rows: a university (min_centre_scale 4) refused
  on open land (`needs_centre`) and on a Town (`centre_too_small` — the two refusals distinct); a
  schooling building refused outside any centre and placed on the smallest; a heavy processor
  refused beyond its `centre_proximity_radius` (`far_from_centre`), placed within/at it, and the
  radius wrapping the east-west cylinder; the default (empty) gate gating nothing, so every
  pre-BL-615 call site is unchanged; `recipe_registry::placement_gate_for` resolving the
  per-named-building gate (recipe radius overrides the type's); `construct_building` enforcing
  the gate itself (the authoritative seam — no caller passes it by hand); and replay determinism.
  Links the world superset — `node tools/verify/build_harness.js stratum_gate_harness --run`.
- **`population_dynamics`** — Centre promotion/decline + migration (BL-616 / BL-617,
  POPULATION.md § Growth, decline and razing / § Migration). P1: a centre held above the next
  tier's rung (`k_population_for_scale`) with met supply and habitability promotes at exactly the
  sustained window, replay-identical across fresh runs. P2: failing conditions shed population
  and demote — scale floors at 1, population at 1, the centre is never destroyed passively, and
  the failing streak rides `growth_accumulator` negative. P3: the `raze_centre` corp_verb through
  `apply_corp_command` — rejected without the acting corp's unit on the centre's body
  (occupation precondition), applied with one, erasing all three centre stores; rejections
  mutate nothing. M1: heads flow from low- toward high-attractiveness centres deterministically.
  M2/M3: the nation-grain stance gate (friendly 1000 / neutral throttle / hostile closed, riding
  the existing stance tables keyed by nation ids) and qualified-head conservation — cross-border
  migrants carry `min(1, origin_q × selectivity)` qualification, debiting the origin nation's
  fraction and crediting the destination's, total conserved. Links the world superset —
  `node tools/verify/build_harness.js population_dynamics --run`.
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
- **`chain_depth`** — ⚠️ **CARRIES ONE INTENDED FAILURE since 2026-08-26 (BL-648). DO NOT "FIX" IT.**
  R1's exemption row fails while any resource's admission claim names a consumer that no real pass
  injects. Eleven goods are red today — tobacco, spices, coffee, furs, trade_goods_misc,
  spacecraft_components, ceramics, dressed_stone, tools, leather, rigging — and the way to turn the
  row green is to **build the demand channel that buys them** (Sprint 21, MARKETS.md § Demand
  channels), never to weaken the assertion or re-point an exemption at a pass that does not move
  the good. A green row here after a harness edit alone is the loophole this item closed, reopened.
  Everything else in the harness passes; treat a SECOND failure as a real regression.
  The growth-spine metric (BL-428): how far down the production graph a corp can
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
  `planetology::endemics` in `tile_generation.cpp`; since BL-647 the four endemic luxuries are
  ALSO in `k_extractable`, so extraction can target them and the older "not in `k_extractable`"
  reading of this row is stale) and wanted (a recipe consumes it, or a named
  actor does via an **explicit** exemption table, so an orphan cannot hide as an assumed terminal).
  **Shipped RED on eight resources when this row was authored, which was the check working** — five
  of them (grain, fodder, salt, transport_capacity, bullion) were orphaned in both directions and were
  exactly the BL-286 "behaviour unfiled" values BL-432 was filed over; three more
  (platinum_group_metals, regolith, machinery) were obtainable but unconsumed. See NR-257 for the
  content decision. **Green as of 2026-08-24** — the five were deleted (NR-257), the three given
  consumers, and every good BL-585's ancient-goods append and BL-586's roster slice added since has
  landed with both a producer and a consumer in the same change, per this row's own admission rule.

  **R2 — no dominant production method, between recipes a corp can actually choose between.** The
  grouping is the whole content of this row. `recipe_switch_harness`'s old R1 grouped by (primary
  output, era) and reported four dominated pairs (NR-243); measured against the axis `recipes.lua`
  already states in its own comments, three of those have **disjoint raws** — supply routes, where
  deposit access rather than price decides which you run — and the fourth differs by a placement
  precondition. All four were grouping artefacts, and no recipe magnitude was changed (NR-258). Every
  sibling pair is now bucketed as a supply route, an explicitly-exempted precondition pair, or a
  genuine interchangeable method, and **only the third is price-compared**; bucketing every pair is
  what stops a pair escaping by being unclassifiable, and the counts print on every run. The duplicate
  in `recipe_switch_harness` was **deleted**, not left as a second answer.

  **Current reading (2026-08-24, BL-587/589): 13 sibling pairs — 10 supply routes, 1 precondition, 2
  interchangeable methods, 0 dominated.** The two — `charcoal_from_kiln` "Coking Kiln" and
  `steel_bessemer` "Bessemer Converter" — are BL-587's first genuine alternates, each trading BL-430's
  chain-depth axis (cheaper by the reference snapshot, but gated behind a reagent that needs the
  shallow route's own chain to have already run once).

  **Two price vectors, not one, since BL-592.** A single fixed snapshot cannot see a method that is
  only better "depending on which market it builds to" — it would read as dominated under the one
  regime tested even though a corp on a different deposit mix would genuinely prefer it. `fuel_cheap`
  and `fuel_dear` bracket the axis BL-587's own methods trade on; a pair is only flagged **dominated
  if BOTH regimes agree** — "must win under at least one," restated as its negation. Neither of the
  two existing methods needed the second vector to pass (both already win on chain depth alone,
  independent of price), so this is forward guard-rail for BL-586's still-widening roster, not a
  currently-failing case it fixed.

  **Run it with the unmasked band.** Both R rows call `set_era(era_band::any)`. `industrial` is a
  band like any other and hides the entire ancient roster — measured mid-build, that misreported
  charcoal, iron_blooms, timber, clay and peat as orphans and hid two of the four sibling pairs.

  **G5 — the start gate: a RULED opening, exactly, and nothing stranded** (BL-589, 2026-08-24).
  A fresh ancient corp (reached depth 0, no tech earned) is walked against a table of the opening
  Ben actually ruled — not a narrower first-cut guess — and every recipe outside the one deliberate
  lock (`refined_copper`, gated by `E0-EC-03`) must match its ruled open/closed state exactly. Then
  proves the lock is not a permanent orphan: builds a corp meeting `E0-EC-03`'s own authored
  predicate (one processing facility, Cr 400+ surplus), calls `advance_tech_gates`, and confirms the
  gate resolves and `recipe_unlocked` flips. Four assertions, all currently green.

  **R3 — every named building's material basket is obtainable in its own band** (BL-590/BL-592,
  2026-08-24). A per-building material override (BL-590) could name a good only the industrial arc
  makes for an ancient building — unbuildable at 0 CE, and nothing else in this suite would catch
  it, since R1/R1b ask whether a good is obtainable SOMEWHERE, not in the SAME band as the specific
  building that costs it. Generic over the whole roster — every extraction target and every recipe
  `resource_build_cost_for` can be called on, not a hand-picked list — so a future override earns
  this check for free. **18 named buildings carry an override, 0 offenders** as of 2026-08-24.
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

- **`settlement_density`** — how many population centres a world places, and how they land per
  NATION (Sprint B3, 2026-08-21). Report-only on the tuning question; S1–S4 assert only structural
  properties (every seed produces land, the divisor places centres, every nation holds at least one —
  BL-463's acceptance test — and a re-run at the shipped divisor reproduces the generated world).

  It sweeps alternative divisors through `generate_population_centres`'s optional parameter, so an
  alternative can be measured without a recompile and without changing what production generates.
  **Read its per-nation column, not its world total**: the world-level centre count looks reasonable
  while 86.3% of nations hold fewer than two centres, which is exactly `road_generation.cpp`'s
  `n < 2` gate for a per-nation backbone. The world total is what hid the stale 180×84 clamp through
  both a map tripling and a map shrinking.

- **`interdiction_harness`** and **`corp_ai_harness`** were unregistered here until 2026-08-21
  despite both being live; `campaign_roster_band` still is. An unregistered harness is one nobody
  runs after an unrelated change.

- **`sea_leg_census`** — what `crosses_ocean` actually selects (Sprint B2, 2026-08-21). Report-only,
  over market-pair routes; takes an optional seed count (`sea_leg_census 16`), default 8. It exists
  because a mode flag nobody had counted turned out to be firing on **64.5% of routes** and
  mispricing them by **1.59×** — `logistics_path::crosses_ocean` is one bit over a whole route, so a
  single water tile bills every land tile at the sea rate (BL-522).

  Run it after ANY change to `logistics.cpp`, `supply_system.cpp` or the `land`/`sea` rates in
  `scripts/economy.lua`. **Its `kLandRate`/`kSeaRate` restate those Lua values as constants** — the
  harness is deliberately Lua-free — so if the Lua moves, these must move with it or the census
  silently measures the old economy. S4 asserts the census is identical across two generations of one
  seed; S0–S3 report and assert nothing about the finding.

- **`history_conquest_gap`** — WHY the Era −1 sim fights and never conquers (BL-384), measured on
  **the era generation actually runs** (BL-462, 2026-08-23). Takes an optional sweep width:
  `history_conquest_gap 32`. Instruments rather than re-asserts — `history_sim_harness`'s
  B384a/B384b already assert that a region changes hands by war — but it is **no longer
  report-only**: it now carries the acceptance test for BL-462.

  **Read this before trusting any number it prints.** Until 2026-08-23 this harness, like every
  other Era −1 check, took `history_sim_params`'s struct default: a 4000 BCE → 0 CE span on a
  six-band clock, null creeds, the bare world seed, no works, and the report's POST-sim settlement.
  Generation runs 400 years on one 4-year band with real creeds and a folded seed. **Six
  divergences, so every earlier finding described a sim the game does not run** — including "2 of 8
  worlds fight nothing" and its n=32 restatement. All six are closed by
  `world/era_minus_one.hpp`: the derivations live in one place that `make_hard_coded_world` calls,
  and everything a caller cannot re-derive is captured at generation's own call site into
  `era_minus_one_fixture`. The harness hands those values straight back and constructs nothing.

  **A1 is the row that makes the rest mean anything**: for every swept seed the re-run's
  battles/conquests/foundings must equal `generation_report::prehistory_*` from the same
  generation, bit for bit. Green at 32/32. Red would mean no other number applies. R1 (the fork is
  answered for every seed), R2 (the classification comes from counters, never from a battle set),
  R3 (the pinned 8-seed table — re-pinned 2026-08-23, old values kept in the source comment) and
  R4 (tracing is inert AND gated) sit under it.

  **THE ONE REMAINING GAP, printed as a banner on every run:** `scripts/works.lua` is Lua-authored
  and a headless harness cannot load it, so `build_work` is gated out and the run scores **four**
  verbs where the shipped game scores five. Both sides of A1 pass the same null registry, so A1
  stays exact — but any statement it makes about what beats Campaign is about a four-verb contest.
  It was deliberately not closed by transcribing works.lua into C++, which would put the roster in
  two places and rebuild BL-462's own failure mode one layer down.

  What it finds on the real fixture at n=32: **11 of 32 worlds fight nothing** (not 17 of 32);
  every one of those clears the campaign threshold and loses the argmax on every round, so the
  fork's durable answer survives; **Settle takes 83–100% of the rounds Campaign loses** on every
  world; and neither ceiling-vs-floor (32/32 overlap) nor score level separates a silent world from
  a fighting one. BL-384's three original mechanisms stay refuted: hub mismatch 0/2503, 92.7% of
  victories convert. Runs `2 × n` real 400-year sims plus `n` generations; ~28 s at n=8, ~7 min at
  n=32.

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

- **`nation_wiring`** — Does anybody CALL the nation spines? (Sprint N3 slice 1, 2026-08-23.) The
  item-spanning check that BL-537/BL-542/BL-538-line-5 are reachable from the tick, not just green
  in isolation: it drives economy → clearing → budget → `run_nation_step` → tech gates on the real
  generated world and asserts a cash-gated rival's survey claim is paid and the survey dispatched
  the same tick, with conservation (treasury falls by the dispatched earmarks; the corp balance
  returns to start) and replay determinism through the fold. **Its § REALISTIC block asserts nothing
  and is the finding**: at the scorer's ~1.3% exploration weight one tick's line share is far below
  the cheapest survey, and Ben's whole-or-nothing earmark rule means the loop does not close at
  realistic treasuries (NR-572) — so R1 proves the mechanism on a funded off-cadence claimant the
  scorer will not overwrite, rather than rigging the scorer's weight. Needs a reconfigure to be
  globbed in.

- **`cadence_schedule`** — Does every rival get its turn on the schedule the LIVE APP runs?
  (BL-568, 2026-08-23.) corp_ai's stagger was keyed on the DAY tick, which is 90n at every
  quarter boundary in the app, and 90n mod 4 is only ever 0 or 2 — so half the rival roster
  never evaluated in a played game while every harness (passing 1..N) rotated all four slots.
  The key is now `world::current_econ_tick`, set by every driver beside the day tick. This
  harness drives `run_economy_step` on the APP's pair (day 90n, econ n) and asserts on
  `economy_report::corps_evaluated`: every rival within `cadence_k` ticks (C2), and the record
  equals `corp_strategic_eval_due`'s set each tick (C3). **Mutation-checked** — on the old key
  C2/C3 go red, 4 of 7 rivals never due. **Any harness that sets `current_day_tick` before
  stepping must set `current_econ_tick` too**, or it measures a sim where only index 0 acts.

- **`mercenary_contract_harness`** — The mercenary-contract seam, accept through evaluate through
  pay-or-fail (BL-573, Sprint 16 Wave 4; extended by BL-574, Wave 5). 66 checks. BL-573's own
  R1-R6 prove the mechanism at all: `accept_offer` as an untrusted seam (every rejection mutates
  nothing), the double-commit/unit-lock rule, tick evaluation ("take" judges only at the deadline,
  "hold" fails the moment its predicate goes false), abandonment as a distinct lesser penalty than
  failure, `active_mercenary_contract_for` wired for real, and determinism by replay.

  **BL-574 added M1-M7**, the item's own "one observed instance of every contract terminal state"
  requirement. Three are already satisfied elsewhere and not duplicated: **M1** (an offer issued
  by a threatened nation) by `nation_scorer_harness.cpp`'s own R8a-R8e, **M5** (abandon) by this
  file's own R4, **M7** (a mid-contract save/load round trip) by `save_roundtrip.cpp`'s own P12.
  The rest are new rows in this file: **M2** proves the OTHER half of "accept conserves exactly"
  — the client nation's treasury is untouched by `accept_offer` itself; **M3** replays a "take"
  completion off a REAL, hand-built decisive battle (the `battle_engagement_harness.cpp` B15
  idiom) rather than a hand-set `province_holder`; **M4** extends the existing early-fail "hold"
  row with the money outcome and the reputation ORDERING CONTRACTS.md § Q2 demands (a hold
  contract's failure costs strictly more Trust than an otherwise-identical abandon); **M6**
  extends the existing determinism row with a `world::state_hash` comparison.

  **NOTE (NR-583):** the item's own requirement text describes M4's failure as having "the escrow
  returned" — a `mercenary_contract` carries no escrow field at all, and the code never returns
  one on failure (only a still-open OFFER's TTL expiry does, before acceptance, a different event
  R8e already covers). M4 asserts the actually-observed behaviour instead; flagged rather than
  silently reworded, since amending the requirement text is out of this item's scope.

  Live-Lua, same shape as `condition_set_harness`'s C9 row: needs both `recipe_registry` and
  `contract_template_registry`. Hand-declared in `CMakeLists.txt`, listed in
  `IO_TEST_SCRIPT_ROOTED_HARNESSES` — run with the repo root as the working directory.

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

- **`value_anchor`** — Does the content honour Ben's value anchor? (BL-543, Sprint N1, 2026-08-22;
  reworked 2026-08-23.) Ben's sentence — *"a unit's annual equipment costs about twice their annual
  salary"* — is the only thing in the project anchoring military value to money, and nothing checked
  it because nothing could: both halves of BL-454's upkeep vector ship at zero. So this is **the
  argument for what the rates should be, not a gate on what they are.** It authors no rate; it reads
  `scripts/economy.lua`'s `unit_upkeep`, `scripts/world_gen.lua`'s `base_price` and
  `scripts/recipes.lua`'s baskets out of the Lua *text* (the Lua TUs are the excluded set here) and
  states the identity they must satisfy, at **base prices** and at **authoring time** — both
  narrowings Ben's.

  **R3 IS A BRANCH, NOT AN ASSERTION, and that is the point of it.** The first cut hard-asserted the
  shipped rates were zero, so it would have gone RED on the one day it exists for. The adversarial
  pass authored the harness's own solved fixture into `economy.lua` and got exit 1 *with the anchor
  satisfied and all 19 rows in band.* It now asks which state the content is in and asserts the
  invariant belonging to that state — inert: nothing is drawn at any scale; authored: R1's band is a
  claim about shipped content. Verified in both directions.

  **It is scale-invariant and says so (R6).** Multiply all 33 base prices by ten and every row stays
  green — because the anchor is a *ratio*. That was a silent false green; it is now an asserted
  property, paired with the row that does bind an absolute price: ordnance's markup over its own
  recipe basket, against the five Advanced-Fabrication peers `recipes.lua` id 27 says it was derived
  from (1.415–1.443; ordnance 1.433). Tripling steel alone drops it out of the band.

  **Run from the repo root** — it refuses to run without the authored Lua rather than measure a
  default. Links `combat.cpp`, `law.cpp`, `unit_roster.cpp`, `world.cpp`. 22 assertions.

- **`nation_budget_harness`** — The national budget's spend side (BL-537, Sprint N1, 2026-08-22;
  fixed and re-verified 2026-08-23). Exercises `run_national_budget` against the three rules in
  `nation_budget.hpp`: every credit out is a direct transfer to a named corporation, nations save,
  and a nation is never overdrawn.

  **RUN IT UNDER AddressSanitizer AS WELL AS PLAIN.** R4f is a *memory* row — a claim naming a line
  outside `budget_priority` (whose underlying type is `uint8_t`, so 0..255 are valid values while
  only 0..8 index the weight vector) was an out-of-bounds write, and an out-of-bounds write is not
  something a value assertion can see. The unfixed pass scores 34/34 on a plain build and aborts at
  `nation_budget.cpp:62` the moment ASan is on. Build it with
  `-fsanitize=address,undefined` and treat the clean run as part of the pass.

  Two of its rows are **generated rather than authored** (R4g/R4h): 512 seeded shapes, varying
  nation and corp counts, treasuries, reserves, weight sets and claim volumes. Measured against the
  pre-fix pass they catch 156 overdraws and 26 solvent treasuries driven negative — none of which
  the authored fixtures could reach, because the bound only fails once enough claims land on enough
  lines. The generator is a fixed-order xorshift: pure, replayable, part of the check and not of the
  simulation.

  Its dyadic reference fixture proves the *arithmetic*; `make_awkward_fixture` (non-dyadic weights,
  awkward reserve, off-grid prices) proves the arithmetic is not what was making the reference
  fixture pass. Links `world.cpp`, `nation_budget.cpp`. 36 assertions.

- **`sentiment_harness`** — The sentiment substrate (BL-545, Sprint N1, 2026-08-22; one row rebuilt
  2026-08-23). Checks `sentiment.{hpp,cpp}` against the stance invariant (*sentiment informs a
  declaration and may never make one*), directedness, decay with no floor, two independent
  dimensions, inertness at authored zero, determinism and the flat-binary round trip.

  R1's strongest form is **structural and cannot be printed**: `sentiment.cpp` is never handed a
  `world&`, so it has no reach to a stance table at all. The harness asserts it anyway — a
  structural argument nobody checks is one refactor away from being false.

  **The order-independence rows run on `dense_stream`, not `wide_stream`, and that distinction is
  the whole row.** `wide_stream` emits exactly one event per ordered pair, so nothing accumulates,
  so no order can matter: the adversarial pass deleted `std::stable_sort` from `sentiment.cpp:159`
  and all 39 rows still passed. `dense_stream` puts five events on every key and drives the pair to
  the saturation limit, which makes the fold order-*sensitive* by two whole points rather than by a
  low bit. Verified by mutation: without the sort, R6d–R6f fail. Links `world.cpp`, `sentiment.cpp`,
  `stance.cpp`. 41 assertions.

- **`nation_scorer_harness`** — What a nation CARES ABOUT (BL-542, the requirement group
  `nation-scorer`). `run_national_budget` consumes a weight vector and nothing authored one;
  `src/world/nation_ai.{hpp,cpp}` authors it, and this is the check on the three terms — **niche
  fit**, **conflict avoidance**, and **grudges that bias the first two and target nothing**.
  Because it is the first use anyone has made of the 2026-08-18 nation grant, the rows that
  matter most are the grant's own terms: R1 asserts the sweep is pure and produces bit-identical
  weights across 11 distinct `w.units` walk orders and a reordered `w.nations`; R2 asserts the
  cadence is an index over the **sorted** nation set, so a nation admitted with the highest id
  shifts nobody's slot; R6 asserts the **terrestrial horizon** — a nation sharing no border is
  outside it, each row paired with a control proving the same mutation on a real neighbour DOES
  move the subject.

  **Read R7 before trusting a green run.** It is the anti-vacuity block, and it earned its place
  twice while this harness was being written: the border-force table was six units, whose sums
  agree across almost every ordering, so the unit sort could be deleted with the suite still at
  100%; and the scale-invariance row used an abundance mass that drove the mutant straight onto
  its clamp, so it passed while the property was broken. R7 now asserts the fixture really
  constrains the code — the terms sit strictly inside (0, 1), two nations differ, and the float
  sum R1 guards is order-sensitive under two reorderings. **Every row was confirmed by mutation**
  (14 of them: the sorts deleted, the share normalisation removed, the `lack` sign flipped, the
  hostility read dropped, the deterrence divisor dropped, the grudge made additive and then given
  a line of its own, the horizon widened to every nation, purity broken with a `const_cast`).

  Nothing in the shipped tick calls the scorer, so this harness is the only caller and a world
  runs bit-identical without it. **Run it under ASan/UBSan as well as plain** — the raster walk
  and the six-side neighbour probe index raw vectors, and a wrap or bounds slip there is
  invisible to a value assertion:
  `g++ -std=c++20 -O0 -g -fsanitize=address,undefined -Isrc tools/verify/nation_scorer_harness.cpp ...`.

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

0. **No configured build tree? Use the one-line builder** (Ben, 2026-08-23, ruling on NR-392):

   ```
   node tools/verify/build_harness.js <name> [--run] [--debug]
   ```

   This is the path for a **worktree agent**, a **fresh clone**, or any session whose network
   policy refuses FetchContent — `cmake -B build` pulls SDL3, Lua, sol2 and ImGui from
   codeload.github.com, which some sessions get a 403 for, and none of it is needed for a headless
   check. It replaces two ad-hoc recipes that said the same thing in two dialects: the
   agent-authored `build_gen_harness.bat`, and the Linux lib-then-link recipe.

   It globs **every `src/world/*.cpp` minus the four sol2/Lua TUs** — the same set CMake's
   `io_world_obj` uses — so the source list cannot rot the way a hand-picked one does. It picks
   `cl` behind `vcvars64` on Windows and `g++` elsewhere, writes to `build_gen/verify/<name>`, and
   deletes any stale binary first so a failed compile can never be mistaken for a fresh pass.
   `--debug` adds ASan/UBSan, which is what to reach for when proving a row can fail.

   It **refuses by name** the two harnesses needing a live Lua state (`pregame_balance_harness`,
   `persona_counsel_harness`) — build those through CMake. That a headless build cannot reach the
   shipped Lua configuration at all is a standing gap, recorded as NR-558.

   Prefer `cmake --build build --target <name>` when a configured tree exists; this is the fallback,
   not the default.

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
