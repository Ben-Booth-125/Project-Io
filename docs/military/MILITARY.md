# Project Io — Military

> **Settles:** how force works in the campaign — what resolves a campaign battle · how terrain
> defends and attrits · what a unit is and which domains it may traverse · where units are raised
> and on what roster · how a march is queued and what turns stance plus position into a fight ·
> what standing force costs each tick · why economic reach is military reach · the four theatres
> and their units' stats · soft and hard attack · fronts · missiles and nuclear use.
> **Not here:** how force works inside the Era −1 sim — its own resolver, band ladder, naval and
> forage rules (MILITARY_HISTORY, a generation doc) · how a rival *decides* to use force
> (AI_OPPONENT) · what hostility and friendship *are* as quantities (RELATIONS) · what a nation
> may declare or hold (NATIONS).
> **Confused with:** generation/MILITARY_HISTORY.md, politics/RELATIONS.md, ai/AI_OPPONENT.md.

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

## The two resolvers, and the two documents

There are **two** battle resolvers, and the split is deliberate. Ben ruled it 2026-08-13,
overturning BL-315's (armed house conflict spine) own earlier engine-parity strand.

They answer different questions. `resolve_battle` answers "region beats region, this year" in
one scored evaluation, because the Era −1 sim runs millions of those. `resolve_campaign_battle`
answers "two forces in a province, over a short span, with a player who may pull out".

**Since 2026-09-06 the two resolvers also have two documents.** `resolve_battle` and everything
else the Era −1 sim does with force — the band ladder, naval, sea legs, the forage simplification
— moved to [`../generation/MILITARY_HISTORY.md`](../generation/MILITARY_HISTORY.md). **This
document owns the campaign-era model: what the player meets.**

Ben's reason for the split is worth carrying, because it is a rule about doc scope and not just
filing. The sim's rules *"apply specifically to generation, and more specifically to ancient
history"* — they are the cheapest things that produce a believable past, and stating them here
made them read as claims about the game. One of them was actively false as a general claim.

**Nothing is shared but the terrain table (Ben, 2026-10-26).** The campaign resolver scores its
own rounds from soft and hard attack against hardness (§ The campaign force model), and no
longer calls `resolve_battle`. `resolve_battle` stays the Era −1 sim's alone. This overturns the
2026-08-13 reading that the calibration was shared: the two resolvers now answer their questions
with two different models, and only `terrain_combat`'s table is read by both.

### `resolve_battle` — nation scale — MOVED

**It lives in [`../generation/MILITARY_HISTORY.md`](../generation/MILITARY_HISTORY.md)**
§ The resolver, together with everything else the Era −1 sim does with force (Ben, 2026-09-06).
That document is a **generation** doc: the sim is a pass that builds a world, and its rules are
the simplifications that make two thousand years of history cheap enough to generate — not claims
about how war works in Io.

**Nothing here depends on it** (Ben, 2026-10-26). The campaign round is scored by the soft/hard
arithmetic in § The campaign force model; `resolve_battle` is a generation function.

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

**Each round.** Every participating unit deals its hits by the soft/hard arithmetic (§ The
campaign force model), then exactly two draws apply a swing — attacker first, defender second,
always in that order. A side breaks when its units' readiness is spent, not at a strength
threshold.

Tuning lives in `campaign_battle_params`, as data, on the `corp_ai_params` precedent:

| Field | Default | Meaning |
|---|---|---|
| `max_rounds` | 6 | Rounds before the fight is called a stalemate |
| `swing_permille` | 600 | Half-width of the per-round power swing — a uniform draw in [400, 1600] |
| `absorbed_land_permille` | 250 | Share at which hits within the target's defence (or breakthrough) land; hits beyond it land in full |
| `hp_share_permille` | 300 | Share of landed damage that kills men; the rest spends readiness |
| `hp_cost` | 400 | Milli-hits of HP damage that kill one man |
| `readiness_cost` | 185 | Milli-hits per man that spend a unit's whole readiness |
| `terrain_attack_scale` | 1000 | How far `terrain_defence` divides the attacker's hits — a mountain halves them |
| `wavering_readiness` | 300 | Side readiness at or below which the phase reads wavering |
| `morale_win` / `morale_loss` | 50 / 100 | Morale a battle's winners gain, and its losers and every retreating unit lose |
| `readiness_recovery_permille` / `morale_recovery_permille` | 100 / 10 | Out of contact, per economy tick (a quarter, not a day: recovery follows the quarterly supply factor): readiness regained (by supply, never above morale), and morale's step toward supply |
| `withdraw_base_permille` | 20 | Flat price of turning your back at all |
| `withdraw_per_round_permille` | 25 | Added per round already fought |
| `withdraw_pursuit_scale` | 400 | Share of how far behind the withdrawing side is |
| `rounds_per_day` | 1 | Rounds fought per day (§ Battles ruling 3, Ben 2026-10-28) |

**The strength reading is side readiness**: a side's men-weighted mean readiness, read against the
units still in the fight. The withdrawal price is charged **in men**, per-mille of the side's
standing men.

**The curve** (`campaign_battle_harness` C4, infantry on plains, 200 seeds a ratio): attacking is
dear, and breaking a defender takes about 2:1 in men — decisive 0% at 1:1, 15% at 1.5:1, 64% at
1.75:1, 89% at 2:1, 99% from 2.5:1. `swing_permille` stays at 600 (NR-204: a narrower swing made the
edge too safe).

**A battle lasts days.** At one round a day against `max_rounds` 6, a fight is over in at most six
days. Measured through the daily pass (`move_by_days_harness` D6, infantry on plains, 40 openings a
ratio, days from contact to close): **about five days at parity** (4.9), 5.6 at 1.5:1, 3.7 at 2:1 and
2.1 at 3:1 — the curve's mean rounds, a round a day. The round's damage is calibrated to the day:
one round is one day's fighting, so the per-round numbers above are a day's. A commander
gets a decision every day of the fight — a withdrawal is honoured at the next day's boundary —
rather than one between two quarters.

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

## The campaign force model (Ben, 2026-10-26)

Force acts in four theatres — **land, air, sea and space** — and is set across 1960 to the near
future. The research behind it, and the comparable games it learns from, is
[`../research/MILITARY_1960_PRESENT.md`](../research/MILITARY_1960_PRESENT.md). Every stat a fight
reads is shown on the surface that reads it.

### Land — the unit's stats

| Stat | Grain | Meaning |
|---|---|---|
| **HP** | stored, per unit | Men and equipment fit to fight. **At 0 the unit dies.** Refilled over time by drawing equipment and manpower (§ Refit). |
| **Readiness** | stored, per unit | Cohesion in contact. Takes most of the damage. **At 0 the unit retreats** one tile away from the enemy. Recovers out of contact. |
| **Morale** | stored, per unit | Will to fight. Scales the unit's attack and caps its maximum readiness. Moved by supply, victories, losses and its nation's war exhaustion. |
| **Supply** | stored, per unit | `supply_factor_permille`, as today (§ Upkeep). Scales attack and recovery alongside morale. |
| **Soft attack** | class × equipment | Hits per round against a target's soft share. |
| **Hard attack** | class × equipment | Hits per round against its hard share. |
| **Hardness** | class × equipment, 0..1 | Share of the unit that is armoured. |
| **Defence** | class | Hits absorbed when defending. |
| **Breakthrough** | class | Hits absorbed when attacking. |

