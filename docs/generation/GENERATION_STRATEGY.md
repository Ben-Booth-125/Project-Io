# Project Io — Generation Strategy

This document is the **map of the generation layer** — the strategy that ties the
per-subject generation docs together — and the home of the **economic premise** the whole
campaign setup derives from. Each subject below has its own authoritative doc; this one
summarises how they relate and records the cross-doc decisions that no single one owns.

The subject docs:

- **`PLANETOLOGY.md`** — the body-level history pass: generated atmosphere/chemistry and a
  simulated abiogenesis/evolution history, ahead of tile generation, in the spirit of Shadow
  Empire's Planetology phase. It *derives* each `body_profile` rather than hand-authoring it.
- **`CONTINENTS.md`** — the plate-drift pass: plates derived from Planetology's Engine output,
  feeding a height bias into the tile pipeline's Pass 1.
- **`TILE_GENERATION.md`** — the procedural tile pipeline (terrain, ocean, deposits) per body.
- **`PROVINCES.md`** — the partition of every body's tiles into small spatial cells, the grain a
  battle, a unit position and a building ceiling are measured in.
- **`NATION_GENERATION.md`** — Voronoi territory placement and nation profiles over the tile map,
  driven by the pre-national history ladder.
- **`../lore/HISTORY.md`** — the institutional history ladder: *why* the campaign world is
  market-based and non-hegemonic. **The campaign epoch is 0 CE (Ben, 2026-08-12, NR-177)**, and
  generation runs a stepped 4000 BCE → 0 CE prehistory. Stages 5–6 (the energy transition and
  saturation) fall *past* the epoch entirely and are DLC-era material (BL-223, averted rupture,
  owns their reshaping).
- **`CORPORATION_GENERATION.md`** — corporation placement, focus, holdings, and finance.
- **`GENERATION_LEDGER.md`** — the tuning surface that explains *why* a tile generated as it did.

Generation runs, in `make_hard_coded_world`:

```
planetology → continents → tiles → rivers              (per body)
  → history ladder → creeds → settlement                 (homeworld only, from here)
  → history sim, pass 1 (ancient) → history sim, pass 2 (industrial)
  → population centres → nations → national character
  → ruptures → institutional history → provinces → roads → corporations → markets
  → other bodies' tiles → laws
  → provinces                                            (every other body, last)
  → background firms → the economic settle (pass 3)      (after the worker, before play)
```

The three simulated passes — two polity spans and one economic settle — are § Three passes of
simulated history; `../lore/HISTORY.md` owns the polity spans.

A body's atmosphere/history precedes its plates; plates precede its terrain; deposits exist
before territory is drawn over them. On the homeworld, population centres are placed **after the
Era −1 sim and before nations** — after, because the sim has grown, warred and plagued the
regions' populations, so the centre count and scale distribution are the history's *consequence*
rather than a land-area divisor and an authored draw (BL-610, centres from demography); before,
so the substrate-density pass can read them. The history ladder runs **before**
`generate_nations` because it *drives* the seed budget, and Stages 1–2 of the institutional
history are recorded **after** — they name and count nations that did not exist a moment
earlier. On the homeworld the **province partition runs before roads** (Ben, 2026-08-25;
BL-623, provinces before roads — overturning the 2026-08-21 roads-bind-provinces input): the
partition reads rivers and urban transforms but **not** roads, every settlement-less province
is then given its settlement, and only then are roads stamped — over the *complete* settlement
set, so every anchor founding joins the lattice like any village. Corporations are placed after
roads and before markets, so placement reach reads the real network and the market carve can
read who competes where. On every other body the partition still runs last. Each pass sits
under its own seed offset so it perturbs nothing above it. All passes are **deterministic**
from the campaign seed.
Within the tile-generation layer, the six-pass core stays fixed and every extension lands as a
**sibling pass** reading the shared `generation_record` rather than growing the core pipeline
(settled 2026-07-21, BL-051, generation-record sibling passes).

---

## The economic premise

The campaign opens on a **saturated, earth-like economy**. The home world's broad industrial
base — the bulk of ordinary extraction, processing, and manufacture — is **owned and run by
real background corporations** (BL-365, background corporations), not a nation actor. It is not
the player's playing field and is not surfaced as manageable detail on any per-firm basis; it is
the saturated background the contest happens *on top of*.

