# Project Io — The Empire Tree

> **Settles:** what the Empire tree is for and what it must produce · its spire, its four branches
> and every node on them · the one fork and the four milestones · the scorer — for every node,
> the situation in which a polity pursues it · what the tree hands the Exploration tree.
> **Not here:** the grammar the tree obeys — kinds, rings, the five rules, diffusion, state, where
> research comes from (TREES) · the phase it runs in — seats, stores, reach-gating, culture
> relations (../CIVILISATION) · what a work row does once unlocked (../../lore/HISTORY § The
> works roster) · what a unit row does (../MILITARY_HISTORY) · the trees either side of it
> (COLONISATION_TREE, INDUSTRY_TREE).
> **Confused with:** TREES.md, ../CIVILISATION.md, ../../research/ANCIENT_TECH_LADDER.md.

The store is `empire_tree.json`; `node tools/session/tree_lint.js empire` holds the two together.

---

## Aims

**The Empire phase is sixteen hundred years of city states becoming empires and falling apart
again** (`../CIVILISATION.md`), and this tree is what a polity *spends its rounds on* when it is
not campaigning, settling or consolidating. Every node feeds one of the four things that phase is
about: how far a seat reaches, what it can field, what it keeps, and whether it can hold what it
took.

**Pre-industrial invention is slow, so this tree differentiates by doctrine and by ground, never
by capacity alone.** Before the industrial band connected polities sit within a band of each
other; what makes two empires different at 1200 CE is which fork they took, which gates their
ground let them pass, and how many rounds they had left when the spire's rim opened. The tree's
rim milestone is the reason a polity enters the Exploration tree early or late, which is where the
1960 spread is manufactured.

**Sizing.** Four rings, four branches and the spire: 52 nodes of a 64 cap — 34 majors, 14 minors,
4 milestones, one fork. The sizing rule in `TREES.md` § Sizes is the sweep's to assert: the
leading polity in a world reaches *The Enforceable Promise* shortly before 1200 CE, and the median
stands near *The Sworn Province*.

---

## The spire — Institutions

The sovereign counts, promises, administers, and finally binds itself. Ring by ring the spire is
the state learning to act at a distance from the seat, which is the whole political ladder of the
phase read as technology.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EM-SP-1a | Codified Law & Census | major | 1 | — (root) | — | practice | institution: the sovereign counts; manpower +40‰ | *spire* — held at unlock |
| EM-SP-1m | **The Written Ledger** | milestone | 1 | SP-1a | — | — | opens ring 2 | *spire* — requires HA-1a, RD-1a |
| EM-SP-2a | Coinage at Scale | major | 2 | SP-1m · RD-2a · HA-2c | ore_q | practice | stores +80‰; research +40‰ | *stores_low* — the seat cannot pay the levy, and there is ore to strike |
| EM-SP-2m | **The Sworn Province** | milestone | 2 | SP-2a | — | — | opens ring 3 | *spire* — requires PE-2a, RD-2a, and the granary fork taken |
| EM-SP-3a | Endowed Scholarship | major | 3 | SP-2m · PE-3c | — | practice | research +120‰; institution: the record survives the dynasty | *surplus* — nothing binds; the polity buys the rate |
| EM-SP-3m | **The Lettered Court** | milestone | 3 | SP-3a | — | — | opens ring 4 | *spire* — requires PE-3b, HA-3a |
| EM-SP-4a | Credit Instruments & Double Entry | major | 4 | SP-3m · PE-4a | — | practice | stores +100‰; research +60‰ | *stores_low* — campaigns outrun the seat; a promise is cheaper than a store |
| EM-SP-4m | **The Enforceable Promise** | milestone | 4 | SP-4a | — | — | opens the Exploration tree | *spire* — requires PE-4a, RD-4b |

