---
name: market-viability
description: Run the sprint 49 market viability gate (BL-1184) — one 16-seed reading on the shipped 1960 start that says whether the market economy is viable from day 1, against Ben's targets (pooled: >= 85% of built processors running at handoff, <= 5% input-starved, field income at tick 50 >= 50% of the settle close, >= 70% of firms alive at tick 400). Use it to judge ANY sprint 49 economy or generation change, before and after; a world-mover measures its own before/after with it, in isolation.
---

# market-viability

The sprint 49 gate. `tools/verify/market_viability.cpp` builds the **shipped start** per seed
(`build_app_start_world`, `tools/verify/harness_params.hpp`), runs the 12-tick settle as the
app's validation run does, seats the player as `app::seat_player` does, then steps play ticks
as `run_app_live_window` does. It is a **reader**: it writes nothing the simulation reads.

Authority: `docs/economy/MARKETS.md`. The item: BL-1184 (market viability gate) —
`node tools/session/backlog_query.js --grep BL-1184 --full` carries the 2026-10-04 reading it
formalises. Targets: Ben, the sprint 49 form, 2026-10-04.

## Build and run (repo root)

```bash
bash tools/verify/build_lua_harness.sh market_viability      # Lua class: never build_harness.js
./build_gen/verify/market_viability.exe                       # 16 library seeds, 400 play ticks
./build_gen/verify/market_viability.exe --seeds 0,43 --ticks 50   # a quick look (G3 then reads t50)
```

