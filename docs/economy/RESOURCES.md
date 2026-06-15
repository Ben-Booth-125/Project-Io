# Project Io — Resources

Resources are the goods that flow through the economy: extracted from tiles, refined by processing buildings, assembled into products, and bought and sold through markets. Every resource has a base price derived from global rarity; local supply and demand shift the market price each Tick.

---

## Resource categories

Resources fall into three **production tiers** and two **value tracks**. The tiers reflect the manufacturing chain; the tracks reflect the purpose of the good.

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

Ambient and habitability resources exist at the edges of the market. They are worth producing and trading, but rarely the primary profit driver. Their value is to ensure every tile is economically meaningful in some way and that population welfare has a supply chain behind it.

---

## Tier 1 — Raw materials

Raw materials are extracted by buildings placed on tiles with matching deposits.

### Earth-sourced

Available in Era 0 and all subsequent eras. Found predominantly on habitable or rocky planets.

| Resource | Terrain affinity | Notes |
|----------|-----------------|-------|
| Iron ore | Barren, rocky, volcanic | Most common structural mineral; the backbone of early industry. |
| Coal | Barren (sedimentary) | Carbon-rich energy source; consumed as fuel and reagent in smelting. |
| Petroleum | Barren (geological) | Liquid hydrocarbon; chemical feedstock and fuel precursor. |
| Silica | Barren, rocky | Silicon dioxide; raw input for semiconductor-grade silicon and bulk construction. |
| Copper ore | Rocky, volcanic | Primary conductive metal ore. |
| Rare earth ore | Volcanic, rocky | Suite of critical minerals used in electronics and high-performance magnets. Low deposit concentration; high base price. |
| Agricultural produce | High-habitability, water-adjacent | Food crop output. Only viable on planets with substantial surface water and habitability above a settlement threshold. |

### Space-sourced

Available in Era 1 and beyond. Found predominantly on moons, asteroids, and outer bodies.

| Resource | Terrain affinity | Notes |
|----------|-----------------|-------|
| Water | Icy | Extracted from surface and subsurface ice by an Ice Extractor; the output is liquid water. Also the baseline life-support input for off-world populations. |
| Iron-nickel ore | Rocky (metallic asteroid) | Found in metallic asteroids; feeds the same smelting chain as iron ore and eliminates dependence on Earth-side steel once accessible. |
| Platinum group metals | Rocky, volcanic (asteroid) | Ultra-rare catalytic and industrial metals. Very low deposit concentration; extremely high base price. The primary high-value trade good of the asteroid belt. |
| Regolith | All terrain (airless bodies) | Loose surface dust and broken rock. Used for bulk construction in-situ; not typically traded (high mass, low unit value). Included in the resource model but excluded from market tables; see note below. |

**Regolith note.** Regolith is present on every tile of every airless body and serves as a local construction material rather than a tradeable commodity. It has no base price and does not appear in market supply or demand tables. It is defined in the resource enum so that building cost formulas referencing it are consistent with the rest of the model.

### Ambient raw materials (Tier 1)

Ambient resources are present at low deposit values on almost every eligible tile type. They ensure that every tile has at least one resource that can be extracted, even if it is not economically compelling to do so. They feed local construction and low-value production chains.

| Resource | Found on | Extraction building | Notes |
|----------|----------|---------------------|-------|
| Stone | All non-water, non-icy compositions | Quarry | Universal construction aggregate; base price very low. |
| Timber | Forest, wetland; sparse on grassland | Lumber Camp | Construction material and fuel; base price low. |
| Sand | Barren plains, barren canyon | Quarry | Glass precursor; construction aggregate. |
| Clay | Wetland, valley landform | Quarry | Ceramics and construction; base price low. |
| Peat | Tundra plains and valley | Mine (surface layer) | Fuel; pre-industrial energy source. |

Ambient resources feed into construction material chains (stone → building materials, timber → processed lumber) that reduce construction costs locally and support habitability-track production.

---

## Tier 2 — Refined goods

Refined goods are the primary goods in inter-body trade during the early game.

| Resource | Primary inputs | Processing building |
|----------|---------------|---------------------|
| Steel | Iron ore or iron-nickel ore (+ coal as reagent) | Smelter |
| Refined fuel | Petroleum | Refinery |
| Silicon | Silica | Refinery |
| Refined copper | Copper ore | Smelter |
| REE alloy | Rare earth ore | Refinery |
| Liquid oxygen | Atmospheric air (Era 0) or water (Era 1) | Chemical Plant |
| Food rations | Agricultural produce | Food Processor |

---

## Tier 3 — Products

Products are the highest-value goods and the primary driver of market price divergence. Most require multiple refined-good inputs.

| Resource | Primary inputs | Processing building |
|----------|---------------|---------------------|
| Machinery | Steel + refined copper | Fabricator |
| Electronics | Silicon + refined copper + REE alloy | Electronics Lab |
| Propellant | Refined fuel + liquid oxygen | Chemical Plant |
| Alloys | Steel + REE alloy | Fabricator |
| Spacecraft components | Alloys + electronics | Assembly Plant |

Propellant and spacecraft components are the key outputs enabling space access. Propellant is the operating cost of any launch; spacecraft components are consumed by infrastructure construction in orbit and on remote bodies.

---

## Habitability goods

Habitability goods are consumed by population centres for welfare rather than production. They do not feed industrial chains, but their supply raises local habitability, which raises workforce efficiency, which indirectly raises production throughput. Their profit margins are lower than industrial products, but they are load-bearing for any body that hosts a significant population.

| Resource | Primary inputs | Building | Effect if undersupplied |
|----------|---------------|----------|------------------------|
| Clean water | Water | Water Treatment Plant | Reduces habitability; suppresses population growth |
| Building materials | Stone + timber | Construction Yard | Increases construction cost of all buildings if scarce |
| Consumer goods | Food rations + refined goods (various) | Consumer Goods Factory | Reduces workforce efficiency |
| Medical supplies | Chemical outputs + agricultural | Pharmaceutical Lab | Reduces habitability; raises mortality (long-term) |
| Utilities | — (abstracted as budget cost) | Power Plant, Sanitation | Habitability floor drops without continuous supply |

Habitability goods are not in the prototype. Their market demand slots exist in `market_component.demand` and can be authored in a later pass. The buildings that produce them are listed in `docs/economy/PRODUCTION.md`.

---

## Prototype scope

The full resource list above is the design target. For the prototype (Layers 3–6), a representative subset of **seven resources** is live in code:

| Resource | Tier | Extraction building | Processing step |
|----------|------|---------------------|-----------------|
| Iron ore | 1 | Mine | → Steel (Smelter) |
| Petroleum | 1 | Oil Platform | → Refined fuel (Refinery) |
| Water | 1 | Ice Extractor | — |
| Agricultural produce | 1 | Farm | → Food rations (Food Processor) |
| Steel | 2 | — | from Iron ore |
| Refined fuel | 2 | — | from Petroleum |
| Food rations | 2 | — | from Agricultural produce |

All resource type enum values are defined from the start so array sizes are correct and no data-model retrofit is required later. Resources outside the prototype subset have zero tile deposits and no authored recipes; they do not appear in market tables until a later pass authors them.

The full resource count including ambient and habitability goods is approximately **35–40 entries**. This is a design target, not a final count; the exact list will be settled when ambient and habitability resources are authored.
