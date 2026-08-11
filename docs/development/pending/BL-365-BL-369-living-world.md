# BL-365 … BL-369 — the living world (substrate replacement) — DRAFT

> **Staging note.** Drafted 2026-08-10 on Ben's call: *"My call is to replace the substrate
> entirely."* Same staging reason as the BL-340/BL-350 file — `backlog.json` still carries conflict
> markers from the in-flight BL-293 merge, so these cannot be filed yet.
>
> Ids allocated with `node tools/session/next_id.js` (next safe: **BL-365**). Fold all five into
> `backlog.json` when the merge resolves, then delete this file.
>
> **Warning surfaced by the id scan:** `BL-325` currently means **two different items** —
> `MILITARY_BASE_AND_SUPPLY` on `main`, `CORP_BORDER_HEXES` on `claude/infallible-faraday-93b0ed`,
> the branch being merged right now. That collision has to be resolved as part of the merge, or one
> of the two items is silently lost.

---

## 0. What the measurement showed, and why it matters here

`pregame_balance_harness 80` (run 2026-08-10, the real generated world) produced the trajectory
that motivates all five items:

| Phase | Ticks | Behaviour |
|---|---|---|
| Linear growth | 1–23 | ~5,530 cr/tick, dead straight |
| Knee | ~24 | growth begins decaying |
| Shoulder | 24–46 | decelerating |
| **Plateau** | **47–80** | **~185k cr, oscillating ±60, drifting slightly down** |

**The economy has a carrying capacity and reaches it.** That is the empirical case for this pivot:
the player corp saturates at ~185k not because it has run out of ambition but because the abstract
substrate clears a fixed fraction and there is nothing further to trade against. A market made of
real actors is what turns that ceiling into a contested one.