**Real firms, calibrated count, not an injected substrate.** Background industry is
**generated corporations** — `corporation_component.is_background = true` — with real buildings
that produce and consume through the ordinary economy tick. There is no per-capita demand basket
or abstract nation-level supply capacity injected into market arrays; every unit of background
supply and demand has a building behind it. Generation is **calibrated, not authored**: firms are
placed until real production reaches ~90% of real demand for the tradeable resource set
(`docs/economy/MARKETS.md` § Background corporations) — a clearing fraction that is **emergent**
rather than **injected**. This is a **corporation**, not a nation, actor deliberately: the broad
base needs an actor that runs itself, and the corporation is the actor class with a sanctioned
scored-utility layer. Background firms run the **full corp_ai scored-utility layer**, identical to
the named rivals, not a cheaper reduced model (Ben, 2026-08-11). The markets they feed are
**resource-carved** (BL-096, resource-carved markets): a nation's territory fractures into more
markets where its tradeable-resource concentration is high and folds into a neighbour where it is
barren, with nations the carving actor even though they do not own the industry. Together these
keep the premise (a saturated base the player competes *on top of*) while making it a legible,
fillable opportunity surface rather than an inert price floor.

**Corporations are specialists, not full-chain industrialists.** The player and the major AI
rivals each occupy a **focused slice** of the resource chain and are differentiated by a single
shared trait: an **interest in expanding to space**. The strategic contest is therefore between
a small number of competing, space-interested specialists — not a field of generic firms
reproducing the whole economy.

This premise is what **simplifies the gameplay loop**: the player does not have to own or
balance an entire economy, only to compete as a specialist and convert that position into
off-world reach. It is the reason corporation generation produces **lean, focus-coherent
holdings** rather than a broad spread (see `CORPORATION_GENERATION.md` § Pass 3), and the
reason the broad industrial base is carried by generated background firms rather than by the
player managing it.

**Market co-generation** (BL-132, market co-generation). Three rules on top of the nation-carved
market model, each reading the previous rather than replacing it: (1) population centres weight
their placement toward nearby extractable-deposit richness, not adjacency to an existing centre
alone — population spawns near rich resources. (2) A market's site is a static
resource↔population **trade-flow proxy**: since live trade routes do not exist at generation
time, each market centres on the strongest nearby pull toward a rich deposit within a bounded
radius of its anchoring population tile, rather than sitting exactly on that tile.
(3) **Corporations generate before markets**, and the nation-carving gate's concentration term
factors how many distinct corporations already hold an asset in that territory — a
commercially-contested territory fractures into more markets on top of raw geology, not geology
alone. Nations remain the carving actor; this adds *who is competing there* as a second signal
alongside *what the ground holds*.

---

## Asymmetry is the deliverable (Ben, 2026-08-31)

> *"There is no reason that every start needs to be equally good and fair… If we send some of this
> work into generation, we should be making rules that encourage a level of asymmetry."*

`docs/economy/MARKETS.md` § Three properties, 4 and 5, settle the demand side: every resource needs
a path to a terminal sink, terminal demand follows population and is therefore universal, and
supply is regional. What that leaves to generation is the **shape of the supply asymmetry**, and it
is a deliverable rather than a side effect.

**The enemy is UNIFORMITY, not self-sufficiency.** Two failure modes, and they look opposite while
being the same defect:

- Every region can close its own chains → no region needs another → trade has no reason to exist.
- No region can close any chain → every region is dependent in the same way → the dependency
  carries no information, and one trade route is as good as any other.

Both are *flat*. A world is interesting when regions differ in **what** they can close and **how
much** of it — one rich in ore and starved of food, one that can feed itself and make nothing, one
that happens to hold a whole chain and is therefore worth taking.

**The measurable is CHAIN COMPLETENESS, and the target is its SPREAD.** For a market or region, the
fraction of the chains terminating there that can be sourced within reach. Generation is answerable
for the *distribution* of that number across the world — wide, with real tails at both ends — and
answerable for nothing at all about any individual region's value. No start is clamped up to
viability or down to fairness.

That is the same discipline the rest of this document runs on: a stage is a **deterministic
consequence of upstream scalars, not a roll**. The machinery to produce the spread already exists
and is not yet being read as an economic instrument —

