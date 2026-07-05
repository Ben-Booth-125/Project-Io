# Project Io — Development Practices

This document defines the coding standards, documentation conventions, and testing approach for Project Io. Apply these consistently across all code written or reviewed. Where code deviates from these standards, note it and suggest a correction — but do not refuse to proceed or treat it as a blocker.

---

## Testing

Tests are **headless C++ harnesses**, not a unit-test framework. Each `tools/verify/<name>.cpp` is a standalone program that exercises a slice of the SDL/Lua/ImGui-free `world/*` logic and prints `PASS`/`FAIL` lines through its own small `check()`-style assertions, exiting non-zero on any failure. They are built and run by CMake/CTest (`ctest --test-dir build --output-on-failure`, or `check.bat`) and by the CI headless loop (`.github/workflows/build.yml`), which globs every `tools/verify/*.cpp` so a new harness is picked up automatically. *(Catch2 was evaluated as the unit-test framework but **not adopted** — the printf-assert harness pattern needs no dependency and mirrors how the code is actually structured. The `verifier-headless` skill runs a harness on demand.)*

Tests are written alongside the layer they cover, not deferred to the end. Each milestone in `ROADMAP.md` should have a harness for its core logic before the next begins.

### What to test

Focus on the simulation's pure, deterministic logic — the parts that transform state — in `world/*` (no SDL/Lua/ImGui):

- Price resolution given a supply/demand ratio (`econ_harness`, `market_clearing`)
- Extraction yield / workforce-efficiency curves (`workforce_harness`)
- Convoy progress + persistent trade-route recording at the tick boundary (`trade_routes_harness`)
- Budget arithmetic, debt, and bankruptcy (`econ_bankruptcy`, `econ_stability`)
- Generation audits — tile/nation/corp placement legality and world determinism (`world_audit`, `corp_terrain_matrix`, `determinism_harness`)

Do not test rendering, ImGui panels, or SDL3 platform behaviour in a harness — those are not unit-testable in a meaningful way and are validated by observation and by the **visual verification** tier below.

### Authoring a harness

- One `tools/verify/<name>.cpp` per behaviour; `#include` the `world/*` headers it needs and build a small fixture (usually `make_hard_coded_world()` or a hand-built registry).
- Print one `PASS`/`FAIL`/`WARN` line per assertion, naming what failed, and `return` non-zero on any hard failure so CTest and the CI loop detect it.
- Keep it free of SDL/Lua/ImGui so it links against the `world/*` superset (`recipe_registry.cpp` excepted — it pulls sol2/Lua). CMake's `foreach` over `tools/verify/*.cpp` wires each one into `ctest` automatically; name a genuinely new *check class* in the `verifier-headless` skill so it stays discoverable.

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
- **Output:** PNGs for inspection **and automatic pass/fail** — golden-image diffing is **built and
  shipped** (BL `golden-image-diff`), no longer deferred. Each capture is diffed against its committed
  reference at `scripts/verify/golden/<name>.png` (note: singular `golden/`); the harness logs
  `Golden PASS/FAIL <name>: <pct>% differing` and exits non-zero on any fail. Re-bless a golden
  deliberately with `ProjectIo --verify <script> --bless` (overwrites it — a reviewable diff in
  version control). The tolerance model below is the **shipped** behaviour.

#### Golden-image diffing — tolerance model (designed 2026-06-15, [F3]; build deferred)

When automatic pass/fail is built, it diffs each freshly-captured `screenshots/<name>.png` against
a committed reference. The settled model:

- **Golden storage.** Reference images live beside their script under
  `scripts/verify/goldens/<name>.png`, committed to the repo. A `--update-goldens` flag on the
  verify harness rewrites them deliberately (never silently), so a golden change is a reviewable
  diff in version control.
- **Tolerance (perceptual, not exact).** Exact-match is too brittle — anti-aliasing and font
  rasterisation jitter by a pixel. The model is a **two-threshold budget**: a per-pixel RGB delta
  ignored below a small magnitude (`ε_channel`), and a cap on the **fraction of pixels** allowed
  to exceed it (`ε_area`). A frame passes when the differing-pixel fraction stays under `ε_area`.
- **Masked regions.** Zones known to jitter (text glyphs, sub-pixel-animated markers — see the
  body-label stepping note in OPENS § Known Bug) may carry an optional **ignore mask** per golden,
  excluded from the budget, so legitimate text-AA noise never fails a structural check.
- **CI decision.** It runs as a **local pre-commit / on-demand** check first, not gated in CI —
  there is no rendering-capable CI runner today. Promotion into CI is a later call once a headless
  GPU/software-raster runner exists; the harness is designed to run identically in both.

