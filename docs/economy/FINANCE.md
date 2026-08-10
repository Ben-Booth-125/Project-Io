# Corporate Finance

The money loop: how a corporation's balance moves each economy tick, where the costs come
from, and which surfaces read it. This doc was carved out in the 2026-07-31 doc sweep —
the budget system had shipped (BL-072 budget breakdown, BL-073 debt interest, BL-074
building profitability) with no authority doc owning it. Everything below is implemented;
the authority for the *market* half of the cash flows is `docs/economy/MARKETS.md`.

Code home: `src/world/budget_system.{hpp,cpp}`, breakdown struct in
`src/world/economy_system.hpp` (`corp_budget`).

## The money loop

Once per economy tick (one quarter), `apply_budget` applies to every corporation:

```
balance += income − expenditure − maintenance − wages − interest − levies
```

- **Income / expenditure** — the market cash flows from `clear_markets`
  (`corp_cash_flow`): goods sold, inputs bought.
- **Maintenance / wages** — per-building operating costs, summed (below).
- **Interest** — the debt charge, zero while the balance is non-negative.
- **Levies** — what enacted law took (BL-343, below). Zero unless a law is enacted
  that reaches this corp, which is the shipped default.

The six flows are retained per corp in `corp_budget` (BL-072); its `net()` is exactly
the delta applied to the balance, so the ledger can never disagree with the loop. The
breakdown sink is optional — the headless harnesses skip it and take only the mutation.

## Levies — the law enforcement seam (BL-343)

This is where a **law** touches money, and the rule that governs it is one sentence:
*a law is a modifier over the market, never an override of it.* The extraction levy
(BL-155's law #1) is a per-unit charge on raw output, so it applies where the flow is
**accounted** — here, in `apply_budget` — and never where the price is **resolved**
(`clear_markets`). Extraction output is priced by the market exactly as before; the
levy is a separate accounted cost. Two consequences worth stating: the market stays
the only thing that sets prices, and the player sees the tax as its own number rather
than as an unexplained worse price.

Mechanically: `evaluate_laws(w, corp)` resolves every enacted law's `condition_set`
**once per corp per tick**, before the money loop reads it, so evaluation order is
fixed and the determinism invariant holds. The charge then walks that tick's
`economy_report::buildings` and sums `rate × output_quantity` over **extraction sites
only** — a processor's output is not levied, because the ore was already levied once.
Levies from several enacted laws stack additively. `apply_budget`'s `production`
parameter is optional and defaults to null: a caller that omits it charges nothing and
runs the pre-BL-343 arithmetic bit-identically, which is why every headless harness
was untouched by the change.

Surfaced as its own **Levies** bar in the Corporations dashboard's Finance card, and
switched on from the Budget ledger's *Laws* section. Authority for the law object
itself: `docs/SYSTEMS.md` § Policy.

## Building operating cost (the BL-049 split)

`compute_building_opex` is the single source of the formula — the live loop and the
per-building profitability estimate (`building_profit.hpp`, BL-074) both call it, so
they cannot drift.

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

## Debt interest (BL-073)

A negative balance compounds once per tick by `k_debt_interest_per_quarter` (0.02 —
~2 %/qtr, ≈ 8 %/yr). Non-negative balances are never charged. The constant is the single
source of truth: the live loop and the `econ_bankruptcy` harness read the same value.
Interest is a pure function of balance × rate — deterministic.

## Surfaces

- **Budget ledger** (BL-072) — the itemised five-flow breakdown, player corp only
  (`src/ui/balance_ledger.cpp`, `economy_panel.cpp`).
- **Header** (BL-177 runway, BL-073 in-debt flag) — the RUNWAY read ("how long until
  underwater") shows only while the balance or net is negative — a growing balance has
  no runway to run out; plus the `[in debt]` flag. See `docs/ui/HEADER.md`.
- **Per-building profitability** (BL-074) — unit economics per building via
  `building_profit.hpp`, surfaced in selection / construction / hover.

## Open ends

- Storage caps and per-node throughput charges are unowned (see
  `docs/economy/PRODUCTION.md` § logistics note).
- Policy levers on the budget (taxes, budget laws) are owed to the laws arc
  (BL-155, law/policy surface design).
