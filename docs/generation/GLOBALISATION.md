# Project Io — Globalisation

> **Settles:** what the exploration age is for and which question it answers · what capital is
> in a phase that has one for the first time · what a treaty is as data and what it binds · what
> a colony is and why it wants things of its own · how ports, navies and standing armies are
> paid for and how they decay · how a good acquires a cultural preference · why conflict moves
> away from the home coast and out to the frontier · what the phase hands digitisation.
> **Not here:** what crosses into it from the Empire phase (CIVILISATION § The closure of the
> Empire era) · the nodes of its technology tree and the grammar every tree obeys
> (trees/GLOBALISATION_TREE, trees/TREES) · the phase after it (DIGITISATION) · how a market
> clears once there is a price (../economy/MARKETS) · the campaign-era money loop
> (../economy/FINANCE) · the pass map and the calendar (GENERATION_STRATEGY § Pass 2).
> **Confused with:** CIVILISATION.md, COLONISATION.md, DIGITISATION.md,
> ../economy/MARKETS.md.

**Globalisation is the third simulated span and the first with a price on anything.** It runs
**1200 → 1660 CE, 460 years**, opening the instant the Empire phase closes — the 1200 → 1560
coast is retired (`GENERATION_STRATEGY.md` § Pass 2). Where Empires asks *who holds this ground*,
Globalisation asks *who wants what someone else holds, and what they will do about it*.

**Ben, 2026-09-11, naming the phase's two jobs:** *"to stimulate international demand for goods,
and strengthen the economic power of colonial actors."* Every mechanism below exists to move one
of those two, and a mechanism that moves neither has no business here.

**The name is Globalisation** (Ben, 2026-09-11). *Exploration* describes what the age looks like
and is not a term; the phase, its tree and its store all carry the one name.

---

## The shape of the age, and why it is not Empires again

**Empires is a phase of fear and adjacency; Globalisation is a phase of appetite and distance.**
The Empire phase's engine is a neighbour who might come for you, and its verbs resolve against
ground you can walk to. That engine produced the world this one starts on, and running it again
for another 460 years would produce more of the same world.

**So the signature of this phase is that conflict MOVES.** Ben, 2026-09-11: *"we will need to
solidify diplomatic ties, so that conflicts close to home are less likely than skirmish conflicts
over territorial bodies"* — and, clarifying the same day, **the territory in question is on the
home planet**. Other continents, not other worlds; Era 2 owns the sky (`../economy/ERAS.md`).

That gives the phase one structural claim, and it is the claim the sweep must confirm: **the rate
of conflict between long-contacted neighbours FALLS across the span while the rate of conflict on
newly-contacted ground RISES.** A phase in which both fall is a peace nobody asked for; a phase in
which both rise is Empires wearing a hat.

**Three things make that displacement happen, and none of them is a term inside an actor.** The
standing rule against agent handicaps binds here as everywhere: treaties raise the *cost* of a
neighbour war, ports lower the *cost* of a distant one, and wants point *outward* because contact
is what creates them. The actor's scorer is untouched; the world around it changed price.

---

## Capital arrives, as one number per polity

**SETTLED (Ben, 2026-09-11): capital is a per-polity TREASURY SCALAR.** Not valued regional
stores, not a per-good ledger. One integer per polity, in the sim's usual 0–1000-style quantised
currency, earned and spent.

**This is where `CIVILISATION.md`'s deferral lands.** That phase abstracts natural resources into
what conquest consumes and says explicitly that *resources become capital in pass 2, not here*.
Globalisation is pass 2's first half, so the promise is kept exactly here and the Empire phase
stays priceless.

**Why one scalar rather than a valued map.** A treasury is the smallest object that can express
the three things this phase needs to price — a port that must be built, a fleet that must be kept,
and a treaty that must be worth more than a war. A regional-stores model would price all three
*and* re-open who-may-spend-what, which is a logistics question this phase does not ask. When
Digitisation needs balance sheets it will build them on firms, not by retrofitting this.

**The treasury is a consequence, never an allowance.** It is fed by what the polity already holds
and already reaches — endowment on held ground, the surviving corridor network it inherited, the
markets standing on its seats, and what its colonies remit. Nothing tops it up; a polity that
inherited little is poor, which is the asymmetry the handoff exists to produce.

