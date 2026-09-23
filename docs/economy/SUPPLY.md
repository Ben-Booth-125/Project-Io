# Project Io — Supply (Layer 5)

> **Settles:** what a convoy is and what it carries · what triggers a dispatch and who may order
> one · what a leg costs its owner · what arrival does to the destination pool · what
> infrastructure a route demands before traffic runs on it.
> **Not here:** the network beneath it — traversal cost, path, reach, roads, physical scale and
> how long a leg takes, interdiction, and the movement cap (LOGISTICS) · the price the cargo
> meets on arrival (MARKETS) · the promise a shipment may be settling (CONTRACTS).
> *Logistics is the road; Supply is the traffic.*
> **Confused with:** LOGISTICS.md, MARKETS.md, CONTRACTS.md.

> This document owns the **flow**: the convoy — its cargo, dispatch, cost and arrival.
> **[`LOGISTICS.md`](LOGISTICS.md) owns the network it runs on** — traversal cost, A\*, the reach
> field, roads, physical scale and travel time, cache invalidation, interdiction, and Logistic
> Points. *Logistics is the road; Supply is the traffic.* **Travel time is the network's**: how
> long a leg takes is a property of the ground crossed and the medium that crosses it, not of the
> cargo, and a marching unit reads the same model a convoy does.

Layer 5 of the economy is the **logistics / convoy layer** — the mechanism that physically moves goods between markets and bodies, coupling otherwise-isolated price pools through cargo movement. A convoy is the unit of flow; there is no abstract price-coupling term between bodies: the convoy *is* the coupling. The layer is BL-039 (supply convoys); `src/world/supply_system.{hpp,cpp}` is the implementation.

---

## Convoy entity

A convoy is a world ECS component. Each active convoy carries:

| Field | Type / values | Notes |
|---|---|---|
| `source_market` | market entity | The market the cargo was dispatched from |
| `dest_market` | market entity | The market the cargo is being delivered to |
| `mode` | `{land, sea, air, space}` | Determines infrastructure gate and cost multiplier |
| `cargo_resource` | resource enum | The good being transported |
| `cargo_qty` | quantity | Units in transit |
| `progress` | `0.0–1.0` | Fraction of route completed |
| `speed` | progress/Tick | Fixed linear advance per economy Tick |
| `id` | uint32 | Stable handle from `world::allocate_convoy_id`; what `hold_convoy` names. Never reused. A **transient** id — valid only while the convoy is in flight |
| `held` | bool | While true `advance_convoys` skips the convoy: stopped, not slowed |
| `cost_paid` | credits | What the haul was charged at dispatch; read by the Convoys tab |

The coupling is **market-to-market**, not body-to-body. A convoy is created when a seller's goods fetch more at another market, net of the haul, than at home (§ Dispatch trigger). It advances `progress` by `speed` each Tick (linear; no orbital mechanics in the prototype). On arrival (`progress >= 1.0`) it credits the destination **`(corp, market)`** pool, pays any import duty owed at a border (`MARKETS.md` § Tariffs), then is retired; the cargo reaches the destination market's supply through the ordinary auto-surplus path at the next clear — **at the destination, not at the owner's home market**, because the pool it lands in belongs to that market (`PRODUCTION.md` § Stockpile and output flow). There is no direct supply write on arrival — the clearing pass would zero it before pricing read it.

`speed` is fixed at dispatch from the leg's travel time, which [`LOGISTICS.md`](LOGISTICS.md) § 5 (physical scale and travel time) settles: the body's tile scale, the terrain weighting the path already carries, and the medium's rate, quantised to whole econ ticks.

Cargo leaves the source pool at **dispatch**, not arrival. Goods in transit are committed — the source pool shrinks immediately when a convoy departs.

**Trade-route recording** (BL-088, persistent trade routes). Before retiring an arrived convoy, `credit_arrived_convoys` (`src/world/supply_system.cpp`) also upserts a persistent `trade_route` into `world.trade_routes` — keyed on the unordered `(body_a, body_b)` pair + `corp`, with `last_tick` set to the completion Tick and `convoy_count` incremented. Intra-body lanes (source and destination collapse to the same body) are excluded — they light nothing. A route is never erased once recorded; staleness is a **read-time** concern owned by the activity fog, not a write-time one here. See `docs/ui/DISCOVERY.md` (BL-089, activity fog) for the fog that reads this substrate.

**Convoys are outside `world::state_hash`.** Their determinism check is `tools/verify/convoy_command.cpp` R5 (identical convoy sets across two runs) rather than the hash; folding them in would move the byte-identity baseline `spectator_determinism.cpp` pins.

