# Project Io — Military history

> **Settles:** how force works **inside the Era −1 sim**, which is a generation pass and not
> the game — how `resolve_battle` settles a war at nation scale · how a polity's roster
> advances up the band ladder as its institutions do · how naval is scored and exercised
> here · what forage simplifies, and what it is a simplification *of* · how sea legs produce
> colonisation.
> **Not here:** how force works in the **campaign** — `resolve_campaign_battle`, muster and
> hire, march, upkeep, and the roster table itself — all `../military/MILITARY.md`, a
> **sibling and not a parent** · why a polity chooses to fight (../lore/COLLAPSE).
> **Confused with:** `../military/MILITARY.md` above all — the two resolvers are constantly
> mistaken for one another; also ../lore/HISTORY.md, ../lore/COLLAPSE.md.

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
[`../lore/COLLAPSE.md`](../lore/COLLAPSE.md), the polity strategies and culminating events.

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

## Naval — real here, and nowhere else yet

**Ben's ruling was framed at this grain (2026-09-06):** *"We can also use coastal units in the
ancient sim alongside other unit types. This allows the rare case of naval warfare and trading
provinces."*

So the naval class scores real power **in this sim**, and campaign-era naval stays **unmodelled** —
not as an oversight, but because nothing in the campaign has asked for it and a model with no
consumer is a model nobody has tested.

**They are not a separate system.** Coastal Galley, Broadside Ship and Ironclad are rows in the
same roster, gated on the same `port_q` axis, scored in the same contest by the same resolver.
What distinguishes them is **domain**: they are the only rows that may occupy open ocean, and the
only rows that may contest coastal water a rival holds ([`PROVINCES.md`](PROVINCES.md) § Who owns
water). Until the water ruling the class returned base power 0 and was skipped by the stack sum
outright — authored, raisable, and worth nothing.

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

**The premise to correct before building it, because it inverts the item.** The sim does not
currently *lack* overseas reach; it has too much. Region adjacency is a **water-blind Chebyshev
radius**, so a polity already campaigns across up to nine tiles of open water for free, and **43%
of the sim's adjacency edges cross sea**. It also already founds regions on ocean with no terrain
test, where `terrain_combat` returns 0 defence — so such a region is silently undefendable.

The consequence is worth stating plainly: the "before" figure for any sea-leg measurement is **not
zero**, and every tuning constant in `history_sim_params` was measured with free overseas conquest
happening.

**What prices it is a LEGALITY test, not a penalty.** Under `MILITARY.md` § Domains and traversal a
land stack cannot enter unowned coastal water or open ocean at all, and may cross only coastal
water its own polity owns. That is cheaper to reason about than a supply penalty and it cannot be
tuned into meaninglessness. Naval rows carry force across everything else.

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
