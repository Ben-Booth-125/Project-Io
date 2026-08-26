# Project Io — Nations

**The nation as an actor** — what a nation *is* once generation has finished making it, what it
holds, what it may do, and what it does each tick. This document is the authority for that
question, and it is the one `docs/generation/NATION_GENERATION.md` explicitly declines:

> *"Nation system design is **an open item**. This document covers only the generation strategy."*
> — NATION_GENERATION.md, line 7

The split between the two is clean and worth keeping. **NATION_GENERATION.md owns how a nation
comes to exist** — territory, resource profile, political character, name. **This document owns
what it does afterwards.** Where they disagree about a field, generation wins on how it is set and
this doc wins on what it means.

§ Settled records Ben's rulings of 2026-08-22, which closed the six questions this document
originally posed and added a system — the lobbying/budget channel — that he raised in the same
session. The mechanism design for that channel lives in its backlog items; the rulings live here.

---

## What a nation is

A nation is a **territorial polity** in the campaign era. It owns tiles, it has a character, it
holds money, it states what it cares about, and it is the only actor that can author law.

It is **not** the player, and the player is not accountable to it beyond what law and stance
impose. Under the ancient arc the player is a mercenary company: a **law subject**, never a
legislator (`docs/MANUAL.md` § 1.2, and SYSTEMS.md § Policy). A nation is the thing whose rules you
operate inside, route around, pay to have changed, or price in.

It is also **not** a corporation. The two are different kinds of object with different faculties,
and the distinction is load-bearing:

| | Corporation | Nation |
|---|---|---|
| Owns | buildings, stockpiles, orders | **tiles** |
| Money | `balance`, spent on the market every tick | `treasury`, spent **only by direct transfer** |
| Acts through | `corp_command`'s verbs | the **national budget** — a weight vector over priority lines |
| Decides | `corp_ai`, every tick | `nation_ai`, on the same staggered cadence |
| Objective | profit — accumulative, unbounded | **positional** — needed and unthreatened |
| Can author law | no | **yes — and only it can** |

That table is the whole of this document in miniature. A nation has the two faculties a
corporation lacks — territory and legislative authority — and it never touches the market: every
credit that leaves it lands on a named corporation's balance.

---

## What a nation holds

### 1. Identity and territory — *generation output, stable thereafter*

`nation_component` carries a generated `name`, an ordered `tiles` list, a summed
`resource_abundance` profile, and three character fields: `politics` (an `ideology`), `posture`
(an `expansionism`) and `focus` (an `economic_focus`). The three are **outputs of the settlement
history** (`derive_national_character` — expansionism from the border-contest integral, focus from
the resource class of the regions settled during industrialisation, ideology from
industrialisation timing against neighbours); the seeded Pass 4 draw is the fallback for a body
with no settlement.

The three character fields shape the world handed to you at the epoch. Of the three, `posture` is
also read in play — it is one input to the grudge term of the nation scorer (§ What a nation
wants). `ideology`'s enumerator comments read as pair sentiment, and pair sentiment is the
substrate's job (`docs/politics/RELATIONS.md`), so the scorer deliberately does not read it: one
sentiment model, not two.

`world::tile_to_nation` is the reverse index, and it is what gives a law its **jurisdiction**.

### 2. The treasury — *a balance with both halves*

`nation_component::treasury` is a float. It is zero the instant the field is constructed —
deliberately, since a treasury that started full would be a balance change smuggled in as a
field. **Generation itself is ruled to credit it before the campaign tick ever runs** (Ben,
2026-08-24, NR-580) — see `docs/generation/NATION_GENERATION.md` § Pass 7 for the settled shape
(a levy/tariff transfer, the same conservation-checked mechanism the campaign tick already uses,
not yet implemented). The rule above is about the ONGOING campaign tick: nothing but the levy,
the tariff and the budget outflow may ever move this field once play starts.

Two flows credit it; one pass debits it.

| Flow | Direction | Where |
|---|---|---|
| **Extraction levy** | in | `apply_budget`, `budget_system.cpp` |
| **Import tariff** | in | the clearing tick, `market_clearing.cpp` |
| **The national budget** | out | `run_national_budget`, `nation_budget.cpp`, called from `run_nation_step` |

