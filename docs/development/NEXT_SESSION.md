# Next session — the sweeps run

The success-lever session (2026-09-01) landed the two missing buyers and designed the measurement
programme. The backlog holds exactly that programme: BL-723 (campaign lapse instrument) is the
gate — everything else queues behind it.

## NR-774 is RULED (Ben, 2026-09-01, via the session form) — resolved and archived

1. **GDP** := valued production. 2. **Influence** := market share + footprint + routes.
3. **Band** := **INDUSTRIAL 1960 ONLY** — every success-programme sweep passes `--epoch 1960`.
   The `world_params` default epoch is 0 (ancient), so an unflagged run is the WRONG band; the
   lapse manifest's band line is the check. The ancient rounds already run stand as labelled
   history.
4. **Proxy** := the corp AI with the two stated brackets. The sprint-31 spawn-intelligence idea
   was ruled HOLD — nothing filed.

## Order of work

1. **BL-723 (campaign lapse instrument)** — acquisition_viability's setup + per-tick CSV +
   parameter overrides on the `--reach` pattern + the lens-capture time-lapse script. T0 validity
   rows built in (A/A byte-identity, every metric differentially proved, zero-observation fails).
2. **BL-724 (spawn distribution)** and **BL-728 (demand composition)** — the two priority-A
   sweeps. Expect BL-728 to dominate every other lever's effect size.
3. **BL-725 (price levers)** — carries a live finding: the ceiling derivation has drifted.
   `haulage_measure` now demands **ceil > 14.07** at the binding case against the authored 10.0
   (p90 haul 5.65 cr/unit vs the 1.67 the 10.0 was derived from). Trade is healthy today (1,481
   dispatches vs the 1055 baseline) but the worst-tail market pair is unservable at any scarcity.
4. **BL-731 (nation_scorer_harness rot)** when convenient — calm_space has no direct scorer
   verification until it compiles again.

## Two delegated calls awaiting a word

- **BL-644's player exclusion under spectate:** `derive_space_programme_claims` skips
  `w.player_entity` unconditionally. Under BL-409's no-subject ruling the skip should arguably
  lift when spectating; today it stands, so one corp in every spectated field never sells to the
  state — a small measurement asymmetry the sweeps inherit until ruled.
- **BL-647 basket scope:** `trade_goods_misc` deliberately excluded (the design names four
  goods); BL-730 owns finding its buyer.

## Traps (unchanged from last hand-off, still true)

- Lua-linked harnesses build via `cmd //c tools\verify\build_lua_harness.bat <name>`; run as
  `./build_gen/verify/<name>.exe` from the repo root. `cmake --build` needs a vcvars shell.
- Worktree agents cut from a stale base — cut worktrees by hand from local `main` and name the
  expected commit in the brief (it caught both agents this session; the step-0 check works).
- chain_depth's one red row is the DELIBERATE named-list guard (tools, rigging,
  trade_goods_misc). Do not quiet it; shrink it by landing owners.
- The k_extractable widening moved every world: any remembered seed-0 number from before
  2026-09-01 is stale. Re-baseline from today's census.
