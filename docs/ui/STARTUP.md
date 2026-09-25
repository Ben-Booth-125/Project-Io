# Project Io — Startup (Entry Screens)

> **Settles:** which screens sit between launch and the first frame of play, and in
> what order · what the main menu offers · what the player chooses in the world
> wizard and what its globe is for · what round 6's tail builds behind its lapse (the
> carve, the search, the settle) and what Begin itself does · how the player comes to
> be seated on a corporation.
> **Not here:** the in-game shell (LAYOUT) · the view play opens on (CANVASES,
> PLANETARY) · what the generator produces (generation/*).
> **Confused with:** MENU.md, LAYOUT.md.

The app's entry flow — everything between launch and the first frame of play.
Written against `src/core/app.{hpp,cpp}` and `src/ui/startup_screens.cpp`. See
[LAYOUT.md](LAYOUT.md) for the in-game shell and [CANVASES.md](CANVASES.md) for
the opening view the flow hands over to.

---

## The screen state machine

`app_screen` (`app.hpp`) has five states; `run()` opens on **`menu`**:

```
menu  →  generating  →  building  →  choosing_seat  →  in_game
(main menu)  (New World wizard)  (the cold build, or the wait on round 6)  (the seat canvas)  (play)
```

`building` is the loading screen of the **cold path** — menu → Begin with no wizard behind it,
or `--autostart` — where the world is carved (CORPORATION_GENERATION.md § Corporate seeding is
watched) and then finished: the landscape search, phase 6's single validation run on the
selected landscape (GENERATION_STRATEGY.md § The eight phases, phase 6), the settle. When the
wizard ran, all of that work happened inside its last round (§ Round 6; § Handoff), and
`building` is only the wait Begin makes if that round's worker has not yet landed. The player
then **picks the seat on the corporation selection canvas at Begin**. See § The seat below.

**Only `in_game` simulates.** On the menu the loop just pumps events and draws. The wizard
builds the world forward, one round at a time (§ The world cache), and its last round proves
the field with the twelve pre-game ticks (§ Handoff) — but those have no calendar meaning
(`../economy/ERAS.md` § The opening position). The economy's registry is loaded before round 6
launches and banded in that round's worker, and the twelve settle ticks run there too
(§ Handoff); only the sim clock and the presentation half of setup are set up at Begin. The
clock is rebased at handoff, so time spent reading either screen never lands as elapsed
in-game days. `run_verify()` jumps straight to `in_game`
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

The wizard is BL-167 (planetology pass). Its first **two rounds** (`wizard_planetology_round_count`)
— System and Life — are batched thematically from the ten-stage Planetology chain (Ben,
2026-07-22: fewer rounds, too slow otherwise). Each round stacks its stages' charts and
explanations, then takes that round's preferences. Four further rounds carry the migration, the
empires, the exploration age and the industrialisation — § Rounds below.

- **Preferences, not parameters** (`world_preferences`,
  `src/world/planetology.hpp`): a named lean per axis ("Dimmer", "Metal-rich"),
  resolved against the seed by `resolve_preferences` — no raw generated value
  is editable. Model authority: PLANETOLOGY.md § Preferences, not parameters.
- **The charts preview; the Life round builds.** Every control move re-runs the chain as a
  pure, throwaway preview (`refresh_wizard_preview` → `resolve_preferences` +
  `preview_system`) for the charts; the resolution rerolls internally until the homeworld
  clears the Earth-like floor, and surfaces what that cost (`resolved_world::attempts`). The
  Life round then builds the **real** Life-gate world — the star, the system, the homeworld's
  tiles with their deposits, its rivers — into the first slot of § The world cache, rebuilding
  it when a control moves: the globe's homeworld raster packs off it, and the Culture round
  takes it forward rather than building its own. `m_world` is untouched until Begin adopts the
  last slot.
- **Rounds are causal downward.** A change on round A invalidates every round below it and
  none above; Back is a plain step up the stack that keeps every record and world it passes
  (§ The world cache), so it recomputes nothing.
- **Charts** come from `ui::generation_charts`, shared verbatim with the
  History ledger's Chain view (BL-211, history ledger) — the plots a player
  decided a world on are the plots they can reopen mid-campaign.

### The globe — and why it does not take input

The globe is BL-256 (wizard globe). The pane's right two-thirds is the world
itself: round 1, System, draws the **system** (star colour and size, orbits, body sizes)
so the screen is never empty while there is no body yet; round 2, Life, draws the
**homeworld**, from the tile raster of the Life-gate world that round builds (§ New World
wizard). It is the primary view and the charts are the extras on top. **The globe does not
leave with the Life round (Ben, 2026-09-24):** it holds in the pane through the Culture
round's wait and cross-fades into that round's map under a 2400 BCE year stamp, so the first
thing the map shows is the world the globe was showing.

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

## Rounds — System, Life, Culture, Empires, Exploration, Industrialisation

The wizard has **six rounds** (`wizard_round_count`; Ben, 2026-09-08, the set settled
2026-09-09 and 2026-09-13): two planetology rounds — **System** and **Life** — and four lapse
rounds — **Culture**, **Empires**, **Exploration** and **Industrialisation**
(`wizard_pass_round_count` and `wizard_lapse_round_count`, both 4). **Every pass round is a
lapse round:** each plays its own span, on the same 2D map, and the spans meet end to end.

| Round | Span | Authority |
|---|---|---|
| 3 — Culture | 2400 BCE → 400 BCE | [`COLONISATION.md`](../generation/COLONISATION.md) |
| 4 — Empires | 400 BCE → 1200 CE | [`CIVILISATION.md`](../generation/CIVILISATION.md) |
| 5 — Exploration | 1200 → 1660 CE | [`EXPLORATION.md`](../generation/EXPLORATION.md) |
| 6 — Industrialisation | 1660 → 1960 CE | [`INDUSTRIALISATION.md`](../generation/INDUSTRIALISATION.md) |

The wizard does not stop at planetology. The four lapse rounds carry the generation
phases — [`GENERATION_STRATEGY.md`](../generation/GENERATION_STRATEGY.md) § The eight
phases — into the same idiom the planetology rounds established: a primary view filling
the pane, charts as the extras on top, and **preferences, not parameters**.

**INHERITANCE IS NOT A ROUND (Ben, 2026-09-09; held 2026-09-24).** The third planetology round —
*Inheritance*, which asked what the era before you already took — is retired and is not revived:
its subject is what the pass rounds draw down. The chart chain keeps all three of its groups,
because the in-game History ledger reads the same table; the Life round walks *water* through
*legacy*, and Spend is parked in the third group for the ledger alone. **The drawdown lean lives
on the Life round, under the Legacy fold (Ben, 2026-09-24):** it is a planetology input — Spend
scales the endowments before a tile is laid — so it belongs where the world is built, and the
world the Culture round inherits already carries it. No later round takes a new axis for it.

**THE MIGRATION AND THE EMPIRES ARE SEPARATE ROUNDS (Ben, 2026-09-09).** The peopling of the
world and the polities that contest it have different subjects, different rules and different
terminating conditions. Fused into one round they show either conquest with the migration
already finished off-screen, or a migration with no conquest at all and a static final third;
so each is a round of its own, with its own span.

**No named individual appears in any round (Ben, 2026-09-24).** No ruler, founder or chartering
magnate is named on a board, in a ticker line or at a mark. PEOPLE.md's rule is the reason: a
person exists only where a role gates something, and no role gates anything in a lapse — a
realm, a people, a creed and a firm are the actors the rounds name, and a face on one would be
decoration. **Every lapse header names the body** the round is playing on.

**Round 3 — Culture.** A 2D map in the globe's place — the globe holds through this round's
wait and cross-fades into the map under a 2400 BCE year stamp (Ben, 2026-09-24; § The globe) —
playing the peopling of an empty world: where people started, the routes they took, and the
cultures those routes produced. Its authority is
[`COLONISATION.md`](../generation/COLONISATION.md). The map opens on the Life-gate world the
round inherited (§ The world cache), deposits and all, so the ground a people settles is the
ground the campaign will mine. The round's record
(`generation_report::body_entry::migration_timelapse`) is built from the same fold on every
full run, not only on the wizard's path, and is saved in the app envelope with the report, so
a later Ages replay can read it (Ben, 2026-09-24).

- **It ends when all land has some culture** — a derived terminating condition, not a
  calendar year, so the round is exactly as long as the filling took on this world. **The world
  then coasts to 400 BCE** holding what the migration left it, because round 4's span is stated
  (Ben, 2026-09-09; `../generation/CIVILISATION.md` § The span is 400 BCE to 1200 CE). The round's
  own span is **2400 BCE → 400 BCE**, two thousand years.
  **The playhead is clamped at 400 BCE (Ben, 2026-09-24).** A world still filling when round
  4's span opens plays to 400 BCE and says so in an overrun caption. It is a presentation
  clamp, not a record clamp — the record runs on to its own end, and the Empires round takes
  the world as the migration actually left it.
- **Every people begins with a `cradle` (Ben, 2026-09-24):** one event per people, dated 2400 BCE
  at the cradle seat, carrying the cradle's name and its domestication package, so the ticker's
  first lines say where each people came from and what it brought.
- **Coastal and overseas routes are the emphasis.** The first cultures should string out
  along shorelines and hop crude, short water crossings, because that is how people
  actually moved; an inland-first map is the tell that the walk is mispriced.
- **Migration spawns cultures, and each founding draws its route (Ben, 2026-09-24).** A stream
  that travels far enough from its origin founds a new people derived from its parent, so the
  round's output is dozens of related cultures grouped by route — not the handful its cradles
  started with. A founding draws a fading line from its people's previous region to the new
  one, dashed where the hop crossed water: the region-grain kin line, parent region to
  daughter region, not the walk itself. A region that changes people shows its parent's hue
  until the split year, then its own. The daughters the fold census merges back into a parent
  do not reach the ticker; the board's census counts them.
- **Its board is peoples and Homeland.** The rows are peoples and the column is each one's
  homeland; there are no battle cells, because nothing fights in a migration.
- **Most of the land ends habitable and peopled.** Unsettled ground is the exception
  marking hostile country, never the background state of half a continent.
- **Kin cultures share a hue family** (BL-919, lineage palette). Dozens of related cultures
  under a dozen identity colours would be plaid, so the map's colour is derived once from the
  culture tree: hue comes from the root cradle, with the cradles spread evenly around the wheel;
  a daughter takes its parent's hue shifted by a fixed step (siblings alternate sides of the
  parent) and bounded to the family's own wedge; lightness steps down by depth, bounded so a deep
  lineage stays readable. A family reads at a glance and a member on a second look. The same
  palette is built for every lapse round, not only this one: rounds 4–6 tint a culture base
  with it beneath the polity fill, and a realm's own hue is drawn from its founding family's
  wedge (§ Identity across the rounds).

**Round 4 — Empires.** The span **400 BCE → 1200 CE**, sixteen hundred years (Ben, 2026-09-09),
which is the second half of pass 1's three thousand six hundred and what this round owns alone:
polities contesting the world migration left them. Its authority is
[`CIVILISATION.md`](../generation/CIVILISATION.md). Origin is round 3's; this round is *communication → conquest or
diplomatic union → a stable dark age*, and that arc is its acceptance criterion. A run
reaching 1200 CE without that shape has failed even if every number is plausible.
**Asymmetry is completely fine and expected** — it is the deliverable, not a defect.

On the left, where the planetology rounds stack their charts, round 4 keeps a
**leaderboard** — how realms grew and fell, on four columns: **People** (share of the world's
people held), **Land** (share of ground, the qualifying column), **Pop** (the population held)
and **Might** (military might). Each row is a realm under its coined name, and a row whose
realm holds a hard border is bold (§ Identity across the rounds). It is the round's chart
surface, and it moves with the map. **The board ranks by people, not
ground (Ben, 2026-09-15, NR-876):** a polity's share is its held population over the
population every living polity holds at that instant — the same arithmetic
`history_sweep` reads its own share by — with share of land as a second, qualifying
column. Share of land measures founding as much as conquest: read by population the
largest realms come out two to five points *more* concentrated, and most region-count
risers turn out to have been founding empty ground. The arc readout under the board
("the largest empire held N% of the world's people") prints its peak on the same column,
**and it is the only summary a round gets (Ben, 2026-09-24):** no round closes on a card, no
close-of-round event kind exists, and accepting simply moves on.

**The Empires map draws less than the rounds after it (Ben, 2026-09-25, after walking the
round: "the Empires round shows too much").** It draws the ground, the realm fill over the
culture base, the frontiers and hard borders, the rivers, the promoted roads, the cross-border
trade links, the seat dots, the schism crack and the civilisation diamond. It does **not** draw
the caravan glyphs, the seat-captured ring, the capital slide (a moved capital's dot simply sits
at the new seat) or the fleets, harbours and treaty arcs. Those belong to the rounds whose
subject they are.

**Every lapse round carries a legend (Ben, 2026-09-25):** a key naming each layer its map draws,
so what is on the map never has to be guessed. **The sea is a deep blue on every lapse round
(Ben, 2026-09-25)**, so water reads as water and what sails on it reads as being at sea.

**Round 5 — Exploration (BL-946, Ben 2026-09-13).** The span **1200 → 1660 CE**, four
hundred and sixty years, on the same shared engine as round 4 — its authority is
[`EXPLORATION.md`](../generation/EXPLORATION.md). Where Empires asks who holds this
ground, Exploration asks who wants what someone else holds: conflict moves off the home
coast, treasuries consolidate at every capital, treaties bind, colonies and trade
provinces appear across water, and ports/navies/standing armies persist and decay. Its
own record (`generation_report::body_entry::exploration_timelapse`) is recorded once, at
the one call site that runs the span, and drawn on the same 2D map as rounds 3 and 4 —
same terrain base, same seat dots, same frontier — with its own battle/conquest/founding
counters rather than round 4's. `world_params::exploration_sim_enabled` defaults **true**.
**It draws the fleets that are there (Ben, 2026-09-24).** Everything below is read from the
record, and the span is unmoved by any of it.

- **Ties.** A colonial tie is a dashed line from overlord seat to subject seat, laid when a
  subject is bound and lifted when it is freed or its overlord ends; a treaty is an arc
  between the two capitals for the treaty's term, from formed to broken. A tie that changes
  layer is tinted, not pinged (§ Identity across the rounds).
- **Hulls and harbours.** A realm's navy is a **hull glyph** at its capital that scales with
  the navy it holds; its harbour is a **harbour mark** at the capital that silts as the
  capital's port stock decays — both series on the polity sample, so both move as the sample
  does.
- **Crossings and landings.** A wet campaign draws a **sail crossing** from its staging hub to
  its target in the year it launches; a seat captured across water is drawn as a **landing**
  rather than the inland ring.
- **The lane.** A sea leg used often enough to open a lane (the sea-lane tier,
  [`EXPLORATION.md`](../generation/EXPLORATION.md)) bakes a persistent **lane line**, distinct
  from a tie: a tie runs between two polities and goes when the bond does; a lane runs between
  two shores and stays. **The lane is drawn as a wide, soft sea-blue band (Ben, 2026-09-25):**
  most lanes run the same line as a colonial tie, so the tie reads as dashes on the band rather
  than hiding a thin lane under it. A corridor the treasury promotes to Post Road pulses once along its
  length, as it does on round 6.

**Round 6 — Industrialisation.** The span **1660 → 1960 CE**, three hundred years, and its
authority is [`INDUSTRIALISATION.md`](../generation/INDUSTRIALISATION.md). **A time-lapse that ends
on the seeded map (Ben, 2026-09-15)** — not the still globe this round was first specified
as, because its three beats are things that happen across a span, and a globe shows a
state. It draws on the same 2D map as rounds 3–5, over the same terrain base. The phase's
three beats are the span's design, and INDUSTRIALISATION.md states them — **industrialisation**
(cities accumulate industry points and the realms holding them cross into the industrial
materials band), **mass migration** (people stream from countryside into cities and across
borders) and **decolonisation** (subjects refuse renewal and stand as their own polities; wars
over empire flare, and on some worlds one becomes general).
**The round draws what the record carries of each, and no more (Ben, 2026-09-24).**

- **Company creation flashes.** A `works_chartered` event at a region's anchor, in-span, each
  time the region's industry points cross the next multiple of a fixed fraction of the
  **running** price — the world's stock so far over the charter divisor, the same price the
  close settles on, so an early charter and a late one are priced by one rule — capped per
  region per round and gated exactly as a furnace crossing is. **Each is a real charter (Ben,
  2026-09-25, NR-925):** the firm is founded at that crossing and its points are debited, so a
  flash in 1720 is a firm that exists from 1720 (INDUSTRIALISATION.md § Firms are chartered in
  the span; BL-1122). The fraction is measured against the 1960 firm
  count before it is fixed. **At the close the
  flashes are the real charters** — the firms the search chartered from each city's budget,
  richest centre first, each at its anchor tile, each dated by pairing a region's k-th real
  firm with its k-th in-span crossing — and never the world-gen roster, which the player never
  meets. Heat blooms at a charter are not drawn.
- **The rung crossing is narrated.** A generated world lights no region furnace; what the span
  computes is the **realm's** crossing into the industrial materials band, and that is the
  event the round marks — at the realm's capital, from its crossing year — and the ticker
  names (*"the realm of Y lights its furnaces at X"*). On a seed where no realm crosses, the
  layer is honestly empty rather than filled from a lower rung.
- **The map heats industry points.** A span on a world no realm industrialises draws no
  crossing, and its mark layer stays empty while the points climb, so each realm's ground is
  stippled with sparks: the fraction of its ground that sparks is set by its points per region
  held over the record's peak density (square-rooted, so the skewed low end still shows). The
  heat is by realm territory, not by region, because the record samples points per polity
  only. Sparks, not a wash, so the polity colours and frontiers stay readable beneath; a spark
  lit at one heat stays lit at every higher one, so the heat visibly grows across the span.
- **The board** gains an industry column: each polity's industry points at the step, beside
  People, Land, Pop and Might, and only on a record that carries points, so the earlier
  rounds keep their board.
- **Roads pulse when promoted; rail only if laid.** A corridor the treasury promotes to Post
  Road pulses once along its length — a mark on the thing, not a ping over it (§ Identity
  across the rounds). Rail is drawn only if the span lays it; the span's corridors are the
  road ladder the earlier rounds already draw. The migration and decolonisation beats show
  through what every lapse round draws — the culture base under the fill, the frontiers and
  the ties round 5 lays — and take no layer of their own.

**Its run is the whole build, and its record lands at the 1960 close.** (BL-1068, round six
plays the span; Ben, 2026-09-24.) Rounds 3–5 each stop generation at their own close. Round 6
sets no stop: its worker runs the Industrialisation span and then everything after it —
borders, roads, the old-road stamp, companies, the finishing of the outer bodies, and then the
landscape search and the settle that finish a campaign world (§ Handoff) — so the world it
hands forward is the world play opens on. The span is most of the round's meaning and a small
part of its wall time, so the two are split: the record is published whole at the span's last
year, and **the lapse starts playing there while the same worker builds the tail behind it**
into § The world cache. Landing swaps only the world slot; a caption beside the playing map
names the one step under way ("Drawing borders", "Searching the landscape", "Proving the
field") and nothing else. **The tail lands within 35 s of the round's arrival (Ben,
2026-09-25, after a round 6 that took "just over one minute")**; the road pass is where the
time goes, and the road rule (LOGISTICS.md § 4, the detour test) is how it gets there. Its record
(`generation_report::body_entry::industrialisation_timelapse`, with its own
battle/conquest/founding counters) is recorded once, at the call site that runs the span, and
plays on the same map as rounds 3–5. The span runs only when Exploration ran and
`world_params::industrialisation_span_enabled` is on (the default). The record is write-only:
nothing at world setup reads it, so a watched and an unwatched build are the same build.

**The round closes on the map you will play, by construction (Ben, 2026-09-24).** The search
and the settle run inside the round, so when the tail lands the firms the search chartered
from each city's budget flash in place (the close above), over the market carve and its price
field — the landscape that was selected, never the candidates that were scored. The player's
last sight before Begin is the map they will play on, and Begin adds nothing to it.

**Each pass round is rerollable, and rerolling re-runs the pass** rather than re-drawing
a cached one (Ben, 2026-09-08) — which is the whole reason the wait (§ The wait, then the
lapse) has to be affordable rather than merely tolerable. A history you cannot reject is a history you
were assigned. **A reroll is per stage (Ben, 2026-09-24):** rerolling round N re-seeds span N
and nothing else. Each span carries a seed of its own — `world_params::span_seed[4]`, one
each for the migration, Empires, Exploration and Industrialisation, folded into that span's
own seed so that all zeros is the unrerolled world (`era_seed` stays as the legacy term). **The
Culture round carries no Reroll (Ben, 2026-09-25, NR-931).** The walk is seed-free by design
(COLONISATION.md § No actor, and no infrastructure: where people go is a consequence of the
ground, never a roll), so a reroll there could re-coin the peoples' names, tongues and temper but
never move the map — a button that promises a different migration and cannot give one. The
migration is the world's consequence, and the Life round's reroll is how a player rejects it.
`span_seed[0]` stays folded and zero-neutral, so the unrerolled world is unchanged by the ruling.
A later reroll leaves the migration alone. The
rounds above the rerolled one are untouched; the rounds below it are invalidated; the reroll
starts from the closing world of the round before it (§ The world cache), which is why nothing
a round shows is ever a calculation a later round contradicts. A lean change on a round takes
the same path: it invalidates that round onward, relaunches it at once, and touches nothing
above.

**Begin becomes Next.** The wizard's commit press moves to the last round; every round
before it advances rather than commits.

### Identity across the rounds

A realm is one thing from the round that founds it to the nation Begin makes of it, and the
map says so by carrying five things by id — name, colour, shade, border and mark — rather than
re-deriving any of them per round (Ben, 2026-09-24).

**A realm's name is coined at founding, in its founding culture's tongue, and never changes.**
The coining runs over the founding region's culture speech in the founding year — the same
lexicon the region names come from — and the board and the ticker print it; a resumed span
carries it by id, and the nation Begin makes of the realm inherits it verbatim
(NATION_GENERATION.md owns the fold). The seat dot follows the capital — it slides when the
capital moves — but **the name never follows the capital**. A record resumed from an earlier
span does not re-announce its living realms as risings; it inherits them silently, dated from
the span's start. An ownerless founding reads *"A people settle at X"*.

**Colour is lineage, both ways.** A realm's hue is drawn from its founding culture family's
wedge of the wheel — the lineage palette § Round 3 describes, built for every lapse round —
with an offset inside the wedge chosen greedily, so kin realms read as kin; and beneath every
polity fill on rounds 4–6 sits a dull culture base in the lineage hue of each region's
plurality people, read from the record's culture changes. **Slots are pinned:** a realm's
colour slot lives on the record from the round that seats it, the greedy table fills only the
slots no pin holds, and a **clash** — two *pinned* realms the new record makes neighbours while
they share a slot — re-slots the one with the smaller people share at the round's opening step.
A fresh realm cannot clash except on a spill — the palette exhausted around it, where it shares a
neighbour's slot and no re-slot happens: it is coloured around the pins it touches, so the only pair the walk
can find sharing a slot is two pins that were never neighbours in the record that slotted them
(the 2026-09-24 ruling's "pinned/fresh" wording named a case the walk never produces; the
pinned/pinned reading is the one the rule was built and measured on — 2026-09-25, on the cold
review). A dead realm's slot is retired only
for a newcomer seated inside its last-held ground. The same slot is what the nation's colour
is pinned to at Begin and on load, so the national border band (LENSES.md § The Country lens
has retired — national borders are chrome), the seat map and the wizard show one colour for one
realm.

**Shade is a ratchet.** A realm darkens one rung at a named moment — when a civilisation forms
under it, or when it crosses the *rose* threshold the arc readout already reads by (a peak at
least double its start and three regions more) — and holds that rung everywhere else, so the
shade carries by id and a viewer reads "this one has grown" from the fill alone. There is no
continuous tint. **The start is the realm's founding size** — the first sample the record that
founded it holds — carried by id across the seams exactly as the rung is, so a realm crosses
*rose* once in its life and never again by re-basing at a later span's opening; a span that
re-based the start at its own first year would let one steady climb earn a rung per round
(2026-09-25, on the cold review).

**A hard border is people share, with hysteresis.** A realm whose share of the world's people
stands above a fixed threshold draws hard: the two frontier passes lay 2 px of dark and a 1 px
inner stroke in the realm's colour on any edge **between two different realms** either side of
which is hard, and the board bolds the same rows. **A coast is not a frontier (Ben, 2026-09-25,
NR-937):** a hard realm's shoreline draws as any other coast. Hysteresis keeps a realm sitting on
the threshold from flickering across it. The threshold was **measured** across the curated seeds
before it was fixed (Rule 0b): **on at 10% of the world's people, off at 6%** (Ben, 2026-09-25,
NR-937) — one to six realms on six of sixteen worlds at 1000 CE, none on the city-state worlds.
The flag carries into round 5 by id.

**There is no blanket ring (Ben, 2026-09-16, ruled watching round 4 run), and four kinds earn
a mark, each its own glyph (Ben, 2026-09-24).** A white ring at every event inside the marker
window — one mark for a realm dying, a treaty taken and a trade route opening alike — reads as
a snowstorm with fifteen event kinds carrying a region and the trade links firing several a
year, and pulls the eye off the border changes, which are what a time-lapse of who-held-what is
for. **The borders are the story.** It is the ring the map does without, not the record:
every event still crosses, the ticker still names it with its year and place, the arc readout
still counts it. A mark is earned by kind, each with its own glyph: a **seat captured** draws a
ring in the winner's colour at the fallen seat; a **break-away or schism** draws a crack from
the parent seat to the successor's; a **civilisation formed** draws a two-tone diamond that
stays at the coining region; a **capital moved** slides the seat dot from old to new; a **works
chartered** (round 6, § Round 6) draws a works — a bright block with a furnace-toned stack —
at the region's anchor, fading over the window, and the close's real charters wear the same
glyph so the two read as one kind of thing (bright rather than furnace-toned, because the
heat stipple and the ember squares are furnace-toned and a flash must read apart from the
ground it lands on). A layer's transition is a mark on the thing, not a ping over it: a
corridor promoted to Post Road pulses once along its length, a tie that changes layer is
tinted, and a **realm's rung crossing** is the ember square at its capital from its crossing
year (the same state mark a region furnace earns). A realm ending and a creed preached do not
mark; nothing else does.

### Leans per pass

Each round takes a lean per axis, resolved against the seed exactly as
`world_preferences` are (PLANETOLOGY.md § Preferences, not parameters). No raw
generated value is editable and no outcome is targeted; the axes name a *force*, and
GENERATION_STRATEGY.md § Asymmetry is the deliverable still forbids steering to a result. The globe's
no-input rule holds for the same reason it always did: **you set conditions here, you
do not steer.**

### The wait, then the lapse

The planetology rounds re-run their chain as a pure throwaway preview on every control
move. **The pass rounds cannot**: the history sim is the most expensive pass in the
project, and a live preview per keystroke is not affordable at any budget.

**THE WAIT IS A WAIT, AND THE LAPSE IS THE ROUND (Ben, 2026-09-16).** A pass round is two
moments, and they do not overlap: *"there is a clear phase where the simulation is done
rapidly, and this sort of breaks the narrative flow of the time-lapse. Let us use that to our
advantage, and separate each part with an otherwise completely blank Loading X Round. This way
the player can see that they have to wait, and what they are watching is a time-lapse of that
very fast calculation."* The calculation is fast and jerky; the lapse is paced and readable;
a pass drawn as it computes makes the lapse impossible to follow and hides the hand-over
between rounds.

**The wait** (Ben, 2026-09-16; the bar 2026-09-18) is one centred line — *Loading the Culture
round*, *Loading the Empires round*, *Loading the Exploration round*, *Loading the
Industrialisation round* — and under it the building screen's pair of bars, read from the
round's own run: an **outer bar** over the passes the run reports, moving only forward, and a
thinner **span-years bar** under it, drawn only while a span is counting years. Nothing else
is on the surface: no map, no board, no stage list.

**A wait never looks stopped, and it says what it is doing (Ben, 2026-09-24).** A bar that
sits on one step for the length of a long pass reads as a wait on nothing, so the outer bar's
steps are weighted by what each pass costs (measured, not counted as equals); every long pass
reports progress within itself, so the bar moves for as long as the wait lasts; a **caption**
under the bars names the one step under way ("Running the Exploration span"); and an
**elapsed-seconds count** shows the run is alive. ("Drawing borders" is a caption of the cold
path's loading bar, where the tail runs in front of the player; on the wizard the tail's
captions sit beside the playing map, § Round 6.)

**The lapse** begins when the record is whole and plays it from its first year at the pace the
player chose. Accepting moves to the next round; rerolling runs it again.
**The Industrialisation round's wait is its span alone (Ben, 2026-09-24).** The rest of the
build — borders, roads, companies, the search, the settle — is the tail, and it runs behind the
lapse rather than in front of it (§ Round 6); the caption rule follows the tail there, naming
the one step under way beside the playing map.

**There is no Run button (Ben, 2026-09-09).** Arriving on the round IS the instruction
to run it, so the press that moves onto a pass round starts its pass — it is already
under way while the round's first frame draws. A button here asked a question with one
answer. Ben, 2026-09-08: *a watched wait needs no budget* — which is why the
generation-budget chain was dropped rather than deferred. What is still owed is that a
watched wait must be **worth watching**; a round that shows a frozen globe for ninety
seconds is worse than a bar, not better.
**The round after is not built ahead while this one plays (Ben, 2026-09-24).** Arriving is
the instruction, and nothing runs before it is given.

**Rounds stay causal, downward only** — the per-stage reroll rule is stated once, at § Each pass
round is rerollable.

**The lapse plays a whole record (Ben, 2026-09-11; 2026-09-16).** It advances in **fixed
ticks** at one constant rate for the whole span, the rate set by the pace control so the span
plays in the wall clock the player chose; the record is whole before the first tick, so the
playhead has nothing to wait on — round 6's playhead runs while the tail builds, and the tail
is not in the record. The sim publishes its record through a **write-only tap** — the same
contract the market carve uses to fill the loading screen — so a watched run and an unwatched
run are byte-identical, and the standing determinism rule is untouched. Once the pass has
landed, the record stays: the round can be paused and scrubbed at the player's own pace. There
is no Restart button on any pass round (Ben, 2026-09-11): a scrubber makes it redundant, and
its row belongs to the ranking board.

**THE PACE IS THE PLAYER'S (Ben, 2026-09-13, revised 2026-09-16).** A lapse that runs too
fast to follow is a wait spent rather than watched, so every pass round carries the same
three-way control beside its transport: **30 s, 1 m, 1 m 30 s of wall clock for the whole
span**, a minute by default. It sets the WALL CLOCK, not years per second — a longer span at
the same setting moves faster rather than taking longer, because what a viewer budgets is
their own time. It changes the autoplay rate and nothing else; the scrubber still goes
anywhere.

**A ROUND OPENS ON THE GROUND THE ROUND BEFORE IT LEFT (Ben, 2026-09-16).** By construction
(2026-09-24): the rounds are one continuous history because they are one world moved forward
(§ The world cache): a round's span begins on the closing state of the round before, so its
first frame is the previous round's last, not a repainting of it. The surface says so with a
cross-fade, not a cut — the previous round's final map is carried into the next and painted
under ground nobody holds yet, fading out over the opening tenth of the new span while the new
round's own holders fade in over the same stretch. The Empires round opens on the migration's
peoples and watches city states organise them, and the same carry runs Empires into Exploration
and Exploration into Industrialisation. **What carries is identity, by id, not paint:** a realm
keeps its coined name, its pinned colour slot, its shade rung and its hard-border flag from the
round that gave it each (§ Identity across the rounds), and a 1960 nation keeps its realm's
name and colour at Begin. The Culture round has no lapse before it: it opens on the globe, which
cross-fades into its map (§ Round 3).

**The pass rounds draw the ground, not only the fill (Ben, 2026-09-11).** Rivers and the
landform relief — mountains, highlands, the barriers the walk and the campaign both price — are
drawn beneath the culture or polity fill on every pass round, 3 to 6, and
the fill is a tint over that ground rather than a flat colour that hides it. A frontier, a road
and a bridge are legible only against the terrain they cross.

## Handoff — `start_new_game`

The wizard's "Begin" — and, on the cold path, the one and only generation call.
**Round 6 does all of Begin's world work (Ben, 2026-09-24).** Everything that mutates the
world before play is one `world/*` function, `finish_campaign_world` — the world-only half of
setup (the genesis history, the survey states), the recipe band read from the history's
industry state, the default recipes, the stockpile budget, the landscape search (phase 6) and
the winner's apply, the recipe pass and the twelve-tick settle — called by the round-6 worker
after its build (§ Round 6) and by the cold path's worker (menu → Begin with no wizard,
`--autostart`) after its own, **the same call in the same order**, so an adopted world and a
cold build open the campaign on one state hash. The settle's tick body is one function in
`src/world`, shared by the app, the worker and every harness. Begin itself is the
presentation half:

1. **Adopt the world** (§ The world cache) — round 6's finished world, moved into play. Begin
   pressed while that round's worker is still running **waits on it**, on the loading screen;
   it never starts a second build. With no wizard behind it, Begin builds cold on the loading
   screen instead — `make_hard_coded_world`, then the same finish — and either way fills
   `m_generation_report`, which a save carries in full in the app envelope
   (`../tech/TECH_FOUNDATIONS.md` § What a save contains) — what lets a load re-pin the nation
   colours and replay the rounds.
2. **Set up the presentation:** the epoch formatter, the chat line, zoom/focus/frame;
   `m_econ_steps = 12`; the tech tree and the persona bench; the balance series from the
   returns the settle filed. The recipe registry is loaded from Lua on the main thread before
   round 6 launches and a copy handed to the worker, which bands it from the world it built;
   Begin moves that banded copy into `m_registry`. The twelve pre-game ticks have no
   presentation half of their own — no agency comms, history samples or last report come from
   them (delegated reading, 2026-09-24, NEEDS_REVIEW): the ticks have no calendar and no
   seated corp, so nothing they emit has a reader.
3. **Seat the player**: rank every specialist on phase 6's static landscape score,
   marking the ones below the viability floor, and open the selection canvas
   (§ The seat); the player's Confirm re-points `is_player` / `world::player_entity`
   onto the firm picked, through the `take_seat` command. A path with no player to
   ask (`--autostart`, the windowed autostart) draws one against the world seed
   instead and says so; `--seat <corp-id>` makes the pick at launch. Owned by
   CORPORATION_GENERATION.md § The spawn shortlist, and the seat.
4. **Rebase the clock** (fresh `sim_loop`; speed from the Lua config — generation, search and
   settle wall time must not become in-game days), then `m_screen = in_game`.

**The settle keeps its meaning.** It is twelve quarterly econ ticks with no calendar meaning
(`../economy/ERAS.md` § The opening position; BL-978, warm start retired), run **in spectate**
— `corp_ai_params::spectating`, no corp seated — because the seat comes after it, so the card
can show the figures it files; every corp opens onto non-empty pools and live markets rather
than a cold zero state. Only the wait's caption names it ("Proving the field"), and the loading
bar's label table carries that step and "Searching the landscape" at their measured weights.
`run_serve` adopts the searched and settled world; `run_verify` opens on the unsearched
seed-candidate world and stays cold — a golden reads generation alone, so it is pinned to the
world before the search.

Play opens on the corporation's home planet — the Planetary rung, home body
selected (CANVASES.md § Default state).

### The world cache — Begin adopts the wizard's world

The wizard builds **one world**, and each round moves it forward (Ben, 2026-09-16, NR-811;
2026-09-24: *"pass the calculations of each round over without having to recalculate at each
step"*). The generator is a composition of resumable stage functions over one cursor — the
world, its report, its naming and the state each span resumes from — and the composition is
byte-identical to the single call every harness, fixture and seed-library pin makes. **The
Life round** builds the gate world (the star, the system, the homeworld's tiles with their
deposits, its rivers) into slot 0; **each lapse round takes the slot before its own and runs
only its own stage into the next** — the migration, Empires, Exploration, Industrialisation
and its tail; **Begin adopts the last slot** and takes it into play rather than building a
second world. No round rebuilds what the round before it built, and nothing is built to be
discarded. The gate world is the homeworld's system alone — Helios, Cinder and Kepler;
**Selene and Pallas are built in Finishing**, in the tail, because nothing a round shows stands
on them and building them earlier would shift every entity id after them (delegated reading,
2026-09-24, NEEDS_REVIEW).

- **Owner.** The app holds the slots, next to the rounds' records: one per round, filled by
  that round's worker while it runs, read by the round after. A slot is **moved** into the next
  round's worker, not copied — a world that has been run forward is consumed by the round that
  ran it.
- **The predecessor is held.** Because a round's worker consumes what it is given, a reroll of
  round N needs round N−1's closing world back. It is held by a **faithful copy** taken at
  landing — a copy ticks as its original (the standing `world_copy_determinism` invariant) —
  and moved in a second time on reroll; the copy is an in-memory copy and never a serialised
  snapshot, because a save round trip reorders the unordered stores. **Where the predecessor
  is not held, a reroll replays from the Life gate** with the fixed per-span seeds (§ Each pass
  round is rerollable), which is bit-identical by determinism: the forward path never
  recomputes, and the reroll path recomputes only what it must. The copy is kept only where it
  costs less than the replay it saves; otherwise the reroll is pure replay (delegated reading,
  2026-09-24, NEEDS_REVIEW). Back keeps every record and world it passes and recomputes
  nothing.
- **Invalidation.** A slot goes with its round's record, on every path that clears that
  record: a reroll or lean change on that round or any above it, any planetology change, and
  leaving the wizard for the menu. A run that lands after its round went stale is dropped with
  its record. Begin compares the held world's params with the wizard's, field by field, and
  builds cold on any difference — the invalidation rule is what keeps the cache honest, and
  this is the check that it did.
- **Begin waits for round 6.** Round 6's worker runs the span, publishes the record, and goes
  on to build the tail — the finishing, the search and the settle — into the last slot
  (§ Round 6). Begin pressed before it lands waits on it; it never starts a second build.
- **Memory.** One world per round, plus the copy that holds each landed predecessor; a slot is
  released when it is invalidated or when Begin spends the last. While a stale run is still
  computing, its half-built world lives until that run lands, and then goes. The cost is a
  homeworld-scale world per round held through the wizard — the same world play would hold a
  moment later, a few times over.

A cold build and an adopted world on the same settings open the campaign on the same state
hash; the round-by-round path and the single call are one calculation, and they are held equal
at every round boundary, not only at Begin.

## The seat

**The player picks the seat, on a corporation selection canvas at Begin** (Ben, 2026-09-09:
*"Begin should go directly to a 'corporation selection' interactive canvas"*; asked whether that
reverses the 2026-08-26 retirement of the selection screen: *"Yes, it's time to reverse that
ruling."*). The random draw is retired as the seat mechanism. **The canvas offers every specialist**
(Ben, 2026-09-24): the static score ranks them, and the viability floor **marks** the ones below
it rather than removing them — a marked firm stays pickable, so a player can knowingly take a
hard seat. The player chooses rather than being drawn for.
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

**The surface (Ben, 2026-09-24, the select-company design session).** *Its question: which
corporation am I?*

- **A ranked list beside the map.** The list is every specialist in static-score order, the
  marked ones visibly below the floor; the map is the home body at the wizard's world, and it
  **highlights the hovered row's firm** — its HQ, its works and its home market's catchment — so
  where a firm stands is read by pointing at it, not by reading coordinates.
- **A firm's card** shows four things and no more: the **industry and the goods it makes**; its
  **HQ and home market, with that market's prices** for the firm's goods and inputs; its **cash,
  debt and assets**; and **its nation and that nation's stance**. The landscape score is the list's
  order, not a card line; trailing figures and rivals are not on the card.
- **Picking opens a briefing, then a confirm.** The briefing is the firm's opening position in
  prose — who you are, what you make, where you sell, what you owe, whose law you trade under,
  and, for a marked firm, why the floor marked it — **and one sentence of origin (Ben,
  2026-09-24):** *"Chartered from <city>'s industry, in <region>, under <nation>, the realm of
  <X> since <year>"*, read from the firm's own founding year and origin region and from the
  record's owner slice at that year. **When X is the realm the nation grew from, the sentence
  says so once (Ben, 2026-09-25, NR-941):** *"…under <nation>, its own realm since <year>"*; the
  full form is kept for ground the nation's realm did not hold (absorbed since, bought, a
  colony's overlord). **"Since" is the realm's tenure, not the record's opening (Ben,
  2026-09-25, NR-939):** where the same polity id holds the region at the close of the earlier
  span, the sentence walks back into that span's record, and on until the holder differs, so a
  realm that took the ground in 1310 reads *since 1310 CE*, not the year its record opens. A
  polity id is one table across the three records, so the walk is well-defined. That sentence is the whole of the history the seat
  carries: no opening card, no header change, and no city renamed for it. **Confirm** seats
  the player and opens play; **Back** returns to the list with nothing changed. Choosing is a
  deliberate act, so it takes two presses.
- **The seat is reproducible from (seed, pick):** the same world and the same pick seat the same
  firm in the same state.
- **The pick is a game act** (Ben, 2026-09-24): Confirm issues the `take_seat` command, which has
  an action-dictionary entry although the wizard has none, so an agent can make the same pick
  (`--seat <corp-id>` at launch). The seat is taken once, before play: the in-play agent hosts
  refuse the command.
