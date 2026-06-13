# Project Io — Development Log

Entries are newest-first. Each entry covers one development session and records what was built, what in-session decisions were made, and what was left open. Decisions that affect the whole project permanently belong in TECH_FOUNDATIONS or a dedicated ADR; this log is for session-scoped choices and progress notes.

---

## 2026-06-13 — Layer 1: ECS data model

**Status:** Complete. Layer 2 (extraction and production) is next.

### What was built

- `src/world/entity.hpp` — `entity_id` typedef (`uint32_t`); `null_entity = 0` sentinel.
- `src/world/components.hpp` — Shared enums (`resource_type`, `terrain_type`, `body_type`, `building_type`) and all six Layer 1 component structs: `tile_component`, `body_component`, `building_component`, `stockpile_component`, `market_component`, `unit_component`. Resource deposits and market arrays are `std::array<float, resource_count>` indexed by `resource_type`.
- `src/world/world.hpp` / `world.cpp` — ECS registry: one `std::unordered_map<entity_id, Component>` per component type, `create_entity()` allocating monotonically increasing IDs.
- `src/world/hard_coded_world.hpp` / `hard_coded_world.cpp` — `make_hard_coded_world()` populating two authored bodies: Kepler (4×4 planet, 1.0 AU, iron/silicate deposits, two buildings) and Vesta (3×3 moon, 5.2 AU, ice/rare-metal deposits, one building). ~200 authored float values across 25 tiles, 2 markets, 3 buildings, 1 unit stub.
- `src/ui/tile_inspector.cpp` — Layer 1 ImGui panel: body selector combo, scrollable tile table (terrain, hazard, habitability, per-resource deposits), buildings list, market supply/demand/price table. Serves as the functional specification for the production tile canvas and market ledger.
- `src/core/app` updated — `world m_world` member added; `make_hard_coded_world()` called at startup; `ui::draw_tile_inspector(m_world)` called each frame.

### In-session decisions

**ECS over OOP for the data model.**
The developer chose ECS explicitly. Entities are plain `uint32_t` IDs; all data lives in per-component maps on the `world` registry. No base classes, no virtual dispatch. Layer 1 has no systems yet — only data.

**`std::unordered_map` for component storage.**
Dense arrays would require a stable maximum entity count upfront. Sparse maps are correct for the prototype's authored, bounded world and keep entity creation trivial. Revisit if component iteration becomes a hot path in later layers.

**Four resource types for prototype scope.**
`iron_ore`, `ice`, `silicates`, `rare_metals` — enough to produce meaningful supply/demand divergence between bodies without expanding the market or extraction logic prematurely.

**`resource_count` constant from enum sentinel.**
`resource_type::count` used as array size via `static_cast<std::size_t>`. Avoids a separate manifest constant; adding a new resource type automatically sizes all arrays correctly.

**`tile_spec` local struct in `hard_coded_world.cpp`.**
A private helper struct used only during world construction — not part of the runtime data model. Keeps the authored values readable as a flat table without polluting `components.hpp`.

**Market prices seeded to `base_price` at init.**
Prices are set equal to `base_price` at construction so the market is in a neutral state before the first economy tick runs price resolution (Layer 3). No placeholder zeroes that would require special-casing.

### Corrections made during session

`SDL3::SDL3main` removed from `target_link_libraries` and `#include <SDL3/SDL_main.h>` removed from `main.cpp`. The SDL_main entry-point shim is only needed for Windows GUI subsystem builds; CMake defaults to the console subsystem, making it redundant. This also resolved the `SDL3::SDL3main` target-not-found error produced by the Visual Studio generator when building against FetchContent SDL3.

`onelua.c` added to the Lua exclusion list in `CMakeLists.txt`. The Lua repository includes this single-file amalgamation which re-includes `lua.c`, causing a duplicate `main` symbol at link time. Excluding it alongside `lua.c` and `luac.c` resolves the error.

### Open items

