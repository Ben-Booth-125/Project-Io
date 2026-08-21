---
name: verifier-visual
description: Run a Project Io visual-verification check. Builds the app, runs ProjectIo --verify on a scripts/verify/<name>.lua script in headless capture mode, and inspects the resulting PNG captures. Use when asked to visually verify a canvas/lens/rendering feature, re-run a saved visual check, or confirm an on-canvas change looks right. Authorising a new check = adding a scripts/verify/*.lua script and naming it here.
---

# verifier-visual

Runs the headless visual-verification harness (Phase 1 + Phase 2) for a named
check and reports the captured frames. This wraps `ProjectIo --verify <script>`
so re-running a proven visual check is a single invocation rather than bespoke
authoring. Design context: `docs/development/DEVELOPMENT_PRACTICES.md`
§ Visual verification; `docs/development/BACKLOG.md` § Canvas.

## Argument

The verify **script** to run, as a `scripts/verify/<name>.lua` path or just the
feature name (e.g. `corporation_lens`). If omitted, list the available scripts in
`scripts/verify/` (each `*.lua` except `lib.lua`) and ask which to run.

## Available checks

Every `scripts/verify/*.lua` (except `lib.lua`) is a runnable check — the list is
**auto-discovered** from the directory, so a newly committed script is available
without editing this file. The lens / canvas / panel checks form the bulk
(`corporation_lens`, `country_lens`, `market_lens`, `population_lens`,
`resource_lens`, `scarcity_lens`, `supply_lens`, `opportunity_lens`,
`production_lens`; `building_management`, `construction_panel`, `selection_go_to`,
the ledger checks, …). v0.0.8 (Discovery & Intelligence) additions, named here per
the "authorising a new check = naming it" convention:

- **`survey.lua`** (BL-067) — Planetary survey mask progression (masked → raster
  partial → full) and the Solar-canvas survey badges (hidden `?` / scanning `k∕N` /
  surveyed). Driver: `verify.set_survey(body, regions_done)`.
- **`visibility.lua`** (BL-068) — competitor information asymmetry: selecting a rival
  building shows the competitor Selection panel (owner + tile + `private` placeholders,
  no production/stockpile) vs a player building's full management detail. Drivers:
  `verify.select_building` / `select_tile`.
- **`population_legibility.lua`** (BL-069) — the Population lens re-keyed to workforce
  efficiency (the 0.6 cliff) plus the Selection panel population-centre read (scale /
  population / habitability / absolute workforce cap). Driver: `verify.select_tile`,
  `verify.population_centres()`.
- **`continents_terrain.lua`** (BL-210 first slice) — Kepler's terrain now that Pass 1's
  heightmap is biased by the Continents/Drift sibling pass instead of pure noise. Captures
  plain terrain (landmass shape) and the Country lens (nation borders over it) at a
  full-planet zoom. Driver: `verify.goto_surface`, `verify.set_zoom`, `verify.set_overlay`.
- **`settlement_labels.lua`** (BL-363) — City+ conurbation labels come from the seeded
  naming system (`world::population_centre_name`, the BL-290 tongue banks), not a
  hard-coded bank; guards the standing no-Earth-names rule at the canvas. It locates the
  largest population centre via `verify.population_centres()` and frames *that*, rather
  than hard-coding a tile — the pattern to copy for any check over generated content,
  since `pop_markers.lua`'s fixed `(66, 6)` now frames empty terrain after the BL-257 /
  BL-348 generation changes moved the world under it.
- **`history_ledger_and_comms.lua`** (BL-211 + BL-212) — the History nav-rail slot's three
  views: **Story** (the oral-history biography, including Continents/Drift's merged lines),
  **Chain** (the wizard's generation stage charts redrawn from the persisted report, captured
  for each of the three rounds), and **Tiles**; plus the Public comms channel's epoch line
  confirming it is nation-voiced, not corporation-voiced. Story is captured **twice** —
  head and foot — because the column clips the biography after ~8 lines and the head
  capture alone could never prove the recent-epoch events render. Drivers:
  `verify.goto_surface`, `verify.show_panel("tile", ...)`,
  `verify.panel_view("history"|"history_round", i)` — the sub-view hook that lets a
  capture reach a ledger tab without a click — and `verify.scroll_panel` (below).

- **`corp_choice.lua`** (BL-435, 2026-08-16) — the starting-corp selection stage. Notable for
  *how it reaches the screen*: the stage lives in one frame of the real start-up sequence, and
  every automated path (`--autostart`, this harness's own start) takes the generator's seeded pick
  the instant it appears — deliberately, since that is what keeps all other captures and goldens
  bit-identical. So it was unreachable to `--verify` until `verify.show_corp_choice(true)`
  re-entered it from the started world, and a check written against an unreachable surface is
  green-but-blind. Asserts what is true of every world — more than one opening, a pool the size of
  the specialist set, every row named and focused — via `verify.corp_choices()`, then captures.
  Capture-only, **no golden**: Ben has not eyeballed the layout, and blessing an unreviewed screen
  pins whatever got built. It also needs a **warm-up capture** first, as `main_menu.lua` does; the
  window is `AlwaysAutoResize` and a cold frame-1 capture came back empty (measured). Its first
  real capture immediately earned its keep — the Holdings cell was clipped to "/ 1 otl" behind the
  Choose button.

- **`tile_texture.lua`** (BL-520, 2026-08-21) — the substrate grain and the cover pattern on
  the Planetary canvas. Four claims, and the captures are grouped by which one they test:
  the two passes are distinguishable (cover reads per tile, grain does not draw a seam); the
  pattern scales with `cover_density`; texture survives every lens attenuated to 0.45; and the
  LOD bound fires — **no texture at all below 14 px of drawn circumradius**, stricter than
  BL-269's 7 px coarse-fill threshold because a sampled pattern at hex scale is moiré rather
  than merely invisible. Captures Kepler (the biotic covers on sedimentary ground) and Cinder
  (dunes / ash / snow / salt over rocky, regolith, volcanic and metallic grain), plus the
  ocean-is-flat and unsurveyed-is-blank negatives. **No golden yet**: the pass was written in a
  container that could neither build nor display the app, so nothing about it has been seen —
  bless only after eyeballing `texture_lens_country` and `texture_lod_*`, which are the two
  frames carrying decisions taken on Ben's behalf.

## Text-overflow floor check (BL-215)

**`text_overflow_floor.lua`** — the render-precision audit's saved check. Every measured text
draw routes through `ui::text_fit` and records any string that did not fit its container
(`elided` = sanctioned WARN; `clipped` / `unfittable` = FAIL). The script walks every ledger,
wizard round, lens and Selection kind at 1280×720 and ends with `verify.expect_no_clipping("floor")`.

- The harness writes `screenshots/text_overflow.txt` (container, site, text, needed vs available
  px, frame) and **exits non-zero** on any FAIL record — the integer is the verdict; the PNGs are
  incidental, never the pass criterion. Bindings: `verify.clipping()` (running FAIL count),
  `verify.expect_no_clipping(label)`.
- **Coverage step** (the ledger only sees opted-in sites): `rg -n "AddText\(" src/ui src/core`
  must return only hits inside `text_fit.cpp`, `icons.cpp`, or lines carrying a trailing
  `// fit-exempt: <reason>` comment. A bare hit is a site invisible to the check — route it
  through `ui::text_fit` or exempt it on the record.

## Determinism: software renderer + headless display (read first)

Goldens must be **renderer- and machine-independent**, so always run `--verify`
(and `--bless`) through the **software renderer** — set `SDL_RENDER_DRIVER=software`.
A GPU/accelerated renderer rasterises tiles and text differently from the software
path, so goldens blessed on a GPU diff ~8–45% against the CI software renderer
(the BL-057 spike confirmed this). The software renderer is deterministic across
machines, so CI and any dev box agree.

- **Linux, no monitor:** wrap the run in `xvfb-run -a` (a virtual X display — the
  capture uses `SDL_RenderReadPixels`, which is renderer-agnostic, so no real
  display is needed). CI's `visual-verify` job does exactly this.
- **Windows / a desktop with a display:** `SDL_RENDER_DRIVER=software` alone is
  enough; the window can render on the real display.
- **Re-bless everything at once:** `scripts/verify/bless_all.sh [path-to-exe]`
  forces the software renderer (and Xvfb if present) and blesses every script.

## Procedure

1. **Build** the target so the binary and the copied scripts are current
   (Linux/Ninja Release: `cmake --build build --target ProjectIo` → `./build/ProjectIo`;
   Windows/MSVC Debug: `cmake --build build --config Debug --target ProjectIo`):
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
   Panel drivers: `show_panel(name, open)`, `panel_view(name, i)`,
   `scroll_panel(name, fraction)`, `frames(n)` — see § Content below the fold.
3. **Inspect** every capture with the Read tool (PNG is directly readable) and
   report what each frame shows against the requirement being checked. Cite the
   capture file names.

## Content below the fold (a golden's quietest blind spot)

A capture composits the panel **exactly as laid out**, so anything the shell column
clips is invisible to the golden — and a diff over content that was never rendered
reads `0.0000% differing`. That is the failure mode worth naming: not a red FAIL,
but a green PASS over a panel whose lower half could be rewritten unnoticed. It hid
the whole of the Generation History biography past its first ~8 lines.

`verify.scroll_panel(name, fraction)` parks a fold-out ledger's vertical scroll at a
fraction of its extent — `0` = top, `1` = foot. Names mirror `show_panel`'s
vocabulary (`tile`/`history`, `market`, `economy`, `balance`, `corporation`,
`construction`); an unknown name clears the request.

**It is sticky, and it lands on the FOLLOWING frame** (ImGui resolves a scroll target
inside `Begin`, and the fraction resolves against the previous frame's content
height). So two rules, both learned the hard way:

```lua
verify.scroll_panel("history", 1.0)
verify.frames(2)                      -- or the capture shows the OLD scroll
verify.capture("history_story_kepler_foot")

verify.scroll_panel("history", 0.0)   -- reset, don't clear: clearing merely stops
verify.frames(1)                      -- setting it, leaving the panel at the foot
```

Skipping that reset+frame is what made a later Chain capture come out **blank** —
the view changed but the scroll was still at the old foot.

Limit: it scrolls the ledger **window**. A panel nesting its own `BeginChild`
scroller (Market Ledger's `##price_scroll`) is not reached and would need its own
hook. Implementation: `ui::foldout_request_scroll` (`src/ui/foldout_column.cpp`),
bound to Lua in `src/core/verify_api.cpp`.

When a check covers a panel whose content can outgrow the column, capture it **twice**
— head and foot — as `history_ledger_and_comms.lua` now does.


## Golden-image diffing (automatic PASS/FAIL — demoted to a curated set, 2026-08-15)

**Golden-diffing is demoted to a small, world-independent curated set** (Ben's ruling,
NR-237; policy authority: `DEVELOPMENT_PRACTICES.md` § Visual verification). Captures are
the product and assertions (`verify.expect`, `expect_no_clipping`) are the verdict; a
golden exists only where any pixel diff is guaranteed meaningful — surfaces that draw no
generated world content (currently the `icon_silhouettes` pair). `--bless` refreshes
existing goldens and **never creates one**: admitting a new surface to the set means
copying its capture into `golden/` once, deliberately.

Goldens live in a `golden/` directory **beside the verify scripts** —
`scripts/verify/golden/<capture>.png`, one per named `capture()`; a capture without one
runs capture-only.

- **Compare (default).** When a golden exists for a capture, `--verify` diffs the
  captured frame against it, logs `Golden PASS <name>: …%` or `Golden FAIL <name>: …%`,
  and writes a highlighted diff image to `build/Debug/screenshots/diff/<name>.png`
  (differing pixels flagged magenta over a dimmed base). The process **exits non-zero**
  if any capture fails. No golden present = capture-only (the original behaviour), so
  the upgrade is incremental per check.
- **Bless (regenerate goldens).** When a change is intentional, eyeball the captures
  then regenerate the goldens with `--bless` **through the software renderer** (so
  the committed goldens match CI):
  ```
  SDL_RENDER_DRIVER=software ./build/ProjectIo --verify scripts/verify/<name>.lua --bless   # Linux
  SDL_RENDER_DRIVER=software build/Debug/ProjectIo.exe --verify scripts/verify/<name>.lua --bless   # Windows
  ```
  Or re-bless every script at once with `scripts/verify/bless_all.sh`. Each capture is
  written into `scripts/verify/golden/` instead of compared. **Run the bless (and the
  iterating compare) against the source script path** so the golden dir resolves to the
  committed source tree, not a stale build copy — the golden directory is always derived
  from the script path's own parent.
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

- **`battle_card.lua`** (BL-468/BL-469, 2026-08-21) — the battle card and the Field
  dispatch channel. **Read it before writing any check that needs a simulation state
  the script must CREATE rather than find.** Most checks here open a panel and look;
  this one has to stage a fight: identify the player corp from the buildings table's
  own `player` flag (never a hard-coded corp id), declare hostility in both
  directions, march the player's unit into a rival's province, and step to contact.
  It hard-codes no grid coordinate, so a generation change moves the fight instead of
  silently invalidating the check.

  It became writable only with the generic `verify.corp_command{…}` binding: before
  that, `verify_api` could issue exactly one verb (`place_sell_order`), so no script
  could declare hostility, so no battle could exist in a verify run at all — every
  visual requirement on the surface was green-but-blind by construction (NR-345,
  NR-472). If a surface you are checking has no reachable path from a script, that is
  the bug to fix first; a capture of a state the script could not produce proves
  nothing.

  **Not yet run** — authored 2026-08-21 in a container with no SDL3, so it awaits a
  Windows/Linux-with-SDL run plus the live click the standing rule requires.
