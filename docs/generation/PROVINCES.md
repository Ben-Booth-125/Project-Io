# Project Io — Provinces

**The province is the game's spatial unit of consequence.** A tile is where a building stands; a
province is where a battle happens, where a unit *is*, what the map is coloured by, and what a
building ceiling counts against.

Its design is spread across four documents and one source file (`src/world/province.{hpp,cpp}`).
This document is its home. It is written against the source: where a file, function or constant
is named, that name is the one in the code.

---

## What a province is, and what it deliberately is not

**A province is a deterministic, seeded partition of a body's tiles into small, contiguous, purely
spatial cells.** Every tile on every body belongs to exactly one.

> **It is NOT a region.** No name, no culture, no economy of its own.

**Provinces are always parts of a nation** (Ben, 2026-08-22, design register). Ownership is not
the axis the restraint is about — a province sits inside a nation's territory like every other
piece of ground. What a province lacks is a **name, a culture and an economy of its own**; what it
has is a place in the political map. `world::tile_to_nation` gives every tile an owner, so a
province's owner is **derived from its tiles** and needs no field of its own.

That restraint is the design. The province exists because **a battle needs an engagement
envelope with a stable identity** (BL-467, battle state) — something a battle can happen *in*,
whose id can be folded into the battle's seed stream. Everything else it carries sits on top of
that one requirement, and any of a name, a culture or an economy would be a new system, not a
field.

**How it relates to a nation's territory.** A nation owns tiles (`nation_component::tiles`,
`world::tile_to_nation`); the partition cuts space into cells **inside** that assignment. **No
province contains tiles of two nations** (Ben, 2026-08-22: *"generate provinces alongside
national borders"*) — the partition takes the national assignment as a hard input, so a province
edge and a national border are the same kind of line by construction, and a straddling province
never exists to need an owner rule. BL-563 (province respects nation) owns it; the headless check
is one scan over `w.provinces` against `nation_component::tiles`. The conquest half of the same
ruling — *"impossible to conquer a tile without also controlling the province"*, so **the province
is the unit of conquest** — is BL-567 (province is the conquest unit).

---

## Where this sits in the chain

Under SYSTEMS.md § The progression chain — *each system's ceiling is the next system's door* — the
province is not a rung the player climbs. It is the **grain the later rungs are measured in**:

    tile        what you build on          extraction, deposits, terrain
    PROVINCE    what you contest           battle, unit position, building ceiling
    body        what you travel between    convoys, markets, survey
    nation      what enacts law            jurisdiction, treasury

The moment force enters the game the tile stops being the right unit, because a fight between
tokens on single tiles is a skirmish and not a campaign. **The province is where the game changes
from a map of buildings to a map of positions.**

---

## The partition — grown from settlement, stopped by terrain

The partition model is BL-515 (partition). A perfectly packed 3×3-block partition was considered
and Ben overruled it, 2026-08-21: *"packing each province perfectly looks nice, but it is scarcely
how borders were defined in history."* What a province *is*, and everything downstream, is
independent of how the shapes are drawn.

**The five rulings the algorithm implements:**

1. **Provinces grow from population centres**, and seed strength scales with the centre's scale
   (1–5): *a metropolis draws a larger province than a village does.*
2. **Boundaries are rivers, elevation difference, and sometimes roads — but a road BINDS.** Tiles a
   road links tend to share a province. **A road is never a divider.**
3. **Every province is anchored by a population centre** (Ben, 2026-08-25; BL-611, province
   centre anchor — superseding the hinterland ruling below). Centre density now derives from
   Era −1 demography (`docs/economy/POPULATION.md` § Generation; BL-610, centres from
   demography), dense enough that every province seeds from a centre of *some* scale; the
   centre is the province's **political decider** — its nation is the province's nation, and
   under BL-567 (province is the conquest unit) taking the centre takes the province, making
   every anchor a strategic objective. *Superseded original ruling, kept for the record:*
   country no centre reaches became hinterland, seeded from the least-accessible tile.
4. **Size is a growth budget, not a clamp.** *"Don't reject tiny provinces"* — nothing is merged
   away to satisfy a floor, and **boundaries win ties.**
5. **A national border is a hard edge.** Seeds are placed per nation and the terrain cost function
   operates only *within* a nation's territory, so a region's frontier is the border wherever it
   reaches one (Ben, 2026-08-22; BL-563, province respects nation). Regions grown to fit the border
   need no cutting, so there is no scatter of one- and two-tile offcuts and no merge pass.

The cost model that makes an edge a border lives in `province.hpp`: a base edge cost
(`k_province_edge_base_cost`), a river crossing cost (`k_province_river_edge_cost`), a height
difference cost (`k_province_height_cost`, pinned so a p90 height step costs the same as a river),
a road-binding divisor (`k_province_road_bind_divisor`) and a small seeded jitter
(`k_province_edge_jitter`). Hinterland seeds are spaced `k_province_seed_spacing` apart, measured
geodesically over land — and it is the spacing, not the budget, that sets hinterland size.

**How rulings 3 and 5 are one mechanism** (BL-611, province centre anchor). The LAND fill is
**nation-locked**: a region — centre-seeded or leftover — claims only tiles of its seed's
nation, and singleton absorption honours the same lock, so a land province is single-nation by
construction and its anchor's nation *is* its tile-derived nation. On a settled body the spaced
hinterland seeding is retired; ground no centre's budget reaches (ice caps, deep desert, the far
side of a border no centre stands behind) is mopped up by the leftover pass, and every leftover
province then receives a **scale-1 anchor founding** on its best ground
(`ensure_province_anchor_centres`, `population_generation.cpp`) before the holder is derived —
a pure-ice province gets its anchor on its least-bad tile, counted rather than hidden. The
**anchor** is derived, never stored: the highest summed centre scale in the province, ties to
the lowest tile id. The spaced hinterland survives for the water domains and for the land of an
unsettled body (no centres anywhere), where there is nothing else to seed from.

