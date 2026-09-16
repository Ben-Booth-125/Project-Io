# Project Io — Military history

> **Settles:** how force works **inside the Era −1 sim**, a generation pass and not the game
> — how `resolve_battle` settles a war at nation scale · why an army is a pool distinct from
> the population that raised it, and what that makes an undefended region ·
> how a polity's roster advances up
> the band ladder as its institutions do · what domain a region carries, and what it gates ·
> how naval is scored and exercised here · what forage simplifies, and what it is a
> simplification *of* · how sea legs produce colonisation, and where a Settle may found.
> **Not here:** how force works in the **campaign** — `resolve_campaign_battle`, muster and
> hire, march, upkeep, and the roster table itself — all `../military/MILITARY.md`, a
> **sibling and not a parent** · why a polity chooses to fight, and how it falls (CIVILISATION).
> **Confused with:** `../military/MILITARY.md` above all — the two resolvers are constantly
> mistaken for one another; also ../lore/HISTORY.md, CIVILISATION.md.

**How force works INSIDE THE ERA −1 SIM**, which is a generation pass and not the game. This
document owns the ancient half of the military model: the nation-scale resolver the sim runs
millions of times, the band ladder a polity climbs as its institutions do, what the sim does with
water and ships, and the overseas reach that produces colonisation.

[`../military/MILITARY.md`](../military/MILITARY.md) owns the **campaign-era** model — the model
the player meets. It is a sibling, not a parent.

**Why this split exists (Ben, 2026-09-06).** These answers *"apply specifically to generation, and
more specifically to ancient history"*. Rules written for a 2,000-year sim that resolves a war in
one scored evaluation are not rules about how war works in Io; they are the simplifications that
make a history cheap enough to generate. Keeping them in `MILITARY.md` made them read as claims
about the game — and one of them, a fleet starving where it sits, is not a claim anyone would
defend about the real thing. **Historically, nations supplied overseas perfectly well.**

**This document lives in `docs/generation/` on purpose.** It is a generation doc that happens to be
about force, not a military doc that happens to be about the past. Its neighbours are
[`../lore/HISTORY.md`](../lore/HISTORY.md) — the institutional ladder that drives the sim — and
[`CIVILISATION.md`](CIVILISATION.md), how an empire rises, reaches and falls.

---

## The boundary, stated once

| Question | Here | `MILITARY.md` |
|---|---|---|
| Which resolver | `resolve_battle` — region beats region, this year, in one evaluation | `resolve_campaign_battle` — two forces in a province, over rounds, with withdrawal |
| Which roster gate | `available_rows(const region&, band)` — authored region endowment | `available_rows(const world&, corp, band)` — the corp's stockpile and market access |
| Naval | **Scores, and is exercised here** | **Unmodelled** (§ Naval) |
| Supply over water | A simplification, applied by this caller (§ Forage) | Overseas supply works; there is no starvation model |
| Sea legs, colonisation | Owned here | — |

**What is shared, and must stay single-sourced: the roster TABLE and the calibration.** The
nineteen rows — their bands, classes, gates and weights — live in `src/world/unit_roster.{hpp,cpp}`
and are described by `MILITARY.md` § The roster. **Two gate paths read one table.** This document
owns one of those paths and never a second copy of the table.

Likewise `terrain_combat`: both resolvers read the same terrain tables, which is exactly why a rule
that applies to only one of them belongs in a **caller** and never in the table (§ Forage).

---

## The resolver — `resolve_battle`, at nation scale

`src/world/combat.{hpp,cpp}` (BL-272, unit/doctrine combat model). A pure function of its inputs:
matchup × doctrine × terrain × supply × season. No RNG, no world reads, no hidden state.

Its callers are `src/world/history_sim.cpp` — the Era −1 history sim (BL-271, Era −1 sandbox) —
and, per round, `campaign_battle.cpp`. The campaign layer never calls it directly;
`tools/verify/combat_harness.cpp` calls it to assert.

Arithmetic is **integers in per-mille throughout** (1000 = neutral). Battle outcomes move borders
in the sandbox, so no float decides who wins.

