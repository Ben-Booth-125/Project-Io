# Project Io — Nations

> **Settles:** what a nation is once generation has finished making it · what it holds — territory,
> treasury, budget weights, law · how a law reaches the market · what an import tariff does and
> which direction it points · what a nation may do, what it does each tick, and in what order it
> spends · what a nation wants · what separates a major nation from a minor one.
> **Not here:** how a nation comes to exist — territory placement, resource profile, character,
> naming (NATION_GENERATION) · which quantity says how one nation reads another (RELATIONS) ·
> the predicate/effect substrate a law is composed from (META_LAYER) · the money loop's flows
> (FINANCE).
> **Confused with:** generation/NATION_GENERATION.md, politics/RELATIONS.md, META_LAYER.md.

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
originally posed and added a system — the lobbying/budget channel, retired 2026-10-23 — that he
raised in the same session. The mechanism design for that channel lives in its backlog items; the rulings live here.

---

## What a nation is

A nation is a **territorial polity** in the campaign era. It owns tiles, it has a character, it
holds money, it states what it cares about, and it is the only actor that can author law.

**The player is a nation (Ben, 2026-10-10; `docs/CONCEPT.md` § Player identity).** The player
plays one **major** nation. For its own nation the player is the legislator and the builder: it sets
the budget weights, the tariffs and the law that `nation_ai` sets for a rival, and it builds and runs
every building on its ground (Ben, 2026-10-23). For every other nation it is a neighbour — one it
trades with, tariffs, and may fight.

### Major and minor nations — two depths of one actor

A **major** nation is one generation raises to a full actor; a **minor** nation is every other one
(`docs/GLOSSARY.md` § Major nation; the counts are generation's,
`docs/generation/NATION_GENERATION.md` § The nation count is a target). The majors are the full
actors, the minors the cheaper backdrop that is traded with and fought over.

- **A major runs the whole of this document.** Treasury, budget, law, tariff, the nation step, the
  scorer of § 5, and the build scorer over its own industry. The player's major is the same object
  with both scorers switched off for it (§ What a nation does each tick).
- **A minor runs shallow (Ben, 2026-10-10).** It holds a treasury, sets budget weights and tariffs,
  and is re-scored on a slower cadence than a major. It enacts no law of its own beyond what
  generation leaves it. **It builds its own economy** on its own ground under the same build scorer
  as a major (Ben, 2026-10-23; `../ai/AI_OPPONENT.md` § 11). It owns tiles and garrisons, so it can
  be traded with and conquered like any nation.

| | A nation |
|---|---|
| Owns | **tiles**, and **every building on them** — never goods: what it makes is sold onto a market's shelf |
| Money | `treasury`: its buildings' sales land in it, and their inputs, wages and maintenance are paid from it |
| Acts through | the **national budget** — a weight vector over priority lines — and its **build verbs** |
| Decides | `nation_ai` for the budget and the build scorer for its industry, on the staggered cadence; the player for its own nation |
| Objective | **positional** — needed and unthreatened |
| Can author law | **yes — and only it can** |

### What a nation owns — every building on its ground (Ben, 2026-10-23)

Ben: *"We can remove the corp/company layer entirely. Just have nations building their own
buildings."* There is no corporation and no company: **a nation owns every building on its ground,
in every sector**, outright. This overturns the 2026-10-11 split between state industry (a major's
strategic sectors) and private industry (everything a corporation owned), and with it the rule that
a nation owns tiles and never buildings.

- **Its money is the treasury's.** A building's sales land in the treasury, and its inputs, wages and
  maintenance are paid from it.
- **A losing building idles.** When its costs exceed what the treasury can pay, a building stops
  producing and paying until the treasury can pay again. The nation never runs into debt. **Network
  nodes are paid first (Ben, 2026-10-22):** when the treasury cannot pay for everything, its hubs,
  stations and ports are funded before any other building, so the points its industry produces with
  come before the industry that draws them.
- **It builds what it pays for.** Construction is paid from the treasury. A building opens with no
  stock, so one fed only by trade waits out the haul.
