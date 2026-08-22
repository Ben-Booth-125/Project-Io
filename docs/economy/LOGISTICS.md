# Project Io — Logistics

**The network.** How far anything is from anything else, what it costs to cross, how long it takes,
and what the network permits. This document owns the **substrate**; `SUPPLY.md` owns the **flow that
runs on it** (convoys).

> The split, stated once so it stops being ambiguous: **LOGISTICS is the road; SUPPLY is the
> traffic.** A convoy's existence, cargo, dispatch and arrival are SUPPLY's. The cost it pays per
> tile, the path it takes, how long that takes, and whether the leg is admissible at all are this
> document's.

> **Status: written 2026-08-22 as capture, not design.** § Build status is transcribed from the
> code. § Logistic Points is a *designed and unbuilt* system whose rulings are settled — it is here
> because BL-464 has been ruled on twice and had two first cuts rejected, and that reasoning was
> reachable only by reading a backlog item. § Open questions are calls nobody has made.

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

**What it caps you at, and this is the important one:** reach is currently binary. The network says
*can this be reached*, never *how much can move*. **Logistic Points is the designed answer to that
ceiling**, and it is the rung that would make the chain continue rather than plateau.

---

## Build status

### 1. Traversal cost — one weight function, shared by everything

`tile_traversal_cost` is the per-node weight used by **A\*, the reach-field Dijkstra, and a marching
unit alike.** It was deliberately promoted out of an anonymous namespace so `run_unit_march` spends
march points against *the same cost function* rather than a second invented model.

Landform multipliers, from TILES.md: plains 1.0, valley 1.1, highland 1.25, crater 1.3, canyon 1.5,
rift 1.6, mountain 2.0. Ocean is crossable at a higher **sea-leg** cost, so a path exists on any
connected body — and **whether the cheapest path touches ocean is what selects sea vs land mode**.

Roads discount it: `road_traversal_multiplier` = `1 / (1 + 0.5 × tier)`.

An **edge** cost is the mean of its two nodes, which is what makes a path symmetric.

### 2. Pathfinding — `intra_body_path` (BL-077)

Terrain-weighted A\* over a body's tile grid, respecting the **east-west cylinder wrap**. Grid
topology matches `nation_generation.cpp`: 4-cardinal neighbours, raster index
`grid_y × grid_width + grid_x`.

Results cache on `world.astar_cost_cache` under a **canonicalised endpoint key**, so the per-tick
dispatch loop pays each search once.

> **A trap worth carrying forward.** Because the cache key is canonicalised, a caller reading a
> cached path must apply its own orientation. `body_surface_canvas.cpp` copies and conditionally
> reverses it. BL-458's design flagged this precisely: get the orientation wrong and a convoy's head
> lands at the wrong end of the lane half the time — **invisible on screen, fatal to interdiction.**

### 3. Reach — the placement constraint (BL-323 S2)

**The problem it solves:** before reach, placement had no distance rule of any kind. A corp could
site a building arbitrarily far from anything at no cost and with no refusal, which makes optimal
siting *"the richest tile anywhere"* — **a lookup rather than a decision, and the first thing an AI
on the command seam finds.**

`body_reach_field` is one multi-source Dijkstra from every **supply anchor**, giving each tile its
weighted cost to the nearest one. Infinity where none is reachable.

**An anchor is a city, or a built and active port or inland logistics hub.** These are exactly
BL-148/149's logistics nodes — the places a convoy can already start cheaply — *"so reach inherits
that vocabulary rather than introducing a rival one."*

**The first-anchor bootstrap:** on a body with no anchor at all, an anchor-type placement skips the
reach rule; committing that first anchor immediately makes the body anchored, so the exemption
cannot be used twice while the first hub is still building.

`tile_reach_cost` is the const read: **−1 means "not computed", infinity means "computed and
unreachable"** — a distinction a UI holding a `const world&` needs, because it must not trigger the
Dijkstra itself.

### 4. Roads — generated, then extended by hand (BL-146, BL-172)

