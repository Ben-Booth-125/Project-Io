# Project Io — Technical Foundations

This document captures the decisions that must be in place before development begins. It is divided into three categories: **Direction** (scope and design intent), **Engine** (core technology and architecture), and **UI** (rendering approach and view structure). The Engine and UI decisions all still hold. Several **Direction** items were written before the prototype outgrew them; each superseded one carries a dated note in place rather than a silent rewrite (sweep 2026-07-31). Where the rationale for a decision is non-obvious, it is stated briefly.

---

## Direction

*Settled decisions about what the prototype is and is not.*

### Prototype scope
The prototype validates the **economy loop only**. It covers resource extraction, market behaviour, supply routing, and the tick-gated price resolution system. Combat is explicitly out of scope for this phase.

> **Status note (superseded 2026-08-02 — BL-272 landed).** A battle resolver now exists:
> `src/world/combat.{hpp,cpp}` — `resolve_battle` over a class-matchup matrix × formation
> doctrine × terrain × supply × season, integer arithmetic with a deterministic tie-break, and
> `unit_component` carries a `uint16_t` type field. Its consumer is the **Era −1 history sim**
> (BL-271), not the campaign layer: nothing in Era 0 commands a unit, and the campaign-side stub
> is BL-157 (military datamodel stub, v0.1.4). So combat resolution is no longer excluded from the
> *codebase*, only from *campaign play*.
>
> The earlier note, still accurate on terrain: `terrain_combat.{hpp,cpp}` provides `terrain_defence` / `terrain_attrition` / `terrain_resistance` (0–1000, pure functions of composition × landform — no stored field, nothing on the serialisation seam). No consumer is wired yet: the history ladder still prices conquest through its own `is_barrier` bool, and swapping it onto the graded scalars — plus any gameplay use — is BL-233 (terrain combat modifiers), open. When combat lands it reads these same functions rather than inventing a second answer to "how does ground resist an army".

> **Re-based 2026-08-18 — "campaign play" is no longer the clean line either.** The note above
> drew the boundary at *codebase vs campaign play*. That boundary has since moved twice, and this
> document was not updated with it. What ships now:
>
> - **A second, campaign-scale resolver.** `src/world/campaign_battle.{hpp,cpp}` (BL-315, armed
>   house conflict spine) — seeded rounds, priced withdrawal, a begin/step/withdraw state machine.
>   Compiled and harnessed (`tools/verify/campaign_battle_harness.cpp`), with **no production
>   caller**. Designed-and-built-but-unreachable, not designed-and-unbuilt.
> - **A recurring cost of force.** `run_unit_upkeep` (`src/world/economy_system.cpp`) draws goods
>   per unit per tick and `apply_budget` charges the credit half (`src/world/budget_system.cpp:173`)
>   — BL-454 (unit upkeep). **Every authored rate is 0.0** (`scripts/economy.lua`
>   § `economy.military.unit_upkeep`), so the plumbing ships inert and an army costs nothing today.
> - **A military good.** `resource_type::ordnance` (BL-457, ordnance) — the roster's first terminal
>   military good, produced by the Fabricator recipe and named as unit upkeep's draw.
> - **A muster building.** `building_type::military_base` (BL-325, military bases and supply).
>
> The honest statement of scope is therefore: **combat resolution and the economy that would feed
> it are built; campaign combat is not *reachable*.** Nothing composes an army from live units in
> production, nothing decides two forces are fighting, and no verb names a unit. The absent list
> is enumerated in `docs/military/MILITARY.md` § What is absent — that doc, not this one, is the
> authority for the military layer's landed/outstanding split.

### Units
Units exist in the **data model** as minimal entities. They are defined enough that they will not need to be retrofitted when combat is added, but no combat rules are implemented. The following are explicitly excluded from the prototype:

