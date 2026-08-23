# Project Io — Provinces

**The province is the game's spatial unit of consequence.** A tile is where a building stands; a
province is where a battle happens, where a unit *is*, what the map is coloured by, and what a
building ceiling counts against.

It arrived late — the partition landed 2026-08-21 — and its design is spread across four documents
and one 936-line source file. This document is its home.

> **Status: written 2026-08-22 as capture, not design.** § Build status is transcribed from the
> code and from Ben's rulings of 2026-08-21. § What is absent names holes. § Open questions are
> calls nobody has made, several of which have open review entries already.

---

## What a province is, and what it deliberately is not

**A province is a deterministic, seeded partition of a body's tiles into small, contiguous, purely
spatial cells.** Every tile on every body belongs to exactly one.

> **It is NOT a region.** No name, no culture, no economy of its own.

> **Corrected 2026-08-22 (Ben, design register).** This document originally said *"no **owner**"*
> and that was wrong. Ben: **"Provinces are always parts of a nation."** Ownership is not the axis
> the restraint is about — a province sits inside a nation's territory like every other piece of
> ground. What a province lacks is a **name, a culture and an economy of its own**; what it has, and
> always had, is a place in the political map. `world::tile_to_nation` already gives every tile an
> owner, so a province's owner is **derived from its tiles** and needs no new field.

That restraint is the design. The province exists because **BL-467 (battle state) needed an
engagement envelope with a stable identity** — something a battle could happen *in*, whose id could
be folded into the battle's seed stream. Everything else it now carries was added on top of that
one requirement.

**How it relates to a nation's territory.** A nation owns tiles (`nation_component::tiles`,
`world::tile_to_nation`); the partition cuts space into cells. **A province is always part of a
nation** (Ben, 2026-08-22) — its owner is read off its tiles rather than stored. The open detail is
what happens at a boundary: the partition is grown from terrain and settlement, not from borders, so
a province *can* straddle two nations' tiles. Either the partition learns to stop at a border, or a
province's owner is its majority holder. **That call is owed and is now the substance of § Open
questions 3.**

---

## Where this sits in the chain

Under SYSTEMS.md § The progression chain — *each system's ceiling is the next system's door* — the
province is not a rung the player climbs. It is the **grain the later rungs are measured in**:

    tile        what you build on          extraction, deposits, terrain
    PROVINCE    what you contest           battle, unit position, building ceiling
    body        what you travel between    convoys, markets, survey
    nation      what enacts law            jurisdiction, treasury

The moment force entered the game the tile stopped being the right unit, because a fight between
tokens on single tiles is a skirmish and not a campaign. **The province is where the game changes
from a map of buildings to a map of positions.**

---

## Build status

### The partition — grown from settlement, stopped by terrain (BL-515)

**This is the third and settled algorithm.** A 3×3-block partition shipped first and Ben overruled
it on sight, 2026-08-21: *"packing each province perfectly looks nice, but it is scarcely how borders
were defined in history."* What a province *is*, and everything downstream, was unchanged — only the
shapes are drawn differently.

**The four rulings the algorithm implements:**

1. **Provinces grow from population centres**, and seed strength scales with the centre's scale
   (1–5): *a metropolis draws a larger province than a village does.*
2. **Boundaries are rivers, elevation difference, and sometimes roads — but a road BINDS.** Tiles a
   road links tend to share a province. **A road is never a divider.**
3. **Country no centre reaches becomes hinterland**, under the same boundary rules, seeded from the
   **least-accessible tile** — so a hinterland is shaped by its terrain rather than being whatever
   was left over.
4. **Size is a growth budget, not a clamp.** *"Don't reject tiny provinces"* — nothing is merged
   away to satisfy a floor, and **boundaries win ties.**

### The size band, and why it has three numbers

| Constant | Value | Meaning |
|---|---|---|
| `k_province_min_tiles` | **7** | Soft floor. Past it, a region annexes only ground **no harder to reach than the ground it already holds** — the mean of its own step costs. |
| `k_province_max_tiles` | **12** | *Preferred* ceiling; the clamp **growth** obeys. Not what a shipped province is guaranteed to satisfy. |
| `k_province_hard_cap_tiles` | **20** | The bound that really is absolute, and **the only size claim the harness asserts.** |

