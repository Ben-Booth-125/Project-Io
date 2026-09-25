# Project Io — Colonisation

> **Settles:** what the colonisation span is and where it ends · what a domestication
> package is, how it spreads, and what it gates · how a cradle is announced, and the ground
> profile it coins beside its package · what makes people move and what that movement
> costs · what caps the pressure on a people that cannot spread, and what a cradle stopping
> actually means · where fragmentation comes from once no creed marches · which culture a
> founded region inherits, and on what terms · what the span records of its routes, and at
> which grain the round draws them · what the span hands the settlement, sim, round and
> province layers · why no actor and no infrastructure appear anywhere in it.
> **Not here:** what a pantheon is or how a tongue is coined (../lore/CREEDS) · the stage
> ladder this span sits inside (../lore/HISTORY) · how force resolves once polities contest
> ground (MILITARY_HISTORY) · how the partition is drawn from what this span leaves
> (PROVINCES) · what a centre consumes once the campaign starts (../economy/POPULATION) ·
> how the Culture round lays the record out on screen — its board, ticker, palette and
> playhead (../ui/STARTUP).
> **Confused with:** ../lore/HISTORY.md, MILITARY_HISTORY.md, PROVINCES.md.

**How a people comes to live where it lives, before anybody fights over it.**

The Era −1 sim plays polities forward with scored verbs, and `MILITARY_HISTORY.md` owns what
force does inside it. This document owns the span *before* that is the right model — the one in
which humanity spreads into empty ground, and the interesting question is not who wins but who
arrives, and by which road.

**The premise, in Ben's words (2026-09-09):** *"The rules for initial colonisation and spreading
of peaceful societies are vastly different from the later periods of war and empire."* And the
ordering that follows from it: **doctrine is downstream** of how a people came to live where it
lives, never an independent axis bolted on beside it.

---

## The span, and where it ends

Colonisation is **its own round of the generation wizard, and its own span** — the migration
that fills an empty world. It runs from `start_year` until **every habitable landmass carries
some culture**, and then it stops. What follows is a *separate* round with a separate span: **400 BCE to 1200 CE**, in which
polities contest what migration left (`CIVILISATION.md` § The span is 400 BCE to 1200 CE).

**THIS ROUND'S OWN SPAN IS 2400 BCE → 400 BCE, two thousand years**, inside a pass 1 that now
covers three thousand six hundred (Ben, 2026-09-09).

**A COAST JOINS THE TWO (Ben, 2026-09-09, confirming NR-818).** This round's end is derived and
the next round's start is stated, so they are not the same year and are not meant to be. When the
filling finishes, the world holds what migration left it until 400 BCE. A coast is the right
device for a span whose defining property is that little changes, and this is the one the design
keeps: the far side of 1200 CE is simulated, not coasted (`EXPLORATION.md`). Nothing is simulated
in between, and nothing is reset. Where the
filling outruns 400 BCE the record keeps its true end: the round may clamp what it *shows* at
400 BCE and say so in a caption, but that clamp is presentation, `../ui/STARTUP.md`'s, and never
a cut to the record (Ben, 2026-09-24).

**THE ROUNDS ARE NOT CONTINUOUS (Ben, 2026-09-09).** This is the framing correction, and it is
the load-bearing one: the migration round is not the opening slice of the ancient pass with the
polity loop bolted onto its end. It is a distinct process, with a distinct question — *how did
people come to be everywhere* — and it finishes when that question is answered. The history round
then starts from the world it left.

**THE TERMINATING CONDITION IS DERIVED, AND THIS OVERTURNS THE RULING OF EARLIER THE SAME DAY
(Ben, 2026-09-09).** This document previously settled the boundary as a **stated year**, on the
precedent of the arc calendar, and explicitly recorded the derived alternative as *considered and
not taken* — the objection being that a derived boundary makes the span a function of tuning
constants elsewhere. That objection is overruled by what the round is *for*. A migration round
that stops on a calendar year stops mid-migration on some worlds and long after the map filled on
others; on seed 0 the last **1,300 years of a 4,000-year span were measurably static**, which is a
third of the round showing nothing. The condition is now simply: **all land has some culture.**

