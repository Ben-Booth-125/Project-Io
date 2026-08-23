# Project Io — People

**Named individuals holding specific roles.** Io generates a world full of institutions and not one
person. This document opens that subject.

> **Status: DESIGN-OWED, written 2026-08-22 from Ben's ask** — *"we might also want to include a
> system whereby named people get specific roles."* Everything below the § Precedent section is a
> **proposal awaiting Ben's ruling**, not a settled design. § Open questions are the calls; they are
> the point of the document. Owned by BL-547.

---

## The claim

Every quantity in Io is currently held by an institution. A corporation has a balance, a nation has
a treasury, a unit has a strength. **Nobody holds anything.**

That is defensible for an economy and it becomes a real cost the moment the game has *relations*.
Sentiment ([`politics/RELATIONS.md`](politics/RELATIONS.md)) is a directed float from one
institution to another; lobbying (BL-539) is a corporation influencing a nation. Both are correct
and both are **abstract in a way that makes them hard to care about.**

> **A person is what makes an institutional relation legible.** Sentiment is a number. A person is
> who holds it.

**The rule that keeps this from being decoration:** a person exists only where a **role** exists,
and **every role must gate or bias something.** A named figure who only decorates is the
`military_points` defect with a face — generated, displayed, and read by nothing.

---

## Precedent — what Io already has

**Names are free and are already correct.** `tongue.cpp` generates per-culture phoneme systems,
`city_names.cpp` and `body_names.cpp` coin from them, and `creeds.cpp` names pantheons in the same
tongues. A person's name is one more draw from machinery that ships.

> **The standing rule binds absolutely.** Every generated name in Io — nation, province, city,
> corporation, body, **person** — is sci-fi/fantasy, produced by the seeded template banks and
> phoneme tables, **never drawn from an Earth list and never Earth-flavoured.** Real history is a
> mechanism reference, never a name source.

**Two items already sit in this space, and this document does not replace either:**

- **BL-370 (corp leader figure)** — `design-owed`, parked, from Ben's 2026-08-11 notes session: a
  named figure standing in for the player's corp, *"so the player feels a personal connection to
  the corp they play as."* That is **one role**, and it is the role this system should ship first.
- **BL-207 (persona counsel packs)** — `designed`, A, parked: advisory personas seated per
  corporation, reading the blackboard, biasing the strategy weight through a bounded, transparent
  nudge. **That is already a person-with-a-role**, fully designed, deterministic, and moddable in
  Lua. It is the strongest precedent in the repo and this design should extend it rather than
  invent a rival.

---

## Where this sits in the chain

Under SYSTEMS.md § The progression chain, people are unusual: **they are not a rung, they are the
handles on the rungs above.**

    ...institutions act...
           ↓
    PEOPLE make institutional action attributable, biased, and mortal

**What forces you in.** Nothing mechanically — and that is the honest statement. This system's
argument is not that the game stops working without it, but that **an institutional relation you
cannot picture is one you will not reason about.**

**What it opens.** Three things nothing else supplies:

1. **A target for lobbying.** BL-539 has a corporation shifting *a nation's* weights. You do not
   lobby a nation; you lobby someone in it.
2. **A filler for doctrine.** MILITARY.md lists doctrine as an **all-zero stub**. A commander is the
   natural thing for a doctrine to belong to.
3. **A generator of change.** People age and die. Over a ~100-hour campaign that is a source of
   discontinuity nothing else provides — and it is the cleanest bridge into
   [`EVENTS.md`](EVENTS.md).

**What it caps you at.** Attention. See § Scale.

---

## The proposal

### A person is a name, a role, a tenure, and at most one bias

    person {
        name        generated from the holder's own tongue
        role        WHAT they do — the mechanic
        seat        WHERE they do it — a corp, a nation, a unit
        tenure      when they took the seat; when they leave it
        bias        at most one, and it must gate or bias something real
    }

**One bias, not a trait sheet.** A person with five modifiers is a spreadsheet with a name; a
person with one is a character. The bias should be expressible as a `scalar_modifier`
([`META_LAYER.md`](META_LAYER.md)) so it inherits determinism, serialisation and ledger legibility
for free — and so **a person costs an enumerator, not a subsystem.**

### Five candidate roles, each mapped to machinery that exists

| Role | Seat | Biases / gates | Precedent |
|---|---|---|---|
| **Company head** | the player's corp | attachment first; a bias second | **BL-370** — ship this one first |
| **Counsel** | any corp | the strategy weight, boundedly | **BL-207**, fully designed |
| **Commander** | a unit | doctrine — currently an all-zero stub | MILITARY.md § What is absent |
| **Minister** | a nation | which priority line BL-537's budget favours | BL-537, BL-539 |
| **Contact** | a (buyer, supplier) pair | who the sentiment is *with* | RELATIONS.md, BL-546 |

**Minister is the one that changes another system's design.** If a nation's budget weights are held
by a person, then **lobbying has a target, succession is a political event, and BL-542's scorer has
somewhere for its preferences to live** other than in the nation as an abstraction.

### Scale — the constraint that decides whether this is buildable

Io has ~88 corporations, ~43 nations, and a unit roster that grows. **A person per institution per
role is thousands of people**, none of whom the player will ever look at.

