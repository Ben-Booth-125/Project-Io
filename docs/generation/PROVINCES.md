# Project Io — Provinces

> **Settles:** what a province is, and what it deliberately is not · how the partition is
> grown, and what stops it · what size band it targets and why that takes three numbers ·
> how the three domains stay unmixed, and who owns water · what contracts downstream code
> may depend on · what reads a province, and at what grain.
> **Not here:** how a tile got its terrain or its deposit (TILE_GENERATION) · what a plate
> did (CONTINENTS) · who owns the territory a province falls inside (NATION_GENERATION) ·
> what a battle in one costs (../military/MILITARY).
> **Confused with:** TILE_GENERATION.md, CONTINENTS.md, NATION_GENERATION.md.

**The province is the game's spatial unit of consequence.** A tile is where a building stands; a
province is where a battle happens, where a unit *is*, what the map is coloured by, and what a
building ceiling counts against.

Its design is spread across several documents and one source file (`src/world/province.{hpp,cpp}`).
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
   (1–5): *a metropolis draws a larger province than a village does* — a growth budget of 7
   tiles at scale 1 up to 12 at scale 5, with every centre growing simultaneously as one
   multi-source fill.
2. **Boundaries are rivers and elevation difference.** *Superseded half (Ben, 2026-08-25;
   BL-623, provinces before roads): roads were a binding input — tiles a road links tended to
   share a province, never divide one. Overturned with the ordering: the partition now runs
   BEFORE roads, so every province's settlement exists when the lattice is laid and anchor
   foundings join it like any village. Roads still never divide — they simply are not read.*
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

The cost model that makes an edge a border lives in `province.hpp`, and it is integer arithmetic
throughout: base 10 (`k_province_edge_base_cost`) + river 40 (`k_province_river_edge_cost`) +
round(|Δheight| × 683) (`k_province_height_cost`, pinned so a p90 height step costs the same as a
river) + a seeded jitter of 0–4 (`k_province_edge_jitter`). The road-binding divisor retired with
ruling 2's supersession (BL-623) — roads are laid after the partition and are not an input. The
height term reads `tile_component::height` — Pass 1's normalised heightmap, retained for this
consumer (BL-517, retained height) — and never the seven landform classes, whose numeric order
means nothing (`TILE_GENERATION.md` § Pass 1 — Heightmap, `GENERATION_LEDGER.md` § Data lifetime).
Hinterland seeds are spaced `k_province_seed_spacing` = 3 apart, measured geodesically over
land — and it is the spacing, not the budget, that sets hinterland size.

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
cheapest-edge rule the whole growth model is expressed in. Ben chose the bound (2026-08-21,
NR-438) — *"we prefer up to 12 tiles, but up to 20 is permitted in rare cases"* — so the
cheapest-edge rule survives intact, which is what the ruling protects. The cap is therefore **a
claim about what the cost model produces, which breaks loudly if that stops being true.** (The
prefer-room variant was measured at 241 provinces over the preference, max 14, and rejected; the
breach was its only justification.)

Measured headroom: the partition tops out at **16 tiles** across the six-seed sweep, four short of
the cap. The over-12 share (4.9% at the ruling) is **reported by the harness, never asserted** —
*"rare" is Ben's judgement to make against a number*, and no threshold for it has been chosen. The
preferred ceiling is therefore advisory in a way the hard cap is not.

**The measured distribution, 6 seeds** (`tools/verify/province_partition_harness.cpp`,
sections C and D — which is also the re-pinning instrument for the two
measurement-pinned coefficients):

| Partition | provinces | min | max | mean | < 7 | < 3 | > 12 | % in 7–12 |
|---|---|---|---|---|---|---|---|---|
| Organic, pre-absorption | 24,498 | 1 | 12 | 7.87 | 6,195 | 3,008 | 0 | 74.71% |
| Organic, **with absorption** | 22,390 | 1 | **16** | 8.61 | 4,098 | 913 | 1,096 | 76.80% |

The spread is wide **on purpose** and is reported rather than tuned: organic
borders are irregular, and the sub-floor tail is the pockets a ceiling leaves
behind — kept by ruling, not repaired.