The consequence the old ruling warned about is real and is accepted: the round's *length* now
varies per world. That is correct for a process whose whole subject is how long the filling took.

**Nothing resets between the rounds.** The region table, the culture shares and the demography
carry across exactly as they do at the ancient/industrial seam. What changes is which rules run.

---

## Most of the land is habitable, and it carries people

**Ben, 2026-09-09, on watching the round: the aim is for MOST of the land to be habitable and to
hold human population.** The first build left roughly half the homeworld permanently grey — ground
no cradle package could farm, which the model reads as a legitimate outcome and a player reads as
a broken map.

**Emptiness stays a real outcome; it stops being the DEFAULT one.** § The domestication package's
"ground no package suits is not settled" is unchanged as a rule. What changes is the calibration
target it is measured against: a finished migration round should leave the map substantially
peopled, with unsettled ground the exception that marks genuinely hostile country — not the
background condition of half a continent.

This is the acceptance criterion the sweep argues the package's affinity floor and breadth
against, and it is a **measurement**, not a constant to be clamped: if most land is not habitable,
the fix is in what a package can farm and how it broadens, never a floor that fills the map
regardless.

---

## Migration spawns cultures

**A culture is not only something a cradle coins; it is something a migration PRODUCES (Ben,
2026-09-09).**

The first build carried exactly one culture per cradle, for the life of the run — so a stream that
crossed a continent arrived as the same people who set out, and a world with five viable cradles
had five cultures at the epoch however far anybody walked. That is the wrong shape for a round
whose subject is migration: distance and time are precisely what make a people diverge.

**A stream that has travelled far enough from its origin founds a NEW culture** rather than
extending its parent. What "far enough" is — in years walked, in distance, or in some combination
— is the sweep's to argue like every other magnitude here. What is settled is the shape:

- the daughter culture is **derived from its parent**, not rolled fresh — it inherits the
  pantheon's shape and the tongue's phonology, and diverges from them, exactly as
  `../lore/CREEDS.md` already coins a tongue from a parent;
- the parent's package travels with it, subject to § Packages broaden by crossing;
- the split is a **consequence of the walk**, never a decision — no actor, in the sense this
  document's own § No actor, and no infrastructure means it.

**This is what makes the round's output worth looking at.** Twelve cradles producing five surviving
peoples is a map of where agriculture started. Twelve cradles producing dozens of related peoples,
grouped by the routes their ancestors took, is a map of a *migration* — and it is the input the
history round needs if its contests are to be between neighbours who are recognisably kin or
recognisably not.

**Diversity is the deliverable, and the splits are tuned TOWARD it (Ben, 2026-09-11).** Many
cultures that are recognisably similar are worth more than one culture over a lot of ground, so
where a magnitude is in doubt the split is set to fire *more* readily, not less — *"if you need to
over-tune splits, I encourage that."* Three things divide a people, and each is a consequence of
the ground and the walk, never a roll: **distance** (a stream that has travelled far enough
diverges), **biome** (a people that settles ground of a different farm class than the one it was
coined on becomes a daughter — a barrier landform, a marsh, a forest, a desert is a cultural rift
as much as a farming one), and **isolation** (a single culture spread across a range whose parts
are cut off from one another — by mountains, by water, by sheer distance — breaks down into
insular groups over time). All three are **visible inside the round's own time-lapse**: a lobe
changing hue as it crosses a barrier, a range coming apart into kin, are what the player watches,
not a count read off at the end. Daughters stay kin — the pantheon and the tongue are inherited
and diverge, so the map reads as families of peoples rather than as noise. The isolation split
belongs to this span alone (Ben, 2026-09-11): the migration ends with the families it made, and the
Empires phase makes peoples only by mixing them (`CIVILISATION.md` § A civilisation is what mixing
makes).