**No market clears in this phase, and the treasury does not need one.** Goods are named and their
preference is legible (§ A good acquires a cultural preference); price discovery, order books and
the price field are Digitisation's (`../economy/MARKETS.md`). What this phase has is a quantity
of capital and a set of things it can buy.

---

## Diplomacy becomes real, and this phase is where the exclusion lifts

**`CIVILISATION.md` § Fear of being next states three exclusions, and the third is that there are
no treaties, no negotiation and no alliance objects.** That exclusion is correct *for Empires*:
it is what keeps the coalition a fear response inside a generation sim rather than a diplomacy
layer, and nothing here relaxes it backwards.

**Ben, 2026-09-11, overturning it FORWARD:** *"we should move away from the rapid claim and grudge
system. We should be creating lengthy treaties and pushing slowly towards gaining territory."* So
Globalisation has real treaty objects, and Empires still does not. The two phases sharing an
engine (§ The engine is shared) does not make them share a diplomatic model.

### What a treaty is

**A directed or mutual pair, a term of years, and one or more bound clauses.** It is an object in
the same family as `grudge` and `contact` — a named pair plus what joined them — with the two
additions this phase needs: it **expires**, and it **binds** rather than merely records.

Five clause kinds, all five settled (Ben, 2026-09-11):

| Clause | What it binds | Why the phase needs it |
|---|---|---|
| **Non-aggression** | Neither party campaigns against the other while the term runs | The direct cause of the home-coast quiet |
| **Trade access** | One party's market is legible and reachable to the other | Turns a want into a flow instead of a landing |
| **Sphere of claim** | Which unclaimed ground each party may claim | Two powers divide a frontier instead of fighting over it |
| **Tribute** | A remittance from subject to overlord | The colonial actor's economic strengthening, made explicit |
| **Mutual defence** | An attack on one draws the other | Makes a small polity expensive to eat |

**A term of years is what makes it "lengthy" rather than a stance.** `../politics/RELATIONS.md`
separates declared stance from derived sentiment; a treaty is neither. It is a *commitment with
an end date*, and the end date is the whole reason a polity plans around it.

**Breaking one is legal and costly.** A treaty that cannot be broken is a rule, not a promise, and
a phase whose actors never defect produces a flat map. Defection writes a grudge in the existing
table — which is how the Empire phase's machinery is reused rather than duplicated — and the cost
lands on every *other* party's willingness to bind with the defector. Nothing reads the defector's
own ledger to choose a target; that remains revenge and remains declined.

**Nobody negotiates.** A treaty forms when both parties independently score the binding as worth
more than the freedom it costs, evaluated against the same seeded world state. This keeps the
determinism rule intact and keeps the phase free of a bargaining layer it cannot afford.

---

## A colony is a subject, and it wants things of its own

**SETTLED (Ben, 2026-09-11): a colony is a LIVE POLITY with an overlord link.** Not a region
annotation and not a new actor class. The polity table already holds everything a colony needs —
a capital, a culture, cohesion, held ground, a tree mask — and the colony's whole distinction is
that one field points at somebody else.

**`polity::parent` is NOT that field.** `parent` records lineage: who this polity broke away
*from*. Subjection is a live relation that can begin, end, and be transferred, and overloading
lineage onto it would make a colony indistinguishable from a secession the moment either changes.
The overlord link is its own field.

**Ben, 2026-09-11: colonies are subjects "with their own desires."** That is the load-bearing
half. A colony keeps its own entry in the want table, and its wants are its own — pointed at what
its ground lacks and its neighbours hold, not at what the metropole needs. Two consequences follow
and both are wanted: a colony can want something its overlord does not care about, and a colony's
wants can point at a *rival's* colony across a frontier the metropoles have treatied over.

**A colony that can pay for itself can stop paying.** Tribute is a treaty clause, and a clause
with a term is a clause that comes up for renewal. The condition under which a subject refuses
renewal is the phase's own secession mechanism, and it should read the same quantities the Empire
phase's secession reads — cohesion, distance, and whether the overlord is reachable at all — so
that a colonial empire outrunning its network is the same failure as a land empire doing it.

### Where subjects come from

