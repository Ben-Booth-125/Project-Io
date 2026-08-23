# Kepler — Historical Foundations

Why the campaign world is market-based and non-hegemonic, told as a stage ladder the
generation stack hangs dated history lines off. Companion to `../generation/PLANETOLOGY.md`
(which ends at the civilisation gate) and `../generation/NATION_GENERATION.md` /
`../generation/GENERATION_STRATEGY.md` (which assume the world this document explains).

## The epoch and the run

**The campaign epoch is 0 CE** (Ben, 2026-08-12; the refocus to the ancient era, NR-177). The
ladder runs up to an ancient start, not an industrial one. Stages 0–4 — agrarian surplus, the
enforceable promise, fragmentation-with-connectivity, capital disciplines the sovereign, the
energy transition — are pre-industrial in mechanism and sit inside the pre-epoch run. The old
Stages 5 and 6 (the rupture and saturation) lie past the epoch entirely and are DLC-era material
alongside the parked space arc; what replaces them is § Stage 5 below.

The pre-epoch history is **produced by a running simulation, not narrated over a finished map.**
The one-shot passes (`history_ladder`, `creeds`, `settlement`) found the cradles, the cultures and
the regions; the Era −1 sim (`src/world/history_sim.{hpp,cpp}`, BL-271) then plays the polities
forward from `start_year` to the epoch on a stepped year-tick clock, and the wars, borders and
collapses the campaign inherits are its output. Under an antiquity epoch the pre-computed
modern-era hinges — pre-resolved ruptures, a Charter Act written after the political map, the
globalisation event — do not run; the sim produces that history live.

**The span is a parameter, and the derivation lives in one place.** `era_minus_one.cpp` derives
`history_sim_params` from `world_params`: `start_year = epoch_year − prehistory_years`,
`stop_year = epoch_year`, and the tick bands. `prehistory_years` is a **scope knob, not a tuning
dial** — set to 0 it skips the pass entirely, which is how harnesses that do not test the era
avoid paying for it. Every harness derives its parameters through the same helpers, so no check
measures a different run from the one that generates a world (BL-462).

The clock is **stepped**: `history_sim_params::tick_bands` is a ladder of (until-year, years per
tick) pairs whose struct default is Ben's (2026-08-12) — 100 → 50 → 20 → 10 → 5 → 1 over 4000
years, with boundaries chosen so resolution concentrates near the epoch, 136 decision rounds
against 4000 on a flat tick. Generation's configuration flattens it to **one 4-year band
over `prehistory_years = 400`** — 100 rounds. The 4000-year ladder is the target (Ben, 2026-08-20:
the 400-year band is a placeholder, and the four allegories of `COLLAPSE.md` need the long run to
chain); making it affordable is BL-494 (four-thousand-year ladder) and `COLLAPSE.md` § The
4000-year problem. A settle-dominated run is the intended shape (NR-205, ruled 2026-08-12), at
either span.

## Purpose

The campaign premise makes three big historical claims:

1. **Markets, not command.** Every body hosts independent markets with dynamic prices; the
   background economy is saturated and privately legible.
2. **No hegemon.** ~45 nations coexist; none can conquer the rest; power projection happens
   through firms, not armies. The invariant is owned by **BL-224 (non-hegemony invariant)**, which
   turns "none can conquer the rest" from an assertion into an enforced property; its intended
   input is the `terrain_combat` scalars (`terrain_defence` / `terrain_attrition` /
   `terrain_resistance`, `src/world/terrain_combat.cpp`).
3. **Corporations as first-class actors.** An immortal legal fiction can own, trade, and expand
   across borders — and nations tolerate it. The *mechanism* — the enforceable promise, the
   charter — is settled. The *player* is not a free-floating corporation: the live seat is a
   mercenary company (`docs/CONCEPT.md`), and the rupture is **averted, not past** (Ben,
   2026-07-30, BL-223). Do not read any "why the player is a corporation" framing as settled.

None of these are natural defaults. Each is the residue of specific historical stages. This
document names those stages, states the causal lesson each one carries, and marks the hooks
where the generator's endowment variables (oxygen, ocean fraction, arable land, fossil fuels,
ore accessibility) bend the story per seed.

