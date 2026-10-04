# Sprint 49 — why no surplus crosses a market (BL-1186 diagnosis)

**Diagnosis only.** Nothing in `src/` or `scripts/` moved. The probe is `tools/verify/steel_chain_probe.cpp`.
It now prints a `[BL-1186]` block at the real decision point: after `run_economy_step` and before
`dispatch_convoys`. The block breaks every refused (source, destination) pair down by the first rule
it fails, and it covers both senders: a market's own shelf (`export_market_shelves`) and every
corporation's pool (`dispatch_convoys`). It is a pure reader; only the A* and flood caches warm.

```
bash tools/verify/build_lua_harness.sh steel_chain_probe
build_gen/verify/steel_chain_probe.exe --seeds 0,43,37 --ticks 21 --quiet --good steel   # or coal
    [--detail-ticks -1,0,...,20]   # sample pairs, the market table, the to-WANTING verdicts
```

**Where "handoff" sits.** The probe's ticks 0–11 are the 12-tick validation settle; the seat is handed
over after tick 11. "Tick 12 pre-dispatch" below is the first play decision. Tick -1 is the built world
before the settle. The brief's figures (52 / 135 / 29) are the probe's old post-clear line at tick 0.
That line is the same read as the new block's tick 1. Ticks 12–20 here are spectated, not seated.

## The short answer

Four mechanisms stack, and each one alone would stop most of the flow.

1. **The price gate sees no gradient.** Every steel-consuming market runs the same four-tick cycle,
   in phase. A destination bids on one tick in four. On that tick its same-landmass sources stand at
   the same price. (§ A)
2. **Unroutable means "different landmass, no port".** The port gate wants a Port on the exact origin
   tile and on the exact destination centre tile. No market centre has one. (§ B)
3. **The corporations stop owning the steel.** The first dispatch runs before any clear has written
   demand, so it sees flat prices. That clear then sells the opening stock onto home shelves. (§ F)
4. **Latent:** the passive-LP cap is all-or-nothing at 20 u per anchor per tick. Once 1–3 open, any
   cargo over 20 u is refused whole. (§ E)

## Counts per seed (shelf = market export; pairs over home-body markets)

Columns are the first rule failed. "Gate" is `price_d <= price_src x 1.05`. "No dem" means the
destination had zero registered demand.

| seed | tick | good | sources / surplus | gate (no dem) | unroutable (water, no port) | margin | no room (no dem) | would send |
|---|---|---|---|---|---|---|---|---|
| 0 | 1 | steel | 9 / 10,274 | 52 (36) | 135 (135) | 0 | 29 (29) | 0 |
| 0 | 10 | steel | 9 / 10,577 | 184 (182) | 25 (25) | 0 | 7 (1) | 0 |
| 0 | 12 handoff | steel | 9 / 11,073 | 187 (186) | 22 (22) | 0 | 7 (6) | 0 |
| 0 | 20 | steel | 9 / 10,166 | 186 (185) | 22 (22) | 0 | 8 (7) | 0 |
| 43 | 1 | steel | 7 / 10,703 | 32 (20) | 65 (65) | 0 | 22 (22) | 0 |
| 43 | 10 | steel | 7 / 11,146 | 104 (104) | 11 (11) | 0 | 4 (2) | 0 |
| 43 | 12 handoff | steel | 7 / 10,914 | 104 (104) | 11 (11) | 0 | 4 (4) | 0 |
| 43 | 20 | steel | 8 / 8,835 | 107 (107) | 21 (21) | 0 | 8 (8) | 0 |
| 37 | 1 | steel | 9 / 9,612 | 58 (58) | 65 (65) | 0 | 102 (102) | 0 |
| 37 | 10 | steel | 7 / 10,680 | 140 (140) | 22 (22) | 0 | 8 (8) | 5 pairs, 31.5 u |
| 37 | 12 handoff | steel | 9 / 9,652 | 184 (180) | 21 (21) | 0 | 20 (11) | 0 |
| 37 | 20 | steel | 9 / 8,692 | 184 (180) | 21 (21) | 0 | 20 (11) | 0 |
| 0 | 12 handoff | coal | 10 / 2,174 | 79 (79) | 133 (133) | 7 | 21 (21) | 0 |
| 0 | 20 | coal | 10 / 3,398 | 75 (73) | 136 (136) | 7 | 22 (21) | 0 |
| 43 | 12 handoff | coal | 5 / 1,181 | 24 (24) | 41 (41) | 1 | 19 (19) | 0 |
| 43 | 20 | coal | 5 / 1,878 | 21 (20) | 44 (44) | 1 | 19 (19) | 0 |
| 37 | 12 handoff | coal | 6 / 1,317 | 35 (35) | 19 (19) | 8 | 88 (88) | 0 |
| 37 | 20 | coal | 6 / 2,333 | 47 (47) | 16 (16) | 11 | 76 (76) | 0 |