- ~~Combat resolution of any kind~~ — *superseded 2026-08-02, 2026-08-18 and again 2026-08-21; **both** resolvers ship and **both are now called**. `resolve_battle` (BL-272) by the Era −1 sim; `resolve_campaign_battle` by `run_battles` in the live economy tick since BL-467, which built the engagement trigger and battle state the resolver had always lacked. BL-468/BL-469 give it its two surfaces. Campaign combat is reachable — see `docs/military/MILITARY.md` § Build status for the stubs that remain (doctrine, season, reinforcement, and the field having no consequence).*
- ~~Opponent or AI factions~~ — *superseded; see § Factions below (2026-07-31)*
- Unit transport infrastructure (troop carriers, shuttles, transit buildings) — *still holds (checked against source 2026-08-18: no such building type exists)*
- ~~Weaponry or equipment systems~~ — *narrowed 2026-08-18. **Weaponry as a traded good ships**: `resource_type::ordnance` (BL-457, ordnance) is produced by the Fabricator and drawn per-head per-tick by unit upkeep (BL-454, unit upkeep), at an authored rate of 0.0. What still does not exist is an **equipment system** — nothing equips, upgrades or re-arms a unit; a roster row's `power_mod` is its whole materiel model.*

Also no longer accurate as written: "no combat rules are implemented". Both resolvers implement
rules; what is missing is anything that *invokes* them on live units.

The design goal is that adding combat later extends the existing model rather than replaces it.

> **Where the unit data model actually stands (2026-08-18).** `unit_component` carries
> `{position, owner, count, type, supply_factor_permille, muster_base}`. The `strength` field this
> section's "minimal entities" framing assumed was **removed** by BL-459 (unit strength is a
> duplicate of count) — strength is now derived, not stored. Detail in
> `docs/military/MILITARY.md` § The unit model.

### Procedural generation

> **Superseded (2026-07-31).** The original position — generation out of scope, bodies authored or hard-coded — did not survive contact. The settled position now: **generation is in scope and deterministic** (seeded — same seed, same world; the `world/*` determinism rule covers it). The shipped chain, in run order, all in `src/world/`: `planetology.cpp` (atmosphere / chemistry / evolution history, BL-167 planetology), `continents.cpp` (drifting plates → height bias), `tile_generation.cpp` (the six-pass tile pipeline), `population_generation.cpp`, `history_ladder.cpp` (the pre-national ladder that sets the nation seed budget, BL-221), `nation_generation.cpp`, `road_generation.cpp`, and `corporation_generation.cpp`. The entry point is the **New World wizard** (`src/core/app.cpp` — "New Game" builds nothing until the wizard finishes). `hard_coded_world.cpp` survives as the assembly point for the chain, not as authored content. Design authority: `docs/generation/GENERATION_STRATEGY.md` and the per-subject generation docs.

### Buildings and infrastructure
Economy-supporting infrastructure (extraction sites, processing facilities, ports) is in scope only to the degree needed to demonstrate the trade loop. Buildings whose primary purpose is unit production, transport, or military logistics are excluded.

> **Superseded on the military clause (2026-08-18).** A **unit-production building ships**:
> `building_type::military_base` (BL-325, military bases and supply). It is the single
> economy → military interface and the only place units come from — `hire_unit` requires the
> acting corp's own completed base on the named tile. Priced in `scripts/economy.lua` at build
> cost 300, 35 steel, 4 ticks, maintenance 15; tech-gated behind `E0-ML-01`.
>
> **Military *logistics* was not excluded so much as declined a separate existence.** BL-325's
> ruling 3 (Ben, 2026-08-08) is that economic reach **is** military reach: there is one
> `body_reach_field` and armies pay it. A `military_base` is deliberately not a supply anchor and
> extends nothing. So the correct statement is not "military logistics is out of scope" but
> "there is no second logistics network, by decision".
>
> The transport clause **still holds** — no troop carrier, shuttle or transit building exists.

### Factions

