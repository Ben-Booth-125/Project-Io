# Project Io — Logistics

**The network.** How far anything is from anything else, what it costs to cross, how long it takes,
and what the network permits. This document owns the **substrate**; `SUPPLY.md` owns the **flow that
runs on it** (convoys).

> The split, stated once so it stops being ambiguous: **LOGISTICS is the road; SUPPLY is the
> traffic.** A convoy's existence, cargo, dispatch and arrival are SUPPLY's. The cost it pays per
> tile, the path it takes, how long that takes, and whether the leg is admissible at all are this
> document's.

---

## The one ruling everything else hangs off

> **There is one reach field. Economic reach IS military reach.**
> — Ben, 2026-08-08, BL-325 ruling 3: *"a nation's reach for economy is also the military reach."*

A parallel base-anchored supply envelope for the military was proposed and **overridden**.
`body_reach_field` — the economic logistics network — *is* the military supply envelope.

Two consequences that shape every design downstream:

- **A `military_base` is not a supply anchor and extends nothing.** Which is why it earns no
  exemption from the reach rule governing its own placement.
- **To project force further, you extend the same road and hub network everyone else uses.** There
  is one distance metric in the game and armies pay it.

This is why logistics is load-bearing rather than plumbing: it is the single system both pillars —
Trade and Conflict — are rate-limited by.

---

## Where this sits in the chain

Io's systems are meant to chain, so that **each system's ceiling is the next system's door**
(SYSTEMS.md § The progression chain). Logistics is the second rung and the busiest junction on it:

    extraction → LOGISTICS → markets → force → territory
                    ↑
       you outgrow your first tile, and distance becomes the problem

**What forces you in:** a building must be within reach of a supply anchor, so the richest tile on
the map is not automatically sitable. Placement stops being a lookup.

**What it opens:** distant markets (arbitrage is *source price + haulage < destination price*),
distant deposits, and — under the one-network ruling — the ability to put an army anywhere at all.

**What it caps you at:** reach says *can this be reached*; **Logistic Points** say *how much can
move*. Reach is the door; throughput is the ceiling that makes the chain continue rather than
plateau.

---

## The network

### 1. Traversal cost — one weight function, shared by everything

`tile_traversal_cost` is the per-node weight used by **A\*, the reach-field Dijkstra, and a marching
unit alike.** It is a named, public function rather than an anonymous-namespace helper precisely so
`run_unit_march` spends march points against *the same cost function* rather than a second invented
model.

Landform multipliers, from TILES.md: plains 1.0, valley 1.1, highland 1.25, crater 1.3, canyon 1.5,
rift 1.6, mountain 2.0. Water is crossable at a higher **sea-leg** cost, so a path exists on any
connected body — and **whether the cheapest path touches water is what selects sea vs land mode**.

Roads discount it: `road_traversal_multiplier` = `1 / (1 + 0.5 × tier)`.

An **edge** cost is the mean of its two nodes, which is what makes a path symmetric.

### 2. Pathfinding — `intra_body_path`

Terrain-weighted A\* over a body's tile grid, respecting the **east-west cylinder wrap**. Grid
topology matches `nation_generation.cpp`: 4-cardinal neighbours, raster index
`grid_y × grid_width + grid_x`. The core pathing design is BL-077 (intra-body pathfinding).

Results cache on `world.astar_cost_cache` under a **canonicalised endpoint key**, so the per-tick
dispatch loop pays each search once.

> **A trap worth carrying forward.** Because the cache key is canonicalised, a caller reading a
> cached path must apply its own orientation. `body_surface_canvas.cpp` copies and conditionally
> reverses it. Get the orientation wrong and a convoy's head lands at the wrong end of the lane half
> the time — **invisible on screen, fatal to interdiction.**

### 3. Reach — the placement constraint

**The problem it solves:** without a distance rule, a corp can site a building arbitrarily far from
anything at no cost and with no refusal, which makes optimal siting *"the richest tile anywhere"* —
**a lookup rather than a decision, and the first thing an AI on the command seam finds.** Reach as a
placement constraint is BL-323 (buildings rework) S2.

