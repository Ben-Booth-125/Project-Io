# Project Io — Production

Production converts tile resource deposits into tradeable goods through two stages: **extraction**, which harvests raw materials from tiles, and **processing**, which refines or manufactures higher-tier goods from those inputs. Workforce shapes throughput at both stages.

See **`docs/economy/RESOURCES.md`** for the full resource list, tier definitions, and prototype scope. The market model production sells into — clearing, price resolution, the order book — is **`docs/economy/MARKETS.md`**.

---

## Extraction

An extraction building placed on a tile reads the tile's deposit for its authored target resource (`building_component.target_resource`) and credits a fractional quantity to its corporation's stockpile pool **at each economy tick**.

Output rate is the product of three factors:

| Factor | Source | Effect |
|--------|--------|--------|
| Deposit richness | `tile_component.resource_deposit[r]` | Linear multiplier; richer tiles produce proportionally faster. |
| Workforce fraction | `building_component.workforce_assigned` (0–1) | Linear scalar. Zero workforce produces nothing. |
| Hazard penalty | `tile_component.hazard_level` (0–1) | Applied as `(1 − hazard)` multiplier. High-hazard tiles cost more to operate and yield less per worker. |

i.e. `output = base_rate × richness × workforce × (1 − hazard)`, where `base_rate` is a Lua-authored economic constant.

Deposits **deplete** (BL-079): `resource_deposit[r]` is the fixed **richness** (the rate multiplier above), while `resource_remaining[r]` is a finite reserve — seeded at generation to richness × a reserve factor — that extraction draws down each tick. As the reserve nears empty the output **tapers**, then the building reports the deposit **exhausted** and stops. Richness sets the rate; the reserve sets how long the tile pays out — the boom-bust arc a resource-dependent corporation rides (see `docs/development/backlog.json` § BL-079).

### Extraction buildings

> **Design targets, not enum values (recorded 2026-07-31).** The named building types below are
> **not** in the `building_type` enum (`src/world/components.hpp`), which has six values: `none`,
> `extraction_site`, `processing_facility`, `port`, `launchpad`, `inland_logistics_hub`. The
> prototype ships one **generic** extraction building: what an `extraction_site` *does* is its
> `building_component.target_resource` field, authored at placement — a "Mine" and an "Oil
> Platform" are the same type pointed at different deposits. The table stays as the design
> vocabulary the named types would carve out of that generic building.

Each building type targets a specific class of resource. Placement is valid only on tiles with a non-zero deposit of the target type, or on terrain where that deposit type can occur.

| Building | Target resources | Valid terrain | Era |
|----------|-----------------|---------------|-----|
| Mine | Iron ore, coal, silica, copper ore, rare earth ore, peat | Barren, rocky, volcanic, tundra | 0 |
| Oil Platform | Petroleum | Barren (geological deposit) | 0 |
| Quarry | Stone, sand, clay | All non-water compositions | 0 |
| Lumber Camp | Timber | Forest, wetland | 0 |
| Farm | Agricultural produce | Grassland, wetland, forest-adjacent | 0 |
| Ice Extractor | Water (from ice deposits) | Icy | 1 |
| Surface Extractor | Regolith, iron-nickel ore, platinum group metals | Regolith, metallic | 1 |
| Fishing Wharf (BL-168) | Agricultural produce | Coastal (any composition adjacent to ocean) | 0 |

**Fishing Wharf (BL-168), implemented.** The extraction_site's target resource is again
`agricultural_produce`, but the placement gate is coastal adjacency rather than a deposit: valid on
any tile with an ocean neighbour, deposit-agnostic. A tile can satisfy Farm's deposit rule, the
Fishing Wharf's coastal rule, both, or neither — they are two independent ways the same generic
extraction_site can reach agricultural_produce, not two building types (`placement_rules.cpp`
`can_place` / `can_place_in_world`, mirroring the existing Port coastal check via `is_coastal`).

The Mine covers all terrestrial hard-mineral deposits and adjusts its output to whatever the tile holds: the same building type on one volcanic tile yields rare earth ore and on another yields copper ore. The distinction between deposit types is in the tile data, not the building type. Off-world metallic deposits (iron-nickel ore, platinum group metals) are harvested by the Surface Extractor, the Era 1 airless-body counterpart to the Mine; both feed the same smelting chain, so the distinction is one of era and deployment environment, not of downstream product.