All three are **transfers, not mints**: the payer is debited exactly what the payee is credited, in
the same float and the same tick. The levy's conservation is asserted by
`tools/verify/law_author_harness.cpp`; the tariff's by `tools/verify/money_conservation.cpp`; the
budget's by `tools/verify/nation_budget_harness.cpp`, which proves the distinction between
bit-exact transfer and the destination's own float rounding rather than assuming it.

The treasury crosses the serialisation seam in `world_save.cpp`'s nation record, and `state_hash`
folds every treasury — in ascending nation id — whenever any nation's is non-zero, together with
the authored `nation_budgets` map. A world in which no nation has ever been paid hashes exactly as
a world without the nation layer, so empty-nation fixtures keep their pinned values.

### 3. A budget — *weights, not amounts*

`world::nation_budgets` maps a nation to a `nation_budget`: a weight per `budget_priority` line
plus a `reserve_fraction`. **A nation states what it cares about and the amounts follow from what
it holds** (Ben, 2026-08-22) — so a poor nation and a rich one of the same character behave
recognisably alike, differing in scale and not in kind, and no authored number is re-tuned when a
treasury grows. All weights default to zero: a nation with no authored budget spends nothing.

The nine lines, in their authored enum order (append-only once serialised — a weight vector is
indexed by them):

| Line | What it buys |
|---|---|
| `logistics_maintenance` | keeping the road/hub network standing (LOGISTICS.md) |
| `schooling` | population capability |
| `military_research` | force-side research, and a nation's own garrison upkeep (MILITARY.md § Nation garrisons) |
| `academic_research` | the civil tech ladder — `science` is reached, not spent, and this is the debit BL-478 (ancient research spend) is shaped around |
| `public_exploration` | state-funded survey — DISCOVERY.md's geographic fog |
| `contracted_force` | buying force the nation does not raise — CONTRACTS.md § Where offers come from (BL-572) derives an offer from this line's spendable share |
| `strategic_reserve` | buying goods to **hold** — through BL-350's procurement seam from a named supplier, never on the market. Distinct from `reserve_fraction`, which withholds credits; this line spends them |
| `public_works` | works a corporation builds and the nation pays for |
| `charters` | paying a corporation to exist somewhere it otherwise would not |

| `space_programme` | government satellite launches — `spacecraft_components` and `propellant`, bought through the procurement seam. The first buyer for space goods that is not a militia contract, and the state's own stake in the gate into space (BL-644) |

The first five are Ben's (2026-08-22); the next four were proposed alongside them and accepted;
`space_programme` is Ben's, 2026-08-26.