> **Superseded (2026-07-31).** "Only the player's corporation exists" is dead. The generated world carries a full political layer — generated nations, their count a derived consequence of the homeworld's geography rather than an authored number (43 on the default seed) — and rival corporations generated alongside the player (`corporation_generation.cpp`). Rivals act, within scoped limits: BL-079 (background-corp agency) gives per-building reflexes, and BL-202 (corp AI stage A) adds a scored-utility strategic layer over the corp-command seam (`src/world/corp_ai.cpp`; design in `docs/ai/AI_OPPONENT.md`). The limits are standing rules, not scope absence — `.claude/rules/io-standing-rules.md` carries the two sanctioned exceptions (BL-079 agency; BL-181 workforce auto-solve, the one auto-action on the player's corp). Sentiment tracking and diplomacy remain unbuilt — data-model comments only. The original constraint that the data model must not preclude multi-faction play paid off; that is exactly how this landed.

---

## Engine

*Architecture decisions required before development begins.*

### Language and scripting
The engine is written in **C++** with **Lua** used for scripting. The split is: simulation logic lives in C++; Lua handles data definitions, balance values, and future scripting of behaviour. The boundary is kept narrow — the less data shared between C++ and Lua, the better. Performance-critical paths (extraction yields, price resolution, supply calculations) stay in C++.

### Agent interface — out-of-process only *(added 2026-08-04; BL-278 landed)*
The app ships **three headless CLI modes** beside its windowed one, all in `src/main.cpp`:
`--verify <script>` (visual-verification capture), `--export-blackboard <corp|all>` (BL-206, the
read leg), and `--serve [--ticks N]` (BL-278) — a persistent mode that reads one request per line
from stdin and answers on stdout.

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

Real-time pacing is a speed multiplier (1×–5×, non-linear; 0 pauses). At 1× a day takes `seconds_per_day_1x = 2.0` real seconds (retuned 3× quicker, 2026-07-15). The calendar and pacing values are deliberately tentative — `sim_loop` is the single place to retune game time; the render layer reads the tick counters but never writes them.

> **Superseded detail (2026-07-31).** The original framing — "fixed timestep, target 20 steps per second; economy tick a free tunable" — is gone. The clock is calendar-derived (steps per *in-game day*, paced by real seconds per day), not a fixed real-time step rate, and the economy tick is pinned to the quarter rather than independently tunable.

**Input handling:** input events are collected by the render loop and accumulated between simulation steps. They are applied in batch at the start of each simulation step. This keeps input processing deterministic and simulation-isolated.

**In-flight actions at tick boundaries:** entities (supply convoys, construction jobs) carry a fractional progress value that increments each simulation step. Completion is evaluated at the economy tick boundary. The render layer reads progress values for interpolation; it does not modify them.

**Save model:** whole-world snapshot, **built** (BL-536, 2026-08-22). `write_save_game` / `read_save_game` (`src/core/save_game.{hpp,cpp}`) write one `.iosave` file: a magic + version header, the world snapshot, then the app-layer envelope. Saving is a discrete act rather than an automatic per-tick snapshot — F5 quick-saves, F6 quick-loads, `--load <path>` opens straight into one, and `verify.save` / `verify.load` drive it from a capture script. *The per-economy-tick autosnapshot this section used to describe was never built and is not what landed:* nothing writes a save the player did not ask for, which keeps a 18.7 MB write off the tick path. The *format* had been hardened in place ahead of the serialiser (2026-07-31) and that groundwork held: save-format records carry `static_assert` guards (`molecular_event` in `chemistry_tables.hpp` — "must stay 8 bytes — it is a save-format record") and append-only id disciplines (the BL-209 event-trace ids in `planetology.hpp`).

**What a save contains, and the one thing that surprised us.** Three buckets: every authoritative container on `world` (including the entity-allocator cursor, or a load would re-issue live ids); the derived caches, which are *rebuilt* rather than written; and the app envelope — the sim clock, `world_params`, the `generation_report` in full, the per-tick histories and a narrow view slice. `corp_modifiers` looks derived and is not: its stored order is the cross-tick earn order, and `modified_scalar` folds `add` against `multiply`, which do not commute, so a re-fold from `earned_techs` returns a different number. It is serialised directly (NR-510).