**Inputs.** Two `std::vector<army_stack_entry>`, two `doctrine_row`s, a terrain triple
(`terrain_substrate` + `terrain_cover` + `cover_density`) plus a `terrain_landform`, a `season`,
and two supply values 0..1000 (clamped). Nothing is rejected — an empty stack resolves rather
than erroring.

> **The degenerate case that follows from "nothing is rejected", and what guards it.** A stack
> that scores zero power still resolves, and the victory test is a strict `>`, so a fight where
> BOTH sides score nothing resolves as a **defender victory with 400/200 per-mille losses** —
> casualties inflicted on forces that scored no power at all, in a shape indistinguishable from a
> real outcome. The engagement trigger opens a battle on stance and position alone and never
> inspects unit class, so nothing downstream would catch it; `battle_system.cpp`'s
> `stack_can_fight` screens both stacks before opening.
>
> **An all-naval stack is no longer one of these cases**, and the change of status is worth
> stating rather than absorbing. It used to be degenerate *by construction*, because the class
> scored zero; under `MILITARY.md` § Domains and traversal it scores like any other, so an all-naval fight is
> an ordinary fight — the rare one the water model exists to make expressible. What the guard
> still covers is the genuinely empty stack, which is a caller error in any era.

An **`army_stack_entry`** is one unit type's contribution, already reduced to numbers:
`{type_id, cls, count, type_power_mod}`. It is deliberately **not** a lookup key into a roster
table — the engine scores whatever stack it is handed and does not know which era it is refereeing.
`type_id` is carried for the caller's bookkeeping and never interpreted.

**Unit classes** are five and coarse: infantry, cavalry, ranged, siege, naval. Base power per unit
is authored in one table, and **naval is authored there like the other four** — it is a fighting
class, not a tag.

**Naval scores, and it is the only class that may hold water.** A naval entry contributes power and
weight to the matchup average exactly as a land class does. What separates it is not its arithmetic
but its *domain*: naval rows are the only rows that may occupy open ocean, and the only rows that
may contest coastal water a rival holds
([`../military/MILITARY.md`](../military/MILITARY.md) § Domains and traversal).

> **This overturns "naval is strategic-only", which this section asserted until 2026-09-06.** The
> earlier reading gave the class zero power and zero weight, and the matchup matrix carried a row
> marked *unused*. That was coherent while water was a wall — there was nowhere for a fleet to be,
> so there was nothing for it to do. Ben's water-domain ruling makes water a place, and a class that
> is the sole occupant of a place has to be able to fight over it.

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

---

## Armies are distinct from population (Ben, 2026-09-08)

**Ben's ruling, in his words:** *"population as a civilian thing — where armies are distinct
from population, and we don't simulate total warfare in stage 4."* BL-835 (civilian population,
armies apart) owns the design.

**Population is civilian.** It moves on demography, habitability, famine and plague. It does not
move for war — not for a battle, not for a sack of the countryside, not as an ambient drawdown
under war pressure. A conquest is a change of flag over the same people.

**An army is a separate pool**, held per region as `region::army_stock`. Three quantities sit in
a line and each answers a different question:

| Quantity | Answers |
|---|---|
| `population` | How many people live here |
| `manpower_stock` | How many of them could be called up — a bounded fraction of the population, refilling slowly |
| `army_stock` | How many are under arms **right now**, standing on this region |

**Raising an army costs manpower, not population.** The muster draws from `manpower_stock`,
which refills off a population war never touched — so an army destroyed is rebuilt over
decades, through two stages that each move a fraction of their own gap per year. Discharged
soldiers return to the pool, never to the civilian count, because they were never subtracted
from it.

**A region's defence is the army standing on it**, plus the levy it can call up in the year it
is attacked. Both the scorer's estimate and the battle itself read the same rule — the standing
trap in this layer is a cost authored on one scale and paid on another, so the estimate is the
mutation asked as a question.

**Therefore an undefended region is NORMAL and TEMPORARY.** An army marched away; a levy was not
raised; a garrison was broken last spring. It is not a property of dead ground. Walking in is
cheap exactly once, because the army that walked in is then the army standing there — a region
that has just changed hands is the best-defended province on that frontier, not the worst.

