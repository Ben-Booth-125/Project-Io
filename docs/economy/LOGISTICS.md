# Project Io — Logistics

> **Settles:** what it costs to cross a tile and which path is taken · how far a placement may
> reach · where roads come from and who may extend them · how physical scale becomes travel
> time and how long a leg takes · what can cut a route · what caps how much may be in motion at
> once.
> **Not here:** what ships and who decides — the trade (TRADE) · the convoy itself — its cargo,
> cost and arrival (SUPPLY) · what the cargo is worth at either end (MARKETS) · what the ground is
> made of (TILES).
> *Logistics is the road; Supply is the traffic.*
> **Confused with:** SUPPLY.md, TILES.md, MARKETS.md.

**The network.** How far anything is from anything else, what it costs to cross, how long it takes,
and what the network permits. This document owns the **substrate**; `SUPPLY.md` owns the **flow that
runs on it** (convoys).

> The split, stated once so it stops being ambiguous: **LOGISTICS is the road; SUPPLY is the
> traffic.** A convoy's existence, cargo and arrival are SUPPLY's; what it carries and where is a
> trade's (TRADE). The cost it pays per
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

An **edge** cost is the mean of its two nodes, but a river discounts an edge in one direction only
(downstream is cheaper than upstream), so **a path is directed**. Its cost is always the origin →
destination cost, read one way only — never whichever direction a cache happens to hold (Ben,
2026-09-25; BL-1126, path cost reads the cache). A cost that depended on what was cached would
make a loaded game continue differently from the one that was saved.

### 2. Pathfinding — `intra_body_path`

Terrain-weighted A\* over a body's tile grid, respecting the **east-west cylinder wrap**. Grid
topology matches `nation_generation.cpp`: 4-cardinal neighbours, raster index
`grid_y × grid_width + grid_x`. The core pathing design is BL-077 (intra-body pathfinding).

Results cache on `world.astar_cost_cache` under the **ordered** (origin, destination) key, since a
path is directed (§ 1), so the per-tick trade pass pays each search once.

> **A trap worth carrying forward.** A cached path's tiles are stored low tile to high tile
> whichever way it was asked, so a caller reading one must apply its own orientation.
> `convoy_route_tiles` orients a convoy's route once for every reader (the canvas, interdiction),
> and a unit's march orients its own. Get the orientation wrong and a convoy's head lands at the
> wrong end of the lane half the time — **invisible on screen, fatal to interdiction.**

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

### 3a. Power rides this network (Ben, 2026-08-31)

**Power is transmitted on the road network, and it needs no new graph.** Ben: *"Power travels via
road infrastructure, although it is different from a convoy, it has a 1 qtr travel time."*

Three properties, and each one reuses something already here:

1. **Connectivity, not distance.** A building can draw from a generator if a path exists at all.
   That is exactly `tile_reach_cost` read as a boolean — **finite means connected, infinity means
   not** — so the multi-source Dijkstra of § 3 already answers the question and no second field is
   needed.
2. **Flat latency: one tick, regardless of distance.** Power is not priced by traversal cost the way
   cargo is; a connection either exists or it does not, and if it does the power is available on the
   next tick. This is what makes it *unlike* a convoy despite sharing the road.
3. **Interdiction cuts it, for free.** § 7's rule already lets a hostile unit sever the network. A
   cut that strands a market from its generation is now a cut that turns the lights off there —
   Conflict reaching Trade through infrastructure rather than through a special case.

**Connection gates the TRADE, not only the draw** (Ben, 2026-08-31). Power is bought and sold
(`PRODUCTION.md` § Power), so what this network decides is **who may match whom**: a buyer can only
clear against a generator its roads reach. Power therefore has a price that is **regional by
construction** — a well-connected region with generation is cheap, a stranded one is expensive or
dark — without any rule naming a region. It is the first good whose **movement and market are
separate questions**: a price, and no convoy.

The design consequence worth stating: **roads become dual-purpose.** A road built for convoys also
carries power, so the network's value is no longer only throughput, and a region's power is a fact
about its *connectedness* rather than about its ground. That is a supply asymmetry with a cause a
player can read and change, which is what `docs/generation/GENERATION_STRATEGY.md` § Asymmetry is the
deliverable asks generation to produce.

