# Corporate Finance

The money loop: how a corporation's balance moves each economy tick, where the costs come
from, and which surfaces read it. The authority for the *market* half of the cash flows is
`docs/economy/MARKETS.md`; the authority for the law object behind the levy is
`docs/politics/NATIONS.md`.

Code home: `src/world/budget_system.{hpp,cpp}`, breakdown struct in
`src/world/economy_system.hpp` (`corp_budget`).

## The money loop

Once per economy tick (one quarter), `apply_budget` applies to every corporation:

```
balance += income − expenditure − maintenance − wages − interest − levies − upkeep
```

- **Income / expenditure** — the market cash flows from `clear_markets`
  (`corp_cash_flow`): goods sold, inputs bought.
- **Maintenance / wages** — per-building operating costs, summed (below).
- **Interest** — the debt charge, zero while the balance is non-negative.
- **Levies** — what enacted law took (below). The prototype extraction levy is
  generation-seeded *enacted*, authored by the player's home nation, so it reaches the
  player from turn one (NR-369).
- **Upkeep** — standing-force upkeep (below). Its authored rates are all `0.0`, so the
  term is present and carried but contributes nothing at the authored values.

The seven flows are retained per corp in `corp_budget` (BL-072, budget breakdown); its
`net()` is exactly the delta applied to the balance, so the ledger can never disagree with
the loop. The breakdown sink is optional — the headless harnesses skip it and take only
the mutation.

## Standing-force upkeep

A regiment is not free once hired: `hire_unit` debits once, and the unit then pays upkeep
every tick, exactly as every building beside it pays maintenance and wages. The design is
BL-454 (unit upkeep).

Upkeep is **credits AND military goods** (Ben, 2026-08-17), so the cost is a **vector**,
not a float — a credit wage plus a set of `{resource, qty}` draws. The two halves land
in two different places on purpose:

- The **credit** half is a money flow, so it is `apply_budget`'s own `upkeep` term —
  deliberately *not* folded into `wages`, because a hidden term is a term nobody tunes
  and the ledger has to be able to say what the army costs as against the factories.
- The **goods** half is not money at all. It is a pool debit, run by `run_unit_upkeep`
  (`economy_system.hpp`) against the owner's pool **on the unit's own body**, in
  ascending unit id — the order is load-bearing, because two units of one corp on one
  body draw the same stock and the order decides which goes short.

Rates are authored in `scripts/economy.lua` under `economy.military.unit_upkeep`
(`unit_upkeep_params`, declared with the roster in `world/unit_roster.hpp`, since the
cost is per-type data). **Ordnance** is the good; `food_rations` is the sanctioned second
line. A zero entry is skipped by the pass exactly as an absent one is.

**The rates carry a units hazard, and it has bitten once.** They are authored **per head**,
while a unit is raised as a batch (`hire_batch_manpower`) and priced **per unit**. Deriving a
per-head rate against a per-unit hire price therefore inflates the standing cost by the batch
size — which is how a levy spear came to cost 40 credits to raise and 1,200 a year to keep,
and how upkeep came to be **90 % of a spawning corporation's operating outgoings**. The
property the derivation must hold, in consistent units: a few years of upkeep on a full batch
stays under the batch's own hire price. Scale both halves of the vector by one factor when
retuning, so the equipment-to-wage ratio the roster is anchored on survives.

**The shortfall rule is ONE rule with TWO triggers.** A pool can be empty, and when the
goods do not arrive the unit *weakens* rather than vanishing. The unit's
`supply_factor_permille` takes the same subtraction whether (a) it is beyond the reach
field (the out-of-supply decay of BL-325, reach as the placement constraint) or (b) its
draw went unmet. Same subtraction, same reason; a met draw and a unit in reach recover
it. Deterministic scalar arithmetic, no RNG. Because the supply factor feeds
`unit_strength` and the combat adapter, an unsupplied army is measurably weaker **in the
resolver**, not merely more expensive.

The pass also carries **orphan cleanup**: `demolish_building` erases the building, the
corp asset and the building stockpile but never touches `w.units`, so without it
demolishing a muster base would orphan every unit raised there. Each unit records the
`muster_base` it was raised at, and the pass disbands a unit whose base, tile or owner
has been erased. A unit with no recorded muster base is never disbanded.

