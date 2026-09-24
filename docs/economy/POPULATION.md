# Project Io — Population and Development

> **Settles:** what a population centre is, how it is placed, and how it grows, declines or is
> razed · what a centre consumes and what its habitability rests on · where labour supply comes
> from and how contention over it resolves · what wages are paid and what development does to a
> region.
> **Not here:** what a building does with the labour it is allocated (PRODUCTION) · what the
> goods a centre consumes cost (MARKETS) · what the wage bill does to the balance (FINANCE) ·
> the body-level strain and hazard behind habitability (../CLIMATE.md) · the ground the centre
> stands on (TILES).
> **Confused with:** PRODUCTION.md, ../CLIMATE.md, TILES.md.

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

**The population map is drawn EARLY, and history then grows and destroys it** (Ben, the
eight-phase generation reorder, 2026-09-03; BL-766, population map early). The map is drawn
over the settled regions **before** the Era −1 sim runs, weighted toward ground that farms
easily, and the sim then grows the cities, sacks them and razes them as it goes. Centres are
still the history's **consequence** — the goal BL-610 (centres from demography) set — but now
because history grew and sacked them rather than because they were placed once it had
finished, and the sim no longer runs over a world with no cities in it.

The record is kept at **sim grain**, three integers on the `region` (`centres`,
`centres_razed`, `urban_population`), not as entities: the Era −1 sim has no ECS access by
design, so a city it can grow and sack cannot be a `population_centre_component` while it
runs. `generate_population_centres` materialises the campaign-era entities from that record
once the sim has finished. Three rules govern it, and they are pure integer functions with no
RNG anywhere on the path:

- **The draw.** A region whose ground clears a farming floor receives an opening urban
  headcount — a share of its people that itself rises with how easily the ground farms, so
  easy-farming country towns a larger fraction of itself — and a settlement to stand them in.
  The same rule applies at **every** founding, including the ones the sim makes mid-era, so a
  frontier region settled in year 300 gets its town on the same terms as an ancient core.
- **Growth only promotes.** Each simulated year a region's urban headcount converges a
  fraction of the way toward its target, and centres are promoted as the heads cross a rung.
  A shrinking city keeps its centre — the same asymmetry § Growth, decline and razing states
  for play: passive failure shrinks a centre and never destroys one.
- **The sack destroys.** A conquest costs the taken region's cities a multiple of what it
  costs its countryside, because a sack falls on the walls and not the fields. Centres fall to
  what the surviving heads can stand up, and every one lost is recorded in `centres_razed` —
  so a razed city that is later rebuilt still says it was razed. Razing stays **rare**, as
  § Growth, decline and razing requires: an occupier almost always prefers to occupy.

- **A centre stands in the region that grew it.** Each carved centre is **bound to its source
  region** and placed inside that region's own cell of the settlement partition
  (`nearest_region` — the same partition city naming already reads, so a centre's name and its
  ground now agree by construction rather than by luck). A region whose cell is built out
  spills to its nearest neighbour rather than losing the settlement.

  Without the binding the causal chain died at its last step: history grew and sacked *specific*
  regions, and an undifferentiated body-wide scatter then threw that away, so a player could not
  find the war behind a ruin. **Measured** (BL-783): centres displaced from their source region
  fall from 56.3% to 17.9%, and every remaining displacement is a region wanting more cities
  than it has ground — not a placement that ignored it.

- **The campaign placement path consumes no randomness.** Within the region it is a pure argmax
  over habitability, tie-broken by lowest raster index — count, scale *and* place are all the
  demography's consequence and nothing else's. The seeded weighted draw survives only on the
  no-settlement fallback, where there is no region record to be a consequence of.

- **The candidate weighting is habitability-gated, clustered, and pulled toward farmland.** A
  candidate tile must pass the placement rules' habitability gate; among candidates, a tile
  adjacent to an existing centre carries 3× weight, multiplied by a 1–5 richness bucket and a
  1–3 **food** bucket on the tile's own agricultural deposit, so centres cluster progressively,
  a rich tile can outweigh a merely adjacent one, and cities stand on ground that feeds them.
  The food bucket is deliberately narrower than richness: it tilts placement toward farmland
  without overturning the deposit pull. It is the tile-grain half of the weighting the early
  urban map applies at region grain.