- `planetology_state::endowment`, the per-resource deposit multiplier per body, where **0.0 means
  the body cannot carry that resource at all** (`PLANETOLOGY.md`);
- the endemic goods' **latitude band × longitude sector**, which already makes a cash crop a fact
  about one region rather than about a world;
- the six-pass tile pipeline's terrain and deposit passes (`TILE_GENERATION.md`), which decide what
  ground is where.

What is missing is not a mechanism but a **read**: nothing currently measures chain completeness,
so nothing can say whether a generated world is flat or varied. Until something does, asymmetry is
an intention rather than a property, and a tuning change to any upstream pass can quietly flatten
the map without a single check going red.

**A note on fairness, so it is not re-litigated.** An unequal start is not an unfair one here. The
player is one corporation among several in a world none of them chose, and the campaign's shape —
`docs/CONCEPT.md` § Stagnation as loss — already treats falling behind as progressive rather than
terminal. A generator that made every start equivalent would be answering a multiplayer-ladder
question this game does not ask.

---

## The world descriptor — seed + generation parameters

Generation is driven by a small **world descriptor** (BL-114, world descriptor) — a master
**seed** plus a `world_params` struct — chosen on the main-menu **New World** setup and threaded
through `make_hard_coded_world(world_params)`. Same descriptor → identical world on a given
binary; this is the reproducible key the setup screen surfaces (with a dice-randomise and a
copyable readout). The descriptor lives in the app, **not** the `world` struct, so it stays off
the world's serialisation seam; the save file records it so a load rebuilds the same world.

**`world_params` fields and how each maps onto the generators:**

| Field | Maps to | Cost |
|---|---|---|
| `seed` (`uint32_t`) | XOR-folded into each **per-body seed literal** (`params.seed ^ 0xC1D0001u`, …). Seed `0` yields the bare literals, so the **default descriptor is the reference world**. | cheap |
| `abundance` (`sparse`/`lean`/`standard`) | A **deposit-density scalar** applied as a pure post-multiply in `generate_body_tiles` Pass 6 (`0.40` / `0.65` / `1.00`). Consumes no RNG, so `standard` (1.0) is bit-identical to the unscaled surface. | cheap, isolated |
| `epoch_year` (`int64_t`) | The campaign epoch, `0` by default. `1960` selects the parked space arc; the generators key on it. | cheap |
| `prehistory_years` (`int`) | Years of year-tick prehistory the antiquity branch simulates before the epoch (400, at 4 years a tick). A **scope knob, not a tuning dial**: `0` skips the pass, which is how harnesses that do not test the era avoid paying for it. Part of the params, so determinism is untouched. | the most expensive pass |
| `body_count` (`int`) | **Reserved.** The body set is hand-authored prototype *profiles* (hot inner planet / homeworld / moon / metallic asteroid — their **names** are generated per seed, BL-257, body naming); a true count knob needs the generator to synthesise variable body profiles. The field exists so the descriptor is forward-shaped. | heaviest |
| `preferences` (`world_preferences`) | The New World wizard's input: eight **leans** (`any`/`low`/`mid`/`high`), resolved against the seed by `resolve_preferences` with reject-and-reroll until the homeworld clears the strict Earth-like floor. Preferences, not parameters — see `PLANETOLOGY.md` § Preferences, not parameters. | cheap |

**There is no nation-count field.** The number of nations on the home body is a *consequence* of
generation, not a descriptor input: seeds scale with habitable land area, every nation below a
minimum viable territory is absorbed (`NATION_GENERATION.md` § Pass 1 / Pass 2c), and the seed
budget itself is **driven by the history ladder's `fragmentation_q`** — a broken, many-cradled
world seeds more densely and keeps smaller survivors (`nation_params_from_ladder`,
`history_ladder.cpp`). The New World setup screen therefore has no nations slider — a world's
political granularity is something the player discovers, not something they dial in.

**Abundance honours the resource ceiling.** `standard` **is** the earth-like ceiling (1.0×); the
other tiers step *down* (`lean` 0.65, `sparse` 0.40) — there is no tier above Earth. Determinism
is verified headlessly by `tools/verify/world_determinism.cpp` (same seed → identical world;
different seed → different; `sparse < lean < standard`). The descriptor is also the natural
input to the Generation Ledger as a tuning surface.

---

## How the layers compose