The Quarry and Lumber Camp exist specifically to harvest ambient resources (stone, timber, sand, clay) that are present at low levels on most tiles. They ensure every tile can be productive in some capacity, even if only as a local construction material source. They share the same workforce and hazard scalar model as other extraction buildings.

---

## Processing

A processing building holds a **recipe** (`building_component.recipe`, indexing the Lua recipe registry): a set of input resources consumed and a set of outputs produced per economy tick, with a fixed conversion rate authored in Lua. Recipes support multiple inputs and outputs and reagents (e.g. coal in the steel recipe is an input that yields no separate product).

Processing buildings draw inputs from — and add outputs to — the shared per-`(corporation, body)` stockpile pool (inputs are taken pool-first; any shortfall is auto-bought from the market, see § Stockpile and output flow). They use the same workforce scalar as extraction buildings.

When the inputs available cannot cover a full conversion, the building does **not** simply halt: it follows a **two-threshold partial run**. If the limiting input covers at least `T_full` of one conversion, the building runs at full rate; between `T_idle` and `T_full` it scales its output down to what the limiting input allows; below `T_idle` it idles. The two thresholds are tunable economic constants.

> **The thresholds are the no-market fallback only (recorded 2026-07-31).** On any body that
> hosts a market the thresholds are **bypassed**: the building runs a full batch (`run = 1.0`),
> draws inputs pool-first, and auto-buys the whole shortfall (`run_processing`,
> `src/world/economy_system.cpp`). The two-threshold model above governs only bodies with no
> market — today that is every body except Kepler. Note `scripts/economy.lua`'s comment block
> disagrees with **both** branches (it describes the mid-band as "run full, auto-buying"
> regardless of market) — flagged for a code-comment fix, not edited here.

### Recipes by building type

> **Design targets, not shipped recipes (recorded 2026-07-31).** `scripts/recipes.lua` holds
> **three** recipes: iron ore → steel, petroleum → refined fuel, agricultural produce → food
> rations — all single-input, on the one generic `processing_facility`. The named processor
> types below are not in the enum, and most of the goods in their tables have no `resource_type`
> value (RESOURCES.md § Prototype scope). The tables stay as the recipe design target.

A building type may support multiple recipes. The active recipe for a given building is configured by the player (or defaulted at construction). Multiple buildings of the same type on the same body can run different recipes simultaneously.

#### Smelter

| Inputs | Output | Era |
|--------|--------|-----|
| Iron ore + coal (reagent) | Steel | 0 |
| Iron-nickel ore | Steel | 1 |

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
| Refined fuel + liquid oxygen | Propellant | 0 |
| Atmospheric air (no stockpiled input) | Liquid oxygen | 0 |
| Water | Liquid oxygen | 1 |

On a body with an atmosphere, liquid oxygen is produced in Era 0 by cryogenic air separation — the Chemical Plant draws oxygen from the local atmosphere and consumes no stockpiled input (energy cost only, abstracted into the recipe rate). Propellant is therefore an Era 0 capability anywhere refined fuel is available. On airless bodies there is no atmosphere to separate, so the Era 1 water-electrolysis recipe is the only liquid-oxygen route off-world; closing the in-situ propellant loop there (water → liquid oxygen, refined fuel shipped or synthesised) is the defining Era 1 logistical problem.

#### Electronics Lab

| Inputs | Output | Era |
|--------|--------|-----|
| Silicon + refined copper + REE alloy | Electronics | 0 |

#### Fabricator

| Inputs | Output | Era |
|--------|--------|-----|
| Steel + refined copper | Machinery | 0 |
| Steel + REE alloy | Alloys | 0 |

#### Food Processor

| Inputs | Output | Era |
|--------|--------|-----|
| Agricultural produce | Food rations | 0 |

#### Hydroponics Bay (BL-166, implemented)

| Inputs | Output | Era |
|--------|--------|-----|
| Water + steel | Agricultural produce | 0 |