**A range comes apart on the map when the ticker says it does (Ben, 2026-09-24).** A region the
isolation split moves to a daughter is recorded twice, each change dated: founded under its
parent at its founding year, re-cultured to the daughter at the split year. So it wears the
parent's hue until the split and the daughter's after it, and the moment the record announces
the split is the moment the map shows it. Painting such a region in the daughter's hue from its
founding would show the range already apart centuries before it divided — the record
contradicting itself.

**Over-tuning is paid for at the boundary, not at the split (Ben, 2026-09-16, NR-879).** A daughter
that ends the round holding no ground folds back into its nearest living ancestor, keeping the
lineage link; the splits themselves are never gated. `CIVILISATION.md` § Empty cultures fold into
their parent owns the rule.

**Folded daughters are counted, not narrated (Ben, 2026-09-24).** The great majority of the
peoples the walk coins hold no ground by the round's end, and a ticker that named each one "on
the march" would drown the founding it sits beside. A daughter that folds is a figure on the
round's board — peoples coined, peoples folded, read from the settlement's own census — never a
ticker line of its own. The ticker names the peoples that come to hold ground.

---

## Coastal and overseas routes are the ones that need emphasis

**Ben, 2026-09-09: there is not enough emphasis on crude coastal and overseas migration routes.**

The walk prices the shoreline as cheap and open ocean as impassable, and that is currently the whole
of the model's relationship with water. Two things are missing, and both are what actually moved
people:

- **The coast is not merely cheap, it is the ROAD.** Early migration follows shorelines because
  the shore feeds you while you walk it. The shoreline discount should be strong enough that a
  coastal route beats an inland one over any comparable distance, so the map's first cultures string
  out along the coasts and only later push inland.
- **Crude overseas hops exist.** Open ocean is impassable to a walk and was never impassable to
  people: a short crossing to a visible island or across a strait is exactly how the awkward
  corners of a world get peopled. What is wanted is the **crude** version — a bounded hop across a
  small number of water tiles, not seafaring, not a naval capability, and emphatically not the
  staged harbour-works model of `MILITARY_HISTORY.md` § Sea legs, which belongs to a later era with
  institutions in it. A hop costs years like everything else here, and it is a fact of the route
  (§ The route record) — which is what lets the map draw it as a crossing.

Both are the same claim the span already makes about mountains, applied to water: **the routes
people actually followed should be the cheap ones**, and the map should show it.

---

## No actor, and no infrastructure

Two exclusions, both load-bearing, both stated before the mechanisms so nothing below is read as
softening them.

**Nothing decides to colonise.** There is no verb, no score, no utility comparison, and no polity
making a call. Surplus population, the ground in front of it, and the package it carries are the
whole of the model; where people end up is a **consequence** of those three in the sense
`GENERATION_STRATEGY.md` § Asymmetry is the deliverable means it — a deterministic function of
upstream scalars, never a roll and never a plan.

> **This is why the span needs no AI-behaviour grant, and it must not be read as one.**
> `.claude/rules/io-standing-rules.md` requires that a new widening of the AI-behaviour
> prohibition be *raised*, never assumed. Colonisation raises nothing because it contains no
> agent: a diffusion is a force, not an actor, and the register in `../ai/AI_OPPONENT.md` § 11
> is untouched by this document. Should a later design want a people to *choose* where to
> spread, that is a new grant and it is Ben's to give.

**Colonisation consumes no infrastructure (Ben, 2026-09-09).** No roads, no works, no logistics
network, no supply. The **only** cost of colonisation is that people physically move, and that
cost is paid in **years**. This is not a simplification awaiting enrichment; it is the correct
model for a span running before any of those systems exists in the world it is generating.

