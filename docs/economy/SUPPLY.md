# Project Io — Supply (Layer 5)

> **Settles:** what a convoy is and what it carries · what a leg costs the trader · what arrival
> does at the destination · what infrastructure a route demands before traffic runs on it.
> **Not here:** what ships, between which markets, and who decides — the trade (TRADE) · the
> network beneath it — traversal cost, path, reach, roads, physical scale and how long a leg
> takes, interdiction, and the movement cap (LOGISTICS) · the price the cargo meets on arrival
> (MARKETS) · the promise a shipment may be settling (CONTRACTS).
> *Logistics is the road; Supply is the traffic.*
> **Confused with:** TRADE.md, LOGISTICS.md, MARKETS.md, CONTRACTS.md.

> This document owns the **flow**: the convoy — its cargo, cost and arrival.
> **[`TRADE.md`](TRADE.md) owns what ships**: a trade decides the good, the source and destination
> markets and the quantity; a convoy is that trade's **shipment** in transit.
> **[`LOGISTICS.md`](LOGISTICS.md) owns the network it runs on** — traversal cost, A\*, the reach
> field, roads, physical scale and travel time, cache invalidation, interdiction, and Logistic
> Points. *Logistics is the road; Supply is the traffic.* **Travel time is the network's**: how
> long a leg takes is a property of the ground crossed and the medium that crosses it, not of the
> cargo, and a marching unit reads the same model a convoy does.

Layer 5 of the economy is the **convoy layer** — the shipment that physically carries a trade's
goods between markets and bodies. A convoy is the unit of flow; there is no abstract price-coupling
term between bodies: trade is the coupling, and the convoy is how it travels
(`TRADE.md`). The layer is BL-039 (supply convoys); `src/world/supply_system.{hpp,cpp}` is the
implementation.

---

## Convoy entity

A convoy is a world ECS component. Each active convoy carries:

| Field | Type / values | Notes |
|---|---|---|
| `source_market` | market entity | The market the cargo was bought at |
| `dest_market` | market entity | The market the cargo is being delivered to |
| `mode` | `{land, sea, air, space}` | Determines infrastructure gate and cost multiplier |
| `cargo_resource` | resource enum | The good being transported |
| `cargo_qty` | quantity | Units in transit |
| `progress` | `0.0–1.0` | Fraction of route completed |
| `speed` | progress/Tick | Fixed linear advance per economy Tick |
| `id` | uint32 | Stable handle from `world::allocate_convoy_id`; what `hold_convoy` names. Never reused. A **transient** id — valid only while the convoy is in flight |
| `held` | bool | While true `advance_convoys` skips the convoy: stopped, not slowed |
| `cost_paid` | credits | What the haul was charged when the shipment left; read by the Convoys tab |
| `origin_tile`, `port_a`, `port_b` | tile entities | An intra-body route's waypoints, fixed when the shipment leaves (the leg paths between them are not): the source market's centre, and the loading and unloading Ports of a route with a sea leg (none on a single overland leg). They make the lane the route the haul was priced on (§ Logistical cost) |

The coupling is **market-to-market**, not body-to-body. A convoy is created only by a trade's
shipment (`commit_trade_shipment`, the one place a convoy is made): the trade buys the cargo off
the source market's shelf, pays the haul, and the convoy carries the goods (§ A shipment). It
advances `progress` by `speed` each Tick (linear; no orbital mechanics in the prototype). On
arrival (`progress >= 1.0`) the cargo **lands** on the destination market, pays any import duty
owed at a border (`MARKETS.md` § Tariffs), and the convoy is retired; the landing is listed as that
tick's supply and **sold to the market at that tick's clearing price** — at the destination, where
it landed (`MARKETS.md` § The clearing tick). Landing is selling, for a trade's cargo as for a
building's output.

`speed` is fixed when the shipment leaves, from the leg's travel time, which
[`LOGISTICS.md`](LOGISTICS.md) § 5 (physical scale and travel time) settles: the body's tile scale,
the terrain weighting the path already carries, and the medium's rate, quantised to whole econ
ticks.

