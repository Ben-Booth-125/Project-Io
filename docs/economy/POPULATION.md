# Project Io — Population and Development

Population is the human layer of the economy — the source of workforce, the driver of consumer demand, and the reason habitability matters. Development is the act of improving a tile or region in ways that affect population, efficiency, or amenity rather than raw extraction. Population centres produce workforce supply and demand, carry a habitability feedback, and grow; the full model is designed here so each implementation step extends it rather than replacing it.

---

## Population centres

A **population centre** is a cluster of inhabited tiles on a body, characterised by its population level and the type of development present. In the simulation model, a population centre:

- Is attached to one or more tiles on a body.
- Provides workforce to the body's `(corp, body)` labour pools (§ Workforce model).
- Generates demand for food rations, consumer goods, and habitability goods each Tick.
- Has a habitability score that reflects its amenity, infrastructure, and input supply (food, clean water, utilities).

## Generation

Population centres are **placed at campaign start by a deterministic generation pass**,
`generate_population_centres` (`src/world/population_generation.cpp`). It runs **before**
nation generation and drives it: the Era −1 settlement ladder (`docs/lore/HISTORY.md`,
`src/world/settlement.{hpp,cpp}`) counts and grows regions from the centres, and
`generate_nations` reads the result, so the political map is a consequence of where people
settled rather than the other way round.

- **Placement is habitability-gated and clustered.** A candidate tile must pass the placement
  rules' habitability gate; among candidates, a tile adjacent to an existing centre carries 3×
  weight, multiplied by a 1–5 richness bucket, so centres cluster progressively and a rich tile
  can outweigh a merely adjacent one.
- **Centre count derives from land area, not grid area** (BL-463, centre count from land): one
  centre per `k_land_tiles_per_centre` land tiles (a measured divisor, ~410), bounded below by
  one and above by the tiles able to host one, so a seed with more land is more settled.
- **Scale (1–5, Outpost → Metropolis) is drawn from a weighted distribution** — 40 / 30 / 20 /
  8 / 2 % — with `k_population_for_scale` = 10 / 50 / 200 / 1,000 / 5,000 thousand heads.
  Deterministic from the campaign seed.

A centre's tile keeps its full deposit: population and extraction compete for a tile through
§ Land use, not through generation.

## Centre rendering

The presentation layer (BL-083, population rendering) draws the generated centres on the
Planetary canvas (`src/ui/body_surface_canvas.cpp`) as **always-on civic chrome** — not
lens-gated — so the surface reads as inhabited rather than "resources with industry on top":

- **Conurbation clustering** — contiguous `population_centres` are clustered transitively at
  Chebyshev grid distance ≤ 3 (cylinder-wrapped east–west in columns), so the map shows a
  handful of legible cities and towns rather than a dust of villages. Display-only; the
  simulation entities are untouched.
- **Anchor and tier** — each conurbation is anchored at its highest-scale member and takes
  that member's scale as its tier.
- **Marker** — the tiered `icons::settlement` skyline glyph (tower count and height grow with
  tier), in the civic-neutral `palette::settlement` colour; the host nation's tint applies
  only under the Country lens. Tier is carried by glyph size, keeping colour out of the
  ownership vocabulary (see `docs/ui/ICONS.md`).
- **Labels** — only City-and-above conurbations (tier ≥ 4) are labelled, named
  deterministically from the anchor tile id against a fixed settlement name bank, so labels
  are stable per campaign. Names are sci-fi / fantasy, never Earth-drawn (standing rule).

---

## Scale mechanics

Larger, better-developed population centres confer an agglomeration bonus on production: nearby buildings benefit from a denser labour market, better logistics, and shared infrastructure. This bonus scales with population level.

However, development also imposes a land-use constraint: **a developed tile cannot simultaneously be a resource extraction tile**. Building a housing district on a grassland tile trades away its agricultural produce deposit for population capacity. A factory district displaces what would otherwise be extraction. This creates a hard trade-off between extensive (raw material focus) and intensive (urban, high-value) development:

- **Extensive strategy:** keep extraction tiles undeveloped, maximise raw output, accept lower efficiency per worker.
- **Intensive strategy:** invest in urban development for scale bonuses and advanced product manufacturing, at the cost of reduced raw extraction capacity on those tiles.

