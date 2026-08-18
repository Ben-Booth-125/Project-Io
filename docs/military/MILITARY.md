# Project Io — Military

The military layer is **parts, not a system**. Two battle resolvers, a unit roster, a muster
building, a hire verb and a terrain model all ship compiled in the binary. What does not ship is
anything that makes them meet: no hostility, no engagement trigger, no unit verbs, and no code
that turns a `unit_component` into an army.

This document is the authority for what is built, written 2026-08-17 against the source. It exists
because the absence of one cost real work: three sprint proposals in a row described the campaign
battle resolver as unbuilt while `src/world/campaign_battle.cpp` sat compiled with zero callers.
Nobody had a doc to check.

Read § Build status first if you only want the landed/outstanding split. Read § What is absent
before assuming any capability here is reachable in play.

---

## The two resolvers

There are **two** battle resolvers, and the split is deliberate. Ben ruled it 2026-08-13,
overturning BL-315's (armed house conflict spine) own earlier engine-parity strand.

They answer different questions. `resolve_battle` answers "province beats province, this year" in
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

**Inputs.** Two `std::vector<army_stack_entry>`, two `doctrine_row`s, a
`terrain_composition` + `terrain_landform` pair, a `season`, and two supply values 0..1000
(clamped). Nothing is rejected — an empty or all-naval stack resolves rather than erroring.

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
carries five fields:

| Field | Type | Notes |
|---|---|---|
| `position` | `entity_id` | **The tile the unit occupies.** Tile-canonical — see below. |
| `owner` | `entity_id` | Corporation or faction entity that controls it. |
| `count` | `int` | Number of units in the group. |
| `type` | `uint16_t` | Opaque index into the roster table. |
| `strength` | `int32_t` | Documented as a fixed-point combat scalar — see § Two live inconsistencies. |

**Position is tile-canonical.** BL-157 (military datamodel stub) ruled tile position canonical over
body/province on 2026-08-07; the struct shipped with a `body` field ahead of that ruling, and
BL-324 (unit hire surface) replaced it with `position` on 2026-08-08.

Units live in `world::units`, an `unordered_map<entity_id, unit_component>`. Every consumer that
aggregates over it does so order-independently (integer sums, owner filters), which is what keeps
the container's iteration order off the determinism seam.

**Three writers exist**, and no other code creates a unit:

- `src/world/hard_coded_world.cpp` — the Kepler player stub, count 50, type left at 0.
- `src/world/corporation_generation.cpp` — the player corp's starting unit beside its starting base (BL-331, starting military presence), count 50, roster row 0.
- `src/world/corp_command.cpp` — the `hire_unit` verb, count `hire_batch_manpower` = 50.

**No code turns a `unit_component` into an `army_stack_entry`.** `roster_stack` composes a stack
from *province manpower*, and its only caller is the Era −1 sim. The campaign has units and it has
resolvers, and there is no bridge between them.

**What reads a unit today.** Two condition subjects — `military_units` (summed counts) and
`military_strength` (summed strengths) — in `src/world/condition_set.cpp`, available to laws, techs
and quests. The Selection element's read-only unit card (Strength / Roster pages) and the hover
summary in `src/ui/entity_summary.cpp`. The corp AI, counting its own units against a soft cap of
three. That is the whole list.

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

On success: a new unit entity at the muster tile, `count` and `strength` both
`hire_batch_manpower`, returned through `apply_corp_command`'s out-param. The player's press then
selects the new unit; the rival scorer's path additionally reports an `agency_event::kind::hired`
for the decision feed and session history.

**Rivals hire through the same verb** (BL-324's widening of the AI-agency exception, standing
rules § Determinism & data model). `src/world/corp_ai.cpp` scores hiring alongside build, demolish,
survey and road; a corp with no completed base has no muster tile and must build one first, priced
by the ordinary build-candidate machinery. A soft cap of three units per corp keeps hiring from
becoming an unconditional extra action every evaluation.

### The roster