## The stage ladder

Stages are ordered and causal: each consumes the output of the one before. The planetology gate
chain hands off at Stage 0. Campaign play begins at the top of the ladder.

### Stage 0 — Agrarian surplus (hand-off from planetology)

**What happens.** Surplus food → dense settlements → division of labour → strangers who must
trade rather than share.

**Lesson.** Markets are a technology for cooperation between non-kin. They cannot precede
surplus and density.

**Generator hook.** Arable land and ocean fraction set *where* this happens first and how many
independent cradles emerge. More cradles → deeper eventual fragmentation (feeds Stage 2).
History line: `"-XXXX: First granary cities on the {region} floodplain."`

### Stage 1 — The enforceable promise

**What happens.** Contract law extends beyond kinship: partnerships, credit, and eventually the
charter — an entity that outlives its members and can own, sue, and be sued.

**Lesson.** The deep enabler of markets is not coinage but the enforceable promise between
strangers. Corporate personhood is this stage's apex, and the game's core rule — *the
corporation persists while it holds any asset* — is that legal doctrine made mechanical.

**Kepler divergence — the Charter Age.** Kepler needs a founding myth for corporate personhood,
its VOC moment, and it comes *early* relative to Earth. The registered-in-a-home-nation but
operationally-free compromise in `../generation/CORPORATION_GENERATION.md` is exactly the
bargain sovereigns strike with chartered companies: taxability and seizability in exchange for
freedom of operation. The charter cradle raises a **sealed-oath god** (`CREEDS.md`), and that
creed buys its regions an industrialisation-date bonus — contract law reaching capital one stage
early.
History line: `"YYYY: The {nation} Charter Act — first perpetual company registered."`

### Stage 2 — Fragmentation with connectivity (the non-hegemony ingredient)

**What happens.** Many competing states, close enough to trade, copy, and poach talent; none
able to conquer the rest.

**Lesson.** A hegemon can *choose* stagnation — suppress a technology by decree and it stays
suppressed. Forty-five rivals cannot: a capability banned in one polity re-emerges next door.
**Competition among states is the market mechanism applied to governance itself.**

**Kepler divergence — no unification epoch.** Kepler never had a Rome or a Ming. Its geography
(mountain ranges landing on borders) kept conquest expensive and exit cheap. This is the single
cheapest lore assertion with the largest downstream payoff: it explains why Kepler at the epoch is
more multipolar than Earth ever was.

**Generator hook.** Nation count is an outcome of landmass and of the ladder. The history pass
reads the final nation count back into the narrative: more nations → earlier and harder
Stages 3–4.
History line: `"YYYY: The {range} Peace — {n} realms confirm mutual borders."`

### Stage 3 — Capital disciplines the sovereign

**What happens.** Interstate competition makes states dependent on credit → dependent on
merchants and bond markets → forced into credible commitment. States that default or
confiscate lose the next war.

**Lesson.** This is how the state that *serves* markets (rather than merely taxing them) is
born. It is also why the campaign's nations respect corporate property: their ancestors that
didn't were outcompeted.

**Game-mechanical echo.** Asset seizure exists but is costly to the seizing nation (sentiment,
credit access). Progressive loss instead of a lose screen mirrors the historical norm:
sovereigns squeezed companies far more often than they destroyed them.

**What the generator carries of it.** The stage's *mechanism* — credit disciplining the sovereign
— is not simulated as such. Its two legible consequences are: the charter culture's sealed-oath
god buys its regions an industrialisation-date bonus (Stage 1), and the **war** rupture branch
costs both belligerents abundance rather than paying the winner. The seizure-cost half belongs to
BL-223 (averted rupture), alongside the diplomacy origin it defines.

### Stage 4 — Energy transition

**What happens.** Fossil fuels break the organic-economy ceiling; growth compounds; wealth
stops being zero-sum land.

**Lesson.** Coal-near-cities made Britain — endowment, not virtue. Once wealth compounds,
conquest's return-on-investment collapses relative to trade's, which changes what war is *for*.

