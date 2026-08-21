# Project Io — Military

The military layer is **parts, not a system**. Two battle resolvers, a unit roster, a muster
building, a hire verb, a terrain model, a per-tick upkeep pass and a roster→combat adapter all
ship compiled in the binary. Corp stance (hostility/friendship, BL-448/BL-449) also ships now, with
a surface, and a unit can now march, halt and disband (BL-470, 2026-08-19) — but what still does not
ship is anything that makes force meet stance — **until BL-467 (2026-08-21), which built the
engagement trigger and battle state; what remains absent is listed below.** Formerly: no trigger, and nothing in production
that calls the adapter.

This document is the authority for what is built, written 2026-08-17 against the source. It exists
because the absence of one cost real work: three sprint proposals in a row described the campaign
battle resolver as unbuilt while `src/world/campaign_battle.cpp` sat compiled with zero callers.
Nobody had a doc to check.

Read § Build status first if you only want the landed/outstanding split. Read § What is absent
before assuming any capability here is reachable in play.

> **CORRECTED 2026-08-18 against the source.** This document was written at commit `6153278` and
> the commit that landed Sprint 25a's military work — `0936400`, BL-454 (unit upkeep) and BL-459
> (unit strength is a duplicate of count) — **did not touch it**. It updated `FINANCE.md` instead.
> Three sections were stale within a day of being written: the unit model's field table, the
> upkeep and decay entries in § What is absent, and § Build status.
>
> **THE ONE THING TO TAKE FROM THIS PASS: upkeep ships at RATE ZERO.** An army costs nothing to
> keep today. The pass runs, the plumbing is real, and every authored rate in `scripts/economy.lua`
> § `economy.military.unit_upkeep` is literally `0.0`. Do not read anything below as saying force
> has a running cost — see § Upkeep, at rate zero.

---

## The two resolvers

There are **two** battle resolvers, and the split is deliberate. Ben ruled it 2026-08-13,
overturning BL-315's (armed house conflict spine) own earlier engine-parity strand.

They answer different questions. `resolve_battle` answers "region beats region, this year" in
one scored evaluation, because the Era −1 sim runs millions of those. `resolve_campaign_battle`
answers "two forces on a tile, over a short span, with a player who may pull out".

**What is shared is the calibration, not the resolver.** The roster is single-sourced, and every
campaign round scores its powers by calling `resolve_battle` and reading the two power numbers
out. The campaign path adds round structure, a seeded swing and withdrawal on top.

### `resolve_battle` — nation scale, history-sim called

`src/world/combat.{hpp,cpp}` (BL-272, unit/doctrine combat model). A pure function of its inputs:
matchup × doctrine × terrain × supply × season. No RNG, no world reads, no hidden state.

**Its only production caller is `src/world/history_sim.cpp`** — the Era −1 history sim (BL-271,
Era −1 sandbox). The campaign layer does not call it directly; `campaign_battle.cpp` calls it per
round, and `tools/verify/combat_harness.cpp` calls it to assert.

Arithmetic is **integers in per-mille throughout** (1000 = neutral). Battle outcomes move borders
in the sandbox, so no float decides who wins.

**Inputs.** Two `std::vector<army_stack_entry>`, two `doctrine_row`s, a terrain triple
(`terrain_substrate` + `terrain_cover` + `cover_density`, since BL-519) plus a
`terrain_landform`, a `season`, and two supply values 0..1000 (clamped). Nothing is rejected —
an empty or all-naval stack resolves rather than erroring.

> **The degenerate case that follows from "nothing is rejected", and what guards it.** Naval
> entries score EXACTLY zero, and the victory test is a strict `>`. So a fight where BOTH sides
> are empty or all-naval resolves as a **defender victory with 400/200 per-mille losses** —
> casualties inflicted on forces that scored no power at all, in a shape indistinguishable from
> a real outcome. BL-467's discovery opens a battle on stance and position alone and never
> inspects unit class, so nothing downstream would have caught it; `battle_system.cpp`'s
> `stack_can_fight` screens both stacks before opening. Unreachable with today's land-only
> production roster, and guarded rather than left to be found when the first naval row lands.

An **`army_stack_entry`** is one unit type's contribution, already resolved to numbers:
`{type_id, cls, count, type_power_mod}`. It is deliberately **not** a lookup key into a roster
table — the engine scores whatever stack it is handed and does not know which era it is refereeing.
`type_id` is carried for the caller's bookkeeping and never interpreted.

**Unit classes** are five and coarse: infantry, cavalry, ranged, siege, naval. Base power per unit
is 100 / 130 / 90 / 150 / 0.

**Naval is strategic-only in this cut.** A naval entry contributes zero power *and* zero weight to
the matchup average. Tactical naval resolution is explicitly deferred.

The **class matchup matrix** is a rock-paper-scissors core: infantry beats ranged, ranged beats
cavalry, cavalry beats infantry. Siege is uniformly weak in the open field, because this cut has no
fortification to give it its real job.

Both sides are looked up — `matchup(attacker, defender)` for the attacker's own power,
`matchup(defender, attacker)` for the defender's. Each is a count-weighted average over the
opposing composition, with a single division at the end.

A **`doctrine_row`** is pure modifier data: `frontal_bonus`, `flank_fragility`, `mountain_penalty`,
`stance`. Adding a doctrine is adding a value of this struct; it never touches `resolve_battle`.

`flank_fragility` is modelled as an intrinsic weakness of the formation, not conditioned on the
opponent actually flanking — a stated first-cut simplification. `mountain_penalty` fires only on
`terrain_landform::mountain`.

