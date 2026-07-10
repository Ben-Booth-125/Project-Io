# Project Io — Supply (Layer 5)

Layer 5 of the economy is the **logistics / convoy layer** — the mechanism that physically moves goods between markets and bodies, coupling otherwise-isolated price pools through cargo movement. A convoy is the unit of flow; there is no abstract price-coupling term between bodies: the convoy *is* the coupling. Full design authority for this layer is BL-039 (`[S5]`) in `docs/development/BACKLOG.md` § Supply; the build is v0.0.7's whole theme.

---

## Convoy entity

A convoy is a world ECS component. Each active convoy carries:

| Field | Type / values | Notes |
|---|---|---|
| `source_market` | market entity | The market the cargo was dispatched from |
| `destination_market` | market entity | The market the cargo is being delivered to |
| `mode` | `{land, sea, air, space}` | Determines infrastructure gate and cost multiplier |
| `cargo_resource` | resource enum | The good being transported |
| `cargo_qty` | quantity | Units in transit |
| `progress` | `0.0–1.0` | Fraction of route completed |
| `speed` | progress/Tick | Fixed linear advance per economy Tick |

The coupling is **market-to-market**, not body-to-body. A convoy is created when goods are dispatched toward a destination shortfall. It advances `progress` by `speed` each Tick (linear; no orbital mechanics in the prototype). On arrival (`progress >= 1.0`) it credits the destination `(corp, body)` pool and market supply, then is retired.

Cargo leaves the source pool at **dispatch**, not arrival. Goods in transit are committed — the source pool shrinks immediately when a convoy departs.

**Trade-route recording (BL-088).** Before retiring an arrived convoy, `credit_arrived_convoys` (`src/world/supply_system.cpp`) also upserts a persistent `trade_route` into `world.trade_routes` — keyed on the unordered `(body_a, body_b)` pair + `corp`, with `last_tick` set to the completion Tick and `convoy_count` incremented. Intra-body lanes (source and destination collapse to the same body) are excluded — they light nothing. A route is never erased once recorded; staleness is a **read-time** concern owned by the activity fog, not a write-time one here. See `docs/ui/DISCOVERY.md` (BL-089) for the fog that reads this substrate.

---

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

The cost is charged at dispatch (or amortised per Tick — the exact timing is a build decision). It is the term that makes distant arbitrage marginal: a profitable inter-body trade requires `source_price + logistical_cost_per_unit < destination_price`.

> **Implemented (BL-077 economic core, 2026-07-08).** The intra-body A* distance above and same-body dispatch are live. `dispatch_convoys` now fills a same-body market shortfall from the corp's on-body pool, hauling from the corp's representative tile (its lowest-id building on the body) to the short market's `centre_tile` at the A* cost + mode above — previously same-body pairs were skipped ("resolved by production"). The inter-body space path is unchanged. Road-network generation + player placement (which populate `road_level`), cities-as-hubs, and the Inland Logistics Hub follow as BL-146–BL-149.

---

## Dispatch trigger

**Auto-dispatch is the default.** On each economy Tick the system scans for destination shortfalls (demand exceeds local supply) and dispatches convoys from the cheapest reachable source to fill them. The loop runs without player intervention.

**Player-direction is the exception.** A player can direct a specific convoy — for example, fulfilling a standing sell order whose counterparty is on another body. This covers targeted arbitrage and order-book matching across bodies.

**Space launches are always player-directed.** Leaving the gravity well is an explicit decision, never auto-dispatched — in Era 0 and Era 1 alike. Terrestrial (land / sea / air) convoys auto-dispatch.

**Reachability.** In the prototype all bodies are treated as reachable (Exploration is a data-model stub). Infrastructure gates (below) are the operative constraint on reachability, not exploration state.

---

## Infrastructure gates

Each convoy mode is gated on endpoint infrastructure. Mode is selected by the source/destination pair: inter-body → **space**; intra-body → **land** by default, **sea** when the route must cross water.

| Mode | Gate | Status |
|---|---|---|
| **Land** | Ungated — available across contiguous land with no built prerequisite | Prototype default for intra-body |
| **Sea** | **Port** building at both endpoints | Port is in the Era 0 building set |
| **Air** | **Airfield** building at both endpoints | Designed; building not in the prototype set — deferred |
| **Space** | **Launchpad** at the origin + **Orbital Port** at the destination; **Era 1 required** | Era gate already enforced by `docs/economy/ERAS.md` |

Roads are a land cost-reducer: `road_level` is a real `tile_component` field (BL-077 core, default 0) that discounts the A* traversal cost of the tiles a route crosses (`road_traversal_multiplier` = `1 / (1 + 0.5·tier)`, so tier 1 → ×0.67, tier 2 → ×0.50). The **generated road network** that populates it landed as **BL-146** (`src/world/road_generation.cpp`, `generate_roads`): after nations + population centres exist, each nation's centres are joined by an MST + relative-neighbour-redundancy backbone over terrain-weighted A* costs — **trunk** (tier 2) between major centres (population `scale ≥ 3`), **local** (tier 1) elsewhere — rasterised along the A* path (ocean skipped), with one local **border link** between the nearest centre pair of each territorially-adjacent nation. Generation measures the lanes road-free (to lay the network out), then clears `world.astar_cost_cache` so gameplay dispatch recomputes against the stamped roads. **Player road placement** (extending the lattice to remote sites) remains the BL-147 follow-on. Determinism + connectivity are pinned by `tools/verify/road_generation_harness.cpp`.

Per-node throughput capacity (a larger Port or Orbital Port carrying more cargo per Tick) is the natural infrastructure tuning lever but is out of prototype scope.

---

## Build status

This layer is **designed but not yet built**. BL-039 (`[S5]`) is v0.0.7's whole theme — the largest remaining v0.1.0 build. The decomposition at promotion (foundation-first) is:

1. Convoy component + per-Tick advance
2. Logistical-cost budget term + economy-constants entries
3. Dispatch triggers (auto, then player-directed)
4. Destination crediting + inter-body market effect
5. Supply lens render passes (disjoint from 1–4)

Files touched at build time: `src/world/supply_system.{hpp,cpp}` (new), `src/world/components.hpp` (convoy component), `src/world/budget_system.cpp` (cost), `scripts/economy.lua` (per-mode constants), `src/world/market_clearing.{hpp,cpp}` (delivery).
