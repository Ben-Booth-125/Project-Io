# Project Io — Development Practices

This document defines the coding standards, documentation conventions, and testing approach for Project Io. Apply these consistently across all code written or reviewed. Where code deviates from these standards, note it and suggest a correction — but do not refuse to proceed or treat it as a blocker. The note rides along with the work.

---

## Testing

Tests are **headless C++ harnesses**, not a unit-test framework. Each `tools/verify/<name>.cpp` is a standalone program that exercises a slice of the SDL/Lua/ImGui-free `world/*` logic and prints `PASS`/`FAIL` lines through its own small `check()`-style assertions, exiting non-zero on any failure. They are built and run by CMake/CTest (`ctest --test-dir build --output-on-failure`, or `check.bat`), which globs every `tools/verify/*.cpp` so a new harness is picked up automatically. CTest is the *only* runner — there is no CI (see § Merge gate). Since BL-287 the world layer builds once as the `io_world_obj` OBJECT library rather than once per harness, so adding a harness is now near-free. *(Catch2 was evaluated as the unit-test framework but **not adopted** — the printf-assert harness pattern needs no dependency and mirrors how the code is actually structured. The `verifier-headless` skill runs a harness on demand.)*

Tests are written alongside the layer they cover, not deferred to the end — alongside, the check catches the quietly-wrong while the layer is still fresh in the head. Each milestone in `ROADMAP.md` should have a harness for its core logic before the next begins.

### Measuring a generated system

Five rules, each earned by getting it wrong first (the earth-like battery, 2026-08-04). They apply
to anything with tuned parameters and an emergent output — generation, economy, diplomacy, combat.

**Measure the OFF state, always.** A new feature emits plausible numbers immediately, and plausible
numbers look like success. Ore provinces reported 15.8% concentration and appeared to work; the
baseline was 15.7%, so the effect was nil. Twice in one day a feature looked like it worked and did
nothing, and only an explicit with/without comparison caught either. Budget for the baseline run.

**Never assert a conservation property you have not summed.** "This only redistributes X" is a
claim, not a comment. The province field was documented as conserving a world's ore while quietly
losing 47% of its petroleum — the normalisation was over the wrong set, which reads as correct until
someone adds up both sides.

**One-at-a-time sweeps cannot see interaction, so measure pairs before trusting them.** A per-knob
corridor sweep pronounced every parameter individually safe; a knob × knob atlas then found a
28.2-point interaction, because two knobs fed the same two-sided band and compounded. Where a system
is *inherently* joint — relations between parties, unit matchups — the joint instrument is day-one
work, not a contingency.

**Watch for quantities that cancel.** A parameter can be provably inert without anyone noticing.
Stellar mass changed nothing because the derived orbit cancelled it exactly: two individually-correct
decisions that annihilated each other. Any value normalised against something derived from itself is
a candidate, and a lean-to-outcome trace (signal against seed noise) is how it gets caught.

**A guard that never fires is not a guard.** Ten of the fourteen homeworld-floor clauses never fire,
because the sampling bands were tuned to sit inside them — the floor reads as the specification and
the bands actually are it. Check that every constraint you write can still trigger.

**Then ask for the interesting instance, not the median.** Distributions are for calibration; a
player experiences one campaign. The most valuable instrument in the battery was the last one, which
stopped asking "what does the median world look like?" and started asking "show me one worth
playing" — and immediately found both named exemplar seeds and a real defect (rivers never reached
the inland sea) that every median-based harness had missed.

### What to test

Focus on the simulation's pure, deterministic logic — the parts that transform state — in `world/*` (no SDL/Lua/ImGui):

*(The categories below are a legend, not a roster — **run `ls tools/verify/` for the real list**,
46 harnesses as of 2026-08-04. A pasted count has now gone stale twice; do not paste a third.
Most link `io_world_obj` and nothing else; three are hand-declared with extra deps —
`pregame_balance` and `persona_counsel` need Lua, `font_glyph` needs ImGui. See
`tools/verify/README.md`.)*

- Economy — price resolution, budget arithmetic, debt, stability, workforce curves, pre-game
  balance (`econ_harness`, `econ_bankruptcy`, `econ_stability`, `workforce_harness`,
  `pregame_balance_harness`)