Generated per nation after population, deterministically from the campaign seed. Five passes: local
streets (every centre's own tile gets at least a Track), a weighted graph over centre pairs, a
**Kruskal MST backbone** plus relative-neighbour redundancy edges for realistic loops, a three-tier
assignment, and rasterisation along each edge's A\* path taking the **max** `road_level` on overlap.

**Three tiers** (Ben, 2026-07-11): **Highway** (3) between two major centres, **Road** (2) when at
least one endpoint is Town+, **Track** (1) otherwise. Then one Track border link between the nearest
centre pair of each territorially-adjacent nation pair, so the lattice connects across the continent.

**Roads are a land feature.** Ocean tiles are skipped, and an edge whose route crosses *open* ocean
is not stamped at all — that is a sea route, and stamping it would scatter fragments on distant
shores. Territorial adjacency tolerates a short unowned gap, so an island or coastal nation is
reachable rather than silently left off the lattice.

The player extends the lattice with `place_road`; rivals do too, through the same verb.

### 5. Physical scale and travel time (Ben, 2026-08-12)

**What was missing:** the codebase had no tile scale at all and no intra-body travel time. Convoy
speed was `1 / distance_in_AU` — an *interplanetary* calibration — and two markets on the same body
return distance 0, so speed clamped to 1.0 and **every intra-body convoy arrived in exactly one econ
tick whether it crossed one tile or all 312.** Distance cost money and never cost time.

> That is why tripling the map could not on its own make distance feel bigger: with travel time
> constant, a bigger map means the same 90 days buys three times the reach.

**Scale is derived, not authored.** Planetology already generates `home_mass`; a rocky planet's
radius follows roughly `R ∝ M^0.27`, so tile width falls out of a scalar the generation chain
already settled. At Earth mass on the 312-column grid that is **~128 km per tile** — which puts a
day's march at about a fifth of a tile and makes a tile **a region-sized unit rather than a field.**

**Two speeds, and the gap between them is a design lever:** caravan **25 km/day**, coastal vessel
**130 km/day**. Roughly five times, *"and that difference is the whole reason coastal trade is worth
designing"* (BL-188, parked).

**Terrain cost doubles as a time multiplier** — the A\* weights do double duty rather than needing a
parallel table. Travel is quantised to whole econ ticks (minimum 1), because the economy resolves
quarterly.

### 6. Cache invalidation — narrowed once, for a real reason

`invalidate_logistics_caches` clears both caches together. An over-clear costs one Dijkstra; **a
missed clear is a reach field that lies.**

Ben's 2026-08-08 ruling chose a simple every-event rule because *"each of these is rare against the
per-frame reads."* **The 0 CE world broke that premise:** with the corp AI building every tick and
hundreds of generated sites completing through the warm start, the caches cleared essentially every
econ tick, and the rebuilds — per-pair Dijkstras over 45,240 tiles plus the reach field — *became*
the tick. That was the AppHangB1 stall.

Sim-rate call sites now gate on `building_affects_logistics`: **only a port or inland hub can change
the anchor set**, and no building type changes traversal cost (that is `road_level`, and
`place_road` still clears unconditionally). Player-rate UI sites keep the unconditional clear —
over-clearing at click rate is free.

### 7. Interdiction — the network can be cut (BL-458)

A hostile unit standing on a convoy's tile **intercepts it**. The check is deliberately narrow and
one-directional: the tile's holder must have **declared** hostility toward the cargo's owner —
*"a corp that has been declared against but has not answered is a victim, not a raider."* Your own
escort is never your ambusher.

**Capture, with destruction as the fallback** (Ben, 2026-08-17). Cargo leaves the source pool at
dispatch, so the goods are already committed and either answer conserves. On interception the cargo
credits the interceptor's pool at the interception tile's body; if no pool is reachable there it is
destroyed instead, and the outcome says which. **An interceptor holding goods it cannot sell is a
legitimate outcome that is not special-cased away.**

*Why capture rather than destroy:* destroy-only gives an interception a payoff of zero, so a
scored-utility rival would correctly never rank it — interdiction would ship and only ever fire when
the player did it. **Capture gives the scorer a number.**

It also earns BL-315's third name. Army, mercenary and pirate are three *derived* readings of one
company; taking cargo is what makes "pirate" the honest one.

> **It ships silent (NR-407).** Interdiction works and nothing tells the player it happened. The
> designed surfaces — a comms message naming the lane and the interceptor, the convoy's row leaving
> the Convoys tab with a stated cause, the tile marked for a few ticks — are not built.
> **An interception is the most consequential thing that can happen to a player's economy without
> them pressing anything, so silence is the wrong default.**

---

## Logistic Points — designed, ruled twice, not built

BL-464. Ben, 2026-08-18: *"Let's begin codifying Logistic Points in this sprint. It's an important
layer for military and goods transport."* Recorded here because the reasoning has survived two
rulings and two rejected first cuts, and was reachable only inside a backlog item.

### What it is

**A per-tick RATE, not a stock** (Ben, ruling on NR-343, 2026-08-20). Regenerated throughput, used
or wasted each tick, never banked; **what carries over is the goods it moved, never the points.**
LP is the pipe; the tanks are separate.

Three consequences, all in Io's favour: a rate crosses no tick boundary, so it needs no `state_hash`
entry and no serialisation; a stock that accumulates while never binding is the `military_points`
failure with extra steps; and it removes the question of where LP lives between ticks, because it
does not live between ticks.

### Three rules that are settled

**1. LP is a CAP, not a price.** The convoy already charges distance in credits. Adding LP as a
second price would double-charge distance and move every economy golden. **Credits stay the price;
LP decides whether the leg is admissible at all.**

**2. Adopt the node half; refuse the link half.** The reference system runs two currencies —
throughput from nodes, and distance from links. **Io already owns the link half, twice over:**
`road_traversal_multiplier` is distance-per-tile under another name, and `body_reach_field` is the
free-distance envelope expressed as a cost field. Importing a second parallel distance budget is
what BL-325 ruling 3 forbids outright.

**3. It lands with its consumer, in the same batch.** Non-negotiable. `military_points` was deleted
in 2026-08-17 for being a write-only accumulator and five resources were deleted a day earlier for
the same reason. **A Logistic Point generated and never spent — or spent and never felt — is the
identical defect with a new name.**

### The seven findings that rejected the first cut

Recorded so the next attempt does not re-run into them. Two are structural.

1. **The inertness proof is unachievable as sequenced.** Two-pass allocation reorders float
   subtractions on a hashed field, so a "rate-zero, byte-identical" commit cannot pass *at any
   rate*. The pattern BL-409 and BL-454 used does not transfer, and pretending it does is how a
   golden gets blessed dishonestly.
2. **LP as drafted had no spatial locus.** Justified as *"how much can move through HERE"* — a node
   — then pooled per `(corp, body)`. That is a per-corp haul allowance, **the exact abstraction
   `military_points` was deleted for.**
3. The LP cost formula is never specified, yet the allocation sort key is a function of it. If cost
   is proportional to distance, LP *is* haulage cost again; if flat, the sort degenerates.
4. **The base allowance would be the whole mechanic** — no corp is seeded a hub, so "your own nodes
   decide how much you may move" is fictional at the shipped generator.
5. **The AI pays LP and cannot build the generator** — `corp_ai.cpp` has no port or hub candidate,
   so the rate would be an asymmetric tax on rivals.
6. **A reserved military share deletes the coupling it exists to create.** Two budgets means guns
   and butter stop competing, which was the whole point.
7. `supply_decay_permille` would ship a one-way ratchet, since recovery is also zero.

### The finding worth keeping above all the others

> **Goods-vs-force priority is currently decided invisibly, by tick phase order.** Convoys claim
> before armies on every code path. So *"the army goes unsupplied"* is an **inherited default nobody
> chose** — not a design.

That is the thing LP would make explicit, and it is the strongest argument for building it.

### Determinism

Allocation under contention must be **order-independent** — a deterministic priority rule over a
sorted set, never first-come by iteration order.

---

## What is absent, and known to be

- **Throughput.** The network answers *can this be reached*, never *how much can move*. Roads
  discount cost and nothing else; ports have no throughput meaning; the Reach lens shows a binary
  field; convoys queue without contention. **Four shipped systems are waiting on the same consumer.**
- **Interdiction is silent** (NR-407) — see above.
- **No sea routes.** Roads are land-only by ruling, and coastal ports (BL-188) are parked. The
  five-times speed gap between caravan and coastal vessel is authored and unreachable.
- **No inter-body logistics beyond a straight line.** Space distance is Euclidean body-centre to
  body-centre; there are no orbital mechanics in the prototype, by design.
- **Nothing generates a hub.** No corp is seeded an `inland_logistics_hub` and the rival scorer has
  no build candidate for one, so the anchor set in a played game is essentially *the generated
  cities*, unchanged forever.
- **No escort.** A convoy is cargo and cannot be defended. Assigning a unit to a convoy — turning
  interception into a real battle — is named in BL-458's design and deliberately excluded from it.

---

## Open questions

1. **What generates LP, at what rate, and what is the smallest honest first cut?** Ruled: a rate,
   a cap, node-half-only, with its consumer. Unruled: the numbers, and the two consumers that both
   already exist (`commit_convoy` and `run_unit_upkeep`).
2. **Does an over-subscribed network fail, queue, or degrade?** Named as open when BL-464 was filed
   and still open. It decides whether LP reads as a wall or as friction.
3. **Does the player see LP as a number, a lens, or only as a refusal?** A refusal-only mechanic is
   cheap and is how BL-458 shipped silent.
4. **Should a hub be buildable by a rival?** Finding 5 says the AI cannot build what it would pay
   for. Either the scorer gains a candidate or LP is asymmetric on arrival.
5. **What does the network cost to maintain?** BL-538's *logistics maintenance* budget line assumes
   a nation can pay for road upkeep. Nothing decays, so nothing needs paying for — the line has no
   sink to fill.

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
(stance, which is interdiction's only predicate), [`PRODUCTION.md`](PRODUCTION.md) (§ Logistics and
transport capacity, an older open note this document supersedes).

**Backlog.** **BL-464 (logistic points)** is the live design, `design-owed`, A. BL-458
(interdiction) shipped without its surfaces. BL-188 (coastal ports) is parked and holds sea trade.
BL-452 (convoy verbs) and BL-453 (convoys ledger) are the player-facing halves. BL-323 (buildings
rework) landed reach; BL-146/BL-172 landed roads; BL-077 landed the core.