The rule has a consequence worth stating, because it is what makes the span cheap: since nothing
is spent, nothing must be budgeted, accounted or balanced. A stream either has reached ground or
has not reached it yet.

---

## The domestication package

**A package is the bundle of domesticated clades a cradle raised from its own biosphere**, and it
is the mechanism that makes distance from a cradle cost something. Without it every region is
founded on identical terms whatever its distance from the first granary — so there is no frontier,
nothing stalls, and a migration route carries nothing but headcount.

**One package per cradle, coined exactly as one pantheon is.** A cradle rolls its package at
Stage 0 from the `endemics` its window holds (`PLANETOLOGY.md`'s generated biosphere, already the
input to `agrarian_score`). Package and pantheon are the same act of self-description read on two
axes — which is why they travel together, and why the map of gods and the map of farming end up
being the same map rather than two maps that happen to agree.

A package carries two things:

- **Affinity** — which terrain classes it can farm, and how well, over the two-axis terrain of
  `../economy/TILES.md`. Derived from the cradle's own ground: a package coined on a floodplain
  farms floodplains, and carries nothing about a steppe it never saw.
- **Breadth** — how many classes it tolerates at all, derived from how varied the cradle's own
  window was. A cradle in uniform country coins a **narrow** package; a cradle spanning a
  gradient coins a **broad** one.

**Breadth is the span's asymmetry generator, and that is its job.** A broad package colonises a
continent; a narrow one fills its valley and stops. Two cradles of identical richness therefore
produce wildly different worlds, and neither is clamped toward the other — which is precisely
what `GENERATION_STRATEGY.md` § Asymmetry is the deliverable asks generation to produce, and
currently has no instrument for.

**Ground no package suits is not settled**, and stays empty for as long as that holds. Emptiness
is a real outcome here, not a failure to fill.

**The cradle is announced (Ben, 2026-09-24).** Every cradle people opens the round with a
`cradle` moment — one per people, pinned to the cradle's seat and dated the span's start,
2400 BCE — that names the people and the package it raised. For the round to say them, the
cradle names and the packages are retained on the settlement record as **pure outputs** of the
pass: written once at the coining, read by the round, read by nothing inside the walk.

**Otherwise a package is a fact of the STREAM, never of the ground.** It rides on the source a
settled region sends from, is unioned there when a crossing happens (§ Packages broaden by
crossing), and is not a field on a founded region. A founding records its culture and its year,
and the route record says where its people came from (§ The route record); the package its
stream carried is the cradle's, or the union two cradles' made — a property of the people in
motion, not of the ground they settled.

### Packages broaden by crossing

Where a stream carrying package A settles ground marginal for A and adjacent to a people carrying
package B, the founded region carries the **union** of the two affinities, floored so a daughter
is never better than the better parent. Nobody trades and nobody decides; adjacency is the whole
mechanism.

This is the only way a stalled frontier ever moves again, and it makes **contact between peoples
productive** rather than exclusively violent — which a peaceful span needs, since contact is
otherwise only ever the thing that ends it.

> **Kept deliberately, not by default (Ben, 2026-09-09).** Cutting it was the live alternative:
> the world's settlement pattern would then be fully determined at Stage 0 by the cradle windows,
> which is simpler and defensible. It survives because a frontier that can never unstick makes the
> long run static, and the 4000-year ladder is the target (`CIVILISATION.md` § The long run is paid
> for by the table, not the fighting).

### The ground profile

**Materials shape culture (Ben, 2026-09-24; the profile's composition below confirmed and its
reader ruled in, 2026-09-25, NR-926).** A cradle coins a **ground profile** beside its
package, from the same window: the deposits the window holds, summed in the four classes the
founding survey reads — farm, ore, energy, water — plus one **amenity class** read off the
window's cover, the ground `../economy/TILES.md` § Amenity tiles names (forest, coastal grass,
marsh in a valley). Package, pantheon and profile are one act of self-description read on three
axes: what the cradle's people learned to farm, what they came to believe, and what their
country was *made of* and *felt like*.

