# Finance — the nation's money loop

> **Settles:** what moves a nation's treasury through its buildings each tick, and in which order ·
> which costs recur, in credits and in goods · what a quarterly return reports · what becomes of a
> building its treasury cannot pay for.
> **Not here:** what a good sells for and how that clears (MARKETS) · what a building yields for
> its cost (PRODUCTION) · the treasury's other flows — tariff, tax tier, the national budget
> (../politics/NATIONS.md).
> **Confused with:** MARKETS.md, PRODUCTION.md, ../politics/NATIONS.md.

The money loop: how a nation's treasury moves each economy tick through the buildings it owns,
where the costs come from, and which surfaces read it. There are no corporations and no companies
(Ben, 2026-10-23): every building is its nation's, and its money is the treasury's
(`docs/politics/NATIONS.md` § What a nation owns). The authority for the *market* half of the cash
flows is `docs/economy/MARKETS.md`; the treasury's tariff, tax and budget flows are
`docs/politics/NATIONS.md` § 2.

Code home: `src/world/budget_system.{hpp,cpp}`, breakdown struct in
`src/world/economy_system.hpp`.

## The money loop

Once per economy tick (one quarter), `apply_budget` applies to every nation:

```
treasury += income − expenditure − maintenance − wages − upkeep + taxes
```

- **Income / expenditure** — the market cash flows from `clear_markets`. **Income** is what the
  nation's landings sold for: everything its buildings made, every trade cargo that arrived, every
  cargo it captured, landed on a market and paid at that tick's clearing price — the market is the
  buyer, bid or no bid (`MARKETS.md` § The shelf economy). **Expenditure** is every draw it took
  off a shelf — inputs, materials, upkeep goods, a trade's purchase and its launch propellant —
  billed at the posted price the draw was made at. **The haul is the trader's (Ben, 2026-10-10):**
  a trade's shipment charges its leg cost to the trading nation when it leaves (`SUPPLY.md` § A
  shipment), and the nation keeps the margin between the two prices less that haul (`TRADE.md` § A
  trade).
- **Maintenance / wages** — per-building operating costs, summed (below).
- **Upkeep** — standing-force upkeep (below): the credit half of a unit's upkeep vector.
- **Taxes** — the tier's share of the wages its buildings paid, returned to the treasury (Ben,
  2026-10-23; `NATIONS.md` § Taxes and Discontent). A higher tier lowers the net wage bill.

**Interest and levies retire (Ben, 2026-10-23).** A nation never runs into debt, so there is no
debt charge (§ Debt interest); and the extraction levy was a nation charging a firm, which a nation
cannot do to itself (§ Levies).

**A losing building idles (Ben, 2026-10-23).** When its costs exceed what the treasury can pay, a
building stops producing and paying until the treasury can pay again. The treasury never goes
below zero. **Network nodes are paid first (Ben, 2026-10-22):** hubs, stations and ports are funded
before any other building (`NATIONS.md` § What a nation owns).

**Household consumption is not a flow.** A household draws its basket off the market's shelf and
pays nothing for it (`MARKETS.md` § The clearing tick, step 9). The maker was already paid when
the goods landed and the market bought them, so consumption moves goods, never credits.

The flows are retained per nation in the budget breakdown (BL-072, budget breakdown); its `net()`
is exactly the delta applied to the treasury, so the ledger can never disagree with the loop. The
breakdown sink is optional — the headless harnesses skip it and take only the mutation.

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
- The **goods** half is bought off a market's shelf, run by `run_unit_upkeep`
  (`economy_system.hpp`, `draw_goods_or_bid`) at the posted price under the ceiling (below), in
  ascending unit id — the order is load-bearing, because two units drawing on one shelf draw
  the same stock and the order decides which goes short. It is expenditure like any shelf draw,
  billed by the clear; the credit half above is the wage.

Rates are authored in `scripts/economy.lua` under `economy.military.unit_upkeep`
(`unit_upkeep_params`, declared with the roster in `world/unit_roster.hpp`, since the
cost is per-type data). **Ordnance** is the good; `food_rations` is the sanctioned second
line. A zero entry is skipped by the pass exactly as an absent one is.