- **Two later passes are deliberately region-blind**, and it is worth knowing which:
  `ensure_province_anchor_centres` and `ensure_national_population_centres` add coverage seats
  that answer to the province and nation layers, not to the demography. They are not bound and
  should not be — a measurement of the binding has to exclude them or it reads their coverage
  as the carve's failure.
- **Count and scale derive from Era −1 region demography** (Ben, 2026-08-25; BL-610, centres
  from demography). Density is history's consequence: the simulated regions decide how many
  centres a body carries and how large each is, replacing the land-area divisor and the
  authored weighted scale draw. `k_population_for_scale` = 10 / 50 / 200 / 1,000 / 5,000
  thousand heads remains the scale→headcount mapping. The carve is a pure integer function of
  the region record, no RNG: the **count** is the sum of the living regions' own `centres` —
  the settlements the era drew, grew and left standing, so a razed region contributes nothing
  and a sacked one contributes fewer; the **scales** are a rank-size share-out of the body's
  whole urban headcount, banded to the nearest `k_population_for_scale` rung in log space — a
  few cities over many towns over a train of villages, real settlement concentration as
  mechanism, never a name. A body whose urban map was never drawn falls back to the flat urban
  share of population the carve used before it, and a body with no settlement record at all
  keeps the land-area fallback.
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
  tier), in the civic-neutral `palette::settlement` colour under every lens; ownership is read
  off the national border band (`docs/ui/PLANETARY.md` § The national border band), so the host
  nation's tint has no live gate now that the Country lens is retired. Tier is carried by glyph
  size, keeping colour out of the ownership vocabulary (see `docs/ui/ICONS.md`).
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
be rare because the occupier almost always prefers to occupy.

**A razed settlement is a TIER, not an erasure** (Ben, 2026-08-25; BL-624, razed settlement
tier). Razing demotes the centre to the **razed** state: population zeroed, no labour, no
demand, no agglomeration — but the entity, its name, its tile, and its urban ground all
persist. Two consequences are the point: the province **keeps its capture anchor** (a ruin can
still be taken, so razing never deletes a conquest handle — the province-settlement invariant
survives play), and **rebuilding there is cheap** — the urban ground and the entity already
exist, so the ordinary growth pass re-settles a razed centre at a reduced gate rather than
anyone founding from nothing. A shrunk or razed centre reads as historied, not deleted. Razing
is a `corp_verb` under the ordinary command seam; no rival-AI grant covers it, so only a human
presses it today.

---

## Population demand

Each Tick, a population centre consumes a basket of goods drawn from the local market. The basket size scales with population level; the composition is:

| Good | Role |
|------|------|
| Food rations | Basic subsistence; shortage reduces population growth rate |
| Clean water | Sanitation input; shortage reduces habitability |
| Consumer goods | Discretionary welfare; shortage reduces workforce efficiency |
| Habitability goods | Amenity and services; shortage caps population growth |

Demand is supplied from the centre's catchment market. If demand exceeds local supply, the deficit is met by imports (via convoys, `SUPPLY.md`). Persistent unmet demand reduces habitability, which reduces workforce efficiency, which propagates as a production penalty — the first indirect feedback loop in the economy.

**The whole basket is one per-centre signal.** Each centre bids its basket into its catchment
market every tick, price-elastically (`inject_population_demand`, `MARKETS.md` § The clearing
tick). Clean water, consumer goods and medical supplies are tradeable `resource_type` values
(`RESOURCES.md` § Habitability goods); no nation-level substrate basket stands beside it.

**Demand ladders with scale** (Ben, 2026-08-25). The basket's *composition*, not only its size,
follows the stratum: higher strata consume up the value chain — a city pulls consumer goods, a
metropolis electronics — so big centres are demand **endpoints** that give goods value from day
one, and the markets worth reaching.

### The stratum ladder — size, rungs, wealth, promotion (Ben, 2026-09-15)

Four rulings, taken on one form, make the direction above a mechanism.