- **The player's controls** over a building are the building verbs — build, recipe, sell — paid from
  the treasury. **Workforce is not one of them**: the game steps every building's workforce one 25 %
  step a quarter toward the most profitable level, a raise taken only when the treasury funds it (Ben,
  2026-10-14; `../economy/PRODUCTION.md` § Workforce target and the auto-solver).
- **The player's economy grows only by the player's builds** (Ben, 2026-10-23). Every building it
  opens with keeps running; nothing builds on its ground for it. A rival nation's builds are its
  build scorer's (`../ai/AI_OPPONENT.md` § 11).
- **Rail and stations** are a nation's like everything else (`../economy/LOGISTICS.md` § 4c). Laying
  track costs the treasury per tile, plus steel per tile, with upkeep per tile.
- **What generation leaves it.** Every building generation places is the nation's whose ground it
  stands on (`../generation/CORPORATION_GENERATION.md`).

### Own ground — a nation builds only on its own tiles (Ben, 2026-10-23)

**A nation builds only on its own ground**: never on another nation's tiles, and never on ground no
nation owns. The rule binds every building a nation may place — production, logistics sources
(hubs, stations, ports) and rail alike.

- **A road is the one reach across.** It may be laid one crossing *into* foreign ground, so that two
  nations' networks can join; never laid *from* foreign ground (`../economy/LOGISTICS.md` § 4).
- **Ground that changes hands carries its buildings.** When a tile passes to another nation — by
  conquest — every building on it passes to the new
  nation with it. A building is therefore always on its owner's ground.
- **Trade still crosses borders.** A nation sells onto any market its shipments reach and buys from
  any shelf; only placement is bounded. A foreign nation's goods entering a market pay that market
  nation's import tariff (§ The import tariff).

That is the whole of this document in miniature. A nation has territory, legislative authority and
all the industry on its ground, and it touches the market through what its buildings sell and buy,
and through the equipment its units carry (§ 2. Conservation).

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

### GDP

**A nation's GDP is the value of production on its tiles at local market prices** (NR-774, promoted
here 2026-09-15 as the player-facing figure Ben's opening map asks for). It is **derived, never
stored**: a reading over what the nation's ground produces and what its markets pay, read over a
trailing window in play and over output capacity at the seeded price field at the epoch
(`../generation/INDUSTRIALISATION.md` § 5. Wealth inequality, market cap and GDP).

**GDP per head is the axis of inequality between nations**, and output per centre is the axis
within one. Neither is a household measure, and neither feeds the treasury — the nation's
own buildings' sales do (§ 2). GDP says how much a nation makes; the treasury says how much the state holds.

**A regime field sits beside the three character fields** as a stub, derived at the epoch from the
Works fork, charter reach, subject history and whether the polity fought a world war
(`../generation/INDUSTRIALISATION.md` § 6. Stubs). Its names are invented, never Earth labels.

### 2. The treasury — *a balance with both halves*

`nation_component::treasury` is a float. It is zero the instant the field is constructed —
deliberately, since a treasury that started full would be a balance change smuggled in as a
field. **Generation itself is ruled to credit it before the campaign tick ever runs** (Ben,
2026-08-24, NR-580) — see `docs/generation/NATION_GENERATION.md` § Pass 7 for the settled shape:
the Exploration span's 1660 polity treasury, folded across the handoff through one stated
per-mille (BL-975, 2026-09-15). The rule above is about the ONGOING campaign tick: once play starts,
only the nation's own buildings' flows, the tariff, the tax and the budget outflow move this field.

Its buildings' market flows credit and debit it (§ What a nation owns). Beside them, two flows
credit it and one pass debits it.

| Flow | Direction | Where |
|---|---|---|
| **Import tariff** | in | the clearing tick, `market_clearing.cpp` — paid by the foreign nation whose goods enter |
| **Tax** | in | each quarter, the tier's share of the wages the nation pays (§ Taxes and Discontent) |
| **The national budget** | out | `run_national_budget`, `nation_budget.cpp`, called from `run_nation_step` |