---

## Logistical cost

Each convoy incurs a budget outflow:

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

For **space convoys**, `distance` is the Euclidean distance between the parent bodies' centres (no path routing — straight-line in the prototype). For **intra-body convoys** (land / sea), `distance` is the **terrain-weighted A\* path** over the body's tile grid (BL-077, intra-body pathfinding; `src/world/logistics.{hpp,cpp}`): each tile weighted by its landform cost (TILES.md — plains 1.0 … mountain 2.0) and discounted by `road_level`, respecting the east–west cylinder wrap; the edge cost is the average of the two tiles (so the path is symmetric) and results cache per fixed endpoint pair. Water tiles carry a higher sea-leg cost, so the cheapest path prefers land and a water crossing selects **sea** mode.

**Mode is a property of the leg, not of the whole route.** A route that crosses water is three legs: **land** from the source to a port, **sea** from port to port, **land** from the far port to the destination, each priced at its own mode and each travelling at its own speed. The ports are chosen to minimise the whole route's cost including both handling fees, and a sea leg runs only port to port (§ Infrastructure gates). A single route-wide `crosses_ocean` bit, which billed every land tile at the sea rate once any water appeared, cannot express a cheap sea leg between two land legs and is retired with this ordering.

The cost is charged in full at dispatch (`dispatch_convoys` debits `corp.balance` before the convoy is created; a corp that cannot afford the cheapest route dispatches nothing). It is the term that makes distant arbitrage marginal: a profitable inter-body trade requires `source_price + logistical_cost_per_unit < destination_price`.

**Logistics-node discount** (BL-148 / BL-149, logistics nodes). The intra-body haul cost is further discounted for each **logistics node** the A\* path crosses, so the world's cities — and the player's own hubs — form a cheap network the specialist corporation plugs into. A **population centre** on the path discounts by `logistics.node_discount.city_per_scale × centre.scale` (tier 1–5); an **Inland Logistics Hub** by a flat `logistics.node_discount.hub`. The summed discount is capped (`node_discount.cap`) so a route is never free, and is applied as `cost × (1 − discount)` (`dispatch_convoys`, over `logistics_path.tiles`). Since intra-body markets are city-seeded, most hauls deliver *into* a city and take the discount; the player extends the reach by placing hubs along a corridor. Deterministic — a pure function of the path tiles and the (population-centre / hub) node sets.

**Same-body dispatch** moves goods from one of the corp's market pools to another market on the same body, hauling from the corp's lowest-id building in the source market's catchment (the market's own
`centre_tile` when it holds none there — convoyed stock sits at the market) to the destination's
`centre_tile`, at the per-leg cost above. It is real trade: the goods leave one pool and sell from another.

---

## Dispatch trigger

**One beat per haul: arrivals land before the clear, and a seller hauls before it sells (Ben,
2026-09-23).** Convoys move only on the economy tick, and within it the order is fixed: convoys
**advance**, **arrivals are credited** to their destination market's pool, the economy runs, then
**dispatch** — and only then do the markets **clear**. Dispatch sits before the clear because
auto-surplus sells every unit a pool holds above its reservation to the local market, and a market's
shelf belongs to no one and never moves: a seller that has not chosen to haul by the clear has sold
at home. So the seller weighs home against elsewhere while the goods are still its own (the rule
below), and a delivery reaches its destination's clear before it can move again, since cargo moves
only toward a strictly better net price. The reverse order (dispatch at the top of the tick,
arrivals at its end) re-exported every delivery before any clearing saw it, and cargo circulated
market to market without reaching a shelf. Owner: BL-995 (trade reaches for price).

**Auto-dispatch is the default, and it is the SELLER chasing a NET PRICE (Ben, 2026-09-15).** On each economy Tick, for every `(corp, market)` pool holding a good above its processor reservation, the system asks where that good fetches the most once the haul is paid, and sends it there if that beats selling at home. The loop runs without player intervention.

It replaced a buyer-side rule — scan each market for a shortfall, fill it from the cheapest reachable source — that never read a price at all. Under it a seller never moved goods toward a better market, only toward an empty one, and trade stayed local however wide the gaps were.

**The rule, stated so it can be checked.** Using last tick's resolved prices:

```
net(d) = price_d − haul_per_unit(src → d) − handling_per_unit − duty_per_unit(d)
send to argmax_d net(d)   if   net(d) − price_src  >  margin_threshold × price_src
```

