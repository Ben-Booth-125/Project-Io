# Project Io — Nation Generation

Nations are the political and territorial layer overlaid on the tile map. They define the
geopolitical backdrop at campaign start: who controls what land, what the diplomatic starting
positions are, and what legal context corporations operate within.

Nation system design is **an open item**. This document covers only the generation strategy.
Mechanical behaviour — taxes, laws, military policy, diplomatic actions — is deferred.

---

## Design principles

**Territory derives from the tile map.** Nation boundaries are placed over an already-generated
body tile map. A nation's resource profile is a consequence of the tiles it controls, not a
separately authored value. This means a nation rich in volcanic or metallic terrain will naturally
tend toward heavy industry; a nation controlling fertile grassland will lean agricultural.

**Procedural with tuned parameters.** Unlike the prototype tile map (fixed seed, authored solar
parameters), nation generation is procedural from the start. A campaign seed governs all
randomness. Tunable parameters — seed density, size variance, fragmentation — allow world
character to be shaped without re-authoring. Different seeds produce different political maps
over the same tile geography.

**The nation count is an outcome, not an input.** There is no nation-count target anywhere in the
pipeline and no campaign-setup slider for it. Seeds scale with the body's habitable land area, and
the merge pass absorbs every nation that fails to hold a minimum viable territory. How many
nations a world ends up with therefore falls out of its landmass and coastline: a large, dry
continental homeworld supports more nations than a small, ocean-heavy one. The two constants that
shape it are `land_tiles_per_seed` and `min_nation_tiles` (`nation_params`,
`src/world/nation_generation.hpp`) — both are retuning dials on *character*, not on count.

**Same pipeline, all bodies.** Nation generation runs only on bodies with sufficient habitable
area. In the prototype this means Kepler only. The pipeline is body-agnostic; it reads tile
compositions and applies the same logic regardless of which body it operates on.

---

## Generation pipeline

### Pass 1 — Seed placement