### Tile and body data model
All tiles for a body are **resident in memory simultaneously**. For the prototype — generated but bounded bodies and tile counts — this is the correct approach. Tile data is pure C++ structs packed into a contiguous array per body, accessed by coordinate index in O(1) with no query overhead. *(The clause "no procedural generation" that used to sit here is superseded — see § Procedural generation; the all-resident decision itself is unaffected, 2026-07-31.)*

Tile data is **not Lua-scriptable at the per-tile level**. The scripting boundary is at higher levels: building definitions, market rules, faction behaviour. Exposing individual tile callbacks to Lua would introduce performance and complexity costs with no clear gain during the prototype phase.

**Serialisation format: flat binary, written FIELD BY FIELD.** Structs are *not* written as raw memory, which this section used to specify: Ben's 2026-08-22 call was that the schema still moves weekly, so a padding-baked layout would turn every added field into a silent misread caught only by a remembered version bump. `src/world/binary_io.hpp` holds the vocabulary; padding, member order and ABI stop mattering. The cost is bounded — the canonical 50,901-tile world writes in ~17.9 MB. **BL-107's magic + version header landed with the serialiser**, as five separate streams now carry it: `IOSV` (world snapshot), `IOSG` (save game), `IOHL` (history log), `IOOB` (order book) and `IOPC` (procurement). A mismatch in any of them rejects the whole load rather than reinterpreting a changed layout; broader versioning stays deferred until the data model stabilises, so **rejection is the entire migration story for the prototype**.

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
external deps, no display — which is why it runs in any sandbox. It is the standing
guard, and since CI was removed (2026-07-31) the *only* one. Link each harness against
every `src/world/*.cpp` except `recipe_registry.cpp` (the one TU that pulls sol2/Lua) —
in practice via the `io_world_obj` OBJECT library, which since BL-287 compiles the world
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
| Tick architecture | Three-layer calendar clock (`sim_loop`: 12 sim ticks/day → day tick → 90-day quarter econ tick); tick-boundary snapshots for saves (planned) |
| Generation | In scope, deterministic (seeded) — planetology → continents → tiles → history ladder → nations/population/roads/corporations (superseded the out-of-scope call, 2026-07-31) |
| Factions | Generated nations + rival corps with scoped AI (BL-079 reflexes, BL-202 corp AI, BL-203 predictive spending) |
| Military (2026-08-21) | Both resolvers ship and both are called — `resolve_battle` by the Era −1 sim (BL-272), `resolve_campaign_battle` by `run_battles` in the economy tick (BL-467), with dispatches and a battle card over it (BL-468/BL-469). Muster building (BL-325), hire verb, ordnance good (BL-457), unit upkeep (BL-454, **rates 0.0**), stance (BL-448/BL-449) and the unit march seam (BL-470) all landed. Campaign combat is **reachable**; what is still absent is a list, not a gap — doctrine is an all-zero stub, season is hardcoded, membership snapshots at open, and holding the field has no consequence. Authority: `docs/military/MILITARY.md` |
| Tile memory | All tiles resident; flat binary structs; no per-tile Lua |
| UI (prototype) | Dear ImGui |
| UI (production) | Lua-driven retained layer — deferred to UI document |
| Serialisation | Flat binary; format hardened in place ahead of the serialiser; SQLite deferred until world scale requires it |
| Agent interface | **Out-of-process only.** `ProjectIo --serve` speaks a line protocol on stdio; `tools/mcp/` wraps it as an MCP server (Node, tooling tier). The engine ships no HTTP client, no API key, no cloud dependency. Authority: `docs/ai/AI_OPPONENT.md` § 10 |
| Build | CMake + FetchContent (self-contained), Linux + Windows; headless tier needs only a C++20 compiler |