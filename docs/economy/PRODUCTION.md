# Project Io — Production

Production converts tile resource deposits into tradeable goods through two stages: **extraction**, which harvests raw materials from tiles, and **processing**, which refines or manufactures higher-tier goods from those inputs. Workforce shapes throughput at both stages.

See **`docs/economy/RESOURCES.md`** for the full resource list, tier definitions, and prototype subset. The market model production sells into — clearing, price resolution, the order book — is **`docs/economy/MARKETS.md`**. The network goods move over — reach, roads, travel time, throughput — is **`docs/economy/LOGISTICS.md`**.

**The admission rule.** A `resource_type` value earns its place by being consumed by an authored recipe or contracted for by a named actor, and nothing else gets in. A raw with an authored deposit and no price is *minable but unsellable* — a processing building drawing on it stalls forever — so every deposit raw carries a base price, and every priced good has a producer and a consumer reachable in the same era band. `tools/verify/chain_depth.cpp` (rows R1 and R1b) holds the line.

**A consumer is a mechanism, not a noun (BL-648).** The rule above has one loophole and it has
already been walked through: `chain_depth`'s exemption table lets a good pass by *naming* a
consumer — "sold to the market", "mercantile demand" — and a name is not a pass that injects
demand. Ten goods were admitted on a mercantile demand that was never built, which is how the
ancient roster came to terminate in artisan goods nobody buys. The rule is therefore sharpened:
**an exemption must name a pass that actually adds to a market's demand or draws from a pool**, and
the row fails when it cannot find one. The register of legitimate passes is
[`MARKETS.md`](MARKETS.md) § Demand channels.

---

## Extraction

An extraction building placed on a tile reads the tile's deposit for its authored target resource (`building_component.target_resource`) and credits a fractional quantity to its corporation's stockpile pool **at each economy tick**.

Output rate is the product of four factors:

| Factor | Source | Effect |
|--------|--------|--------|
| Deposit richness | `tile_component.resource_deposit[r]` | Linear multiplier; richer tiles produce proportionally faster. |
| Workforce fraction | `building_component.workforce_assigned` (0–1) | Linear scalar. Zero workforce produces nothing. |
| Hazard penalty | `tile_component.hazard_level` (0–1) | Applied as `(1 − hazard)` multiplier. High-hazard tiles cost more to operate and yield less per worker. |
| Stack rank | This site's place in the tile's stack | Applied as `0.8^(k−1)`. The first site on a deposit is undiminished; each later one yields less (§ Building stacks). |

i.e. `output = base_rate × richness × workforce × (1 − hazard) × 0.8^(k−1)`, where `base_rate` is a Lua-authored economic constant and `k` is the site's 1-based rank in its tile's stack.

Deposits **deplete** (BL-079, deposit depletion): `resource_deposit[r]` is the fixed **richness** (the rate multiplier above), while `resource_remaining[r]` is a finite reserve — seeded at generation to richness × a reserve factor — that extraction draws down each tick. As the reserve nears empty the output **tapers**, then the building reports the deposit **exhausted** and stops. Richness sets the rate; the reserve sets how long the tile pays out — the boom-bust arc a resource-dependent corporation rides.

A tile can carry **several** extraction sites on one deposit. What that costs, and what it
buys, is § Building stacks below — a fourth factor (stack rank) on the output above, and a
depletion taper shared across the whole stack rather than computed per site.

### Building stacks