`body_reach_field` is one multi-source Dijkstra from every **supply anchor**, giving each tile its
weighted cost to the nearest one. Infinity where none is reachable.

**An anchor is a city, or a built and active port or inland logistics hub.** These are exactly the
logistics nodes of BL-148/BL-149 (logistics nodes) — the places a convoy can already start cheaply —
*"so reach inherits that vocabulary rather than introducing a rival one."*

**The first-anchor bootstrap:** on a body with no anchor at all, an anchor-type placement skips the
reach rule; committing that first anchor immediately makes the body anchored, so the exemption
cannot be used twice while the first hub is still building.

`tile_reach_cost` is the const read: **−1 means "not computed", infinity means "computed and
unreachable"** — a distinction a UI holding a `const world&` needs, because it must not trigger the
Dijkstra itself.

### 4. Roads — generated, then extended by hand

Generated per nation after population, deterministically from the campaign seed (BL-146 /
BL-172, road generation). Five passes: local streets (every centre's own tile gets at least a
Track), a weighted graph over centre pairs, a **Kruskal MST backbone** plus relative-neighbour
redundancy edges for realistic loops, a three-tier assignment, and rasterisation along each edge's
A\* path taking the **max** `road_level` on overlap.

**Three tiers** (Ben, 2026-07-11): **Highway** (3) between two major centres, **Road** (2) when at
least one endpoint is Town+, **Track** (1) otherwise. Then one Track border link between the nearest
centre pair of each territorially-adjacent nation pair, so the lattice connects across the continent.

**Roads are a land feature.** Water tiles are skipped, and an edge whose route crosses *open* ocean
is not stamped at all — that is a sea route, and stamping it would scatter fragments on distant
shores. A short crossing made of shore (a strait, TILES.md § Water kinds) does get a road.
Territorial adjacency tolerates a short unowned gap, so an island or coastal nation is reachable
rather than silently left off the lattice.

The player extends the lattice with `place_road`; rivals do too, through the same verb.

**Roads do not decay** (Ben, 2026-08-22): *"Roads do not decay, but nations have to pay tax to
support them. If a nation runs into too much debt supporting infrastructure, it can go bankrupt
with major penalties. But between these states nothing changes."* The cost is **binary, not
graduated**: a solvent nation's roads behave exactly as built; a bankrupt one suffers major
penalties; there is no middle band where a strained network degrades. A graduated version would
be a second decay model wearing a budget's clothes, which the first half of the ruling rejects.
This is the sink the *logistics maintenance* line of the national budget (BL-538, national budget)
pays into, and it gives a nation its first failure state — BL-550 (national insolvency).

### 5. Physical scale and travel time (Ben, 2026-08-12)

**Scale is derived, not authored.** Planetology generates `home_mass`; a rocky planet's radius
follows roughly `R ∝ M^0.27`, so tile width falls out of a scalar the generation chain has already
settled. At Earth mass on the 312-column grid that is **~128 km per tile** — which puts a day's
march at about a fifth of a tile and makes a tile **a region-sized unit rather than a field.**

Without a tile scale, convoy speed would be an *interplanetary* calibration (`1 / distance_in_AU`)
and every intra-body convoy would arrive in one econ tick whether it crossed one tile or all 312 —
distance would cost money and never cost time, and tripling the map could not make distance feel
bigger.

**Two speeds, and the gap between them is a design lever:** caravan **25 km/day**, coastal vessel
**130 km/day**. Roughly five times, *"and that difference is the whole reason coastal trade is worth
designing"* — BL-188 (coastal ports) owns the sea-trade design that reaches the faster speed.

**Terrain cost doubles as a time multiplier** — the A\* weights do double duty rather than needing a
parallel table. Travel is quantised to whole econ ticks (minimum 1), because the economy resolves
quarterly.

### 6. Cache invalidation — narrowed, for a real reason

