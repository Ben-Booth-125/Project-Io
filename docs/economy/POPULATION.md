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
- **Count and scale derive from Era −1 region demography** (Ben, 2026-08-25; BL-610, centres
  from demography). Density is history's consequence: the simulated regions' populations
  (§ Region demography) decide how many centres a body carries and how large each is, replacing
  the land-area divisor and the authored weighted scale draw. `k_population_for_scale` =
  10 / 50 / 200 / 1,000 / 5,000 thousand heads remains the scale→headcount mapping.
  The carve is a pure integer function of the region populations, no RNG: an urban share
  (a tenth, `k_demography_urban_share_q`) of each living region's headcount towns; the
  **count** is each region's urban headcount over one village's-worth
  (`k_demography_heads_per_centre` = `k_population_for_scale[0]`), floored at one — a razed
  region contributes nothing; the **scales** are a rank-size share-out of the body's whole
  urban headcount, banded to the nearest `k_population_for_scale` rung in log space — a few
  cities over many towns over a train of villages, real settlement concentration as mechanism,
  never a name. A body with no settlement record keeps a land-area fallback.
- **Every province is anchored by a centre** (Ben, 2026-08-25; BL-611, province centre anchor).
  A centre of *any* scale — most are small; towns stand where history earned them. The anchor
  is the province's political decider: the centre's nation is the province's nation, and taking
  the centre takes the province (`docs/generation/PROVINCES.md` § The partition; BL-567,
  province is the conquest unit). This retires the centre-less hinterland province. The
  guarantee is structural: after the partition ships, any land province the centre-seeded fill
  left without one receives a **scale-1 anchor founding** on its best ground
  (`ensure_province_anchor_centres` — argmax of habitability × richness, the placement gate
  preferred and relaxed only where no tile passes it, counted rather than hidden).
- **Urban ground is stamped at generation** (Ben, 2026-08-25; BL-612, urban ground stamped).
  A centre arrives with an urban land-use footprint scaled by its tier, so city ground is
  scarce and contested from turn one rather than notionally open (§ Land use). The footprint
  is the centre's own tile plus its most-livable land neighbours — 1/1/2/4/7 tiles by scale
  (`k_urban_footprint_tiles`); a footprint the coast cuts short stays short, and a tile two
  cities share is stamped once. Extraction already standing is grandfathered
  (`docs/economy/TILES.md` § Urban transform).

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

### Strata gate buildings

The scale ladder doubles as the **stratum ladder** (Ben, 2026-08-25; BL-615, stratum placement
gates): certain buildings are only placeable **in** a centre, and some only in a centre of a
minimum stratum. A **university** requires City (4)+; a schooling building any centre; heavy
processors (the steel-mill class) must sit **near** a population centre rather than in open
country. The gate is a placement rule (`placement_rules::can_place` is the seam), not a recipe
property — it is about where the workforce lives, not what the building does.

