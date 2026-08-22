# Project Io — Nations

**The nation as an actor** — what a nation *is* once generation has finished making it, what it
holds, what it may do, and what it actually does today. This document is the authority for that
question, and it is the one `docs/generation/NATION_GENERATION.md` has always explicitly declined:

> *"Nation system design is **an open item**. This document covers only the generation strategy."*
> — NATION_GENERATION.md, line 7

The split between the two is clean and worth keeping. **NATION_GENERATION.md owns how a nation
comes to exist** — territory, resource profile, political character, name. **This document owns
what it does afterwards.** Where they disagree about a field, generation wins on how it is set and
this doc wins on what it means.

> **Status: written 2026-08-22 as capture, then settled the same day.** Everything in § Build
> status is transcribed from the code and is true today. § What is absent names the holes and, for
> each, the item that now owns it. § Settled records Ben's rulings of 2026-08-22, which closed all
> six questions this document originally posed and added a system — the lobbying/budget channel —
> that he raised in the same session. Two questions remain, in § Still open.

---

## What a nation is

A nation is a **territorial polity** in the campaign era. It owns tiles, it has a character, it
holds money, and it is the only actor that can author law.

It is **not** the player, and the player is not accountable to it in any way the code models yet.
Under the ancient arc the player is a mercenary company: a **law subject**, never a legislator
(`docs/MANUAL.md` § 1.2, and SYSTEMS.md § Policy). A nation is the thing whose rules you operate
inside, route around, or price in.

It is also **not** a corporation. The two are different kinds of object with different faculties,
and the distinction is load-bearing:

| | Corporation | Nation |
|---|---|---|
| Owns | buildings, stockpiles, orders | **tiles** |
| Money | `balance`, spent every tick | `treasury`, **spent by nothing** |
| Acts through | `corp_command`'s 25 verbs | **no verb exists** |
| Decides | `corp_ai`, every tick | **nothing decides for it** |
| Can author law | no | **yes — and only it can** |

That table is the whole of this document in miniature. A nation has the two faculties a
corporation lacks — territory and legislative authority — and none of the machinery that makes a
corporation an actor.

---

## Build status

Three things about a nation are live. One is generation output; two are campaign state.

### 1. Identity and territory — *generation output, stable thereafter*

`nation_component` carries a generated `name`, an ordered `tiles` list, a summed
`resource_abundance` profile, and three character fields drawn from seeded RNG in Pass 4:
`politics` (an `ideology`), `posture` (an `expansionism`) and `focus` (an `economic_focus`).

**The three character fields are read by generation and by almost nothing else.** They shape the
world that is handed to you at the epoch. They do not currently make a nation behave differently
during play, because nothing makes a nation behave at all.

`world::tile_to_nation` is the reverse index, and it is what gives a law its **jurisdiction**.

### 2. The treasury — *live state, credited, never debited*

`nation_component::treasury` is a float. It is **zero at generation** — deliberately, since a
treasury that started full would be a balance change smuggled in as a field.

Two flows credit it. **Nothing spends it.**

| Flow | Where | Shipped and reachable? |
|---|---|---|
| **Extraction levy** | `apply_budget`, `budget_system.cpp` | **Yes** — enacted at generation |
| **Import tariff** | the clearing tick, `market_clearing.cpp` | Built and proved; **unreachable in play** |

Both are **transfers, not mints**: the payer is debited exactly what the nation is credited, in
the same float and the same tick. The levy's conservation is asserted by
`tools/verify/law_author_harness.cpp`; the tariff's by `tools/verify/money_conservation.cpp`,
which covers it alongside BL-392's procurement flows and against control worlds.

> **The treasury is not serialised and is not covered by `state_hash`.** Nations are hashed
> nowhere. BL-107 (save format version) must pick this field up; until it does, a treasury
> divergence is only detectable through the debit half, on corporation balances.

### 3. Law authorship — *one law, seeded, enacted*

A `law` is an id, a `condition_set` predicate, an effect kind, a rate, a resource scope, an
`enacted` flag and — since BL-480 (law has an author) — an `enacting_nation`.