**The arithmetic.** A unit's hits on a target are
`soft_attack × (1 − target.hardness) + hard_attack × target.hardness`, scaled by its morale and
supply. The defender's ground divides the attacker's hits (`terrain_defence`: a mountain halves
them). The target absorbs up to its defence (defending) or breakthrough (attacking), raised by
fortification; absorbed hits land at a fraction, the rest in full. Landed damage
splits between HP and readiness, readiness taking the larger share, so most fights end in a
retreat rather than a death — which is how real units fail. **There is no piercing-versus-armour
check**: hardness alone carries "infantry cannot hurt tanks".

### Land — the classes

**A unit is raised to a DESIGN (Ben, 2026-10-27).** A design is a land class with a choice of kit
in each of its slots: the class fixes the role, and the kit is what research has made available —
better weapons, munitions and armour. Slots swap: infantry fields rifles or anti-tank launchers;
artillery fields its guns and, alongside them, drones or anti-armour. **Kit is researched before it
can enter a design, and a unit is designed before it can be raised**, so a technology shows in the
next unit raised and, as kit arrives, in the units already in the field (§ Land — equipment, refit
and manpower). The kit is a good on the shelf. Designs are made on the Military ledger's **Design**
tab (`../ui/ledgers/military.md`).

**The kit (Ben, 2026-10-27; the item list is `../research/MILITARY_KIT.md` §§ 1–2).**

- **Each class has its own slots (Ben, 2026-10-28, overturning two slots a class).** *"Tanks might
  have armament / armour / tracks, while foot soldiers only have weapon / armour."* A slot is one
  changeable piece of equipment:

  | Class | Slots |
  |---|---|
  | Infantry | Weapon · Armour |
  | Motorised infantry | Weapon · Armour · Vehicle |
  | Mechanised infantry | Weapon · Armour · Vehicle |
  | Light armour | Armament · Armour · Wheels |
  | Medium armour | Armament · Armour · Tracks |
  | Artillery | Gun · Munitions · Tow |
  | Air defence | Launcher · Fire control · Vehicle |

  The swaps Ben named live in these slots: infantry's weapon is rifles or an anti-tank launcher;
  artillery's munitions are shells, anti-armour submunitions or loitering drones. A class's speed
  stays its class's (§ Marching) until kit changes it.
- **Each class starts on its base kit** — the 1960 standard — with nothing researched; each later
  item adds to the class's numbers.
- **Kit is researched in the age tree**, not on a ladder of its own: the military kit is nodes of
  the age's tree like any other technology (`../economy/RESEARCH.md` § What technology is for).
- **One live design per class.** Choosing an item per slot from what is researched and saving it
  makes it what Recruit raises for that class; named designs kept side by side are a later step.