The profile is stored on the culture beside the farm class it was coined on, and a daughter
inherits it whole with the rest of its descent. It is a fact about where a people **began**,
never about where it now stands — exactly as its origin farm class is — so a people that walked
a continent still carries the profile of the valley that coined it.

**Its reader is the cultural preference for goods**
(`EXPLORATION.md` § A good acquires a cultural preference), which derives what a people wants
from what its ground never held and what its route exposed it to: the profile is the first of
those two inputs, at culture grain. That is what lets a people that never raises a polity carry
a preference all the same — even a primitive culture leans toward the amenities of the country
it was coined on. Like the package, the profile is seeded and never rolled: the same window, and
no die.

---

## The stream — what moves, and what it costs

**Surplus is the only thing that moves.** A settled region whose population exceeds
`region_carrying_capacity(farm_q)` carries surplus; a region at or under capacity sends nobody.
The demography producing it is already built and is not this document's —
`advance_region_demography`, in `../economy/POPULATION.md` § Region demography, owns it.

**Surplus flows to the cheapest reachable ground its package can farm.** Cheapest by a walk whose
cost is **ground alone**:

- river courses and coastal shelf are cheap — the routes people actually followed;
- open lowland is ordinary;
- barrier terrain — mountain, desert, dense forest — is dear;
- open ocean is impassable, gated by `region::domain`, which already exists and is already the
  real water test (`MILITARY_HISTORY.md` § Regions carry a domain).

**The walk's cost is denominated in years.** A stream crossing dear ground takes longer to arrive;
that is the entire cost model, and it is what *people physically move* means when nothing may be
spent. A range that costs a stream three centuries to cross is a range that shaped a civilisation,
and it did so without a single resource changing hands.

**This is the same terrain that later stalls an army**, and the coincidence is the point. Stage 2's
central lore claim — geography kept conquest expensive and exit cheap, so no hegemon ever formed
(`../lore/HISTORY.md` § Stage 2) — currently rests on an assertion. Under this model the ranges
that slowed the spreading are the ranges that slow the conquering, read from one terrain table by
two consumers. The claim becomes a property of the map rather than a sentence about it.

**Reaching ground founds a region on settlement's existing terms.** `run_settlement`'s founding
rules — the endowment survey, the furnace gate and its lag, the naming — are unchanged. What
colonisation replaces is *which* ground is founded, *when*, and *by whom*; it does not replace
what a founding is.

---

## Predation — what caps the pressure

**Ben, 2026-09-09, answering what a penned people does with surplus it cannot send anywhere:**
*"Death from natural causes can put a cap on said pressure. We won't model the whole tree of life,
but we can make certain areas start with varying wildlife safety measures."*

**A region carries a predation scalar, and it is a consequence rather than a roll.** Two terms,
both read from passes that already run:

- the **body** term, from `PLANETOLOGY.md`'s simulated biosphere — how far life got and how
  productive it is. A world that never reached land animals carries none of this at all;
- the **region** term, from terrain cover — dense forest and wetland are dangerous, open lowland
  and cold country much less so.

**What it does is raise baseline mortality**, holding sustainable population *below*
`region_carrying_capacity(farm_q)`. That is the cap: a penned people's surplus is consumed by
death rather than accumulating into a pressure the model has nowhere to send. The brake is an
in-world force with a cause a player can point at, never a term inside anybody's head.

**It prices the same ground twice, and that is why it earns its place.** Dense forest is dear for
a stream to cross *and* dangerous to live in once crossed, so forest colonisation stalls on both
terms at once — with no constant tuned to make it happen. Good farming ground in dangerous country
becomes a real dilemma rather than a free pick.

