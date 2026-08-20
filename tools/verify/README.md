# Headless verification harnesses

C++ harnesses that verify pure `world/*` logic without the SDL/Lua/ImGui stack
(see memory `reference_headless_build`). They live outside `src/` so the CMake
`GLOB_RECURSE` does not pull them into the real build.

These are the cases behind the **`verifier-headless`** skill
(`.claude/skills/verifier-headless/`) — run them through it rather than re-deriving
the build commands. Adding a `tools/verify/*.cpp` here and naming it in that skill
is how a new headless check becomes a permanent, reusable asset.

Compile and run from the repo root, after sourcing the VS BuildTools `vcvars64`.

**Prefer the CMake target where one exists.** The hand-written `cl` recipes below name their
translation units explicitly, and that list **drifts stale as `world/*` grows** — a harness that
link-fails with `LNK2019` usually needs a TU added here, not a code fix (grep `src/world/*.cpp`
for the unresolved symbol's definition). The CMake tree cannot drift, because it globs:

```bat
cmake --build build --target <harness_name>
.\build\<harness_name>.exe
```

`price_band_harness` (BL-442) is CMake-only for a second reason: it is a **live-Lua** harness
(it links `recipe_registry.cpp` + `lua_state.cpp` + `lua54` on top of `io_world_obj`, and is
hand-declared in `CMakeLists.txt`), because its P2 row asks what the shipped
`scripts/economy.lua` actually delivers. Run it from the repo root — P2 writes a probe script
to `build_gen/verify/`, and the Lua loads are relative paths.

`haulage_measure` (BL-442 step 2) is CMake-only and live-Lua for the same reason, and must also
run from the repo root. It is the harness that *derives* the price ceiling `price_band_harness`
guards the plumbing of: it measures the per-unit haulage between adjacent markets in the real
generated world and prints the `ceil_mult` that follows. Report-only apart from a vacuity guard —
re-run it and re-derive `economy.price_band.ceil_mult` whenever the logistics cost table, the map
scale, or the `base_price` table changes.

`agent_seam_harness` (BL-412) is CMake-only for a third reason: it links two **`src/core`** TUs
the world superset deliberately excludes — `agent_protocol.cpp` (the `--serve` line protocol,
shared with `run_serve`) and `agent_seam.cpp` (the rendered app's loopback listener + tick-boundary
drain) — plus `ws2_32` on Windows. Still SDL/Lua/ImGui-free; it proves the live agent control
seam's two contracts headless (socket schedule ≡ in-process schedule by `state_hash`, transcript
replays to the same hash; out-of-domain commands rejected whole with the hash untouched).

**`build_gen_harness.bat <name>`** (repo root) is the CMake-free route for any world-superset
harness: it derives its TU list by globbing `src\world\*.cpp` minus the four sol2/Lua TUs, exactly
as `io_world_obj` does, so unlike the hand-written `cl` recipes below it **cannot drift stale**.
Output lands in `build_gen\verify\<name>.exe` with the compiler log beside it. Use it when a
worktree has no configured `build\` tree and a full CMake configure (SDL + Lua FetchContent) is not
worth paying for one harness.

`road_reach_census` (Sprint B2) is the road-network reach instrument: it counts, over the 8-seed
census set, how many nations end generation with **no roaded tile in their territory**, splits that
count by whether the nation has a single population centre, and asserts the no-ocean-roads
invariant plus determinism (identical `road_level` field *and* `state_hash` across two generations
of one seed). Report-first — the road-less count is a number to watch, not a pinned band. Re-run it
after any change to `road_generation.cpp`, to nation/population placement, or to the logistics
traversal costs the road MST is laid out against.

Use that route for anything linking the world superset — `world_audit`, `ai_skill_harness`,
`history_ladder_harness`, **`settlement_harness`**, `data_creep_harness`, `corp_terrain_matrix`,
`trade_routes_harness`, `tech_effect_union_harness` (BL-479: the tech effect union — a fixture
modify_scalar tech moves the named scalar for the earning corp only, the stack-taper pre-pass
prices the modified rate, and the no-effect real world's `state_hash` marks stay bit-identical
to the pre-change build) — and for `font_glyph_harness`, which links ImGui and is hand-declared in
`CMakeLists.txt` above the glob. These have deliberately **no** `cl` recipe below: writing one
would be inventing a TU list with a short shelf life — which is exactly how `trade_routes_harness`
stopped linking when BL-170 landed rivers (fixed 2026-08-02 by deleting its hand-declaration, not
by patching its list). The recipes that remain are the small, stable-link harnesses where the
`cl` line is genuinely quicker than a CMake configure.

**Output goes to `build_gen\verify\` — never `%TEMP%`, never the repo root.** Every recipe below
passes both `/Fe:` (exe) and `/Fo:` (objects, trailing `\` required); `cl` defaults both to the
current directory, so dropping either scatters artifacts across the tree. Keep the harness's
**full name** in the output path — an abbreviated `hlh.exe` or `ct.exe` is unidentifiable later
and reads as malware to a virus scanner. `%TEMP%` is banned outright: unsigned exes in
user-writable staging are exactly the shape of a dropper, and excluding `%TEMP%` from a scanner
to quieten the warning would blind it to real threats. Nothing here dirties `git status` —
`.gitignore` covers `build_gen/` outright, plus `*.exe` / `*.obj` / `*.pdb` by extension.

```bat
:: Layer 3 economy logic: production -> market clearing -> budget, including
:: supply/demand price resolution and deposit-depletion taper/exhaustion.
cl /nologo /std:c++20 /EHsc /I src tools\verify\econ_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp ^
   /Fo:build_gen\verify\econ_harness\ /Fe:build_gen\verify\econ_harness.exe