It also disproves the fear that fixed the warm start at 12 ticks (*"short enough not to diverge
under the prototype's un-tuned economy"*). It converges. `pre_game_ticks` is now **80**
(`src/core/app.cpp`), landed and built.

Two incidental findings from the same run, both feeding items below:

- **A resource sits pegged at exactly 4.00× base in every run** — `food_rations` at 12 ticks,
  `steel` at 80. The harness reports that peg as a PASS for "the fillable gap is lucrative", so the
  assertion currently passes *because* a price is jammed against the band ceiling. That is a
  vacuous green, and BL-365 must not inherit it.
- **Per-tick cost on the real world is ~3.5 ms**, rising to ~4.9 ms by tick 160 — against BL-250's
  0.0018 ms/tick on the small synthetic sweep world. The real world is ~2000× the per-tick cost.
  This is the number the saturation work moves, and § BL-365's sequencing turns on it.

---

## 1. BL-365 — Background industry replaces the abstract substrate

```
id            BL-365
short_name    LIVING_WORLD_BACKGROUND_INDUSTRY
category      Economy
status        design-owed
priority      A
difficulty    5
version_goal  v0.1.13
requires      BL-253, BL-366, BL-368
authority_doc docs/economy/MARKETS.md
files         src/world/market_clearing.cpp, src/world/corporation_generation.cpp,
              src/world/components.hpp, scripts/economy.lua,
              docs/economy/MARKETS.md, docs/generation/GENERATION_STRATEGY.md
```

**Title.** The market saturates because real firms produce and consume, not because a substrate
pass injects supply and demand.

**Summary.** Ben, 2026-08-10: *"we should do more work to fill out a living world, with saturated
markets, and plenty of buildings … this is the only way I can see a saturated market working, where
the player is part of a larger market where most of the mundane trades happen behind the scenes."*
Call taken the same day: **replace the substrate entirely**, rather than keeping it and adding
buildings as texture. Supersedes the approach BL-050 (saturated substrate) and BL-078 (elastic
substrate) shipped.

### What exists today, stated exactly

`inject_substrate_demand` (`market_clearing.cpp`, step 2 of the clearing tick) writes:

- **demand** = `population_weight × demand_basket[r] × elasticity(price)` — a per-capita basket over
  seven authored resources.
- **supply** = `min(capacity[r] × capacity_scale, demand × clearing_fraction)` — an abstract
  nation capacity that tracks demand and clears it only to 90 %, leaving the margin by construction.

No building produces any of it. The saturation is an assertion made once per tick.

### The premise this reverses

`GENERATION_STRATEGY.md` § economic premise states a saturated base whose broad industry **the
Nation AI owns**, with the player and rivals as *specialist* corporations. Corp holdings are lean
*because of* that premise — `holdings_range` gives 3–4 / 2–3 / 1–2 buildings by focus, so eight
corps plus the player put **~16–32 buildings on Kepler's 15,120 tiles** (~0.2 % of the map).

This item rewrites that premise. The doc change is part of the work, not a follow-on.

### Who owns the background industry — settled, and constrained

**More background corporations. Not a nation actor.**

This is not a preference; it is what the standing rules allow. `.claude/rules/io-standing-rules.md`
prohibits nation behaviour (BL-054 stays deferred) while explicitly sanctioning background
**corporations** running a deterministic scored-utility layer over the corp-command seam
(BL-202/BL-203, widened by BL-324). Filling the world with nation-owned industry would need the
one actor class the rules forbid; filling it with firms needs no new exception at all.

### The count is calibrated, not authored

The load-bearing design decision. Do **not** author "place 800 buildings".

Generate background firms and their holdings until the body's real production meets a **target
fraction of its real demand** — first cut **0.90**, deliberately the same figure as today's
`clearing_fraction`, so the opportunity gap is preserved *by construction* rather than *by
injection*. Generation stops on a measured condition, so the world stays saturated when tuning
moves recipes, deposits or population.

This is what keeps BL-078's and BL-112's requirements meaningful under a new mechanism: the same
assertions (differentiated demand, a live margin, a lucrative fillable gap) still hold, but they
are now *emergent* rather than *arranged*. **And the 4× peg noted in § 0 must be an explicit
failure**, not a pass — a good pegged at the band ceiling means supply is absent, which under this
model is a generation bug rather than a lucrative opportunity.

### Sequencing — BL-253 is a hard prerequisite, not an adjacent nicety

BL-253 (`run_corp_strategic_step` rescans every tile per due corp — O(corps × tiles)) is currently
`designed`, priority C, **v0.2.0**. It must move to **v0.1.13 and land first.**

The arithmetic: the term is linear in corp count. This item multiplies corp count by roughly an
order of magnitude. At ~3.5 ms/tick today with 8 corps, and 80 warm-start ticks now running before
the first frame, a 10× corp count against an un-fixed O(corps × tiles) scan turns a ~280 ms warm
start into something in the tens of seconds — paid on every new game, before anything renders.

BL-347 (stack pre-pass tick cost, complete) already records BL-193's restructure roughly doubling
the econ tick with the 128× sweep rung breaching 1 ms. This item pushes the same path much harder.

### Open questions the design pass must settle

1. **Do background firms use `corp_ai` or a cheaper path?** Running the full scored-utility layer
   for ~80 corps is the perf risk above. A reduced per-tick model for background firms — produce,
   sell, maintain, but no build/demolish scoring — may be the honest answer, with `corp_ai` reserved
   for the handful of *rival* corps that actually contest the player. This is probably the single
   most important question here.
2. **Does the player see 80 corporations in every corp-facing surface?** The Corporation lens, the
   dashboard, and the market's preferred-seller list all enumerate corps today. A background/rival
   distinction may need to exist in the UI even if it does not exist in the data model.
3. **What happens to `nation.resource_abundance` and the substrate capacity arrays?** They become
   dead state if the substrate goes. Removing them is a components change; leaving them is a
   different kind of debt.
4. **Does BL-130 (real market inventory) become a prerequisite?** "No stored inventory" is a much
   larger simplification once supply is real — a market that conjures any shortfall undercuts the
   whole point of modelling real producers.

---

## 2. BL-366 — Multi-building tiles: lift the capacity-1 rule for non-extraction

```
id            BL-366
short_name    MULTI_BUILDING_TILE
category      Economy
status        design-owed
priority      A
difficulty    3
version_goal  v0.1.13
authority_doc docs/economy/PRODUCTION.md
files         src/world/placement_rules.cpp, src/world/placement_rules.hpp,
              docs/economy/PRODUCTION.md
```

**Title.** A tile carries several buildings of *any* type, not just several extraction sites.

**Correcting the premise first.** Ben's framing was *"pivot on the design philosophy that a tile is
one building"*. **That is already not the rule**, and the item should not be written as though it
were. `placement_rules.cpp` § `stack_capacity`:

```cpp
if (type != building_type::extraction_site)
    return 1;
const float richness = tc.resource_deposit[target];
return max(1, richness / k_richness_per_site);   // 1–5 by richness
```

and `buildings_on_tile` counts per `(tile, type, target)`. So one tile **already** takes several iron
sites plus several coal sites plus one processor plus one port plus one hub. What is capacity-1 is
every **non-extraction** type — which is the exact half BL-193 (building stacks) deferred rather
than answered: *"Whether processors stack on land, workforce or road tier is a separate question."*

So this item **answers BL-193's deferred question**. It is a completion, not a pivot.

**Why it matters to BL-365, beyond Ben asking for it.** Saturation needs a lot of buildings, and
Kepler's land is a fraction of its 15,120 tiles. One-building-per-tile would spread industry as a
uniform carpet. Multi-building tiles let it **cluster** around population and deposits — an
industrial district rather than a lawn. Clustering is what makes a saturated world legible instead
of noisy, so this is a legibility item as much as a capacity one.

**The design question BL-193 actually left open.** What bounds a non-extraction stack? Extraction
has a natural bound (deposit richness). A processor has none. The three candidates BL-193 named:

- **Land** — a flat per-tile ceiling. Simplest; says nothing interesting.
- **Workforce** — the stack is bounded by what the body's population can staff. Ties into a shipped
  system (workforce contention already throttles an over-built corp uniformly) and makes the bound
  *diegetic*.
- **Road tier** — the tile's logistics capacity bounds what can sit on it. Ties into roads (BL-146–149)
  and makes infrastructure investment raise the ceiling.

**Recommendation: workforce, with road tier as a modifier.** It reuses two shipped systems, it gives
the player a lever (build population / build roads to raise the cap), and it produces the clustering
BL-365 wants without a magic number. Not settled — Ben's call.

**Note the output-decay question.** Extraction stacks pay `0.8^(k−1)` per site, because they share
one deposit. Processors do not share a deposit, so the same decay has no physical justification.
Whether a processor stack decays at all is part of this design.

---

## 3. BL-367 — Building management for a multi-building tile

```
id            BL-367
short_name    MULTI_BUILDING_MANAGEMENT_SURFACE
category      UI
status        design-owed
priority      B
difficulty    3
version_goal  v0.1.13
requires      BL-366
authority_doc docs/ui/SELECTION.md
files         src/ui/selection_panel.cpp, docs/ui/SELECTION.md, docs/ui/question_log.json
```

**Title.** The building-management surface handles a tile holding several buildings of several types.

**Summary.** BL-229 (building management in Menu Space) is `complete` at v0.1.10 and put management
into the construction element's format. Ben, 2026-08-10: *"redesign the building management item to
allow multiple buildings."* Once BL-366 lifts the capacity-1 rule, a tile can hold a heterogeneous
set — several extraction stacks against different deposits, several processors, plus infrastructure
— and the surface has to make that readable and individually actionable.

**Open questions.** Does the surface list buildings flat, or group them by type/target as *stacks*
(matching how `stack_members` already thinks)? What is selected when the player clicks the tile
versus a specific marker? How does the on-canvas marker render a tile with eight buildings without
becoming a blob — ICONS.md owns the glyph vocabulary and would need a "stack of N" treatment.

**Requires `question_log.json` entries** — the pair is mandatory on every changed surface.

---

## 4. BL-368 — Population demand becomes real market demand

```
id            BL-368
short_name    REAL_POPULATION_DEMAND
category      Economy
status        design-owed
priority      A
difficulty    4
version_goal  v0.1.13
authority_doc docs/economy/MARKETS.md
files         src/world/economy_system.cpp, src/world/market_clearing.cpp,
              scripts/economy.lua, docs/economy/MARKETS.md, docs/economy/POPULATION.md
```

**Title.** Population centres generate real, clearing market demand — the half of the substrate that
is genuinely demand rather than fake supply.

**Why it is separate from BL-365.** Removing the substrate removes two things at once: fake supply
(which BL-365 replaces with real firms) and per-capita demand (which has no replacement unless
population centres actually consume). Splitting them keeps each independently verifiable, and the
demand half is the one with a known bug attached.

**The known bug it must fix.** MARKETS.md § Known limitations, verbatim in substance: *"the
population agri demand stub never reaches clearing"* — `run_economy_step`'s population pass writes
`agricultural_produce` demand directly into markets, but `clear_markets` zero-resets demand before
accumulating, so the write is erased the same tick. The real pull today is the substrate basket. Take
the substrate away without fixing this and population demand is **zero**, not restored.

**Scope.** Generalise the erased agri stub into a real per-centre basket across the tradeable set,
written at the right point in the clearing order (after reset, alongside the other demand sources),
with the price elasticity the substrate basket already models moved onto it.

**Open question.** Habitability goods (clean water, consumer goods, medical supplies) are what a
population *should* consume, and none of them exist as `resource_type` values. Either population
demand is restricted to the goods that do exist (food, water, refined fuel), or this item grows a
resource tranche of its own — which would then want reading against BL-340's admission rule.

---

## 5. BL-369 — Does a 20-year warm start move the campaign calendar?

```
id            BL-369
short_name    WARM_START_CALENDAR_SEMANTICS
category      Economy
status        design-owed
priority      C
difficulty    1
version_goal  v0.1.13
authority_doc docs/economy/ERAS.md
files         src/core/app.cpp, src/world/hard_coded_world.hpp
```

**Title.** The warm start runs 80 ticks but consumes no in-game time — decide whether that is right.

**Summary.** Filed from the 2026-08-10 warm-start change. Ben's framing was *"start the game running
from perhaps 1956 or so"* — which reads as the campaign *beginning* earlier. It does not, today.

`app::start_new_game` runs `pre_game_ticks` and then **rebases the clock**, so the warm start costs
no calendar time and play still opens at `epoch_year` (1960). The world is generated *at* 1960 state
and then ticked forward 20 years, with the result relabelled as the 1960 opening position.

**The question.** Is the warm start (a) a settling pass on a 1960 world, with the calendar correctly
untouched, or (b) genuinely the years 1940–1960, in which case generation should produce a **1940**
world and the clock should advance to 1960 during it?

(b) is the more coherent story and much the larger change — it would interact with BL-271's Era −1
history sim, which already runs 0→1960 and would need to stop at 1940 and hand over. (a) is what is
built and is defensible: the pre-game history is a fiction that produces a plausible opening
position, not a simulated two decades.

**Recommendation: (a), documented.** Say plainly in ERAS.md that the warm start is a settling pass
with no calendar meaning. Revisit only if the opening year becomes player-visible or configurable.

---

## 6. Existing items this pivot changes

| Item | Change |
|---|---|
| **BL-253** (`run_corp_strategic_step` O(corps × tiles)) | **Re-goal C/v0.2.0 → A/v0.1.13.** Hard prerequisite for BL-365 (§ 1 sequencing). |
| **BL-050** (saturated substrate, complete) | Superseded in approach. Add a dated note; do not reopen. |
| **BL-078** (elastic substrate, complete) | Same — its *requirements* survive, its *mechanism* does not. |
| **BL-130** (real market inventory, C/v0.1.13) | Promote in priority, or fold into BL-365 — see § 1 open question 4. |
| **BL-132** (market cogeneration, C/v0.1.13) | Reads much more naturally alongside BL-365; consider co-delivery. |
| **BL-193** (building stacks, complete) | BL-366 answers its explicitly deferred question. Cross-reference. |
| **BL-229** (building management, complete) | BL-367 redesigns its surface. Cross-reference. |

## 7. Review entries owed (NEEDS_REVIEW.json)

| Kind | Entry |
|---|---|
| `decision-taken` | **Background industry is corporations, not nations** — the standing rules forbid a nation actor and sanction background corps, so the ownership question is settled by the rulebook rather than by preference (§ 1). |
| `decision-taken` | **Saturation is a calibrated generation invariant** (production ≥ 0.90 × demand), not an authored building count (§ 1). |
| `decision-taken` | **`pre_game_ticks` 12 → 80**, landed in `app.cpp` with the measured trajectory recorded in the comment. |
| `observation` | **Ben's "a tile is one building" premise was not the shipped rule** — extraction already stacks 1–5 per target. BL-366 is BL-193's deferred half, not a pivot (§ 2). |
| `observation` | **The "lucrative gap" assertion passes off a 4× band peg** in `pregame_balance_harness`, at both 12 and 80 ticks. A vacuous green that BL-365 must not inherit (§ 0). |
| `observation` | **`BL-325` is a live id collision** between `main` and the branch being merged. Must be resolved during the merge or one item is lost. |
| `question` | **Do ~80 background firms run the full `corp_ai` scored-utility layer, or a reduced produce/sell model?** The dominant perf question in BL-365 (§ 1 open question 1). |
