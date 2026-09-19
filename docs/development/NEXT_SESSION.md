# Next session — finish sprint 45's wave 1, then measure, re-bless, cut sprint 46

Written 2026-09-19 at ~21:15, as Ben put the PC to sleep. **Read this, then
`docs/development/REFINED.md` § Sprint 45, then the DEVLOG entry of 2026-09-19.**

## First thing, in this order

1. **BL-1060 (charter spend hardening) round 3** was in its lane (worktree
   `.claude/worktrees/agent-abc618b633af63360`, branch `worktree-agent-abc618b633af63360`) when the
   PC slept. The brief: NR-906 — the even share of a binding ceiling is a RESERVATION, not a cap
   (a good may go past its share while the room left covers every other short good's unfilled
   share; the yards' places come off the ceiling first; a ceiling smaller than the turn is refused),
   plus probe cases that fail when reverted. Check `git -C <worktree> log -1` and `status`: if it
   committed, merge it; if not, snapshot its diff, checkpoint-commit on the branch and finish in the
   main session (a stopped agent's worktree survives; SendMessage may refuse it). Then a cold check
   of round 3, focused on the reservation arithmetic and the account closing.
2. **Chain 12 on the final main** (after round 3 merges), serial:
   `world_determinism`, `charter_refusal_probe`, `stockpile_budget_check`,
   `player_seed_sweep --digest-check` (16 seeds, ~50 min),
   `--digest-check --seeds 0,28,46 --charter-budget empty|zero|refused`,
   `--digest --charter-budget stockpile --seeds 0,28,46` (per seed: firms, firms per good, goods
   without a firm, unspent by reason — no false `no_gap`),
   and one `--charter-cost --seeds 28 --budget-scales 4 --resource-cap sqrt --density-ceilings 120
   --specialist-prices 4 --no-extra --no-forced` row (the sweep's share and overflow checks run only
   in that mode). Build each with `bash tools/verify/build_lua_harness.sh <name>`.
   That closes **BL-1060** (R1-R8). BL-1042 closed on chain 11 (it finished at 21:27 before the
   sleep).
3. **Wave 2: BL-1043 (real-stockpile charter sweep)** — quiet machine, keep-awake, serial; ends in
   NEEDS_REVIEW calls (the specialist price anchored to the seat menu; the provisional prices
   BL-1042 set in `stockpile_budget.hpp`). Then **BL-1050** and **BL-1044 (Beat 1 ships: the
   re-bless)**. BL-1044 must ask Ben before any re-bless.
4. **Sprint 46 cut** after the re-bless: `docs/development/drafts/sprint-46-items.json` holds 19
   item drafts and BL-1047's amendment from a fresh code read (2026-09-19). Mint ids then, re-check
   the file:lines, file with sprint "46", write groups, raise the campaign-tech scope flag, delete
   the draft file. Three calls wait for the cut: the convoy-arrival duty's scope, the depletion
   formula, the IN-node -> campaign-tech mapping. The select-corporation screen needs a design
   session with Ben first.

## Sprint 45 — where it stands

- **Delivered:** BL-1034, 1036, 1037, 1038 (wave 0); BL-1039, 1040, 1041, 1051, 1053, 1056, 1059
  (wave 1).
- **Delivered at the close:** BL-1042 (stockpile to budget), on chain 11.
- **Merged, owed round 3 and chain 12:** BL-1060 (charter spend hardening).
- **Not started:** BL-1043, BL-1050, BL-1044.
- **Continuity pass (after Digitisation):** BL-1045, 1046, 1048, 1049, 1052, 1055, 1057 (capacity
  kind filter), 1058 (span checks that cannot fail).

## Rulings taken 2026-09-19 (all written into their docs)

NR-896/897 (share, not best; treasury by scale), NR-899 (top third per resource), NR-900 (every
region, unclamped share), NR-901 (no town, no conversion; razed is lost), NR-902 (ceiling 120),
NR-903 (the turn skips an unplaceable good), NR-904 (a tie at the cut clears whole), NR-905/906
(each good keeps an even share of a binding ceiling, as a reservation). The Fuel Doctrine now reads
coke 239 / charcoal 277 / neither 340 at 1960 over the 16 library seeds.

## Standing hazards

- Memory caps concurrency (15.5 GB; a `player_seed_sweep` holds ~2.9 GB). Lanes never run the
  16-seed digest check, `exploration_sweep` or `--charter-cost`.
- Timing runs need League fully closed (the client, not just the game) and no lanes building.
- The PC sleeps; long correctness runs resume on wake, timing runs must be re-run.
- Every lane so far needed a fix round from its cold review; budget one, and verify figures on a
  main-session run before quoting them.
