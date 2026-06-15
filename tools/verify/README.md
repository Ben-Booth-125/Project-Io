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
econ_harness.exe

:: World audit — Kepler biome balance (S2) + extraction placement (S1) +
:: deposit-reserve seeding (resource_remaining = richness x reserve factor).
cl /nologo /std:c++20 /EHsc /I src tools\verify\world_audit.cpp ^
   src\world\world.cpp src\world\tile_generation.cpp src\world\nation_generation.cpp ^
   src\world\corporation_generation.cpp src\world\hard_coded_world.cpp ^
   src\world\orbital_system.cpp /Fe:world_audit.exe
world_audit.exe
```

Each exits non-zero on a failed assertion. The economy *panel* (the visual class)
is verified separately via `ProjectIo --verify scripts/verify/economy_panel.lua`
(the `verifier-visual` skill).
