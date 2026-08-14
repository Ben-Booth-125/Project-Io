# Project Io — Supply (Layer 5)

Layer 5 of the economy is the **logistics / convoy layer** — the mechanism that physically moves goods between markets and bodies, coupling otherwise-isolated price pools through cargo movement. A convoy is the unit of flow; there is no abstract price-coupling term between bodies: the convoy *is* the coupling. BL-039 (supply convoys) landed as v0.0.7's theme and is **complete**, so this document is now the authority for the shipped layer (§ Build status for the landed/outstanding split, updated 2026-07-31).

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

The coupling is **market-to-market**, not body-to-body. A convoy is created when goods are dispatched toward a destination shortfall. It advances `progress` by `speed` each Tick (linear; no orbital mechanics in the prototype). On arrival (`progress >= 1.0`) it credits the destination `(corp, body)` pool, then is retired; the cargo reaches the destination market's supply through the ordinary auto-surplus path at the next clear (BL-382, the dead market writes, removed a direct supply write the clearing pass zeroed before pricing ever read it).

Cargo leaves the source pool at **dispatch**, not arrival. Goods in transit are committed — the source pool shrinks immediately when a convoy departs.

**Trade-route recording (BL-088).** Before retiring an arrived convoy, `credit_arrived_convoys` (`src/world/supply_system.cpp`) also upserts a persistent `trade_route` into `world.trade_routes` — keyed on the unordered `(body_a, body_b)` pair + `corp`, with `last_tick` set to the completion Tick and `convoy_count` incremented. Intra-body lanes (source and destination collapse to the same body) are excluded — they light nothing. A route is never erased once recorded; staleness is a **read-time** concern owned by the activity fog, not a write-time one here. See `docs/ui/DISCOVERY.md` (BL-089) for the fog that reads this substrate.

---

## Travel time — distance costs time, not only money (2026-08-12)

Until this date **distance cost money and never cost time.** Convoy speed was `1 / distance_in_AU`
— an interplanetary calibration — and `body_distance_au` returns 0 for two markets on the same
body, so speed clamped to 1.0 and **every intra-body convoy arrived in exactly one econ tick
(90 days)**, whether it crossed one tile or the whole map.

That is why tripling the map (312×145, 45,240 tiles) could not on its own make distance feel
bigger: with travel time constant, a bigger map just means the same 90 days buys three times the
reach.

**A tile now has a physical size, and it is derived rather than authored.** Planetology already
generates `home_mass`; a rocky planet's radius follows its mass as roughly R ∝ M^0.27, so radius →
circumference → `circumference / grid_width` gives kilometres per tile. At Earth mass on the
312-column grid that is **~128 km per tile** (`body_km_per_tile`, `src/world/logistics.hpp`).

**Travel time reuses the terrain weighting the pathfinder already computes.** `logistics_path::cost`
is terrain-weighted (plains ×1.0 … mountain ×2.0), so it is a count of *effective* tiles — and
terrain cost is already a time multiplier. No parallel table was needed:

```
days   = path.cost × km_per_tile ÷ km_per_day
ticks  = ceil(days ÷ 90)          # the economy clears quarterly
```

Two modes, differing by roughly five times, which is what makes coastal trade worth designing
(BL-188): **land ~25 km/day** (an ox-and-cart caravan) and **sea ~130 km/day** (a coasting vessel).
A short regional haul still lands in one quarter; a long one now takes several.

The space lane keeps its own ~1-tick-per-AU calibration, unchanged — it is the only leg the AU
model was ever right for, and it is parked with the space arc on `era/space`.

## Logistical cost

Each convoy incurs a budget outflow:

```
logistical_cost = base_logistics_cost × distance × cargo_qty
```

`base_logistics_cost` is a per-mode multiplier from the Lua economy-constants registry (`scripts/economy.lua`), ordered:

```
land < sea < air < space
```

For **space convoys**, `distance` is the Euclidean distance between the parent bodies' centres (no path routing — straight-line in the prototype). For **intra-body convoys** (land / sea), `distance` is the **terrain-weighted A* path** over the body's tile grid (BL-077, `src/world/logistics.{hpp,cpp}`): each tile weighted by its landform cost (TILES.md — plains 1.0 … mountain 2.0) and discounted by `road_level`, respecting the east–west cylinder wrap; the edge cost is the average of the two tiles (so the path is symmetric) and results cache per fixed endpoint pair. Ocean tiles carry a higher sea-leg cost, so the cheapest path prefers land and a water crossing selects **sea** mode.

