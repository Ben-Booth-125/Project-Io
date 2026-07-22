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
| **Mercantile** | **Endemic goods whose value comes from geography rather than utility** — they grow in one region and nowhere else, so their price is a function of distance from where they grow | Tobacco, spices, coffee, furs |

Ambient and habitability resources exist at the edges of the market. They are worth producing and trading, but rarely the primary profit driver. Their value is to ensure every tile is economically meaningful in some way and that population welfare has a supply chain behind it.

### Deposit rarity & scarcity (implemented — BL-040)

Deposit authoring covers the full raw-material set, driven by a **per-resource rarity scalar** — a
**seeded decimal in `[0, 1]`** (0 = effectively absent / trace, 1 = near-universal ambient). Rules:

- **Raw-tier (Tier 1) resources only carry a rarity scalar.** Refined and product resources are
  *made, not mined* — they receive no tile deposits, so scarcity does not apply to them.
- The scalar **modulates deposit frequency and magnitude** on top of the existing terrain
  affinity: a low scalar (e.g. platinum-group metals ≈ rare) keeps deposits sparse and small even
  on affine terrain; a high scalar (ambient stone/sand) approaches the every-tile ambient floor.
- The scalar is **seeded**, so a campaign's exact distribution varies but the rarity *ordering*
  (rare goods rare, ambient goods abundant) is stable, matching each resource's base-price
  rarity already noted in the Tier 1 tables.

**Implementation (`src/world/tile_generation.cpp`).** `build_rarity_profile()` builds the per-body
scalar field: the v0.0.4 seven-resource subset is pinned at `1.0` (the ambient-floor end — its
hand-calibrated authoring and the economy tuned on it are left bit-for-bit unchanged), and the
full-set additions (silica, coal, iron-nickel ore, copper ore, rare-earth ore, platinum-group
metals) carry fixed base rarities ordered by base price plus a small seeded jitter that varies the
campaign without disturbing the ordering. The additions are drawn on an **independent rng stream**
so they cannot perturb the calibrated subset or the derived environment — preserving determinism.
The distribution is guarded by the `world_audit` headless harness (BL-040 R1: all additions
authored; R2: PGM strictly rarer than copper). Metallic-only goods (iron-nickel, PGM) appear only
where metallic terrain exists — in Era 0 that is the asteroid belt, so they read as scarce until
space access opens.

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

## Mercantile — endemic trade goods (Tier 1, BL-191)

Implemented. Tier 1 raws like any other, but on a different **value track**: an industrial good is
worth something because it is *useful*; an endemic good is worth something because it only grows
**there**.

That distinction is the whole mechanic. It is also real history — nutmeg, cinchona and tobacco were
worth crossing oceans for not because they were useful like iron, but because you could not grow them
at home.

**Generated by the biosphere, in the C → D stage** (`docs/generation/PLANETOLOGY.md`). Each good
carries an **origin**: a latitude band (the climate it evolved in) *and* a longitude sector (the
region it actually arose in). The sector is what makes it endemic rather than merely climatic — two
worlds can both have temperate grassland and only one of them grow tobacco.

| Resource | Climate band | Terrain affinity | Notes |
|---|---|---|---|
| Tobacco | Temperate / subtropical | Grassland | Cured leaf crop; the archetypal colonial cash crop. |
| Spices | Tropical | Wetland, forest | Aromatics; the highest value-to-mass good in the set. |
| Coffee | Subtropical | Forest (highland-favouring) | Stimulant bean; typically the scarcest, and so the widest-margin. |
| Furs | Subpolar / polar | Tundra | Pelts; the one good that comes from the cold end of the map. |

**A world only carries the ones its own biosphere produced** — roughly 2–3 of the four, rolled per
world. A world with no land biosphere carries none at all, because the C → D stage sits downstream of
B → C: no forests, no cash crops.

### Pricing by distance

**No change to the clearing engine.** `market_component::base_price` is already per-market and
authored at world creation, so an endemic good is simply cheap at its origin and dear far from it;
supply and demand then push around that base exactly as for any other resource.

```
base_price = 1.5 x (1 + 7.0 x normalised_distance_to_nearest_source)
```

Measured on a generated world: tobacco 2.45 → 8.19 (×3.34), spices 3.72 → 7.56 (×2.03), coffee
1.84 → 8.59 (**×4.67**). At the far end these beat steel (8.0) — a distant market pays more for
coffee than for structural alloy, which is the point.

**The scarcest good commands the widest margin, and that is emergent rather than authored:** coffee
had 30 source tiles against tobacco's 179, so it is further from more markets. Nobody tuned that.

**Distance is physical for now.** "Geopolitical" earns its meaning when diplomacy and AI behaviour
land (Ben, 2026-07-22) — at which point the multiplier gains a political term rather than being
replaced.

### Why this exists

Every market previously traded the same industrial goods, so there was no structural reason to trade
*between* markets — only local shortage. Endemic goods create trade that exists because of **where
things are**.

And the premium does not stop at the coastline. An endemic good is by construction the thing that can
never be produced off-world, so it is the highest-margin cargo in the system — an Era 0 trade game
that becomes the reason early off-world runs pay for themselves, before the industrial chain reaches
out there.

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
