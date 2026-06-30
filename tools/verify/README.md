# Headless verification harnesses

C++ harnesses that verify pure `world/*` logic without the SDL/Lua/ImGui stack
(see memory `reference_headless_build`). They live outside `src/` so the CMake
`GLOB_RECURSE` does not pull them into the real build.

These are the cases behind the **`verifier-headless`** skill
(`.claude/skills/verifier-headless/`) — run them through it rather than re-deriving
the build commands. Adding a `tools/verify/*.cpp` here and naming it in that skill
is how a new headless check becomes a permanent, reusable asset.

Compile and run from the repo root, after sourcing the VS BuildTools `vcvars64`:

```bat
:: Layer 3 economy logic: production -> market clearing -> budget, including
:: supply/demand price resolution and deposit-depletion taper/exhaustion.
cl /nologo /std:c++20 /EHsc /I src tools\verify\econ_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp /Fe:econ_harness.exe
.\econ_harness.exe

:: Multi-tick economy stability — runs the loop 100 ticks on a small fixed world
:: and asserts price-band, finiteness, monotonic reserve, and bounded balances.
cl /nologo /std:c++20 /EHsc /I src tools\verify\econ_stability.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp /Fe:econ_stability.exe
.\econ_stability.exe

:: World audit — Kepler biome balance (S2) + extraction placement (S1) +
:: deposit-reserve seeding (resource_remaining = richness x reserve factor).
cl /nologo /std:c++20 /EHsc /I src tools\verify\world_audit.cpp ^
   src\world\world.cpp src\world\tile_generation.cpp src\world\nation_generation.cpp ^
   src\world\corporation_generation.cpp src\world\placement_rules.cpp ^
   src\world\population_generation.cpp src\world\hard_coded_world.cpp ^
   src\world\orbital_system.cpp /Fe:world_audit.exe
.\world_audit.exe

:: Layer 4 player construction — construct_building validation, build-cost spend,
:: component authoring, and the insufficient-funds / unknown-corp/tile guards.
cl /nologo /std:c++20 /EHsc /I src tools\verify\construction_harness.cpp ^
   src\world\world.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   /Fe:construction_harness.exe
.\construction_harness.exe

:: Supply layer — advance_convoys (R1), logistics constants (R4), dispatch_convoys
:: gate check + balance debit + pool debit (R5, R6), credit_arrived_convoys pool +
:: market supply injection (R7, R8). BL-039 / BL-038 / BL-045.
cl /nologo /std:c++20 /EHsc /I src tools\verify\supply_advance.cpp ^
   src\world\world.cpp src\world\supply_system.cpp /Fe:supply_advance.exe
.\supply_advance.exe

:: Population MVP + workforce-pool step 2 — population centres on Kepler (R3),
:: agricultural demand from pop (R4), agglomeration workforce contention (R5 / BL-042 R1).
:: Also covers population-dynamic R2 (hab scalar) and R3 (growth level-up).
:: Note: recipe_registry.cpp is excluded — its Lua dependency is not needed here.
cl /nologo /std:c++20 /EHsc /I src tools\verify\population_mvp.cpp ^
   src\world\world.cpp src\world\tile_generation.cpp src\world\nation_generation.cpp ^
   src\world\corporation_generation.cpp src\world\placement_rules.cpp ^
   src\world\population_generation.cpp src\world\hard_coded_world.cpp ^
   src\world\orbital_system.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp /Fe:population_mvp.exe
.\population_mvp.exe
```

```bat
:: Survey system (BL-067) — cost/duration vs size+distance, deterministic raster
:: region partition + reveal order, home starts surveyed, concurrent surveys
:: advance independently, dispatch guards + upfront debit.
cl /nologo /std:c++20 /EHsc /I src tools\verify\survey_harness.cpp ^
   src\world\world.cpp src\world\survey_system.cpp /Fe:survey_harness.exe
.\survey_harness.exe
```

On Linux (the primary dev target), the same harness builds via CMake
(`cmake --build build --target survey_harness`) or directly:
`g++ -std=c++20 -I src tools/verify/survey_harness.cpp src/world/world.cpp src/world/survey_system.cpp -o survey_harness`.

```bat
:: Visibility model (BL-068) — read-side ownership accessors: owner_corp_of resolves
:: a building to its owning corporation (null when unowned); is_player_owned is the
:: single uniform rival branch point.
cl /nologo /std:c++20 /EHsc /I src tools\verify\visibility_harness.cpp ^
   src\world\world.cpp /Fe:visibility_harness.exe
.\visibility_harness.exe
```

On Linux: `cmake --build build --target visibility_harness` or
`g++ -std=c++20 -I src tools/verify/visibility_harness.cpp src/world/world.cpp -o visibility_harness`.

```bat
:: Population legibility (BL-069) — regression guard: workforce_efficiency reproduces
:: the prior inline economy_system curve bit-identically across [0,1]. Header-only.
cl /nologo /std:c++20 /EHsc /I src tools\verify\workforce_harness.cpp /Fe:workforce_harness.exe
.\workforce_harness.exe
```

On Linux: `cmake --build build --target workforce_harness` or
`g++ -std=c++20 -I src tools/verify/workforce_harness.cpp -o workforce_harness`.

Each exits non-zero on a failed assertion. The economy *panel* (the visual class)
is verified separately via `ProjectIo --verify scripts/verify/economy_panel.lua`
(the `verifier-visual` skill).
