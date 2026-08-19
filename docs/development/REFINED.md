# Project Io — REFINED (active worklist)

## Sprints 26a / 26b / 27 / B1 — The Fall opens, four lanes at once (promoted 2026-08-18)

Four sprints opened together because their file sets are disjoint. Requirements groups are owed
for the two that change `tools/verify/*` behaviour and are written before the results land.

**Main session owns** `.claude/rules/io-standing-rules.md`, `ROADMAP.md`, `backlog.json`,
`SPRINTS.md` and this file. **No agent touches any of them** — that is what keeps four concurrent
worktrees mergeable.

### Sprint 26a — the three authority edits

- **[1] A — NEW-12 the nation-behaviour grant.** BL-054's prohibition widened for both grains in
  the BL-202/BL-203 shape: pure, seeded, deterministic, scored-utility, legal verbs, never a
  planner. Explicit exclusions written in (nothing timing-dependent; no cloud model).
  **DONE.** File: `.claude/rules/io-standing-rules.md`.
- **[1] A′ — the BL-450 corp-grain grant.** Stated as its own widening, not folded into A. Its
  subject is the one actor the prohibition protects. Hostility stays a **declared** state a corp
  opts into — a rival may score and declare it, never acquire it ambiently. **DONE.** Same file.
- **[1] B — un-park v0.1.11.** Seven items flipped `parked: false`; ROADMAP gains the minor back
  under the ancient arc, themed on *who enacts*. **DONE.**
  Files: `backlog.json`, `ROADMAP.md`.
- **[2] C — NEW-1 the prehistory-on determinism case.** **IN FLIGHT** (sub-agent, worktree).
  File: `tools/verify/world_determinism.cpp`. Three cases: same-seed replay, different-seed
  divergence, and prehistory-on ≠ prehistory-off (proving the pass is not a silent no-op).
  **A red result is a finding and must be reported, not softened.**

### Sprint 26b — close-out and the doc truth pass

- **[1] D — close-out on evidence.** Six items → `complete`, each with a commit hash in this
  file's Sprint 25a block (BL-452, BL-454, BL-455, BL-456, BL-457, BL-459). **DONE.**
- **[0] D′ — the deliberate non-flips, recorded so they are not re-litigated.** BL-453 (convoy
  ledger) stays open: *"DONE but UNVERIFIED"*, R6 pending, NR-313. BL-417, BL-429, BL-437, BL-439,
  BL-440, BL-441, BL-442, BL-443 stay open pending per-item verification — Sprint 19/20's records
  show several held or forked, and a bulk flip on an audit's count lies in the other direction.
- **[3] E — the doc truth pass.** **IN FLIGHT** (sub-agent, worktree).
  Files: `docs/lore/HISTORY.md`, `docs/economy/ERAS.md`, `docs/tech/TECH_FOUNDATIONS.md`,
  `docs/military/MILITARY.md`. Span → 400 in one band; combat/weaponry/military-logistics
  un-excluded; MILITARY.md's Build status corrected to say upkeep ships at **rate zero**, and
  the industrial-band roster in a 0 CE product recorded as a known defect.

### Sprint 27 — Lane A: the run is retained, and its failure is falsifiable

- **[3] F — BL-384 the assertion half.** **IN FLIGHT** (sub-agent, worktree).
  File: `tools/verify/history_sim_harness.cpp`. Conquest-count assertion plus elimination and
  dominance-share counters, reusing `history_sweep.cpp`'s `hegemony_threshold_q` shape.
  **Expected outcome is RED.** No constant tuned, no scorer term touched.
- **[3] G — NEW-2 retain the generation run.** Not promoted; follows F.
  Would touch `hard_coded_world.cpp`, `history_sim.hpp`.
- **[3] H — NEW-3 the per-year decision trace.** Not promoted; follows F.

### Sprint B1 — Lane B: the substrate becomes a number

- **[3] I — NEW-10 substrate census.** **IN FLIGHT** (sub-agent, worktree).
  File: `tools/verify/substrate_census.cpp` (new). Report, never a gate. Measured **after**
  `generate_background_firms`, at `prehistory_years` default. Runtime reported explicitly — over
  60 s it Times Out under ctest, which has hidden a harness before (NR-259).

### Collision map — why these four can run concurrently

| Sprint | Writes | Collides with |
|---|---|---|
| 26a C | `tools/verify/world_determinism.cpp` | nothing |
| 26b E | four docs under `docs/` | nothing |
| 27 F | `tools/verify/history_sim_harness.cpp` | nothing |
| B1 I | `tools/verify/substrate_census.cpp` (new) | nothing |
| main | standing rules, ROADMAP, backlog.json, SPRINTS.md, REFINED.md | all agents, by exclusion |

**The collisions this arc will hit LATER, named now:** Sprint 29 and Sprint B3 both touch
`settlement.cpp`; Lanes C and D both append to `condition_subject` in `condition_set.hpp`, an
append-only serialised enum, so one appender per batch and it belongs to **D**.

**Toolchain.** Every agent is instructed to pin MSVC 14.44 — `spectator_determinism` records that
its hash is toolchain-specific, so a baseline taken under the wrong compiler is not a baseline.

---

## Sprint 25a — the draw (promoted 2026-08-17) — **6/6 DONE** (closed 2026-08-18)

Requirements: requirements.json § `ordnance-military-terminal-good`,
§ `military-points-deleted-science-wired`, § `convoy-dispatch-seam-and-ledger`

The half of Sprint 25 that runs without Sprints 21 and 23. Phase 25b (BL-458, interdiction) is
**not promoted** — it requires BL-315 (conflict spine) and BL-448 (stance), neither opened.

- **[3] A — BL-457 ordnance.** The blocking foundation: nothing below draws without a good.
  `resource_type` 37 → 38, Fabricator recipe (`steel + machinery`), `base_price` 43.0 **derived**
  from the roster's 1.415–1.443 markup band. Both name maps, the UI presentation row, and
  `chain_depth`'s R1 exemption naming BL-454 as the consumer. **DONE** (`44166a4`).
  Files: `components.hpp`, `recipe_registry.cpp`, `world_gen_config.cpp`, `presentation.cpp`,
  `recipes.lua`, `world_gen.lua`, `chain_depth.cpp`, `RESOURCES.md`, `PRODUCTION.md`.
- **[1] A′ — the presentation-table gap A forced.** A positional array cannot gain index 37
  without filling 34–36, so the three habitability goods that had rendered as
  "(unnamed resource)" since 2026-08-11 are now authored. Guard added
  (`tools/session/resource_table_check.js`), **shown to fail on the pre-change tree first**.
  **DONE** (`44166a4`). Unplanned; recorded as NR-314.
- **[2] B — BL-455 accumulators.** `military_points` deleted at every site; `science` wired as
  `condition_subject::science`, reached-not-spent per BL-344. Harness retargeted, 8/8.
  **DONE** (`9644eaa`). Files: `components.hpp`, `economy_system.cpp`, `recipe_registry.{hpp,cpp}`,
  `condition_set.{hpp,cpp}`, `economy.lua`, `military_capability_harness.cpp`.
- **[2] C — BL-456 military authority doc.** `docs/military/MILITARY.md` + the CLAUDE.md entry;
  ten items repointed. **DONE** (`6153278`, sub-agent, merged and reconciled — it was written
  against a base predating B and described `military_points` as scheduled for deletion).
- **[3] D — BL-452 convoy seam.** `dispatch_convoy` + `hold_convoy`, with the auto-dispatcher's
  dispatch half factored into `price_convoy_leg` / `commit_convoy` so there is **no second code
  path**. **DONE** (`bd71143`, sub-agent). Satisfies R1–R5.
- **[3] E — BL-453 Convoys tab.** Ticks-to-arrival, Hold press, question-log entry.
  **DONE but UNVERIFIED** (`bd71143`) — not compiled, not run, not photographed (NR-313).
  R6 stays `pending`. The market-Selection dispatch press was **CANCELLED**, scope returned.
- **[4] F — BL-454 unit upkeep + BL-459 unit strength.** Paired: the upkeep pass writes the
  `supply_factor` the strength model reads, so splitting them strands both. **DONE** (`0936400`,
  sub-agent). Upkeep as a cost vector and its own budget term; one decay rule with two triggers;
  stored `strength` **removed** and derived instead; the demolish-orphan fix in the unit pass; and
  the missing `unit_component` → `army_stack_entry` adapter. `unit_upkeep` 62/62,
  `condition_set_harness` 41/41, `combat_harness` and `campaign_battle_harness` (16/0) green.
  **Inert as shipped** — every rate zero, balance bit-identical after 25 ticks. Its four UI
  surfaces are unverified (R7 `pending`) and owe two question-log entries.

**Owed before any cut**, and none of it is optional:

1. **`chain_depth`** — unrunnable here, and it is BL-457's own named guard.
2. **A Lua load** of the four edited scripts (`recipes.lua`, `world_gen.lua`, `economy.lua`,
   plus the registry loader) — no interpreter was available, so none was ever parsed.
3. **A `verifier-visual` pass** over the Convoys tab and the four unit/finance surfaces, which
   between them are five uncompiled UI changes. Two question-log entries land with it (NR-323).
4. **A Windows re-bless of `spectator_determinism`** with **two** named structural causes, isolated
   across three commits rather than assumed (NR-319): `resource_count` 37 → 38, and
   `unit_component`'s two new hashed fields. The `military_points` deletion moved it not at all.
5. **Set the upkeep rates** (NR-321) — until then nothing consumes ordnance at runtime, and the
   R1 exemption naming BL-454 as its consumer is structurally true but not true in play.

---

## Widen the price band (promoted from BL-442 step 2) — **5/5 DONE** (2026-08-17)

Requirements: requirements.json § price-band-derived-ceiling (R1–R5)

Step 1 made the band data (`economy.price_band`, read by `resolve_price` and `wf_target_price`
through `recipe_registry::price_band()`) and deliberately left the values alone. Step 2 changes
the values, and only the values — Ben's requirement makes the ceiling **derived**: scarcity must
price high enough to cross the margin for a nearby market.

- **[3] A — Measure the haulage first.** A ceiling picked before the number is known is a guess
  wearing a derivation's clothes. New harness walks every market in the real generated world,
  finds its nearest market neighbour by the terrain-weighted A* cost, and reports
  `logistics_cost(mode) x path.cost` — exactly what `dispatch_convoys` charges. **DONE.**
  Measured: median **0.70**, p90 **1.67**, max **4.83** credits per unit; cheapest priced good
  **0.60**. Files: `tools/verify/haulage_measure.cpp`, `CMakeLists.txt`. Satisfies: R1.