The cost is charged in full at dispatch (`dispatch_convoys` debits `corp.balance` before the convoy is created; a corp that cannot afford the cheapest route dispatches nothing). It is the term that makes distant arbitrage marginal: a profitable inter-body trade requires `source_price + logistical_cost_per_unit < destination_price`.

**Logistics-node discount (BL-148 / BL-149).** The intra-body haul cost is further discounted for each **logistics node** the A* path crosses, so the world's cities — and the player's own hubs — form a cheap network the specialist corporation plugs into. A **population centre** on the path discounts by `logistics.node_discount.city_per_scale × centre.scale` (tier 1–5); an **Inland Logistics Hub** (BL-149) by a flat `logistics.node_discount.hub`. The summed discount is capped (`node_discount.cap`) so a route is never free, and is applied as `cost × (1 − discount)` (`dispatch_convoys`, over `logistics_path.tiles`). Since intra-body markets are city-seeded, most hauls deliver *into* a city and take the discount; the player extends the reach by placing hubs along a corridor. Deterministic — a pure function of the path tiles and the (population-centre / hub) node sets.

> **Implemented (BL-077 core 2026-07-08; BL-146–149 landed v0.1.1).** The intra-body A* distance and same-body dispatch (BL-077) are live: `dispatch_convoys` fills a same-body market shortfall from the corp's on-body pool, hauling from the corp's representative tile (its lowest-id building on the body) to the short market's `centre_tile` at the A* cost + mode above. The road lattice is generated at world-gen (BL-146) and the player extends it by placing roads (BL-147, `place_road` — a per-tile `road_level` raise that lowers the tile's A* cost); cities (BL-148) and the Inland Logistics Hub (BL-149) discount hauls per the node model above. The inter-body space path is unchanged.

---

## Dispatch trigger

**Auto-dispatch is the default.** On each economy Tick the system scans for destination shortfalls (demand exceeds local supply) and dispatches convoys from the cheapest reachable source to fill them. The loop runs without player intervention.

**Player-direction is the exception.** A player can direct a specific convoy — for example, fulfilling a standing sell order whose counterparty is on another body. This covers targeted arbitrage and order-book matching across bodies.

**Space launches auto-dispatch too (corrected 2026-07-31 — the "always player-directed" rule
written here was never built).** `dispatch_convoys` auto-dispatches inter-body convoys exactly
like intra-body ones, gated only on the corp holding a launchpad on the source body
(`corp_has_launchpad_on`). Whether leaving the gravity well *should* be an explicit player
decision is a design call still open against the shipped behaviour, not a description of it.

**Reachability.** In the prototype all bodies are treated as reachable (Exploration is a data-model stub). Infrastructure gates (below) are the operative constraint on reachability, not exploration state.

---

## Infrastructure gates

Mode is selected by the source/destination pair: inter-body → **space**; intra-body → **land** by default, **sea** when the cheapest A* path crosses ocean (`path.crosses_ocean` — the *path* picks the mode, not an infrastructure check). The designed endpoint gates are mostly **not built** (table corrected 2026-07-31):

| Mode | Designed gate | Code truth |
|---|---|---|
| **Land** | Ungated | Ungated — as designed |
| **Sea** | **Port** building at both endpoints | **No port check exists** — sea mode fires whenever the path crosses ocean. Port gating is owed to BL-188 (coastal ports / sea trade, design-owed) |
| **Air** | **Airfield** building at both endpoints | Air mode is never dispatched; no airfield building exists — deferred |
| **Space** | **Launchpad** at origin + **Orbital Port** at destination; Era 1 required | **Launchpad at origin only** (`corp_has_launchpad_on`). No orbital-port check, no era gate (the era system is unimplemented — ERAS.md banner) |

Note `supply_system.hpp`'s own header comment still claims sea/air "are not dispatched in the prototype" — it disagrees with the dispatch code beneath it; flagged for a code-comment fix, not edited here.

Roads are a land cost-reducer over a **three-tier ladder** (BL-172; BL-146/BL-147 shipped a two-tier local/trunk form). `road_level` is a real `tile_component` field (BL-077 core, default 0) that discounts the A* traversal cost of the tiles a route crosses (`road_traversal_multiplier` = `1 / (1 + 0.5·tier)`):

