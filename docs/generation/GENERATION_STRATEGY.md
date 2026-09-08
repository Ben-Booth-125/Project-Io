# Project Io — Generation Strategy

> **Settles:** which generation doc owns which question, and in what order the passes
> compose · what each pass hands the next · what economic premise the campaign start is
> derived from · what a world descriptor carries · which passes the player may watch ·
> where real history may be borrowed from, and where a name may not.
> **Not here:** what any single pass computes — each subject doc settles its own ·
> what the world does once generation ends.
> **Confused with:** TILE_GENERATION.md, PLANETOLOGY.md, ../lore/HISTORY.md.
> *Start here when the question spans more than one generation doc.*

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
| 5 | **The map of consequence** | **Finalise** what the history produced, rather than invent it: the anchors of one polity fold into one nation, city states survive the size floor, and the tariff posture is enacted. City states and pseudo-national borders belong to phase 4. | BL-769 (consequence folds into history), BL-750 (tariff posture) |
| 6 | **The economic substrate** | Search for a corporate landscape that is **viable but uneven**. A **static** search with no clock and therefore no span — see § Phase 6 is a STATIC SEARCH below. | BL-770 (Era 0 candidate search) |
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

**Phase 6 is a STATIC SEARCH, not a simulation (Ben, 2026-09-03).** He put the goal narrowly:
*"we are not looking for a 100% accurate series of trades… and neither what makes the most profit
per tile. We want to find, given a planet with markets and national borders — how can we saturate
all the resources in said market, so that each part makes some profit?"*

**That is not a simulation question.** A tick answers *who traded what, at which price, this
quarter*. Saturation — every resource has a supplier and a buyer, every chain reaches a terminal
sink, every participant clears its costs — is a **static property of a candidate roster against a
fixed world**. Roads, borders, markets, deposits and population are all settled by the end of
phase 4, and none of them moves during phase 6, so there is nothing to step.

**Both measures already exist**, which is the part worth knowing before anyone writes a simulator:
`measure_completeness` returns terminals-closed over terminals-total per market from a static world
and a recipe registry, and the recipe-margin computation returns revenue minus marginal cost
(inputs at base plus the wage per batch) from the registry alone. Between them they answer both
halves of that sentence. They sit inside harness anonymous namespaces today, so generation cannot
link them — BL-775's to fix, and worth fixing regardless, because two callers sharing one
implementation is the discipline that stops a check measuring something different from the code.

So the method is **evaluate statically, validate dynamically, once**: score every candidate as a
coverage-and-margin problem in milliseconds, pick the winner by a deterministic argmax over a total
order with an explicit tie-break, then run **one** short tick simulation on the winner alone to
confirm it holds up live. That dissolves the span question — there is no span, because there is no
clock — and it turns the candidate count from a budget question into a design one.

**The one thing a static check cannot see is price feedback**, and it is the failure that actually
killed the industrial field: processors buying inputs at the ceiling and being idled as
loss-making. The cheap proxy is a per-resource supply-to-demand **ratio** per market — the ratio is
static, and it is what drives a price to the ceiling in the first place. The validation run is what
confirms the proxy was good enough.

**What the objective is made of (Ben, 2026-09-06; the fifth term, Ben, 2026-09-08).** **Five
terms.** The third is what makes it an *asymmetry* objective rather than a coverage one, and the
fourth and fifth are the ones that can see a candidate at all:

1. **Chain completeness** — terminals closed over terminals total, per market. Does a chain reach a
   sink here at all.
2. **The supply-to-demand ratio**, per resource per market — the static price-feedback proxy above.
   A market whose ratio pins a good at the band edge is not saturated, it is broken, and only this
   term can tell the difference.
3. **The spread of the first two across markets, explicitly rewarded for unevenness.** Not tolerated
   as a side effect — *scored for*. A candidate landscape in which every market is equally complete
   scores worse than one with rich and poor markets at the same mean, because an even map is the
   outcome § Asymmetry is the deliverable exists to prevent, and a search that is merely neutral
   about evenness will drift toward it.
4. **Realisation — actual closure over potential closure.** Of the terminals a market *could*
   close, how many are closed by a building that actually exists in its catchment. A landscape
   with rich ground and no firms scores near 0; one whose firms close every chain the ground
   allows scores 1.