Cargo leaves the source shelf when the shipment **leaves**, not when it arrives. Goods in transit
are committed — the trader has bought them, and the source shelf shrinks immediately.

**Trade-route recording** (BL-088, persistent trade routes). Before retiring an arrived convoy, `credit_arrived_convoys` (`src/world/supply_system.cpp`) also upserts a persistent `trade_route` into `world.trade_routes` — keyed on the unordered `(body_a, body_b)` pair + `corp`, with `last_tick` set to the completion Tick and `convoy_count` incremented. Intra-body lanes (source and destination collapse to the same body) are excluded — they light nothing. A route is never erased once recorded; staleness is a **read-time** concern owned by the activity fog, not a write-time one here. See `docs/ui/DISCOVERY.md` (BL-089, activity fog) for the fog that reads this substrate.

**Convoys are outside `world::state_hash`.** Their determinism check is `tools/verify/convoy_command.cpp` R5 (identical convoy sets across two runs) rather than the hash; folding them in would move the byte-identity baseline `spectator_determinism.cpp` pins.

---

## Logistical cost

Each shipment charges its trader a budget outflow:

```
logistical_cost = base_logistics_cost × distance × cargo_qty
```

`base_logistics_cost` is a per-mode multiplier from the Lua economy-constants registry (`scripts/economy.lua`), ordered **per distance**:

```
sea < land (at its best road tier) < air < space
```

**SEA IS THE CHEAPEST WAY TO MOVE A TON A LONG WAY, AND A PORT IS WHAT IT COSTS TO START (Ben,
2026-09-15).** That is the mechanism history shows and the one the opening map needs: a long
overseas haul should beat a short overland one, and a short coastal hop should not beat a road.
Two terms carry it:

- **Per distance, a sea leg costs less than land on a highway.** The ordering is the ruling; the
  values are measured.
- **Every port a cargo passes through charges a HANDLING fee per unit** — at loading and again at
  unloading — independent of distance. A short sea leg pays two fees for little saving; a long one
  amortises them. The crossover distance at which sea beats land is the design reading, and it is
  reported whenever either term moves.

**The crossover reading (2026-10-05, BL-1194 sea cheaper than highway).** A rate multiplies a
terrain-weighted path, so the ordering is read per tile. A water tile weighs 2.5 against plains
1.0; a Highway cuts a land tile to ×0.40, a Road ×0.50, a Track ×0.67. At land 0.02 and sea 0.002
per unit distance, one unit costs per tile:

| Leg | Per tile |
|---|---|
| land, plains, Highway | 0.0080 |
| land, plains, Road | 0.0100 |
| land, plains, no road | 0.0200 |
| sea, open water | 0.0050 |
| sea, on a sea lane | 0.0025 |

With handling 0.10 a port, a sea route pays 0.20 a unit to start. It beats an equal-length land
haul once it saves `0.20 / (land tile − 0.0050)` tiles: **67 against a Highway, 40 against a Road,
24 against a Track, 13 against unroaded plains** (the home body is 261 tiles round). The node
discount on a land leg (up to 50%) can take a Highway through cities to 0.0040 a tile. That one
land haul stays cheaper than open sea at any length. Constants: `scripts/economy.lua`
`logistics.base_cost_per_unit_distance` and `port_handling`; the 2.5 weight is `sea_leg_cost`
(`src/world/logistics.cpp`).

For **space legs**, `distance` is the Euclidean distance between the parent bodies' centres (no path routing — straight-line in the prototype). For **intra-body legs** (land / sea), `distance` is the **terrain-weighted A\* path** over the body's tile grid (BL-077, intra-body pathfinding; `src/world/logistics.{hpp,cpp}`): each tile weighted by its landform cost (TILES.md — plains 1.0 … mountain 2.0) and discounted by `road_level`, respecting the east–west cylinder wrap; the edge cost is the average of the two tiles (so the path is symmetric) and results cache per fixed endpoint pair. Water tiles carry a higher sea-leg cost, so the cheapest path prefers land and a water crossing selects **sea** mode.

