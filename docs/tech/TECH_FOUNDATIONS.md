# Project Io — Technical Foundations

This document captures the decisions that must be in place before development begins. It is divided into three categories: **Direction** (scope and design intent), **Engine** (core technology and architecture), and **UI** (rendering approach and view structure). Where the rationale for a decision is non-obvious, it is stated briefly.

---

## Direction

*Settled decisions about what the prototype is and is not.*

### Prototype scope
The prototype validates the **economy loop first**: resource extraction, market behaviour, supply routing, and the tick-gated price resolution system. Everything else is in scope only to the degree it feeds trade or conflict through that loop.

Combat resolution is **in the codebase and reachable from campaign play**, but as a thin layer over the economy rather than a pillar of its own. There are two resolvers, and the split is deliberate: `resolve_battle` (`src/world/combat.{hpp,cpp}`) — a class-matchup matrix × formation doctrine × terrain × supply × season, integer arithmetic with a deterministic tie-break — answers "region beats region, this year" for the Era −1 history sim (`src/world/history_sim.cpp`); `resolve_campaign_battle` (`src/world/campaign_battle.{hpp,cpp}`) — seeded rounds, priced withdrawal, a begin/step/withdraw state machine — is called by `run_battles` in the live economy tick when two hostile forces share a province. Both read the same terrain functions: `terrain_combat.{hpp,cpp}` provides `terrain_defence` / `terrain_attrition` / `terrain_resistance` (0–1000, pure functions of composition × landform — no stored field, nothing on the serialisation seam), consumed by both resolvers and by the history ladder's conquest pricing. There is one answer to "how does ground resist an army".

The economy that feeds force is part of the same loop: `run_unit_upkeep` (`src/world/economy_system.cpp`) draws goods per unit per tick and `apply_budget` (`src/world/budget_system.cpp`) charges the credit half; every authored upkeep rate in `scripts/economy.lua` § `economy.military.unit_upkeep` is `0.0`, so the pass runs and an army costs nothing. `resource_type::ordnance` is the roster's terminal military good, produced by the Fabricator recipe and named as upkeep's draw. `docs/military/MILITARY.md` is the authority for the military layer; this section only fixes its scope boundary.

### Units
Units are **minimal entities** in the data model: `unit_component` carries `{position, owner, count, type, order, supply_factor_permille, muster_base}`, with position tile-canonical and strength **derived** from count × roster quality × supply rather than stored. They are defined so that extending combat later extends the existing model rather than replacing it. Detail in `docs/military/MILITARY.md` § The unit model.

Explicitly **outside** the prototype:

- **Unit transport infrastructure** — troop carriers, shuttles, transit buildings. No such building type exists, by decision.
- **An equipment system** — nothing equips, upgrades or re-arms a unit; a roster row's `power_mod` is its whole materiel model. Weaponry as a *traded good* (ordnance) is in scope because it has an economic consumer; weaponry as a per-unit system is not.

### Procedural generation
Generation is **in scope and deterministic** — seeded, same seed same world; the `world/*` determinism rule covers it. The chain, in run order, all in `src/world/`: `planetology.cpp` (atmosphere / chemistry / evolution history), `continents.cpp` (drifting plates → height bias), `tile_generation.cpp` (the six-pass tile pipeline), `population_generation.cpp`, `history_ladder.cpp` (the pre-national ladder that sets the nation seed budget), `nation_generation.cpp`, `road_generation.cpp`, and `corporation_generation.cpp`. The entry point is the **New World wizard** (`src/core/app.cpp` — "New Game" builds nothing until the wizard finishes). `hard_coded_world.cpp` is the assembly point for the chain, not authored content. Design authority: `docs/generation/GENERATION_STRATEGY.md` and the per-subject generation docs.

### Buildings and infrastructure
Economy-supporting infrastructure (extraction sites, processing facilities, ports) is in scope to the degree needed to demonstrate the trade loop. The one military building is `building_type::military_base` — the single economy → military interface and the only place units come from: `hire_unit` requires the acting corp's own completed base on the named tile. It is priced in `scripts/economy.lua` at build cost 300, 35 steel, 4 ticks, maintenance 15, and tech-gated behind `E0-ML-01`.