`siege_stance` (`field` / `assault` / `invest`) is carried on the row so a siege is a doctrine
choice rather than a separate code path. **`resolve_battle` does not currently read it** — the
field is declared and unconsumed.

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

### `resolve_campaign_battle` — campaign scale, **uncalled**

`src/world/campaign_battle.{hpp,cpp}` (BL-315, armed house conflict spine). Campaign-scale: two
forces on a tile, fought over a bounded number of rounds, with priced withdrawal.

**Its only caller is `tools/verify/campaign_battle_harness.cpp`.** Nothing in the game reaches it —
no UI, no command verb, no AI scorer, no economy tick. It is compiled, asserted and unreachable.

**The randomness is real to the player and absent to the engine.** Every draw comes from a
splitmix64 stream folded from the battle's own identity, consumed in a fixed order. The same save
replays the same battle; the player, who cannot see the seed, faces a genuinely uncertain fight.

The **`campaign_battle_identity`** folded into that seed is
`{attacker, defender, tile_index, tick, world_seed}`. The attacker/defender pair is packed in role
order, not sorted — swapping the roles is a different battle and must be a different stream.

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

Attrition is applied **multiplicatively** to remaining strength, so a long fight compounds rather
than sums past zero.

`swing_permille` was widened from 300 to 600 (BL-400, NR-204). At 300 a 1.4:1 attacker won 99% of
fights, which Ben ruled too safe. Measured at 600 over 1000 seeds per ratio: 1.0:1 → 50%,
1.2:1 → 76%, 1.4:1 → 89%, 2.0:1 → 99.8%.

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
tile's two-axis character. No stored field, no component change, nothing on the serialisation seam.

They exist because the predecessor — an `is_barrier` bool — conflated defensible ground,
attritional ground and open water. A mountain and a barren plain scored identically while a forest
scored nothing.

**`terrain_defence(comp, lf)` — how well ground holds, 0..1000.** Landform dominates; composition
modulates for cover and going.

| Landform | Defence | | Composition | Defence |
|---|---|---|---|---|
| mountain | 1000 | | wetland | 250 |
| rift | 700 | | forest | 200 |
| canyon | 650 | | volcanic | 150 |
| highland | 400 | | rocky / icy | 100 |
| crater | 300 | | tundra | 50 |
| valley | 150 | | grassland / barren / regolith / metallic | 0 |
| plains | 0 | | ocean | 0 |

Wetland is the composition standout, on the reasoning that marsh has stopped more armies than most
mountains — impassable to weight without being high at all.

**`terrain_attrition(comp, lf)` — what crossing costs an army in supply, 0..1000**, independently
of any defender. Composition dominates here: this is forage, water and shelter.

| Composition | Attrition | | Landform | Adds |
|---|---|---|---|---|
| regolith | 900 | | mountain | 200 |
| icy | 850 | | rift | 120 |
| barren / metallic | 800 | | canyon | 100 |
| volcanic | 700 | | crater | 80 |
| tundra | 500 | | highland | 60 |
| rocky | 400 | | valley / plains | 0 |
| wetland | 350 | | | |
| forest | 250 | | | |
| grassland | 100 | | | |
| ocean | 0 | | | |

**Ocean returns 0 from both.** Water is a movement *mode*, not a magnitude, so the caller must
branch on it rather than read a value — the same way logistics already branches land/sea.

**`terrain_resistance(comp, lf)`** combines them 60/40 toward defence. It is the graded replacement
for the old barrier bool, and **neither resolver reads it** — its consumers are
`src/world/history_ladder.cpp` (pricing conquest at generation time) and
`tools/verify/world_audit.cpp`.

---

## The unit model

A unit is a **group token**, not a stat block. `unit_component` in `src/world/components.hpp`
carries six fields (**table corrected 2026-08-18** — BL-459 removed `strength`, BL-454 added the
last two):

| Field | Type | Notes |
|---|---|---|
| `position` | `entity_id` | **The tile the unit occupies.** Tile-canonical — see below. |
| `owner` | `entity_id` | Corporation or faction entity that controls it. |
| `count` | `int` | Number of units in the group. |
| `type` | `uint16_t` | Opaque index into the roster table. |
| `supply_factor_permille` | `int32_t` | How well supplied, 1000 = full. Moved only by the upkeep pass's one decay rule (BL-454, unit upkeep). |
| `muster_base` | `entity_id` | The `military_base` this unit was raised at, or `null_entity`. The **orphan key** — see § Upkeep, at rate zero. |
| `order` | `movement_order` | BL-470, unit march seam, landed 2026-08-19. `{dest, path, next_index, progress}`; `dest == null_entity` means halted/unordered. See § Marching below. |

**There is no stored `strength`.** BL-459 (unit strength is a duplicate of count) removed the
field and made strength derived — `unit_strength(w, u)` in `src/world/unit_roster.hpp` returns
`count × roster_type_quality × supply_factor` at ×100 fixed point. It returns an **integer** on
purpose, so a sum over the unordered `w.units` stays order-independent.

**Position is tile-canonical.** BL-157 (military datamodel stub) ruled tile position canonical over
body/region on 2026-08-07; the struct shipped with a `body` field ahead of that ruling, and
BL-324 (unit hire surface) replaced it with `position` on 2026-08-08.

Units live in `world::units`, an `unordered_map<entity_id, unit_component>`. Every consumer that
aggregates over it does so order-independently (integer sums, owner filters), which is what keeps
the container's iteration order off the determinism seam.