- **Tiles** establish the physical reality: terrain, hazard, habitability, and resource
  deposits. Nothing downstream contradicts tile data — nations and corporations are *placed
  onto* it.
- **Provinces** partition the tiles into the cells force and ceilings are measured in. The
  partition is grown from settlement and stopped by terrain and by national borders — no
  province holds tiles of two nations — and versions with generation (`PROVINCES.md`).
- **Nations** draw territory over the tiles. A nation's `economic_focus` biases which
  corporations register there; carving resource-carved markets stays a nation-level act even
  though the broad industrial base itself is firm-owned.
- **Corporations** are placed within their home nation's territory as **specialists**: a lean,
  focus-coherent set of holdings clustered in the nation (see `CORPORATION_GENERATION.md`). A
  separate, later generation pass places **background** corporations that carry the broad
  industrial base the premise above describes (`CORPORATION_GENERATION.md` § Pass 6) — the
  specialist premise for the player and named rivals is untouched; background firms fill in the
  rest of the economy.

---

## Visibility — which passes the player can watch

The **nation carve** and **corporate seeding** have a live surface on the loading screen
(`app_screen::building`, `app::draw_building_carve` in `src/core/app.cpp`), fed by the
atomics-only `generation_progress` sink (BL-305, loading-screen carve). The obvious follow-on
question — *which of the other passes happen invisibly?* — is answered here, walking the pass
map in run order.

Three verdicts are used. **Watched** — the player can see it happen, or can inspect its output
in-game. **Owed** — it produces something a player would want to see and there is no surface for
it; a real gap. **Invisible by design** — showing it would be noise, and its effect is legible
through what it produced.

| # | Pass | Surface | Verdict |
|---|---|---|---|
| 1 | Body naming | Every label in the game | Invisible by design — the output *is* the surface |
| 2 | Planetology, per body | Wizard stage folds + charts; History ledger | Watched |
| 3 | Continents / drift | Continent lens; biography lines | Watched |
| 4 | Tile generation, six passes | The planetary canvas; the wizard globe samples the real surface | Watched (outcome) / Owed (derivation — the Generation Ledger) |
| 5 | Rivers | River edges on the planetary canvas | Watched |
| 6 | Population centres | Population lens | Watched |
| 7 | History ladder, Stages 0–2 | Dated lines in the body biography | Watched (as text) |
| 8 | Creeds / pantheons | Biography lines; culture on regions | Watched (as text) |
| 9 | Settlement & industrialisation | `generation_report.settlement`; History ledger | **Owed** — regions are the anchors the carve grows from and have no map surface of their own |
| 10 | **Nation carve (Voronoi BFS)** | **Loading screen, live**; Country lens in play | Watched |
| 11 | National character derivation | Nation detail in the Selection band | Watched |
| 12 | Historical ruptures | Checkpoints + lacunae in the History ledger | Watched (as text) |
| 13 | Institutional history / globalisation | Biography lines | Watched (as text) |
| 14 | Roads | Road tiers on the planetary canvas | Watched |
| 15 | Pre-authored homeworld installations | On-canvas buildings | Invisible by design — two authored stubs, not a generated fact |
| 16 | **Corporations (placement + finance)** | **Loading screen, live** — map markers + charter ledger | Watched |
| 17 | Market carving | Market lens; market ledger | Watched (outcome) / **Owed** (*why* a nation fractured into N markets is nowhere) |
| 18 | Prototype laws | Law panel | Watched |
| 19 | Background firms | Corporations panel, in play | **Owed** — runs on the main thread *after* the worker, so the loading screen cannot show it; the one generation pass with no live surface at all |
| 20 | Pre-game warm start (80 econ ticks) | The inner bar and a caption only | Partly watched — the bar is honest, but the balances it produces are not shown |

Four owed items, none of them blocking: the tile-derivation ledger (designed in
`GENERATION_LEDGER.md`), a region surface, a market-carving explanation, and background firms —
which the worker split puts out of the loading screen's reach. Recorded here rather than filed as
items so the map stays in one place; promote from this table when one is picked up.

---

## Real history in, invented names out (Ben, 2026-08-03)

A standing constraint on every generation pass, stated here because it cuts across all of them.

