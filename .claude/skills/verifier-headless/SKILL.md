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
- **`world_audit`** — builds the hard-coded world and audits Kepler biome balance
  (forest + wetland fraction), extraction-asset placement, deposit-reserve seeding,
  the reusable `placement_rules::can_place` seam (placed assets pass it; ocean /
  zero-deposit tiles are rejected), and the **generated corp starting stockpile**
  (BL-115: every corp opens non-empty + prototype-scoped on its home body; extraction
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

1. **Compile** from the repo root, after sourcing the VS BuildTools `vcvars64`
   (`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat`),
   with the harness's own source list (see `tools/verify/README.md`), e.g.:
   ```
   cl /nologo /std:c++20 /EHsc /I src tools\verify\econ_harness.cpp ^
      src\world\world.cpp src\world\economy_system.cpp ^
      src\world\market_clearing.cpp src\world\budget_system.cpp /Fe:econ_harness.exe
   ```
   Keep harnesses **outside `src/`** — CMake `GLOB_RECURSE`s `src/*.cpp` into the
   real build.
2. **Run** the produced exe. From PowerShell a bare name fails (`9009`) when cwd
   isn't on PATH — invoke as `& ".\econ_harness.exe"` or with an absolute path.
   Each harness prints `PASS` / `FAIL` lines and exits non-zero on any failure.
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