**Settlement clears predators, and the decay is LOGARITHMIC IN POPULATION (Ben, 2026-09-09).**
Predation falls with the log of the region's headcount, so **each doubling of population buys the
same fixed reduction** and the returns diminish forever. The first settlers pay the most and their
descendants pay steadily less, without any population ever quite clearing the ground.

That form answers the open call this section previously carried. Predation is a **transient
frontier cost** rather than a permanent wall — ground that killed a cradle's founders can be taken
by its descendants — but it is **never fully bought off**, because a log curve has no population at
which it is done. Wild country stays marginally wild. A permanent wall would have made the long run
static in exactly the way § Packages broaden by crossing exists to avoid; a decay that reached zero
would have made every frontier temporary, which is the same defect wearing the opposite sign.

**It reads CURRENT population, so predation comes back.** A region the sim sacks loses the heads
that were holding the wild down, and its predation rises again toward what the ground carries on
its own. **Conquest re-wilds ground** — for free, out of a rule written for something else, and it
gives `centres_razed` (`../economy/POPULATION.md` § Generation) a consequence beyond the record.

**It is not `hazard`, and must not be folded into it.** `tile_component::hazard_level` is an
*extraction* danger consumed by the economy's site draw, and Ben ruled against `hazard` as a
body-scale state on 2026-08-31 — its design job is off-world. Predation is a distinct quantity
with a distinct consumer, and sharing the name would silently couple two unrelated models.

### What a cradle stopping actually means

Four outcomes look identical from outside — *the cradle stopped* — and only one is death. They are
listed because a sweep that cannot separate them cannot argue a single constant in this document.

| Outcome | What happened | Is it death |
|---|---|---|
| **Sterility** | Narrow affinity, no adjacent ground of a matching class | No — sessile. People persist, gods never leave home |
| **Encirclement** | Affinity is fine; every exit is barrier terrain whose year-cost the span never pays | No — sessile, and distinguishable from sterility only by *why* the stream stopped |
| **Dilution** | A neighbour's stream reached the same ground and `culture_shares` split | No — the gods survive as a minority share, record intact |
| **The predation floor** | Sustainable population sits below the density Stage 0 needs for surplus | Yes, and it should never be reached |

**The predation floor is a selection test, not a failure mode.** `agrarian_score` reads predation,
so lethal ground is never chosen as a cradle in the first place. A cradle that forms and then fails
is the worse design of the two: it spends a simulation discovering what a score could have said for
free.

**Sessile is a normal outcome and not a defect.** A cradle that fills its valley and stops for four
thousand years is a legitimate history, and § The domestication package's breadth term makes it a
frequent one by design.

---

## Fragmentation comes from contact

**Ben, 2026-09-09, resolving where the creeds' tribal marches sit: they retire, and
fragmentation is re-derived from contact.**

`fragmentation_q` drives the seed budget, and the seed budget drives how many nations a world
has — so something must produce it. It used to be produced by war: a culture whose
`aggression_q` cleared its neighbour's defence marched, and a won war **welded** two cradles into
one (`../lore/CREEDS.md` § The creed drives). That is retired.

**It is now read off how far two peoples' streams interpenetrate.** Two cultures whose shares mix
heavily across a broad frontier are **less** fragmented; two that never met are **more**. The
input is `culture_shares`, which the span already produces at every founding — so the same number
comes out of a mechanism already running, with no roll, no pairwise comparison, and no war.

**The grain was the real defect, not the timing.** The marches compared *cradle to cradle*: a
scalar against a scalar, between two discrete points. Under diffusion a cradle is a **source**,
not a point, and what meets at a frontier is two streams that are already mixing. The marches
were comparing objects that had stopped describing where contact happens. Moving them past the
boundary would have fixed when they ran and left that untouched.

**And it separates two causes that were tangled.** `aggression_q` used to drive both how
consolidated the political map is *and* how a polity fights once it exists. Now **package
breadth** drives consolidation — a people who spread far ends up one culture over a lot of ground
— and aggression drives only conduct after the boundary. One cause each.