A set of nation seeds are placed across the body's landmass tiles. The seed count is **derived
from the habitable landmass**: one seed per `land_tiles_per_seed` non-ocean tiles (80 by default,
so Kepler's ~6,000 habitable tiles budget ~75 seeds). Placement avoids stacking seeds in small
geographic areas: a minimum separation distance (in tiles) is enforced between seeds, which also
caps how many seeds physically fit whatever the density asks for.

A seed is only a **candidate core**, not a nation — the density is deliberately several times
denser than the surviving count, because Pass 2c absorbs most of them.

Seeds prefer habitable compositions (grassland, forest, wetland) but can land on any non-ocean
tile. This produces nations that form around productive cores rather than uniformly across
terrain.

### Pass 1b — Growth weights (BL-053)

Each seed is assigned a **growth weight** drawn from a skewed distribution (the cube of a
uniform draw → most seeds small, a few large). A seed's BFS step cost is divided by its weight,
so high-weight seeds expand cheaper, reach further, and claim more land. This turns the
near-uniform Voronoi cells the flat approach produces into a **strongly varied size
distribution** — a few large "great powers", several mid-sized, many small.

### Pass 2 — Territory expansion (Voronoi BFS)

Each seed expands outward via BFS. Ocean tiles are never claimed. The expansion follows a
weighted Voronoi approach: step cost decays with distance from the seed and is **divided by the
seed's growth weight** (Pass 1b); high-`H` tiles (mountains, highlands) act as soft barriers —
they can be claimed but reduce the expansion budget of the claiming nation, producing mountain
ranges that naturally fall near or at borders.

Expansion continues until all claimable land tiles are assigned.

### Pass 2b — Orphan-island assignment

The cardinal-adjacency BFS cannot cross water, so landmasses disconnected from every seed
would otherwise stay unclaimed. A deterministic post-pass closes that gap: unclaimed
non-ocean tiles are grouped into connected components (cardinal adjacency, column-wrapped),
and each whole component is assigned to the nation owning the **nearest already-claimed land
tile across the water** (by Chebyshev grid distance; ties break to the lower nation index,
then the lower tile index). After this pass every non-ocean land tile on the body belongs to
a nation — there are no unclaimed islands.

### Pass 2c — Light "in history" merges: the size floor (BL-053)

A deterministic post-pass gives the map a "grown in history" feel without simulating history.
The body is **over-seeded** in Pass 1, then the smallest nations are repeatedly **absorbed into
their largest cardinally-adjacent neighbour**. The stopping condition is a **minimum viable
territory, not a target count**: the pass ends when the smallest surviving nation clears
`min_nation_tiles` (80 by default — one seed's worth of land, so a nation must end up holding at
least what its own seed was budgeted), or when only one nation is left. Surviving owner indices
are then compacted.

Because the loop stops on *size* rather than *count*, the number of nations is whatever the
geography leaves standing. The result amplifies the size spread (rich-get-richer) and produces
**irregular, non-convex borders** — a nation may be the union of a core and an absorbed
neighbour. Tie-breaks are fully deterministic (smallest by tile count then lowest index;
absorbing neighbour by tile count then lowest index; an island with no land neighbour is absorbed
into the globally largest nation). This is *not* historical fragmentation (exclaves, disputed
zones) — that remains an open item.

### Pass 3 — Resource profile derivation

Each nation's resource profile is computed by summing the deposit profiles of its tiles,
weighted by composition type. The result is a read-only descriptor — `iron_ore_abundance`,
`agricultural_abundance`, `petroleum_abundance`, etc. — used to characterise the nation
economically for diplomacy initialisation and corporation generation.

### Pass 4 — Political character assignment

Each nation receives a set of parametric political attributes drawn from a seeded random
pass. These seed the sentiment graph and the starting tone of diplomatic interactions.

| Attribute | Values | Effect |
|---|---|---|
| `ideology` | `authoritarian`, `technocratic`, `mercantile`, `isolationist` | Shapes starting sentiment toward other nation types |
| `expansionism` | `passive`, `moderate`, `aggressive` | Modulates early military posture |
| `economic_focus` | `extraction`, `processing`, `trade` | Biases nation investment behaviour (see § Open items) |

Sentiment between any two nations at generation time is a function of ideology compatibility,
territory adjacency, and resource overlap. High-overlap neighbours start with lower sentiment
regardless of ideology.

### Pass 5 — Naming

Each nation receives a generated name from a culture-flavoured template bank. Templates are
seeded and combine a structural form (adjective + noun, compound noun, etc.) with a
phoneme table. No human-authored list of specific names is required.

---

## Prototype scope

In the prototype, Kepler is the only body with nation generation. Selene, Cinder, and Pallas
are unclaimed territory.

Nation count for Kepler: **not authored**. Its ~6,000 habitable tiles budget ~75 candidate seeds
(one per 80), and the size floor leaves **17–21 nations** across sampled campaign seeds (21 on the
default seed 0), with strongly varied sizes — the largest holds ~850 tiles against a floor of 80.
Halving Kepler's habitable area roughly halves the count. The knobs are `land_tiles_per_seed`,
`min_seed_separation`, and `min_nation_tiles`; the New World setup screen has **no nations
slider** — the count is not the player's to set.

Nation system behaviour — taxes, laws, diplomatic actions, military response, territorial
ambition — is **not implemented in the prototype**. Nations are generated and exist as data
(territory, resource profile, political character, name), but take no autonomous actions.
See § Open items.

---

## Relationship to corporations

Corporations are legally registered within a nation and operate within its territory by default.
The nature of this relationship — what obligations it creates, what it restricts, and how it
evolves across Eras — is an open design item. See `CORPORATION_GENERATION.md` and
`docs/development/DEVLOG.md` for the current position.

---

## Open items

**Nation system design.** What nations actually *do* — tax corporations, issue licences,
declare war, provide infrastructure — is a major design task deferred from the prototype.
The generation layer is implemented first so the world has political structure from turn one
without requiring the system to be fully designed.

**Era-based reform.** There is a working hypothesis that later Eras will see corporations
become de-facto powers above states, with the nation layer diminishing in authority as the
game progresses. This has significant implications for what nations need to be able to *do*
and how that capability decays. It is noted here but not yet scoped.

**Fragmentation and history.** A *light* "in history" pass landed with BL-053 (Pass 2c:
over-seed + merge-below-the-size-floor, giving varied sizes and irregular borders). Full historical
fragmentation — exclaves, disputed zones, contested tiles — remains a deferred production pass
(parked BL-054, nation behaviour).

**Non-Kepler politics.** As colonies establish on other bodies, some form of jurisdiction
and governance will be needed there. Whether this extends the nation model or introduces a
new corporate governance model is unresolved.
