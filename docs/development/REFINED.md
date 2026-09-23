# REFINED — active worklist

## Drained 2026-09-16

Three finished blocks were cleared at the session close; each one's record lives in the
DEVLOG entry for its session, its requirement group, and the archived backlog rows.

- **Sprint 41 — Exploration trade** (COMPLETE 2026-09-14). Nine items; world shape authorised
  at Alarm 525. Plan and collision map in `cacd27dc`.
- **Post-sprint-40 review block** (opened 2026-09-13). Its last open task was the lapse pace
  control, which landed 2026-09-16 at 30 s / 1 m / 1 m 30 s after Ben watched the first cut.
- **Sprint 42 wave 0** (COMPLETE 2026-09-15). Instruments and gates; re-blessed on NR-875.
- **Sprint 42 wave 1** (COMPLETE 2026-09-16). Sixteen items; re-blessed on NR-877 the same day.
- **Backlog wave A** (CLOSED 2026-09-16). Twelve lanes, sixteen items: twelve delivered, four archived unmerged; re-blessed in 32e04a19. Waves B-D never ran -- Ben stopped at wave A and archived the rest. Record in the DEVLOG entry of the same date.
- **Sprint 44 — the corporate web's plumbing** (CLOSED 2026-09-18). BL-1030..1033 delivered; the retro is archived with the sprint; NR-889 carries the cost readings.

## Sprint 45 — industrialisation makes the web real (opened 2026-09-18)

**CLOSED 2026-09-23.** All seventeen items delivered, one re-bless spent; the retro is in
sprints.json. What the sprint raised and did not answer: NR-915 (the tick tail), BL-1065 (the
CTest tier), BL-1066 (the player cannot build) and the sprint 46 draft DENSITY_FOLLOWS_CITIES.

Three waves. Every world-mover lands behind a switch and turns on together in BL-1044, so the sprint
spends one re-bless. Memory caps concurrency (15.5 GB; a sweep holds ~2.9 GB): lanes run their own
harnesses one at a time and never `player_seed_sweep`; the main session runs the 16-seed
`--digest-check` on the integrated tree after each merge. Requirement groups:
`world-copy-ticks-exact`, `span-boundary-resume`, `resume-road-tier`, `industry-tree-wired`,
`charter-spend-rules`; later waves get theirs when promoted.

**Wave 0 — neutral, parallel worktrees**

- [x] **BL-1034 (world copies diverge)** — DELIVERED 2026-09-18: merged d61b6ae8, cold-reviewed, --digest-check 16/16 on 26c8ec6e. files: src/world/faithful_unordered_map.hpp (new), world.hpp, province.hpp, tools/verify/world_copy_determinism.cpp (new), world_digest.hpp (new).
  provides: a faithful world copy; the reproduction harness. consumes: harness_params.hpp's app-order build and settle (landed).
- [x] **BL-1036 (span boundary resume)** then **BL-1037 (resume road tier)** — DELIVERED 2026-09-18: merged 614a02e5, follow-ups 58193812, --digest-check 16/16. BL-1036 R6 partial (BL-1049). Lane A, generation-dev. files: src/world/history_sim.hpp/.cpp, era_minus_one.cpp, tools/verify/digitisation_sim_harness.cpp.
  provides: resume_dated_objects, consolidation_year, near_home_cutoff_year, resume_seeds_corridor_tier (off), the fidelity mode. consumes: exploration_output (landed).
- [x] **BL-1038 (Industry tree wired)** — DELIVERED 2026-09-18: merged 29b74f69, fix round 191c212b, --digest-check 16/16. Lane B, generation-dev. files: src/world/industry_tree_data.hpp (generated), history_sim.hpp/.cpp, tools/session/tree_lint.js, tools/verify/exploration_sim_harness.cpp. Shares history_sim.* with lane A in disjoint regions; the main session resolves the merge.
  provides: industry_tree_enabled (off), industry_open_year, the Industry mask triple and fold, the urban-mass rate, the fork-reachability lint. consumes: the amended industry_tree.json (this cut).