`invalidate_logistics_caches` clears both caches together. An over-clear costs one Dijkstra; **a
missed clear is a reach field that lies.**

Ben's 2026-08-08 ruling chose a simple every-event rule because *"each of these is rare against the
per-frame reads."* That premise fails in a world where the corp AI builds every tick and hundreds of
generated sites complete through the warm start: the caches clear every econ tick, and the rebuilds
— per-pair Dijkstras over 45,240 tiles plus the reach field — *become* the tick.

So sim-rate call sites gate on `building_affects_logistics`: **only a port or inland hub can change
the anchor set**, and no building type changes traversal cost (that is `road_level`, and
`place_road` clears unconditionally). Player-rate UI sites keep the unconditional clear —
over-clearing at click rate is free.

### 7. Interdiction — the network can be cut

A hostile unit standing on a convoy's tile **intercepts it** (BL-458, interdiction). The check is
deliberately narrow and one-directional: the tile's holder must have **declared** hostility toward
the cargo's owner — *"a corp that has been declared against but has not answered is a victim, not a
raider."* Your own escort is never your ambusher. The lowest-id hostile unit on a contested tile is
the interceptor, so the outcome is order-independent.

**A friend is never an interceptor** (Ben, 2026-08-22, design register — friendship permits
*immunity from interdiction*, BL-549 (friendship permits two things)). The check is safe rather than
contradictory because `declare_hostile` **dissolves a friendship row atomically**, so the two states
cannot both hold: the friendship test is an early-out on a pair hostility has already excluded, not
a competing predicate.

**A rival's hostility declaration is signalled** to the player (Ben, 2026-08-22, overturning
NR-350's discovered-on-contact rule). Interdiction is therefore **a known risk rather than a
surprise** — the ambush property `stance.hpp`'s directed hostility exists for still holds between
rivals, but the player's first lost convoy is never the player's first news.

**Capture, with destruction as the fallback** (Ben, 2026-08-17). Cargo leaves the source pool at
dispatch, so the goods are already committed and either answer conserves. On interception the cargo
credits the interceptor's pool at the interception tile's body; if no pool is reachable there it is
destroyed instead, and the outcome says which. **An interceptor holding goods it cannot sell is a
legitimate outcome that is not special-cased away.**

*Why capture rather than destroy:* destroy-only gives an interception a payoff of zero, so a
scored-utility rival would correctly never rank it — interdiction would only ever fire when the
player did it. **Capture gives the scorer a number.**

It also earns BL-315's (armed house conflict spine) third name. Army, mercenary and pirate are three
*derived* readings of one company; taking cargo is what makes "pirate" the honest one.

**An interception is announced in the same change that resolves it.** A comms message names the
lane and the interceptor, the convoy's row leaves the Convoys ledger (BL-453, convoys ledger) with
a stated cause, and the tile is marked for a few ticks. An interception is the most consequential
thing that can happen to a player's economy without them pressing anything, so silence is the
wrong default (NR-407).

**A convoy is cargo and cannot be defended.** Assigning a unit to a convoy — turning interception
into a real battle — is named in BL-458's design and deliberately excluded from it.

### 8. Inter-body distance

Space distance is Euclidean body-centre to body-centre; there are no orbital mechanics in the
prototype, by design (`TECH_FOUNDATIONS.md` § Prototype scope).

---

## Logistic Points

BL-596 (LP active march) and BL-597 (LP passive convoys) own the build, BL-606 (throughput lens)
the surface — all 2026-08-24, replacing the purged BL-464 umbrella. Ben, 2026-08-18: *"Let's begin codifying Logistic Points
in this sprint. It's an important layer for military and goods transport."* The reasoning below has
survived two rulings and two rejected first cuts, and is recorded so the next cut starts from it.

### What it is

**A per-tick RATE, not a stock** (Ben, ruling on NR-343, 2026-08-20). Regenerated throughput, used
or wasted each tick, never banked; **what carries over is the goods it moved, never the points.**
LP is the pipe; the tanks are separate.