Links are written without the `EM-` prefix in the tables for width; the store carries them in
full, each edge once on the node farther from the root (the root's own list is empty).

---

## Roads — reach

How far a seat can supply and govern, over land and across water. Reach **gates** a campaign in
this phase (`../CIVILISATION.md` § The road is the empire's skeleton), so this is the branch a
growing polity cannot avoid, and the sea majors hang off the land chain as coastal-gated leaves
that a landlocked polity never sees.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EM-RD-1a | Engineered Road Network | major | 1 | SP-1a | — | capacity | reach +100‰; unlock Way Station | *reach_bound* — holdings past `free_holdings`; the burden of breadth charges every round |
| EM-RD-1b | Road Surveys | minor | 1 | RD-1a | — | practice | reach +30‰ | *reach_bound* — on the way to the posted road |
| EM-RD-2a | Way Stations & Post | major | 2 | RD-1b | — | capacity | reach +80‰; cohesion +30‰ | *reach_bound* — a campaign target lies just past sustainable reach |
| EM-RD-2b | Deep-Hull Sail | major | 2 | RD-1b | coastal | capacity | access: strait leg; unlock Harbour Mole | *coastal_holdings* — the nearest unheld region is across a narrow water |
| EM-RD-2c | Span Bridges | minor | 2 | RD-2a · **AR-2b** | — | practice | reach +40‰ | *reach_bound* — holdings cut by rivers; masonry into roads is the cross-link |
| EM-RD-3a | Passes & Causeways | major | 3 | RD-2c | — | capacity | reach +100‰; unlock Span Bridge | *reach_bound* — the terrain term dominates the supply figure |
| EM-RD-3b | Lateen & Long-Range Rig | major | 3 | RD-2b | coastal | capacity | access: open-sea leg; reach +60‰ | *coastal_holdings* — a rival's seat lies across a sea leg the strait rig cannot stage |
| EM-RD-3c | Caravan Halts | minor | 3 | RD-3a | — | practice | forage +40‰ | *reach_bound* — stacks starve on the march off owned ground |
| EM-RD-4a | Ocean-Rated Hulls | major | 4 | RD-3b | coastal | capacity | access: far leg; unlock Deepwater Wharf | *coastal_holdings* — every reachable coast held; rivals beyond the open sea |
| EM-RD-4b | Imperial Cartography | major | 4 | RD-3c | — | practice | reach +60‰; intel: neighbours' holdings and stacks visible to the scorer | *threatened* — a grudge-bearing neighbour with a larger muster; the map is the cheapest defence |
| EM-RD-4c | Toll Stations | minor | 4 | RD-4b · **PE-4a** | — | practice | stores +40‰ | *stores_low* — trade crosses the roads and the seat keeps none of it; the link to chartered companies |

---

## Arms — muster and metal

What a seat can field and what it can hold against. The unit roster turns over on the band
boundary, so the branch's majors *open* rows and *upgrade* them rather than authoring units; what
it owns outright is the wall, the fortress and the forge.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EM-AR-1a | Bloomery Iron | major | 1 | SP-1a | ore_q | capacity | unlock Ore Pits; unlock unit roster: classical; defence +40‰ | *ground_ore* — ore in the hinterland and the classical rows unopened |
| EM-AR-1b | Drill & Levy Rolls | minor | 1 | AR-1a | — | practice | manpower +40‰ | *manpower_bound* — the muster wanted exceeds the ceiling |
| EM-AR-2a | Siegecraft | major | 2 | AR-1b | — | practice | upgrade unit roster: siege train; muster_cost −30‰ | *threatened* — the campaign the scorer wants ends at a walled seat |
| EM-AR-2b | Wall Circuit | major | 2 | AR-1b | — | capacity | unlock Wall Circuit; defence +100‰ | *threatened* — a neighbour's stacks outpower the seat's; the wall is what a weaker polity can afford |
| EM-AR-2c | Quartermasters | minor | 2 | AR-2a | — | practice | forage +40‰ | *reach_bound* — columns lose strength to supply before battle |
| EM-AR-3a | Stirrup Cavalry | major | 3 | AR-2c | grassland | capacity | unlock unit roster: medieval heavy horse; reach +40‰ | *ground_grass* — grassland held and horse in the roster |
| EM-AR-3b | Pattern-Forged Steel | major | 3 | AR-3c | ore_q | capacity | upgrade unit roster: arms and armour; defence +60‰ | *ground_ore* — a forge running and a rival whose stacks match |
| EM-AR-3c | Armourers' Guild | minor | 3 | AR-2b · **HA-2d** | — | practice | muster_cost −30‰ | *manpower_bound* — every levy takes more than the seat can spare; the water wheel's hammer is the cross-link |
| EM-AR-4a | Stone Fortress | major | 4 | AR-3c | — | capacity | unlock Stone Fortress; defence +150‰ | *cohesion_low* — ground lost and the polity consolidating; make the seat too expensive to take |
| EM-AR-4c | Blast Furnace | major | 4 | AR-4d | fuel | capacity | industrial +150‰; upgrade unit roster: cast iron | *ground_fuel* — ore and fuel both open, steel forged; the furnace crossing within reach |
| EM-AR-4d | Charcoal Burners | minor | 4 | AR-3b | — | practice | industrial +40‰ | *ground_fuel* — forest fuel in the hinterland |

---

## Harvest — food and stores

What the hinterland yields and what the seat keeps. The branch carries the tree's one fork, and
the prime movers — wheel and windmill — sit here rather than in Arms because their first
consumer is the mill, not the forge.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EM-HA-1a | Iron-Shod Plough | major | 1 | SP-1a | — | practice | carrying_capacity +80‰ | *food_bound* — population near the asymptote on the seat's own region |
| EM-HA-1c | Irrigation Works | major | 1 | HA-1a | arable | capacity | carrying_capacity +120‰; unlock Channel Works | *ground_farm* — arable ground with a river through it |
| EM-HA-2a | Temple Stores | major | 2 | HA-2c | — | practice | stores +150‰; cohesion +40‰; doctrine: Granary — by command | *stores_low* — stores fall to nothing after every muster; stability over depth |
| EM-HA-2b | Open Granaries | major | 2 | HA-2c | — | practice | stores +60‰; research +60‰; carrying_capacity +40‰; doctrine: Granary — priced | *surplus* — population under the asymptote and stores steady; depth over stability |
| EM-HA-2c | Harvest Tallies | minor | 2 | HA-1a · (SP-2a) | — | practice | stores +40‰ | *stores_low* — the seat does not know what its hinterland yielded; the shared minor under the fork |
| EM-HA-2d | Water Wheel | major | 2 | HA-1c | arable | capacity | unlock Water Mill; industrial +60‰; carrying_capacity +40‰ | *manpower_bound* — milling eats the labour split; work detaches from muscle |
| EM-HA-3a | Three-Field Rotation | major | 3 | HA-3c | — | practice | carrying_capacity +150‰ | *food_bound* — every held region at its asymptote and no ground inside reach |
| EM-HA-3b | Windmill | major | 3 | HA-2d | — | capacity | industrial +60‰; carrying_capacity +30‰ | *manpower_bound* — no river to turn a wheel |
| EM-HA-3c | Manuring & Fallow | minor | 3 | HA-2a · HA-2b | — | practice | carrying_capacity +40‰ | *food_bound* — on the way to rotation, from either side of the fork |
| EM-HA-4a | Land Reclamation | major | 4 | HA-3a | arable | capacity | carrying_capacity +150‰; access: wetland becomes arable | *food_bound* — at the asymptote everywhere, and ground it cannot yet farm |
| EM-HA-4b | Mill Networks | major | 4 | HA-3b | arable | capacity | industrial +120‰; stores +40‰ | *ground_fuel* — a furnace suggested by the windows; power in a building before power that moves |

### The fork — Granary Doctrine (ring 2)

> **Temple Stores** — the surplus is gathered and redistributed by command: deep stores, cohesion,
> markets stay thin ⊘ **Open Granaries** — the surplus is priced and traded: shallower stores,
> research and growth, famine risk stays live.

Both hang off *Harvest Tallies*; taking one closes the other permanently. *The Sworn Province*
requires the fork **taken**, either side, so no polity reaches ring 3 without having decided
whether its surplus is commanded or priced — the campaign's own premise, markets not command,
becomes something a polity chose. For an AI polity the scorer's *stores_low* versus *surplus*
reading decides it, which is the creed reading its own granary.

---

## Peoples — holding what was taken

Assimilation, cohesion, health and the record. This is the branch that decides whether an empire
persists past its founding culture, and *Syncretic Rites* at its rim is the mixing a civilisation
forms from (`../CIVILISATION.md` § A civilisation is what mixing makes).

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| EM-PE-1a | Physicians' Canon | major | 1 | SP-1a | — | practice | plague +60‰ | *plague_struck* — a drawdown struck the seat's region in the last rounds |
| EM-PE-1b | Shared Measures | minor | 1 | PE-1a | — | practice | assimilation +30‰ | *many_peoples* — held regions carry another people's shares |
| EM-PE-2a | Provincial Governors | major | 2 | PE-1b | — | practice | assimilation +80‰; cohesion +40‰; reach +30‰ | *many_peoples* — hinterland far from the seat, carrying foreign shares |
| EM-PE-2b | Paper | major | 2 | PE-1b | — | capacity | research +80‰; stores +30‰ | *surplus* — buying the rate; the record gets cheap |
| EM-PE-2c | Founded Infirmaries | minor | 2 | PE-2a | — | practice | plague +40‰ | *plague_struck* — drawdowns recur across the holdings |
| EM-PE-3a | Quarantine Doctrine | major | 3 | PE-2c | — | practice | plague +120‰ | *plague_struck* — a drawdown arrived along the polity's own roads |
| EM-PE-3b | Positional Arithmetic | major | 3 | PE-2b | — | practice | research +80‰; stores +40‰ | *surplus* — a record kept and rounds to make calculation cheap |
| EM-PE-3c | Temple Schools | minor | 3 | PE-3b · (SP-3a) | — | practice | assimilation +40‰ | *many_peoples* — foreign shares and a record to teach from |
| EM-PE-4a | Chartered Companies | major | 4 | PE-3c · (SP-4a) · (RD-4c) | — | practice | institution: an entity that outlives its members can own; stores +80‰ | *surplus* — stores exceed what campaigns need; the surplus wants an owner that is not the sovereign |
| EM-PE-4b | Syncretic Rites | major | 4 | PE-4c | — | practice | assimilation +150‰; cohesion +60‰ | *many_peoples* — the taken outnumber the founders and cohesion sags with every seat |
| EM-PE-4c | Pilgrim Roads | minor | 4 | PE-3a | — | practice | assimilation +30‰ | *many_peoples* — peoples who share a shrine but not a seat |

Parenthesised links are edges the other node's row already lists.

---

## Milestones

| id | name | ring | thesis | requires |
|---|---|---|---|---|
| EM-SP-1m | The Written Ledger | 1 | the sovereign counts a fed hinterland it can reach, therefore the sovereign can promise | Iron-Shod Plough (HA), Engineered Road Network (RD) |
| EM-SP-2m | The Sworn Province | 2 | a seat rules ground it cannot see | Provincial Governors (PE), Way Stations & Post (RD), **and the Granary Doctrine taken** |
| EM-SP-3m | The Lettered Court | 3 | a fed realm whose accounts are kept in figures can plan a year ahead | Positional Arithmetic (PE), Three-Field Rotation (HA) |
| EM-SP-4m | The Enforceable Promise | 4 | an entity that outlives its members can own, sue and be sued, and the realm is mapped | Chartered Companies (PE), Imperial Cartography (RD) |

Each requires majors from two branches at its own ring (rule 4), so no polity climbs the spire
by running one wedge to the rim. The rim milestone is what the Exploration tree's root requires.

---

## The scorer

**One shape** (`TREES.md` § The scorer — one shape, three trees): a frontier node's score is the sum of its effects,
each weighted by how hard that term binds the polity *now*, plus ground pull, plus spire pull,
plus a discount for a neighbour already holding it, minus cost. The tables above give each node's
dominant term; this section says what each term **reads**.

### The terms

Every term is a reading of state the sim already carries, and each has a cause a player could
point at on the map. None is a rank, and none grows with the polity's own size.

| term | reads | binds when |
|---|---|---|
| `reach_bound` | holdings past `free_holdings`; the burden-of-breadth charge; the best campaign candidate refused for reach | the polity wants ground it cannot stage against |
| `manpower_bound` | `manpower_stock` against `manpower_ceiling`; muster wanted by the campaign scorer | the levy the scorer wants cannot be raised |
| `food_bound` | region population against `region_carrying_capacity` across holdings | the hinterland is at its asymptote |
| `stores_low` | the seat's stores against the cost of the campaign the scorer wants | the polity cannot afford the action it prefers |
| `cohesion_low` | `polity::cohesion_q` below its consolidate threshold | ground has been lost and defeat is compounding |
| `threatened` | a bordering polity's stack power and `aggression_q`, and any directed grudge against this polity | a neighbour can and wants to take a seat |
| `plague_struck` | a drawdown on any held region inside the last N rounds | the population is falling for a reason the roster cannot fight |
| `ground_ore` · `ground_farm` · `ground_fuel` · `ground_port` · `ground_grass` | the mean endowment window of held ground — the `invest_ground_pull_q` reading, per window | the ground offers a node's gate |
| `many_peoples` | share of held population that is not the polity's own culture | the taken outnumber the takers |
| `coastal_holdings` | held regions with a port window; the nearest unheld or hostile region across water | the frontier is a coast |
| `spire` | rings held on the spire; majors held toward the next milestone's requirement | the milestone is one or two majors away |
| `known` | count of contact-graph neighbours holding the node | the node is already in the neighbourhood |
| `surplus` | none of the above above its threshold | nothing binds; ground and spire decide |

### Three polities, three first buys

**The forge people at their reach.** Ore in the hinterland, holdings two past `free_holdings`, a
neighbour's seat just out of range. *reach_bound* dominates, so the first buy is *Engineered Road
Network* and the road surveys behind it; *Bloomery Iron* follows on ground pull once the reach
charge eases. Its Written Ledger comes quickly because both its requirements are the two things
it wanted anyway.

**The river empire with too many peoples.** Arable everywhere, stores deep, three cultures on held
ground and cohesion slipping with every seat. *many_peoples* dominates: *Shared Measures* then
*Provincial Governors*, and the Sworn Province is reached on the Peoples side before Roads catches
up. At the granary fork *surplus* reads high and it takes *Open Granaries*; a century later the
same reading buys *Paper* and *Positional Arithmetic*, and this is the polity that reaches the
Lettered Court first.

**The coastal trader with no ore.** Every held region has a port window, no ore, a strait between
it and the next landmass. *coastal_holdings* dominates and *Deep-Hull Sail* is the first buy
after the root road; Arms stalls at *Drill & Levy Rolls* because *Bloomery Iron*'s gate is never
satisfied, so its defence comes from *Wall Circuit* on the *threatened* reading. Its Blast Furnace
never lights — which is the endowment story the ladder exists to tell, and the polity that enters
the Exploration tree with *Chartered Companies* and *Ocean-Rated Hulls* and nothing to smelt.

### What "research to spare" buys

A polity whose binding terms all sit below threshold reads *surplus*, and the scorer then falls
to ground and spire. It buys the research rate (*Endowed Scholarship*, *Paper*, *Positional
Arithmetic*), because the tree is not finished; it buys roads, because holdings grow under them;
and it buys walls and quarantine, because it can afford to be wrong. No node is authored for the
comfortable polity alone — every one of those is a node a pressed polity also wants.

---

## What the tree hands the Exploration tree

- **Entry timing.** A polity holding *The Enforceable Promise* at 1200 CE starts the
  **Exploration** tree at its root the moment that phase opens — which is 1200 CE itself, with
  the coast retired (`../EXPLORATION.md`); one that does not starts once it holds the rim. This
  is the first input to the 1960 spread, and it is now two trees away rather than one.
- **Sea legs.** Held *Deep-Hull Sail* / *Lateen* / *Ocean-Rated Hulls* are what Exploration's
  crossings and overseas claims stage from; a landlocked rim is a landlocked empire.
- **The furnace road.** *Blast Furnace* and *Mill Networks* held are the `industrial_mod` a polity
  crosses the industrial rung with; a polity without them lights late.
- **Corporate soil.** *Chartered Companies* is the institutional half of what the Industry tree's
  Works Doctrine and General Incorporation decide about whether a corporation can be chartered
  there at 1960 — reached through Exploration, which is where a charter first has a treasury
  behind it.
- **The civilisation.** *Syncretic Rites* held on ground carrying two peoples is the condition
  under which a civilisation record is coined (`../CIVILISATION.md`).

---

## Open questions

- **Magnitudes.** Every `per_mille` above is a placeholder by judgement; the sweep re-prices them
  against the sizing rule, and the works magnitudes are the precedent.
- **Where the terms' thresholds sit.** Each binding term needs the point at which it counts as
  binding; a measurement over the sweep's polities, not a number picked to make one seed read
  well.
- **Whether *Siegecraft* wants a term of its own.** *threatened* reads a neighbour's power; a
  walled rival seat is a different fact, and if the sweep shows sieges never bought it is a term
  missing, not a weight.
- **Whether the sea leaves are too cheap to be a real gate.** Three coastal majors is the whole
  water story of the phase; if every coastal polity holds all three by 800 CE the branch wants a
  fork.
