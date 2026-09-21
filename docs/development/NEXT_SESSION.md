# Next session — build BL-1044 (Beat 1 ships), then the sprint 46 cut

Written 2026-09-21 at ~23:50. **Read the memory `io-sprint-45-open` first, then this, then
`docs/development/REFINED.md` § Sprint 45 — BL-1044's INTEGRATION PLAN is written there in full.**

## Where it stands

- **Delivered today:** BL-1064 (derived charter price, b89d0c2b) — a firm charter costs the world's
  whole stockpile over a divisor, fixed at build; two cold-review rounds, --digest-check 16/16.
- **BL-1043 stage 2 is in** (e28fc758, `runs.real_stockpile_bl1043_stage2`, serial, 6.4 h) and the
  **seat curve** too (`stockpile_budget_check --seat-curve`, ab655703). Every call is RULED: NR-908
  (the divisor answers live-play cost, m the seat menu), NR-909 (the search-less paths spend the
  budget on the seed candidate), NR-910 (the pins, below). BL-1043's only open row is R7, the
  sprint-wide cold review at BL-1044.
- **Left in sprint 45:** BL-1050 (on its branch, merged only with BL-1044) and BL-1044.

## The pins (NR-910, Ben 2026-09-21 — all in DIGITISATION.md § 1)

- A specialist costs **2** firm charters (`k_stockpile_specialist_firm_charters`: 4 -> 2).
- The divisor is **580** (`k_stockpile_price_divisor`: 650 -> 580): the one at which the median
  library world opens the anchor's 9 seats at two charters. Seat curve at m = 2 (16 seeds):
  median 6.5 / 7 / 8 / 9.5 / 12.5 at d = 520 / 540 / 560 / 580 / 600; 580 is also the smallest
  divisor measured at which no library world falls back (seed 37 opens 0 at 560, 2 at 580);
  its spread runs 2 to 73. The tick at 580 is ~x0.8 the legacy world by interpolation — Step 2
  measures it.
- The seat spread is ACCEPTED (the anchor is a median; 2 to 73 seats across the library at 580:2).
- **No-specialist world:** falls back to the no-budget world, as a refused spend does, decided from
  the budget BEFORE anything is chartered (no centre affords a specialist -> the legacy branch).
- Per-province cap stays **2**; the sqrt base **c = 8**. Drop PROVISIONAL from all of them.

## BL-1044, in the plan's order (REFINED.md has the file:lines)

1. Merge BL-1050 (`worktree-agent-ab112b1821025f551`, 3c9c5792). `git merge-tree` shows it CLEAN.
2. Flip `digitisation_span_enabled` and `resume_seeds_corridor_tier` on by default; rewrite their
   "OFF BY DEFAULT" comments.
3. Pin the constants above; the no-specialist fallback; state the one-is_player invariant on the
   seat line (app.cpp:970) and count it per seed in player_seed_sweep.
4. NR-909: verify_api.cpp:529, main.cpp:148 and main.cpp:258 spend a non-empty budget on the seed
   candidate; an empty one lays today's web byte for byte.
5. Re-points: exploration_sim_harness.cpp:812 (R6.6); haulage_measure `--epoch 0`; player_seed_sweep
   pins gain an ARC field (legacy rows keep D_search/D_land, take BL-1050's D_settle/D_seat under
   NR-894, old -> new recorded; the shipped arc gets new rows).
6. Measure on a QUIET machine (Release, serial, keep-awake; ~5-6 h): the list in REFINED.md Step 2.
7. **Ben authorises the re-bless against that shape** before anything is re-pinned. Then the cold
   review of the sprint's integrated diff (fix round budgeted), BL-1050 and BL-1044 close, the
   sprint 45 retro, and the sprint 46 cut (`drafts/sprint-46-items.json`: mint ids, re-check
   file:lines, file with sprint "46", raise the campaign-tech scope flag, delete the draft).

## Standing hazards

- **Keep-awake:** no host tool exposes it. Start the harness, then
  `powershell -NoProfile -ExecutionPolicy Bypass -File tools/session/keepawake.ps1` in the background
  (it holds SetThreadExecutionState while any player_seed_sweep runs); its log must say "held".
- A `player_seed_sweep` peaks ~6 GB; ~6-7 GB is free. One sweep at a time for any timing reading.
- Timing rows need the machine quiet; seats, firms and accounts are deterministic and do not.
- Before killing any `player_seed_sweep`, read its COMMAND LINE, and kill the bash parent too.
- The validation run's SECOND tick spikes 30-80 s on every row, legacy included — structural, not
  a sleep; scan for hours-long outliers, not that.
- Every lane this sprint needed a fix round from its cold review. BL-1064's review needed two.
