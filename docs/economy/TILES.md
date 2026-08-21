# Project Io — Tile Design

Tiles are the smallest addressable unit of land. Every tile has a fixed character set at world generation — it does not change during play except through deliberate player development. The character has **three** independent axes (BL-519, 2026-08-21): **terrain substrate** (what the ground is made of), **terrain cover** (what sits on it, which may be nothing), and **terrain landform** (its physical shape). Together they determine resource deposit potential, construction cost, habitability ceiling, and visual identity.

This document is the design authority for tile classification. The procedural generation rules that produce the authored world bodies are in `docs/generation/TILE_GENERATION.md` and implemented in `hard_coded_world.cpp`.

---

## Three-axis terrain model (BL-519, 2026-08-21)

### Why there are three axes and not two

Until BL-519 there were two, and the first of them was doing **three unrelated jobs at once**:

| Job | Values it held |
|---|---|
| **Substrate** — what the ground is made of | barren, rocky, volcanic, metallic, regolith |
| **Cover** — what is growing or lying on it | forest, grassland, tundra, wetland |
| **State** — what has happened to it | urban, icy, ocean |

Ben's brief was *"a mountain might have a forest or not"*. A mountain **with** a forest was
already expressible (`composition = forest` × `landform = mountain`). What could not be said was
*a **rocky** mountain that happens to be forested* — because the one slot had been spent on the
forest.

`urban` was the proof rather than the exception. It is a one-way transform that **overwrote** the
composition, so paving a metallic tile destroyed the fact that it was metallic. That is a slot
conflict that had already shipped, and the split fixes it as a side effect.

**So this adds no concept. It un-mixes one that got overloaded.**

### Terrain substrate

What the ground is **made of** — the geology. Fixed at generation and **never transformed**;
nothing in the simulation rewrites a tile's substrate, which is the property that separates this
axis from cover. It decides which **mineral** deposits can appear, the base build cost, the base
habitability ceiling, and the terrain colour the lenses tint.

| Substrate | Description | Habitability ceiling | Primary deposit category |
|-----------|-------------|---------------------|--------------------------|
| Barren | Dry, dusty, minimal organic matter | Low | Iron ore, coal, petroleum, stone |
| Rocky | Hard rock outcrops, fractured surface | Low | Iron ore, copper ore, rare earth ore, stone |
| **Sedimentary** | Soil, silt and sandstone — the ground a biotic cover grows on | High | Decided by the cover (see below) |
| Volcanic | Geologically active, lava flows or recent ejecta | Very low | Rare earth ore, iron ore |
| Metallic | High metal content; asteroid or ancient impact surface | Very low | Iron-nickel ore, platinum group metals, regolith |
| Regolith | Loose surface material on airless bodies | Very low | Regolith, stone |
| Icy | Ice-dominated ground; frozen subsurface | Low | Water ice (only — see deposit tables) |
| Ocean | Open deep water | — (no buildings) | Marine goods (deferred) |

**Sedimentary is the axis's new value**, and the finding the item turned on: grassland, forest,
wetland and tundra were never four kinds of *ground*. They were **one** kind of ground under four
kinds of cover, and spending the slot on the cover is what made "a rocky mountain that happens to
be forested" inexpressible.

**Ocean is interim.** BL-516 splits water into lake / coast / ocean kinds. It lives on this axis
meanwhile because every existing consumer asks "is this water?" of the ground, and `is_ocean_tile`
stays a one-line substrate test.

### Terrain cover

What **sits on** the substrate — and it may be absent. **`none` is a first-class value**, and that
absence is the whole point of the axis: "a mountain with no forest" stops being a different kind of
ground from "a mountain with one". Cover decides **biotic** deposits (timber, produce), defensive
cover and forage in combat, and the overlay pattern BL-520 draws. Unlike the substrate, a cover
**can** change — `urban` is the one transform that does so today.

