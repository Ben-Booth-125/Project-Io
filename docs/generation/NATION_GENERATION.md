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
randomness. Tunable parameters — nation count, size variance, fragmentation — allow world
character to be shaped without re-authoring. Different seeds produce different political maps
over the same tile geography.

**Same pipeline, all bodies.** Nation generation runs only on bodies with sufficient habitable
area. In the prototype this means Kepler only. The pipeline is body-agnostic; it reads tile
compositions and applies the same logic regardless of which body it operates on.

---

## Generation pipeline

### Pass 1 — Seed placement

A set of nation seeds are placed across the body's landmass tiles. Seed count is derived from
a tunable `nation_count` parameter. Placement avoids stacking seeds in small geographic areas:
a minimum separation distance (in tiles) is enforced between seeds.

Seeds prefer habitable compositions (grassland, forest, wetland) but can land on any non-ocean
tile. This produces nations that form around productive cores rather than uniformly across
terrain.

### Pass 2 — Territory expansion (Voronoi BFS)

Each seed expands outward via BFS. Ocean tiles are never claimed. The expansion follows a
weighted Voronoi approach: expansion probability decays with distance from the seed, and
high-`H` tiles (mountains, highlands) act as soft barriers — they can be claimed but reduce
the expansion budget of the claiming nation, producing mountain ranges that naturally fall
near or at borders.

Expansion continues until all claimable land tiles are assigned.

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

Nation count for Kepler: **8–12**, tunable via campaign parameters.

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

**Fragmentation and history.** The current pipeline produces geographically coherent nations.
A production pass could add historical fragmentation — exclaves, disputed zones, contested
tiles — to produce more realistic and strategically interesting political maps.

**Non-Kepler politics.** As colonies establish on other bodies, some form of jurisdiction
and governance will be needed there. Whether this extends the nation model or introduces a
new corporate governance model is unresolved.