**The non-hegemony floor survives the change, re-derived over the same base value (BL-852).** The
old welding was floored at half the INCOMING fragmentation, so creeds alone could not manufacture
a hegemon. The replacement keeps the identical shape: contact can pull `fragmentation_q` down but
never below half the STRUCTURAL reading Stage 0/3 computed from terrain and cradle count alone,
before any culture existed to meet another. Same invariant, same base value, a different force
doing the pulling.

**This moved the nation count, and it was measured.** The seed budget is downstream, so the
distribution across a seed spread was measured before the change landed (`world_audit`'s six-seed
sweep) rather than argued from the mechanism's shape — it was the one thing in this document whose
consequence could not be reasoned to. It moved modestly: median nation count within a few nations
of the pre-BL-852 reading, same order of magnitude, no seed pushed outside `BL-053`'s runaway
guard.

---

## Culture arrives by route

**A region inherits the culture of the stream that reached it.**

This supersedes the proximity rule in `../lore/HISTORY.md` § Settlement — Stages 3–4, and in `../lore/CREEDS.md`
§ Where a pantheon sits on the ground — *a region inherits its nearest cradle's culture* — which
is a straight-line geometric assignment standing in for a history nothing was simulating. Under
diffusion the same field becomes a **path fact**: whoever's stream arrived, arrived.

Two consequences, and they are the whole of what *migration routes colour differing philosophies*
means mechanically:

- **A cradle does not own the ground on the far side of its mountains.** The culture that came the
  long way round by the coast does, because it got there first. The distribution of gods stops
  being a Voronoi of cradles and becomes a **record of routes** — the thing `CREEDS.md` already
  claims it is ("a record of who walked where") and could not previously deliver, because nobody
  walked.
- **Belief and subsistence agree by construction.** Package and pantheon ride the same stream, so
  "the forge god's country industrialises early" (`../lore/HISTORY.md` § Stage 4) stops being a
  special case wired between two passes and becomes one instance of a general rule.

**No new axis, and none is wanted (Ben, 2026-09-09).** A philosophy is not a new quantity beside
the pantheon; the pantheon **is** the philosophy, and `aggression_q`, derived from its war god,
remains exactly the doctrine input the sim reads. What changed is that the distribution is now
**earned** rather than assigned — the same data, arrived at causally.

**Two streams reaching the same ground need no new type.** `region::culture` is already a
`culture_shares` distribution, so a region reached from two directions inside the span carries
both, and the assimilation machinery that resolves it already exists.

### The route record

**A route exists at two grains, and the record says which is which (Ben, 2026-09-24).** "A
record of who walked where" is a claim this document makes of the map; this is what the span
keeps so the claim can be drawn.

**Region grain — what the round draws.** The migration record holds one dated change per
founding, owned by the people who founded it, and the round draws its **kin arrows** from that
record alone: each founding a line from its people's *previous* region — the region the same
people most recently founded before it in the record — to the new one, fading over the years
after the founding, dashed where the line between the two anchors crosses water: the crude hop,
at region grain. **"Crosses" means at least two interior samples of the line fall on water (Ben,
2026-09-25, NR-942):** a strait is a crossing, and a coast-hugging people's one-tile graze of a
bay is not. Any single wet sample would dash nearly every coastal founding and hide the hops; the
road bake's majority rule would dash none. The arrow is honest about its grain. It says a people was there and is now
here; it does not say which stream carried them, which shore they walked or which strait they
crossed. The exact answer — the source region whose stream reached the founded anchor — is the
walk's, and the hop record below is what keeps it.