`src/world/unit_roster.{hpp,cpp}` (BL-274, era-keyed unit rosters). An authored **table**, not a
tree: availability is derived from ground and era band, with no research, no player choice and no
unlock events.

Eighteen rows across four cumulative bands — `classical`, `medieval`, `gunpowder`, `industrial`.
Bands are cumulative because nothing un-invents a spear: an industrial polity still fields levies
where its ground cannot pay for better.

Each row carries `{name, band, cls, gate, power_mod, weight}`. The `roster_gate` is four thresholds
on 0..1000 endowment axes: `ore_q`, `farm_q`, `port_q`, `energy_q`.

Two of those axes are **named substitutions** — the convention is to name the stand-in, never fake
the input. `farm_q` proxies for pasture, because horses have no signal of their own. `energy_q`
proxies for saltpetre, then genuinely for fuel from the industrial band.

**The file placement deviates from BL-274's own file list**, deliberately. BL-274 named
`combat.{hpp,cpp}` as the roster's home, but `combat.hpp` states the opposite boundary in its own
words. So the roster lives separately and *resolves into* combat's types.

**Two gate paths, one table.** `available_rows(const province&, band)` is the Era −1 path, gated on
authored province endowment. `available_rows(const world&, corp, band)` is the campaign path
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

`campaign_roster_band` is fixed at `industrial`. It is **not** derived from a military-capacity
score — that is the Era −1 settlement model. The campaign is simply industrial-era throughout, and
because bands are cumulative, every earlier row the corp's ground supports is still exposed.

`roster_stack(manpower, province, band, readiness)` composes an army stack from province manpower.
Later bands crowd out earlier ones — a row's weight decays ×0.35 per band it sits below the
polity's own, so a rifle-era army is not half levy spears. Readiness folds into each entry's
`type_power_mod` as `(readiness − 1000) / 10`, which is the same channel cohesion and defensive
works use.

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

Its consequence — units beyond the envelope losing strength each tick — is BL-325's slice S3 and is
**not built**. That slice is sequenced behind BL-315 (armed house conflict spine), per NR-177: do
not land a decay rule for units that nothing yet commands.

---

## Two live inconsistencies

Both are recorded here as **in-flight**, not settled. Do not write code against the intended shape;
write it against what the source does today.

**1. `unit_component::strength` duplicates `count`.** It is documented as a fixed-point combat
strength scalar (BL-157, military datamodel stub), but all three writers set it equal to the raw
manpower count with no scale factor. Both UI readers — the Selection strength page and the hover
summary — print it raw, rather than guessing a divisor that would disagree with each other.

BL-459 (unit strength is a duplicate of count) is in flight and changes this to a **derived** value:
`count × roster_type_quality × supply_factor` at ×100 fixed point, with the stored field dropped so
the duplicate cannot return. Until it lands, `strength` carries no information `count` does not.

Note the third reader, which is not UI: `condition_subject::military_strength`
(`src/world/condition_set.cpp`) sums `uc.strength` across a corp's units, so laws and tech gates
read it too.

**2. `corporation_component::military_points` — RESOLVED 2026-08-17, by deletion.** Every completed
`military_base` used to credit it a flat rate each economy tick, and nothing in `src/` ever read it.

BL-455 (military points and science are write-only) removed the field, its Lua rate, its parameter,
its write branch and every assertion about it. No consumer could be named: the economy→military
interface is already the base tile plus the reach field (§ One reach field), and the recurring cost
of force is already credits and ordnance (BL-454, unit upkeep).

**A military base now accumulates nothing.** It gates hiring and it occupies a tile; it does not
produce a currency. If the governing-body pivot (BL-094) later wants a military-capacity scalar,
it is a fresh field with its behaviour filed in the same change.

---

## What is absent, and known to be

This list is the reason this document cannot be read as describing a finished system. Everything
below is a real gap with a named owner.