The soft floor's self-referential brake is what *"boundaries win ties"* means mechanically — and it
needs no threshold constant of its own, which is why the rule reads as terrain rather than as tuning.

**The hard cap is asserted, not imposed, and deliberately so.** Singleton absorption picks a
tile's *cheapest* neighbour; choosing a costlier one to respect a size bound would contradict the
cheapest-edge rule the whole growth model is expressed in. So the cap is **a claim about what the
cost model produces, which breaks loudly if that stops being true.**

Measured headroom: the shipped partition tops out at **16 tiles** across the six-seed sweep, four
short of the cap. The over-12 share was 4.9% at the time of the ruling and is **reported by the
harness, never asserted** — *"rare" is Ben's judgement to make against a number*, and no threshold
for it has been chosen.

### Three domains, never mixed (BL-516)

Ben, 2026-08-21: *"We can also draw provinces over the ocean, using 3–12 size coastal tile
provinces. Ocean provinces should be much larger, but not larger than say 80 tiles."*

| Domain | Band | Notes |
|---|---|---|
| **Land** | 7–12 soft, 20 hard | Unchanged in every respect |
| **Coastal water** | 3–12 | The shoreline ring and the lakes. **No population centre seeds one** — hinterland only |
| **Open ocean** | much larger | The sea out of sight of land |

**A province never spans two domains.** The land-only invariant was *narrowed, not deleted*
(NR-428): every land province is still hex-connected land, and no province mixes land with water or
a lake with the sea. The domains are **exclusive by construction** — a tile's substrate names
exactly one — so the claim is structural rather than checked.

> **Nothing can be in a sea province yet, and that is expected.** Units are land-bound (`march_unit`
> refuses a water destination outright), buildings refuse water, and a sea province sustains zero of
> them. They are **addressable empty space, built without inventing the naval model that will
> eventually fill them.**

### Two contracts downstream code depends on

**1. The id order is the contract.** Downstream code walks provinces in ascending `province::id` and
gets an order that does not depend on container internals, tile-map iteration order, or the order
bodies were created in.

**2. The partition is part of world generation and versions with it.** It is **never patched in
place**: a change to the algorithm re-rolls every battle in every world. That is why the province
harness fixtures could not survive the repartition (NR-422) — and why that was correct rather than
a defect.

### What reads the province today

| Consumer | Reads it as |
|---|---|
| `campaign_battle.cpp` / `battle_system.cpp` | the **engagement envelope**, its id folded into the battle seed (BL-467) |
| `corp_command.cpp` § `march_unit` | the **destination payload** — retargeted from a tile, 2026-08-21 (NR-405) |
| `body_surface_canvas.cpp` | the **rendered and selected unit** (BL-511) |
| every fill lens | the **reduction grain** — see below |
| `construction.cpp` | the **building ceiling** it counts against (BL-513) |

### Unit position is province grain (NR-405, 2026-08-21)

**This overturned the earlier tile-canonical ruling.** A unit's position and movement are province
grain, which collapsed BL-467's command-at-tile / engagement-at-province split.

`march_unit` keeps its verb value and its position in the serialised `corp_verb` enum — **only the
field it reads changed**, from `tile` to `province`. The enum is append-only and nothing renumbered.

*Note for anyone writing a fixture:* **province id 0 is a real province**, so the march seam needed
a sentinel that was not zero (NR-412).

### Every lens blends across provinces (Ben, 2026-08-22)

Province-grain rendering forces every fill lens to state how it reduces from tile to province. An
agent decided all thirteen and wrote a per-lens reduction table, which NR-415 flagged as a **new
standing design artifact nobody had agreed to** — a contract every future lens would inherit.

**Ben ruled against the refusals.** Asked whether Country and Continent should fill uniformly per
province or blend across province vertices like every other lens, he chose **blend**, on an option
explicitly labelled as overruling the entry.

So **adding a lens no longer means answering a reduction question.** The interesting content was the
refusals, and the argument for them is worth keeping even though it lost: *the mean of two nation
colours is a third nation colour*, so a blended political map draws borders that do not exist.
Ben's ruling accepts that in exchange for one visual language across the whole map.

---

## What is absent, and known to be