**1. A centre's appetite follows its HEADCOUNT, renormalised.** A metropolis of five million is
not five villages. Basket volume is `heads × basket[r] × elasticity`, divided by one derived
constant — **heads per demand unit** — so the world's total household demand at generation equals
what the tier index produced. The rank-size rungs (10k / 50k / 200k / 1M / 5M) then give a
metropolis 500× a village's appetite instead of 5×, and big cities become the markets worth a long
haul. The constant is **derived, not picked**: the mean heads per scale point over the generated
centre distribution (on the generation weights 40/30/20/8/2 that is 239k heads over 2.02 scale
points, about 118k heads a unit), recorded with its derivation and re-derived whenever the rungs
or the distribution move. It is held fixed in play, so a growing city grows its market.

**2. The rungs are CUMULATIVE.** Each stratum keeps everything below it and adds a rung; nothing is
substituted away. Industrial band:

| Stratum | Adds |
|---|---|
| 1 | Staples — food rations, agricultural produce, water |
| 2 | Clean water, medical supplies |
| 3 | Consumer goods |
| 4 | Refined fuel |
| 5 | Electronics |

Rung volumes are measured against the recipe-margin anchor, not authored to taste. The ancient
band composes the same way over its own terminal goods (PROPOSED: charcoal and timber at 2, cloth
and ceramics at 3, planks and leather at 4, dressed stone at 5). Era decides the chain; stratum
decides how far up it a centre reaches.

**3. Qualification scales the UPPER rungs.** A nation's qualification fraction (§ Qualification) —
already seeded from how early it industrialised — multiplies the volume of rungs 4 and 5,
normalised to the world mean at generation. A qualified nation's cities buy more fuel and
electronics than an equally large city in a nation that industrialised late. Staples and welfare
goods do not scale with it: a head eats whatever the nation's history. This is the campaign's half
of *wealthier nations leverage their wealth into production* (`../generation/INDUSTRIALISATION.md` § 2),
read from a slow, seeded quantity rather than from balances that swing every tick.

**4. An unmet top rung BLOCKS PROMOTION.** A centre promotes only while its own highest rung is met
— the sustained-met-supply precondition of § Growth, decline and razing, made specific. Lower rungs
keep their habitability effects (`RESOURCES.md` § Habitability goods). So supplying a city's top
rung is how a city grows, and a player who wants a metropolis in their catchment has a good to
bring it.

**Cultural weight composes on top** (`MARKETS.md` § Three properties the set has to hold, property
5 as amended): stratum decides which goods and how much; the culture of the heads weights each.

