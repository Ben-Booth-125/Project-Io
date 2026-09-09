# Project Io — Colonisation

> **Settles:** what the colonisation span is and where it ends · what a domestication
> package is, how it spreads, and what it gates · what makes people move and what that
> movement costs · what caps the pressure on a people that cannot spread, and what a
> cradle stopping actually means · where fragmentation comes from once no creed marches ·
> which culture a founded region inherits, and on what terms · what the span hands the
> settlement, sim and province layers · why no actor and no infrastructure appear anywhere
> in it.
> **Not here:** what a pantheon is or how a tongue is coined (../lore/CREEDS) · the stage
> ladder this span sits inside (../lore/HISTORY) · how force resolves once polities contest
> ground (MILITARY_HISTORY) · how the partition is drawn from what this span leaves
> (PROVINCES) · what a centre consumes once the campaign starts (../economy/POPULATION).
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

Colonisation is the **opening span of the ancient pass**, running from `start_year` to a
**stated boundary year** in `history_sim_params`. Before the boundary the world is filling;
after it the world is full enough that the interesting events are contests, and the polity loop
with its five scored verbs is the right model.

**The boundary is stated, not derived (Ben, 2026-09-09).** It follows the precedent the arc
calendar already set in `../lore/HISTORY.md` § The epoch and the run, where the ancient pass ends
at 1200 CE and the 1200 → 1560 coast is a stated span rather than a computed one. A derived
boundary — a settled-density threshold, or first sustained contact between peoples — was the
alternative and was not taken: both make the boundary a function of tuning constants elsewhere,
so a change to carrying capacity would silently move the shape of every generated history.

**A stated boundary is a scope decision, and the value is the sweep's to argue.** Like every
other magnitude in this layer it is calibrated in `history_sweep` and never in a harness
(`../lore/HISTORY.md` § Settlement — *calibration is the sweep's, not the harness's*). What a
harness may assert is the **structure**: that no polity verb runs before the boundary, and no
diffusion runs after it.

**Nothing resets at the boundary.** The region table, the culture shares and the demography carry
across exactly as they do at the ancient/industrial seam. What changes is which rules are running.

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
> long run static, and the 4000-year ladder is the target (`../lore/COLLAPSE.md` § The 4000-year
> problem).

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

**The non-hegemony floor survives the change and must be re-derived, not assumed.** The old
welding was floored at half the incoming fragmentation precisely so creeds alone could not
manufacture a hegemon. Contact-derived fragmentation needs its own equivalent bound; that it is
a different mechanism does not make BL-224's invariant somebody else's problem.

**This moves the nation count, and that is a measurement.** The seed budget is downstream, so the
distribution across a seed spread must be measured before the change lands — not argued from the
mechanism's shape. It is the one thing in this document whose consequence cannot be reasoned to.

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

---

## What the span hands downstream

| Consumer | What it receives | What changes for it |
|---|---|---|
| `run_settlement` | The founding set, with culture, package and `founded_year` | Founding *rules* unchanged; the set is earned rather than scattered |
| `run_history_sim` | A populated, culturally uneven map at the boundary year | Starts from a world with a settlement history instead of an even fill |
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

- **The boundary year's value.** Stated by design, unargued as yet; `history_sweep` is where it is
  settled, against the shape of the histories it produces.
- **The non-hegemony floor on contact-derived fragmentation.** § Fragmentation comes from contact
  retires a welding rule that carried an explicit half-fragmentation floor for BL-224's sake. The
  replacement needs its own bound, and what that bound is has not been derived.
- **The predation decay's coefficient.** The *form* is settled — logarithmic in population
  (§ Predation) — so what remains is one magnitude: how much a doubling buys. `history_sweep`'s
  to argue, like every other magnitude here.
- **What a package costs to represent.** Affinity over the two-axis terrain is a small fixed-width
  field per cradle, but the founding-time affinity test runs inside the walk, and the walk is the
  span's whole cost. Whether it is a table lookup or something cheaper is an implementation
  question this document does not settle.