**Generator hook.** This is the stage most sensitive to the endowment vector. Fossil fuel and
ore accessibility on owned tiles let the history pass *name* which nation industrialised first,
purely from tile data. Each seed gets its own Britain.
History line: `"YYYY: {nation} lights the first coke furnaces of the {region} basin."`

**How it is closed.** `run_settlement` scores each region's ancient fuel endowment against the
world's own mean, gates industrialisation on *above-average* fuel, and
`derive_national_character` names the three earliest furnaces once nations exist. The creed sits
on top of the endowment rather than beside it — a people who raised a forge god did so because
their cradle held ore (`CREEDS.md`), so that god's regions light up earlier. Endowment, not
virtue, in both directions. Under an ancient epoch the furnace date is past the stop year, and
the industrial clock the sim *does* run is the capacity ladder (§ The works roster).

### Stage 5 — The averted rupture

The original Stages 5–6 placed a WW3-scale rupture in the **past** — hegemony failing its final
audition, leaving corporations the only intact actors, discrediting conquest and forcing ambition
into economic channels — and then a saturated post-hegemonic economy as the campaign's starting
condition. Ben rejected that account on 2026-07-30: **the rupture is averted, not past.** The
campaign opens under the *threat* of the catastrophe, not its residue, and the saturation
economics survive on their own merits only as a restatement of `GENERATION_STRATEGY.md`.

The replacement — the bloc structure the threat produces, the diplomacy origin, the seizure cost
of Stage 3, and the reconciliation of where the rupture sits across `docs/CONCEPT.md` (the Era 0
exit) and `docs/research/ERA1_TECH_LANDSCAPE.md` (a visible countdown) — is **owned by BL-223
(averted rupture, diplomacy origin)**. `COLLAPSE.md` supplies the mechanism the pre-epoch version
of the same idea runs on: strain, culminations, and the reach-fed slow strain that BL-510 makes
Stage 5's parameterisation rather than a second system. The compact thesis's closing corollary
("learned this one catastrophe earlier than Earth did") is BL-223's to restate.

---

## The ladder pass — Stages 0–2

`src/world/history_ladder.{hpp,cpp}` (BL-221), a sibling pass that runs after the tile pipeline
and **interleaves with** nation generation; verified by `tools/verify/history_ladder_harness.cpp`
(H1–H5):

```
generate_body_tiles                 terrain exists
run_history_ladder            ->    cradles, fragmentation, Stage 0's line
nation_params_from_ladder     ->    fragmentation DRIVES the seed budget
generate_nations                    polities grow
record_institutional_history  ->    Stages 1-2, which need the outcome
```

Two entry points rather than one, because the Charter Act names a nation and the border accord
counts them — neither exists until the political pass has run. `record_institutional_history`
runs for a modern epoch; under an antiquity epoch those lines are the sim's to write.

**It drives, it does not narrate** (Ben, 2026-07-30). Stage 0's cradle count and Stage 2's
fragmentation are computed *before* the political map and shape it: a fragmented world seeds
more densely and lets smaller nations survive the merge. Asserted, not assumed — harness group H2.

**Nation count emerges; it is not tuned to a target.** Ben, 2026-07-30: *"Ignore the previous
assertions. We will simulate war to narrow down the count if needed. Just let naturally different
cultures emerge here."* `world_audit`'s R1 asserts the ladder's construction guarantee (the
derived floor can never fall below half the base) rather than a literal, and R3's ceiling is a
runaway guard rather than a target. Consolidation is the war stages' job — the creeds' tribal
marches and the Era −1 sim — never a dial.

**The cradle score.** `agrarian_score` scores cradles from arable terrain, landform,
habitability, coastal access and the biosphere's generated `endemics`. It marks where river
connectivity and domesticable clades slot in as refining terms.

**Determinism.** Stage tags `0x5A11` / `0xC4A7` / `0xF2A6` (none collides; PLANETOLOGY.md's
warning about `0x4A71012u` being folded twice stands). All scoring is integer with an explicit
tie-break by tile index; the cradle argmax keeps the lowest index on a tie. No transcendentals,
and the nation walk is over a sorted key list rather than raw `unordered_map` order.