5. **Reach quality — at what traversal cost a market's catchment is actually crossed.** Not
   *whether* a resource is reachable, which terms 1 and 4 already read as a boolean, but how
   dearly. This is the only term a **road tier** can move, and it is read per market so that its
   spread feeds term 3 like the others.

**Why the fifth term exists, and why roads are invisible without it (Ben, 2026-09-08: “roads
should be visible in phase 6 too”).** A road tier scales traversal **cost** — a continuous
quantity. Terms 1 and 4 read reach through a **per-resource coverage boolean**: is there any
reachable deposit of resource R in this catchment. That boolean saturates. Once a catchment
already covers every resource it is ever going to cover, pulling more ground inside the reach
budget introduces no pair that was not already covered, and the score cannot move however much
cheaper the ground became. So an objective built only of terms 1–4 ranks every road tier
identically while the world underneath it genuinely changes — and the search then spends a third
of every round proposing a change it cannot score, looking like it explores three axes when it
explores two.

This is the **same shape** as the fourth term's own reason for existing, one level down: an axis
the objective is asked to choose along, and cannot see. The fix is the same — a term that reads
the axis in the currency the axis moves in.

**The constraint that makes it hard, and it must not be dodged.** Cheaper traversal everywhere is
trivially “better”, so a reach-quality term entered as a flat viability bonus would reward a
uniformly well-connected map — which is precisely the evenness § Asymmetry is the deliverable
and term 3 exist to prevent. A landscape where every market is equally cheap to cross must not
beat one with a well-served core and an expensive frontier at the same mean. The term is
therefore a **per-market reading whose spread is scored**, exactly like terms 1 and 2, never a
single global number added to the composite.

**What the term reads is left open, deliberately, and settled by measurement rather than by
argument.** Two candidates answer the axis: the mean traversal cost from a market to the tiles
in its catchment, and the in-reach tile **count** rather than the coverage boolean. Both are
continuous and both move when a tier does. The first slice's job is the one § The first slice is
the scorer already states — score candidates that differ only in road tier and ask whether the
chosen reading **discriminates between them at all**. A reading that does not is not worth
keeping, and finding that out is cheaper than building the rest on it.

**Why the fourth term is not optional, and why the first three could not do its job.** Terms 1–3
are computed from tiles, markets and population — none of which a candidate changes. Phase 6
chooses among **rosters**, so an objective made only of those three is blind to the choice it is
being asked to make; scored over candidates that differ by corporation count and placement seed,
every term comes back identical to the last digit. Realisation is what turns a measure of the
*world's* saturation potential into a measure of **this roster's** use of it.

Term 1 is kept rather than replaced, because it is the denominator: realisation without potential
alongside it cannot distinguish a roster that closed everything available on poor ground from one
that closed half of what rich ground offered. The pair is the reading; either alone is not.

**Recipe margin is deliberately NOT a term.** It exists (the registry computes revenue minus inputs
at base plus the wage per batch) and it stays the *authoring* check that every recipe can pay — but
it is a property of the roster, not of the landscape, and it is near-identical across candidates
that differ only in placement and road tier. Scoring it would add a constant to every candidate and
pull the objective back toward "most profitable", which is the reading point 4 above rejects.

**The first slice is the scorer, and its job is to fail informatively.** Before any search is built,
score a handful of hand-made candidate rosters and ask whether these terms **discriminate between
them at all**. If completeness is flat across every candidate, the search has nothing to search on
and everything downstream of it is wasted — that is a result worth having in an afternoon rather
than after the parallel harness is written.

**How a candidate is PRODUCED — greedy refinement (Ben, 2026-09-06).** The objective says how to
rank a landscape; this says where landscapes come from, and it was the half nothing owned.

The search **starts from one seed candidate, scores it, perturbs the winner along one axis,
re-scores, and keeps the better** — for a **fixed number of rounds**, never until convergence. The
three axes are the ones point 3 above names and no others: corporation **rosters**, starting
**placements**, and **road/infrastructure tiers**. A round proposes a perturbation on each axis,
scores the proposals, and the argmax over {incumbent, proposals} becomes the next incumbent.

**Fixed rounds, not convergence, and that is a determinism requirement rather than a budget one.**
A convergence test makes the amount of work depend on the landscape, so two worlds do the same
search for different lengths and a threshold becomes a hidden tuning knob. A fixed round count
makes the search a pure function of (world, seed, round count) with a cost known before it starts.