The scale bonus and land-use constraint are the primary mechanism preventing a player from simultaneously maximising raw extraction and finished goods production in the same region — a deliberate design constraint, not an oversight.

### Scale bonus model

| Population level | Agglomeration bonus | Notes |
|-----------------|---------------------|-------|
| Outpost | — | No bonus; minimal presence |
| Settlement | +5% processing throughput | Small community |
| Town | +15% processing throughput, +5% extraction yield | Labour market forming |
| City | +30% processing throughput, +10% extraction yield | Full industrial economy |
| Metropolis | +50% processing throughput, +20% extraction yield | Benchmark for late-game Earth bodies |

Exact values are balance targets, not commitments. The tiers and the direction (scale confers advantage) are the design decisions. Scale also sets a centre's labour contribution (§ The labour pool) and its logistics-node discount (SUPPLY.md § Logistical cost).

---

## Population demand

Each Tick, a population centre consumes a basket of goods drawn from the local market. The basket size scales with population level; the composition is:

| Good | Role |
|------|------|
| Food rations | Basic subsistence; shortage reduces population growth rate |
| Clean water | Sanitation input; shortage reduces habitability |
| Consumer goods | Discretionary welfare; shortage reduces workforce efficiency |
| Habitability goods | Amenity and services; shortage caps population growth |

Demand is supplied from the body's market. If demand exceeds local supply, the deficit is met by imports (via convoys, Layer 5). Persistent unmet demand reduces habitability, which reduces workforce efficiency, which propagates as a production penalty — the first indirect feedback loop in the economy.

Two demand signals carry this. A centre's **own** market demand is one unit of `agricultural_produce` per scale level per tick — the subsistence half of the basket, the only half with a `resource_type` value (RESOURCES.md § Prototype scope: clean water, consumer goods and habitability goods are a resource *category*, not enumerators). The broader consumption signal is the **nation-substrate basket** (`scripts/economy.lua` § `substrate`), which is nation-level rather than per-centre.

---

## Land use

Each tile has a **land use state**: undeveloped, extraction, urban, amenity, or infrastructure (`land_use_component`, `components.hpp`).

| Land use | Description | Effect |
|----------|-------------|--------|
| Undeveloped | No buildings; tile deposit present but unharvested | Ambient amenity value from habitable ground |
| Extraction | An extraction building occupies the tile | Deposit harvested; amenity reduced |
| Urban | Population development on the tile | Contributes to population centre size; deposit inaccessible |
| Amenity | Park, recreation, or green space designation | Habitability bonus; no extraction |
| Infrastructure | Port, launchpad, warehouse, etc. | Logistical function; no extraction |

Changing land use from undeveloped to urban or amenity permanently sacrifices the tile's extraction potential. Changing from extraction to urban requires the building to be demolished first. These transitions are deliberate and costly — there is no "undo for free."

The urban **cover** transform (TILES.md § Urban transform) is the tile-axis expression of the same rule: a tile whose non-extraction stack fills its cap is built over and takes no new extraction, while extraction already standing is grandfathered.

---

## Habitability and workforce efficiency

Habitability is a tile property (a float on `tile_component`, derived at generation — TILES.md § Data model). At the population level, habitability becomes a body-level aggregate: the scale-weighted mean habitability over the centres' tiles.

Body habitability affects:
- **Population growth rate** — higher habitability → population grows faster.
- **Workforce efficiency** — a body with high habitability produces more effective workforce per head.
- **Recruitment** — military units drawn from a high-habitability body are cheaper to maintain.

Habitability is raised by:
- Urban development on naturally high-habitability ground (grass, forest cover on sedimentary).
- Amenity buildings (parks, healthcare, services).
- Supply of habitability goods (food rations, clean water, consumer goods).
- Research unlocks.

Habitability is reduced by:
- High-hazard extraction tiles in the vicinity.
- Undersupply of food, water, or consumer goods.
- Active conflict.

**The feedback is live in `run_economy_step`** (`src/world/economy_system.cpp`): the scale-weighted body habitability mean drives a workforce-efficiency multiplier (BL-069, habitability efficiency), and centre growth is gated on met supply (BL-048B / BL-078, population growth).

