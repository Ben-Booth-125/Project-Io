# Project Io — Tile Design

Tiles are the smallest addressable unit of land. Every tile has a fixed character set at world generation — it does not change during play except through deliberate player development. The character has **three** independent axes: **terrain substrate** (what the ground is made of), **terrain cover** (what sits on it, which may be nothing), and **terrain landform** (its physical shape). Together they determine resource deposit potential, traversal cost, habitability ceiling, and visual identity.

This document is the design authority for tile classification. The procedural generation rules that produce the world bodies are in `docs/generation/TILE_GENERATION.md` and implemented in `src/world/tile_generation.cpp`.

---

## Three-axis terrain model

### Why there are three axes and not two

A single "composition" axis would be doing **three unrelated jobs at once**:

| Job | Values it would hold |
|---|---|
| **Substrate** — what the ground is made of | barren, rocky, volcanic, metallic, regolith |
| **Cover** — what is growing or lying on it | forest, grassland, tundra, wetland |
| **State** — what has happened to it | urban, icy, ocean |

Ben's brief (2026-08-21, the ruling the model is built on) was *"a mountain might have a forest or
not"*. A mountain **with** a forest is expressible on two axes (`composition = forest` ×
`landform = mountain`). What cannot be said is *a **rocky** mountain that happens to be forested* —
because the one slot has been spent on the forest.

`urban` is the proof rather than the exception. A one-way transform that **overwrites** a single
composition slot destroys the fact that a paved tile was metallic. Separating the mutable cover
from the fixed substrate is what makes the transform lossless.

**So the third axis adds no concept. It un-mixes one that would otherwise be overloaded.**

### Terrain substrate

What the ground is **made of** — the geology. Fixed at generation and **never transformed**;
nothing in the simulation rewrites a tile's substrate, which is the property that separates this
axis from cover. It decides which **mineral** deposits can appear, the base habitability ceiling,
and the terrain colour the lenses tint.

| Substrate | Description | Habitability ceiling | Primary deposit category |
|-----------|-------------|---------------------|--------------------------|
| Barren | Dry, dusty, minimal organic matter | Low | Iron ore, coal, petroleum, stone |
| Rocky | Hard rock outcrops, fractured surface | Low | Iron ore, copper ore, rare earth ore, stone |
| **Sedimentary** | Soil, silt and sandstone — the ground a biotic cover grows on | High | Decided by the cover (see below) |
| Volcanic | Geologically active, lava flows or recent ejecta | Very low | Rare earth ore, iron ore |
| Metallic | High metal content; asteroid or ancient impact surface | Very low | Iron-nickel ore, platinum group metals, regolith |
| Regolith | Loose surface material on airless bodies | Very low | Regolith, stone |
| Icy | Ice-dominated ground; frozen subsurface | Low | Water ice (only — see deposit tables) |
| Ocean | Open water, out of sight of land | — (no buildings) | None |
| Coast | Shallow sea with land alongside — the shoreline ring | — (no buildings) | None |
| Lake | Inland water with no path to the sea | — (no buildings) | None |

**Sedimentary is the substrate the biotic covers grow on.** Grassland, forest, wetland and tundra
are not four kinds of *ground*; they are **one** kind of ground under four kinds of cover, and
spending the ground slot on the cover is what makes "a rocky mountain that happens to be
forested" inexpressible.

### Water kinds: lake, coast and ocean

Ben, 2026-08-21: *"We can also draw provinces over the ocean, using 3-12 size coastal tile
provinces. That creates another tile type, which is ok. We can have lakes, coasts, and oceans.
Ocean provinces should be much larger, but not larger than say 80 tiles."*

Water lives on the **substrate** axis, because every consumer asks "is this water?" of the ground.
`ocean` means open water; `lake` and `coast` are the other two kinds, appended after it so
`ocean` keeps its id. A water tile carries `cover::none` whatever its kind — the cover axis
describes what grew on ground, and water has no ground. The water-kind design is BL-516 (lakes,
coasts and oceans).

The three kinds are **structural** — no threshold picks between them
(`tile_generation.cpp` § Pass 4e, `classify_water_kinds`):

1. Flood-fill the water into connected components on the body's hex grid (columns wrap).
2. **The sea** is the largest component; every other component is a **lake**. "Does not reach
   the sea" is the whole definition of a lake, so no size cut-off is needed or wanted.
3. Within the sea, a tile with at least one land neighbour is **coast** — the shoreline ring —
   and a tile with none is **ocean**.

