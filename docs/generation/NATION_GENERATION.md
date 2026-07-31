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

### Pass 0 — The history ladder (landed 2026-07-30, BL-221 pre-national ladder)

Before any seed is placed, `run_history_ladder` (`src/world/history_ladder.cpp`) runs over the
finished tile map and the body's planetology state. It scores every tile for agrarian value
(`agrarian_score` — composition, landform, habitability), picks the independent **cradles** by
greedy argmax with a separation floor, and measures the **barrier field** (ocean, mountain,
canyon, dead ground). From those it computes `fragmentation_q`, `conquest_cost_q` and
`exit_cost_q` (integer, 0–1000 — gate-path arithmetic stays integer per `PLANETOLOGY.md`
§ Determinism).

`nation_params_from_ladder` then **modulates the nation parameters** from those scalars: a
fragmented world lowers `land_tiles_per_seed` and `min_nation_tiles` (denser seeds, smaller
viable survivors — both clamped to [½, 1½] of the base so a tuning mistake cannot produce a
one-nation or thousand-nation world), and a high conquest cost raises `min_seed_separation`.
The ladder **drives** the political map rather than narrating one it was handed (Ben,
2026-07-30). A world with no cradles (below a land biosphere) leaves the caller's defaults
untouched. Full stage design: `../lore/HISTORY.md`.

### Pass 1 — Seed placement

A set of nation seeds are placed across the body's landmass tiles. The seed count is **derived
from the habitable landmass, as modulated by the ladder** (Pass 0): one seed per
`land_tiles_per_seed` non-ocean tiles (base 80, scaled down on a fragmented world). Placement
avoids stacking seeds in small geographic areas: a minimum separation distance (in tiles) is
enforced between seeds, which also caps how many seeds physically fit whatever the density asks
for.

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

### Pass 6 — Substrate density (BL-050 saturated substrate)

The last pass in `generate_nations` seeds the background economy's geography. For every
nation-owned tile it finds the nearest population centre on the same body (Chebyshev distance)
and computes a **density ripple**: `max(0, 1 − dist/8) × centre.scale`, taking the strongest
centre. That value is written to `tile_component.substrate_density`, and for every tile with
density > 0 it accumulates into the per-(nation, body) `nation_substrate`:
`population_weight += density` and `capacity[r] += density × tile.resource_deposit[r]`.

Only **generation baselines** are stored here (BL-078): the economic scalars — capacity scale,
demand basket, elasticity, clearing fraction — are applied at tick time by
`inject_substrate_demand`, so the demand/supply model is retunable from `economy.lua` without
regenerating the world. This is the surface any future substrate work reads. Requires
population centres to already exist — which is why `generate_population_centres` runs before
`generate_nations` in `hard_coded_world.cpp` (see § Settlement generation below).

---

## Settlement generation (companion pass)

Not part of `generate_nations`, but sequenced around it and documented here because the nation
passes read it.

**Population centres** — `generate_population_centres` (`src/world/population_generation.cpp`)
runs on Kepler *before* the ladder and the nation pass. It places 20–40 centres (grid tiles /
1000, clamped) on valid tiles (`placement_rules::can_place_population_centre`), one at a time
with a 3× weighting for tiles adjacent to an existing centre, so agglomeration is progressive.
Each centre draws a **scale** 1–5 from a weighted distribution (40/30/20/8/2%) mapping to
10k–5M population, and is named from an independent seeded stream (`generate_city_name`) so
naming never perturbs placement.

**Market carving** — after nations exist, `hard_coded_world.cpp` seeds Kepler's markets from
the centres (the surface BL-036, seed market centres, shipped). Markets anchor to
population-centre tiles but are **resource-carved** (BL-096): each nation's tradeable-raw
concentration (mean deposit per owned tile, seeded jitter ×[0.85, 1.15]) is classified against
the cross-nation mean into a population-scale gate — ≥1.30× mean → gate 2 (fracture: more of
its centres get markets), <0.70× mean → gate 4 (fold: its small centres route to a
neighbour's market via `market_for_tile`), otherwise gate 3. A centre seeds a market only if
its scale clears its nation's gate; one unanchored fallback market is seeded if none qualifies.
Endemic goods are then priced by distance from where they grow (BL-191).

---

## Prototype scope

In the prototype, Kepler is the only body with nation generation. Selene, Cinder, and Pallas
are unclaimed territory.

Nation count for Kepler: **not authored**. Since the ladder landed (BL-221, 2026-07-30) the
size floor and seed density are fragmentation-modulated, and the default seed settles at
**43 nations** — up from the pre-ladder 17–21 (21 on seed 0). The jump is the ladder doing its
job, and it was settled deliberately (Ben, 2026-07-30 — recorded in `../lore/HISTORY.md`
§ Implementation): let naturally different cultures emerge; a future war/conflict stage narrows
the count if needed, rather than tuning the generator to a target. Sizes stay strongly varied.
The base knobs are `land_tiles_per_seed`, `min_seed_separation`, and `min_nation_tiles`
(`nation_params`), now modulated by `nation_params_from_ladder`; the New World setup screen has
**no nations slider** — the count is not the player's to set. (Note: the `min_nation_tiles`
doc-comment in `nation_generation.hpp` still quotes the pre-ladder 17–21 — stale.)

**What planetology data nation placement does and does not consume.** The ladder consumes the
planetology state directly (`life_stage` peak, `arable_share`, `endemics` — BL-221 closed that
half of `PLANETOLOGY.md`'s "hands nothing downstream" weakness). `generate_nations` itself
still reads only tiles: seed preference over habitable compositions and Pass 3's
deposit-summed resource profile are *indirectly* downstream of the biosphere, but no endowment,
province, or biography data is read at placement time.

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
