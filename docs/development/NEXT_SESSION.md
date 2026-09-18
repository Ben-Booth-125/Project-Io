# Next session — rule NR-889, then cut sprint 45

Written 2026-09-18, at the close of sprint 44. **Read this, then the three open entries in
`docs/development/NEEDS_REVIEW.md`, then `docs/generation/DIGITISATION.md` § 1 and § Beat 1.**

## Where things stand

- **Sprints 43 and 44 are closed.** The 1960 baseline is measured, and the corporate web's plumbing
  is in: a per-centre charter budget reaches the landscape search and charters specialists and
  background firms around each centre, **off by default** — the app passes no budget, and
  `player_seed_sweep --digest-check` holds all 16 pinned seeds with no budget, an empty budget, an
  all-zero budget and a refused one.
- **The hot backlog holds one item:** BL-1034 (world copies diverge), filed and unscheduled.
- **Sprint 45 (industrialisation makes the web real) is a proposed goal row** in `sprints.json`.

## Three calls are waiting for Ben

- **NR-889 — the density cap.** His 2026-09-06 ruling (a body holds 8 firms per demanded good)
  against his 2026-09-17 one (the budget decides how many firms stand where). The cost sweep is in
  (`charter_cost_sweep.json`): keeping the cap freezes firms at 81 a body and leaves capital
  unspent; lifting it reaches the 200-per-body guard at 4-6x the legacy live tick; clustering alone
  costs up to 6x on seed 28. The recommendation on the entry is to keep the cap for now.
- **NR-890 — scheduling BL-1034.** A copied world ticks differently from its original (seed 28:
  D_settle 18F78EB9B2B20F29 against the pinned 265C48A23E313B1A). Latent today; recommended early in
  sprint 45, before anything previews or branches a world.
- **NR-886** — items 1, 2, 4, 5, 6 of wave A's calls (item 3's figure re-read on the app's world:
  74.3%; item 7 answered).

## Then cut sprint 45

Its goal: Digitisation runs 1660 → 1960 from the `exploration_output` struct, the Industry tree is
invested in, large cities accumulate located industry points, and each centre's unspent points
become the charter budget sprint 44's path spends — one re-bless, one cold review. Owed when its
items are cut: whether consolidation and the near-home cutoff re-anchor at 1660; a resume path for
dated objects and trade flows; whether Beat 1's first cut carries the three sinks or only
stockpiles; what a world with no affordable specialist gets (Ben deferred it here); and the charter
prices, off NR-889's table.

## Tools the two sprints left

- `exploration_sweep --seeds --out --through Y --cost` — per-half weakness counters.
- `digitisation_sim_harness --through Y --out` — the thirteen readings beside a 1660 control.
- `player_seed_sweep --digest | --digest-check [--charter-budget none|empty|zero|synthetic|refused]`
  — four world digests per seed against the BL-1031 pins. **Never re-pin them.**
- `player_seed_sweep --charter-cost [...]` — the cost matrix (`--budget-scales --resource-cap
  --province-cap --specialist-prices --ladder-scales --live-ticks --out --note`).
- `seed_library.js --seed-list | --from path` — the library reads `seed_library_sweep.json`.

## Standing hazards

- **Keep the machine awake for long sweeps.** The PC slept 22:07 → 09:03 mid-sweep on 2026-09-17.
- **Timings need a quiet machine**, serial, Release, build tree quoted. A 16-seed digest check is
  ~55 min; a cost row 200-1,000 s; the readings harness ~24 min.
- **Never baseline on `epoch_year = 1960`** (the superseded two-span arc, Exploration off).
- **Do not copy a world and then tick it** until BL-1034 is fixed.
- Write measuring runs to `--out`; regenerate tracked artefacts on the final integrated tree.
- Mint `NR-` ids against the hot store and the archive together.
- `history_sim_harness` carries a tracked 2-failure baseline (R3a2/R3a3); `story_check` fails 2 on
  US-016 — both pre-existing.
