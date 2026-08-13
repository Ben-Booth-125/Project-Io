# Next session — startup crash + the loading screen's second bar

Handoff written 2026-08-12. Self-contained: you should not need the previous
session's transcript.

> **UPDATE, later on 2026-08-12 (session 2).** Task 2 (the second bar) is
> **done**, and Task 1 is **SOLVED**: it was never a crash. Windows Error
> Reporting showed every death as **AppHangB1** — the post-generation tail froze
> the UI, Ben clicked, Windows closed the "hung" app. The tail was measured:
> **persona counsel was 83.9 s of the 90 s warm start** (Lua bench + blackboard
> export per due corp per tick, 35 corps since BL-365). Landed: counsel skipped
> during the warm start, the warm start sliced across loading-screen frames
> (un-hangable by construction), a reach-field seeding fix and narrowed
> logistics invalidation. Warm start ~90 s → ~6.5 s. Records: **NR-182**
> (resolved, full diagnosis), **NR-183** (the two behaviour notes), **BL-379**
> (persona counsel's remaining ~1 s/tick cost in live play). The sections
> below are amended in place.

---

## Repo state before you start

**Nothing since `9d39fee` is committed.** The tree carries roughly 20 modified
files and three new tools. Read `git status` first, and **do not** `git checkout --`
anything — that work is not recoverable from a branch.

Last four commits (all landed, all green at the time):

| Commit | What |
|---|---|
| `9c54526` | Era −1 stepped decision clock, 4000 BCE → 0 CE |
| `35067d1` | 3× map (312×145 = 45,240 tiles), tile scale from planet mass, distance→time |
| `9a6cd01` | Gate repairs + `tools/verify/tick_profile.cpp` |
| `9d39fee` | Docs for the 0 CE epoch |

Uncommitted since then, and all of it working except the crash:

- `world_params::epoch_year` default **1960 → 0** (the ancient refocus, NR-177)
- **The Era −1 year-tick sim is now wired into generation** — it had existed
  since BL-271 and was never called from the campaign path, only from the tile
  inspector. It runs 400 years at 4 years/tick before the epoch.
- Three scorer fixes in `history_sim.cpp`, all the same shape (a cost authored on
  one scale subtracted from a value on another): `w_cult` made proportional,
  `w_dist` made proportional, and **supply now projects from the staging holding
  rather than the capital** (`hub_dist`) — Ben's "supply hubs" idea.
- **Async world generation + a loading screen** (`app::begin_new_game`,
  `poll_worldgen`, `draw_building_screen`, `app_screen::building`).
- `--autostart` flag and `tools/verify/seed_sweep_probe.cpp` — both written to
  chase the crash below.

Measured on the current tree: generation ~25 s, and the pre-epoch era produces
**62 battles, 16 conquests, 1107 foundings** over 400 years.

---

## Build and run — read this before your first build

**Always `.\build_app.bat [target]` from the repo root.** Never raw `cmake --build`
and never another Visual Studio's `vcvars`. The build tree is pinned to VS2022
BuildTools MSVC **14.44**; VS18's vcvars puts 14.51 headers on `INCLUDE` against
the 14.44 compiler and detonates inside `<variant>` with a wall of bogus STL
errors that look like a corrupt toolchain. `build_app.bat` exists to close exactly
that trap.

Two traps that cost the last session real time:

- **`ctest` runs binaries but does not build them**, and `build_app.bat` with no
  argument builds only the `ProjectIo` target. Running the gate without a full
  `cmake --build build` first means testing **stale executables**. This produced a
  completely bogus 63/67 result and two rounds of wrong conclusions.
- **Do not run the gate while building or generating in parallel.** The last gate
  returned ~10 failures that were nearly all `Timeout` under machine load, not
  assertions.

Adding a new `tools/verify/*.cpp` needs a CMake **reconfigure** before it appears
as a target (the file list is a glob).

---

## Task 1 — the startup crash

### Symptom

Launch `build/ProjectIo.exe`, click through the New World wizard, press **Begin**.
The loading screen appears and advances; the app dies while the label reads
**"Placing companies"**. Reported as a crash — the process exits.

### Read this before hunting: the label is probably lying to you

The label is `generation_progress::label`, set by `bump(11)` in
`hard_coded_world.cpp`. It stays at 11 until `bump(12)` immediately before
`make_hard_coded_world` returns. **More importantly**, after generation completes
`poll_worldgen()` calls `start_new_game()` on the main thread, which runs
`load_economy`, `generate_background_firms` and an 80-tick warm start — all while
the last drawn frame still shows whatever the label last said.

So "Placing companies" marks **where the UI stopped repainting**, not necessarily
where execution died. Treat it as a hint, not a location.

### Already ruled out — do not redo these

Each of these was measured, not reasoned about:

1. **Generation itself.** `tools/verify/seed_sweep_probe.cpp` generates a world
   per seed across 8 seeds, headlessly. All pass (corps=8, nations 55–84).
2. **Worker-thread stack size.** The same probe was changed to run generation
   through `std::async` exactly as `begin_new_game` does. Still passes — so it is
   not a smaller default stack on the worker.
3. **Shared mutable statics in the generation path.** `tile_generation.cpp`,
   `continents.cpp`, `planetology.cpp`, `body_names.cpp`, `city_names.cpp`,
   `tongue.cpp` were swept; the only statics are two pure functions.
4. **`start_new_game`'s whole tail.** `build/ProjectIo.exe --autostart` runs
   generation → `load_economy` → `generate_background_firms` → warm start,
   headlessly, and **completes clean**: `bodies=5 nations=56 corps=35 markets=16
   tiles=64560`.
5. **Concurrency with the wizard's own async surface build.** `--autostart` calls
   `launch_wizard_surface_build()` before `begin_new_game()` to reproduce two
   concurrent generations. Still completes.

**Conclusion: it is specific to the windowed/interactive path**, not to
generation, not to the startup tail, and not to async in isolation.

### Session 2 (2026-08-12, later): four more reproductions, all clean

The mid-frame `start_new_game()` lead above is now **measured clean, not
reasoned about**: `--autostart-windowed` (new flag) runs the real window, real
ImGui frames, walks the wizard round by round with rerolls, presses Begin from
inside the wizard's own frame, rides the loading screen through the mid-frame
transition, injects synthetic centre-screen clicks on the first five in-game
frames (the queued impatient-player clicks), and renders 120 in-game frames.
Four variants, all exit 0, no crash.

What is now in place for the next manual repro (**read NR-182 first**):

- **`build/crash.log`** — every death shape now lands there: the fatal-error
  catch mirrors its message (stderr was invisible on double-click), an SEH
  filter names hardware faults with code + address, and a `std::set_terminate`
  handler catches thread/noexcept escapes.
- **An EMPTY crash.log beside a dead process is itself an answer**: external
  termination (Defender / Smart App Control killing a fresh unsigned exe — this
  machine already blocks fresh harness exes) or a Debug assert dialog. Check
  Event Viewer before touching code.
- Remaining unautomatable deltas: Ben's exact wizard inputs/seed, sustained
  human mouse activity, external kills.

### Tools you have

- `build/ProjectIo.exe --autostart` — headless full startup. **Keep it**; it
  covers a path `--verify` never reached (`run_verify` calls `setup_world` +
  `load_economy` and stops, so `generate_background_firms` and the warm start had
  no automated coverage at all, which is how this reached a player build).
- `tools/verify/seed_sweep_probe.cpp` — per-seed generation, catches
  seed-dependent failures.
- `tools/verify/tick_profile.cpp` — per-phase econ-tick timing plus a world
  generation timer. Built after two rounds of wrong guesses about where time
  went; it answered it in one run.

---

## Task 2 — the loading screen's second bar — **DONE (session 2, 2026-08-12)**

Built exactly to the suggested shape below: `generation_progress` gained
`sub_progress`/`sub_total` atomics (same worker-writes / renderer-reads, no-mutex
contract), `run_history_sim` gained a defaulted `std::atomic<int>* year_progress`
sink (null for every headless caller; never read back, so determinism is
untouched), the ancient-era block in `hard_coded_world.cpp` announces its year
span and clears it when done, and `draw_building_screen` draws the inner bar +
"N / M years" only while `sub_total > 0`. The outer bar still steps one pass at
a time — no smoothing. Visual eyeball owed to Ben on his next launch.

Original brief, kept for context — Ben, 2026-08-12: *"we could stagger the
progress a bit so we see more of the larger steps with a second bar"*.

### What exists

- `generation_progress` in `src/world/hard_coded_world.hpp` — three atomics
  (`stage`, `stage_count`, `label`), written by the generation worker, read by
  the render thread every frame. No mutex, no callback.
- `generation_stage_labels[]` — 13 labels, same header.
- `bump(n)` calls through `src/world/hard_coded_world.cpp`, one per pass.
- `app::draw_building_screen()` in `src/core/app.cpp` — one `ImGui::ProgressBar`
  over `stage/stage_count`, the label, and a subtitle.

### The problem to solve

The passes are wildly uneven. **"Running the ancient era" is ~23 s of the ~25 s
total** — the bar jumps to that stage and then appears frozen for the entire
wait, which is exactly the impression the loading screen was added to remove.

### Suggested shape

Add a **second, inner bar** for progress *within* the current stage:

- Extend `generation_progress` with `sub_progress` and `sub_total` atomics
  (keep the no-mutex, atomics-only contract — the worker writes, the renderer
  reads).
- `run_history_sim` is the one pass that needs it and the one that can supply it:
  it loops years from `start_year` to `stop_year`, so year position over span is a
  free, honest fraction. It will need an optional progress pointer parameter,
  defaulted to null so every headless caller is unaffected.
- Draw the inner bar under the outer one, only when `sub_total > 0`.

Keep the outer bar advancing one pass at a time. **Do not smooth or interpolate
it** — a smoothly animating bar over discrete passes is a lie told at 60 Hz, and
the honesty is the point.

---

## Also outstanding, lower priority

- **Every generation-touching harness now pays the 400-year sim.** `world_audit`,
  `world_determinism`, `settlement_harness`, `planetology_harness`,
  `era_world_harness`, `history_ladder_harness` and others each gained ~23 s and
  several now exceed their ctest timeouts. Options: let harnesses pass an
  `epoch_year` that skips the sim, give them a cheap sim config, or re-tier them.
  **This needs deciding before the gate can be trusted again.**
- **`era_world_harness` will fail by design** — it asserts the *default* epoch is
  1960, and the default is now 0. Update it to assert the new default and keep a
  1960 case for the parked space arc.
- **Goldens will have moved again.** `epoch_year` 1960 → 0 changes every generated
  world. `ai_skill_harness` was blessed three times in one day last session; read
  its header comment before blessing again — it records why each earlier bless was
  wrong (stale binary, then a half-applied map change).
- **Two pre-existing gate failures**, diagnosed from source and unrelated to any
  of this — see **NR-179**. `stack_capacity_harness` in particular should **not**
  be "fixed" without reading BL-193's intent first; it may be catching a real
  defect rather than being stale.
- **Settling still dominates** the pre-epoch era (1107 foundings vs 16
  conquests). Left deliberately — see **NR-181**: Ben ruled procedural generation
  a stepping stone toward AI players starting worlds, so the scorer is explicitly
  not to be gold-plated.

## Review entries worth reading first

`NR-177` (the refocus ruling), `NR-178` (the stepped clock is not
behaviour-neutral), `NR-179` (the two pre-existing failures), `NR-180`
(BL-253 was already fixed; the tick cost is `run_economy_step`), `NR-181`
(stepping stone → AI players start worlds).

Query them with `node tools/session/backlog_query.js` or read
`docs/development/NEEDS_REVIEW.md`.
