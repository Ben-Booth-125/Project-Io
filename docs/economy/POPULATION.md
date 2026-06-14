# Project Io — Population and Development

Population is the human layer of the economy — the source of workforce, the driver of consumer demand, and the reason habitability matters. Development is the act of improving a tile or region in ways that affect population, efficiency, or amenity rather than raw extraction. Both are deferred for the prototype but are designed here so that future implementation extends the existing model rather than replacing it.

---

## Population centres

A **population centre** is a cluster of inhabited tiles on a body, characterised by its population level and the type of development present. In the simulation model, a population centre:

- Is attached to one or more tiles on a body.
- Provides workforce to buildings within a defined catchment area (or to the body's whole workforce pool — the exact spatial model is open).
- Generates demand for food rations, consumer goods, and habitability goods each Tick.
- Has a habitability score that reflects its amenity, infrastructure, and input supply (food, clean water, utilities).

Population centres do not exist in the prototype. The prototype treats workforce as an authored constant (`workforce_assigned`) on each building. The population model is designed here to ensure that the workforce field, the habitability tile property, and the demand side of the market are all positioned correctly for the later implementation.

---

## Scale mechanics

Larger, better-developed population centres confer an agglomeration bonus on production: nearby buildings benefit from a denser labour market, better logistics, and shared infrastructure. This bonus scales with population level.

However, development also imposes a land-use constraint: **a developed tile cannot simultaneously be a resource extraction tile**. Building a housing district on a grassland tile trades away its agricultural produce deposit for population capacity. A factory district displaces what would otherwise be extraction. This creates a hard trade-off between extensive (raw material focus) and intensive (urban, high-value) development:

- **Extensive strategy:** keep extraction tiles undeveloped, maximise raw output, accept lower efficiency per worker.
- **Intensive strategy:** invest in urban development for scale bonuses and advanced product manufacturing, at the cost of reduced raw extraction capacity on those tiles.

The scale bonus and land-use constraint are the primary mechanism preventing a player from simultaneously maximising raw extraction and finished goods production in the same region — a deliberate design constraint, not an oversight.

### Scale bonus model (design target)

| Population level | Agglomeration bonus | Notes |
|-----------------|---------------------|-------|
| Outpost | — | No bonus; minimal presence |
| Settlement | +5% processing throughput | Small community |
| Town | +15% processing throughput, +5% extraction yield | Labour market forming |
| City | +30% processing throughput, +10% extraction yield | Full industrial economy |
| Metropolis | +50% processing throughput, +20% extraction yield | Benchmark for late-game Earth bodies |

Exact values are balance targets, not commitments. The tiers and the direction (scale confers advantage) are the design decisions.

---

## Population demand

Each Tick, a population centre consumes a basket of goods drawn from the local market. The basket size scales with population level; the composition is:

| Good | Role |
|------|------|
| Food rations | Basic subsistence; shortage reduces population growth rate |
| Clean water | Sanitation input; shortage reduces habitability |
| Consumer goods | Discretionary welfare; shortage reduces workforce efficiency |
| Habitability goods | Amenity and services; shortage caps population growth |

Demand is supplied from the body's market. If demand exceeds local supply, the deficit is met by imports (via convoys, Layer 5+). Persistent unmet demand reduces habitability, which reduces workforce efficiency, which propagates as a production penalty — the first indirect feedback loop in the economy.

---

## Land use

Each tile has a **land use state**: undeveloped, extraction, urban, amenity, or infrastructure.

| Land use | Description | Effect |
|----------|-------------|--------|
| Undeveloped | No buildings; tile deposit present but unharvested | Ambient amenity value from habitable compositions |
| Extraction | An extraction building occupies the tile | Deposit harvested; amenity reduced |
| Urban | Population development on the tile | Contributes to population centre size; deposit inaccessible |
| Amenity | Park, recreation, or green space designation | Habitability bonus; no extraction |
| Infrastructure | Port, launchpad, warehouse, etc. | Logistical function; no extraction |

Changing land use from undeveloped to urban or amenity permanently sacrifices the tile's extraction potential. Changing from extraction to urban requires the building to be demolished first. These transitions are deliberate and costly — there is no "undo for free."

---

## Habitability and workforce efficiency

Habitability is a tile property in the current data model (a float on `tile_component`). At the population level, habitability becomes a body-level aggregate: the weighted average of the habitability of all urban and amenity tiles that the population centre occupies.

Body habitability affects:
- **Population growth rate** — higher habitability → population grows faster.
- **Workforce efficiency** — a body with high habitability produces more effective workforce per head.
- **Recruitment** — military units drawn from a high-habitability body are cheaper to maintain (deferred, post-prototype).

Habitability is raised by:
- Urban development on naturally high-habitability compositions (grassland, forest).
- Amenity buildings (parks, healthcare, services).
- Supply of habitability goods (food rations, clean water, consumer goods).
- Research unlocks (deferred).

Habitability is reduced by:
- High-hazard extraction tiles in the vicinity.
- Undersupply of food, water, or consumer goods.
- Active conflict (deferred).

---

## Prototype notes

Population centres, land use transitions, and the agglomeration bonus are all **deferred** from the prototype. The following are in place and must not be retrofitted when population is implemented:

- `tile_component.habitability` — already exists; is the per-tile habitability ceiling.
- `building_component.workforce_assigned` — already exists; will be driven by the population model.
- `market_component.demand` — already exists; will reflect population consumption once the population model is live.

The tile `land_use` field does not yet exist in `tile_component`. It should be added when population is implemented.