The tariff is a **transfer, not a mint**: the importing nation is debited exactly what the market
nation is credited, in the same float and the same tick, asserted by
`tools/verify/money_conservation.cpp`. The budget's spend is asserted by
`tools/verify/nation_budget_harness.cpp`. The **extraction levy** retires (Ben, 2026-10-23): it
charged a firm for what it extracted on a nation's ground, and a nation charging itself moves nothing.

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

The ten lines, in their authored enum order (append-only once serialised — a weight vector is
indexed by them):

| Line | What it buys |
|---|---|
| `logistics_maintenance` | keeping the road/hub network standing (LOGISTICS.md) |
| `schooling` | population capability |
| `military_research` | force-side research, and a nation's own garrison upkeep (MILITARY.md § Nation garrisons) |
| `academic_research` | the civil tech ladder — treasury the line spends converts to the nation's research points (`../economy/RESEARCH.md` § Research points) |
| `public_exploration` | state-funded survey — DISCOVERY.md's geographic fog |
| `contracted_force` | buying force the nation does not raise — BL-572 (contract offers) derives an offer from this line's spendable share; the contract form itself carries no sell side to buy through (CONTRACTS.md § Explicitly out of scope) |
| `strategic_reserve` | buying goods to **hold**, off a market's shelf. Distinct from `reserve_fraction`, which withholds credits; this line spends them |
| `public_works` | **retired** (Ben, 2026-10-23): it paid a corporation to build works; the nation builds its own |
| `charters` | **retired** (Ben, 2026-10-23): it paid a corporation to exist; there are none |
| `space_programme` | government satellite launches — `spacecraft_components` and `propellant`, bought through the procurement seam and **consumed** (a terminal sink: the satellite launched). The first buyer for space goods that is not a militia contract, and the state's own stake in the gate into space (BL-644) |

The first five are Ben's (2026-08-22); the next four were proposed alongside them and accepted;
`space_programme` is Ben's, 2026-08-26.

**Two of these lines are how a NATION becomes a demand channel** ([`MARKETS.md`](../economy/MARKETS.md)
§ Demand channels). `strategic_reserve` already names goods-buying and no consumer claims on it yet;
`space_programme` is the new one. State demand behaves unlike household demand and that is
deliberate — it arrives in lumps and follows a weight the nation sets, so a rival's demand can be read and
anticipated rather than merely scaled into.
The spend mechanics are generic over the enum, and a line no consumer claims on is simply skipped.
(`logistics_maintenance` also buys goods, but it funds the **Infrastructure** channel, not the
State one — its demand scales with network size, not with treasury × weight; the weight only
decides how much of the bill gets paid.)
Most lines take no subject — a flat weighted claim on the tick's spendable — but `line_takes_subject`
(`nation_budget.hpp`) names the three that do: `public_exploration`, `contracted_force` and
`space_programme`, whose claims name a target (a survey site; an offer's escrow; the body a launch
lot stands on) rather than only an amount. The earmark's whole-or-nothing fill (rule 3a) is what
makes `space_programme`'s spend a lump: a share that cannot cover a whole launch lot buys nothing,
banks the difference, and fires later.

`logistics_maintenance`'s consumer is `derive_network_upkeep_claims` / `settle_network_purchases`
(`src/world/network_upkeep.{hpp,cpp}`; BL-643, network upkeep draws): each tick the nation's road
network — road tiles by level plus active ports and hubs, tallied from the world, never stored —
bills stone and timber at authored rates ([`LOGISTICS.md`](../economy/LOGISTICS.md) § 4 owns the
channel). The same state-purchase shape as the space programme below, with one deliberate
difference: the claim is **unearmarked**, so rule 3's pro-rata fill applies — upkeep is continuous,
and half the repair budget meaningfully buys half the materials, where rule 3a's lump exists for
purchases a fraction of which buys nothing. The materials are bought off a market's shelf at its
posted price, under the fair-price ceiling, and consumed; the supplier is always a market
(`../economy/MARKETS.md` § The shelf economy).