**The design leans on real history deliberately, and should keep doing so.** The institutional
ladder (`docs/lore/HISTORY.md`) derives fragmentation, nation count and industrialisation timing
from a real causal sequence; the Era −1 sim (BL-271, Era −1 sandbox) was filed off "use Rome as a
sandbox"; the mil-sim (BL-272, unit/doctrine combat model) takes real constants. That is the
cheapest source of mechanisms that are known to work, and abandoning it would mean inventing
social physics from nothing.

**What transfers is the mechanism. What never transfers is a proper noun.**

| Transfers | Does not |
|---|---|
| How a charter makes a promise enforceable | "The Hanseatic League" |
| How a growth front stalls at a strait and leaves an exclave | "Sicily" |
| How an inland sea concentrates littoral power | "The Mediterranean" |
| Plausible hegemony-formation speed, campaign-season length, supply radii | "Rome" |

Every generated proper name — nation, region, population centre, corporation, body, person —
is **sci-fi / fantasy**, produced by the seeded template banks and phoneme tables described in
the per-subject naming passes (`NATION_GENERATION.md` § Pass 5, `CORPORATION_GENERATION.md`
§ Pass 5, `generate_city_name`). Two consequences worth stating because they are easy to get
wrong:

1. **"Culture-flavoured" must not mean "Earth-culture-flavoured."** The template banks are
   flavoured by *generated* cultural character — the phoneme tables should not read as
   recognisably Latin, Han, Norse or anything else an Earth reader can place. A name that makes
   a player think "that's the Roman one" has failed, however good the mechanism underneath it.
2. **Analogy language in docs is for the reader, not the generator.** When a design doc says
   "its VOC moment" or "Rome as a sandbox", it is orienting a human. Nothing downstream should
   read those words as content, and no name pool should be seeded from them.

The one exception lives outside Io: **Project-Rival** plays an actual RTS with actual
civilisations, because rehearsing the method needed an arena that exists today. It returns
numbers and doctrine to Io — never names.

---

## One continuous simulated history

**Settled direction (Ben, 2026-07-28):** generation is **one continuous simulated history**
rather than four separate mechanisms, extending Planetology's S0–S9 chain forward and backward:

```
S0 System → S1–S4 Continents (simulated plate drift/collision/rift) → S5–S8 Biosphere
  (the Planetology chain) → Settlement → Industrialisation → the campaign epoch
```

The pieces and who owns them:

| Slice | Owner |
|---|---|
| Continents/Drift — plates from Engine output, height bias into tile Pass 1; the Continent lens | `CONTINENTS.md`; BL-226 (Continent lens) |
| Settlement Stages 0–2 — cradles, charter, border accord; the ladder drives the nation seed budget | BL-221 (pre-national ladder); `../lore/HISTORY.md` § Implementation |
| Full S1–S4 continents simulation, replacing the remaining noise machinery | BL-210 (oral-history pivot) |
| Industrialisation / later ladder stages | BL-222 (industrial ladder), BL-223 (averted rupture) |
| Branch checkpoints (historical-extinction analogues) + lean × branch sweep | BL-210 (oral-history pivot) |

The planetology state flows into settlement and nations: `run_history_ladder` consumes it and
`nation_params_from_ladder` feeds its fragmentation into nation seeding. The corporation half of
that connection — planetology and ladder outcomes shaping which corporations exist and where — is
BL-210's to close. **Full architecture, rationale, and the per-doc open questions live in
BL-210** (`backlog.json`).

---

## The eight phases (Ben, 2026-09-03)

The pass map above is the *call order*. This section is the **phase structure** it serves — the
reorder Ben set out on 2026-09-03, with the five open calls taken on an elicitation form the same
day. Where a phase's aim and its current implementation disagree, the phase wins and the gap is
work.

| # | Phase | Its aim | Owns |
|---|---|---|---|
| 1 | **The Body** | The world as geology: atmosphere, chemistry, plates, terrain — and **geological deposits**, metals among them. | BL-762 (resource origin split) |
| 2 | **Life** | The biosphere's residue as resources — coal where the ancient swamps were, oil where the ancient seas were, timber where the forest still stands. | BL-763 (continent time axis), BL-764 (Lagrangian tiles), BL-765 (paleo deposits) |
| 3 | **The People** | Where people are, weighted toward ground that farms easily — drawn **before** history and evolved by it. | BL-766 (population map early) |
| 4 | **The History** | Empires that form, grow and collapse; the roads that supplied them; the markets that emerged from their trade. | BL-767 (empires reliably form), BL-768 (roads and markets from history) |
| 5 | **The map of consequence** | **Finalise** what the history produced, rather than invent it. City states and pseudo-national borders belong to phase 4. | BL-769 (consequence folds into history) |
| 6 | **The economic substrate** | The Era 0 sim, 1560–1960: search in parallel for a corporate landscape that is **viable but uneven**. | BL-770 (Era 0 candidate search), BL-771 (tick length is a constant) |
| 7 | **The rest** | The other bodies, the laws, the partitions. Expands as core systems land. | — |
| 8 | ~~Warm start~~ | **Retired.** Its burden moves to phase 6. | BL-772 (retire warm start) |