**Stage 2 has a failure branch** — a hegemon instead of the multipolar accord — and the harness
prints the accord/hegemon split every run. Ben asked to *see* failure cases, so the rate at which
the branch is reached is a tuning target for the rarity sweep, not a number to hide.

---

## Settlement — Stages 3–4

`src/world/settlement.{hpp,cpp}` (BL-218, nations rewrite; BL-219, corporations rewrite),
sequenced between the creeds and the political map; verified by
`tools/verify/settlement_harness.cpp` (S1–S8):

```
run_history_ladder            ->  cradles, fragmentation
run_creeds / tribal conflict  ->  one pantheon per cradle; welding
run_settlement                ->  REGIONS: culture, ancient endowment, furnaces
run_history_sim               ->  the polities play forward to the epoch
generate_nations                  seeded on the region anchors
derive_national_character     ->  the three axes, as outputs
generate_corporations         ->  focus from the corp's home region
```

**The region is the unit, and it exists to carry two things at once.** A cradle is a *people*; a
nation is a *territory*; neither can say "these particular fields, under these particular gods,
sitting on this particular ore". The region can, which is why belief, endowment and industrial
timing all hang off it and why corporate focus reads straight out of it without a new mechanism.

**Pantheons are mapped, not re-rolled.** A region inherits its nearest cradle's culture, so the
distribution of gods across the map is a record of who walked where. That mapping then feeds back
into the material history: a forge god only exists where the cradle window held ore, so "the
forge god's country industrialises early" is not flavour laid over the data — it is the same fact
read twice, one stage apart.

**Scores are world-relative.** Every endowment class scores 500 at the world's mean and 1000 at
twice it. An absolute gain either saturates one class or never fires another; relative scoring
also survives the `deposit_scalar` abundance tier without re-tuning.

**Seeding changes, expansion does not.** `nation_params::seed_tiles` carries the region anchors
into Pass 1 and the growth machinery is reused untouched. The size variance emerges from where
people settled instead of being dialled in — the cheap alternative (keep Voronoi, narrate over it)
is the lying-figure problem: prose asserting a settlement history the territory does not reflect.

**The political axes are outputs.** `derive_national_character` sets expansionism from the
border-contest integral, economic focus from the resource class of the regions settled during
industrialisation, and ideology from industrialisation timing against neighbours, overwriting
the seeded Pass 4 draw, which remains the fallback for a body with no settlement.

**The record is destructible, and the hole is visible.** A won war plants the victor's pantheon
on the regions taken and erases the lines naming them, leaving a dated lacuna with a count of
what was lost (Ben, 2026-08-02). A conquered region keeps its founders in `founding_culture` and
its conquerors in `culture` — the erasure is of the record, never of the fact, which is the pair a
religion or diplomacy layer needs to describe a grievance.

**Ruptures draw through the checkpoint model.** The modern-epoch ruptures (collapse, war,
revolution — `resolve_historical_ruptures`) are a second checkpoint class, drawing through
`resolve_checkpoint` with eligibility as a filter and never a weight (BL-217's mechanism, reused
unchanged). They are bounded to the six most-contested nations, so their count is a property of
the design rather than of the map size. Under an ancient epoch the sim's culminations
(`COLLAPSE.md`) are the ruptures.

**Calibration is the sweep's, not the harness's.** A green harness means the pass is
self-consistent, deterministic and wired into the political map — *not* that its dates or
thresholds are calibrated against anything. `history_sweep` (BL-275) reports the distributions
across a seed spread, and that is where every magnitude in this layer is argued.

---

## The Era −1 sim

Ben's steer, 2026-08-02: the one-shot settlement becomes a **running year-tick simulation** to the
campaign epoch, the proving ground for the nation AI and the mil-sim, tuned against a seed spread
of earth-like worlds. And an **overturned decision**: simulated wars do not resolve as abstract
scalar comparisons; they fight with **real typed units and doctrine-parameter tactics** — the same
combat engine the main era inherits (`resolve_battle`, `docs/military/MILITARY.md`). "Drives, not
narrates" survives; the *abstract* half does not.