A processing_facility recipe (`scripts/recipes.lua` id 3, `hydroponics_bay`) that produces
`agricultural_produce` from refined inputs instead of a terrain deposit — no "energy" resource
exists in the prototype set, so water (irrigation) and steel (the bay's own structure) stand in for
it. Feeds the same Food Processor -> Food rations chain the Farm feeds; no new market good.
**Placement is terrain-gated the opposite way from the Farm:** only valid where the terrestrial
Farm deposit was NOT seeded (`resource_deposit[agricultural_produce] == 0`), keyed off the
processing_facility's target resource in `placement_rules::can_place` (mirror image of the
extraction deposit check; `deposit_present` is the rejection reason on Farm-viable terrain).

#### Assembly Plant

| Inputs | Output | Era |
|--------|--------|-----|
| Alloys + electronics | Spacecraft components | 1 |

---

## Amenity and habitability buildings

Amenity and habitability buildings do not produce tradeable industrial goods — they produce conditions. Their output is a change in the local habitability score or population growth rate, which feeds into workforce efficiency and long-term productive capacity. They are deferred from the prototype but designed here.

### Amenity buildings

| Building | Input | Effect | Land use set |
|----------|-------|--------|-------------|
| Park | — (no input; tile locked to amenity use) | Habitability bonus in vicinity | Amenity |
| Recreation Facility | Consumer goods (small ongoing consumption) | Habitability bonus; population satisfaction | Amenity |
| Cultural Centre | Consumer goods + luxury goods | Larger habitability bonus; reduces unrest | Amenity |

Amenity buildings are placed on tiles and lock them to amenity land use — no extraction is possible on that tile. High-habitability compositions (forest, grassland) give a larger base amenity value, so placing a Park on a forest tile is more valuable than on a barren tile. Leaving a habitable tile undeveloped has a smaller inherent amenity contribution without consuming the tile's potential.

### Habitability production buildings

These produce habitability goods (see `docs/economy/RESOURCES.md`) consumed by population centres.

| Building | Inputs | Output | Era |
|----------|--------|--------|-----|
| Water Treatment Plant | Water | Clean water | 0 |
| Construction Yard | Stone + timber | Building materials | 0 |
| Consumer Goods Factory | Food rations + refined goods | Consumer goods | 0 |
| Pharmaceutical Lab | Chemical outputs + agricultural produce | Medical supplies | 0 |
| Power Plant | Coal or petroleum | Utilities (abstracted) | 0 |

Habitability production buildings use processing building mechanics (recipe, workforce scalar, stockpile). They differ from industrial processing buildings only in that their outputs feed population demand rather than further industrial chains.

---

## Infrastructure buildings

Infrastructure buildings affect logistical or economic capacity rather than extraction or processing directly. They are designed here and implemented in later layers.

| Building | Function | Era | Layer |
|----------|----------|-----|-------|
| Port | Enables stockpile exchange and convoy dispatch on the body | 0 | 5 |
| Launchpad | Required to dispatch convoys to off-world bodies; consumes propellant per launch | 0 (built), 1 (operational) | 5 |
| Inland Logistics Hub | Land-mode logistics node: its tile discounts the A\* haul cost of any intra-body convoy routed through it (SUPPLY.md § Logistics-node discount). The player-placeable counterpart to a city's free-hub discount — extends the cheap land network out to remote sites. Produces nothing (0 workforce). | 0 | 5 |
| Orbital Port | Receives off-world convoys; required on any non-terrestrial body to accept supply | 1 | 5 |
| Warehouse | Increases the stockpile capacity of the body | Any | 5+ |
| Storage Depot | Increases the throughput cap for a body's stockpile (see logistics note below) | 0 | 5+ |

> **Implemented (BL-149, v0.1.1).** The **Inland Logistics Hub** is live as `building_type::inland_logistics_hub`: a placeable non-producing building whose tile joins the population-centre set that discounts intra-body haul cost (`dispatch_convoys`). Placement, cost (`economy.buildings.inland_logistics_hub`), the build-front-door affordance, and the hexagon marker glyph all landed; the discount reuses the BL-148 node scan.

The Launchpad is the physical gate to space: a corp must hold one on the source body before any inter-body convoy can depart (`corp_has_launchpad_on`, `src/world/supply_system.cpp`) — and today that presence check is the *whole* gate; per-launch propellant consumption is design intent with no code backing (propellant has no `resource_type` value). See **`docs/economy/ERAS.md`** for the designed Era 0→1 transition conditions and their implementation status.

---

## Logistics and transport capacity (open design note)