### The size band, and why it has three numbers

| Constant | Value | Meaning |
|---|---|---|
| `k_province_min_tiles` | **7** | Soft floor. Past it, a region annexes only ground **no harder to reach than the ground it already holds** — the mean of its own step costs. |
| `k_province_max_tiles` | **12** | *Preferred* ceiling; the clamp **growth** obeys. Not what a finished province is guaranteed to satisfy, because singleton absorption can push past it. |
| `k_province_hard_cap_tiles` | **20** | The bound that really is absolute, and **the only size claim the harness asserts.** |

A fourth constant, `k_province_hard_min_tiles` = **3**, is the hard-target floor: a region takes
its first three tiles whatever they cost. It can still be missed — an island of two tiles ships at
two tiles because the land genuinely ran out — and that is the case *"don't reject tiny
provinces"* protects. The harness reports how often it happens rather than asserting it away;
**Ben accepted the resulting share (~12% of provinces) on 2026-08-22** as the ruling working as
intended, not a defect (NR-433).

The soft floor's self-referential brake is what *"boundaries win ties"* means mechanically — and it
needs no threshold constant of its own, which is why the rule reads as terrain rather than as tuning.

**The hard cap is asserted, not imposed, and deliberately so.** Singleton absorption picks a
tile's *cheapest* neighbour; choosing a costlier one to respect a size bound would contradict the
cheapest-edge rule the whole growth model is expressed in. So the cap is **a claim about what the
cost model produces, which breaks loudly if that stops being true.**

Measured headroom: the partition tops out at **16 tiles** across the six-seed sweep, four short of
the cap. The over-12 share (4.9% at the ruling) is **reported by the harness, never asserted** —
*"rare" is Ben's judgement to make against a number*, and no threshold for it has been chosen. The
preferred ceiling is therefore advisory in a way the hard cap is not.

### Three domains, never mixed

Provinces over water are BL-516 (provinces over water). Ben, 2026-08-21: *"We can also draw
provinces over the ocean, using 3–12 size coastal tile provinces. Ocean provinces should be much
larger, but not larger than say 80 tiles."*

