# Tech Effects — what a tech *does*

> **Status: research scaffolding — the design conversation's home, not authority.** Written
> 2026-08-05 from Ben's ask: *map this to real buildings and units — these trees should unlock,
> upgrade, retire, improve reconnaissance, open law/tax/automation capacity, begin space, and
> unlock war and communication doctrines.*
>
> Tracked under **BL-156 (tech system early design)**, which already owns "the tech record shape,
> effect taxonomy, capstone-as-effect" — this is that taxonomy worked out loud. The pre-game side
> of the mapping is **BL-296 (ancient tech ladder)**; the campaign side is `scripts/tech_tree.lua`
> under **BL-087 (filter system)**. Authority propagates into `docs/SYSTEMS.md` when the work lands.
>
> **Honesty marker used throughout:** every target is tagged **[shipped]**, **[designed]** or
> **[unbuilt]**. Most of the building and recipe vocabulary below is *design target*, not enum
> value — `building_type` has six values and `scripts/recipes.lua` has three recipes. A mapping
> that hid that would read as far more real than it is.

---

## The one structural change: an effect is **(kind, target)**

Ben's list mixes two axes. **A / B / C** name what an effect *does to* something — unlock,
upgrade, retire. **D / F** name a *subject domain* — reconnaissance, space. **E / G** name
*systems* that are themselves unlocked. Collapsed into one list they can't compose: "unlock a
space building" and "upgrade reconnaissance" have no shared shape.

Splitting them fixes it. An effect is a pair:

```
effect: kind    — one of a closed set (below); what changes
        target  — what it changes: a content id, a system, a resource, a region
        note    — one line, reader-facing
```

That is the same move BL-155 made for laws (a closed tagged union over effect families) and the
same reason: the set has to be closed for the consumer to switch on it exhaustively. It also lets
one node carry several effects, which nearly every interesting node does — Railway unlocks a
building **and** changes haul cost.

| Ben's category | Becomes |
|---|---|
| **A** unlock buildings / units / production methods | `unlock` × (building \| unit \| recipe) |
| **B** upgrade the same | `upgrade` × the same targets |
| **C** retire when something better arrives | `retire` × the same targets — *see § Retirement* |
| **D** upgrade reconnaissance | `intel` × (survey \| activity fog \| disclosure \| lens) |
| **E** laws, taxation capacity, automation | `institution` × (law family \| tax mechanism \| automation dial) |
| **F** begin mapping and exploring space | not a kind — a **target domain**, reached by `unlock` (launchpad, orbital port), `intel` (survey), `reach` (space-mode convoys) and `open` (the Era gate) |
| **G** war and communication doctrines | `doctrine` × (unit doctrine \| diplomacy verb \| comms channel) |

---

## The kinds — a closed set of eleven

| kind | What changes | Worked example | Consumer |
|---|---|---|---|
| `unlock` | A content id joins the unlocked set | Railway → **Inland Logistics Hub** [shipped] | BL-156 set membership, tested at placement validation and recipe selection |
| `upgrade` | Same content id, better numbers | Converter Steel → Smelter steel recipe [designed] | recipe registry / building stats |
| `retire` | A content id leaves the available set | Converter Steel retires the bloomery route [designed] | BL-087 availability windows |
| `modifier` | A continuous scalar on an existing process | Germ Theory → tile **hazard penalty** [shipped] | the BL-155 summed-rate machinery |
| `access` | Placement becomes legal where it wasn't | Land Reclamation → wetland buildable [designed] | `placement_rules::can_place` [shipped seam] |
| `reach` | Logistics distance, cost or throughput | Railway → intra-body haul cost [shipped] | `supply_system` / `dispatch_convoys` |
| `intel` | What the player can see | Telegraph → activity-fog freshness window [shipped] | `survey_system`, `body_activity_visibility` |
| `institution` | A *system* becomes available | General Incorporation → the chartered corporate form [unbuilt] | BL-155 laws, BL-280 tax, BL-181 automation |
| `doctrine` | A posture in war or diplomacy | the T5 general-staff regime [unbuilt] | BL-157 units, BL-297 diplomacy verbs |
| `resource` | A good becomes economically real | Coal Haulage → coal as a traded good [shipped resource] | RESOURCES.md Era split, market seeding |
| `open` | Another region, tree or era becomes reachable | The Unwearied Fire → ring T5 Energy | the capstone rule (BL-156) |

`open` is BL-156's settled capstone: **not a flag, a tech whose effect opens a tree**. Every vertex
quest in the ladder is exactly this, and so is `E0-ROCKET` in the campaign tree.

---

## The targets — and how much of each actually exists