**The province is the grid's cell (Ben, 2026-10-07; BL-1230, power crosses markets).** A province
with a road in it is **wired**, and every building in a wired province is on the power grid there —
the province, not the building's own tile, is what the road has to reach. Wired provinces whose
roads join form **one grid**: a building in any of them draws from any generator on that grid,
across market boundaries, on the one-tick latency above. A building pays **its own market's power
price**, cleared against the generation on its grid, so power keeps a market price while its
supply is the grid's. A province with no road is dark, and a cut that splits the roads splits the
grid (§ 7). **A market's shelf is on its centre's grid (Ben, 2026-10-07):** power listed into a
market feeds the grid that market's centre stands on, and a building bids at its own market — so
"on the grid" is read through the market centre, which keeps power on the ordinary per-market shelf
(measured: buildings on a grid other than their market centre's hold 1.3–2.1% of power need).
Pooled over a grid, the shelf's share in the price law (`MARKETS.md` § Price resolution, at most
k ticks of demand) is capped once, at the grid, not once per market on it. A
wired grid with no generation on it runs short and decays like any short draw; the answer is
generation on that grid, not a softer rule. Found by the BL-1228 (mine upkeep supply) diagnosis: power held on one market's shelf
never reached a mine in the next market's catchment, and that alone held most mines at the
shortfall floor.

`docs/economy/PRODUCTION.md` § Power owns the generation buildings, the upkeep draw and the
shortfall rule; this section owns only the transmission.

### 4. Roads — generated, then extended by hand

Generated per nation after population, deterministically from the campaign seed (BL-146 /
BL-172, road generation). Local streets first: every centre's own tile gets at least a Track.
Then the **backbone**, over towns-and-up only (scale ≥ 2): a weighted graph over the town
pairs, a **Kruskal MST**, loops admitted only by the **detour test**, a three-tier assignment,
and rasterisation along each edge's A\* path taking the **max** `road_level` on overlap.