`space_programme`'s consumer is `derive_space_programme_claims` / `settle_space_purchases`
(`src/world/space_programme.{hpp,cpp}`): the state buys its lump **off a market's shelf** — the
lowest-id market whose shelf holds the most of the good, at least a whole lump, at a posted price
the fair-price ceiling admits — pays the posted price as a direct, whole-or-nothing treasury debit,
and the goods leave the shelf **without landing anywhere** — state demand is terminal. The supplier
is always a market: no actor holds stock (`../economy/MARKETS.md` § The shelf economy), and
whoever stocked the shelf was paid when the goods landed. The lump sizes are data
(`economy.space_programme`, scripts/economy.lua).

### Taxes and Discontent (Ben, 2026-10-16)

**A nation taxes the wages it pays, at a tier, I to V** (Ben, 2026-10-23: *"Taxes are taken from
wages, which are also paid for by the nation"*, re-subjecting the 2026-10-16 tax on home firms'
profit, which had no firm left to tax). Each quarter the tier's share of every wage its buildings
paid that quarter comes back into the treasury, in the same float and tick. A higher tier therefore
lowers the nation's net wage bill. The tier rates are dials in `scripts/economy.lua`. The player sets
its nation's tier on the Budget ledger's Taxes control; a rival nation's scorer sets its own, under
the 2026-08-18 grant ("a nation setting a tariff or tax rate").

**High taxes breed Discontent, and Discontent costs output.** Every nation carries a Discontent
reading between 0 and 1. A tier above the middle raises it each quarter; the middle tier holds it;
a tier below lowers it. At tier IV it takes **two years** of high taxes before Discontent starts to
bite; tier V bites sooner. Once it bites, every building on the nation's ground loses output in
proportion, up to **15%** at full Discontent. Cutting taxes lets it ebb at the same pace.

This is the downside of a high budget, and it is a **systemic force with a visible cause**: the
penalty sits on the ground, not inside any scorer, and the tier that caused it is on the ledger.
A nation that raises taxes to fund a programme pays for it in output two years on; a scorer that
weighs the tier sees the same Discontent the player does.

### 4. Law authorship

A `law` is an id, a `condition_set` predicate, an effect kind, a rate, a resource scope, an
`enacted` flag and an `enacting_nation` (BL-480, law has an author).

**A law with no author cannot exist.** Every seeding path assigns one, and a record whose author is
null or dangling is ill-formed and charges *nothing*. That is defensive by design: an authorless
charge is money destruction wearing a law's clothes.

Generation seeds **no extraction levy** (Ben, 2026-10-23): `LAW-EXTRACTION-LEVY` charged a firm for
what it extracted on its author's ground, and with no firms the author would charge itself. The laws
a world opens with are its tariffs (§ The import tariff), authored by its history.

Enactment is **the author nation's act, never a player checkbox.** The Budget ledger's Laws
section is browse-only and shows each law's author and rate.

---

## The enforcement seam — a law is a modifier over the market

> **A law is a modifier OVER the market, never an override OF it.**

This is the principle established when price clamps were vetoed on 2026-07-11: a clamp *fights*
price resolution rather than shifting a flow's cost.

So a law's charge applies where the flow is **accounted**, not where the price is **resolved**
(`clear_markets`). The market stays the only thing that sets prices, and the player sees the charge
as **its own number**, rather than as an unexplained worse price.

**Evaluation is once per law, per tick**, before the money loop reads it. Laws by the same author
stack additively into one schedule. Ordering is fixed, the function is pure, and the determinism
invariant holds.