**What keeps it deterministic**, and every clause is load-bearing: perturbations are drawn from a
seeded stream in a fixed axis order; a round's proposals are scored independently and never in
completion order; the argmax runs over a **total** order with an explicit tie-break, so an exact
tie resolves the same way on every machine and at every thread count; and the incumbent is
replaced only on a **strict** improvement, so a tie leaves the incumbent standing rather than
churning between equals.

**Greedy is chosen knowing what it costs.** It will find a local optimum and not the global one —
which is acceptable here in a way it would not be elsewhere, because the objective is
*viable-but-uneven* rather than maximal. A landscape that is good enough and unevenly good is the
deliverable; a search that ground toward the single best landscape would be re-introducing exactly
the "most profitable" reading that point 4 rejects. The greedy walk also gives the search
something the alternatives do not: a **path**, so what a round changed and what it bought is
inspectable rather than being one draw among hundreds.

**Determinism is the binding constraint, and it survived the collapse of the budget argument.**
The parallelism was once load-bearing — six candidates at 53–71 s each is 5–7 minutes serially,
about one candidate's wall clock in parallel. The static ruling above deleted that arithmetic
along with the clock, so parallelism is now an optimisation and nothing rests on it. What does
**not** change is the constraint it imposed: each candidate must be a pure function of (shared
world, candidate seed, candidate parameters), and the winner chosen by a deterministic argmax
with an explicit tie-break — **never by which thread finished first**, and
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
| **3 — Settle** | The static candidate scorer, plus **one** validation run of `run_economy_step` on the winner | No calendar; the scorer has no clock and the validation run is short | Market conditions at game start: which firms exist, what each market can close, the price field |

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

**Pass 3 SELECTS a landscape; it does not settle one.** The earlier design made pass 3 the warm
start promoted — the same undirected pre-game ticks, run longer, with firm spawn added and firm
exit as the cull. That is superseded: § The eight phases retires the warm start outright and
replaces it with phase 6's **directed static search**, and the two are not variations of one
another. A settle asks *what survives whatever generation happened to place*; a search asks *which
placement is worth handing over*. Keeping both would pay twice for the weaker answer.

So the acts of pass 3 are the scorer's: score every candidate landscape statically on the three
terms in § The eight phases, pick the winner by a deterministic argmax with an explicit tie-break,
and run **one** short validation tick-simulation on that winner alone to confirm the static proxy
held. The player still enters a field that has already been selected rather than one about to be —
which is the property the warm start was there for, and the one thing that must not be lost.

Two constraints bound it, and both survive the change of mechanism. **Stable is not saturated**:
the objective is viable-but-uneven, never every chain closed, and the chain-completeness *spread*
is scored rather than merely tolerated. And **a search can only select over a roster that can
pay**: on a roster that loses at base price, every candidate loses and the argmax ranks degrees of
failure. The recipe-margin anchor (`../economy/PRODUCTION.md`) is therefore a **precondition** of
pass 3 — which is precisely why margin is not one of the scoring terms. It is the gate the roster
passes before the search runs, not an axis the search trades against.

**What crosses each handoff, and nothing else.**

- Pass 1 → pass 2: the region table, cultures, works, the strain accumulators. Nothing is reset.
- Pass 2 → the political map: the same outputs `generate_nations` reads today, plus the
  **polity map itself** — which polity held each region at the epoch, so a realm arrives as one
  nation rather than as a Voronoi cell per region — plus a nation's **tariff posture**, enacted
  as an ordinary `import_tariff` law at world setup where pass 2's polity ended up protective,
  and its **colonial ties**, which seed the order book's preferred-seller relationships so a
  colony's chains close through its metropole before they close anywhere else.

  **The fold is what makes phase 5 a finalisation.** Before it, the political map was re-derived
  from the region anchors as though the history had not just drawn one, and every empire the sim
  built was dissolved back into its provinces at the handoff. The carve is unchanged and stays
  unchanged: it answers where the line between two of a realm's own regions falls, which is a
  geometric question the history never asked. What crosses is whose flag flies over both.

  **A city state crosses as itself.** A polity that reached the epoch holding one region with a
  city on it is exempt from the size floor that would otherwise absorb it — "it is fine to
  consider city states as population centres" (Ben, point 5) — and never absorbs anybody, since a
  city state that annexed its neighbours would stop being one. A world with no city state is a
  legitimate outcome; the exemption is a permission, never a quota.
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