**The rates carry a units hazard, and it has bitten once.** They are authored **per head**,
while a unit is raised as a batch (`hire_batch_manpower`) and priced **per unit**. Deriving a
per-head rate against a per-unit hire price therefore inflates the standing cost by the batch
size — which is how a levy spear came to cost 40 credits to raise and 1,200 a year to keep,
and how upkeep came to be **90 %** of a spawning owner's operating outgoings. The
property the derivation must hold, in consistent units: a few years of upkeep on a full batch
stays under the batch's own hire price. Scale both halves of the vector by one factor when
retuning, so the equipment-to-wage ratio the roster is anchored on survives.

**SETTLED (Ben, 2026-10-03): an army eats what its owner can buy at a fair price.** A unit's
rations and ordnance are bought at most at their reservation price; above it the unit draws less
and fights weaker, so a supply shortage weakens an army rather than draining the treasury that
keeps it. **"Draws less" is literal (Ben, 2026-10-03):** a unit buys only the part of its need
that is offered at or under the ceiling, and pays for it at the posted price (below); no fill is
subsidised and no money appears from nowhere. **What the owner can afford is the ceiling alone
(Ben, 2026-10-03):** the draw reads the price, not the owner's treasury. **A draw pays the posted
price (Ben, 2026-10-03):** a draw from a market's shelf is decided and billed at the price that
stood when it was made — the price it checked against the ceiling — with one exchange row at that
price. The shelf's seller is the market, whose suppliers were paid when their goods landed, so the
posted price leaves no unpaid gap, and a unit beside a full shelf is never refused by a price its
own want drove up. **The same ceiling governs every goods draw (Ben, 2026-10-03):** unit and
building upkeep, processor inputs and construction alike buy only at or under it, and a draw over
it does not bid either, so its want leaves the price and the price can ease back. **A nation's
network upkeep and its space programme too (Ben, 2026-10-03):** they draw under the same ceiling
and the same posted price — every draw off a market's shelf, and the shelf is the only stock there
is. Measured on the curated seeds, a 50-head unit cost about 970 a year at the prices the market
actually charged (rations ~3.5x and ordnance ~6x their base) against a hire price near 120, and
standing-force upkeep was what wound most of the field up.

**SETTLED (Ben, 2026-10-03), for the next sprint: hiring answers a threat.** A nation's standing
force follows the hostility it faces — hostile stance or contested ground — rather than filling a
flat cap. It widens the AI-behaviour grant and is recorded in `../ai/AI_OPPONENT.md` § 11 when
granted.

**Until then the rates carry the load (Ben, 2026-10-04, a stopgap — BL-1191, upkeep retune).**
While every owner fills a flat cap whether or not anyone threatens it, the rates are sized so a
guard under no threat does not bleed its owner. Both halves of the vector move by the one factor,
per the retuning rule above, so the equipment-to-wage anchor holds.

**A draw BUYS before it goes short (Ben, 2026-08-26, BL-654).** Before the shortfall rule
below fires, the draw bids its need onto the market and pays for what it gets — so
wanting a good becomes a price signal and somebody has a reason to supply it. It bids only below a
**reservation ceiling**; above that it declines to buy and goes without, which is where the rule
below takes over. One rule for every goods draw: this is the same shape building upkeep takes, not
a second one. The market half is [MARKETS.md](MARKETS.md) § Settled: every draw BUYS.

**The shortfall rule is ONE rule with TWO triggers.** A shelf can be empty, and when the
goods do not arrive the unit *weakens* rather than vanishing. The unit's
`supply_factor_permille` takes the same subtraction whether (a) it is beyond the reach
field (the out-of-supply decay of BL-325, reach as the placement constraint) or (b) its
draw went unmet. Same subtraction, same reason; a met draw and a unit in reach recover
it. **The subtraction stops at the unit's supply share** (BL-1172, fair-price upkeep): out of
reach the share is zero, and a short draw's share is the mean over its goods of the fraction
actually met, in integer per-mille. A unit fed half its draw therefore settles at half
strength rather than starving to nothing, and climbs back toward its share from below.
Deterministic integer arithmetic, no RNG. Because the supply factor feeds
`unit_strength` and the combat adapter, an unsupplied army is measurably weaker **in the
resolver**, not merely more expensive.

