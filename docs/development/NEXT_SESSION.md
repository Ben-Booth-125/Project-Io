# Next session — the sprint 43 gate, then the corporate web

Written 2026-09-17, at the close of sprint 43 (the 1960 baseline). **Read this, then NR-888 in
`docs/development/NEEDS_REVIEW.md`, then `docs/generation/DIGITISATION.md`.**

## Where things stand

- **Sprint 43 is closed.** Four items delivered, no shipped world moved (world_determinism digests
  identical before and after). Its retro sits in `archive/sprints-2026-Q3.json`.
- **The gate is open: NR-888.** Ben rules W1 (the alarm seal), W2 (new contacts never bind) and the
  span's clock on the 1960 baseline. The recommendation is to inherit both into Digitisation's beats
  at Exploration's 4-year band.
- **The hot backlog is empty again.** Sprints 44 (the corporate web's plumbing) and 45
  (industrialisation makes the web real) are proposed goal rows; their items are cut at the gate.
- **NR-886** is still open; item 7 is answered by BL-1029 (Digitisation readings at 1960), and
  item 3 (solvency no longer gates the seat) sits on sprint 44's path.

## What the baseline says

Two checked-in tables at the repo root, over the sixteen library seeds:
`exploration_sweep_1960.json` (cost and the weakness counters, per half) and
`digitisation_readings_1960.json` (the thirteen readings at 1660 and 1960).

- **"Through 1960" is `world_params::exploration_stop_year = 1960` at epoch 0.** The run to 1660
  reproduces every library fingerprint, so it is the shipped world continued. `epoch_year = 1960`
  is the superseded two-span arc with Exploration off; never baseline on it.
- **300 more years cost less than the 460 before them:** a median 1.7-1.9 s a seed in Release.
- **The continued world coasts.** No urbanisation, no decolonisation, flat trade, mixing that
  fades; only industrial polities climb. Every Digitisation beat must arrive as mechanism.
- **Advanced chains stays a structural zero** until the epoch flip decouples arc from recipe band.
- **The alarm is a seal:** ~89% of near-home reads at the ceiling, 99%+ of near-home campaign
  candidates treaty-blocked.
- **Polities meet, and what they meet never binds:** 1,250 first contacts by 1660, 0 of 492 new
  pairs bound; after 1660 contact nearly stops.

## Tools this sprint left

- `exploration_sweep --seeds a,b --out path --through Y --cost` — per-half weakness counters with a
  prefix check whenever Y > 1660.
- `digitisation_sim_harness --through Y --out path` — the thirteen readings beside a
  fingerprint-checked 1660 control.
- `seed_library.js --seed-list | --from path` — the library reads its own table,
  `seed_library_sweep.json`; regenerate it with the command in the store's note.

## Next, once NR-888 is ruled

Cut sprint 44's items from its goal row in `sprints.json`: the charter-budget seam into the landscape
search, off by default and byte-identical when empty, charter scope the whole web (Ben, 2026-09-17),
and ms per tick and per search evaluation at 1x/2x/4x a synthetic budget.

## Housekeeping owed

- The BL-1028 lane's worktree `.claude/worktrees/wf_b731ceb1-4dd-1` is merged and can be pruned.
- Mercenary tails (`NATIONS.md` § Trust, `SELECTION.md`'s contract card, `mercenary_contract` in the
  save headers) are still unfiled; a save-format seam change.

## Standing hazards

- Regenerate sweep artefacts on the **final integrated tree**; never overwrite a tracked table from a
  measuring run — use `--out`.
- Timings in **Release**, serial, on a quiet machine, with the build tree quoted. The readings
  harness takes ~24 min for the library quiet.
- Mint `NR-` ids against the **hot store and the archive together**.
- `history_sim_harness` carries a tracked 2-failure baseline (R3a2/R3a3); compare, don't expect green.