**A law with no author cannot exist.** Every seeding path assigns one, and a record whose author is
null or dangling is ill-formed and charges *nothing*. That is defensive by design: an authorless
charge is money destruction wearing a law's clothes.

The world ships exactly one law, seeded by `seed_prototype_laws` in `hard_coded_world.cpp`:

- **`LAW-EXTRACTION-LEVY`** — a per-unit charge on raw extraction output, all resources, rate
  **1.0 credits per unit** by default, **enacted at generation**.
- Its author is **the player corporation's home nation** — chosen so the law binds the player and
  the ledger's levy line reads non-vacuously from turn one (NR-369). Fallback is the largest
  territory, ties to the lowest entity id; with no nations, there is no law.
- Its predicate is an **empty `condition_set`** — unconditional once enacted, which is BL-155's
  stated common case and the path that exercises BL-342's always-true degenerate branch.

Enactment is **the author nation's act, not a player checkbox.** The Budget ledger's enact control
was removed by BL-480; the Laws section is browse-only, and shows each law's author and rate.

---

## The enforcement seam — the one settled design decision

> **A law is a modifier OVER the market, never an override OF it.**

This is the principle established when price clamps were vetoed on 2026-07-11: a clamp *fights*
price resolution rather than shifting a flow's cost.

So the levy applies where the flow is **accounted** (`apply_budget`), not where the price is
**resolved** (`clear_markets`). Extraction output is priced by the market exactly as before, and
the levy is a separate accounted cost.

Two consequences, both intended. The market stays the only thing that sets prices. And the player
sees the tax as **its own number**, rather than as an unexplained worse price.

**Jurisdiction bounds who pays.** The levy reaches only output on tiles the author nation owns.
A corporation extracting outside that territory pays that nation nothing.

**Evaluation is once per law, per corp, per tick** (`evaluate_laws`), before the money loop reads
it. Laws by the same author stack additively into one schedule. Ordering is fixed, the function is
pure, and the determinism invariant holds.

---

## The import tariff — built, proved, and unreachable

`law_effect_kind::import_tariff` is an ad-valorem duty on a sale made **in the enacting nation's
own market** to a buyer **domiciled elsewhere**. A same-nation sale is charged nothing — a tariff
that taxed domestic trade would be a sales tax wearing the wrong name.

The mechanism is complete. Rates from several enacted laws stack additively and clamp to `[0, 1]`,
so a stack of laws can never charge a buyer more than the goods are worth. The duty is a transfer
into the market nation's treasury. It is deliberately **not** folded into `evaluate_laws`, because
a tariff is a property of a *jurisdiction and a good*, resolved when a trade matches — not a
property of a corp.

**And nothing in the shipped binary can enact one** (NR-400). Outside `law.{hpp,cpp}` and
`market_clearing.cpp` there is not one reference to `import_tariff` anywhere in `src/`. No
`corp_verb`, no UI control, no generation seeding, no agent-seam command creates a tariff law.

So `any_import_tariff_enacted` is permanently false in a real campaign, and the entire tariff pass
is dead code in play. It is proved — in a harness fixture. The whole pass is gated on that flag, so
with no tariff enacted the world is bit-identical to the pre-tariff build.

This is the clearest instance of the pattern this document exists to name: **a nation has a
faculty and no way to exercise it.**

---

## What a nation MAY do — the 2026-08-18 grant

Nation and polity behaviour is **granted**, and the grant is recent. Ruling 4 of NR-331, Ben,
2026-08-18 — recorded in `.claude/rules/io-standing-rules.md`, which is the authority for its
exact terms.

It is the exception BL-054 (nation behaviour passes) had deferred indefinitely, and it covers both
grains Ben named: Era −1 polities inside the generation sim, and campaign-era nations.

**The binding constraints are the whole basis of the grant.** Behaviour must be pure, seeded,
deterministic and replayable — a **scored-utility layer issuing only legal verbs**, in the
BL-202/BL-203 shape that `corp_ai` already runs in. Never a planner.

What the grant **admits**:

- a nation holding a treasury;
- a nation **setting a tariff or tax rate**;
- a nation **enacting a law**;
- a nation carrying pair-state toward another polity.

What it does **not** admit:

- anything whose timing, latency or ordering can vary the generated world — `docs/lore/HISTORY.md`
  is the authority for what the ladder produces, not a licence to randomise it;
- any cloud model in the loop. The no-cloud invariant in `docs/ai/AI_OPPONENT.md` § 10 is untouched.

**Reason for the grant, in Ben's framing:** three of the four systems his 2026-08-18 brief names —
international trade, logistics and diplomacy — are nation-grain, and `GENERATION_STRATEGY.md`'s
economic premise already assumes nations that act.

---

## What is absent, and known to be

Named so the absence is visible rather than discovered. Pattern borrowed from
`docs/military/MILITARY.md` § What is absent.

- **No nation actor.** There is no `nation_ai`, no `nation_verb`, no `nation_command` — a grep of
  `src/` returns nothing for all three. The grant above is a permission nobody has used. A nation
  decides nothing, ever, on any tick. → **BL-542 (nation scorer)** is the first use of it.
- **No spend side.** The treasury only ever rises. Nothing anywhere debits it, so a nation's
  balance is currently a scoreboard with no game attached. → **BL-537 (national budget)**,
  **BL-538 (treasury priority lines)**.
- **No authoring path for any law.** One law is seeded at generation and no actor can enact,
  repeal, or amend a law thereafter — not the player (correctly, they are a law subject), not a
  nation (incorrectly, it is the legislator), not the agent seam. → **BL-541** enacts the first
  tariff at generation; **BL-539 (lobbying)** is how a law subject reaches law at all.
- **One effect family of four.** `law_effect_kind` has two enumerators and BL-155 (law/policy
  surface) settled a **four-family taxonomy**: margin modifiers *(shipped)*, production,
  permission, relationship. The enum is the extension point and adding one is a new enumerator
  plus a branch in `apply_law_effect` — no change to the record, the predicate, or the seam.
- **No military effect.** BL-094's design test asks whether a system changes what the player can
  **field** or must **answer to**. An extraction levy reaches economic outcomes only, and
  pretending otherwise would be dishonest. What the seam owes instead is that *nothing* in the law
  record or effect dispatch assumes an economic subject — the predicate is BL-342's, which already
  carries military subjects. The answer can become yes; it is not yes.
- **No serialisation.** `nation_component::treasury` and `law::enacting_nation` have no serialiser.
  Nations and laws are not saved, so a treasury survives nothing (NR-399). There is no game
  save/load path at all today, so this costs nothing yet — and it is the first thing BL-107 must
  pick up when there is one.
- **No answerability.** BL-399 (company answerability) owns which law classes the company is
  *subject to* — an embargo it must route procurement around, basing rights it must be granted.
  It is `design-owed`. → **BL-540 (nation→corp stance)** supplies the Access dimension it needs.
- **No client for the mercenary loop.** BL-377 (mercenary contract seam) is the player's income and
  has no counterparty with money. → **BL-538**'s contracted-force line is that counterparty, and
  **BL-540**'s Trust dimension is the relationship it rides on.

---

## Settled — Ben's rulings, 2026-08-22

The six questions this document opened with were settled in one session, together with a system
Ben raised in the same breath: **a two-way channel between corporations and nations.** The rulings
are recorded here because they are settled; the **mechanism design lives in the backlog items**
until the work lands (DELIVERY.md § Design state).

### The channel — lobbying one way, the budget the other

Ben: *"each corporation affects the national priorities, laws, and budget... lobbying should be an
option, however it can also go completely in reverse — perhaps it is still fun to have our
corporations funded directly via taxes."*

It closes a circuit that was open at one end. The levy and the tariff **fill** a treasury nothing
spent; the budget is the outflow. Money now makes a full loop — corp → nation → corp — instead of
draining into a field no reader could account for.

It also fits the player identity rather than straining it. A mercenary company is a **law subject,
never a legislator**, which since the pivot has left it with no lever at all on the rules it works
inside. Lobbying is the one mechanism that changes that without changing what the player is: **you
do not pass the law, you pay someone who does.**

### 1. What a treasury is for — **a weighted budget over priority lines**

Ben: *"Treasury can be used for things that it is used for in the real world. Logistics
maintenance, Schooling, Military research, Academic research, public space exploration."*