**Three writers exist**, and no other code creates a unit:

- `src/world/hard_coded_world.cpp` — the Kepler player stub, count 50, type left at 0. **No `muster_base`** (that world has none), so it is deliberately never orphaned.
- `src/world/corporation_generation.cpp` — every non-background corp's starting unit beside its starting base (BL-331, starting military presence; extended from player-only to every named rival by BL-476, 2026-08-19 — RIVALS_START_UNARMED), count 50, roster row 0. Records that base as its `muster_base`. BL-365 background firms are excluded on purpose.
- `src/world/corp_command.cpp` — the `hire_unit` verb, count `hire_batch_manpower` = 50. Records the tile's qualifying base, lowest entity id first.

**The bridge now exists — and nothing in production crosses it (corrected 2026-08-18).** This
section used to say no code turns a `unit_component` into an `army_stack_entry`. BL-459 added
`unit_to_stack_entry(w, u)` (`src/world/unit_roster.hpp`), the adapter that resolves a live unit
into the value both resolvers accept: supply is carried in the entry's `count`, quality in its
`type_power_mod` — deliberately not both in one place, since doubling quality into count would
square it against `unit_strength`.

**Its only caller is `tools/verify/unit_upkeep.cpp` (check U7).** So the accurate statement is now
narrower and worse: the campaign has units, resolvers *and* a bridge, and still nothing decides
that a fight happens. `roster_stack` — the other composition path, from *region manpower* — is
still Era −1's alone.

**What reads a unit today.** Two condition subjects — `military_units` (summed counts) and
`military_strength` (summed derived strengths) — in `src/world/condition_set.cpp`, available to
laws, techs and quests. The Selection element's read-only unit card (Strength / Roster pages) and
the hover summary in `src/ui/entity_summary.cpp`, both of which now print the derived strength plus
its `count × quality × supply` derivation. The corp AI, counting its own units against a soft cap
of three. The upkeep pass and the budget's `upkeep` term. That is the whole list.

**A unit is not drawn on the canvas.** The old stroke-only unit chevron was deleted uncalled
(BL-294) and no marker replaced it. A unit is reached by clicking its tile: the Planetary canvas
cycles unit → building → tile on repeat clicks of the same tile.

---

## The muster interface

`building_type::military_base = 6` is the single economy → military interface. It is where units
come from, and the only place they come from.

**The building** (BL-325 S1, landed 2026-08-08). A tile-occupying installation on any non-ocean
land tile, with no deposit requirement. It produces nothing and staffs at zero, like the port and
the logistics hub.

It obeys the ordinary reach rule at the world level and earns **no anchor exemption** — see
§ One reach field. Its glyph is the filled shield in `icons::building`; `scripts/economy.lua`
prices it at build cost 300, 35 steel, 4 ticks, maintenance 15.

**It is gated behind a technology, not a build menu.** `E0-ML-01` "Standing Garrison Doctrine"
(`src/world/tech_gate.cpp`) unlocks it on two extraction sites plus a Cr 2,000 balance. A military
*subject* is deliberately not used in that predicate — gating the first military building on
already having a military would be circular.

**Hire moves onto the base** (BL-325 S2, landed 2026-08-13; supersedes BL-324's hire-anywhere).
`corp_verb::hire_unit` takes `{tile, unit_type}` and checks, in order:

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

The credit floor exists because of BL-394 (hire is free on the seam): row 0's gate is all-zero, so
the resource leg alone tested and spent nothing, and the cheapest rows were free and unbounded for
any seam caller. Both legs are checked before either is debited, so a refusal leaves the corp
wholly uncharged.

On success: a new unit entity at the muster tile with `count = hire_batch_manpower`, returned
through `apply_corp_command`'s out-param. *(Corrected 2026-08-18: this line also said `strength`
was set — BL-459 removed that field.)* The verb records the base it was raised at as the unit's
`muster_base`; where the tile carries more than one qualifying base, the **lowest entity id**
wins, so the choice is deterministic.

The player's press then selects the new unit; the rival scorer's path additionally reports an
`agency_event::kind::hired` for the decision feed and session history.

**Rivals hire through the same verb** (BL-324's widening of the AI-agency exception, standing
rules § Determinism & data model). `src/world/corp_ai.cpp` scores hiring alongside build, demolish,
survey and road; a corp with no completed base has no muster tile and must build one first, priced
by the ordinary build-candidate machinery. A soft cap of three units per corp keeps hiring from
becoming an unconditional extra action every evaluation.

### The roster

`src/world/unit_roster.{hpp,cpp}` (BL-274, era-keyed unit rosters). An authored **table**, not a
tree: availability is derived from ground and era band, with no research, no player choice and no
unlock events.

**Nineteen** rows across four cumulative bands — `classical` 7, `medieval` 4, `gunpowder` 4,
`industrial` 4. (Counted from `g_table` in `src/world/unit_roster.cpp` on 2026-08-18; this
document said "eighteen", which was wrong on the day it was written.) Bands are cumulative because
nothing un-invents a spear: an industrial polity still fields levies where its ground cannot pay
for better.

Each row carries `{name, band, cls, gate, power_mod, weight}`. The `roster_gate` is four thresholds
on 0..1000 endowment axes: `ore_q`, `farm_q`, `port_q`, `energy_q`.

Two of those axes are **named substitutions** — the convention is to name the stand-in, never fake
the input. `farm_q` proxies for pasture, because horses have no signal of their own. `energy_q`
proxies for saltpetre, then genuinely for fuel from the industrial band.