**There is no second logistics network, by decision.** BL-325 ruling 3 (Ben, 2026-08-08): economic reach **is** military reach — there is one `body_reach_field` and armies pay it. A `military_base` is deliberately not a supply anchor and extends nothing (`docs/economy/LOGISTICS.md`).

Transport buildings for units are excluded (§ Units above).

### Factions
The generated world carries a full political layer: generated nations, their count a derived consequence of the homeworld's geography rather than an authored number (43 on the default seed), and rival corporations generated alongside the player (`corporation_generation.cpp`). Rivals act within scoped limits: per-building reflexes (idle a loss-maker, switch a floored recipe, throttle a depleting deposit) and a deterministic scored-utility strategic layer over the corp-command seam (`src/world/corp_ai.cpp`; design in `docs/ai/AI_OPPONENT.md`). Nations act in the same shape (`src/world/nation_ai.cpp`; `docs/politics/NATIONS.md`). The limits are standing rules, not scope absence — `.claude/rules/io-standing-rules.md` carries every sanctioned exception, including the one auto-action permitted on the player's corp (the workforce auto-solve dial). The data model must not preclude multi-faction play: every corporation is an equal `corporation_component` acting through the same seam, and the player is a marker, not a type.

---

## Engine

*Architecture decisions required before development begins.*

### Language and scripting
The engine is written in **C++** with **Lua** used for scripting. The split is: simulation logic lives in C++; Lua handles data definitions, balance values, and future scripting of behaviour. The boundary is kept narrow — the less data shared between C++ and Lua, the better. Performance-critical paths (extraction yields, price resolution, supply calculations) stay in C++.

### Agent interface — out-of-process only
The app ships **three headless CLI modes** beside its windowed one, all in `src/main.cpp`:
`--verify <script>` (visual-verification capture), `--export-blackboard <corp|all>` (the read
leg), and `--serve [--ticks N]` — a persistent mode that reads one request per line from stdin
and answers on stdout. `--load <path>` opens the windowed app straight into a save.

`tools/mcp/` wraps `--serve` as an **MCP server**: Node, JSON-RPC 2.0 over stdio, spawning
`ProjectIo --serve` as a child. That adds a third runtime, but **only at the tooling boundary** —
it is not an engine dependency, and the engine cannot call out.

The hard invariant, and the reason the seam is shaped this way: **`ProjectIo.exe` ships no HTTP
client, no API key and no cloud dependency.** A model reaches the game by speaking to a wrapper
that speaks to the process; the process never reaches a model. Anything that would put a network
client in the engine is out of scope. Authority: `docs/ai/AI_OPPONENT.md` § 10.

### Framework — SDL3
The platform layer is **SDL3**, providing windowing, input, audio, and 2D rendering on Windows and Linux.

SDL3 was chosen over SFML and raylib for three reasons. First, it sits at exactly the right level of abstraction for a custom engine: it handles platform-specific concerns without imposing any opinions about game structure, leaving the simulation layer, scripting boundary, and view systems entirely under our control. Second, SDL3's new GPU API is available for future use on the solar system view without requiring a platform layer change. Third, SDL2 is in maintenance mode; starting a new project on SDL3 gives access to active development, a fully revised API reference, and a larger community knowledge base.

SFML was ruled out due to slower evolution and a C++ OOP API that works against a data-oriented simulation model. Raylib was ruled out because its built-in main loop and camera abstractions create friction when building a custom engine with a separate simulation tick and multiple distinct view systems.

### Lua embedding — sol2
Lua is embedded via **sol2 (v3)**. sol2 provides the highest-ergonomics C++ ↔ Lua binding available: automatic type conversion, clean class registration, and thorough documentation. The cost is heavier compile-time metaprogramming; this is acceptable given that Lua's role in the prototype is limited to data definitions and balance values, not hot-path simulation code.

sol2 calls that can produce errors must use the `protected_` variants. LuaBridge3 remains a viable fallback if compile times become a practical problem — it is simpler and leaner at the cost of fewer features.

### Simulation tick architecture
The simulation runs a render loop over a fixed-timestep clock:

- **Render loop** — runs every frame at the display refresh rate. Interpolates visual state between simulation steps to produce smooth rendering regardless of simulation rate.
- **Simulation clock** — a three-layer fixed-timestep *calendar* (`sim_loop`, `src/core/sim_loop.hpp`), finest to coarsest:
  - **sim tick** — the smallest step; `sim_ticks_per_day = 12` of them make one in-game day. Fine enough to interpolate smooth visual motion between.
  - **day tick** — fires once per in-game day.
  - **econ tick** — fires every `econ_tick_days = 90`: one quarter, three 30-day months (`days_per_month × months_per_econ`). Supply, demand, and market prices resolve here.

Real-time pacing is a speed multiplier (1×–5×, non-linear; 0 pauses). At 1× a day takes `seconds_per_day_1x = 2.0` real seconds. The calendar and pacing values are deliberately tentative — `sim_loop` is the single place to retune game time; the render layer reads the tick counters but never writes them. The clock is calendar-derived (steps per *in-game day*, paced by real seconds per day), not a fixed real-time step rate, and the economy tick is pinned to the quarter rather than independently tunable.

**Input handling:** input events are collected by the render loop and accumulated between simulation steps. They are applied in batch at the start of each simulation step. This keeps input processing deterministic and simulation-isolated.

**In-flight actions at tick boundaries:** entities (supply convoys, construction jobs) carry a fractional progress value that increments each simulation step. Completion is evaluated at the economy tick boundary. The render layer reads progress values for interpolation; it does not modify them.

**Save model:** a whole-world snapshot, taken as a **discrete act** rather than an automatic per-tick write — nothing writes a save the player did not ask for, which keeps an ~18.7 MB write off the tick path. `write_save_game` / `read_save_game` (`src/core/save_game.{hpp,cpp}`) write one `.iosave` file: a magic + version header, the world snapshot (`write_world_snapshot` / `read_world_snapshot`, `src/world/world_save.{hpp,cpp}`), then the app-layer envelope. F5 quick-saves, F6 quick-loads, `--load <path>` opens straight into one, and `verify.save` / `verify.load` drive it from a capture script. Save-format records carry `static_assert` guards (`molecular_event` in `chemistry_tables.hpp` — "must stay 8 bytes — it is a save-format record") and append-only id disciplines (the event-trace ids in `planetology.hpp`).

**What a save contains.** Three buckets: every authoritative container on `world` (including the entity-allocator cursor, or a load would re-issue live ids); the derived caches, which are *rebuilt* rather than written (`clear_derived_state`); and the app envelope — the sim clock, `world_params`, the `generation_report` in full, the per-tick histories and a narrow view slice. `world/*` stays SDL- and UI-free, so the headless harness (`tools/verify/save_roundtrip.cpp`) exercises the world half alone. `corp_modifiers` looks derived and is not: its stored order is the cross-tick earn order, and `modified_scalar` folds `add` against `multiply`, which do not commute, so a re-fold from `earned_techs` returns a different number. It is serialised directly (NR-510).

### Tile and body data model
All tiles for a body are **resident in memory simultaneously**. For the prototype — generated but bounded bodies and tile counts — this is the correct approach. Tile data is pure C++ structs packed into a contiguous array per body, accessed by coordinate index in O(1) with no query overhead.

Tile data is **not Lua-scriptable at the per-tile level**. The scripting boundary is at higher levels: building definitions, market rules, faction behaviour. Exposing individual tile callbacks to Lua would introduce performance and complexity costs with no clear gain during the prototype phase.

**Serialisation format: flat binary, written FIELD BY FIELD.** Structs are *not* written as raw memory: Ben's 2026-08-22 call was that the schema still moves weekly, so a padding-baked layout would turn every added field into a silent misread caught only by a remembered version bump. `src/world/binary_io.hpp` holds the vocabulary; padding, member order and ABI stop mattering. The cost is bounded — the canonical 50,901-tile world writes in ~17.9 MB. Every stream carries a **magic + version header** (BL-107, save-format versioning): `IOSV` (world snapshot), `IOSG` (save game), `IOHL` (history log), `IOOB` (order book) and `IOPC` (procurement). A mismatch in any of them rejects the whole load rather than reinterpreting a changed layout; broader versioning (migration, forward compatibility) stays deferred until the data model stabilises, so **rejection is the entire migration story for the prototype**.

SQLite is the natural next step if the world grows to the point where bodies cannot all be held in memory, or where querying across tile properties becomes necessary. That decision is deferred deliberately — the data structures must not preclude it, but nothing is built for it now.