**Almost every consumer asks "is this water?" and must keep asking exactly that.** That question
is `is_water()` in `components.hpp`, and it is the choke point: placement, the urban transform,
logistics traversal, river termination, terrain defence and attrition, nation and settlement land
counts all go through it. Two consumers genuinely care *which* water:

- **The road pass.** A crossing is a strait when it is short **and made of shore**. Length alone
  cannot tell a three-tile clip across the corner of an ocean from a three-tile channel between
  two shores; only the second gets a road.
- **`is_coastal`** (ports, the Fishing Wharf, coastal-only extraction) means the **sea**
  specifically. A lakeshore is not a coast.

Water carries no deposits, no buildings, no cover and no population on any of the three kinds —
the distinction is about connectivity and locality, not about resources.

### Terrain cover

What **sits on** the substrate — and it may be absent. **`none` is a first-class value**, and that
absence is the whole point of the axis: "a mountain with no forest" is not a different kind of
ground from "a mountain with one". Cover decides **biotic** deposits (timber, produce), defensive
cover and forage in combat, and the overlay pattern the tile texture draws (BL-520, tile
texturing). Unlike the substrate, a cover **can** change — `urban` is the one transform that
does so.

| Cover | Reads on | Why it earns a slot |
|---|---|---|
| None | anything | Bare ground is a real answer, not a missing one |
| Grass | sedimentary, barren | Open fertile cover; agricultural produce |
| Scrub | barren, rocky, sedimentary | The half-step that makes a treeline gradual instead of a hard edge. Tundra is a climate outcome, not a terrain slot (Ben, 2026-08-21): cold sedimentary ground reads as scrub or bare |
| Forest | sedimentary, rocky, volcanic | Ben's example case; timber |
| Marsh | sedimentary, on low landforms | Wetland; produce and clay, and it wants valley/plains |
| Snow | anything cold | Falls out of latitude × retained height with no new generation input |
| Dunes | barren, rocky | Explains why some barren ground is workable and some is not |
| Ash | volcanic | A hazard reading rather than a resource one |
| Salt | barren, low basins | Dry-basin crust. No salt resource exists in the roster (RESOURCES.md), so the cover carries no deposit |
| Urban | anything | The one-way transform — see below |

