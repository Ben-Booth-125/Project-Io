# Project Io — Technical Foundations

This document captures the decisions that must be in place before development begins. It is divided into three categories: **Direction** (scope and design intent), **Engine** (core technology and architecture), and **UI** (rendering approach and view structure). All items are settled. Where the rationale for a decision is non-obvious, it is stated briefly.

---

## Direction

*Settled decisions about what the prototype is and is not.*

### Prototype scope
The prototype validates the **economy loop only**. It covers resource extraction, market behaviour, supply routing, and the tick-gated price resolution system. Combat is explicitly out of scope for this phase.

### Units
Units exist in the **data model** as minimal entities. They are defined enough that they will not need to be retrofitted when combat is added, but no combat rules, faction AI, or opponent behaviour is implemented. The following are explicitly excluded from the prototype:

- Combat resolution of any kind
- Opponent or AI factions
- Unit transport infrastructure (troop carriers, shuttles, transit buildings)
- Weaponry or equipment systems

The design goal is that adding combat later extends the existing model rather than replaces it.

### Procedural generation
Procedural generation of bodies and tiles is **out of scope** for the prototype. Bodies and their tile profiles will be authored or hard-coded for testing purposes. The data structures must accommodate generated content without requiring it.

### Buildings and infrastructure
Economy-supporting infrastructure (extraction sites, processing facilities, ports) is in scope only to the degree needed to demonstrate the trade loop. Buildings whose primary purpose is unit production, transport, or military logistics are excluded.

### Factions
Only the **player's corporation** exists in the prototype. No faction behaviour, sentiment tracking, or diplomacy is implemented. The data model must not preclude multi-faction play, but nothing is built for it at this stage.

---

## Engine

*Architecture decisions required before development begins.*

### Language and scripting
The engine is written in **C++** with **Lua** used for scripting. The split is: simulation logic lives in C++; Lua handles data definitions, balance values, and future scripting of behaviour. The boundary is kept narrow — the less data shared between C++ and Lua, the better. Performance-critical paths (extraction yields, price resolution, supply calculations) stay in C++.

### Framework — SDL3
The platform layer is **SDL3**, providing windowing, input, audio, and 2D rendering on Windows and Linux.

SDL3 was chosen over SFML and raylib for three reasons. First, it sits at exactly the right level of abstraction for a custom engine: it handles platform-specific concerns without imposing any opinions about game structure, leaving the simulation layer, scripting boundary, and view systems entirely under our control. Second, SDL3's new GPU API is available for future use on the solar system view without requiring a platform layer change. Third, SDL2 is in maintenance mode; starting a new project on SDL3 gives access to active development, a fully revised API reference, and a larger community knowledge base.

SFML was ruled out due to slower evolution and a C++ OOP API that works against a data-oriented simulation model. Raylib was ruled out because its built-in main loop and camera abstractions create friction when building a custom engine with a separate simulation tick and multiple distinct view systems.

### Lua embedding — sol2
Lua is embedded via **sol2 (v3)**. sol2 provides the highest-ergonomics C++ ↔ Lua binding available: automatic type conversion, clean class registration, and thorough documentation. The cost is heavier compile-time metaprogramming; this is acceptable given that Lua's role in the prototype is limited to data definitions and balance values, not hot-path simulation code.

sol2 calls that can produce errors must use the `protected_` variants. LuaBridge3 remains a viable fallback if compile times become a practical problem — it is simpler and leaner at the cost of fewer features.

### Simulation tick architecture
The simulation runs in two loops:

- **Render loop** — runs every frame at the display refresh rate. Interpolates visual state between the last and current simulation states to produce smooth rendering regardless of simulation rate.
- **Simulation loop** — runs at a fixed timestep (target: 20 steps per second) using an accumulator pattern. All gameplay logic, movement progress, and supply calculations advance here. This loop is entirely independent of real-world time and frame rate.

**Economy tick** is a distinct, lower-frequency checkpoint layered on top of the simulation loop. Supply, demand, and market prices resolve at the economy tick boundary. The economy tick rate is a tunable parameter; it does not need to be tied to the simulation step rate.

**Input handling:** input events are collected by the render loop and accumulated between simulation steps. They are applied in batch at the start of each simulation step. This keeps input processing deterministic and simulation-isolated.

**In-flight actions at tick boundaries:** entities (supply convoys, construction jobs) carry a fractional progress value that increments each simulation step. Completion is evaluated at the economy tick boundary. The render layer reads progress values for interpolation; it does not modify them.

**Save model:** tick-boundary snapshot. The full simulation state is serialised at each economy tick. This gives clean, deterministic save points that are cheap to implement and straightforward to debug. Manual save is a named copy of the most recent snapshot.

### Tile and body data model
All tiles for a body are **resident in memory simultaneously**. For the prototype — authored bodies, bounded tile counts, no procedural generation — this is the correct approach. Tile data is pure C++ structs packed into a contiguous array per body, accessed by coordinate index in O(1) with no query overhead.

Tile data is **not Lua-scriptable at the per-tile level**. The scripting boundary is at higher levels: building definitions, market rules, faction behaviour. Exposing individual tile callbacks to Lua would introduce performance and complexity costs with no clear gain during the prototype phase.

**Serialisation format: flat binary.** Tile structs are written and read as raw binary. This is the fastest format to implement and the fastest at runtime. The schema is fixed for the prototype; versioning is deferred until the data model stabilises.