**Corporation pools.** Steel at tick -1 sits in 123–128 pools holding 12.5–13.3k on the three seeds.
By tick 10 the corporations hold only their processor reservations: 13–19 pools, all wholly reserved
(seed 0 at tick 12: 19 pools, 216.6 u). At tick 20, 0–1 pools hold any steel surplus, at most 6 u.
Coal surplus stays in corporation pools: 92–151 u across 22–60 pools. Its refusals have the same shape:

- **Seed 0, tick 20:** unroutable 420, gate 136, no room 110 (106 with no demand), margin 6.
- **Seed 37, tick 20:** no room 860 (all no demand), unroutable 174, gate 122.

**Sub-cases that never occur.** The probe tests every one of these, and every count is 0 on all three
seeds: "no path", "no anchor tile", "cross-body", "non-finite cost", "endpoint tile is water", and
"landed cost at or over the ceiling".

## A. Price gate — the destination is silent three ticks in four

The gate refuses a pair when `price_d <= price_src x (1 + 0.05)`
(`supply_system.cpp:1041-1043` market export; `:1300-1305` corporations; margin `economy.lua:1584`).
From tick 10 on, 98–100% of refused pairs have a destination with **no registered demand** (62–100%
at tick 1). The mean source price runs
at 2.3–3.7x base and the mean destination price at 1.2–1.5x. Most destinations want no steel at all,
and refusing those is correct. The trouble is the markets that *do* want it.

**The cycle**, read off the market table (seed 0, m47753: no shelf, 109–317 steel-needing sites):

| tick | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | … |
|---|---|---|---|---|---|---|---|---|---|---|
| price / base | 5.50 | 3.25 | 2.12 | 1.56 | 5.78 | 3.39 | 2.20 | 1.60 | 5.80 | … |
| demand | 160 | 0 | 0 | 0 | 870 | 0 | 0 | 0 | 1,090 | … |

It is exact arithmetic on four rules:

1. **The shelf is not supply.** `pricing_supply` with `shelf_supply_ticks = 0` reads listings only
   (`market_clearing.hpp:265-272`, `economy.lua:1400`).
2. **A bid with nothing listed aims at the ceiling.** `price_target` returns `base x ceil_mult`
   (10x) for demand with no supply (`market_clearing.cpp:35-36`, `economy.lua:1391`).
3. **The price eases halfway each tick.** EMA 0.5 (`market_clearing.cpp:25`, `:64`):
   1.6 → 5.8 → 3.4 → 2.2 → 1.6.
4. **A draw over the fair-price ceiling does not bid.** The ceiling is 2.0x base (`shelf_admits`,
   `components.hpp:1070-1077`; `economy.lua:1392`). Construction drops its want at
   `economy_system.cpp:779-786`, and processors drop theirs at `:363-365`. So for the three ticks
   at 5.8, 3.4 and 2.2 the market registers zero demand and the price eases.

**Every steel market on a landmass runs this cycle in phase.** They start together and obey the same
dynamics. On seed 0, m47652/m47664/m47689/m47697/m47812 share m47753's landmass, and on m47753's bid
ticks (9, 13, 17) all five stand at 5.4–5.8x too. The to-WANTING verdicts show it:

> `GATE src m47664 -> m47753: p_src 93.03 … gate 97.68 p_d 93.08 … d.demand 870`

The destination is the dearest market on the continent for one tick in four, and on that one tick
every neighbour that could ship is exactly as dear. Seed 43 shows the same cycle (m47537, m47544).

**The same rules strand shelves in place.** m47652 (seed 0) holds 1,188–1,527 steel and up to 51 sites.
Its own bid takes it to 5.8x, and from then on its own sites may not draw its own shelf
(`admits 0`). Seed 43's m47544 does the same with 97 u. This is the BL-1183 (ceiling stalls
construction) mechanism; it and BL-1186 are one defect seen from two ends.

**The rest of the gate count** is "destination below source": 176 of 186 at seed 0, tick 20. Shelf
markets price themselves *up*, because their shelf is not supply and their consumers bid. So the
stock sits in the markets with the highest posted price on the body.

## B. Unroutable — separate landmasses, and the port gate's exact-tile test

Every unroutable pair, on every seed, tick and good, is one case. The cheapest path crosses water, and
there is no Port on both the origin tile and the destination centre tile (`supply_system.cpp:476-489`,
the test at `:488`).

- **For shelves the two markets are on different landmasses.** A land-only flood
  (`bounded_cost_to_tile(..., land_only=true)`) reaches the destination from the source centre in only
  0–5 pairs per tick.
- **Seed 0 has 4 active Ports, seed 43 has 3.** No market centre carries one (`port 0` on every shelf
  source).