- **The destination is chosen by net price**, ties to the lower market id — a total order, so every run picks the same market.
- **The quantity is what the gap can absorb, not what the pool holds.** From `price ∝ √(demand/supply)`, adding `q` units to a destination's supply brings its price down to the source's landed price when `q = supply_d × ((price_d / (price_src + haul + handling + duty))² − 1)`. The send is `min(surplus, q)`, less what the corp already has in transit to that market — so a gap draws enough to close it and no herd of convoys floods it. A destination with **no** supply has nothing for the formula to scale, so its absorbable quantity is its unmet demand instead.
- **The threshold is authored in data and measured**, never zero: a margin of a rounding error is not a reason to move a cargo across a continent.
- **A shortfall is not a separate trigger.** A market short of a good prices it high, so the net-price rule already reaches it — and reaches it from wherever landing it is cheapest, not merely from wherever is nearest.

**It is one rule for every corporation, the player's included** (Ben, 2026-09-15). Auto-dispatch is logistics automation of the same kind as auto-surplus, not a strategic act on the player's behalf, so the player's goods move by the same rule a rival's do. The player keeps `hold_convoy` on any convoy and the directed verb below for any haul the rule would not choose. The rival scorer's own directed-dispatch valuation reads the same net price, home price subtracted — a scorer that valued a haul by the destination price alone would send goods away from a better home market.

**Player-direction is the exception** (BL-452, convoy verbs). A player (or an agent) directs a
specific convoy through the `dispatch_convoy` corp_verb: subject = source market, `counterparty` =
destination market, `target` = cargo, `quantity` = units. It is the auto-dispatch body above
**with the shortfall scan removed** — `price_convoy_leg` + `commit_convoy` (`supply_system.hpp`)
are shared by both callers, so a player's convoy and a rival's of the same shape cost the same,
travel at the same speed and pick the same mode. There is deliberately no fourth code path, and
`tools/verify/convoy_command.cpp` asserts the two agree rather than trusting that they do.

**`hold_convoy` stops a convoy; nothing cancels one.** Cargo leaves the source pool at dispatch, so
a cancel would have to invent a return leg or mint the goods back at the source. `hold_convoy`
instead flips a `held` flag that `advance_convoys` skips: the convoy stops dead on its lane, pays
nothing further (the haul was paid once), and resumes from the same progress when the verb is issued
again. A toggle, not a one-way door.

**The in-app dispatch form lives on the market Selection card** (BL-607, dispatch form) — a
dispatch starts from a source
you are looking at, and it is a resource + quantity + destination-market form, not a press. The
Market Ledger's Convoys tab (BL-453, convoys ledger) lists the result, shows `cost_paid`, and
carries the Hold press.

**Space launches auto-dispatch too.** `dispatch_convoys` auto-dispatches inter-body convoys exactly
like intra-body ones, gated only on the corp holding a launchpad on the source body
(`corp_has_launchpad_on`). Whether leaving the gravity well *should* be an explicit player
decision is a design call that belongs to the space arc.

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

"Throughput" here is *cost-discount*, not a capacity cap — capacity is Logistic Points (LOGISTICS.md § Logistic Points). The **generated road network** (BL-146, road generation; `src/world/road_generation.cpp`, `generate_roads`): after nations + population centres exist, each nation's centres are joined by an MST + relative-neighbour-redundancy backbone over terrain-weighted A\* costs; each edge's tier is chosen from the two centres' scales — **Highway** between two major centres (population `scale ≥ 3`), **Road** when at least one endpoint is Town+ (`scale ≥ 2`), **Track** otherwise — rasterised along the A\* path (water skipped), with one **Track** border link between the nearest centre pair of each territorially-adjacent nation. Generation measures the lanes road-free (to lay the network out), then clears `world.astar_cost_cache` so gameplay dispatch recomputes against the stamped roads. **Player placement** (BL-147, player roads): the tile build front door offers all three tiers (`place_road(tile, tier)`, cost `economy.roads.{track,road,highway}`); placement is **upgrade-in-place** — valid when the chosen tier strictly exceeds the tile's current `road_level`, so a Track can be raised to a Highway but the same-or-lower tier is refused. The on-canvas render (PLANETARY.md) draws each roaded tile's own half of every shared edge, so a road **spans symmetrically** between the two tiles it joins with no "from vs to" asymmetry, weighted by tier. Determinism + connectivity + the 3-tier ceiling are pinned by `tools/verify/road_generation_harness.cpp`; placement + upgrade by `tools/verify/logistics_harness.cpp` (T10). A distinct **railroad** *mode* (not a road tier) is BL-173 (railroad mode).

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