**And it is BIFOLD** (Ben, 2026-08-22, design register):

> *"Let's go with city generation, but only because cities have to affect automatic trading. LP
> should be bifold then, passive and active. Militaries can only use active LP, and what's more, it
> should cost actual money to resolve LP usage."*

| | **Passive LP** | **Active LP** |
|---|---|---|
| Serves | automatic trading — the convoy layer that runs itself | movement a player or rival **directs** |
| Drawn by | the market's own flow | **militaries draw active only** |
| Owned? | ambient — the network's | **owned**, and not a rival's to use |

**Cities generate it.** That answers two constraints at once: a node-generated rate is real from
turn one because cities are generated in the hundreds, where no corp is seeded a hub; and a city is
a spatial locus — LP is *"how much can move through HERE"*, never a per-corp haul allowance.

**The split is what stops free-riding.** Ben, on whether rivals should build hubs: *"the passive /
active split explains how we would be unable to use rival active LP."* Ambient throughput serves
everyone; directed throughput is yours. It also dissolves the asymmetry of a rival paying LP it
cannot generate, since the half that serves trade is not owned by anybody.

### Active use costs credits — and that is not a reversal of "a cap, not a price"

> *"It should cost actual money to resolve LP usage. This can be in the form of moving units, or
> supplying items."*

The cap/price split holds. What changes is **which side of the game pays**. A convoy pays credits
per unit-distance. A marching unit spends march points against traversal cost, and **that march is
priced in credits by active LP** — so moving an army is not free, which was never a decision anyone
made.

So: **passive LP caps trade, which is already priced; active LP caps force, which this ruling
prices.** Read that way it is a widening of the existing rule rather than an exception to it. The
credit cost of active resolution is argued against BL-543's (value anchor) unit-cost anchor rather
than guessed.

Three consequences, all in Io's favour: a rate crosses no tick boundary, so it needs no `state_hash`
entry and no serialisation; a stock that accumulates while never binding is the `military_points`
failure with extra steps; and it removes the question of where LP lives between ticks, because it
does not live between ticks.

### Three rules that are settled

**1. LP is a CAP, not a price.** The convoy already charges distance in credits. Adding LP as a
second price would double-charge distance and move every economy golden. **Credits stay the price;
LP decides whether the leg is admissible at all.** A draw measured in *distance* breaks this rule
however it is dressed up — see constraint 3 for the settled formula, and NR-620 for what it cost
to learn: a distance draw cut real convoy traffic by 73% while every golden held steady.

**2. Adopt the node half; refuse the link half.** The reference system runs two currencies —
throughput from nodes, and distance from links. **Io already owns the link half, twice over:**
`road_traversal_multiplier` is distance-per-tile under another name, and `body_reach_field` is the
free-distance envelope expressed as a cost field. Importing a second parallel distance budget is
what BL-325 ruling 3 forbids outright.

**3. It lands with its consumer, in the same batch.** Non-negotiable. `military_points` was deleted
for being a write-only accumulator and five resources were deleted for the same reason. **A
Logistic Point generated and never spent — or spent and never felt — is the identical defect with a
new name.**

### Seven constraints a cut must satisfy

Each rejected an earlier cut. Two are structural.

1. **Do not promise an inertness proof the sequencing cannot deliver.** Two-pass allocation
   reorders float subtractions on a hashed field, so a "rate-zero, byte-identical" commit cannot
   pass *at any rate*. The pattern BL-409 and BL-454 used does not transfer, and pretending it does
   is how a golden gets blessed dishonestly.
2. **LP must have a spatial locus.** A rate justified as *"how much can move through HERE"* and
   then pooled per `(corp, body)` is a per-corp haul allowance — **the exact abstraction
   `military_points` was deleted for.** Cities are the locus.