**Buildings.** `building_type` [shipped] holds six values: `none`, `extraction_site`,
`processing_facility`, `port`, `launchpad`, `inland_logistics_hub`. PRODUCTION.md's named
buildings — Mine, Farm, Quarry, Lumber Camp, Oil Platform, Fishing Wharf, Smelter, Refinery,
Chemical Plant, Electronics Lab, Fabricator, Food Processor, Hydroponics Bay, Assembly Plant, Power
Plant, Water Treatment Plant, Warehouse, Storage Depot, Orbital Port — are **[designed]** vocabulary
that the generic types carve out via `target_resource` and recipe. A tech that "unlocks the
Smelter" therefore unlocks *a recipe on a processing_facility*, which is the honest wording.

**Recipes.** Three ship: iron ore → steel, petroleum → refined fuel, agricultural produce → food
rations, plus the Hydroponics Bay [shipped, BL-166]. Everything else in PRODUCTION.md § Recipes is
**[designed]**. Production methods are therefore the *easiest* target to hang effects on — the
registry already exists and recipe selection is already a validation point.

**Units.** None. BL-157 settles the record — id-keyed, owner (nation *or* corporation), a tile-id
position, a small type enum, one strength scalar — and `selection_kind::unit` exists but is
unreachable. Every unit effect below is **[unbuilt]**, and the roster turnover it describes is
BL-274's table, not this taxonomy's.

**Systems.** Laws are **[unbuilt]** (BL-155: ten named, two enacted in the prototype cut); the
negotiated tax rate is **[unbuilt]** (BL-280); automation has one shipped dial (BL-181 workforce
auto-solve) and a standing line in the campaign tree (`L-AUTO`).

**Information.** Both fogs ship: the Survey system's four phases [shipped, BL-067] and
`body_activity_visibility`'s four tiers with its `route_fresh_ticks` window [shipped, BL-089].
These are the most under-used effect targets in the whole design — a tech that shortens survey time
or widens the freshness window needs no new machinery at all.

**Resources.** 31 authored, seven in the prototype subset [shipped]. RESOURCES.md's Era 0 / Era 1
split is exactly a `resource` effect waiting to be written down: a good is *present* on the map long
before it is *economically real*.

---

## Retirement — the one that fights the settled model

Ben's **C** is the interesting one, because BL-156 settled the opposite: the unlocked set is
**monotonic** — techs complete, never un-complete, so the set only grows and there is no revocation
path to get wrong. Retirement breaks that on purpose.

The sanctioned mechanism already exists: **BL-087 availability windows** (v0.3.0), which the
constellation's keystone exclusion already leans on. A window is a predicate over the same set, not
a different structure, so the shape survives — but three questions come with it, and none are
answered anywhere yet:

1. **What happens to what's already built?** Grandfather it (the building keeps running, cannot be
   re-placed) or force obsolescence (it degrades or must be replaced)? Grandfathering is the
   legible choice and matches "unlocked content is absent rather than disabled-and-visible" — the
   old building is simply no longer *offered*.

> **SETTLED 2026-08-05 (Ben): obsolete content is not rendered at all.** *"There's no use for a
> player to see 'water mill' if they will never build it."* Retired content leaves the UI
> completely — no greyed row, no struck-through entry, no tooltip explaining why it's unavailable.
> This is the **absent-not-disabled** rule (BL-156, BL-229) extended to the far end of the
> lifecycle: content is absent before it unlocks and absent again after it retires, and the build
> menu only ever shows what you can actually build *now*.
>
> **The Martian water mill — obsolescence is contextual, not global.** Ben's aside carries a real
> constraint: a water mill is obsolete on a 1960 industrial homeworld and not obviously obsolete on
> a body where nothing better can run. So retirement cannot be a global flag on a content id. It is
> a predicate evaluated **per context** — which is exactly what a BL-087 availability window is,
> and one more reason the window is the right mechanism rather than an erasure. The rule to hold:
> *hide what this player cannot build here, not what the tech tree has moved past.*
2. **Does retirement fire on availability or on economics?** A charcoal smelter that nobody builds
   because coke is cheaper has retired itself. Explicit retirement is only needed where the game
   wants to *stop* the player, not merely out-price them.

> **SETTLED 2026-08-06 (Ben, NR-066): explicit, used sparingly.** Retirement stays a window
> predicate, not a global flag, and it is authored mostly on **units** (where a roster genuinely
> turns over) and on **doctrine** branches (where exclusion is the point). For buildings and
> recipes, price is the retirement mechanism — no explicit gate needed.
3. **Is retirement ever reversible?** Under blockade a T5-capacity nation may need the T3 route
   back. The diffusion axis already says capacity can be destroyed but awareness cannot — which
   argues retirement should be an availability window, never an erasure.