- **No hostility model.** No corp may be at war with, or allied to, another. BL-448 (friend/neutral/hostile corp stance) is the item; the settled shape is hostility **directed** (declared unilaterally) and friendship **symmetric** (offered and accepted). It is not built, and until it is there is no legal predicate on an attack verb and no condition for an engagement trigger.
- **No unit-subject verbs.** Every command verb names a tile, a building, a body or an order; none names a unit. So a unit cannot be moved, merged, disbanded, garrisoned or ordered — it is created at its muster tile and stays there for the campaign. BL-393 (units are write-only and inert) and BL-314 (unit verb family) own this.
- **No engagement trigger.** Nothing in the world decides that two forces are fighting. `resolve_campaign_battle` has no caller but its harness, and no code composes an `army_stack_entry` from a `unit_component`, so the resolver could not be called even if something wanted to.
- **No battle state in the world.** `campaign_battle_state` is a value a caller holds, not a world component. Nothing serialises a fight in progress, and nothing steps one across ticks.
- **No upkeep.** A standing force costs credits once, at hire, and nothing thereafter. `scripts/economy.lua` records the absence explicitly. BL-454 (unit upkeep) owns it.
- **No out-of-supply decay.** BL-325 slice S3, above. Units outside the reach envelope suffer nothing.
- **No fortification.** Siege is uniformly weak in the open field precisely because there is no held position to reduce; `siege_stance` is declared and unread.
- **No tactical naval.** Naval rows exist in the roster and naval entries are accepted by `resolve_battle`, contributing zero power and zero weight. Naval presence is strategic tagging only.
- **No unit marker on the canvas.** Units are selectable through the tile click-cycle and have no glyph of their own.
- **No blackboard export of units.** An agent can raise a unit and cannot read back that it exists (BL-393, units are write-only and inert).
- **No demolish interaction.** Demolishing a muster base leaves its units standing on a tile whose base is gone; nothing cleans them up and nothing reports it (BL-393 again).

---

## Verification

Three headless harnesses cover this layer. Run them with the `verifier-headless` skill.

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

Adjacent coverage: `buildings_rework_harness` R6/R7 for `military_base` placement, zero staffing
and the not-an-anchor property.

---

## Build status

**Landed:**

- `resolve_battle` — the nation-scale resolver, consumed by the Era −1 history sim (BL-272, unit/doctrine combat model)
- `resolve_campaign_battle` and its begin/step/withdraw state machine — compiled, harnessed, **no production caller** (BL-315, armed house conflict spine)
- Terrain defence / attrition / resistance (BL-233, terrain combat modifiers)
- `unit_component` with tile-canonical position (BL-157, military datamodel stub; BL-324, unit hire surface)
- The 18-row era-keyed roster and both gate paths (BL-274, era-keyed unit rosters; BL-352, hire gate live store)
- `military_base` as a building, tech-gated behind `E0-ML-01` (BL-325 S1; BL-344)
- `hire_unit` gated on a completed owned base at the tile, with a credit cost and a resource draw (BL-325 S2; BL-394, hire is free on the seam)
- Rival hiring through the same verb (BL-324's exception; BL-293's seam widening)
- The player corp's starting base and unit (BL-331, starting military presence)
- ~~Passive `military_points` accumulation (BL-332)~~ — **removed 2026-08-17** by BL-455 (military points and science are write-only); a base accumulates nothing

**Outstanding:**

- Corp stance / hostility (BL-448, friend/neutral/hostile corp stance) — designed, unbuilt
- Unit verbs and the unit-command seam (BL-314, unit verb family; BL-393, units are write-only and inert)
- Anything that calls the campaign resolver: an engagement trigger, a `unit_component` → `army_stack_entry` bridge, battle state in the world (BL-315)
- Unit upkeep (BL-454, unit upkeep)
- Out-of-supply decay (BL-325 S3, sequenced behind BL-315)
- Hire price on screen (BL-405, hire has no price on screen)
- The Era −1 sim's conquest failure — 267 battles, zero province transfers (BL-384, Era −1 sim conquers nothing)
- Fortification, tactical naval, and a unit canvas marker — no owning item yet