**A doc–code divergence.** `SUPPLY.md` § Infrastructure gates puts the Port "at each end of the sea
leg — not at the source or destination market, which may lie inland behind a land leg". The code
wants the Port on the market centre tile itself, so a route of land leg, sea leg, land leg is never
built.

**Secondary: a pair is refused even when a land route exists.** The cheapest path may cross water
while a dearer land path exists. A sea lane prices water at 1.25, under a mountain's 2.0. The code
refuses that pair rather than re-pricing it on land. The comment at `:479-485` says there is no
cheaper land-only route, which is true, but there may be a dearer one. It matters mostly for
corporation pools, whose origin is their own building (`convoy_origin_tile`):

- seed 43 steel at tick -1: 95 of 453 water refusals have a land path;
- seed 37 coal at tick 20: 33 of 174;
- shelves: 0–5 per tick.

## C. No room — mostly the same silence

`dispatch_absorbable` answers `D - S` when nothing is listed, and 0 when demand is 0
(`supply_system.cpp:887-898`). With the destination silent (§ A), room is 0. That explains 85–100%
of the no-room refusals.

The remainder is the destination's **own shelf** at or above its absorbable amount. That is the
export rule's shelf term (`:1074-1075`), and it is correct: the destination already holds the good.

## D. Net under margin — not the lever

- **Steel:** 0 pairs on every seed. The land rate is 0.02 per path-cost unit (`economy.lua:1559`), so a
  haul is about 0.2–1.3 per unit against prices of 16–93.
- **Coal:** 1–20 pairs per tick. Base is 2.0–2.5 and the haul is 0.2–0.7 per unit. Moving
  `dispatch_margin` (0.05) would change almost nothing.

## E. Passive LP — not reached today, binding the day the gate opens

Every anchor carries a flat 20 u per tick (`logistics.cpp:634-641`, `economy.lua:877`), and a convoy
draws its whole quantity from its nearest anchor. `passive_lp_admit` refuses the convoy outright when
the anchor holds less than the quantity (`supply_system.cpp:721-730`). The caller then gives up on
that (pool, good) for the tick:

- the market-export pass breaks at `:1091-1093`;
- the corporation pass breaks at `:1363-1365`.

So **no single convoy can carry more than 20 u**. A larger cargo is refused whole, never trimmed.
m47753's room on a bid tick is 870–1,900 u, and the steel sources hold 100–3,900 u each. Steel
refusals never reach this test today, because § A–B refuse them first. Across every good, 2–18
convoys a tick are refused here already.

## F. Do corporations dispatch their own surplus? Not steel, and why

- **Tick -1/0: no signal.** Corporations hold the opening steel: 12.5–13.3k in 123–128 pools. The
  first dispatch runs before any clear, so every `demand[]` is 0 and every price is base.
  - Seed 0: 2,284 of 3,024 pairs fail the gate on a flat price; 518 are water-unroutable.
  - Dispatch also skips 24–27 pools under a sell order (`supply_system.cpp:1206-1209, :1267`).
- **The tick-0 clear sells everything at home.** Auto-surplus sells every pool down to its
  reservation, so the 10.3k lands on the home shelves. By tick 10 the corporations hold only their
  reservations, and the steel belongs to no one. Only the market's own export can move it, and § A–B
  stop that.
- **New steel is nearly nil.** Steel processors fall from 29 to 7 by tick 3 on seed 0. Mills switch
  recipe to `steel_from_iron_nickel`, whose ore the body does not carry, and 5 decommission by
  tick 8. So no fresh pooled surplus appears.
- **Other goods do move.** Corporation dispatches run 11–68 a tick across every good; the
  corporation pass works where the gates pass.
- **The scorer cannot do better.** The directed dispatch (`corp_ai.cpp:1958-2131`) applies the same
  gate, leg, margin and `dispatch_room` to the corp's own pools only, less `trade_hold_threshold`
  (`:2038`). It can never choose a haul the auto-rule refused. No verb lets a corporation buy another
  market's shelf and haul it.

**Coal "one market away".** Coal-drawing processors fall from 27 to 1 by tick 3 on seed 0: the mills
switched away. By tick 20 the only coal bid is 6 u, at m46769. The mills that wanted coal had already
left before any coal could arrive. Separately, the cross-landmass pairs fail § B.

## Candidate fixes — what each changes, and its likely side effects

None is taken here. Each is a world-mover.