- **[1] B — Change the value, data only.** `ceil_mult` 4.0 → **10.0**, from
  `1 + 4.83/0.60 = 9.06` rounded up. Floor left at 0.25 (NR-290). No C++ touched. **DONE.**
  Files: `scripts/economy.lua`. Deps: A. Satisfies: R2.
- **[3] C — Prove the outcome instead of assuming it.** Count convoys at both bands, split
  intra-body market-to-market / inter-body / BL-088 trade routes. **DONE, and it contradicted the
  premise** — intra-body inter-market trade was already happening at the old band, and the space
  lane is zero at both because it is launchpad-gated, not price-gated (NR-289). Files:
  `tools/verify/haulage_measure.cpp`. Deps: A. Satisfies: R3.
- **[2] D — Run the full gate, report every number that moved, bless nothing.** **DONE.**
  Deps: B. Satisfies: R4, R5.
- **[1] E — Propagate the derivation to MARKETS.md** so the number can be re-derived rather than
  trusted. **DONE.** Files: `docs/economy/MARKETS.md`. Deps: B.

## Unmet demand is never registered (promoted from BL-441) — **5/5 DONE** (2026-08-17)

Requirements: requirements.json § demand-register-records-wants (R1–R5)

`market_clearing.cpp` registers `demand[r] += bought[r]` — what a corp **actually bought**. A
processor that needed 16 units of an input and could draw only 2 registers demand of 2, so the
resource reads to `resolve_price` as one almost nobody wants and scarcity is invisible. The fix is
to register the **want** alongside the **fill**: `run_processing` already computes the full-run need
per input *before* deciding coverage, and that figure is the demand.

The distinction is the one BL-422 landed for the sell side — SUPPLY is an offer, INVENTORY is a
delivery. Demand needs the same split: what was **wanted** versus what was **bought**. Only the
price-resolution input changes; the purchase record (`auto_buys`, the VWAP accumulator, the money
actually paid) keeps reading the fill.

- **[2] A — Guard first, run against the pre-change build.** Two assertions on observable market
  state, so they compile and run *before* the fix and are seen to fail there: a starved processor
  registers demand equal to its full-run NEED, not the quantity it drew; and a resource with real
  demand and no supply resolves to the TOP of the price band, not to base. Files:
  `tools/verify/order_book_harness.cpp`. Deps: foundation. Satisfies: R1, R2.
- **[2] B — The want register.** A `wants` map on `economy_report`, keyed and containered exactly
  as `purchases` (`std::map` over `(corp, body)` — a **sorted** key set; this is the seam where
  BL-422 found a latent `unordered_map` float-accumulation nondeterminism). Files:
  `src/world/economy_system.hpp`. Deps: foundation. Satisfies: R4.
- **[3] C — Record the want at both consumer sites.** `run_processing`: the full-run need net of
  the corp's own pool, recorded in the coverage loop so it survives the early return that a starved
  building takes. `run_construction`: the full-rate material need, recorded **before** the
  `rate <= 0` continue, so a paused build still says what it wanted. C is not optional scope — once
  demand reads `wants`, a construction site with no want row would register zero demand where it
  previously registered its draw. Files: `src/world/economy_system.cpp`. Deps: B. Satisfies: R1, R3.
- **[2] D — Split the read in the clearing pass.** `mc.demand` reads `report.wants`; `auto_buys`,
  the VWAP accumulator and the money paid keep reading `report.purchases`. Both loops iterate a
  `std::map`. Each site states in a comment which of the two it wants. Files:
  `src/world/market_clearing.cpp`. Deps: B, C. Satisfies: R3, R4.
- **[2] E — Measure the economy, report it, bless nothing.** Run `tier_margin` and the determinism
  reads; report every number that moved. Propagate the settled design into `docs/economy/MARKETS.md`
  as part of landing. Files: `docs/economy/MARKETS.md`. Deps: D. Satisfies: R5. **DONE.**

