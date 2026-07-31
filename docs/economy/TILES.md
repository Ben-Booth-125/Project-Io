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
| Icy | Ice-dominated surface; frozen subsurface | Low | Water ice, silica (trapped) |
| Tundra | Cold but not ice-covered; sparse vegetation | Low–medium | Iron ore (surface), peat, stone |
| Grassland | Open fertile land; moderate climate | High | Agricultural produce, stone, sand |
| Forest | Dense tree cover | High | Timber, agricultural produce (clearing) |
| Wetland | Marsh, bog, floodplain | Medium–high | Agricultural produce, timber, clay |
| Ocean | Open deep water | — (no buildings) | Marine goods (deferred) |
| Regolith | Loose surface material on airless bodies | Very low | Regolith, water ice (polar) |
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

**Barren:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron ore | High | Mountain, rift: higher |
| Coal | Moderate | Plains, highland: higher |
| Petroleum | Moderate | Plains only |
| Silica | Low | Canyon: higher |
| Stone (ambient) | Always present | — |
| Sand (ambient) | Plains and canyon | — |

**Rocky:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron ore | High | Mountain: higher |
| Copper ore | Moderate | Mountain, canyon: higher |
| Silica | Moderate | — |
| Rare earth ore | Low | Mountain, rift: higher |
| Stone (ambient) | Always present | — |

**Volcanic:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Rare earth ore | High | Rift: higher |
| Iron ore | Moderate | — |
| Stone (ambient) | Always present | — |

**Icy:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Water | High | — |
| Silica | Low | Mountain: higher |

**Tundra:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron ore | Low | Mountain: higher |
| Peat (ambient) | Moderate | Plains, valley |
| Stone (ambient) | Always present | — |

**Grassland:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Agricultural produce | High | Valley: higher |
| Stone (ambient) | Always present | — |
| Sand (ambient) | Plains: moderate | — |

**Forest:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Timber | High | — |
| Agricultural produce | Low | Valley: moderate |

**Wetland:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Agricultural produce | High | — |
| Timber | Moderate | — |
| Clay (ambient) | Moderate | — |

**Regolith:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Regolith | Always present | — |
| Water (from icy pole) | Low | Crater: higher |

**Metallic:**
| Deposit | Relative weight | Landform modifier |
|---------|---------------|-------------------|
| Iron-nickel ore | High | — |
| Platinum group metals | Low | Rift: higher |
| Regolith | Always present | — |

---

## Ambient resources

Certain deposits are present on virtually every tile and require no special geological conditions. Their deposit values are low and their base prices are minimal, but they provide:

- Local construction inputs (stone, timber) without supply chain dependency
- Low-value trade goods that can sustain marginal markets
- A production option for any tile, even if not economically compelling

| Ambient resource | Found on | Building to extract |
|-----------------|----------|---------------------|
| Stone | All non-water compositions | Quarry |
| Timber | Forest, wetland (also very sparse on grassland) | Lumber Camp |
| Sand | Barren plains, barren canyon | Quarry (sand variant, or same building) |
| Clay | Wetland, valley | Quarry (clay variant) |
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

**Open tuning question raised by that measurement: Kepler generates no valley tiles at all.** Valley is assigned to unclaimed non-ocean ground below the height threshold (Pass 5), but on a wet body the ocean has already taken everything that low — so the ×1.1 fertile landform is unreachable on exactly the bodies where river valleys should be most characteristic. Dry bodies carry 20–27% valley. This is self-consistent rather than a defect, but it is a generation-tuning question, not a rendering one; it belongs with the tile-generation refinements (BL-051).