**The effect family is an enum.** `law_effect_kind` carries the import tariff and the retired
extraction levy. BL-155 (law/policy surface) settles a **four-family taxonomy** — margin modifiers,
production, permission, relationship — and the enum is the extension point: a new family is an
enumerator plus a branch in `apply_law_effect`, with no change to the record, the predicate, or the
seam. The predicate is BL-342's, which already carries military subjects, so nothing in the law
record assumes an economic subject — a law may change what the player can **field** or must
**answer to** (BL-094's design test), not only what it earns.

---

## The import tariff

`law_effect_kind::import_tariff` is an ad-valorem duty on **a foreign nation's goods entering the
enacting nation's market**, paid by that foreign nation to the market nation (Ben, 2026-10-23, re-
subjecting the duty from a buyer domiciled elsewhere, which assumed firms). A nation's own goods in
its own market are charged nothing — a tariff that taxed domestic trade would be a sales tax wearing
the wrong name.

Rates from several enacted laws stack additively and clamp to `[0, 1]`, so a stack of laws can
never charge an importer more than the goods are worth. The duty is a transfer into the market nation's
treasury. It is deliberately **not** folded into `evaluate_laws`, because a tariff is a property of
a *jurisdiction and a good*, resolved when goods arrive — not a property of a building. The whole
pass is gated on `any_import_tariff_enacted`, so a world with no tariff law pays nothing for the
mechanism.

**A tariff is directional** (§ Settled 4): `(author, target, resource) → rate`, with an
`all_nations` sentinel for the blanket form. The intended authoring path is a derivation at campaign
setup from the Era −1 sim's pair outcomes.

**Its author is the history that produced it.** `seed_national_tariffs` enacts one blanket tariff
per nation whose inherited protection clears the floor, authored by that nation — so the duty falls
in the author's own market and credits the author's own treasury, through the single
`enacting_nation` field every law carries. No second author, no branch in `market_clearing`, no
verb and no control: the *only* path that writes a tariff is the handoff out of pre-history
(§ 4 Tariffs).

That is what closes the ordering this section used to record. `import_tariff` was **vocabulary ahead
of its consumer** — a member of the law-effect enum and of the save format, with the whole duty pass
built and nothing enacting one — the same shape META_LAYER.md's unwired modifier subjects have, and
subject to the same discipline: *a shape is proven by an instance, and an instance is owed*. The
instance is the generated posture, and it arrives from history rather than from a dial, which is the
only form of it § 4 Tariffs would accept.

**`any_import_tariff_enacted` therefore answers a real question now**, and the answer varies by
world: a world whose pre-history produced no protective polity enacts nothing and pays nothing for
the mechanism, exactly as before. No tariff is a legitimate world, not a gap.

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
`apply_budget`** — the treasury holds this quarter's sales, tax and tariff only then; running it inside
`run_economy_step` would spend last quarter's revenue. Four moves, in order, and each is
somebody else's machinery:

1. **Score.** `score_national_budgets` returns a weight vector for exactly the nations **due** on
   this economy tick under the staggered cadence (§ What a nation wants). Each returned slot
   overwrites one entry of `w.nation_budgets`; the rest keep last quarter's weights.
2. **Gather.** This tick's claims are the nation's own: its survey targets, its upkeep bill and its
   `space_programme` purchase claims, derived here (§ A budget). The claimant is always the nation
   itself, its payee a market or its own dispatch.
   Every field of a claim is validated at
   gather time, the line index included: a claim carrying an out-of-range line is dropped whole,
   never clamped onto a line nobody asked for, because the moment a claim arrives over `--serve`
   this is an untrusted input boundary.
3. **Spend.** `run_national_budget` — the pure pass over (weights, claims).
4. **Dispatch the earmark** (Ben, 2026-08-23, NR-568). A paid `public_exploration` claim names a
   body, and `dispatch_survey` spends exactly `survey_cost(body)` of the line's share the same tick —
   so the money never sits loose for the build scorer to spend. A dispatch that cannot proceed (the body was surveyed by someone else this
   very tick) is recorded on the report as a failed earmark and its credit **clawed back** to the
   treasury — a nation does not pay for a survey that did not start.

**Research is the step's last move.** `advance_research` (`src/world/nation_tech.hpp`) runs after
every other move in the step: the academic-research line pays what the queue still
owes, and the pool flows down the queue (`docs/economy/RESEARCH.md` § Research points).

The cadence key is `world::current_econ_tick` — the quarter counter, never the day tick.

