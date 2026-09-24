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
(main menu)  (New World wizard)  (carve, then validation run)  (play)
```

`building` is the loading screen the world is carved on (CORPORATION_GENERATION.md
§ Corporate seeding is watched); it also hosts the settle — phase 6's single validation run
on the selected landscape (GENERATION_STRATEGY.md § The eight phases, phase 6). The player then **picks the seat on
the corporation selection canvas at Begin**. See § The seat below.

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
  **The wizard OPENS on a rolled seed, not on a fixed one (Ben, 2026-09-10):**
  *"Let's lose the framing of seeded worlds... it allows the player to have a higher degree of
  chance when they do choose to reroll, and it gives the player the ability to see if their world
  will contain these types of structures."* A default of `0` makes every new game the same
  reference world unless the player thinks to press Roll, which turns rerolling into a thing you
  must know to do rather than the ordinary way in.
  **This does not touch the determinism rule.** The entropy stops here, exactly as the Roll
  button's already does — `world/*` stays a pure function of the seed, so save, replay and the
  multiplayer argument are untouched, and every harness and golden keeps passing its seed
  explicitly. Seed `0` still names the reference world; it is simply no longer what you get by
  accident.
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

## Rounds — System, Life, Culture, Empires, Exploration, Industrialisation (Ben, 2026-09-08; reframed and renamed 2026-09-09; a fourth pass round added 2026-09-13, BL-946)

**FIVE ROUNDS, AND THE SPENDING ROUND RETIRED (Ben, 2026-09-09).** The third planetology round —
*Inheritance*, which asked what the era before you already took and carried the **drawdown** lean —
is no longer a round of its own: its subject belongs with the last pass round, which is what draws
a world down in the first place. The chart chain keeps all three of its groups, because the in-game
History ledger reads the same table; only the wizard stops walking the third. **Owed, and recorded
rather than dropped: the drawdown lean is editable nowhere until the substrate pass is built.**

**SIX ROUNDS AS OF BL-946 (Ben, 2026-09-13, resolving NR-847/NR-857).** The Exploration span
(1200 → 1660 CE, [`EXPLORATION.md`](../generation/EXPLORATION.md)) used to run opt-in behind
`world_params::exploration_sim_enabled` (default false) with no round to show it — a real
generation pass, invisible to the player. The default flips to **true** and a new round —
**Exploration** — is inserted between Empires and the last round, which is renamed
**Industrialisation**. `wizard_round_count` 5 → 6 and `wizard_pass_round_count` 3 → 4.
**Every pass round is a lapse round** (BL-1068, round six plays the span): Industrialisation
plays its own span like the three before it, so `wizard_lapse_round_count` is 4.

The wizard does not stop at planetology. Four further rounds carry the generation
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
- **Kin cultures share a hue family** (BL-919, lineage palette). Dozens of related cultures
  under a dozen identity colours would be plaid, so the map's colour is derived once from the
  culture tree: hue comes from the root cradle, with the cradles spread evenly around the wheel;
  a daughter takes its parent's hue shifted by a fixed step (siblings alternate sides of the
  parent) and bounded to the family's own wedge; lightness steps down by depth, bounded so a deep
  lineage stays readable. A family reads at a glance and a member on a second look. The same hue
  family is what round 4 tints its culture base with beneath the polity fill.

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
**research speed**, **population**, and **share of the world's people held**. It is the
round's chart surface, and it moves with the map. **The board ranks by people, not
ground (Ben, 2026-09-15, NR-876):** a polity's share is its held population over the
population every living polity holds at that instant — the same arithmetic
`history_sweep` reads its own share by — with share of land as a second, qualifying
column. Share of land measures founding as much as conquest: read by population the
largest realms come out two to five points *more* concentrated, and most region-count
risers turn out to have been founding empty ground. The arc readout under the board
("the largest empire held N% of the world's people") prints its peak on the same column.

**Round 5 — Exploration (BL-946, Ben 2026-09-13).** The span **1200 → 1660 CE**, four
hundred and sixty years, on the same shared engine as round 4 — its authority is
[`EXPLORATION.md`](../generation/EXPLORATION.md). Where Empires asks who holds this
ground, Exploration asks who wants what someone else holds: conflict moves off the home
coast, treasuries consolidate at every capital, treaties bind, colonies and trade
provinces appear across water, and ports/navies/standing armies persist and decay. Its
own record (`generation_report::body_entry::exploration_timelapse`) is recorded once, at
the one call site that runs the span, and drawn on the same 2D map as rounds 3 and 4 —
same terrain base, same seat dots, same frontier — with its own battle/conquest/founding
counters rather than round 4's. `world_params::exploration_sim_enabled` defaults **true**
(BL-946; it was opt-in and invisible to the player before).

**Round 6 — Industrialisation.** The span **1660 → 1960 CE**, three hundred years, and its
authority is [`INDUSTRIALISATION.md`](../generation/INDUSTRIALISATION.md). **A time-lapse that ends
on the seeded map (Ben, 2026-09-15)** — not the still globe this round was first specified
as, because its three beats are things that happen across a span, and a globe shows a
state. It draws on the same 2D map as rounds 3–5, over the same terrain base, and shows
the phase's three beats as they occur:

- **Industrialisation** — large cities light and accumulate industry points; rail spreads
  along the corridors they pay for.
- **Mass migration** — people stream from countryside into cities and across borders, and
  the cities' culture tint turns mixed as they arrive.
- **Decolonisation** — subjects refuse renewal and stand as their own polities; wars over
  empire flare, and on some worlds one becomes general.

**Its run is the whole build (BL-1068, round six plays the span).** Rounds 3–5 each stop
generation at their own close. Round 6 sets no stop: it runs the Industrialisation span and
then everything after it — borders, roads, companies — which is the world Begin builds. Its
record (`generation_report::body_entry::industrialisation_timelapse`, with its own
battle/conquest/founding counters) is recorded once, at the call site that runs the span, and
plays on the same map as rounds 3–5. The span runs only when Exploration ran and
`world_params::industrialisation_span_enabled` is on (the default). The record is write-only:
nothing at world setup reads it, so a watched and an unwatched build are the same build.

**The round closes on the epoch's opening map.** When the playhead reaches 1960 the firms
the search chartered from each city's budget draw in place, with the market carve and its
price field — the landscape that was selected, never the candidates that were scored. The
player's last sight before Begin is the map they will play on.

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

**THE WAIT IS A WAIT, AND THE LAPSE IS THE ROUND (Ben, 2026-09-16, reversing the earlier
reading).** The pass was once drawn as it computed, on the argument that a watched wait needs
no budget. Watching it decided otherwise: *"there is a clear phase where the simulation is done
rapidly, and this sort of breaks the narrative flow of the time-lapse. Let us use that to our
advantage, and separate each part with an otherwise completely blank Loading X Round. This way
the player can see that they have to wait, and what they are watching is a time-lapse of that
very fast calculation."*

So a pass round is two moments, and they do not overlap. **The wait** is one centred line —
*Loading the Culture round*, *Loading the Empires round*, *Loading the Exploration round*,
*Loading the Industrialisation round* — and
under it a progress bar (Ben, 2026-09-18: *"wire in a progress bar for 'Loading x round'"*), and
nothing else on the surface: no map, no board, no stage list, no year counter. The bar is the
building screen's pair, read from the round's own run: an outer bar over the passes the run
reports, which only moves forward, and a thinner one under it for the sim's years within a span,
drawn only while a span is counting them. **The lapse**
begins when the record is whole and plays it from its first year at the pace the player chose.
The calculation is fast and jerky; the lapse is paced and readable; showing the first as though
it were the second made the second impossible to follow, and hid the hand-over between rounds
entirely. Accepting moves to the next round; rerolling runs it again.

**There is no Run button (Ben, 2026-09-09).** Arriving on the round IS the instruction
to run it, so the press that moves onto a pass round starts its pass — it is already
under way while the round's first frame draws. A button here asked a question with one
answer. Ben, 2026-09-08: *a watched wait needs no budget* — which is why the
generation-budget chain was dropped rather than deferred. What is still owed is that a
watched wait must be **worth watching**; a round that shows a frozen globe for ninety
seconds is worse than a bar, not better.

**Rounds stay causal.** Rerolling a pass round invalidates the rounds below it, as
rerolling a planetology round already re-draws the ones below it.

**The render follows the calculation, it no longer races it (Ben, 2026-09-11, narrowed 2026-09-16).**
A pass round does not compute its record and then play it back; the map begins drawing the
moment the pass begins, and it advances in **fixed ticks** at one constant rate for the whole
span, always **behind** the year the pass has reached and never ahead of it. The rate is set so
the whole span plays in the duration the player has chosen; if the pass is slower than that the playhead
waits at the computed frontier, so a longer round is acceptable and a playhead that overtakes
the calculation is not. The sim publishes its growing record through a **write-only tap** — the
same contract the market carve uses to fill the loading screen — so a watched run and an
unwatched run are byte-identical, and the standing determinism rule is untouched. Once the pass
has landed, the record stays: the round can be paused and scrubbed at the player's own pace. There is no Restart button on either pass round (Ben, 2026-09-11): a scrubber makes it redundant, and its row belongs to the ranking board.

**THE PACE IS THE PLAYER'S (Ben, 2026-09-13, revised 2026-09-16).** A lapse that runs too
fast to follow is a wait spent rather than watched, so every pass round carries the same
three-way control beside its transport: **30 s, 1 m, 1 m 30 s of wall clock for the whole
span**, a minute by default. It sets the WALL CLOCK, not years per second — a longer span at
the same setting moves faster rather than taking longer, because what a viewer budgets is
their own time. It changes the autoplay rate and nothing else; the scrubber still goes
anywhere, and a live round still draws as fast as its pass computes.

**A ROUND OPENS ON THE GROUND THE ROUND BEFORE IT LEFT (Ben, 2026-09-16).** The rounds are one
continuous history and the surface says so: the previous round's final map is carried into the
next and painted under ground nobody holds yet, fading out over the opening tenth of the new
span while the new round's own holders fade in over the same stretch. It is a cross-fade, not a
cut — the Empires round opens on the migration's peoples and watches city states organise them
(BL-920 made that literally what happens), and the same carry runs Empires into Exploration and
Exploration into Industrialisation. A round whose predecessor was never run carries nothing
and draws at full strength from its first frame.

**The pass rounds draw the ground, not only the fill (Ben, 2026-09-11).** Rivers and the
landform relief — mountains, highlands, the barriers the walk and the campaign both price — are
drawn beneath the culture or polity fill on every pass round, 3 to 6, and
the fill is a tint over that ground rather than a flat colour that hides it. A frontier, a road
and a bridge are legible only against the terrain they cross.

## Handoff — `start_new_game`

The wizard's "Begin", and the one and only generation call:

1. Rebase the sim clock (fresh `sim_loop`; speed from the Lua config).
2. `setup_world(m_pending_world_params)` — build the world; fills
   `m_generation_report` (presentation artefact, off the serialisation seam).
3. `load_economy()` — recipes + economy constants from Lua.
4. **The winner's validation run** (BL-978, warm start retired): seed the balance
   history, search the landscape (phase 6), then run `app::validation_ticks`
   quarterly econ ticks on the winner — the length and its measurement are
   `../economy/ERAS.md` § The opening position — so every corp opens onto non-empty
   pools and live markets rather than a cold zero state. Batched across
   loading-screen frames (a tick on a searched landscape is ~0.9 s in Release, so
   the run in one frame would trip Windows' hang kill). It runs **in spectate** —
   `corp_ai_params::spectating`, no corp seated — because the seat comes after the
   run, so its card can show the figures the run files. `run_verify` stays cold.
5. **Seat the player**: shortlist the specialists whose ground clears the viability
   floor on phase 6's static landscape score, ranked by it; draw one against the
   world seed, and re-point `is_player` / `world::player_entity` onto it. Owned by
   CORPORATION_GENERATION.md § The spawn shortlist, and the seat.
6. Rebase the clock again (generation + validation-run wall time must not become
   in-game days), then `m_screen = in_game`.

Play opens on the corporation's home planet — the Planetary rung, home body
selected (CANVASES.md § Default state).

## The seat

**The player picks the seat, on a corporation selection canvas at Begin** (Ben, 2026-09-09:
*"Begin should go directly to a 'corporation selection' interactive canvas"*; asked whether that
reverses the 2026-08-26 retirement of the selection screen: *"Yes, it's time to reverse that
ruling."*). The random draw is retired as the seat mechanism. The spawn shortlist survives as
**what the canvas offers** — the viability floor still filters, the static score ranks what
passes, and the player chooses among it rather than being drawn for.
[`CORPORATION_GENERATION.md`](../generation/CORPORATION_GENERATION.md) § The spawn shortlist,
and the seat owns the shortlist; this doc owns only the screen.

**The floor reads the ground, not a trading record** (Ben, 2026-09-16, NR-881). The settle is
too short for a trailing net to exist at any window, so the shortlist gates and ranks on the
static landscape score phase 6 already computed for each seat. Trailing figures stay on the seat card
as information; they are never the gate.

**The canvas draws over a world that already exists.** It sits over the landscape the final
wizard round selected, so the wizard hands a **world** forward rather than a record, and Begin
takes that world instead of building a second one (Ben, 2026-09-16, NR-811). A control move
that changes the world invalidates what the wizard holds; a held world that no longer matches
the controls is worse than a slow one.

**The seat comes after the firms and after the settle**, which is what lets the canvas show a
seat worth weighing at all — the 2026-08-26 stage showed no balances because it ran before any
had moved.

**The canvas chooses the firm the player *is*** (Ben, 2026-09-17, NR-885; `../CONCEPT.md` §
Player identity). The seat is an identity, not an address: the shortlist is a list of corporations,
and picking one is becoming it.