**All five tasks complete.** The guard was run against the pre-change build first and failed there
on all three new assertions — `demand=0.000 price=5.000` starved, `fill=2 demand=2` partial.
`tier_margin` was measured on **both** sides from the same worktree: processing input cost
10.86 → 11.59, processing net −9.52 → −10.42, extraction net 7.27 → 7.11, and several resources left
the price floor for the first time. The failing assertion set is **identical** on both sides — three
pre-existing failures, none new, none masked. **Nothing was blessed**; that stays Ben's call
(NR-269). Two delegated calls are logged: NR-281 (the want is net of the corp's own pool) and
NR-282 (`run_construction` was pulled into scope, because leaving it out would have zeroed
construction's market demand).

## Mines only target the richest (promoted from BL-440) — **2/4, C SUPERSEDED BY BL-441/442** (2026-08-17)

Requirements: none written yet — the item is mid-flight and (c) is a design call, not an
implementation detail. Write them with (c)'s shape, so decomposition is shaped by it.

`richest_extractable` gives a site the single richest deposit on its tile, so a resource that is
common but rarely dominant is structurally unmineable — it loses every richness comparison it is
entered into, so no scorer is ever offered it.

- **[3] A — Guard first: tier_margin R4b.** Assert no wanted recipe input sits on 200+ tiles with
  zero sites naming it. **DONE**, and run against the pre-change build where it fails by
  construction on **seven** resources, one of them on 16,361 tiles. Files: `tools/verify/tier_margin.cpp`.
- **[3] B — Enumerate, and weight by unmet demand.** `rank_extraction_sites` emits a candidate per
  extractable deposit rather than one for the richest, weighted by `input_demand_weights()`
  (recipes-wanting-it over sites-already-named-for-it, self-limiting). `input_demand_pull` in
  `corp_ai_params` makes it a data change. **DONE.** Files: `src/world/corp_ai.{hpp,cpp}`.
- **[3] C — Generation sites by the same broken rule.** **SUPERSEDED 2026-08-17, do not build.**
  Ben chose the static demand hint, then redirected past it the same session on a better reading:
  the price signal was never structurally unable to do this job. It was starved, because the demand
  register records FILLS rather than WANTS. Fix that and price sites the mine by itself, with task
  B's per-deposit enumeration as the thing that lets the scorer act on it. Now BL-441 (unmet demand
  is never registered) and BL-442 (price band is code, not data).
- **[1] D — Propagate to PRODUCTION.md.** Not started. Deps: C.

**The measurement that says C is the load-bearing half.** tier_margin is **byte-identical** across
A+B — extraction 14.53/7.82, processing 12.37/−10.44, samples 2993/1916, all seven R4b rows still
red. A 3-seed 20-tick run is dominated by generated assets, and generation still calls
`richest_extractable`. The AI-side fix cannot show until generation stops digging the hole.

The blocker on C is the same ordering constraint that produced the no-recipe defect:
`generate_corporations` runs inside `make_hard_coded_world`, before the Lua economy layer loads, so
it can see neither recipe ids nor recipe demand.

## AI never builds processors (promoted from BL-439) — **3/4, C HELD FOR BEN** (2026-08-17)

Requirements: requirements.json § ai-builds-processors (R1–R7)

The structural finding behind Sprint 19's keystone. `corp_ai.cpp` emits `corp_verb::build` from
exactly two sites — the `ranked_sites` loop hard-coded to `extraction_site` (line 606) and one
`military_base` (line 960). A rival therefore owns only the processors it was *generated* with,
for the whole campaign. "The AI prefers mines" is not a scoring-curve artefact (NR-265); BL-428's
growth ladder has no AI player (NR-267); and BL-436's calibration narrative describes a mechanism
that cannot happen (NR-266).

- **[3] A — The processor build candidate.** Add a `processing_facility` candidate to
  `run_corp_strategic_step`'s enumeration, on the same score curve and the same solvency / glut /
  reserve gates as the extraction candidate. Three things extraction does not have to solve:
  **siting** (no deposit ranks a processor — site it on the corp's own asset tiles, which is
  literally "next to its own input supply", shares the body pool and the market, and costs
  O(assets) not O(tiles)); **recipe choice** (`recipe_margin` scores it; the id must cross from
  the BROWSE space to the ABSOLUTE one — NR-254's exact trap); **input access** (pool + local
  market inventory against the tick's own coverage threshold). Files: `src/world/corp_ai.cpp`.
  Deps: foundation. Satisfies: R2, R3, R4, R5.
- **[2] B — Guard: a rival actually builds one.** Extend `ai_skill_harness` with the assertion
  that at least one rival constructs at least one processing facility over the run — and run it
  against the **pre-change** build first, where it must fail by construction. Files:
  `tools/verify/ai_skill_harness.cpp`. Deps: A. Satisfies: R1.
- **[2] C — Pay the golden reshuffle deliberately.** Every evolved world changes, so goldens and
  bands move. Re-bless with the cause recorded, confirmed across two runs. The bands lowered on
  2026-08-16 are expected to **rise**; if they do not, that is a finding about the item, not a
  number to bless. Deps: A, B. Satisfies: R6.
- **[1] D — Propagate to AI_OPPONENT.md.** The doc describes a scorer that only ranks extraction
  sites. Files: `docs/ai/AI_OPPONENT.md`. Deps: A. Satisfies: R7.

**Parallelisation.** Strictly A → {B, D} → C. One logic file with `corp_ai.cpp` as the hotspot —
main session, no fan-out.

**Status 2026-08-17. A, B and D complete; C is held, not skipped.** The guard was written and run
against the pre-change build *first*, where it failed by construction (`processors_gained = 0` on
all five seeds); after the change it reads 12/15/16/16/10, 69 across the set. So the structural
defect is closed and demonstrated in both directions.

What it then found is why C stops here. Three of five seeds go insolvent — seed 0 from 498k to
−295k — and `ai_skill_harness` is 18 rows red. The estimator was **ruled out rather than assumed
innocent**: scored with the inline model and with `estimate_prospective_profit`, the candidate picks
the same recipes and builds the same counts, and net worth moves only on seed 0. The new
per-building read shows processors realising **−6 to −12 per tick against a predicted −0.4 to
+0.1**, on the same buildings.

Sprint 19's success criterion was written before the work started: these bands should **rise**, and
a bless that does not raise them means the fix did not work. They fell. Blessing here would record
bankruptcy as the expected outcome, so the goldens are **left red** and the call is Ben's —
NR-269 lays out the three options and states the one taken in the meantime (change nothing, bless
nothing).

## Held-order phantom inventory (promoted from BL-422) — **COMPLETE** (3/3, 2026-08-16)

Requirements: requirements.json § held-order-phantom-inventory (R1–R4)

Sprint 19's early-clearing item: phantom supply distorts every price BL-436's margin work is
measured against, so it is worth removing before more numbers are read off this seam.

`market_clearing.cpp` credits a standing order's **listed** quantity to `mc.inventory` at listing
time, before clearing runs. Pre-BL-386 that was harmless — everything listed also sold. Under the
reservation rule a held order (floor above the resolved price) sells nothing, yet the market gains
stock that never left the seller's pool. `inventory` is not a display field: `economy_system.cpp`
draws processor inputs from it (lines 312/363/563), so a held order's phantom stock is bought by a
processor, decrements, and is never paid for — **goods from nothing, and money destroyed**, the
same family as BL-351's over-listing.

- **[2] A — Credit inventory where stock actually leaves a pool.** Delete the listing-time credit;
  credit inside the three places a pool is debited instead — the auto-surplus clearing loop, the
  matched-trade debit loop, and the auto-clear pass (`se.rem`, floor ≤ resolved only). Files:
  `src/world/market_clearing.cpp`. Deps: foundation. Satisfies: R1, R2, R3.
- **[2] B — Guard: an R7 group on `order_book_harness`,** beside BL-386's R6 hold rows it extends
  — held leaves inventory untouched, a clearing order credits exactly what left its pool, and a
  mixed tick credits only the clearing half. Files: `tools/verify/order_book_harness.cpp`.
  Deps: A. Satisfies: R1, R2, R3.
- **[1] C — Propagate to MARKETS.md**: the real-inventory paragraph, and the recorded decision on
  whether held stock stays visible to the supply-side price signal. Files:
  `docs/economy/MARKETS.md`. Deps: A. Satisfies: R4.

**Parallelisation.** Strictly A → {B, C}: B asserts what A changes and C describes it. Two logic
files, one seam — main session, no fan-out.

**Closed 2026-08-16. All three tasks complete; `order_book_harness` 64/64 (12 new R7 rows).**
Three of the twelve — R7.2, R7.3, R7.10 — were run against the **pre-fix** build first and
confirmed to fail there, so the guard discriminates rather than merely passing. The R6 hold rows
it sits beside were the reason the rest of BL-386's rule looked complete when half of it was not.

Two things surfaced that are recorded rather than folded in, because each is a change to a
*different* seam than the one this item names:

- **Held stock stays visible to the price signal, against the item's own stated default**
  (NR-261). Hiding it is not implementable as written: a hold is decided by comparing the floor
  to the resolved price, and the resolved price is computed *from* `supply` — removing held stock
  raises the price, which can un-hold the order that was removed. That is a fixed point, and it
  costs determinism risk to reach. `supply` is now documented as the **offer** and `inventory` as
  the **delivery**.
- **A matched explicit trade still delivers to the shared market shelf, not to the buyer**
  (NR-262). Pre-existing and currently unreachable — nothing writes `world::buy_orders` in play —
  so it belongs to BL-160, which gives the buy side its first live emitter. R7.12 pins today's
  behaviour as conservation, so a future correction fails it loudly.

One measurement worth carrying rather than filing away (NR-263): `ai_skill_harness` is
**byte-identical** on all five seeds before and after. The AI places 9–10 standing orders per seed
and none of them hold, so the benchmark never reached the defect. That is a fact about the
benchmark, not a defence of the old code — and it matters to BL-436, whose calibration will move
resolved prices under those orders.

## Starting-corp selection (promoted from BL-435) — **COMPLETE** (6/6, 2026-08-17)

> **Resumed and finished 2026-08-17.** A–D landed 2026-08-16 (`d80ca55`, `2f0f929`); **E and F
> landed this pass**. All six tasks terminal, requirements R1–R6 complete.
>
> **E cleared the catch the pause was called on.** `--verify` genuinely could not reach the
> screen — both automated paths take the Surprise-me fallback by design, which is what keeps
> every other capture bit-identical. `verify.show_corp_choice(true)` re-enters the stage from a
> started world, `verify.corp_choices()` exposes the armed list, and
> `scripts/verify/corp_choice.lua` asserts + captures. It paid for itself on the first run:
> the Holdings cell was clipped to "/ 1 otl" behind the Choose button (NR-275). The headless
> half is `player_seed_sweep --guard` (G1–G4, own ctest `player_seed_sweep_guard`).
> `build_corp_choices` also now skips `is_background` explicitly, so the specialist-only pool
> stops being a property of *when* it is called (NR-273).
>
> **Still owed — Ben's eyeball, and it is why there is no golden.** The capture is
> capture-only on purpose; blessing now would pin a layout he has never seen.
> `--autostart-play` will NOT show the screen (it auto-picks), so it needs a game started from
> the menu. NR-275 carries the note.
>
> **BL-436 (processing loses money) remains the loose thread**, unchanged: the screen is a
> **depth** choice, not a wealth one, and every doc and check touched this pass says so
> explicitly rather than implying the processor-heavy corp is richer.

Requirements: requirements.json § starting-corp-selection (R1–R4)

The seed hands the player a corporation by lottery, and on 13 of 24 seeds that corp has no
processing facility — so the Method page (BL-430/431) and the chain-depth ladder (BL-428) have
nothing to stand on. Ben's two calls, 2026-08-16:

- **Pool = the 8 specialist corps**, not BL-365's background firms — *and raise how many of the 8
  open with a processor*, so the choice has substance rather than one obvious answer.
- **The choice happens BEFORE the pre-game warm start.** That keeps the standing prohibition
  intact: the corp the player picks is the corp `corp_ai` excludes for the whole 80 ticks.

**What the measurement found before any work** (`player_seed_sweep --roster`, task A). The premise
holds — a better specialist existed in every bad world — but the pool is thinner than BL-435's
filing suggests: only **1–5 of 8** specialists carry a processor, and the "plenty of corporations
with processing facilities" in the item's text are overwhelmingly the 17–29 background firms.
Root cause is not tuning: `focus_asset_pattern`'s own comments promise extraction corps "a single
processor" and trade corps "light … processing", but the processor sits at a pattern index the
holdings draw frequently never reaches — index 3 for extraction (holdings 3–4, so a 4-draw only)
and index 2 for trade (holdings 1–2, so **never**).

- **[1] A — `player_seed_sweep --roster <seed>`: every corp's generated opening for one seed,
  split SPECIALIST vs background firm.** The measurement BL-435's premise rests on, and the same
  per-corp shape the screen renders. Files: `tools/verify/player_seed_sweep.cpp`. Deps: foundation.
  Satisfies: R2. — **COMPLETE 2026-08-16.**
- **[2] B — Make `focus_asset_pattern` realise its own documented intent: move the processor to a
  slot the holdings draw actually reaches for extraction. Trade keeps none — a pure-trade opening
  is a real archetype.** Measure the before/after across 24 seeds and report it; do NOT clamp,
  reject-sample or whitelist. Files: `src/world/corporation_generation.cpp`. Deps: A.
  Satisfies: R3.
- **[3] C — The selection state: a `pending_corp_choice` on the app, the candidate list built from
  the specialist set, and `apply_corp_choice` re-pointing `is_player` / `world::player_entity`.
  The generator's seeded pick stays and remains the "surprise me" default, so every harness that
  never opens the screen is bit-identical.** Files: `src/core/app.{hpp,cpp}`,
  `src/world/corporation_generation.{hpp,cpp}`. Deps: B. Satisfies: R1, R4.
- **[3] D — The screen: a stage between generation completing and the warm start starting, listing
  name / focus / home nation / holdings by type, plus "surprise me".** Balance is deliberately not
  a column — it is seeded BY the warm start, so it reads 0.0 for every corp at this point. Files:
  `src/ui/startup_screens.cpp`, `src/core/app.cpp`. Deps: C. Satisfies: R1.
- **[2] E — Guard: a headless row asserting the specialist/background split and the raised
  processor count, and a `scripts/verify/*.lua` visual check for the screen.** Files:
  `tools/verify/player_seed_sweep.cpp`, `scripts/verify/`, `.claude/skills/verifier-*/SKILL.md`.
  Deps: D. Satisfies: R1, R2, R3, R5, R6. — **COMPLETE 2026-08-17.**
- **[1] F — Propagate: CORPORATION_GENERATION.md (the deferred selection screen it already names,
  and the asset-pattern change), STARTUP.md (the new stage), question_log.json (the surface's
  question).** Files: `docs/generation/CORPORATION_GENERATION.md`, `docs/ui/STARTUP.md`,
  `docs/ui/question_log.json`. Deps: D. — **COMPLETE 2026-08-17.**

**Parallelisation.** Strictly sequential A → B → C → D → {E, F}: B changes what C's candidate list
contains, C provides the state D renders, and E/F both describe D. One vertical seam, so it runs
in the main session — a fan-out would spend more on merge than it saves, the same call the
chain-depth group made for the same reason.

**Watch for.** `corporation_generation.cpp` is the hotspot: B changes generated holdings, which
moves every world. Expect `world_audit`, `world_determinism`, `ai_skill_harness` and the
`earthlike_*` set to need re-running, and expect some to move — a deliberate generation change is
exactly the case their golden policy says to re-bless, with the reason recorded.


> Drained 2026-08-12: BL-132 (market cogeneration), Sprint 10's last item — 3 sequential slices
> COMPLETE per the item's own settled sequencing (population resource-awareness -> trade-flow
> market siting -> corp-level carving reorder, deliberately not entangled) — removed per the
> retain-one policy. Record lives in backlog.json BL-132's `resolution` field and
> requirements.json § market-cogeneration (R1–R4). (1) `generate_population_centres` weights its
> candidate pool by nearby extractable-deposit richness. (2) A market's `centre_tile` is a static
> resource↔population trade-flow proxy (bounded-radius gravity search) rather than the bare
> population tile. (3) `generate_corporations` now runs before market seeding (the reorder), and
> the market-carving gate's concentration term factors distinct-corp presence in a nation's
> territory (`corp_presence_gain`, tunable in world_gen.lua). All three are pure functions of
> generation-time data — no new RNG draws — so determinism carries through unchanged:
> `world_determinism` R1 (bit-identical incl. corps/entities), BL-096 R1/R2/R4, BL-182 R1/R2, and
> BL-257 R1/R2 all hold. `GENERATION_STRATEGY.md` gained a Market co-generation paragraph.

> Drained 2026-08-12: BL-367 (multi-building management surface), 1 task COMPLETE (light-weight
> enough for one direct pass) — removed per the retain-one policy. Record lives in backlog.json
> BL-367's `resolution` field and requirements.json § multi-building-management-surface (R1–R5).
> construction_panel.cpp's building-detail view groups a tile's buildings by stack
> (`tile_stacks`/`draw_tile_stack_list`), mirroring `placement_rules::stack_members`'s own
> grouping; a single-building tile still routes straight to its detail. selection_panel.cpp's
> Manage button and body_surface_canvas.cpp's marker-click/fallback both now count
> buildings-per-tile instead of grabbing an arbitrary first match. On-canvas marker gained a
> "+N" count badge, staggered past the corp-identity tag. Docs: SELECTION.md, ICONS.md,
> question_log.json updated. ProjectIo builds clean, launches without crash; a live screenshot
> of a real multi-building tile is owed (NR-173), expected to surface naturally given BL-365's
> background firms.

> Drained 2026-08-11: BL-365 (background industry keystone), Sprint 10's keystone and the
> reason BL-130/BL-263/BL-368 were pulled in out of turn — 2 worktree tasks COMPLETE
> (T1 core swap, T2 docs) — removed per the retain-one policy. Record lives in backlog.json
> BL-365's `resolution` field and requirements.json § background-industry-keystone (R1–R7).
> Real background corporations (`corporation_component.is_background`) replace the abstract
> nation-substrate injection: `generate_background_firms` calibrates per body until real
> production reaches ~90% of real demand (measured, not authored), background firms run the
> FULL corp_ai scored-utility layer per Ben's explicit 2026-08-11 call, `nation_substrate` /
> `inject_substrate_demand` are deleted outright. `pregame_balance_harness 80` (real world)
> ALL PASS, plateau moved from ~185k cr to ~42k cr (real competitors now claim market share).
> `nation.resource_abundance` deliberately kept — independent of the substrate mechanism
> (NR-172). Three new NEEDS_REVIEW entries: NR-170 (pre-existing population_mvp failure,
> unrelated), NR-171 (data_creep_harness 1500-tick building growth from the larger corp
> population — not a leak, needs a longer-horizon rerun to confirm convergence), NR-172
> (resource_abundance keep decision). `ai_skill_harness` golden-band drift (NR-169) not
> re-blessed this session — left for a dedicated stewardship pass, now that the
> substrate-to-real-firms transition BL-365 itself anticipated as the likely trigger is done.

> Drained 2026-08-11: BL-130 (real market inventory), the last link in BL-365's blocker chain,
> 1 task COMPLETE — removed per the retain-one policy. Record lives in DEVLOG.md (2026-08-11
> entry), backlog.json BL-130's `resolution` field, and requirements.json
> § real-market-inventory (R1–R5). Found and fixed a real, pre-existing bug while diagnosing an
> unrelated crash: BL-368's three new resources were never registered in
> `recipe_registry.cpp`'s Lua name table, so the actual game had been crashing on startup since
> BL-368 landed earlier this session. Also fixed three existing harnesses whose fixtures assumed
> the retired unconditional-auto-buy model. BL-365 (the keystone) is now unblocked.

> Drained 2026-08-11: BL-263 (spontaneous market emergence), promoted out of turn to unblock
> BL-365 (blocked_on BL-130, which requires BL-263), 1 task COMPLETE — removed per the
> retain-one policy. Record lives in DEVLOG.md (2026-08-11 entry), backlog.json BL-263's
> `resolution` field, and requirements.json § spontaneous-market-emergence (R1–R5). Corrected an
> earlier stale-exe reading in NR-169 in passing: BL-368/BL-263 do not move `ai_skill_harness`'s
> golden bands beyond what BL-366 already did. BL-130 (real market inventory) is next — now the
> only thing standing between here and BL-365 itself.

> Drained 2026-08-11: BL-368 (real population demand + habitability tranche), Sprint 10's second
> foundation item, 1 task COMPLETE — removed per the retain-one policy. Record lives in DEVLOG.md
> (2026-08-11 entry), backlog.json BL-368's `resolution` field, and requirements.json
> § real-population-demand-habitability-tranche (R1–R5). Found and fixed a stale-doc issue in
> passing: the "known bug" BL-368's own design cited had already been fixed by BL-190
> (2026-07-31); MARKETS.md repeated the same stale claim, corrected this pass. BL-365 (the
> keystone) and BL-367/BL-130/BL-132/BL-369 stay `designed`, not yet promoted.

> Drained 2026-08-11: BL-366 (multi-building tile stack cap + urban transform), Sprint 10's
> first foundation item, 1 task COMPLETE — removed per the retain-one policy (light-weight enough
> to land in one direct pass rather than a multi-task promotion). Record lives in DEVLOG.md
> (2026-08-11 entry), backlog.json BL-366's `resolution` field, and requirements.json
> § multi-building-tile-urban-transform (R1–R5).

> Drained 2026-08-09: build-heavy v0.1.1 batch — render-precision audit (BL-215, 5 tasks
> COMPLETE) and selection always open (BL-266, 3 tasks COMPLETE; R4 golden re-bless pending,
> NR-104) — removed per the retain-one policy. Record lives in DEVLOG.md (2026-08-09 entry),
> backlog.json BL-215/BL-266 `resolution` fields, and requirements.json
> § text-wrap-render-audit (R1–R6) / § selection-always-open (R1–R4).

> Drained 2026-08-08: critique batch (BL-326/327/328/329/330), all 3 tasks COMPLETE — removed
> per the retain-one policy. Record lives in DEVLOG.md (2026-08-08 entry), backlog.json's five
> `resolution` fields, and requirements.json § critique-batch-ui-polish (R1–R6).
>
> Drained 2026-08-08: unit hire surface (BL-324), all 5 tasks COMPLETE — removed per the
> retain-one policy. Record lives in DEVLOG.md (2026-08-08 entry), backlog.json BL-324/BL-157
> `resolution` fields, and requirements.json § unit-hire-surface (R1–R7).
>
> Drained 2026-08-08: buildings rework first slice (BL-323 S1 partial + S3 + S4; S2/S2b were
> already landed pre-promotion), all 4 tasks COMPLETE — removed per the retain-one policy. Record
> lives in DEVLOG.md (2026-08-08 entry), backlog.json BL-323 `resolution` field, and
> requirements.json § buildings-rework-first-slice (R1–R7). **Not closed**: BL-323's full S1
> (processing chains needing new resource_type values) stays `designed`, next slice ready.

> Drained 2026-08-08: military base S1 (BL-325), all 4 tasks COMPLETE — removed per the
> retain-one policy. Record lives in DEVLOG.md (2026-08-08 entry), backlog.json BL-325's design
> field (§ S1 landed), and requirements.json § military-base-s1 (R1–R5). **Not closed**: BL-325's
> S2 (hire moves onto the base) and S3 (out-of-supply decay) stay `designed`, next slices ready.

> Drained 2026-08-10: hygiene batch (BL-351 sell-order over-commit, BL-352 hire-gate live store,
> BL-353 persona eval guard, BL-354 orbital tick purity, BL-355 warning sweep, BL-356 body→market
> index, BL-357 pop-growth aggregate, BL-358 determinism sweep, BL-359 deferred demolish, BL-360
> hot-path scans) — all seven agent slices COMPLETE, merged, harness suite green — removed per the
> retain-one policy. Record lives in DEVLOG.md (2026-08-10 hygiene entry), the ten backlog.json
> `resolution` fields, and requirements.json § sell-order-pool-overcommit through
> § hot-path-spatial-scans. BL-361 (app.cpp split), BL-362 (UI frame caches), BL-363 (misc
> sweep) were filed, not delivered.

> Drained 2026-08-10: hygiene batch wave 2 (BL-361 app.cpp decomposition, BL-362 UI frame
> caches, BL-363 misc sweep) — all 6 agent slices COMPLETE, merged, build + 25 harnesses green,
> five visual checks verified against pre-wave-2 control goldens — removed per the retain-one
> policy. Record lives in DEVLOG.md (2026-08-10 wave-2 entry), the three backlog.json
> `resolution` fields, and requirements.json § app-cpp-decomposition /
> § ui-frame-recompute-caches / § misc-hygiene-sweep. The review barrier caught three real
> faults before close (NR-130/133/134 carry the residuals).

> Drained 2026-08-14: seam batch (BL-386 floor reservation fix, BL-387 seam actor authority,
> BL-396 wire parser validation, BL-397 seam read privacy) — all 13 tasks COMPLETE across two
> worktree agents + main-session doc/integration slices, review barrier ran (verdict FIX FIRST,
> three Criticals fixed pre-compile), integrating MSVC build green, smoke.js ALL CHECKS PASSED,
> order_book_harness 52/52, econ_harness/econ_stability ALL PASS, spectator_determinism golden
> re-blessed (deliberate BL-386 move). Removed per the retain-one policy. Record lives in DEVLOG.md
> (2026-08-14 seam-batch entry), the four backlog.json resolution fields, and requirements.json
> § sell-order-floor / § seam-actor-authority / § wire-parser-validation / § seam-read-privacy.
> ai_skill_harness band re-bless deliberately deferred to BL-416 (golden stewardship) with the
> post-fix numbers recorded in its design note. Follow-on filed: BL-422 (held-order phantom
> inventory).

## Chain-depth growth gate (promoted from BL-428) — **COMPLETE** (6/6, 2026-08-16)

Requirements: requirements.json § chain-depth-growth-gate (R1–R4)

BL-428's metric half landed with BL-429/430/431 (`depth_of` / `is_raw` / `max_depth`, and the
min-across-recipes / max-within-recipe asymmetry). This group builds the **gate** — the half that
makes depth the growth track rather than a readout. Ben's 2026-08-16 call: **ancient roster only**
for the first cut, so no existing industrial campaign changes shape.