> **This replaces a model in which population WAS the army, and the replacement is a root fix
> rather than a refinement.** Under the old accounting a battle killed civilians in both
> regions, a conquest sacked a fifth of the countryside, and defence was read off the manpower
> a population could support. Those compound: a region taken and retaken empties, an empty
> region can field nothing, and a region that can field nothing outscores every real objective
> on the map for the rest of the run. It is not a tuning failure. **Measured on the seed-0
> fixture: 258 battles and 258 conquests over four thousand years, all of them the same
> region.** `battles == conquests`, exactly 1:1, is the signature.
>
> Three patches were considered and are superseded: a no-battle-target flag, a release rule, and
> a value floor. Each blocks the symptom. Under a civilian population that war does not consume,
> the empty region is never produced.

**And it gives the culture shares their subject back.** Conquest transfers *people*, and the
people transferred are what the assimilation pass then digests. A region emptied by the taking
had nobody to assimilate, which is how conquest had become free of the cohesion cost that lever
depends on.

**What it asks of the scorer.** Question A sharpens from *can I take this* to **can I keep an
army there** — which is what makes logistics and roads load-bearing rather than decorative. A
campaign gathers a pooled levy from every held region the capital can supply (§ Force follows
the network below), so a realm brings as much of itself to a border as its network can carry;
those regions are genuinely uncovered while the campaign runs, and the survivors have to be
somewhere. An offensive is a bet made with a finite object that can
be in only one place.

**The collapse path survives, in the shape that was always the legible one.** A sack still falls
on the walls: `sack_region_urban` razes centres and records every one it took, so a sacked city
reads as smaller or absent on the epoch map and still says it was sacked if it regrows. What is
gone is war as a demographic event.

**And that is the settled position, not an implementation consequence (Ben, 2026-09-09).** The
question was put explicitly, because removing the countryside sack and the ambient war-pressure
drawdown leaves **plague as the only force that lowers a civilian count** — which is a larger claim
than the ruling above literally made. The answer is that war stays non-demographic and only the
sacking of walls touches people at all. Armies die; farmers do not. If a war should ever shrink a
population, it does so through a **famine or displacement mechanism of its own**, authored as such
and visible as such — never as a coefficient hidden inside a battle. A battle that quietly killed
farmers is exactly the accounting this section replaced.

**Digitisation takes that exception, and only by that route (Ben, 2026-09-15):** *"we really need
to ensure that people die when wars happen."* Industrial war kills through named, visible
mechanisms — conscript dead drawn back from the regions that raised them, displacement toward safe
cities, and famine under interdicted supply — and never by a battle coefficient, so no region is
emptied by being fought over. The ruling above is unchanged for the Empire and Exploration spans
(`DIGITISATION.md` § War kills people, and it is bad for everyone).

---

**FORCE FOLLOWS THE NETWORK (Ben, 2026-09-11).** A campaign's stack is not the staging hub's own
garrison plus what its immediate neighbours can spare. It is a **pooled levy**: every held region
the capital can supply contributes a share of its garrison, in proportion to its supply as read
from the capital and discounted by the roads between, so a realm with a network brings more of
itself to a border than one without, and a realm of many regions fights a larger battle than a
realm of three. The defender's emergency levy stays local. The regions that contributed are
genuinely uncovered while the campaign runs, and a bigger stack eats more upkeep, so the pooled
levy is a decision with a cost rather than a free accumulation. This is what lets success
compound into an empire and, when the network fails, lets failure compound into a collapse.

## The band ladder — how a polity's roster advances

`available_rows(const region&, band)` is the Era −1 gate path: what a polity may field is derived
from **authored region endowment** on four 0..1000 axes — `ore_q`, `farm_q`, `port_q`, `energy_q` —
and from the band its institutions have reached.

**Asymmetry is the point** (BL-274). Two polities at the same date field different rosters because
their *ground* differs, which is the function that makes belief-and-environment-onto-war visible in
a generated history. A polity on ore-poor ground fields levies in the same century its neighbour
fields heavy infantry, and neither is a handicap applied to an agent — it is the map.

**Bands are cumulative, because nothing un-invents a spear.** A gunpowder polity still fields
infantry; an industrial one still fields levies where its ground cannot pay for better. Later bands
crowd earlier ones out by weight rather than by exclusion — a row's weight decays per band it sits
below the polity's own — so a rifle-era army is not half levy spears.