| Domain (`province_kind`) | Band | Notes |
|---|---|---|
| **`land`** | 7–12 soft, 20 hard | The only domain anything can stand in |
| **`coastal_water`** | the land band exactly — 7–12 soft, 20 hard | The shoreline ring and the lakes. **No population centre seeds one** — the least-accessible-tile seeding survives here (water holds no centres; the BL-611 anchor rule is a land-domain rule). Reuses the land constants; none of its own |
| **`open_ocean`** | `k_sea_province_soft_target` 42, `k_sea_province_max_tiles` 80 | The sea out of sight of land; seeds spaced `k_sea_province_seed_spacing` = 7 |

Ben gave the sea **one** number, 80, and everything else is derived from it by the same lattice
argument that pins the land spacing: seeds at minimum separation *d* tile the plane in cells of
area (√3/2)·*d*², so the soft target is that cell (42.4 at *d* = 7) and the spacing is the largest
at which the cap is still a guard rather than a clamp. **There is deliberately no separate sea hard
cap** — inventing one would be a threshold nobody chose. The harness asserts the exact identity
instead: every tile above 80 arrived by singleton absorption, never by growth.

`province_kind` is **derived from the substrate of any member tile**, never stored, so it cannot
desynchronise from the tiles it describes.

**A province never spans two domains.** Every land province is hex-connected land, and no province
mixes land with water or a lake with the sea (the land-only invariant narrowed rather than deleted,
NR-428). The domains are **exclusive by construction** — a tile's substrate names exactly one — so
the claim is structural rather than checked.

> **Nothing can be in a sea province, and that is expected.** Units are land-bound (`march_unit`
> refuses a water destination outright), buildings refuse water, and a sea province sustains zero
> of them. Sea provinces are **addressable empty space, built without inventing the naval model
> that will eventually fill them** — ships, blockade and coastal trade are settled as eventual and
> deferred (Ben, 2026-08-22: *"we can defer this for now"*). BL-188 (coastal ports) is the first
> thing that would occupy one.

### Two contracts downstream code depends on

**1. The id order is the contract.** Downstream code walks provinces in ascending `province::id` and
gets an order that does not depend on container internals, tile-map iteration order, or the order
bodies were created in. **Province id 0 is a real province**, so any seam needing a sentinel must
not use zero (NR-412).

**2. The partition is part of world generation and versions with it.** It is **never patched in
place**: a change to the algorithm re-rolls every battle in every world, so partition fixtures do
not survive a repartition, and that is correct rather than a defect (NR-422).

### Storage and determinism

The partition is built once at the end of `make_hard_coded_world` from the world seed and the
finished tile map, and held in `world::provinces`. It is **derived but stored**, because a battle
must not be re-identified by a lazy rebuild. It joins the flat-binary serialisation seam as the
trailing section of the history-log stream, so an earlier stream is still a valid prefix.

It is **not folded into `state_hash`**: the hash folds the fields a tick may mutate, and the
partition never moves once built. Its determinism is checked where it belongs —
`determinism_harness` compares the partition field-for-field across two generations of the same
seed, and `province_partition_harness` P6/P7 recompute it from the stored seed.

---

## What reads the province

| Consumer | Reads it as |
|---|---|
| `campaign_battle.cpp` / `battle_system.cpp` | the **engagement envelope**, its id folded into the battle seed (BL-467, battle state) |
| `corp_command.cpp` § `march_unit` | the **destination payload** |
| `body_surface_canvas.cpp` | the **rendered and selected unit** (BL-511, province as render unit) |
| every fill lens | the **reduction grain** — see below |
| `construction.cpp` | the **building ceiling** it counts against (BL-513, province building ceiling) |

### Unit position is province grain

A unit's position and movement are province grain (NR-405, Ben, 2026-08-21), overturning the
earlier tile-canonical ruling and collapsing the command-at-tile / engagement-at-province split.
`march_unit` reads a `province` field; its verb value and position in the serialised `corp_verb`
enum are those of the tile form, because the enum is append-only and nothing renumbers.

### The building ceiling