Surfaced as its own **Force** bar in the Corporations dashboard's Finance card, with a
force line beneath the net that calls the **unsupplied** count out explicitly, and as
the upkeep block on the unit Selection card's Strength page.
Check: `tools/verify/unit_upkeep.cpp`.

## Levies — the law enforcement seam

> **The law itself is owned by [`docs/politics/NATIONS.md`](../politics/NATIONS.md)** — the record,
> its author, its jurisdiction, and the four-family effect taxonomy. This section owns the
> **accounting half**: where the levy lands in `apply_budget`'s flows and how the player sees it.
> The seam is BL-343 (law enforcement seam).

This is where a **law** touches money, and the rule that governs it is one sentence:
*a law is a modifier over the market, never an override of it.* The extraction levy
(law #1 of BL-155, law/policy surface) is a per-unit charge on raw output, so it applies
where the flow is **accounted** — here, in `apply_budget` — and never where the price is
**resolved** (`clear_markets`). Extraction output is priced by the market exactly as it
would be unlevied; the levy is a separate accounted cost. Two consequences worth stating:
the market stays the only thing that sets prices, and the player sees the tax as its own
number rather than as an unexplained worse price.

Mechanically: `evaluate_laws(w, corp)` resolves every enacted law's `condition_set`
**once per corp per tick**, before the money loop reads it, so evaluation order is
fixed and the determinism invariant holds. The charge then walks that tick's
`economy_report::buildings` and sums `rate × output_quantity` over **extraction sites
only** — a processor's output is not levied, because the ore was already levied once.
Levies from several enacted laws stack additively. `apply_budget`'s `production`
parameter is optional and defaults to null: a caller that omits it charges nothing and
runs the unlevied arithmetic bit-identically, which is why the headless harnesses can
skip it.

Surfaced as its own **Levies** bar in the Corporations dashboard's Finance card, and
read — browse-only — in the Budget ledger's *Laws* section. A law carries an **enacting
nation** and an applicability scope; enactment belongs to the nation actor, and the levy
is a **transfer**, not a sink — the same amount debited from the payer is credited to the
author nation's `treasury`, conserved bit-exactly (`tools/verify/law_author_harness.cpp`).
A corporation cannot flip world law. Authority for the law object itself:
`docs/SYSTEMS.md` § Policy.

## Building operating cost

`compute_building_opex` is the single source of the formula — the live loop and the
per-building profitability estimate (`building_profit.hpp`, BL-074 building
profitability) both call it, so they cannot drift. The wage/maintenance split is
BL-049 (wage/maintenance split).

- **Material maintenance** — a fixed 30 % floor of the building's maintenance constant,
  charged even when decommissioned.
- **Labour maintenance** — the remainder, scaled by `workforce_target` (0–200 %,
  `wt_scalar` clamped [0, 2]); zero when decommissioned.
- **Wages** — `workforce_assigned × contention_scalar × base_wage × wt_scalar × hab`.
  `contention_scalar` is the (corp, body) labour throttle from the economy step — a
  building pays for the labour it actually used, not its target. `hab` is the body's
  mean population-centre habitability, clamped [0.1, 2.0] (`body_mean_habitability`).

Maintenance and wage constants per building type load from the recipe registry
(`scripts/economy.lua`).

### Upkeep is credits AND goods — for buildings too

A building's operating cost is **credits only**, while a unit standing beside it pays credits *and
a goods vector*. That asymmetry is not a design; it is an omission, and it is the largest single
reason the goods roster has more producers than consumers. Design: BL-641 (building upkeep in
goods).

The shape is already built and does not need inventing — it is § Standing-force upkeep's, applied
to the other kind of asset:

- The **credit** half stays where it is, in `maintenance` and `wages`.
- The **goods** half is a pool draw against the owner's pool on the building's own body, in a fixed
  order, exactly as `run_unit_upkeep` draws — because two buildings of one corp on one body draw the
  same stock and the order decides which goes short.
- **The shortfall rule is the same rule.** A draw that goes unmet weakens the building rather than
  destroying it, by the same subtraction an out-of-supply unit takes. A factory short of its tools
  runs badly; it does not vanish.

What it changes economically is the point: **every firm in the world becomes a consumer**, so
demand scales with how much industry exists rather than with an authored weight somebody has to
maintain. An ancient workshop keeps running on tools and planks; an industrial one on machinery and
electronics. Rates are per building type, authored in `scripts/economy.lua`, and — as with unit
upkeep — a zero entry is skipped exactly as an absent one is, so the shape can land before the
numbers are settled.

## Debt interest

A negative balance compounds once per tick by `k_debt_interest_per_quarter` (0.02 —
~2 %/qtr, ≈ 8 %/yr). Non-negative balances are never charged. The constant is the single
source of truth: the live loop and the `econ_bankruptcy` harness read the same value.
Interest is a pure function of balance × rate — deterministic. Design: BL-073 (debt
interest).

## The quarterly return

One economy tick is one quarter, so the ledger's cadence is the money loop's own. At the end of
`apply_budget` every corporation writes a **quarterly return** — the seven flows it just paid, its
closing balance, and two stock figures. It is a *retain*, not a second computation: every flow
already exists in `corp_budget`, and `net()` is by construction the delta applied to the balance,
so a return can never disagree with the loop that produced it.

Design: BL-626 (quarterly return record). Ben, 2026-08-26: *"a table ledger that tracks
profitability, updated each quarter with company balance sheets."*

| Field | Source |
|---|---|
| `income` / `expenditure` | `corp_budget` — the market cash flows from `clear_markets` |
| `maintenance` / `wages` | `corp_budget` — summed `compute_building_opex` |
| `interest` / `levies` / `upkeep` | `corp_budget` — the debt charge, enacted law, standing force |
| `net` | The difference of two consecutive balances across the money loop — see below |
| `balance` | closing `corporation_component.balance` |
| `holdings` | `assets.size()` — the building count |
| `book_value` | Sum of `build_cost` over the holdings, from the recipe registry — historical cost, deliberately **not** what the build press charges |

Nothing here is estimated. `holdings` and `book_value` are the two **stock** figures that make the
row a balance sheet rather than an income statement alone.

**Two fields need their reason stated, because the obvious source is the wrong one.**

- **`net` is a balance difference, not `corp_budget::net()`.** The two are a ULP apart: the money
  loop accumulates its delta interleaved and forbids re-grouping, so only the difference of two
  consecutive balances telescopes *exactly*. The retain property below is the whole point of the
  record, and it is worth more than naming a tidier source.
- **`book_value` is historical cost, and deliberately NOT what the build press charges.** The press
  charges `build_cost` **plus** a materials term valued at the *current market price*. Folding that
  in would make a firm's balance sheet mark-to-market — the same building's book value moving every
  quarter with commodity prices the firm does not own, and the acquisition price swinging with it. A
  book value that moves on its own is not a book value.

**Retention is a rolling 40 quarters** (ten years; Ben, 2026-08-26), oldest dropped first, per
corporation. Bounded by construction, and that is the point: the record is world state and crosses
the serialisation seam, so it must not grow with campaign length. Ten years carries a trailing
profit read through a full boom-bust while keeping the whole field's history small.

**It is world-side, and that is the change.** `ui::player_plot_history` is a UI cache — player corp
only, 64 samples, never serialised. The return is none of those things: the buyout prices every
corporation from it, the spawn shortlist reads it before a player exists, and a reloaded campaign
must come back with its history intact. The player's own profit chart keeps reading the UI cache;
that surface is unchanged.

**Determinism.** Returns append in a sorted walk by `entity_id`, one row per corp per tick, fixed
width on the save seam. No branch reads a return during the tick that writes it, so the record is
a pure downstream observation of the loop and cannot feed back into it.

**What a return does NOT see, stated because it changes what can be priced from one.** The money
loop is not the only thing that moves a balance in a quarter. National transfers and mercenary
contract payouts land *after* it, and construction, hire, survey and convoy spend land elsewhere in
the tick. A return therefore explains the **money loop's** movement exactly and the *quarter's*
movement only partly — which is fine for reading profitability and **not** fine for pricing a firm,
since a corporation earning through contracts would read as unprofitable on its own returns. Closing
that gap is owed work, and it is a precondition of the acquisition price below rather than a detail
of it.

## Disclosure — who may read a return

Every corporation writes a return. **What may be read is decided by the target's ownership class**,
not by the fog: `public` corporations file, `private` and `closed` corporations do not. The class
is generated from the home region's industrialisation timing —
[`CORPORATION_GENERATION.md`](../generation/CORPORATION_GENERATION.md) owns that derivation.

**A corporation always reads its own books.** Disclosure governs what one firm may learn about
**another** — it is a competitor rule, and turning it on its author would be nonsense: a firm that
could not read its own balance sheet could not be run. The observer's own corporation therefore
shows exact figures on every surface regardless of its class, and the class decides only what it
publishes to everyone else. (A closed firm reading its own books and publishing none is exactly
what "closed" means.)

**Otherwise disclosure is binary.** Another public firm's row shows exact figures; another private
or closed firm's row shows its name, focus and class, and a dash where the figures would be. There
is no graded middle:
the banded standing read — negligible / minor / notable / major / dominant — is **retired**
(Ben, 2026-08-26: *"We don't need company information to be invisible"*). So the reason a number is
absent is always that the firm does not file, never that the player has not earned it.

Two consequences fall out, and both are wanted. **Class decides what is legible**, so how readable
a world is becomes a generated fact varying by seed rather than a global setting. And **class
decides what is buyable** (below) — a firm that files is a firm you can price.

## Whole-firm acquisition — the buyout

Ben's call, 2026-08-26: a buyout takes the **whole firm outright**, never a fractional stake. There
is no equity relation, no share count, no controlling-holder threshold, no dividend split — a
corporation has one owner, and the verb moves it in a single step. Design: BL-628 (whole-firm
acquisition).

**The price is read off the target's own returns:**

```
price = max(0, book_value + k_acquisition_multiple x trailing_net + balance)
```

- `trailing_net` is the mean `net` over the target's last **8** filed quarters — two years of the
  forty retained, or fewer if the firm is younger than that.
- `k_acquisition_multiple` is authored in `scripts/economy.lua`, so tuning the acquisition market
  is a data change rather than a code one.
- `balance` is signed. Buying a firm buys its cash **and its debts**, so a leveraged target is
  cheaper by exactly what it owes and a hoarding one costs more.
- **Nothing clamps the profit term.** A chronically loss-making firm prices below its book value,
  and it should — that is the ledger telling the truth about it.
- **A non-finite price is REFUSED, never floored.** The order of those two operations is
  load-bearing and the obvious order is wrong: flooring first turns a `NaN` into a clean `0`, and a
  corrupt filed return then buys a firm for free. Propagate non-finite, *then* floor.
- **A public firm that has never filed cannot be priced, and is refused.** Pricing from an empty
  ledger invents a number, and the natural invention — zero — would make every newly public firm
  free.

**Why the floor is zero, and why it is not book value.** There is no salvage in the prototype:
`construction.hpp` refunds nothing on demolition and names salvage a separate design question, so
book value is not a redemption anyone can take, and flooring there would be an invented one. Zero
is the floor for a mechanical reason instead — **the price is a sink**, the same treatment
construction already gives a build cost (a levy is explicitly a transfer *because* an ordinary
spend is not). A public firm's sellers are a diffuse shareholder base, not a modelled actor, so
there is nobody a negative price could be paid by. A firm priced at zero is worthless, stated
plainly.

### Dissolution — the three-way rule

A buyout is the only verb in the game that **erases an actor**. Everything else moves goods,
credits, units or sentiment; this ends a party that roughly nineteen stores are keyed by. Naming
three of them and leaving the rest to judgement is how a dangling id gets shipped, so the
disposition of every store follows one rule with three outcomes:

| Outcome | What it applies to | Why |
|---|---|---|
| **TRANSFER** | Property — holdings, stockpile pools, the balance, filed returns, units, convoys in flight, trade routes, accepted contracts | It has value and a new owner exists to hold it. A convoy in flight transfers rather than cancels **because its cargo already left the pool** — cancelling would mint the goods back into nothing. |
| **CANCEL** | A promise with nobody left to keep it — open market orders on both sides, unaccepted quotes, live battles | An order is a promise made by a party that no longer exists. Reassigning it would bind the acquirer to a bargain it never struck. |
| **DROP** | An opinion or a permission that was the dissolved firm's alone — its stance rows, its embargo conditions, its modifiers, a contract that would leave the acquirer on both sides | These were *about* being that firm. There is no one left to hold the opinion, and inheriting it would be inventing one. |
| **KEEP** | The historical record — a history entry naming the firm, a battle it fought, a contract it completed | **History is not property and is never rewritten.** A firm that existed and was bought did exist, and a ledger that edits its own past to tidy up a dissolution is worth nothing. This is the one outcome where a reference to the erased corp SURVIVES on purpose, so it is stated here rather than left to look like an oversight. |

The three seams with the most moving parts: `hq_building` / `influence_range` recompute from the
merged holding set (Pass 3b's rule, unchanged), standing units re-point to the acquiring owner
through their recorded `muster_base`, and colliding pools **merge** — stock summed, never replaced.

**Two histories become one.** Filed returns merge pairwise from the newest end, summing flows and
stock figures, because a return carries no tick stamp and concatenation would produce two rows for
one quarter. The result is a pro-forma combined history — which is also what a second buyer prices
the merged firm from.

**Ground follows the firm.** A province held by a corporation is property, not an opinion, so control of
it transfers with the holdings. Holding ground is the sharpest case for the rule above: it is the one
store where leaving a stale id would read as an unowned province rather than as inert data.

**The check that makes this real is not a checklist.** A test that walks the same list the
implementation walks passes whenever the implementation is self-consistently wrong. The harness
instead **re-walks the whole world** and names every store still holding the erased id, on a
fixture loaded with a reference of every kind, on a warm-started generated world, and again after
four further economy ticks — the sharpest of the three, because it proves every downstream pass
survives ticking over the merged world. A store added later fails that row rather than passing
silently.

**The player's corporation is not buyable.** The class rules above are silent on it, and read
literally a public player corp could be erased, leaving `player_entity` dangling. The gate sits at
the command seam rather than in the scorer, because a scorer-side guard would not bind a wire
caller. Whether a rival should ever be able to buy the player out is a live question, and it is the
kind that belongs beside the standing rules on what may be done to a corp a human owns — not a
default fallen into.

**Who may be bought.** A **public** corporation may be bought on the open market at the price
above, with no consent — its books are open, so it can be priced. A **private** or **closed** one
may not: there is no filed return to price it from and no negotiation verb yet. That is the second
job the ownership class does, and it is why the class is worth generating rather than authoring.

**What it costs politically.** Nothing new is needed. `equity_taken` already exists as a sentiment
factor ([`RELATIONS.md`](../politics/RELATIONS.md) § The factors) — *"the observer wanted a firm
and the subject took it"*. An acquisition's political cost lands there, on the rivals who wanted
the same firm.

**Rivals buy too.** `buy_corporation` joins the `corp_command` seam and is scored in
`corp_ai.cpp`'s existing deterministic candidate list, capped at one acquisition per evaluation and
carried in the candidate's `spend` under the solvency gate — the shape every widening in
`.claude/rules/io-standing-rules.md` takes, a legal verb on the scorer and never a planner.
Design: BL-629 (rival acquisition).

## Surfaces

- **Budget ledger** — the itemised flow breakdown, player corp only
  (`src/ui/balance_ledger.cpp`, `economy_panel.cpp`).
- **Header** — the RUNWAY read ("how long until underwater", BL-177 runway) shows only
  while the balance or net is negative — a growing balance has no runway to run out;
  plus the `[in debt]` flag. See `docs/ui/HEADER.md`.
- **Per-building profitability** — unit economics per building via
  `building_profit.hpp`, surfaced in selection / construction / hover.
- **Profitability ledger** — the whole field's filed returns, one row per corporation,
  refreshed each quarter, and the host of the buyout press. It is the only surface that reads
  another corporation's return, and it reads only what that corporation files. Design: BL-627
  (profitability ledger); its `question_log.json` pair is Ben's wording and is owed before it
  is built.

## Adjacent design

- Storage caps and per-node throughput are the province of Logistic Points —
  `docs/economy/LOGISTICS.md` § Logistic Points (BL-464).
- Policy levers on the budget (taxes, budget laws) belong to the laws arc —
  BL-155 (law/policy surface design) and `docs/politics/NATIONS.md`.