**What is discovered is people, not empty land** — `CIVILISATION.md` § Contact is polity to
polity fixes that, and it binds here. The migration filled every habitable landmass, so a far
continent at 1200 CE is settled and unmet. **There is no terra nullius to claim.**

So a subject arises one of two ways, and Ben, 2026-09-11 names both: *"the most technologically
dominant nations are able to lay claim to polities they have no direct control over — beginning to
sprout trade provinces, and steal land from natives."*

- **A trade province** — a foothold on a native polity's coast, taken by a power that arrived with
  capital and a want. It is a *seat with a market and an overlord*, not a conquest: the native
  polity survives beside it, which is what makes the relation extractive rather than terminal.
- **A subjected polity** — the native polity itself, brought under an overlord link, whole.

**The asymmetry that permits either is technological, and it is a node, not a rank.** Nothing
reads size. What decides who can arrive at all is what the arriving polity holds — the sea legs
and the institutions — exactly as the explorer set at 1200 CE is a held rim milestone rather than
a ranking (`CIVILISATION.md` § Some peoples gain the capacity to explore).

---

## Force persists now, and persistence has a bill

**SETTLED (Ben, 2026-09-11): `army_stock` CARRIES across the handoff; the navy is NEW and starts
at zero, everywhere.** No polity inherits a fleet, which means the first ocean-capable power in a
world built its fleet inside this phase and the sweep can see when.

**Ben, 2026-09-11:** *"Spending capital on ports within the Exploration round enables cheaper
overseas skirmishes. So building a navy and standing army that persists is important — these
degrade over time if insufficient capital is provided."*

Three objects, and each is a stock the treasury maintains:

| Stock | Built from | What it buys | If underfunded |
|---|---|---|---|
| **Port** | Capital, on a region with a port window | Lowers the cost of every crossing staged from it | Silts: its discount decays toward nothing |
| **Navy** | Capital, staged from a port | Crossing capacity, and contest of a crossing | Decays; a fleet is a running cost, not a purchase |
| **Standing army** | Capital, on top of inherited `army_stock` | Force that is *already raised* when a skirmish opens | Falls back toward what muster alone provides |

**Decay is what makes the choice a choice.** A purchase the treasury never revisits is a one-time
score; a stock with upkeep means a polity that over-builds is poorer every round afterwards and a
polity that under-builds arrives late. That is the pressure the phase wants, and it is a cost in
the world rather than a handicap in the scorer.

**`port_q` is endowment, not a port.** The region field the Empire phase carries says the ground
*could* take a harbour. Building one is an act of this phase, paid for, and the two must not be
conflated — otherwise every coastal polity begins with the thing the phase is about acquiring.

**The port is why the skirmish is cheap and the home war is not.** A neighbour war is fought where
treaties bind and no port discount applies. A frontier skirmish is staged from a port the polity
paid for, against ground whose defender has no fleet. That price gap *is* the displacement of
conflict, expressed in the only currency the phase has.

---

## A good acquires a cultural preference

**Ben, 2026-09-11: Globalisation "gives us cultural preference to goods."** A good is not equally
wanted everywhere, and which people wants which good is the difference between a trade map and a
distance map.

**Preference attaches to a CULTURE, not to a polity.** Cultures already cross polity borders as
shares on every region (`CIVILISATION.md`), so a preference carried by culture spreads with people
and survives conquest — which is what makes a market's demand a fact about who lives there rather
than about who rules there.

**It is derived, never rolled** (the standing rule: consequences, not dice). A culture's
preference for a good follows from what its ground never held and what its route exposed it to —
the same two inputs the want table already uses, read at culture grain instead of polity grain. A
people that walked past a good and never held it is the people that pays most for it.

**What the preference does in this phase is weight a want, not set a price.** The want table
(`CIVILISATION.md` § The directed want) is directed and unpriced; preference makes some directed
wants *stronger* than others, which is enough to rank where a fleet goes first. Turning a weight
into a price is Digitisation's job.

---

## The engine is shared

**SETTLED (Ben, 2026-09-11): "let's use the same engine — if needs be we can revise this later."**
Globalisation runs on `history_sim`, over the same region table, the same polity table and the
same round loop. It is not a second simulator.

**Two things the shared engine does not already have, and both are honest additions rather than
forks.** A **round-level upkeep step** — the treasury earns, then pays its stocks, then invests —
because every mechanism above depends on a recurring bill. And **objects with a term** — treaties
expire, which nothing in the Empire phase does.