**The ceiling is checked once, at the posted price, and the draw pays that price.** The ceiling
is `price_band.reservation_mult` (2.0 — twice what a good is worth at an even market: dear, but a
price a supplied market reaches, where the 10× cap is the signal of a good nobody makes). Every
goods draw from a shelf — unit and building upkeep, processor inputs, construction, and a
nation's network upkeep and space programme — reads the posted price, the price the last clearing
left standing (base, on a market that has never resolved one), takes the good only where that
price is at or under the ceiling, and is billed at it, with one exchange row at that price. The
price the tick goes on to resolve is never what the draw pays. Above the ceiling the good is not
drawn **and not bid for**, so its want leaves the price and the price can ease: a unit or
building goes short and takes the shortfall rule, a processor idles on that input, a trade ships
none of that good, a construction site pauses for want of that material (construction capacity over the ceiling
stretches the build instead, as an empty yard does), and a nation buys elsewhere or not at all.
What a draw is billed is only what the shelf gave.

**The cost of listings-only supply, stated (at k = 0, Ben 2026-10-03).** While
the shelf does not count as supply (MARKETS.md § Price resolution), a draw on a market nobody lists
into prices itself out of a full shelf: its own want is demand against zero supply, the price
resolves from 1.5× toward the 10× cap (5.75× in one tick), the draw is then over the ceiling and
does not bid, and the price eases back under it over three ticks. Measured on a unit and on a
processor beside a full shelf (`unit_upkeep` U13, `fair_price_ceiling` M5): each is fed **one tick
in four** — 3 of 12 — never more than three ticks running without. The processor runs at a quarter
of its capacity. The unit's supply factor nets −50 per four-tick cycle and walks down to a floor
cycle of 100, 50, 0, 0: an army fed only from such a shelf ends near zero strength. A market that
is also being listed into at a rate near its demand does not pulse this way, since its listings
are supply; thin listings do not save it (0.1 listed against a demand of 1 still resolves 1.5× to
2.33×, over the ceiling). The cost is
accepted until spoilage (BL-1179) lets a shelf count without its glut flooring prices.
A ceiling of zero switches the rule off, and off means what each draw did before it: upkeep never
buys; processors, construction and the nations buy whatever the shelf holds.

The pass also carries **orphan cleanup**: `demolish_building` erases the building, its holding
record and the building stockpile but never touches `w.units`, so without it
demolishing a muster base would orphan every unit raised there. Each unit records the
`muster_base` it was raised at, and the pass disbands a unit whose base, tile or owner
has been erased. A unit with no recorded muster base is never disbanded.

Surfaced as its own **Force** bar in the Budget ledger's flow breakdown, with a force line
beneath the net that calls the **unsupplied** count out explicitly, and as the upkeep block on the
unit Selection card's Strength page.
Check: `tools/verify/unit_upkeep.cpp`.

## Levies — the law enforcement seam

**Retired (Ben, 2026-10-23).** The extraction levy was a per-unit charge on a firm's raw output,
credited to the nation whose law enacted it. With every building the nation's own, a levy would be
a nation charging itself, and moves nothing.

The principle it carried stays, and is owned by [`docs/politics/NATIONS.md`](../politics/NATIONS.md)
§ The enforcement seam: *a law is a modifier over the market, never an override of it.* The one
money-moving law left is the import tariff (`NATIONS.md` § The import tariff).

## Building operating cost

`compute_building_opex` is the single source of the formula — the live loop and the
per-building profitability estimate (`building_profit.hpp`, BL-074 building
profitability) both call it, so they cannot drift. The wage/maintenance split is
BL-049 (wage/maintenance split).

- **Material maintenance** — the **idle floor**, `economy.thresholds.idle_maintenance_floor`
  (`scripts/economy.lua`, reached in code as `recipe_registry::idle_maintenance_floor`): the
  fraction of the building's maintenance constant charged even at workforce 0 or decommissioned.
  It is an **authored parameter, not a constant** — BL-739 (idle floor to data) owns the move out
  of code, and the loader rejects a value outside [0, 1] rather than clamping it. Every call site
  passes the registry's value into `compute_building_opex`, which takes it undefaulted so a new
  one cannot fall back to a stale copy. Non-zero deliberately: holding land is never free.
- **Labour maintenance** — the remainder, scaled by `workforce_target` (0–200 internal, shown as 0–100 % of full employment,
  `wt_scalar` clamped [0, 2]); zero when decommissioned, and floored at zero rather than going
  negative where `wt_scalar` falls below the idle floor — there the floor already covers more
  than the scaled total, and maintenance is the floor alone.
- **Wages** — `workforce_assigned × contention_scalar × base_wage × (1 + wage_bid) × wt_scalar
  × hab`. `contention_scalar` is the building's own grant from the wage-competition allocation
  (BL-614, wage competition), not a uniform pool throttle — a building pays for the labour it
  actually won, not the labour it requested, and at the rate it offered. `hab` is the body's
  mean population-centre habitability, clamped [0.1, 2.0] (`body_mean_habitability`).