3. **Specify the LP cost formula before the allocation sort key**, which is a function of it. If
   cost is proportional to distance, LP *is* haulage cost again; if flat, the sort degenerates.
   **Settled (Ben, 2026-08-25): the draw is what MOVES, not how far.** A passive convoy draw is
   its **cargo quantity**; an active march draw is its **march points**. One rate serves both, so
   a unit of goods and a march-point's worth of movement burden an anchor equally — the implicit
   exchange rate, and a first cut. Distance stays priced in credits and only in credits.
4. **The base allowance must not be the whole mechanic.** No corp is seeded a hub, so "your own
   nodes decide how much you may move" is fictional; city generation is what makes the rate real.
5. **A rival must be able to build the generator.** `corp_ai.cpp`'s scorer carries `place_road`
   and a port / inland-hub build candidate (Ben, 2026-08-22: yes, and before LP lands; BL-599
   (rival roads and hubs) is the scorer's road/anchor item), so the rate is not an asymmetric tax
   on rivals.
6. **No reserved military share.** Two budgets means guns and butter stop competing, which is the
   whole point of one rate.
7. **No one-way ratchet.** A `supply_decay_permille` with zero recovery is a stock, not a rate.

### The finding worth keeping above all the others

> **Goods-vs-force priority is otherwise decided invisibly, by tick phase order.** Convoys claim
> before armies on every code path. So *"the army goes unsupplied"* is an **inherited default nobody
> chose** — not a design.

LP is what makes that priority explicit, and it is the strongest argument for it.

### Refusal, surface and determinism (Ben, 2026-08-22, design register)

- **A leg over the cap fails — refused outright, and the player is told why.** A refused leg is
  legible; a queued one is not. The cost is that LP reads as a wall, which is the honest trade.
  **Surfacing is non-optional** — a refusal nobody sees is silent interdiction again.
- **Throughput is a lens**, extending Reach. The Reach lens shows a binary field; throughput is
  that field with a magnitude, so it is a small step from an existing surface rather than a new one.
- **Allocation under contention is order-independent** — a deterministic priority rule over a
  sorted set, never first-come by iteration order.
- **Active lands first (Ben, 2026-08-24, the Sprint 18 design form)** — the priced march is the
  first consumer; passive convoy admissibility follows in the same sprint, each half landing with
  its consumer (settled rule 3, applied per half). Rates for both halves are first-cut in the
  landing items, argued against BL-543's (value anchor) unit-cost anchor and flagged for tuning
  rather than ruled ahead (NR-600).

---

## Where the parts live

| Concern | File |
|---|---|
| Traversal cost, A\*, caches, reach field, scale, travel time | `src/world/logistics.{hpp,cpp}` |
| Road generation and tiers | `src/world/road_generation.{hpp,cpp}` |
| Convoy dispatch, cost, arrival, interdiction | `src/world/supply_system.{hpp,cpp}` |
| Placement's reach refusal | `src/world/placement_rules.cpp` |
| A marching unit spending the same cost | `src/world/economy_system.cpp` § `run_unit_march` |
| The Reach lens | `docs/ui/LENSES.md` |

**Related authorities.** [`SUPPLY.md`](SUPPLY.md) (convoys — the flow on this network),
[`../military/MILITARY.md`](../military/MILITARY.md) (BL-325 ruling 3 in its military reading),
[`TILES.md`](TILES.md) (the landform multipliers), [`../politics/RELATIONS.md`](../politics/RELATIONS.md)
(stance and friendship, which are interdiction's two predicates), [`PRODUCTION.md`](PRODUCTION.md)
(§ Logistics and transport capacity, which this document supersedes).

**Owning items.** BL-596 / BL-597 (LP active march / passive convoys) — throughput; BL-606
(throughput lens) — its lens. BL-458 (interdiction) — the cut network.
BL-608 (sea port gate) — the sea endpoint gate (BL-188's archived coastal-trade prose is
reference). BL-452 (convoy verbs) and BL-453 (convoys ledger) — the
player-facing halves. BL-323 (buildings rework) — reach. BL-146 / BL-172 (road generation) — roads.
BL-077 (intra-body pathfinding) — the core. BL-550 (national insolvency) — what the network costs.