Transport capacity is a constraint on supply throughput, not a direct modifier on market price. The intended behaviour is: **a body that cannot move its output does not accumulate surplus, rather than accumulating surplus at a suppressed price**.

Concretely: if a mine produces iron ore faster than the body's port and convoy network can carry it away, the excess ore sits in the building's stockpile but does not flow into the market. The market sees normal or high iron ore prices (no supply glut) while production slows because the stockpile fills. This prevents unrealistically cheap goods appearing in landlocked markets, and makes transport infrastructure a genuine bottleneck rather than purely a cost reduction.

Possible implementations:
- **Storage cap per body:** each body has a maximum total stockpile. When full, extraction and processing buildings idle. Building Warehouses or Storage Depots raises the cap; building Ports raises the throughput rate at which stockpile drains to convoys.
- **Throughput rate separate from capacity:** stockpile can hold X units; Port can ship Y units per quarter. Production above Y/quarter accumulates until cap is hit, then production idles.
- **Implicit abstraction:** do not model storage separately; instead, market supply per quarter is capped at convoy throughput. Production that cannot be shipped simply does not enter the supply figures.

This remains **open** — but against a *shipped* Layer 5, not an undesigned one (updated
2026-07-31). BL-039 (supply convoys) is complete: `src/world/supply_system.cpp` moves goods
between markets, and nothing in it caps storage or throughput — pools hold unbounded quantities
and a dispatch pass moves whatever a shortfall wants. The storage-cap / throughput question above
is therefore still a genuine design decision, now about *adding a constraint to* the live convoy
layer rather than shaping an unbuilt one. The Storage Depot building is included in the
infrastructure table as a placeholder. See `docs/economy/SUPPLY.md` for what actually shipped.

---

## Workforce model

Landed as the per-`(corp, body)` pool model (rewritten 2026-07-31 to match
`run_economy_step` in `src/world/economy_system.cpp` and `compute_building_opex` in
`src/world/budget_system.cpp`; the design history is POPULATION.md § Workforce model).

**Supply** derives from the body's population centres (BL-042, workforce supply derivation):
each centre contributes labour by scale — `labour_by_scale` = 1 / 3 / 10 / 30 / 100 units for
scale 1–5. A corp's share of that body supply is its share of the building count there; a body
with no centres falls back to the authored `world::workforce_supply` figure (default 3.0).

**Demand** is the sum of `workforce_assigned` over the corp's producing buildings on the body
(extraction and processing only; ports and hubs demand no labour), capped by the body's
habitability cap `min(1, mean_hab / 0.6)` (BL-041, habitability gates workforce — complete).

**Contention** is `min(1, supply / demand)` per `(corp, body)`, applied uniformly: an over-built
corp sees *every* building throttled, none starved to zero. The scalar is then multiplied by
`workforce_efficiency(hab)` (`src/world/workforce.hpp`, BL-069): full labour at habitability
≥ 0.6, ramping linearly to 0.5× at 0. Effective workforce =
`workforce_assigned × contention`.

**Cost** follows the BL-049 wage/maintenance split (`compute_building_opex`): maintenance
carries a fixed **30 % material floor** charged even when decommissioned, plus a labour
remainder scaled by the workforce target (zero when decommissioned); wages are
`workforce_assigned × contention × base_wage × wt_scalar × hab` — paid on the labour actually
allocated, not the request.

`workforce_assigned` itself is an authored constant set at placement (0.5 for producing types,
0 for passive infrastructure) and is never player-edited. The **player lever is
`workforce_target`** (0–200 %), auto-solved by default (BL-181, below).

### Workforce target and the auto-solver (BL-181)

On top of the assigned fraction sits a player-facing **workforce target** (`building_component.workforce_target`, 0–200 % of nominal, default 100 %) — a scalar on both output and the labour portion of wages/maintenance, applied identically in `economy_system` and `budget_system`.