SQLite is the natural next step if the world grows to the point where bodies cannot all be held in memory, or where querying across tile properties becomes necessary. That decision is deferred deliberately — the data structures must not preclude it, but nothing is built for it now.

### Building (Linux + Windows)

The project is **cross-platform** (BL-057): Linux is the primary development OS and
Windows is the playtest OS. All dependencies (SDL3, Lua 5.4, sol2, Dear ImGui) are
acquired by CMake `FetchContent` — there are no system-package or vcpkg
assumptions — so a configure + build is self-contained on either OS.

**Full app (either OS):**

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

On Linux this needs the SDL3 system build deps (X11/Wayland + GL/EGL + audio dev
headers); the exact apt set is the `linux` job in `.github/workflows/build.yml`.
The native Linux GUI build is **confirmed working** (Ubuntu 24.04, configure +
build + run) — the cross-platform path is no longer source-audit-only.

**Toolchain requirements (settled while bringing up the first native Linux build).**

- *C and C++ are both project languages.* `project(... LANGUAGES C CXX)` — Lua 5.4
  is compiled from `.c` sources (`lua54`), so C must be enabled explicitly. It must
  not rely on SDL3's own `FetchContent` `project(LANGUAGES C)` enabling C as a side
  effect; when that ordering doesn't hold, configure fails with
  `CMAKE_C_COMPILE_OBJECT / CMAKE_C_ARCHIVE_* not set`.
- *sol2 ≥ v3.5.0 for modern compilers.* GCC 14+ and Clang 19+ parse template bodies
  strictly and reject sol2 v3.3.0's `optional<T&>::emplace` (calls a `construct`
  member the specialisation lacks — `[-Wtemplate-body]`). The pinned tag is v3.5.0,
  which carries the upstream fix; on an older sol2 the workaround is to build with
  GCC 13.

**Headless verification tier** (the `tools/verify/*.cpp` harnesses over `world/*`,
deliberately SDL/Lua/ImGui-free) builds with nothing but a C++20 compiler — no
external deps, no display — which is why it runs in any sandbox and is the standing
CI guard. Link each harness against every `src/world/*.cpp` except
`recipe_registry.cpp` (the one TU that pulls sol2/Lua):

```
WORLD=$(ls src/world/*.cpp | grep -v recipe_registry.cpp)
g++ -std=c++20 -O2 -I src tools/verify/<name>.cpp $WORLD -o <name> && ./<name>
```

**Source portability note.** GCC rejects a struct field whose name matches its own
type (`-Wchanges-meaning`, ill-formed per `[basic.scope.class]`) where MSVC accepts
it. Keep enum-typed fields named for their *role*, not their type
(`terrain_composition composition`, not `ideology ideology`) — see
`src/world/components.hpp`. The portable font path is bundled under `assets/fonts/`
and resolved cwd-relative first (`src/ui/fonts.cpp`), so on-screen text and visual
goldens render identically across machines.

---

## UI

*Rendering approach and view structure. Per-view detail is deferred to the UI document.*

### Rendering dimension
The game renders in **2D**. No 3D terrain, unit models, or depth-based rendering is required. Detailed surface visualisation is not a prototype concern.

### Main views
The main game window has two primary views:

- **Solar system view** — bodies, orbits, supply routes, and faction presence at the scale of the whole system.
- **Body surface view** — tile grid, installations, and local economic detail for a selected body.

These views are considered distinct enough to be designed and implemented separately, though they share the same application window. The transition mechanic between them is a detail to resolve in the UI document.

### Additional views
Both primary views will require supplementary panels or overlay screens for detailed information (market data, budgets, resource ledgers). These are expected but their structure is deferred to the UI document. In the project glossary these are referred to as **Canvases** (screens for understanding a body or space situation) and **Ledgers** (sub-system reports for decision making).

### UI framework — Dear ImGui (prototype)
The prototype UI is built with **Dear ImGui**. ImGui's immediate-mode paradigm minimises UI state management, eliminates synchronisation bugs between simulation data and display state, and allows functional panels to be wired to live simulation data very quickly. For a prototype validating the economy loop, iteration speed and correctness matter more than visual polish.

ImGui is not the intended production UI. Grand strategy games require data-dense, highly customised interfaces that ImGui's styling system cannot easily produce. The planned path is:

1. **Prototype:** Dear ImGui — all panels, ledgers, and canvases built as ImGui windows.
2. **Production:** a custom Lua-driven retained UI layer. The prototype's ImGui panels serve as the functional specification: by the time they are replaced, the exact data requirements and interaction patterns of each view are known.

This means ImGui panel code should be written clearly, not cleverly. It is reference material as much as working code.

---

## Decision log

| Category | Decision |
|---|---|
| Engine | C++ simulation, Lua for data definitions and scripting |
| Framework | SDL3 |
| Lua binding | sol2 (v3) |
| Tick architecture | Fixed simulation loop + accumulator; economy tick layered on top; tick-boundary snapshots for saves |
| Tile memory | All tiles resident; flat binary structs; no per-tile Lua |
| UI (prototype) | Dear ImGui |
| UI (production) | Lua-driven retained layer — deferred to UI document |
| Serialisation | Flat binary; SQLite deferred until world scale requires it |
| Build | CMake + FetchContent (self-contained), Linux + Windows; headless tier needs only a C++20 compiler |