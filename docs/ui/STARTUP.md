# Project Io — Startup (Entry Screens)

The app's entry flow — everything between launch and the first frame of play.
Code-derived from `src/core/app.{hpp,cpp}` (2026-07-31). See
[LAYOUT.md](LAYOUT.md) for the in-game shell and [CANVASES.md](CANVASES.md) for
the opening view the flow hands over to.

---

## The screen state machine

`app_screen` (`app.hpp`) has three states; `run()` opens on **`menu`**:

```
menu  →  generating  →  in_game
(main menu)  (New World wizard)  (play)
```

**Only `in_game` simulates.** On the menu and wizard the loop just pumps events
and draws — the world, economy, and sim clock are not built until the wizard's
"Begin". The clock is rebased at handoff, so time spent reading either screen
never lands as elapsed in-game days. `run_verify()` jumps straight to `in_game`
(the harness renders the live world, not the menu) unless a script opts in via
`verify.show_menu` / `verify.show_generation`; both screens draw inside
`render()`'s single capture path, so they are golden-verifiable.

## Main menu — `draw_main_menu`

A dark, centred title card — deliberately spare, a launch entry point, not a
settings hub (no Load/Save in the prototype). Contents:

- **Seed** — hex entry, a one-shot **Roll** (a `random_device` draw feeds *only*
  the seed value; generation stays a pure function of it), and **Copy seed**.
- **Resources** — abundance radio: Sparse / Lean / Standard (Standard is the
  Earth-like ceiling, GENERATION_STRATEGY.md § The resource ceiling).
- **Bodies** — a disabled slider, fixed at 5; the count knob is phased to a
  later update. No nation knob: nation count is a consequence of landmass, not
  a target (NATION_GENERATION.md).
- **New Game** → `open_new_world_wizard()` (commits nothing); **Quit**.

Every widget edits `m_pending_world_params` (`world_params`), which the wizard
continues to edit and `start_new_game()` finally consumes. The six Planetology
sliders that once sat here moved into the wizard — a slider whose effect you
cannot see is a slider you cannot judge.

## New World wizard — `draw_generation_screen` (BL-167)

**Three rounds** (`wizard_round_count`), batched thematically from the ten-stage
Planetology chain (Ben, 2026-07-22: fewer rounds, too slow otherwise). Each
round stacks its stages' charts and explanations, then takes that round's
preferences.

- **Preferences, not parameters** (`world_preferences`,
  `src/world/planetology.hpp`): a named lean per axis ("Dimmer", "Metal-rich"),
  resolved against the seed by `resolve_preferences` — no raw generated value
  is editable. Model authority: PLANETOLOGY.md § Preferences, not parameters.
- **Nothing is generated here.** Every control move re-runs the chain as a
  pure, throwaway preview (`refresh_wizard_preview` →
  `resolve_preferences` + `preview_system`); `m_world` is untouched. The
  resolution rerolls internally until the homeworld clears the Earth-like
  floor, and surfaces what that cost (`resolved_world::attempts`).
- **Rounds are causal** — rerolling round A re-draws B and C downstream, so
  Back is a plain revision with no per-round snapshot.
- **Charts** come from `ui::generation_charts`, shared verbatim with the
  History ledger's Chain view (BL-211) — the plots a player decided a world on
  are the plots they can reopen mid-campaign.

### The globe (BL-256) — and why it does not take input

The pane's right two-thirds is the world itself: round 0 draws the **system**
(star colour and size, orbits, body sizes) so the screen is never empty while
there is no body yet; rounds 1–2 draw the **homeworld**, from the real tile
raster once one has been built. It is the primary view and the charts are the
extras on top, which is the reorder BL-319 landed.

**It spins on a clock, and it takes no mouse input. That is the design, not a
gap** (Ben, 2026-08-10): the globe turns one revolution a minute on wall time,
frozen to 0 under `--verify` so a golden capture never races the animation.

BL-256 originally specified **pan**, clamped in latitude so the camera could not
pass either pole. That requirement is **cut**, on Ben's call, for a reason worth
keeping: *an uncontrollable globe tells the player that generation is slightly
beyond their reach.* It says the same thing the preferences model already says —
**you set conditions here, you do not steer** — so a draggable camera would have
quietly contradicted the screen's own premise in order to add a control nobody
needed. The wizard resolves leans against a seed; the globe should feel like
something you are watching resolve, not something you are operating.

**Render technique note.** The globe is drawn as **48 meridian slices**, each
subdivided in latitude, every cell a quad — not the per-pixel inverse projection
into a texture that BL-256's design proposed. Both avoid the failure mode that
design was written to dodge (projecting ~7,500 hexes as polygons against ImGui's
16-bit draw indices); the slice path simply got there with less machinery. Noted
because the item and the code otherwise read as disagreeing.

## Handoff — `start_new_game`

The wizard's "Begin", and the one and only generation call:

1. Rebase the sim clock (fresh `sim_loop`; speed from the Lua config).
2. `setup_world(m_pending_world_params)` — build the world; fills
   `m_generation_report` (presentation artefact, off the serialisation seam).
3. `load_economy()` — recipes + economy constants from Lua.
4. **Pre-game warm start** ([C3]): seed the balance history, then run 12
   quarterly econ ticks (~3 in-game years) so every corp opens onto non-empty
   pools and live markets, not a cold zero state. `run_verify` stays cold.
5. Rebase the clock again (generation + warm-start wall time must not become
   in-game days), then `m_screen = in_game`.

Play opens on the corporation's home planet — the Planetary rung, home body
selected (CANVASES.md § Default state).