**The file placement deviates from BL-274's own file list**, deliberately. BL-274 named
`combat.{hpp,cpp}` as the roster's home, but `combat.hpp` states the opposite boundary in its own
words. So the roster lives separately and *resolves into* combat's types.

**Two gate paths, one table.** `available_rows(const region&, band)` is the Era −1 path, gated on
authored region endowment. `available_rows(const world&, corp, band)` is the campaign path
(BL-324), gated on the corp's own stockpile and market access.

The campaign gate is **binary by design** — each axis is 1000 or 0, the table's own max threshold.
"You may field rifles because you can buy steel" is a yes/no supply-chain question here, not a
graduated dial; quantity-scaled readiness is a follow-on.

| Axis | Campaign source |
|---|---|
| `ore_q` | Holds any steel, iron ore or iron-nickel ore |
| `farm_q` | Holds any food rations or agricultural produce |
| `port_q` | Owns a `port` building anywhere |
| `energy_q` | Holds any coal, refined fuel or petroleum |

**`campaign_roster_band_for(era_band)` derives the band from the campaign's own era (BL-461,
2026-08-19).** It is fixed neither at `industrial` nor at any other single value — that was the
BL-324 shape, and it went stale when the 0 CE refocus (NR-177, 2026-08-12) moved the default epoch
to 0 without moving this constant, letting a 0 CE company field industrial-era units. It is still
**not** derived from a military-capacity score — that is the Era −1 settlement model — but from
`recipe_registry::era()`, the same coarse ancient/industrial split `era_band_for_epoch` already
gives every other era gate: an ancient campaign opens at the roster's lowest band (`classical`), an
industrial one at its highest (`industrial`). Because bands are cumulative, every earlier row the
corp's ground supports is still exposed either way.

`roster_stack(manpower, region, band, readiness)` composes an army stack from region manpower.
Later bands crowd out earlier ones — a row's weight decays ×0.35 per band it sits below the
polity's own, so a rifle-era army is not half levy spears. Readiness folds into each entry's
`type_power_mod` as `(readiness − 1000) / 10`, which is the same channel cohesion and defensive
works use.

---

## Marching (BL-470, unit march seam, landed 2026-08-19)

