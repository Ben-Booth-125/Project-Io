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
line. Every rate — `credits_per_head`, `credits_per_head_per_power`, and both
`goods_per_head` entries — is authored at **`0.0`**: the shape is complete, and turning
upkeep on is editing a number rather than adding a line. A zero entry is skipped by the
pass exactly as an absent one is.

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

## Debt interest

A negative balance compounds once per tick by `k_debt_interest_per_quarter` (0.02 —
~2 %/qtr, ≈ 8 %/yr). Non-negative balances are never charged. The constant is the single
source of truth: the live loop and the `econ_bankruptcy` harness read the same value.
Interest is a pure function of balance × rate — deterministic. Design: BL-073 (debt
interest).

## Surfaces

- **Budget ledger** — the itemised flow breakdown, player corp only
  (`src/ui/balance_ledger.cpp`, `economy_panel.cpp`).
- **Header** — the RUNWAY read ("how long until underwater", BL-177 runway) shows only
  while the balance or net is negative — a growing balance has no runway to run out;
  plus the `[in debt]` flag. See `docs/ui/HEADER.md`.
- **Per-building profitability** — unit economics per building via
  `building_profit.hpp`, surfaced in selection / construction / hover.

## Adjacent design

- Storage caps and per-node throughput are the province of Logistic Points —
  `docs/economy/LOGISTICS.md` § Logistic Points (BL-464).
- Policy levers on the budget (taxes, budget laws) belong to the laws arc —
  BL-155 (law/policy surface design) and `docs/politics/NATIONS.md`.