- Default seeds: `docs/generation/seed_library.json`, in library order (market_census's scan).
- `--ticks N`: PLAY ticks after the handoff (default 400). Below 50 the tick-50 rows are
  empty and the run fails its honesty check.
- Never target `%TEMP%`; the exe lives in `build_gen/verify/`.

## How long it takes

The build is ~4 minutes. On an idle machine a seed takes 1.5-5 minutes (world build plus 400
play ticks): the full 16 x 400 is about an hour. Under load it stretches badly — the
2026-10-04 baseline saw seeds of 82 s to 1,697 s, 8,772 s summed. The per-seed header prints
the build / settle / play split.

An agent's background command is capped at 2 hours, and that cap stopped the baseline run
after 10 seeds. On a busy machine, split it: `--seeds` the first ten, then the last six. Each
seed's rows are deterministic, so a split run reads the same per-seed numbers; only the
pooled block is per invocation, so pool the per-seed rows by hand (sum / sum). Hold
keep-awake (`tools/session/keepawake.ps1 -Process market_viability`) on an unattended run.

## What each row means

"Handoff" is the end of the 12-tick settle. "Tick N" is play tick N, counted from the handoff.

- **G1, processors by state**, at the handoff (the settle's last economy report) and at tick 50.
  `run` produced output. `input` is input-starved: the scarcest input covers less than `t_idle`.
  `nolab` has no effective workforce. `unsupplied` has labour but zero batches (supply scalar 0
  or workforce target 0). `decom` is idled (the loss reflex or an idle verb); a second line
  splits it by the state on the last tick it reported — ran, input-starved, other, or never
  reported. `build` is under construction.
  **Share running = run / built processors** (every state but `build`). The share over all
  processors, construction included, is printed beside it. Target: pooled >= 85% at handoff
  (Ben, 2026-10-08; it was 70%).
- **G1b, processors input-starved at handoff** — `input` now, plus `decom` whose last reported
  state was input-starved (mothballed because its inputs never came), over built processors.
  Target: pooled <= 5% (Ben, 2026-10-08). G1 alone counts a plant idled for want of a buyer as
  a failure, which is the economy working; G1b is the starvation the sprint chases.
- **G1 at tick 50** — the same share running, read at play tick 50. Target: pooled >= 85% (Ben,
  2026-10-08): a plant rescued at the handoff only to be mothballed in play does not count.
- **G2, field income per tick** — the sum of every corporation's filed `quarterly_return::income`,
  as WINDOW MEANS: the play 26-50 mean over the 12-tick settle mean. Pooled = sum / sum.
  Target: pooled >= 50%. The single ticks (settle close, tick 50) print per seed as context.
  **Why windows (main session, 2026-10-04).** The settle's last tick comes after the opening
  stock is auctioned and swings 12k-45k across seeds, so the single-tick ratio PASSED the broken
  baseline (64.8%, seed 12 at 204%) while the window form read 27.5%. The baseline file's
  pooled G2 line predates the switch; its window line (27.5%) is the gate's number.
- **G3, firm survival** — of the corporations present at the handoff (seat included), how many
  stand at the last play tick. Pooled = sum / sum. Target: pooled >= 70%. Firms born later are
  not in the cohort; the end count prints beside it.
- **G4, reported, no target.**
  The seat's operating net (income less expenditure, maintenance, wages, levies, upkeep;
  interest excluded) for each of the last 8 settle quarters, and each of its processors with
  how many of those 8 ticks it ran.
  Home-body markets with no exchange in play ticks 1-4 (market_census's CLEARS NOTHING; an
  overflowed exchange ring marks it an upper bound).
  All buildings at tick 50: live, under construction, decommissioned.

The run closes with a per-seed table, the pooled rows, one `PASS`/`FAIL` line per target, and:

```
market_viability: G1 x/70 G2 y/50 G3 z/70 | L to-short a% dry/read b starved/read c top-class K d%
```

The `| L ...` tail is appended by the logistics row (below); the three G fields keep their
place. `--no-logistics` drops the row and the tail.

## The logistics row (L) — BL-1223, reported, no target

**What it answers.** When goods sit on one market's shelf and another market or processor
goes without, which rule of the market-export dispatcher kept them apart? It is the
diagnosis for BL-1217 (inputs reach processors). It is never a gate.

**When it reads.** Play ticks 10, 25, 50 and the last (those within `--ticks`) — several,
never one. Each read is taken after the `run_economy_step` lap, when production,
construction and dispatch have run and the clear has not. That is the instant the
dispatcher itself saw. Per seed, one `L tN` block per read plus an `L all` block over them;
the pooled section repeats this summed over seeds (sum / sum). Figures marked "per read" are
means over the reads in that block.

**The shared classifier.** The classes come from `tools/verify/export_refusal.hpp`, the
reader `water_pair_probe` also uses, so the two tools cannot disagree. It applies the rules
of `export_market_shelves` (`src/world/supply_system.cpp`) to one (surplus market, dry
market, good) triple, in the dispatcher's own order. The class is the FIRST rule that fails:

| Class | Meaning |
|---|---|
| `body` | The destination is on another body. A market has no space lane. |
| `gate` | The price gap fails the gate: `price_d <= price_s x (1 + dispatch_margin)`. |
| `noroute` | No viable export leg (`price_market_export_leg`). `water_pair_probe` splits it into sub-reasons; this row does not. |
| `costly` | Routed, but `price_d - haul - price_s` does not clear the margin. |
| `noroom` | Routed and priced, but absorbable room at the landed cost, less pending and the destination's shelf, is <= 0. |
| `room` | The rule would send. It was held by the one-destination-per-pass rule or the passive-LP cap, or sent elsewhere this tick. |

A destination's **BEST** class is the closest any surplus source came:
`room > noroom > costly > gate > noroute > body`. The row adds labels outside the
dispatcher's order. They are not refusals by a rule:

| Label | Meaning |
|---|---|
| `noshelfsurplus/poolheld` | No market shelf anywhere holds a surplus of the good. Some corporation's pool on the destination's body holds >= 1 unit the corporations' dispatcher would ship. That is `dispatch_convoys`' own surplus: pool less `processor_reservation` less this tick's deliveries (`dispatch_arrived`), outside any standing sell order. It is read on the PRE-STEP pools (after the `convoys` lap): last tick's post-clear leftover. This tick's fresh output is excluded, since any producer on the body would make the test true. |
| `noshelfsurplus/ordered` | As `poolheld`, but every such pool surplus is under a standing sell order. The corp dispatcher never hauls an order-controlled (corp, body, good); the order sells it at home. |
| `noshelfsurplus/none` | No shelf surplus and no shippable pool surplus on the body, pre-step. Nothing to ship. |
| `grid` | A grid good. It is never cargo, so it is not classified. |
| `stocked/thin` | Processor block only. Before the economy step, the processor's market shelf held >= 1 unit, but pool plus the WHOLE shelf covers less than `t_idle` of a full run: `run_processing`'s early idle return, at ANY price. Tested first, so a thin shelf is never counted as a ceiling lock; its "over ceiling" count says how many were also priced over. |
| `stocked/ceiling` | Stocked enough to run, but priced over the BL-1172 fair-price ceiling (`!shelf_admits`, `reservation_mult x base`). The draw does not buy and does not bid. Only this class would run if the price fell. |
| `stocked/contended` | Stocked, admitted and enough. Earlier draws in the step, or the BL-1209 pro-rata share of a short shelf, left this processor too little. |
| `unpriced` | The processor's market has no base price for the input. Unpriced is unbuyable. |
| `nomarket` | The processor has no tile market and draws a body pool. No market-export rule applies. |

**The sets.** A market is **consuming** a good when its households bid it. It is **dry** when
consuming and its shelf holds < 1 unit (`water_pair_probe`'s definition, applied to every
good). Industrial inputs are never household-bid, so they never count as dry; the processor
block covers them instead. A **surplus source** in this row uses the DISPATCHER's own test:
`market_shelf_surplus > 0` and an anchored centre (`export_market_shelves` probes
`min(surplus, 1)`). `water_pair_probe` keeps its >= 1 unit test, so its output does not move.

### The lines, field by field

- **`shipped N convoys U u (shelf export S u) -> to a SHORT mkt a%, to a DRY mkt b%`**.
  These are convoys dispatched this tick: ids above the previous tick's highest, any good,
  corporations' and markets' own exports. `S` is the share that markets exported off their
  own shelves (convoy corp null). A destination is **SHORT** of the good when its shelf at
  dispatch is below its last-clear demand. That is the one tick of want
  `market_shelf_surplus` keeps home. `a%` is the share of units moved from surplus to
  shortage; the rest went to markets that could already cover a tick. `b%` uses the stricter
  dry test.
- **`dry (mkt, good) D of C consuming: good n ...`** — dry pairs, with the top goods by count.
- **`dry by BEST class`** — each dry pair by its best class over every surplus source of that
  good, with its share of dry pairs. A pile on `noroute` / `body` means land-locked from
  every surplus. `noshelfsurplus/poolheld` means the goods exist but are still in pools.
  `noshelfsurplus/none` means nothing to send. A pile on `noroom` / `room` means a route and
  a price exist, and room or pass rules held the goods.
- **`INPUT-STARVED processors P; scarcest input's class at its market`** — every processor
  in G1's `input` state on that tick's report. Each is filed under its scarcest input (the
  report's `limiting_input`) and that input's class at the processor's market. Only nonzero
  classes print. The order of the cuts:
  1. `nomarket`, `grid`, `unpriced`.
  2. `stocked/*`, if the shelf held >= 1 unit in the snapshot.
  3. Otherwise the best dispatcher class, read at dispatch.
  4. Otherwise `noshelfsurplus/*` (`poolheld`, `ordered`, `none`).

  Each class prints **`N over ceiling`**: how many of those processors' inputs are priced
  above the fair-price ceiling. A delivery would not unblock them until the price falls.
- **`stocked/* S (pre-step shelf mean X u) | input priced over the ceiling C | input
  shelf-exported from its market this tick E`** — the stocked total and its mean shelf. `C`
  is all starved processors whose input is over the ceiling, whatever their class. `E` counts
  processors whose input `export_market_shelves` shipped off their own market this tick.
  `UNREAD` counts starved processors that had a market but no pre-step snapshot. It prints
  for a plant completed this tick, which was not live before the step.
- **`starved by input: good n [class n, ...]`** — the same processors listed by input good,
  top goods first, each with its class split.

**When the processor block reads.** The stocked cut uses a SNAPSHOT taken after the
`convoys` lap, before `run_economy_step`. It holds the shelf, the owner's pool and
`shelf_admits`. Later draws in the step and this tick's shelf export therefore cannot hide
stock. Price is written only at the end of `clear_markets`, so the pre-step price is the
price the draw saw. Biases remain:
- The `thin` need is estimated, as `water_pair_probe`'s latent want is. The formula is
  `input x base_rate x effective workforce x workforce target x supply scalar`.
- The pool is read before this step's extraction credits it.
- Between the snapshot and the processor's draw, other things move the shelf and pool.
  Construction sites draw shelves (phase 1). Contract deliveries and drains run (phase 2).
  Sibling buildings of the same corporation draw the same pool. Any of these can swap a
  processor between `stocked/thin` and `stocked/contended`. Read the two together when
  the margin matters.

### How to read the processor-input block (BL-1217)

Read the class split first, then the over-ceiling counts, then the goods.
- **`stocked/ceiling`**: the input sits on the processor's shelf, priced over the fair-price
  ceiling. The fix is the price: why it is so high, and whether the ceiling is right. It is
  not logistics.
- **`stocked/thin`**: a shelf too thin to clear `t_idle`. The fix is more supply into that
  market.
- **`stocked/contended`**: enough stock for this processor alone, but other draws took it.
  The fix is total supply into that market, or the BL-1209 sharing rule.
- **`noshelfsurplus/none`**: no surplus on any shelf or pool. The fix is upstream supply:
  extraction or another processor's output.
- **`noshelfsurplus/poolheld`**: shippable goods sat in pools from last tick and have not
  reached a shelf. Look at listing and auto-surplus, and at corporations' dispatch.
- **`noshelfsurplus/ordered`**: the only pool surplus is under standing sell orders, which
  keep it home by design.
- **`noroute` / `body`**: shelf surplus exists but cannot reach. The fix is the network:
  ports, roads, reach.
- **`gate` / `costly`**: reachable, but the price gradient does not pay for the haul. The
  fix is prices, haul cost, or the margin.
- **`noroom` / `room`**: the dispatcher would send, or nearly would. Look at absorbable room,
  pending cargo, the one-destination-per-pass rule, and the passive-LP cap.
- **Over ceiling in a non-stocked class**: a delivery alone will not help. The price must
  fall too.

The goods list ranks which inputs to fix first. Read it per seed as well as pooled: one
seed's land-locked steel can dominate the pool.

**Pure reader.** No field the simulation reads is written. The router's path caches are
warmed through a `const_cast`; a cache fill computes the answer the next call would. To
check it, run with and without `--no-logistics`. The G1-G4 lines must match apart from
timings.

## Exit code

`0` for a completed reading, **whatever the targets say** — the baseline is expected to fail
them, so they are reported rather than enforced. `1` only when an honesty check fails: a seed
that did not build, no corporations, zero processors at the handoff, zero field income at the
settle close, no seat, or fewer than 50 play ticks.

## How to use it

- **Before and after, in isolation.** A change that moves the world runs the full reading on
  its own base and on its own tip, nothing else merged between. Report which G rows moved and
  why that movement is the intended one. Do not compare against a reading taken on another
  base: main moves, and the baseline moves with it.
- **Read per seed, not only the pool.** A pooled pass can hide one collapsed seed.
- The committed baseline lives at `docs/development/drafts/sprint-49-viability-baseline.txt`;
  it is a dated reading on one base, not a pin.