.\build_gen\verify\econ_harness.exe

:: Multi-tick economy stability — runs the loop 100 ticks on a small fixed world
:: and asserts price-band, finiteness, monotonic reserve, and bounded balances.
cl /nologo /std:c++20 /EHsc /I src tools\verify\econ_stability.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp ^
   /Fo:build_gen\verify\econ_stability\ /Fe:build_gen\verify\econ_stability.exe
.\build_gen\verify\econ_stability.exe

:: Resource-chain roster (BL-340) — every one of the seven new goods (silicon,
:: refined_copper, ree_alloy, machinery, alloys, electronics,
:: spacecraft_components) is produced (a building's output_quantity > 0 at
:: least once) and priced off the 4x band ceiling, over an 80-tick rollout on
:: a hand-built three-extractor + seven-processor chain.
cl /nologo /std:c++20 /EHsc /I src tools\verify\resource_chain_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp ^
   /Fo:build_gen\verify\resource_chain_harness\ /Fe:build_gen\verify\resource_chain_harness.exe
.\build_gen\verify\resource_chain_harness.exe

:: Military capability points (BL-332) — a completed military_base credits its
:: owning corp's military_points and a completed research_institute credits
:: science, symmetric across player and non-player corps; neither accumulates
:: while under construction or decommissioned.
cl /nologo /std:c++20 /EHsc /I src tools\verify\military_capability_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp ^
   /Fo:build_gen\verify\military_capability_harness\ /Fe:build_gen\verify\military_capability_harness.exe
.\build_gen\verify\military_capability_harness.exe

:: Laws MVP (BL-343, reshaped by BL-480) — the law record, per-tick resolution
:: (evaluate_laws) and the enforcement seam in apply_budget: a repealed law is
:: inert, an enacted levy charges exactly rate x in-jurisdiction units on its
:: own `levies` line, no other flow moves, repeal is bit-identical reversal,
:: scoping/stacking/conditions work, processing output is never levied. Since
:: BL-480 the fixture owns a nation (laws carry an author; jurisdiction bounds
:: the levy) and the seed ships ENACTED by the player's home nation.
cl /nologo /std:c++20 /EHsc /I src tools\verify\law_harness.cpp ^
   src\world\world.cpp src\world\budget_system.cpp src\world\law.cpp ^
   src\world\condition_set.cpp src\world\unit_roster.cpp ^
   /Fo:build_gen\verify\law_harness\ /Fe:build_gen\verify\law_harness.exe
.\build_gen\verify\law_harness.exe

:: Law author + treasury transfer (BL-480, group "law-author-and-treasury") —
:: the levy is a TRANSFER: sum debited from in-jurisdiction corps == sum
:: credited to the enacting nation's treasury, bit-exact over a tick span (R1);
:: out-of-jurisdiction corps (foreign territory, unclaimed tiles) pay zero;
:: repeal returns the arithmetic bit-identical to the never-enacted baseline
:: (R2); a no-enactment world is bit-identical to a no-law world; author choice
:: is deterministic (player home nation, else largest territory/lowest id, else
:: no law) and an authorless record is inert. R3's live half (read-only ledger
:: line) is a UI check outside this harness.
cl /nologo /std:c++20 /EHsc /I src tools\verify\law_author_harness.cpp ^
   src\world\world.cpp src\world\budget_system.cpp src\world\law.cpp ^
   src\world\condition_set.cpp src\world\unit_roster.cpp ^
   /Fo:build_gen\verify\law_author_harness\ /Fe:build_gen\verify\law_author_harness.exe
