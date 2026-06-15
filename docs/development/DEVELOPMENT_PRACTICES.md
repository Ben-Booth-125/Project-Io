# Project Io — Development Practices

This document defines the coding standards, documentation conventions, and testing approach for Project Io. Apply these consistently across all code written or reviewed. Where code deviates from these standards, note it and suggest a correction — but do not refuse to proceed or treat it as a blocker.

---

## Testing

The project uses **Catch2** for unit testing.

Tests are written alongside the layer they cover, not deferred to the end. Each milestone in `development/ROADMAP.md` should have tests for its core logic before the next begins.

### What to test

Focus tests on the simulation's pure logic — the parts that transform state deterministically:

- Price resolution given a supply/demand ratio
- Extraction yield given tile properties and workforce scalar
- Convoy progress accumulation and completion evaluation at tick boundary
- Budget arithmetic (revenue, outgoings, running balance)

Do not test rendering, ImGui panels, or SDL3 platform behaviour. These are not unit-testable in a meaningful way and should be validated by observation.

### How Claude handles tests

Claude will **suggest tests** when writing or reviewing logic, but will not write test files unprompted. When suggesting tests, Claude names the cases worth covering and explains why — the developer writes them. If asked to write a test directly, Claude will do so using Catch2 syntax.

### Catch2 conventions

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("market price resolves from supply/demand ratio", "[market]") {
    SECTION("excess supply drives price below base") { ... }
    SECTION("excess demand drives price above base") { ... }
}
```

- Tag each test with the system it covers in square brackets: `[market]`, `[extraction]`, `[supply]`, `[budget]`
- One `TEST_CASE` per logical behaviour; use `SECTION` for variations
- Test names are plain English descriptions of what the code should do, not how it does it

### Visual verification (rendering / lenses)

Rendering is *not* unit-tested (see above) but it **is** verifiable by observation,
and that observation is automated. The harness runs the real app in a headless,
deterministic capture mode and writes PNGs that a human — or Claude, via the Read
tool — inspects against the requirement.

```
ProjectIo --verify scripts/verify/<name>.lua
```

- **Deterministic:** fixed window size, seeded world (`make_hard_coded_world`), sim
  paused — so a captured frame is reproducible.
- **Driver:** the script drives view and overlay state through the `verify` Lua API
  (`goto_surface`, `set_overlay`, `set_zoom`, `set_pan`, `add_pan`,
  `center_tile(col,row[,zoom])`, `command(name)`, `capture`, `buildings`,
  `log_buildings`) — direct state manipulation, no synthetic input. `command(name)`
  uses the shared canvas command vocabulary (`canvas_command`) that also backs the
  player keyboard bindings (CANVASES.md § Keyboard), so a script reads as a key
  sequence; `center_tile` centres a Planetary tile using the canvas's own transform,
  so scripts no longer hand-compute pan.
- **Helpers:** `scripts/verify/lib.lua` is auto-loaded by the harness from the
  script's directory before the script runs (no `require` — the `package` lib is not
  opened). It provides high-level helpers — `sweep_overlays(prefix)`,
  `tour_buildings(zoom)`, `frame_tile(col,row,zoom)` — so a new check is "call a
  helper" rather than a from-scratch script. Prefer them over raw `set_pan` math.
- **Capture:** `capture("name")` renders one frame and writes `screenshots/name.png`
  (in-app `SDL_RenderReadPixels` → `write_png_rgba`; nothing leaves the window).
- **Output:** PNGs for inspection. Golden-image diffing is not yet built (deferred).

This is the standard tool for the `visual` verification class in
[`req/REQUIREMENTS.md`](req/REQUIREMENTS.md). When a requirement's verification is
`visual` and no other tool fits, author (or extend) a `scripts/verify/*.lua` script
and capture the frames rather than deferring to a manual human check. A proven check
can be promoted to the **`verifier-visual` skill** (`.claude/skills/verifier-visual/`),
which wraps `ProjectIo --verify <script>` so re-running it is a single invocation.
Entry points: `app::run_verify` (`src/core/app.cpp`), `write_png_rgba`
(`src/core/png_writer.cpp`), `ui::apply_canvas_command` (`src/ui/canvas_command.hpp`).

### Headless logic verification (pure simulation / generation)

The `world/*` translation units depend only on the standard library (no SDL / ImGui /
Lua), so pure logic — economy arithmetic, tile generation, placement audits — is
verified by a small headless C++ harness compiled against just those units, with no
GUI. Harnesses live in `tools/verify/*.cpp` (outside `src/`, so the CMake
`GLOB_RECURSE` does not pull them into the real build) and print `PASS`/`FAIL`
assertions; their build lines are in `tools/verify/README.md`.

This is the standard tool for the `headless` verification class. Author (or extend) a
`tools/verify/*.cpp` harness and run it through the **`verifier-headless` skill**
(`.claude/skills/verifier-headless/`). Keep `recipe_registry.hpp` pure data (sol2 only
in its `.cpp`) so the economy logic stays harness-buildable. See memory
`reference-headless-build` for the toolchain. Tool creation here follows the
*tool-creation-is-skill-creation* workflow in CLAUDE.md § Skills.

---

## Code style

### Naming

`snake_case` throughout — types, functions, variables, files, and namespaces.

Private member variables use an `m_` prefix: `m_window`, `m_sim_loop`. Applied consistently to all class members.

```cpp
// Correct
struct market_state { ... };
float resolve_price(float supply, float demand, float base_price);
int workforce_assigned;

// Incorrect
struct MarketState { ... };
float resolvePrice(float supply, float demand, float basePrice);
int workforceAssigned;
```

### General conventions

- Prefer explicit types over `auto` where the type is not immediately obvious from context
- Keep functions short and single-purpose; if a function needs a comment to explain what it does (not why), consider splitting it
- No magic numbers — name constants, even simple ones
- Use `const` by default; only drop it when mutation is required

### File structure

- One type or closely related group of types per header
- Implementation in a matching `.cpp` file
- Headers use `#pragma once`

---

## Documentation

All public interfaces are documented with **Doxygen-style comments**. This applies to structs, public member functions, and any free function callable from outside its translation unit.

```cpp
/// Resolves the market price for a good at the economy tick boundary.
///
/// Price is derived from the ratio of local supply to demand, scaled
/// against the global rarity base. A ratio above 1.0 (oversupply) drives
/// price down; below 1.0 (undersupply) drives it up.
///
/// @param supply     Current local supply quantity.
/// @param demand     Current local demand quantity.
/// @param base_price Global rarity-derived price floor.
/// @return           Resolved price for this tick.
float resolve_price(float supply, float demand, float base_price);
```

### Design-direction Q&A (Batch Publish)

A **Batch Publish** (multiple Briefs in one block; see GLOSSARY) that made non-trivial or
ambiguous design calls **closes by raising a short design-direction Q&A** — the open questions
and the calls made on the user's behalf — and **records the outcome in the DEVLOG** with the
session. No dedicated log file: DEVLOG is the home, the way the 2026-06-14 Layer 3 Q&A was kept.

**Rationale:** code lands faster than design intent is pinned, so a batch quietly makes calls
(what shape a generated thing takes, which of two mechanics wins). Surfacing them at the close
catches a wrong call while the context is fresh, before it ossifies into "documented".

**Keep it proportional.** This is a guideline, not ceremony — skip the Q&A for a batch that
surfaced nothing worth asking, and keep the questions few. The point is to catch the one
genuine fork, not to manufacture questions.

### Inline comments

Use inline comments to explain **why**, not what. If the code requires a comment to explain what it is doing, rewrite the code first.

```cpp
// Correct — explains a non-obvious decision
// Progress is clamped to 1.0 here rather than at completion evaluation
// to keep the render interpolation from overshooting on a slow tick.
progress = std::min(progress + delta, 1.0f);

// Incorrect — restates the code
// Add delta to progress
progress += delta;
```

ImGui panel code is an exception — brief section comments are encouraged there since panels serve as the functional specification for the production UI and will be read as reference material.

### ImGui panels — one per milestone, as it is built

Wire an ImGui panel alongside each milestone as it is built, not at the end. Each panel needs
only to make that milestone's state observable — a tile inspector, a market price readout, a
convoy list, a budget line. These panels are debugging tools *and* the functional
specification for the production UI. Write ImGui code clearly, not cleverly — it is reference
material as much as working code.

---

## Development constraints

Standing prohibitions for the prototype, migrated from the retired build-sequence doc. The
full scope and its exclusions are owned by [`../tech/TECH_FOUNDATIONS.md`](../tech/TECH_FOUNDATIONS.md);
these are the recurring "do not" rules that come up while building:

- Do not suggest or implement anything outside the prototype scope in TECH_FOUNDATIONS.
- Do not design or implement a milestone that depends on an earlier one not yet complete. If
  asked to, flag it (see `ROADMAP.md` for the sequence).
- Do not expose individual tile data to Lua.
- Do not use unprotected sol2 calls where errors can occur.
- Do not add SQLite — flat binary serialisation is correct for now.
- Do not build AI faction behaviour beyond the data-model minimum stub.
- Do not introduce a retained-mode UI framework in place of ImGui for the prototype.

---

## Tone and approach

- Every system should justify its existence by feeding into **Trade** or **Conflict**. Favour
  solutions that are legible and composable over solutions that are locally clever but opaque.
- When the right approach is uncertain, state the uncertainty and present options with
  trade-offs rather than picking one silently. Stay the advisor — the developer makes the calls.

---

## Cutting a release

A **Cut** finalises a version. Replaces the old versioned-backup scheme: the authoritative
record is now an annotated git tag (`vX.Y.Z`), recoverable forever with `git checkout vX.Y.Z`;
the local `backups/vX.Y.Z/` snapshot is kept only as a convenience and is gitignored.

Work starts on a `feature/*` branch. To cut version `vX.Y.Z`:

1. **Finalize** — build green; fill the `[Unreleased]` section of `CHANGELOG.md`; pick the version.
2. **Merge** the working branch into `main` locally.
3. **Backup** — copy `src/` to `backups/vX.Y.Z/` (local-only, gitignored — belt-and-braces).
4. **Stamp** — move `CHANGELOG.md`'s `[Unreleased]` entries under `## [vX.Y.Z] — <date>`, refresh
   its compare links, and update the README "Latest release" summary.
5. **Commit** the stamp on `main`.
6. **Tag** — `git tag -a vX.Y.Z -m "<one-line summary>"` on that commit. *This is the version history.*
7. **Push** — `git push origin main --follow-tags` (pushes the commit and its tag together).
8. **(optional)** `gh release create vX.Y.Z --notes-file -` with the changelog section for a
   browsable GitHub release page.

Tags are the source of truth — no `previous`/`stable` branch is maintained. Every released
version is reachable by its tag, not just the most recent one.

## Lua files

Data definition files in Lua follow the same spirit as the C++ standards:

- `snake_case` for all keys and function names
- Each file covers one domain (one building type catalogue, one resource table)
- A header comment states what the file defines and what system consumes it