| Cover | Reads on | Why it earns a slot |
|---|---|---|
| None | anything | Bare ground is a real answer, not a missing one |
| Grass | sedimentary, barren | Open fertile cover; agricultural produce |
| Scrub | barren, rocky, sedimentary | The half-step that makes a treeline gradual instead of a hard edge. **This is where `tundra` went** — tundra was a climate outcome wearing a terrain slot (Ben, 2026-08-21) |
| Forest | sedimentary, rocky, volcanic | Ben's example case; timber |
| Marsh | sedimentary, on low landforms | Was `wetland`; produce and clay, and it wants valley/plains |
| Snow | anything cold | Falls out of latitude × BL-517's retained height with no new generation input |
| Dunes | barren, rocky | Explains why some barren ground is workable and some is not |
| Ash | volcanic | A hazard reading rather than a resource one |
| Salt | barren, low basins | Dry-basin crust; a home for a salt deposit if one is restored |
| Urban | anything | BL-366's one-way transform — see below |

**Cover is graded, not binary** (Ben's call, 2026-08-21). `tile_component::cover_density` is a
0–255 scalar: sparse scrub at the bottom, closed canopy at the top. **One number, two consumers**,
which is why it earns a field rather than being two — BL-520's texture reads it as how heavily to
draw the pattern, and the economy reads it as biotic yield (timber richness, forage in combat). A
sparse wood should both *look* thin and *cut* thin.

**Invariant:** density is 0 **if and only if** cover is `none`. `tile_axes_harness` asserts it over
generated worlds; it is not merely a convention.

**Habitable ground** — a biotic cover (grass, forest, marsh) on **sedimentary** substrate —
supports population centres and amenity production. Both halves matter: a forested crag is not a
cradle, and the pre-split model had no way to say so.

### What is deliberately NOT on these axes

Recorded so the model is not later widened to swallow them:

- **Edge features** live *between* tiles. `tile_component::river_edges` is already a per-side
  bitmask and BL-515 draws province borders against it — the precedent exists and works. Cliffs,
  escarpments and coastline fit the same shape. These are what make a map read as terrain rather
  than as tiles.
- **Point features** occupy one tile and do not tile-fill: volcano cone, oasis, spring, cave mouth,
  waterfall. Cheap, disproportionately legible, and what a player names a place after.

Both are separate items when wanted.

### Urban transform (BL-366)

The substrate is fixed at generation, and the **cover** is the mutable axis: a tile whose
**non-extraction** building stack (processors, ports, hubs, admin, amenity, military base,
research institute — combined, not per type) fills its cap **transforms**, one-way, to
`cover = urban`.

**Since BL-519 the transform writes ONE axis, and that is the shipped bug it fixed.** Before the
split it overwrote the composition, so a metallic tile that built up to a city stopped being
metallic. Urban is a **cover** (Ben's call, 2026-08-21): the city still erases what *grew* here,
and the geology underneath survives it. This
answers the half of BL-193 (building stacks) that stacking left open — extraction already stacks
by deposit richness (`k_richness_per_site`, unchanged by this item); non-extraction stacking is
now bounded by this cap instead of the old flat ceiling of 1.

**Non-extraction cap** (`non_extraction_stack_cap`, `placement_rules.cpp`). Since BL-519 it is a
**substrate base plus a cover modifier** — the substrate says how much weight the ground will take,
the cover says how much what grows on it gets in the way:

| Substrate | Base | Rationale |
|---|---|---|
| Sedimentary | 6 | Habitable; the settlement-favoured ground (POPULATION.md) |
| Barren, Rocky, Regolith, Metallic | 4 | Industrial-friendly, low habitability ceiling |
| Volcanic, Icy | 2 | Hostile; already carry the lowest habitability ceiling |
| Ocean | 0 (pinned, exempt) | No buildings at all — `can_place` already refuses it |

| Cover | Modifier | Rationale |
|---|---|---|
| Urban | **12, pinned** | The paving decides the cap, not the geology under it. Soft-bounded in practice by workforce contention |
| Scrub | −3 on sedimentary, −1 elsewhere | On soil this pair **is** the old `tundra` row, and its −3 reproduces BL-366's 3 exactly |
| Forest, Marsh | 0 on sedimentary, −1 elsewhere | On soil these were already settlement-favoured; on rock, vegetation is one more thing to clear |
| Snow, Dunes, Ash | −1 | Something to clear |
| Grass, Salt, None | 0 | Nothing in the way |

Never below 1. **Every pre-split number is reproduced exactly** — the calibration table is in
`placement_rules.cpp` and asserted by `multi_building_tile_harness` R1 and `tile_axes_harness` A4.
What is new is that the pairs the old model could not name now have caps too: a forested crag is
3 where the bare rock beside it is 4, and urban on metallic is still 12.

Once urban, a tile takes **no new extraction or ambient-resource placement** — the ground is
built over. Extraction sites already standing when the transform fired are grandfathered; they
keep operating, unaffected. The transform also raises the tile's `habitability` to at least
`0.80` (never lowers it) — established infrastructure, not raw terrain, now bounds who can live
there. It is a pure function of tile state (no RNG) and fires identically for player, rival and
background corps.

**Wetland is a drainage feature, not a climate zone (BL-338, 2026-08-09).** Every other
composition falls out of the Pass 4 `(latitude band, moisture)` table alone. Wetland is the one
that cannot: a marsh is defined by *where the water fails to leave*, which is an elevation
question, and elevation had no say in composition at all. Generated from the table alone it went
effectively extinct — the home body carried **12 wetland tiles, 0.20% of land**, because the table
reaches wetland only from the subtropical and tropical wet cells (16% of Kepler's rows), moisture
is latitude-blind noise, and on the shipped seed that noise left exactly those rows dry.

So Pass 4 now has a **Pass 4b drainage override** (`tile_generation.cpp`): low-lying, high-moisture
ground carrying grassland or forest becomes wetland. "Low-lying" is the bottom **15%** of land by
elevation measured against *this body's own sea level* — a percentile, like the ocean threshold
itself, so the coastal-plain slice exists on every seed rather than depending on where a noise blob
landed. It is deliberately a post-table override that draws no RNG, so it cannot shift any later
draw, and Pass 5's landform distribution is bit-for-bit unchanged.

It **extends** the table rather than contradicting it, and so stays out of the bands where the
table has already named what wet ground is: polar wet ground is icy, and subpolar wet ground is
*tundra*. Real subpolar peatland arguably belongs, but claiming it would overrule the table's own
answer for that band — and tundra scores 9 for settle quality against wetland's 58, so converting
it redraws the region map. Left as the table's call to make.

Measured: Kepler **159 wetland tiles, 2.61% of land** (from 0.20%); across the 120-seed
`earthlike_tile_census`, wetland median **4.3% of land** against Earth's ~6%. Coming in under Earth
is expected and correct — Earth's figure is carried substantially by the boreal peatlands this
pass deliberately declines to generate.

### Terrain landform

Landform describes the physical geography of the tile — its elevation, slope, and shape. It modifies the base properties set by composition without changing them fundamentally.

| Landform | Description | Build cost modifier | Notes |
|----------|-------------|---------------------|-------|
| Plains | Flat, easy access | ×1.0 | Baseline. Default for most generated tiles. |
| Highland | Elevated plateau, moderate slope | ×1.25 | |
| Mountain | Steep peaks, difficult terrain | ×2.0 | Boosts mineral deposit richness. High hazard. |
| Canyon | Deep gorge; access from above | ×1.5 | Sheltered; deposits exposed by erosion. |
| Valley | Low area between higher terrain | ×1.1 | Fertile; good for agriculture and habitation. |
| Crater | Impact basin; common on airless bodies | ×1.3 | Crater rim/floor distinction possible later. |
| Rift | Geological fault zone | ×1.6 | Strong volcanic association. Elevated rare earth ore. |

The combination of the three axes produces the full tile identity. A **volcanic canyon** has the
deposit profile of volcanic ground (rare earth ore, iron ore) with the access difficulty of a
canyon (high construction cost, erosion-exposed deposits). A **grassland valley** — grass on
sedimentary ground, in a valley — has high agricultural potential plus better habitability than a
highland equivalent. And since BL-519, a **forested rocky mountain** is expressible for the first
time: it carries timber from the cover *and* iron from the substrate, defends like woodland on
rock, and forages far better than the bare crag beside it.

---

## Resource deposit profiles

The deposits that can appear on a tile are determined by the terrain axes, modulated by landform.

**Which axis decides which deposit is the whole content of the BL-519 split, and it is a real
economic change rather than a renaming:**

- **Ore follows the SUBSTRATE.** Iron, copper, rare earth, iron-nickel, PGM, coal, silica, regolith
  and water ice all key on the ground alone. Dressing a tile with vegetation no longer costs it its
  geology.
- **Timber and produce follow the COVER**, scaled by `cover_density`. Timber wants forest or marsh;
  produce wants grass, forest or marsh.
- **Peat** is the old `tundra` row and keys on the pair `sedimentary + scrub`, which nothing else
  produces.
- **Crops** (tobacco, spices, coffee, furs) follow the cover, because each is a claim about what
  grows here. Furs moved from `tundra` to `scrub` — the ground where the trapping happens.

The payoff the single slot made impossible: **a forested metallic mountain now carries both timber
and ore.** Measured on the shipped homeworld, 110 tiles carry both timber and iron
(`tile_axes_harness` A5d).

### Deposit tables (by substrate, and by cover where the cover decides)

> **⚠ These tables are no longer the final word (2026-08-04).** They remain faithful to
> `generate_deposits` — spot-checked and accurate — but `generate_deposits` is now one factor of
> four. Three **pure post-multiplies** run after it in `generate_body_tiles`, and they can move a
> row by an order of magnitude or delete it outright:
>
> 1. **`deposit_scalar`** (BL-114) — sparse 0.40 / lean 0.65 / standard 1.00, world-wide.
> 2. **Planetology `endowment[r]`** (BL-167) — per-resource, and **0.0 removes the resource
>    outright rather than thinning it**. A generated world can therefore have *no coal at all*,
>    whatever the "Coal 30–140, rarity-gated" row below says: coal is zeroed unless the biosphere
>    reached land, petroleum unless the atmosphere oxygenated.
> 3. **Ore fields** (2026-08-04) — copper, petroleum, iron and coal each redistribute 45–65%
>    of the world total into 2–3 seeded fields, so the "Relative weight" column now describes a
>    **pre-field** quantity.
>
> Read the tables as the *shape* of a deposit's distribution, not its delivered magnitude.
> `TILE_GENERATION.md` § Post-multiplies is authoritative on the chain.

Regenerated 2026-07-31 from `generate_deposits` (`src/world/tile_generation.cpp`) — the earlier
tables carried ~10 row-level drifts against the code (petroleum landform rule, phantom icy
silica and regolith water, missing volcanic copper, invented rarity-row modifiers, and others).
Reading the tables:

- Ranges are the richness roll `[lo, hi]`; a landform modifier multiplies the **upper bound
  only** (`roll_mod`), leaving the floor fixed.
- **Rarity-gated** rows (BL-040, full-set deposit authoring) appear with probability equal to
  the resource's seeded rarity scalar and are magnitude-scaled by it — they carry **no landform
  modifiers**. Base rarities: silica 0.65, coal 0.60, iron-nickel 0.55, copper 0.50, rare
  earth 0.30, PGM 0.15 (± a small seeded jitter).
- **Clay** is cross-cutting: any composition's **valley** landform rolls clay 8–20, not just
  wetland. It is listed only in the wetland table below.
- **Ocean** tiles carry nothing.

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

Note the inversion from the old table: **iron ore is the volcanic primary** (guaranteed roll);
rare earth is rarity-gated at 0.30, so it is both sparser and smaller than iron here.

**Icy:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Water | 0–400 | — |

Icy is **water-only** — no silica, and no stone ambient (the stone rule excludes icy alongside
ocean).

**Tundra:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron ore | 0–60 | Mountain ×1.3 |
| Peat (ambient) | 5–15, plains and valley | — |
| Stone (ambient) | 10–30, always present | — |

**Grassland:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Agricultural produce | 40–180 | Valley ×1.3 |
| Stone (ambient) | 10–30, always present | — |

Grassland has **no sand and no timber** ambient — sand is barren-only, timber forest/wetland-only.

**Forest:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Agricultural produce | 10–80 | Valley ×1.15 |
| Timber (ambient) | 15–40 | — |
| Stone (ambient) | 10–30, always present | — |

**Wetland:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Agricultural produce | 40–200 | — |
| Timber (ambient) | 15–40 | — |
| Clay (ambient) | 8–20 (also on any valley landform) | — |
| Stone (ambient) | 10–30, always present | — |

**Regolith:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Regolith | 20–50, always present | — |
| Stone (ambient) | 10–30, always present | — |

Regolith composition carries **no water** — polar ice on airless bodies is icy-composition
tiles, which carry the water.

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
| Stone | All compositions except ocean **and icy** | Quarry |
| Timber | Forest, wetland only | Lumber Camp |
| Sand | Barren plains, barren canyon | Quarry (sand variant, or same building) |
| Clay | Wetland (any landform), or valley landform on any composition | Quarry (clay variant) |
| Peat | Tundra plains and valley | Mine (surface) |

Ambient resources are always generated at a low baseline deposit value on eligible tiles, so every tile has at least one extractable resource.

---

## Amenity tiles

Some tiles have intrinsic amenity value regardless of their extractable deposits. Amenity value contributes to local habitability and supports population growth, even when no extraction building is placed.

High-amenity compositions: forest, coastal (grassland + water-adjacent), wetland (valley landform).

A tile's amenity potential can be realised directly (leaving it undeveloped as green space) or through amenity buildings (parks, recreation infrastructure). Placing an extraction building on a high-amenity tile reduces its amenity contribution while increasing resource output — a deliberate trade-off. See `docs/economy/POPULATION.md`.

---

## Implementation note

The terrain model is **implemented**. It began as two axes (2026-06-14; see DEVLOG § "Two-axis
terrain model + six-pass procedural generation") and became **three** on 2026-08-21 (BL-519). The
enums live in `components.hpp`, all carried on `tile_component`:

- `terrain_substrate` — what the ground is made of (8 values: barren, rocky, sedimentary, volcanic,
  metallic, regolith, icy, ocean)
- `terrain_cover` — what sits on it (10 values: none, grass, scrub, forest, marsh, snow, dunes, ash,
  salt, urban), plus `cover_density`, a 0–255 scalar
- `terrain_landform` — the physical shape (7 values: plains, highland, mountain, canyon, valley,
  crater, rift) — **unchanged by the split**; it already did exactly one job

**How the split was landed, and why the numbers can be trusted.** Pass 4a — the (band, moisture)
tables that decide a tile's biome — is **unchanged in its values and, critically, in its RNG
consumption draw for draw**. It now returns an internal `biome` that Pass 4c decomposes onto the two
axes with **no RNG stream** (density varies through a stateless fold), and Pass 4d — the only
genuinely new behaviour — dresses ground the biome table left **bare**, also with no stream. So every
downstream pass sees the draws it saw before, and anything that moved is attributable to the new
cover axis rather than to stream drift. The 120-seed `earthlike_tile_census` came back
**bit-identical** to the pre-split baseline, which is the evidence for that claim.

**There is no save migration**, and that is why an item of this size was landable in one pass: tiles
are never serialised — they are regenerated from the seed (confirmed while landing BL-517, NR-430).
The only flat-binary streams are the history log, the order book and procurement.

The prototype bodies are generated against the full model by the six-pass pipeline in `src/world/tile_generation.cpp` (see `docs/generation/TILE_GENERATION.md`); there is no remaining retrofit. Tuning refinements that remain open are tracked in `docs/development/BACKLOG.md` § Environment.

**Both axes now render (BL-231, 2026-07-31).** Until then the canvas drew *composition only* — `terrain_colour` switched on the composition enum and landform never reached the screen, so mountain, highland, canyon, valley, crater and rift all appeared as flat hexes despite driving build cost, hazard, habitability and deposit richness. Landform is now a second, independent render channel: a subtle **relief tint** for the common ground and a **glyph** for the four dramatic landforms. The split is authored in `docs/ui/CANVASES.md` § Terrain channels; the glyph shapes are catalogued in `docs/ui/ICONS.md` § Landform glyphs.

**Measured landform distribution — re-issued 2026-08-07 (BL-291).**

Measured by `tools/verify/world_audit.cpp` against `make_hard_coded_world()` on the default seed.
This replaces the table superseded twice on 2026-08-04 by `802421c` (larger mountain clusters) and
`71e8a9b` (convergent-boundary seeding), and it retires the "cannot be re-measured, harness is
broken" banner that stood here: **the harness was never broken.** It builds, runs, and emits this
census in full; it exits non-zero on one unrelated assertion (see § Biome balance below), which had
been read as the whole harness failing.

Percentages are of each body's **land** tiles; ocean is excluded.

| Landform | System | Kepler (home) | Build cost |
|---|---|---|---|
| Plains | 68.8% | 83.1% | ×1.0 |
| Valley | 18.1% | 0.0% | ×1.1 |
| Highland | 10.7% | 13.1% | ×1.25 |
| Mountain | 1.4% | 2.2% | ×2.0 |
| Crater | 0.7% | 0.6% | ×1.3 |
| Canyon | 0.2% | 0.8% | ×1.5 |
| Rift | 0.1% | 0.3% | ×1.6 |

Land-tile counts behind the shares: system 25,412 — Cinder 15,120 (no ocean, so every tile is
land), Kepler 6,092 of 15,120, Selene 3,780, Pallas 420.

> **RE-MEASURED 2026-08-12 for the 3× homeworld grid** (180×84 → 312×145; `world_audit`, clean
> build). The figures above and in the table are the pre-change census and are kept for
> comparison; these are current:
>
> | Body | Land | Total |
> |---|---|---|
> | Cinder | 15,120 | 15,120 (no ocean) |
> | **Kepler** | **18,227** | **45,240** |
> | Selene | 3,780 | 3,780 |
> | Pallas | 420 | 420 |
> | **System** | **37,547** | — |
>
> System-wide landform shares: plains 75.8%, valley 12.1%, highland 9.6%, mountain 1.3%,
> crater 0.6%, canyon 0.3%, rift 0.1%. Kepler alone: plains 88.1%, highland 9.0%, mountain 1.4%,
> canyon 0.6%, crater 0.6%, rift 0.3%, **valley 0.0%**.
>
> **Kepler's biome balance improved, and its valley share is still zero.** Forest+wetland is now
> **10.96% of land** (1,090 forest + 907 wetland) against the ~7.4% the smaller grid produced,
> comfortably clear of the S2 ≥3% bar. The 0.0% valley finding recorded below survives the
> rescale unchanged, which is worth knowing: it is a property of the `(band, moisture)` table
> rather than of grid resolution.

**Relief lands lower than the 2026-08-04 note projected.** Mountain plus highland is **12.1%** system-wide
and **15.3%** on Kepler, against the ~14.0% that note recorded — it was quoting a home-body figure as
though it were the system one. **Kepler's 0.0% valley is real, not a stale artifact**: it survives
re-measurement, because Kepler's ring→landform mapping routes its mid-elevation band to highland
where the drier bodies route it to valley.

*(Build-cost multipliers are unaffected — they are authored constants in
`landform_logistics_cost`, not measurements.)*

**Biome balance — ⚠ the one live failure (2026-08-07).** `world_audit`'s S2 check wants
forest + wetland ≥ 3% of Kepler's tiles. It measures **2.41%** (forest 353 = 2.33%, wetland
**12 = 0.08%**) and fails. Wetland is the striking half: twelve tiles on the whole home body is
effectively extinct, not merely scarce. The likely cause is collateral from the same two relief
commits — more highland displaces the low, wet ground both biomes need — but that is untested.
Two things are unsettled and are Ben's call: whether 3% is still the right target after the relief
change, and whether the denominator should be *all* tiles or *land* tiles (against land, the same
measurement reads 6.0%). Filed as a review entry rather than retuned silently.

**Terrain has a graded combat value (BL-233, 2026-07-31).** SYSTEMS.md § Environment has always
assigned terrain "the difficulty of military operations", but the only code expressing it was a
single boolean in the history ladder, so a mountain and a barren plain scored identically and a
forest scored nothing. `src/world/terrain_combat.{hpp,cpp}` replaces it with two pure functions of
`(composition, landform)` — parallel in shape to `landform_logistics_cost`, with no stored field
and nothing on the serialisation seam:

| Function | Range | What it answers | Dominated by |
|---|---|---|---|
| `terrain_defence` | 0–1000 | How well the ground holds against an attacker | Landform |
| `terrain_attrition` | 0–1000 | What crossing costs an army, with no defender at all | Composition |
| `terrain_resistance` | 0–1000 | The combined "this resists an army" quantity | Defence-weighted |

**Water is deliberately not a third scalar.** Open water is a movement *mode*, not a magnitude, so
all three functions return 0 for ocean and callers branch on it — the same way logistics already
branches land/sea. The history ladder consumes this as `barrier_q = mean(terrain_resistance) over
land + ocean_share × 0.75`, the ocean weight being a tuning knob independent of terrain.

This adds **no** combat resolution, units, unit stats, fortifications or AI — all still excluded by
`TECH_FOUNDATIONS.md` § Prototype scope. Today's only consumer is generation-time; a later combat
layer reads the same two functions rather than inventing a third answer. Measured effect, `world_audit`
§ S5: Pallas's barrier field was a flat **0** (no mountain or canyon, and compositions outside the
barren/icy pair) and is now **325**; per-body land means now separate (defence 80–135, attrition
509–777) where the boolean gave one bit.

**Runs are bridged, not repeated (BL-232, 2026-07-31).** A contiguous run of the same linear landform draws as **one** spanning marker rather than the same glyph stamped on each tile — mountain as a chain of peaks, rift as one continuous fissure, canyon as paired rims. Crater never spans; a basin is a blob, not a line. A lone tile keeps its centred glyph. Contiguity was measured first (`world_audit` § S4): 71% of mountain and 81% of rift tiles have a same-landform cardinal neighbour and modal run length is 2–3, so bridging fires on the majority; **no** tile anywhere has all four, so the designed "filled interior" case was cancelled before it was written. The **hover card** now names the landform in every variant and states its cost, so the glyph vocabulary is learnable without clicking through to the Selection panel.

> **Discrepancy: the "Build cost modifier" column above is not implemented (recorded 2026-07-31, BL-232).**
> No construction or placement path reads landform. The identical multiplier table
> (×1.0 / ×1.25 / ×2.0 / ×1.5 / ×1.1 / ×1.3 / ×1.6) *is* implemented, but as
> `landform_logistics_cost` (`src/world/logistics.cpp`) — the **traversal** cost the intra-body A*
> pays. What landform verifiably drives today is movement cost, hazard, habitability and deposit
> richness; build cost is design intent that was never wired. The hover card deliberately says
> "convoys pay ×N to cross" rather than naming a build cost, so the UI does not teach the player
> something untrue. **Whether landform should also modify build cost is a design call, not a bug
> fix** — it belongs with the tile-generation/economy refinements, not with a rendering item.

**Open tuning question raised by that measurement: Kepler generated no valley tiles at all.** Valley is assigned to unclaimed non-ocean ground below the height threshold (Pass 5), but on a wet body the ocean has already taken everything that low — so the ×1.1 fertile landform was unreachable on exactly the bodies where river valleys should be most characteristic. Dry bodies carried 20–27% valley. Self-consistent rather than a defect, but a generation-tuning question; it belongs with the tile-generation refinements (BL-051).

*Re-measured and it still holds (2026-08-09, closing BL-291). Pass 5's ring→landform mapping was rewritten and the ocean band moved after the original measurement, so this was re-run against the current generator: **Kepler 0.0% valley**, against Cinder 24.9%, Pallas 24.8% and Selene 19.8%. The wet-body/dry-body split is reproducible and is a real property of the generator, not a stale artifact.*

> **And the fix shape is now known (BL-338, 2026-08-09).** It is the same defect the wetland work
> above diagnosed, in the landform axis instead of the composition one: an **absolute** height cut
> (`height < 0.35`) cannot find low ground on a body whose ocean has already taken the bottom 60%
> of the heightmap. Pass 4b solves it by measuring elevation as a **percentile of land above this
> body's own sea level**, and `lowland[]` — the mask it builds in Pass 2 — is already sitting there
> for the valley fill to read. Kepler's 0.0% valley is a one-line change away whenever BL-051 is
> picked up; it was left alone here only because moving landform would have moved every relief
> number in the same commit as the biome change.
