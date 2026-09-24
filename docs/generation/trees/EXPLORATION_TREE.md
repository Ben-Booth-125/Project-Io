# Project Io — The Exploration Tree

> **Settles:** the nodes of the Exploration tree — the spire that is the purse, its four branches
> and its three milestones · the three forks and what each side trades away · every node's effects
> and the situation in which the scorer buys it · the terms this phase adds to the shared scorer ·
> what a polity's held set hands the Industry tree.
> **Not here:** the grammar every tree obeys — kinds, rings, the five rules, diffusion, the
> scorer's shape, the JSON schema (TREES) · the phase this tree runs in, its treasury, its
> treaties and its subjects (../EXPLORATION) · the phase before it (../CIVILISATION,
> EMPIRE_TREE) · the phase after it (../INDUSTRIALISATION, INDUSTRY_TREE).
> **Confused with:** EMPIRE_TREE.md, INDUSTRY_TREE.md, ../../economy/RESEARCH.md.

The Exploration tree is the third of the four trees `TREES.md` defines, and it runs inside the
exploration age, **1200 → 1660 CE** (`../EXPLORATION.md`). It is **invested**: the Era −1 sim's
polities are the actors, and the Invest verb takes one node a round, chosen by the scorer.

Its root is gated behind the Empire tree's rim milestone, *The Enforceable Promise*; its own rim,
*The Long Reckoning*, opens the Industry tree.

The store is `exploration_tree.json`; this document is the authority and the store transcribes its
tables. `node tools/session/tree_lint.js exploration` holds the two together and enforces the five
rules.

---

## This is a MOCK-UP (Ben, 2026-09-11, resolving NR-843)

**What is designed here is the SHAPE:** which four concerns the phase divides into, where the three
forks sit and what each side gives up, what the milestone chain demands, and the situation in which
the scorer reaches for each node. That shape is a claim and it is meant to be argued with.