- Construction — placement gates, build flow, spree behaviour (`construction_harness`,
  `construction_gate_harness`, `build_spree_harness`)
- Supply & trade — convoy advance, persistent routes, logistics, roads (`supply_advance`,
  `trade_routes_harness`, `logistics_harness`, `road_generation_harness`)
- Discovery — survey, visibility rule, commercial fog (`survey_harness`, `visibility_harness`,
  `commercial_fog_harness`)
- Generation — world/placement audits, continents, planetology chain, chemistry tables, history
  ladder, population MVP (`world_audit`, `corp_terrain_matrix`, `continents_harness`,
  `planetology_harness`, `planetology_sweep`, `chemistry_tables_harness`,
  `history_ladder_harness`, `population_mvp`)
- Determinism (`determinism_harness`, `world_determinism`)
- Corp AI — agency triggers, scored utility, blackboard export, persona counsel
  (`corp_agency_harness`, `corp_ai_harness`, `blackboard_harness`, `persona_counsel_harness`)

Do not test rendering, ImGui panels, or SDL3 platform behaviour in a harness — those are not unit-testable in a meaningful way and are validated by observation and by the **visual verification** tier below.

### Authoring a harness

- One `tools/verify/<name>.cpp` per behaviour; `#include` the `world/*` headers it needs and build a small fixture (usually `make_hard_coded_world()` or a hand-built registry).
- Print one `PASS`/`FAIL`/`WARN` line per assertion, naming what failed, and `return` non-zero on any hard failure so CTest detects it.
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
- **Settle before you capture — use `shot(name)`, not `verify.capture(name)`.**
  `verify.capture` composites **exactly one** frame, while ImGui settles auto-layout
  over the following frame or two: a child's content region, a table's column widths
  and a fresh window's scroll state are all *provisional* on the frame they first
  appear. So a capture taken immediately after a window opens, a ledger view switches,
  or a layout changes records a **half-laid-out frame**. `shot(name, frames)`
  (`scripts/verify/lib.lua`, auto-loaded) renders settle frames first.
  The trap is that the unsettled frame is **deterministic**: it blesses cleanly and
  re-passes at 0.0000% forever, so a stable golden of the *wrong picture* is never
  flagged by anything. Found 2026-08-01 (Ben) — four of ten BL-214 fold captures moved
  once settled. A bare `verify.capture` is still correct for a capture taken with no
  preceding state change.
- **Driver:** the script drives view and overlay state through the `verify` Lua API
  (`goto_surface`, `set_overlay`, `set_zoom`, `set_pan`, `add_pan`,
  `center_tile(col,row[,zoom])`, `command(name)`, `capture`, `buildings`,
  `log_buildings`) — direct state manipulation. `command(name)`
  uses the shared canvas command vocabulary (`canvas_command`) that also backs the
  player keyboard bindings (CANVASES.md § Keyboard), so a script reads as a key
  sequence; `center_tile` centres a Planetary tile using the canvas's own transform,
  so scripts no longer hand-compute pan.
