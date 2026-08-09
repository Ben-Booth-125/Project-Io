# Project Io

Project Io is a near-future, space-based 4X grand strategy game. You control a **corporation**
— not a nation — competing across an Earth-like solar system through resource extraction, trade,
and military conflict. Unlike a nation-state, a corporation begins with no territory and no
standing force: every asset must justify itself through economic or strategic return, and the
corporation persists only as long as it holds at least one asset.

The game progresses through **Eras** — beginning on the home planet (Era 0, Terrestrial) and
opening up the solar system once the gate to Era 1 (Early Space) is met. Play is structured
around three nested **canvases** (Solar → Circumplanetary → Planetary) and a tick-based economy
where supply, demand, and prices resolve per body.

The project is in **prototype phase**, solo-developed in C++ with Lua scripting. Design and
technical documentation lives under [`docs/`](docs/) — start with
[`docs/CONCEPT.md`](docs/CONCEPT.md) and [`docs/SYSTEMS.md`](docs/SYSTEMS.md).

## Building & running

The game is a single CMake project. Its C++ dependencies — **SDL3**, **Lua 5.4**, **sol2**, and
**Dear ImGui** — are fetched and built automatically by CMake's `FetchContent` on the first
configure, so there is nothing to install by hand beyond a toolchain, CMake, and (on Linux) the
system libraries SDL3 needs.

### Prerequisites

- **CMake ≥ 3.25** and a **C++20** compiler (GCC 13+, Clang, or MSVC / Visual Studio Build Tools).
- A network connection for the first configure (dependencies are downloaded then).
- **Linux only** — the development headers SDL3 links against:

  ```sh
  sudo apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
    libxinerama-dev libxss-dev \
    libwayland-dev wayland-protocols libxkbcommon-dev \
    libegl1-mesa-dev libgl1-mesa-dev \
    libpulse-dev libasound2-dev
  ```

### Configure & build

From the repository root:

```sh
# Configure (downloads and builds dependencies on the first run)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j
```

On **Windows** with the Visual Studio generator, pass the config at build time instead:

```sh
cmake -B build
cmake --build build --config Release
```

The build copies the `scripts/` and `assets/` directories next to the executable, so the binary
is self-contained once built.

### Run

Launch the game from the repository root:

```sh
# Linux / macOS
./build/ProjectIo

# Windows (Visual Studio generator places the exe under the config directory)
build\Release\ProjectIo.exe
```

### Headless verification harnesses (optional)

The SDL/Lua/ImGui-free logic harnesses under `tools/verify/` are built as extra CMake targets and
exercise the pure simulation code (economy arithmetic, tile generation, placement audits):

```sh
cmake --build build --target econ_bankruptcy survey_harness visibility_harness workforce_harness
./build/econ_bankruptcy
```

There is also a headless **visual-verify** mode that renders a canvas script offscreen and compares
it against a committed golden image:

```sh
./build/ProjectIo --verify scripts/verify/corporation_lens.lua
```

## Latest releases — v0.1.1, v0.1.2 and v0.1.8

**v0.1.8 — build health: the gate stops lying.** No player-facing content. `ctest` had been
reporting **ten failures of which exactly one was a real failing assertion**; the rest were
harnesses exceeding a bound they were never sized against, open-ended research sweeps that never
finish, and wall-clock benchmarks failing under machine load. Three test tiers plus `sweep` and
`bench` labels fix that. Also: `next_id.js` stopped silently defending nothing (it was issuing
backlog ids 25 below the true ceiling — the account of how eight items each landed twice), the
GCC goldens were re-blessed, and a seeded FetchContent cache lets fresh worktrees configure
offline.


**v0.1.1 — the word interface.** An agent can read the world state (blackboard export), look up
what every one of the 115 controls *means* (the action dictionary, transcribed from the command
seam rather than authored beside it), and drive the game over a socket — `ProjectIo --serve` plus
`tools/mcp/`. No HTTP client, no API key, no cloud dependency in the engine. Ships with the sticky
detail-card family, the corporation dashboard, commercial-activity fog, and the radial tech-tree
viewer. The write leg is deliberately partial — standing sell orders are not yet issuable by
command (BL-293).

**v0.1.2 — buildings rework: remoteness stops being free.** Placement now enforces a **logistical maximum
range** from a supply anchor, so siting is a trade-off rather than a lookup of the richest tile on
the map; **build time scales with the site** (landform, distance from an anchor, established
stack); and **construction reads as a process** on the canvas with its remaining time legible. Also
widens extraction from 4 to 15 targets, groups the construction ledger, and warns that a build will
starve *before* you commit. The processing half of the roster is deliberately out of scope
(BL-340). Full history in [`CHANGELOG.md`](CHANGELOG.md).

## Previous release — v0.1.0

**The prototype cut.** The economy loop, validated and playable end-to-end: construction,
population-grounded workforce, spatial price divergence via supply convoys, gated discovery
(survey + competitor-intelligence scoping), a legible budget, and a green, performance- and
data-growth-audited build. Adds the generated **road & logistics lattice**, the **continent lens**
and Continents/Drift generation pass, **terrain landform render** with graded terrain combat
costs, the **tile construction ledger**, and three new quality-audit instruments (frame budget,
econ-tick scaling, data-creep). Everything past this cut — laws, techs, military, politics, the AI
opponent — is the v0.1.x → v0.3.0 arc, not this release. Full history in
[`CHANGELOG.md`](CHANGELOG.md).

## Versioning & releases

Version history is tracked with annotated git tags (`vX.Y.Z`); each tag is a recoverable
release point and `CHANGELOG.md` carries its summary. Releases are produced by the **Cut**
process — see [`docs/development/DEVELOPMENT_PRACTICES.md`](docs/development/DEVELOPMENT_PRACTICES.md)
§ Cutting a release.