**What is NOT designed here is any magnitude.** Every `per_mille` is a placeholder authored by
judgement, exactly as the other three trees' are, and the sweep re-prices them all
(`TREES.md` § Effects and the sim's terms).

**Sizing.** Three rings, four branches and the spire: **31 nodes** of a 64 cap — 18 majors, 10
minors, 3 milestones, 3 forks. `TREES.md` § Sizes gives a target of ~34 for a 460-year phase; 31 is
under it, and where a branch comes in sparse that is the sparse-branch rule doing its job rather
than a hole. The sizing rule's real assertion is the sweep's: the leading polity reaches *The Long
Reckoning* shortly before 1660, and the median stands near *The Standing Charter*.

---

## The spire — The Purse

Material becomes capital, capital acquires an institution that outlives a reign, and the
institution learns to keep accounts. This is the phase's own clock, and it is the whole economic
argument of `../EXPLORATION.md` read as technology.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EX-SP-1a | Consolidated Stores | major | 1 | SP-1m | — | practice | stores +200‰; institution: the treasury stands at the capital | *stores_low* — seats hold what the realm cannot spend as one |
| EX-SP-1m | **The Common Purse** | milestone | 1 | SP-1a · SP-2a | — | — | opens ring 2 | *spire* — requires PT-1a, WY-1a |
| EX-SP-2a | The Chartered Company | major | 2 | SP-1m · SP-2m | — | practice | institution: an entity that outlives its members may hold capital and bind a term; stores +120‰ | *purse_low* — one reign's revenue cannot fund a distant venture |
| EX-SP-2m | **The Standing Charter** | milestone | 2 | SP-2a · SP-3a | — | — | opens ring 3 | *spire* — requires GD-2a, PT-2a, and either side of the Hulls fork |
| EX-SP-3a | Double-Entry Reckoning | major | 3 | SP-2m · SP-3m | — | practice | research +140‰; stores +100‰ | *surplus* — nothing binds; the realm buys knowing what it is owed |
| EX-SP-3m | **The Long Reckoning** | milestone | 3 | SP-3a | — | — | opens the Industry tree | *spire* — requires GD-3a, HL-3a, and either side of the Ways fork |

Links are written without the `EX-` prefix in the tables for width; the store carries them in full,
each edge once on the node farther from the root (the root's own list is empty).

**The spire is the phase's economic argument in three steps.** A realm that can only spend what
each seat holds; a realm with one purse; a realm with an institution that can promise past a reign.
`../EXPLORATION.md` § Capital arrives makes the first step the phase's visible opening act, and the
spire is where it is *earned* rather than granted.

---

## Hulls — what crosses

What can make the passage, how much it carries, and whether it arrives able to fight. The gates are
`coastal` throughout, so a landlocked realm never sees this branch — which is the endowment story
the phase exists to tell, not an exclusion.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EX-HL-1a | Decked Coaster | major | 1 | SP-1a · HL-1b · HL-2c | coastal | artifact | access: a crossing may be staged from a held port; reach +80‰ | *coastal_holdings* — water frontage, and a want across it |
| EX-HL-1b | Sounding & Chart | minor | 1 | HL-1a · GD-1a | — | practice | reach +40‰ | *coastal_holdings* — a known depth is a passage that repeats |
| EX-HL-2c | Careening & Refit | minor | 2 | HL-1a · HL-2a · HL-2b | — | practice | reach +50‰ | *reach_bound* — the shared step before the fleet's kind is chosen |
| **EX-HL-2a** | **Ocean Carrack** | major | 2 | HL-2c · HL-3a · PT-2b | coastal | artifact | reach +200‰; forage +120‰ | *wants_unmet* — burden, not broadside, closes the gap |
| **EX-HL-2b** | **Fleet of the Line** | major | 2 | HL-2c | coastal | artifact | defence +180‰; unlock Ship of the Line | *threatened* — contest the water rather than carry across it |
| EX-HL-3a | Oceanic Navigation | major | 3 | HL-2a · GD-3b | coastal | practice | access: a crossing to unmet ground no longer needs an adjacent shore; reach +260‰ | *wants_unmet* — every reachable want is met already |

**THE FORK: cargo or guns.** *Ocean Carrack* and *Fleet of the Line* exclude each other, and the
closed side goes dark permanently (`TREES.md` § Diffusion follows kind).

This is the fork that matters most, because **only the carrack leads anywhere**. *Oceanic
Navigation* hangs off HL-2a, so a realm that chose the fighting fleet holds a coast nobody crosses
and never reaches ring 3 of this branch. That is deliberate: a realm may buy security on the water
and pay for it in reach, and two equally wealthy realms end the phase incapable of the same things
because they *chose* differently — which is the property `../EXPLORATION.md` § The shape of the age
depends on.

---

## Port & Garrison — what stages, and what is seen

The harbour a crossing launches from, and the standing force a neighbour reads. This branch is
where `../EXPLORATION.md` § The arms race lives: capability that is *visible* is what buys quiet at
home while the same purchase buys opportunity abroad.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EX-PT-1a | Harbour Works | major | 1 | SP-1a · PT-1b · PT-2a | coastal | capacity | unlock Port; stores +90‰ | *ground_port* — a port window with nothing built on it |
| EX-PT-1b | Pilot & Lighthouse | minor | 1 | PT-1a · HL-1a | — | practice | reach +40‰ | *ground_port* — a harbour a stranger can enter at night |
| EX-PT-2a | Standing Garrison | major | 2 | PT-1a · PT-2b · PT-3c | — | practice | defence +150‰; muster_cost −80‰; doctrine: force already raised, visible to neighbours | *threatened* — a raised force makes a treaty cheaper than a test |
| EX-PT-2b | Naval Stores | minor | 2 | PT-2a · HL-2a | — | practice | stores +60‰ | *stores_low* — the difference between a fleet that sails and one that is listed |
| EX-PT-3c | Dry Dock | minor | 3 | PT-2a · PT-3a · PT-3b | — | practice | defence +50‰ | *ground_port* — the shared step before the coast's posture is chosen |
| **EX-PT-3a** | **Blue-Water Squadron** | major | 3 | PT-3c · WY-3d | coastal | practice | reach +220‰; doctrine: force projected to ground the realm does not hold | *subject_held* — something across water must be reachable faster than a rival can take it |
| **EX-PT-3b** | **Fortified Roadstead** | major | 3 | PT-3c | coastal | capacity | defence +300‰; unlock Sea Fort | *threatened* — better no hull enters our water than own hulls that leave it |

**THE FORK: project or deny.** The squadron reaches outward and is the coloniser's rung; the
roadstead is the consolidator's, and it is the stronger *defensive* node by a wide margin. Both are
legitimate ends of the branch, which is what stops the phase having one winning shape.

**EX-PT-2a is the node the deterrence pair reads.** `../../economy/ERAS.md` § The two scalars gives
the Ceiling/Alarm discipline — capability raises a neighbour's alarm by being *seen*, never by being
used — and a standing garrison is the first thing in this tree that is visible at rest.

---

## Ways — what carries, and how much

The road ladder's third rung, and the throughput that runs on it. This is the branch a
**consolidator** cannot avoid, and the one that pays without anybody crossing water.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EX-WY-1a | Post Roads | major | 1 | SP-1a · WY-2a | — | capacity | reach +150‰; upgrade: Road becomes Highway on a corridor that carries | *reach_bound* — corridors worn past what a Road answers |
| EX-WY-2a | Bonded Warehouse | major | 2 | WY-1a · GD-2b · WY-3c | — | practice | stores +180‰; institution: goods may stand in transit without a holder | *throughput_bound* — the corridor carries more than the far end absorbs |
| EX-WY-3c | Wayhouse Relay | minor | 3 | WY-2a · WY-3a · WY-3b | — | practice | reach +60‰ | *throughput_bound* — the shared step before bulk chooses land or water |
| **EX-WY-3a** | **Trunk Highway** | major | 3 | WY-3c · WY-3d | — | capacity | reach +280‰; muster_cost −60‰ | *reach_bound* — a continental realm whose constraint is interior distance |
| **EX-WY-3b** | **Canal Cut** | major | 3 | WY-3c | arable | capacity | stores +300‰; carrying_capacity +90‰ | *throughput_bound* — bulk is the constraint, not speed |
| EX-WY-3d | Toll & Escort | minor | 3 | WY-3a · PT-3a | — | practice | stores +70‰ | *throughput_bound* — a route that pays for its own protection |

**THE FORK: pace or tonnage.** The highway serves trade *and* the march — it is the only node in
the tree that discounts muster — while the canal moves far more and moves it slowly, and is gated
on ground that permits the cut. A realm's terrain therefore argues for one side before its
disposition does.

**EX-WY-1a is Ben's "upgrade of roads" (2026-09-11).** The Empire sim promotes a corridor by use —
Track at four uses, Road above it, nothing beyond. This is the rung beyond, and it is **bought with
capital** rather than earned by traffic, which is what makes a treasury matter to a realm that never
sails.

---

## Goods — what a people does with what it did not grow

Naming a good, counting it, and wanting it. This branch produces the **scarcity signal**
(`../EXPLORATION.md` § There is no price here) and the durable **cultural preference** — the two
things Industrialisation is waiting for.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EX-GD-1a | Named Staples | major | 1 | SP-1a · GD-1b · HL-1b · GD-2a | — | practice | intel: goods are named and countable rather than an undifferentiated store; stores +70‰ | *wants_unmet* — a shortage with no name has no direction |
| EX-GD-1b | Weights & Tally | minor | 1 | GD-1a · WY-1a | — | practice | stores +50‰ | *stores_low* — a measure both ends of a road agree on |
| EX-GD-2a | Quayside Market | major | 2 | GD-1a · GD-2b · GD-3a | — | practice | intel: a market's scarcity signal is legible to a stranger; stores +110‰ | *wants_unmet* — legibility converts a want into an arrival |
| EX-GD-2b | Bill of Lading | minor | 2 | GD-2a · WY-2a | — | practice | stores +60‰ | *throughput_bound* — a cargo owned while it is still moving |
| EX-GD-3a | Standing Preference | major | 3 | GD-2a · GD-3b | — | practice | intel: a people's preference for a good is durable and readable across the contact graph; stores +140‰ | *surplus* — knowing what others will keep paying for |
| EX-GD-3b | Factor's Ledger | minor | 3 | GD-3a · HL-3a | — | practice | research +70‰ | *known* — an agent kept at a foreign market |

**No fork here, deliberately.** Naming, counting and wanting are not alternatives; a realm that
skips them is simply behind. The branch is the phase's *information* wedge, and information is the
one thing no strategy trades away.

---

## The three milestones

| id | milestone | ring | the capability regime beyond it | requires |
|---|---|---|---|---|
| EX-SP-1m | The Common Purse | 1 | A realm spends as one rather than as a set of provisioned cities | Harbour Works (PT), Post Roads (WY) |
| EX-SP-2m | The Standing Charter | 2 | A promise can run for years, and an entity can hold capital past a reign | Quayside Market (GD), Standing Garrison (PT), **and either side of the Hulls fork** |
| EX-SP-3m | The Long Reckoning | 3 | Capital that is accounted and mobile | Standing Preference (GD), Oceanic Navigation (HL), **and either side of the Ways fork** |

Each requires majors from at least two branches at its own ring (rule 4), so no polity climbs the
spire by running one wedge to the rim. The rim milestone is what the Industry tree's root requires.

**Two milestones use `requires_fork` rather than a named major**, which is the softer AND
`TREES.md` § Milestones defines. That matters here more than in the Empire tree: this phase's forks
are *strategy* forks, and a milestone that named one side would quietly make the other side a
dead end.

---

## The scorer

**One shape** (`TREES.md` § The scorer — one shape, four trees): a frontier node's score is the sum
of its effects, each weighted by how hard that term binds the polity *now*, plus ground pull, plus
spire pull, plus a discount for a neighbour already holding it, minus cost.

**Four terms this phase adds**, because none of the Empire tree's terms can express them:

| Term | What it reads |
|---|---|
| `purse_low` | The capital treasury is short of what the realm's open ventures need |
| `wants_unmet` | Directed wants this polity holds that its reachable ground cannot satisfy |
| `throughput_bound` | Corridors carrying at their ceiling — flow refused, not reach refused |
| `subject_held` | The polity holds ground under an overlord link, across water |

**`throughput_bound` is the one that makes the consolidator legible.** Every other pressure term in
the project reads *can I get there*; this one reads *how much arrives*, and it is the only term a
realm with no coast and no subjects can ever be bound by. Without it the scorer would have nothing
to say to a continental power, and the tree's two-strategy claim would be decoration.

---

## What the tree hands the Industry tree

- **Entry timing.** A polity holding *The Long Reckoning* at 1660 starts the Industry tree at its
  root when Industrialisation opens; one that does not starts once it holds the rim. This is the second
  of the two inputs to the 1960 spread, the first being Empire's rim.
- **The charter.** *The Chartered Company* held is the institutional precondition for a firm — the
  Industry tree's Works Doctrine and General Incorporation decide what may be chartered, and this
  decides whether anything can be.
- **The network.** Which fork of Ways a realm took, and whether *Post Roads* ever fired, is the
  road ladder Industrialisation inherits.
- **The signal.** *Quayside Market* and *Standing Preference* held are what make a realm's scarcity
  signals and its peoples' preferences legible at 1660 — the inputs a price field resolves against.
- **The water.** The Hulls fork decides whether the realm arrives at 1660 with ocean reach or with
  a defended coast, which is the difference between a colonial and a continental industrialisation.

---

## Open questions

- **Whether 31 nodes is enough for 460 years**, or whether the branches want a fourth ring. The
  sizing rule is the sweep's to assert and the sweep does not exist yet (`NR-844`).
- **Whether the Hulls fork is too punishing.** Only the carrack leads to ring 3, which is a strong
  claim; the alternative is a second ring-3 node behind *Fleet of the Line* so the fighting fork
  ends somewhere of its own.
- **Whether `subject_held` belongs in the scorer at all.** It reads a relation rather than a
  pressure, and every other term reads a pressure.
- **How the four new terms are measured.** Each needs a definition against real sim state before
  any node's `pursued_when` can fire, and three of the four read quantities
  (`../EXPLORATION.md`'s treasury, scarcity signals and corridor throughput) that are themselves
  unbuilt.