**Two of these lines are how a NATION becomes a demand channel** ([`MARKETS.md`](../economy/MARKETS.md)
§ Demand channels). `strategic_reserve` already names goods-buying and no consumer claims on it yet;
`space_programme` is the new one. State demand behaves unlike household demand and that is
deliberate — it arrives in lumps, it follows a weight a rival can lobby to move, and it is therefore
worth playing *politics* over rather than merely scaling into.
The spend mechanics are generic over the enum, and a line no consumer claims on is simply skipped.
Most lines take no subject — a flat weighted claim on the tick's spendable — but `line_takes_subject`
(`nation_budget.hpp`) names the two that do: `public_exploration` and `contracted_force`, whose
claims name a target (a survey site; an offer's escrow) rather than only an amount.

### 4. Law authorship

A `law` is an id, a `condition_set` predicate, an effect kind, a rate, a resource scope, an
`enacted` flag and an `enacting_nation` (BL-480, law has an author).

**A law with no author cannot exist.** Every seeding path assigns one, and a record whose author is
null or dangling is ill-formed and charges *nothing*. That is defensive by design: an authorless
charge is money destruction wearing a law's clothes.

Generation seeds exactly one law, in `seed_prototype_laws` (`hard_coded_world.cpp`):

- **`LAW-EXTRACTION-LEVY`** — a per-unit charge on raw extraction output, all resources, rate
  **1.0 credits per unit** by default, enacted at generation.
- Its author is **the player corporation's home nation** — chosen so the law binds the player and
  the ledger's levy line reads non-vacuously from turn one (NR-369). Fallback is the largest
  territory, ties to the lowest entity id; with no nations, there is no law.
- Its predicate is an **empty `condition_set`** — unconditional once enacted, which is the common
  case BL-155 (law/policy surface) names and the always-true degenerate branch of BL-342.

Enactment is **the author nation's act, never a player checkbox.** The Budget ledger's Laws
section is browse-only and shows each law's author and rate.

---

## The enforcement seam — a law is a modifier over the market

> **A law is a modifier OVER the market, never an override OF it.**

This is the principle established when price clamps were vetoed on 2026-07-11: a clamp *fights*
price resolution rather than shifting a flow's cost.

So the levy applies where the flow is **accounted** (`apply_budget`), not where the price is
**resolved** (`clear_markets`). Extraction output is priced by the market exactly as it would be
without the law, and the levy is a separate accounted cost.

Two consequences, both intended. The market stays the only thing that sets prices. And the player
sees the tax as **its own number**, rather than as an unexplained worse price.

**Jurisdiction bounds who pays.** The levy reaches only output on tiles the author nation owns.
A corporation extracting outside that territory pays that nation nothing.

**Evaluation is once per law, per corp, per tick** (`evaluate_laws`), before the money loop reads
it. Laws by the same author stack additively into one schedule. Ordering is fixed, the function is
pure, and the determinism invariant holds.

**The effect family is an enum.** `law_effect_kind` carries the extraction levy and the import
tariff. BL-155 (law/policy surface) settles a **four-family taxonomy** — margin modifiers,
production, permission, relationship — and the enum is the extension point: a new family is an
enumerator plus a branch in `apply_law_effect`, with no change to the record, the predicate, or the
seam. The predicate is BL-342's, which already carries military subjects, so nothing in the law
record assumes an economic subject — a law may change what the player can **field** or must
**answer to** (BL-094's design test), not only what it earns. Which law classes the company is
*subject to* — an embargo it must route procurement around, basing rights it must be granted — is
BL-399 (company answerability).

---

## The import tariff

`law_effect_kind::import_tariff` is an ad-valorem duty on a sale made **in the enacting nation's
own market** to a buyer **domiciled elsewhere**. A same-nation sale is charged nothing — a tariff
that taxed domestic trade would be a sales tax wearing the wrong name.

Rates from several enacted laws stack additively and clamp to `[0, 1]`, so a stack of laws can
never charge a buyer more than the goods are worth. The duty is a transfer into the market nation's
treasury. It is deliberately **not** folded into `evaluate_laws`, because a tariff is a property of
a *jurisdiction and a good*, resolved when a trade matches — not a property of a corp. The whole
pass is gated on `any_import_tariff_enacted`, so a world with no tariff law pays nothing for the
mechanism.

**A tariff is directional** (§ Settled 4): `(author, target, resource) → rate`, with an
`all_nations` sentinel for the blanket form. The intended authoring path is a derivation at campaign
setup from the Era −1 sim's pair outcomes.

**The tariff has no author** (Ben, 2026-08-23, ruling on NR-400). This is a deliberate ordering, and
it is stated here so the section above is not read as describing a live duty. `import_tariff` is a
member of the law-effect vocabulary and of the save format, and `market_clearing` carries the whole
duty pass — but **nothing enacts one**. No corp verb, no control, and no generation path authors a
tariff law; the generator seeds the extraction levy alone. So `any_import_tariff_enacted` is false
for the whole of a played campaign, and the duty pass is unreached.

That makes the tariff **vocabulary ahead of its consumer** — the same shape META_LAYER.md's unwired
modifier subjects have, and subject to the same discipline: a shape is proven by an instance, and
an instance is owed. What it is *not* is an inert mechanic the reader should design against as
though it were charging anyone. The nation grant (§ The 2026-08-18 grant) is what a rate-setting
author would be built on: setting a tariff rate is named there as a nation power.

---

## What a nation MAY do — the 2026-08-18 grant

Nation and polity behaviour is **granted**. Ruling 4 of NR-331, Ben, 2026-08-18 — recorded in
`.claude/rules/io-standing-rules.md`, which is the authority for its exact terms.

It covers both grains Ben named: Era −1 polities inside the generation sim, and campaign-era
nations.

**The binding constraints are the whole basis of the grant.** Behaviour must be pure, seeded,
deterministic and replayable — a **scored-utility layer issuing only legal verbs**, in the
BL-202/BL-203 shape that `corp_ai` runs in. Never a planner.

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

## What a nation does each tick — the nation step

`run_nation_step` (`src/world/nation_step.{hpp,cpp}`) is where a nation acts. It runs **after
`apply_budget`** — the treasury holds this quarter's levy and tariff only then; running it inside
`run_economy_step` would spend last quarter's revenue — and **before `advance_tech_gates`**, whose
`surplus` gate reads the corp balance a subsidy has just moved. Four moves, in order, and each is
somebody else's machinery:

1. **Score.** `score_national_budgets` returns a weight vector for exactly the nations **due** on
   this economy tick under the staggered cadence (§ What a nation wants). Each returned slot
   overwrites one entry of `w.nation_budgets`; the rest keep last quarter's weights.
2. **Gather.** This tick's claims come off `economy_report::budget_claims`, emitted earlier in the
   same tick by `corp_ai`'s cash gate — a rival that wanted to survey a body and could not afford
   it asks its home nation. Nothing here invents a claim. Every field of a claim is validated at
   gather time, the line index included: a claim carrying an out-of-range line is dropped whole,
   never clamped onto a line nobody asked for, because the moment a claim arrives over `--serve`
   this is an untrusted input boundary.
3. **Spend.** `run_national_budget` — the pure pass over (weights, claims).
4. **Dispatch the earmark** (Ben, 2026-08-23, NR-568). A paid `public_exploration` transfer names a
   body. The corp is credited exactly `survey_cost(body)` and `dispatch_survey` debits exactly
   that, the same tick — so the credit never sits on the balance for the corp's next evaluation to
   spend on a build. A dispatch that cannot proceed (the body was surveyed by someone else this
   very tick) is recorded on the report as a failed earmark and its credit **clawed back** to the
   treasury — a nation does not pay for a survey that did not start.

The cadence key is `world::current_econ_tick` — the quarter counter, never the day tick.

**This pass has no human subject.** It runs regardless of `corp_ai_params::spectating`. A subsidy
landing on the player's corp is a transfer *to* the player, not an action *on* the player's corp,
so the standing prohibition has nothing to protect here (NR-569d). The player corp is never a
claimant, because `corp_ai` never scores it, so in a played session it is never funded either.

### The spend rules

Three rules keep the budget honest, each a requirement row in `nation_budget_harness`:

1. **Every credit out is a direct transfer to a named corporation.** Ben: *"Payment to corps is
   direct and not on the market."* A budget line does not bid, does not clear, and never touches
   `clear_markets` — it debits the treasury and credits a corporation balance in the same float,
   the same tick. `nation_budget_result::paid`, `national_budget_tick::total_transferred` and the
   treasury's delta are one accumulation, not three that agree approximately.
2. **Nations save.** `reserve_fraction` is withheld from the tick's spendable total, and an
   underspent line accumulates rather than evaporating — the only credits that leave are the ones
   actually paid. Deliberately *not* a per-line carry bucket: a weight is a claim on **this**
   tick's spendable, not a pot, and a bucket would make a line's spending power depend on history
   the player cannot see.
3. **Bounded by the treasury.** Where a line's demand exceeds its share the line is partially
   filled and says so (`budget_line_result::fill_fraction` < 1) — never overdrawn, never silently
   dropped. The bound is a **running total of what has been paid**, never a decremented remainder:
   a remainder drops the low bits of every pay it subtracts, under-counts, and lets a nation facing
   enough claims pay past its allotment — which drives a solvent treasury negative and then locks
   it out of spending on every following tick through the `treasury > 0` gate.

   **3a. Earmarked claims are whole or nothing** (Ben, 2026-08-23, NR-568). A claim that names a
   `subject` is not a request for credits; it is a request that one named thing be paid for. A
   pro-rata partial fill would leave a corp holding a fraction of a survey — a credit that
   dispatches nothing, exactly the fungible top-up Ben ruled out. So an earmarked claim is paid in
   full when the line's remaining share and the nation's remaining headroom both cover it, and
   otherwise skipped: pay 0, the skip counted on the line (`budget_line_result::skipped`), the line
   flagged `rationed`. Unearmarked claims keep rule 3's partial fill.

An unearmarked transfer is folded onto `report.budgets[corp].subsidies` so the corp's `net()`
explains the credit. An earmark is not: it leaves the balance where it found it, so it lives on
`report.earmarks` — the line BL-555 (who is paying me) renders.

**Determinism.** Once per economy tick, over nations in ascending id, over lines in the fixed enum
order, over each line's claims in ascending (corp id, arrival index). `w.nations` and
`w.corporations` are unordered maps and float addition is not associative, so every accumulation
order is pinned. No RNG, no wall clock.

---

## Settled — Ben's rulings, 2026-08-22

The six questions this document opened with were settled in one session, together with a system
Ben raised in the same breath: **a two-way channel between corporations and nations.**

### The channel — lobbying one way, the budget the other

Ben: *"each corporation affects the national priorities, laws, and budget... lobbying should be an
option, however it can also go completely in reverse — perhaps it is still fun to have our
corporations funded directly via taxes."*

It closes a circuit. The levy and the tariff **fill** a treasury; the budget is the outflow. Money
makes a full loop — corp → nation → corp — instead of draining into a field no reader could account
for.

It also fits the player identity rather than straining it. A mercenary company is a **law subject,
never a legislator**, which leaves it no lever of its own on the rules it works inside. Lobbying is
the one mechanism that changes that without changing what the player is: **you do not pass the law,
you pay someone who does.** The `lobby` verb is the *only* route to influence over a nation, and it
is owned by BL-539 (lobbying).

### 1. What a treasury is for — **a weighted budget over priority lines**

Ben: *"Treasury can be used for things that it is used for in the real world. Logistics
maintenance, Schooling, Military research, Academic research, public space exploration."*

A nation holds **weights, not amounts** — § What a nation holds 3 is this ruling made data. Every
line spends through machinery Io already has. Research in particular is the **debit mechanism
BL-478 (ancient research spend) is shaped around**, since `science` is reached, not spent.

*Owned by BL-537 (national budget) and BL-538 (treasury priority lines).*

### 2. Conservation — **actor-to-actor transfers conserve; the market does not, and that stays**

Ben: *"Nations should be able to save up, and so should corps. Payment to corps is direct and not
on the market."*

Two rules follow. **Every nation↔corp flow is a direct transfer** — it debits one balance and
credits another in the same float and the same tick, never bidding and never clearing. And **both
sides accumulate**: a nation does not spend to zero, a reserve is held back, and an underspent line
carries forward.

The market is untouched. `clear_markets` remains a buyer of last resort that pays sellers with
nobody's money, and total corporate cash rises and falls every tick by design. The two standards
are explicitly different things, which is what BL-392's silent value destruction came from
conflating.

*One consequence, handled: the strategic-reserve line looks like it must buy on the market. It does
not — it buys through BL-350's procurement seam from a named supplier, so the rule holds and an
existing mechanism is reused.*

### 3. The levy rate — **not a number, an anchor**

Ben: *"It really depends on the unit. We don't have a stable way to consider how valuable things
are right now. Perhaps we can consider that the equipment needed to sustain a unit costs about 2x
their salary for that year."*

The anchor fixes **one ratio between two quantities a player already understands**, and every
other price is argued against it.

Both halves live in `scripts/economy.lua` § `unit_upkeep`, charged per head per tick. An economy
tick is one quarter, so the annual factor of 4 appears on both sides and cancels — *"for that
year"* needs no calendar arithmetic anywhere:

    Σ ( goods_per_head[r] × base_price[r] )  ≈  2 × credits_per_head

**Clarified the same session** — *"let's divorce wages from goods somewhat. My 1/2 figure can use
base prices. We don't have to rederive wages every tick."* Three consequences:

- **`base_price`, never the resolved market price.** It is an **authored, seed-invariant table** in
  `scripts/world_gen.lua` (ordnance 43.0, food_rations 6.0), so the anchor means the same thing in
  every generated world and does not drift with local scarcity.
- **Authoring-time, not runtime.** Nothing recomputes per tick. The upkeep pass charges the
  authored rates; the anchor is the *argument for what those rates should be*, applied once and
  re-checked only when the base-price table or the roster moves.
- **A band, not an identity.** The wage is authored independently and the goods draw is sized
  against it. A class whose equipment is deliberately cheap or dear may sit outside the band and
  say so.

It also sets the **unit upkeep rate** MILITARY.md carries at `0.0`, and turns the levy question
into a legible one: *this nation's levy funds N units a year.*

*Owned by BL-543 (value anchor). Divorcing the two halves leaves the wage itself unanchored — that
is BL-544 (unit wage reference), whose likely answer is a multiple of the civilian `base_wage` the
game already pays. Anchor and upkeep first, re-measure, then rule on the levy — doing both at once
makes the benchmark uninterpretable.*

### 4. Tariffs — **directional, and seeded from the pre-history**

Ben: *"This should be an emergent consequence of generation, wherein nations gain their tariffs as
a directional mode of diplomacy."*

A tariff stops being a posture toward the world and becomes a statement about a **specific
relationship**: `(author, target, resource) → rate`, with an `all_nations` sentinel preserving the
blanket form. The initial map is derived at campaign setup from the Era −1 sim's pair outcomes —
who invaded whom, who never touched whom — deterministically, and legibly enough that a player can
read a tariff and find the war behind it. Those same pair outcomes are the true input to the
scorer's grudge term (§ 5).

*Owned by BL-541 (directional tariffs).*

### 5. What a nation wants — **a positional objective, not an accumulative one**

Ben: *"Nation objectives should be terrestrial diplomacy. Figuring out which economic niche can be
well filled, and how to avoid conflict while respecting historical grudges."*

A corporation maximises profit — unbounded, accumulative, identical for every firm. A nation does
not want to be rich; it wants to be **needed and unthreatened**. `src/world/nation_ai.{hpp,cpp}`
is that objective as a pure, seeded, deterministic scorer over three terms in a fixed order:

- **Niche fit** — complement what the neighbours lack, read as output **shares** so a nation never
  scores by producing the most, and weighted by the resolved market price on its own ground so a
  niche in something nobody values is a weak niche, not a non-existent one.
- **Conflict avoidance** — a negative term on expected war cost, read from force standing across
  its own border and `stance.hpp`'s pair-state.
- **Grudges** — which bias the first two **multiplicatively and per neighbour** rather than
  setting a goal of their own (`niche_grudge_bias`, `conflict_grudge_bias`), so the pre-history
  stays load-bearing instead of becoming scenery. The grudge is derived in one function,
  `grudge_from_border`, from the residue that survives generation — contested border length plus
  the pair's `expansionism` posture — so when BL-541's pair outcomes replace that input nothing
  else moves.

*"Terrestrial"* is doing work: a nation's horizon is its tile-edge neighbours, the markets on its
own bodies, and force on its own border — never the whole map. That is also what keeps the scorer
cheap across ~43 of them. Nation *n* is re-scored on the tick where `tick % cadence_k == index(n)
% cadence_k`, with `index` its position in the **ascending-id** nation set — the `corp_ai` stagger,
keyed on the sorted set so that admitting a nation with the highest id shifts nobody's slot. Every
constant lives in `nation_ai_params`; `tools/verify/nation_scorer_harness.cpp` guards each term
by mutating the thing it reads.

A well-played nation in Io mostly **stays out of fights**. That is unusual for a strategy game and
is much of the point — nations avoiding each other leaves the Conflict pillar to the companies,
which is where the player lives.

*Owned by BL-542 (nation scorer).*

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
stored as a flag. Access and Trust are the two dimensions of the sentiment substrate at
nation→corp grain (`docs/politics/RELATIONS.md` § The settled model), not a table of their own.

**Access** gates who may operate inside the borders — the dimension Ben named, and the first
place Io gates placement on a political fact rather than a physical one. **Trust** is the
dimension **BL-377 (mercenary contract seam) rides**: a mercenary company's client relationship
lives here rather than on a parallel axis invented for it.

*Owned by BL-540 (nation→corp stance).*

### 7. The grant reaches a rival acting politically against the player — **granted** (NR-517)

A rival lobbying to shift a law that binds the player's corp, or a nation gating the player out
of a territory, is a consequence imposed on **a corp a human owns** — the BL-450 class of widening,
which needs an explicit dated grant rather than a reading of the 2026-08-18 nation grant as
already covering it. Ben granted it on the same terms: deterministic, seeded, scored-utility,
legal verbs only, never a planner. A rival may **lobby** a nation against the player, and a
nation's derived stance may **gate the player's corp** out of a territory. It is recorded as a
dated widening in `.claude/rules/io-standing-rules.md`.

**What the grant does not admit:** a rival *enacting* law — only a nation can — and influence
acquired by any route other than the `lobby` verb.

### 8. A lobbying fee is **consumed** (NR-517)

A cost, not a transfer. The treasury's inflows stay purely legal — levy, tariff, charter fees — and
**a nation never profits from being lobbied**. The reason is the scorer: a nation that profited
from lobbying would have an incentive to be corruptible. Transferred would also make lobbying a
second tax.

This is the one flow in the corp↔nation channel that deliberately does **not** conserve, and the
harness must say so where a reader would otherwise assume the channel's rule.

---

## Where the parts live

| Concern | File |
|---|---|
| The nation record | `src/world/components.hpp` § `nation_component` |
| Territory reverse index | `src/world/world.hpp` § `tile_to_nation` |
| The authored budgets | `src/world/world.hpp` § `nation_budgets` |
| The law object, evaluation, tariff rates | `src/world/law.{hpp,cpp}` |
| The predicate every law reads | `src/world/condition_set.{hpp,cpp}` |
| The levy transfer | `src/world/budget_system.cpp` § `apply_budget` |
| The tariff transfer | `src/world/market_clearing.cpp` |
| The budget pass | `src/world/nation_budget.{hpp,cpp}` § `run_national_budget`, `budget_priority` |
| The scorer | `src/world/nation_ai.{hpp,cpp}` § `score_national_budgets`, `nation_ai_params` |
| The tick-level caller | `src/world/nation_step.{hpp,cpp}` § `run_nation_step` |
| Law seeding | `src/world/hard_coded_world.cpp` § `seed_prototype_laws` |
| The browse-only Laws surface | `src/ui/balance_ledger.cpp` |
| Save and hash | `src/world/world_save.cpp` (nation and law records), `src/world/world.cpp` § `state_hash` |
| Checks | `tools/verify/law_harness.cpp`, `law_author_harness.cpp`, `money_conservation.cpp`, `nation_budget_harness.cpp`, `nation_scorer_harness.cpp`, `nation_wiring.cpp` |

**Related authorities.** `docs/generation/NATION_GENERATION.md` (how a nation is made),
`docs/economy/FINANCE.md` (the money loop the levy is accounted in), `docs/economy/MARKETS.md`
(§ Tariffs, the clearing-tick half), `docs/politics/RELATIONS.md` (sentiment, the substrate
nation→corp stance reads), `docs/economy/CONTRACTS.md` (§ Where offers come from, the
`contracted_force` line's consumer), `docs/military/MILITARY.md` (§ Nation garrisons, the
`military_research` line's other consumer), `docs/SYSTEMS.md` (§ Policy, § Conditions),
`.claude/rules/io-standing-rules.md` (the grant's exact terms).

**Owning items.** BL-539 (lobbying), BL-540 (nation→corp stance), BL-541 (directional tariffs);
BL-543 (value anchor) and BL-544 (unit wage reference) for the price anchor; BL-155 (law/policy
surface) for the four-family taxonomy and the ten-law list; BL-186 (laws ledger UI); BL-399
(company answerability); BL-555 (who is paying me) for the earmark surface; BL-572 (contract
offers) for the `contracted_force` line's derivation; BL-571 (nation garrisons) for the
`military_research` line's garrison-upkeep consumer.