> **Proposed rule: a cast, not a population.** People are generated only where a role is *seated*,
> and roles are seated sparsely — the player's own corp, nations the player has met, units the
> player commands, suppliers the player has dealt with. **A person is created when someone would
> plausibly know their name**, which makes the discovery layer (BL-089's activity fog) the natural
> gate and costs nothing to generate.

### Mortality and succession

**Tenure is what makes a person more than a label.** A person who never leaves is a permanent
modifier wearing a name.

Proposed: people age on the economy tick, leave their seat deterministically, and a successor is
drawn with a different bias. That gives the game a source of **change the player did not cause and
cannot prevent** — which is the same argument BL-289 makes for a supernova, at a much smaller scale
and much more often.

Succession is also the cleanest first **event** ([`EVENTS.md`](EVENTS.md)), which is why the two
documents were written together.

### Determinism

A person is generated from the campaign seed and their seat, exactly as a city name is. Ageing and
succession advance on the economy tick in sorted seat order. **No wall clock, no unseeded draw.**

---

## What must NOT happen

- **A person must not be a second scorer.** BL-207's discipline is the model and it is
  non-negotiable: counsel **biases scoring, never bypasses validation** — no new verbs, no fog
  access, reads only the blackboard. Any role that could *act* rather than *bias* is a planner, and
  planners are prohibited.
- **A person must not carry an Earth name**, ever, by any route.
- **A person must not be generated in bulk** on the chance they are looked at.
- **A person must not decorate.** If a role's bias were removed and nothing measurable changed, the
  role should not ship.

---

## Settled — Ben's rulings, 2026-08-22

All six answered from the design register. **One overturns this document's own proposal.**

**All five roles, in the proposed order:** company head → counsel → commander → minister → contact.

**SEVERAL biases, not one.** *"People are a real optimisation axis."* The proposal argued for one,
on the grounds that a person with five modifiers is a spreadsheet with a name. **That is overturned**,
and the concern is kept rather than deleted — because it names the failure mode the design must now
guard against by other means. The guard is no longer *scarcity* of modifiers; it has to be
**legibility of the set**. A person is a build choice, and a build choice you cannot read is a
spreadsheet whatever its length.

**People are MORTAL.** Age and replacement, deterministically — so succession is real, and it is
what makes a person more than a permanent modifier wearing a name.

**The player's own figure is CHOSEN AT START, bias included.** So the company head is a build
decision at campaign setup rather than a portrait, which makes it a New World wizard concern
(`ui/STARTUP.md`) as well as a generation one.

**Banded like everything else** (BL-068). You learn who a rival's people are as you learn the rest
of them. No special case, and the visibility question needs no new machinery.

**A few exist in the pre-history** — only the figures whose deeds the campaign world still
remembers. `history_log` already records deeds with nobody to attribute them to; this names the
memorable ones and leaves the rest institutional.

### The consequence that reaches another document

**PEOPLE now gates EVENTS.** Ben chose *personal* as the first event family
([`EVENTS.md`](EVENTS.md)), and that family depends on this one. What ranked fourth by available
machinery is first by ruling, so this document's system moves **ahead** of the event system rather
than beside it.

---

## The questions as they were asked

1. **Which roles, and in what order?** Five are proposed. **Company head (BL-370) is the obvious
   first** because it is your own ask and needs no other system. **Minister is the highest-value**
   because it gives lobbying a target, but it depends on BL-537's budget landing.
2. **One bias, or several?** The proposal says one, on the grounds that a trait sheet is a
   spreadsheet. This is the call most likely to be overturned and the cheapest to change now.
3. **Are people mortal?** Succession is what makes the system generate change rather than sit
   still. It is also the thing that could make a good run feel arbitrarily punished.
4. **Does the player's own figure have a bias at all**, or is the company head purely attachment?
   A bias on your own head is a build choice; no bias is a portrait. BL-370 does not say.
5. **Is a person visible to rivals?** Everything else about a competitor is banded under BL-068. A
   named opposing commander is intelligence; an anonymous one is a number.
6. **Do people exist in the pre-history?** The Era −1 sim runs 4,000 years of polities. Named
   figures there would be *lore* rather than mechanics — and `history_log` already records deeds
   with no one to attribute them to.

---

## Where the parts would live

| Concern | File |
|---|---|
| Name generation, per-culture tongues | `src/world/tongue.{hpp,cpp}`, `city_names.cpp` |
| The advisory precedent | `src/scripting/persona_pack.hpp`, `scripts/personas/` |
| The bias vocabulary | `src/world/modifier_set.hpp` |
| Doctrine, currently a stub a commander would fill | `src/world/combat.cpp`, `campaign_battle.cpp` |
| A person | **nowhere — this system does not exist** |

**Related authorities.** [`EVENTS.md`](EVENTS.md) (succession is the first event),
[`politics/RELATIONS.md`](politics/RELATIONS.md) (the sentiment a person would hold),
[`politics/NATIONS.md`](politics/NATIONS.md) (the budget a minister would weight),
[`ai/AI_OPPONENT.md`](ai/AI_OPPONENT.md) § personas (BL-207's discipline),
[`military/MILITARY.md`](military/MILITARY.md) (doctrine),
`.claude/rules/io-standing-rules.md` (the naming rule, which is absolute).

**Backlog.** **BL-547 (named people and roles)** owns this document. **BL-370 (corp leader figure)**
is the first role and is Ben's own earlier ask. **BL-207 (persona counsel packs)** is the designed
precedent every other role should follow.