**A site under construction pays the idle floor and nothing else (Ben, 2026-10-05; BL-1183).**
Until it completes, a building charges no wages and no labour maintenance — only the idle floor,
as a decommissioned building does. Labour demand already skips a site, so a wage there paid a
crew nobody supplied; and a site stalled for want of materials should cost what holding the land
costs, not what running the plant costs. Its materials are paid as they arrive (`PRODUCTION.md`
§ Construction as a rate).

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
- The **goods** half is bought off the shelf of the building's own market, at the posted price
  under the ceiling, in a fixed order, exactly as `run_unit_upkeep` draws — because two buildings
  drawing on one shelf draw the same stock and the order decides which goes short.
- **The shortfall rule is the same rule.** A draw that goes unmet weakens the building rather than
  destroying it, by the same subtraction an out-of-supply unit takes. A factory short of its tools
  runs badly; it does not vanish.

What it changes economically is the point: **every building in the world becomes a consumer**, so
demand scales with how much industry exists rather than with an authored weight somebody has to
maintain. An ancient workshop keeps running on tools and planks; an industrial one on machinery and
electronics. Rates are per building type, authored in `scripts/economy.lua`, and — as with unit
upkeep — a zero entry is skipped exactly as an absent one is, so the shape can land before the
numbers are settled.

## Debt interest

**Retired (Ben, 2026-10-23).** It compounded a firm's negative balance each quarter. A nation never
runs into debt: a building its treasury cannot pay for idles instead (§ The money loop), so there
is no negative balance to charge. The opening working capital that buffered a firm against the
spiral retires with it; a nation's opening money is generation's credit to its treasury
(`NATIONS.md` § 2).

## The quarterly return

One economy tick is one quarter, so the ledger's cadence is the money loop's own. At the end of
`apply_budget` every nation writes a **quarterly return** (Ben, 2026-10-23, re-subjected from the
retired corporation) — the flows it just paid and took, any refund booked since its last return,
its net and closing treasury, and two stock figures. It is a *retain*, not a second computation:
every flow already exists in the budget breakdown, and `net()` is by construction the delta applied
to the treasury, so a return can never disagree with the loop that produced it.

Design: BL-626 (quarterly return record). Ben, 2026-08-26: *"a table ledger that tracks
profitability, updated each quarter with company balance sheets."*

| Field | Source |
|---|---|
| `income` / `expenditure` | the budget breakdown — the market cash flows from `clear_markets` |
| `maintenance` / `wages` | the budget breakdown — summed `compute_building_opex` |
| `upkeep` | the budget breakdown — standing force |
| `taxes` | the tier's share of the quarter's wages, returned to the treasury (`NATIONS.md` § Taxes and Discontent) |
| `refunds` | Credits refunded to the nation since its last return, booked on the next one — a cancelled construction. A credit: `net` = the flows **+ refunds**, and the return's opening is taken from before the refund so it still telescopes |
| `net` | The difference of two consecutive treasuries across the money loop — see below |
| `treasury` | the closing `nation_component::treasury` |
| `holdings` | the nation's building count |
| `book_value` | Sum of `build_cost` over the holdings, from the recipe registry — historical cost, deliberately **not** what the build press charges |

Nothing here is estimated. `holdings` and `book_value` are the two **stock** figures that make the
row a balance sheet rather than an income statement alone.

**Two fields need their reason stated, because the obvious source is the wrong one.**

- **`net` is a treasury difference, not the breakdown's `net()`.** The two are a ULP apart: the
  money loop accumulates its delta interleaved and forbids re-grouping, so only the difference of
  two consecutive treasuries telescopes *exactly*. The retain property below is the whole point of
  the record, and it is worth more than naming a tidier source. The difference is held at **double**
  precision: a single-precision difference can need more significand than a single has, and a
  rounded `net` would neither state the quarter nor replay onto the treasury the loop wrote.
- **`book_value` is historical cost, and deliberately NOT what the build press charges.** The press
  charges `build_cost` **plus** a materials term valued at the *current market price*. Folding that
  in would make the balance sheet mark-to-market — the same building's book value moving every
  quarter with commodity prices. A book value that moves on its own is not a book value.

**Retention is a rolling 40 quarters** (ten years; Ben, 2026-08-26), oldest dropped first, per
nation. Bounded by construction, and that is the point: the record is world state and crosses
the serialisation seam, so it must not grow with campaign length. Ten years carries a trailing
profit read through a full boom-bust while keeping the whole world's history small.