.\build_gen\verify\law_author_harness.exe

:: Procurement/contract seam (BL-350) — one contract quoted, accepted, paced
:: and delivered end to end with the treasury debited in the split shape
:: (deposit at accept, remainder paced across lead time), plus one decline
:: observed for each of the four refusal conditions (capacity, input access,
:: embargo, reputation), plus cancellation forfeiting the deposit and moving
:: reputation down.
cl /nologo /std:c++20 /EHsc /I src tools\verify\procurement_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   src\world\condition_set.cpp src\world\survey_system.cpp src\world\logistics.cpp ^
   src\world\unit_roster.cpp ^
   /Fo:build_gen\verify\procurement_harness\ /Fe:build_gen\verify\procurement_harness.exe
.\build_gen\verify\procurement_harness.exe

:: BL-392 (procurement contracts destroy value) + the Sprint D4 import tariff.
:: Money and goods conservation across BOTH flows: every credit a contract
:: debits from the buyer arrives at the supplier (deposit, instalments and
:: freight alike), and every credit the tariff debits arrives in the enacting
:: nation's treasury. Also re-measures the round-trip economics the item was
:: filed on (the -0.14-credit 20-unit iron round trip), asserts the lead time
:: now tracks the SUPPLIER's throughput, and asserts that with no tariff law
:: enacted the world is bit-identical to a control carrying no law record.
::
:: NOTE ON WHAT IT DOES *NOT* CLAIM: the wider economy is not a closed system —
:: the market is a buyer of last resort and pays sellers with nobody's money —
:: so the conservation spans are run against the seam in isolation, or against a
:: law-free control world. See the harness's own header comment.
::
:: A batch wrapper lives at build_money_conservation.bat (repo root) for the
:: same reason run_harness.bat does.
cl /nologo /std:c++20 /EHsc /I src tools\verify\money_conservation.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   src\world\condition_set.cpp src\world\survey_system.cpp src\world\logistics.cpp ^
   src\world\unit_roster.cpp src\world\law.cpp src\world\building_profit.cpp ^
   src\world\corp_ai.cpp src\world\stance.cpp src\world\supply_system.cpp ^
   src\world\tech_gate.cpp src\world\river_generation.cpp ^
   /Fo:build_gen\verify\money_conservation\ /Fe:build_gen\verify\money_conservation.exe
.\build_gen\verify\money_conservation.exe

:: Corp stance (BL-448) — the directed hostility map + canonicalised friendship
:: map + pending-offer table, and the four corp_command verbs that mutate them
:: (declare_hostile / offer_friendship / accept_friendship / return_to_neutral).
:: Asserts directed hostility (not reciprocal), symmetric accepted friendship,
:: a pending offer never reading as a stance, declare_hostile atomically
:: dissolving an existing friendship, return_to_neutral's unilateral/
:: direction-scoped rules, rejections mutating nothing, and determinism across
:: both independent world construction and sequence replay. In-memory only —
:: this project has no save-format serialiser to round-trip through yet.
cl /nologo /std:c++20 /EHsc /I src tools\verify\stance_determinism.cpp ^
   src\world\world.cpp src\world\corp_command.cpp src\world\construction.cpp ^
   src\world\placement_rules.cpp src\world\condition_set.cpp src\world\survey_system.cpp ^
   src\world\logistics.cpp src\world\unit_roster.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp src\world\supply_system.cpp ^
   src\world\stance.cpp src\world\tech_gate.cpp src\world\river_generation.cpp ^
   src\world\building_profit.cpp src\world\corp_ai.cpp src\world\law.cpp ^
   src\world\orbital_system.cpp ^
   /Fo:build_gen\verify\stance_determinism\ /Fe:build_gen\verify\stance_determinism.exe
.\build_gen\verify\stance_determinism.exe

:: World audit — Kepler biome balance (S2) + extraction placement (S1) +
:: deposit-reserve seeding (resource_remaining = richness x reserve factor).
cl /nologo /std:c++20 /EHsc /I src tools\verify\world_audit.cpp ^
   src\world\world.cpp src\world\tile_generation.cpp src\world\nation_generation.cpp ^
   src\world\corporation_generation.cpp src\world\placement_rules.cpp ^
   src\world\population_generation.cpp src\world\hard_coded_world.cpp ^
   src\world\orbital_system.cpp src\world\city_names.cpp src\world\planetology.cpp ^
   src\world\road_generation.cpp src\world\logistics.cpp ^
   /Fo:build_gen\verify\world_audit\ /Fe:build_gen\verify\world_audit.exe