> **SETTLED 2026-08-06 (Ben, NR-066): permanent.** Once a window closes it does not reopen — no
> blockade-driven fallback to an earlier route. This overrides the lean this document previously
> carried (which argued for reversibility from the diffusion axis); the diffusion axis still holds
> for *awareness* (a nation remembers a route existed), it just does not restore *capacity*.

---

## The worked mapping — the industrial region (rings T4–T5)

Every tech in the region, with the effects it carries. `→` reads *kind → target*. Status tags mark
how much of the target exists today.

### Ring T4 — Gunpowder / Early Modern

| id | node | effects |
|---|---|---|
| T4-AG-01 | Crop-Package Exchange | `access` → Farm on new terrain compositions [designed] · `modifier` → farm output |
| T4-AG-02 | Convertible Husbandry | `modifier` → farm output per worker · feeds the workforce release The Freed Hands tests |
| T4-MA-01 | Movable-Type Press | `modifier` → practice-diffusion rate (the ladder's own acquisition model) · `institution` → administrative capacity [unbuilt] |
| T4-MA-02 | Cast Ordnance | `unlock` → siege/artillery roster row [unbuilt, BL-274] |
| T4-MA-03 | Precision Instruments | `unlock` → machinery recipe precursor [designed] · `modifier` → construction pacing (BL-095) [shipped] |
| T4-EN-01 | Deep Mining & Drainage | `access` → deposits below the shallow band [designed] · `modifier` → Mine output |
| T4-EN-02 | Coal Haulage & Urban Fuel | `resource` → coal as a bulk traded good [shipped resource] · `reach` → coastal bulk haulage |
| T4-TR-01 | Full-Rigged Ship | `reach` → inter-region route range · `upgrade` → Port throughput [shipped building] |
| T4-TR-02 | Celestial Navigation | `intel` → route reliability; fewer lost lanes · `reach` → open-ocean legs |
| T4-IN-01 | Empirical Method | `modifier` → invention roll · `institution` → research capacity [unbuilt] |
| T4-IN-02 | Joint-Stock & Public Credit | `institution` → public credit; debt interest terms (FINANCE.md, BL-073) [shipped seam] |
| T4-IN-03 | Patent Grants | `modifier` → invention roll · `institution` → property in invention [unbuilt] |
| T4-ME-01 | Preventive Inoculation | `modifier` → mortality rate (BL-273 demography) [shipped] |
| T4-MIL | *regime: flintlock line, artillery fortress, broadside fleet* | `unlock` + `retire` → roster turnover [unbuilt, BL-274] |

### Ring T5 — Industrial

| id | node | effects |
|---|---|---|
| T5-EN-01 | Atmospheric & Rotative Steam | `unlock` → Power Plant, coal recipe [designed] · `modifier` → output per worker across extraction |
| T5-EN-02 | High-Pressure & Compound Engines | `upgrade` → Power Plant tier [designed] · enables the two T5 Transport rows |
| T5-MA-01 | Coke Smelting | `upgrade` → the steel recipe at scale [shipped recipe] · `retire` → the charcoal route *if the Coke branch is taken* |
| T5-MA-02 | Machine Tools | `unlock` → machinery recipe (steel + refined copper) [designed] · `modifier` → construction pacing [shipped] |
| T5-MA-03 | Converter Steel | `upgrade` → steel output and cost [shipped recipe] · `unlock` → alloys recipe [designed] |
| T5-MA-04 | Framed Construction & Cement | `access` → build on steeper landforms [designed] · `unlock` → Warehouse / Storage Depot [designed] |
| T5-AG-01 | Farm Mechanisation | `modifier` → farm output per worker · `institution` → workforce release to industry |
| T5-AG-02 | Soil Chemistry & Fertiliser Trade | `resource` → fertiliser as a traded good [designed] · `modifier` → farm yield. *Artifact-class: importable without the chemistry* |
| T5-TR-01 | Railway | **`unlock` → Inland Logistics Hub [shipped]** · `reach` → intra-body haul cost [shipped] |
| T5-TR-02 | Steamship | `upgrade` → Port throughput [shipped] · `intel` → shorter route staleness (`route_fresh_ticks`) [shipped] |
| T5-IN-01 | Telegraph | `intel` → activity-fog freshness; `known_stale` decays slower [shipped] · `unlock` → market pulse at range |
| T5-IN-02 | Mass Schooling & Press | `modifier` → workforce efficiency · `institution` → the literate substrate T6 needs |
| T5-IN-03 | General Incorporation | `institution` → chartering by registration; the corporate form itself [unbuilt] |
| T5-ME-01 | Germ Theory & Sanitation | `modifier` → demographic growth (BL-273) · **`modifier` → tile hazard penalty [shipped]** |
| T5-MIL | *regime: rifle, ironclad, general staff* | `unlock` + `retire` → roster turnover · `doctrine` → general staff [unbuilt] |

Two of these are worth pulling out because they map onto **shipped** machinery with no new
mechanism at all. **Railway → Inland Logistics Hub** is exact: BL-149 shipped a placeable
non-producing building whose tile discounts intra-body haul cost, which is what a railway *is*.
And **Germ Theory → hazard penalty** lands on `tile_component.hazard_level`, already a
`(1 − hazard)` multiplier on every extraction building's output.

### The keystones — effects per branch

| keystone | branch | effects |
|---|---|---|
| **Fuel Doctrine** | *Coke* | `modifier` → smelting scale · `retire` → the charcoal route · gates on `fuel` |
| | *Charcoal* | `modifier` → output quality · keeps the forest-gated route open, forgoes scale |
| **Works Doctrine** | *State Arsenal* | `institution` → heavy plant is nation-owned; corporations lease capacity [unbuilt] |
| | *Private Works* | `institution` → corporations may own processing capacity outright [unbuilt] |
| **Labour Doctrine** | *Cleared Holdings* | `modifier` → urban workforce pool up, unrest up, rural demography down |
| | *Smallholder Tenure* | `modifier` → slower release, deeper domestic market, resilient rural demography |
| **Sovereign Doctrine** | *Chartered Capital* | `institution` → cheaper state credit, seizure carries a credit cost |
| | *Command Estate* | `institution` → immediate revenue, chronic credit penalty |

Every vertex quest carries exactly one effect — `open` → the ring region named in its `opens`
field. That is the capstone rule, and it is why quests need no effect vocabulary of their own.

---

## What A–G omits — seven categories the docs already imply

1. **Placement access.** Land Reclamation, Deep Mining and Framed Construction don't unlock a
   building; they make a *place* legal. The seam exists (`placement_rules::can_place`), and TILES.md
   already carries the two-axis terrain model this would key off.
2. **Continuous modifiers.** By count this is the biggest class in the region above, and A/B can't
   express it: nothing new appears, a rate moves. The machinery exists — BL-155 settled that margin
   modifiers accumulate as a **summed** rate applied once, never successive multiplications.
3. **Logistics reach.** Railway and Steamship change haul cost, range and throughput. SUPPLY.md owns
   this and it is fully shipped; PRODUCTION.md § Logistics leaves storage/throughput caps open, which
   is where an `upgrade` on Warehouse or Storage Depot would land.
4. **Resource realisation.** RESOURCES.md's Era 0 / Era 1 split is a tech effect nobody has written
   down: petroleum sits on the map long before Internal Combustion makes it worth extracting.
   Propellant is the sharpest case — designed, with no `resource_type` value.
5. **Demography.** Medicine and Agriculture move mortality, growth and workforce release. BL-273
   (province demography) is landed and reads exactly these parameters at the 1960 handoff.
6. **Finance and credit terms.** Joint-Stock, Public Credit and General Incorporation change what a
   corporation *is* and what it can borrow — FINANCE.md's interest model (BL-073) is the consumer.
   Adjacent to your **E**, but it is not a law.
7. **Instrument access.** A lens or ledger becoming available is a real unlock: LENSES.md has eleven
   built lenses and Supply gated. Telegraph unlocking the market pulse at range is the cleanest
   example — your **D** covers what the player *learns*, this covers what they can *look through*.

Two of these — **space** and **war/diplomacy** — you named as categories but they behave as
*target domains*: space is reached by `unlock` (launchpad, orbital port, ice extractor), `intel`
(survey), `reach` (space-mode convoys) and `open` (the Era gate), and none of those are space-only
kinds. Keeping them as domains rather than kinds is what stops the taxonomy growing a new kind
every time the game grows a new subject.

---

## Open questions

1. **Where does the effect vocabulary live?** BL-156 owns the tech object; the ladder store and
   `scripts/tech_tree.lua` would both need the same closed enum. Lean: declare it once in the
   ladder store's header (as the gate/diffusion vocabularies already are), and mirror it into the
   Lua tree when BL-087 authors content.
2. **Retirement's three questions** (§ Retirement) — **resolved 2026-08-06 (NR-066).** Grandfather
   (absent-not-disabled, settled 2026-08-05); explicit retirement used sparingly, mostly units and
   doctrine forks; permanent, no blockade-driven reversal.
3. **Do pre-game effects ever fire?** — **resolved 2026-08-06 (NR-066): descriptive only.** On the
   ancient layer nobody clicks, so its effects are read once at the **1960 handoff** as a
   description of the starting capacity band, not fired as live events. Only the campaign tree's
   effects fire live — the two trees do not share one effect runtime, only one vocabulary.
4. **One `modifier` target vocabulary, or per-system?** A summed-rate modifier needs a named
   subject (farm output, hazard, construction pacing, invention roll). That list wants to be closed
   too, and it does not exist yet.
