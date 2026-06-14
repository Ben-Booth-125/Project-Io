# Headless verification harnesses

Throwaway-but-kept C++ harnesses that verify pure `world/*` logic without the
SDL/Lua/ImGui stack (see memory `reference_headless_build`). They live outside
`src/` so the CMake `GLOB_RECURSE` does not pull them into the real build.

Compile and run from the repo root, after sourcing the VS BuildTools `vcvars64`:

```bat
:: Layer 3 economy logic (production -> market clearing -> budget)
cl /nologo /std:c++20 /EHsc /I src tools\verify\econ_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp /Fe:econ_harness.exe
econ_harness.exe

:: World audit — Kepler biome balance (S2) + extraction placement (S1)
cl /nologo /std:c++20 /EHsc /I src tools\verify\world_audit.cpp ^
   src\world\world.cpp src\world\tile_generation.cpp src\world\nation_generation.cpp ^
   src\world\corporation_generation.cpp src\world\hard_coded_world.cpp ^
   src\world\orbital_system.cpp /Fe:world_audit.exe
world_audit.exe
```

Each exits non-zero on a failed assertion. The economy *panel* (the visual class)
is verified separately via `ProjectIo --verify scripts/verify/economy_panel.lua`
(the `verifier-visual` skill).