- [x] **BL-1039 (charter spend rules)** — DELIVERED 2026-09-19: timed rows on a quiet machine (runs.bl1039_sqrt), ceiling 120 (NR-902), --digest-check 16/16 and empty/zero/refused 3/3 on be5b3d72 (chain 11). files: src/world/charter_budget.hpp, corporation_generation.cpp, budget_system.hpp, tools/verify/player_seed_sweep.cpp, harness_params.hpp, charter_cost_sweep.json.
  provides: the sqrt cap rule, density_ceiling reason (filling goods in turn), per-good tallies; the unspent-points capital rule was built and reverted (NR-895). consumes: charter_budget / charter_spend_params (landed, BL-1032).

**Wave 1 — the span and Beat 1, still behind switches**

- [x] **BL-1040 (Digitisation span)** — DELIVERED 2026-09-19: merged 70923856, cold-reviewed, --digest-check 16/16 on 849a358d (chain 8). files: era_minus_one.*, hard_coded_world.*, world_gen_config.hpp, history_sim.hpp, src/ui/startup_screens.cpp, digitisation_sim_harness.cpp, exploration_sweep.cpp.
- [x] **BL-1051 (span open survey)** — DELIVERED 2026-09-19: merged 44545123, cold-reviewed, --digest-check 16/16 on 849a358d (chain 8). Every region surveyed for fuel and forest at the span open; ground_forest and furnace_lit read from it (NR-891, NR-892). files: settlement.hpp/.cpp, hard_coded_world.cpp, history_sim.*, exploration_sim_harness.cpp, digitisation_sim_harness.cpp.
- [x] **BL-1041 (industry points)** — DELIVERED 2026-09-19: merged f8137b8d, fix round 849a358d, --digest-check 16/16 (chain 8), readings 8 and 9 on 849a358d. files: settlement.hpp, history_sim.*, hard_coded_world.cpp, digitisation_sim_harness.cpp.
- [x] **BL-1053 (setup reads span close)** — DELIVERED 2026-09-19: merged 9725ee79, --digest-check 16/16 (chain 8), cold review clean (two low findings in BL-1058). When the span runs, world setup reads the 1960 close (treasuries, grudges, roads, junction markets); plus the BL-1040 review's harness and validator tightening and the loading-bar count. Must land before BL-1043. files: hard_coded_world.cpp, nation_generation.cpp, history_sim.*, app.cpp, startup_screens.cpp, digitisation_sim_harness.cpp.
- [x] **BL-1056 (points size-neutral)** — DELIVERED 2026-09-19: merged acc81398, fix round 2867cdaa, --digest-check 16/16 (chain 9). The resulting Fuel Doctrine split (coke 142 / charcoal 373) is NR-899, Ben's call before BL-1043 measures. files: history_sim.*, settlement.hpp, digitisation_sim_harness.cpp, exploration_sim_harness.cpp.
- [x] **BL-1059 (top-third bar)** — DELIVERED 2026-09-19: three rounds (NR-900, NR-904 in the build), --digest-check 16/16 on be5b3d72 (chain 11). files: history_sim.*, settlement.*, digitisation_sim_harness.cpp, exploration_sim_harness.cpp.
- [x] **BL-1042 (stockpile to budget)** — DELIVERED 2026-09-19: merged c9431b0b, fix round 587b5657 (NR-901), --digest-check 16/16, stockpile accounts close on 0/28/46 (chain 11).
- [x] **BL-1060 (charter spend hardening)** — DELIVERED 2026-09-20: four rounds (NR-902, NR-903, NR-905, NR-906), --digest-check 16/16 and the budget modes 3/3 on 9a39152a (chain 12). Round 3's review caught a refusal that would have switched the charter web off on a data tune.

**Wave 2 — measure, then Ben's calls**

