# Project Io — Military

The military layer is **how force works**, as opposed to how a rival *decides* to use it — that
stays in [`docs/ai/AI_OPPONENT.md`](../ai/AI_OPPONENT.md). Its parts are: two battle resolvers
and the calibration they share; a terrain model; a unit that is a group token on a tile; a muster
building and the hire verb that raises units at it; an era-keyed roster; a march seam; an
engagement trigger that turns stance plus position into a fight, and the two surfaces a fight
reports through; a per-tick upkeep pass; and one rule above all of them — **economic reach *is*
military reach**.

The relational substrate a fight reads — directed hostility, symmetric friendship — is owned by
[`docs/politics/RELATIONS.md`](../politics/RELATIONS.md). This document owns what hostility
permits *militarily*: interdiction, engagement, and the march queue.

---

## The two resolvers

There are **two** battle resolvers, and the split is deliberate. Ben ruled it 2026-08-13,
overturning BL-315's (armed house conflict spine) own earlier engine-parity strand.

They answer different questions. `resolve_battle` answers "region beats region, this year" in
one scored evaluation, because the Era −1 sim runs millions of those. `resolve_campaign_battle`
answers "two forces in a province, over a short span, with a player who may pull out".

**What is shared is the calibration, not the resolver.** The roster is single-sourced, and every
campaign round scores its powers by calling `resolve_battle` and reading the two power numbers
out. The campaign path adds round structure, a seeded swing and withdrawal on top.

### `resolve_battle` — nation scale

`src/world/combat.{hpp,cpp}` (BL-272, unit/doctrine combat model). A pure function of its inputs:
matchup × doctrine × terrain × supply × season. No RNG, no world reads, no hidden state.

Its callers are `src/world/history_sim.cpp` — the Era −1 history sim (BL-271, Era −1 sandbox) —
and, per round, `campaign_battle.cpp`. The campaign layer never calls it directly;
`tools/verify/combat_harness.cpp` calls it to assert.

Arithmetic is **integers in per-mille throughout** (1000 = neutral). Battle outcomes move borders
in the sandbox, so no float decides who wins.

**Inputs.** Two `std::vector<army_stack_entry>`, two `doctrine_row`s, a terrain triple
(`terrain_substrate` + `terrain_cover` + `cover_density`) plus a `terrain_landform`, a `season`,
and two supply values 0..1000 (clamped). Nothing is rejected — an empty or all-naval stack
resolves rather than erroring.

> **The degenerate case that follows from "nothing is rejected", and what guards it.** Naval
> entries score EXACTLY zero, and the victory test is a strict `>`. So a fight where BOTH sides
> are empty or all-naval resolves as a **defender victory with 400/200 per-mille losses** —
> casualties inflicted on forces that scored no power at all, in a shape indistinguishable from
> a real outcome. The engagement trigger opens a battle on stance and position alone and never
> inspects unit class, so nothing downstream would catch it; `battle_system.cpp`'s
> `stack_can_fight` screens both stacks before opening. Unreachable with a land-only roster,
> and guarded rather than left to be found by the first naval row.

An **`army_stack_entry`** is one unit type's contribution, already reduced to numbers:
`{type_id, cls, count, type_power_mod}`. It is deliberately **not** a lookup key into a roster
table — the engine scores whatever stack it is handed and does not know which era it is refereeing.
`type_id` is carried for the caller's bookkeeping and never interpreted.

**Unit classes** are five and coarse: infantry, cavalry, ranged, siege, naval. Base power per unit
is 100 / 130 / 90 / 150 / 0.

**Naval is strategic-only.** A naval entry contributes zero power *and* zero weight to the matchup
average. Naval rows exist in the roster and naval entries are accepted; naval presence is strategic
tagging, and there is no tactical naval resolution.

The **class matchup matrix** is a rock-paper-scissors core: infantry beats ranged, ranged beats
cavalry, cavalry beats infantry. Siege is uniformly weak in the open field, because there is no
fortification — no held position to reduce — to give it its real job.

Both sides are looked up — `matchup(attacker, defender)` for the attacker's own power,
`matchup(defender, attacker)` for the defender's. Each is a count-weighted average over the
opposing composition, with a single division at the end.

A **`doctrine_row`** is pure modifier data: `frontal_bonus`, `flank_fragility`, `mountain_penalty`,
`stance`. Adding a doctrine is adding a value of this struct; it never touches `resolve_battle`.

`flank_fragility` is modelled as an intrinsic weakness of the formation, not conditioned on the
opponent actually flanking — a stated first-cut simplification. `mountain_penalty` fires only on
`terrain_landform::mountain`.

`siege_stance` (`field` / `assault` / `invest`) is carried on the row so a siege is a doctrine
choice rather than a separate code path. `resolve_battle` does not read it — the field is
declared for the fortification model and unconsumed until one exists.

**Terrain and supply.** `terrain_defence` multiplies the **defender only**. `terrain_attrition`
costs **both**, scaled ×1.5 in winter (no other season is distinguished), and mitigated by supply:
1000 cancels attrition entirely, 0 takes the full hit.

**Tie-break: the defender wins an exact tie.** Holding ground is the default outcome of an
inconclusive engagement, and it is called out explicitly rather than left to comparison order.

**Outputs** (`battle_outcome`): result, both final powers, both loss fractions, and
`decisiveness` = `(winner − loser) / winner × 1000`. Losses are **per-mille of each side's own
committed count**, not absolute numbers — `resolve_battle` never mutates a stack, so spending them
is the caller's job.

Loss shape: the loser takes `400 + 0.6 × decisiveness`, the winner `200 − 0.2 × decisiveness`,
both clamped to 0..1000.

### `resolve_campaign_battle` — campaign scale