A unit no longer stays pinned to its muster tile. Three `corp_verb` rows extend the **same**
corp-command seam every other action goes through — no parallel `unit_command` type (the item's own
ruling 2: BL-314's "future unit_command seam" phrasing resolves to this seam, extended):

- **`march_unit {subject: unit, tile: dest}`.** Sets/replaces the unit's `movement_order`. Rejects
  `rejected_not_owner` (not the caller's unit), `rejected_invalid` (unknown unit/tile, ocean dest,
  dest off the unit's own body, or unreachable), `rejected_state` (dest already `== position`). The
  in-battle rejection the design calls for is **currently unreachable** — there is no engagement
  trigger or battle-state field on `unit_component` yet (BL-467), so the check has nothing to test.
- **`halt_unit {subject: unit}`.** Clears the order. `rejected_state` on a unit with none.
- **`disband_unit {subject: unit}`.** Erases the unit outright. No refund — manpower walks away;
  a confirm step, if any, lives in the UI (BL-471), never in the seam.

**Movement is a path march, not a hop.** `march_unit` computes the path ONCE, via
`intra_body_path` (`src/world/logistics.hpp`) — the same terrain-weighted A* the convoy system and
`body_reach_field` already use — and stores it on `unit_component::order`. No per-tick Dijkstra:
`run_unit_march` (`src/world/economy_system.cpp`) only re-paths when a step is found blocked, which
cannot currently happen in play (terrain never changes post-generation) but is exercised directly by
the determinism harness as a defensive guard.

**March points are per-CLASS, authored data.** `economy.military.march_points_per_class`
(`scripts/economy.lua`), keyed by the roster row's `cls` (`unit_class`: infantry/cavalry/ranged/
siege/naval). Spent per tick against the per-tile traversal-cost weight
(`logistics::tile_traversal_cost`, the same plains=1.0/mountain=2.0 table road placement discounts),
with fractional remainder banked in `order.progress` across ticks. Shipped defaults: infantry 1.0,
cavalry 1.5, ranged 1.0, siege 0.5, naval 0.0 (no naval movement model exists — combat.hpp's own
"strategic-only presence" note). A composite unit (BL-472, formations, not this item) will read its
**slowest** component's class entry; the hook exists, the composite logic does not.

**Visit order — NR-344, "war flips the queue" (resolved alongside this item).** At peace, convoys
already claim the network first: `advance_convoys` runs in the sim loop *before*
`run_economy_step` is even called (`main.cpp`/`app.cpp`), which was a fact of phase order before
this item and is now a **stated rule**. What this item adds: a corp party to **any** declared
hostility, either direction (`is_hostile` checked both ways against `corp_hostile_pairs` —
"being attacked mobilises too"), has its own units visited **first** within `run_unit_march`,
evaluated once per tick from the stance table (BL-448). This changes **nothing observable today** —
there is no shared logistics-point pool for a march and a convoy to actually contend over until
BL-464 (Logistic Points) lands one — so it ships the way BL-454's upkeep did: real, written down,
inert until something exists to contend over.

**The blackboard export now shows a corp its own force** (BL-393's open half, taken over by this
item). `export_corp_blackboard` (`src/world/corp_ai.cpp`) emits `unit_tile`, `unit_type`,
`unit_count`, `unit_strength` and `unit_order_state`/`unit_order_dest` facts for the corp's OWN
units — no BL-068 visibility question, exactly like cash/buildings/pools. Rival units stay
ungated by this section (it has no rival counterpart); the rival scorer can now legally issue
`march_unit` (legality only — scoring march *quality* is BL-450's conflict-AI arc, not this item).

**Determinism.** `tools/verify/unit_march_harness.cpp` covers replay determinism with marching
units in flight and the blocked-path recompute path (see § Verification).

---

## Upkeep, at rate zero

Added 2026-08-18, describing BL-454 (unit upkeep) as it landed at commit `0936400`.

**Read this first: an army costs nothing to keep.** The pass is wired, harnessed and running every
economy tick, and **every authored rate is `0.0`** — `scripts/economy.lua`
§ `economy.military.unit_upkeep`. Nothing is charged, no pool is drawn, no draw can go unmet, and
no unit's supply factor ever moves. The item shipped **inert on purpose**, so no economy golden
moved on landing and the pricing can be tuned by playtest against a measured baseline.

The authored table, in full, as it ships:

| Rate | Value | What it would do |
|---|---|---|
| `credits_per_head` | `0.0` | Flat wage per head per tick |
| `credits_per_head_per_power` | `0.0` | Wage scaled by the roster row's `power_mod` |
| `goods_per_head.ordnance` | `0.0` | The **ordnance** draw (BL-457, ordnance) |
| `goods_per_head.food_rations` | `0.0` | The sanctioned second line |
| `supply_decay_permille` | `0` | The one decay subtraction |
| `supply_recovery_permille` | `0` | Regained per tick while supplied |
| `out_of_supply_reach` | `0.0` | Reach cost past which a unit is out of supply; ≤ 0 **disables the trigger** |

**Two halves, two homes.** The credit half is its own term in `apply_budget`
(`src/world/budget_system.cpp:173`, surfacing as `corp_budget::upkeep`). The goods half is a pool
debit in `run_unit_upkeep` (`src/world/economy_system.cpp:1468`), drawn from the unit's
`(owner, body)` stockpile.

**Ordnance is the good, and it is authored as data.** BL-457 (ordnance) added
`resource_type::ordnance` as the roster's first terminal military good precisely so this draw had
something to consume. Naming a good here is a one-line `economy.lua` change, never a code change.

> **The ordnance draw could not be met at the shipped epoch anyway (noted 2026-08-18).** The
> Fabricator recipe that makes ordnance is tagged `era = "industrial"` (`scripts/recipes.lua`
> id 27), and the default 0 CE campaign resolves to `era_band::ancient`
> (`era_band_for_epoch`, `src/world/recipe_registry.hpp:36`), which masks it out of the allowed
> roster. So even at a non-zero rate, an ancient campaign has no way to produce the good its
> upkeep would draw. The same epoch mismatch as `campaign_roster_band`, one layer down.

**One decay rule, two triggers.** A unit is unsupplied if it sits beyond the reach field *or* its
goods draw went unmet. Both fire the **same** subtraction on `supply_factor_permille`, because both
mean the same thing — an army that is not being supplied gets weaker. It is deliberately not two
rules.

The reach trigger is **opt-in**: a non-positive `out_of_supply_reach` disables it *and* the
Dijkstra behind it, so the default rates cost no time as well as no credits.

**Determinism.** The pass visits units in **ascending entity id**, sorted out of the unordered
`w.units`, because two units of one corp on one body draw from a shared pool — the visit order
decides which one goes short. That is load-bearing, not cosmetic.

**Orphan cleanup — the demolish gap, closed.** `demolish_building` erases the building, the corp
asset and the stockpile but never touches `w.units`, so demolishing a muster base used to leave its
units standing on a tile whose base was gone. The upkeep pass disbands a unit whose `owner`, tile,
or `muster_base` has been erased. It runs **last** in the economy step, so a base torn down this
tick orphans its units in the same tick rather than leaving them live for one.

`muster_base == null_entity` means "no base recorded" — the hard-coded world's stub unit — and is
never orphaned.

**Supply is measurably real in the resolver, not merely expensive.** `unit_strength` and
`unit_to_stack_entry` both read `supply_factor_permille`, so an unsupplied army is weaker where it
counts. At rate zero no unit ever becomes unsupplied, so this path is untravelled in play and
exercised only by the harness.

---

## One reach field — economic reach *is* military reach

This is the most load-bearing military decision taken so far. It is BL-325's (military bases and
supply) ruling 3, Ben's own words on 2026-08-08: *"a nation's reach for economy is also the
military reach."*

**There is deliberately no second reach field.** The proposed parallel base-anchored supply
envelope was overridden. `body_reach_field` — the existing economic logistics network — *is* the
military supply envelope.

Its anchors are cities, and built active ports and inland logistics hubs
(`is_supply_anchor`, `src/world/logistics.hpp`). **A `military_base` is not an anchor and extends
nothing**, which is why it earns no exemption from the reach rule that governs its own placement.

**To project force further, you extend the same road and hub network everyone else uses.** There is
one distance metric in the game and armies pay it. This is verified rather than merely asserted:
`buildings_rework_harness` R6/R7 assert that a completed base is not a supply anchor, and that
building one changes nothing in the reach field.

The ruling's reason is legibility over local cleverness. A second field would mean two answers to
"how far can I operate", diverging quietly, and a player learning the road network twice.

Its consequence — units beyond the envelope losing strength each tick — is BL-325's slice S3.

> **S3 is now BUILT, and OFF (corrected 2026-08-18).** This paragraph said "not built" and was
> written the day before BL-454 (unit upkeep) landed it. `run_unit_upkeep` calls `body_reach_field`
> and `tile_reach_cost`, and a unit whose tile cost exceeds `out_of_supply_reach` — or which is
> unreachable outright — takes the decay subtraction. `!(rc <= limit)` rather than `rc > limit`, so
> infinity and NaN both read as out of supply.
>
> **The authored limit is `0.0`, which disables the trigger entirely** (§ Upkeep, at rate zero), so
> no unit has ever gone out of supply in a shipped campaign. The rule exists; the number does not.
> The sequencing worry NR-177 recorded — do not land a decay rule for units nothing commands —
> was answered by landing it inert rather than by deferring it.

---

## Two inconsistencies — both now RESOLVED

Retitled 2026-08-18: this section was headed "Two live inconsistencies". Neither is live.

**1. `unit_component::strength` duplicated `count` — RESOLVED 2026-08-17, by derivation.** All
three writers used to set the stored field equal to raw manpower, so it carried no information
`count` did not, and both UI readers printed it raw.

BL-459 (unit strength is a duplicate of count) landed at commit `0936400`. The stored field is
**dropped** — so the duplicate cannot return — and strength is computed on read:
`count × roster_type_quality × supply_factor`, ×100 fixed point, integral throughout
(`unit_strength`, `src/world/unit_roster.hpp`). Scale anchor: at quality 1.0 and full supply,
`strength == count * 100` exactly.

Quality is **derived from the row's `power_mod`** rather than authored as a second number, so there
is one per-type quality figure and it cannot drift from the one combat already reads. Levy Spear
(power_mod 0) is 1000; Rifle Regiment (380) is 1380.

All three readers moved with it: the Selection strength page and the hover card now show the
derivation, and `condition_subject::military_strength` (`src/world/condition_set.cpp:138`) sums
`unit_strength` — an **integer** sum, which is what keeps the walk over the unordered `w.units`
order-independent.

**2. `corporation_component::military_points` — RESOLVED 2026-08-17, by deletion.** Every completed
`military_base` used to credit it a flat rate each economy tick, and nothing in `src/` ever read it.

BL-455 (military points and science are write-only) removed the field, its Lua rate, its parameter,
its write branch and every assertion about it. No consumer could be named: the economy→military
interface is already the base tile plus the reach field (§ One reach field), and the recurring cost
of force is already credits and ordnance (BL-454, unit upkeep — *shape* only; both rates are zero,
§ Upkeep, at rate zero).

**A military base now accumulates nothing.** It gates hiring and it occupies a tile; it does not
produce a currency. If the governing-body pivot (BL-094) later wants a military-capacity scalar,
it is a fresh field with its behaviour filed in the same change.

---

## What is absent, and known to be

This list is the reason this document cannot be read as describing a finished system. Everything
below is a real gap with a named owner.

- ~~**No hostility model.**~~ — *landed 2026-08-19 (BL-448, BL-449). `src/world/stance.{hpp,cpp}` gives every corp pair a directed hostility state and a symmetric friendship state (`corp_hostile_pairs`, `corp_friend_pairs`, a pending `corp_friend_offers` table), reached through four `corp_command` verbs (`declare_hostile`, `offer_friendship`, `accept_friendship`, `return_to_neutral`) and a Corporation panel Stance column, gated on ordinary BL-068 competitor-visibility (NR-350: a declaration stays silent, discovered on contact rather than announced). Landing the substrate is deliberate and still carries* **no consequence** *— stance gates nothing yet. What hostility permits is BL-315 (armed house conflict spine, still absent, see below); what friendship permits is a later call. No serialiser exists for it either (see `src/world/serialization.cpp` — the file does not exist anywhere in the repo; NR-349), so stance does not yet survive a save.*
- ~~**No unit-subject verbs.**~~ — *landed 2026-08-19 (BL-470, unit march seam). `march_unit`/`halt_unit`/`disband_unit` extend `corp_command`; a unit can now be marched (path, across ticks, on the shared traversal-cost metric), halted, or disbanded. Merge/split and garrison/scout stay absent — BL-472 (formations) owns them.*
- ~~**No engagement trigger.**~~ **BUILT 2026-08-21 (BL-467).** `src/world/battle_system.cpp` is
  both the trigger and the step: a battle opens when two corps' units stand in the same province
  and at least one holds a hostile stance toward the other, and it is stepped several rounds per
  tick from `run_economy_step`, immediately before `run_unit_march`. The record lives on
  `world::battles`, kept sorted by (province, attacker, defender). *This bullet stood through two
  amendments — 2026-08-18 noted the `unit_to_stack_entry` bridge existed and "nothing wants to"
  call the resolver. Something does now.*
- **No battle state in the world.** `campaign_battle_state` is a value a caller holds, not a world component. Nothing serialises a fight in progress, and nothing steps one across ticks.
- **No upkeep *rate*.** *(Rewritten 2026-08-18 — the old bullet said "No upkeep", which BL-454 falsified.)* The upkeep **pass** ships and runs every economy tick, both halves wired: credits through `apply_budget`, goods through `run_unit_upkeep`. What is absent is a **number** — every authored rate is `0.0`, so a standing force still costs credits once, at hire, and nothing thereafter. Turning it on is an `economy.lua` edit, not a code change. See § Upkeep, at rate zero.
- **No out-of-supply decay *in effect*.** *(Rewritten 2026-08-18 — the old bullet said the rule was unbuilt; BL-325 S3 landed with BL-454.)* The trigger, the reach lookup and the decay subtraction all ship. `out_of_supply_reach` is authored at `0.0`, which disables the trigger outright, so units outside the reach envelope still suffer nothing.
- **The campaign roster band is hard-coded to the wrong era.** `campaign_roster_band = roster_band::industrial` (`src/world/unit_roster.hpp:61`) is a compile-time constant with a comment still describing "the 1960s campaign", while the default epoch is **0 CE**. Nothing derives it from `epoch_year` or from `era_band`, so an ancient campaign offers industrial rows. A known defect with no owning item as of 2026-08-18 — see § The roster.
- **No fortification.** Siege is uniformly weak in the open field precisely because there is no held position to reduce; `siege_stance` is declared and unread.
- **No tactical naval.** Naval rows exist in the roster and naval entries are accepted by `resolve_battle`, contributing zero power and zero weight. Naval presence is strategic tagging only.
- **No unit marker on the canvas.** Units are selectable through the tile click-cycle and have no glyph of their own.
- ~~**No blackboard export of units.**~~ — *landed 2026-08-19 (BL-470's Rider 1, taking over BL-393's open half). `export_corp_blackboard` now emits `unit_tile`/`unit_type`/`unit_count`/`unit_strength`/`unit_order_state` facts for a corp's own force. Rival units still carry no unit-subject facts — no rider claims that gap.*
- ~~**No demolish interaction.**~~ — *resolved 2026-08-18 by BL-454 (unit upkeep). The upkeep pass disbands a unit whose owner, tile or `muster_base` has been erased, and runs last in the economy step so the cleanup happens in the same tick as the demolition. It reports the count as `unit_upkeep_tick::disbanded`.*

---

## Verification

**Five** headless harnesses cover this layer (a fifth, `unit_march_harness.cpp`, landed with BL-470
on 2026-08-19; a fourth landed with BL-454/BL-459 on 2026-08-17). Run them with the
`verifier-headless` skill.

**`tools/verify/combat_harness.cpp`** — `resolve_battle`. R1: a unit carries a type and armies are
`(type, count)` stacks. R2: determinism on identical inputs, a hand-picked matchup landing on the
expected side, the exact-tie defender tie-break, and losses bounded as fractions. R3: doctrine is
pure modifier data — the same phalanx wins on open ground and loses in mountain. R4: naval
contributes zero power without crashing or branching, and the harness compiles against
`combat.hpp` + `components.hpp` alone.

**`tools/verify/campaign_battle_harness.cpp`** — `resolve_campaign_battle`. C1: replay determinism
including the per-round trace, stepping matching the scripted wrapper, and the seed genuinely
folding from the identity (a different tick, and a role swap, both give different streams). C2: the
randomness reaches the outcome and a near-even fight is uncertain. C3: withdrawing later usually
costs more, and a later exit only ever gets cheaper because the deficit narrowed. C4: a strength
sweep that is monotone in force, with an uncertain band and a near-unloseable top. C5: the two
resolvers never disagree on a lopsided fight.

C4's rate threshold is written as a **prediction**, not a tuned-to-pass number.

**`tools/verify/military_capability_harness.cpp`** — the capability accumulators (BL-332). A
completed research institute
credits `science` on a *non-player* corp (the symmetry check), and neither a base under
construction nor a decommissioned institute accumulates anything.

**`tools/verify/unit_upkeep.cpp`** — the upkeep pass and derived strength (BL-454, unit upkeep;
BL-459, unit strength is a duplicate of count). U1: the credit charge is exactly N × the type's
rate and nothing else in the budget moves; the goods debit is exactly N × each authored good. U2:
an empty pool decays at the authored scalar and stock never goes negative. U3: demolishing the
muster base leaves no orphan unit. U4: replay determinism across balances, pools and strengths.
**U5: at the shipped (zero) rates nothing moves at all — no balance, no pool, no supply factor, no
pool even created.** U6: strength derivation, including `strength == count * 100` at quality 1.0
and full supply. U7: the combat adapter round-trips a live unit into an `army_stack_entry` the
resolver accepts.

U5 is the load-bearing row. It is what lets the economy goldens be re-blessed honestly, because it
asserts the item is inert until someone authors a rate.

**`tools/verify/unit_march_harness.cpp`** — the march seam and pass (BL-470). M1: `march_unit`
sets a reachable path and rejects not-owner/invalid/ocean-dest/off-body/already-there without
mutating the world. M2: `run_unit_march` spends per-class march points against the shared
traversal-cost weight, carries fractional progress across ticks, and clears the order on arrival.
M3: `halt_unit` clears the order and rejects a unit with none; `disband_unit` erases the unit and
rejects a non-owned/unknown one. M4: NR-344 — a mobilised corp's units are visited before a
peaceful corp's within one tick's pass. M5: state-hash replay determinism with marching units in
flight. M6: a corrupted path (the recompute guard's trigger, since terrain never actually blocks a
step in this engine) recomputes identically across two independent runs — no iteration-order
dependence in the recompute path.

Adjacent coverage: `buildings_rework_harness` R6/R7 for `military_base` placement, zero staffing
and the not-an-anchor property.

---

## Build status

Re-verified against the source 2026-08-19 (BL-476, RIVALS_START_UNARMED).

**Landed:**

- `resolve_battle` — the nation-scale resolver, consumed by the Era −1 history sim (BL-272, unit/doctrine combat model)
- `resolve_campaign_battle` and its begin/step/withdraw state machine — ~~no production caller~~ **called by `run_battles` since BL-467 (2026-08-21)**; the begin/step/withdraw machine now runs in the live economy tick (BL-315, armed house conflict spine)
- Terrain defence / attrition / resistance (BL-233, terrain combat modifiers)
- `unit_component` with tile-canonical position (BL-157, military datamodel stub; BL-324, unit hire surface)
- The **19**-row era-keyed roster and both gate paths (BL-274, era-keyed unit rosters; BL-352, hire gate live store)
- `military_base` as a building, tech-gated behind `E0-ML-01` (BL-325 S1; BL-344)
- `hire_unit` gated on a completed owned base at the tile, with a credit cost and a resource draw (BL-325 S2; BL-394, hire is free on the seam)
- Rival hiring through the same verb (BL-324's exception; BL-293's seam widening)
- **Every non-background corp's starting base and unit, not just the player's** (BL-331, starting military presence; BL-476, RIVALS_START_UNARMED — landed 2026-08-19). `seed_starting_military` runs once per player + named rival; BL-365 background firms stay unarmed on purpose
- **Unit upkeep — the pass, both halves, at rate zero** (BL-454, unit upkeep). Credits via `apply_budget`, goods via `run_unit_upkeep`. **Moved from Outstanding 2026-08-18**; landed at commit `0936400` on 2026-08-17. Every authored rate is `0.0` — see § Upkeep, at rate zero
- **Out-of-supply decay — the rule, with the trigger disabled** (BL-325 S3). **Moved from Outstanding 2026-08-18**; `out_of_supply_reach` is authored at `0.0`
- **Orphan cleanup** — demolishing a muster base disbands its units in the same tick (BL-454)
- **Derived unit strength** — `count × roster quality × supply factor`, the stored field removed (BL-459, unit strength is a duplicate of count)
- **`unit_to_stack_entry`** — the roster→combat adapter (BL-459). Harness-only caller
- **`resource_type::ordnance`** — the roster's first terminal military good, named as upkeep's draw (BL-457, ordnance)
- ~~Passive `military_points` accumulation (BL-332)~~ — **removed 2026-08-17** by BL-455 (military points and science are write-only); a base accumulates nothing
- **Corp stance / hostility, with a surface** (BL-448, BL-449). Directed hostility, symmetric friendship, the Corporation panel Stance column and its three presses — landed 2026-08-19, still inert (no consequence wired, no serialiser)
- **`campaign_roster_band` derives from the epoch** (BL-461). Was hard-coded to `industrial` against a 0 CE default; now `campaign_roster_band_for(era_band)` off `recipe_registry::era()` — landed 2026-08-19
- **Unit march seam — march/halt/disband, path-marching on the shared traversal-cost metric** (BL-470). Extends `corp_command`, no parallel type. Per-class march points (`economy.military.march_points_per_class`), fractional carry-over, blocked-step recompute. Riders: the blackboard units export (BL-393's open half) and NR-344 ("war flips the queue") written down in phase order — landed 2026-08-19
- **Battle dispatches and the battle card** (BL-468, BL-469 — landed 2026-08-21). A battle now
  *reports*: `battle_tick::dispatches` carries one pure-data record per battle that acted this
  tick, including battles that CONCLUDED this tick and are therefore already erased from
  `world::battles` — which is the only way the aftermath (who held the field, what it cost) can
  reach a surface at all. `battle_phase` is derived **once, in the world layer**
  (`read_battle_phase`), because two surfaces consume it and BL-468 requires they never disagree.
  `src/core/battle_dispatch_text.cpp` turns a record into a Field-channel line by folding
  `stream_seed` with the round index — the BL-290 tongue-bank idiom, so wording varies between
  fights, replays identically, and **consumes no draw**, which is what makes the dispatch layer
  structurally unable to move the simulation. The Selection element gains a **battle kind**
  (dispatched ahead of province and kind resolution), with per-unit strength bars both sides and
  the withdrawal price shown as its three separate terms, quoted from `quote_withdrawal()` — the
  resolver's own arithmetic, never recomputed in the card. A crossed-blades marker sits on the
  province anchor tile. See `docs/ui/SELECTION.md` § The battle element and `docs/ui/LAYOUT.md`
  § Comms dock.

**Outstanding:**

- Merge/split and garrison/scout unit verbs (BL-472, formations)
- ACTIONS.json transcription of the march/halt/disband verbs (BL-314, unit verb family — now *requires* BL-470)
- ~~Anything that **calls** the campaign resolver~~ — **BUILT (BL-467, 2026-08-21).** Both the
  engagement trigger and battle state in the world. What is STILL absent, and named so it is not
  read as finished: **doctrine is an all-zero stub** on both sides (no per-corp doctrine field
  exists, so the resolver's doctrine machinery contributes nothing to any production battle);
  **season is hardcoded to summer** (the world carries none, so the winter penalty is dead in
  production); **battle membership is snapshotted at open** (a unit marching in later does not
  reinforce); and **"the field is held by the side that stayed" has no consequence**, because no
  territorial-control concept exists for a battle to affect.
- **Authoring the upkeep numbers.** The pass is inert until someone sets a rate; that tuning has no owning item as of 2026-08-18
- Hire price on screen (BL-405, hire has no price on screen)
- The Era −1 sim's conquest failure — 267 battles, zero region transfers (BL-384, Era −1 sim conquers nothing)
- Fortification, tactical naval, and a unit canvas marker — no owning item yet