A province sustains a bounded number of buildings, regardless of type (Ben: *"it does not matter
if you can build 60 buildings, whether they are 5 of one type and 5 of another, or 10 of one
type"*). The shape, from `province.hpp`:

    sustain_units(province) = pop_factor × Σ over land tiles of habitability_t × (1 + road_level_t / 3)
    ceiling(province)       = max(1, round(k_province_buildings_per_sustain_unit × sustain_units))

Area is the number of terms; habitability is each tile's weight; infrastructure is the road
multiplier spanning exactly [1, 2] over the road ladder's own domain (`k_road_ladder_max` = 3);
population is `1 + Σ(centre scale) / k_population_scale_max`. Every band is read off a domain the
codebase already defines, leaving **one free scalar**, `k_province_buildings_per_sustain_unit`
(12.6468), pinned by measurement in `province_capacity_probe` so the world total matches the
capacity the pooled per-tile cap already grants. The ceiling is **computed on demand, never
cached**, so it moves as roads are built — the one placement bound not fixed at generation.

**Ben's ruling on what the ceiling is for (2026-08-22): drop it to where it bites**, and *"use
technology for deeper mines and denser facilities which use more of the cap."* The ceiling is a
real constraint and **technology is the thing that relieves it** — the first consumer of a
`modifier_set` subject for it, which BL-513 (province building ceiling) owns and names per
META_LAYER § the stopping condition. Pinned at the measured anchor the ceiling stands far above
what a province's own geology supports (NR-421, NR-509: capacity 103 against 24 placement slots),
so at that pin **deposits bind and the ceiling does not** — which is why the ruling moves it.

### Every lens blends across provinces

Province-grain rendering forces every fill lens to state how it reduces from tile to province.
**Ben ruled (2026-08-22) that every lens blends across province vertices**, Country and Continent
included, on an option explicitly labelled as overruling a per-lens reduction table that had
proposed uniform fills for those two (NR-415). BL-532 (lens province blend) and BL-514 (global
tile blend) own the rendering.

So **adding a lens does not mean answering a reduction question.** The argument for the refusals is
worth keeping even though it lost: *the mean of two nation colours is a third nation colour*, so a
blended political map draws borders that do not exist. Ben's ruling accepts that in exchange for
one visual language across the whole map.

---

## Open questions

1. **Do provinces feed back into where national borders fall? — ANSWERED** (Ben, 2026-08-25):
   through the anchor centre. At generation, ruling 5 stands — the national assignment is an
   input, and anchor and territory agree by construction. After generation, the **centre decides**:
   a province's nation is its anchor centre's nation, so conquest moves borders by taking centres
   (BL-567, province is the conquest unit; BL-611, province centre anchor). BL-518 (war redraws
   borders) inherits this as its mechanism.
2. **What conquest moves.** With the province the unit of conquest (BL-567), BL-518 (war redraws
   borders) moves whole provinces between nations rather than tiles — how a border redraw
   interacts with ruling 5's per-nation seeding after generation is that item's to settle.

---

## Where the parts live

| Concern | File |
|---|---|
| The partition, the size band, the three domains, the ceiling shape | `src/world/province.{hpp,cpp}` |
| Engagement envelope and battle seed | `src/world/battle_system.cpp`, `campaign_battle.cpp` |
| March destination | `src/world/corp_command.cpp` § `march_unit` |
| Rendering, selection, per-lens blend | `src/ui/body_surface_canvas.cpp`, `hex_render.cpp` |
| Building ceiling enforcement | `src/world/construction.cpp` |
| The check | `tools/verify/province_partition_harness.cpp` § P5a, `province_capacity_probe` |

**Related authorities.** [`TILE_GENERATION.md`](TILE_GENERATION.md) § Province partition (the
generation-pass view), [`../ui/PLANETARY.md`](../ui/PLANETARY.md) § Province grain (the rendered
view), [`../ui/SELECTION.md`](../ui/SELECTION.md) § The province element (the selected view),
[`../military/MILITARY.md`](../military/MILITARY.md) (what a battle does inside one),
[`../GLOSSARY.md`](../GLOSSARY.md) (the spatial vocabulary).

**Owning items.** BL-515 (partition) and BL-516 (provinces over water) own the partition; BL-511
(province as render unit) the selected grain; BL-513 (province building ceiling) and BL-512 (firm
cap tunables) the ceiling; BL-532 (lens province blend) and BL-514 (global tile blend) the
rendering; BL-563 (province respects nation) the border edge; BL-567 (province is the conquest
unit) and BL-518 (war redraws borders) what conquest moves.