The sim is a polity loop over regions with **scored verbs** — Campaign, Settle, Invest,
Consolidate, and Build Work — priced in one shared currency (the round each consumes) and chosen
by a deterministic scored-utility layer under the 2026-08-18 nation grant: pure, seeded,
replayable, never a planner. Its objective weights (`w_farm`, `w_ore`, `w_port`, …) live in
`history_sim_params`. Breadth costs: a polity is charged supply for every region held past
`free_holdings` (`holdings_burden_q`, the burden of breadth), and the strain that burden feeds,
how it resolves, and the strategies that play it are `COLLAPSE.md`'s. Region demography
(BL-273), era-keyed unit rosters (BL-274) and the sweep (BL-275) are its siblings.

The registry of works below is a **parameter, not a global**: `run_history_sim` takes
`const works_registry*`, null meaning works disabled. The app passes its startup-loaded table; a
harness hand-builds one — the same dependency-injection seam `clear_markets` uses for the recipe
registry.

---

## The works roster

**What a pre-history polity builds, and what those works do** (BL-321, Era −1 works). The Era −1
layer has a noun axis with two halves: `unit_roster` says what a polity can **field**, and the
works roster says what it can **build** — an authored, endowment-gated table of works, each a
small id held by the **region**.

**It is not `building_component`.** That struct is the campaign-era building: a tile entity with a
recipe index, a workforce target, credit and resource build costs, per-tick maintenance and wages,
decommission state. Era −1 has no credits, no market, no recipe registry and no economy tick — it
has a year loop over regions. Sharing the struct would drag the campaign economy into the
generation layer to satisfy fields nothing sets.