**Until the budget verbs exist, the scorer stands in for the player (Ben, 2026-10-16, NR-1048).**
The player's nation has no way yet to set its own weights, so move 1 scores it like a rival and
writes its slot. The rule below holds the moment the budget verbs exist; the stand-in is the gap
between the two, not a second rule.

**The player's nation is never scored (Ben, 2026-10-10).** Its weights are the player's: move 1
skips it, so no scorer ever writes its slot of `w.nation_budgets`, and the player sets the slot
through the budget verbs. Moves 2 to 4 are mechanical — they pay the nation's own claims over the player's own
weights — so the spend is the player's decision carried out, not a decision made for it. Under
spectating there is no human, and the player's nation is scored like any rival. **Nor does the
build scorer act for it** (Ben, 2026-10-23): the player's economy grows only by the player's builds.

**The player's verbs are everything the scorer sets (Ben, 2026-10-11).** The player's nation gets
the four faculties a rival major exercises: **budget weights** over the priority lines, **import
tariffs**, **law** (§ 4. Law authorship), its **build verbs** (§ What a nation owns), and **muster** (`../military/MILITARY.md` § The muster
interface). No faculty a rival nation has is withheld from the human, and none is added: stance and
embargo stay derived and declared as `RELATIONS.md` owns them. Each verb arrives over the AI-facing
seam as untrusted input (the standing rules' seam rule), and each has an entry in `ACTIONS.json`.

### The spend rules

Three rules keep the budget honest, each a requirement row in `nation_budget_harness`:

1. **Every credit out is paid to a named payee** — a market's shelf for goods, the research pool, or
   the nation's own survey dispatch (Ben, 2026-10-23: the corporations the 2026-08-22 rule paid
   directly are retired). A budget line does not bid and does not clear — it debits the treasury and
   credits its payee in the same float, the same tick. `nation_budget_result::paid`, `national_budget_tick::total_transferred` and the
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
   pro-rata partial fill would leave a fraction of a survey — a credit that
   dispatches nothing, exactly the fungible top-up Ben ruled out. So an earmarked claim is paid in
   full when the line's remaining share and the nation's remaining headroom both cover it, and
   otherwise skipped: pay 0, the skip counted on the line (`budget_line_result::skipped`), the line
   flagged `rationed`. Unearmarked claims keep rule 3's partial fill.

Every paid claim lands on the nation's own quarterly return, so its `net()` explains the debit, and
an earmark also lives on `report.earmarks`.

**Determinism.** Once per economy tick, over nations in ascending id, over lines in the fixed enum
order, over each line's claims in ascending (claimant id, arrival index). `w.nations` is an unordered
map and float addition is not associative, so every accumulation
order is pinned. No RNG, no wall clock.

---

## Settled — Ben's rulings, 2026-08-22

The six questions this document opened with were settled in one session. Of them, the channel
between corporations and nations (§ The channel, § 6 to § 8) **retired with the corporations** (Ben,
2026-10-23); the rest stand.

### The channel — lobbying one way, the budget the other — **retired** (Ben, 2026-10-23)

It was a two-way channel between corporations and nations: firms lobbied nations to move their law
and budget, and nations paid firms through the budget. With no firms, nobody lobbies and nobody is
paid; the `lobby` verb and its item retire with it. A nation still answers to other nations and to its
population (`docs/CONCEPT.md` § Player identity).

### 1. What a treasury is for — **a weighted budget over priority lines**

Ben: *"Treasury can be used for things that it is used for in the real world. Logistics
maintenance, Schooling, Military research, Academic research, public space exploration."*

A nation holds **weights, not amounts** — § What a nation holds 3 is this ruling made data. Every
line spends through machinery Io already has. Research in particular converts treasury into the
nation's research points, which buy nodes of the age's tree (`../economy/RESEARCH.md`).

*Owned by BL-537 (national budget) and BL-538 (treasury priority lines).*

### 2. Conservation — **actor-to-actor transfers conserve; the market does not, and that stays**

