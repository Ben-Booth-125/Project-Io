# Project Io — Resources

Resources are the goods that flow through the economy: extracted from tiles, refined by processing buildings, assembled into products, and bought and sold through markets. A tradeable resource has a base price derived from rarity; local supply and demand shift the market price each Tick (the clearing model is `docs/economy/MARKETS.md`; which resources actually carry a base price is § What actually trades below).

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
| Agricultural produce | High-habitability, water-adjacent | Food crop output. Only viable on planets with substantial surface water and habitability above a settlement threshold. Three producers as of BL-166/BL-168: the Farm (terrain deposit), the Hydroponics Bay (a processing_facility recipe, only where the Farm's deposit was NOT seeded), and the Fishing Wharf (an extraction_site gated on coastal adjacency instead of a deposit) — see `docs/economy/PRODUCTION.md` § Extraction / § Processing. |

### Space-sourced

Available in Era 1 and beyond. Found predominantly on moons, asteroids, and outer bodies.

> **Water is not actually Era-gated (corrected 2026-07-31).** It trades on the home-body markets
> from tick 0 — an authored base price (1.5) in the Kepler market template — and sits in the
> substrate demand basket (weight 0.40). The "space-sourced / Era 1" framing describes where its
> *deposits* concentrate (icy terrain), not when it enters the economy.

| Resource | Terrain affinity | Notes |
|----------|-----------------|-------|
| Water | Icy | Deposits on icy terrain, extracted as liquid water; also the baseline life-support input for off-world populations. Trades terrestrially from tick 0 — see note above. |

### Logistics goods (BL-286, 2026-08-04)

Eight resources added for the army/unit logistics family. This entry is **BL-286 only** — enum +
serialisation + authored base price. None of the consumption, range-cap, shelf-life or purchase
mechanics are implemented, and **their follow-on items are not yet filed**. Allocate ids with
`node tools/session/next_id.js` when they are; do not cite one before it exists.

*(Corrected 2026-08-04. This section previously cited "BL-286–291" and named BL-287/288/289/290 as
the follow-ons. Those ids belong to unrelated items — BL-287 is the verify-tier build change,
BL-288 is the Release-only test failures — and BL-289–291 do not exist. BL-286's own design said
"not yet filed"; the caveat was dropped in transcription. The same bad ids are copied into a
`components.hpp` comment.)*

| Resource | Tier | Notes |
|----------|------|-------|
| Grain | 1 (raw) | Human ration staple. Per-tick army/unit draw is a follow-on, unfiled. |
| Fodder | 1 (raw) | Draft-animal/cavalry feed, drawn down alongside grain. Follow-on, unfiled. |
| Salt | 1 (raw) | Preservative. Intended to gate ration shelf-life. Follow-on, unfiled. |
| Transport capacity | 1 (abstract) | Logistics-train throughput good; intended to cap supply range. Follow-on, unfiled. |
| Charcoal | 2 (refined) | Refined fuel-wood; pre-coal smelting/heating input. No recipe yet. |
| Iron blooms | 2 (refined) | Bloomery-refined iron intermediate — distinct from raw iron ore / iron-nickel ore. No recipe yet. |
| Bullion | 2 (refined) | Minted precious-metal specie; intended as a local purchase medium via `resolve_price`. Follow-on, unfiled. |
| Trade goods (misc) | 1 (endemic, placeholder) | Generic endemic-luxury-class placeholder. Not a specific named luxury — that naming is a separate design step. |

All eight carry an authored mid-tier `base_price` in the Kepler market template
(`src/world/world_gen_config.hpp`). **They are priced but inert** — none has a tile deposit
(`generate_deposits` never emits them), a recipe (`recipes.lua` holds four, none producing them),
or a `demand_basket` entry. A good with no supply and no demand never clears, so no market will
ever price one in play; the base price is scaffolding for when the behaviour lands.
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

> **Steel, refined fuel, food rations, silicon, refined copper and REE alloy** exist in
> `resource_type` (silicon/refined copper/REE alloy landed 2026-08-11, BL-340). Liquid oxygen
> stays an unmodelled design target — it is folded into the Chemical Plant's two propellant
> recipes (BL-308) rather than given its own enum value, since nothing outside the plant would
> ever hold it.

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

> **Machinery, alloys, electronics and spacecraft components** exist in `resource_type`
> (landed 2026-08-11, BL-340); **propellant** already existed (BL-308). The whole tier is
> modelled — the "unmodelled design target" banner is retired.

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

> **None** of this table exists in `resource_type` — the whole track is an unmodelled design target (marked 2026-07-31).

| Resource | Primary inputs | Building | Effect if undersupplied |
|----------|---------------|----------|------------------------|
| Clean water | Water | Water Treatment Plant | Reduces habitability; suppresses population growth |
| Building materials | Stone + timber | Construction Yard | Increases construction cost of all buildings if scarce |
| Consumer goods | Food rations + refined goods (various) | Consumer Goods Factory | Reduces workforce efficiency |
| Medical supplies | Chemical outputs + agricultural | Pharmaceutical Lab | Reduces habitability; raises mortality (long-term) |
| Utilities | — (abstracted as budget cost) | Power Plant, Sanitation | Habitability floor drops without continuous supply |

Habitability goods are not in the prototype — and, having no enum values, they have **no** market demand slots either (the arrays are sized by `resource_count`; corrected 2026-07-31). Authoring them means the save-format retrofit described under § Prototype scope. The buildings that would produce them are listed in `docs/economy/PRODUCTION.md`.

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

The `resource_type` enum (`src/world/components.hpp`) holds **39 values** as of BL-340
(2026-08-11; 32 after BL-308, 31 after BL-286, 23 before it). The seven added by BL-340 are the
processing-chain roster — silicon, refined copper, REE alloy, machinery, alloys, electronics,
spacecraft components — closing the Tier 2/3 tables above bar liquid oxygen (folded into the
Chemical Plant's recipes, no enum value of its own by design) and the deferred habitability
goods. See `docs/economy/PRODUCTION.md` § Chemical Plant / § Launchpad for propellant, and this
file's Tier 2/3 tables above for the BL-340 roster's recipes. Adding a resource changes
`resource_count` and with it the width of every serialised `std::array<float, resource_count>` —
tile deposits and reserves, market supply/demand/price/base-price, stockpiles, nation abundance
and substrate capacity. **Extending the enum IS a save-format retrofit**, but every one of those
arrays is already sized off `resource_count` rather than a hardcoded width, so both BL-286's and
BL-340's extensions needed no manual per-array edit — only the enum + base-price/recipe authoring.
Habitability goods still have no enum value; they cannot be held, priced, or traded until BL-368
lands.

The full design list including ambient and habitability goods is approximately **35–40 entries**
— a design target now substantially covered. The shipped count is 39 (23 pre-BL-286 + 8 logistics
goods + propellant + 7 processing-chain roster), frozen for the prototype pending BL-368's
habitability tranche.

---

## What actually trades (recorded 2026-07-31; updated 2026-08-11 for BL-340)

The load-bearing fact for any market work: of the 39 enum values, only a fraction carry a
non-zero `base_price`, and `resolve_price` / the clearing pass ignore everything else
(`docs/economy/MARKETS.md`). The tradeable set is:

- **Seven authored base prices** (`world_gen_config.hpp`'s `kepler_base_price`, the Kepler
  market template): iron ore 2.5, petroleum 3.5, water 1.5, agricultural produce 3.0, steel 8.0,
  refined fuel 10.0, food rations 6.0.
- **Eight logistics-goods base prices** (BL-286, same `kepler_base_price` table): grain 2.0,
  fodder 1.2, salt 2.0, transport capacity 5.0, charcoal 4.0, iron blooms 6.0, bullion 50.0,
  trade goods (misc) 15.0 — mid-tier authored placeholders; no consumption/gate mechanic reads
  them yet (§ Logistics goods above).
- **Endemic goods** (tobacco, spices, coffee, furs — BL-191, endemic trade goods) with
  **distance-derived** per-market base prices: `1.5 × (1 + 7.0 × normalised distance to the
  nearest source)` (§ Mercantile below). Only the 2–3 goods the world's biosphere actually
  evolved get priced.
- **Six raws, newly priced (BL-340)**: coal 2.0, silica 2.0, copper ore 3.0, rare earth ore 6.0,
  iron-nickel ore 3.0, platinum-group metals 40.0 — closing the minable-but-unsellable asymmetry
  below.
- **Seven processing-chain goods (BL-340)**: silicon 5.0, refined copper 7.5, REE alloy 16.0,
  machinery 22.0, alloys 34.0, electronics 29.0, spacecraft components 140.0 — a deliberately
  widening margin ladder up the tiers (spacecraft components sits 56× iron ore).

Everything else has `base_price` 0 and is **never traded** — this no longer includes any BL-040
(full-set deposit authoring) raw; all six are priced above. Pricing the remainder (habitability
goods, most logistics goods) is part of the owed market-rework family (BL-130, real market
inventory, BL-368, and kin — MARKETS.md § Owed follow-ons).

Water is in this tradeable set from tick 0: it carries an authored base price on the home-body
markets and sits in the substrate demand basket (`scripts/economy.lua`, weight 0.40).