#### Golden staleness — shared chrome regresses every capture (observed 2026-07-05)

A golden captures the **whole 1280×720 window**, so it regresses when *shared chrome* changes — the
profile card, header, or minimap toolbar — **even if the feature under test is untouched**. So a
change that edits shell chrome staleens **every** golden that shows it, at once. A full sweep on
2026-07-05 found this concretely: **9 pass / 66 fail / 55 no-golden** across `scripts/verify/*.lua`.
The 9 passes were exactly the goldens blessed *after* the v0.0.9 chrome cluster (BL-070 system menu,
BL-080 corp name, BL-085 presence, BL-090 emblem, BL-093 lens-strip relocation), at ≤0.5%; the 66
failures were pre-cluster goldens whose **only** delta is that chrome (confirmed by eye:
`survey_planetary_masked` differs *only* in the chrome — the surveyed surface is pixel-identical).
This is not a cross-platform or capture-timing artefact: if it were, the fresh goldens would fail too.
Two disciplines follow:

- **Re-bless dependent goldens as part of the chrome change** (DELIVERY step 5), not later — a stale
  golden hides real regressions behind chrome noise, which is what "66 red" was masking.
- **The canonical baseline platform is unresolved** — Linux (per memory
  `cross-platform-golden-mismatch`) vs. this Windows box, where fresh goldens pass ≤0.5%, making
  Windows-blessing self-consistent. Decide before any bulk re-bless; do **not** blanket `--bless`
  until then.

The **55 no-golden** captures are a second gap: they render frames but have no committed reference, so
they enforce nothing — candidates to bless or retire.

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

**A formal Q&A is itself the review — no transient `⟳` note needed.** The `> ⟳` "pending review"
blockquote exists to flag a design call the user has *not yet* seen (the Batch Publish
documentation discipline; see OPENS § Publish). When a design call is settled **through a formal
Q&A with the user** — the user chose the direction live — that review has already happened, so the
resulting doc change **does not carry a `⟳` note**. Write the settled design directly. Reserve
`⟳` for calls made on the user's behalf that still await their eyes.

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

## Display environment

The runtime display and the verification harness do **not** render at the same size, so UI chrome
must be **resolution-robust** — sized from content and fixed anchors, never pinned to a
resolution-scaled value.

- The window opens at **1280×720** (`window_w`/`window_h` in `src/core/app.cpp`,
  `SDL_WINDOW_RESIZABLE`) and persists its size to **`options.cfg`** in the run directory
  (`app::load_settings`/`save_settings` — keys `window_w`/`window_h`/`fullscreen`/`vsync`).
- The **`--verify` capture harness renders at 1280×720**; the interactive window can be larger.
  The dev machine's desktop is **1920×1080 @ 60Hz, content-scale 1.0** (see the `display-environment`
  memory). The app logs its runtime display on startup: `Display: window WxH, desktop WxH @ ..Hz,
  content-scale ..` (`SDL_GetDesktopDisplayMode`).
- **Anti-pattern (BL-093):** the Selection element once pinned its height to the minimap height
  `mm_h` (`mm_w = max(240, 0.20·min(disp.x,disp.y))`), so it ballooned with empty space / clipped at
  other resolutions. The fix was to size it to its content in **text-line units** per selection kind.
  When adding chrome, size from `GetTextLineHeightWithSpacing()`/content and anchor to a shell edge;
  do not derive a panel's extent from another element's resolution-scaled size.

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

### Merge gate — what actually guards `main`

Recorded 2026-07-05 (BL-105). `main` is **not** protected by GitHub branch-protection rules: the
repository is private on a free plan, where the branch-protection API returns HTTP 403
(*"Upgrade to GitHub Pro or make this repository public to enable this feature"*). There is
therefore **no enforced required-check gate** — a push to `main` lands regardless of CI, and the
Cut process merges the working branch into `main` locally (step 2) and pushes directly, never
through a PR.

The gate is thus **procedural, not enforced**: CI (`.github/workflows/build.yml` — Linux
g++-13/g++-14 + the Windows build + the headless-harness tier) runs on every push as the signal,
and the **pre-Cut local build-green** (step 1 above) is the human gate before a release commit
lands. Treat a red CI run on `main` as stop-and-fix. To make the gate *enforced* later, either
make the repo public (branch protection is free for public repos) or move to Pro, then require the
`linux (g++-13)`, `linux (g++-14)`, and `windows` checks and route Cuts through a PR so the checks
apply to the release commits.

## Lua files

Data definition files in Lua follow the same spirit as the C++ standards:

- `snake_case` for all keys and function names
- Each file covers one domain (one building type catalogue, one resource table)
- A header comment states what the file defines and what system consumes it