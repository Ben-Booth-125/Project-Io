# Project Io — Production

Production converts tile resource deposits into tradeable goods through two stages: **extraction**, which harvests raw materials from tiles, and **processing**, which refines or manufactures higher-tier goods from those inputs. Workforce shapes throughput at both stages.

See **`docs/economy/RESOURCES.md`** for the full resource list, tier definitions, and prototype scope.

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

The Mine covers all terrestrial hard-mineral deposits and adjusts its output to whatever the tile holds: the same building type on one volcanic tile yields rare earth ore and on another yields copper ore. The distinction between deposit types is in the tile data, not the building type. Off-world metallic deposits (iron-nickel ore, platinum group metals) are harvested by the Surface Extractor, the Era 1 airless-body counterpart to the Mine; both feed the same smelting chain, so the distinction is one of era and deployment environment, not of downstream product.

The Quarry and Lumber Camp exist specifically to harvest ambient resources (stone, timber, sand, clay) that are present at low levels on most tiles. They ensure every tile can be productive in some capacity, even if only as a local construction material source. They share the same workforce and hazard scalar model as other extraction buildings.

---

## Processing

A processing building holds a **recipe** (`building_component.recipe`, indexing the Lua recipe registry): a set of input resources consumed and a set of outputs produced per economy tick, with a fixed conversion rate authored in Lua. Recipes support multiple inputs and outputs and reagents (e.g. coal in the steel recipe is an input that yields no separate product).

Processing buildings draw inputs from — and add outputs to — the shared per-`(corporation, body)` stockpile pool (inputs are taken pool-first; any shortfall is auto-bought from the market, see § Stockpile and output flow). They use the same workforce scalar as extraction buildings.

When the inputs available cannot cover a full conversion, the building does **not** simply halt: it follows a **two-threshold partial run**. If the limiting input covers at least `T_full` of one conversion, the building runs at full rate; between `T_idle` and `T_full` it scales its output down to what the limiting input allows; below `T_idle` it idles. The two thresholds are tunable economic constants.

### Recipes by building type

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
| Orbital Port | Receives off-world convoys; required on any non-terrestrial body to accept supply | 1 | 5 |
| Warehouse | Increases the stockpile capacity of the body | Any | 5+ |
| Storage Depot | Increases the throughput cap for a body's stockpile (see logistics note below) | 0 | 5+ |

The Launchpad is the physical gate to Era 1: it must be constructed on the home body before any convoy can depart for a space destination, and each operation consumes propellant. See **`docs/economy/ERAS.md`** for the full Era 0→1 transition conditions.

---

## Logistics and transport capacity (open design note)

Transport capacity is a constraint on supply throughput, not a direct modifier on market price. The intended behaviour is: **a body that cannot move its output does not accumulate surplus, rather than accumulating surplus at a suppressed price**.

Concretely: if a mine produces iron ore faster than the body's port and convoy network can carry it away, the excess ore sits in the building's stockpile but does not flow into the market. The market sees normal or high iron ore prices (no supply glut) while production slows because the stockpile fills. This prevents unrealistically cheap goods appearing in landlocked markets, and makes transport infrastructure a genuine bottleneck rather than purely a cost reduction.

Possible implementations:
- **Storage cap per body:** each body has a maximum total stockpile. When full, extraction and processing buildings idle. Building Warehouses or Storage Depots raises the cap; building Ports raises the throughput rate at which stockpile drains to convoys.
- **Throughput rate separate from capacity:** stockpile can hold X units; Port can ship Y units per quarter. Production above Y/quarter accumulates until cap is hit, then production idles.
- **Implicit abstraction:** do not model storage separately; instead, market supply per quarter is capped at convoy throughput. Production that cannot be shipped simply does not enter the supply figures.

This is marked **open** and will be decided when Layer 5 (supply routing) is designed. The Storage Depot building is included in the infrastructure table as a placeholder.

---

## Workforce model

Workforce is a corporation-wide pool divided across all active buildings. The `workforce_assigned` field on `building_component` (0.0–1.0) represents the fraction of that building's rated labour requirement currently staffed.

The full policy allocation system — where the player sets targets and shortages cascade automatically — is a post-prototype feature, and the corporation-wide labour pool itself is deferred to the population layer. In the prototype, `workforce_assigned` is an authored constant per building, making it a fixed modifier on output rate rather than a dynamically contested resource.

Staffed workforce carries an operating cost: each building incurs a per-tick **wage** of `workforce_assigned × base_wage` (a tunable constant), charged against the corporation's balance (see § Stockpile and output flow and the Budget item in `docs/development/BACKLOG.md`). A sensible `base_wage` is set now and refined once population centres model labour supply.

---

## Stockpile and output flow

Extraction and processing outputs accrue into a shared stockpile pool held per `(corporation, body)` (a world-level map, not the per-building `stockpile_component`, which is unused in the prototype economy). At the economy tick boundary:

1. **Supply** is the goods each corporation lists for sale — its surplus above what its own processors will consume that tick.
2. **Demand** is the total input shortfall auto-bought by processing buildings (inputs not covered by the corporation's own pool), plus any standing convoy cargo orders (from Layer 5 onward).
3. **Transactions clear at the resolved market price (BL-078).** Sales credit, and purchases debit, the corporation's balance at `market_component.price`, resolved each Tick from the supply/demand ratio as `base × √(demand/supply)`, clamped to the `[0.25×, 4×]` band and EMA-smoothed. Demand and supply are no longer flat/deposit-flooded: the nation substrate is redefined into a **price-elastic per-capita basket demand** and an **abstract nation-capacity supply** that clears demand only partially, leaving a live margin (the fillable opportunity gap). See `docs/economy/RESOURCES.md` and `scripts/economy.lua` § `substrate` for the model and its tunables.

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
- Per-body market supply/demand aggregation and base-price transactions (price resolution deferred).
- A running per-corporation balance: sales income less input purchases, maintenance, and wages.
- The Layer 3 economy panel making all of the above observable.

All building type enum values for the full set above are defined in the prototype data model. Buildings outside the prototype scope have no authored placement or recipe data; they will not appear in the game until a later implementation pass.