Read the absorption row against the hard cap, not against 12. **Max 16 against a cap
of 20**, so the bound holds with four tiles of headroom, and **4.90% sit above the
preferred 12**. Absorption is what moves every one of those numbers: it converts
2,098 one-tile provinces into member tiles of their cheapest neighbour, which is
why the count falls, the mean rises, and the sub-floor tail more than halves. The
harness asserts the cap and the accounting identity (every tile above 12 arrived
by absorption, so growth's own clamp is still proven separately) and **reports**
the 4.90% — whether that counts as "rare" is Ben's judgement against a number, and
no threshold for it has been chosen.

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

**The sea spacing is measurement-pinned, and the pin rule is "the cap must stay a
guard, not a clamp."** Seeds at separation *d* tile a plane in cells of area
(√3/2)·*d*², so the lattice predicts a mean size; where growth is running into the
ceiling instead of meeting its neighbours, the measured mean falls away from that
prediction and provinces pile up on the clamp exactly:

| d | ideal cell | measured mean | max | exactly on the 80 | provinces |
|---|---|---|---|---|---|
| 6 | 31.2 | 32.17 | 75 | 0 (0.0%) | 2,901 |
| **7** | **42.4** | **41.07** | **82** | **26 (1.1%)** | **2,272** |
| 8 | 55.4 | 49.29 | 83 | 207 (10.9%) | 1,893 |
| 9 | 70.1 | 55.25 | 83 | 507 (30.0%) | 1,689 |

At *d* = 8 one province in nine sits exactly on 80 — the clamp is drawing the size
rather than guarding it. At *d* = 7 the measured mean still matches its lattice
prediction, which is the evidence that terrain and spacing set the size. 41 tiles
against land's 8.6 is also "much larger" by nearly five times.

`province_kind` is **derived from the substrate of any member tile**, never stored, so it cannot
desynchronise from the tiles it describes.

**A province never spans two domains.** The land-only invariant is narrowed, not deleted (NR-428):
land provinces are hex-connected land that never spans water; the general claim — **asserted by the
harness as P2b** — is that **a province holds exactly one domain**, which is strictly stronger,
since it also forbids a lake joining the sea. The domains are **exclusive by construction** — a
tile's substrate names exactly one — so construction is *why* the claim holds, and
`province_partition_harness` P2b is what breaks loudly if it stops holding. Structural and checked,
not one instead of the other.

### Who owns water (Ben, 2026-09-06)

**Coastal water belongs to whoever owns the shore. Open ocean belongs to nobody.** That is the
whole ownership rule, and it replaces the deferral this section used to carry.

| Domain | Owned? | Who can be there |
|---|---|---|
| **`land`** | Yes, as always | Land units; buildings |
| **`coastal_water`** | **Yes** — derived from the shore that claims it | Coastal units, and land units crossing **owned** coastal water |
| **`open_ocean`** | **No, structurally** | Coastal/naval units only; never a territory |

**The ownership half is DERIVED, and derived is not the same as claimed.** A province's owner comes
from its tiles (§ above — `world::tile_to_nation`, no field of its own). The nation carve **never
grows across water at all**; water ownership is worked out afterwards, from the shore:

| Domain | How it gets an owner |
|---|---|
| **Coastal water** | The **shoreline ring**: a tile touching owned land takes that land's owner. Two nations on one strait → the lower owner index, a total and stable tie-break. It spreads no further. |
| **Lake** | Filled **whole** by the shore enclosing it — a lake is bounded by its own coast, so ownership crossing it means something. |
| **Open ocean** | Never. Structurally unowned, and it never conducts ownership between two coasts a deep sea separates. |

**Water that touches no owned shore stays unowned**, which is the point: with most land unowned,
much open coastline is unowned too, and *"you may walk your own shore, not someone else's"* meets
shore belonging to nobody.

> **This paragraph described the wrong mechanism until 2026-09-07 (NR-792).** It said the carve
> narrowed its ocean mask to `is_open_ocean` and thereby "claims the shoreline ring". That is what
> the code did, and it is not the same rule: letting the flood claim water made ownership a question
> of which seed's growth arrived first, not of who owns the adjacent land. Measured, it gave **100%
> of coastal water owned against 39% of land** — an apron derived from a shore that is itself mostly
> unowned, which cannot be right.
>
> Worth recording that the first fix did not work either, and only measurement caught it: excluding
> water from growth and then spreading ownership through **all** non-ocean water returned figures
> **byte-identical** to the flood. The coastal band is globally connected, so one owned shore tile
> conducts ownership around every landmass it touches. The sea is a ring because it is enclosed by
> nothing; a lake fills because it is enclosed by its own shore.

Nothing about the partition changes: the three domains still never mix, and growth still never
leaves its domain.

**Why unowned open ocean is the right asymmetry.** A territory is something a polity can hold, and
holding requires standing somewhere. Coastal water is the shore's apron — reachable, contestable,
and naturally the shore-owner's. The deep sea is not held; it is *crossed*, and control of it is a
matter of who is sailing, not who owns the square. So open ocean stays addressable empty space,
and the thing that makes it matter is traffic rather than title.

**What this un-defers.** This section previously read: *"Sea provinces are addressable empty space,
built without inventing the naval model that will eventually fill them — ships, blockade and
coastal trade are settled as eventual and deferred (Ben, 2026-08-22)."* That deferral is lifted for
the coastal half. The naval model that fills these provinces is `docs/military/MILITARY.md`
§ Domains and traversal, and the ancient sim's use of it is
[`MILITARY_HISTORY.md`](MILITARY_HISTORY.md) § Naval.

**Two consequences worth stating before anyone builds them.** `march_unit` refuses a water
destination outright, so it becomes a domain question rather than a flat refusal. And nation
territory grows by the whole shoreline ring, which moves every carve — see BL-776 (coastal
territory) for what that costs.

### Two contracts downstream code depends on

**1. The id order is the contract.** Downstream code walks provinces in ascending `province::id` and
gets an order that does not depend on container internals, tile-map iteration order, or the order
bodies were created in. The id is the province's **lowest-id member tile** — derived, never
allocated, so ascending id order is ascending lowest-member-tile order and an id cannot be
handed out in the wrong order. **Province id 0 is a real province**, so any seam needing a
sentinel must not use zero (NR-412).

**2. The partition is part of world generation and versions with it.** It is **never patched in
place**: a change to the algorithm re-rolls every battle in every world, so partition fixtures do
not survive a repartition, and that is correct rather than a defect (NR-422).

### Storage and determinism

The partition is built inside `make_hard_coded_world` from the world seed and the finished tile
map, and held in `world::provinces`. Where in the pass order it runs is
[`GENERATION_STRATEGY.md`](GENERATION_STRATEGY.md)'s to state, and it is not a single call: the
homeworld is partitioned before its roads, and the canonical whole-world partition is rebuilt once
every body's tiles exist. The rebuild reproduces the first call byte-identically for the bodies
that call covered — the fill reads no road data, none of its other inputs moves between the two,
and the anchor centres founded in between are skipped as seeds — so *one partition* is a claim
about the result, not about the number of calls. It is **derived but stored**, because a battle
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

### Richness is absorbed here, never clamped at generation (Ben, 2026-09-06)

**When a deposit field comes out "too rich", the correction belongs in this score and not in the
generator.** Ben, ruling on a paleo-deposit measurement that moved the world's petroleum by +68%:
*"our per-province infrastructure scores will account for anything which seems 'too rich'."*

The reasoning is the one this project applies everywhere else. Ground is a **fact about the
world** — geology and biosphere history put oil where the ancient seas were, and how much is
there is not a dial. What a corporation can *do* with it is bounded by something else entirely:
how many buildings the province sustains, which is area × habitability × **roads** × population.
So an extravagantly rich province is not an error to be tuned away at the point of placement; it
is a province whose wealth is **gated behind infrastructure it has to build**, and that is a
force with a visible cause a player can read on the map.

Clamping the deposit instead would break two standing rules at once. It would make the generator
produce an outcome rather than a consequence (§ Asymmetry is the deliverable — the spread is the
point), and it would hide the richness from the player rather than making it *expensive to
reach*. A seam of oil under an unroaded province should read as an opportunity nobody has paid
for yet.

**The practical consequence for anyone measuring a generation change:** a magnitude moving is not
by itself a defect, and the question to ask is never "is this number too big" but "does the
province score already bound what can be taken from it". If the answer is no, the fix is in the
score's terms — the road multiplier's domain, the habitability weight, the free scalar — and not
in the placement rule. `k_province_buildings_per_sustain_unit` is pinned by measurement precisely
so that this stays checkable rather than a matter of taste.

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
3. **Which province a lake belongs to.** A lake is partitioned on the coastal band as its own
   province — chosen because it invents no new size rule and keeps the one-domain invariant,
   and consistent with § Who owns water filling a lake whole from its enclosing shore. Ben
   named lakes as a tile kind but has not ruled between its own province, the surrounding
   land province, and a coastal one.

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

**Related authorities.** [`GENERATION_STRATEGY.md`](GENERATION_STRATEGY.md) (the pass order —
where the partition runs), [`../ui/PLANETARY.md`](../ui/PLANETARY.md) § Province grain (the
rendered view), [`../ui/SELECTION.md`](../ui/SELECTION.md) § The province element (the selected view),
[`../military/MILITARY.md`](../military/MILITARY.md) (what a battle does inside one),
[`../GLOSSARY.md`](../GLOSSARY.md) (the spatial vocabulary).

**Owning items.** BL-515 (partition) and BL-516 (provinces over water) own the partition; BL-511
(province as render unit) the selected grain; BL-513 (province building ceiling) and BL-512 (firm
cap tunables) the ceiling; BL-532 (lens province blend) and BL-514 (global tile blend) the
rendering; BL-563 (province respects nation) the border edge; BL-567 (province is the conquest
unit) and BL-518 (war redraws borders) what conquest moves.