**Five calls, taken on the form rather than assumed** (the NR-517 precedent — read a silence as a
decision and you set the quiet precedent this project files items to avoid):

1. **Empires are tuned for, never clamped.** "Force outcomes where empires form" means *tune the
   forces until empires are a common outcome across the seed spread*, not guarantee one per world
   and not impose one post-hoc. A world with no empire stays a legitimate outcome, which is what
   keeps § Asymmetry is the deliverable, the 2026-07-30 emergent-nation-count ruling and BL-224's
   non-hegemony invariant all intact.
2. **The population map is drawn early and evolved.** This overturns BL-610's ordering while
   keeping its goal: centres are still history's consequence, but now because history *grew and
   sacked them* rather than because they were placed afterwards. It also stops the sim running
   over a world with no cities in it.
3. **The candidate search varies rosters, placements and road tiers — not worlds.** So generation
   runs once and only the economic sim repeats, which is the whole reason the budget closes.
4. **The search optimises for viable-but-uneven, not maximum profit.** Maximising profit would
   flatten exactly the spread this document asks generation to produce.
5. **Life samples the real drift history**, rather than proxying ancient climate from the present
   landform. That is the expensive answer and it was taken deliberately; BL-764 is the largest item
   in the reorder because of it.

**The parallelism is not an optimisation, it is why the shape fits.** Six candidates at 53–71 s
each is 5–7 minutes serially and breaks the 3–6 minute budget; run in parallel they cost about one
candidate's wall clock. That makes determinism the binding constraint: each candidate must be a
pure function of (shared world, candidate seed, candidate parameters), and the winner chosen by a
deterministic argmax with an explicit tie-break — **never by which thread finished first**, and
never varying with thread count. BL-773 owns the budget as a whole.

**Two of the eight points needed new machinery rather than a reorder**, and are filed at that size
rather than as tweaks. Sampling real drift history has no time axis to sample (nothing integrates
plate motion), no frame in which ground moves (tiles do not ride plates — a tile's latitude *is*
its grid row, fixed for all time), and no per-tile past climate. And roads cannot be laid *inside*
the history sim, which is deliberately world-free and whose pathfinder returns a cost between
regions rather than a list of tiles — so they are **stamped from** the history's record instead.

---

## Three passes of simulated history (Ben, 2026-09-03)

> *"We have one pass to determine ancient borders and cultural doctrines, and then a second pass
> to determine the extent of colonisation by major powers, and market conditions upon game
> start."*

The continuous history above is produced by **three passes on two engines**, and the abstraction
problem the design has to solve is the **handoff** between them, not either engine. Generation is
being tuned to reach a described output — a functioning global trade network at the epoch — and
the rule for reaching it is unchanged from § Asymmetry is the deliverable: **tune the forces,
never the outcome**. Each pass has a scoreboard read over a seed sweep; no world is steered to a
target.

| Pass | Engine | Span | Produces |
|---|---|---|---|
| **1 — Ancient** | The polity sim (`history_sim`), Classical and Medieval bands | The prehistory span to the **boundary year** | Ancient borders, cultural doctrines, the lacunae — who walked where |
| **2 — Industrial** | The same polity sim, Gunpowder and Industrial bands unlocked, sea legs open | The boundary year to the epoch | The extent of colonisation by major powers, which polities industrialised and when, each nation's tariff posture |
| **3 — Settle** | The campaign economy tick (`run_economy_step`) with the full corp AI | No calendar meaning; banded coarse, at the epoch | Market conditions at game start: which firms exist, what each market can close, the price field |

