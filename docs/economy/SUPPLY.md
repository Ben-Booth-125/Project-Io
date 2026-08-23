# Project Io — Supply (Layer 5)

> This document owns the **flow**: the convoy — its cargo, dispatch, cost, travel and arrival.
> **[`LOGISTICS.md`](LOGISTICS.md) owns the network it runs on** — traversal cost, A\*, the reach
> field, roads, physical scale, cache invalidation, interdiction, and Logistic Points. *Logistics
> is the road; Supply is the traffic.* Where the two overlap (travel time, logistical cost), this
> document keeps the convoy-facing statement and LOGISTICS.md keeps the network-facing one.

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

The coupling is **market-to-market**, not body-to-body. A convoy is created when goods are dispatched toward a destination shortfall. It advances `progress` by `speed` each Tick (linear; no orbital mechanics in the prototype). On arrival (`progress >= 1.0`) it credits the destination `(corp, body)` pool, then is retired; the cargo reaches the destination market's supply through the ordinary auto-surplus path at the next clear. There is no direct supply write on arrival — the clearing pass would zero it before pricing read it.

Cargo leaves the source pool at **dispatch**, not arrival. Goods in transit are committed — the source pool shrinks immediately when a convoy departs.

**Trade-route recording** (BL-088, persistent trade routes). Before retiring an arrived convoy, `credit_arrived_convoys` (`src/world/supply_system.cpp`) also upserts a persistent `trade_route` into `world.trade_routes` — keyed on the unordered `(body_a, body_b)` pair + `corp`, with `last_tick` set to the completion Tick and `convoy_count` incremented. Intra-body lanes (source and destination collapse to the same body) are excluded — they light nothing. A route is never erased once recorded; staleness is a **read-time** concern owned by the activity fog, not a write-time one here. See `docs/ui/DISCOVERY.md` (BL-089, activity fog) for the fog that reads this substrate.

**Convoys are outside `world::state_hash`.** Their determinism check is `tools/verify/convoy_command.cpp` R5 (identical convoy sets across two runs) rather than the hash; folding them in would move the byte-identity baseline `spectator_determinism.cpp` pins.

---

## Travel time — distance costs time, not only money (Ben, 2026-08-12)

**A tile has a physical size, and it is derived rather than authored.** Planetology generates
`home_mass`; a rocky planet's radius follows its mass as roughly R ∝ M^0.27, so radius →
circumference → `circumference / grid_width` gives kilometres per tile. At Earth mass on the
312-column grid that is **~128 km per tile** (`body_km_per_tile`, `src/world/logistics.hpp`).

Without it, convoy speed would be `1 / distance_in_AU` — an interplanetary calibration —
and since `body_distance_au` returns 0 for two markets on the same body, **every intra-body
convoy would arrive in exactly one econ tick (90 days)** whether it crossed one tile or the whole
map. Distance would cost money and never time, and a bigger map would only mean the same 90 days
buys more reach.

**Travel time reuses the terrain weighting the pathfinder already computes.** `logistics_path::cost`
is terrain-weighted (plains ×1.0 … mountain ×2.0), so it is a count of *effective* tiles — and
terrain cost is already a time multiplier. No parallel table is needed:

```
days   = path.cost × km_per_tile ÷ km_per_day
ticks  = ceil(days ÷ 90)          # the economy clears quarterly
```

Two modes, differing by roughly five times, which is what makes coastal trade worth designing
(BL-188, coastal ports): **land ~25 km/day** (an ox-and-cart caravan) and **sea ~130 km/day**
(a coasting vessel). A short regional haul lands in one quarter; a long one takes several.

The space lane keeps its own ~1-tick-per-AU calibration — it is the only leg the AU model is right
for, and it belongs to the space arc on `era/space`.

## Logistical cost

Each convoy incurs a budget outflow:

```
logistical_cost = base_logistics_cost × distance × cargo_qty
```

`base_logistics_cost` is a per-mode multiplier from the Lua economy-constants registry (`scripts/economy.lua`), ordered:

```
land < sea < air < space
```

For **space convoys**, `distance` is the Euclidean distance between the parent bodies' centres (no path routing — straight-line in the prototype). For **intra-body convoys** (land / sea), `distance` is the **terrain-weighted A\* path** over the body's tile grid (BL-077, intra-body pathfinding; `src/world/logistics.{hpp,cpp}`): each tile weighted by its landform cost (TILES.md — plains 1.0 … mountain 2.0) and discounted by `road_level`, respecting the east–west cylinder wrap; the edge cost is the average of the two tiles (so the path is symmetric) and results cache per fixed endpoint pair. Water tiles carry a higher sea-leg cost, so the cheapest path prefers land and a water crossing selects **sea** mode.

**Mode is a property of the leg, not of the whole route.** `logistics_path::crosses_ocean` as a single route-wide bit makes one water tile bill every land tile on that route at the sea rate and travel at the coastal speed; the honest shape is per-leg pricing, which BL-522 (crosses-ocean is a whole-route bit) owns.

The cost is charged in full at dispatch (`dispatch_convoys` debits `corp.balance` before the convoy is created; a corp that cannot afford the cheapest route dispatches nothing). It is the term that makes distant arbitrage marginal: a profitable inter-body trade requires `source_price + logistical_cost_per_unit < destination_price`.

**Logistics-node discount** (BL-148 / BL-149, logistics nodes). The intra-body haul cost is further discounted for each **logistics node** the A\* path crosses, so the world's cities — and the player's own hubs — form a cheap network the specialist corporation plugs into. A **population centre** on the path discounts by `logistics.node_discount.city_per_scale × centre.scale` (tier 1–5); an **Inland Logistics Hub** by a flat `logistics.node_discount.hub`. The summed discount is capped (`node_discount.cap`) so a route is never free, and is applied as `cost × (1 − discount)` (`dispatch_convoys`, over `logistics_path.tiles`). Since intra-body markets are city-seeded, most hauls deliver *into* a city and take the discount; the player extends the reach by placing hubs along a corridor. Deterministic — a pure function of the path tiles and the (population-centre / hub) node sets.

**Same-body dispatch** fills a same-body market shortfall from the corp's on-body pool, hauling from the corp's representative tile (its lowest-id building on the body) to the short market's `centre_tile` at the A\* cost + mode above.

---

## Dispatch trigger

**Auto-dispatch is the default.** On each economy Tick the system scans for destination shortfalls (demand exceeds local supply) and dispatches convoys from the cheapest reachable source to fill them. The loop runs without player intervention.

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

**The in-app dispatch form lives on the market Selection card** — a dispatch starts from a source
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

Mode is selected by the source/destination pair: inter-body → **space**; intra-body → **land** by default, **sea** when the cheapest A\* path crosses water (`path.crosses_ocean` — the *path* picks the mode, not an infrastructure check). The endpoint gates per mode:

| Mode | Gate |
|---|---|
| **Land** | Ungated |
| **Sea** | **Port** building at both endpoints — BL-188 (coastal ports) owns the port model and the gate |
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