**Hop grain — what a true route needs.** The walk moves tile by tile, and an overseas hop is a
bounded crossing from a shore tile to a landfall tile; a route *is* that sequence. The **hop
record** keeps it: one entry per hop a stream takes — the region it flows from, the tile it
left, the tile it landed on, the year, the culture it carried and whether the hop was water —
written by the walk as it claims ground, rather than dying with the walk's field when the pass
returns. Only that record draws a coastal route as a coast and a crossing as the strait it
actually crossed; the kin arrow is its projection onto the region table. The walk knows every
hop it takes, so the record is the walk keeping what it knows — a cost of retention, never of
recomputation.

Both grains are **records, not mechanisms**: neither changes where anybody arrives, and a world
generated with or without them is the same world. That is the test either must keep passing.

---

## What the span hands downstream

| Consumer | What it receives | What changes for it |
|---|---|---|
| The founding rules (`run_settlement`) | The founding set: each region with its culture and its `founded_year`. **No package rides on a founding** — a package is coined per cradle, carried by the stream and unioned where two meet (§ The domestication package); it is retained only as the cradle's own record | Founding *rules* unchanged; the set is earned rather than scattered |
| The **Culture round** (`../ui/STARTUP.md`) | The migration record: one dated change per founding and a second per re-culturing, the `cradle` and split moments (from which the kin arrows are drawn — § The route record), the cradle names and packages, and the census the board counts folded daughters from | Draws the record; the record is the round's whole input |
| The **cultural preference for goods** (`EXPLORATION.md` § A good acquires a cultural preference) | Each culture's ground profile (§ The ground profile), inherited down the tree | Reads what a people's first ground held, at culture grain |
| The **history round** (`run_history_sim`) | A filled, culturally uneven map — every habitable landmass carrying some culture | Starts from a world with a migration behind it rather than an even fill. A SEPARATE round with its own span (`../ui/STARTUP.md`), not a continuation of this one |
| The province partition | **The settled cells and their anchors, as a hard input** | A new binding input; ordering and nation lock unchanged |
| `../economy/POPULATION.md` | Regions whose `centres` record begins at a founding the span dates | Nothing — the urban record already works this way |

**The province ruling, stated exactly (Ben, 2026-09-09).** Colonisation **seeds** the partition;
the late pass still **draws** it. `PROVINCES.md` keeps its ordering — after the sim, before roads
— and keeps its nation lock, and gains the settled cells as a hard input the way it already takes
the national assignment. The stronger alternative was considered and not taken: colonisation
drawing the real partition, with the nation carve then assigning whole provinces rather than
tiles. That would make single-nation provinces true by construction rather than by a check, and
it reorders the generation chain — which is why it is recorded here as the road not taken, rather
than left to be rediscovered.

---

## Open questions

- **What "all land has some culture" means exactly.** The terminating condition is settled in
  shape (§ The span, and where it ends) and not in detail: whether it means every habitable tile,
  every landmass above some size, or a share of habitable land above a threshold — and what the
  round does about ground no package can farm, which by construction never gets a culture and so
  could hold the condition open forever. A safety stop is needed and its form is not settled.
- **How far is far enough to spawn a culture.** § Migration spawns cultures settles that distance
  divides a people and leaves the magnitude — years walked, distance, or both — to `history_sweep`.
- **How strong the coastal discount and how long an overseas hop.** § Coastal and overseas routes
  settles that both exist and that the crossing is CRUDE. The numbers are the sweep's, and the
  acceptance test is whether the first cultures string along the coasts.
- **What share of land counts as "most".** § Most of the land is habitable states the target and
  deliberately does not put a number on it; the sweep argues it against the package's affinity
  floor and breadth, never against a floor that fills the map regardless.
- **The predation decay's coefficient.** The *form* is settled — logarithmic in population
  (§ Predation) — so what remains is one magnitude: how much a doubling buys. `history_sweep`'s
  to argue, like every other magnitude here.
- **What a package costs to represent.** Affinity over the two-axis terrain is a small fixed-width
  field per cradle, but the founding-time affinity test runs inside the walk, and the walk is the
  span's whole cost. Whether it is a table lookup or something cheaper is an implementation
  question this document does not settle.