- [ ] **BL-1043 (real-stockpile charter sweep)** — STAGE 2 IN (2026-09-21, serial, runs.real_stockpile_bl1043_stage2): seats median 1/3/4 at d/m 113/163/225 against the anchor's 9; no-specialist worlds at every d/m; 650:4 runs the live tick at x0.91 legacy. R3 and R6 complete; the seat curve read (d/m 162-1300) and every call ruled (NR-910), R5 complete; owed: R7 (the sprint-wide cold review, at BL-1044). STAGE 1 IN (2026-09-21): 16 seeds x 3 firm prices in three parallel shards, folded as runs.real_stockpile_bl1043_a/_b/_c; R1, R2 and R4 complete. It found that no fixed price holds the seat menu (NR-907, ruled: the price is a share of the world's stockpile), so stage 2 sweeps BL-1064's divisor instead. Owed: stage 2, the serial timing pass (R3), the province-cap reading, the remaining calls (R5), R6 and R7.
- [x] **BL-1064 (derived charter price)** — DELIVERED 2026-09-21: main session, two cold-review rounds, --digest-check 16/16 (2620 s), world_determinism digests unchanged, stockpile_budget_check --r8 ALL PASS. Ben 2026-09-21 (NR-907): a charter's price is the world's own stockpile divided by a constant, fixed at build; the specialist keeps its price in firm charters. Built before BL-1043 stage 2, which reads the divisor. Requirement group `derived-charter-price`. Built in the main session (2026-09-21), cold review after. files: stockpile_budget.*, harness_params.hpp, app.cpp, player_seed_sweep.cpp, stockpile_budget_check.cpp, world_determinism.cpp (the fold carries the price; span-on only).
  - [x] T1 derive the price in the builder; the divisor constant; the spend reads it (R1, R2).
  - [x] T2 the mirror and the app pass the derived spend; the harness records the spend it charged (R1).
  - [x] T3 the sweep's --price-divisors axis, the charged price per row, the seat-menu close (R3).
  - [x] T4 stockpile_budget_check's derivation and 5x cases, part 2's price (R4).
  - [x] T5 build, run the checks, world_determinism, digest-check 16/16, cold review (R5, R6).
  - STAGE 2, RE-AIMED (Ben, 2026-09-21, NR-908): the divisor answers live-play cost, the specialist price m answers the seat menu, which turns on d/m alone (the handoff's "150" was d/m, direction inverted). Five rows per seed as --price-pairs: 450:4, 650:4, 900:4 (seat menu at d/m 112/162/225) and 325:2, 1300:8 (density at 162).

**Wave 3 — ship**

- [ ] **BL-1050 (order-dependent reads)** — BUILT and VERIFIED on branch worktree-agent-ab112b1821025f551 (2026-09-20): seven readers ordered, fix round 3c9c5792 after a cold review; --copy-by snapshot 16/16 PASS (4166 s); the 16-seed --digest-check on the branch fails on D_settle and D_seat ONLY, never D_search or D_land, so the re-bless is tick-only (NR-894). R1-R6 complete; R7 (the re-pin) is BL-1044's. MERGED ONLY WITH BL-1044.
- [ ] **BL-1044 (Beat 1 ships)** — waits on BL-1037, BL-1043, BL-1050 and Ben's rulings. The one re-bless, the 1960 readings, the cold review.

  **INTEGRATION PLAN (drafted 2026-09-21 while BL-1043 stage 2 ran; read the item's design text first).**
  Three gates, in order: Ben's calls, then the build and the measurement, then Ben authorises the
  re-bless against the shape. Main session throughout; the machine quiet for Step 2.

  *Gate 0 — RULED 2026-09-21 (NR-908, NR-909, NR-910): m = 2 with the divisor tuned so the median
  library world opens 9 seats (near 580, from the seat curve); the seat spread accepted; the
  no-specialist world falls back to the no-budget world; province cap 2; sqrt base 8; the search-less
  paths spend the budget on the seed candidate. The list below is the call record.*
  - C1 the DIVISOR, against live-play cost (NR-908): stage 2's density pairs 325:2 / 650:4 / 1300:8
    at d/m 162 and their tick ratio against the legacy row.
  - C2 m, the specialist price in firm charters, against the seat menu. EARLY STAGE 2 (6 seeds): at
    d/m 162 the median is ~2.5 seats against a legacy 8, and 225 gives ~3.5 — the anchor's 9 sits
    ABOVE the bracket. Seats do turn on d/m alone (the three d/m-162 pairs agree on 5 of 6 seeds), so
    a supplementary SEAT-CURVE reading is owed before C2: seats at d/m 300 / 450 / 650. Cheap if
    built as a budget-only instrument (centres affording a specialist, from `build_stockpile_budget`
    alone — no search, no settle), else a stage 2b row set (seats are deterministic, so shards may
    run in parallel).
  - C3 THE NO-SPECIALIST WORLD — now real: seed 46 opens no seat at four of five pairs, seed 37 none
    at 450:4. Today the budget path leaves the player NULL (corporation_generation.cpp:3907-3926) and
    spawn_seat returns early (spawn_seat.cpp:118), against world.hpp:300's "exactly one is_player".
  - C4 the per-province cap (PROPOSED held at 2): read `prov` unspent off stage 2.
  - C5 the sqrt base c = 8 (`k_stockpile_per_resource_firm_cap`, still in the PROVISIONAL block).
  - C6 NR-909: the search-less paths on a span world (below).

  *Step 1 — integrate (on main, committed in increments; `git merge-tree` shows the merge CLEAN).*
  BUILT 2026-09-22 (6fddf614 merge, 99b8d1ce, 49a6deb1); each task's verification is in Step 2.
  Requirement group `beat-one-ships` (R1-R11, 2026-09-22). Step 2 is R8, Gate 2 is R9-R11.
  - [x] T1 (R1) merge BL-1050 (3c9c5792). Its eight files are untouched on main since the merge-base
    1dc8332b, so no conflict. It moves the legacy world's D_settle and D_seat only (NR-894).
  - [x] T2 (R2) flip `digitisation_span_enabled` (hard_coded_world.hpp:181) and `resume_seeds_corridor_tier`
    (history_sim.hpp:1962; the struct default, which also covers Exploration's resume) on by default;
    rewrite both "OFF BY DEFAULT" comments and era_minus_one.cpp:470. DONE, plus a
    `world_params::resume_seeds_corridor_tier` (not saved) so the legacy arc stays buildable.
  - [x] T3 (R3) pin C1, C2, C4, C5 in stockpile_budget.hpp and drop PROVISIONAL.
  - [x] T4 (R4) C3's ruling, and the invariant stated where it binds: print it on the seat line
    (app.cpp:970) and count it per seed in player_seed_sweep. DONE: `charter_budget_affords_specialist`
    in the search and the apply, reason `no_specialist`, guard S7; the walk's residual is NR-911.
  - [x] T5 (R5) C6: verify_api.cpp:529, main.cpp:148 and main.cpp:258 call `generate_background_firms`
    directly and only warn when the stockpile is non-empty — once T2 lands, EVERY `--verify`,
    `--serve` and headless world is a span world whose budget they ignore. DONE:
    `spend_stockpile_on_seed_candidate` (stockpile_budget.hpp).
  - [x] T6 (R6) harness re-points: exploration_sim_harness.cpp:812 (R6.6 reads the 1660 grudges; setup reads
    the 1960 close with the span on, BL-1053); haulage_measure run with `--epoch 0` (its default is
    1960, :178); player_seed_sweep's pins gain an ARC field — legacy rows keep BL-1031's D_search
    and D_land and take BL-1050's D_settle/D_seat under NR-894, recorded old -> new; the shipped
    rows are new; `--digest-check` checks the arc it builds, and a legacy mode keeps the span-off
    pins a live check. DONE: `world_arc` in harness_params.hpp, `--arc`; also era_world_harness R1/R7,
    digitisation_sim_harness's 1660/--continued/--fidelity/--resume-tier modes (they assumed the old
    default).
  - [x] T7 (R7) the rulings into DIGITISATION.md § 1 and CORPORATION_GENERATION.md; nothing else in a doc. Both already
    carried them (0b2e42f4); no edit.

  *Step 2 — measure (Release, serial, keep-awake; roughly 5-6 h of machine time).*
  world_determinism old -> new (A/A records); stockpile_budget_check --r8; charter_refusal_probe;
  world_copy_determinism --copy-by snapshot 16/16 (BL-1050's owed re-run, ~70 min);
  player_seed_sweep --digest then --digest-check on the shipped arc, and the legacy arc's check;
  exploration_sweep --out then seed_library.js --check --from (fingerprints may move ONLY through
  BL-1037's tier) then --bless; history_sim_harness (its 2-failure baseline), exploration_sim_harness,
  digitisation_sim_harness; haulage_measure --epoch 0 against the 1055/802 baseline; story_check
  (2 pre-existing US-016 failures); the 1960 readings per library seed (density follows cities:
  rho(firms, urban) against rho(firms, goods), BL-1029's 0.431 vs 0.460; industrialisation);
  setup cost per seed in Release AND the Debug app; the nation treasury credit rate against its two
  stated scales (BL-1053 (1)); the ctest suite — world-building rows may breach the 60 s default
  now that setup takes 21 -> 51 s on seed 28; the visual verify suite if C6 routes verify through
  the budget.

  *Step 2 RAN 2026-09-22 (5 h 20, serial, keep-awake).* Every determinism check held
  (world_determinism A/A twice; world_copy_determinism --copy-by snapshot 16/16; --fidelity and
  --resume-tier 16/16; the legacy arc kept D_search/D_land on 16/16 and moved D_settle/D_seat on
  16/16, NR-894's shape). haulage 2035 dispatches / 1627 market-to-market against the 1055/802
  floor. history_sim (2), story_check (2 US-016) and era_world (R5, BL-1010) sat at their known
  baselines; world_determinism R3.7, era_world R1/R7 and the digitisation modes were stale checks,
  re-pointed. FOUR FINDINGS went to Ben as NR-911 to NR-914.

  *Gate 2 — RULED 2026-09-22 (Ben, two forms). NR-911 the residual is reported and counted; NR-912
  the treasury rate stands and § Pass 7 is restated from the shipped world; NR-914 the divisor is
  re-pinned to the shipped world's nine-seat value, 650 (580 was its tier-off reading); NR-913
  pooling was built and measured (region/market/nation lift rho(firms, urban) 0.139 -> 0.186-0.211,
  the 120 ceiling binds, seats move) and NOT shipped — the web ships unpooled, the done-when is
  restated as taken-and-reported, density becomes the sprint 46 draft DENSITY_FOLLOWS_CITIES, and
  the pool stays an off-by-default switch. THE RE-BLESS is authorised and taken: the shipped arc's
  16 pins at 650:2 (every row one player, none fell back), the legacy rows' D_settle/D_seat moved
  (old -> new in the DEVLOG), the seed library blessed (6 of 16; the tier-off control reproduced all
  16, so the tier is the only mover). TWO COLD-REVIEW ROUNDS: eight findings, all fixed — the
  heaviest, the launch view framed world-gen's player whom the search replaces (latent since
  BL-977, on every world here).*

  *OWED before BL-1050 and BL-1044 close: the 650:2 live-play cost row (the tick against legacy and
  setup cost per seed), the ctest suite, the visual suite, and the requirement rows' results. Then
  sprint 45's retro and the sprint 46 cut.*

## Sprint 46 — the generation reaches the game (opened 2026-09-23)

BL-1066 (the player cannot build) opens it, and Ben's rulings on it (2026-09-23) put BL-1003
(pools per market) first. The cut from `drafts/sprint-46-items.json` follows. Requirement groups:
`pools-per-market`; BL-1066's gets written when BL-1003 has landed and capacity can be measured.

- [ ] **BL-1003 (pools per market)** — economy-dev in a worktree; main session merges, verifies, cold-reviews. Digest mover: reported, not re-pinned.
  - [ ] T1 rename the pool map; key (corp, market) with the body-level fallback and absorb-on-spawn (R1).
  - [ ] T2 production, processing, construction and upkeep draw and deposit at the tile's market; wants, purchases, upkeep_wants key the market (R1, R2).
  - [ ] T3 clearing: auto-surplus and standing orders per (corp, market); retire market_for_corp_on_body from goods flow (R2).
  - [ ] T4 convoys: source and destination market pools; the dispatcher's source walk (R3).
  - [ ] T5 every other reader: a body-aggregate helper or a stated market (R4).
  - [ ] T6 save format, state hash, the new pools_per_market harness (R1-R3, R5).
  - [ ] T7 main session: merge, build, world_determinism, the straddle count, the BL-1066 probe re-read, cold review (R5-R8).
- [ ] **BL-1066 (the player cannot build)** — after BL-1003: price the site multiplier at placement (gate, Build door preview, rival scorer); construction capacity where the player stands (measure first); the fixture places near home and waits out the tech gate.