**Pass 1 and pass 2 are one engine, not two.** The works roster is cumulative across its four
bands and the unit roster is era-keyed, so the second span is the first span continued with more
rows offered, not a second mechanism. What pass 2 adds is **reach across water** — a campaign or
settle target across a sea leg, staged from harbour works — because colonisation by a major is
the Metropole strategy played overseas, and it culminates as every major does (`../lore/COLLAPSE.md`).
The epoch still arrives multipolar; the non-hegemony invariant is not relaxed for the sea.

**The boundary year is a parameter with a default, not a fact.** The default is 400 years before
the epoch, so that on a 1960 arc pass 2 is 1560 → 1960 and pass 1 is whatever
`prehistory_years` leaves before it. A derived boundary — the year the first polity lights a
furnace — is the better-founded alternative and is open; both are consequences of upstream
scalars, and neither is a roll. On an ancient epoch there is no pass 2: the boundary falls past
the epoch and the sim stops where it stops today.

**Pass 3 is the warm start, promoted.** `app::start_new_game`'s pre-game ticks already settle
the economy before play; pass 3 makes that a generation pass with the two acts it lacks:
background firm generation (`CORPORATION_GENERATION.md` § Pass 6) **recurs** through the settle
rather than running once, and **firm exit** is the cull. Firms spawn and collapse until the field
is operating-positive and steady; the survivors are viable by construction, and the player enters
a field that has already been selected rather than one that is about to be. Two properties bound
it. **Stable is not saturated**: the stop condition is operating-positive and steady, never every
chain closed, and the chain-completeness spread is asserted at the end of the pass, so the
opportunity surface the economic premise needs survives the settle. And **a settle can only select
over a roster that can pay**: on a roster that loses at base, a longer settle produces an empty
world, not a stable one. The recipe-margin anchor (`../economy/PRODUCTION.md`) is therefore a
precondition of pass 3, not a neighbour.

**What crosses each handoff, and nothing else.**

- Pass 1 → pass 2: the region table, cultures, works, the strain accumulators. Nothing is reset.
- Pass 2 → the political map: the same outputs `generate_nations` reads today, plus two new
  ones — a nation's **tariff posture**, enacted as an ordinary `import_tariff` law at world
  setup where pass 2's polity chose protection, and its **colonial ties**, which seed the
  order book's preferred-seller relationships so a colony's chains close through its metropole
  before they close anywhere else.
- Pass 3 → play: the world state, as the warm start hands it over today. Pass 3 seeds no
  behaviour (§ Generation seeds no behaviour in `CORPORATION_GENERATION.md` still holds).

**Separate market conditions are produced by in-world forces with visible causes** — never by a
term inside an agent. Tariffs, because a nation that industrialised late relative to its
neighbours protects what it has; distance, because the landed price of a far competitor's good
is high on the real road and sea network; ties, because history routed a colony's trade through
its metropole. A local firm's early sales are sheltered by the same three, and a player can read
why on the map.

**The cost question is open and is measured first.** Pass 1 is already the most expensive pass;
pass 2 doubles it and pass 3 lengthens the warm start. The budget is the generating screen's wait,
and the affordability rungs in `../lore/COLLAPSE.md` § The 4000-year problem become load-bearing
in the order that document gives. Whether the shape can be had cheaply is the sprint's question,
not this document's.

## Open cross-doc items

These are design questions that span more than one generation doc; each is also noted in the
owning doc's § Open items.

- **Building tiers / levels.** A level/tier axis for buildings — distinct from **production
  methods (recipes)**. A specialist corporation's footprint may be characterised as much by the
  *tier* of its assets as by their number. Unsettled; interacts with `PRODUCTION.md`.
- **International relations & corporate origin.** Whether **allied nations share corporations**,
  or **prefer generated franchises** across borders, is open — it couples nation diplomacy to
  corporation generation (the Franchise open item in `CORPORATION_GENERATION.md`).
- **Post-WW2 industrial grounding.** The focus→asset-mix patterns should be grounded in research
  on the post-WW2 industries that led to space-related capability, so a specialist's holdings
  read as a plausible pathway toward off-world reach rather than an arbitrary mix.
- **The Industry lens and the background.** The background industry is generated, not authored
  (§ The economic premise), and the Industry map-lens (`docs/ui/LENSES.md`) renders its aggregate
  per body. The lens's rendering job is independent of the mechanism beneath it: it draws the
  industry that real background firms own, not a per-tile field.