| Tier | `road_level` | Traversal ×  | Placed by |
|---|---|---|---|
| **Track** (minor / low-throughput) | 1 | ×0.67 | player, generation (spurs + border links) |
| **Road** (regular) | 2 | ×0.50 | player, generation |
| **Highway** (high-throughput backbone) | 3 | ×0.40 | player, generation (major-city backbone) |

"Throughput" here is *cost-discount*, not a capacity cap — per-node throughput remains out of prototype scope (below). The **generated road network** landed as **BL-146** and gained the third tier in **BL-172** (`src/world/road_generation.cpp`, `generate_roads`): after nations + population centres exist, each nation's centres are joined by an MST + relative-neighbour-redundancy backbone over terrain-weighted A* costs; each edge's tier is chosen from the two centres' scales — **Highway** between two major centres (population `scale ≥ 3`), **Road** when at least one endpoint is Town+ (`scale ≥ 2`), **Track** otherwise — rasterised along the A* path (ocean skipped), with one **Track** border link between the nearest centre pair of each territorially-adjacent nation. Generation measures the lanes road-free (to lay the network out), then clears `world.astar_cost_cache` so gameplay dispatch recomputes against the stamped roads. **Player placement** (BL-147 core, BL-172 tiers): the tile build front door offers all three tiers (`place_road(tile, tier)`, cost `economy.roads.{track,road,highway}`); placement is **upgrade-in-place** — valid when the chosen tier strictly exceeds the tile's current `road_level`, so a Track can be raised to a Highway but the same-or-lower tier is refused. The on-canvas render (PLANETARY.md) draws each roaded tile's own half of every shared edge, so a road **spans symmetrically** between the two tiles it joins with no "from vs to" asymmetry, weighted by tier. Determinism + connectivity + the 3-tier ceiling are pinned by `tools/verify/road_generation_harness.cpp`; placement + upgrade by `tools/verify/logistics_harness.cpp` (T10). A distinct **railroad** *mode* (not a road tier) is deferred to BL-173.

**River discount (BL-170, landed).** A river is generated as a directed **edge** across one of a
tile's 6 hex sides (never a tile-occupying feature — see `docs/generation/TILE_GENERATION.md`
§ Rivers), traced downhill from high ground to ocean/basin over Kepler's Pass-1 heightmap
(`src/world/river_generation.cpp`, `generate_rivers`). Where the intra-body A* path
(`src/world/logistics.cpp`) crosses a river-carrying edge, `river_edge_discount` applies a
further multiplier — **0.75× downstream, 0.85× upstream** — folded into the edge cost
**alongside**, and **multiplicatively with**, the road-tier discount above: a Highway that also
runs downstream compounds to `0.40 × 0.75 = 0.30`. No new resource or market good is involved;
this is a pure logistics-cost effect, sized to sit within the road ladder's scale (Track 0.67 …
Highway 0.40) rather than outstrip it — a river is a bonus lane, not a road-network replacement.
`tile_component::river_edges` / `river_downstream` (`src/world/components.hpp`) carry the per-side
bitmasks; `tile_borders_river` reads water-adjacency as a secondary consequence of the same
bitmask (consumed by a separate BL-166/168-style farming task, not wired here).

Per-node throughput capacity (a larger Port or Orbital Port carrying more cargo per Tick) is the natural infrastructure tuning lever but is out of prototype scope.

---

## Build status

**BL-039 (supply convoys) is complete** (status rewritten 2026-07-31 — this section previously
still read "designed but not yet built"). `src/world/supply_system.cpp` (~340 lines) is the
shipped layer.

**Landed:**
- Convoy component, per-Tick advance, destination pool crediting (the on-arrival market supply injection was removed by BL-382 — it was zeroed before pricing read it)
- Terrain-weighted A* intra-body routing with roads and the node discount (BL-077 planetary logistics; BL-146–149; BL-172 road tiers)
- River generation + logistics discount, stacking multiplicatively with the road ladder (BL-170)
- Auto-dispatch of shortfall-filling convoys, intra- **and** inter-body (launchpad-gated)
- Persistent trade-route recording (BL-088) + the proximity-glimpse peek (BL-099)

**Outstanding:**
- Per-node throughput capacity (out of prototype scope, § Infrastructure gates)
- Air mode (never dispatched; no airfield building)
- Port gating for sea mode (BL-188, coastal ports — design-owed)
- Player-directed dispatch (the § Dispatch trigger "exception" has no UI or code path yet)