---

## Workforce model

The per-`(corp, body)` pool with contention and the population-derived supply feeding it are specified in PRODUCTION.md § Workforce model. `building_component.workforce_assigned` is an authored constant in `[0, 1]` — the *request* the contention scalar throttles.

### The labour pool

Workforce is a **pool**, not a per-building free parameter. The pool is held **per `(corp,
body)`** — the same granularity as the stockpile pool — because labour does not cross
bodies without transport, and a corporation's contention is local to where its buildings
sit. (A corporation-wide pool was considered and rejected: it would let a labour surplus on
one body silently staff buildings on another, which the spatial economy must not allow.)
Each pool has:

- a **supply** — the total effective workforce available to that corporation on that body,
  and
- a **demand** — the sum of the labour its buildings on that body want this Tick.

Supply **derives from the population centres on the body** (BL-042, workforce supply
derivation): centre scale → labour units (1 / 3 / 10 / 30 / 100 for scale 1–5), with a
corp's share set by its share of the body's building count. The fixed authored figure
(`world::workforce_supply`, default 3.0) survives only as the fallback for bodies with no
centres.

### Contention

When **demand ≤ supply**, every building is fully staffed and runs at its requested level.
When **demand > supply**, the pool is **rationed proportionally**: each building receives
`supply / demand` of its request (a single contention scalar applied uniformly), so a
corporation that over-builds relative to its labour force sees *every* building throttled
rather than some starved to zero. This proportional rule is the workforce counterpart of
the two-threshold input model in PRODUCTION.md, and is deliberately simple — priority
weighting between buildings is standing allocation **policy** (SYSTEMS.md § Policy), a
separate layer over the pool.

The contention scalar multiplies the existing linear `workforce_assigned` term, so the
production arithmetic gains a factor rather than changing shape:
`effective_workforce = workforce_assigned × contention_scalar`.

### Player-set vs. system-allocated

The split, stated once — the player lever is **not** `workforce_assigned`:

- **The player sets** the *target* staffing of each building via
  `building_component.workforce_target` (0–200 % of nominal), which by default is
  **auto-solved** each tick to maximise the building's profit (BL-181, workforce auto-solver);
  a manual choice pins it, opting out. `workforce_assigned` is an authored constant set at
  placement (0.5 producing, 0 passive) and is never player-edited.
- **The system allocates** the actual labour: it computes pool supply from population,
  sums demand from the assigned requests, derives the contention scalar, and applies it.
  The player never hand-assigns headcount; they express intent and the pool resolves it.

### Wages

Wages are paid from the pool's **effective** (allocated) workforce, not the requested
target — a throttled building pays for the labour it actually used. The per-building wage
is `effective_workforce × base_wage` (the budget term, FINANCE.md). `base_wage` is an
authored constant; wage *level* tracks body habitability and population pressure (higher
demand for scarce labour raises the clearing wage), and the unit wage reference that
anchors it is BL-544 (unit wage reference).

The data the pool model reuses, and must not retrofit: the `workforce_assigned` field,
`market_component.demand`, and `tile_component.habitability`.

---

## Region demography

Above the centre, the Era −1 ladder carries **region-level demography** (BL-273, region
demography): `src/world/settlement.{hpp,cpp}`'s `region` struct carries `population`,
`last_demography_year` and `manpower_stock`, driven by `advance_region_demography`
(integer-fixed-point logistic growth toward `region_carrying_capacity(farm_q)`, plus
war-pressure drawdown), `resolve_plague_event` (a checkpoint-eligibility draw reusing
`resolve_checkpoint` from `planetology.hpp`, per BL-217's reuse rule — severity uses a
grid-proximity connectivity proxy rather than the full logistics/trade graph, a noted
simplification), and the manpower budget triple (`manpower_ceiling` / `replenish_manpower` /
`raise_manpower` — a bounded fraction of population that depletes, self-limiting rather than
capped by fiat). Every rate is a `_q` thousandths quantity — no floats in a gate path.
Verified by `tools/verify/demography_harness.cpp`.

Region demography is self-contained at the region level. On graduation to the campaign era it
is the source the population-centre scale distribution draws from, replacing the weighted
random draw in § Generation with a consequence of the simulated history.