- **Obsolete unit types.** A nation marks a roster row obsolete, and Recruit hides it (a "show
  obsolete" switch brings it back); units already raised are untouched. **Every row below the
  modern band starts obsolete** for every nation, so Recruit opens on the modern types. A rival
  nation's scorer never raises an obsolete row.

| Class | Hardness | Soft | Hard | Role |
|---|---|---|---|---|
| Infantry | none | medium | low, raised by anti-tank missiles | Holds ground; cheapest. |
| Motorised infantry | slight | medium | low | Infantry in trucks; trucks are its equipment, not a unit. |
| Mechanised infantry | high | medium | medium | Infantry in fighting vehicles. |
| Armour | very high | medium | high | One main battle tank line, by generation. |
| Light armour | medium | high | low | Fast reconnaissance. |
| Artillery | low, higher self-propelled | very high | low | Fights from the second line; the largest munitions draw. |
| Air defence | low | — | — | Shoots aircraft, drones and missiles over its province. |

**Small drones are equipment, not units.** An FPV drone stock raises a land unit's soft and hard
attack and draws a drone good, which is how they are used.

### Land — equipment, refit and manpower

**Equipment is goods on the shelf**, bought as muster already buys (§ The muster interface).
**Six goods, by kind, not by generation (Ben, 2026-10-27):** small arms, armoured vehicles,
artillery pieces, guided weapons and drones, with the existing electronics; later kit needs more of
them and adds electronics, so the economy feels each generation without a good for every rung.
Ordnance stays every unit's per-tick draw. **Refit**: a wounded unit regains HP by drawing equipment and manpower each tick. A unit
whose kit cannot be bought stays wounded; the economy is the war's ceiling.

**Soldiers come from the labour pool** — shown, with the conscript or professional choice, on the
Military ledger's **Recruit** tab (Ben, 2026-10-27). Every soldier is drawn from the same `(nation, body)`
labour pool buildings staff from (`../economy/POPULATION.md`), so a mobilised army is an emptied
factory. A nation chooses **conscript** or **professional** service: conscripts are cheap per head
and drain labour widely; professionals cost wages and are fewer and better.

### Land — on the canvas

A land unit is drawn as a **rectangle within its tile**, showing how many units stand there, their
HP and their readiness. One rectangle stands for each owner on the tile, marked with the owner's
colour; HP is men over establishment, readiness men-weighted. The bars are the player's own units'
(a rival's internals stay private, BL-068). The unit card carries HP, readiness and morale, and
the land class the unit fights as (`../ui/ICONS.md` § Entity markers).

### Fortifications

A **bunker** is a building. Units on its tile gain defence, and the tile's defenders gain hardness
— concrete makes infantry hard to soft attack. Strikes and artillery degrade it; it repairs over
time.

### Fronts

**A front is one contiguous span of a nation's border with one neighbour (Ben, 2026-10-27).** A
nation's **border tiles** toward neighbour B are its own tiles with at least one hex side onto B's
ground. Those tiles, joined wherever two of them touch, fall into one or more **spans**; each span
is a front. **Every neighbour has fronts (Ben, 2026-10-27)** — at peace as at war; a front toward a
neighbour the nation is not hostile to can only **hold**. A border that runs unbroken is one front; a border that the sea or a third nation cuts
in two is two. Fronts are **derived** from the territory map (`world::tile_to_nation`) and
re-derived whenever it changes, so a front follows a border as provinces change hands.

**A front lives on the canvas**, not in a ledger (Ben, 2026-10-27): drawn as a line along the shared
border edges of its span, coloured by the stance toward its neighbour, with the number of units
assigned to it. **Clicking the line selects the front**, and the Selection band shows a **front
card** — its neighbour and span, its assigned units, and its order (hold or push), which is where
the order is set (Ben, 2026-10-27).

**Units are assigned from their selection (Ben, 2026-10-27).** The unit card, and the stack panel for
a stack, carries **Assign to front**: the fronts of the unit's own nation, each named by its
neighbour and its span. An assigned unit keeps its assignment until it is unassigned, assigned
elsewhere, or its front disappears.

**What a front does with its units.** Assigned units spread **evenly along the span** — each takes a
slot tile spaced along the span in walk order — march to their slots, fill a gap when a unit dies or
retreats, and return to the span after a retreat. A front has two orders: **hold** (the default),
and **push** — open only toward a neighbour the nation is hostile to — which marches the front's
units onto the neighbour's tiles across the span, attacking what stands there (§ Battles). A front
decides only who stands where and what it marches onto; every move is a legal `march_unit`. Moving the player's assigned units is a granted auto-action, and
**rival nations use fronts too** — assigning their own units and choosing hold or push by their
scorer (Ben, 2026-10-27; both grants are `../ai/AI_OPPONENT.md` § 11).

**How a front is named and walked.** A front is named by a **key**: a tile in its span, normally
its lowest tile id. Keys are unique within a nation. One tile can border two neighbours, so spans
claim keys smallest first; each takes its lowest tile that no earlier span took. A span runs
**end to end**: from the tile farthest from its lowest tile, along the shortest path to the tile
farthest from that end. A span that closes on itself, ringing an enclave, runs on round the other
side. Tiles off that line sit beside their nearest line tile. Every tie goes to the lower tile id.

**Slots.** A front with *n* assigned units cuts its span into *n* equal stretches. Unit *i*, in
ascending unit id, takes the middle tile of stretch *i*. A unit is on its slot once it stands on
the slot tile; a march ends on its tile (§ Marching). The front re-issues a march whenever a unit's
order points anywhere else. To move an assigned unit by hand,
unassign it first. A unit in a battle or refitting is left alone. A dead unit frees its slot, and
the rest re-space on the next tick.

**The push target (Ben, 2026-10-27: "fronts and push use tiles too").** A push considers the
neighbour's land tiles with a hex side onto the span. Each pushing unit marches onto the one whose
standing stack is weakest — the summed `unit_strength` of the units on it, an empty tile weakest
of all — nearest its own slot, ties to the lower tile id. A tile the nation's own units already
stand on is taken, not a target. Marching onto a held tile is an attack (§ Battles). A unit already standing on the neighbour's ground holds what it took. A push is open only while the nation has declared hostility
toward the neighbour, which is the battle trigger's one-way test. If that hostility ends, the stored
push acts as hold until it returns.

**When the border moves.** Fronts re-derive from the territory map. An assignment or order follows
its key. If the key no longer names a front, it moves to the front toward the **same neighbour**
whose span now holds the key tile. Failing that, it goes to the front toward that neighbour nearest
the key tile, in hex steps, ties to the lower key. If no front toward that neighbour remains, it is
dropped. When two fronts merge, the merged front keeps the first stored order.

**A rival's fronts.** A rival assigns its idle units: its own, unassigned, unordered, out of
battle and not refitting. The capital province's units stay home. Each goes to the front with the
fewest assigned units per span tile, taken in ascending id. A rival pushes where it is hostile to
the neighbour and its assigned strength exceeds 3:2 over the push target's defence. Otherwise it
holds. It never declares the hostility a push needs.

### Air

An air wing is **based** at an airbase and **assigned** to provinces in range with a mission —
**air superiority**, **strategic bombing** of infrastructure and production, or **tactical
bombing** of land and sea units. It never stands on a tile. Air units carry HP, morale, an air
attack, and ground soft and hard attack; they carry **no hardness**, because every aircraft is
soft. Classes: planes (fighter, strike, strategic bomber, helicopter) and strike drones. Ground air
defence engages every mission flown over its province.

### Sea

Sea units operate on coast and ocean tiles, on **patrol**, **escort** and **convoy raiding**.
They carry HP, morale and **one attack** straight to HP, plus a **detection** value and a **target
mask** (which classes they can hit). A unit is engaged only once detected, which is a submarine's
whole value. Classes: submarines, destroyers and capital ships. A raided convoy is SUPPLY's convoy.

### Missiles

**A missile is a munition, not a unit**: a good fired by a launcher — a land unit, a ship, an
aircraft or a silo building — at a province or a building, and consumed on use. **Missile defence**
is an interceptor stock at an air defence unit or building. It thins short-range attacks well and
long-range ones poorly, and an interceptor costs more than much of what it stops, so defence drains
the defender's economy.

**Tactical nuclear weapons may be used; strategic nuclear weapons may not.** A tactical
warhead is a munition with a severe Alarm cost (`../politics/RELATIONS.md` § 6). A strategic
arsenal is held, seen and deterring, and no verb fires it.

### Space

Pre-space military in orbit is **intelligence satellites** and **missiles**. A reconnaissance
satellite lifts fog over enemy ground and reveals unit stacks; the satellite classes are
`../economy/SPACE_ASSETS.md`'s. There is no combat between orbital units.

*Owners: BL-1421 (soft hard resolver), BL-1422 (land classes and kit), BL-1423 (refit and
retreat), BL-1424 (soldiers from labour), BL-1425 (unit counter render), BL-1426 (bunkers),
BL-1427 (fronts), BL-1429 (air theatre), BL-1430 (sea theatre), BL-1431 (missiles and
satellites).*

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
| `owner` | `entity_id` | The nation that controls it (Ben, 2026-10-23: every unit is a nation's). |
| `count` | `int` | Number of men in the group. |
| `type` | `uint16_t` | Opaque index into the roster table. |
| `order` | `movement_order` | `{dest, path, next_index, progress}`; `dest == null_entity` means halted/unordered. See § Marching. |
| `supply_factor_permille` | `int32_t` | How well supplied, 1000 = full. Moved only by the upkeep pass's one decay rule. |
| `muster_base` | `entity_id` | The `military_base` this unit was raised at, or `null_entity`. The **orphan key** — see § Upkeep. |

**A unit also carries HP, readiness and morale (Ben, 2026-10-26)** — three stored per-unit
quantities, defined in § The campaign force model. They are the fight's state; what follows is
the one derived figure the condition subjects and cards read.

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
- `src/world/corporation_generation.cpp` — `seed_starting_military`: a starting unit beside a
  starting base (BL-331, starting military presence; BL-476, rivals start armed), count 50, the
  modern infantry row (§ The roster), recorded with that base as its `muster_base`. Its subject was every non-background
  corporation, placed by the HQ; the corporations and the HQ are retired (Ben, 2026-10-23), so the
  unit is a nation's, on its own ground. **Every nation opens with a military base at its capital
  (Ben, 2026-10-27)** — § Nation garrisons — and the opening unit musters there: its base is the
  capital base, never a second one. Which nations also open with the unit is the opening-force
  rule: every rival major does, the seat does not.
- `src/world/corp_command.cpp` — the `hire_unit` verb, count `hire_batch_manpower` = 50. Records
  the tile's qualifying base, lowest entity id first.
- `src/world/nation_generation.cpp` — a nation's garrisons, owner set to the nation entity. A
  garrison is posted, not raised: the **capital** garrison records its capital's military base as
  its `muster_base`, and a border garrison records none. See § Nation garrisons, below.

**The bridge from a unit to the campaign resolver** is the `land_combatant` `run_battles` builds
from each participating unit: its men, readiness, morale and supply, and `unit_land_stats` — its
row's land class, scaled by the row's quality (`src/world/unit_roster.hpp`). `unit_to_stack_entry`
is the Era −1 resolver's adapter, and `roster_stack` — composition from *region manpower* — is
Era −1's too.

**What reads a unit.** Two condition subjects — `military_units` (summed counts) and
`military_strength` (summed derived strengths, an integer sum) — in `src/world/condition_set.cpp`,
available to laws, techs and quests. The Selection element's unit card (Strength / Roster pages)
and the hover summary in `src/ui/entity_summary.cpp`, both printing the derived strength with its
`count × quality × supply` derivation. A rival nation's build scorer, counting its own units against a soft cap of
three. The battle pass, the march pass, the upkeep pass and the budget's `upkeep` term. The
blackboard export, for a nation's own units.

**The unit on the canvas.** A unit has its own marker and command surface — BL-471 (unit marker
and command surface) owns both. A unit is also reached by clicking its tile: the Planetary canvas
cycles unit → building → tile on repeat clicks of the same tile.

---

## Domains and traversal (Ben, 2026-09-06)

**A unit type declares which domains it can cross.** That one variable replaces the flat rule that
units are land-bound, and it is what makes water a place rather than a wall.

| Domain | Land classes | Coastal/naval classes |
|---|---|---|
| `land` | Yes | No |
| `coastal_water` | **Only where OWNED** (`docs/generation/PROVINCES.md` § Who owns water) | Yes |
| `open_ocean` | No | Yes |

**Land units crossing owned coastal water is the deliberate middle case.** A shoreline your polity
holds is a shallow, bridged, causewayed thing — the road layer already reasons this way, treating a
short crossing with no open ocean in it as a strait rather than a sea. Making the crossing depend on
*ownership* rather than on distance gives it a cause the player can read: you can walk your own
shore, not someone else's.

**Campaign-era naval is § The campaign force model's Sea (Ben, 2026-10-26)**, which overturns
the 2026-09-06 reading that the campaign raises no fleet. The Era −1 sim's naval rows stay
[`../generation/MILITARY_HISTORY.md`](../generation/MILITARY_HISTORY.md) § Naval's. The domain
table above still says which classes may be where.

**Overseas supply WORKS, and there is no starvation model here.** Historically nations supplied
overseas expeditions perfectly well; that is most of what a navy is for. The Era −1 sim applies a
shore-adjacency forage **simplification** because it needs overseas reach to cost *something* and
cannot afford a logistics model — it is a stand-in, labelled as one in its own document, and it
does not describe the campaign.

**Water gives 0 defence, and that part is general.** There is nothing to stand behind at sea. It
is the *forage* half of the same table that is an ancient-sim question, which is why the rule
lives in that sim's caller and `terrain_combat`'s table is left alone — **both resolvers read that
table**, so a rule true of only one of them can never be expressed in it.

**A coastal province holds a port, and nothing else (Ben, 2026-09-06).** Buildings refuse water
outright today; the exception is the port and it is the only one. The reasoning is the same one
that makes coastal water ownable at all — a shore you hold is a place you can build a thing that
faces the sea, and a port is precisely that thing. Every other building type wants ground, workers
and a deposit, and none of the three is on the water.

**What that does NOT open.** A port on water is still gated by the ordinary placement seam
(`placement_rules::can_place`) and still needs its tile owned; it is not a way to claim water by
building on it, because ownership is derived from the shore and never from an installation
(`docs/generation/PROVINCES.md` § Who owns water). Whether a water port differs from a land port
in what it *does* is `docs/economy/LOGISTICS.md`'s question, not this document's.


## The muster interface

**The nation musters (Ben, 2026-10-10; `../CONCEPT.md` § Development through ages).** The player is a
major nation, and a nation **raises its own units and buys their equipment off the market's shelf**,
made by its own works or another nation's (Ben, 2026-10-23). So the hirer at a base is the nation. The
nation buys the equipment **as a real buyer on the order book** (Ben, 2026-10-10, NR-1028), so a nation's war
demand moves the prices of the goods it arms with (`../politics/NATIONS.md` § 2. Conservation).

**The nation builds its own base (Ben, 2026-10-23).** A nation builds every building on its own
ground, paid from its treasury, and the military base is one of them; the 2026-10-11 commission to a
corporation retired with the corporations. It then hires through its **muster** verb at a base it
owns.

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

**No technology closes it.** The military base is open to every nation from the start — NR-1022
(old gates open, Ben, 2026-10-10). A building type no node unlocks is open to every nation
(`src/world/nation_tech.hpp` § `structure_unlocked`).

**Hire happens at the base** (BL-325 S2). `corp_verb::hire_unit` takes `{tile, unit_type}` and
checks, in order:

1. `unit_type` indexes the roster table, else `rejected_invalid`.
2. That row is available to this nation under the **live** gate — re-checked at apply time, so a caller's cached list is refused. Else `rejected_invalid`.
3. `tile` names a real tile, else `rejected_invalid`.
4. **That tile carries the acting nation's own `military_base`, completed (`ticks_remaining <= 0`) and not decommissioned.** A base still building does not qualify. Else `rejected_placement`.
5. The nation can pay **both** legs, else `rejected_funds`.

The two payment legs. A **credit cost** of
`hire_base_cost + hire_cost_per_power × row.power_mod` (economy.lua § military: 120 + 1.4 × power,
BL-744 / NR-781, so Levy Spear costs 120, Rifle Regiment 652 and Rifle Infantry 736). Plus a **flat resource draw** of 5.0 per *gated*
axis, **bought** off the shelves of the markets the nation's buildings sit in, lowest market id first, at each
shelf's posted price — a hire buys its kit like any buyer (`../economy/MARKETS.md` § The shelf
economy).

`port_q` is exempt from the resource leg — it is a building check, so there is nothing to debit.
The other three axes draw from the same preference order the gate reads, so a hire never spends a
resource the gate did not already confirm access to.

The credit base is a **floor**: row 0's gate is all-zero, so the resource leg alone would test and
spend nothing, and the cheapest rows would be free and unbounded for any seam caller. Both legs are
checked before either is debited, so a refusal leaves the nation wholly uncharged.

**Raising takes time (Ben, 2026-10-27).** An accepted hire enters a **muster queue** at its base, as
a building enters construction, and becomes a unit when its muster completes; the Military
ledger's **Recruit** tab (rail slot 7, `../ui/ledgers/military.md`) shows the queue and is the
door to every hire. A muster takes **90 days** for infantry, motorised infantry and light armour,
**135** for artillery and air defence, and **180** for mechanised infantry and armour — a quarter
a tick, so one, two or two ticks (`muster_days_for`). A hire still mustering counts against a
rival nation's soft cap of three units. An order whose base is demolished or decommissioned is
dropped with no unit, as a unit whose base is torn down is disbanded. On completion: a new unit entity at the muster tile with
`count = hire_batch_manpower`, returned through `apply_corp_command`'s out-param. The verb records the base it was raised at as the unit's
`muster_base`; where the tile carries more than one qualifying base, the **lowest entity id**
wins, so the choice is deterministic.

The player's press then selects the new unit; the rival scorer's path additionally reports an
`agency_event::kind::hired` for the decision feed and session history. The hire price is shown on
the press (BL-405, hire price on screen).

**Rival nations hire through the same verb** (the BL-324 widening of the AI-agency exception,
carried to the nation unchanged, Ben 2026-10-23; `../ai/AI_OPPONENT.md` § 11). The build scorer
(`src/world/corp_ai.cpp`) scores hiring alongside build, demolish, survey and road. Every nation
opens with a completed base at its capital (Ben, 2026-10-27; § Nation garrisons); a nation that has
lost it has no muster tile and must build one first on its own ground, priced by the ordinary
build-candidate machinery. A soft cap of three units per nation keeps hiring from becoming an
unconditional extra action every evaluation. Availability is decided by the stock on the shelves of
the markets the nation's buildings sit in alone; the credit cost is subject to the solvency gate like every other spend
(Ben, 2026-08-13, NR-218).

**Refit to a newer type (Ben, 2026-10-27).** A unit can be re-equipped to a newer model of its
own land class, which is how a nation upgrades the force it already has. The target is a roster
row of the same class (`land_class_for`), newer — a later band, or the same band with a higher
`power_mod` — and one the nation can field now; an obsolete row is allowed, listed after the live
ones. The unit must be the nation's own, out of battle, not already refitting, and standing on its
own nation's ground. It pays at once: the hire price difference, floored at zero, plus the target's
gated goods off the shelf as a hire buys them. It takes the target's muster days, a quarter a tick.
Until then the unit keeps its old type, fights as it and is drawn into battles as it, but does not
march: re-equipping is done in place, so a standing march order is cleared. On completion it takes
the new type and keeps its men, establishment, readiness and morale. The press is on the unit card
(`refit_unit`). Rival nations do not refit; no AI grant covers it. This is distinct from the
**Refit** of § Land — equipment, refit and manpower, which refills a wounded unit's HP.

### The roster

`src/world/unit_roster.{hpp,cpp}` (BL-274, era-keyed unit rosters). An authored **table**, not a
tree: availability is derived from ground and era band, with no research, no player choice and no
unlock events.

**Twenty-six** rows across five cumulative bands — `classical` 7, `medieval` 4, `gunpowder` 4,
`industrial` 4, and the campaign-only `modern` 7. Bands are cumulative because nothing un-invents a
spear: an industrial polity still fields levies where its ground cannot pay for better.

Each row carries `{name, band, cls, gate, power_mod, weight, domains, land}`. The `roster_gate` is
four thresholds on 0..1000 endowment axes: `ore_q`, `farm_q`, `port_q`, `energy_q`. Rows are only
ever **appended**: `unit_component::type` is a saved index into the table.

**The modern band is the campaign's, and the Era −1 sim never reaches it (BL-1422, land classes and
kit).** It holds one row per land class (§ Land — the classes), and each declares its class in
`land`; every earlier row leaves `land` unset and `land_class_for` derives its class from `cls` and
band. The sim reaches the roster only through `roster_band_for_capacity`, which tops out at
`industrial`, and the region-gated `available_rows`; no ladder capacity maps to `modern`. The sim's
per-band counters stay sized by the four ladder bands (`roster_band_count`).

| Row | Land class | Gate (ore / energy) | `power_mod` |
|---|---|---|---|
| Rifle Infantry | Infantry | — / — | 440 |
| Motor Rifles | Motorised infantry | 300 / 500 | 450 |
| Mechanised Infantry | Mechanised infantry | 600 / 600 | 470 |
| Medium Armour | Armour | 800 / 700 | 500 |
| Light Armour | Light armour | 500 / 600 | 460 |
| Field Gun Battery | Artillery | 700 / 400 | 480 |
| Air Defence Battery | Air defence | 600 / 400 | 470 |

The **class carries the role and `power_mod` the generation**. A modern row sits one step above the
industrial rows (380–420), and the spread is kept narrow so the class, not the modifier, decides the
fight: `unit_land_stats` scales the class stats by 1000 + `power_mod`. Ore gates steel-heavy kit and
energy gates fuelled vehicles; infantry is ungated, the floor a modern state can always raise. Each
modern row also names the nearest Era −1 `cls` for what still keys on it — the stack adapter. A
march does **not** key on it: speed keys on the land class (§ Marching), and only a pre-modern row,
which declares no land class, marches by its `cls`.

**Seeded forces open on modern infantry.** A nation's garrisons and its opening unit take
`campaign_opening_unit_row()`, the modern row of land class infantry, looked up by class and band
rather than by index.

Two of those axes are **named substitutions** — the convention is to name the stand-in, never fake
the input. `farm_q` proxies for pasture, because horses have no signal of their own. `energy_q`
proxies for saltpetre, then genuinely for fuel from the industrial band.

**The roster lives apart from `combat.{hpp,cpp}`**, deliberately. `combat.hpp` states in its own
words that it scores whatever stack it is handed and knows no roster; so the roster lives
separately and *resolves into* combat's types.

**Two gate paths, one table.** `available_rows(const region&, band)` is the Era −1 path, gated on
authored region endowment. `available_rows(const world&, actor, band)` is the campaign path, gated
on the stock standing on the shelves of the markets the nation's buildings sit in — the `stockpile`
subject's reading, since a nation holds no goods of its own.

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
split every other era gate reads. Every nation opens on the industrial band (`../economy/ERAS.md`
§ Where the ladder starts; Ben, 2026-10-10), so the campaign roster opens at its highest band,
`modern`. An ancient campaign opens at `classical`. Because bands are cumulative, every earlier row
the nation's ground supports is exposed either way.

`roster_stack(manpower, region, band, readiness)` composes an army stack from region manpower.
Later bands crowd out earlier ones — a row's weight decays ×0.35 per band it sits below the
polity's own, so a rifle-era army is not half levy spears. Readiness folds into each entry's
`type_power_mod` as `(readiness − 1000) / 10`, which is the same channel cohesion and defensive
works use.

---

## Marching

A unit is not pinned to its muster tile. Three `corp_verb` rows extend the **same** command
seam every other action goes through — there is no parallel `unit_command` type (BL-470 ruling 2;
BL-314's unit verb family resolves to this seam, extended):

- **`march_unit {subject: unit, tile: dest}`.** Sets/replaces the unit's `movement_order`. **Every march
  goes to a tile and ends on it (Ben, 2026-10-27: "all marches go to a tile")** — the province grain
  a march once had is retired. Rejects
  `rejected_not_owner` (not the caller's unit), `rejected_invalid` (unknown unit/tile, ocean dest,
  dest off the unit's own body, or unreachable), `rejected_state` (dest already `== position`, the
  unit is refitting, or the unit is in a battle — `unit_in_battle`; walking away from contact is a
  priced withdrawal, never a march). A command that names a province and no tile names no
  destination and is refused.
- **`halt_unit {subject: unit}`.** Clears the order. `rejected_state` on a unit with none.
- **`disband_unit {subject: unit}`.** Erases the unit outright. No refund — manpower walks away;
  a confirm step, if any, lives in the UI (BL-471), never in the seam.

Merge/split and garrison/scout are formation verbs, owned by BL-472 (formations).

**One nation per tile, always (Ben, 2026-10-27).** Units of one nation stack freely; units of two
nations never share a tile, whatever their stance. A march path is solved around tiles another
nation's units stand on, save its own destination. A step onto a tile that another nation's units
hold is never taken as a move:

- **Toward a nation the marcher is hostile to, it is an attack (Ben, 2026-10-27: "attacks should take
  place when an attacking unit attempts to move into a defending unit's current tile").** The march
  stops on the tile it stands on and a battle opens on the defender's tile (§ Battles). This holds
  wherever the path meets that unit — "stops and attacks it" — not only at the march's end. The
  march's order is spent only by the battle it opens: if no battle opens with the unit — the stack
  walked off the same day, or the unit was drawn into another battle first — it keeps its order
  and carries on. A spent unit (readiness 0) starts no fight, so a march that would be an attack by
  one is refused when it is given.
- **Toward any other nation, the step is blocked.** The march re-paths around the tile; if no path
  remains, the order clears.

A unit in a battle does not march: an order it holds waits until the fight releases it. A muster
that finishes while another nation's units stand on its base's tile waits for the tile to clear, and
a retreat never steps onto a tile another nation holds (§ Battles).

A right-click on a tile with a unit selected is a march to that tile; on another nation's stack it is
an attack, and on a nation the player is not hostile to it first offers to declare hostility
(`../ui/SELECTION.md`). A unit assigned to a front that is given a hand march — by right-click or
by the unit card's March — is unassigned once the march is accepted, and obeys. A declare-and-attack
in which no unit's march would be accepted declares nothing.

A world saved before this rule may hold two nations on one tile. On load each such tile is resolved:
the nation of the lowest-id unit standing there keeps it, every other nation's units step off by the
retreat rule (one crossable land tile no other nation holds, preferring their own nation's ground),
and a unit with nowhere to step is disbanded.

**Movement is a path march, not a hop.** `march_unit` computes the path ONCE, via
`intra_body_path` (`src/world/logistics.hpp`) — the same terrain-weighted A* the convoy system and
`body_reach_field` use — and stores it on `unit_component::order`. No per-tick Dijkstra:
`run_unit_march` (`src/world/economy_system.cpp`) only re-paths when a step is found blocked. Terrain
never changes post-generation, so the guard is defensive, and the determinism harness exercises it
directly.

**Units march by the day, at their class's speed (Ben, 2026-10-28).** A march no longer waits on the
quarterly economy tick: it advances every day. Each land class takes an authored number of **days
to cross one plains tile** — foot infantry 3, motorised infantry 1, mechanised infantry 1, light
armour 1, medium armour 1.5, towed artillery 2, air defence 1.5 — so trucks and tracks are what make
a force fast. Terrain multiplies the days by the tile's traversal cost
(`logistics::tile_traversal_cost`, plains 1.0, mountain 2.0) and a road divides them, as it does
for every other traveller. Partial progress banks in `order.progress` from day to day. The speed
is the **class's**; a design's transport kit does not change it until kit exists (BL-1436, kit in
the age tree). A sea unit moves on coast and ocean tiles under § The campaign force model's Sea,
not on this land table. A composite unit (BL-472, formations) moves at its **slowest** component's
speed. Logistic Points are still granted once a quarter (§ One reach field); a march spends its
grant day by day, and a unit whose grant is spent halts until the next quarter's grant.

The days are authored data: `economy.military.march_days_per_tile`, keyed by land class, and
`economy.military.march_days_per_tile_by_cls` for a **pre-modern row**, which declares no land
class and marches by its `cls` — infantry 3, cavalry 1.5, ranged 3, siege 3 (foot pace for a siege
train); naval has no entry, so a naval row cannot march on this table. The loader rejects any entry
that is not a positive, finite number, and any key that names no class. A day's movement is
1 / the class's days, banked in `order.progress` in units of traversal cost — the unit the field
always had, so a saved part-tile reads as the same part-tile — and the unit steps whenever the bank
covers the next hop's price (`edge_traversal_cost`). A road divides by the road ladder's own
discount, `road_traversal_multiplier` (track ×0.67, road ×0.50, highway ×0.40): foot infantry takes
1.5 days a tile on a road, 6 on a mountain, 3 on a mountain road. Measured in
`move_by_days_harness` D1–D3.

**The grant is the quarter ahead.** At each economy tick a marching unit claims the traversal cost of
the path it can walk in ninety days at its speed, capped by what its order still needs, and is granted
it whole or refused (LOGISTICS.md § How a tile's points get to it). The grant is spent a point per
unit of traversal cost as each hop is crossed, and the next tick's grant replaces what is left. An
order given mid-quarter walks on what is left of the unit's grant, and a unit with none halts until
the next tick. The credit price is charged per hop as it is crossed — `active_lp_credit_per_unit_distance`
× the hop's cost × the men — so a tile costs the same whatever the class's speed.

**Traversal cost over water is uniform.** `logistics::tile_traversal_cost` prices land by
landform, and water has no landform to price — so a sea leg costs the same per tile wherever it
runs, and distance alone sets its length. The thing that varies over water is *legality*, not
cost (§ Domains and traversal), which is the cheaper model to reason about and the one that
cannot be tuned into meaninglessness.

**Visit order — NR-344, "war flips the queue".** At peace, convoys claim the network first:
`advance_convoys` runs in the sim loop *before* `run_economy_step` is called (`main.cpp`/`app.cpp`),
a stated rule rather than an accident of phase order. A nation party to **any** declared hostility,
either direction (`is_hostile` checked both ways against `corp_hostile_pairs` — being attacked
mobilises too), has its own units visited **first** within `run_unit_march`, evaluated once per
tick from the stance table. The contention this decides is over the Logistic Points pool
(BL-464, Logistic Points — [`LOGISTICS.md`](../economy/LOGISTICS.md)); absent a shared pool, the
order is written down and changes nothing observable.

**The blackboard export shows a nation its own force.** `export_corp_blackboard`
(`src/world/corp_ai.cpp`) emits `unit_tile`, `unit_type`, `unit_count`, `unit_strength` and
`unit_order_state`/`unit_order_dest` facts for the nation's OWN units — no BL-068 visibility
question, exactly like cash and buildings. Rival units carry no unit-subject facts in the
export. The rival scorer can legally issue `march_unit`; scoring march *quality* is BL-450's
(rivals score stance) conflict-AI arc.

---

## Battles

`src/world/battle_system.{hpp,cpp}` (BL-467, battle state in world) is both halves of making
force meet stance: it discovers that a fight should happen, and it owns the fight between ticks.
Both run **every day**, in the daily military pass (`run_military_day`, economy_system.hpp): the
fronts issue their orders, then `run_battles` steps every live battle a day's rounds, then
`run_unit_march` walks the day's marches and opens the battles their contacts make. A unit in contact
fights today before it can march out. The pass runs from the app's day loop between economy ticks,
and every driver that steps whole quarters walks the same days in the economy step's own catch-up
(`advance_quarter_to`, one cursor with construction's days). What stays in the quarterly step is
economic: musters, refits, the Logistic Point grant, out-of-contact recovery (it follows the
quarterly supply factor), and `run_unit_upkeep`, whose orphan cleanup is what disbands a unit a
battle reduced to zero — such a unit stands inert, out of every fight, until then. There is
deliberately no second disband mechanism.

**Three rulings** (Ben, 2026-08-19):

1. **Footprint is the defended tile (Ben, 2026-10-27, overturning the 2026-08-19 province
   envelope).** A battle is fought over one tile: the tile the defender stands on, which the
   attacker tried to step onto. The province envelope let a line of units along a province boundary
   make attack impossible; contact is now a move, not co-presence. The attacking stack fights from
   the tile it stands on, the defending stack on its own, and **adjacent stacks join (Ben,
   2026-10-27)**: each side's own nation's units on tiles adjacent to the defended tile, out of
   every other battle, are drawn in on their side; an ally's stack beside the tile does not join. The
   defending stack on the tile defends whatever else it is fighting. Units never abstract into a pool — they keep their tiles.
2. **Terrain is one tuple for the whole battle**, exactly as the resolver takes it. Per-unit
   terrain was offered and declined. The ground is the **defended tile** — `battle_ground`,
   exposed as a named function so it can be asserted.
3. **Pacing is by the day (Ben, 2026-10-28).** Battles step every day, and contact is checked every
   day — the day a march meets a hostile stack is the day the battle opens. The round's damage is
   calibrated to that day grain.

**The trigger.** A battle opens when a unit whose nation holds a **directed** hostility toward
another tries to march onto a tile that nation's units stand on (§ Marching). Directed suffices —
the declarer attacks, the target is drawn in regardless of what it thinks. Standing next to an enemy
starts nothing; the trigger is a move, not proximity, and a defender never opens a battle by
sitting still. Friendship
permits what BL-549 (friendship permits two things) names; it gates nothing here. A
retired second trigger, which opened over an active mercenary contract rather than a declared
hostility, is § Nation
garrisons, below.

**Identity is (tile, attacker, defender)**, the tile being the defended one. A nation pair fights at
most one battle per tile at a time; two attacks onto one tile in one tick open it once, in the
direction whose attacker sorts lower — a property of the ids, not of who moved first. A third
nation attacking the same tile opens its **own** battle rather than joining. **Membership is snapshotted at
open** — a unit marching in after the first round reinforces nothing — and the participating unit
ids are stored ascending per side.

**Who can fight.** A battle opens only where the attacker has a unit with readiness left and an
attack to make; the defender need only have men standing — spent defenders are caught and overrun.
Supply, morale and quality reach the round per unit (§ The campaign force model), so an unsupplied
unit is weaker with no second mechanism. The round reads no doctrine and no season.

**What makes it deterministic.** The containers a battle is discovered from are unordered, so every
stage recovers order explicitly: candidates are sorted by (tile, attacker, defender) before any
battle opens; `world::battles` is kept in that order (`battle_order_less`); unit ids are ascending;
the terrain pick is an argmax broken by tile id; and no stage consumes a draw outside
`step_campaign_battle`. `state_hash` folds the battle list.

**Per-unit hits.** The round kills men across a side's fighting units in proportion to their men —
floor shares, then the remainder one man at a time in ascending unit id, integer arithmetic
throughout — and spends each one's readiness by its share of the hits. The pass copies the men and
readiness back to the units, so **conservation is exact**: men removed equal the losses the pass
reports. Losses land per round batch, so a battle weakens units while it runs, which is what makes
a mid-fight withdrawal a real decision.

**Retreat.** A unit spent to readiness 0, and every unit of a side that withdrew, steps one tile
away from its enemies — to a neighbour a march can reach, land, never a foe's tile, preferring
ground outside the province and then its own nation's — loses morale once, and leaves every battle
it was in. A unit with nowhere to go stays put, out of the fight. The side's strength reading is
then re-read over the units still in it.

**Withdrawal** is `request_withdraw(w, actor, tile, against)`, the battle named by its defended
tile. `against` names the fight, because a nation can be in more than one battle over one tile;
`null_entity` means "the first, in sorted order". The request is **honoured at the day boundary**, before the next day's rounds — that delay
*is* the withdrawal window. Strength is captured before the withdrawal is applied, so the parting
cost reaches the men and not only a number. A request naming no live battle, or from a
non-participant, returns false and mutates nothing — the seam rule for an AI-facing boundary.

**The field.** A concluded battle names `field_held_by` — the side that stayed — and is erased at
the end of the tick. **When the attacker holds the field, the stack that attacked advances onto the
defended tile (Ben, 2026-10-27)**; adjacent stacks that joined stay where they stood. The advance
never puts two nations on one tile: a defender with nowhere to retreat stays put, and the attacking
stack then stays where it stood. `world::province_holder` is what tracks *who currently holds a province*
across battles (BL-569, province holder): seeded at generation from the territorial plurality,
moved by a decisive battle's `field_held_by`, and left untouched by a stalemate. **A won battle
moves the holder only when the beaten side has no unit left anywhere in the province (Ben,
2026-10-27)**: a battle is one tile, so beating one stack takes that tile, and the province changes
hands when its last defending stack falls. It is deliberately
narrow — a military fact a contract predicate can read, not a political one: `tile_to_nation` (the
territorial map) does not follow the holder, so what holding a province does *beyond* that fact is
still unowned work. Battle state survives a save under BL-536 (world snapshot save).

### Dispatches and the card

A battle **reports**. `battle_tick::dispatches` carries one pure-data `battle_dispatch` per battle
that acted this day — the defended tile and its province, sides, phase, rounds fought / today / max, both strengths and
swings, men lost, and for an ending the `withdrew`, `end` and `field_held_by` fields. It includes
battles that **concluded** that day and are already erased from `world::battles`; that is the only
way the aftermath — who held the field, what it cost — can reach a surface at all.

**`battle_phase`** is derived **once, in the world layer** (`read_battle_phase`), because two
surfaces consume it and they must never disagree. Rounds are mechanically uniform; the phase is a
*reading*: `opening` (the first batch), `committed` (after it — a statement about the withdrawal
price, since by round 4 the per-round term alone has tripled), `crisis` (a swing of 150 or more
across the batch, either side), `wavering` (a side's readiness at or below `wavering_readiness`, read
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

> Nations field force of their own. A garrison is the standing force a nation's ground is held with.

Nations own units — every unit (Ben, 2026-10-23). `unit_component::owner` takes a nation entity;
the field was never corp-typed. A garrison is seeded at generation, not hired: one in the nation's
capital province, plus one in each border province it shares with its highest-grudge neighbour
(`nation_score_terms.mean_grudge`). **Where each stands (Ben, 2026-10-03):** the capital
garrison on the capital province's **anchor centre** — its chief city, the tile carrying the
province's highest summed centre scale — not on the nation's capital seed tile, which need hold no
centre at all. A border garrison stands on its province's tile **facing the neighbour it guards**:
the member tile with the most hex sides onto that neighbour's ground, ties to the lowest tile id. A
garrison is read as border force only through its own tile's neighbours, so one posted deep inside
its province would guard nothing. A capital province that is also a border province takes the
capital rule. Neither post is the province's lowest-id tile, which is its id rather than its place
(BL-1145, a province's readers take its anchor).

**Every capital is generated with a military base (Ben, 2026-10-27).** Each nation, major or minor,
opens owning one completed `military_base` on its own ground in its capital province, on the legal
tile nearest its capital garrison: one `can_place` accepts, an empty tile before one the stacking
rule still has room on, ties to the lowest tile id. A capital province with no legal tile takes the
nearest legal tile of the nation's own ground anywhere; a nation with no legal tile at all opens
with no base. The capital garrison records that base as its `muster_base`, so the nation can hire
from its first day and the orphan rule (§ Upkeep) holds the garrison to its base. A border garrison
records none. The opening unit (`seed_starting_military`) musters at the same base, so a nation
opens with one base, never two. A seat moved at Begin keeps its capital base and its garrisons and
loses only the unit raised there. A garrison is told from a raised unit by its id: it was posted
before its base stood and adopted it; every hire is created after the base it musters at.

This joins the existing three unit writers (§ The unit model)
as a fourth, generation-time source, alongside the hard-coded stub, `seed_starting_military`, and
`hire_unit`.

**Sizing is treasury-scaled against the value anchor.** BL-543's (value anchor) unit-equipment-
costs-twice-annual-salary anchor prices a garrison's count off the nation's own
`nation_component::treasury`, so a rich nation fields a bigger standing force and a poor one a
token one — no separate garrison-strength dial. A garrison holds its seeded tile until it is
assigned to a front (§ Fronts). A rival assigns its border garrisons and keeps its capital's home.

**There is ONE battle trigger: nation-vs-nation directed hostility** (§ Battles, above).

There were two. The second opened a corporation-vs-nation fight automatically over an ACTIVE mercenary
contract — no `declare_hostile` row was authored for it, because the contract itself was the
hostility. It went with the mercenary contract (Ben, 2026-08-30), and it had **never fired**: its
lookup returned nothing from every production state, because the contracts that would have
populated it were never created.

**A garrison is fought through declared stance.** `open_battle` is entity-id-agnostic, and with
stance between nations (Ben, 2026-10-23; [`../politics/RELATIONS.md`](../politics/RELATIONS.md))
a garrison is drawn into battle whenever its nation is the target of a directed hostility in its
province. Hostility is a state a party opts into (Ben, 2026-08-17), which is exactly the shape the
retired trigger bypassed.

**Upkeep is a budget claim, not the hired-unit vector.** A garrison does not draw the credits+goods
vector `run_unit_upkeep` charges hired units (§ Upkeep, below) — its upkeep is a `military_research`
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
wage plus a set of `{resource, qty}` draws bought off the shelf of the market the unit stands in,
at the posted price under the fair-price ceiling. When the goods do not arrive the unit **weakens**
rather than vanishing.

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
measured baseline rather than guessed. At zero nothing is charged, no shelf is drawn, no draw can go
unmet, and no supply factor moves; turning upkeep on is editing this table, not a code change.
The numbers are anchored by BL-543 (value anchor) — a unit's annual equipment costs twice its
annual salary (Ben, 2026-08-22, [`NATIONS.md`](../politics/NATIONS.md)) — and the ordnance rate
rides battle consumption (BL-496, ordnance rate; NR-321). `tools/verify/unit_upkeep_rates.cpp`
is the measurement: it sweeps candidate rate sets and prints the wage bill against income, the
goods draw against production, and the supply curve.

**Two halves, two homes.** The credit half is its own term in `apply_budget`
(`src/world/budget_system.cpp`, surfacing as `corp_budget::upkeep`). The goods half is a shelf
draw in `run_unit_upkeep` (`src/world/economy_system.cpp`, `draw_goods_or_bid`), bought off the
market under the unit and billed by the clear as expenditure.

**Ordnance is the good, and it is authored as data.** `resource_type::ordnance` is the roster's
first terminal military good (BL-457, ordnance), added so this draw has something to consume.
Naming a good here is a one-line `economy.lua` change, never a code change.

> **The ordnance recipe is industrial.** The Fabricator recipe that makes ordnance is tagged
> `era = "industrial"` (`scripts/recipes.lua` id 27). Every nation opens on the industrial band
> (`../economy/ERAS.md` § Where the ladder starts; Ben, 2026-10-10), so the good every unit's upkeep
> draws is one every nation may make.

**One decay rule, two triggers.** A unit is unsupplied if it sits beyond the reach field *or* its
goods draw went unmet. Both fire the **same** subtraction on `supply_factor_permille`, because both
mean the same thing — an army that is not being supplied gets weaker. It is deliberately not two
rules. The subtraction stops at the unit's **supply share**: zero out of reach, and on a short draw
the mean over its goods of the fraction met, in integer per-mille (BL-1172, fair-price upkeep). A
unit fed half its draw hits at half strength, since the land round scales each unit's hits by its
supply factor, and its morale drifts toward it.

The reach trigger is **opt-in**: a non-positive `out_of_supply_reach` disables it *and* the
Dijkstra behind it, so a zero table costs no time as well as no credits. The test is
`!(rc <= limit)` rather than `rc > limit`, so infinity and NaN both read as out of supply.

**Determinism.** The pass visits units in **ascending entity id**, sorted out of the unordered
`w.units`, because two units drawing on one shelf draw the same stock — the visit order
decides which one goes short. That is load-bearing, not cosmetic.

**Orphan cleanup.** `demolish_building` erases the building, its owner's asset record and the stockpile but
never touches `w.units`. The upkeep pass disbands a unit whose `owner`, tile, or `muster_base` has
been erased, and a unit a battle reduced to zero. It runs **last** in the economy step, so a base
torn down this tick orphans its units in the same tick rather than leaving them live for one. It
reports the count as `unit_upkeep_tick::disbanded`.

`muster_base == null_entity` means "no base recorded" — the hard-coded world's stub unit, or a
border garrison — and is never orphaned. A capital garrison records its capital base (§ Nation
garrisons), so it is held to that base like any unit raised there.

**Supply is measurably real in the resolver, not merely expensive.** The land round reads each
unit's `supply_factor_permille`, so an unsupplied army is weaker where it counts.

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
`combat.hpp` + `components.hpp` alone — **R4 inverts with the water model**: what it must now
assert is that a naval entry contributes REAL power, that an all-naval stack resolves without
dividing by zero, and that the matrix row previously marked *unused* is exercised. The subject it
asserts over is
[`../generation/MILITARY_HISTORY.md`](../generation/MILITARY_HISTORY.md) § The resolver, since
`resolve_battle` is that document's.

**`campaign_battle_harness.cpp`** — `resolve_campaign_battle`. C1: replay determinism including the
per-round trace, stepping matching the scripted wrapper, and the seed genuinely folding from the
identity (a different tick, and a role swap, both give different streams). C2: the randomness
reaches the outcome and a fight inside the band is uncertain. C3: withdrawing later usually costs
more men, a later exit only ever gets cheaper because the deficit narrowed, and a many-unit side's
charge is exactly the quoted share of its men. C4: a strength sweep that is monotone in force, with
an uncertain band, a near-unloseable top and the defender holding at parity, plus the printed
decisive curve. C5: every roster row maps to a land class; armour beats infantry in the open; a
mountain costs an attacker; hardness turns soft attack away. C6: readiness falls faster than men,
and a side of spent units deals nothing, is overrun and broken. C4's rates are written as
**predictions**, not tuned-to-pass numbers.

**`battle_engagement_harness.cpp`** — `run_battles`. B1: the trigger fires from hostility plus a
shared province alone, men die through the tick rather than a direct resolver call, and two
neutral nations sharing a province do not fight. B2: twin worlds produce the same battle set in the
same order. B3: mutual hostility and four units open exactly one battle, and a second tick opens no
duplicate. B4: province and role order each change the stream. B5: no count goes negative and
conservation is exact, per tick and end to end. B6: five ticks twice gives identical strengths and
an identical `state_hash`. B7: the battle anchors on the defender's best tile, ties on ascending
tile id. B8: a bogus or non-participant withdraw mutates nothing; a participant's request is
honoured on the following day. B9: a unit in contact reads as in-battle and `march_unit` refuses
it. B10–B11: a battle that fought emits a dispatch naming the fight, rounds/max and the stream
seed; a concluded dispatch names who held the field after the record is gone. B12: the three
withdrawal terms sum to the total and **the quote matches the charge**. B13: twin worlds emit
byte-identical dispatch sequences and stay identical. B14: every phase produces a line. B18: a spent
unit retreats one marchable tile and leaves the fight, losing morale. B19: the winner gains morale,
the loser loses it. B20: out of contact readiness climbs toward morale, never above it, and morale
moves toward supply either way, a quarter's recovery at a time. B21: a world of fully supplied units
at full readiness and morale hashes as it did; an unsupplied one moves. B22: a unit in two battles
retreats once and pays the morale loss once; the daily pass recovers nothing, the quarter's recovery
one step.

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
rejects not-owner/invalid/ocean-dest/off-body/already-there without mutating the world. M2: a day of
`run_unit_march` spends the class's daily movement against the shared traversal-cost weight, carries
fractional progress from day to day, and clears the order on arrival. M3: `halt_unit` clears the order
and rejects a unit with none; `disband_unit` erases the unit and rejects a non-owned/unknown one.
M4: NR-344 — a mobilised nation's units are visited before a peaceful nation's within one day's pass.
M5: state-hash replay determinism with marching units in flight. M6: a corrupted path (the
recompute guard's trigger) recomputes identically across two independent runs.

**`move_by_days_harness.cpp`** — the day grain, against the authored tables. D1: each land class's
days per plains tile measured over twelve tiles (3 / 1 / 1 / 1 / 1.5 / 2 / 1.5), and the pre-modern
rows by `cls`. D2: a mountain doubles the days, a road halves them. D3: progress banks from day to
day. D4: the grant is the quarter's path, spent a point a tile with the credit price charged per
tile; a spent grant halts the unit until the next grant resumes it. D5: contact opens the battle on
the day of contact. D6: battle length in days by force ratio. D7: twice-equal. D8: a save mid-march
round-trips and marches on identically. D9: a quarter walked day by day and one caught up by the
economy step hash alike. D10: the loader rejects a non-positive, non-finite or unknown entry.

**`rival_military_seeding_harness.cpp`** — `seed_starting_military`. R1: every seeded owner
ends generation owning exactly one `military_base` and one unit (the modern infantry row, 50
manpower); its
corporation-and-HQ reading retired with the corporations (Ben, 2026-10-23). R2: two same-seed runs
seed identically.

**`capital_military_base.cpp`** — every capital's base, over the library seeds on the shipped
start. C1: every nation owns exactly one completed `military_base`, in its capital province, on its
own ground, on a tile `can_place` accepts. C2: it stands on the road network. C3: the capital
garrison records it as `muster_base`; no border garrison records one. C5: a picked seat keeps its
capital base and garrison and loses the unit raised there; the seat it left is armed at its own.

**`stance_determinism.cpp`** — the stance tables that gate all of the above replay identically.

Adjacent coverage: `buildings_rework_harness` R6/R7 for `military_base` placement, zero staffing
and the not-an-anchor property.
