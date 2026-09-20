# Next session — read BL-1043's stage 1, then BL-1044 (the re-bless) and the sprint 46 cut

Written 2026-09-20 at ~20:45, with two sweep shards running overnight. **Read the memory
`io-sprint-45-open` first, then this, then `docs/development/REFINED.md` § Sprint 45.**

## First thing: collect BL-1043 stage 1

Two shards of `player_seed_sweep --charter-cost --budget stockpile` ran in PARALLEL (Ben, to halve
the wait), 16 library seeds x firm price 10000/20000/40000 x specialist price 4 firm charters:

- **Shard A** — seeds 46, 28, 11, 31, 40, 12, 37, 13, 41, 43. Log `o_bl1043_s1.log`, JSON
  `charter_cost_stockpile_s1.json`; a watcher (`watch_a.sh`) stops it when it reaches seed 32, which
  is shard B's first. The JSON is rewritten after EVERY seed, so a kill keeps what is done.
- **Shard B** — seeds 32, 10, 25, 38, 9, 0. Log `o_bl1043_s1b.log`, JSON
  `charter_cost_stockpile_s1b.json`.
- Both in the session scratchpad
  `C:\Users\benbo\AppData\Local\Temp\claude\C--Users-benbo-Project-Io\1a59b838-3376-4f8f-9496-569285950a0c\scratchpad`.
  **The PC sleeps after 4 h idle on AC, so expect a pause around 00:35 and a resume on wake** — check
  the logs' seed count before assuming a hang.

Then: fold each shard into `charter_cost_sweep.json` with `charter_cost_merge.js` under its own key
(`real_stockpile_bl1043_a`, `_b`) with the note; the none rows of both shards together are a 16-seed
pin check, so read them. **The per-tick MILLISECONDS on both shards are contended and are NOT R3
evidence** — a short serial timing pass on 2-3 seeds at the chosen price is owed (~1 h).

**What stage 1 answers, and the calls it ends in (BL-1043 R5):** the firm price, the specialist price
in firm charters, whether a no-specialist world is acceptable, and whether the per-province cap
stays. The seat-menu anchor is the MEDIAN library world offering about as many seats as a world with
no budget. Early rows (seeds 46/28/11): legacy anchors 8 / 6 / 17 specialists; the budget opens
24 / 33 / 182 at 10k, 23 / 8 / 77 at 20k, 4 / 1 / 39 at 40k — so 10k is far too cheap and **20k
brackets the anchor on two of three**. Stage 2 narrows between the two prices that bracket it.

**Also file from stage 1:** the chartered good is NOT the extracted good — on seed 28's one cell, 128
of 132 extraction buildings extract something other than what they were chartered for (placement
anchors on the richest deposit on the tile). Every "every good holds firms" reading, including what
NR-903 and NR-905 were ruled to protect, is bookkeeping rather than ground truth. Put it to Ben with
the sweep's own figures.

## Then, in order

1. **BL-1044 (Beat 1 ships)** — the last item. It turns the span and the budget on by default, pins
   the prices from Ben's BL-1039 and BL-1043 rulings, implements the no-specialist ruling, restores
   the one-player-corp invariant, and spends the sprint's ONE re-bless. **Ben authorises the re-bless
   against the shape before anything is re-pinned.** BL-1050's branch
   (`worktree-agent-ab112b1821025f551`, commit 3c9c5792) merges in the SAME integration — never
   alone. Its R7 (the 16-seed re-pin) closes there.
2. **The sprint 46 cut** — `docs/development/drafts/sprint-46-items.json`: 20 item drafts (including
   the phase RENAME, whose name is Ben's call — he leans Industrialisation; the objection is that the
   word already names reading 8 and a region's own event) plus BL-1047's amendment. Mint ids then,
   re-check the file:lines, file with sprint "46", write groups, raise the campaign-tech scope flag,
   delete the draft file.
3. **The continuity pass** (after Digitisation, not before): BL-1045, 1046, 1048, 1049, 1052, 1055,
   1057, 1058, 1061 (flood-field warm order), 1062 (the order-dependence lint), 1063 (a tripwire
   throw in a worker loses its message).

## Sprint 45 — where it stands

- **Delivered (12):** BL-1034, 1036, 1037, 1038, 1039, 1040, 1041, 1042, 1051, 1053, 1056, 1059,
  1060.
- **Built, verified, waiting on BL-1044's integration:** BL-1050 (R1-R6 complete; 16/16 on
  `--copy-by snapshot`; the 16-seed pin check fails on D_settle and D_seat ONLY, never D_search or
  D_land, so the re-bless is the tick-only move NR-894 authorised).
- **In flight:** BL-1043 (harness merged; stage 1 running).
- **Left:** BL-1044.

## Standing hazards

- **Memory, corrected:** a `player_seed_sweep` peaks at ~6 GB, not the ~2.9 GB the sprint note
  assumed (measured 2026-09-20). Two shards fit 15.5 GB only because their peaks rarely collide;
  THREE do not fit. Lanes still never run a sweep.
- Before killing any `player_seed_sweep`, read its COMMAND LINE: on 2026-09-20 a chain-12 run 10 h
  into a 16-seed digest check was killed as a supposed orphan. A stopped background task's script can
  survive its wrapper — kill the bash parent too, and check for a second copy.
- Timing runs need League's CLIENT closed (not just the game) and no lanes building.
- Every lane this sprint needed a fix round from its cold review, and two needed a second; budget one.