**What advances the band is the institutional ladder, not a military score.** That ladder is
[`../lore/HISTORY.md`](../lore/HISTORY.md)'s subject, and this document reads its output rather
than defining it. The distinction matters: a polity does not reach the gunpowder band by fighting,
it reaches it by becoming the kind of polity that can sustain gunpowder.

**The unrestricted ladder is deliberate.** Capping how far a polity may climb inside the sim was
considered and declined (2026-08-24) on the ground that it would move every 0 CE world for a
constraint nobody had asked the generated history to honour.

---

## Regions carry a domain (Ben, 2026-09-06)

**A region carries its domain**, the same three-way split the province layer already uses and
for the same reason: a domain is a fact about ground, and the sim is the first pass that acts
on it. `region::domain` is the real `is_sea` test — exclusive by construction — and it is what
any coastal gate reads. [`../lore/HISTORY.md`](../lore/HISTORY.md) owns the sim's polity loop
and its five scored verbs; this document owns what the domain gates.

---

## Naval — real here, and nowhere else yet

**Ben's ruling was framed at this grain (2026-09-06):** *"We can also use coastal units in the
ancient sim alongside other unit types. This allows the rare case of naval warfare and trading
provinces."*

So the naval class scores real power **in this sim**, and campaign-era naval stays **unmodelled** —
not as an oversight, but because nothing in the campaign has asked for it and a model with no
consumer is a model nobody has tested.

**They are not a separate system.** Coastal Galley, Broadside Ship and Ironclad are rows in the
same roster, raisable on the same `port_q` endowment axis as every other row, scored in the same
contest by the same resolver. What distinguishes them is **domain**: they are the only rows that
may occupy open ocean, and the only rows that may contest coastal water a rival holds
([`PROVINCES.md`](PROVINCES.md) § Who owns water). Until the water ruling the class returned base
power 0 and was skipped by the stack sum outright — authored, raisable, and worth nothing.

**`port_q` says whether ships can be RAISED; `region::domain` says whether they may be
THERE**, and the distinction is stated because an earlier draft ran the two together. `port_q`
is not sea access: `survey_endowment` counts `is_water` tiles — **lakes included** — over a
neighbourhood window, and a region founded by the Settle verb inherits 0.7× its parent's
without ever re-surveying. So it is a decayed wetness fraction, and a landlocked region beside
a big lake can carry more of it than a genuine harbour. The endowment axis is what
`available_rows` reads; the domain field is what any coastal gate reads.

**A stack fields ships only where the campaign CROSSES WATER (Ben, 2026-09-07).** An inland
objective composes no naval rows at all; they are dropped before the weighting, so the land rows
divide the whole manpower rather than sharing it with a fleet that cannot be there.

> **This section said the opposite until 2026-09-07, and measurement is what settled it.** It read
> *"composed into the same stack"* — one roster, one composition, no filter — which was written
> before anyone could see what one stack produces. `roster_stack` composes **every available row**,
> so any polity clearing `port_q` carried a galley contingent into landlocked fights. Harmless
> while the class scored zero; once it scored 140 it became a **combat bonus for owning a port**,
> applied inland, with no cause anyone could point at in the world.
>
> The measurement: **84% of battles carried a naval contingent**, against the **18%** that actually
> reached over water. "Rare is the design" and 84% is not rare. With the filter the two figures
> converge — 273 of 1500 carry ships, 274 of 1500 cross water.
>
> **The one-battle gap is the model working, not a rounding error.** A sea leg with no naval
> contingent is a polity crossing coastal water *it owns* with land units — the middle case of
> `MILITARY.md` § Domains and traversal, showing up in the data on its own.

**Rare is the design, not a shortfall.** Most contests stay on land. The water model exists so the
uncommon ones — a contested strait, a coastal province changing hands, a trade shore denied — are
expressible *at all*. A sim in which sea battles were routine would be generating a different
history, so `history_sweep`'s count of how often naval combat occurs is a **calibration reading**,
never a coverage target to raise.

---

## Forage — the simplification, and what it is a simplification OF

