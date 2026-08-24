# Project Io — Resources

Resources are the goods that flow through the economy: extracted from tiles, refined by processing buildings, assembled into products, and bought and sold through markets. A tradeable resource has a base price derived from rarity; local supply and demand shift the market price each Tick (the clearing model is `docs/economy/MARKETS.md`; which resources carry a base price is § What trades below).

The roster is **38** values of `resource_type` (`src/world/components.hpp`), and every one of them is held to the **admission rule** (`docs/economy/PRODUCTION.md`): a value earns its place by being consumed by an authored recipe or contracted for by a named actor, and nothing else gets in. A base price is not a behaviour — a good that is priced but produced by nothing and consumed by nothing is an orphan, and `tools/verify/chain_depth.cpp`'s R1 row (no orphan resources) and R1b row (producer and consumer reachable in the *same* era band) hold the line.

---

## Resource categories

Resources fall into three **production tiers** and four **value tracks**. The tiers reflect the manufacturing chain; the tracks reflect the purpose of the good.

### Production tiers

| Tier | Description | Examples |
|------|-------------|---------|
| 1 — Raw materials | Extracted directly from tile deposits | Iron ore, petroleum, water |
| 2 — Refined goods | Produced by processing one or more raw inputs | Steel, refined fuel, food rations |
| 3 — Products | Manufactured from refined goods; highest value, widest price divergence | Electronics, propellant, spacecraft components |

Price volatility and trade margins increase with tier. Raw materials are abundant and cheap on their source body; products are scarce and expensive wherever supply chains have not yet reached.

### Value tracks

| Track | Purpose | Examples |
|-------|---------|---------|
| Industrial | Inputs to production, trade, and military supply | Iron ore, steel, propellant |
| Ambient | Low-value surface materials; local construction inputs; foundation for every tile having at least one deposit | Stone, timber, sand, clay |
| Habitability | Goods consumed by population for welfare rather than production; indirect economic benefits via workforce efficiency and population growth | Clean water, consumer goods, medical supplies |
| **Mercantile** | **Endemic goods whose value comes from geography rather than utility** — they grow in one region and nowhere else, so their price is a function of distance from where they grow | Tobacco, spices, coffee, furs |

Ambient and habitability resources exist at the edges of the market. They are worth producing and trading, but rarely the primary profit driver. Their value is to ensure every tile is economically meaningful in some way and that population welfare has a supply chain behind it.

### Deposit rarity & scarcity

Deposit authoring covers the full raw-material set, driven by a **per-resource rarity scalar** — a
**seeded decimal in `[0, 1]`** (0 = effectively absent / trace, 1 = near-universal ambient). The
design is BL-040 (full-set deposit authoring). Rules:

- **Raw-tier (Tier 1) resources only carry a rarity scalar.** Refined and product resources are
  *made, not mined* — they receive no tile deposits, so scarcity does not apply to them.
- The scalar **modulates deposit frequency and magnitude** on top of the terrain affinity: a low
  scalar (e.g. platinum-group metals ≈ rare) keeps deposits sparse and small even on affine
  terrain; a high scalar (ambient stone/sand) approaches the every-tile ambient floor.
- The scalar is **seeded**, so a campaign's exact distribution varies but the rarity *ordering*
  (rare goods rare, ambient goods abundant) is stable, matching each resource's base-price
  rarity in the Tier 1 tables.

**Implementation (`src/world/tile_generation.cpp`).** `build_rarity_profile()` builds the per-body
scalar field: the seven-resource prototype subset is pinned at `1.0` (the ambient-floor end — its
hand-calibrated authoring and the economy tuned on it stay bit-for-bit fixed), and the full-set
additions (silica, coal, iron-nickel ore, copper ore, rare-earth ore, platinum-group metals) carry
fixed base rarities ordered by base price plus a small seeded jitter that varies the campaign
without disturbing the ordering. The additions are drawn on an **independent rng stream** so they
cannot perturb the calibrated subset or the derived environment — preserving determinism. The
distribution is guarded by the `world_audit` headless harness (all additions authored; PGM
strictly rarer than copper). Metallic-only goods (iron-nickel, PGM) appear only where metallic
terrain exists — in Era 0 that is the asteroid belt, so they read as scarce until space access
opens.

---

## Tier 1 — Raw materials

Raw materials are extracted by buildings placed on tiles with matching deposits.

