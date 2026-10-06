# Sprint 50 handoff — sprint 49 (market viability) closing, written 2026-10-06

Read this, then `SPRINTS.md` (sprint 49's record and checkpoint notes), then
`backlog_query.js --grep` for each item you pick up. The sprint 50 cut is Ben's form; the
proposal below is a recommendation, not a cut.

## Where sprint 49 stands

**Goal:** the market economy is profitable from day 1 at its run-rate, judged on BL-1184
(market viability gate; the `market-viability` skill) against Ben's targets: G1 processors
running at handoff >= 70%, G2 play-window income / settle income >= 50%, G3 firms alive at
tick 400 >= 70%.

| Gate | Sprint start (16 seeds) | Latest on main (5 seeds, 0/43/10/28/38) |
|---|---|---|
| G1 processors running at handoff | 17.6% | ~49% — FAIL |
| G2 run-rate income | 27.5% (window form) | ~49% — borderline, swings by seed |
| G3 firms alive at t400 | 31.3% | 86.6% — PASS |

**Merged (18):** BL-1184 gate · BL-1186 routing through ports · BL-1187 build only what runs ·
BL-1188/1185 chain-feasible placement (hard rule) · BL-1193 household diagnosis · BL-1198 the
Well · BL-1200 lake size cap · BL-1201 orders are price floors · BL-1197 wells in generation +
derived demand · BL-1163/1196/1179 households consume, grow on their own market, shelves spoil
(k = 0) · BL-1204/1205 density per good served + scorer speed-up · BL-1191 upkeep stopgap ·
BL-1194 sea cheaper than highway · BL-1183 C4 (a site under construction pays the idle floor).

**Rulings taken this sprint (all in their authority docs):** orders are price floors (MARKETS.md
step 4); the Well (PRODUCTION.md); lakes below 150 tiles (TILES.md); derived demand and the
anchor-route closure (CORPORATION_GENERATION.md § Pass 6); k = 0 (MARKETS.md); density 7.5 per
good served (INDUSTRIALISATION.md § 1); households draw before the nation (MARKETS.md step 12);
a site pays only the idle floor (FINANCE.md); a hauler sees households at the landed price and
the want the ceiling suppressed (SUPPLY.md § Dispatch trigger). Open decisions taken on Ben's
behalf, in NEEDS_REVIEW: NR-970 (how the shelf spoils), NR-975 (lake cap 150).

## Sprint 49 close-out — do these first

1. **BL-1203 (water reaches dry markets), room fix — in flight** on branch `bl-1203-room`
   (worktree-agent-a829d49ff28da3f77). Option B is built (72487856, cold-reviewed); the review
   found the send is sized to land at zero margin (size to L x (1 + dispatch_margin)) and the
   suppressed want can overshoot below landed cost. The lane was sent that fix round plus an
   attribution of the 5.6% play-income drop. Merge only when realised sale price >= landed cost
   per convoy and G2 recovers; save v36 rides with it.
2. **Re-measure after BL-1203:** BL-1183's held C1 (no site whose materials are unobtainable;
   7c6c6d8a on worktree-agent-a90f2be8107c16fd4) and the stall count; then decide C1.
3. **The full 16-seed gate reading** (`market-viability` skill) on the final tree.
4. **The wave's single re-bless**, described in SHAPE (DELIVERY.md § the digest re-bless;
   `rebless_shape_probe`), every cause named — Ben authorises against the shape. The pins have
   been stale since BL-1185; nothing in this sprint re-blessed.
5. **Live clicks owed (Ben):** Market Ledger order form ("Cap / qtr", 0 = no cap, rows read
   "all", "Cap" header); the Well row and placement tint; a site's idle-floor cost in the
   construction panel; sea lanes carried into round 6.
6. **Version cut** (v0.1.28) after the re-bless, then push (Ben pushes, or asks).

## Weak areas that could still land in sprint 49 (small, in scope)

- **The seat's own day-1 profit.** The sprint's question was the player's profit from day 1,
  and the seat still runs a loss on some seeds (the BL-1185 tip read seat operating net
  -44 / -2 / +48 / -13 / +14 on seeds 0/43/10/28/38). File and measure: seat operating net >= 0
  over its first 8 quarters on all 16 seeds, and which kit choice or input causes the loss.
  **Recommended — it is the sprint's headline promise.**
- **G1 is the furthest gate (~49% vs 70%).** Diagnose what is starved at handoff on the current
  tree (by input, by market) before choosing a lever; BL-1189 (opening stock sized) is the filed
  candidate.
- **BL-1199 (Fishing Wharf yields nothing)** — a real bug, small, and fish feeds food rations.
- **BL-1202 (order close notice)** — small UI; completes the order-floor rule for the player.
- Housekeeping: BL-1166 (industrialisation harness self-check), BL-1176-1178 (works-note
  leftovers), BL-1180 (construction panel drift).

## Sprint 50 proposal: logistics and the trading loop — split in two

Ben (2026-10-06): sprint 50 focuses on the logistics system, the core trading loops, and
visibility. **Recommended split** — building lenses over mechanics still moving wastes the lens
work, but debugging logistics needs to see flows:

### Sprint 50 — logistics and trade flow (the simulation), with one diagnostic lens
The sprint 49 readings point straight at it: goods exist but do not move (water, steel).
- BL-1203 follow-ups: the one-destination-per-pass rule (binds: ~113 units unfilled per probe
  tick), the price gate (22% of dry markets), no route (13%: port placement / ports per body).
- BL-1195 (convoy lane follows legs) — interdiction reads a lane the cargo does not take.
- BL-1165 (untraced re-bless movements) incl. haulage_measure's stale pricing copy.
- BL-1192 (catchment ignores water), BL-1190 (markets meet firms).
- BL-1175 (firm entry) — competition returns after a glut; design first.
- One trade-flow lens (who ships what, where, and what was refused) — the instrument for this
  sprint, built early.
- Gate: extend BL-1184 with a logistics row (units moved surplus -> shortage; dry markets).

### Sprint 51 — the player's trading loop and visibility
- The player's trade verbs end to end (orders, directed hauls, contracts — CONTRACTS.md), and
  what the player reads to choose them.
- Visibility: the lenses Ben held for "before sprint 50" (sprint 48 surfaces), the order close
  notice if not done, ledger legibility for prices, shortages and routes.
- Live-click heavy: plan for Ben's walks.

### Displaced to sprint 52 (currently tagged 50)
BL-1170 (heartlands reach overseas) + BL-1148 (migration carries culture), BL-1174 (hiring
answers a threat — needs its AI grant), BL-1181 (bind-and-free churn).

## Hazards this sprint taught

- **Cold review caught a defect in nearly every lane** (zero-margin sizing, order-dependent
  roster enforcement, the sell-order trap). Budget a fix round per world-mover.
- **A ratio gate can hide a real drop:** read the absolute numbers (G2 is play / settle income).
- **Agents' "before" baselines go stale as main moves:** measure main yourself before merging.
- **Lanes cannot run `cmd`:** build the app and CMake-only harnesses in the main session.
- **The play build (`build_rel`) goes stale:** rebuild it before Ben opens the game
  (scratchpad `main_session_build_rel.bat` pattern; check no app is running first).
- **Save versions:** v35 on main, v36 rides with BL-1203; old saves are refused outright.