**Mode is a property of the leg, not of the whole route.** A route that crosses water is three legs: **land** from the source to a port, **sea** from port to port, **land** from the far port to the destination, each priced at its own mode and each travelling at its own speed. The ports are chosen to minimise the whole route's cost including both handling fees, and a sea leg runs only port to port (§ Infrastructure gates). A single route-wide `crosses_ocean` bit, which billed every land tile at the sea rate once any water appeared, cannot express a cheap sea leg between two land legs and is retired with this ordering.

**The lane is the legs** (BL-1195, convoy lane follows legs). A convoy stands on, is seen along and is cut on the route its cargo travels and was priced on — overland, or land to the loading Port, port to port across the water, and land on from the unloading Port — never on the direct line between the two market centres. Its position along that lane follows the leg TIMES: each leg at its own speed (a land leg roughly five times slower than a sea leg), so the cargo stands where that clock puts it, not at an even share of the tiles. The times are recomputed from the current leg paths, so they equal the times the haul was priced on while the network is unchanged. The clock is stretched over the haul's whole travel ticks: a convoy's progress is the fraction of those ticks elapsed, and the lane's clock reads 0 at the origin and 1 at the destination. Within a leg, the time splits over its hops in proportion to each hop's priced edge cost — the same cost, river discount included, the leg's path was priced on. The head the canvas draws, the vision beam, interdiction and capture all read that one lane, `convoy_route_tiles` (`src/world/logistics.cpp`). **Interdiction sweeps the tick's travel**: a hostile unit standing on any tile the cargo crosses during a tick — a land leg inland, a Port, or the sea leg's water — intercepts it, and the first such tile in lane order is where it is cut; water the route never enters is no ambush. **A captured cargo lands on the market under the interception tile**, and is sold there for the interceptor at that tick's clear; with no market under the tile, the cargo is destroyed. **What is fixed when the shipment leaves is the origin and the two Ports, nothing more.** Each leg's path between them is read from the network as it stands, so a road or hub built while the cargo is in transit can re-route a leg, and move the head along it, on the next read; a Port built or lost in transit never moves the crossing.

The cost is charged in full when the shipment leaves (`commit_trade_shipment` charges the trader before the convoy is created; a trader that cannot afford the cheapest route ships nothing). It is the term that makes distant arbitrage marginal: a profitable trade requires `source_price + logistical_cost_per_unit < destination_price`.

**Logistics-node discount** (BL-148 / BL-149, logistics nodes). The intra-body haul cost is further discounted for each **logistics node** the A\* path crosses, so the world's cities — and the player's own hubs — form a cheap network the specialist corporation plugs into. A **population centre** on the path discounts by `logistics.node_discount.city_per_scale × centre.scale` (tier 1–5); an **Inland Logistics Hub** by a flat `logistics.node_discount.hub`. The summed discount is capped (`node_discount.cap`) so a route is never free, and is applied as `cost × (1 − discount)` (the leg pricing, over `logistics_path.tiles`). Since intra-body markets are city-seeded, most hauls deliver *into* a city and take the discount; the player extends the reach by placing hubs along a corridor. Deterministic — a pure function of the path tiles and the (population-centre / hub) node sets.

**A same-body leg runs market centre to market centre** (`price_market_leg`): from the source
market's `centre_tile` — the goods were bought off its shelf — to the destination's `centre_tile`,
overland or through two Ports, at the per-leg cost above. It is real trade: the goods leave one
shelf and land on another.

---

## A shipment

**What ships is the trade's to decide; this section is how it travels.** A trade — manual or auto —
names the good, the source and destination markets and, through its trade points, the most it may
move this tick (`TRADE.md` § A trade, § Auto and reserved trade). Its shipment then runs these
mechanics, the same for every owner and for manual and auto alike.