By default a player building's target is **auto-solved** each economy tick (`workforce_auto = true`): `solve_workforce_target` (economy_system.cpp) picks the target that maximises that building's estimated net profit this tick. Because the model is otherwise *linear* in the target (output, wages, and the labour part of maintenance all scale with it), a fixed-price optimum would be degenerate bang-bang (0 % or max) — the **interior optimum comes from the local market price response**: more output raises local supply, which lowers the clearing price (`base · √(demand/supply)`, `docs/economy/MARKETS.md`). The solver reprices each candidate tier against that response and takes the best net. It is deterministic (reads last tick's market state), **player-corp only**, and applies only to producing types (extraction / processing).

The target is the *heuristic*, not a hard goal: a manual tier chosen in the building-management UI **pins** the value and clears `workforce_auto`; the **Auto** control re-enables the solver. This is the sole sanctioned auto-action on the player's corp (see `.claude/rules/io-standing-rules.md` § player-corp exception).

*First-pass fidelity (BL-181):* labour contention is held at its current value, input-price response is ignored (inputs valued at the current price), and the tier search is coarse (10 % steps) — so the solved target can hunt by ±one step. A finer model, input-price response, and hysteresis are future work.

---

## Stockpile and output flow

Extraction and processing outputs accrue into a shared stockpile pool held per `(corporation, body)` (a world-level map, not the per-building `stockpile_component`, which is unused in the prototype economy). At the economy tick boundary:

1. **Supply** is the goods each corporation lists for sale — its surplus above what its own processors will consume that tick.
2. **Demand** is the total input shortfall auto-bought by processing buildings (inputs not covered by the corporation's own pool), plus any standing convoy cargo orders (from Layer 5 onward).
3. **Transactions clear at the resolved market price (BL-078).** Sales credit, and purchases debit, the corporation's balance at `market_component.price`, resolved each Tick from the supply/demand ratio as `base × √(demand/supply)`, clamped to the `[0.25×, 4×]` band and EMA-smoothed. Demand and supply are no longer flat/deposit-flooded: the nation substrate is redefined into a **price-elastic per-capita basket demand** and an **abstract nation-capacity supply** that clears demand only partially, leaving a live margin (the fillable opportunity gap). See `docs/economy/MARKETS.md` for the clearing model and `scripts/economy.lua` § `substrate` for the tunables.

---

## Construction pacing (BL-095)

Construction is a **market-gated, pay-as-you-build** process, not an instant purchase.
Placing a building gates only on affordability (the corp must be able to afford the whole
build to commit to it) and does **not** debit up front. Each economy tick a building under
construction:

- draws `resource_build_cost / build_duration_ticks` of its materials as **real market
  demand** (competing with population and other builds, bidding the local price up) and pays
  the resolved price for them, plus the same fraction of the flat `build_cost`;
- progresses at a **rate set by how much of that per-tick material need the local market can
  supply** — read from the market's recent throughput (`market_component.supply`, a *derived*
  figure, not a stored inventory): market supplies the full need → full speed; supplies part →
  **stretched** (up to `max_stretch ≈ 10×` the base duration); supplies less than
  `1/max_stretch` → **paused** until supply recovers.

So a build in a thin market slows or stalls rather than completing instantly, and its cost is
spread across the build. The front door shows the analog rate/ETA/paused status rather than a
binary reject. Tunables live in `scripts/economy.lua` § `construction`. A depletable real
market inventory (vs. the derived figure) is revisited in `backlog.json` § BL-130.

## Layer 3 prototype scope

Layer 3 implements:

- Extraction logic for Mine, Oil Platform, Farm, and Ice Extractor reading the seven prototype resources.
- Processing logic for Smelter (iron ore → steel), Refinery (petroleum → refined fuel), and Food Processor (agricultural produce → food rations).
- Workforce scalar applied to both extraction and processing output.
- The shared per-`(corporation, body)` stockpile pool, with quantities incrementing each economy tick.
- Per-body market supply/demand aggregation with live price resolution — `base × √(demand/supply)`, banded and EMA-smoothed (`src/world/market_clearing.cpp`; the full model is `docs/economy/MARKETS.md`). The "price resolution deferred" note that stood here was stale (corrected 2026-07-31).
- A running per-corporation balance: sales income less input purchases, maintenance, and wages.
- The Layer 3 economy panel making all of the above observable.

The `building_type` enum does **not** carry the named building set above (corrected 2026-07-31 —
the previous claim here was false). It holds six values: `none`, `extraction_site`,
`processing_facility`, `port`, `launchpad`, `inland_logistics_hub`. Introducing a named building
type is an enum extension, not just an authoring pass; the named vocabulary above is design
intent only until then.