Required depth is **derived, never authored**: a recipe's requirement is the depth of its deepest
input, read straight off the graph. That is the ruling's own argument — a hand-authored per-type
minimum would be the second system chain depth was chosen to avoid.

- **[2] A — `recipe_registry::recipe_required_depth(id)`: max `depth_of` over the recipe's
  inputs, 0 for an input-free recipe, -1 if any input is unreachable. Precomputed alongside
  `m_depth` in the same rebuild, so it cannot drift from the graph or from the era mask.**
  Files: `src/world/recipe_registry.hpp`. Deps: foundation. Satisfies: R1, R4.
- **[2] B — Per-corp reached depth: `corporation_component.produced_ever` (a
  `std::array<bool, resource_count>`, produced-ONCE-EVER per the design's legibility call) plus
  `reached_depth(reg)`. Never cleared — idling or demolishing must not lower it.**
  Files: `src/world/components.hpp`. Deps: foundation. Parallel-safe with A. Satisfies: R2.
- **[1] C — Record production: set the owning corp's `produced_ever` bit wherever a good is
  actually made, in both `run_processing` and `run_extraction`.** Files:
  `src/world/economy_system.cpp`. Deps: B. Satisfies: R1, R2.
- **[2] D — The gate: refuse an ancient-band recipe whose required depth exceeds the corp's
  reached depth, as `construction_result::depth_locked`, beside the existing `era_locked`.
  Mirror it into `corp_command_result` (the seam's switch is exhaustive — the BL-433 lesson) and
  filter the Build door so a locked method is not offered in the first place.** Files:
  `src/world/construction.{hpp,cpp}`, `src/world/corp_command.{hpp,cpp}`,
  `src/ui/construction_panel.cpp`. Deps: A, C. Satisfies: R1, R3.
- **[2] E — Guard: extend `tools/verify/chain_depth.cpp` with the four rows, including BL-432's
  no-unreachable-building assertion and the two-load byte-identity check.** Files:
  `tools/verify/chain_depth.cpp`, `.claude/skills/verifier-headless/SKILL.md`. Deps: D.
  Satisfies: R1, R2, R3, R4.
- **[1] F — Propagate to the authority doc and retire NR-246's dead check.** Files:
  `docs/economy/PRODUCTION.md`, `scripts/verify/building_management_shell.lua`. Deps: D.

**Parallelisation.** A ∥ B (disjoint headers, no shared type); C after B; D after A+C; E, F after
D. The whole group is one vertical seam of ~5 logic files with `construction.cpp` as the hotspot —
merge cost exceeds the saving, so it runs in the main session rather than fanning out.

**Closed 2026-08-16. All six tasks complete; `chain_depth` reports 11 new assertions ALL PASS
(ancient ladder climbs to depth 3, nothing stranded); `econ_harness`, `construction_harness`,
`era_roster` green; `ProjectIo` builds clean.** Three things were found mid-build and folded in
rather than deferred, each because leaving it would have made the gate a half-gate:

- **The retool path needed the same gate.** Guarding only `construct_building` left a one-click
  bypass — place the shallowest ancient method the corp can reach, then switch onto the deepest
  sibling in the same group, and the ladder never has to be climbed. `try_switch_recipe` now
  refuses with `recipe_switch_result::depth_locked`, mapping to the same
  `rejected_depth_locked` on the seam so an agent cannot tell the two routes apart.
- **A real pre-existing wrong-recipe bug (NR-254).** The Build door stored the *browse* index
  (era-masked) in `candidate.recipe` and passed it where an *absolute* id was expected. The two
  spaces coincide exactly while the mask is the identity — every `any`-band campaign — so it was
  invisible in normal play and would have stayed invisible, while silently naming the wrong
  recipe in precisely the ancient campaigns BL-429/430/431 have been building out.
- **The verify script could not reach the pages it claimed to check.** NR-246's rewrite needed a
  new verb, `verify.building_page(n)`: `fold("building_metric", k)` sets the drill *key*, not the
  page, so the first rewrite still captured page 1 three times. The first honest photograph of the
  Method page immediately surfaced a live layout defect (NR-255, profit figure overprinting the
  method name) — which is the argument for the check existing.

One limitation stated rather than hidden: the gate is scoped to **ancient-band recipes** by Ben's
first-cut ruling, so the industrial roster is ungated and chain depth does not yet gate anything a
1960 campaign can see.

---

## Era-gated economy roster (promoted from BL-433) — **COMPLETE** (5/5, 2026-08-15)

Requirements: requirements.json § era-gated-roster

The mask-not-removal constraint is the whole shape of this group: a recipe's id is its index in
`m_recipes` and that id is **stored** in `building_component.recipe`, so filtering by deletion
would silently repoint every building whose recipe sat after a filtered one. Storage stays
absolute (`get_recipe`/`recipe_id`); browsing goes through the era mask (`recipe_count`/
`recipe_at`). Task A establishes that split and everything else depends on it.

- **[2] A — Add the era band to the registry: `era_band` enum, `era` field on `recipe` and
  `building_economics`, the `m_allowed` position mask, `set_era()`, `building_available()`, and
  `era_band_for_epoch()`. `recipe_count`/`recipe_at` map through the mask; `get_recipe`/
  `recipe_id` stay absolute.** Files: `src/world/recipe_registry.hpp`. Deps: foundation.
  Satisfies: R2, R3.
- **[2] B — Parse `era = "any"|"ancient"|"industrial"` for both recipes and buildings, throwing
  on an unknown string with the offending entry named.** Files: `src/world/recipe_registry.cpp`.
  Deps: A. Satisfies: R4.
- **[1] C — Tag the space-era data: `launchpad` and the petroleum/propellant/spacecraft-chain
  recipes as `industrial`; leave everything shared untagged (defaults to `any`).** Files:
  `scripts/economy.lua`, `scripts/recipes.lua`. Deps: B. Satisfies: R1.
- **[1] D — Set the band from the live campaign: derive it from `world_params::epoch_year` in
  `load_economy()`, against the same 1700 threshold the antiquity branch already documents.**
  Files: `src/core/app.cpp`. Deps: A. Satisfies: R1.
- **[2] E — Guard harness `tools/verify/era_roster.cpp`: the four requirement rows, including the
  byte-identity check for the default band.** Files: `tools/verify/era_roster.cpp`,
  `.claude/skills/verifier-headless/SKILL.md`. Deps: C, D. Satisfies: R1, R2, R3, R4.

**Parallelisation.** A is the foundation and is not splittable — every other task reads the types
it introduces. B/D are disjoint after A (`recipe_registry.cpp` vs `app.cpp`) and could fan out,
but the group is small enough that the merge cost exceeds the saving; running it in the main
session. C is data-only and trivially serial after B. E last, because it verifies the others.

**Closed 2026-08-15. All five tasks complete; `era_roster` reports 15 assertions ALL PASS.**
Two things were found mid-build and folded in rather than deferred, both because leaving them
would have made the gate a half-gate:

- **The seam needed its own refusal code.** `map_construction`'s switch over `construction_result`
  is exhaustive, so `era_locked` could not simply be added — and folding it into
  `rejected_placement` would have been exactly the overloading BL-395 (untyped result line) exists
  to complain about. Added `corp_command_result::rejected_era_locked`, distinct from
  `rejected_tech_locked` because no amount of research reaches an era-locked type.
- **The build door had to filter too.** The gate refusing a Launchpad is not the same as the
  player never being offered one. The processing rows needed nothing — they are built from
  `recipe_count`/`recipe_at`, which *are* the masked browse path, which is the design paying for
  itself.

One limitation stated rather than hidden: `draw_tile_selection`'s "is anything placeable here"
hint takes no registry and so is not era-aware. It only fires when nothing else is placeable, and
processing/port/hub accept any land tile, so a launchpad-only true cannot occur in practice —
but it is a signature change away from being exactly right if that ever stops holding.

---

## Nation/corp generation visibility (promoted from BL-305) — **PAUSED, no tasks started**

**Resume here.** Paused 2026-08-08 before any code (see NR-085): task A's file scope
(`hard_coded_world.cpp`, `app.cpp`) exactly matches uncommitted, unreviewed work already in the
tree from another session (the New World wizard's async real-surface preview pane + the Era −1
sim's terrain-view adapter — see DEVLOG's 2026-08-08 audit-note entry). Resume once that work
has either landed (rebase onto it) or is confirmed gone/safe to build around — check `git status`
for `src/ui/generation_preview.{cpp,hpp}` and `src/world/sim_terrain_build.hpp` first.

Requirements: requirements.json § nation-corp-generation-visibility (R1–R5)

- **[2] A — Live territory-carve stage.** Extend BL-256's generation-screen globe with a stage
  that animates the Voronoi BFS carve as it runs, rather than only being inspectable after
  generation completes. Files: `src/world/hard_coded_world.cpp`, `src/core/app.cpp`. Deps:
  foundation. Satisfies: R2.
- **[2] B — Corp asset-placement overlay.** Render corp asset seeding spatially on the same
  generation-screen canvas/globe as A. Files: `src/world/hard_coded_world.cpp`, `src/core/app.cpp`.
  Deps: A (shares the generation-screen staging A introduces). Satisfies: R3.
- **[1] C — Corp financial-profile card.** Surface financial-profile derivation as a card/ledger
  entry (not a canvas overlay) alongside B. Files: `src/core/app.cpp`. Deps: A. Parallel-safe
  with B (disjoint UI regions once A's staging exists). Satisfies: R4.
- **[1] D — Coverage pass.** Walk GENERATION_STRATEGY.md's pass map; confirm every step now has
  a named visibility surface (BL-256, BL-303/304, BL-211, and A–C above), and add an explicit
  "invisible by design" note to any step that doesn't. Files: `docs/generation/GENERATION_STRATEGY.md`,
  `docs/generation/NATION_GENERATION.md`, `docs/generation/CORPORATION_GENERATION.md`. Deps: A, B, C
  (needs the finished surface list to audit against). Satisfies: R5.

Parallelisation note: A is the foundation (the staging both B and C hang off); B ∥ C once A
lands (disjoint UI concerns — canvas vs. card); D runs last, after the surfaces it's auditing
exist.

> Drained 2026-08-09: header chrome tightening (BL-312/BL-313) — the work landed in 9ecbbcf but
> the section was never flipped; items were closed retroactively by the NR-075 cut audit and the
> requirement group closed 2026-08-09 (verification overtaken by the closure). Record lives in
> req/requirements.json (§ header-chrome-tightening) and NR-075.

## Tech tree radial canvas (promoted from BL-310) — **COMPLETE**

Requirements: requirements.json § tech-tree-radial-canvas (R1–R10, all met). Round 1: Era 0/1
gate quests render as a radial constellation (rings = graph depth or authored tier, sectors =
quests), keystones larger/gold, Era-1 branch pairs colour-differentiated with an "excludes"
mark, nav slot 4 wired. Round 2 (same session, Ben's live-playtest feedback): converted to a
full-canvas takeover (`ui::canvas_rect()`, BL-265's task 1, first consumer) with a drawn
top-left `‹` return control; NR-054 resolved — the canonical ancient ladder JSON (71 nodes, 5
keystones) now renders on the Antiquity tab as a muted read-only history; Standing lines
dropped from rendering, tab 3 relabelled "Era 2" (placeholder only, data stays in
`tech_tree.lua`). Pan tried left-click, reverted to middle-click for consistency. Round 3 (same session): era
tabs are now icon-only, bigger, each icon a real tiny render of that era's own nodes (not a
generic glyph); on-canvas labels switched from bare id to name/short_name (short_name
hand-authored for the Era-1 keystones + branches this pass, other ~120 nodes fall back to
truncated name). Verified via `scripts/verify/tech_tree_panel.lua` — 3/3 golden PASS (tabs,
era1, antiquity), goldens re-blessed against every intentional change across all three rounds.

> Drained 2026-08-09: corp standing profile (BL-262 first slice, 4 tasks COMPLETE,
> standing_harness 41/41), Mediterranean rift sea (BL-276, R1–R4 met, 500-seed sweep), and
> Sprint 5 Era −1 foundation wave (BL-272 combat engine + BL-273 province demography, harnesses
> green) — removed per the retain-one policy. Records live in DEVLOG.md and
> req/requirements.json (§ corp-standing-profile, § mediterranean-rift-sea,
> § unit-doctrine-combat, § province-demography).

---

> Drained 2026-08-02: BL-270 (action dictionary) and BL-268 (planetary canvas cull + cache),
> both COMPLETE, removed per the retain-one policy — their record lives in DEVLOG.md and
> req/requirements.json (§ action-dictionary, § planetary-pan-perf).

---

> Drained 2026-08-02: Sprint 2 (BL-210 oral-history pivot: BL-217 checkpoint/branch model, BL-208
> world history log, BL-218 nations settlement rewrite, BL-219 corporations history rewrite — all
> four `complete` in backlog.json, BL-218/219's code landed via commit 0b9351d) and Sprint 1
> (procgen v1's food cluster — BL-166/168/170, all complete) removed per the retain-one policy —
> record lives in DEVLOG.md, backlog.json `resolution` fields, and requirements.json
> § checkpoint-branch-model / § world-history-log / § settlement-history-rewrite /
> § hydroponics-bay / § fishing-wharf / § river-generation.

The **active, prioritised, actionable worklist** (formerly TASKS.md). Unlike the backlog
([`backlog.json`](backlog.json) metadata + [`BACKLOG.md`](BACKLOG.md) design bodies), every
entry here is a concrete, file-scoped, individually-buildable step ready to execute. Tasks are
**promoted** from a `designed` (`✓`) backlog item (see [`DELIVERY.md`](DELIVERY.md)) and cleared
as they complete — this file is transient and is expected to be empty between work blocks.

> **Proportionality (see DELIVERY.md § Proportionality, and Rule 0).** Promoting an item into
> this file is for *substantial* (Full-mode) work. A quick low-risk high-value change — a
> one-file fix, an obvious cleanup, a cheap optimisation — does **not** need a task group or a
> REQUIREMENTS table: make and verify it directly, then commit. Reach for the full lifecycle
> only where its coordination cost pays back; applying it to trivial work is over-engineering.

## Task format

List tasks in **execution order**, grouped by the item they were promoted
from. Each task carries:

- **A group-scoped ID letter** (A, B, C, …), so dependencies and parallel pairs
  can be named.
- **A difficulty** in brackets (the backlog 1–5 time scale; tasks carry difficulty
  only — priority is a backlog-level triage concept, not a per-task field).
- **A one-line action** — imperative; what to change.
- **File scope** — the files the task is expected to touch. This is what makes
  collisions between tasks visible.
- **Dependencies** — which sibling tasks must land first (or "foundation" /
  "independent root").
- **Parallelisation** — whether it can run concurrently with a sibling, and
  whether as a sub-agent. Only true when the file scopes are **disjoint**.

End each group with a **parallelisation note**: the dependency shape and which
roots are safe to fan out. Concurrent sub-agents run in **separate git worktrees**
(the isolation mechanism); the collision map is a *splitting heuristic* for carving
focused agents, not a hard disjointness gate. Agents build and commit on their own
worktree branch; the integrating session merges, builds, and verifies. See
[`DELIVERY.md`](DELIVERY.md) § Sub-agents & worktrees for the authoritative model.

---

> Drained 2026-08-02: the disclosure spine (2026-08-01 — BL-214/BL-247/BL-248, all
> complete) removed per the retain-one policy — record lives in DEVLOG.md,
> backlog.json BL-214/BL-247/BL-248 `resolution`, and requirements.json
> § drill-through-fold / § chart-question-log / § corp-dashboard-rollups. Authority:
> `docs/ui/LAYOUT.md` § Drill-through, `docs/ui/MENU.md` § Slot 1. The visual-golden
> staleness it surfaced is filed separately as BL-259.

---

## <Group name> (promoted from BACKLOG § <item>)

Requirements: requirements.json § <slug>

- **[<difficulty>] A — <action>.** Files: `<paths>`. Deps: foundation. Satisfies: R1, R2.
- **[<difficulty>] B — <action>.** Files: `<paths>`. Deps: A. Parallel-safe with C. Satisfies: R3.
- **[<difficulty>] C — <action>.** Files: `<paths>`. Deps: A. Parallel-safe with B. Satisfies: R4.

Parallelisation note: A → {B, C}; B ∥ C (disjoint files). Promote D once B and C
land; D depends on both.
```

Requirement records (the data + permanent history) live in
[`req/requirements.json`](req/requirements.json); the schema and workflow policy are in
[`req/REQUIREMENTS.md`](req/REQUIREMENTS.md).

## Definition of "complete"

A task is **complete** only when, for **every** requirement it satisfies, all three
of the following hold — completeness is measured against the requirements, not
against "the code is written":

- **Reviewed** — the change has been read back against each requirement it claims to
  satisfy, for correctness and for project conventions (naming, comments,
  determinism).
- **Implemented** — the change exists and the affected target builds clean.
- **Tested** — each requirement's **Verification** has actually been *run* and
  passed, not merely assumed. A requirement whose verification has no available
  skill or tool follows the verification-method policy in
  [`req/REQUIREMENTS.md`](req/REQUIREMENTS.md) (§ Verifying when no skill or tool
  exists) — until its method is run (or explicitly deferred and the requirement
  left `pending`), the task is **not** complete.

A task that is implemented and builds but whose requirements have not all been
reviewed and tested is *code-complete*, not complete. Only mark a group cleared
(and its item removed) when its requirements are complete by this definition,
or its remaining rows are explicitly accepted as out of scope. See also
[`../GLOSSARY.md`](../GLOSSARY.md) **Complete (task state)**.

## Cancelling a task group

REFINED.md is a **working state**: a group is meant to be driven to *complete* (see
above) in **one working block**. A group that cannot be — blocked, out of time, or
superseded — is **cancelled** rather than left half-tracked. Cancelling a group:

1. **Marks its requirements `failed`** in [`req/requirements.json`](req/requirements.json)
   (with the reason in `notes`), so the failed attempt is on record. Rows genuinely met
   before the block stalled keep their real status. The group `status` is then flipped to
   `"cancelled"` with a `resolution` recording the cancellation — the record is never
   deleted. Re-promoting flips it back to `"active"`.
2. **Rewrites the group's task intent back into the backlog** (a new or existing item in
   [`backlog.json`](backlog.json) / [`BACKLOG.md`](BACKLOG.md)) as described intent, **merging
   into a related existing item** where one exists rather than duplicating.
3. **Removes the task stubs** (the A–F entries) from this file.

Cancelling reverts *tracking*, not committed code — code already landed stays in the
tree; its intent simply returns to the backlog to be re-promoted later. A group is
thus always in one of two terminal states: **completed**, or **cancelled** back to
the backlog. See also [`../GLOSSARY.md`](../GLOSSARY.md) **Cancelled (task state)**.

## Pausing a task group (deliberate handoff)

Driving a group to *complete* in one block is the default, **not** a mandate (see CLAUDE.md
§ Proportionality and session boundaries). When ending a session early serves the work — the
batch is large, context is drifting, or a natural checkpoint is reached — **pause** the group
rather than force completion or cancel it. A paused group is a deliberate scoping choice,
distinct from a *cancelled* one (which reverts intent to the backlog): the tasks stay in this file,
ready for the next session to resume.

Pausing is only legitimate if the stop is **clean and resumable**:

1. **REFINED.md is true to state** — completed tasks marked done, the in-flight task marked as
   the resume point, untouched tasks left as-is. No silent half-edits.
2. **The build is green, or the breakage is noted** — if the tree does not build, say exactly
   why and what the next session must finish to green it.
3. **A one-line handoff** records where to resume ("resume here: D — wire the panel into
   `app.cpp`; B/C landed and verified").

A paused group is therefore *not* a terminal state — it is an explicit, recorded intermission.
The barrier semantics for a multi-item set still hold *within* a session; pausing is how a
session boundary is drawn *between* them.

---

## Dividing work across agents & authoring tasks

This is the method used to promote an item and (optionally) fan it out to
parallel sub-agents. It is descriptive of how the v0.0.3 Environment groups were
run; follow it when promoting future work.

> **Note (2026-06-16):** the **authoritative** sub-agent model is now
> [`DELIVERY.md`](DELIVERY.md) § Sub-agents & worktrees — **worktrees are the primary isolation
> mechanism** and agents build/commit on their own branch. The decomposition and
> file-mapping guidance below still holds (it is how you carve focused agents); where this
> section's older "disjoint write-sets / sub-agents don't commit" phrasing conflicts with the
> worktree model, DELIVERY.md wins.

### 1. Decompose, then map every task to its files

Break the intent into the **smallest independently-buildable steps**, foundation
first. For each step, write down the **exact files it will write** — not roughly,
literally. This file list is the single most important field: it is what makes
collisions visible and is the input to every parallelisation decision below. A
task whose file scope you cannot name yet is not ready to promote; it needs more
design first.

Build the **collision map** before deciding anything about agents: list each file
and the tasks that touch it. Hotspot files (touched by many tasks) and shared
headers/data-model files are where parallelism dies — see them early.

### 2. The rule for what can run concurrently

Two tasks may run as **concurrent agents only if their file write-sets are
disjoint.** No exceptions — two agents writing the same file race and corrupt
each other's edits. Consequences that follow directly from this rule:

- **Passes inside one generator do not parallelise.** This codebase keeps all
  passes of a generator in one `.cpp` (e.g. `tile_generation.cpp` holds six
  passes). They share a file, so they are sequential. Do **not** split a file
  into per-pass translation units just to win parallelism — fighting the
  codebase's structure costs more than it saves. Concurrency lives **across**
  groups, not within a generator.
- **Compile/data dependencies gate whole groups.** If group B reads a type or
  data that group A produces (corporations need the nation component to exist and
  nations to be populated), B cannot start until A has landed — regardless of
  whether their files happen to be disjoint.
- **Prefer whole-pipeline-per-agent over pass-per-agent.** When a group's passes
  share a file, give the *whole group* to one agent that works sequentially
  inside it, rather than trying to slice it.

### 3. Adjust the design to *create* disjointness (cheaply)

Small, principled data-model choices can turn a collision into two disjoint
scopes. Example from v0.0.3: storing tile→nation ownership in a `world` map
(`tile_to_nation`) instead of adding a field to `tile_component` kept the nation
group off `tile_generation.{hpp,cpp}`, so it could run concurrently with the tile
tuning group. Make these moves when they are also *good design*; never contort the
model purely for parallelism.

### 4. Keep hotspot files and integration in the main session

The file every group eventually touches (here, `hard_coded_world.cpp` — the
campaign wiring) is **never given to a sub-agent.** The integrating (main) session
owns it, wires each group's entry point as that group lands, runs the build, and
verifies. **Sub-agents do not build or commit** — they write code on a disjoint
scope and report their public signature + the one-line hook the integrator should
add. This keeps the one shared file single-writer and makes integration a small,
deliberate step rather than a merge.

### 5. Author a task so a cold agent can execute it

A sub-agent starts with **no memory of the planning conversation.** A task handed
to one must therefore carry, in its own text: the authoritative design doc to
read, the exact file scope (and an explicit "do not touch X" for the hotspots),
the project conventions (naming, comments, determinism), the entry-point/signature
to expose, the "do not build/commit" instruction, and the report-back shape.
Known footguns belong in the prompt too (e.g. the member-name-shadows-enum-type
qualification trap). If the task can't be executed from its text alone, it is
under-specified.

### 6. Run order

Group concurrent agents into **waves** of disjoint scopes; run dependent groups in
later waves. After each wave the integrator wires hooks, **builds, and verifies**
(headless harness for pure `world/*` logic per `reference_headless_build`; a full
`cmake` build to confirm the real target still links) before starting the next
wave. Verify retroactively — do not assume an agent's self-reported success.

---

## The inter-body pull reads a counterpart market (promoted from BL-406, then BL-404) — 2026-08-17

Requirements: requirements.json § interbody-pull-counterpart (R1–R4)

Two items, strictly in order: **BL-406 (home market is an arbitrary pick)** first, because it
defines *what* is being netted against; **BL-404 (inter-body pull is unnetted)** second.

Ben's ruling 2026-08-15 took **BL-406 option (c)** — a per-counterpart-market pull — and deferred
BL-404's own a/b/c to the same pass. Outpost prices are cleared to move.

> **Closed 2026-08-17, all five tasks complete, in ONE commit rather than the usual one-per-item.**
> The two items rewrite the same twenty lines, and BL-404's own ruling put its decision inside the
> pass that lands BL-406. Splitting them would mean committing an intermediate whose own guard
> (C3) fails by design — a red commit for the sake of a boundary the ruling had already dissolved.

**Baseline, measured before any change** (`interbody_pull_harness`, seed 0, 60 ticks, MSVC): the
home body carries **9** markets holding 3861.3 supply and 34.0 demand; the market
`market_for_body` hands the pull holds **0.0 supply and 1.81 demand — 5% of the body's**.

### Tasks

- **A — define the counterpart relation** (`src/world/market_clearing.cpp`). A per-resource
  `counterpart_home_market(w, r)`: the home-body market with the greatest demand for `r`,
  lowest market id as tiebreak. Order-independent by construction, so it is stable across
  standard-library container orders. — **complete**
- **B — the pull reads it** (`src/world/market_clearing.{hpp,cpp}`). Retire the single
  `market_for_body(w.home_body)` read; select per resource inside the loop. — **complete**
- **C — the netting becomes real** (BL-404; `market_clearing.{hpp,cpp}`). Snapshot every market's
  supply **before** `clear_markets`' reset loop and net the counterpart's snapshot supply. This is
  BL-404 option **(b)**, one tick of lag, no reordering, no new persisted state. — **complete**
- **D — the guard** (`tools/verify/interbody_pull_harness.cpp`, `market_emergence_harness.cpp`).
  Behavioural assertions on `inject_interbody_demand`'s observable output, run against the
  **pre-change** build first and observed to fail — **3 of 3 did**. — **complete**
- **E — propagate** (`docs/economy/MARKETS.md`). Authority time-slice: the settled design moves
  out of the two items and into the market doc as part of landing. — **complete**

---

# Sprint 21 — the fight becomes reachable (PROPOSED, not promoted; Ben has not opened Sprint 21)

Decomposition for the military engagement surface, carved for **parallelism**. Written 2026-08-17
from a source audit, not from the item text — see SPRINTS.md § Sprint 21 for the BUILT / ABSENT
table and why the sprint's shape changed.

**The one-sentence framing.** The battle resolvers are built and unreachable. Everything below
exists to give them a caller: a verb, a target rule, a trigger, a state store, and a screen.

## The collision map

| File | Slice that OWNS it | Also read by |
|---|---|---|
| `src/world/corp_command.hpp` / `.cpp` | **F1** | S1, S3 (read-only) |
| `src/world/construction.cpp` (demolish) | **F1** | — |
| `src/world/standing.{hpp,cpp}` *(new)* | **F2** | S1, S2, S3 |
| `src/world/battle_system.{hpp,cpp}` *(new)* | **S1** | S2, S4 |
| `src/world/unit_movement.{hpp,cpp}` *(new)* | **S1** | — |
| `src/world/world.hpp` (one new store) | **F1** *(declared once, up front)* | S1 |
| `src/world/corp_ai.cpp` | **S2** | — |
| `src/ui/*` (`selection_panel`, `body_surface_canvas`, new battle window) | **S3** | — |
| `docs/ai/ACTIONS.json`, `docs/ui/question_log.json` | **S4** | — |
| `src/world/history_sim.cpp`, `tools/verify/history_sim_harness.cpp` | **S5** | — |

**No slice below writes `market_clearing.cpp`, `economy_system.cpp`, `budget_system.cpp` or
`scripts/economy.lua`.** That is deliberate and it is the reason BL-325's out-of-supply decay is cut
(§ what this does not carry). Three agents are live in that seam; nothing here goes near it.

## Foundation — run ALONE, and first

These two are not parallelisable with anything, including each other's review. F1 defines the seam
every later slice calls; F2 defines the predicate every later slice tests.

- **[F1] The `unit_command` seam** — BL-393 (units are write-only and inert).
  Files: `src/world/corp_command.hpp`, `src/world/corp_command.cpp`, `src/world/construction.cpp`,
  `src/world/world.hpp`.
  Append (**append-only**, after `cancel_contract` — the enum is serialised by contract even though
  no serialiser exists yet) `move_unit`, `disband_unit`, `merge_unit`, `engage`. Validate each: the
  acting corp owns the unit, the destination tile exists, a rejection mutates **nothing**. Settle
  BL-393's third gap explicitly — `demolish_building` currently never touches `w.units`, so a
  demolished muster base orphans its garrison; refuse the demolish (`rejected_state`) or disband
  with an agency event, but do not leave it silent. Declare the battle store on `world` here, in one
  pass, so S1 does not have to reopen `world.hpp` later.
  Deps: none. **Blocks: everything except S5.** Cannot fan out.

- **[F2] Hostility and standing** — the missing capability, and it needs a Ben ruling before code.
  Files: `src/world/standing.{hpp,cpp}` *(new)*, `docs/SYSTEMS.md`.
  There is no model anywhere of who may fight whom. A pure, deterministic predicate
  `may_engage(w, attacker_corp, defender_corp)` plus whatever minimal state it reads. BL-315 (armed
  house conflict spine) settled that army/mercenary/pirate are **derived readings** of one company —
  that is a naming rule and answers nothing about targeting, so the ruling is genuinely open.
  **File this as a backlog item and get it answered in session one**; it is Sprint 21's equivalent of
  BL-440's fork, and leaving it open serialises the whole sprint.
  Deps: none (can be *designed* alongside F1; its code lands after F1 only because S1 needs both).
  **Blocks: S1, S2, S3.** Cannot fan out.

## Fan-out — all four run CONCURRENTLY once F1 and F2 land

Disjoint write sets by construction. Each is a coherent vertical slice, narrow enough for one agent
reading two or three docs.

- **[S1] Movement, contact, and the battle loop** — BL-315 (armed house conflict spine), the half
  that is actually missing.
  Files: `src/world/unit_movement.{hpp,cpp}` *(new)*, `src/world/battle_system.{hpp,cpp}` *(new)*,
  `tools/verify/engagement_harness.cpp` *(new)*.
  Three things, all headless: (a) apply `move_unit` over ticks with a deterministic step rule;
  (b) detect contact — two units on one tile whose owners satisfy `may_engage` — and open a
  `campaign_battle_state`; (c) **the stack adapter**, `unit_component` → `army_stack_entry`, which
  does not exist and is where NR-247's raw-vs-fixed-point `strength` must finally be settled rather
  than deferred a fourth time. Step rounds from the sim loop; apply the per-mille losses to real
  units on end, since neither resolver mutates anything itself.
  Docs to read: `campaign_battle.hpp`, `unit_roster.hpp`. **Not** the design corpus.
  Deps: F1, F2. Concurrent with S2, S3, S4, S5.

- **[S2] The AI can see and use its force** — BL-393 gap 1, plus military scoring.
  Files: `src/world/corp_ai.cpp`.
  `export_corp_blackboard` has four sections and none mentions units; add unit facts (id, tile,
  type, count, strength) as `own_asset` — a corp reading its own force engages no visibility rule.
  Then score the new verbs in the same deterministic scored-utility layer, under the standing rules'
  existing BL-202/BL-203 grant. Availability is cash-free, spend is not (the NR-218 ruling).
  Deps: F1, F2. Concurrent with S1 — the seam is F1's, so S2 codes against the enum, not against S1.

- **[S3] The player can see and drive the fight** — the UI half, plus BL-405.
  Files: `src/ui/selection_panel.cpp`, `src/ui/body_surface_canvas.cpp`, `src/ui/icons.{hpp,cpp}`,
  new battle window, `scripts/verify/engagement.lua` *(new)*.
  Move orders from the unit Selection card; a battle window that steps rounds with a **Withdraw**
  button whose displayed cost rises per round (the withdrawal price is already modelled — surface
  it); outcomes into the comms dock; a force ledger. **BL-405 (hire has no price on screen) rides
  here** — one line, same file, and shipping it separately is ceremony.
  Deps: F1, F2. Concurrent with S1 — render from world state; a stub battle store is enough to build
  against.

- **[S4] The dictionary and the justification store** — BL-314 (unit verb family), docs only.
  Files: `docs/ai/ACTIONS.json`, `docs/ui/question_log.json`, regenerated mirrors.
  **Transcribed from F1's seam, never authored ahead of it** — preconditions mirror the rejection
  enum exactly, as the gameplay family already mirrors `corp_command_result`. A `question_log` entry
  per new surface S3 adds.
  Deps: F1 only (not F2, not S1). Concurrent with everything. Cheapest slice on the board.

## Independent — starts on day one, alongside F1

- **[S5] The Era −1 sim conquers nothing** — BL-384.
  Files: `src/world/history_sim.cpp`, `tools/verify/history_sim_harness.cpp`.
  267 battles and zero provinces taken across 1960 years; no assertion covers conquest count, which
  is why six existing reds never surfaced it. Shares **no file** with any slice above and depends on
  none of them. Give it to an agent immediately.
  Deps: none. Concurrent with F1, F2 and all of S1–S4.

## Recommended first fan-out

**Two agents, not five.** Session one runs **F1** and **S5** in parallel — the only two slices with
no dependency and no shared file — while **F2's ruling goes to Ben as a question**, because it is a
design call and no agent should take it. Session two lands F2's code. Session three fans out to
four agents across S1–S4.

Fanning out wider before F1 lands produces exactly the three-incompatible-designs failure this
decomposition exists to prevent: S1, S2 and S3 all code against a verb enum that would not yet exist.

## What this sprint does NOT carry, and why

- **BL-325 (military bases and supply) tail — out-of-supply decay.** Cut on the seam, not on scope:
  it needs `economy_system.cpp` and `scripts/economy.lua`, where three agents are live. Its own item
  already sequences it behind the conflict spine (NR-177). It stays designed-but-unbuilt, and it is
  the first thing to pick up in the sprint that follows.
- **Unit-vs-building interaction — capturing or razing.** No model exists for a building changing
  hands, and inventing one drags ownership, reach and the corp border into a combat sprint. Battles
  resolve between forces; ground changes nothing yet.
- **Healing, reinforcement, veterancy.** Each is a state field and a UI surface for a system with no
  proven loop. Land the loop first.
- **Naval and siege resolution.** `resolve_battle` already excludes naval from the tactical
  calculation by design, and sieges are a doctrine stance rather than a code path. Neither is missing
  — both are deliberately out of the first cut, and stay there.
- **Diplomacy verbs at nation grain.** BL-297's, not this sprint's, and F2 must be careful to define
  hostility narrowly enough that it does not quietly become a diplomacy system.
- **Unifying the two resolvers.** Ben ruled the two-path split 2026-08-13 and `campaign_battle.hpp`
  says "do not unify it back" in its own header. The roster and calibration stay single-sourced;
  the resolvers do not merge.
- **BL-094 (player militia pivot) and the governing-body arc.** Parked with the space arc. BL-315 no
  longer requires it.

---

## BL-470 — the unit march seam (promoted 2026-08-19) — **COMPLETE** (6/6, 2026-08-19)

Settled 2026-08-19 (Ben, elicitation form). Difficulty 4, priority A, v0.1.17. Absorbs BL-393's
open half (units are write-only and inert); no longer a separate item. Full design in
`backlog.json` — read it before starting, this is a condensed task breakdown against it.

**Foundation first — everything else reads this:**

- **[1] Movement order data model.** A `movement_order` (or similar) component/field on
  `unit_component`: `{dest tile, path, progress}`. Path computed ONCE at order time via the same
  cost function `body_reach_field` uses, ties broken by ascending tile id; recomputed only when
  invalidated (a blocked step) — no per-tick Dijkstra.
  Files: `src/world/components.hpp`.
- **[2] The three verbs.** `march_unit {unit, dest tile}`, `halt_unit {unit}`, `disband_unit
  {unit}` as `corp_verb` rows extending the existing `corp_command` enum/seam — NOT a parallel
  `unit_command` type. Rejections per the design field: not-owner, invalid (nonexistent
  unit/tile, ocean dest), state (already halted, or in-battle — withdraw first). A rejection
  mutates nothing (this project's untrusted-seam rule).
  Files: `src/world/corp_command.hpp`, `src/world/corp_command.cpp`. Depends on [1].
- **[3] Per-tick movement resolution.** `march_points` per tick is AUTHORED DATA in
  `economy.lua` § military, per-CLASS (not flat — this was overturned in the same elicitation),
  keyed off the roster row's `cls`. Spent against per-tile traversal cost with fractional
  carry-over. Visit marching units in ascending entity id, AFTER battle discovery (a unit that
  marched into a hostile province this tick fights before it can march out next tick). A
  composite unit (BL-472, not this item) moves at its slowest component's class speed — leave
  the hook, don't build the composite logic here.
  Files: `src/world/economy_system.cpp`, `scripts/economy.lua`. Depends on [1], [2].
- **[4] Rider — blackboard export.** `export_corp_blackboard` gains a units section: id, tile,
  type, count, derived strength, order state. Own units only — no BL-068 visibility question
  for your own force; rivals stay gated as everywhere else.
  Files: `src/world/corp_ai.cpp`. Depends on [1].
- **[5] Rider — NR-344 resolved, write it down.** "War flips the queue": at peace, convoys claim
  the supply network first; once a corp is party to any declared hostility (either direction —
  being attacked mobilises too), its units claim first that tick. Evaluated per-corp at tick
  start from the stance table (BL-448, landed). State this explicitly in
  `economy_system.cpp`'s phase-order comment AND in `MILITARY.md` — this is a written rule now,
  not an accident of phase order.
  Files: `src/world/economy_system.cpp`, `docs/military/MILITARY.md`. Depends on [3].
- **[6] Determinism guard.** State-hash replay with marching units in flight; a harness case
  where a re-run recomputes an invalidated path identically (the recompute-on-block path is the
  one place this item could accidentally introduce iteration-order dependence).
  File: `tools/verify/` — new or extended harness, item-spanning requirement per DELIVERY.md
  step 0. Depends on [1]–[5].

**Explicitly NOT this item (own follow-ups):** merge/split and garrison/scout (BL-472,
formations); ACTIONS.json transcription (BL-314, and it now *requires* this item — do not
transcribe ahead of the seam); rival-scorer march quality (BL-450's arc — legality here only,
scoring quality is conflict-AI work); the engagement trigger itself (BL-467).

**Collision note.** `corp_command.{hpp,cpp}` and `economy_system.cpp` are both hot files for
other open work this arc (Lane C's BL-315, Lane D's tech/law items) — worktree-isolate, don't
assume disjointness.

---

## BL-474 + BL-475 — the diplomatic lens and its Selection-panel detail (promoted 2026-08-19)

Both settled 2026-08-19 (Ben, elicitation form) — read their full `backlog.json` design fields
first, this is a condensed task breakdown. BL-474 supersedes BL-449's retired table-column UI;
BL-475 is where the three transition presses (`declare_hostile`/`offer_friendship`/
`return_to_neutral`, BL-448) actually live now.

**BL-474 (Diplomatic lens, difficulty 3) tasks:**

- **[1] `overlay_mode` gets a `diplomatic` value, on the bar.** Add to the enum in
  `src/ui/ui_state.hpp` and wire it into the lens bar alongside Corporation/Country/etc. — it is
  a primary lens per Ben's ruling, not off-bar.
  Files: `src/ui/ui_state.hpp`, `src/ui/lens_bar.cpp`.
- **[2] Tile-fill rendering, same family as the Corporation lens.** Tints a rival corp's owned
  tiles by the player's stance toward it — read via `is_hostile`/`are_friends` (BL-448,
  `stance.hpp`), gated on `corp_is_discovered` (the same BL-068 visibility check BL-449's retired
  column used — reuse it, don't reinvent). Four states, four marks: Hostile red, Friend green,
  Neutral grey, **Unknown a dashed/hollow grey ring — never a blank tile** (NR-350's hard rule).
  Files: `src/ui/planetary_canvas.cpp`, `src/ui/icons.{hpp,cpp}` (the four state glyphs),
  `docs/ui/ICONS.md`. Depends on [1].
- **[3] Click-to-select jumps to the Selection panel.** Clicking a tinted tile under this lens
  selects the owning corp through the existing canvas click-to-select path (SELECTION.md's
  model) — no bespoke popup, no new selection mechanism. This is the entry point into BL-475.
  Files: `src/ui/planetary_canvas.cpp`. Depends on [2].
- **[4] Legend/key.** Every other lens carries a key explaining its marks (LENSES.md's own
  convention) — four states need one here too.
  Files: `src/ui/lens_bar.cpp` or wherever the active lens's key renders. Depends on [2].
- **[5] Doc updates.** `docs/ui/LENSES.md`'s roster table gets a `diplomatic` row (bar position,
  status); `docs/ui/question_log.json` gets an entry for the new surface.
  Files: `docs/ui/LENSES.md`, `docs/ui/question_log.json`. Depends on [1]–[4].

**BL-475 (corp ledger stance detail, difficulty 2) tasks:**

- **[1] Move the three presses from BL-449's retired column into the Selection panel.** Same
  `corp_command` verbs, same confirm-popup-on-`declare_hostile` (demolish precedent), same
  rejection handling, same NR-350 visibility gate (an undiscovered corp shows nothing beyond
  Unknown). This is a re-host, not new logic — the verbs and their validation already exist and
  are correct (BL-448, `stance_determinism.cpp` 36/36).
  Files: `src/ui/selection_panel.cpp`. Depends on nothing new — can start in parallel with BL-474.
- **[2] Stance block in the existing corp-detail layout.** Position within the Selection card
  stack is an implementation call, not re-elicited (Ben's own note) — use judgement, follow the
  existing card rhythm.
  Files: `src/ui/selection_panel.cpp`, `src/ui/selection_card.hpp`. Depends on [1].
- **[3] Dictionary + justification store.** `docs/ai/ACTIONS.json` gets the three verb entries
  **retitled** (not duplicated) from BL-449's now-retired `corporation_panel` entries to
  reference this surface instead; `docs/ui/question_log.json`'s `corporation_panel` entry is
  either retired or repointed. Regenerate both mirrors after editing.
  Files: `docs/ai/ACTIONS.json`, `docs/ui/question_log.json`. Depends on [1], [2].

**BL-449's retired UI — what to actually do with it.** Once BL-474/BL-475 are live, remove the
Stance column and its three press blocks from `src/ui/corporation_panel.cpp` entirely (the
`corp_is_discovered`/`stance_label` helpers may still be useful — check before deleting) rather
than leaving a second, unreachable copy of the same interaction sitting in dead code. Flip
BL-449's own `backlog.json` status once this cleanup lands — it currently reads `designed`
(reopened) and should move to `complete` only once the column is actually gone, not before.

**Collision note.** `src/ui/selection_panel.cpp` is BL-475's own file only in this batch — no
known collision this session. `src/ui/planetary_canvas.cpp` may be touched by other lens work;
worktree-isolate regardless.

---