.\build_gen\verify\world_audit.exe

:: Layer 4 player construction — construct_building validation, build-cost spend,
:: component authoring, the insufficient-funds / unknown-corp/tile guards, and
:: (BL-166/BL-168) Hydroponics Bay / Fishing Wharf placement + recipe resolution.
:: construction.cpp pulls in market_clearing.cpp -> economy_system.cpp, which
:: pulls in the rest of the non-Lua world/* superset transitively (corp_ai,
:: corp_command, survey_system, ...) — this drifted stale once (this recipe
:: previously listed only world/construction/placement_rules and link-failed);
:: it is now the full world/*.cpp set minus the sol2/Lua-dependent TUs
:: (recipe_registry.cpp, tech_tree.cpp, world_gen_config.cpp).
cl /nologo /std:c++20 /EHsc /I src tools\verify\construction_harness.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\chemistry_tables.cpp ^
   src\world\city_names.cpp src\world\construction.cpp src\world\continents.cpp ^
   src\world\corp_ai.cpp src\world\corp_command.cpp src\world\corporation_generation.cpp ^
   src\world\creeds.cpp src\world\economy_system.cpp src\world\hard_coded_world.cpp ^
   src\world\history_ladder.cpp src\world\logistics.cpp src\world\market_clearing.cpp ^
   src\world\nation_generation.cpp src\world\orbital_system.cpp src\world\placement_rules.cpp ^
   src\world\planetology.cpp src\world\population_generation.cpp src\world\road_generation.cpp ^
   src\world\supply_system.cpp src\world\survey_system.cpp src\world\terrain_combat.cpp ^
   src\world\tile_generation.cpp src\world\world.cpp ^
   /Fo:build_gen\verify\construction_harness\ /Fe:build_gen\verify\construction_harness.exe
.\build_gen\verify\construction_harness.exe

:: Supply layer — advance_convoys (R1), logistics constants (R4), dispatch_convoys
:: gate check + balance debit + pool debit (R5, R6), credit_arrived_convoys pool +
:: market supply injection (R7, R8), econ-tick orbital purity (BL-354).
:: BL-039 / BL-038 / BL-045 / BL-354.
cl /nologo /std:c++20 /EHsc /I src tools\verify\supply_advance.cpp ^
   src\world\world.cpp src\world\supply_system.cpp ^
   src\world\orbital_system.cpp ^
   /Fo:build_gen\verify\supply_advance\ /Fe:build_gen\verify\supply_advance.exe
.\build_gen\verify\supply_advance.exe

:: Population MVP + workforce-pool step 2 — population centres on Kepler (R3),
:: agricultural demand from pop (R4), agglomeration workforce contention (R5 / BL-042 R1).
:: Also covers population-dynamic R2 (hab scalar) and R3 (growth level-up).
:: Note: recipe_registry.cpp is excluded — its Lua dependency is not needed here.
cl /nologo /std:c++20 /EHsc /I src tools\verify\population_mvp.cpp ^
   src\world\world.cpp src\world\tile_generation.cpp src\world\nation_generation.cpp ^
   src\world\corporation_generation.cpp src\world\placement_rules.cpp ^
   src\world\population_generation.cpp src\world\hard_coded_world.cpp ^
   src\world\orbital_system.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp ^
   /Fo:build_gen\verify\population_mvp\ /Fe:build_gen\verify\population_mvp.exe
.\build_gen\verify\population_mvp.exe

:: Population demand ordering (BL-190 fix, 2026-07-31) — inject_population_demand
:: unit routing (R1); the demand survives clear_markets' per-tick reset into the
:: cleared state price resolution reads (R2); stale pre-clearing demand is erased
:: while the injection lands (R3). Links the full SDL/Lua-free world superset
:: (every src\world\*.cpp except recipe_registry.cpp and tech_tree.cpp).
:: (List current as of 2026-07-31; if it drifts, mirror CMakeLists' IO_WORLD_SOURCES glob.)
cl /nologo /std:c++20 /EHsc /I src tools\verify\population_demand_harness.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\chemistry_tables.cpp ^
   src\world\city_names.cpp src\world\construction.cpp src\world\continents.cpp ^
   src\world\corp_ai.cpp src\world\corp_command.cpp src\world\corporation_generation.cpp ^
   src\world\economy_system.cpp src\world\hard_coded_world.cpp src\world\history_ladder.cpp ^
   src\world\logistics.cpp src\world\market_clearing.cpp src\world\nation_generation.cpp ^
   src\world\orbital_system.cpp src\world\placement_rules.cpp src\world\planetology.cpp ^
   src\world\population_generation.cpp src\world\road_generation.cpp src\world\supply_system.cpp ^
   src\world\survey_system.cpp src\world\terrain_combat.cpp src\world\tile_generation.cpp ^
   src\world\world.cpp ^
   /Fo:build_gen\verify\population_demand_harness\ /Fe:build_gen\verify\population_demand_harness.exe
.\build_gen\verify\population_demand_harness.exe
```

```bat
:: Survey system (BL-067) — cost/duration vs size+distance, deterministic raster
:: region partition + reveal order, home starts surveyed, concurrent surveys
:: advance independently, dispatch guards + upfront debit.
cl /nologo /std:c++20 /EHsc /I src tools\verify\survey_harness.cpp ^
   src\world\world.cpp src\world\survey_system.cpp ^
   /Fo:build_gen\verify\survey_harness\ /Fe:build_gen\verify\survey_harness.exe
.\build_gen\verify\survey_harness.exe
```

On Linux (the primary dev target), the same harness builds via CMake
(`cmake --build build --target survey_harness`) or directly:
`g++ -std=c++20 -I src tools/verify/survey_harness.cpp src/world/world.cpp src/world/survey_system.cpp -o build_gen/verify/survey_harness`.

```bat
:: Visibility model (BL-068) — read-side ownership accessors: owner_corp_of resolves
:: a building to its owning corporation (null when unowned); is_player_owned is the
:: single uniform rival branch point.
cl /nologo /std:c++20 /EHsc /I src tools\verify\visibility_harness.cpp ^
   src\world\world.cpp ^
   /Fo:build_gen\verify\visibility_harness\ /Fe:build_gen\verify\visibility_harness.exe
.\build_gen\verify\visibility_harness.exe
```

On Linux: `cmake --build build --target visibility_harness` or
`g++ -std=c++20 -I src tools/verify/visibility_harness.cpp src/world/world.cpp -o build_gen/verify/visibility_harness`.

```bat
:: Population legibility (BL-069) — regression guard: workforce_efficiency reproduces
:: the prior inline economy_system curve bit-identically across [0,1]. Header-only.
cl /nologo /std:c++20 /EHsc /I src tools\verify\workforce_harness.cpp ^
   /Fo:build_gen\verify\workforce_harness\ /Fe:build_gen\verify\workforce_harness.exe
.\build_gen\verify\workforce_harness.exe
```

On Linux: `cmake --build build --target workforce_harness` or
`g++ -std=c++20 -I src tools/verify/workforce_harness.cpp -o build_gen/verify/workforce_harness`.

```bat
:: Corp AI stage A (BL-202) — the corp-command seam (R1: seam-only mutation,
:: rejected commands mutate nothing, byte-identical decision logs), the scored
:: utility layer (R2: hysteresis, cooldown, action budget, solvency gate,
:: player never commanded), and the visibility-honest state export (R3).
:: Repo-root build_corp_ai.bat wraps this compile+run.
cl /nologo /std:c++20 /EHsc /I src tools\verify\corp_ai_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp src\world\market_clearing.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\corp_ai.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   src\world\survey_system.cpp ^
   /Fo:build_gen\verify\corp_ai_harness\ /Fe:build_gen\verify\corp_ai_harness.exe
.\build_gen\verify\corp_ai_harness.exe
```

```bat
:: AI skill-regression harness (BL-204) — freezes a 5-seed benchmark set
:: (world_params.seed 0-4) and runs 300 ticks of the real bot-vs-bot economy
:: loop per seed, asserting net-worth curve / solvency / survival / thrash
:: action-counts against disposable golden bands, plus world::state_hash
:: (the BL-204 tick-boundary FNV-1a checksum) identity across two same-seed
:: runs. Hand-builds a recipe_registry (mirrors scripts/economy.lua/recipes.lua);
:: links the generation TU superset (as world_audit/corp_terrain_matrix) plus
:: the corp-AI/economy TUs.
cl /nologo /std:c++20 /EHsc /I src tools\verify\ai_skill_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp src\world\market_clearing.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\corp_ai.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   src\world\survey_system.cpp src\world\supply_system.cpp src\world\logistics.cpp ^
   src\world\hard_coded_world.cpp src\world\tile_generation.cpp src\world\planetology.cpp ^
   src\world\continents.cpp src\world\nation_generation.cpp src\world\population_generation.cpp ^
   src\world\city_names.cpp src\world\corporation_generation.cpp src\world\road_generation.cpp ^
   src\world\orbital_system.cpp /Fe:ai_skill_harness.exe
.\ai_skill_harness.exe
```

```bat
:: Molecular vocabulary (BL-209) — the species/reaction dictionary the seven-gate
:: abiogenesis chain is written against. Asserts the 8-byte molecular_event record
:: shape, NO ORPHAN IDS (every process names a gate and resolves every species it
:: references), names-never-ids, the table-driven RNA half-life curve, and that the
:: S5e survival floor cuts through the Lost City band (45-90 C).
:: Links chemistry_tables.cpp only — no world.cpp, no generation TUs.
cl /nologo /std:c++20 /EHsc /I src tools\verify\chemistry_tables_harness.cpp ^
   src\world\chemistry_tables.cpp ^
   /Fo:build_gen\verify\chemistry_tables_harness\ ^
   /Fe:build_gen\verify\chemistry_tables_harness.exe
.\build_gen\verify\chemistry_tables_harness.exe
```

```bat
:: Continents/Drift (BL-210 first slice) — the plate-drift sibling pass. Asserts
:: determinism (R1), mobile-lid plate count lands in [4,10] (R2), the stagnant-lid
:: special case is one immobile plate with zero height bias (R3), convergent AND
:: divergent boundaries both fire across a seed spread so the classifier isn't
:: sign-biased (R4), and every emitted history line names its consequence (R5).
:: Links planetology.cpp (reads mobile_lid/theta) + continents.cpp.
cl /nologo /std:c++20 /EHsc /I src tools\verify\continents_harness.cpp ^
   src\world\planetology.cpp src\world\continents.cpp ^
   /Fo:build_gen\verify\continents_harness\ /Fe:build_gen\verify\continents_harness.exe
.\build_gen\verify\continents_harness.exe
```

```bat
:: Creeds (BL-235) - one pantheon per cradle-culture in its own generated tongue.
:: Asserts determinism of the full biography across two generations (C1); one
:: shrine per culture, pairwise-distinct chief-god names, every creed line
:: carries its consequence (C2); the creed DRIVES - a won tribal war lowers
:: fragmentation_q before nation seeding, peaceable creeds are a no-op, welding
:: floors at half, conquest cost can hold a frontier (C3); exactly one 1951
:: common-tongue globalisation line, after every shrine (C4). Calls
:: make_hard_coded_world, so it links the full SDL/Lua-free world superset
:: (every src\world\*.cpp except recipe_registry.cpp and tech_tree.cpp) -
:: mirror CMakeLists' IO_WORLD_SOURCES glob if this list drifts.
cl /nologo /std:c++20 /EHsc /I src tools\verify\creeds_harness.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\chemistry_tables.cpp ^
   src\world\city_names.cpp src\world\construction.cpp src\world\continents.cpp ^
   src\world\corp_ai.cpp src\world\corp_command.cpp src\world\corporation_generation.cpp ^
   src\world\creeds.cpp src\world\economy_system.cpp src\world\hard_coded_world.cpp ^
   src\world\history_ladder.cpp src\world\logistics.cpp src\world\market_clearing.cpp ^
   src\world\nation_generation.cpp src\world\orbital_system.cpp src\world\placement_rules.cpp ^
   src\world\planetology.cpp src\world\population_generation.cpp src\world\road_generation.cpp ^
   src\world\supply_system.cpp src\world\survey_system.cpp src\world\terrain_combat.cpp ^
   src\world\tile_generation.cpp src\world\world.cpp ^
   /Fo:build_gen\verify\creeds_harness\ /Fe:build_gen\verify\creeds_harness.exe
.\build_gen\verify\creeds_harness.exe
```

On Windows via CMake (no need to hand-list sources): `cmake --build build --target creeds_harness`
then `.\build\creeds_harness.exe` — it is picked up by the generic `tools/verify/*.cpp` glob batch
at the foot of `CMakeLists.txt`, the same path `continents_harness` and `population_demand_harness`
use, so no hand-declared target is needed.

**BL-202 TU ripple:** `run_economy_step` now calls the strategic tier
(`run_corp_strategic_step`), so ANY harness linking `economy_system.cpp` also
needs `corp_ai.cpp corp_command.cpp construction.cpp placement_rules.cpp
survey_system.cpp building_profit.cpp` — the older TU lists above predate this.

**BL-170 TU ripple (found while building BL-208's harness, 2026-08-02):**
`hard_coded_world.cpp` now includes `river_generation.hpp`, so ANY harness
linking `hard_coded_world.cpp` also needs `river_generation.cpp` — every
world-superset recipe above that predates the rivers pass is one file short of
linking clean (link error names an unresolved `run_rivers`-family symbol).
Prefer the CMake target where one exists (its glob cannot drift); this note is
for the hand-`cl` fallback only.

```bat
:: World history log (BL-208) — the append-only, tagged, serialised world log.
:: R1/R2: seed_genesis_history bridges PLANETOLOGY's per-body dated history +
:: checkpoints into world::history_log, tagged by body, over the REAL generated
:: world (not hand-fabricated entries). R3: decision/agency entries appear over
:: a short bot-vs-bot run; a trade_route entry fires once per lane, on FIRST
:: establishment only. R4/R5: write_history_log/read_history_log round-trip
:: field-identically and reject a wrong-magic/wrong-version/truncated stream.
:: R6: export_corp_blackboard / body_activity_visibility (neither touched by
:: this item) still behave. Links the full SDL/Lua-free world superset (every
:: src\world\*.cpp except recipe_registry.cpp/tech_tree.cpp/world_gen_config.cpp,
:: including history_log.cpp and river_generation.cpp) — mirror CMakeLists'
:: IO_WORLD_SOURCES glob if this list drifts.
cl /nologo /std:c++20 /EHsc /I src tools\verify\history_log_harness.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\chemistry_tables.cpp ^
   src\world\city_names.cpp src\world\construction.cpp src\world\continents.cpp ^
   src\world\corp_ai.cpp src\world\corp_command.cpp src\world\corporation_generation.cpp ^
   src\world\creeds.cpp src\world\economy_system.cpp src\world\hard_coded_world.cpp ^
   src\world\history_ladder.cpp src\world\history_log.cpp src\world\logistics.cpp ^
   src\world\market_clearing.cpp src\world\nation_generation.cpp src\world\orbital_system.cpp ^
   src\world\placement_rules.cpp src\world\planetology.cpp src\world\population_generation.cpp ^
   src\world\river_generation.cpp src\world\road_generation.cpp src\world\supply_system.cpp ^
   src\world\survey_system.cpp src\world\terrain_combat.cpp src\world\tile_generation.cpp ^
   src\world\world.cpp ^
   /Fo:build_gen\verify\history_log_harness\ /Fe:build_gen\verify\history_log_harness.exe
.\build_gen\verify\history_log_harness.exe
```

```bat
:: Era -1 antiquity start (BL-271 first slice) - world_params.epoch_year = 0 stops
:: generated history at 0 CE. Asserts the stop holds (R1: founded_year <= 0, no
:: furnace, median 0), demography seeded within (0, capacity] with manpower under
:: ceiling while the 1960 world stays unseeded (R2), multipolar at 0 CE (R3),
:: byte-identical region tables across two generations (R4), and the default
:: 1960 arc untouched (R5). Prints the 0 CE dossier: regions by population,
:: nations by tiles. Calls make_hard_coded_world, so it links the full
:: SDL/Lua-free world superset (mirror IO_WORLD_SOURCES, as history_log_harness).
:: Prefer the CMake target: cmake --build build --target era_world_harness
.\build\era_world_harness.exe
```

```bat
:: Order book in world state (BL-293) - the book moved out of ui_state, matching
:: moved into the economy tick, and three presses joined the corp-command seam.
:: R0/R1: a corp places, holds and removes a standing order entirely through
:: apply_corp_command, and the tick sells against it with nobody passing it in.
:: R2: write_order_book/read_order_book round-trip the book IN ITS STORED ORDER
:: (price-time priority makes the sequence state) and refuse a wrong-magic /
:: wrong-version / truncated stream without writing anything partial. R3: every
:: rejection reason of place_sell_order / remove_sell_order / set_workforce_auto
:: is distinguishable and mutates nothing, including the per-corp book cap.
:: R4: state_hash is identical across two same-seed runs WITH order traffic, and
:: moves when an order changes - including the same orders in a different
:: sequence. R5: the rival-corp scorer reaches the trade verb conservatively.
:: Links the SDL/Lua-free world superset (mirror IO_WORLD_SOURCES, as
:: history_log_harness). Prefer the CMake target:
::   cmake --build build --target order_book_harness
.\build\order_book_harness.exe
```

On Windows via CMake (no need to hand-list sources): `cmake --build build --target history_log_harness`
then `.\build\history_log_harness.exe` — picked up by the generic `tools/verify/*.cpp` glob batch
at the foot of `CMakeLists.txt`, the same path `creeds_harness`/`continents_harness` use.

### interbody_pull_harness (BL-404 / BL-406) — one of three that need a live Lua state

Measures what `inject_interbody_demand` actually reads, against the **real**
`scripts/economy.lua` + `recipes.lua` rather than a hand-built registry — the
question it asks is what the *authored* numbers do, so nothing may be authored
locally. Declared explicitly in `CMakeLists.txt` (adds back `recipe_registry.cpp`
+ `scripting/lua_state.cpp`, the sol2 include dir, and links `lua54`), the same
shape as `pregame_balance_harness` and `persona_counsel_harness`.

```bat
cmake --build build --target interbody_pull_harness
.\build\interbody_pull_harness.exe
```

**It must run with the repo root as its working directory** — the script paths are
relative. `IO_TEST_SCRIPT_ROOTED_HARNESSES` in `CMakeLists.txt` sets
`WORKING_DIRECTORY` for the three harnesses in that position, and this one also
hard-exits if it cannot open the scripts. Both guards exist because its first run
measured a default registry with no demand baskets and reported a clean result
about an economy that was not there.

Reports: R1 the shortfall subtraction is a no-op at the injector's read point;
R2 what a corrected netting would silence, per resource; R2a the anti-vacuity
guard plus the market-selection finding (the home body carries many markets and
the pull reads one of them). Deliberately asserts only the structural half —
which market gets picked is not stable across standard libraries, so the varying
numbers are printed rather than asserted.

Each exits non-zero on a failed assertion. The economy *panel* (the visual class)
is verified separately via `ProjectIo --verify scripts/verify/economy_panel.lua`
(the `verifier-visual` skill).

## spectator_determinism (BL-409)

Spectator mode's no-human-seat rule. Asserts that the strategic tier evaluates the
player corp under `corp_ai_params::spectating` and never without it (R1), that an
unspectated run is byte-identical to the pre-BL-409 build (R2, golden state_hash
`3CBAD1D44EE71EDE`), and that a spectated run is itself deterministic and diverges
from a played one (R3). Also asserts that admitting the player corp shifts no
rival's cadence slot, since the index is over the sorted corp set.

R1 is a controlled A/B rather than a literal assertion, deliberately: the generated
player corp on seed 0 is insolvent with no extractors (NR-231), so asserting a
four-verb-family bar on it would assert that corp is rich rather than that the guard
lifted. The harness seats the best-endowed corp instead — picked by a stable
property, not by outcome — so only the seat varies between the two halves.

Links the world superset. CMake target `spectator_determinism` via the generic glob;
no CMakeLists entry needed.

```
cmake --build build --target spectator_determinism    # from a vcvars shell
build_app.bat spectator_determinism                   # or via the pinned script
ctest --test-dir build -R spectator_determinism
```

## substrate_census (NEW-10, Sprint B1)

How civilised the generated world actually is, over N seeds (default 3), measured
**after** `generate_background_firms` and at the shipped 400-year prehistory. Reports
tiles by composition and landform; population centres and centres per nation;
corporations and their focus split; buildings by type and per nation; road tiles by
tier and how many nations have **no** network; markets and how many nations contain
none; and the share of land within N tiles of any building, settlement or road.

**A report, not a gate** — per BL-224's discipline, it measures across seeds and
asserts no per-world density threshold. Its structural rows (S1–S3) pass. A fourth
row, *every nation holds at least one population centre*, was written, measured and
**fails at 64%** (109 of 169 nations); it is deliberately non-gating and is the
acceptance test for BL-463 (settlement count is seed-invariant).

**Needs an explicit live-Lua target, and this is not a convenience.** The generic
glob target links `io_world_obj` only, which excludes the sol2 TUs, and
`generate_background_firms` sizes itself against `economy.lua`'s demand baskets —
all-zero on a default registry. Built on the generic target it places **zero firms**
and the census describes a world nobody plays (NR-338).

Registered as a **`sweep`** (skipped unless `IO_RUN_SWEEPS` is set): ~1.0 s/world at
`/O2` but ~62 s/world in Debug, so the three-seed default is ~186 s in a Debug tree —
inside 25% of the long tier, which is the flaky-by-luck margin BL-288 re-tiered the
suite to remove (NR-259).

```
cmake --build build --target substrate_census    # from a vcvars shell
build_app.bat substrate_census                   # or via the pinned script
IO_RUN_SWEEPS=1 ctest --test-dir build -R substrate_census
```