- **A province is anonymous.** No name, no owner, no culture, no economy — by design, and the design
  is now load-bearing in four places. Any of those would be a new system, not a field.
- **No naval model.** Sea provinces exist and nothing can occupy them.
- **Nothing bounds "rare".** 4.9% of provinces exceed the preferred 12 and the harness reports the
  figure without asserting on it. That is deliberate and it means the preferred ceiling is
  advisory in a way the hard cap is not.
- **12% of provinces are below the hard 3-tile floor** (NR-433, open). Raised for Ben and unruled.
- **No serialisation.** Like everything else in `world/*`, the partition is regenerated rather than
  saved — which is *fine*, because it is a pure function of the seed, and is the reason it versions
  with generation rather than needing a migration.
- **The per-province firm cap is inert by orders of magnitude** (NR-421, NR-509). BL-513's ceiling
  stands more than four times above what a province's own geology could support: measured capacity
  103 against 24 placement slots. **The binding constraint on building is deposits, not the
  ceiling** — so the ceiling currently changes nothing.

---

## Open questions

1. ~~**Is 12% below the 3-tile floor acceptable?**~~ **Settled 2026-08-22: accepted.** Tiny
   provinces are the *"don't reject tiny provinces"* ruling working as intended, not a defect.
   NR-433 closed.
2. ~~**What is the province ceiling FOR?**~~ **Settled 2026-08-22: drop it to where it bites** —
   and Ben added the half neither option named: *"Use technology for deeper mines and denser
   facilities which use more of the cap."* So the ceiling becomes a real constraint **and
   technology becomes the thing that relieves it**, which makes this the first consumer of a
   modifier subject that does not exist yet. See BL-513, and META_LAYER § the stopping condition —
   under Ben's ruling there, every unwired subject must name the item that will wire it, and this
   is the namer.
3. ~~**Does a province ever gain an owner?**~~ **Settled 2026-08-22: a province is always part of
   a nation** — see the correction at the head of this document. **What is now open is the
   boundary case:** the partition is grown from terrain and settlement, so a province can straddle
   two nations' tiles. Either the growth passes learn to stop at a border — which would make
   borders a fourth boundary rule alongside rivers, elevation and roads — or a province's owner is
   its **majority holder** and a straddling province simply belongs to whoever holds most of it.
   The second is cheaper and needs no repartition. **BL-518 (war redraws borders) is where this
   first bites**, because conquest moves tiles and therefore moves majorities.
4. ~~**Do sea provinces get a naval model?**~~ **Settled 2026-08-22: eventually yes — ships,
   blockade, coastal trade — and deferred for now** (Ben: *"we can defer this for now"*). So sea
   provinces stay addressable empty space in the near term, and the design is not closed against
   filling them. BL-188 (coastal ports) is the first thing that would.

---

## Where the parts live

| Concern | File |
|---|---|
| The partition, the size band, the three domains | `src/world/province.{hpp,cpp}` |
| Engagement envelope and battle seed | `src/world/battle_system.cpp`, `campaign_battle.cpp` |
| March destination | `src/world/corp_command.cpp` § `march_unit` |
| Rendering, selection, per-lens blend | `src/ui/body_surface_canvas.cpp`, `hex_render.cpp` |
| Building ceiling | `src/world/construction.cpp` |
| The check | `tools/verify/province_partition_harness.cpp` § P5a |

**Related authorities.** [`TILE_GENERATION.md`](TILE_GENERATION.md) § Province partition (the
generation-pass view), [`../ui/PLANETARY.md`](../ui/PLANETARY.md) § Province grain (the rendered
view), [`../ui/SELECTION.md`](../ui/SELECTION.md) § The province element (the selected view),
[`../military/MILITARY.md`](../military/MILITARY.md) (what a battle does inside one),
[`../GLOSSARY.md`](../GLOSSARY.md) (the spatial vocabulary).

**Backlog.** BL-515 (partition) and BL-516 (provinces over water) built it. BL-511 (province as
render unit) made it the selected grain; BL-513 (province building ceiling) and BL-512 (firm cap
tunables) own the ceiling; BL-532 (lens province blend) and BL-514 (global tile blend) own the
rendering. BL-518 (war redraws borders) is the item that will first ask a province to be owned.