| # | Lever | Changes | Likely side effects |
|---|---|---|---|
| A1 | `shelf_supply_ticks` k > 0 (`economy.lua:1400`), with BL-1179 (shelf spoilage) | Shelf-rich markets price down and empty ones price up, so a real gradient appears and the cycle damps. | BL-1172 measured k > 0 floor prices on gluts (firms 1/1/11 on seeds 0/10/28); Ben ruled k = 0 until spoilage. Every economy golden moves. |
| A2 | A draw over the ceiling **still registers its want** for pricing but does not buy (`economy_system.cpp:363-365, :779-786`) | Demand stays visible every tick, and dispatch sees `p_d` high with `D > 0`. Prices sit at the 10x ceiling while unmet. | Reverses part of the BL-1172 ruling ("its want leaves the price so the price can ease", FINANCE.md, Ben 2026-10-03). **Ben's call.** Corp AI's glut and price readers see more demand. |
| A3 | Dispatch reads a **demand memory** (e.g. last N clears, or unmet want) rather than last clear only | Removes the 1-in-4 blindness without touching the price law. | A new persistent per-market field: **the save seam is owed**. Risk of over-sending into a market that has gone quiet. |
| A4 | Raise `reservation_mult` (2.0) or lower `ceil_mult` (10) | Shortens or breaks the cycle: at ceil ≤ 2x the price never leaves the admit band. | Blunts the scarcity signal BL-654/BL-1172 tuned. Ceil 10x was derived (economy.lua:1319-1384). |
| B1 | Port gate per the doc: route land → Port → sea → Port → land (`supply_system.cpp:476-489`) | Opens inter-landmass pairs wherever Ports exist (3–4 per body). | A multi-leg router; port handling is still uncharged (SUPPLY.md says it should be). More haulage, and haulage_measure re-baselines. |
| B2 | On a water path without ports, **fall back to the land-only flood** | Recovers the land-connected pairs, mostly corporation pools (≈20% of their water refusals on seed 43). | Small and local; `astar_cost_cache` keying needs care (a second answer per pair). |
| E1 | LP: **send `min(qty, anchor pool)`** instead of refusing the whole cargo | Lets large surpluses move at the cap's rate, rather than not at all. | haulage_measure 1055/802 re-baseline (the 20 u rate was measured against a refuse-whole rule). It raises convoy counts. |
| F1 | Run one clear before the first dispatch of the settle, or place opening stock where demand will be | The opening 12–13k is weighed against real prices before it is auctioned at home. | Settle digests move; it touches the settle order (`campaign_settle.cpp`), which is pinned. |
| F2 | A buy-and-haul verb for the scorer | Corporations could arbitrage shelves. | **A NEW AI widening: raise it to Ben, never assume it** (AI_OPPONENT.md § 11). |

**What I would weigh first.** A1/A2 and B1 are the two that unblock steel; neither alone suffices.

- **With A only,** the gradient appears, but the source and destination are mostly on different
  landmasses (seed 0's empty-shelf consumer m47753 has every shelf-holding neighbour in phase, and
  the rest across water).
- **With B only,** the pairs route, but the gate still sees in-phase prices and silent destinations.
- **E1 is cheap and must ride with either,** or the first opened pair is refused for size.

## For BL-1185 (chain-feasible placement): what "within reach" should mean

Use the rule a convoy actually obeys, not grid distance. A producer at tile *p* is **within reach** of
a processor at tile *c* when either of these holds:

1. **Same market.** `market_for_tile(p) == market_for_tile(c)`. The good lists and lands on the same
   shelf, and no haul is needed.
2. **Routable market pair.** The pair (M_p = `market_for_tile(p)`, M_c = `market_for_tile(c)`) passes
   both tests below.
   - **The leg is viable.** `price_market_export_leg(w, reg, nodes, M_p, M_c, 1).viable`. Today that
     means the same body, and a centre-to-centre path that crosses no water, unless both centre tiles
     carry an active Port. In practice: **the same landmass**, checkable with
     `bounded_cost_to_tile(w, body, M_c.centre, ∞, land_only = true)` reaching M_p's centre.
   - **The haul fits under the fair-price ceiling.** `haul_per_unit ≤ (reservation_mult − (1 +
     dispatch_margin)) x base(input)`, which is `0.95 x base` at today's 2.0 and 0.05. Above that, a
     landed cargo can never be bought at a price the processor will pay.

If B1 lands, (2) widens to "reachable by land, or by land to a Port, sea to a Port, and land again".
BL-1185 should call the **same function the dispatcher calls**, so that placement and shipping can
never disagree.

**Necessary, not sufficient.** Today a reachable input still does not arrive on steel. The § A cycle
silences the destination three ticks in four, and § E refuses any cargo over 20 u. BL-1185's
"within reach" makes a chain *possible*; it cannot make it run until A and E are fixed.

## Novelty and scope notes

- Two things here go beyond refusal accounting. The mill recipe switch to a recipe with no ore on the
  body (§ F) is agency reflex, not dispatch. Market catchments are assigned by grid distance across
  water (`market_for_tile`), so a building can belong to a market on another landmass. Both are
  flagged, not diagnosed further.
- The probe's construction "PAUSED" line reads the shelf without the ceiling, so it under-counts sites
  paused by the ceiling. The market table's `admits` column is the honest read.