- **Click injection — a script can PRESS, not only stage (BL-521, 2026-08-21).** Until this
  landed the API could only *write* `ui_state`, so a check could arrive at a selection but never
  perform the gesture that produces one. The consequence was structural, not occasional: every
  requirement phrased "click X and see Y" shipped with its live half **owed to a human by
  construction** (NR-416, NR-424, both on BL-511 alone). Five bindings close it, and all of them
  drive the **real** path — ImGui's own event queue, the canvas's own hit-test, the canvas's own
  click handler:

  | Call | Does |
  |---|---|
  | `verify.click(x, y [, button])` | Press + release at a screen position. Button: `"left"` (default) / `"right"` / `"middle"`. |
  | `verify.double_click(x, y [, button])` | Two press/release pairs ImGui is *forced* to read as one double-click — the descend gesture on the Solar and Circumplanetary canvases. |
  | `verify.click_tile(col, row [, button])` | Click Planetary tile (col,row). Centres it first (so it **pans**), then presses its centre. Returns false if the Planetary canvas is not the primary rung. |
  | `verify.hover(x, y [, frames])` | Move the synthetic cursor and dwell `frames` frames there. Unlike `verify.mouse`, this moves **ImGui's** cursor too, so panels and buttons hover as well as canvases. |
  | `verify.tile_screen(col, row)` | `{ok, x, y}` — the screen point that *is* that tile, asked of the canvas rather than re-derived in Lua. Also pans. |
  | `verify.pointer_target()` | The assertion half: `{hovered_province, selected_province, has_selection, hover_card, hover_card_stuck, x, y}` — ids and booleans only. |

  Each call renders its own frames and returns with the gesture complete, so a following
  `capture`/`shot` shows the result. `dwell_on_tile(col, row [, frames])` in `lib.lua` pairs
  `tile_screen` with `hover`.
  - **Determinism is by frame count, never by clock.** The one place ImGui itself consults a
    clock is double-click promotion (`g.Time` vs `MouseDoubleClickTime`), which would have made
    "two singles or one double" depend on how long two Debug frames took to render;
    `app::inject_pointer` forces that window per press instead (0 can never chain, `FLT_MAX`
    always does), so the outcome is fixed by the script, not by the machine.
  - **A move settles one frame before the press.** `WantCaptureMouse` and the canvas's
    `hovered_tile` both resolve from the position ImGui saw *last* frame, so a press in the same
    frame as the move would hit-test where the cursor used to be.
  - **The shortcuts stay.** `select_tile` / `select_province` / `clear_selection` are cheaper
    (no pan) and remain right for *staging* a selection. Use the click where the check is about
    the **gesture** — and `click_injection.lua` (the BL-521 acceptance script) asserts the two
    agree, so a divergence between the shortcut and the real handler is now caught rather than
    assumed away.
  - **Still direct state, not a screen recording.** Nothing here replays OS input; the harness
    posts events into ImGui's queue between the backend's `NewFrame` and `ImGui::NewFrame`, and
    a no-op guard keeps interactive play and every pre-BL-521 script byte-identical.
- **Helpers:** `scripts/verify/lib.lua` is auto-loaded by the harness from the
  script's directory before the script runs (no `require` — the `package` lib is not
  opened). It provides high-level helpers — `sweep_overlays(prefix)`,
  `tour_buildings(zoom)`, `frame_tile(col,row,zoom)` — so a new check is "call a
  helper" rather than a from-scratch script. Prefer them over raw `set_pan` math.
- **Capture:** `capture("name")` renders one frame and writes `screenshots/name.png`
  (in-app `SDL_RenderReadPixels` → `write_png_rgba`; nothing leaves the window).