**And it ladders by ERA as well as by stratum (BL-640).** The basket is masked by era band exactly
as recipes are (BL-433's `era` field), and the omission was load-bearing: a basket authored in
industrial goods left an ancient campaign wanting three things, because the other half of it names
goods nothing in that band can produce. **An ancient household wants ceramics, cloth, leather and
dressed stone; an industrial one wants clean water, consumer goods and medical supplies.** The two
ladders compose — era decides *which* value chain, stratum decides *how far up it* — and together
they are what makes the household channel the sink for terminal artisan goods, which is what those
goods were authored to be. See [`MARKETS.md`](MARKETS.md) § Demand channels for the register this
belongs to.

What remains deliberately **unquantified**: the cost of living — how much it costs a head to live,
and which goods each stratum consumes in what proportion (§ Open items).

### Housing — a centre keeps its dwellings standing (Ben, 2026-09-24)

**A population centre holds a housing stock, and keeping it standing costs goods.** The stock is
sized to the centre's population; it wears at an authored rate every tick, and making good that
wear is an **upkeep draw of building materials** the centre bids for in its catchment market. A
centre that grows must also add stock, so growth itself is a construction demand. This is the
consumer MARKETS.md § The eight channels names in the Construction row — "centres as they grow" —
given a mechanism: a centre is a builder that never finishes.

**The basket is era-banded**, as the household basket is: the ancient band maintains its houses in
timber, stone, clay and planks; the industrial band in steel, dressed stone and ceramics beside
them. It is authored in data per band, so a band's basket names only goods that band makes. Steel
is the reason this exists: an industrial world whose iron ore and steel had almost no genuine buyer
(MARKETS.md § Three properties, the industrial census) gains a terminal sink that scales with how
many people live there.

**It bids, and it scales with the world** (MARKETS.md properties 1, 3 and 4). The want is
registered on the catchment market's demand whether or not the shelf can meet it, so an unmet
housing need prices its materials and calls forth the makers — a draw that could not do that
would only shut the economy down. It scales with population, so housing demand grows as the world
does rather than decaying into a fixed basket.

**Unmet upkeep shows, and only shrinks** (§ Growth, decline and razing; the systemic-force rule).
A centre whose housing is not kept up carries a visible **housing condition** below full; that
condition caps the centre's growth and lowers its habitability, and it recovers as supply returns.
It never destroys a centre and never zeroes it — passive failure only shrinks. A shortage has a
cause the player can see, read and answer by supplying the market; it is never a hidden term.

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

What labour a body's population centres yield, how it is shared between corporations, who wins it
when it is scarce, and what it is paid.

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

**Supply** derives from the body's population centres (BL-042, workforce supply derivation):
each centre contributes labour by scale — `labour_by_scale` = 1 / 3 / 10 / 30 / 100 units for
scale 1–5. A corp's share of that body supply is its share of the building count there; a body
with no centres falls back to the authored `world::workforce_supply` figure (default 3.0).

### Contention

**Contention** clears by **wage competition** (Ben, 2026-08-25; BL-614, wage competition). Uncontended
(`demand ≤ supply`), every building is staffed at request. Contended, scarce labour allocates **per building** — offered wage
descending, building id ascending on a tie, each building granted up to its demand until the
pool is spent — so the marginal building runs partial and those below it idle, superseding the
old uniform proportional scalar. The offered wage is `base_wage × (1 + wage_bid)`
(`building_component.wage_bid`, a per-building premium fraction — the first-cut dial, data-only,
no UI yet; NR-629 flags the shape for overturn). The pool aggregate `min(1, supply/demand)`
survives in `economy_report.workforce_contention` as the report figure; the per-building grant
is `economy_report.building_labour`. A recipe's **qualified** requirement (§ Qualification,
BL-613) clears against its national pool by the same rule, before the ordinary pool; a building's factor is the product of the two grants. Every grant is then multiplied by
`workforce_efficiency(hab)` (`src/world/workforce.hpp`, BL-069 workforce efficiency): full
labour at habitability ≥ 0.6, ramping linearly to 0.5× at 0. Effective workforce =
`workforce_assigned × grant`. A corporation that over-builds relative to its labour force must
outbid itself and its neighbours, so labour scarcity is priced instead of silently averaged. The
player never hand-assigns headcount; they express intent and the pool resolves it. Building counts
lean on available land (§ Land use, the province ceiling), not on the pool alone.

### Wages

Wages are paid from the pool's **effective** (allocated) workforce, not the requested
target — a throttled building pays for the labour it actually used — and **at the offered
rate** (BL-614, wage competition): a building that outbid its siblings for scarce labour pays
the premium it offered. The expression itself is [`FINANCE.md`](FINANCE.md) § Building
operating cost. `wage_bid` is the first-cut wage dial — a per-building premium fraction (`building_component.wage_bid`, default 0, data-only, no UI;
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

Region demography is self-contained at the region level, and carries the **urban record**
alongside it (§ Generation): the cities a region holds, the heads living in them, and the
count history has destroyed there. On graduation to the campaign era that record is the source
both the centre **count** and the **scale distribution** draw from (§ Generation; BL-610,
centres from demography; BL-766, population map early) — density is a consequence of the
simulated history, not a divisor or a weighted draw. It also aggregates into each nation's opening qualification
fraction (§ Qualification).

---

## Open items

- **The wage a head needs is unquantified** (Ben, 2026-08-25). § The stratum ladder settles which
  goods each stratum consumes and how volume scales; the rung volumes are measured, and what a
  head must earn to afford its basket stays open, coupled to BL-544 (unit wage reference).
- **Whether households buy power.** Power is a bought good for buildings (`PRODUCTION.md`
  § Power); a household rung for it is natural and unasked.
- **Wage-clearing detail.** BL-614 (wage competition)'s first cut answers both of its own
  questions provisionally, flagged for overturn (NR-629): the dial is **per building**
  (`wage_bid`, § Wages), and the qualified pool clears **by the same wage rule, before** the
  ordinary pool, a building's factor being the product of the two grants. Whether the dial
  should instead be per body, or a derived clearing wage, stays open on the item.