A nation holds **weights, not amounts** — it states what it cares about, and the amounts follow
from what it has. Four more lines were proposed at Ben's request and are marked as proposals in
BL-538: **contracted force** (which is BL-377's client and its money source), **strategic reserve**,
**public works**, and **charters and monopolies**.

Every line spends through machinery Io already ships. Research in particular is the **debit
mechanism BL-478 (ancient research spend) has been missing** — NR-387 records that item as blocked
precisely because the spend model had no design.

*Owned by BL-537 (national budget) and BL-538 (treasury priority lines).*

### 2. Conservation — **actor-to-actor transfers conserve; the market does not, and that stays**

Ben: *"Nations should be able to save up, and so should corps. Payment to corps is direct and not
on the market."*

Two rules follow. **Every nation↔corp flow is a direct transfer** — it debits one balance and
credits another in the same float and the same tick, never bidding and never clearing. And **both
sides accumulate**: a nation does not spend to zero, a reserve is held back, and an underspent line
carries forward.

The market is untouched. `clear_markets` remains a buyer of last resort that pays sellers with
nobody's money, and total corporate cash still rises and falls every tick by design. The two
standards are now explicitly different things, which is what BL-392's silent value destruction came
from conflating.

*One consequence, handled: the strategic-reserve line looks like it must buy on the market. It does
not — it buys through BL-350's procurement seam from a named supplier, so the rule holds and a
shipped mechanism is reused.*

### 3. The levy rate — **not a number, an anchor**

Ben: *"It really depends on the unit. We don't have a stable way to consider how valuable things
are right now. Perhaps we can consider that the equipment needed to sustain a unit costs about 2x
their salary for that year."*

That is the right diagnosis and it is why the question could not be answered on its own terms.
The anchor fixes **one ratio between two quantities a player already understands**, and every other
price is argued against it.

Both halves already exist in `scripts/economy.lua` § `unit_upkeep`, both at zero, both charged per
head per tick. An economy tick is one quarter, so the annual factor of 4 appears on both sides and
cancels — the anchor reduces to a per-tick identity with no calendar arithmetic:

    Σ ( goods_per_head[r] × price[r] )  ==  2 × credits_per_head

It also sets the **unit upkeep rate MILITARY.md lists as absent**, and turns the levy question into
a legible one: *this nation's levy funds N units a year.*

*Owned by BL-543 (value anchor). Land the anchor and upkeep first, re-measure, then rule on the
levy — doing both at once makes the benchmark uninterpretable.*

### 4. Tariffs — **directional, and seeded from the pre-history**

Ben: *"This should be an emergent consequence of generation, wherein nations gain their tariffs as
a directional mode of diplomacy."*

A tariff stops being a posture toward the world and becomes a statement about a **specific
relationship**: `(author, target, resource) → rate`, with an `all_nations` sentinel preserving
today's blanket form. The initial map is derived at campaign setup from the Era −1 sim's pair
outcomes — who invaded whom, who never touched whom — deterministically, and legibly enough that a
player can read a tariff and find the war behind it.

This also fixes NR-400 as a side effect: it is the first thing that can enact a tariff at all.

*Owned by BL-541 (directional tariffs).*

### 5. What a nation wants — **a positional objective, not an accumulative one**

Ben: *"Nation objectives should be terrestrial diplomacy. Figuring out which economic niche can be
well filled, and how to avoid conflict while respecting historical grudges."*

A corporation maximises profit — unbounded, accumulative, identical for every firm. A nation does
not want to be rich; it wants to be **needed and unthreatened**. Three terms: **niche fit** (produce
what neighbours lack), **conflict avoidance** (a negative term on expected war cost), and
**grudges** — which bias the first two rather than setting a goal of their own, so the pre-history
stays load-bearing instead of becoming scenery.

*"Terrestrial"* is doing work: a nation's horizon is its neighbours and its own ground, never the
whole map. That is also what keeps the scorer cheap across ~43 of them.

A well-played nation in Io mostly **stays out of fights**. That is unusual for a strategy game and
is much of the point — nations avoiding each other leaves the Conflict pillar to the companies,
which is where the player lives.

*Owned by BL-542 (nation scorer). It is the first use anyone has made of the 2026-08-18 grant.*

### 6. A nation's stance toward a corporation — **graded, multi-dimensional, derived**

Ben: *"Nations should have a stance towards corporations, but this is more like a digital spectrum,
possibly multiple dimensions, and mostly derived towards who can act internationally in their
borders."*

It is **not** BL-448's corp-to-corp stance, and differs on each axis Ben named:

| | Corp → corp (BL-448) | Nation → corp (BL-540) |
|---|---|---|
| Shape | discrete tri-state | **graded** |
| Dimensions | one | **two: Access and Trust** |
| Origin | **declared** — a corp opts in | **derived** — nobody declares it |

The last row is load-bearing. Hostility is a state a corp opts into and *"may never acquire
ambiently"* (Ben, 2026-08-17). A nation's stance is the opposite by construction: a **read** of
what a company has actually done inside the jurisdiction, recomputed from public facts, never
stored as a flag.

**Access** gates who may operate inside the borders — the dimension Ben named, and the first time
anything in Io gates placement on a political fact rather than a physical one. **Trust** is the
dimension **BL-377 needs**: a mercenary company's client relationship rides here rather than on a
parallel axis invented for it.

*Owned by BL-540 (nation→corp stance).*

---

## Still open

Two, both raised by the rulings above rather than left over from before.

1. **Does the nation grant reach a rival acting politically against the player?** (NR-517) A rival
   lobbying to shift a law that binds the player's corp, or a nation gating the player out of a
   territory, is a consequence imposed on **a corp a human owns**. That is the BL-450 class of
   widening, which needed an explicit dated grant. The 2026-08-18 grant covers a *nation* acting;
   reading it as covering this would be exactly the quiet precedent the standing rules exist to
   prevent. **The player-facing halves of BL-539 and BL-540 are blocked on this ruling**; the
   nation-side machinery is not.

2. **Is a lobbying fee consumed, or does it land in the treasury?** (NR-517) Consumed reads as
   influence-buying and keeps the treasury's inflows to law. Transferred closes another loop but
   makes lobbying a second tax. BL-539 assumes **consumed** and says so.

## Where the parts live

| Concern | File |
|---|---|
| The nation record | `src/world/components.hpp` § `nation_component` |
| Territory reverse index | `src/world/world.hpp` § `tile_to_nation` |
| The law object, evaluation, tariff rates | `src/world/law.{hpp,cpp}` |
| The predicate every law reads | `src/world/condition_set.{hpp,cpp}` |
| The levy transfer | `src/world/budget_system.cpp` § `apply_budget` |
| The tariff transfer | `src/world/market_clearing.cpp` |
| Law seeding | `src/world/hard_coded_world.cpp` § `seed_prototype_laws` |
| The browse-only Laws surface | `src/ui/balance_ledger.cpp` |
| Checks | `tools/verify/law_harness.cpp`, `law_author_harness.cpp`, `money_conservation.cpp` |

**Related authorities.** `docs/generation/NATION_GENERATION.md` (how a nation is made),
`docs/economy/FINANCE.md` (the money loop the levy is accounted in), `docs/economy/MARKETS.md`
(§ Tariffs, the clearing-tick half), `docs/SYSTEMS.md` (§ Policy, § Conditions),
`.claude/rules/io-standing-rules.md` (the grant's exact terms).

**Backlog — the v0.1.24 cluster** filed 2026-08-22 from Ben's rulings, in dependency order:
**BL-537** (national budget, the spine) → **BL-538** (priority lines), **BL-539** (lobbying),
**BL-540** (nation→corp stance), **BL-541** (directional tariffs), **BL-542** (nation scorer).
**BL-543** (value anchor) is separate and nearer — v0.1.21, and it unblocks the levy rate and the
unit upkeep rate at once.

**Older neighbours.** BL-155 (law/policy surface) holds the four-family taxonomy and the ten-law
list. BL-480 (law has an author) shipped and its item still reads `designed` — see NR-514.
BL-186 (laws ledger UI), BL-399 (company answerability), BL-345 (politics relationship axis) and
BL-158 (politics datamodel stub) remain open.