### Building (Linux + Windows)

The project is **cross-platform** (BL-057, Linux development OS): Linux is the primary development OS and
Windows is the playtest OS. All dependencies (SDL3, Lua 5.4, sol2, Dear ImGui) are
acquired by CMake `FetchContent` — there are no system-package or vcpkg
assumptions — so a configure + build is self-contained on either OS.

**Full app (either OS):**

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

On Linux this needs the SDL3 system build deps (X11/Wayland + GL/EGL + audio dev
headers). The native Linux GUI build works (Ubuntu 24.04, configure + build + run).

**Toolchain requirements.**

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
external deps, no display — which is why it runs in any sandbox. It is the standing
guard, and with no CI the *only* one. Link each harness against
every `src/world/*.cpp` except `recipe_registry.cpp` (the one TU that pulls sol2/Lua) —
in practice via the `io_world_obj` OBJECT library, which compiles the world
layer once for the whole tier rather than once per harness:

```
WORLD=$(ls src/world/*.cpp | grep -v recipe_registry.cpp)
g++ -std=c++20 -O2 -I src tools/verify/<name>.cpp $WORLD -o <name> && ./<name>
```

**Source portability note.** GCC rejects a struct field whose name matches its own
type (`-Wchanges-meaning`, ill-formed per `[basic.scope.class]`) where MSVC accepts
it. Keep enum-typed fields named for their *role*, not their type
(`terrain_substrate substrate`, not `ideology ideology`) — see
`src/world/components.hpp`. The portable font path is bundled under `assets/fonts/`
and resolved cwd-relative first (`src/ui/fonts.cpp`), so on-screen text and visual
goldens render identically across machines.

---

## UI

*Rendering approach and view structure. Per-view detail is deferred to the UI document.*

### Rendering dimension
The game renders in **2D**. No 3D terrain, unit models, or depth-based rendering is in the prototype. Detailed surface visualisation **is** in scope for the Planetary canvas (Ben, 2026-09-01, the sprint-29 design forms — overturning this section's earlier exclusion): the ground renders as **baked painterly terrain chunks** (hillshade from the height field + authored biome brushes, C-F art direction) on the existing 2D renderer, with ambient overlay animation. Authority: `docs/ui/RENDERING.md`. The staged end-state — a tilting oblique camera with terrain and structures as true geometry — is a **flagged future milestone** (2.5D pre-render vs SDL3 GPU 3D, undecided; `docs/research/CANVAS_RENDERING.md` § The end-state choice), not prototype scope; the SDL3 GPU API remains the door held open for it.

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
| Tick architecture | Three-layer calendar clock (`sim_loop`: 12 sim ticks/day → day tick → 90-day quarter econ tick); saves are discrete whole-world snapshots |
| Generation | In scope, deterministic (seeded) — planetology → continents → tiles → history ladder → nations/population/roads/corporations |
| Factions | Generated nations + rival corps with scoped, deterministic scored-utility AI; nations act in the same shape |
| Military | Two resolvers — `resolve_battle` for the Era −1 sim, `resolve_campaign_battle` for `run_battles` in the economy tick — over one terrain model; muster building, hire verb, ordnance good, unit upkeep (authored rates 0.0), stance and the unit march seam. Authority: `docs/military/MILITARY.md` |
| Tile memory | All tiles resident; flat binary structs; no per-tile Lua |
| UI (prototype) | Dear ImGui |
| Ground rendering | Baked painterly terrain chunks on the 2D renderer (Planetary rung only; C-F direction; no on-ground hex grid; installations as rendered geometry; overlay animation; vector bake as fallback by coverage). Authority: `docs/ui/RENDERING.md` |
| UI (production) | Lua-driven retained layer — deferred to UI document |
| Serialisation | Flat binary, field by field, magic + version per stream, mismatch rejected; SQLite deferred until world scale requires it |
| Agent interface | **Out-of-process only.** `ProjectIo --serve` speaks a line protocol on stdio; `tools/mcp/` wraps it as an MCP server (Node, tooling tier). The engine ships no HTTP client, no API key, no cloud dependency. Authority: `docs/ai/AI_OPPONENT.md` § 10 |
| Build | CMake + FetchContent (self-contained), Linux + Windows; headless tier needs only a C++20 compiler |
