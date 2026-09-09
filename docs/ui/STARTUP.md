# Project Io — Startup (Entry Screens)

> **Settles:** which screens sit between launch and the first frame of play, and in
> what order · what the main menu offers · what the player chooses in the world
> wizard and what its globe is for · what happens during the carve and the warm
> start · how the player comes to be seated on a corporation.
> **Not here:** the in-game shell (LAYOUT) · the view play opens on (CANVASES,
> PLANETARY) · what the generator produces (generation/*).
> **Confused with:** MENU.md, LAYOUT.md.

The app's entry flow — everything between launch and the first frame of play.
Written against `src/core/app.{hpp,cpp}` and `src/ui/startup_screens.cpp`. See
[LAYOUT.md](LAYOUT.md) for the in-game shell and [CANVASES.md](CANVASES.md) for
the opening view the flow hands over to.

---

## The screen state machine

`app_screen` (`app.hpp`) has four states; `run()` opens on **`menu`**:

```
menu  →  generating  →  building  →  in_game
(main menu)  (New World wizard)  (carve, then warm start)  (play)
```

`building` is the loading screen the world is carved on (CORPORATION_GENERATION.md
§ Corporate seeding is watched); it also hosts the pre-game warm start that runs
after the carve. There is **no corp-selection stage** — the player is seated on a
corporation drawn from the spawn shortlist once the warm start has finished. See
§ The seat below.

**Only `in_game` simulates.** On the menu and wizard the loop just pumps events
and draws — the world, economy, and sim clock are not built until the wizard's
"Begin". The clock is rebased at handoff, so time spent reading either screen
never lands as elapsed in-game days. `run_verify()` jumps straight to `in_game`
(the harness renders the live world, not the menu) unless a script opts in via
`verify.show_menu` / `verify.show_generation`; both screens draw inside
`render()`'s single capture path, so they are golden-verifiable.

## Main menu — `draw_main_menu`

A dark, centred title card — deliberately spare, a launch entry point, not a
settings hub (no Load/Save on the menu; `--load` is a command-line path). Contents:

- **Seed** — hex entry, a one-shot **Roll** (a `random_device` draw feeds *only*
  the seed value; generation stays a pure function of it), and **Copy seed**.
- **Resources** — abundance radio: Sparse / Lean / Standard (Standard is the
  Earth-like ceiling, GENERATION_STRATEGY.md § The resource ceiling).
- **Bodies** — a disabled slider, fixed at 5; the count knob is phased to a
  later update. No nation knob: nation count is a consequence of landmass, not
  a target (NATION_GENERATION.md).
- **New Game** → `open_new_world_wizard()` (commits nothing); **Quit**.

Every widget edits `m_pending_world_params` (`world_params`), which the wizard
continues to edit and `start_new_game()` finally consumes. The Planetology
sliders live in the wizard, not here — a slider whose effect you cannot see is a
slider you cannot judge.

## New World wizard — `draw_generation_screen`

The wizard is BL-167 (planetology pass). Its first **three rounds** (`wizard_planetology_round_count`),
batched thematically from the ten-stage Planetology chain (Ben, 2026-07-22: fewer
rounds, too slow otherwise). Each round stacks its stages' charts and
explanations, then takes that round's preferences. Three further rounds carry the migration, the history and the economic substrate — § Rounds 4, 5 and 6 below.

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
  History ledger's Chain view (BL-211, history ledger) — the plots a player
  decided a world on are the plots they can reopen mid-campaign.

### The globe — and why it does not take input

The globe is BL-256 (wizard globe). The pane's right two-thirds is the world
itself: round 0 draws the **system** (star colour and size, orbits, body sizes)
so the screen is never empty while there is no body yet; rounds 1–2 draw the
**homeworld**, from the real tile raster once one has been built. It is the
primary view and the charts are the extras on top.

**It spins on a clock, and it takes no mouse input. That is the design, not a
gap** (Ben, 2026-08-10): the globe turns one revolution a minute on wall time,
frozen to 0 under `--verify` so a golden capture never races the animation.

There is **no pan**, on Ben's call, for a reason worth keeping: *an uncontrollable
globe tells the player that generation is slightly beyond their reach.* It says
the same thing the preferences model already says — **you set conditions here,
you do not steer** — so a draggable camera would quietly contradict the screen's
own premise in order to add a control nobody needed. The wizard resolves leans
against a seed; the globe should feel like something you are watching resolve,
not something you are operating.

**Render technique.** The globe is drawn as **48 meridian slices**, each
subdivided in latitude, every cell a quad — not a per-pixel inverse projection
into a texture. Both avoid projecting ~7,500 hexes as polygons against ImGui's
16-bit draw indices; the slice path gets there with less machinery.

## Rounds — System, Life, Culture, Empires, Industrialisation (Ben, 2026-09-08; reframed and renamed 2026-09-09)

**FIVE ROUNDS, AND THE SPENDING ROUND RETIRED (Ben, 2026-09-09).** The third planetology round —
*Inheritance*, which asked what the era before you already took and carried the **drawdown** lean —
is no longer a round of its own: its subject belongs with industrialisation, which is what draws a
world down in the first place. The chart chain keeps all three of its groups, because the in-game
History ledger reads the same table; only the wizard stops walking the third. **Owed, and recorded
rather than dropped: the drawdown lean is editable nowhere until round 5 is built.**

The wizard does not stop at planetology. Three further rounds carry the generation
phases — [`GENERATION_STRATEGY.md`](../generation/GENERATION_STRATEGY.md) § The eight
phases — into the same idiom the planetology rounds established: a primary view filling
the pane, charts as the extras on top, and **preferences, not parameters**.

**THE ROUNDS ARE NOT CONTINUOUS (Ben, 2026-09-09), and this reframes what was one round
into two.** Round 4 was originally specified as "a time-lapse of the first 4000 years,
ending at 1200 CE" — one round covering both the peopling of the world and the empires
that followed. Watching it built showed why that is the wrong cut: the two processes have
different subjects, different rules and different terminating conditions, and fusing them
produced a round that showed conquest with the migration already finished off-screen, and
then — once the migration was moved inside it — a round with **no conquest at all** and a
static final third. They are separate rounds now, each with its own span.

**Round 3 — Culture.** A 2D map replacing the globe, playing the peopling of an
empty world: where people started, the routes they took, and the cultures those routes
produced. Its authority is [`COLONISATION.md`](../generation/COLONISATION.md).

- **It ends when all land has some culture** — a derived terminating condition, not a
  calendar year, so the round is exactly as long as the filling took on this world. **The world
  then coasts to 400 BCE** holding what the migration left it, because round 4's span is stated
  (Ben, 2026-09-09; `../generation/CIVILISATION.md` § The span is 400 BCE to 1200 CE). The round's
  own span is **2400 BCE → 400 BCE**, two thousand years.
- **Coastal and overseas routes are the emphasis.** The first cultures should string out
  along shorelines and hop crude, short water crossings, because that is how people
  actually moved; an inland-first map is the tell that the walk is mispriced.
- **Migration spawns cultures.** A stream that travels far enough from its origin founds
  a new people derived from its parent, so the round's output is dozens of related
  cultures grouped by route — not the handful its cradles started with.
- **Most of the land ends habitable and peopled.** Unsettled ground is the exception
  marking hostile country, never the background state of half a continent.

**Round 4 — Empires.** The span **400 BCE → 1200 CE**, sixteen hundred years (Ben, 2026-09-09),
which is the second half of pass 1's three thousand six hundred and what this round owns alone:
polities contesting the world migration left them. The earlier text gave this round the whole
four thousand years; that was before the migration became a round of its own. Its authority is
[`CIVILISATION.md`](../generation/CIVILISATION.md). Origin is round 4's; this round is *communication → conquest or
diplomatic union → a stable dark age*, and that arc is its acceptance criterion. A run
reaching 1200 CE without that shape has failed even if every number is plausible.
**Asymmetry is completely fine and expected** — it is the deliverable, not a defect.

On the left, where the planetology rounds stack their charts, round 4 keeps a
**leaderboard** — how cultures grew and fell, on four metrics: **military might**,
**research speed**, **population**, and **share of the world owned**. It is the round's
chart surface, and it moves with the map.

**Round 5 — Industrialisation.** The same globe, at the epoch, gaining four things in
order: **metros growing** out of the population centres the history sacked and grew,
**colonial reach across water**, **firm markers with their charters**, and the
**market carve with its price field**. This is phase 6's search made watchable — the
player sees the landscape that was selected, not every candidate that was scored.

**Each pass round is rerollable, and rerolling re-runs the pass** rather than re-drawing
a cached one (Ben, 2026-09-08) — which is the whole reason § The wait is the round has to
be affordable rather than merely tolerable. A history you cannot reject is a history you
were assigned. Each carries its own reroll; the era carries a seed of its own
(`world_params::era_seed`) so a reroll varies the history without re-drawing the
planetology rounds above it.

**Begin becomes Next.** The wizard's commit press moves to the last round; every round
before it advances rather than commits.

### Leans per pass

Each round takes a lean per axis, resolved against the seed exactly as
`world_preferences` are (PLANETOLOGY.md § Preferences, not parameters). No raw
generated value is editable and no outcome is targeted; the axes name a *force*, and
§ Asymmetry is the deliverable still forbids steering to a result. The globe's
no-input rule holds for the same reason it always did: **you set conditions here, you
do not steer.**

### The wait is the round, not a loading bar

The planetology rounds re-run their chain as a pure throwaway preview on every control
move. **The pass rounds cannot**: the history sim is the most expensive pass in the
project, and a live preview per keystroke is not affordable at any budget.

So these rounds invert it. The player sets the leans, and the pass runs *inside the
round* with its output drawn as it computes. The wait is not hidden behind a bar — it
**is** the content. Accepting moves to the next round; rerolling runs it again.

**There is no Run button (Ben, 2026-09-09).** Arriving on the round IS the instruction
to run it, so the press that moves onto a pass round starts its pass — it is already
under way while the round's first frame draws. A button here asked a question with one
answer. Ben, 2026-09-08: *a watched wait needs no budget* — which is why the
generation-budget chain was dropped rather than deferred. What is still owed is that a
watched wait must be **worth watching**; a round that shows a frozen globe for ninety
seconds is worse than a bar, not better.

**Rounds stay causal.** Rerolling a pass round invalidates the rounds below it, as
rerolling a planetology round already re-draws the ones below it.

## Handoff — `start_new_game`

The wizard's "Begin", and the one and only generation call:

1. Rebase the sim clock (fresh `sim_loop`; speed from the Lua config).
2. `setup_world(m_pending_world_params)` — build the world; fills
   `m_generation_report` (presentation artefact, off the serialisation seam).
3. `load_economy()` — recipes + economy constants from Lua.
4. **Pre-game warm start** ([C3]): seed the balance history, then run
   `app::pre_game_ticks` (**80**) quarterly econ ticks, sliced across loading-screen
   frames, so every corp opens onto non-empty pools and live markets rather than a
   cold zero state. It runs **in spectate** — `corp_ai_params::spectating`, no corp
   seated — because the seat is decided from what the warm start produces.
   `run_verify` stays cold.
5. **Seat the player**: shortlist the specialists whose filed returns clear the
   viability floor, draw one against the world seed, and re-point `is_player` /
   `world::player_entity` onto it. Owned by CORPORATION_GENERATION.md § The spawn
   shortlist, and the seat.
6. Rebase the clock again (generation + warm-start wall time must not become
   in-game days), then `m_screen = in_game`.

Play opens on the corporation's home planet — the Planetary rung, home body
selected (CANVASES.md § Default state).

## The seat

The starting-corp **selection screen is retired** (Ben, 2026-08-26): which corporation
the player runs is drawn at random from a viability shortlist rather than picked. The
mechanism — the warm start in spectate, the floor, the draw, and what the reorder costs
in re-blessed goldens — is owned by
[`CORPORATION_GENERATION.md`](../generation/CORPORATION_GENERATION.md) § The spawn
shortlist, and the seat. This doc owns only the screen consequences:

- `app_screen::choosing_corp`, `draw_corp_choice_screen`, `build_corp_choices`,
  `apply_corp_choice` and the *Surprise me* press all go, along with
  `verify.show_corp_choice` and `scripts/verify/corp_choice.lua`.
- The three hard ordering constraints that pinned the old stage to a single frame
  dissolve with it. The seat now happens **after** background firms and **after** the
  warm start, which is what makes a profitability read possible at all — the old stage
  showed no balances precisely because it ran before any had moved.
- The loading screen gains a second phase: the carve, then the warm start it now hosts.
  What that phase shows while it runs is unbuilt and is BL-632's (warm-start progress).