Ben: *"Nations should be able to save up, and so should corps. Payment to corps is direct and not
on the market."*

**A nation saves**: it does not spend to zero, a reserve is held back, and an underspent line carries
forward. **Every transfer between actors conserves** — it debits one and credits another in the same
float and the same tick. Since the corporations retired (Ben, 2026-10-23) the actors are nations
alone, and the transfers are the tariff (importer to market nation) and the tax (wages back to the
treasury).

**The market is the other half, and it does not conserve.** A nation's buildings sell onto a
market's shelf and buy their inputs as any building does; its units' equipment is bid for on the
order book as a real buyer (Ben, 2026-10-10, NR-1028). `clear_markets` is the buyer of every landing
and pays its owner with nobody's money, so total treasury rises and falls every tick by design. The
two standards are explicitly different things, which is what BL-392's silent value destruction came
from conflating.

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

It also sets the **unit upkeep rate** MILITARY.md carries at `0.0`.

*Owned by BL-543 (value anchor). Divorcing the two halves leaves the wage itself unanchored — that
is BL-544 (unit wage reference), whose likely answer is a multiple of the civilian `base_wage` the
game already pays. Anchor and upkeep first, re-measure, then rule on the levy — doing both at once
makes the benchmark uninterpretable. The levy itself retired on 2026-10-23.*

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

**A polity's protection is a DERIVED output, not a scored verb (Ben, 2026-09-06).** The pre-history
polity does not spend a round choosing protectionism. It carries a protection scalar moved
deterministically by facts it already accumulates — when it industrialised relative to its
neighbours (the axis `derive_national_character` already reads for ideology), and whether it holds
colonies, since a metropole protects its ties. At handoff a polity above the threshold enacts an
ordinary `import_tariff` law on its campaign nation, the rate banded off the scalar.

**The timing term is a PRODUCT of two readings, and neither alone would do.** How much of the field
is ahead — the share of surviving polities that lit a furnace strictly before this one — multiplied
by how far behind it is, its own lag from the world's first furnace as a share of the span from that
furnace to the epoch. Rank alone is uniform by construction: the last polity in a twelve-way field
always scores top whether it lit two years late or never. Lag alone makes every non-industrialiser
saturate, so a world where one polity of twelve industrialises would tariff eleven nations
identically. The product says *behind, and far behind*.

**A world where nobody lit a furnace scores zero for everyone**, and it falls out rather than being
special-cased: with no furnace nobody is strictly ahead of anybody and the share term collapses.
That is the honest reading — protection is a response to an industrial competitor, and a world
without one has nothing to protect against.

**The colony term is owed, not forgotten.** It has no input while a polity cannot claim ground across
water; the exploration age's two claim verbs (`../generation/EXPLORATION.md` § Two ways to claim
ground across water; BL-832, colonial ties) are what give it one, and it lands as an addend on the
same scalar. A purchased province counts as a colony exactly as a conquered one does — a metropole
protects what it bought no less than what it took. A far-flung holding is a large empire, not an
overseas one, so no proxy stands in for it meanwhile.

**A nation's opening treasury is the same span's output.** The wealth scalar the colonial era derives
per polity (`../generation/EXPLORATION.md` § Capital arrives, and it sits in the capital) seeds the balance § 2 opens
with — furnace timing, discoveries, purchases, spoils and lanes — so a rich nation is rich because
its history was, and the spread across nations is reported rather than flattened.

**Rates are banded, blanket, and first-cut.** Three ad-valorem bands above a floor, so the report can
show *how hard* a history protects and not merely *whether* — a single threshold yields one number
per world and cannot show whether the scalar carries variation, which is the measurement the
paragraph below turns on. The directional form — `(author, target, resource) → rate` — stays
BL-541's; scoping the first instance to one resource would be building half of that item here.

The alternative — protection as a **verb** the polity spends a round on, which would make it a real
strategy weighting alongside the others — is held as the **fallback, conditioned on a
measurement**: if the derived form produces a *flat* distribution, where every world tariffs the
same nations to the same degree or none at all, then the scalar is not carrying real variation and
the verb is what would give it some. Flatness is the trigger and it is the only one; the verb costs
a candidate per round and is not bought for elegance.

