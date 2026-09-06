# Next session — sprint 32b, the generation-side market chain

Sprint 32b was re-planned 2026-09-06 on Ben's call. The market-work elicitation form settled **eight
open calls**, and the scope answer moved the sprint's ordering: **the generation-side market chain
leads, not the water wave.** `docs/development/SPRINTS.md` § Sprint 32b carries the full re-plan;
this note is the handoff.

## Start here

**BL-770 (Era 0 candidate search), first slice only — and it is the only reachable item.**

Slice out the **scorer** and nothing else. Score a handful of hand-made candidate rosters and measure
whether the three terms **discriminate between them at all**. The job of this slice is to fail
informatively: if chain completeness is flat across every candidate, the search has nothing to search
on and everything downstream of it is wasted. That answer is worth an afternoon; it is not worth the
parallel harness.

The objective, as Ben set it (`GENERATION_STRATEGY.md` § The eight phases):

1. **Chain completeness** — terminals closed over terminals total, per market.
2. **The supply-to-demand ratio** per resource per market — the static price-feedback proxy, and the
   only term that separates a saturated market from one whose good is pinned at a band edge.
3. **The spread of both across markets, explicitly rewarded for unevenness.** Scored for, not merely
   tolerated.

**Recipe margin is deliberately not a term** — it is the authoring gate the roster passes *before*
the search runs, not an axis the search trades against. Both measures already exist and BL-775 put
them where generation can link them.

## The gate Ben accepted, and what it costs

BL-770 was gated on sprint 33 showing valued production flat or rising. It is not: production falls
**×0.2 on seed 0 and ×0.6 on seed 1** across the standard lapse. A search over a shrinking field
selects the **least-bad shrinking economy** — it ranks degrees of failure and still names a winner.

Ben took that knowingly. The discipline it buys back: **phase 6 candidate scores in this state are
ordinal and provisional**, never evidence that the chosen landscape is viable, and whatever the slice
reports must say so on its face. The scorer slice is the part least exposed to this, which is what
makes the ordering survivable.

## What is NOT reachable, and why

| Item | Blocked on |
|---|---|
| BL-772 (retire warm start) — the 72 s budget win | BL-770 |
| BL-768 (roads and markets from history) | BL-766 (population map early), difficulty 5, not started |
| BL-750 (tariff posture) — design now settled | BL-748 (industrial pass ladder), not started |
| BL-752 (colonial ties) — require repointed to BL-770 | BL-749, itself held on the five calls at NR-785 |

The honest read: this ordering buys the phase 6 scorer experiment now, and the rest of the chain
still waits on phase foundations that are not market work at all.

## The eight calls, settled 2026-09-06

Five are written into authority docs; all eight are on their items.

- **BL-746 stage 2 — no price, no draw.** A building draws a grid good only once its catchment market
  has priced it. `PRODUCTION.md` § A shortfall scales output. **The done-when is a pair** — mean
  supply factor off 0.57 *and* the grid good's price real and moving — because the gate silencing the
  draw is indistinguishable from the gate working if you read the supply factor alone.
- **BL-745** — both purchase leaks in one item, ordered after BL-746 stage 2.
- **BL-782 (new)** — the agency's idle rule reads operating net at the recipe's own bid cap, split out
  of BL-745 on Ben's call.
- **BL-738** — industry rates wait for the field to hold power. Its stage 1 figures are void: they
  were taken pre-floor, pre-no-wire and pre-price-gate.
- **BL-751 — cancelled superseded.** Its parts live on BL-770 (selection, stop condition, the spread)
  and BL-772 (the retirement, and the settled-position-at-the-epoch property that must survive it).
- **BL-770** — the objective above; scorer first.
- **BL-750** — protection **derived** at handoff. The scored-verb form is held with **flatness as its
  only trigger**. `NATIONS.md` § 4 Tariffs.
- **BL-730** — `trade_goods_misc` joins the endemic luxury basket, with the asymmetry dilution stated
  in `MARKETS.md` rather than left to be discovered.

`GENERATION_STRATEGY.md` § Three passes was rewritten in the same pass: it still described pass 3 as
"the warm start, promoted", which BL-751's cancellation makes a fiction. Pass 3 now **selects** a
landscape rather than settling one.

## The water wave is deferred, not withdrawn

BL-776 → BL-780 keep their shape, their ordering and their captured before-figures. BL-780 still
exists so the water model moves every world **once**. Do not let 776–779 each re-bless.

```
world_determinism   039EE9880739CDF6 / B0EBBA249B3DDABB / DE55600457797638
era report seed A   years=400 battles=270 conquests=207 foundings=833
1960 two-span       1160 -> 1560 -> 1960, digest DB86651B9A596F7B
sim_water_census    1105 of 3819 regions on water (982 sea, 613 OPEN OCEAN)
                    43% of adjacency edges cross sea (54% / 22% / 57% by seed)
warm start          72-73 s on BOTH arcs, ~12x the ~6 s app.cpp budgets
generation          ~8.2 s Release; the era pass itself only 197-323 ms
```

## Calls still waiting on Ben

These were **not** on the market-work form and remain open. None blocks the scorer slice.

- **NR-783** — is the span boundary authored at epoch − 400, or derived from the first furnace?
  Gates BL-748, and therefore BL-750's sequence.
- **NR-784** — should the ancient arc be capped at medieval? Measure the band distribution first.
- **NR-785** — BL-749's five design calls, which hold the sea leg and therefore BL-752.
- **BL-758** — does era-seeded demography at 1960 belong? `era_world_harness` R2 is deliberately
  **red** for it. Do not weaken it to pass.
- **Water's 0 forage** — blockade pressure, or an accident of a table written for land?
- **Can a coastal province hold a port?** Buildings currently refuse water outright.

## Debts from 32a, stated rather than hidden

- **The batch's cross-slice review barrier never ran.** Sub-agent capacity was unavailable for the
  whole session — 12 launches, every one a 529 with zero tool calls — so each slice was verified
  individually instead. If agents are available, run a cold review over the five delivered items.
- **Three harnesses are ad hoc** pending Ben naming them in `.claude/skills/verifier-headless/`:
  `continent_drift`, `sim_water_census`, and the promoted saturation measure.
- **BL-774** — worktree agents cannot build the Lua-linked harness class, which includes
  `world_determinism`. Every agent inheriting the byte-identity invariant pays that toll first.

## Two traps this session hit, worth not repeating

**A green check is not evidence that it looked.** Four instruments were measuring something other
than their subject, and one of them (`era_world_harness`) let a real regression through a wave I had
called verified.

**A requirement written after the code describes the code, not the intent.** BL-775 said "promote
both"; half was promoted and everything went green on it. Write the group from the item, first.

**And a filed premise goes stale.** BL-770's prose still poses a span question that
`GENERATION_STRATEGY.md` dissolved when phase 6 became a static search. Read the authority doc before
trusting an item's framing.