**Where the table lives.** In `scripts/works.lua`, loaded by `src/world/works_registry.cpp` — the
one translation unit that pulls sol2 for it (Ben's call, 2026-08-07). `works_roster.hpp` stays
pure data and `works_roster.cpp` stays Lua-free, so the headless harnesses that are this sim's
verification link without Lua. Validation lives **in the loader**: a typo'd band or a duplicated
name is runtime data, and the loader throws at startup.

### The rows

Twenty-one, across the four `roster_band` values (cumulative — nothing un-invents a granary):

| Band | Rows |
|---|---|
| Classical | Granary · Channel Works · Ore Pits · Wall Circuit · Way Station · Harbour Mole |
| Medieval | Water Mill · Stone Fortress · Guild Quarter · Deepwater Wharf · Span Bridge |
| Gunpowder | Bastion Fort · Powder Mill · Counting House · Cut Canal · Naval Yard |
| Industrial | Blast Works · Rail Head · Signal Line · Arsenal · Coaling Station |

Availability is **derived from the ground** — a row is offered when the region's endowment
windows and population clear its gate. No research, no unlock events, no player choice. Names are
generic mechanism nouns; the standing rule that real history is a *mechanism* reference and never
a name source applies here as everywhere.

The band a region builds at is keyed off the polity's **materials** capacity, not its military
one. The unit roster reads the military column because that is the column whose rows turn over at
a roster boundary; a Blast Works turns over with metallurgy instead. Two tables, one band enum,
different columns.

### Reach is the row that matters

The burden of breadth charges a polity supply for every region held past `free_holdings`. With
nothing to buy against it, the only answer to the charge is to stop expanding, which makes the
frontier stall a **ceiling**. Works make it a **decision**: a polity that spends its rounds on Way
Stations and Cut Canals administers the same breadth more cheaply.

Relief is proportional and capped below 1000 in both places it applies, so building buys a
discount and never an exemption. The stall still arrives; it arrives later, and by the polity's
own choice. BL-224's non-hegemony stays emergent.

### Where each effect lands

| Effect | Consumer |
|---|---|
| `capacity_mod` | `region_carrying_capacity(farm_q, mod)` — raises the **asymptote** the logistic growth term climbs toward, so it changes trajectory rather than granting a step growth would re-flatten. Read by `advance_region_demography` and by the Settle verb's pressure test, which must divide by the same K. |
| `manpower_mod` | `manpower_ceiling(population, mod)`, read by `replenish_manpower` — so every caller gets the effect without being told works exist. |
| `reach_mod` | Two places: the **staging hub's** own works discount the terrain-weighted term of `supply_here` (scorer) and `supply_raw` (execution) — the two must agree, or a polity decides on one supply figure and fights on another — and the polity's **mean** reach relieves the burden of breadth. Mean, not total, so conquest alone cannot make an empire count itself as well-roaded. |
| `defence_mod` | Readiness on the defender's stack, which `roster_stack` turns into an additive per-mille offset on `type_power_mod` — the same channel cohesion uses, and for the reason `combat.hpp` gives: the engine scores whatever stack it is handed and knows nothing about walls. Also visible to the **scorer**, so a polity does not walk into a bastion it could not see. A Bastion Fort at +640 is worth ~+64 against row power values of 90–380: it tilts a fight, never decides one. |
| `industrial_mod` | The polity's mean industrial investment accelerates Invest's progress up the capacity ladder. The sim's industrial clock is that ladder — Stage 4's furnace date is `run_settlement`'s and is fixed before the loop starts — so the pull-forward is expressed against the mechanism that runs. |

### The verb

`build_work` is the fifth scored candidate, priced in the shared currency alongside campaign,
settle, invest and consolidate — **not in a treasury Era −1 does not have, but in the round it
consumes**. Building a Way Station competes directly with raising an army because both spend the
polity's one action that round, which is the whole point of putting it on the shared scale rather
than beside it.

A work's benefit splits by who receives it: local effects are valued against the region's own
endowment, reach against the polity's **mean** holding. Scoring reach against the *total* makes it
worth ~40× every local effect and reduces the roster to its five road rows.

**Bounded by construction**, like the other four verbs. Two candidate regions per polity per
round — the capital, plus one rotated through the holdings by a hash of (polity, year, slot).
Scoring every holding would be O(held × rows) inside the most expensive pass in generation; the
rotation still reaches every region over a run, because the round count is large against any one
polity's holdings.

**A work saturates.** `apply_work_to_region` refuses to build the same row twice, so a region's
works are a finite investment and the polity is eventually forced to spend its rounds on
something else. Without that refusal the scorer would re-raise the same Granary every round it
stayed the best candidate.

### Storage

`region::works_built` is a **32-bit mask** plus five plain-int accumulators — no heap ownership,
because the struct is copied on every Settle and lives in a vector that grows past 400 entries.
The accumulators are maintained incrementally, so every read is O(1) against a write that happens
at most once per polity per round. The mask imposes a hard 32-row ceiling on the table, which the
loader checks: a 33rd row would otherwise be authored, validated, offered — and silently
impossible to build.

Works are **physical**: conquest transfers them with the region rather than razing them, which
is what makes a developed region worth taking.

### Magnitudes

The magnitudes in `works.lua` and every `w_work_*` weight in `history_sim_params` are **authored
by judgement, not calibrated** — placeholders in the same sense as the surrounding `w_*` weights,
put at plausible magnitudes so works land in the same band as the other verbs. `history_sweep`
carries the roster on every run and reports works raised per world; whether the frontier stall
moves from ceiling to decision is a question for that sweep's numbers.

---

## The compact thesis

> **Markets scale where no one can dominate, and no one dominates where exit is cheap** — a
> rival state next door, a rival market next planet.

Stages 0–2 generate it. What Kepler learned from its averted catastrophe, and when, is BL-223's
to state.

## Open questions

- Should the averted rupture's claimants and outcome be seeded (varying per campaign) or
  canonical? Seeded is consistent with the rest of the generation stack; canonical is cheaper to
  write quest/tech fiction against. *BL-223's to answer.*
- Does Kepler get a nuclear-equivalent deterrence ceiling explicitly, or is the threat's memory
  alone the ceiling? (Affects Era 1+ conflict design.)
- How much of this ladder surfaces to the player — a codex, generated history lines only, or
  ambient flavour in quest text? `COLLAPSE.md`'s ruling that the story surfaces in the history
  tab, quietly, answers the pre-epoch half.
- Does the Charter Age line name the player company's home-nation legal tradition? The Charter
  Act names the charter cradle's nation; whether the player's company is bound to it depends on
  the player-identity question `docs/CONCEPT.md` owns.