**Cover is graded, not binary** (Ben's call, 2026-08-21). `tile_component::cover_density` is a
0–255 scalar: sparse scrub at the bottom, closed canopy at the top. **One number, two consumers**,
which is why it earns a field rather than being two — the texture reads it as how heavily to draw
the pattern, and the economy reads it as biotic yield (timber richness, forage in combat). A
sparse wood both *looks* thin and *cuts* thin.

**Invariant:** density is 0 **if and only if** cover is `none`. `tile_axes_harness` asserts it over
generated worlds; it is not merely a convention.

**Habitable ground** — a biotic cover (grass, forest, marsh) on **sedimentary** substrate —
supports population centres and amenity production. Both halves matter: a forested crag is not a
cradle.

### What is deliberately NOT on these axes

Recorded so the model is not later widened to swallow them:

- **Edge features** live *between* tiles. `tile_component::river_edges` is a per-side bitmask and
  province borders are drawn against it (BL-515, organic province borders) — that is the shape.
  Cliffs, escarpments and coastline fit the same shape. These are what make a map read as
  terrain rather than as tiles.
- **Point features** occupy one tile and do not tile-fill: volcano cone, oasis, spring, cave mouth,
  waterfall. Cheap, disproportionately legible, and what a player names a place after.

Both are their own designs, outside the three axes.

### Urban transform

The substrate is fixed at generation, and the **cover** is the mutable axis: a tile whose
**non-extraction** building stack (processors, ports, hubs, admin, amenity, military base,
research institute — combined, not per type) fills its cap **transforms**, one-way, to
`cover = urban`. The transform is BL-366 (urban transform).

**The transform writes ONE axis.** Urban is a **cover** (Ben's call, 2026-08-21): the city erases
what *grew* here, and the geology underneath survives it — a metallic tile that builds up to a
city is still metallic. Extraction stacks by deposit richness (`k_richness_per_site`);
non-extraction stacking is bounded by the cap below (BL-193, building stacks).

**Non-extraction cap** (`non_extraction_stack_cap`, `placement_rules.cpp`) is a **substrate base
plus a cover modifier** — the substrate says how much weight the ground will take, the cover says
how much what grows on it gets in the way:

| Substrate | Base | Rationale |
|---|---|---|
| Sedimentary | 6 | Habitable; the settlement-favoured ground (POPULATION.md) |
| Barren, Rocky, Regolith, Metallic | 4 | Industrial-friendly, low habitability ceiling |
| Volcanic, Icy | 2 | Hostile; already carry the lowest habitability ceiling |
| Ocean, Coast, Lake | 0 (pinned, exempt) | No buildings at all, on any water kind — `can_place` refuses them |

| Cover | Modifier | Rationale |
|---|---|---|
| Urban | **12, pinned** | The paving decides the cap, not the geology under it. Soft-bounded in practice by workforce contention |
| Scrub | −3 on sedimentary, −1 elsewhere | On soil this pair **is** tundra, and tundra caps at 3 |
| Forest, Marsh | 0 on sedimentary, −1 elsewhere | On soil these are settlement-favoured; on rock, vegetation is one more thing to clear |
| Snow, Dunes, Ash | −1 | Something to clear |
| Grass, Salt, None | 0 | Nothing in the way |

Never below 1. The calibration table is in `placement_rules.cpp` and asserted by
`multi_building_tile_harness` R1 and `tile_axes_harness` A4. A forested crag is 3 where the bare
rock beside it is 4, and urban on metallic is 12.

Once urban, a tile takes **no new extraction or ambient-resource placement** — the ground is
built over. Extraction sites already standing when the transform fires are grandfathered; they
keep operating, unaffected. The transform also raises the tile's `habitability` to at least
`0.80` (never lowers it) — established infrastructure, not raw terrain, now bounds who can live
there. It is a pure function of tile state (no RNG) and fires identically for player, rival and
background corps.

**Marsh is a drainage feature, not a climate zone.** Every other biome falls out of the Pass 4
`(latitude band, moisture)` table alone. Marsh is the one that cannot: a marsh is defined by
*where the water fails to leave*, which is an elevation question, and the table gives elevation
no say. Generated from the table alone it goes effectively extinct, because the table reaches
wetland only from the subtropical and tropical wet cells and moisture is latitude-blind noise.

So Pass 4 has a **Pass 4b drainage override** (`tile_generation.cpp`): low-lying, high-moisture
ground carrying grassland or forest becomes wetland. "Low-lying" is the bottom **15%** of land by
elevation measured against *this body's own sea level* — a percentile, like the ocean threshold
itself, so the coastal-plain slice exists on every seed rather than depending on where a noise blob
landed. It is deliberately a post-table override that draws no RNG, so it cannot shift any later
draw, and Pass 5's landform distribution is unaffected by it.

It **extends** the table rather than contradicting it, and so stays out of the bands where the
table has already named what wet ground is: polar wet ground is icy, and subpolar wet ground is
tundra (scrub). Real subpolar peatland arguably belongs, but claiming it would overrule the table's
own answer for that band — and tundra scores 9 for settle quality against wetland's 58, so
converting it redraws the region map. Left as the table's call to make. Across the 120-seed
`earthlike_tile_census`, wetland runs at a median of roughly **4% of land** against Earth's ~6%;
coming in under Earth is expected and correct, since Earth's figure is carried substantially by
the boreal peatlands this pass deliberately declines to generate.

### Terrain landform

Landform describes the physical geography of the tile — its elevation, slope, and shape. It modifies the base properties set by the other two axes without changing them fundamentally.

| Landform | Description | Traversal cost | Notes |
|----------|-------------|----------------|-------|
| Plains | Flat, easy access | ×1.0 | Baseline. Default for most generated tiles. |
| Highland | Elevated plateau, moderate slope | ×1.25 | |
| Mountain | Steep peaks, difficult terrain | ×2.0 | Boosts mineral deposit richness. High hazard. |
| Canyon | Deep gorge; access from above | ×1.5 | Sheltered; deposits exposed by erosion. |
| Valley | Low area between higher terrain | ×1.1 | Fertile; good for agriculture and habitation. |
| Crater | Impact basin; common on airless bodies | ×1.3 | Crater rim/floor distinction is not modelled. |
| Rift | Geological fault zone | ×1.6 | Strong volcanic association. Elevated rare earth ore. |

The cost column is the multiplier `landform_logistics_cost` (`src/world/logistics.cpp`) charges
— the **traversal** cost the intra-body A\* pays, the one cost function LOGISTICS.md names. What
landform drives is movement cost, hazard, habitability and deposit richness. **Construction cost
does not read landform**; the hover card deliberately says "convoys pay ×N to cross" rather than
naming a build cost, so the UI teaches nothing untrue.

**Valley is assigned in Pass 5** to unclaimed land below an absolute height of `0.35`. On a wet
body the ocean has already taken that band, so valley is in practice a dry-body landform (dry
bodies carry 20–27% valley; a wet homeworld carries none) and its mid-elevation band routes to
highland instead. The Pass 4b percentile mask `lowland[]` is the shape that would make valley
body-relative.

The combination of the three axes produces the full tile identity. A **volcanic canyon** has the
deposit profile of volcanic ground (rare earth ore, iron ore) with the access difficulty of a
canyon (high traversal cost, erosion-exposed deposits). A **grassland valley** — grass on
sedimentary ground, in a valley — has high agricultural potential plus better habitability than a
highland equivalent. A **forested rocky mountain** carries timber from the cover *and* iron from
the substrate, defends like woodland on rock, and forages far better than the bare crag beside it.

---

## Resource deposit profiles

The deposits that can appear on a tile are determined by the terrain axes, modulated by landform.

**Which axis decides which deposit:**

- **Ore follows the SUBSTRATE.** Iron, copper, rare earth, iron-nickel, PGM, coal, silica, regolith
  and water ice all key on the ground alone. Dressing a tile with vegetation does not cost it its
  geology.
- **Timber and produce follow the COVER**, scaled by `cover_density`. Timber wants forest or marsh;
  produce wants grass, forest or marsh.
- **Peat** keys on the pair `sedimentary + scrub` (tundra), which nothing else produces.
- **Crops** (tobacco, spices, coffee, furs) follow the cover, because each is a claim about what
  grows here. Furs key on `scrub` — the ground where the trapping happens.

The payoff of separating the axes: **a forested metallic mountain carries both timber and ore.**
`tile_axes_harness` A5d asserts that tiles carrying both timber and iron exist on the homeworld.

### Deposit tables (by substrate, and by cover where the cover decides)

> **These tables are the shape of a deposit's distribution, not its delivered magnitude.** They
> are faithful to `generate_deposits`, but `generate_deposits` is one factor of four. Three
> **pure post-multiplies** run after it in `generate_body_tiles`, and they can move a row by an
> order of magnitude or delete it outright:
>
> 1. **`deposit_scalar`** (BL-114, world descriptor) — sparse 0.40 / lean 0.65 / standard 1.00,
>    world-wide.
> 2. **Planetology `endowment[r]`** (BL-167, planetology) — per-resource, and **0.0 removes the
>    resource outright rather than thinning it**. A generated world can therefore have *no coal at
>    all*, whatever the "Coal 30–140, rarity-gated" row below says: coal is zeroed unless the
>    biosphere reached land, petroleum unless the atmosphere oxygenated.
> 3. **Ore fields** — copper, petroleum, iron and coal each redistribute 45–65% of the world total
>    into 2–3 seeded fields, so the "Relative weight" column describes a **pre-field** quantity.
>
> `TILE_GENERATION.md` § Post-multiplies is authoritative on the chain.

Reading the tables (`generate_deposits`, `src/world/tile_generation.cpp`):

- Ranges are the richness roll `[lo, hi]`; a landform modifier multiplies the **upper bound
  only** (`roll_mod`), leaving the floor fixed.
- **Cover-keyed rows** (timber, produce) are further scaled by **thickness** — `0.6 + 0.55 ×
  cover_fraction(density)`, from 0.6× at sparse scrub to 1.15× at closed canopy, centred so a
  typical forest lands near 1.0.
- **Rarity-gated** rows (BL-040, full-set deposit authoring) appear with probability equal to
  the resource's seeded rarity scalar and are magnitude-scaled by it — they carry **no landform
  modifiers**, and draw from an independent rng stream so the calibrated rows are unaffected.
  Base rarities: silica 0.65, coal 0.60, iron-nickel 0.55, copper 0.50, rare earth 0.30, PGM 0.15
  (± a small seeded jitter).
- **Clay** is cross-cutting: any **valley** landform rolls clay 8–20, not just marsh. It is listed
  only in the marsh table below.
- **Water** tiles (ocean, coast, lake) carry nothing.

**Barren:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron ore | 0–150 | Mountain ×1.4, rift ×1.2 |
| Petroleum | 0–120 (all landforms) | Valley ×1.2 |
| Coal | 30–140, rarity-gated | — |
| Silica | 20–90, rarity-gated | — |
| Stone (ambient) | 10–30, always present | — |
| Sand (ambient) | 10–25, plains and canyon | — |

**Rocky:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron ore | 0–200 | Mountain ×1.5 |
| Silica | 20–100, rarity-gated | — |
| Copper ore | 30–160, rarity-gated | — |
| Rare earth ore | 10–70, rarity-gated | — |
| Stone (ambient) | 10–30, always present | — |

**Volcanic:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron ore | 0–150 | Rift ×1.3 |
| Copper ore | 30–180, rarity-gated | — |
| Rare earth ore | 20–100, rarity-gated | — |
| Stone (ambient) | 10–30, always present | — |

**Iron ore is the volcanic primary** (guaranteed roll); rare earth is rarity-gated at 0.30, so
it is both sparser and smaller than iron here.

**Icy:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Water | 0–400 | — |

Icy is **water-only** — no silica, and no stone ambient (the stone rule excludes icy alongside
water).

**Sedimentary + scrub (tundra):**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron ore | 0–60 | Mountain ×1.3 |
| Peat (ambient) | 5–15, plains and valley | — |
| Stone (ambient) | 10–30, always present | — |

**Sedimentary + grass (grassland):**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Agricultural produce | 40–180, × thickness | Valley ×1.3 |
| Stone (ambient) | 10–30, always present | — |

Grassland has **no sand and no timber** ambient — sand is barren-only, timber forest/marsh-only.

**Sedimentary + forest:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Agricultural produce | 10–80, × thickness | Valley ×1.15 |
| Timber (ambient) | 15–40, × thickness | — |
| Stone (ambient) | 10–30, always present | — |

**Sedimentary + marsh (wetland):**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Agricultural produce | 40–200, × thickness | — |
| Timber (ambient) | 15–40, × thickness | — |
| Clay (ambient) | 8–20 (also on any valley landform) | — |
| Stone (ambient) | 10–30, always present | — |

A **forest or marsh cover on any other substrate** (rocky, volcanic) rolls the same timber
ambient on top of that substrate's ore rows; only sedimentary rolls produce.

**Regolith:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Regolith | 20–50, always present | — |
| Stone (ambient) | 10–30, always present | — |

Regolith substrate carries **no water** — polar ice on airless bodies is icy-substrate tiles,
which carry the water.

**Metallic:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron ore | 50–250 (prototype primary) | — |
| Iron-nickel ore | 60–260, rarity-gated | — |
| Platinum group metals | 20–120, rarity-gated | — |
| Regolith | 20–50, always present | — |
| Stone (ambient) | 10–30, always present | — |

The metallic **primary maps to iron ore** in the prototype (the seven-resource subset);
iron-nickel ore and PGM ride the rarity-gated full-set pass on top.

---

## Ambient resources

Certain deposits are present on virtually every tile and require no special geological conditions. Their deposit values are low and their base prices are minimal, but they provide:

- Local construction inputs (stone, timber) without supply chain dependency
- Low-value trade goods that can sustain marginal markets
- A production option for any tile, even if not economically compelling

| Ambient resource | Found on | Building to extract |
|-----------------|----------|---------------------|
| Stone | All substrates except water **and icy** | Quarry |
| Timber | Forest and marsh cover only | Lumber Camp |
| Sand | Barren plains, barren canyon | Quarry (sand variant, or same building) |
| Clay | Marsh cover (any landform), or valley landform on any substrate | Quarry (clay variant) |
| Peat | Scrub on sedimentary, plains and valley | Mine (surface) |

Ambient resources are always generated at a low baseline deposit value on eligible tiles, so every tile has at least one extractable resource.

---

## Amenity tiles

Some tiles have intrinsic amenity value regardless of their extractable deposits. Amenity value contributes to local habitability and supports population growth, even when no extraction building is placed.

High-amenity ground: forest cover, coastal (grass cover + water-adjacent), marsh in a valley.

A tile's amenity potential can be realised directly (leaving it undeveloped as green space) or through amenity buildings (parks, recreation infrastructure). Placing an extraction building on a high-amenity tile reduces its amenity contribution while increasing resource output — a deliberate trade-off. See `docs/economy/POPULATION.md`.

---

## Data model

The enums live in `components.hpp`, all carried on `tile_component`:

- `terrain_substrate` — what the ground is made of (10 values: barren, rocky, sedimentary,
  volcanic, metallic, regolith, icy, ocean, coast, lake)
- `terrain_cover` — what sits on it (10 values: none, grass, scrub, forest, marsh, snow, dunes, ash,
  salt, urban), plus `cover_density`, a 0–255 scalar
- `terrain_landform` — the physical shape (7 values: plains, highland, mountain, canyon, valley,
  crater, rift)

**Hazard and habitability are derived, not authored** (`derive_environment`): the substrate
supplies a base pair (sedimentary 0.15 hazard / 0.80 habitability; volcanic 0.70 / 0.10; and so
on), the cover modulates it (marsh caps habitability at 0.65, scrub at 0.40 with hazard raised to
0.30; snow, dunes, ash and salt scale it down), and the landform applies a slope/exposure
modifier (valley ×1.15 habitability).

**Generation order.** Pass 4a — the (band, moisture) tables that decide a tile's biome — returns
an internal `biome` that Pass 4c decomposes onto the substrate and cover axes with **no RNG
stream** (density varies through a stateless fold), and Pass 4d dresses ground the biome table
leaves **bare**, also with no stream. So every downstream pass sees the same draws whatever the
cover axis does, and a change in cover is never attributable to stream drift. The 120-seed
`earthlike_tile_census` is the harness that holds that line.

**Tiles are never serialised** — they are regenerated from the seed (NR-430). The only
flat-binary streams are the history log, the order book and procurement. So a change to the tile
model needs no save migration.

The bodies are generated against the full model by the six-pass pipeline in
`src/world/tile_generation.cpp` (see `docs/generation/TILE_GENERATION.md`).

---

## Rendering

**All three axes render.** Substrate supplies the base hex colour (`terrain_colour`), cover
supplies the overlay texture scaled by density (BL-520, tile texturing), and landform is a third,
independent channel: a subtle **relief tint** (`landform_relief`, `src/ui/hex_render.hpp`) for
the common ground and a **glyph** for the dramatic landforms. Mountains read as raised ground
from the relief fill rather than from a glyph (BL-565, mountains read as elevation — relief
+0.45 for mountain, +0.25 for highland); canyon, crater and rift — which sink or pit, and which
relief serves far less well — keep their glyphs. The split is authored in `docs/ui/CANVASES.md`
§ Terrain channels; the glyph shapes are catalogued in `docs/ui/ICONS.md` § Landform glyphs.

**Runs are bridged, not repeated** (BL-232, landform spans). A contiguous run of the same linear
landform draws as **one** spanning marker rather than the same glyph stamped on each tile — rift
as one continuous fissure, canyon as paired rims. Crater never spans; a basin is a blob, not a
line. A lone tile keeps its centred glyph. The **hover card** names the landform in every variant
and states its traversal cost, so the glyph vocabulary is learnable without clicking through to
the Selection panel.

**Landform distribution is measured, not authored.** `tools/verify/world_audit.cpp` emits a
per-body census of landform shares over land tiles against `make_hard_coded_world()`; on an
earth-like homeworld plains dominate (~85–90% of land), highland runs ~10%, and mountain, crater,
canyon and rift together a few percent. Its S2 check asserts forest + marsh cover at least 3% of
the homeworld's land. The traversal multipliers are authored constants in
`landform_logistics_cost`, not measurements.

---

## Terrain as a combat quantity

SYSTEMS.md § Environment assigns terrain "the difficulty of military operations".
`src/world/terrain_combat.{hpp,cpp}` expresses it as three pure functions of
`(substrate, cover, density, landform)` — parallel in shape to `landform_logistics_cost`, with no
stored field and nothing on the serialisation seam:

| Function | Range | What it answers | Dominated by |
|---|---|---|---|
| `terrain_defence` | 0–1000 | How well the ground holds against an attacker | Landform |
| `terrain_attrition` | 0–1000 | What crossing costs an army, with no defender at all | Substrate |
| `terrain_resistance` | 0–1000 | The combined "this resists an army" quantity | Defence-weighted |

**Water is deliberately not a third scalar.** Open water is a movement *mode*, not a magnitude, so
all three functions return 0 for water and callers branch on it — the same way logistics
branches land/sea. The history ladder consumes this as `barrier_q = mean(terrain_resistance) over
land + ocean_share × 0.75`, the ocean weight being a tuning knob independent of terrain. The
campaign battle resolver reads the same two functions (`docs/military/MILITARY.md`) rather than
inventing a third answer.