**In the sim, a force forages where it is adjacent to water or land its own polity owns, and
starves where it is not.** That makes overseas reach a question of distance from home, which is the
property the sim needs: it is what stops a polity conquering across nine tiles of open ocean for
free, and it is cheap enough to evaluate inside a walk that runs millions of times.

**It is NOT a claim about how supply works.** Nations supplied overseas expeditions perfectly well
— that is most of what a navy is *for*. The rule is a stand-in for a logistics model the Era −1 sim
deliberately does not have, and it should be read the way every other constant here is read: the
cheapest thing that produces a believable history, not a mechanism anyone would defend on its own
terms.

**Which is why it lives in the CALLER, not the terrain table.** `terrain_combat` returns 0 defence
and 0 forage for every water kind, and **both** resolvers read it. Expressing this rule by editing
that table would silently impose an ancient-sim simplification on the campaign, where the opposite
is true. The sim applies the rule; the table stays as it is.

> **The trap this avoids, recorded because it nearly happened.** The rule was first written into
> `MILITARY.md` § Domains and traversal as though it were general. It reads plausibly there — *a
> fleet parked on a rival's shore cannot feed itself* is a nice sentence — and it would have made
> the campaign-era model assert something false about supply, in the document that owns what the
> player meets. A simplification is only honest where it is labelled as one.

---

## Sea legs and colonisation

**This document is the authority for the overseas campaign** (BL-749, sea leg campaign) — the item
that produces *the extent of colonisation by major powers*, which is half of what a generated
history is for.

**The premise to correct before building it, because it inverts the item** — measured, not assumed
(`tools/verify/sim_water_census.cpp`, 2026-09-03, three seeds of generation's own era). A sim
without a water model does not *lack* overseas reach; it has too much. With region adjacency a
**water-blind Chebyshev radius**, a polity campaigns across up to nine tiles of open water for
free: **43% of the sim's adjacency edges crossed sea** at `neighbour_radius` 9, which means nearly
half of every campaign target was already across water, reachable with no harbour and no fleet.
The Settle verb applied no terrain test either, and `terrain_combat` returns 0 defence on every
water kind — so a region founded there was silently undefendable. **1105 of 3819 regions sat on
water at their anchor**, about 29%, of which **613 on open ocean**. Under the ownership rule
(`PROVINCES.md` § Who owns water) those numbers split rather than being deleted wholesale — the
~492 coastal and lake regions are legitimate owned shoreline, and only the 613 anchored on open
ocean have no owner to belong to.

The consequence is worth stating plainly: the "before" figure for any sea-leg measurement is **not
zero**, and every tuning constant in `history_sim_params` was measured with free overseas conquest
happening.

**What prices it is a LEGALITY test, not a penalty.** Under `MILITARY.md` § Domains and traversal a
land stack cannot enter unowned coastal water or open ocean at all, and may cross only coastal
water its own polity owns. That is cheaper to reason about than a supply penalty and it cannot be
tuned into meaninglessness. Naval rows carry force across everything else.

**The Settle verb's terrain test is a domain test.** Not "no water" — the ownership rule makes
coastal founding legitimate — but no founding on open ocean, which has no owner to found under.
That is the smaller and better-founded half of the blanket "no water" test the ownership rule
replaced: ~613 regions anchored where nobody can own them, not the ~1105 sitting on water at all.

**Five calls remain open**, and none should be guessed: which walk the range is measured along and
how the four harbour rows map onto it; where the walk is anchored, since a region has no harbour
tile and adding one is a save-format change; how *holds a harbour work* is tested, since `work_row`
has no harbour flag; whether an overseas daughter region re-surveys `port_q` or inherits a fraction
of its parent's; and what the four harbour ranges actually are.

---

## Verification

**`sim_water_census`** is the instrument this document is judged against — regions on water, the
stored `region_domain` against the substrate, and the share of adjacency edges crossing sea. Run it
either side of any change to the carve, the partition, or traversal.

**`history_sweep`** reports what the sim produced: years, battles, conquests, foundings, and how
often naval combat occurs.

Both are named in the `verifier-headless` skill. A change here **moves the generated world**, so it
owes the digest discipline in [`../development/DELIVERY.md`](../development/DELIVERY.md) § The
digest re-bless is one act per WAVE — report the movement, measure your own before/after in
isolation, and never re-bless on your own authority.