**The verbs are the Empire verbs plus what a treasury makes possible.** Settle, Invest,
Consolidate and Campaign carry over unchanged. What this phase adds is the ability to spend on a
port, a hull, or a binding — and the scorer weighs those against the old four on one shape
(`trees/TREES.md` § The scorer).

**If the shared engine turns out to be the wrong frame, the tell will be a reading it cannot
move** (§ What the phase is judged on). Revising it then is Ben's call and is explicitly left
open; forking it pre-emptively is not.

---

## The tree

**The Globalisation tree is the third of four** (`trees/TREES.md`), and its existence is Ben's
ruling of 2026-09-11: *"Globalisation and industry use different trees."* The chain is
Colonisation → Empire → **Globalisation** → Industry.

**The Empire tree's rim milestone — *The Enforceable Promise* — opens THIS tree**, not the
Industry tree, and the Industry tree's root is gated behind Globalisation's own rim. That is the
whole structural consequence of the ruling, and it is why a 1960 spread is now manufactured across
two intervening trees rather than one.

**Sized to 460 years**, which is roughly a quarter of the Empire phase's span, so the tree is
correspondingly shallower — three rings, four branches, three milestones (`trees/TREES.md`
§ Sizes). Its concerns are the phase's: hulls and crossings, the charter and the treasury,
the port and the garrison, and what a people does with a good it did not grow.

Its nodes, its forks and its scorer terms are `trees/GLOBALISATION_TREE.md`'s.

---

## What the phase is judged on

Readings taken at **1660 CE**, all over a **seed spread**, never per world — the same
distributional discipline `CIVILISATION.md` § What the closure is judged on applies to the
Empire handoff.

| Reading | What it must show |
|---|---|
| **Displacement** | Neighbour-war rate falls across the span; frontier-skirmish rate rises |
| **Treaty depth** | Treaties standing at 1660 with years still to run, not a churn of one-round bindings |
| **Colonial asymmetry** | Some polities hold subjects, most hold none, and at least one subject sits under a distant overlord |
| **Subject friction** | At least one subject's wants point somewhere its overlord's do not |
| **Fleets** | Navies exist, unevenly, and at least one polity that built one let it decay |
| **Treasury spread** | Wide, and correlated with the inherited corridor network rather than with size alone |
| **Preference** | Goods wanted differently by different cultures, with the difference traceable to route |

**A reading is a requirement, not a target.** A seed that refuses one is a legitimate world; a
*spread* that refuses one is a phase that did not do its job.

---

## What this phase hands digitisation

`DIGITISATION.md` owns what happens next; this is the list, and nothing else crosses.

- **Treasuries**, per polity — the capital that firms will be chartered against.
- **The overlord graph** — who holds whom, and on what tribute terms.
- **Standing treaties and their remaining terms** — the diplomatic position 1660 opens on.
- **Ports, navies and standing armies**, at whatever level upkeep left them.
- **Cultural good preference** — the demand shape a price field will resolve against.
- **The contact and want tables**, grown: a world that has met itself, mostly.
- **Globalisation tree masks** — who enters the Industry tree early and who enters late.

---

## Open questions

- **What earns the treasury, in terms.** § Capital arrives names the four sources; the arithmetic
  is a measurement, and it must be set from a sweep that produces both rich and poor survivors,
  never from a number picked to make one seed solvent.
- **Where the treaty threshold sits** — how much a binding must be worth before both parties
  independently take it. Too low and the map freezes; too high and the displacement never happens.
  Same discipline as the opposition bar (`CIVILISATION.md` § Open questions).
- **Whether a colony can itself hold a colony.** The overlord link permits a chain; whether the
  phase should is unsettled, and the answer decides whether a subject is an actor or a leaf.
- **How preference interacts with assimilation.** A culture absorbed into another carries its
  preferences somewhere; whether they blend, persist or lapse is unowned.
- **Whether the 460-year span is enough rounds** for a tree, a treasury and a fleet to all turn
  over believably. A measurement owed before any constant is tuned.
- **Whether Globalisation should also raise the Digitisation-era world war** (Ben, 2026-09-11:
  on the fence, deferred). If it does, its cause is built here and fires there.
