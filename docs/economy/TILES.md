# Project Io — Tile Design

Tiles are the smallest addressable unit of land. Every tile has a fixed character set at world generation — it does not change during play except through deliberate player development. The character has two independent axes: **terrain composition** (what the tile is made of) and **terrain landform** (its physical shape). Together they determine resource deposit potential, construction cost, habitability ceiling, and visual identity.

This document is the design authority for tile classification. The procedural generation rules that produce the authored world bodies are in `docs/generation/TILE_GENERATION.md` and implemented in `hard_coded_world.cpp`.

---

## Two-axis terrain model

### Terrain composition

Composition describes the material character of the tile — the geology, ecology, or surface type. It determines what resource deposits can appear, what terrain colour and visual identity it carries, and its base habitability ceiling.

| Composition | Description | Habitability ceiling | Primary deposit category |
|-------------|-------------|---------------------|--------------------------|
| Barren | Dry, dusty, minimal organic matter | Low | Iron ore, coal, petroleum, stone |
| Rocky | Hard rock outcrops, fractured surface | Low | Iron ore, copper ore, rare earth ore, stone |
| Volcanic | Geologically active, lava flows or recent ejecta | Very low | Rare earth ore, iron ore |
| Icy | Ice-dominated surface; frozen subsurface | Low | Water ice (only — see deposit tables) |
| Tundra | Cold but not ice-covered; sparse vegetation | Low–medium | Iron ore (surface), peat, stone |
| Grassland | Open fertile land; moderate climate | High | Agricultural produce, stone, sand |
| Forest | Dense tree cover | High | Timber, agricultural produce (clearing) |
| Wetland | Marsh, bog, floodplain | Medium–high | Agricultural produce, timber, clay |
| Ocean | Open deep water | — (no buildings) | Marine goods (deferred) |
| Regolith | Loose surface material on airless bodies | Very low | Regolith, stone (polar ice sits on icy tiles) |
| Metallic | High metal content; asteroid or ancient impact surface | Very low | Iron-nickel ore, platinum group metals, regolith |

**Habitable compositions** — grassland, forest, wetland — support population centres and amenity production. The others are primarily extraction or infrastructure terrain.

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

The combination of composition and landform produces the full tile identity. A **volcanic canyon** has the deposit profile of volcanic terrain (rare earth ore, iron ore) with the access difficulty of a canyon (high construction cost, erosion-exposed deposits). A **grassland valley** has high agricultural potential plus better habitability than a highland equivalent.

---

## Resource deposit profiles

The deposits that can appear on a tile are determined primarily by composition, modulated by landform.

### Composition deposit tables

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

This two-axis model is **implemented** (2026-06-14; see DEVLOG § "Two-axis terrain model + six-pass procedural generation"). The old single flat `terrain_type` enum was replaced in `components.hpp` by two enums, both carried on `tile_component`:

- `terrain_composition` — the geological/ecological type (11 values: barren, rocky, volcanic, icy, tundra, grassland, forest, wetland, ocean, regolith, metallic)
- `terrain_landform` — the physical shape (7 values: plains, highland, mountain, canyon, valley, crater, rift)

The prototype bodies are generated against the full model by the six-pass pipeline in `src/world/tile_generation.cpp` (see `docs/generation/TILE_GENERATION.md`); there is no remaining retrofit. Tuning refinements that remain open are tracked in `docs/development/BACKLOG.md` § Environment.

**Both axes now render (BL-231, 2026-07-31).** Until then the canvas drew *composition only* — `terrain_colour` switched on the composition enum and landform never reached the screen, so mountain, highland, canyon, valley, crater and rift all appeared as flat hexes despite driving build cost, hazard, habitability and deposit richness. Landform is now a second, independent render channel: a subtle **relief tint** for the common ground and a **glyph** for the four dramatic landforms. The split is authored in `docs/ui/CANVASES.md` § Terrain channels; the glyph shapes are catalogued in `docs/ui/ICONS.md` § Landform glyphs.

**Measured landform distribution** (`world_audit` § S3, the canonical seed — reported per body because the profile varies sharply between wet and airless bodies):

| Landform | System | Kepler (home) | Build cost |
|---|---|---|---|
| Plains | 77.0% | 89.8% | ×1.0 |
| Valley | 18.0% | 0.0% | ×1.1 |
| Highland | 3.5% | 7.7% | ×1.25 |
| Crater | 0.8% | 0.4% | ×1.3 |
| Mountain | 0.6% | 1.5% | ×2.0 |
| Canyon | 0.1% | 0.4% | ×1.5 |
| Rift | 0.1% | 0.3% | ×1.6 |

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

**Open tuning question raised by that measurement: Kepler generates no valley tiles at all.** Valley is assigned to unclaimed non-ocean ground below the height threshold (Pass 5), but on a wet body the ocean has already taken everything that low — so the ×1.1 fertile landform is unreachable on exactly the bodies where river valleys should be most characteristic. Dry bodies carry 20–27% valley. This is self-consistent rather than a defect, but it is a generation-tuning question, not a rendering one; it belongs with the tile-generation refinements (BL-051).