> **Which terrain axis a deposit follows.** Tiles carry three axes — `terrain_substrate` (what
> the ground is made of), `terrain_cover` (what sits on it, graded by `cover_density`) and
> `terrain_landform` (its shape). **Ore follows the substrate; timber and produce follow the
> cover.** Because the two are separate slots, a forested metallic mountain carries timber *and*
> ore, never one at the expense of the other. The "Terrain affinity" columns below name the axis
> they mean; `docs/economy/TILES.md` is the authority for the axes themselves (BL-519,
> substrate/cover split).

### Earth-sourced

Available in Era 0 and all subsequent eras. Found predominantly on habitable or rocky planets.

| Resource | Terrain affinity | Notes |
|----------|-----------------|-------|
| Iron ore | **Substrate**: barren, rocky, volcanic, metallic; also sedimentary under scrub (surface iron) | Most common structural mineral; the backbone of early industry. |
| Coal | **Substrate**: barren. Ore-field siting additionally wants low, wet, **vegetated** ground — a **cover** test (grass, forest or marsh; scrub is excluded, sparse woody cover is not the swamp that lays down a seam) | Carbon-rich energy source; consumed as fuel and reagent in smelting. |
| Petroleum | **Substrate**: barren (marine legacy — ore-field siting wants ground that sat low) | Liquid hydrocarbon; chemical feedstock and fuel precursor. |
| Silica | **Substrate**: barren, rocky | Silicon dioxide; raw input for semiconductor-grade silicon and bulk construction. |
| Copper ore | **Substrate**: rocky, volcanic | Primary conductive metal ore. |
| Rare earth ore | **Substrate**: volcanic, rocky | Suite of critical minerals used in electronics and high-performance magnets. Low deposit concentration; high base price. |
| Agricultural produce | **Cover**: grass, forest or marsh — scaled by `cover_density` — on **substrate** sedimentary | Food crop output. Only viable on planets with substantial surface water and habitability above a settlement threshold. Three producers: the Farm (terrain deposit), the Hydroponics Bay (a processing_facility recipe, only where the Farm's deposit was NOT seeded), and the Fishing Wharf (an extraction_site gated on coastal adjacency instead of a deposit) — see `docs/economy/PRODUCTION.md` § Extraction / § Processing. |

### Space-sourced

Found predominantly on moons, asteroids, and outer bodies; reachable in Era 1 and beyond.

> **Water is not Era-gated.** It trades on the home-body markets from tick 0 — an authored base
> price (1.5) in the Kepler market template — and sits in the substrate demand basket (weight
> 0.40). The "space-sourced" framing describes where its *deposits* concentrate (icy terrain),
> not when it enters the economy.

| Resource | Terrain affinity | Notes |
|----------|-----------------|-------|
| Water | **Substrate**: icy | Deposits on icy ground, extracted as liquid water; also the baseline life-support input for off-world populations. Trades terrestrially from tick 0 — see note above. |
| Iron-nickel ore | Rocky (metallic asteroid) | Found in metallic asteroids; feeds the same smelting chain as iron ore and eliminates dependence on Earth-side steel once accessible. |
| Platinum group metals | Rocky, volcanic (asteroid) | Ultra-rare catalytic and industrial metals. Very low deposit concentration; extremely high base price (40.0). The primary high-value trade good of the asteroid belt — a good to be *sold*. It also has one consumer, `electronics_contact_grade`, a contact-grade/catalytic route to electronics at 0.5 PGM per unit: at base price 40 a premium alternative rather than a cheap bypass, so the belt export remains the obvious use. |
| Regolith | All terrain (airless bodies) | Loose surface dust and broken rock, present on every tile of every airless body at deposits of 20–50. Its purpose is **in-situ construction** — the route for building where you already are, not a trade good. |

**Regolith pricing and its one consumer are one decision (Ben, 2026-08-16, NR-257).** Regolith
carries a `base_price` of **0.6** — the cheapest good in the table, below the stone/sand bulk floor
of 1.0, which is what "high mass, low unit value" means for something on every airless tile. It is
priced because it is consumed: `steel_from_regolith` "In-Situ Smelter" (`scripts/recipes.lua`)
reduces regolith to steel at a deliberately poor **12:1**, and a recipe input pulls the good into
market demand at the tiles that run one — an unpriced input stalls the building forever. The ratio
and the price are coupled: at 8:1 a bulk-low price would have made the deliberately poor route the
most profitable steel in the game (clearing 3.2 against the Smelter's 1.0). At 12:1 it clears 0.8,
the worst of the three industrial steel routes, which is the authored intent.

### Ambient raw materials

Ambient resources are present at low deposit values on almost every eligible tile type. They ensure that every tile has at least one resource that can be extracted, even if it is not economically compelling to do so. They feed local construction and low-value production chains.

| Resource | Found on | Extraction building | Base price | Notes |
|----------|----------|---------------------|-----------|-------|
| Stone | **Substrate**: everything except ocean and icy | Quarry | 1.0 | Universal construction aggregate. |
| Timber | **Cover**: forest or marsh, scaled by `cover_density` — on ANY substrate, so a forested crag yields timber without giving up its ore | Lumber Camp | 1.5 | Construction material and fuel. |
| Sand | Barren plains, barren canyon | Quarry | 1.0 | Glass precursor; construction aggregate. |
| Clay | **Cover**: marsh; or the valley **landform** on any ground | Quarry | 1.2 | Ceramics and construction. |
| Peat | **Pair**: scrub cover on sedimentary substrate, plains or valley | Mine (surface layer) | 1.2 | Fuel; pre-industrial energy source. |

Ambient goods are priced as bulk commons: cheap, heavy, worth something only in volume or after
work. Stone and timber sit below iron ore (2.5) because they are everywhere; peat below both,
being the poor fuel. They feed the **ancient chain** (`docs/economy/PRODUCTION.md` § The ancient
chain) and the habitability-track producers.

### Ancient intermediates

Three Tier 2 goods belong to the ancient (pre-coal) chain rather than the industrial one. Each has a producer and a consumer on the ancient roster; see `docs/economy/PRODUCTION.md` § The ancient chain.

| Resource | Tier | Base price | Produced by | Notes |
|----------|------|-----------|-------------|-------|
| Charcoal | 2 (refined) | 4.0 | Charcoal Burner / Peat Kiln / Coking Kiln | Refined fuel-wood; pre-coal smelting/heating input. Dearer than the timber it comes from — a burn takes days and loses mass. The Coking Kiln (BL-587) is a genuine alternate method, not a third route — see PRODUCTION.md § Alternate production methods. |
| Iron blooms | 2 (refined) | 9.0 | Bloomery | Bloomery-refined iron intermediate — distinct from raw iron ore / iron-nickel ore. Carries the charcoal plus the ore. |
| Trade goods (misc) | 1 (endemic, placeholder) | 8.0 | Potter & Weaver, Glassworks | Generic endemic-luxury-class placeholder, priced as a modest trade good, not a treasure. A specific luxury name is a separate design step. |

**Adding a resource is an append at the END of the enum, with its behaviour filed in the same
change** — never an insertion at an interior position, which would repoint every id after it.

### Ancient roster, slice 1 (BL-585/BL-586, 2026-08-24)

Four more processing outputs, all on **existing** raws — no new tile deposit, no new extraction
target. `hides` and its two consumers (`leather`, `cloth`) are a deferred later slice: they need
a new extractable raw with real tile-generation deposits, deliberately not attempted here.

| Resource | Tier | Base price | Produced by | Notes |
|----------|------|-----------|-------------|-------|
| Ceramics | 2 (refined) | 3.4 | Potter's Kiln | Clay's second consumer, alongside Potter & Weaver's `trade_goods_misc`. Terminal — sold, not reprocessed. |
| Dressed stone | 2 (refined) | 2.9 | Stonemason | Stone's second consumer, alongside the Miller. Terminal — sold, not reprocessed. |
| Planks | 2 (refined) | 4.3 | Sawmill | Timber's third consumer. NOT terminal — feeds the Toolmaker below. |
| Tools | 2 (refined) | 25.5 | Toolmaker (blooms + planks) | Required depth 2 (both inputs' depth), so `depth(tools) = 3` — tied with the existing ancient ceiling, not past it. Terminal for now; BL-590 (per-building materials) gives it a real construction-material consumer. |

Every price above is DERIVED at the roster's observed ~1.433x markup over its input basket
(`recipes.lua`'s id-27 ordnance comment states the method), not picked.

### Ancient roster, slice 2 (BL-586, 2026-08-24)

The three chains slice 1 deliberately deferred: `hides` is the roster's first **endemic** raw —
lat/sector-restricted, richness-scored, rarer and regional, like tobacco/spices/coffee/furs above
(Ben's ruling) — not the plain cover-based ambient mechanic `fibre` uses. `fibre` is the ordinary
case: a grass/marsh crop, generated by the SAME cover-based ambient/biotic mechanic
`agricultural_produce` uses, additively.

| Resource | Tier | Base price | Produced by | Notes |
|----------|------|-----------|-------------|-------|
| Hides | 1 (endemic) | 2.5 | Endemic (temperate/subtropical grassland) | Dearer than the ambient bulk floor for being endemic — rarer, regional. Consumed by the Tannery. |
| Fibre | 1 (ambient) | 1.3 | Ambient (grassland, wetland), alongside Agricultural Produce | Same tier as Clay/Peat. Consumed by the Weaver. |
| Leather | 2 (refined) | 7.2 | Tannery (hides) | Terminal — sold, not reprocessed. |
| Cloth | 2 (refined) | 3.7 | Weaver (fibre) | NOT terminal — feeds the Shipwright below, alongside Planks. |
| Rigging | 2 (refined) | 14.5 | Shipwright (planks + cloth) | Required depth 1 (both inputs' depth), so `depth(rigging) = 2` — past the flat depth-1 ceiling every other slice-2 chain sits at. The roster's chosen name for the "terminal trade good" the design table calls for: ropework/cordage/tackle, in the same generic-material-noun register as Ceramics/Dressed Stone/Tools. Terminal — sold, not reprocessed. |

Every price above is DERIVED the same way as slice 1's: raws priced as bulk commons (hides a
little dearer for being endemic), refined goods at the roster's ~1.433x markup over their input
basket.

---

## Mercantile — endemic trade goods

Tier 1 raws like any other, but on a different **value track**: an industrial good is worth
something because it is *useful*; an endemic good is worth something because it only grows
**there**. The design is BL-191 (endemic trade goods).

That distinction is the whole mechanic. It is also real history — nutmeg, cinchona and tobacco were
worth crossing oceans for not because they were useful like iron, but because you could not grow them
at home.

**Generated by the biosphere, in the C → D stage** (`docs/generation/PLANETOLOGY.md`). Each good
carries an **origin**: a latitude band (the climate it evolved in) *and* a longitude sector (the
region it actually arose in). The sector is what makes it endemic rather than merely climatic — two
worlds can both have temperate grassland and only one of them grow tobacco.

| Resource | Climate band | Terrain affinity | Notes |
|---|---|---|---|
| Tobacco | Temperate / subtropical | **Cover**: grass | Cured leaf crop; the archetypal colonial cash crop. |
| Spices | Tropical | **Cover**: marsh or forest | Aromatics; the highest value-to-mass good in the set. |
| Coffee | Subtropical | **Cover**: forest (highland-favouring) | Stimulant bean; typically the scarcest, and so the widest-margin. |
| Furs | Subpolar / polar | **Cover**: scrub — the ground the trapping happens on | Pelts; the one good that comes from the cold end of the map. |

**A world only carries the ones its own biosphere produced** — roughly 2–3 of the four, rolled per
world. A world with no land biosphere carries none at all, because the C → D stage sits downstream of
B → C: no forests, no cash crops.

### Pricing by distance

**No change to the clearing engine.** `market_component::base_price` is per-market and authored at
world creation, so an endemic good is simply cheap at its origin and dear far from it; supply and
demand then push around that base exactly as for any other resource. The constants are
`endemic_pricing_params` (`src/world/world_gen_config.hpp`, authored under
`world_gen.kepler_market.endemic`).

```
base_price = 1.5 x (1 + 7.0 x normalised_distance_to_nearest_source)
```

Measured on a generated world: tobacco 2.45 → 8.19 (×3.34), spices 3.72 → 7.56 (×2.03), coffee
1.84 → 8.59 (**×4.67**). At the far end these beat steel (8.0) — a distant market pays more for
coffee than for structural alloy, which is the point.

**The scarcest good commands the widest margin, and that is emergent rather than authored:** coffee
had 30 source tiles against tobacco's 179, so it is further from more markets. Nobody tuned that.

**Distance is physical.** "Geopolitical" earns its meaning through diplomacy and AI behaviour
(Ben, 2026-07-22) — the multiplier gains a political term there rather than being replaced.

### Why this exists

If every market traded the same industrial goods there would be no structural reason to trade
*between* markets — only local shortage. Endemic goods create trade that exists because of **where
things are**.

And the premium does not stop at the coastline. An endemic good is by construction the thing that can
never be produced off-world, so it is the highest-margin cargo in the system — an Era 0 trade game
that becomes the reason early off-world runs pay for themselves, before the industrial chain reaches
out there.

---

## Tier 2 — Refined goods

Refined goods are the primary goods in inter-body trade during the early game.

| Resource | Primary inputs | Processing building | Base price |
|----------|---------------|---------------------|-----------|
| Steel | Iron ore or iron-nickel ore (+ coal as reagent) | Smelter | 8.0 |
| Refined fuel | Petroleum | Refinery | 10.0 |
| Silicon | Silica | Refinery | 5.0 |
| Refined copper | Copper ore | Smelter | 7.5 |
| REE alloy | Rare earth ore | Refinery | 16.0 |
| Food rations | Agricultural produce | Food Processor | 6.0 |

**Liquid oxygen has no enum value of its own.** It is folded into the Chemical Plant's two
propellant recipes — separated cryogenically from the local air on the Era 0 route, electrolysed
from water on the Era 1 route — because nothing outside the plant would ever hold it
(`docs/economy/PRODUCTION.md` § Chemical Plant).

---

## Tier 3 — Products

Products are the highest-value goods and the primary driver of market price divergence. Most require multiple refined-good inputs.

| Resource | Primary inputs | Processing building | Base price |
|----------|---------------|---------------------|-----------|
| Machinery | Steel + refined copper | Fabricator | 22.0 |
| Electronics | Silicon + refined copper + REE alloy | Electronics Lab | 29.0 |
| Propellant | Refined fuel + liquid oxygen | Chemical Plant | — (unpriced; consumed by the Launchpad, never sold) |
| Alloys | Steel + REE alloy | Fabricator | 34.0 |
| Spacecraft components | Alloys + electronics | Assembly Plant | 140.0 |
| **Ordnance** | **Steel + machinery** | **Fabricator**; also the **Smithy** on the ancient roster | 43.0 |

The margin ladder widens up the tiers — spacecraft components sits 56× iron ore — which is the
value gradient the space-equipment premise rests on. Propellant and spacecraft components are the
key outputs enabling space access. Propellant is the operating cost of any launch; spacecraft
components are consumed by infrastructure construction in orbit and on remote bodies.

### The two terminal goods

The tier ends in **two** places rather than one, and the pair is the point.

**Spacecraft components** is the *space* terminal — the object procurement contracts buy
(BL-350, procurement), with deliberately no background demand so the militia is its only buyer.
**Ordnance** is the *military* terminal, drawn per tick by unit upkeep (BL-454) rather than bought
as a lump. The player's trade is *"coloured directly with military use, and space equipment"*
(NR-120), and the roster carries one object for each half of that sentence. Ordnance is BL-457
(ordnance).

**One value, not three.** Ben's words were "supplies, rations, weapons". `food_rations` covers
rations; `medical_supplies` is a habitability good a population centre competes for, so borrowing
it would put an army and a city on one price. A distinct field ration or medical draw is an
**append with its behaviour filed in the same change** — never an interior insertion.

**Its price is derived, not picked.** The processing roster marks an output up over its input
basket by a strikingly tight ratio — machinery 1.419, alloys 1.417, electronics 1.415, spacecraft
components 1.443. Ordnance draws steel 8.0 + machinery 22.0 = 30.0, so its `base_price` of **43.0**
is a ratio of **1.433**, inside that band rather than beside it. Re-derive if either input's price
or the recipe quantities move.

**It has a producer in every era band.** The Fabricator recipe carries `era = "industrial"`, and
unit upkeep draws ordnance every tick regardless of band, so an ancient campaign (the default,
`epoch_year = 0`) needs an ancient producer too: the Smithy — the same building that turns
`iron_blooms + charcoal` into steel — carries an alternate recipe to `ordnance` at the identical
basket (`docs/economy/PRODUCTION.md` § The ancient chain). `chain_depth`'s R1b row (producer and
consumer reachable in the *same* concrete era band) is the check that a good admitted on its
consumer can also be made wherever it is drawn.

---

## Habitability goods

Habitability goods are consumed by population centres for welfare rather than production. They do not feed industrial chains, but their supply raises local habitability, which raises workforce efficiency, which indirectly raises production throughput. Their profit margins are lower than industrial products, but they are load-bearing for any body that hosts a significant population.

Three habitability goods are tradeable `resource_type` values — the three population centres
consume as goods (BL-368, habitability tranche). **Building materials and Utilities are
deliberately not resources**: Building materials feeds construction cost, a different consumption
path (`docs/economy/PRODUCTION.md`'s concern, not population demand); Utilities is an abstracted
budget cost with no resource identity of its own.

| Resource | Primary inputs | Building | Base price | Effect if undersupplied |
|----------|---------------|----------|-----------|------------------------|
| Clean water | Water | Water Treatment Plant | 3.0 | Reduces habitability; suppresses population growth |
| Consumer goods | Food rations + steel | Consumer Goods Factory | 12.0 | Reduces workforce efficiency |
| Medical supplies | Water + agricultural produce | Pharmaceutical Lab | 14.0 | Reduces habitability; raises mortality (long-term) |

The three carry recipes (`scripts/recipes.lua` ids 14–16, all on the generic
`processing_facility` — no dedicated `building_type` enum values) and are priced modestly above
their primary inputs — welfare goods, not high-margin industrial products. Their demand comes
from population centres directly (`inject_population_demand`, `docs/economy/MARKETS.md`
§ Population demand), not from the nation-substrate model. The undersupply effects belong to the
habitability feedback model in `docs/economy/POPULATION.md`.

---

## Prototype subset

A representative subset of **seven resources** is the hand-calibrated core the economy is tuned on:

| Resource | Tier | Extraction building | Processing step |
|----------|------|---------------------|-----------------|
| Iron ore | 1 | Mine | → Steel (Smelter) |
| Petroleum | 1 | Oil Platform | → Refined fuel (Refinery) |
| Water | 1 | Ice Extractor | — |
| Agricultural produce | 1 | Farm | → Food rations (Food Processor) |
| Steel | 2 | — | from Iron ore |
| Refined fuel | 2 | — | from Petroleum |
| Food rations | 2 | — | from Agricultural produce |

The `resource_type` enum holds **38 values**: 7 Earth-sourced raws, 4 space-sourced raws, 5
ambient raws, 4 endemic goods, 3 prototype refined goods, 3 ancient intermediates, propellant,
7 processing-chain goods, 3 habitability goods, and ordnance. Adding a resource changes
`resource_count` and with it the width of every serialised `std::array<float, resource_count>` —
tile deposits and reserves, market supply/demand/price/base-price, stockpiles, nation abundance
and substrate capacity. **Extending the enum IS a save-format retrofit**, but every one of those
arrays is sized off `resource_count` rather than a hardcoded width, so an extension needs no
per-array edit — only the enum value plus base-price and recipe authoring.

---

## What trades

The load-bearing fact for any market work: only a value with a non-zero `base_price` is
tradeable, and `resolve_price` / the clearing pass ignore everything else
(`docs/economy/MARKETS.md`). Base prices are authored in `scripts/world_gen.lua` under
`kepler_market.base_price` (the Kepler market template, loaded into `kepler_base_price` in
`src/world/world_gen_config.hpp`). The tradeable set is:

- **The prototype seven**: iron ore 2.5, petroleum 3.5, water 1.5, agricultural produce 3.0,
  steel 8.0, refined fuel 10.0, food rations 6.0.
- **The remaining industrial raws**: coal 2.0, silica 2.0, copper ore 3.0, rare earth ore 6.0,
  iron-nickel ore 3.0, platinum-group metals 40.0. A raw with an authored deposit and no price is
  minable-but-unsellable, and a processing building drawing on it stalls forever; every deposit
  raw is therefore priced (BL-340, processing roster).
- **The ambient raws and regolith**: stone 1.0, timber 1.5, sand 1.0, clay 1.2, peat 1.2,
  regolith 0.6 — the same rule applied to the ancient chain (BL-429, ancient chain).
- **The ancient intermediates**: charcoal 4.0, iron blooms 9.0, trade goods (misc) 8.0.
- **Endemic goods** (tobacco, spices, coffee, furs) with **distance-derived** per-market base
  prices: `1.5 × (1 + 7.0 × normalised distance to the nearest source)` (§ Mercantile). Only
  the 2–3 goods the world's biosphere actually evolved get priced.
- **The processing-chain goods**: silicon 5.0, refined copper 7.5, REE alloy 16.0, machinery
  22.0, alloys 34.0, electronics 29.0, spacecraft components 140.0.
- **The habitability tranche**: clean water 3.0, consumer goods 12.0, medical supplies 14.0.
- **Ordnance** 43.0.

**Propellant is the one value with no base price.** It is made in a Chemical Plant and burned by
a Launchpad, never mined and never sold, so it has no market presence.

Water is in this tradeable set from tick 0: it carries an authored base price on the home-body
markets and sits in the substrate demand basket (`scripts/economy.lua`, weight 0.40).
