---
name: verifier-visual
description: Run a Project Io visual-verification check. Builds the app, runs ProjectIo --verify on a scripts/verify/<name>.lua script in headless capture mode, and inspects the resulting PNG captures. Use when asked to visually verify a canvas/lens/rendering feature, re-run a saved visual check, or confirm an on-canvas change looks right. Authorising a new check = adding a scripts/verify/*.lua script and naming it here.
---

# verifier-visual

Runs the headless visual-verification harness (Phase 1 + Phase 2) for a named
check and reports the captured frames. This wraps `ProjectIo --verify <script>`
so re-running a proven visual check is a single invocation rather than bespoke
authoring. Design context: `docs/development/DEVELOPMENT_PRACTICES.md`
§ Visual verification; `docs/development/OPENS.md` § Canvas.

## Argument

The verify **script** to run, as a `scripts/verify/<name>.lua` path or just the
feature name (e.g. `corporation_lens`). If omitted, list the available scripts in
`scripts/verify/` (each `*.lua` except `lib.lua`) and ask which to run.

## Procedure

1. **Build** the Debug target so the binary and the copied scripts are current:
   `cmake --build build --config Debug --target ProjectIo`
   (the build copies `scripts/` next to the exe). If iterating on a script without
   a rebuild, run `--verify` against the **source** path instead, e.g.
   `--verify ../../scripts/verify/<name>.lua`, so the harness picks up the edited
   script and its `lib.lua` from `scripts/verify/` rather than a stale build copy.
2. **Run** the check from the output directory:
   ```
   build/Debug/ProjectIo.exe --verify scripts/verify/<name>.lua
   ```
   The harness sets up a deterministic, paused world, auto-loads
   `scripts/verify/lib.lua` (the shared helpers: `sweep_overlays`,
   `tour_buildings`, `frame_tile`), runs the script, and writes named PNGs to
   `build/Debug/screenshots/`. `verify` API: `goto_surface`, `set_overlay`,
   `set_zoom`/`set_pan`/`add_pan`, `center_tile(col,row[,zoom])`, `command(name)`
   (the shared canvas command vocabulary), `capture`, `buildings`, `log_buildings`.
   SDL logs each `Screenshot saved:` line to stderr — that is success, not failure.
3. **Inspect** every capture with the Read tool (PNG is directly readable) and
   report what each frame shows against the requirement being checked. Cite the
   capture file names.

## Golden-image diffing (automatic PASS/FAIL)

A capture can be checked automatically against a committed **golden** reference
image instead of eyeballed (OPENS § Canvas [F3]). Goldens live in a `golden/`
directory **beside the verify scripts** — `scripts/verify/golden/<capture>.png`,
one per named `capture()`.

- **Compare (default).** When a golden exists for a capture, `--verify` diffs the
  captured frame against it, logs `Golden PASS <name>: …%` or `Golden FAIL <name>: …%`,
  and writes a highlighted diff image to `build/Debug/screenshots/diff/<name>.png`
  (differing pixels flagged magenta over a dimmed base). The process **exits non-zero**
  if any capture fails. No golden present = capture-only (the original behaviour), so
  the upgrade is incremental per check.
- **Bless (regenerate goldens).** When a change is intentional, eyeball the captures
  then regenerate the goldens with `--bless`:
  ```
  build/Debug/ProjectIo.exe --verify scripts/verify/<name>.lua --bless
  ```
  Each capture is written into `scripts/verify/golden/` instead of compared. **Run
  the bless (and the iterating compare) against the source script path** so the
  golden dir resolves to the committed source tree, not a stale build copy — the
  golden directory is always derived from the script path's own parent.
- **Tolerance knobs.** A pixel *differs* when its max R/G/B channel delta exceeds
  `T` (currently 8/255 — absorbs anti-aliasing and sub-pixel font jitter); a capture
  *fails* when the differing fraction exceeds `F` (currently 0.5%). These absorb
  benign jitter (a clean re-run typically diffs ~0.01%); per-check overrides via the
  Lua API are a future follow-on.

## Notes

- The session is deterministic (seeded world, sim paused, fixed 1280×720 window),
  so captures are reproducible across runs.
- To author a new check: add `scripts/verify/<feature>.lua` (lean on the `lib.lua`
  helpers), confirm it captures what the requirement needs, then run it through
  this skill — that is how a check becomes a permanent, reusable asset.