The gate is **authored data on the building definition**, never a building-name switch: a
`placement_gate` of three fields — `requires_centre` (must stand on a centre's tile),
`min_centre_scale` (the hosting centre's minimum stratum, 1–5), `centre_proximity_radius` (must
stand within this grid distance of some centre; the heavy-processor field, carried per recipe
since a recipe is the named-building identity for processing). Each axis refuses with its own
reason code (`needs_centre` / `centre_too_small` / `far_from_centre`), so the build door can
teach which condition is unmet rather than greying the row. The proximity radius is a balance
target, not a commitment; the ladder positions (university at City+) are the design decision.

### Growth, decline and razing

A centre **promotes up the ladder when preconditions are met** (Ben, 2026-08-25; BL-616, centre
promotion and decline) — sustained met supply, habitability, and population above the next
tier's threshold. Since centres anchor provinces, promotion changes the political map's value
during play.

Decline is asymmetric by design: **passive failure only shrinks a centre — it never destroys
one.** Outright destruction is a deliberate agent action (razing, in occupation), and it should
be rare because the occupier almost always prefers to occupy. A shrunk centre keeps its urban
ground — a fading town reads as historied, not deleted.

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

**Demand ladders with scale** (Ben, 2026-08-25). The basket's *composition*, not only its size,
follows the stratum: higher strata consume up the value chain — a city pulls consumer goods, a
metropolis electronics — so big centres are demand **endpoints** that give goods value from day
one, and the markets worth reaching. What is deliberately **unquantified**: the cost of living —
how much it costs a head to live, and which goods each stratum consumes in what proportion
(§ Open items).

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

## Qualification

**Workforce has a skill axis** (Ben, 2026-08-25; BL-613, qualification fraction). Each
**nation** carries a `qualification` fraction — the share of its workforce that is qualified.
Nation-grain deliberately: it reads as a national development level, feeds road generation, and
avoids per-centre bookkeeping the prototype does not need.

- **Raised by schooling buildings and universities.** A schooling building lifts the host
  nation's fraction; a **university** (City+ only, § Strata gate buildings) lifts it further
  *and* produces research points (RP — `docs/economy/RESEARCH.md`).
- **Consumed by complicated methods.** A building running a complicated production method
  requires that many **qualified** workers from the pool alongside ordinary labour; the
  qualified pool is the scarcer one, and is what gates organically scaling building levels —
  a deeper facility costs qualified heads, not only credits.
- **Seeded from history.** A region's Era −1 industrialisation timing — the same scalar that
  sets corp focus (`docs/generation/CORPORATION_GENERATION.md` § Pass 2) — aggregates into the
  nation's opening fraction: early industrialisers open qualified, late ones raw.
- **Scales generated infrastructure.** A low-qualification nation generates fewer, lower-tier
  roads (`docs/economy/LOGISTICS.md` § Roads; BL-618, roads scale with qualification).
- **Moves with people.** Migration carries qualification — brain drain is real (§ Migration).

## Migration

**Population moves between centres, and between friendly nations** (Ben, 2026-08-25; BL-617,
population migration). Growth is no longer purely local: each tick a deterministic, seeded flow
moves heads from low- toward high-attractiveness centres, attractiveness read from habitability
and the clearing wage. Between nations the flow is **stance-gated** (`docs/politics/RELATIONS.md`):
friendly nations encourage it, hostile ones close it.

Migrants **carry qualification with them** — an emigrating qualified worker debits the origin
nation's fraction and credits the destination's. Brain drain is therefore a real strategic
weapon: high habitability and high wages drain a rival nation's qualified labour. Deterministic
and replayable like every flow — no RNG in the gate path.

The strategic consequence, stated once: money stops being the only scarce input to growth.
Qualified labour, habitability, and province-anchoring centres are each a **non-purchasable
constraint**, and migration is the only lever that moves one of them.

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
When **demand > supply**, labour clears by **wage competition** (Ben, 2026-08-25; BL-614, wage
competition): scarce labour goes to the buildings offering the higher wage, in deterministic
order (wage, then building id), rather than being rationed proportionally. A corporation that
over-builds relative to its labour force must outbid itself and its neighbours, so labour
scarcity is priced instead of silently averaged. The proportional `supply / demand` scalar it
supersedes remains the right mental model for the *fully-uncontended* case — everyone staffed at
request — and building counts lean on available land (§ Land use, the province ceiling), not on
the pool alone.

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
is `effective_workforce × base_wage × (1 + wage_bid)` (the budget term, FINANCE.md) — paid
**at the offered rate** (BL-614, wage competition): a building that outbid its siblings for
scarce labour pays the premium it offered. `wage_bid` is the first-cut wage dial — a
per-building premium fraction (`building_component.wage_bid`, default 0, data-only, no UI;
NR-629 flags the shape for overturn). `base_wage` is an authored constant; wage *level*
tracks body habitability and population pressure (higher demand for scarce labour raises
the clearing wage), and the unit wage reference that anchors it is BL-544 (unit wage
reference).

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
is the source both the centre **count** and the **scale distribution** draw from (§ Generation;
BL-610, centres from demography) — density is a consequence of the simulated history, not a
divisor or a weighted draw. It also aggregates into each nation's opening qualification
fraction (§ Qualification).

---

## Open items

- **Cost of living is unquantified** (Ben, 2026-08-25). Neither how much it costs a head to
  live, nor which goods each stratum consumes in what proportion, has numbers. § Population
  demand states the direction (demand ladders with scale); the quantification — basket
  contents per stratum, and the wage a head needs to afford it — is open, and couples to
  BL-544 (unit wage reference).
- **Wage-clearing detail.** BL-614 (wage competition)'s first cut answers both of its own
  questions provisionally, flagged for overturn (NR-629): the dial is **per building**
  (`wage_bid`, § Wages), and the qualified pool clears **by the same wage rule, before** the
  ordinary pool, a building's factor being the product of the two grants. Whether the dial
  should instead be per body, or a derived clearing wage, stays open on the item.