This keeps the tariff inside the 2026-08-18 nation grant unchanged either way — setting a rate is
named there as a nation power, and a derived posture is pure, seeded and replayable by
construction. And it keeps the player-facing half honest: a home market sheltered because *its
nation's history sheltered it* is a force with a visible cause, which is the standing requirement
that market conditions never come from a term inside an agent.

### 5. What a nation wants — **a positional objective, not an accumulative one**

Ben: *"Nation objectives should be terrestrial diplomacy. Figuring out which economic niche can be
well filled, and how to avoid conflict while respecting historical grudges."*

A nation does not want to be rich; it wants to be **needed and unthreatened**. `src/world/nation_ai.{hpp,cpp}`
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
is much of the point: Conflict lives in the market as much as on the border — in what a nation
builds, what it ships and what it tariffs — and war is the costly last resort.

*Owned by BL-542 (nation scorer).*

### 6 to 8. A nation's stance toward a corporation, the political grant, the lobbying fee — **retired** (Ben, 2026-10-23)

Each was a relation between a nation and the firms on its ground: a derived Access/Trust stance
toward a corporation (BL-540), the grant letting a rival lobby or gate the player's corporation
(NR-517), and the consumed lobbying fee. With no corporations they have no subject. A nation's read
of another nation is `docs/politics/RELATIONS.md`'s.

---

## Where the parts live

| Concern | File |
|---|---|
| The nation record | `src/world/components.hpp` § `nation_component` |
| Territory reverse index | `src/world/world.hpp` § `tile_to_nation` |
| The authored budgets | `src/world/world.hpp` § `nation_budgets` |
| The law object, evaluation, tariff rates | `src/world/law.{hpp,cpp}` |
| The predicate every law reads | `src/world/condition_set.{hpp,cpp}` |
| The tariff transfer | `src/world/market_clearing.cpp` |
| The budget pass | `src/world/nation_budget.{hpp,cpp}` § `run_national_budget`, `budget_priority` |
| The scorer | `src/world/nation_ai.{hpp,cpp}` § `score_national_budgets`, `nation_ai_params` |
| The tick-level caller | `src/world/nation_step.{hpp,cpp}` § `run_nation_step` |
| Law seeding | `src/world/hard_coded_world.cpp` § `seed_prototype_laws` |
| The browse-only Laws surface | `src/ui/balance_ledger.cpp` |
| Save and hash | `src/world/world_save.cpp` (nation and law records), `src/world/world.cpp` § `state_hash` |
| Checks | `tools/verify/law_harness.cpp`, `law_author_harness.cpp`, `money_conservation.cpp`, `nation_budget_harness.cpp`, `nation_scorer_harness.cpp`, `nation_wiring.cpp` |

**Related authorities.** `docs/generation/NATION_GENERATION.md` (how a nation is made),
`docs/economy/FINANCE.md` (the money loop a law's charge is accounted in), `docs/economy/MARKETS.md`
(§ Tariffs, the clearing-tick half), `docs/politics/RELATIONS.md` (sentiment between nations), `docs/economy/CONTRACTS.md` (§ Explicitly out of scope, the sell side
the `contracted_force` line would buy through), `docs/military/MILITARY.md` (§ Nation garrisons, the
`military_research` line's other consumer), `docs/SYSTEMS.md` (§ Policy, § Conditions),
`.claude/rules/io-standing-rules.md` (the grant's exact terms).

**Owning items.** BL-541 (directional tariffs);
BL-543 (value anchor) and BL-544 (unit wage reference) for the price anchor; BL-155 (law/policy
surface) for the four-family taxonomy and the ten-law list; BL-186 (laws ledger UI); BL-555 (who
is paying me) for the earmark surface; BL-572 (contract
offers) for the `contracted_force` line's derivation; BL-571 (nation garrisons) for the
`military_research` line's garrison-upkeep consumer.
