# Project Io — Population and Development

Population is the human layer of the economy — the source of workforce, the driver of consumer demand, and the reason habitability matters. Development is the act of improving a tile or region in ways that affect population, efficiency, or amenity rather than raw extraction. Population centres are **first implemented in v0.0.6 as a static MVP** (seeded fixed-level centres producing workforce supply, demand, and the agglomeration bonus); the **dynamic half** (habitability feedback and growth) follows as a v0.0.6 follow-up. The full model is designed here so each implementation step extends it rather than replacing it.

---

## Population centres

A **population centre** is a cluster of inhabited tiles on a body, characterised by its population level and the type of development present. In the simulation model, a population centre:

- Is attached to one or more tiles on a body.
- Provides workforce to buildings within a defined catchment area (or to the body's whole workforce pool — the exact spatial model is open).
- Generates demand for food rations, consumer goods, and habitability goods each Tick.
- Has a habitability score that reflects its amenity, infrastructure, and input supply (food, clean water, utilities).

## Generation

Population centres are **placed at campaign start by a deterministic generation pass**, run
after nation and corporation generation (it reads finished territory and tile data; see
`docs/generation/GENERATION_STRATEGY.md`). Nations own the broad economy, so the population
that staffs it is seeded onto **their** territory:

- **One or more centres per nation**, placed on **habitability-clustered** tiles — the pass
  prefers high-`tile_component.habitability` compositions (grassland, forest) and grows a centre
  outward across contiguous habitable tiles within the nation.
- **Level is derived, not authored** — a centre's tier (Outpost → Metropolis) follows from the
  size and mean habitability of its tile cluster, so fertile, large territories carry cities and
  marginal ones carry outposts. Deterministic from the campaign seed.
- The pass sets each occupied tile's **`land_use`** to `urban` (or `amenity`), which removes its
  deposit from extraction per § Land use — population and extraction compete for the same tiles
  from turn one.

The exact tier thresholds and per-nation centre counts are tuning targets fixed at promotion;
the decisions here are *nation-seeded, habitability-clustered, level-derived*.

## Centre rendering (implemented 2026-07-04)

The BL-083 presentation layer draws the generated centres on the Planetary canvas
(`src/ui/body_surface_canvas.cpp`) as **always-on civic chrome** — not lens-gated — so the
surface reads as inhabited rather than "resources with industry on top":

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
  are stable per campaign. Deriving names from the host nation is a noted refinement.

---

Until v0.0.6, the prototype treats workforce as an authored constant (`workforce_assigned`) on each building. The population model is designed here to ensure that the workforce field, the habitability tile property, and the demand side of the market are all positioned correctly for the implementation.

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

## Workforce model (prototype → Layer 4)

The prototype ships a placeholder: `building_component.workforce_assigned` is an authored
constant in `[0, 1]`, read-only, applied as a single linear scalar at both the extraction
and processing stages (see PRODUCTION.md § Workforce). This section settles the **real
model** so Layer 4 building management exposes a coherent system rather than a constant —
and so the data model positions for it now. It is design only; the implementation is the
**Workforce pool & population coupling** Brief (OPENS § Workforce).

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

In the prototype, supply is a fixed authored figure per `(corp, body)`; once population
centres exist (the rest of this document), **supply derives from the population centres on
the body**: population level → labour force → the share that contracts to the corporation.

### Contention

When **demand ≤ supply**, every building is fully staffed and runs at its requested level.
When **demand > supply**, the pool is **rationed proportionally**: each building receives
`supply / demand` of its request (a single contention scalar applied uniformly), so a
corporation that over-builds relative to its labour force sees *every* building throttled
rather than some starved to zero. This proportional rule is the workforce counterpart of
the two-threshold input model in PRODUCTION.md, and is deliberately simple — priority
weighting between buildings is a later refinement, noted but not committed here.

The contention scalar multiplies the existing linear `workforce_assigned` term, so the
production arithmetic gains a factor rather than changing shape:
`effective_workforce = workforce_assigned × contention_scalar`.

### Player-set vs. system-allocated

The split, stated once:

- **The player sets** the *target* staffing of each building — the `workforce_assigned`
  request (how hard they want it run) — and, later, standing allocation **policy** (which
  buildings get priority under contention; SYSTEMS.md § Policy).
- **The system allocates** the actual labour: it computes pool supply from population,
  sums demand from the player's targets, derives the contention scalar, and applies it.
  The player never hand-assigns headcount; they express intent and the pool resolves it.

### Wages

Wages are paid from the pool's **effective** (allocated) workforce, not the requested
target — a throttled building pays for the labour it actually used. The per-building wage
stays `effective_workforce × base_wage` (the L3 budget term), so the budget loop is
unchanged in shape; only the workforce figure feeding it becomes pool-resolved. Wage
*level* derives from body habitability and population pressure once population is live
(higher demand for scarce labour raises the clearing wage); in the prototype `base_wage`
is the authored constant it is today.

### Upgrade path from the authored constant

The migration is additive, so nothing in Layer 3 is retrofitted destructively:

1. **L3 (today):** `workforce_assigned` authored per building; `contention_scalar = 1`
   implicitly (no pool). The field stays.
2. **L4 step 1 — pool without population:** introduce the per-`(corp, body)` pool with an
   authored supply; compute demand and the contention scalar; feed `effective_workforce`
   into production and wages. `workforce_assigned` becomes the player-set *target*.
3. **L4 step 2 — population coupling:** replace the authored pool supply with one derived
   from population centres (level → labour force → contracted share), and let wage level
   track habitability/population pressure.

The data already in place that this path reuses (and must not be retrofitted): the
`workforce_assigned` field, `market_component.demand`, and `tile_component.habitability`
(see Prototype notes below).

## Implementation decomposition (v0.0.6)

The [S4] Population-centres Brief decomposes into the foundation-first sequence below. The
**static MVP is Briefs 1–5** (seeded fixed-level centres that produce workforce supply, demand,
and the agglomeration bonus); **Briefs 6–7 (the dynamic half — habitability feedback and growth)
are a v0.0.6 follow-up**, deferred from the first pass so the load-bearing L4 workforce/demand
grounding lands without building the whole feedback economy at once.

| # | Brief | Depends on | MVP |
|---|-------|-----------|-----|
| 1 | **Land-use foundation** — add `land_use` enum + field to `tile_component`; transition rules (extraction occupies; urban/amenity displaces deposit) through `placement_rules`. | — | ✓ |
| 2 | **Population-centre model + generation** — the centre entity (attached tiles, derived level tier) and the nation-seeded, habitability-clustered generation pass (§ Generation). | 1 | ✓ |
| 3 | **Population demand** — per-Tick basket (food / water / consumer / habitability goods) consumed from the body market, feeding `market_component.demand`. | 2 | ✓ |
| 4 | **Workforce supply derivation** — population level → labour force → contracted corp share. *This is [A4] § Workforce step 2* — it stays its own Brief there, grounded by this work. | 2 | ✓ |
| 5 | **Agglomeration / scale bonus** — centre level → production bonus (processing throughput + extraction yield), per § Scale bonus model. | 2 | ✓ |
| 6 | **Habitability aggregate + feedback** — body habitability from urban/amenity tiles → workforce efficiency (and the growth input for 7). | 3 | follow-up |
| 7 | **Population growth** — habitability / met-demand → level change over Ticks. | 6 | follow-up |

Briefs 3, 4, and 5 are **disjoint dependents of 2** (different systems: market demand, workforce
pool, production bonus) — parallel-safe once the model exists. 1 → 2 is the serial foundation.

## Prototype notes

Population centres, land use transitions, and the agglomeration bonus are all **deferred** from the prototype. The following are in place and must not be retrofitted when population is implemented:

- `tile_component.habitability` — already exists; is the per-tile habitability ceiling.
- `building_component.workforce_assigned` — already exists; will be driven by the population model.
- `market_component.demand` — already exists; will reflect population consumption once the population model is live.

The tile `land_use` field does not yet exist in `tile_component`. It should be added when population is implemented.
