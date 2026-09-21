# Next session — build BL-1064, run BL-1043 stage 2, then BL-1044 (the re-bless)

Written 2026-09-21 at ~01:00. **Read the memory `io-sprint-45-open` first, then this, then
`docs/development/REFINED.md` § Sprint 45.**

## First thing: BL-1064 (derived charter price)

Ben ruled NR-907 on 2026-09-21: **a charter's price is the world's whole industry stockpile divided
by a constant**, fixed once when the budget is built; a specialist still costs its whole number of
firm charters, so BL-1039's structure is untouched. The rule is in `DIGITISATION.md` (the seat-menu
paragraphs). BL-1064 has the work: derive the firm price in the stockpile budget, name the divisor
as a PROVISIONAL constant beside the other charter constants, give `player_seed_sweep`'s cost mode a
`--price-divisors` axis that prints the derived price per seed, and add a `stockpile_budget_check`
case where two worlds whose stockpiles differ 5x open menus of similar size. Switch off, nothing
moves (digests and the 16-seed pin check). It is small — a lane, then a cold review, as everything
this sprint has needed one.

## Then BL-1043 stage 2

Sweep the DIVISOR, not a fixed price band (Ben's 80k/160k/320k answer was the fixed-price fallback
and no longer applies). Aim from stage 1: the median world holds ~61M points and the anchor is a
median of 9 seats, which implies a specialist around 420k and so a divisor near 150. **Sweep
100 / 150 / 220** over the 16 library seeds — roughly 13, 9 and 6 seats on the median world — and
report the median AND the min/max spread against stage 1's 1-to-91 at a fixed price: narrowing that
spread is the point of the ruling.

Stage 1 is in the table as `runs.real_stockpile_bl1043_a/_b/_c` (16 unique seeds x firm price
10000/20000/40000 x specialist 4 charters). Its own owed items: **the serial timing pass** (2-3 seeds
at the chosen divisor, ~1 h — stage 1 ran three shards in parallel so its milliseconds are
contended), **the province-cap reading** off the charter unspent reasons, and the remaining calls
(R5: the divisor, the specialist price in charters, the no-specialist world — no seed produced one
at any tested price).

## Then BL-1044 (Beat 1 ships) — the last item

It turns the span and the budget on by default, pins the prices and the divisor from Ben's rulings,
implements the no-specialist ruling, restores the one-player-corp invariant, and spends the sprint's
ONE re-bless. **Ben authorises the re-bless against the shape before anything is re-pinned.**
BL-1050's branch (`worktree-agent-ab112b1821025f551`, commit 3c9c5792) merges in the SAME
integration, never alone; its R7 (the 16-seed re-pin) closes there. Also carry BL-1050's owed
`--copy-by snapshot` re-run after the merge.

## Then the sprint 46 cut

`docs/development/drafts/sprint-46-items.json`: 20 item drafts (including the phase RENAME — Ben
leans Industrialisation, and the objection is that the word already names reading 8 and a region's
own event) plus BL-1047's amendment. Mint ids then, re-check the file:lines, file with sprint "46",
write groups, raise the campaign-tech scope flag, delete the draft file.

## Sprint 45 — where it stands

- **Delivered (13):** BL-1034, 1036, 1037, 1038, 1039, 1040, 1041, 1042, 1051, 1053, 1056, 1059,
  1060.
- **Built and verified, waiting on BL-1044's integration:** BL-1050 (R1-R6 complete).
- **In flight:** BL-1043 (stage 1 in; R1, R2, R4 complete), BL-1064 (filed, not started).
- **Left:** BL-1044.
- **Continuity pass, after Digitisation:** BL-1045, 1046, 1048, 1049, 1052, 1055, 1057, 1058, 1061,
  1062, 1063.

## Standing hazards

- **Memory, corrected:** a `player_seed_sweep` peaks at ~6 GB, not the ~2.9 GB the sprint note
  assumed. Three shards fit 15.5 GB only because their peaks are brief and staggered; watch
  `Memory Compression` — at 4 GB the machine is already squeezing.
- **Size a long run off the most expensive seed, not the first.** Seed 28 is the cheapest world in
  the library and seed 11 is four times it; an estimate off seed 28 was half the true cost.
- **Before killing any `player_seed_sweep`, read its COMMAND LINE.** On 2026-09-20 a chain-12 run 10
  hours into a 16-seed digest check was killed as a supposed orphan. A stopped background task's
  script can outlive its wrapper — kill the bash parent too, and check for a second copy.
- **A watcher that greps a log needs its pattern tested:** `=== seed 9$` never matched, so a shard
  ran two seeds twice (harmless here, and it gave a determinism cross-check).
- Timing runs need League's CLIENT closed and no lanes building. The PC sleeps after 4 h idle on AC.
- Every lane this sprint needed a fix round from its cold review, and two needed a second.