A tile is not a single build slot: several extraction sites may work the same deposit. Two rules
govern the stack, and they are designed to pull against each other. The design is BL-193
(building stacks, Ben's ruling of 2026-07-26).

**1. Diminishing per-site output.** The *k*-th site of a stack — counted in **stored order,
oldest first** — produces

```
base_rate × richness × workforce × (1 − hazard) × d^(k−1),    d = 0.8
```

so the second site adds 0.8 of a lone site, the third 0.64, and so on:

| Sites | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| That site's rate | 1.0 | 0.8 | 0.64 | 0.512 | 0.4096 |
| Stack total | 1.0× | **1.8×** | 2.44× | 2.952× | **3.36×** |

Five sites never make 5×. **There are no clamps and no fake caps under this — the curve is the
economics.**

**2. Honest shared depletion.** Every site draws **real reserve** for exactly what it extracts,
so a full stack drains the deposit *faster than its output multiple*: five sites take 3.36× the
goods while removing 3.36× the reserve, on a taper band sized for one. The depletion taper is
therefore computed against the stack's **combined nominal draw**, not each site's own — every
member sees the same taper and reports `exhausted` on the **same tick**, instead of the stack
desynchronising and dribbling out of a spent tile one site at a time.

Together these make stack-vs-spread a genuine trade-off rather than a dominant strategy:
stacking buys throughput now at the cost of the tile's life; spreading buys life at the cost of
transport and land.

**Rank is build order.** A site's *k* is its position among the tile's sites of that type and
target, ascending entity id — creation order. Decommissioned and still-building sites hold
their rank (they are the same population the placement ceiling counts) but contribute nothing to
the combined draw. The ordered walk is `placement_rules::stack_members`; reading rank off
`world::buildings`' own unordered iteration would make the yield non-deterministic.

**Capacity is `max(1, richness / 50)`** — the 0–250 generation band maps to 1–5 sites
(`k_richness_per_site`, `placement_rules.hpp`). With the decay curve carrying the economics, the
ceiling is a **legibility** bound — how many markers a tile can host and a player can reason
about — not the balance lever.

**Non-extraction stacking** (BL-366, multi-building tiles) is bounded in **aggregate** — every
non-extraction type on one tile combined, not per type — by a per-composition cap table
(`non_extraction_stack_cap`, `placement_rules.cpp`; the table and rationale live in
`docs/economy/TILES.md` § Urban transform). Filling the cap fires a one-way transform to the
`urban` terrain composition, which raises the cap further (12) and blocks new extraction/ambient
placement on that tile. Extraction stacking (`k_richness_per_site`, above) is a separate,
richness-bound axis.

Implementation: the constants and the rank/curve helpers live in
`src/world/placement_rules.{hpp,cpp}` (`k_stack_output_decay`, `stack_output_scalar`,
`stack_members`, `stack_rank`); the combined-nominal taper is a per-tile **stack pre-pass** in
`run_economy_step` (`src/world/economy_system.cpp`), sized before any site draws so the sites
that run first do not taper the sites that run after them. The Selection panel's "On this tile"
block reports the count, the ceiling, this site's rank and its share. Extraction stacking is
verified by `tools/verify/stack_capacity_harness.cpp`; the non-extraction cap and urban
transform by `tools/verify/multi_building_tile_harness.cpp`.

### The province building ceiling

A **second, independent limit**, at province grain (BL-513, province building ceiling). It does
not replace the stack caps above — both must pass — because the two answer different questions:

| Limit | Grain | Asks | Answers from |
|---|---|---|---|
| Stack capacity (§ above) | tile | may this **deposit** support another site? | deposit richness |
| **Province ceiling** | **province** | how much can this **land** sustain? | area + infrastructure + habitability + population |

**It is type-agnostic, by ruling.** Ben, 2026-08-21: *"Building limits do not determine between
different types of building. So it does not matter if you can build 60 buildings, whether they are
5 of one type and 5 of another, or 10 of one type."* It bounds the **total** standing in the
province — extraction sites included, since they occupy land like anything else — and says nothing
about the mix. That is what makes it a statement about land rather than a balance dial on
building types.

**The shape**, with each of the four inputs in its natural role:

```
sustain_units = pop_factor × Σ over the province's land tiles of
                                habitability_t × (1 + road_level_t / 3)

ceiling       = max(1, round(k × sustain_units)),   k = 12.6468
```

* **Area** is the number of terms in the sum — a bigger province sustains more by being bigger.
* **Habitability** is each tile's weight. Land nobody can live on sustains nothing.
* **Infrastructure** is the per-tile road multiplier, spanning exactly `[1, 2]` across the road
  ladder's **own** domain (0 = none … 3 = Highway). Structural, not tuned.
* **Population** is the province-level multiplier, `1 + Σ(centre scale) / 5`, again over the
  scale's own domain (1 = village … 5 = metropolis).

Every band is read off a domain the codebase already defines, which leaves exactly **one** free
coefficient — `k` — and it is **pinned by measurement**. It is set so the ceiling's world total
matches the capacity the world already grants under the pooled per-tile cap: the ceiling is a
*redistribution* of existing capacity onto the four sustaining inputs, not a tighter or looser
regime. Across 8 seeds, `pooled per-tile cap ÷ sustain units` measures **11.4694 … 15.2588,
aggregate 12.6468** (spread 28.36% of the mean); `k` is that aggregate, and at it the ceilings
total **100.01%** of the pooled per-tile capacity.

**At generation density it refuses nothing, and that is the expected outcome.** Measured
post-background-firms across 8 seeds: 1049 buildings against 909,245 ceiling slots — **0.115%
used**, and **zero** provinces at their ceiling on any seed. Its value is forward: it is the
ceiling that makes "pack more productivity into a smaller space" a real trade-off once density
rises, and the natural place for infrastructure to pay off into.

**It is computed on demand, never cached, and adds no persistent field** — the flat-binary
save/load path is untouched by it. Because roads are built during play, this ceiling **moves
during play** — the one placement bound in the game that is not fixed at generation. Whether
that dynamism is intended is an open question for Ben (NR-406); computing on demand keeps either
answer cheap.

Implementation: `measure_province_sustain` / `province_building_ceiling` /
`province_buildings_standing` in `src/world/province.{hpp,cpp}` (the constants and the pinning
evidence live beside them in the header); the gate is the last check in
`placement_rules::can_place_in_world`, refusing with `placement_reason::province_full`.
Generation does not go through that entry point — it uses `can_place` directly — so generation
output is unaffected by it. Measured by `tools/verify/province_capacity_probe.cpp`, which prints
the ceiling alongside the capacity and re-derives the pin on every run.

### Extraction buildings

> **Design vocabulary, not enum values.** The named building types below are **not** in the
> `building_type` enum (`src/world/components.hpp`), which has eight values: `none`,
> `extraction_site`, `processing_facility`, `port`, `launchpad`, `inland_logistics_hub`,
> `military_base`, `research_institute`. There is one **generic** extraction building: what an
> `extraction_site` *does* is its `building_component.target_resource` field, authored at
> placement — a "Mine" and an "Oil Platform" are the same type pointed at different deposits. The
> table is the design vocabulary the named types would carve out of that generic building.

Each building type targets a specific class of resource. Placement is valid only on tiles with a non-zero deposit of the target type, or on terrain where that deposit type can occur.

| Building | Target resources | Valid terrain | Era |
|----------|-----------------|---------------|-----|
| Mine | Iron ore, coal, silica, copper ore, rare earth ore, peat | Barren, rocky, volcanic substrates; scrub-on-sedimentary for peat | 0 |
| Oil Platform | Petroleum | Barren (geological deposit) | 0 |
| Quarry | Stone, sand, clay | All non-water compositions | 0 |
| Lumber Camp | Timber | Forest or marsh cover | 0 |
| Farm | Agricultural produce | Grass, marsh or forest cover on sedimentary substrate | 0 |
| Ice Extractor | Water (from ice deposits) | Icy | 1 |
| Surface Extractor | Regolith, iron-nickel ore, platinum group metals | Regolith, metallic | 1 |
| Fishing Wharf | Agricultural produce | Coastal (any composition adjacent to ocean) | 0 |

**Fishing Wharf** (BL-168, fishing wharf). The extraction_site's target resource is again
`agricultural_produce`, but the placement gate is coastal adjacency rather than a deposit: valid on
any tile with an ocean neighbour, deposit-agnostic. A tile can satisfy Farm's deposit rule, the
Fishing Wharf's coastal rule, both, or neither — they are two independent ways the same generic
extraction_site can reach agricultural_produce, not two building types (`placement_rules.cpp`
`can_place` / `can_place_in_world`, mirroring the Port coastal check via `is_coastal`).

The Mine covers all terrestrial hard-mineral deposits and adjusts its output to whatever the tile holds: the same building type on one volcanic tile yields rare earth ore and on another yields copper ore. The distinction between deposit types is in the tile data, not the building type. Off-world metallic deposits (iron-nickel ore, platinum group metals) are harvested by the Surface Extractor, the Era 1 airless-body counterpart to the Mine; both feed the same smelting chain, so the distinction is one of era and deployment environment, not of downstream product.

The Quarry and Lumber Camp exist specifically to harvest ambient resources (stone, timber, sand, clay) that are present at low levels on most tiles. They ensure every tile can be productive in some capacity, even if only as a local construction material source. They share the same workforce and hazard scalar model as other extraction buildings.

---

## Processing

A processing building holds a **recipe** (`building_component.recipe`, indexing the Lua recipe registry): a set of input resources consumed and a set of outputs produced per economy tick, with a fixed conversion rate authored in Lua. Recipes support multiple inputs and outputs and reagents (e.g. coal in the steel recipe is an input that yields no separate product).

Processing buildings draw inputs from — and add outputs to — the shared per-`(corporation, body)` stockpile pool (inputs are taken pool-first; any shortfall draws the local market's real inventory, see § Stockpile and output flow and MARKETS.md § Real market inventory). They use the same workforce scalar as extraction buildings.

When the inputs available cannot cover a full conversion, the building does **not** simply halt: it follows a **two-threshold partial run**. If the limiting input covers at least `T_full` of one conversion, the building runs at full rate; between `T_idle` and `T_full` it scales its output down to what the limiting input allows; below `T_idle` it idles. The two thresholds are tunable economic constants.

**The thresholds govern every body uniformly.** A market's stock is real and finite
(`market_component.inventory`, MARKETS.md), so it earns its place in the SAME coverage calculation
the no-market case uses: coverage = `(pool + market inventory) / need` per input, full rate
at/above `T_full`, scaled between `T_idle`/`T_full`, idle below `T_idle`. There is one model, not
a market special case.

### Recipes by building type

Every recipe runs on the one generic `processing_facility`; the named processor types below are
the design vocabulary for its **sub-facility groups** (§ Sub-facility groups), and the recipes
are authored in `scripts/recipes.lua`.

A building type may support multiple recipes. The active recipe for a given building is configured by the player (or defaulted at construction). Multiple buildings of the same type on the same body can run different recipes simultaneously.

#### Smelter

| Inputs | Output | Era |
|--------|--------|-----|
| Iron ore ×2 + coal ×1 (reagent) | Steel | 0 |
| Iron-nickel ore ×2 | Steel | 1 |

Coal is consumed as a process fuel and reagent but does not appear as a separate intermediate product. The iron-nickel recipe requires no carbon addition because metallic asteroids are already reduced.

#### Refinery

| Inputs | Output | Era |
|--------|--------|-----|
| Petroleum | Refined fuel | 0 |
| Silica | Silicon | 0 |
| Copper ore | Refined copper | 0 |
| Rare earth ore | REE alloy | 0 |

#### Chemical Plant

| Inputs | Output | Era |
|--------|--------|-----|
| Refined fuel ×2 (oxidiser separated from the local air — no stockpiled input) | Propellant | 0 |
| Water ×3 + refined fuel ×1 (oxidiser by electrolysis) | Propellant | 1 |

The two routes are `propellant_atmospheric` and `propellant_electrolysis` (BL-308, propellant).
Liquid oxygen has no `resource_type`: it is folded into each recipe, because nothing outside the
Chemical Plant would ever hold it.

On a body with an atmosphere, liquid oxygen is produced in Era 1 by cryogenic air separation — the Chemical Plant draws oxygen from the local atmosphere and consumes no stockpiled input (energy cost only, abstracted into the recipe rate). Propellant is therefore an Era 1 capability anywhere refined fuel is available. On airless bodies there is no atmosphere to separate, so the water-electrolysis recipe is the only liquid-oxygen route off-world; closing the in-situ propellant loop there (water → liquid oxygen, refined fuel shipped or synthesised) is the defining Era 2 logistical problem.

#### Electronics Lab

| Inputs | Output | Era |
|--------|--------|-----|
| Silicon + refined copper + REE alloy ×0.5 | Electronics | 0 |

#### Fabricator

| Inputs | Output | Era |
|--------|--------|-----|
| Steel + refined copper | Machinery | 0 |
| Steel + REE alloy | Alloys | 0 |
| Steel + machinery | **Ordnance** | 0 |

The ordnance route is the roster's **military terminal good** (BL-457, ordnance) — see
`RESOURCES.md` § The two terminal goods for why the tier ends in two places. Three notes that
belong here rather than there:

- It sits at the **same depth as alloys**, one step below the Assembly Plant, deliberately. That
  is a statement about the roster's shape, not a lock: depth does not gate, so what keeps ordnance
  off a starting corp's tick-one menu is the availability of its inputs, not a refusal.
- It is `machinery`'s **second** consumer, after the heavy spacecraft route below. A single
  consumer is one revert away from orphaning a good.
- Its `base_price` (140.8 industrial, 113.0 ancient) is **derived**, not authored — what its
  cheapest route needs under § The recipe margin anchor, one value per band — and it should be
  re-derived rather than re-guessed if either input's price moves.

There is **no industrial ancient-arc shortcut here** — the deliberate omission stands. The
ancient arc reaches ordnance through the **Smithy** (§ The ancient chain), because unit upkeep
draws ordnance every tick in either arc, and a corp whose campaign runs the ancient band must be
able to make the one good its own army needs.

#### Assembly Plant

| Inputs | Output | Era |
|--------|--------|-----|
| Alloys ×2 + electronics | Spacecraft components | 1 |
| Machinery ×2 + steel ×2 | Spacecraft components | 1 |

The second route is the **heavy** one (NR-257): more structural mass and worked machinery, none
of the refined-electronics chain. It is a *supply route* rather than an alternate method in
§ Alternate production methods' sense — its inputs are disjoint from the first's, so which one a
corp runs is decided by what its industry already reaches, not by comparing two prices at one
building. It is also `machinery`'s first consumer.

#### In-situ and premium routes

Two further routes exist to give `regolith` and `platinum_group_metals` consumers (NR-257). Both
are deliberately **poor value per unit** — they are about reaching a *place*, not about
efficiency.

At authored prices the three industrial steel routes clear over inputs, per unit of steel:
iron-nickel **7.6**, Smelter **6.6**, in-situ **5.1**. That ordering is the design, and it is why
the regolith ratio and regolith's `base_price` (1.0) cannot be tuned independently of each other —
see the note on the recipe in `scripts/recipes.lua`.

| Inputs | Output | Era | Why it exists |
|--------|--------|-----|---------------|
| Regolith ×8.5 | Steel | 1 | In-situ reduction on an airless body. Eight and a half regolith per steel against the Smelter's two iron ore: regolith is on every tile of every airless body, so the point is that you can build **from where you are**. |
| Platinum group metals ×0.5 | Electronics | 1 | Contact-grade/catalytic route. At base price 40 this is a premium alternative to the silicon + copper + REE chain, not a cheap bypass of it. |

#### Food Processor

| Inputs | Output | Era |
|--------|--------|-----|
| Agricultural produce ×2 | Food rations | any |

#### Hydroponics Bay

| Inputs | Output | Era |
|--------|--------|-----|
| Water ×1.5 + steel ×0.5 | Agricultural produce ×5 | 0 |

A processing_facility recipe (`hydroponics_bay`, BL-166) that produces `agricultural_produce`
from refined inputs instead of a terrain deposit — no "energy" resource exists in the roster, so
water (irrigation) and steel (the bay's own structure) stand in for it. Feeds the same Food
Processor → Food rations chain the Farm feeds; no new market good. **Placement is terrain-gated
the opposite way from the Farm:** only valid where the terrestrial Farm deposit was NOT seeded
(`resource_deposit[agricultural_produce] == 0`), keyed off the processing_facility's target
resource in `placement_rules::can_place` (mirror image of the extraction deposit check;
`deposit_present` is the rejection reason on Farm-viable terrain).

---

## The recipe margin anchor (Ben, 2026-09-02)

**Every recipe pays at base price.** Ben's sentence: *"the simplest way to do this is to ensure
that all recipes (at base price) make a greater profit than marginal costs."* A recipe that cannot
clear its own marginal cost at base price is unprofitable **by authoring** — no demand channel,
band or scorer can rescue it — so this is the anchor every other economy lever sits on. It is a
statement about the three authored tables (`scripts/recipes.lua`, `scripts/economy.lua`,
`scripts/world_gen.lua`'s `base_price`), checked at **authoring time** and never against live
resolved prices, which is BL-740's (maintenance floor anchor) discipline applied to the whole roster.

**Two halves, because base is the middle of a band and not the price.** Resolved prices run from
`floor_mult` to `ceil_mult` of base (`MARKETS.md` § Price resolution), so a recipe positive at base
can still lose under glut.

- **M1 — marginal, at base.** Per batch (per unit for extraction):
  `revenue − marginal_cost ≥ k × marginal_cost`, where `marginal_cost` = inputs at base + wage per
  batch, and wage per batch = `base_wage / base_rate` (output and wages both scale linearly with the
  assigned workforce, so the per-batch figure is staffing-independent at habitability 1). `k` is
  `economy.recipe_margin_anchor.profit_over_marginal`; `k = 1.0` is the sentence verbatim — profit at
  least equal to marginal cost, revenue at least twice it.
- **M2 — fixed cost at the floor.** Per tick at `typical_workforce` (the staffing generation seeds a
  building with): `(revenue − inputs) × floor_mult × batches − wages − maintenance − goods upkeep ≥ 0`.
  Inputs and outputs both at the floor, since a glut market has everything cheap. Maintenance is the
  authored constant at workforce target 100; goods upkeep is the band's per-type basket valued at
  base. This is BL-740's anchor stated once for extraction and processing alike.

**The anchor route.** A good with several in-band routes is priced off its **cheapest** — the
lowest marginal cost per unit of primary output — and that route must clear `profit_over_marginal`.
Every other route clears `alternate_profit_over_marginal` instead (`0` = profitable at base), and
the floor half regardless. A recipe whose primary output is an extractable raw is always an
alternate: extraction is that good's cheapest route.

**Prices are era-banded.** Each band checks against its own price table — `RESOURCES.md` § Base
prices are era-banded.

**What is exempt, and says so.** A recipe whose every output is unpriced (propellant — consumed by
the Launchpad, never sold) has no market margin to anchor and is listed, not failed. An **unpriced
input** is a defect, not an exemption: the good cannot be bought at any price.

**Retune order.** Green is reached from costs and rates first (the per-batch wage and maintenance
share move every row at once), then input quantities where a recipe is authored at zero or negative
value-add, and base prices **last** — each tier's price is the next tier's input cost, so lifting
prices compounds up the chain, widens the ladder `RESOURCES.md` rests on, and moves the derived
`ceil_mult` (re-derive it with `haulage_measure` after any change to the cheapest base price). Each
band carries its own price table; a retune must clear both.

**What the anchor does not claim.** It is necessary, not sufficient. A roster that pays at base can
still lose collectively when the money entering the field is less than what leaves it (`FINANCE.md`
owns the loop); the anchor is the precondition for that measurement meaning anything. The check is
`tools/verify/recipe_margin`; the owner is BL-744 (recipe margin anchor).

---

## Amenity and habitability buildings

Amenity and habitability buildings do not produce tradeable industrial goods — they produce conditions. Their output is a change in the local habitability score or population growth rate, which feeds into workforce efficiency and long-term productive capacity. The feedback model is `docs/economy/POPULATION.md`.

### Amenity buildings

| Building | Input | Effect | Land use set |
|----------|-------|--------|-------------|
| Park | — (no input; tile locked to amenity use) | Habitability bonus in vicinity | Amenity |
| Recreation Facility | Consumer goods (small ongoing consumption) | Habitability bonus; population satisfaction | Amenity |
| Cultural Centre | Consumer goods + luxury goods | Larger habitability bonus; reduces unrest | Amenity |

Amenity buildings are placed on tiles and lock them to amenity land use — no extraction is possible on that tile. High-habitability compositions (forest, grassland) give a larger base amenity value, so placing a Park on a forest tile is more valuable than on a barren tile. Leaving a habitable tile undeveloped has a smaller inherent amenity contribution without consuming the tile's potential.

### Habitability production buildings

These produce habitability goods (see `docs/economy/RESOURCES.md`) consumed by population centres. They use processing building mechanics (recipe, workforce scalar, stockpile) and differ from industrial processing buildings only in that their outputs feed population demand rather than further industrial chains. The three are the **Welfare Goods** group of `processing_facility` recipes (BL-368, habitability tranche):

| Recipe | Inputs | Output | Era |
|---|---|---|---|
| Water Treatment Plant (`clean_water`) | Water ×2 | Clean water | 0 |
| Consumer Goods Factory (`consumer_goods`) | Food rations ×1 + steel ×1 | Consumer goods | 0 |
| Pharmaceutical Lab (`medical_supplies`) | Water ×1 + agricultural produce ×1 | Medical supplies | 0 |

Steel stands in as the Consumer Goods Factory's refined industrial input, and water as the
Pharmaceutical Lab's process input — no standalone chemical `resource_type` exists, mirroring the
Hydroponics Bay's water-as-process-input precedent. **Building materials and Utilities are not
resources** (RESOURCES.md § Habitability goods): Building materials feeds construction cost,
which is a different consumption path, and Utilities is an abstracted budget cost — so there is
no Construction Yard and no Power Plant recipe. Verified by
`tools/verify/habitability_tranche_harness.cpp`.

---

## Infrastructure buildings

Infrastructure buildings affect logistical or economic capacity rather than extraction or processing directly.

| Building | Function | Era |
|----------|----------|-----|
| Port | Enables stockpile exchange and convoy dispatch on the body; coastal placement (`is_coastal`) | 0 |
| Launchpad | Required to dispatch convoys to off-world bodies; consumes propellant per launch | 0 (built), 1 (operational) |
| Inland Logistics Hub | Land-mode logistics node (`building_type::inland_logistics_hub`, BL-149): its tile joins the population-centre set that discounts the A\* haul cost of any intra-body convoy routed through it (LOGISTICS.md). The player-placeable counterpart to a city's free-hub discount — extends the cheap land network out to remote sites. Produces nothing (0 workforce); cost in `economy.buildings.inland_logistics_hub`. | 0 |
| Military Base | Unit muster building (`building_type::military_base`, BL-325). Produces nothing, staffs at zero, and is deliberately **not** a supply anchor — military reach IS the economic reach field. `docs/military/MILITARY.md`. | any |
| Research Institute | The "how does tech get done" building (`building_type::research_institute`, BL-332). Passive: a flat per-tick credit to its owner's `corporation_component::science`, a market-invisible accumulator, not a resource. | any |
| Schooling | Education building any settlement can host (`building_type::schooling`, BL-615 — stratum placement gates). Passive like the Research Institute; its qualification-raising effect belongs to POPULATION.md § Qualification. Placement is what defines it: it must stand **in** a population centre, of any stratum (POPULATION.md § Strata gate buildings). | any |
| University | The Schooling building's City-tier sibling (`building_type::university`, BL-615). Passive; must stand in a centre of stratum **City (4) or above** — "you can't build a university in a town" (Ben, 2026-08-25). | any |
| Orbital Port | Receives off-world convoys; required on any non-terrestrial body to accept supply | 1 |

**Stratum placement gates are authored data, not code** (BL-615, stratum placement gates;
POPULATION.md § Strata gate buildings owns the design). A building definition carries a
`placement_gate` — `requires_centre`, `min_centre_scale`, `centre_proximity_radius` — authored
per type in `economy.lua` and, for processing, per recipe in `recipes.lua`: the heavy
processor class (the steel-mill recipes) carries a small proximity radius, so a mill must sit
near a settlement's workforce while a miller need not. `placement_rules::can_place_in_world`
reads the gate generically and refuses with a distinct reason per axis (`needs_centre`,
`centre_too_small`, `far_from_centre`).

The Orbital Port is design vocabulary with no enum value. Storage capacity and per-node
throughput are **Logistic Points** — `docs/economy/LOGISTICS.md` § Logistic Points (BL-464):
a per-tick rate not a stock, a cap not a price, the node half only. There is no Warehouse or
Storage Depot building; pools hold unbounded quantities, and the constraint on moving goods is
the network.

**The Launchpad is the physical gate to space**: a corp must hold one on the source body before
any inter-body convoy can depart (`corp_has_launchpad_on`, `src/world/supply_system.cpp`), and
the pad must be **fuelled** as well as present. `dispatch_convoys` gates the space lane on the
corp's propellant stockpile on the *source* body and burns **1.0 unit per launch**
(`propellant_per_launch`, `src/world/supply_system.cpp`): per launch, not per unit of cargo and
not per AU — the pad is the thing being fuelled. An unfuelled pad is exactly as shut as no pad at
all. A convoy exporting propellant itself cannot burn the cargo it carries; the gate subtracts
the cargo first. Propellant is deliberately **left out of the market's base-price table**, so it
is made and burned within a corp's own pool rather than traded. See **`docs/economy/ERAS.md`**
for the Era 1→2 transition.

*Save-format note.* Appending a `resource_type` value renumbers nothing but changes the length of
every per-resource array; every such array is sized off `resource_count`, so the append costs a
version bump and no per-array edit (BL-107, save-format header).

---

## Workforce model

The per-`(corp, body)` pool model, as `run_economy_step` in `src/world/economy_system.cpp` and
`compute_building_opex` in `src/world/budget_system.cpp` implement it; the design rationale is
POPULATION.md § Workforce model.

**Supply** derives from the body's population centres (BL-042, workforce supply derivation):
each centre contributes labour by scale — `labour_by_scale` = 1 / 3 / 10 / 30 / 100 units for
scale 1–5. A corp's share of that body supply is its share of the building count there; a body
with no centres falls back to the authored `world::workforce_supply` figure (default 3.0).

**Demand** is the sum of `workforce_assigned` over the corp's producing buildings on the body
(extraction and processing only; ports and hubs demand no labour), capped by the body's
habitability cap `min(1, mean_hab / 0.6)` (BL-041, habitability gates workforce).

**Contention** clears by **wage competition** (BL-614, wage competition; the ruling and its
rationale are POPULATION.md § Contention). Uncontended (`demand ≤ supply`), every building is
staffed at request. Contended, scarce labour allocates **per building** — offered wage
descending, building id ascending on a tie, each building granted up to its demand until the
pool is spent — so the marginal building runs partial and those below it idle, superseding the
old uniform proportional scalar. The offered wage is `base_wage × (1 + wage_bid)`
(`building_component.wage_bid`, a per-building premium fraction — the first-cut dial, data-only,
no UI yet; NR-629 flags the shape for overturn). The pool aggregate `min(1, supply/demand)`
survives in `economy_report.workforce_contention` as the report figure; the per-building grant
is `economy_report.building_labour`. A recipe's **qualified** requirement (§ POPULATION.md
§ Qualification, BL-613) clears against its national pool by the same rule, before the ordinary
pool; a building's factor is the product of the two grants. Every grant is then multiplied by
`workforce_efficiency(hab)` (`src/world/workforce.hpp`, BL-069 workforce efficiency): full
labour at habitability ≥ 0.6, ramping linearly to 0.5× at 0. Effective workforce =
`workforce_assigned × grant`.

**Cost** follows the wage/maintenance split (`compute_building_opex`, BL-049): maintenance
carries a fixed **30 % material floor** charged even when decommissioned, plus a labour
remainder scaled by the workforce target (zero when decommissioned); wages are
`workforce_assigned × grant × base_wage × (1 + wage_bid) × wt_scalar × hab` — paid **at the
offered rate** on the labour actually allocated, not the request (BL-614): a building that
outbid its siblings pays the premium it offered. `docs/economy/FINANCE.md` owns the money side.

`workforce_assigned` itself is an authored constant set at placement (0.5 for producing types,
0 for passive infrastructure) and is never player-edited. The **player lever is
`workforce_target`** (0–200 %), auto-solved by default (below).

### Workforce target and the auto-solver

On top of the assigned fraction sits a player-facing **workforce target** (`building_component.workforce_target`, 0–200 % of nominal, default 100 %) — a scalar on both output and the labour portion of wages/maintenance, applied identically in `economy_system` and `budget_system`. The design is BL-181 (workforce auto-solver).

By default a player building's target is **auto-solved** each economy tick (`workforce_auto = true`): `solve_workforce_target` (economy_system.cpp) picks the target that maximises that building's estimated net profit this tick. Because the model is otherwise *linear* in the target (output, wages, and the labour part of maintenance all scale with it), a fixed-price optimum would be degenerate bang-bang (0 % or max) — the **interior optimum comes from the local market price response**: more output raises local supply, which lowers the clearing price (`base · √(demand/supply)`, `docs/economy/MARKETS.md`). The solver reprices each candidate tier against that response — inside the same authored price band the market clears in (`wf_target_price`, MARKETS.md § Where the band lives) — and takes the best net. It is deterministic (reads last tick's market state), **player-corp only**, and applies only to producing types (extraction / processing).

The target is the *heuristic*, not a hard goal: a manual tier chosen in the building-management UI **pins** the value and clears `workforce_auto`; the **Auto** control re-enables the solver. This is the sole sanctioned auto-action on the player's corp (see `.claude/rules/io-standing-rules.md` § player-corp exception).

*Fidelity:* labour contention is held at its current value, input-price response is ignored (inputs valued at the current price), and the tier search is coarse (10 % steps) — so the solved target can hunt by ±one step.

---

## Stockpile and output flow

Extraction and processing outputs accrue into a shared stockpile pool held per `(corporation, body)` (a world-level map, not the per-building `stockpile_component`, which the economy does not use). At the economy tick boundary:

1. **Supply** is the goods each corporation lists for sale — its surplus above what its own processors will consume that tick (auto-surplus), plus its standing sell orders.
2. **Demand** is what processing buildings and construction sites set out to buy this tick (the *want*, net of the corp's own pool — MARKETS.md § Want and fill), plus population and background demand.
3. **Transactions clear at the resolved market price.** Sales credit, and purchases debit, the corporation's balance at `market_component.price`, resolved each Tick from the supply/demand ratio as `base × √(demand/supply)`, clamped to the `[0.25×, 10×]` band and EMA-smoothed. Demand and supply come from real actors: population centres and the offstage economy on the demand side, real background firms on the supply side. See `docs/economy/MARKETS.md` for the clearing model.

---

## A shortfall scales output; it never idles (Ben, 2026-08-31)

**Any upkeep draw a building cannot meet reduces what that building produces. It does not switch the
building off.** This holds for every goods draw — materials, power, whatever a later channel adds —
and it is a rule about the *economy*, not about any one channel.

The reason is a loop, and it is why the rule is absolute rather than a default. A building idled for
want of an input stops buying that input. It was a **buyer**, and the price it was willing to pay is
the signal that would have called forth the supply. Idle it and the signal disappears, so the supply
is never built, so the draw stays unmet, so the next building idles too. A new universal draw is
unmet everywhere at once on the tick it is introduced, which turns that loop into a cliff rather
than a slope — and halving the rate only delays it, because the cause is structural rather than one
of magnitude.

A building on reduced output keeps bidding. That is the whole difference: it stays a participant in
the market that has to supply it.

**The corollary for authoring:** a channel's rates may ship at zero while its shape ships complete.
A draw for a good the world does not yet make is not a channel that needs tuning down — it is a
channel whose supply has not been induced yet, and the honest response is to ship it inert and turn
it on when the good exists.

---

## Power (Ben, 2026-08-31)

**Power is not a traded good and not a per-body pool. It is a grid.** Ben's ruling:

> *"Power travels via road infrastructure, although it is different from a convoy, it has a 1 qtr
> travel time or weight. So if there is a connection from a power source to the player's HQ, they
> will be able to use the power on the next tick. Power can be stockpiled, but not infinitely."*

### What it is

A **generation building** converts fuel into power. Every building that needs power **buys** it, and
the purchase is its upkeep draw. Ben, 2026-08-31, settling this:

> *"I believe we need to distribute power across all industry. This means that it has to be a bought
> good when it is taken as upkeep. Therefore corporations can buy power from each other, and
> background companies can produce power with a profit."*

So power is:

- **A bought good.** It has a price, it clears on the market, and a short building **bids for it** —
  exactly the shape § Settled: a short pool BUYS gives every goods draw. Power upkeep is the first
  channel built on that rule rather than an exception to it.
- **Never cargo.** It has a price but no convoy: transmission is the road network itself
  (`docs/economy/LOGISTICS.md` § 3a), so it is the first good whose **movement and market are
  separate questions**.
- **Connection-gated.** A buyer can only match a seller its network reaches. That is what keeps
  power a *regional* price rather than a world one, and it is the whole reason a road matters twice.
- **Stockpiled, with a ceiling.** Unlike every other good, its store is capped. A generator running
  into a full store is producing nothing anyone will ever buy — a real decision rather than an
  accounting detail.

**Generation is a business, not a cost centre.** Background firms build power plants and run them at
a profit, which is the point: it gives the world a firm type with a reason to exist, and it means
power supply is induced by price the way every other good's is.

### Why this closes the fuel chain at BOTH ends

`coal` and `petroleum` are on the census's *extractable, no market sink* list — petroleum produced
6169.9 against demand **0.000** in the industrial band. Power gives them an endpoint, and because
power is bought rather than self-supplied, **both links of the chain bid on the market**:

    fuel  --(bought by the generator)-->  power  --(bought by every building)-->  consumed

The generator's fuel purchase is an ordinary processing input. The building's power purchase is an
upkeep bid. Neither is a pool draw, so neither severs the chain — which is MARKETS.md property 3
satisfied twice over, and property 4's *derived demand propagates through links that bid* working
exactly as designed.

**An earlier draft of this section had power NOT traded** — generated by a corp for its own connected
buildings. Ben overturned it the same day, and the reason is the one that matters: as private
infrastructure, power demand is bounded by each corp's own generation, which is a small closed loop.
As a bought good, **every building on the map is a buyer**, which is the economy-scaled sink the
census says is missing. It also fixes the harsher consequence of the private reading, where a corp
with no fuel access was simply stuck; now it buys power from someone who has fuel, which is trade.

### It trades internationally, and that is not a detail

**Power crosses borders** (Ben, 2026-08-31). The road network does not stop at a national boundary,
so neither does the grid: a generator in one nation sells to a building in another wherever the
network connects them.

**The machinery already exists.** MARKETS.md § Tariffs resolves a market to a jurisdiction through
`market_component::centre_tile` and `world::tile_to_nation`, and charges the enacted import duty on
any matched trade whose buyer is domiciled outside it. Cross-border power is an ordinary
cross-border sale. Nothing new is needed to make a nation earn from the power flowing through it.

**And it reaches the Era's catastrophe, which is the strongest thing about it.**
`docs/economy/ERAS.md` § The point of an Era makes Era 1 a test the player is trying to pass, scored
on **Alarm**, and its own danger table names the answer in the economic dimension: *"keep
cross-border routes live; trade interdependence is the cheapest Alarm suppressant in the game"* —
with **Autarkic Substitution** listed as the herring that looks like resilience and cuts the ties
holding Alarm down.

International power is that interdependence in its most legible form. A nation whose lights depend
on a neighbour's generation is a nation with a reason not to escalate, and severing that tie — by
autarky, by tariff, or by interdiction (`LOGISTICS.md` § 7) — is a visible, causal step toward the
rupture. That is a system feeding **Trade and Conflict at once**, which is the test
`docs/SYSTEMS.md` sets every system, and very few pass on both.

### Power is what makes the Industry channel viable

MARKETS.md's Industry channel ships its rates at **zero**, because the goods it drew — tools, planks
— are *produced 0.0* in band, so every draw went unmet and the firms it drew from collapsed 227 → 19.
Power is the first entry in that basket the world will actually make, because making it is
profitable. Turning Industry on for power is therefore a different proposition from turning it on for
tools, and this is the order to do it in.

### The shortfall rule, and the lesson it inherits

A building short of power **scales its output down**; it is not idled. This is not a preference, it
is the BL-641 lesson applied before the fact: an upkeep draw that idles a firm kills the buyer that
would have induced the supply, and the measured result was operating firms collapsing **227 → 19**.
A firm that survives on reduced output keeps bidding for fuel, and the generation that answers it
gets built.

### Band

**Industrial band only** (Ben, 2026-08-31). The ancient band gets no power analogue: `charcoal`
already carries real household demand there and is not an orphan, so a second design against a band
the prototype does not open on would be work without a reading behind it.

---

## Construction as a rate (Ben, 2026-08-31)

**Construction becomes a sector with a throughput, not a per-building lump sum.** A **construction
building** exists, runs a **production method**, draws that method's goods as upkeep, and produces
construction capacity. Building projects consume that capacity.

Ben's reason, and it is the strongest argument for the change:

> *"This makes it easy to start with some construction of a certain method, and gives us an initial
> demand for those construction goods."*

### The channel it fixes

Construction demand in the industrial band measures **0.000**, because `run_construction` fires only
where something is actively building. It is episodic: nothing under construction, no demand. A sector
makes it **continuous and economy-scaled** — capacity exists, draws its inputs every tick, and grows
with the world. That is MARKETS.md property 1 satisfied by construction rather than asserted of it.

And because generation can **seed construction capacity**, the demand for its inputs is non-zero from
tick 0, in every market that has any. That is MARKETS.md property 5's *"every chain terminates in
every market"* delivered by the generator rather than by authoring a basket.

### The methods

Five, era-banded exactly as recipes are (property 2):

| Method | Band | Draws |
|---|---|---|
| Timber frame | ancient | timber, planks |
| Stone and brick | ancient | stone, dressed stone, clay |
| Iron frame | industrial | iron, timber |
| Steel frame | industrial | steel |
| Reinforced concrete | industrial | steel, stone, sand |

Exact baskets and rates are a balance question and are **measured against the census**, never
guessed. The ladder deliberately spans both bands, so the ancient band gains continuous construction
demand too — it currently measures 56.3, better than industrial's zero and still episodic.

### What it touches, and the debt it must clear

This is a structural change, not an additive one: it reaches `construct_building`, placement, the
build door, and **the AI scorer's capex model**.

**NR-592 gets fixed here, on Ben's call.** `corp_ai.cpp` never prices `resource_build_cost` when
scoring a build — a candidate scores on cash alone, and the seam refuses it at apply time if the
materials are absent. Under a lump-sum model that is a missed opportunity. Under a **shared capacity
pool it becomes a correctness problem**, because capacity is contended: a scorer that cannot see it
will propose builds the pool cannot serve, every evaluation, for as long as the pool is short.

Construction is a **market-gated, pay-as-you-build** process, not an instant purchase (BL-095,
construction pacing). Placing a building gates only on affordability (the corp must be able to
afford the whole build to commit to it) and does **not** debit up front. Each economy tick a
building under construction:

- draws `resource_build_cost / build_duration_ticks` of its materials as **real market
  demand** (competing with population and other builds, bidding the local price up) and pays
  the resolved price for them, plus the same fraction of the flat `build_cost`;
- progresses at a **rate set by how much of that per-tick material need the local market can
  supply** — read from, and drained from, the market's real stock (`market_component.inventory`,
  MARKETS.md § Real market inventory): market supplies the full need → full speed; supplies part
  → **stretched** (up to `max_stretch ≈ 10×` the base duration); supplies less than
  `1/max_stretch` → **paused** until supply recovers.

So a build in a thin market slows or stalls rather than completing instantly, and its cost is
spread across the build. The front door shows the analog rate/ETA/paused status rather than a
binary reject. Tunables live in `scripts/economy.lua` § `construction`. A build stalled for want
of an input registers that want as demand, so the stall is visible to the price signal.

### Construction materials are per-named-building, not per-type (BL-590, 2026-08-24)

**Ruling (Ben, 2026-08-23): materials vary per named building.** `resource_build_cost`
(`building_economics`) still names the DEFAULT for a `building_type` — before this item every
building in the game, ancient or industrial, was made of `steel` and nothing else, the same
anachronism the launchpad's era tag (BL-433) exists to fix for the launchpad specifically.

**The override.** `recipe_registry::resource_build_cost_for(type, target, recipe)` is the single
lookup every material-cost call site now goes through — the Build door's capex preview, the
management view's rate, the real per-tick draw (`construction.cpp`, `economy_system.cpp`) — so a
preview can never disagree with what the tick actually charges, the same argument BL-436 makes
about richness. `extraction_site` is keyed by its `target_resource`; `processing_facility` by its
recipe. No `building_type` is needed in either key: only extraction carries a target and only
processing carries a recipe, so the dispatch is unambiguous. Authored in `economy.lua` as an
optional `material_overrides` table nested beside the type's own `resource_costs` — absent means
the type default, so nothing that does not opt in changes.

**First-cut coverage: the whole ancient roster, nothing else.** Five extraction targets (`stone`,
`timber`, `sand`, `clay`, `peat`) and all thirteen ancient processing recipes get a
timber-and-stone basket instead of steel; every industrial and space-sourced entry is untouched —
an industrial mine or smelter genuinely does need steel reinforcement, and the anachronism this
item fixes is specifically the ancient arc's. The Smithy's two recipes (`steel_from_blooms`,
`ordnance_from_blooms`) share the same basket, since `recipes.lua`'s own comment names them as the
same physical building. See `scripts/economy.lua`'s `buildings.extraction_site.material_overrides`
/ `buildings.processing_facility.material_overrides` for the authored table.

**A second sink for the shallow goods**, beyond flavour: timber, stone, clay and the BL-585
goods now cost something to a corp that is *building*, not only something a recipe consumes — the
demand that makes an early quarry worth owning even before its output has a processor waiting.

**A watch this item does NOT resolve**, recorded rather than silently assumed:
`corp_ai.cpp`'s build-candidate scoring prices only the flat `build_cost` (`ex.build_cost` /
`pe.build_cost`), never `resource_build_cost` — a pre-existing simplification, unchanged by this
item, that predates the override. A rival can therefore still propose a candidate whose *materials*
it cannot actually reach even though its *cash* is sufficient; `construct_building`'s own
affordability gate refuses it cleanly (no mutation), so this is a missed opportunity for the
scorer, not a correctness bug. Widening the scorer's estimate is real planner scope, not part of
"materials vary per building" — a decision recorded here rather than a gap papered over.

Guard: `tools/verify/construction_harness.cpp`'s R9 asserts the lookup directly — an overridden
target/recipe reads its override, an unoverridden one falls back to the type default, and a
non-extraction/non-processing type (which carries neither a target nor a recipe in the sense
these overrides key on) ignores both maps regardless of what happens to be passed.

## The ancient chain

The ancient arc's production chains, authored in `scripts/recipes.lua` with `era = "ancient"`
(BL-429, ancient chain). **No `resource_type` values belong to it alone.** Every input is a raw
with authored deposits and extraction rules — `stone`, `timber`, `sand`, `clay`, `peat` — and
every one of them is priced and consumed, per the admission rule.

| Recipe | Inputs → output | Depth |
|---|---|---|
| Charcoal Burner | 1.5 timber → 1 charcoal | 1 |
| Peat Kiln | 2 peat → 1 charcoal | 1 |
| Coking Kiln | 0.8 timber + 0.05 iron blooms → 1 charcoal | 3\* |
| Bloomery | 2 iron ore + 1 charcoal → 1 iron blooms | 2 |
| Smithy | 2 iron blooms + 1 charcoal → 1 steel | 3 |
| Smithy | 2 iron blooms + 1 charcoal → 1 **ordnance** | 3 |
| Potter & Weaver | 2 clay + 1 timber → 1 trade goods | 1 |
| Glassworks | 2 sand → 1 trade goods | 1 |
| Miller | 2 agricultural produce + 1 stone → 1 food rations | 1 |
| Potter's Kiln | 2 clay → 1 ceramics | 1 |
| Stonemason | 2 stone → 1 dressed stone | 1 |
| Sawmill | 2 timber → 1 planks | 1 |
| Toolmaker | 1.5 iron blooms + 1 planks → 1 tools | 3 |
| Tannery | 2 hides → 1 leather | 1 |
| Weaver | 2 fibre → 1 cloth | 1 |
| Shipwright | 1.5 planks + 1 cloth → 1 rigging | 2 |

\* The Coking Kiln's own required depth, not `charcoal`'s overall depth — the Burner keeps that at
1, since chain depth is a **min across recipes** (§ Chain depth below). The Kiln is BL-587's
alternate method: unreachable until a corp has already smelted blooms, cheaper by the guard's
reference prices once it is. See § Alternate production methods.

**Rows five through eight are BL-585/BL-586's first slice of the wide ancient roster** (2026-08-24)
— named buildings on existing raws, no new deposit or extraction target. The Toolmaker is that
slice's deepest chain, needing both a smelted good and a milled one at once (required depth 2 from
`max(depth(blooms)=2, depth(planks)=1)`, so `depth(tools) = 3`), tied with the existing ceiling
rather than past it.

**The last three rows are BL-586's second slice** (2026-08-24), authoring the chains slice 1
deliberately deferred: `hides` (endemic — lat/sector-restricted, richness-scored, like the four
endemic goods above, not the plain cover-based ambient mechanic `fibre` uses) and `fibre` (the
ordinary case, grown by the same cover-based ambient/biotic mechanic `agricultural_produce`
uses) are both new extractable raws (`placement_rules::k_extractable`), with real tile-generation
deposits. The Shipwright is the roster's first chain to clear depth 2 (`max(depth(planks)=1,
depth(cloth)=1) + 1 = 2`) — past the flat depth-1 ceiling every other named building outside the
Toolmaker sits at, and the design's own "every chain should reach depth 2 or better" bar. `rigging`
is the roster's chosen name for the design table's "terminal trade good": ropework, cordage and
tackle — a genuine ancient-craft good a shipyard plausibly makes — in the same generic-material-
noun register as `ceramics`/`dressed_stone`/`tools`.

**The Smithy's second recipe is the ancient arc's route to `ordnance`** (BL-460, ancient
ordnance) — the same building, same `iron_blooms + charcoal` basket as its steel recipe,
switchable between the two (§ Alternate production methods). Unit upkeep draws ordnance every
tick in either arc, so an ancient campaign (the default, `epoch_year = 0`) must be able to make
it. See § Fabricator above and `RESOURCES.md` § The two terminal goods.

**The load-bearing authoring is the tag on the coal-fired `steel` recipe, not the additions.**
That recipe is `industrial`, so the ancient arc reaches steel only through timber → charcoal →
blooms → steel. Were the shallow route available (coal is a raw), min-across-recipes would
correctly collapse steel back to depth 1 — the chain would exist and mean nothing. Ancient max
depth is **3** on exactly that tag.

Food is deliberately **not** gated behind an industry: the Miller is depth 1, because an ancient
start that cannot feed itself until it has built a smelting chain is not a start.

**A trap, and how it is closed.** A call site that defaults an unconfigured processing facility
to a named recipe such as `recipe_id("steel")` still *resolves* in an ancient campaign — the
storage path is band-independent by design — so it would silently seed processors with an
industrial recipe, and nothing would refuse it, since the era gates browsing and placement rather
than execution. Every such site calls `recipe_registry::default_recipe_id()`, which returns the
first recipe the current band allows. **Ask for a sensible default; do not name a recipe and
hope.**

The extraction/processing buildings that would give these chains named identities (quarry,
woodcutter, kiln, smithy) with their own placement rules and glyphs are design vocabulary on the
generic types, as for the industrial roster.

---

## Chain depth — the growth track

**The growth spine is chain depth** (Ben's ruling, chosen over building tiers, the ancient tech
ladder and settlement scale; BL-428, chain depth). How far down the production graph a corp can
reach is what gates its next building, so progress is a *consequence of the economy it has built*
rather than a parallel unlock system laid over it. The decisive argument: every alternative is a
second system that must be kept in agreement with the economy, and depth is read off the recipe
graph that has to exist anyway.

Depth is **computed, never authored** (`recipe_registry::depth_of`):

```
depth(raw)  = 0                       -- a good no allowed recipe produces
depth(good) = min over producing recipes of ( 1 + max over that recipe's inputs of depth(input) )
```

The two composition rules differ deliberately. **Max within a recipe**: you cannot run it until your
deepest input exists, so its difficulty is its hardest input. **Min across recipes**: if two routes
make the same good, you have reached it as soon as the *easier* route is open. That asymmetry is
what makes alternate production methods (below) compose with the ladder — it is settled here
rather than discovered there.

Depth is computed over the **era-allowed** recipes only: a route the campaign's band masks out
does not exist for that campaign, so it must not shorten anything. Masking can only ever *raise*
depth or make a good unreachable, never lower it.

**`-1` means unreachable** — no sequence of allowed recipes bottoms out in raws. That covers a cycle
and an orphaned input with one code, because from the player's side they are the same fact: you
cannot get there. Implemented as a bounded fixed point rather than a graph walk, so it terminates by
construction, needs no recursion, and does not depend on traversal or container order — an
unordered container must never decide a number the economy reads.

Measured on the authored recipes: **industrial max depth 4, ancient max depth 3**.

### What depth does not do — it is a readout, not a gate

Depth is a **measurement of the economy a corp has built**, and it is only that. It does not gate
a building, a recipe, or a retool, and no seam refuses anything on it.

**Tech is the only lock on a method** (Ben, 2026-08-29): *"most direct unlocks should be a
consequence of research, not just economy... allow both corporations and companies to build any
building, or use any method surfaced with current tech."* A corporation may place any building and
run any recipe its **tech** permits, whatever it has or has not produced before.

The reasoning is that a second progression axis has to earn its keep against the one already there.
Research is the axis the design wants unlocks to hang from, and depth-as-a-gate duplicated it from
the other side — the economy re-deriving a lock the tech tree is meant to own. Depth as a *number*
costs nothing and says something true, so it stays; depth as a *refusal* was the part that had to go.

What survives, and where it is still read:

- **`recipe_registry::depth_of`** — the computed depth of every good (above). Still the roster's
  layering measure, and a load-bearing **dominance axis** in `chain_depth.cpp`'s R2 row, which asks
  whether one production method strictly dominates an interchangeable sibling.
- **`recipe_registry::recipe_required_depth`** — a recipe's deepest input's depth. Derived in the
  same fixed point as `depth_of`, so the two cannot drift.
- **`corp_reached_depth`** over **`corporation_component::produced_ever`** — the deepest good a corp
  has ever made, set by `run_extraction` / `run_processing` at the moment a good is actually
  produced. Produced-once-ever and never cleared.

`produced_ever` is **written and never read** by the simulation. It is retained because it is pinned
by the flat-binary save format (`world_save.cpp`, field order for `world_save_version` 16): dropping
the field would invalidate every existing save. Both the field and `mark_produced` carry an explicit
note saying so, because a write with no read is otherwise indistinguishable from a defect.

Guard: `tools/verify/chain_depth.cpp` — D1–D6 for the metric, G1 and G4 for required depth, G5 for
the tick-0 opening. Measured on the authored recipes, the ancient ladder reaches **depth 3**.

**`tech_locked` reaches a recipe too, now** (BL-588, 2026-08-24). Until this item `tech_locked`
only ever meant a *building type* was ungated — the effect union (`tech_gate.hpp`) had no arm that
could name a recipe. `unlock_recipe` is the third arm: a tech gate names a `recipe::name`
(`tech_gate::unlocks_recipe`), and `recipe_unlocked(w, reg, corp, recipe_id)` resolves the id to
that name and checks it against `world::has_tech`, at both `construct_building` (returning the
SAME `construction_result::tech_locked` the structure-level check already used) and
`try_switch_recipe` (a new `recipe_switch_result::tech_locked`). It is checked at **both** doors,
and that is not redundancy: guarding only placement would leave a one-click bypass — place an
ungated method, retool onto the locked sibling in the same group, and the lock never has to be
cleared. **`tech_locked` is the only recipe-level lock**; the era band decides which roster a
campaign sees, and within that roster tech decides what a corp may run.
First-cut authored gates (`tech_gate.cpp`): `E0-EC-01` unlocks the Toolmaker (BL-586) on owning a
processing facility and a Cr 500 surplus; `E1-EC-01` unlocks the Bessemer Converter (BL-587) on
already holding `machinery` in stockpile — the Converter's own reagent, not a cash figure, after a
surplus-only first draft proved satisfiable by any solvent corp regardless of what it had actually
built (caught by `tech_gate_harness`'s T3 fixture, corrected before landing); `E0-EC-03` unlocks
`refined_copper` (BL-589) on owning a processing facility and a Cr 400 surplus — the roster's
widest anachronism before this gate, since `refined_copper` is `any`-band at required depth 0 and
could be smelted for free on tick one of an ancient campaign with no ancient identity to it at all.

**The Build door filters a tech-locked recipe out, the same way it does an era-locked one**
(BL-593, 2026-08-24). The door's candidate filter (`selection_panel.cpp`) carries two clauses —
`building_available` for era and `recipe_unlocked` for tech — both resting on the same argument,
stated in the code's own comment: *"the door not showing what the gate would refuse."* Filtered was the ruled choice over
shown-and-locked (Ben, 2026-08-24, same elicitation form as the opening ruling) — the alternative
would have needed a new lock-reason string and a UI affordance the door doesn't have yet; the
existing precedent already answers the question the same way. `refined_copper` is the first recipe
this clause actually removes — every earlier tech gate targeted a `building_type`, never a recipe,
so the branch was dead code on every campaign before `E0-EC-03`.

**Depth has no player-facing readout.** It is an internal measure of the recipe graph and of what
a corp has produced; no surface displays a reached-depth number, and none needs to, because the
number no longer decides anything the player can act on. A corporation is told what it may build by
the Build door's own contents, which is filtered by era and tech alone.

---

## The start gate — what a fresh corp actually sees (BL-589, 2026-08-24)

Ben's steer authoring Sprint 17: *"most building types, and most recipes are not buildable on
game start."* The opposite is now the design (Ben, 2026-08-29): a corp may run **any method its
tech permits**, so the start gate is a **tech** question and nothing else.

A fresh ancient corp — no tech earned, no buildings, no balance — sees **sixteen** of the ancient
band's seventeen processing recipes open on tick one:

| Group | Open at tick 0 | Why |
|---|---|---|
| Fuel Production | Charcoal Burner, Peat Kiln, Coking Kiln | The Burner and the Kiln are a genuine supply-route pair (disjoint raws — R2's own classification). The Coking Kiln is the deeper fuel route; it is open, and what limits it is whether its reagent can be bought, not a lock. |
| Food Processing | Food Rations, Miller | The any-band/ancient pair, both open. |
| Artisan Goods | Potter & Weaver, Glassworks, Tannery, Weaver | An early corp may sell trade goods without earning anything first. |
| Construction Materials | Potter's Kiln, Stonemason, Sawmill | BL-586's slice-1 buildings — foundational, not earned content. |
| Metal Foundry | Bloomery, and the Smithy's two routes (steel, ordnance) | Open. The chain is limited by charcoal supply and by whether blooms find a buyer — a market fact, not a refusal. |
| Advanced Fabrication | Shipwright | Open, and shut in practice by its own inputs: it draws `planks` and `cloth`, and the shipped ancient world produces no planks. |
| **The one closure** | *(Toolmaker — tech)* | `E0-EC-01` "Tool-and-Die Practice" gates the Toolmaker on owning a processing facility and a Cr 500 surplus. A fresh corp meets neither. |

**Exactly one ancient recipe is closed at tick 0, and tech closes it.** That is the whole shape of
the start gate now. `refined_copper` is absent from the ancient band entirely — it carries an
`industrial` era tag, so the band never lists it (an earlier reading had it closed by `E0-EC-03`
instead; both close it, and the era tag closes it first).

**An open method is not a viable one, and the difference matters.** Most of what opened is limited
by supply rather than permission: the Shipwright needs planks nothing makes, and the Bloomery's
output has no measured buyer. The design's position is that an economy should say *no buyer* or
*no input* — prices a player can read and act on — rather than *not permitted*.

Guard: `tools/verify/chain_depth.cpp`'s **G5** row asserts this opening exactly — every recipe
matches its stated open/closed state, and the one closure genuinely resolves (not a permanent
orphan) once its own authored predicate is met.

---

## The era band — which roster a campaign sees

Every authored building type and recipe carries an optional **era band**: `any` (the default,
shared by both arcs), `ancient`, or `industrial` (BL-433, era band). Authored as `era = "..."` in
`scripts/economy.lua` and `scripts/recipes.lua`; an unknown string is a **load-time error**, not a
silent fallback, because a typo would quietly re-admit a space-era entry to the ancient roster.

The campaign's band is derived from `world_params::epoch_year` against the same 1700 threshold the
antiquity branch uses — below 1700 is ancient — and applied in `app::load_economy` right after the
registry loads. This is why a 0 CE campaign is never offered a Launchpad, the petroleum and
propellant chains, or the spacecraft chain.

**The band masks; it never removes.** A recipe's id is its index in the authored list and that id
is *stored* in `building_component.recipe`, so a filter that compacted the list would silently
repoint every building whose recipe sat after a filtered one. The registry therefore keeps two
paths, and the distinction matters to anyone adding a caller:

| Path | Functions | Behaviour |
|---|---|---|
| **Storage** | `get_recipe(id)`, `recipe_id(name)` | Absolute and band-independent. A stored id means the same recipe in every band. |
| **Browse** | `recipe_count(bt)`, `recipe_at(bt, i)` | Era-masked. Walking `[0, recipe_count)` walks *this campaign's* recipes. |

Every caller of the browse path means "the recipes available to me". `construct_building` is the
authoritative gate (refusing with `construction_result::era_locked`, distinct from `tech_locked`
because no research reaches it), and the Selection panel's build door filters on
`building_available` so the door does not offer what the gate would refuse. Guard:
`tools/verify/era_roster.cpp`.

The band is **not** ERAS.md's Era 1 / Era 2 — that axis is per-corp progression *within* the
industrial arc, gated on space access. The untagged (`any`) processing recipes shared by both
arcs are `food_rations` and `refined_copper`; everything else is banded.

---

## Alternate production methods

**Ruling (Ben, 2026-08-15): alternates with real trade-offs, not tier upgrades** (BL-430,
alternate production methods). `recipes.lua` lets several recipes share a primary output (the
Charcoal Burner and the Peat Kiln both → `charcoal`), and `recipe_registry::recipe_at` /
`recipe_count` browse every era-allowed recipe for a `processing_facility`, so a building's
Production dropdown (`construction_panel.cpp`) offers every era-allowed method in its group
interchangeably. What the mechanic adds is the *cost of choosing*.

**Switching costs something.** Changing a `processing_facility`'s active recipe through the
*player-grade* seam — the management UI's method dropdown, or `corp_command`'s `set_recipe` verb
(which the AI's `dial_recipe` margin-chase also goes through, so it pays the same price) —
debits a one-off credit cost and starts a cooldown, both authored in `economy.recipe_switch`
(`scripts/economy.lua`; `switch_cost = 12.0`, `cooldown_ticks = 6`). The single implementation is
`try_switch_recipe` (`economy_system.{hpp,cpp}`); a refusal (on cooldown, insufficient funds,
unknown/no-op recipe) mutates nothing. `building_component.recipe_switch_cooldown` carries the
countdown, decremented once per economy tick.

**The BL-079 reflex switch is deliberately exempt.** A building whose output has floored
(`economy_system.cpp`'s loss-streak rescue) may still auto-switch to a healthier recipe for free
and instantly — that is sanctioned, narrow, local auto-agency reacting to a sustained loss, not a
strategic commitment, and it never calls `try_switch_recipe`. See
`.claude/rules/io-standing-rules.md`'s BL-079 exception for the standing invariant this preserves.

**The no-dominance guard.** `tools/verify/chain_depth.cpp`'s `R2` is the live guard (moved from
`recipe_switch_harness.cpp`'s retired `R1` on 2026-08-16 — see that file's own header for the
retraction). It groups recipes by (primary output resource, era band) and buckets every
same-output sibling pair as a **supply route** (disjoint raws — which one a corp runs is decided
by deposit access, not price), a named **precondition pair** (differs by a placement fact the
recipe data cannot carry, e.g. atmosphere vs airless body), or a genuine **interchangeable
method** — and only the third bucket is compared, on input-basket cost (a fixed reference-price
snapshot, `world_gen.lua`'s `base_price`) against the chain depth of its deepest input. A pair
identical or split across the two axes is a real trade-off; a pair dominated on both is dead
content the moment its sibling is reachable. Wage and build-duration were considered and rejected
as guard axes: those live on `building_economics` (the *building type*), not the recipe, and every
`processing_facility` offers the same era-allowed recipe set — so those axes are identical across
every sibling by construction and carry no discriminating information.

**The four pairs once reported as "dominated" (NR-243) were a false positive of the old grouping,
not a balance defect.** `steel_from_iron_nickel`/`steel`, `peat_charcoal`/`charcoal` and
`glass`/`trade_goods` have disjoint raws — supply routes, not methods; the propellant pair is the
named atmosphere-vs-airless precondition. None of the four is compared by R2 today, and NR-243 is
closed on that basis rather than by a retune (BL-587, correcting NR-589).

**BL-587 (2026-08-23) authored the roster's first two genuine interchangeable methods**, since
until then the "methods" bucket was empty — BL-430's ruling had mechanism (the Method page
compares whatever R2 would call a method) with no content to exercise it. Both trade the chain-
depth axis: a deeper route that needs a good not yet reached, for a cheaper basket once it is.

| Sibling pair | Shallow route | Deep route (needs) |
|---|---|---|
| `charcoal` (ancient) | Charcoal Burner — `timber` only, required depth 0 | Coking Kiln — `timber` + a reagent quantity of `iron_blooms`, required depth 2 |
| `steel` (industrial) | Smelter — `iron_ore` + `coal`, required depth 0 | Bessemer Converter — `iron_ore` + `coal` + a reagent quantity of `machinery`, required depth 2 |

Neither deep route is a tier upgrade: it is unreachable until the shallow route (or some other
path to the same reagent) has already been run once, and its reference cost is lower only because
the guard's fixed snapshot does not price the reagent's own chain. A market where the reagent is
scarce can still make the shallow route the better-run choice — the point Ben raised authoring
this sprint (*"it's not always clear that a more advanced method is better, depending on which
market it builds to"*), and which `R2` now checks under **two** price snapshots (`BL-592`,
2026-08-24) rather than trusting one as the final word: `fuel_cheap` and `fuel_dear` bracket the
axis these two methods trade on, and a pair is flagged dominated only if it loses under **both** —
a genuine alternate needs to win under at least one. Neither Coking Kiln nor the Bessemer
Converter needed the second vector to pass (both already win on chain depth alone), so this is a
guard-rail for `BL-586`'s still-widening roster, not a fix to a currently-failing case.

**AI scoring.** `corp_ai.cpp`'s `dial_recipe` candidate does not price `switch_cost` into its
projected gain and does not pre-filter on `recipe_switch_cooldown`; the seam enforces both at
apply time, so a losing candidate is refused rather than mutating anything. A cost/cooldown-aware
scorer is real planner scope, recorded as a decision (NR-242).

---

## Sub-facility groups

**The generic split.** Every processing recipe runs on the one `building_type::processing_facility`
enum value. What distinguishes them is a `group` field on `recipe` (`recipe_registry.hpp`,
authored as `group = "..."` in `recipes.lua`, alongside `era`/`display_name`): a sub-facility
KIND — "Metal Foundry", "Food Processing" — so the generic building reads and behaves like
several recognizable facility kinds without a second `building_type` axis (BL-434, sub-facility
groups). Absent means the literal `"General"` (a real group name, not empty string, so the
switch-cost lookup below never needs a special case).

**The taxonomy.** Every authored recipe has a named group with at least one sibling — nothing
sits in the `"General"` catch-all:

| Group | Recipes (era) | Notes |
|---|---|---|
| Metal Foundry | `steel` (industrial), `steel_from_iron_nickel` (industrial), `steel_from_regolith` "In-Situ Smelter" (industrial), `steel_bessemer` "Bessemer Converter" (industrial), `refined_copper` (any), `iron_blooms` "Bloomery" (ancient), `steel_from_blooms` "Smithy" (ancient), `ordnance_from_blooms` "Smithy" (ancient), `toolmaker` "Toolmaker" (ancient) | Every route that smelts or shapes a structural metal — the industrial Smelter and the ancient Bloomery/Smithy chain both land here, since they reach the same terminal goods by different roads. The Bessemer Converter (BL-587) is the roster's first genuine interchangeable method rather than a disjoint-raw supply route — see § Alternate production methods. The Toolmaker (BL-586) is its deepest ancient member, needing a milled good (`planks`) alongside the smelted one. |
| Refinery | `refined_fuel` (industrial) | A singleton — a real, specific kind (distinct from Metal Foundry's smelting), not a forced catch-all; more refined-fuel-family recipes would join it rather than needing a rename. |
| Construction Materials | `ceramics_kiln` "Potter's Kiln" (ancient), `stonemason` "Stonemason" (ancient), `sawmill` "Sawmill" (ancient) | BL-586's new group: named buildings on existing raws (clay, stone, timber) that make construction-grade goods rather than trade goods or fuel — distinct from Artisan Goods below even though the Kiln and Potter & Weaver share `clay`, since the two produce different terminal goods and are never compared as siblings (chain_depth's R2 groups by output, not input). |
| Food Processing | `food_rations` (any), `hydroponics_bay` (industrial), `food_rations_milled` "Miller" (ancient) | Feeding the population, whether growing the produce (Hydroponics Bay) or milling it into rations (Food Processor, Miller). |
| Chemical Works | `propellant_atmospheric` (industrial), `propellant_electrolysis` (industrial) | The Chemical Plant's two propellant routes. |
| Electronics | `silicon` (industrial), `ree_alloy` (industrial), `electronics` (industrial), `electronics_contact_grade` "Contact-Grade Electronics Lab" (industrial) | The silicon/REE/electronics chain. |
| Advanced Fabrication | `machinery` (industrial), `alloys` (industrial), `ordnance` (industrial), `spacecraft_components` (industrial), `spacecraft_components_heavy` "Heavy Assembly Plant" (industrial), `shipwright` "Shipwright" (ancient) | Fabricator + Assembly Plant: goods assembled from refined inputs rather than smelted from ore. The Shipwright (BL-586 slice 2) is the group's first ancient member — new to the ANCIENT roster, not a new group overall, since it shares the name with the industrial machinery/alloys/spacecraft_components chain by the same "goods assembled from refined inputs" argument (planks + cloth, both already-processed goods, not raw hides or fibre). |
| Welfare Goods | `clean_water` (industrial), `consumer_goods` (industrial), `medical_supplies` (industrial) | The habitability tranche — Water Treatment Plant, Consumer Goods Factory, Pharmaceutical Lab. |
| Fuel Production | `charcoal` "Charcoal Burner" (ancient), `peat_charcoal` "Peat Kiln" (ancient), `charcoal_from_kiln` "Coking Kiln" (ancient) | Three producers of `charcoal`: the Burner and the Peat Kiln are disjoint-raw supply routes, but the Coking Kiln shares `timber` with the Burner and is the roster's first genuine interchangeable method (BL-587) — see § Alternate production methods. |
| Artisan Goods | `trade_goods` "Potter & Weaver" (ancient), `glass` "Glassworks" (ancient), `tannery` "Tannery" (ancient), `weaver` "Weaver" (ancient) | Four independent producers, each of a different terminal or intermediate good (`trade_goods_misc`, `leather`, `cloth`) — the Tannery and Weaver (BL-586 slice 2) join on the same "sells finished goods from a raw craft input" argument as the Potter & Weaver and Glassworks. |

Judgment calls recorded in `NEEDS_REVIEW.json`: whether Hydroponics Bay (an agriculture
*producer*, not a food *processor*) belongs in Food Processing or a standalone Agriculture group,
and whether Advanced Fabrication should instead split into a Fabricator group and an Assembly
group once more recipes land in either.

**Build door: one row per group, not per recipe.** `draw_construction_ledger`
(`selection_panel.cpp`) collapses `processing_facility` to one candidate row per GROUP: the
representative recipe is the highest-expected-profit era-allowed recipe in that group (ties keep
the first in authored order), and the row is labelled with the group name rather than the
recipe's own `display_name`. Placing it seeds the fresh building with that representative recipe
(`construct_building`'s trailing `recipe` parameter, via `ui.construction.pending_recipe`).
`recipe_registry::default_recipe_id(group)` gives any other caller (AI, a future seam) the same
"this group's default recipe" lookup without duplicating the era-mask walk.

**Switching within a group stays cheap; switching across groups is refused outright.**
`try_switch_recipe` (`economy_system.cpp`) looks up both the old and new recipe's `group` via the
registry: same group pays `switch_cost`/`cooldown_ticks` (`economy.recipe_switch`); different
groups are REFUSED (`recipe_switch_result::cross_group`), not priced. Ben's ruling: *"switching
methods can mean changing to a different building type — we should retire that completely. So
the only way to access a different building type is fully dismantling the building, then using
the tile selector to reselect and build a new building."* The Method page's candidate list
(`draw_production_method_section`, `selection_panel.cpp`) is filtered to the active recipe's own
group for the same reason — a cross-group recipe is never even offered as a choice, not merely
refused if picked. A recipe with no group tag (the empty-`b.recipe`/`no_recipe` case, e.g.
mid-construction) falls back to permitting the switch at the intra-group cost rather than
crashing or refusing on a data edge case — this seam must never crash on missing data.

`tools/verify/recipe_switch_harness.cpp`'s `S6` asserts the refusal mechanically (three dummy
recipes in two groups, in the same hand-built registry the `S1`-`S5` mechanism checks use): an
intra-group switch applies at the base cost, a cross-group switch is refused and mutates nothing.

---

## The prototype core

The hand-calibrated core the economy is tuned on:

- Extraction for the seven prototype resources (RESOURCES.md § Prototype subset) through the generic `extraction_site`.
- Processing for iron ore → steel, petroleum → refined fuel, and agricultural produce → food rations.
- Workforce scalar applied to both extraction and processing output.
- The shared per-`(corporation, body)` stockpile pool, with quantities incrementing each economy tick.
- Per-body market supply/demand aggregation with live price resolution — `base × √(demand/supply)`, banded and EMA-smoothed (`src/world/market_clearing.cpp`; the full model is `docs/economy/MARKETS.md`).
- A running per-corporation balance: sales income less input purchases, maintenance, and wages (`docs/economy/FINANCE.md`).
- The economy panel making all of the above observable.

Introducing a named building type is an enum extension on `building_type`, not an authoring pass; the named vocabulary in this document is design intent carried by the generic types, their `target_resource`, and their recipe groups.