**It is world-side.** `ui::player_plot_history` is a UI cache — the player's nation only, 64
samples, never serialised. The return is none of those things: a reloaded campaign must come back
with its history intact. The player's own profit chart keeps reading the UI cache; that surface is
unchanged.

**Determinism.** Returns append in a sorted walk by nation id, one row per nation per tick, each
row fixed width on the save seam; the exact nets a row's f32 slot cannot hold follow in one trailing
section, listed only for the nations that have one. No branch reads a return during the tick that
writes it, so the record is a pure downstream observation of the loop and cannot feed back into it.

### What a return does not carry — the movers outside the money loop

The money loop is not the only thing that moves a treasury in a quarter. A return records
`apply_budget` and nothing else: its `net` is the treasury's movement **across that call**, plus the
one refund it books (above). Every other debit or credit lands elsewhere in the tick and appears in
**no** return:

- **Construction** — the build press's charge: the site's whole flat credit cost, paid once at
  placement (Ben, 2026-10-25). (The site's materials are bought at the clear, at the shelf's posted
  price, so they *are* in `expenditure` — Ben, 2026-10-26: costs depend on market prices.)
- **Retooling** — the charge for switching a building's recipe.
- **Hiring** — a unit's hire cost and the kit it buys off the shelves — and a unit's
  **movement credits**, priced per Logistic Point drawn.
- **Survey dispatch** — debited upfront.
- **Trade** — the haul a trade pays at dispatch (its purchase at the source shelf is billed at the
  clear and *is* in `expenditure`), and the **import tariff** a nation's goods pay on entering a
  foreign nation's market, credited to that market's nation (`NATIONS.md` § The import tariff).
- **The national budget** — what `run_nation_step` spends on the nation's priority lines, after
  `apply_budget` (`NATIONS.md` § 3).

So a return explains the money loop's movement exactly and the **quarter's** only partly. That is
right for reading the profitability of a nation's industry.

The return carries **no residue field**. An `other` line — the whole-tick delta less the money-loop
`net`, filed after the tick's last mover — was weighed (Ben, 2026-08-26, on NR-655/NR-668) and is
not part of the design.

## Disclosure — who may read a return

**Disclosure by ownership class is retired (Ben, 2026-10-23)** with the class itself
(`docs/GLOSSARY.md` § Ownership class): it decided whether a firm's books were open and whether it
could be bought, and no firm remains. A nation always reads its own return. The banded standing read
stays retired (Ben, 2026-08-26).

## Whole-firm acquisition — the buyout

**Retired (Ben, 2026-10-23).** A buyout acquired a firm whole, priced from its filed returns. With
no firms there is nothing to buy: ground and the buildings on it change hands only by conquest, and
carry their buildings to the new nation (`docs/politics/NATIONS.md` § Own ground).

### Dissolution — the three-way rule

**Retired with the buyout (Ben, 2026-10-23).** It was the TRANSFER / CANCEL / DROP / KEEP rule for
every store keyed by an erased firm. No verb erases an actor now.

## Firm exit — insolvency has a consequence

**Retired (Ben, 2026-10-23).** It wound up a firm whose filed quarters all closed below a balance
floor. A nation never goes insolvent: a building it cannot pay for idles (§ The money loop), and
an idle producer leaving the shelf is what clears a glut.

## Surfaces

- **Budget ledger** — the itemised flow breakdown, the player's nation only
  (`src/ui/balance_ledger.cpp`, `economy_panel.cpp`).
- **Header** — the RUNWAY read ("how long until the treasury runs dry", BL-177 runway) shows only
  while the net is negative — a growing treasury has no runway to run out. See `docs/ui/HEADER.md`.
- **Per-building profitability** — unit economics per building via
  `building_profit.hpp`, surfaced in selection / construction / hover.
- **Profitability ledger** — the filed returns, one row per nation, refreshed each quarter. Its
  buyout press retires with the buyout. Design: BL-627 (profitability ledger); its
  `question_log.json` pair is Ben's wording and is owed before it is built.

## Adjacent design

- Storage caps and per-node throughput are the province of Logistic Points —
  `docs/economy/LOGISTICS.md` § Logistic Points (BL-464).
- Policy levers on the budget (the tax tier, budget laws) belong to the laws arc —
  BL-155 (law/policy surface design) and `docs/politics/NATIONS.md`.