**One beat per haul: arrivals land before the clear, and trade ships before it (Ben,
2026-09-23; Ben, 2026-10-10).** Convoys move only on the economy tick, and within it the order is
fixed: convoys **advance**, **arrivals land** on their destination market, the economy runs, the
**trade pass** ships — and only then do the markets **clear**. An arrival is therefore that tick's
supply at its destination, sold at that clear; a trade's purchase is a shelf draw billed at that
clear's posted price. The reverse order (shipping at the top of the tick, arrivals at its end)
re-exported every delivery before any clearing saw it, and cargo circulated market to market
without reaching a shelf.

**A shipment, step by step** (`commit_trade_shipment`, `src/world/supply_system.cpp`):

1. **It buys at the source.** The cargo is bought off the source market's shelf at the posted
   price, under the fair-price ceiling ([MARKETS.md](MARKETS.md) § Settled: every draw BUYS),
   and the purchase is posted as want at the source like any bid. A shelf priced over the
   ceiling sells the trade nothing.
2. **On a space leg, it buys the launch.** The trader must hold a Launchpad on the source body, and
   buys the launch's propellant off the same shelf (`PRODUCTION.md` § Launchpad). A cargo of
   propellant cannot burn itself: the launch's own draw comes off the shelf first.
3. **It is capped by the place.** On a leg within a body the cargo passes the **passive Logistic
   Point** cap at the anchor nearest the source market's centre ([LOGISTICS.md](LOGISTICS.md)
   § Logistic Points): a cargo sized above what the anchor still admits this tick leaves at what
   it admits and pays that share of the leg's cost, rather than not leaving at all (Ben,
   2026-10-05, NR-969). Trade points are the owner's capacity; Logistic Points are the place's; a
   shipment passes both (`TRADE.md` § The Planetary Marketplace).
4. **It pays the haul now.** The trader's balance is charged the leg's cost when the shipment
   leaves (§ Logistical cost). A trader that cannot cover the haul and the purchase ships nothing.
5. **It travels as a convoy** on the lane the haul was priced on, seen and interdicted along it.
6. **It lands and sells at the destination** on arrival, at that tick's clearing price, and pays
   any import duty at a border (`MARKETS.md` § Tariffs).

A shipment is **all-or-nothing on refusal**: a leg that is not viable, a shelf the ceiling refuses,
a shelf or anchor that admits nothing, or a trader who cannot pay leaves nothing mutated.

**The trader keeps the margin and pays the haul** (`TRADE.md` § A trade): the destination's
clearing price on landing, less the source's posted price, less the haul. A margin is not
guaranteed by sending: the landing moves the destination's price it is sold at.

**`hold_convoy` stops a convoy; nothing cancels one.** Cargo leaves the source shelf when the
shipment leaves, so a cancel would have to invent a return leg or mint the goods back at the
source. `hold_convoy` instead flips a `held` flag that `advance_convoys` skips: the convoy stops
dead on its lane, pays nothing further (the haul was paid once), and resumes from the same progress
when the verb is issued again. A toggle, not a one-way door. **Held cargo is still in transit
(Ben, 2026-09-24)**: a trade sizing its next shipment counts it as already on its way to its
destination; releasing the hold is what delivers it.

**The Market Ledger's Convoys tab** (BL-453, convoys ledger) lists the shipments in flight, shows
`cost_paid`, and carries the Hold press. Setting a trade is `TRADE.md`'s.

**Reachability.** In the prototype all bodies are treated as reachable (Exploration is a data-model stub). Infrastructure gates (below) are the operative constraint on reachability, not exploration state.

---

## Infrastructure gates

Mode is selected by the source/destination pair: inter-body → **space**; intra-body → a route of **legs**, land by default with a **sea** leg wherever a port-to-port crossing makes the whole route cheaper (§ Logistical cost — the *route* picks the legs, and a sea leg exists only where ports do). The gates per mode:

| Mode | Gate |
|---|---|
| **Land** | Ungated |
| **Sea** | A **Port** at each end of the sea leg — not at the source or destination market, which may lie inland behind a land leg. BL-608 (sea port gate) owns the Port; every port a cargo passes through charges handling (§ Logistical cost) |
| **Air** | **Airfield** building at both endpoints |
| **Space** | **Launchpad** at origin (`corp_has_launchpad_on`) + **Orbital Port** at destination; Era 1 required (ERAS.md) |

Roads are a land cost-reducer over a **three-tier ladder** (BL-172, road tiers). `road_level` is a `tile_component` field (default 0) that discounts the A\* traversal cost of the tiles a route crosses (`road_traversal_multiplier` = `1 / (1 + 0.5·tier)`):

| Tier | `road_level` | Traversal ×  | Placed by |
|---|---|---|---|
| **Track** (minor / low-throughput) | 1 | ×0.67 | player, generation (spurs + border links) |
| **Road** (regular) | 2 | ×0.50 | player, generation |
| **Highway** (high-throughput backbone) | 3 | ×0.40 | player, generation (major-city backbone) |

"Throughput" here is *cost-discount*, not a capacity cap — capacity is Logistic Points (LOGISTICS.md § Logistic Points). The **generated road network** (BL-146, road generation; `src/world/road_generation.cpp`, `generate_roads`): after nations + population centres exist, each nation's centres are joined by an MST backbone over terrain-weighted A\* costs, a loop admitted only where the network's route exceeds twice the direct one (LOGISTICS.md § 4, the detour test); each edge's tier is chosen from the two centres' scales — **Highway** between two major centres (population `scale ≥ 3`), **Road** when at least one endpoint is Town+ (`scale ≥ 2`), **Track** otherwise — rasterised along the A\* path (water skipped), with one **Track** border link between the nearest centre pair of each territorially-adjacent nation. Generation measures the lanes road-free (to lay the network out), then clears `world.astar_cost_cache` so gameplay shipments recompute against the stamped roads. **Player placement** (BL-147, player roads): the tile build front door offers all three tiers (`place_road(tile, tier)`, cost `economy.roads.{track,road,highway}`); placement is **upgrade-in-place** — valid when the chosen tier strictly exceeds the tile's current `road_level`, so a Track can be raised to a Highway but the same-or-lower tier is refused. The on-canvas render (PLANETARY.md) draws each roaded tile's own half of every shared edge, so a road **spans symmetrically** between the two tiles it joins with no "from vs to" asymmetry, weighted by tier. Determinism + connectivity + the 3-tier ceiling are pinned by `tools/verify/road_generation_harness.cpp`; placement + upgrade by `tools/verify/logistics_harness.cpp` (T10). A distinct **railroad** *mode* (not a road tier) is BL-173 (railroad mode).

**River discount** (BL-170, rivers). A river is generated as a directed **edge** across one of a
tile's 6 hex sides (never a tile-occupying feature — see `docs/generation/TILE_GENERATION.md`
§ Rivers), traced downhill from high ground to ocean/basin over the Pass-1 heightmap
(`src/world/river_generation.cpp`, `generate_rivers`). Where the intra-body A\* path
(`src/world/logistics.cpp`) crosses a river-carrying edge, `river_edge_discount` applies a
further multiplier — **0.75× downstream, 0.85× upstream** — folded into the edge cost
**alongside**, and **multiplicatively with**, the road-tier discount above: a Highway that also
runs downstream compounds to `0.40 × 0.75 = 0.30`. No new resource or market good is involved;
this is a pure logistics-cost effect, sized to sit within the road ladder's scale (Track 0.67 …
Highway 0.40) rather than outstrip it — a river is a bonus lane, not a road-network replacement.
`tile_component::river_edges` / `river_downstream` (`src/world/components.hpp`) carry the per-side
bitmasks; `tile_borders_river` reads water-adjacency as a secondary consequence of the same
bitmask, for farming rather than for logistics.

Per-node throughput capacity — how much cargo a node can pass per Tick — is **Logistic Points**, owned by BL-464 (logistic points) and designed in LOGISTICS.md.