**A road is not built where a road already serves (Ben, 2026-09-25, walking round 6: "it should
be heavily discouraged to build a lattice of roads").** The tree comes first. A further link
between two centres is laid only when the network's own route between them costs more than
**twice** the direct route — the detour test, one number. A second link between two centres a
serviceable route already joins is never built, and a loop exists only where the tree forces a
long way round. The test reads links, not tiles: two links can still lay side-by-side runs on
the raster, which the snap (below) makes rare, not absent. This replaces
the relative-neighbour redundancy edges, which laid the lattice.

The test is read on the **town graph**, not the raster: the network's route is the cheapest chain
of already-accepted links, each priced at its own direct cost. Candidates are walked
cheapest-first, and each admitted loop joins the network as it is accepted. **Only a link that
can be laid is a candidate** (delegated reading, NR-945): a route across open sea is never a road,
so it never enters the tree or the test, and a nation the sea divides builds one tree per
landmass.

**Roads pull toward markets (Ben, 2026-09-25: "roads make trade easier, they should pull towards
market centres per nation, and across markets to bridge and provide logistics extension for
larger trades"; BL-1138, roads pull to markets).** Two pulls, read after the market folds leave a
world its tens of markets (`MARKETS.md` § Market centres and seeding). **Within a nation** the
network is drawn toward its market centres: every market centre is on the backbone, and a town's
road is weighed by how much nearer it brings the town to its market. **Across markets** a trunk
joins each market centre to its neighbouring market centres, over a border where the neighbour
lies across one, so a larger trade has a road to travel beyond its own catchment; the trunk
takes the lattice's own tier gates (Road or above where the gate allows; Ben, 2026-10-03: where a
nation falls under 0.40 its roads stay Track, the newer ruling over the trunk's "at Road"). **Each
tile of a trunk or pull takes the tier of the nation whose land it crosses** (Ben, 2026-10-03), so
a sub-0.40 nation's ground stays Track and unowned land reads Track: the Road cliff holds
everywhere. The
detour test still refuses a trunk link a serviceable route already gives. The trunk's neighbours
are a delegated reading, NR-950: each market centre's **three nearest** market centres by direct
route, unioned over both ends (a Delaunay-like set, never all pairs), chosen on the network as the
pass finds it and walked cheapest-first; a pair is priced from a land end, and a pair no land end
can price is dropped.

The two pulls are one pass after the folds, over the laid network. **A market centre
joins its own nation's backbone**: the network it must reach is its nation's own roads from its
nation's towns, never another nation's (a border link is a Track between two networks, not part of
either backbone), and the join runs over the nation's own land. A market whose nation holds no
town has no backbone and stays off it, counted.
**One cost model** (§ 1): every route is priced with `tile_traversal_cost`. The **direct route**
walks any land and any strait of up to two shore-water cells, never open ocean; every link is
priced along it, so it reuses the roads that already shorten it, and laid along it as the snap
(below) reshapes it. Catchments are grid-nearest, so these
roads move no catchment and no fold. **Within a pass, a route is priced on the field its
destination's cost flood was first built on (Ben, 2026-10-09):** the floods are reused, not
rebuilt after every stamp, which is what keeps the road passes inside their time budget —
rebuilding after every stamp measured 2.5–4× slower on the sixteen curated seeds. So a link reuses
the roads that stood when its destination's flood was built, and one laid later can run beside
it. **What is laid is the priced route, reshaped by the snap** (BL-1252, no parallel roads —
the item's handle; the snap makes long parallels rare, below).
Before a national route (tree, loop, spur, border link) or an ancient corridor is stamped, every
stretch of it that would lay two or more consecutive new land tiles, each within one cell (Chebyshev) of a road the route does
not hold, is re-walked over the stretch and the road cells beside it. The re-walk minimises new
ground, not cost: a road cell counts 1 and a new tile 4. It replaces the stretch only when it
lays fewer new tiles, keeps the route simple (no cell visited twice), and costs no more than
**1.75×** the stretch it replaces. Both are priced on the one cost model, on the field as the
stamp leaves it. So a laid link may cost up to that factor more than its priced route over a
snapped stretch. No flood is rebuilt for it. The snap also catches a route that a current flood
priced one cell off a road it nearly tied with.

The snap answers the **adjacent-run** measure: two roads in side-by-side cells (d = 1). It
leaves, by construction:
- a parallel two cells apart;
- a run of one new tile;
- a stretch whose re-walk would fold the route or pass the cost bound;
- contact by diagonal only;
- the market joins, pulls and trunk, which are not snapped.

Long side-by-side runs are therefore rare, not absent. On the sixteen curated seeds, pairs of
routes running side by side for eight or more tiles fall from 235 to 59. A current flood on
every route reads 103.

Three readings, accepted by Ben (2026-10-03) and kept as readings: a trunk link's tier is the gate at the
**lower** of its two nations' percentiles; **the network route** is the same walk over
roaded land only, any nation's roads included (the road network as a convoy prices it; the river
discount, which is directed, is not read); and **the pull is not rationed** by the qualification
percentile, as detour loops are. A town's weight toward its market is the gain, network route less
direct route; per market the heaviest town failing the test is joined first and the network
re-read, so one spoke serves its neighbours.

**Villages join locally, not as lattice members** (BL-620, road generation scales to density):
**only a village at or above a size floor lays a spur (Ben, 2026-09-25)**, and the floor is
**40,000 heads** (Ben, 2026-09-25, from the measured ladder: about the 90th percentile of village
size on the curated seeds). A village's size is its headcount in the population step, the figure
that tells villages apart when roads are laid. Each such village lays one Track spur to its
nearest same-nation tile **already joined to the backbone** — backbone raster, a town's streets,
or an earlier spur that reached the backbone — chosen from a distance-prefiltered candidate set,
never all-pairs. A spur that only reaches another unjoined village joins nothing, so it does not
count: a village is on its nation's network only when its road reaches a town. A village whose nearest target is beyond the spur cap, or
whose every route would cross open sea, keeps only its local street. Low-stratum settlements
feed the network; they do not define it — which is both the honest historical shape and what
keeps generation cost linear in village count at demography-derived density (BL-610).

**Three tiers** (Ben, 2026-07-11): **Highway** (3) between two major centres, **Road** (2) when at
least one endpoint is Town+, **Track** (1) otherwise. Then one Track border link between the nearest
centre pair of each territorially-adjacent nation pair, so the network connects across the
continent. **A border link ends only on a town or a spurring village** (Ben, 2026-09-25): a link
that ended on a bare village street joined nothing, so the nearest pair is chosen among the centres
that are on their nation's network.

**The network scales with the nation's qualification** (Ben, 2026-08-25; BL-618, roads scale with
qualification): a nation's qualification fraction (`docs/economy/POPULATION.md` § Qualification)
modulates how many of its detour loops it keeps and its tier promotion, so a low-qualification
nation generates a sparser, lower-tier network. A national development level the map already
shows, not a new dial.

The gates are **era-relative** (Ben, 2026-08-25, ruling on NR-641; BL-621, era-relative road
gates): they read the nation's qualification **percentile among the world's nations** — mid-rank
on ties — never the absolute fraction. Tier promotion: a Highway needs its two major endpoints
*and* percentile ≥ 0.80; a Road needs its Town+ endpoint *and* percentile ≥ 0.40; a gated-out
edge demotes one rung, never disappears. The loops the detour test admits are **rationed**
cheapest-first — the kept fraction is the percentile itself, so the median nation keeps half its
loops and the MST always survives whole. An all-tied world (the antiquity epoch, every nation at the seeding
floor) grades everyone at 0.5: Roads on every Town+ backbone, Highways nowhere — the
Roman-roads-analogue backbone — while a spread industrial world promotes its leaders to
Highways and demotes its laggards to Track. Only a nation *behind its own world* lays an
all-Track lattice — engineering above the track is a product of relative, not absolute,
qualified labour.

**Roads are a land feature.** Water tiles are skipped, and an edge whose route crosses *open* ocean
is not stamped at all — that is a sea route, and stamping it would scatter fragments on distant
shores. A short crossing made of shore (a strait, TILES.md § Water kinds) does get a road.
**A bridge spans at most two water tiles** (Ben, 2026-10-03, playing the build: *"bridges can
cross a further distance than I expected — let's put a cap on that"*; he ruled two). The cap is
one constant every writer of the road field reads — the national lattice, its spurs and border
links, the ancient corridors (§ 4a), and the market joins, pulls and trunk — and a link whose
route would bridge a longer run is refused or walked round by land. The wizard's lapse draws
its roads anchor to anchor with no tile chain, so it holds the same cap by not drawing a
corridor whose straight line would bridge more.
Territorial adjacency tolerates a short unowned gap, so an island or coastal nation is reachable
rather than silently left off the lattice.

The player extends the lattice with `place_road`; rivals do too, through the same verb.

### 4a. The ancient network — roads stamped FROM the history

> *"We should also be laying simple roads to supply provinces."*
> — Ben, 2026-09-03, the eight-phase reorder, point 4

**The national lattice above is not the only road on the map.** Before any nation existed the
Era −1 sim moved armies and founding parties across the ground, and those lines are the
world's first roads. They are **derived from the history, never laid inside it**: the sim
records each corridor it walked — a campaign's staging-holding-to-objective supply line, a
settle's parent-to-daughter route — and a pass immediately after it stamps those lines onto
`road_level`. `GENERATION_STRATEGY.md` § The eight phases states why the record is the only
possible shape: the sim has no write channel to the world, its pathfinder returns a cost
between regions rather than a list of tiles, and the modern pass's node source does not exist
until after it runs.

**The ancient tier rule is its own, not the industrial one.** § 4's gates read a nation's
qualification percentile — a field derived from industrialisation timing, which an antiquity
world neither has nor has any spread in. What an ancient corridor has instead is:

- **traffic** — how many times the history actually used it. Repeat traffic earns a **Road**;
  a line walked once is a **Track**;
- **works** — a corridor whose **two** ends both raised something reach-bearing from the Era −1
  works roster (`docs/lore/HISTORY.md` § The works roster) promotes one rung. Both ends,
  because a paved trunk with a station at one end and nothing at the other is a road that
  stops. This is the **only** route to a **Highway** before the industrial era, which keeps
  § 4's antiquity shape intact — a world that built nothing carries Roads and no Highways —
  while giving the works roster a payoff that persists onto the campaign map.

**Purely additive.** The stamp takes the maximum per tile, so no national road is ever
downgraded and the ancient corridors appear where the modern lattice did not reach or reached
lower. The land rule is unchanged: water tiles are skipped and a corridor whose route crosses
open ocean is not stamped at all.

Why it matters beyond decoration: `docs/generation/PROVINCES.md` § Richness is absorbed makes
roads the term that **gates what a rich province can yield**. Laying them from the history is
what turns that gate into a fact with a visible cause — a province is well-served because an
empire supplied through it, not because a generator rolled well.

BL-768 (roads and markets from history) owns this design; `src/world/road_generation.hpp`
carries the constants and the measurement they were read off.

**Roads do not decay** (Ben, 2026-08-22): *"Roads do not decay, but nations have to pay tax to
support them. If a nation runs into too much debt supporting infrastructure, it can go bankrupt
with major penalties. But between these states nothing changes."* The cost is **binary, not
graduated**: a solvent nation's roads behave exactly as built; a bankrupt one suffers major
penalties; there is no middle band where a strained network degrades. A graduated version would
be a second decay model wearing a budget's clothes, which the first half of the ruling rejects.
This is the sink the *logistics maintenance* line of the national budget (BL-538, national budget)
pays into, and it gives a nation its first failure state — BL-550 (national insolvency).

**The tax buys materials — the Infrastructure demand channel** (BL-643, network upkeep draws;
`src/world/network_upkeep.{hpp,cpp}`). Each economy tick a nation's network bills **stone and
timber** at authored per-element rates (`economy.network_upkeep`, scripts/economy.lua): every road
tile in its territory by level, plus every active port and inland hub standing on its ground. The
bill is **geography, never population** — network size is derived from the world each tick, so the
channel scales exactly as MARKETS.md § Demand channels property 1 demands. The purchase is the
state-purchase shape (NATIONS.md § A budget): bought off the market's shelf at its posted price,
under the fair-price ceiling, and the goods **consumed** — repairs go into the roadbed.
Unlike the space programme's lumps the claim is **pro-rata** (rule 3): upkeep is continuous, so
half the repair budget buys half the materials, and an underfunded quarter is a reported partial
fill, never a banked lump. Consistent with the binary ruling above, an unfunded draw degrades
nothing — the network's failure state remains insolvency, not decay.

### 4b. Sea lanes — the colonial ties, stamped FROM the colonial record

**The water analogue of § 4a.** The colonial era
(`../generation/EXPLORATION.md` § The colonial tie is a sea lane, and the map reads it) records every sea leg it walked — a purchase party's crossing, a
campaign's sea supply, the standing traffic between a metropole and what it holds — and a pass
after it stamps those legs onto the water as a **sea lane** tier that discounts the sea-leg
traversal cost in § 1. Traffic earns the tier: a leg earns the lane at `sea_lane_tier1_uses`, four
uses by default, mirroring `road_tier1_uses` — the count at which the history's own ladder promotes
a land corridor to its first rung, Track (`src/world/history_sim.hpp`), and the count the stamp onto
the campaign map reads as a Road (`kAncientRoadUses`, § 4a) — so a crossing made once is no lane.
**Purely additive and purely water**: land tiles are untouched, and the § 4a rule that a
corridor crossing open ocean is not stamped as road is unchanged — the crossing becomes a lane
instead. Trade across water is the fourth writer of a use (`EXPLORATION.md`).

**How a lane is laid (BL-1098).** A lane's path is walked over the sea only — ocean and coast, never
a lake — on **the same grid every traversal reader walks**: the four cardinal steps of § 2, with
the columns wrapping. A lane is laid only where a convoy can follow it tile to tile, so its discount
is the same whatever its bearing; a diagonal walk would leave every other step on open water and
the pathfinder could not ride it. A lane goes round a headland rather than across it. Each step is priced by its length
and by the current it runs with or against (`EXPLORATION.md` § Currents), so a lane bends along the
current. **A realm's port is its nearest coastal region's seat (Ben, 2026-09-27, NR-955):** a seat
with no sea within the sim's neighbour radius (nine tiles) lays its lane from the nearest region of
its realm that has one, rather than laying none, and the lane starts at that seat's nearest sea
tile. Nearest is the sim's own region distance (Chebyshev between the seats, columns wrapping), ties
to the lower region; the realm is the one holding the seat when the history closes. A seat no realm
holds, or whose realm holds no coastal region, still lays none. The
lane record carries no direction, so the walk runs toward the busier end — the seat more lanes
touch — as the old-road stamp does (§ 4a), which is where tribute and trade flow (delegated
readings, NR-955). A laned sea tile's traversal cost is **halved** (× 0.50): the lane is the road
ladder's second rung on water, as four uses make a Road on land.

**Lanes reuse lanes, as roads reuse roads (Ben, 2026-10-03).** Lanes are walked busiest first — most
uses, ties by the leg's two regions — and a walk enters water an earlier lane already laid at half
its priced step (`kSeaLaneReuseCostQ` = 500, the lane's own × 0.50 read as a traveller reads it).
So lanes into one port share a trunk near it and fan out far from it, and the map shows a network
rather than a fan of parallel lines. At 1000 every lane is walked alone. **A trunk is shared only
with the current (Ben, 2026-10-03):** the reuse discount applies to a step only where the walk's
step runs with the current; against it the walk pays the full step, trunk or not. A busy trunk
does not carry a later lane upstream, so the lanes the map draws still ride the currents
(`sea_lane_stamp_harness` row B).

**A tie is therefore a force on the map, never a preference inside an actor.** Because § 1 is one
weight function, a lane is read by everything that reads traversal cost: a convoy between a
colony's market and its metropole's lands cheaper than one to a stranger's, so a colony's chains
close through its metropole *first* without any preferred seller being named — and **placement
reach widens across the lane** (§ 3), so a firm on one shore may legally hold a site on the other.
That second effect is wider than a market preference and it is intended: a tie only prices could
see would be invisible to the corporate search, which reads reach cost (BL-812, phase 6 sees roads).

**A lane outlives the polity that made it**, as a road outlives the empire that paved it. Whether
a lane *decays* when its traffic stops is open in `../generation/EXPLORATION.md` § Open questions;
roads do not (§ 4a), but the sea is not a roadbed.

### 5. Physical scale and travel time (Ben, 2026-08-12)

**A tile has a physical size, and it is derived rather than authored.** Planetology generates
`home_mass`; a rocky planet's radius follows its mass as roughly `R ∝ M^0.27`, so radius →
circumference → `circumference / grid_width` gives kilometres per tile — tile width falls out of a
scalar the generation chain has already settled. At Earth mass on the 312-column grid that is
**~128 km per tile** (`body_km_per_tile`, `src/world/logistics.hpp`), which puts a day's march at
about a fifth of a tile and makes a tile **a region-sized unit rather than a field.**

Without a tile scale, speed on this network would be `1 / distance_in_AU` — an *interplanetary*
calibration — and since `body_distance_au` returns 0 for two markets on the same body, **every
intra-body haul would arrive in exactly one econ tick (90 days)** whether it crossed one tile or all
312. Distance would cost money and never cost time, and a bigger map would only mean the same 90
days buys more reach.

**Travel time reuses the terrain weighting the pathfinder already computes.** `logistics_path::cost`
is weighted by § 1's one weight function (plains ×1.0 … mountain ×2.0), so it is a count of
*effective* tiles — and terrain cost is already a time multiplier. The A\* weights do double duty
rather than needing a parallel table:

```
days   = path.cost × km_per_tile ÷ km_per_day
ticks  = ceil(days ÷ 90)          # the economy clears quarterly; minimum 1
```

**Two speeds, and the gap between them is a design lever:** **land ~25 km/day** (an ox-and-cart
caravan) against **sea ~130 km/day** (a coasting vessel). Roughly five times, *"and that difference
is the whole reason coastal trade is worth designing"* — BL-188 (coastal ports) owns the sea-trade
design that reaches the faster speed. A short regional haul lands in one quarter; a long one takes
several.

The **space leg** is the one leg the AU calibration is right for: it keeps its own ~1-tick-per-AU
rate over the Euclidean body-centre distance of § 8, while the tile scale above governs everything
that crosses a body's ground.

### 6. Cache invalidation — narrowed, for a real reason

`invalidate_logistics_caches` clears every derived logistics cache together: the reach field, the
per-pair paths and the flood fields behind them, and the per-body nearest-anchor field. An
over-clear costs one Dijkstra; **a missed clear is a reach field that lies.**

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

**Capture, with destruction as the fallback** (Ben, 2026-08-17). Cargo leaves the source shelf when
the shipment leaves, so the goods are already committed and either answer conserves. On
interception the cargo **lands on the market under the interception tile** and is sold there for
the interceptor at that tick's clear, as any landing is (`MARKETS.md` § The shelf economy); with no
market under the tile it is destroyed instead, and the outcome says which.

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
| Serves | trade — every trade's shipment, manual or auto, capped where it leaves | movement a player or rival **directs** |
| Drawn by | the goods a trade ships | **militaries draw active only** |
| Owned? | ambient — the network's | **owned**, and not a rival's to use |

**Cities generate it.** That answers two constraints at once: a node-generated rate is real from
turn one because cities are generated in the hundreds, where no corp is seeded a hub; and a city is
a spatial locus — LP is *"how much can move through HERE"*, never a per-corp haul allowance.

**The split is what stops free-riding.** Ben, on whether rivals should build hubs: *"the passive /
active split explains how we would be unable to use rival active LP."* Ambient throughput serves
everyone; directed throughput is yours. It also dissolves the asymmetry of a rival paying LP it
cannot generate, since the half that serves trade is not owned by anybody.

**Trade points are not Logistic Points (Ben, 2026-10-10).** Trade points are how much an *owner*
can move, made by its Planetary Marketplaces and Ports (`TRADE.md`); passive LP is how much can move
*through a place*. A shipment spends its owner's trade points and still passes the passive cap at
the anchor nearest its source market's centre, on a leg within a body (`SUPPLY.md` § A shipment).
Neither replaces the other.

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
   `military_points` was deleted for.** Cities are the locus. A draw lands on an anchor at the
   least traversal cost from its origin, with a fixed choice among exact ties (the lower anchor
   tile id wherever the costs can tell them apart); one multi-source field per body answers that
   for every tile at once, so the answer depends on the body's tiles and anchors and on nothing a
   cache happens to hold.
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
  **The rule covers COMMANDED legs (Ben, 2026-10-05, NR-969):** a leg somebody named for this
  tick — a march — is refused whole and told. A trade is a standing route that ships every tick
  on its own, with nobody to tell, so a whole refusal there would be exactly the silent
  interdiction this rule forbids; its shipment sends what the anchor admits instead (`SUPPLY.md`
  § A shipment).
- **Throughput is a lens**, extending Reach. The Reach lens shows a binary field; throughput is
  that field with a magnitude, so it is a small step from an existing surface rather than a new one.
- **Allocation under contention is order-independent** — a deterministic priority rule over a
  sorted set, never first-come by iteration order.
- **Active lands first (Ben, 2026-08-24, the Sprint 18 design form)** — the priced march is the
  first consumer; passive convoy admissibility follows in the same sprint, each half landing with
  its consumer (settled rule 3, applied per half). Rates for both halves are first-cut in the
  landing items, argued against BL-543's (value anchor) unit-cost anchor and flagged for tuning
  rather than ruled ahead (NR-600).
- **Armies claim first (Ben, 2026-09-24, NR-917).** Within a tick the march draws Logistic Points
  before the trade pass does, so when an anchor is contended the front is supplied and the trade
  waits. That is a chosen priority, not a phase-order accident: an army's supply is the more urgent
  draw, and starving trade to feed a war is a legible cost a player can read and plan around.

---

## Where the parts live

| Concern | File |
|---|---|
| Traversal cost, A\*, caches, reach field, scale, travel time | `src/world/logistics.{hpp,cpp}` |
| Road generation and tiers | `src/world/road_generation.{hpp,cpp}` |
| A trade's shipment, its cost, arrival and interdiction | `src/world/supply_system.{hpp,cpp}` |
| What a trade ships | `src/world/trade.{hpp,cpp}` |
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