`src/world/campaign_battle.{hpp,cpp}` (BL-315, armed house conflict spine). Campaign-scale: two
forces in a province, fought over a bounded number of rounds, with priced withdrawal. Its
production caller is `run_battles` (§ Battles); `tools/verify/campaign_battle_harness.cpp`
asserts it in isolation.

**The randomness is real to the player and absent to the engine.** Every draw comes from a
splitmix64 stream folded from the battle's own identity, consumed in a fixed order. The same save
replays the same battle; the player, who cannot see the seed, faces a genuinely uncertain fight.

The **`campaign_battle_identity`** folded into that seed is
`{attacker, defender, province, tick, world_seed}`. The attacker/defender pair is packed in role
order, not sorted — swapping the roles is a different battle and must be a different stream. The
province sits in the fold where a tile index otherwise would (Ben's ruling 1, 2026-08-19: the
province frames the fight), keeping the stream one field wide. `world_seed` is the province
partition's seed — `world` carries none of its own, and the partition's is stable for the
world's lifetime.

**The span is modelled explicitly.** `begin_campaign_battle` opens a fight and resolves nothing;
`step_campaign_battle` fights one round; `withdraw_campaign_battle` breaks off;
`campaign_battle_result` reads the record. A caller steps it round by round, so the withdrawal
window is a real window rather than a parameter pretending to be one.

`resolve_campaign_battle` is a convenience wrapper over those, taking a **scripted** withdrawal
plan. It adds no rules of its own, so a scripted run and a stepped one resolve identically.

**Each round.** Both stacks are scaled by remaining strength (entries scaling to zero drop out),
`resolve_battle` scores the two powers, then exactly two draws apply a swing — attacker first,
defender second, always in that order.

Tuning lives in `campaign_battle_params`, as data, on the `corp_ai_params` precedent:

| Field | Default | Meaning |
|---|---|---|
| `max_rounds` | 6 | Rounds before the fight is called a stalemate |
| `swing_permille` | 600 | Half-width of the per-round power swing — a uniform draw in [400, 1600] |
| `rout_threshold` | 300 | Strength at or below which a side breaks and the fight ends |
| `round_loser_loss_base` / `_margin` | 90 / 400 | The round-loser's attrition: flat base plus a share of the margin |
| `round_winner_loss_base` / `_margin` / `_floor` | 45 / 20 / 5 | The round-winner's, falling as the round gets lopsided |
| `withdraw_base_permille` | 20 | Flat price of turning your back at all |
| `withdraw_per_round_permille` | 25 | Added per round already fought |
| `withdraw_pursuit_scale` | 400 | Share of how far behind the withdrawing side is |
| `rounds_per_tick` | 3 | Rounds fought per economy tick (Ben's ruling 3, 2026-08-19) |

Attrition is applied **multiplicatively** to remaining strength, so a long fight compounds rather
than sums past zero.

`swing_permille` is 600 rather than 300 (NR-204): at 300 a 1.4:1 attacker won 99% of fights,
which Ben ruled too safe. Measured at 600 over 1000 seeds per ratio: 1.0:1 → 50%, 1.2:1 → 76%,
1.4:1 → 89%, 2.0:1 → 99.8%.

`rounds_per_tick` at 3 against `max_rounds` 6 means a full battle spans two ticks and the player
gets exactly one real between-tick decision. Raise it and the fight resolves before anyone can
react; lower it and a campaign tick becomes a tactical game, which Ben's "a short time" ruled out.

**Withdrawal is a first-class verb, not a failure state**, and it is never free. The cost has three
terms — base, per-round, and pursuit — and the second and third are what make a late break-off
dear. Rounds already fought have separately cost their own attrition, so a late exit is expensive
twice over.

Withdrawing consumes **no draws** — disengaging is a decision, not another gamble.

**Who holds the field.** On a withdrawal, the side that stayed holds it, even if it was losing on
points; ground is held by being on it. Breaking is checked *after* a round resolves, so a routed
side still took and dealt that round's damage. Both breaking at once resolves in the defender's
favour, consistent with every other tie across both paths.

Ends are enumerated: `attacker_broken`, `defender_broken`, `attacker_withdrew`,
`defender_withdrew`, `stalemate`. Withdrawal is its own end state rather than a flavour of defeat.

---

## Terrain defence and attrition

`src/world/terrain_combat.{hpp,cpp}` (BL-233, terrain combat modifiers). Three pure functions of a
tile's character on the split axes — **substrate**, **cover × density**, and **landform**. No
stored field, no component change, nothing on the serialisation seam.

They exist because a single `is_barrier` bool conflates defensible ground, attritional ground and
open water. A mountain and a barren plain would score identically while a forest scored nothing.
The two functions are those first two things; water is the third, and it is a mode rather than a
number (below).

**Cover is graded by density** (Ben, 2026-08-21): an authored cover value is the value at full
canopy, scaled by `0.55 + 0.45 × cover_fraction(density)` and rounded, so a thin wood is worth
less than a closed one and a cover barely there barely counts.

**`terrain_defence(sub, cov, density, lf)` — how well ground holds, 0..1000.** The sum of three
terms, clamped. Landform dominates — the shape of the ground is what a defender uses; cover is
next; substrate is small, and that is the honest reading.

| Landform | Defence | | Cover (at full density) | Defence | | Substrate | Defence |
|---|---|---|---|---|---|---|---|
| mountain | 1000 | | marsh | 294 | | volcanic | 150 |
| rift | 700 | | urban | 220 | | rocky / icy | 100 |
| canyon | 650 | | forest | 219 | | barren / sedimentary / regolith / metallic | 0 |
| highland | 400 | | scrub | 73 | | ocean / coast / lake | 0 |
| crater | 300 | | dunes | 60 | | | |
| valley | 150 | | snow | 40 | | | |
| plains | 0 | | ash | 20 | | | |
| | | | grass / salt / none | 0 | | | |

Marsh is the cover standout, on the reasoning that a bog has stopped more armies than most
mountains — impassable to weight without being high at all. Urban ground is fought for street by
street. A crater's rim helps; its basin does not.

**`terrain_attrition(sub, cov, density, lf)` — what crossing costs an army in supply, 0..1000**,
independently of any defender. The substrate is what the ground *denies* — forage, water,
shelter; the cover is what it *gives back*, subtracted as relief; landform adds the climb.

| Substrate | Cost | | Cover relief (at full density) | Relief | | Landform | Adds |
|---|---|---|---|---|---|---|---|
| regolith | 900 | | grass | 614 | | mountain | 200 |
| icy | 850 | | urban | 400 | | rift | 120 |
| barren / metallic | 800 | | forest | 384 | | canyon | 100 |
| volcanic | 700 | | marsh | 294 | | crater | 80 |
| sedimentary | 600 | | scrub | 147 | | highland | 60 |
| rocky | 400 | | none | 0 | | valley / plains | 0 |
| ocean / coast / lake | 0 | | salt | −40 | | | |
| | | | dunes / ash | −60 | | | |
| | | | snow | −80 | | | |

Bare sedimentary ground reads harsh on purpose: soil with nothing growing on it is a dust bowl, not
a meadow — the meadow is the grass, and grass reliefs hardest because open grazing country is the
best marching ground there is. Woodland, for all its fuel and shelter, is slower. A **negative**
relief is a cover that costs: snow, dunes, ash and salt make the ground beneath them worse.

The split is what lets a forested rocky upland read as easier than a bare one — a forested rocky
mountain defends like woodland on rock (100 + 219·d + 1000) and forages like woodland on rock
(400 − 384·d + 200), where a single composition slot had to pick one.

**Water returns 0 from all three**, for every water kind (`is_water(sub)`: ocean, coast, lake).
Water is a movement *mode*, not a magnitude, so the caller must branch on it rather than read a
value — the same way logistics branches land/sea.

**`terrain_resistance(sub, cov, density, lf)`** combines them 60/40 toward defence. A defensible
frontier shapes a border more strongly than a merely expensive one — a frontier sits on a ridge,
not a moor — but attrition alone must still be able to stop an army crossing an ice cap nobody is
defending. **Neither resolver reads it**; its consumers are `src/world/history_ladder.cpp` (pricing
conquest at generation time) and `tools/verify/world_audit.cpp`.

---

## The unit model

A unit is a **group token**, not a stat block. `unit_component` in `src/world/components.hpp`
carries six fields:

| Field | Type | Notes |
|---|---|---|
| `position` | `entity_id` | **The tile the unit occupies.** Tile-canonical — see below. |
| `owner` | `entity_id` | Corporation or faction entity that controls it. |
| `count` | `int` | Number of men in the group. |
| `type` | `uint16_t` | Opaque index into the roster table. |
| `order` | `movement_order` | `{dest, path, next_index, progress}`; `dest == null_entity` means halted/unordered. See § Marching. |
| `supply_factor_permille` | `int32_t` | How well supplied, 1000 = full. Moved only by the upkeep pass's one decay rule. |
| `muster_base` | `entity_id` | The `military_base` this unit was raised at, or `null_entity`. The **orphan key** — see § Upkeep. |

**There is no stored strength.** Strength is derived — `unit_strength(w, u)` in
`src/world/unit_roster.hpp` returns `count × roster_type_quality × supply_factor` at ×100 fixed
point, so at quality 1.0 and full supply `strength == count * 100` exactly. It returns an
**integer** on purpose, so a sum over the unordered `w.units` stays order-independent. Quality is
derived from the row's `power_mod` rather than authored as a second number, so there is one
per-type figure and it cannot drift from the one combat reads: Levy Spear (power_mod 0) is 1000,
Rifle Regiment (380) is 1380.

**Position is tile-canonical** (BL-157, military datamodel stub; Ben, 2026-08-07), over body or
region. A unit is *on* a tile; the province that tile belongs to is who it is in a fight with
(§ Battles) — the two grains answer different questions.

Units live in `world::units`, an `unordered_map<entity_id, unit_component>`. Every consumer that
aggregates over it does so order-independently (integer sums, owner filters, or an explicit sort
by entity id), which is what keeps the container's iteration order off the determinism seam.

**Four writers exist**, and no other code creates a unit:

- `src/world/hard_coded_world.cpp` — the Kepler player stub, count 50, type 0. No `muster_base`
  (that world has none), so it is deliberately never orphaned.
- `src/world/corporation_generation.cpp` — `seed_starting_military`: every non-background corp's
  starting unit beside its starting base (BL-331, starting military presence; BL-476, rivals start
  armed), count 50, roster row 0, placed on the nearest valid land tile to the HQ. Records that
  base as its `muster_base`. BL-365 background firms are unarmed on purpose.
- `src/world/corp_command.cpp` — the `hire_unit` verb, count `hire_batch_manpower` = 50. Records
  the tile's qualifying base, lowest entity id first.
- `src/world/nation_generation.cpp` — a nation's garrisons, owner set to the nation entity, no
  `muster_base` (a nation raises no base). See § Nation garrisons, below.

**The bridge from a unit to the resolver** is `unit_to_stack_entry(w, u)`
(`src/world/unit_roster.hpp`), the adapter that resolves a live unit into the value both resolvers
accept: supply is carried in the entry's `count`, quality in its `type_power_mod` — deliberately
not both in one place, since doubling quality into count would square it against `unit_strength`.
`run_battles` calls it for every participating unit. `roster_stack` — the other composition path,
from *region manpower* — is Era −1's alone.

**What reads a unit.** Two condition subjects — `military_units` (summed counts) and
`military_strength` (summed derived strengths, an integer sum) — in `src/world/condition_set.cpp`,
available to laws, techs and quests. The Selection element's unit card (Strength / Roster pages)
and the hover summary in `src/ui/entity_summary.cpp`, both printing the derived strength with its
`count × quality × supply` derivation. The corp AI, counting its own units against a soft cap of
three. The battle pass, the march pass, the upkeep pass and the budget's `upkeep` term. The
blackboard export, for a corp's own units.

**The unit on the canvas.** A unit has its own marker and command surface — BL-471 (unit marker
and command surface) owns both. A unit is also reached by clicking its tile: the Planetary canvas
cycles unit → building → tile on repeat clicks of the same tile.

---

## The muster interface

`building_type::military_base = 6` is the single economy → military interface. It is where units
come from, and the only place they come from.

**The building.** A tile-occupying installation on any non-ocean land tile, with no deposit
requirement. It produces nothing and staffs at zero, like the port and the logistics hub.

It obeys the ordinary reach rule at the world level and earns **no anchor exemption** — see
§ One reach field. Its glyph is the filled shield in `icons::building`; `scripts/economy.lua`
prices it at build cost 300, 35 steel, 4 ticks, maintenance 15.

**A military base accumulates nothing.** It gates hiring and it occupies a tile; it does not
produce a currency. The economy → military interface is the base tile plus the reach field, and
the recurring cost of force is credits and ordnance (§ Upkeep). A military-capacity scalar, if
one is ever wanted, is a fresh field with its behaviour filed in the same change.

**It is gated behind a technology, not a build menu.** `E0-ML-01` "Standing Garrison Doctrine"
(`src/world/tech_gate.cpp`) unlocks it on two extraction sites plus a Cr 2,000 balance. A military
*subject* is deliberately not used in that predicate — gating the first military building on
already having a military would be circular.

**Hire happens at the base** (BL-325 S2). `corp_verb::hire_unit` takes `{tile, unit_type}` and
checks, in order:

1. `unit_type` indexes the roster table, else `rejected_invalid`.
2. That row is available to this corp under the **live** gate — re-checked at apply time, so a caller's cached list is refused. Else `rejected_invalid`.
3. `tile` names a real tile, else `rejected_invalid`.
4. **That tile carries the acting corp's own `military_base`, completed (`ticks_remaining <= 0`) and not decommissioned.** A base still building does not qualify. Else `rejected_placement`.
5. The corp can pay **both** legs, else `rejected_funds`.

The two payment legs. A **credit cost** of
`hire_base_cost + hire_cost_per_power × row.power_mod` (economy.lua § military: 40 + 0.5 × power,
so Levy Spear costs 40 and Rifle Regiment 230). Plus a **flat resource draw** of 5.0 per *gated*
axis, from the corp's `(corp, body)` pools, drained in ascending body-id order.

`port_q` is exempt from the resource leg — it is a building check, so there is nothing to debit.
The other three axes draw from the same preference order the gate reads, so a hire never spends a
resource the gate did not already confirm access to.

The credit base is a **floor**: row 0's gate is all-zero, so the resource leg alone would test and
spend nothing, and the cheapest rows would be free and unbounded for any seam caller. Both legs are
checked before either is debited, so a refusal leaves the corp wholly uncharged.

On success: a new unit entity at the muster tile with `count = hire_batch_manpower`, returned
through `apply_corp_command`'s out-param. The verb records the base it was raised at as the unit's
`muster_base`; where the tile carries more than one qualifying base, the **lowest entity id**
wins, so the choice is deterministic.

The player's press then selects the new unit; the rival scorer's path additionally reports an
`agency_event::kind::hired` for the decision feed and session history. The hire price is shown on
the press (BL-405, hire price on screen).

**Rivals hire through the same verb** (the BL-324 widening of the AI-agency exception, standing
rules § Determinism & data model). `src/world/corp_ai.cpp` scores hiring alongside build, demolish,
survey and road; a corp with no completed base has no muster tile and must build one first, priced
by the ordinary build-candidate machinery. A soft cap of three units per corp keeps hiring from
becoming an unconditional extra action every evaluation. Availability is decided by stockpile and
market access alone; the credit cost is subject to the solvency gate like every other spend
(Ben, 2026-08-13, NR-218).

### The roster

`src/world/unit_roster.{hpp,cpp}` (BL-274, era-keyed unit rosters). An authored **table**, not a
tree: availability is derived from ground and era band, with no research, no player choice and no
unlock events.

**Nineteen** rows across four cumulative bands — `classical` 7, `medieval` 4, `gunpowder` 4,
`industrial` 4. Bands are cumulative because nothing un-invents a spear: an industrial polity still
fields levies where its ground cannot pay for better.

Each row carries `{name, band, cls, gate, power_mod, weight}`. The `roster_gate` is four thresholds
on 0..1000 endowment axes: `ore_q`, `farm_q`, `port_q`, `energy_q`.

Two of those axes are **named substitutions** — the convention is to name the stand-in, never fake
the input. `farm_q` proxies for pasture, because horses have no signal of their own. `energy_q`
proxies for saltpetre, then genuinely for fuel from the industrial band.

**The roster lives apart from `combat.{hpp,cpp}`**, deliberately. `combat.hpp` states in its own
words that it scores whatever stack it is handed and knows no roster; so the roster lives
separately and *resolves into* combat's types.

**Two gate paths, one table.** `available_rows(const region&, band)` is the Era −1 path, gated on
authored region endowment. `available_rows(const world&, corp, band)` is the campaign path, gated
on the corp's own stockpile and market access.

The campaign gate is **binary by design** — each axis is 1000 or 0, the table's own max threshold.
"You may field rifles because you can buy steel" is a yes/no supply-chain question here, not a
graduated dial; quantity-scaled readiness is a follow-on.

| Axis | Campaign source |
|---|---|
| `ore_q` | Holds any steel, iron ore or iron-nickel ore |
| `farm_q` | Holds any food rations or agricultural produce |
| `port_q` | Owns a `port` building anywhere |
| `energy_q` | Holds any coal, refined fuel or petroleum |

**`campaign_roster_band_for(era_band)` derives the band from the campaign's own era.** It is
fixed at no single value, and it is **not** derived from a military-capacity score — that is the
Era −1 settlement model — but from `recipe_registry::era()`, the same coarse ancient/industrial
split `era_band_for_epoch` gives every other era gate: an ancient campaign opens at the roster's
lowest band (`classical`), an industrial one at its highest (`industrial`). Because bands are
cumulative, every earlier row the corp's ground supports is exposed either way.

`roster_stack(manpower, region, band, readiness)` composes an army stack from region manpower.
Later bands crowd out earlier ones — a row's weight decays ×0.35 per band it sits below the
polity's own, so a rifle-era army is not half levy spears. Readiness folds into each entry's
`type_power_mod` as `(readiness − 1000) / 10`, which is the same channel cohesion and defensive
works use.

---

## Marching

A unit is not pinned to its muster tile. Three `corp_verb` rows extend the **same** corp-command
seam every other action goes through — there is no parallel `unit_command` type (BL-470 ruling 2;
BL-314's unit verb family resolves to this seam, extended):

- **`march_unit {subject: unit, tile: dest}`.** Sets/replaces the unit's `movement_order`. Rejects
  `rejected_not_owner` (not the caller's unit), `rejected_invalid` (unknown unit/tile, ocean dest,
  dest off the unit's own body, or unreachable), `rejected_state` (dest already `== position`, or
  the unit is in a battle — `unit_in_battle`; walking away from contact is a priced withdrawal,
  never a march).
- **`halt_unit {subject: unit}`.** Clears the order. `rejected_state` on a unit with none.
- **`disband_unit {subject: unit}`.** Erases the unit outright. No refund — manpower walks away;
  a confirm step, if any, lives in the UI (BL-471), never in the seam.

Merge/split and garrison/scout are formation verbs, owned by BL-472 (formations).

**Movement is a path march, not a hop.** `march_unit` computes the path ONCE, via
`intra_body_path` (`src/world/logistics.hpp`) — the same terrain-weighted A* the convoy system and
`body_reach_field` use — and stores it on `unit_component::order`. No per-tick Dijkstra:
`run_unit_march` (`src/world/economy_system.cpp`) only re-paths when a step is found blocked. Terrain
never changes post-generation, so the guard is defensive, and the determinism harness exercises it
directly.

**March points are per-CLASS, authored data.** `economy.military.march_points_per_class`
(`scripts/economy.lua`), keyed by the roster row's `cls`. Spent per tick against the per-tile
traversal-cost weight (`logistics::tile_traversal_cost`, the same plains=1.0/mountain=2.0 table
road placement discounts), with fractional remainder banked in `order.progress` across ticks.
Defaults: infantry 1.0, cavalry 1.5, ranged 1.0, siege 0.5, naval 0.0 (there is no naval movement
model — `combat.hpp`'s own "strategic-only presence"). A composite unit (BL-472, formations) reads
its **slowest** component's class entry.

**Visit order — NR-344, "war flips the queue".** At peace, convoys claim the network first:
`advance_convoys` runs in the sim loop *before* `run_economy_step` is called (`main.cpp`/`app.cpp`),
a stated rule rather than an accident of phase order. A corp party to **any** declared hostility,
either direction (`is_hostile` checked both ways against `corp_hostile_pairs` — being attacked
mobilises too), has its own units visited **first** within `run_unit_march`, evaluated once per
tick from the stance table. The contention this decides is over the Logistic Points pool
(BL-464, Logistic Points — [`LOGISTICS.md`](../economy/LOGISTICS.md)); absent a shared pool, the
order is written down and changes nothing observable.

**The blackboard export shows a corp its own force.** `export_corp_blackboard`
(`src/world/corp_ai.cpp`) emits `unit_tile`, `unit_type`, `unit_count`, `unit_strength` and
`unit_order_state`/`unit_order_dest` facts for the corp's OWN units — no BL-068 visibility
question, exactly like cash/buildings/pools. Rival units carry no unit-subject facts in the
export. The rival scorer can legally issue `march_unit`; scoring march *quality* is BL-450's
(rivals score stance) conflict-AI arc.

---

## Battles

`src/world/battle_system.{hpp,cpp}` (BL-467, battle state in world) is both halves of making
force meet stance: it discovers that a fight should happen, and it owns the fight between ticks.
`run_battles` is called from `run_economy_step` immediately **before** `run_unit_march` — so a
unit in contact fights this tick before it can march out next tick — and before
`run_unit_upkeep`, whose orphan cleanup is what disbands a unit the battle reduced to zero. There
is deliberately no second disband mechanism.

**Three rulings** (Ben, 2026-08-19):

1. **Footprint is the province envelope.** Hostile units standing in the same province are drawn
   into one battle; the province frames the fight ([`PROVINCES.md`](../generation/PROVINCES.md)).
   Units never abstract into a pool — they keep their tiles.
2. **Terrain is one tuple for the whole battle**, exactly as the resolver takes it. Per-unit
   terrain was offered and declined. The ground is the **defender-side tile with the highest
   `terrain_defence`** in the envelope, ties broken by ascending tile id — `battle_ground`,
   exposed as a named function so it can be asserted.
3. **Pacing is several rounds per tick** — `campaign_battle_params::rounds_per_tick`.

**The trigger.** A battle opens when a corp with a **directed** hostility toward another has units
in a province where the target also has units. Directed suffices — the ambush property stance
pays two tables for: the declarer attacks, the target is drawn in regardless of what it thinks.
Two neutral corps sharing a province do not fight; the trigger is stance, not proximity. Friendship
permits what BL-549 (friendship permits two things) names; it gates nothing here. A corp-vs-nation
second trigger, over an active mercenary contract rather than a declared hostility, is § Nation
garrisons, below.

**Identity is (province, attacker, defender).** A corp pair fights at most one battle per province
at a time; mutual hostility opens the pair once, in the direction whose attacker sorts lower — a
property of the ids, not of who declared first. A third corp arriving opens its **own** battles
against each existing participant rather than joining theirs. **Membership is snapshotted at
open** — a unit marching in after the first round reinforces nothing — and the participating unit
ids are stored ascending per side.

Both sides are screened by `stack_can_fight` (§ `resolve_battle`). Doctrine is `doctrine_row{}`
on both sides — no per-corp doctrine field exists, so the doctrine machinery contributes nothing
to a campaign battle until a formation carries a row (BL-472, formations). Season is `summer` —
the world carries no season, so the winter penalty is a history-sim-only effect. Side supply is
the mean of the participating units' `supply_factor_permille`; per-unit supply and quality have
already entered through `unit_to_stack_entry`, so an unsupplied unit is weaker in the resolver
with no second mechanism.

**What makes it deterministic.** The containers a battle is discovered from are unordered, so every
stage recovers order explicitly: candidates are sorted by (province, attacker, defender) before any
battle opens; `world::battles` is kept in that order (`battle_order_less`); unit ids are ascending;
the terrain pick is an argmax broken by tile id; and no stage consumes a draw outside
`step_campaign_battle`. `state_hash` folds the battle list.

**Per-unit hits.** Each tick the strength a side lost, as a share of what marched in, is applied
to the men actually standing: `distribute_losses` hands out floor shares, then the remainder one
man at a time in ascending unit id — integer arithmetic throughout. **Conservation is exact**: men
removed equal the losses the pass reports, or the pass would quietly mint or destroy men. Losses
land per round batch, so a battle weakens units while it runs, which is what makes a mid-fight
withdrawal a real decision.

**Withdrawal** is `request_withdraw(w, corp, province, against)`. `against` names the fight, because
a corp can be in more than one battle in one province; `null_entity` means "the first, in sorted
order". The request is **honoured at the tick boundary**, before the next round batch — that delay
*is* the withdrawal window. Strength is captured before the withdrawal is applied, so the parting
cost reaches the men and not only a number. A request naming no live battle, or from a
non-participant, returns false and mutates nothing — the seam rule for an AI-facing boundary.

**The field.** A concluded battle names `field_held_by` — the side that stayed — and is erased at
the end of the tick. What holding a province *does* is owned by BL-567 (province is the conquest
unit). Battle state survives a save under BL-536 (world snapshot save).

### Dispatches and the card

A battle **reports**. `battle_tick::dispatches` carries one pure-data `battle_dispatch` per battle
that acted this tick — province, sides, phase, rounds fought / this tick / max, both strengths and
swings, men lost, and for an ending the `withdrew`, `end` and `field_held_by` fields. It includes
battles that **concluded** this tick and are already erased from `world::battles`; that is the only
way the aftermath — who held the field, what it cost — can reach a surface at all.

**`battle_phase`** is derived **once, in the world layer** (`read_battle_phase`), because two
surfaces consume it and they must never disagree. Rounds are mechanically uniform; the phase is a
*reading*: `opening` (the first batch), `committed` (after it — a statement about the withdrawal
price, since by round 4 the per-round term alone has tripled), `crisis` (a swing of 150 or more
across the batch, either side), `wavering` (a side within a quarter above `rout_threshold`, read
against the parameter so the word stays true if it is tuned), `broke_off`, `concluded`. Later
readings are the more urgent ones and win.

**The Field-channel dispatch** (BL-468, battle dispatches). `src/core/battle_dispatch_text.cpp`
turns a record into a line by folding the record's `stream_seed` with the round index — the BL-290
tongue-bank idiom — so wording varies between fights, replays identically, and **consumes no
draw**. The text is a pure function of the record plus the battle identity, which is what makes the
dispatch layer structurally unable to move the simulation. Every phase has a bank and no bank has a
hole. See [`docs/ui/LAYOUT.md`](../ui/LAYOUT.md) § Comms dock.

**The battle card** (BL-469, battle card). The Selection element has a **battle kind**, dispatched
ahead of province and kind resolution, with per-unit strength bars for both sides and the
withdrawal price shown as its three separate terms. The quote comes from `quote_withdrawal()` —
the resolver's own arithmetic, never recomputed in the card, because a player shown one number and
charged another has been lied to about the only decision the fight contains. A crossed-blades
marker sits on the province anchor tile. See [`docs/ui/SELECTION.md`](../ui/SELECTION.md) § The
battle element.

---

## Nation garrisons

> A contract offer is a want the client nation cannot meet alone; the thing the hired company
> actually fights is the OPPOSING nation's own force. This is that force.

Nations own units. `unit_component::owner` accepts a nation entity exactly as it accepts a corp's —
the field was never corp-typed. A garrison is seeded at generation, not hired: one in the nation's
capital province, plus one in each border province it shares with its highest-grudge neighbour
(`nation_score_terms.mean_grudge`). This joins the existing three unit writers (§ The unit model)
as a fourth, generation-time source, alongside the hard-coded stub, `seed_starting_military`, and
`hire_unit`.

**Sizing is treasury-scaled against the value anchor.** BL-543's (value anchor) unit-equipment-
costs-twice-annual-salary anchor prices a garrison's count off the nation's own
`nation_component::treasury`, so a rich nation fields a bigger standing force and a poor one a
token one — no separate garrison-strength dial. Garrisons are **static** in this slice: they hold
their seeded tile and do not march. A nation that wants ground *hires* — that is the whole point of
the contract, and a garrison that marched would make the nation its own mercenary.

**The trigger.** `run_battles` gains a second trigger, alongside corp-vs-corp directed hostility
(§ Battles, above): a unit owned by a corp holding an ACTIVE mercenary contract, standing in the
contract's target province, engages that province's garrison, owned by the contract's client
nation. No `declare_hostile` row is authored for this pair — **the contract itself is the
hostility**, live only for the contract's term. The pair's stance is a row keyed on the contract id,
cleared at the contract's terminal state (completed, failed or cancelled), never by a corp verb.

**Upkeep is a budget claim, not the corp vector.** A garrison does not draw the credits+goods
vector `run_unit_upkeep` charges corp units (§ Upkeep, below) — its upkeep is a `military_research`
line claim in the nation's own budget pass ([`../politics/NATIONS.md`](../politics/NATIONS.md) § A
budget), paid the same way every other priority line is. A garrison whose nation cannot fund the
line weakens by the ordinary budget-shortfall reading; there is no second mechanism.

*Owner: BL-571 (nation garrisons).*

---

## Upkeep

What it costs to **keep** a unit, as against hire, which is what it costs to raise one. Without
it a regiment is debited once and free forever while every building beside it pays maintenance
and wages every tick.

**Upkeep is credits AND military goods** (Ben, 2026-08-17), so the cost is a **vector**: a credit
wage plus a set of `{resource, qty}` draws against the corp's pool on the unit's own body. When the
goods do not arrive the unit **weakens** rather than vanishing.

The authored table, `scripts/economy.lua` § `economy.military.unit_upkeep`:

| Rate | Authored | Meaning |
|---|---|---|
| `credits_per_head` | `0.0` | Flat wage per head per tick |
| `credits_per_head_per_power` | `0.0` | Wage scaled by the roster row's `power_mod` |
| `goods_per_head.ordnance` | `0.0` | The **ordnance** draw |
| `goods_per_head.food_rations` | `0.0` | The sanctioned second line |
| `supply_decay_permille` | `0` | The one decay subtraction, per tick |
| `supply_recovery_permille` | `0` | Regained per tick while supplied |
| `out_of_supply_reach` | `0.0` | Reach cost past which a unit is out of supply; ≤ 0 disables the trigger |

**Every rate is authored at zero, deliberately**, so the pricing is tuned by playtest against a
measured baseline rather than guessed. At zero nothing is charged, no pool is drawn, no draw can go
unmet, and no supply factor moves; turning upkeep on is editing this table, not a code change.
The numbers are anchored by BL-543 (value anchor) — a unit's annual equipment costs twice its
annual salary (Ben, 2026-08-22, [`NATIONS.md`](../politics/NATIONS.md)) — and the ordnance rate
rides battle consumption (BL-496, ordnance rate; NR-321). `tools/verify/unit_upkeep_rates.cpp`
is the measurement: it sweeps candidate rate sets and prints the wage bill against income, the
goods draw against production, and the supply curve.

**Two halves, two homes.** The credit half is its own term in `apply_budget`
(`src/world/budget_system.cpp`, surfacing as `corp_budget::upkeep`). The goods half is a pool
debit in `run_unit_upkeep` (`src/world/economy_system.cpp`), drawn from the unit's `(owner, body)`
stockpile.

**Ordnance is the good, and it is authored as data.** `resource_type::ordnance` is the roster's
first terminal military good (BL-457, ordnance), added so this draw has something to consume.
Naming a good here is a one-line `economy.lua` change, never a code change.

> **The ordnance recipe is industrial.** The Fabricator recipe that makes ordnance is tagged
> `era = "industrial"` (`scripts/recipes.lua` id 27), and an ancient campaign
> (`era_band_for_epoch`, `src/world/recipe_registry.hpp`) masks it out of the allowed recipes. So
> an ancient-epoch campaign cannot produce the good its upkeep draws; the draw goes unmet and the
> decay rule fires. That is the rule working as written, and a fact to price against when the
> rates are set.

**One decay rule, two triggers.** A unit is unsupplied if it sits beyond the reach field *or* its
goods draw went unmet. Both fire the **same** subtraction on `supply_factor_permille`, because both
mean the same thing — an army that is not being supplied gets weaker. It is deliberately not two
rules.

The reach trigger is **opt-in**: a non-positive `out_of_supply_reach` disables it *and* the
Dijkstra behind it, so a zero table costs no time as well as no credits. The test is
`!(rc <= limit)` rather than `rc > limit`, so infinity and NaN both read as out of supply.

**Determinism.** The pass visits units in **ascending entity id**, sorted out of the unordered
`w.units`, because two units of one corp on one body draw from a shared pool — the visit order
decides which one goes short. That is load-bearing, not cosmetic.

**Orphan cleanup.** `demolish_building` erases the building, the corp asset and the stockpile but
never touches `w.units`. The upkeep pass disbands a unit whose `owner`, tile, or `muster_base` has
been erased, and a unit a battle reduced to zero. It runs **last** in the economy step, so a base
torn down this tick orphans its units in the same tick rather than leaving them live for one. It
reports the count as `unit_upkeep_tick::disbanded`.

`muster_base == null_entity` means "no base recorded" — the hard-coded world's stub unit — and is
never orphaned.

**Supply is measurably real in the resolver, not merely expensive.** `unit_strength` and
`unit_to_stack_entry` both read `supply_factor_permille`, so an unsupplied army is weaker where it
counts.

---

## One reach field — economic reach *is* military reach

This is the most load-bearing military decision. It is BL-325's (military bases and supply)
ruling 3, Ben's own words on 2026-08-08: *"a nation's reach for economy is also the military
reach."*

> **The network itself is owned by [`../economy/LOGISTICS.md`](../economy/LOGISTICS.md)** —
> traversal cost, the reach field, roads, physical scale and travel time, and the Logistic Points
> layer. This section owns the *military reading* of that one ruling.

**There is deliberately no second reach field.** The proposed parallel base-anchored supply
envelope was overridden. `body_reach_field` — the economic logistics network — *is* the military
supply envelope.

Its anchors are cities, and built active ports and inland logistics hubs
(`is_supply_anchor`, `src/world/logistics.hpp`). **A `military_base` is not an anchor and extends
nothing**, which is why it earns no exemption from the reach rule that governs its own placement.

**To project force further, you extend the same road and hub network everyone else uses.** There is
one distance metric in the game and armies pay it: a march spends its points against the same
traversal cost a convoy does, and a unit beyond the envelope takes the upkeep pass's decay
(§ Upkeep — `run_unit_upkeep` calls `body_reach_field` and `tile_reach_cost`, and a unit whose tile
cost exceeds `out_of_supply_reach`, or is unreachable outright, loses supply each tick). This is
verified rather than merely asserted: `buildings_rework_harness` R6/R7 assert that a completed base
is not a supply anchor, and that building one changes nothing in the reach field.

The ruling's reason is legibility over local cleverness. A second field would mean two answers to
"how far can I operate", diverging quietly, and a player learning the road network twice.

Its corollary is interdiction (BL-458, supply lines cannot be cut): because force and goods share
one road, a hostile unit on a lane cuts it for both. Hostility gates **three** things — interdiction
(`supply_system.cpp`), battle engagement (`battle_system.cpp`) and the march queue
(`economy_system.cpp`, NR-344). What goods-versus-force priority on a contested lane means is the
Logistic Points question, and LOGISTICS.md owns it.

---

## Verification

Headless harnesses under `tools/verify/`, run with the `verifier-headless` skill.

**`combat_harness.cpp`** — `resolve_battle`. R1: a unit carries a type and armies are
`(type, count)` stacks. R2: determinism on identical inputs, a hand-picked matchup landing on the
expected side, the exact-tie defender tie-break, and losses bounded as fractions. R3: doctrine is
pure modifier data — the same phalanx wins on open ground and loses in mountain. R4: naval
contributes zero power without crashing or branching, and the harness compiles against
`combat.hpp` + `components.hpp` alone.

**`campaign_battle_harness.cpp`** — `resolve_campaign_battle`. C1: replay determinism including the
per-round trace, stepping matching the scripted wrapper, and the seed genuinely folding from the
identity (a different tick, and a role swap, both give different streams). C2: the randomness
reaches the outcome and a near-even fight is uncertain. C3: withdrawing later usually costs more,
and a later exit only ever gets cheaper because the deficit narrowed. C4: a strength sweep that is
monotone in force, with an uncertain band and a near-unloseable top. C5: the two resolvers never
disagree on a lopsided fight. C4's rate threshold is written as a **prediction**, not a
tuned-to-pass number.

**`battle_engagement_harness.cpp`** — `run_battles`. B1: the trigger fires from hostility plus a
shared province alone, men die through the tick rather than a direct resolver call, and two
neutral corps sharing a province do not fight. B2: twin worlds produce the same battle set in the
same order. B3: mutual hostility and four units open exactly one battle, and a second tick opens no
duplicate. B4: province and role order each change the stream. B5: no count goes negative and
conservation is exact, per tick and end to end. B6: five ticks twice gives identical strengths and
an identical `state_hash`. B7: the battle anchors on the defender's best tile, ties on ascending
tile id. B8: a bogus or non-participant withdraw mutates nothing; a participant's request is
honoured on the following tick. B9: a unit in contact reads as in-battle and `march_unit` refuses
it. B10–B11: a battle that fought emits a dispatch naming the fight, rounds/max and the stream
seed; a concluded dispatch names who held the field after the record is gone. B12: the three
withdrawal terms sum to the total and **the quote matches the charge**. B13: twin worlds emit
byte-identical dispatch sequences and stay identical. B14: every phase produces a line.

**`military_capability_harness.cpp`** — the capability accumulator (BL-332). A completed research
institute credits `science` on a *non-player* corp (the symmetry check), and neither a base under
construction nor a decommissioned institute accumulates anything.

**`unit_upkeep.cpp`** — the upkeep pass and derived strength. U1: the credit charge is exactly N ×
the type's rate and nothing else in the budget moves; the goods debit is exactly N × each authored
good. U2: an empty pool decays at the authored scalar and stock never goes negative. U3: demolishing
the muster base leaves no orphan unit. U4: replay determinism across balances, pools and strengths.
U5: at the zero table nothing moves at all — no balance, no pool, no supply factor, no pool even
created — which is what lets the economy goldens stand while the rates are unauthored. U6:
strength derivation, including `strength == count * 100` at quality 1.0 and full supply. U7: the
combat adapter round-trips a live unit into an `army_stack_entry` the resolver accepts.

**`unit_upkeep_rates.cpp`** — a report, not a gate: the rate sweep described in § Upkeep. Its four
assertions are invariants — the authored rates are zero, a zero rate costs nothing, the draw is
linear in heads, and decay is bounded.

**`unit_march_harness.cpp`** — the march seam and pass. M1: `march_unit` sets a reachable path and
rejects not-owner/invalid/ocean-dest/off-body/already-there without mutating the world. M2:
`run_unit_march` spends per-class march points against the shared traversal-cost weight, carries
fractional progress across ticks, and clears the order on arrival. M3: `halt_unit` clears the order
and rejects a unit with none; `disband_unit` erases the unit and rejects a non-owned/unknown one.
M4: NR-344 — a mobilised corp's units are visited before a peaceful corp's within one tick's pass.
M5: state-hash replay determinism with marching units in flight. M6: a corrupted path (the
recompute guard's trigger) recomputes identically across two independent runs.

**`rival_military_seeding_harness.cpp`** — `seed_starting_military`. R1: every non-background corp
ends generation owning exactly one `military_base` and one unit (roster index 0, 50 manpower) on
the nearest valid land tile to its HQ, and a background firm owns neither. R2: two same-seed runs
seed identically.

**`stance_determinism.cpp`** — the stance tables that gate all of the above replay identically.

Adjacent coverage: `buildings_rework_harness` R6/R7 for `military_base` placement, zero staffing
and the not-an-anchor property.