- `m_` member prefix convention: carried forward from Layer 0, still unaddressed in DEVELOPMENT_PRACTICES. Confirm before Layer 2 adds more types.
- `scripts/init.lua` `config` table not yet wired to `sim_loop` constructor. Still uses hardcoded defaults.
- `unit_component.owner` is `null_entity` — the player corporation entity is not yet defined. Needs a home before Layer 5 (budget) assigns revenue to a faction.

---

## 2026-06-13 — Layer 0: Engine scaffolding

**Status:** Complete. Layer 1 data model begun by end of session.

### What was built

- `CMakeLists.txt` — FetchContent build for SDL3 (`release-3.2.0`), Lua 5.4 (`v5.4.7`), sol2 (`v3.3.0`, header-only), Dear ImGui (`v1.91.6` with SDL3 + SDLRenderer3 backends).
- `src/core/sim_loop` — Fixed-timestep loop at 20 Hz using an SDL `GetTicks` accumulator. Economy tick fires every N sim steps (default: 20, i.e. 1 Hz). Spiral-of-death clamp at 8 steps.
- `src/core/app` — SDL3 window and renderer, ImGui initialised, render loop calling `sim_loop::tick()` each frame.
- `src/scripting/lua_state` — sol2 wrapper; `safe_script_file` used for all file loads per TECH_FOUNDATIONS constraint on unprotected sol2 calls.
- `scripts/init.lua` — Loaded at startup; prints confirmation and defines a `config` table for future use.
- `.gitignore` — Covers build output, CMake artifacts, IDE files, compiled binaries.
- Engine Status ImGui panel — displays live sim tick and econ tick counters to confirm both loops are running.

### In-session decisions

**sol2 integrated as header-only, bypassing its CMake.**
sol2's own `CMakeLists.txt` runs `find_package(Lua)` which conflicts with our FetchContent-built Lua. Using `FetchContent_Populate` and manually adding `${sol2_SOURCE_DIR}/include` to the game target's include dirs avoids the conflict with no functional loss — sol2 is header-only regardless.

**SDL3 linked as shared; DLLs copied post-build on Windows.**
Static SDL3 introduces platform library dependencies (user32, gdi32, etc.) that SDL's CMake handles correctly but which complicate the link on MSVC. Shared + post-build DLL copy via `$<TARGET_RUNTIME_DLLS:ProjectIo>` is the simpler default. Revisit if distribution packaging becomes a concern.

**`SDL3::SDL3main` removed from link.**
The SDL_main redirection was unnecessary for this configuration; removing it resolved a linker issue without changing behaviour.

**`onelua.c` excluded from Lua build.**
The Lua repo includes `onelua.c`, a single-file amalgamation that re-includes `lua.c`. Excluding it alongside `lua.c` and `luac.c` prevents duplicate symbol errors.

**`max_catchup_steps = 8` for accumulator clamp.**
Chosen to allow the sim to catch up after a ~400 ms hitch at 20 Hz without stalling. No empirical basis yet — revisit if the sim loop becomes expensive enough to make 8 steps a meaningful cost.

**`window_w` / `window_h` as compile-time constants in `app.cpp`.**
Not exposed to Lua or config yet. Sufficient for the prototype; move to a config table in `init.lua` if window size needs to vary.

### Corrections made during session

Naming convention violations caught in review: all type names, function names, member variables, and filenames were PascalCase or camelCase on first write. Corrected to `snake_case` throughout per DEVELOPMENT_PRACTICES. Files renamed on disk (two-step rename required for `App` → `app` on Windows NTFS).

Documentation style: public interfaces initially used `//` comments. Corrected to `///` Doxygen with `@param` / `@return` throughout.

### Open items

- Member variable prefix (`m_`) is used throughout but not addressed in DEVELOPMENT_PRACTICES. Confirm whether to keep it or drop it before Layer 1 adds more types.
- `scripts/init.lua` defines a `config` table with `sim_hz` and `econ_per_second`. These are not yet read back by `sim_loop` — the constructor uses hardcoded defaults. Wire this up when the Lua/C++ boundary is exercised further.