- **Output:** PNGs for inspection; golden-image diffing exists but is **demoted to a curated,
  world-independent set** (Ben's ruling, 2026-08-15 — NR-237). The measured record was decisive:
  in a project whose UI and generated world both churn deliberately, every full-suite diff was
  either an intended change or world drift, the answer was always "bless", and the genuine
  catches were harness bugs. So: **captures are the product, assertions are the verdict**
  (`verify.expect`, `expect_no_clipping` — the `text_overflow_floor` model), and a golden exists
  only where any pixel diff is guaranteed meaningful — surfaces that do not draw generated world
  content (the icon vocabulary; candidates join as areas freeze approaching the prototype cut,
  because **freezing is what makes a golden pay**). A capture with a committed
  `scripts/verify/golden/<name>.png` still diffs exactly as before (PASS/FAIL logs, non-zero
  exit, `screenshots/diff/`); a capture without one is capture-only. `--bless` **refreshes the
  curated set and never grows it** — a capture with no existing golden is skipped under bless
  (app_capture.cpp), so admitting a surface is a deliberate act: copy its capture into
  `golden/` once, bless thereafter.
- **Shipped tolerance** (`app::compare_to_golden`, `src/core/app.cpp`): a two-threshold budget — a
  pixel differs when any R/G/B channel delta exceeds 8; the capture fails when differing pixels
  exceed 0.5% of the frame. A diff image lands in `screenshots/diff/<name>.png`. **Still owed**
  from the 2026-06-15 tolerance design (its superseded "build deferred" subsection removed
  2026-07-31): per-golden ignore masks, per-check tolerance overrides via the Lua API (noted as a
  follow-on in `compare_to_golden`). *(A third owed item, "CI promotion", is retired — CI was
  deleted 2026-07-31; see § Merge gate.)*

#### Acceptance flows — driving a real player action through the commit path (BL-113)

Golden capture proves *chrome renders*; it does **not** prove a player *action* has its effect —
the construction deadlock shipped green because no check exercised a build through the real commit
path. The fix is a second use of the same `--verify` harness: **acceptance scripts** that mutate
game state through the *same code path the interactive control uses*, then assert the outcome with
`verify.expect(cond, msg)` (logs `PASS`/`FAIL`, non-zero exit on any fail).

- **The rule:** a verify commit primitive must call the **same function the UI control calls**, never
  a reimplementation or a bypass registry. `verify.set_building_recipe`/`set_building_workforce`
  perform the exact writes the construction panel's combo/slider do; `verify.place_sell_order` pushes
  onto the same `m_world.sell_orders` the panel appends to; `verify.dispatch_survey_of` calls the real
  `dispatch_survey`. That fidelity is what makes the test catch a broken commit path.
- **Shipped acceptance scripts:** `fresh_start_build.lua` (US-002 build), `recipe_workforce.lua`
  (US-007), `sell_order.lua` (US-008), `survey_dispatch.lua` (US-011). Each traces to its user
  story's `testing.note`. `click_injection.lua` (BL-521) is the odd one out — its subject is the
  harness itself: that a synthesised press reaches the canvas's click handler, agrees with the
  shortcut it replaces, and that a second press is a second press.
- **Capture-only is fine here** — these scripts assert via `verify.expect`, so they need no golden;
  the `capture()` at the end is incidental. Stage preconditions with existing primitives
  (e.g. `verify.set_balance` to afford an action) rather than entangling a separate concern.

#### Golden staleness — the coupling that motivated the demotion (history)

A golden captures the **whole 1280×720 window**, so it regressed whenever *shared chrome* changed
(observed 2026-07-05), and whenever a `src/world/` change moved generated content (BL-259,
2026-08-03) — which in a churning prototype was **most changes**. The disciplines that grew here
(re-bless with the change; "Windows is the baseline; re-bless freely; flag only unexplained
diffs"; goldens are disposable) were honest management of that coupling, and they are the record
of why the 2026-08-15 demotion happened: when the routine answer to every diff is "bless", the
diff carries no information. The curated world-independent set above is exempt from the coupling
by construction — that is the admission criterion.

#### World-content staleness — a `src/world/` change owes a re-bless too (BL-259, 2026-08-03)

The chrome rule above has a sibling: a `src/world/` change that moves **generated content** —
terrain colour, nation/corp names, balances, the political map — regresses goldens the same way a
chrome change does, just wider (most of the suite touches world content somewhere on screen). This
went unnoticed for two days (BL-233's terrain re-price, landed 2026-07-31, wasn't re-blessed until
BL-259 caught it 2026-08-03) because nothing owned the coupling — the headless side has the
equivalent discipline (a band change ships with its commit), the visual side didn't.

**Since the 2026-08-15 demotion this obligation shrinks to the curated set**: a `src/world/`
change cannot stale a world-independent golden, so in practice a content change owes nothing
here (`--verify-all --bless` refreshes the curated files in seconds and is safe to run — it
cannot grow the set). What a content change owes instead is a **capture pass eyeballed on the
surfaces it touches** — the captures are the product; the eyeball is the check.

#### Cross-platform goldens — settled 2026-08-01 (BL-252)

The suite could not be green on Windows and Linux at once, and the two golden families turned out
to need **different answers**. Both are now settled; the underlying measurement is in BL-252.

**Visual goldens are Windows-authoritative.** They are blessed and diffed on Windows only. On
Linux the same commit diffs 9–57%, because pixel output depends on font rasterisation and the
GPU/driver as well as the toolchain — pinning a set per platform would mean re-blessing on every
driver update, for no gain in regression detection. On Linux, **inspect captures by eye and do not
golden-diff them**. There is no CI job to defer to — inspection is the whole check (§ Merge gate).

**Headless golden bands are pinned per toolchain.** `ai_skill_harness` carries one blessed set per
compiler, selected by `#if defined(_MSC_VER)` / `#elif defined(__GNUC__)`, with an `#error` for any
third toolchain so it cannot silently inherit another's numbers. Both sets pass today.

- **Why pinned, not widened.** The same commit gives seed 4 a final net worth of 182,746 under
  GCC and 392,148 under MSVC. A band holding both would span ~±100% and detect no plausible
  regression. Widening was measured and rejected, not assumed.
- **Why this is not a determinism failure.** The standing invariant is same-seed, **same-build**
  reproducibility, and it holds on each platform independently — `ai_skill_harness`'s R0 tier
  (state hash, net-worth curve, action tallies, byte-identical across two same-seed runs) passes on
  both. 300 ticks of a feedback-coupled economy amplifies last-bit float differences; bit-identical
  FP across MSVC and GCC is explicitly **not** a goal this prototype adopts.
- **Optimisation level is not the variable.** MSVC `/O2` reproduces MSVC Debug value for value.
  The toolchain is.
- **Re-blessing rule.** Run the harness on the platform whose set you are changing and change
  **only that set**. Never copy one platform's observed values into the other's block.

A corollary worth stating: a golden-band failure on a platform whose set was blessed long ago is
usually **staleness, not regression**. BL-252's Windows bands had been blessed in the same commit
that added the harness, before BL-203 rewrote the Corp AI strategy layer — so the AI was scored
against goldens set for a different AI. Check what landed since the bless before reading a red band
as a skill regression.

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

### Design-direction Q&A (Batch Delivery)

A **Batch Delivery** (multiple items in one block; see `DELIVERY.md` § Batch Delivery) that made
non-trivial or ambiguous design calls **closes by raising a short design-direction Q&A** — the open
questions and the calls made on the user's behalf — and **records the outcome in the DEVLOG** with
the session. No dedicated log file: DEVLOG is the home, the way the 2026-06-14 Layer 3 Q&A was kept.

**Rationale:** code lands faster than design intent is pinned, so a batch quietly makes calls
(what shape a generated thing takes, which of two mechanics wins). Surfacing them at the close
catches a wrong call while the context is fresh, before it ossifies into "documented".

**Keep it proportional.** This is a guideline, not ceremony — skip the Q&A for a batch that
surfaced nothing worth asking, and keep the questions few. The point is to catch the one
genuine fork, not to manufacture questions.

**A formal Q&A is itself the review — no transient `⟳` note needed.** The `> ⟳` "pending review"
blockquote exists to flag a design call the user has *not yet* seen (the Batch Delivery
documentation discipline; see `DELIVERY.md` § Batch Delivery). When a design call is settled **through a formal
Q&A with the user** — the user chose the direction live — that review has already happened, so the
resulting doc change **does not carry a `⟳` note**. Write the settled design directly. Reserve
`⟳` for calls made on the user's behalf that still await their eyes.

### Inline comments

Use inline comments to explain **why**, not what. If the code requires a comment to explain what it is doing, rewrite the code first. Code that states its own *what* frees the comment budget for the part worth reading — the non-obvious decision.

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

### An observable surface per milestone, as it is built

*(Restated 2026-07-31 — the original "one ImGui panel per milestone" wording predates the lens
family and the verify harnesses; the practised rule is broader.)* Each milestone gets an
**observable surface** wired alongside it as it is built, not at the end — a panel, a map lens, or
a harness readout — the first time the milestone shows itself working. The surface needs only to
make that milestone's state observable: a tile inspector, a market price readout, a convoy list, a
budget line, a lens tint, a `world_audit` section. These surfaces are debugging tools *and* the
functional specification for the production UI. Write ImGui code clearly, not cleverly — it is
reference material as much as working code.

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
- Do not build AI faction behaviour beyond the data-model minimum stub. Two dated, scoped
  exceptions are recorded in `.claude/rules/io-standing-rules.md`: BL-079 (background-corp
  per-building agency, landed 2026-07-07) and BL-181 (player workforce auto-solve, landed
  2026-07-15). The corp-AI item family (BL-199 architecture and its BL-202+ stages) extends this
  under its own accepted design — check the standing rules and those items before assuming the
  stub rule applies unqualified.
- Do not introduce a retained-mode UI framework in place of ImGui for the prototype.

---

## Dependency acquisition (BL-302)

SDL3, Lua, sol2 and ImGui are fetched by FetchContent at **configure** time — ~120 MB of
tarballs. That makes a fresh configure the one build step that needs the network, and it is
also the step nobody exercises: day-to-day work happens in an already-populated build dir
that never downloads anything. BL-302 was filed when a fresh configure on Windows could not
reach SDL3 at all (a schannel TLS revocation failure).

`CMakeLists.txt` seeds a shared **source** cache per checkout. Sources are shared; each build
tree keeps its own `<dep>-build`. A whole shared `FETCHCONTENT_BASE_DIR` does *not* work —
the `<dep>-subbuild` dir carries a generator-locked cache, so `build_vs` and `build_gen`
would break each other.

**Seed the cache** (once per checkout; the location is gitignored):

```sh
cmake -S . -B /tmp/io-seed -DFETCHCONTENT_BASE_DIR="$PWD/_deps_cache"
```

After that any **new** build dir configures from the cache with no network. Already-populated
build dirs are left alone. `IO_DEPS_CACHE` (env var) overrides the location — point a sub-agent
worktree at the main checkout's cache and a fresh worktree configures offline:

```sh
export IO_DEPS_CACHE=/path/to/Project-Io/_deps_cache
```

Every configure prints which state it is in, so the cold case is never silent:

```
-- Deps: seeded from /path/to/_deps_cache -> sdl3;lua_src;sol2_src;imgui_src
-- Deps: COLD -> sdl3;lua_src;sol2_src;imgui_src
```

### The from-cold check

Run this deliberately — after changing a dependency version, or when a fresh-clone build is
about to matter. It is the only thing that exercises the download path a green local build dir
hides. Configure-only; no compile.

```sh
IO_DEPS_CACHE=/nonexistent \
cmake -S . -B /tmp/io-coldcheck/build \
      -DFETCHCONTENT_BASE_DIR=/tmp/io-coldcheck/deps \
      -DCMAKE_BUILD_TYPE=Release
```

`IO_DEPS_CACHE=/nonexistent` is what forces it to be genuinely cold — without it the cache
would seed the check and prove nothing. Expect `-- Deps: COLD -> ...` then exit 0. Measured
2026-08-09 on Linux: **74 s, ~120 MB, succeeds** — the schannel fault is Windows-specific and
does not reproduce here, so this check passing on Linux does *not* clear BL-302 on Windows.

**Measured 2026-08-13 on Windows 11 (BL-341): ~120 MB, exit 0 — it succeeds.** This is the
outcome BL-341 named as the second of two, and it settles the posture: the schannel revocation
failure that opened BL-302 was **transient or environmental** (AV TLS interception, or a
cert-chain that has since changed), not a standing property of this machine. So the seeded
`_deps_cache` is **insurance, not a fix** — worth keeping, because a fresh clone on a machine
having a bad TLS day still wants it, but nothing here depends on it. Re-run this check before
trusting a fresh-clone build on a machine you have not built on.

The complement — proves the cache alone is sufficient, with the network fully off:

```sh
cmake -S . -B /tmp/io-offlinecheck/build \
      -DFETCHCONTENT_BASE_DIR=/tmp/io-offlinecheck/deps \
      -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

---

## Display environment

The runtime display and the verification harness do **not** render at the same size, so UI chrome
must be **resolution-robust** — sized from content and fixed anchors, never pinned to a
resolution-scaled value.

**The smallest supported display is 1280×720 at UI scale 1.0x** (BL-215). It is enforced, not
merely stated: `SDL_SetWindowMinimumSize(1280·s, 720·s)` on window creation and from
`app::apply_ui_scale()`, where `s` is the active UI-scale factor (1.0/1.25/1.5).

The floor scales with UI scale because BL-063 grows the font without scaling the px chrome — at
1.5x the same layout honestly needs 1920×1080. The persisted-settings floor clamps to 1280×720;
the automated overflow check (`scripts/verify/text_overflow_floor.lua`) runs at 1280×720 @ 1.0x.

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
  solutions that are legible and composable over solutions that are locally clever but opaque —
  the legible solution reads as obvious in hindsight and is a pleasure to explain; the locally
  clever one is a debt.
- When the right approach is uncertain, state the uncertainty and present options with
  trade-offs rather than picking one silently. Stay the advisor — the developer makes the calls.

---

## Cutting a release

A **Cut** finalises a version. Replaces the old versioned-backup scheme: the authoritative
record is an annotated git tag (`vX.Y.Z`), recoverable forever with `git checkout vX.Y.Z`.

Work starts on a `feature/*` branch. To cut version `vX.Y.Z`:

1. **Finalize** — build green; fill the `[Unreleased]` section of `CHANGELOG.md`; pick the version.
   For a **minor** cut, also run the AI SOTA sweep (NR-167, ruled 2026-08-13): a research agent
   diffs the field against the previous sweep's recorded baseline — never a from-scratch survey.
2. **Merge** the working branch into `main` locally.
3. *(Retired 2026-07-31.)* The "copy `src/` to `backups/vX.Y.Z/`" step was never practised — no
   `backups/` directory exists, and the actual cut commits (e.g. 934a4e5, Cut v0.0.9) stamp
   CHANGELOG + README only. The tag is the record; no local snapshot.
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

**There is no CI, and nothing enforced guards `main`.** Both halves of the old gate are gone.

Branch protection was never available (BL-105, 2026-07-05): the repository is private on a free
plan, where the branch-protection API returns HTTP 403 (*"Upgrade to GitHub Pro or make this
repository public to enable this feature"*). And GitHub Actions was **deleted on 2026-07-31**
(commit `debcefd`, "Remove GitHub Actions CI/CD (build + claude-review)") — `.github/` is absent
from the tree. Do not cite `build.yml` or a "red CI run"; neither can occur.

What actually guards `main` is therefore **entirely local and human**: a green local build plus
`ctest --test-dir build_linux --output-on-failure` (or `check.bat`) before a release commit lands — step
1 of the Cut above. That is the whole gate. Run it deliberately; nothing will run it for you.
*(The `-LE sweep` this line used to carry is no longer needed — since BL-415 each sweep test's
COMMAND is a gate script that reports itself Skipped unless `IO_RUN_SWEEPS` is set in the
environment, so a bare `ctest` skips them by machinery rather than by a flag the caller has to
remember.)*

**Test tiers, and why the gate excludes one (BL-288, 2026-08-09).** The suite holds three kinds
of program, and treating them alike is what made the gate untrustworthy:

| Tier | Timeout | In the gate? | What it is |
|---|---|---|---|
| default | 60 s | yes | ordinary regression harnesses |
| long (`IO_TEST_LONG_HARNESSES`) | 240 s | yes | structurally long generation/sweep passes |
| `sweep` label | none | **no** | open-ended research tools; run them by name |
| `bench` label | (its own tier) | yes | asserts *absolute* wall-clock times |

Before this, every harness had a flat 60 s bound. A `ctest` run on 2026-08-09 reported **ten
failures of which exactly one was a failing assertion**: four were harnesses that simply take
longer than 60 s and pass (`earthlike_lean_trace` 121 s, `notable_worlds` 105 s,
`mediterranean_sweep` 87 s, `earthlike_tile_census` 58 s — that last one passing only by luck),
two were research sweeps that never finish on a bound (`history_sim_harness`, `history_sweep`),
two were `bench` tests failing because a concurrent build was loading the machine, and one was a
world-generation finding. **A red suite that is mostly noise trains you to ignore it** — which is
how the one real defect, `ai_skill_harness`'s stale GCC goldens, sat unnoticed among nine false
positives for days.

So: a `bench` failure means *re-run it on an idle machine* before treating it as a regression,
and the `sweep` tests are run deliberately (`$env:IO_RUN_SWEEPS=1; ctest -L sweep`, or by
running the harness exe directly), never as a gate — since BL-415 each sweep test's COMMAND is
a gate script (`tools/verify/run_sweep.cmake`) that reports itself Skipped without that
environment variable, so a bare `ctest` cannot run one by accident.

One consequence worth stating, because it has already bitten: a build tree's **configuration is
not obvious from its name**. `build_linux/` is Ninja + **Release** and is the canonical Linux tree;
a Debug tree can pass a harness that fails in Release with no signal anywhere — which is how
BL-288 went unnoticed in the first place.

To restore an enforced gate later, CI has to come back first; then either make the repo public
(branch protection is free for public repos) or move to Pro, require the build checks, and route
Cuts through a PR so they apply to release commits.

## Lua files

Data definition files in Lua follow the same spirit as the C++ standards:

- `snake_case` for all keys and function names
- Each file covers one domain (one building type catalogue, one resource table)
- A header comment states what the file defines and what system consumes it